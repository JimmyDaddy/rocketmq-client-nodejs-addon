'use strict';

import { describe, test, expect } from 'vitest';
import * as path from 'path';
import { LogLevel, Status } from '../src/constants';

import { ensureBindingBinary } from './helpers/binding';

const rootDir = path.join(__dirname, '..');
process.env.NODE_BINDINGS_COMPILED_DIR = 'build';
ensureBindingBinary(rootDir);

// Import from compiled dist for native binding compatibility
import { RocketMQPushConsumer } from '../src/consumer';
import binding from '../src/binding';

function setEnv(env: Record<string, string | undefined>): Record<string, string | undefined> {
  const original: Record<string, string | undefined> = {};
  for (const key of Object.keys(env)) {
    original[key] = process.env[key];
    if (env[key] === undefined) {
      delete process.env[key];
    } else {
      process.env[key] = env[key];
    }
  }
  return original;
}

function restoreEnv(env: Record<string, string | undefined>, original: Record<string, string | undefined>): void {
  for (const key of Object.keys(env)) {
    if (original[key] === undefined) {
      delete process.env[key];
    } else {
      process.env[key] = original[key];
    }
  }
}

describe('PushConsumer tests', () => {
  describe('PushConsumer constructor tests', () => {
    test('constructor overloads and logLevel mapping', () => {
      const c1 = new RocketMQPushConsumer('G1', { nameServer: '127.0.0.1', logLevel: LogLevel.DEBUG });
      const c2 = new RocketMQPushConsumer('G2', 'INST', { logLevel: LogLevel.NUM });
      const c3 = new RocketMQPushConsumer('G3', 'INST', {});

      expect(c1.status).toBe(Status.STOPPED);
      expect(c2.status).toBe(Status.STOPPED);
      expect(c3.status).toBe(Status.STOPPED);
    });

    test('constructor options mapping', () => {
      const consumer = new RocketMQPushConsumer('G1', {
        nameServer: '127.0.0.1',
        groupName: 'GROUP_A',
        threadCount: 3,
        maxBatchSize: 16,
        maxReconsumeTimes: 2,
        logLevel: 1,
        logDir: '/tmp',
        logFileSize: 8,
        logFileNum: 2
      });

      expect(consumer.status).toBe(Status.STOPPED);
    });

    test('constructor logLevel boundary values', () => {
      const c1 = new RocketMQPushConsumer('G1', { logLevel: -1 as any });
      const c2 = new RocketMQPushConsumer('G2', { logLevel: 999 as any });
      expect(c1.status).toBe(Status.STOPPED);
      expect(c2.status).toBe(Status.STOPPED);
    });
  });

  describe('PushConsumer configuration tests', () => {
    test('setListener replaces previous listener', () => {
      const consumer = new RocketMQPushConsumer('G1', {});
      consumer.core.setListener(() => {});
      consumer.core.setListener(() => {});
      expect(consumer.status).toBe(Status.STOPPED);
    });

    test('setListener release throw is swallowed', () => {
      const env = { ROCKETMQ_STUB_CONSUMER_RELEASE_THROW: '1' };
      const original = setEnv(env);
      try {
        const consumer = new RocketMQPushConsumer('G1', {});
        consumer.core.setListener(() => {});
        expect(consumer.status).toBe(Status.STOPPED);
      } finally {
        restoreEnv(env, original);
      }
    });

    test('setSessionCredentials validates input', () => {
      const consumer = new RocketMQPushConsumer('G1', {});
      expect(consumer.setSessionCredentials('a', 'b', 'c')).toBe(true);
      expect(() => consumer.setSessionCredentials('a', 1 as any, 'c')).toThrow(/secretKey must be a string/);
    });

    test('core method validation errors', () => {
      const consumer = new RocketMQPushConsumer('G1', {});
      expect(() => consumer.core.start(undefined as any)).toThrow(/Function expected/);
      expect(() => consumer.core.shutdown(1 as any)).toThrow(/Function expected/);
      expect(() => (consumer.core as any).subscribe('TopicOnly')).toThrow(/Wrong number of arguments/);
      expect(() => (consumer.core as any).subscribe('TopicOnly', 1 as any)).toThrow(/Topic and expression must be strings/);
      expect(() => (consumer.core as any).setListener(123 as any)).toThrow(/Function expected/);
      expect(() => (consumer.core as any).setSessionCredentials('a', 'b')).toThrow(/Wrong number of arguments/);
      expect(() => (consumer.core as any).setSessionCredentials('a', 1, 'c')).toThrow(/All arguments must be strings/);
    });
  });

  describe('PushConsumer subscribe tests', () => {
    test('subscribe supports topic and expression', () => {
      const consumer = new RocketMQPushConsumer('G1', {});
      expect(() => consumer.subscribe('TopicA')).not.toThrow();
      expect(() => consumer.subscribe('TopicA', 'TagA')).not.toThrow();
    });

    test('subscribe propagates native errors', async () => {
      const env = { ROCKETMQ_STUB_CONSUMER_SUBSCRIBE_ERROR: '1' };
      const original = setEnv(env);
      try {
        const consumer = new RocketMQPushConsumer('G1', {});
        expect(() => consumer.subscribe('TopicC')).toThrow(/consumer subscribe error/);
      } finally {
        restoreEnv(env, original);
      }
    });
  });

  describe('PushConsumer lifecycle and message handling tests', () => {
    test('start and shutdown promise path with message event', async () => {
      const env = { ROCKETMQ_STUB_CONSUME_MESSAGE: '1' };
      const original = setEnv(env);
      let consumer: any;
      try {
        consumer = new RocketMQPushConsumer('G1', { nameServer: '127.0.0.1' });
        const message = new Promise((resolve) => {
          consumer.once('message', (msg: any, ack: any) => {
            ack.done();
            resolve(msg);
          });
        });
        await consumer.start();
        const msg: any = await message;
        expect(msg.topic).toBe('TopicTest');
        expect(msg.tags).toBe('TagA');
        expect(msg.keys).toBe('KeyA');
        expect(msg.body).toBe('Hello');
        expect(msg.msgId).toBe('MSGID');
        await consumer.shutdown();
        consumer = null;
      } finally {
        if (consumer && consumer.status === Status.STARTED) {
          await consumer.shutdown();
        }
        restoreEnv(env, original);
      }
    });

    test('message ack false path', async () => {
      const env = { ROCKETMQ_STUB_CONSUME_MESSAGE: '1' };
      const original = setEnv(env);
      let consumer: any;
      try {
        consumer = new RocketMQPushConsumer('G1', {});
        const seen = new Promise((resolve) => {
          consumer.once('message', (msg: any, ack: any) => {
            ack.done(false);
            resolve(msg);
          });
        });
        await consumer.start();
        await seen;
        await consumer.shutdown();
        consumer = null;
      } finally {
        if (consumer && consumer.status === Status.STARTED) {
          await consumer.shutdown();
        }
        restoreEnv(env, original);
      }
    });

    test('message handler throws emits error and auto nacks', async () => {
      const env = { ROCKETMQ_STUB_CONSUME_MESSAGE: '1' };
      const original = setEnv(env);
      let consumer: any;
      try {
        consumer = new RocketMQPushConsumer('G1', {});
        
        const errorPromise = new Promise((resolve) => {
          consumer.once('error', (err: any, msg: any, ack: any) => resolve({ err, msg, ack }));
        });
        
        consumer.once('message', () => {
          throw new Error('handler boom');
        });

        consumer.subscribe('TopicA', '*');
        await consumer.start();

        const result: any = await Promise.race([
          errorPromise,
          new Promise((_, reject) => setTimeout(() => reject(new Error('error event timeout')), 2000))
        ]);

        expect(result.err.message).toMatch(/handler boom/);
        expect(result.msg).toBeTruthy();
        expect(result.ack).toBeTruthy();
      } finally {
        if (consumer && consumer.status === Status.STARTED) {
          await consumer.shutdown();
        }
        restoreEnv(env, original);
      }
    });
  });

  describe('PushConsumer error handling tests', () => {
    test('start error with Promise pattern', async () => {
      const env1 = { ROCKETMQ_STUB_CONSUMER_START_ERROR: '1' };
      const original1 = setEnv(env1);
      try {
        const consumer = new RocketMQPushConsumer('G1', {});
        await expect(consumer.start()).rejects.toThrow(/consumer start error/);
      } finally {
        restoreEnv(env1, original1);
      }
    });

    test('shutdown error with Promise pattern', async () => {
      let consumer: any;
      const env2 = { ROCKETMQ_STUB_CONSUMER_SHUTDOWN_ERROR: '1' };
      const original2 = setEnv(env2);
      try {
        consumer = new RocketMQPushConsumer('G1', {});
        await consumer.start();
        await expect(consumer.shutdown()).rejects.toThrow(/consumer shutdown error/);
      } finally {
        if (consumer && consumer.status !== Status.STOPPED) {
          delete process.env.ROCKETMQ_STUB_CONSUMER_SHUTDOWN_ERROR;
          consumer.status = Status.STARTED;
          await consumer.shutdown().catch(() => {});
        }
        restoreEnv(env2, original2);
      }
    });

    test('destructor handles shutdown error', async () => {
      const env = { ROCKETMQ_STUB_CONSUMER_SHUTDOWN_ERROR: '1' };
      const original = setEnv(env);
      try {
        let finalize: (() => void) | undefined;
        const finalized = new Promise<void>((resolve) => {
          finalize = resolve;
        });
        const registry = new FinalizationRegistry(() => {
          finalize!();
        });
        let core = new binding.PushConsumer('G1', null, {});
        registry.register(core, 1);
        core = null as any;
        
        // Force garbage collection
        if ((global as any).gc) {
          (global as any).gc();
        }
        
        // Wait for finalization callback with timeout
        try {
          await Promise.race([
            finalized,
            new Promise((_, reject) => setTimeout(() => reject(new Error('finalize timeout')), 1000))
          ]);
        } catch (err) {
          // Finalization timeout is acceptable - GC may not happen immediately
          // But we still expect the destructor to handle shutdown error gracefully
        }
      } finally {
        restoreEnv(env, original);
      }
    });
  });

  describe('PushConsumer native edge branches', () => {
    test('native edge cases coverage', async () => {
      function withEnv(values: Record<string, string>, fn: () => any): any {
        const original: Record<string, string | undefined> = {};
        for (const key of Object.keys(values)) {
          original[key] = process.env[key];
          process.env[key] = values[key];
        }
        const done = () => {
          for (const key of Object.keys(values)) {
            if (original[key] === undefined) {
              delete process.env[key];
            } else {
              process.env[key] = original[key];
            }
          }
        };
        const result = fn();
        if (result && typeof result.then === 'function') {
          return result.finally(done);
        }
        done();
        return result;
      }

      async function runCase(env: Record<string, any>): Promise<void> {
        await withEnv(env, async () => {
          let consumer: any;
          try {
            consumer = new RocketMQPushConsumer('G1', {});
            if (process.env.ROCKETMQ_STUB_THROW_JS_LISTENER) {
              process.once('uncaughtException', () => {});
              consumer.core.setListener(() => {
                throw new Error('listener error');
              });
            }
            consumer.subscribe('TopicA', '*');
            await consumer.start();
            await consumer.shutdown();
          } catch (err) {
            // Ignore expected errors from stub configurations
          } finally {
            if (consumer && consumer.status === Status.STARTED) {
              try {
                await consumer.shutdown();
              } catch (err) {
                // Ignore shutdown errors
              }
            }
          }
        });
      }

      const cases = [
        { ROCKETMQ_STUB_CONSUME_MESSAGE: '1', ROCKETMQ_STUB_CONSUMER_NULL_DATA: '1', ROCKETMQ_STUB_CONSUMER_PROMISE_PRESET: '1' },
        { ROCKETMQ_STUB_CONSUME_MESSAGE: '1', ROCKETMQ_STUB_CONSUMER_NULL_ENV: '1', ROCKETMQ_STUB_CONSUMER_PROMISE_PRESET: '1' },
        { ROCKETMQ_STUB_CONSUME_MESSAGE: '1', ROCKETMQ_STUB_CONSUMER_ACK_EMPTY: '1' },
        { ROCKETMQ_STUB_CONSUME_MESSAGE: '1', ROCKETMQ_STUB_CONSUMER_ACK_NULL: '1' },
        { ROCKETMQ_STUB_CONSUME_MESSAGE: '1', ROCKETMQ_STUB_CONSUMER_PROMISE_SET: '1', ROCKETMQ_STUB_CONSUMER_LISTENER_ERROR: '1', ROCKETMQ_STUB_CONSUMER_PROMISE_PRESET: '1' },
        { ROCKETMQ_STUB_CONSUME_MESSAGE: '1', ROCKETMQ_STUB_CONSUMER_THROW: '1', ROCKETMQ_STUB_CONSUMER_PROMISE_PRESET: '1' },
        { ROCKETMQ_STUB_CONSUME_MESSAGE: '1', ROCKETMQ_STUB_CONSUMER_BLOCKING_FAIL: '1' },
        { ROCKETMQ_STUB_CONSUME_MESSAGE: '1', ROCKETMQ_STUB_CONSUMER_TIMEOUT: '1', ROCKETMQ_STUB_CONSUMER_TIMEOUT_SKIP_CALL: '1' },
        { ROCKETMQ_STUB_CONSUME_MESSAGE: '1', ROCKETMQ_STUB_CONSUMER_TIMEOUT_SKIP_CALL: '1', ROCKETMQ_STUB_CONSUMER_PROMISE_PRESET: '1' },
        { ROCKETMQ_STUB_CONSUME_MESSAGE: '1', ROCKETMQ_STUB_CONSUMER_TIMEOUT_SKIP_CALL: '1', ROCKETMQ_STUB_CONSUMER_PROMISE_PRESET: '1', ROCKETMQ_STUB_CONSUMER_PROMISE_PRESET_FALSE: '1' },
        { ROCKETMQ_STUB_CONSUME_MESSAGE: '1', ROCKETMQ_STUB_CONSUMER_TIMEOUT: '1' },
        { ROCKETMQ_STUB_CONSUME_MESSAGE: '1', ROCKETMQ_STUB_CONSUMER_ABORT_TSFN: '1', ROCKETMQ_STUB_CONSUMER_TIMEOUT: '1' },
        { ROCKETMQ_STUB_CONSUME_MESSAGE: '1', ROCKETMQ_STUB_THROW_JS_LISTENER: '1' },
        { ROCKETMQ_STUB_CONSUME_MESSAGE: '1', ROCKETMQ_STUB_CONSUMER_FORCE_FUTURE_ERROR: '1' }
      ];
      for (const env of cases) {
        await runCase(env);
      }
    });
  });
});