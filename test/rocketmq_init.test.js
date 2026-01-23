'use strict';

const test = require('node:test');
const assert = require('assert');
const path = require('path');
const childProcess = require('child_process');

const { ensureBindingBinary } = require('./helpers/binding');

const rootDir = path.join(__dirname, '..');
process.env.NODE_BINDINGS_COMPILED_DIR = 'build';
ensureBindingBinary(rootDir);

function runInitWithEnv(extraEnv) {
  const env = { ...process.env, ...extraEnv, NODE_BINDINGS_COMPILED_DIR: 'build' };
  return childProcess.execFileSync(
    process.execPath,
    ['-e', "require('./dist/binding')"],
    { cwd: rootDir, env }
  );
}

test('module init without debug flag', () => {
  assert.doesNotThrow(() => {
    runInitWithEnv({ ROCKETMQ_DEBUG_STACK: '' });
  });
});

test('module init with debug flag', () => {
  assert.doesNotThrow(() => {
    runInitWithEnv({ ROCKETMQ_DEBUG_STACK: '1' });
  });
});
