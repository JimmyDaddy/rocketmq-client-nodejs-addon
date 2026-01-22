#!/bin/bash
set -euo pipefail

# =============================================================================
# RocketMQ Node.js Addon - 全平台构建脚本
# =============================================================================

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

RELEASE_DIR="$SCRIPT_DIR/Release"
LOG_DIR="$SCRIPT_DIR/.build-logs"

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

info()    { echo -e "${BLUE}[INFO]${NC} $*"; }
success() { echo -e "${GREEN}[OK]${NC} $*"; }
warn()    { echo -e "${YELLOW}[WARN]${NC} $*"; }
error()   { echo -e "${RED}[ERROR]${NC} $*"; }

# 检测当前架构
detect_arch() {
    case "$(uname -m)" in
        x86_64|amd64) echo "amd64" ;;
        arm64|aarch64) echo "arm64" ;;
        *) echo "unknown" ;;
    esac
}

# 检测操作系统
detect_os() {
    case "$(uname -s)" in
        Darwin) echo "macos" ;;
        Linux) echo "linux" ;;
        *) echo "unknown" ;;
    esac
}

HOST_ARCH=$(detect_arch)
HOST_OS=$(detect_os)

info "检测到系统: $HOST_OS / $HOST_ARCH"

# 初始化目录
mkdir -p "$RELEASE_DIR" "$LOG_DIR"

# =============================================================================
# 构建函数
# =============================================================================

build_with_act() {
    local job_name="$1"
    local container_arch="$2"
    local log_file="$LOG_DIR/${job_name}.log"

    info "开始构建: $job_name (container: $container_arch)"

    local artifact_name
    case "$job_name" in
        linux-gnu-x64)    artifact_name="linux-x86_64-gnu-rocketmq.node" ;;
        linux-gnu-arm64)  artifact_name="linux-aarch64-gnu-rocketmq.node" ;;
        linux-musl-x64)   artifact_name="linux-x86_64-musl-rocketmq.node" ;;
        linux-musl-arm64) artifact_name="linux-aarch64-musl-rocketmq.node" ;;
    esac

    # 运行 act（忽略 artifact 上传失败的退出码）
    act -j "$job_name" --container-architecture "linux/$container_arch" \
        --action-offline-mode \
        2>&1 | tee "$log_file" || true

    # 从容器中提取构建产物
    local container_name
    container_name=$(docker ps -a --filter "name=act-Build-and-Release-${job_name}" --format '{{.Names}}' | head -1)

    if [[ -n "$container_name" ]]; then
        if docker cp "$container_name:$SCRIPT_DIR/Release/$artifact_name" \
            "$RELEASE_DIR/$artifact_name" 2>/dev/null; then
            success "$job_name 构建完成: $artifact_name"
            return 0
        fi
    fi

    # 检查日志确认构建是否成功（即使 artifact 上传失败）
    if grep -q "Success - Main Build addon" "$log_file" && \
       grep -q "Success - Main Package artifact" "$log_file"; then
        warn "$job_name 构建成功但提取产物失败，请手动检查容器"
        return 0
    fi

    error "$job_name 构建失败，查看日志: $log_file"
    return 1
}

build_macos_universal() {
    info "开始构建: macOS Universal"
    local log_file="$LOG_DIR/macos-universal.log"

    (
        # 清理之前的构建
        rm -rf build/

        # 安装依赖
        npm install --no-audit --prefer-offline

        # 构建 RocketMQ 原生依赖
        ./deps/rocketmq/build.sh

        # 构建 addon
        npx cmake-js compile --CDCMAKE_BUILD_TYPE=Release \
            --CDCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
            --CDCMAKE_OSX_DEPLOYMENT_TARGET=11 \
            --CDCMAKE_SKIP_DEPENDENCY_TRACKING=ON

        # 打包产物
        strip -S -x build/rocketmq.node
        cp build/rocketmq.node "$RELEASE_DIR/darwin-universal-rocketmq.node"
    ) 2>&1 | tee "$log_file"

    if [[ -f "$RELEASE_DIR/darwin-universal-rocketmq.node" ]]; then
        success "macOS Universal 构建完成"
        return 0
    else
        error "macOS Universal 构建失败，查看日志: $log_file"
        return 1
    fi
}

# =============================================================================
# 构建任务
# =============================================================================

build_linux_native() {
    # 原生架构构建（快）
    if [[ "$HOST_ARCH" == "amd64" ]]; then
        build_with_act "linux-gnu-x64" "amd64"
        build_with_act "linux-musl-x64" "amd64"
    elif [[ "$HOST_ARCH" == "arm64" ]]; then
        build_with_act "linux-gnu-arm64" "arm64"
        build_with_act "linux-musl-arm64" "arm64"
    fi
}

build_linux_emulated() {
    # 模拟架构构建（慢，需要 QEMU）
    if [[ "$HOST_ARCH" == "amd64" ]]; then
        warn "ARM64 构建需要 QEMU 模拟，会比较慢..."
        build_with_act "linux-gnu-arm64" "arm64"
        build_with_act "linux-musl-arm64" "arm64"
    elif [[ "$HOST_ARCH" == "arm64" ]]; then
        warn "x86_64 构建需要 QEMU 模拟，会比较慢..."
        build_with_act "linux-gnu-x64" "amd64"
        build_with_act "linux-musl-x64" "amd64"
    fi
}

build_all_linux() {
    build_linux_native
    build_linux_emulated
}

# 本地直接构建当前架构（不依赖 act/docker），用于快速调试
build_local_host() {
    info "=== 本地快速构建当前架构（无容器） ==="
    local log_file="$LOG_DIR/local-${HOST_OS}-${HOST_ARCH}.log"
    local artifact_name=""

    if [[ "$HOST_OS" == "linux" ]]; then
        if [[ "$HOST_ARCH" == "amd64" ]]; then
            artifact_name="linux-x86_64-gnu-rocketmq.node"
        elif [[ "$HOST_ARCH" == "arm64" ]]; then
            artifact_name="linux-aarch64-gnu-rocketmq.node"
        else
            error "当前 Linux 架构不支持快速构建: $HOST_ARCH"
            return 1
        fi
    else
        error "快速构建仅支持 Linux 主机"
        return 1
    fi

    (
        rm -rf build/
        npm install --no-audit --prefer-offline
        ./deps/rocketmq/build.sh
        npx cmake-js compile --CDCMAKE_BUILD_TYPE=Release
        strip -s build/rocketmq.node 2>/dev/null || true
        cp build/rocketmq.node "$RELEASE_DIR/$artifact_name"
    ) 2>&1 | tee "$log_file"

    if [[ -f "$RELEASE_DIR/$artifact_name" ]]; then
        success "本地快速构建完成: $artifact_name"
        return 0
    fi

    error "本地快速构建失败，查看日志: $log_file"
    return 1
}

# =============================================================================
# 主逻辑
# =============================================================================

show_help() {
    cat << EOF
用法: $0 [选项]

选项:
    all           构建所有平台 (默认)
    linux         构建所有 Linux 目标
    linux-native  仅构建当前架构的 Linux 目标 (快)
    linux-cross   仅构建交叉架构的 Linux 目标 (慢)
    macos         构建 macOS Universal
    local         本地快速构建当前 Linux 架构（无 act/docker）

    linux-gnu-x64     单独构建 linux-gnu-x64
    linux-gnu-arm64   单独构建 linux-gnu-arm64
    linux-musl-x64    单独构建 linux-musl-x64
    linux-musl-arm64  单独构建 linux-musl-arm64

    -h, --help    显示帮助信息

示例:
    $0                    # 构建所有
    $0 linux-native       # 仅构建当前架构
    $0 linux-gnu-x64      # 仅构建指定目标
    $0 macos              # 仅构建 macOS
    $0 local              # 本地快速构建当前 Linux 架构
EOF
}

main() {
    local target="${1:-all}"

    case "$target" in
        -h|--help)
            show_help
            exit 0
            ;;
        all)
            info "=== 构建所有平台 ==="
            if [[ "$HOST_OS" == "macos" ]]; then
                build_macos_universal
            fi
            build_all_linux
            ;;
        linux)
            info "=== 构建所有 Linux 目标 ==="
            build_all_linux
            ;;
        linux-native)
            info "=== 构建当前架构 Linux 目标 ==="
            build_linux_native
            ;;
        linux-cross)
            info "=== 构建交叉架构 Linux 目标 ==="
            build_linux_emulated
            ;;
        local)
            info "=== 本地快速构建（当前 Linux 架构） ==="
            build_local_host
            ;;
        macos)
            if [[ "$HOST_OS" != "macos" ]]; then
                error "macOS 构建只能在 macOS 上运行"
                exit 1
            fi
            build_macos_universal
            ;;
        linux-gnu-x64)
            build_with_act "linux-gnu-x64" "amd64"
            ;;
        linux-gnu-arm64)
            build_with_act "linux-gnu-arm64" "arm64"
            ;;
        linux-musl-x64)
            build_with_act "linux-musl-x64" "amd64"
            ;;
        linux-musl-arm64)
            build_with_act "linux-musl-arm64" "arm64"
            ;;
        *)
            error "未知目标: $target"
            show_help
            exit 1
            ;;
    esac

    echo ""
    info "=== 构建产物 ==="
    ls -lh "$RELEASE_DIR"/*.node 2>/dev/null || warn "没有找到构建产物"
}

main "$@"
