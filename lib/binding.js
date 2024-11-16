"use strict";

const os = require('os');

function getPlatform() {
  const platform = os.platform();
  const arch = os.arch();

  if (platform === 'linux') {
    if (arch === 'arm64') {
      return 'linux-aarch64';
    }
    if (arch === 'x64') {
      return 'linux-x86_64';
    }
  }

  if (platform === 'darwin') {
    return 'darwin-universal';
  }

  throw new Error(`Unsupported platform: ${platform}, architecture: ${arch}`);
}

function binding() {
  const platform = getPlatform();

  try {
    return require('bindings')(`${platform}-rocketmq.node`)
  } catch (err) {
    console.log(err)
    throw new Error(`Failed to load RocketMQ addon: ${err.message}`);
  }
}

module.exports = binding();