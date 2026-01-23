'use strict';

import { describe, test, expect } from 'vitest';
import * as path from 'path';
import * as childProcess from 'child_process';

import { ensureBindingBinary } from './helpers/binding';

const rootDir = path.join(__dirname, '..');
process.env.NODE_BINDINGS_COMPILED_DIR = 'build';
ensureBindingBinary(rootDir);

function runInitWithEnv(extraEnv: Record<string, string>): Buffer {
  const env = { ...process.env, ...extraEnv, NODE_BINDINGS_COMPILED_DIR: 'build' };
  return childProcess.execFileSync(
    process.execPath,
    ['-e', "require('./dist/binding')"],
    { cwd: rootDir, env }
  );
}

describe('RocketMQ initialization tests', () => {
  test('module init without debug flag', () => {
    expect(() => {
      runInitWithEnv({ ROCKETMQ_DEBUG_STACK: '' });
    }).not.toThrow();
  });

  test('module init with debug flag', () => {
    expect(() => {
      runInitWithEnv({ ROCKETMQ_DEBUG_STACK: '1' });
    }).not.toThrow();
  });
});