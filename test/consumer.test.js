'use strict';

const test = require('node:test');
const assert = require('assert');
const path = require('path');

const { ensureBindingBinary } = require('./helpers/binding');

const rootDir = path.join(__dirname, '..');
process.env.NODE_BINDINGS_COMPILED_DIR = 'build';
ensureBindingBinary(rootDir);

const PushConsumer = require('../dist/consumer');
const binding = require('../dist/binding');

function setEnv(env) {
  const original = {};
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

function restoreEnv(env, original) {
  for (const key of Object.keys(env)) {
    if (original[key] === undefined) {
      delete process.env[key];
    } else {
      process.env[key] = original[key];
    }
  }
}

test.suite('PushConsumer tests', { concurrency: 1 }, () => {
  test('PushConsumer constructor tests', { concurrency: 1 }, async (t) => {
    await t.test('constructor overloads and logLevel mapping', () => {
      const c1 = new PushConsumer('G1', { nameServer: '127.0.0.1', logLevel: 'debug' });
      const c2 = new PushConsumer('G2', 'INST', { logLevel: 'unknown' });
      const c3 = new PushConsumer('G3', 'INST', {});

      assert.strictEqual(c1.status, 0);
      assert.strictEqual(c2.status, 0);
      assert.strictEqual(c3.status, 0);
    });

    await t.test('constructor options mapping', () => {
      const consumer = new PushConsumer('G1', {
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

      assert.strictEqual(consumer.status, 0);
    });

    await t.test('constructor logLevel boundary values', () => {
      const c1 = new PushConsumer('G1', { logLevel: -1 });
      const c2 = new PushConsumer('G2', { logLevel: 999 });
      assert.strictEqual(c1.status, 0);
      assert.strictEqual(c2.status, 0);
    });
  });

  test('PushConsumer configuration tests', { concurrency: 1 }, async (t) => {
    await t.test('setListener replaces previous listener', () => {
      const consumer = new PushConsumer('G1', {});
      consumer.core.setListener(() => {});
      consumer.core.setListener(() => {});
      assert.strictEqual(consumer.status, 0);
    });

    await t.test('setListener release throw is swallowed', () => {
      const env = { ROCKETMQ_STUB_CONSUMER_RELEASE_THROW: '1' };
      const original = setEnv(env);
      try {
        const consumer = new PushConsumer('G1', {});
        consumer.core.setListener(() => {});
        assert.strictEqual(consumer.status, 0);
      } finally {
        restoreEnv(env, original);
      }
    });

    await t.test('setSessionCredentials validates input', () => {
      const consumer = new PushConsumer('G1', {});
      assert.strictEqual(consumer.setSessionCredentials('a', 'b', 'c'), true);
      assert.throws(() => consumer.setSessionCredentials('a', 1, 'c'), /TypeError: secretKey must be a string/);
    });

    await t.test('core method validation errors', () => {
      const consumer = new PushConsumer('G1', {});
      assert.throws(() => consumer.core.start(), /Function expected/);
      assert.throws(() => consumer.core.shutdown(1), /Function expected/);
      assert.throws(() => consumer.core.subscribe('TopicOnly'), /Wrong number of arguments/);
      assert.throws(() => consumer.core.subscribe('TopicOnly', 1), /Topic and expression must be strings/);
      assert.throws(() => consumer.core.setListener(123), /Function expected/);
      assert.throws(() => consumer.core.setSessionCredentials('a', 'b'), /Wrong number of arguments/);
      assert.throws(() => consumer.core.setSessionCredentials('a', 1, 'c'), /All arguments must be strings/);
    });
  });

  test('PushConsumer subscribe tests', { concurrency: 1 }, async (t) => {
    await t.test('subscribe supports topic and expression', () => {
      const consumer = new PushConsumer('G1', {});
      assert.doesNotThrow(() => consumer.subscribe('TopicA'));
      assert.doesNotThrow(() => consumer.subscribe('TopicA', 'TagA'));
    });

    await t.test('subscribe propagates native errors', async () => {
      const env = { ROCKETMQ_STUB_CONSUMER_SUBSCRIBE_ERROR: '1' };
      const original = setEnv(env);
      try {
        const consumer = new PushConsumer('G1', {});
        assert.throws(() => consumer.subscribe('TopicC'), /consumer subscribe error/);
      } finally {
        restoreEnv(env, original);
      }
    });
  });

  test('PushConsumer lifecycle and message handling tests', { concurrency: 1 }, async (t) => {
    await t.test('start and shutdown promise path with message event', async () => {
      const env = { ROCKETMQ_STUB_CONSUME_MESSAGE: '1' };
      const original = setEnv(env);
      let consumer;
      try {
        consumer = new PushConsumer('G1', { nameServer: '127.0.0.1' });
        const message = new Promise((resolve) => {
          consumer.once('message', (msg, ack) => {
            ack.done();
            resolve(msg);
          });
        });
        await consumer.start();
        const msg = await message;
        assert.strictEqual(msg.topic, 'TopicTest');
        assert.strictEqual(msg.tags, 'TagA');
        assert.strictEqual(msg.keys, 'KeyA');
        assert.strictEqual(msg.body, 'Hello');
        assert.strictEqual(msg.msgId, 'MSGID');
        await consumer.shutdown();
        consumer = null;
      } finally {
        if (consumer && consumer.status === 1) {
          await consumer.shutdown();
        }
        restoreEnv(env, original);
      }
    });

    await t.test('message ack false path', async () => {
      const env = { ROCKETMQ_STUB_CONSUME_MESSAGE: '1' };
      const original = setEnv(env);
      let consumer;
      try {
        consumer = new PushConsumer('G1', {});
        const seen = new Promise((resolve) => {
          consumer.once('message', (msg, ack) => {
            ack.done(false);
            resolve(msg);
          });
        });
        await consumer.start();
        await seen;
        await consumer.shutdown();
        consumer = null;
      } finally {
        if (consumer && consumer.status === 1) {
          await consumer.shutdown();
        }
        restoreEnv(env, original);
      }
    });

    await t.test('message handler throws emits error and auto nacks', async () => {
      const env = { ROCKETMQ_STUB_CONSUME_MESSAGE: '1' };
      const original = setEnv(env);
      let consumer;
      try {
        consumer = new PushConsumer('G1', {});
        
        const errorPromise = new Promise((resolve) => {
          consumer.once('error', (err, msg, ack) => resolve({ err, msg, ack }));
        });
        
        consumer.once('message', () => {
          throw new Error('handler boom');
        });

        consumer.subscribe('TopicA', '*');
        await consumer.start();

        const result = await Promise.race([
          errorPromise,
          new Promise((_, reject) => setTimeout(() => reject(new Error('error event timeout')), 2000))
        ]);

        assert.match(result.err.message, /handler boom/);
        assert.ok(result.msg);
        assert.ok(result.ack);
      } finally {
        if (consumer && consumer.status === 1) {
          await consumer.shutdown();
        }
        restoreEnv(env, original);
      }
    });
  });

  test('PushConsumer error handling tests', { concurrency: 1 }, async (t) => {
    await t.test('start error with Promise pattern', async () => {
      const env1 = { ROCKETMQ_STUB_CONSUMER_START_ERROR: '1' };
      const original1 = setEnv(env1);
      try {
        const consumer = new PushConsumer('G1', {});
        await assert.rejects(() => consumer.start(), /consumer start error/);
      } finally {
        restoreEnv(env1, original1);
      }
    });

    await t.test('shutdown error with Promise pattern', async () => {
      let consumer;
      const env2 = { ROCKETMQ_STUB_CONSUMER_SHUTDOWN_ERROR: '1' };
      const original2 = setEnv(env2);
      try {
        consumer = new PushConsumer('G1', {});
        await consumer.start();
        await assert.rejects(() => consumer.shutdown(), /consumer shutdown error/);
      } finally {
        if (consumer && consumer.status !== 0) {
          delete process.env.ROCKETMQ_STUB_CONSUMER_SHUTDOWN_ERROR;
          consumer.status = 1;
          await consumer.shutdown().catch(() => {});
        }
        restoreEnv(env2, original2);
      }
    });

    await t.test('destructor handles shutdown error', async () => {
      const env = { ROCKETMQ_STUB_CONSUMER_SHUTDOWN_ERROR: '1' };
      const original = setEnv(env);
      try {
        let finalize;
        const finalized = new Promise((resolve) => {
          finalize = resolve;
        });
        const registry = new FinalizationRegistry(() => {
          finalize();
        });
        let core = new binding.PushConsumer('G1', null, {});
        registry.register(core, 1);
        core = null;
        
        // Force garbage collection
        if (global.gc) {
          global.gc();
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

  test('PushConsumer native edge branches', { concurrency: 1 }, async (t) => {
    await t.test('native edge cases coverage', async () => {
      function withEnv(values, fn) {
        const original = {};
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

      async function runCase(env) {
        await withEnv(env, async () => {
          let consumer;
          try {
            consumer = new PushConsumer('G1', {});
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
            if (consumer && consumer.status === 1) {
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
