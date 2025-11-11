"use strict";

const fs = require('fs');
const os = require('os');
const childProcess = require('child_process');

function normalizeArch(arch) {
  if (arch === 'x64') return 'x86_64';
  if (arch === 'arm64') return 'aarch64';
  return arch;
}

function detectLibc() {
  if (process.report && typeof process.report.getReport === 'function') {
    try {
      const report = process.report.getReport();
      if (report?.header?.glibcVersionRuntime) {
        return 'gnu';
      }
    } catch (_) {
      // ignore and continue with fallbacks
    }
  }

  try {
    const output = childProcess.execSync('ldd --version', { encoding: 'utf8', stdio: ['ignore', 'pipe', 'ignore'] });
    if (/musl/i.test(output)) {
      return 'musl';
    }
    if (/glibc|gnu libc/i.test(output)) {
      return 'gnu';
    }
  } catch (_) {
    // ignore failures, fall back to fs detection
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

function getPlatform() {
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

function loadBinding() {
  const platform = getPlatform();
  const candidates = new Set([`${platform}-rocketmq.node`]);

  if (platform.startsWith('linux-')) {
    const legacy = platform.replace(/-(gnu|musl)$/, '');
    candidates.add(`${legacy}-rocketmq.node`);
  }

  let lastError;
  for (const candidate of candidates) {
    try {
      return require('bindings')(candidate);
    } catch (err) {
      lastError = err;
    }
  }

  throw new Error(`Failed to load RocketMQ addon (${platform}): ${lastError?.message || 'unknown error'}`);
}

module.exports = loadBinding();
