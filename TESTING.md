# 测试指南

本项目现在使用 **Vitest** 作为测试框架，并完全支持 **TypeScript**。

## 🎯 测试设置

### 测试框架
- **Vitest**: 现代、快速的测试框架
- **TypeScript**: 所有测试文件使用 `.ts` 扩展名
- **原生绑定支持**: 完全兼容 C++ 原生模块

### 文件结构
```
test/
├── consumer.test.ts      # Consumer 功能测试
├── consumer_ack.test.ts  # Consumer ACK 测试
├── producer.test.ts      # Producer 功能测试
├── rocketmq_init.test.ts # 初始化测试
└── helpers/
    └── binding.ts        # 测试辅助工具
```

## 🚀 运行测试

### 基本命令

```bash
# 完整测试（推荐）- 编译原生模块 + TypeScript + 运行测试
npm test

# 快速测试 - 只编译 TypeScript + 运行测试（需要原生模块已编译）
npm run test:dryrun

# 开发模式测试 - 带内存优化
npm run test:dev

# 覆盖率测试
npm run test:coverage

# 监视模式 - 文件变化时自动重新运行
npm run test:watch
```

### 单独运行 Vitest

```bash
# 直接运行 Vitest
npm run vitest

# 带覆盖率
npm run vitest:coverage
```

## 📝 编写测试

### TypeScript 测试示例

```typescript
import { describe, test, expect } from 'vitest';
import { RocketMQProducer } from '../dist/producer';

describe('Producer tests', () => {
  test('should create producer instance', () => {
    const producer = new RocketMQProducer('test-group');
    expect(producer).toBeTruthy();
    expect(producer.status).toBe(0);
  });

  test('should handle async operations', async () => {
    const producer = new RocketMQProducer('test-group');
    await producer.start();
    expect(producer.status).toBe(1);
    await producer.shutdown();
  });
});
```

### 重要注意事项

1. **导入路径**: 测试文件从 `../dist/` 导入编译后的代码，以确保原生绑定兼容性
2. **类型安全**: 所有测试代码都有完整的 TypeScript 类型检查
3. **异步测试**: 使用 `async/await` 处理异步操作
4. **环境变量**: 测试会自动设置必要的环境变量

## 🔧 配置

### Vitest 配置 (`vitest.config.mjs`)

- **环境**: Node.js 环境，适合原生绑定
- **TypeScript**: 通过 esbuild 自动编译
- **超时**: 原生操作设置 30 秒超时
- **并发**: 限制为单进程以避免原生绑定冲突
- **覆盖率**: 使用 v8 提供商，包含源码覆盖率

### 覆盖率阈值

- 行覆盖率: 80%
- 函数覆盖率: 80%
- 分支覆盖率: 70%
- 语句覆盖率: 80%

## 🐛 调试

### 常见问题

1. **原生绑定加载失败**
   ```bash
   # 确保原生模块已编译
   npm run build:native:test
   ```

2. **TypeScript 编译错误**
   ```bash
   # 清理并重新编译
   npm run clean:ts
   npm run build:ts
   ```

3. **测试超时**
   - 原生操作测试超时设置为 30 秒
   - 如需调整，修改 `vitest.config.mjs` 中的 `testTimeout`

### 调试模式

```bash
# 使用 Node.js 调试器
node --inspect-brk node_modules/.bin/vitest run

# 详细输出
npm run vitest -- --reporter=verbose
```

## 📊 测试覆盖率

生成详细的覆盖率报告：

```bash
npm run test:coverage
```

报告将生成在 `coverage/` 目录中，包括：
- HTML 报告: `coverage/index.html`
- JSON 数据: `coverage/coverage-final.json`
- 文本摘要: 控制台输出
