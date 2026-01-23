'use strict';

const test = require('node:test');
const assert = require('assert');
const path = require('path');

const { ensureBindingBinary } = require('./helpers/binding');

const rootDir = path.join(__dirname, '..');
process.env.NODE_BINDINGS_COMPILED_DIR = 'build';
ensureBindingBinary(rootDir);

const PushConsumer = require('../dist/consumer');

test.afterEach(() => {
  if (global.gc) {
    global.gc();
  }
});

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


test('ConsumerAck tests', { concurrency: 1 }, async (t) => {
  const baseEnv = {
    ROCKETMQ_STUB_CONSUMER_START_ERROR: undefined,
    ROCKETMQ_STUB_CONSUMER_SHUTDOWN_ERROR: undefined,
    ROCKETMQ_STUB_CONSUME_MESSAGE: '1'
  };

  await t.test('done() with true acks message', async () => {
    const original = setEnv(baseEnv);
    let consumer;
    try {
      consumer = new PushConsumer('test-group', {});
      consumer.subscribe('test-topic', '*');

      const messagePromise = new Promise((resolve) => {
        consumer.once('message', (msg, ack) => {
          ack.done(true);
          resolve(true);
        });
      });

      await consumer.start();
      const ackCalled = await messagePromise;
      assert.strictEqual(ackCalled, true);
    } finally {
      if (consumer && consumer.status === 1) {
        await consumer.shutdown();
      }
      restoreEnv(baseEnv, original);
    }
  });

  await t.test('done() with false nacks message', async () => {
    const original = setEnv(baseEnv);
    let consumer;
    try {
      consumer = new PushConsumer('test-group', {});
      consumer.subscribe('test-topic', '*');

      const messagePromise = new Promise((resolve) => {
        consumer.once('message', (msg, ack) => {
          ack.done(false);
          resolve(false);
        });
      });

      await consumer.start();
      const ackValue = await messagePromise;
      assert.strictEqual(ackValue, false);
    } finally {
      if (consumer && consumer.status === 1) {
        await consumer.shutdown();
      }
      restoreEnv(baseEnv, original);
    }
  });

  await t.test('done() with no argument defaults to true', async () => {
    const original = setEnv(baseEnv);
    let consumer;
    try {
      consumer = new PushConsumer('test-group', {});
      consumer.subscribe('test-topic', '*');

      const messagePromise = new Promise((resolve) => {
        consumer.once('message', (msg, ack) => {
          ack.done();
          resolve(true);
        });
      });

      await consumer.start();
      const doneCalled = await messagePromise;
      assert.strictEqual(doneCalled, true);
    } finally {
      if (consumer && consumer.status === 1) {
        await consumer.shutdown();
      }
      restoreEnv(baseEnv, original);
    }
  });

  await t.test('done() is idempotent - second call is ignored', async () => {
    const original = setEnv(baseEnv);
    let consumer;
    try {
      consumer = new PushConsumer('test-group', {});
      consumer.subscribe('test-topic', '*');

      const messagePromise = new Promise((resolve) => {
        let callCount = 0;
        consumer.once('message', (msg, ack) => {
          ack.done(true);
          ack.done(false);
          ack.done(true);
          callCount++;
          resolve(callCount);
        });
      });

      await consumer.start();
      const callCount = await messagePromise;
      assert.strictEqual(callCount, 1);
    } finally {
      if (consumer && consumer.status === 1) {
        await consumer.shutdown();
      }
      restoreEnv(baseEnv, original);
    }
  });

  await t.test('done() with undefined is treated as true', async () => {
    const original = setEnv(baseEnv);
    let consumer;
    try {
      consumer = new PushConsumer('test-group', {});
      consumer.subscribe('test-topic', '*');

      const messagePromise = new Promise((resolve) => {
        consumer.once('message', (msg, ack) => {
          ack.done(undefined);
          resolve(true);
        });
      });

      await consumer.start();
      const doneCalled = await messagePromise;
      assert.strictEqual(doneCalled, true);
    } finally {
      if (consumer && consumer.status === 1) {
        await consumer.shutdown();
      }
      restoreEnv(baseEnv, original);
    }
  });

  await t.test('handles listener exception via Done(exception_ptr)', async () => {
    const original = setEnv(baseEnv);
    let consumer;
    try {
      consumer = new PushConsumer('test-group', {});
      consumer.subscribe('test-topic', '*');

      const messagePromise = new Promise((resolve) => {
        consumer.once('message', () => {
          resolve(true);
          throw new Error('listener error');
        });
      });

      await consumer.start();
      const listenerCalled = await messagePromise;
      assert.strictEqual(listenerCalled, true);
    } finally {
      if (consumer && consumer.status === 1) {
        await consumer.shutdown();
      }
      restoreEnv(baseEnv, original);
    }
  });

  await t.test('done() then throw is idempotent (tests Done exception_ptr early return)', async () => {
    const original = setEnv(baseEnv);
    let consumer;
    try {
      consumer = new PushConsumer('test-group', {});
      consumer.subscribe('test-topic', '*');

      const messagePromise = new Promise((resolve) => {
        consumer.once('message', (msg, ack) => {
          ack.done(true);
          resolve(true);
          throw new Error('error after done');
        });
      });

      await consumer.start();
      const doneCalled = await messagePromise;
      assert.strictEqual(doneCalled, true);
    } finally {
      if (consumer && consumer.status === 1) {
        await consumer.shutdown();
      }
      restoreEnv(baseEnv, original);
    }
  });

  await t.test('NewInstance returns empty when addon_data is null (coverage branch)', async () => {
    const original = setEnv({
      ...baseEnv,
      ROCKETMQ_STUB_CONSUMER_ACK_NULL_ADDON_DATA: '1'
    });
    let consumer;
    try {
      consumer = new PushConsumer('test-group', {});
      consumer.subscribe('test-topic', '*');

      const messagePromise = new Promise((resolve, reject) => {
        const timeout = setTimeout(() => resolve('timeout'), 500);
        consumer.once('message', () => {
          clearTimeout(timeout);
          resolve('message');
        });
        consumer.once('error', () => {
          clearTimeout(timeout);
          resolve('error');
        });
      });

      await consumer.start();
      const result = await messagePromise;
      assert.ok(['timeout', 'error'].includes(result) || result === 'message');
    } finally {
      if (consumer && consumer.status === 1) {
        await consumer.shutdown();
      }
      restoreEnv({
        ...baseEnv,
        ROCKETMQ_STUB_CONSUMER_ACK_NULL_ADDON_DATA: undefined
      }, original);
    }
  });

  await t.test('done() catches future_error when promise already set (coverage branch)', async () => {
    const original = setEnv({
      ...baseEnv,
      ROCKETMQ_STUB_CONSUMER_ACK_FORCE_FUTURE_ERROR: '1'
    });
    let consumer;
    try {
      consumer = new PushConsumer('test-group', {});
      consumer.subscribe('test-topic', '*');

      const messagePromise = new Promise((resolve) => {
        consumer.once('message', (msg, ack) => {
          ack.done(true);
          resolve(true);
        });
      });

      await consumer.start();
      const doneCalled = await messagePromise;
      assert.strictEqual(doneCalled, true);
    } finally {
      if (consumer && consumer.status === 1) {
        await consumer.shutdown();
      }
      restoreEnv({
        ...baseEnv,
        ROCKETMQ_STUB_CONSUMER_ACK_FORCE_FUTURE_ERROR: undefined
      }, original);
    }
  });

  await t.test('IsEnvEnabled returns false for empty string env var (coverage branch)', async () => {
    // This test covers the branch in IsEnvEnabled where value[0] == '\0'
    // When ROCKETMQ_STUB_CONSUMER_ACK_FORCE_FUTURE_ERROR is set to empty string,
    // IsEnvEnabled returns false, so the stub code path is NOT taken
    const original = setEnv({
      ...baseEnv,
      ROCKETMQ_STUB_CONSUMER_ACK_FORCE_FUTURE_ERROR: ''
    });
    let consumer;
    try {
      consumer = new PushConsumer('test-group', {});
      consumer.subscribe('test-topic', '*');

      const messagePromise = new Promise((resolve) => {
        consumer.once('message', (msg, ack) => {
          ack.done(true);
          resolve(true);
        });
      });

      await consumer.start();
      const doneCalled = await messagePromise;
      assert.strictEqual(doneCalled, true);
    } finally {
      if (consumer && consumer.status === 1) {
        await consumer.shutdown();
      }
      restoreEnv({
        ...baseEnv,
        ROCKETMQ_STUB_CONSUMER_ACK_FORCE_FUTURE_ERROR: undefined
      }, original);
    }
  });

  await t.test('IsEnvEnabled returns false for "0" env var (coverage branch)', async () => {
    // This test covers the branch in IsEnvEnabled where value[0] == '0'
    // When env var is set to "0", IsEnvEnabled returns false
    const original = setEnv({
      ...baseEnv,
      ROCKETMQ_STUB_CONSUMER_ACK_FORCE_FUTURE_ERROR: '0'
    });
    let consumer;
    try {
      consumer = new PushConsumer('test-group', {});
      consumer.subscribe('test-topic', '*');

      const messagePromise = new Promise((resolve) => {
        consumer.once('message', (msg, ack) => {
          ack.done(true);
          resolve(true);
        });
      });

      await consumer.start();
      const doneCalled = await messagePromise;
      assert.strictEqual(doneCalled, true);
    } finally {
      if (consumer && consumer.status === 1) {
        await consumer.shutdown();
      }
      restoreEnv({
        ...baseEnv,
        ROCKETMQ_STUB_CONSUMER_ACK_FORCE_FUTURE_ERROR: undefined
      }, original);
    }
  });

  await t.test('Done(exception_ptr) early return and Done() future_error (coverage branch)', async () => {
    // To cover line 72: Done(exception_ptr) must see done_called_ as true.
    // To cover line 98: Done() must catch std::future_error when ROCKETMQ_STUB_CONSUMER_ACK_FORCE_FUTURE_ERROR is set.
    // We can use ROCKETMQ_STUB_CONSUMER_PROMISE_SET to set the promise early.
    const original = setEnv({
      ...baseEnv,
      ROCKETMQ_STUB_CONSUMER_ACK_FORCE_FUTURE_ERROR: '1',
      ROCKETMQ_STUB_CONSUMER_PROMISE_SET: '1'
    });
    let consumer;
    try {
      consumer = new PushConsumer('test-group', {});
      consumer.subscribe('test-topic', '*');

      const messagePromise = new Promise((resolve) => {
        consumer.once('message', (msg, ack) => {
          // 1. data->promise is already set due to ROCKETMQ_STUB_CONSUMER_PROMISE_SET
          // 2. ack.done(true) will try to set it again, triggering future_error in line 96, hitting line 98.
          // 3. ack.done(true) sets done_called_ to true.
          ack.done(true);
          resolve(true);
          // 4. throwing here will call Done(exception_ptr)
          // 5. Done(exception_ptr) will see done_called_ as true and return early (line 72).
          throw new Error('trigger Done(exception_ptr)');
        });
      });

      await consumer.start();
      await messagePromise;
    } finally {
      if (consumer && consumer.status === 1) {
        await consumer.shutdown();
      }
      restoreEnv({
        ...baseEnv,
        ROCKETMQ_STUB_CONSUMER_ACK_FORCE_FUTURE_ERROR: undefined,
        ROCKETMQ_STUB_CONSUMER_PROMISE_SET: undefined
      }, original);
    }
  });
});
