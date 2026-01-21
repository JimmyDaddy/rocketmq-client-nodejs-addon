'use strict';

const test = require('node:test');
const assert = require('assert');
const path = require('path');
const { spawnSync } = require('child_process');

const { ensureBindingBinary } = require('./helpers/binding');

const rootDir = path.join(__dirname, '..');
process.env.NODE_BINDINGS_COMPILED_DIR = 'build';
ensureBindingBinary(rootDir);

const PushConsumer = require('../lib/push_consumer');

test.afterEach(() => {
  if (global.gc) {
    global.gc();
  }
});

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
  try {
    const result = fn();
    if (result && typeof result.then === 'function') {
      return result.finally(done);
    }
    done();
    return result;
  } catch (err) {
    done();
    throw err;
  }
}

function startWithCallback(consumer) {
  return new Promise((resolve, reject) => {
    consumer.start((err, ret) => {
      if (err) return reject(err);
      resolve(ret);
    });
  });
}

function shutdownWithCallback(consumer) {
  return new Promise((resolve, reject) => {
    consumer.shutdown((err, ret) => {
      if (err) return reject(err);
      resolve(ret);
    });
  });
}

test('constructor overloads and logLevel mapping', () => {
  const c1 = new PushConsumer('G1', { nameServer: '127.0.0.1', logLevel: 'debug' });
  const c2 = new PushConsumer('G2', 'INST', { logLevel: 'unknown' });
  const c3 = new PushConsumer('G3', 'INST', {});

  assert.strictEqual(c1.status, 0);
  assert.strictEqual(c2.status, 0);
  assert.strictEqual(c3.status, 0);
});

test('constructor options mapping', () => {
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

test('constructor logLevel boundary values', () => {
  const c1 = new PushConsumer('G1', { logLevel: -1 });
  const c2 = new PushConsumer('G2', { logLevel: 999 });
  assert.strictEqual(c1.status, 0);
  assert.strictEqual(c2.status, 0);
});

test('setListener replaces previous listener', () => {
  const consumer = new PushConsumer('G1', {});
  consumer.core.setListener(() => {});
  consumer.core.setListener(() => {});
  assert.strictEqual(consumer.status, 0);
});

test('setListener release throw is swallowed', () => {
  return withEnv({ ROCKETMQ_STUB_CONSUMER_RELEASE_THROW: '1' }, () => {
    const consumer = new PushConsumer('G1', {});
    consumer.core.setListener(() => {});
    assert.strictEqual(consumer.status, 0);
  });
});

test('setSessionCredentials validates input', () => {
  const consumer = new PushConsumer('G1', {});
  assert.strictEqual(consumer.setSessionCredentials('a', 'b', 'c'), true);
  assert.throws(() => consumer.setSessionCredentials('a', 1, 'c'), assert.AssertionError);
});

test('core method validation errors', () => {
  const consumer = new PushConsumer('G1', {});
  assert.throws(() => consumer.core.start(), /Function expected/);
  assert.throws(() => consumer.core.shutdown(1), /Function expected/);
  assert.throws(() => consumer.core.subscribe('TopicOnly'), /Wrong number of arguments/);
  assert.throws(() => consumer.core.subscribe('TopicOnly', 1), /Topic and expression must be strings/);
  assert.throws(() => consumer.core.setListener(123), /Function expected/);
  assert.throws(() => consumer.core.setSessionCredentials('a', 'b'), /Wrong number of arguments/);
  assert.throws(() => consumer.core.setSessionCredentials('a', 1, 'c'), /All arguments must be strings/);
});

test('subscribe supports topic and expression', () => {
  const consumer = new PushConsumer('G1', {});
  assert.doesNotThrow(() => consumer.subscribe('TopicA'));
  assert.doesNotThrow(() => consumer.subscribe('TopicA', 'TagA'));
});

test('subscribe state assertions', async () => {
  const consumer = new PushConsumer('G1', {});
  await consumer.start();
  assert.throws(() => consumer.subscribe('TopicB'), /STOPPED/);
  await consumer.shutdown();
});

test('subscribe propagates native errors', async () => {
  await withEnv({ ROCKETMQ_STUB_CONSUMER_SUBSCRIBE_ERROR: '1' }, async () => {
    const consumer = new PushConsumer('G1', {});
    assert.throws(() => consumer.subscribe('TopicC'), /consumer subscribe error/);
  });
});

test('start and shutdown promise path with message event', async () => {
  await withEnv({ ROCKETMQ_STUB_CONSUME_MESSAGE: '1' }, async () => {
    const consumer = new PushConsumer('G1', { nameServer: '127.0.0.1' });
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
  });
});

test('message ack false path', async () => {
  await withEnv({ ROCKETMQ_STUB_CONSUME_MESSAGE: '1' }, async () => {
    const consumer = new PushConsumer('G1', {});
    const seen = new Promise((resolve) => {
      consumer.once('message', (msg, ack) => {
        ack.done(false);
        resolve(msg);
      });
    });
    await consumer.start();
    await seen;
    await consumer.shutdown();
  });
});

test('message handler throws emits error and auto nacks', async () => {
  await withEnv({ ROCKETMQ_STUB_CONSUME_MESSAGE: '1' }, async () => {
    const consumer = new PushConsumer('G1', {});
    const errorPromise = new Promise((resolve) => {
      consumer.once('error', (err, msg, ack) => resolve({ err, msg, ack }));
    });
    consumer.once('message', () => {
      throw new Error('handler boom');
    });

    await Promise.race([
      consumer.start(),
      new Promise((_, reject) => setTimeout(() => reject(new Error('start timeout')), 1000))
    ]);

    const { err, msg, ack } = await Promise.race([
      errorPromise,
      new Promise((_, reject) => setTimeout(() => reject(new Error('error timeout')), 1000))
    ]);

    assert.match(err.message, /handler boom/);
    assert.ok(msg);
    assert.ok(ack);

    await consumer.shutdown();
  });
});

test('start and shutdown callback path with timer management', async () => {
  const originalSetInterval = global.setInterval;
  const originalClearInterval = global.clearInterval;
  let setCalls = 0;
  let clearCalls = 0;

  global.setInterval = () => {
    setCalls += 1;
    return { id: setCalls };
  };
  global.clearInterval = () => {
    clearCalls += 1;
  };

  try {
    const c1 = new PushConsumer('G1', {});
    const c2 = new PushConsumer('G2', {});
    await startWithCallback(c1);
    await startWithCallback(c2);
    assert.strictEqual(setCalls, 1);
    await shutdownWithCallback(c1);
    assert.strictEqual(clearCalls, 0);
    await shutdownWithCallback(c2);
    assert.strictEqual(clearCalls, 1);
  } finally {
    global.setInterval = originalSetInterval;
    global.clearInterval = originalClearInterval;
  }
});

test('start and shutdown error paths', async () => {
  await withEnv({ ROCKETMQ_STUB_CONSUMER_START_ERROR: '1' }, async () => {
    const consumer = new PushConsumer('G1', {});
    await assert.rejects(() => consumer.start(), /consumer start error/);
  });

  let consumer;
  await withEnv({ ROCKETMQ_STUB_CONSUMER_SHUTDOWN_ERROR: '1' }, async () => {
    consumer = new PushConsumer('G1', {});
    await consumer.start();
    await assert.rejects(() => consumer.shutdown(), /consumer shutdown error/);
  });
  if (consumer && consumer.status !== 0) {
    consumer.status = 1;
    await consumer.shutdown();
  }
});

test('destructor handles shutdown error', async () => {
  const script = `
    const { ensureBindingBinary } = require('./test/helpers/binding');
    const rootDir = process.cwd();
    process.env.NODE_BINDINGS_COMPILED_DIR = 'build';
    ensureBindingBinary(rootDir);
    const binding = require('./lib/binding');
    process.env.ROCKETMQ_STUB_CONSUMER_SHUTDOWN_ERROR = '1';
    if (!global.gc) process.exit(1);
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
    (async () => {
      for (let i = 0; i < 10; i += 1) {
        global.gc();
        await new Promise((resolve) => setImmediate(resolve));
      }
      await Promise.race([
        finalized,
        new Promise((_, reject) => setTimeout(() => reject(new Error('finalize timeout')), 1000))
      ]);
      process.exit(0);
    })().catch((err) => {
      console.error(err);
      process.exit(1);
    });
  `;
  const result = spawnSync(process.execPath, ['--expose-gc', '-e', script], {
    env: { ...process.env },
    stdio: 'inherit'
  });
  assert.strictEqual(result.status, 0);
});

test('native edge branches', () => {
  const script = `
    const { ensureBindingBinary } = require('./test/helpers/binding');
    const rootDir = process.cwd();
    process.env.NODE_BINDINGS_COMPILED_DIR = 'build';
    ensureBindingBinary(rootDir);
    const PushConsumer = require('./lib/push_consumer');

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
        const consumer = new PushConsumer('G1', {});
        if (process.env.ROCKETMQ_STUB_THROW_JS_LISTENER) {
          process.once('uncaughtException', () => {});
          consumer.core.setListener(() => {
            throw new Error('listener error');
          });
        }
        consumer.subscribe('TopicA', '*');
        await consumer.start();
        await consumer.shutdown();
      });
    }

    (async () => {
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
      process.exit(0);
    })().catch((err) => {
      console.error(err);
      process.exit(1);
    });
  `;

  const result = spawnSync(process.execPath, ['-e', script], {
    env: { ...process.env },
    stdio: 'inherit'
  });
  assert.strictEqual(result.status, 0);
});
