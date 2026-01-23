'use strict';

import * as fs from 'fs';
import * as os from 'os';
import * as path from 'path';
import * as childProcess from 'child_process';

function normalizeArch(arch: string): string {
  if (arch === 'x64') return 'x86_64';
  if (arch === 'arm64') return 'aarch64';
  return arch;
}

function detectLibc(): string {
  if (process.report && typeof process.report.getReport === 'function') {
    try {
      const report = process.report.getReport();
      if (report && report.header && (report.header as any).glibcVersionRuntime) {
        return 'gnu';
      }
    } catch (_) {
    }
  }

  try {
    const output = childProcess.execSync('ldd --version', {
      encoding: 'utf8',
      stdio: ['ignore', 'pipe', 'ignore']
    });
    if (/musl/i.test(output)) {
      return 'musl';
    }
    if (/glibc|gnu libc/i.test(output)) {
      return 'gnu';
    }
  } catch (_) {
  }

  if (
    fs.existsSync('/lib/ld-musl-x86_64.so.1') ||
    fs.existsSync('/lib/ld-musl-aarch64.so.1') ||
    fs.existsSync('/lib/ld-musl.so.1')
  ) {
    return 'musl';
  }

  return 'gnu';
}

function getPlatform(): string {
  const platform = os.platform();
  const arch = os.arch();

  if (platform === 'linux') {
    const normalizedArch = normalizeArch(arch);
    if (!normalizedArch) {
      throw new Error(`Unsupported architecture for Linux: ${arch}`);
    }
    const libc = detectLibc();
    return `linux-${normalizedArch}-${libc}`;
  }

  if (platform === 'darwin') {
    return 'darwin-universal';
  }

  throw new Error(`Unsupported platform: ${platform}, architecture: ${arch}`);
}

function ensureBindingBinary(rootDir: string): void {
  const buildPath = path.join(rootDir, 'build', 'rocketmq.node');
  if (!fs.existsSync(buildPath)) {
    throw new Error('Missing build/rocketmq.node. Run npm run build:coverage first.');
  }

  const platform = getPlatform();
  const candidates = [`${platform}-rocketmq.node`];
  if (platform.startsWith('linux-')) {
    candidates.push(`${platform.replace(/-(gnu|musl)$/, '')}-rocketmq.node`);
  }

  for (const candidate of candidates) {
    const target = path.join(rootDir, 'build', candidate);
    if (!fs.existsSync(target)) {
      fs.copyFileSync(buildPath, target);
    }
  }
}

export {
  ensureBindingBinary,
  getPlatform
};