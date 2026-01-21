'use strict';

const test = require('node:test');
const assert = require('assert');
const path = require('path');

const { ensureBindingBinary } = require('./helpers/binding');

const rootDir = path.join(__dirname, '..');
process.env.NODE_BINDINGS_COMPILED_DIR = 'build';
ensureBindingBinary(rootDir);

const RocketMQProducer = require('../lib/producer');

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

test('Producer constructor tests', { concurrency: 1 }, async (t) => {
  await t.test('with groupId only', () => {
    const producer = new RocketMQProducer('test-group');
    assert.ok(producer);
    assert.strictEqual(producer.status, 0);
  });

  await t.test('with groupId and options', () => {
    const producer = new RocketMQProducer('test-group', {
      nameServer: 'localhost:9876',
      logLevel: 'DEBUG'
    });
    assert.ok(producer);
  });

  await t.test('with groupId, instanceName, and options', () => {
    const producer = new RocketMQProducer('test-group', 'instance-1', {
      nameServer: 'localhost:9876'
    });
    assert.ok(producer);
  });

  await t.test('logLevel string to number conversion', () => {
    const producer = new RocketMQProducer('test-group', { logLevel: 'error' });
    assert.ok(producer);
  });
});

test('Producer setSessionCredentials tests', { concurrency: 1 }, async (t) => {
  await t.test('with valid args', () => {
    const producer = new RocketMQProducer('test-group');
    const result = producer.setSessionCredentials('accessKey', 'secretKey', 'channel');
    assert.strictEqual(result, true);
  });

  await t.test('throws on invalid accessKey', () => {
    const producer = new RocketMQProducer('test-group');
    assert.throws(() => {
      producer.setSessionCredentials(123, 'secretKey', 'channel');
    }, /AssertionError/);
  });

  await t.test('throws on invalid secretKey', () => {
    const producer = new RocketMQProducer('test-group');
    assert.throws(() => {
      producer.setSessionCredentials('accessKey', 123, 'channel');
    }, /AssertionError/);
  });

  await t.test('throws on invalid channel', () => {
    const producer = new RocketMQProducer('test-group');
    assert.throws(() => {
      producer.setSessionCredentials('accessKey', 'secretKey', 123);
    }, /AssertionError/);
  });
});

test('Producer start/shutdown lifecycle tests', { concurrency: 1 }, async (t) => {
  const baseEnv = {
    ROCKETMQ_STUB_PRODUCER_START_ERROR: undefined,
    ROCKETMQ_STUB_PRODUCER_SHUTDOWN_ERROR: undefined
  };

  await t.test('start() with Promise pattern', async () => {
    const original = setEnv(baseEnv);
    let producer;
    try {
      producer = new RocketMQProducer('test-group');
      await producer.start();
      assert.strictEqual(producer.status, 1);
    } finally {
      if (producer && producer.status === 1) {
        await producer.shutdown();
      }
      restoreEnv(baseEnv, original);
    }
  });

  await t.test('start() with callback pattern', async () => {
    const original = setEnv(baseEnv);
    let producer;
    try {
      producer = new RocketMQProducer('test-group');
      await new Promise((resolve, reject) => {
        producer.start((err) => {
          if (err) return reject(err);
          resolve();
        });
      });
      assert.strictEqual(producer.status, 1);
    } finally {
      if (producer && producer.status === 1) {
        await producer.shutdown();
      }
      restoreEnv(baseEnv, original);
    }
  });

  await t.test('start() error with Promise pattern', async () => {
    const env = { ROCKETMQ_STUB_PRODUCER_START_ERROR: '1' };
    const original = setEnv(env);
    try {
      const producer = new RocketMQProducer('test-group');
      await assert.rejects(async () => {
        await producer.start();
      }, /producer start error/);
    } finally {
      restoreEnv(env, original);
    }
  });

  await t.test('start() error with callback pattern', async () => {
    const env = { ROCKETMQ_STUB_PRODUCER_START_ERROR: '1' };
    const original = setEnv(env);
    try {
      const producer = new RocketMQProducer('test-group');
      await new Promise((resolve) => {
        producer.start((err) => {
          assert.ok(err);
          assert.match(err.message, /producer start error/);
          resolve();
        });
      });
    } finally {
      restoreEnv(env, original);
    }
  });

  await t.test('shutdown() with Promise pattern', async () => {
    const original = setEnv(baseEnv);
    let producer;
    try {
      producer = new RocketMQProducer('test-group');
      await producer.start();
      await producer.shutdown();
      assert.strictEqual(producer.status, 0);
      producer = null;
    } finally {
      if (producer && producer.status === 1) {
        await producer.shutdown();
      }
      restoreEnv(baseEnv, original);
    }
  });

  await t.test('shutdown() with callback pattern', async () => {
    const original = setEnv(baseEnv);
    let producer;
    try {
      producer = new RocketMQProducer('test-group');
      await producer.start();
      await new Promise((resolve, reject) => {
        producer.shutdown((err) => {
          if (err) return reject(err);
          resolve();
        });
      });
      assert.strictEqual(producer.status, 0);
      producer = null;
    } finally {
      if (producer && producer.status === 1) {
        await producer.shutdown();
      }
      restoreEnv(baseEnv, original);
    }
  });

  await t.test('shutdown() error with Promise pattern', async () => {
    const original = setEnv(baseEnv);
    let producer;
    try {
      producer = new RocketMQProducer('test-group');
      await producer.start();
      const errEnv = { ROCKETMQ_STUB_PRODUCER_SHUTDOWN_ERROR: '1' };
      const errOriginal = setEnv(errEnv);
      try {
        await assert.rejects(async () => {
          await producer.shutdown();
        }, /producer shutdown error/);
      } finally {
        restoreEnv(errEnv, errOriginal);
      }
      producer.status = 1;
      await producer.shutdown();
      producer = null;
    } finally {
      if (producer && producer.status === 1) {
        await producer.shutdown();
      }
      restoreEnv(baseEnv, original);
    }
  });

  await t.test('status transitions STOPPED -> STARTING -> STARTED', async () => {
    const original = setEnv(baseEnv);
    let producer;
    try {
      producer = new RocketMQProducer('test-group');
      assert.strictEqual(producer.status, 0);
      const promise = producer.start();
      assert.strictEqual(producer.status, 3);
      await promise;
      assert.strictEqual(producer.status, 1);
    } finally {
      if (producer && producer.status === 1) {
        await producer.shutdown();
      }
      restoreEnv(baseEnv, original);
    }
  });

  await t.test('status transitions STARTED -> STOPPING -> STOPPED', async () => {
    const original = setEnv(baseEnv);
    let producer;
    try {
      producer = new RocketMQProducer('test-group');
      await producer.start();
      assert.strictEqual(producer.status, 1);
      const promise = producer.shutdown();
      assert.strictEqual(producer.status, 2);
      await promise;
      assert.strictEqual(producer.status, 0);
      producer = null;
    } finally {
      if (producer && producer.status === 1) {
        await producer.shutdown();
      }
      restoreEnv(baseEnv, original);
    }
  });

  await t.test('start() throws when not STOPPED', async () => {
    const original = setEnv(baseEnv);
    let producer;
    try {
      producer = new RocketMQProducer('test-group');
      await producer.start();
      assert.throws(() => {
        producer.start();
      }, /AssertionError/);
    } finally {
      if (producer && producer.status === 1) {
        await producer.shutdown();
      }
      restoreEnv(baseEnv, original);
    }
  });

  await t.test('shutdown() throws when not STARTED', async () => {
    const original = setEnv(baseEnv);
    try {
      const producer = new RocketMQProducer('test-group');
      assert.throws(() => {
        producer.shutdown();
      }, /AssertionError/);
    } finally {
      restoreEnv(baseEnv, original);
    }
  });

  await t.test('multiple start/shutdown cycles', async () => {
    const original = setEnv(baseEnv);
    let producer;
    try {
      producer = new RocketMQProducer('test-group');
      await producer.start();
      await producer.shutdown();
      await producer.start();
      await producer.shutdown();
      producer = null;
    } finally {
      if (producer && producer.status === 1) {
        await producer.shutdown();
      }
      restoreEnv(baseEnv, original);
    }
  });

  await t.test('multiple producers with producerRef counting', async () => {
    const original = setEnv(baseEnv);
    let producer1, producer2;
    try {
      producer1 = new RocketMQProducer('group1');
      producer2 = new RocketMQProducer('group2');
      await producer1.start();
      await producer2.start();
      await producer1.shutdown();
      producer1 = null;
      await producer2.shutdown();
      producer2 = null;
    } finally {
      if (producer1 && producer1.status === 1) {
        await producer1.shutdown();
      }
      if (producer2 && producer2.status === 1) {
        await producer2.shutdown();
      }
      restoreEnv(baseEnv, original);
    }
  });
});

test('Producer send tests', { concurrency: 1 }, async (t) => {
  const baseEnv = {
    ROCKETMQ_STUB_PRODUCER_START_ERROR: undefined,
    ROCKETMQ_STUB_PRODUCER_SHUTDOWN_ERROR: undefined,
    ROCKETMQ_STUB_SEND_EXCEPTION: undefined,
    ROCKETMQ_STUB_SEND_THROW: undefined
  };

  await t.test('with string body using Promise', async () => {
    const original = setEnv(baseEnv);
    let producer;
    try {
      producer = new RocketMQProducer('test-group');
      await producer.start();
      const result = await producer.send('test-topic', 'hello world');
      assert.strictEqual(result.status, 0);
      assert.strictEqual(result.statusStr, 'OK');
      assert.ok(result.msgId);
    } finally {
      if (producer && producer.status === 1) {
        await producer.shutdown();
      }
      restoreEnv(baseEnv, original);
    }
  });

  await t.test('with Buffer body using Promise', async () => {
    const original = setEnv(baseEnv);
    let producer;
    try {
      producer = new RocketMQProducer('test-group');
      await producer.start();
      const result = await producer.send('test-topic', Buffer.from('hello buffer'));
      assert.strictEqual(result.status, 0);
    } finally {
      if (producer && producer.status === 1) {
        await producer.shutdown();
      }
      restoreEnv(baseEnv, original);
    }
  });

  await t.test('with callback pattern', async () => {
    const original = setEnv(baseEnv);
    let producer;
    try {
      producer = new RocketMQProducer('test-group');
      await producer.start();
      await new Promise((resolve, reject) => {
        producer.send('test-topic', 'hello', (err, result) => {
          if (err) return reject(err);
          assert.strictEqual(result.status, 0);
          resolve();
        });
      });
    } finally {
      if (producer && producer.status === 1) {
        await producer.shutdown();
      }
      restoreEnv(baseEnv, original);
    }
  });

  await t.test('with options', async () => {
    const original = setEnv(baseEnv);
    let producer;
    try {
      producer = new RocketMQProducer('test-group');
      await producer.start();
      const result = await producer.send('test-topic', 'hello', { tags: 'tagA', keys: 'key1' });
      assert.strictEqual(result.status, 0);
    } finally {
      if (producer && producer.status === 1) {
        await producer.shutdown();
      }
      restoreEnv(baseEnv, original);
    }
  });

  await t.test('with options and callback', async () => {
    const original = setEnv(baseEnv);
    let producer;
    try {
      producer = new RocketMQProducer('test-group');
      await producer.start();
      await new Promise((resolve, reject) => {
        producer.send('test-topic', 'hello', { tags: 'tagA' }, (err, result) => {
          if (err) return reject(err);
          assert.strictEqual(result.status, 0);
          resolve();
        });
      });
    } finally {
      if (producer && producer.status === 1) {
        await producer.shutdown();
      }
      restoreEnv(baseEnv, original);
    }
  });

  await t.test('with empty string body returns EMPTY_BODY', async () => {
    const original = setEnv(baseEnv);
    let producer;
    try {
      producer = new RocketMQProducer('test-group');
      await producer.start();
      const result = await producer.send('test-topic', '');
      assert.strictEqual(result.status, -1);
      assert.strictEqual(result.statusStr, 'EMPTY_BODY');
    } finally {
      if (producer && producer.status === 1) {
        await producer.shutdown();
      }
      restoreEnv(baseEnv, original);
    }
  });

  await t.test('with empty Buffer body returns EMPTY_BODY', async () => {
    const original = setEnv(baseEnv);
    let producer;
    try {
      producer = new RocketMQProducer('test-group');
      await producer.start();
      const result = await producer.send('test-topic', Buffer.alloc(0));
      assert.strictEqual(result.status, -1);
      assert.strictEqual(result.statusStr, 'EMPTY_BODY');
    } finally {
      if (producer && producer.status === 1) {
        await producer.shutdown();
      }
      restoreEnv(baseEnv, original);
    }
  });

  await t.test('empty body with callback', async () => {
    const original = setEnv(baseEnv);
    let producer;
    try {
      producer = new RocketMQProducer('test-group');
      await producer.start();
      await new Promise((resolve) => {
        producer.send('test-topic', '', (err, result) => {
          assert.strictEqual(err, null);
          assert.strictEqual(result.status, -1);
          assert.strictEqual(result.statusStr, 'EMPTY_BODY');
          resolve();
        });
      });
    } finally {
      if (producer && producer.status === 1) {
        await producer.shutdown();
      }
      restoreEnv(baseEnv, original);
    }
  });

  await t.test('error with Promise pattern (onException)', async () => {
    const original = setEnv(baseEnv);
    let producer;
    try {
      producer = new RocketMQProducer('test-group');
      await producer.start();
      const errEnv = { ROCKETMQ_STUB_SEND_EXCEPTION: '1' };
      const errOriginal = setEnv(errEnv);
      try {
        await assert.rejects(async () => {
          await producer.send('test-topic', 'hello');
        }, /producer send exception/);
      } finally {
        restoreEnv(errEnv, errOriginal);
      }
    } finally {
      if (producer && producer.status === 1) {
        await producer.shutdown();
      }
      restoreEnv(baseEnv, original);
    }
  });

  await t.test('error with callback pattern', async () => {
    const original = setEnv(baseEnv);
    let producer;
    try {
      producer = new RocketMQProducer('test-group');
      await producer.start();
      const errEnv = { ROCKETMQ_STUB_SEND_EXCEPTION: '1' };
      const errOriginal = setEnv(errEnv);
      try {
        await new Promise((resolve) => {
          producer.send('test-topic', 'hello', (err) => {
            assert.ok(err);
            assert.match(err.message, /producer send exception/);
            resolve();
          });
        });
      } finally {
        restoreEnv(errEnv, errOriginal);
      }
    } finally {
      if (producer && producer.status === 1) {
        await producer.shutdown();
      }
      restoreEnv(baseEnv, original);
    }
  });

  await t.test('throws on invalid topic', async () => {
    const original = setEnv(baseEnv);
    let producer;
    try {
      producer = new RocketMQProducer('test-group');
      await producer.start();
      assert.throws(() => {
        producer.send(123, 'body');
      }, /AssertionError/);
    } finally {
      if (producer && producer.status === 1) {
        await producer.shutdown();
      }
      restoreEnv(baseEnv, original);
    }
  });

  await t.test('throws on invalid body type', async () => {
    const original = setEnv(baseEnv);
    let producer;
    try {
      producer = new RocketMQProducer('test-group');
      await producer.start();
      assert.throws(() => {
        producer.send('topic', 123);
      }, /AssertionError/);
    } finally {
      if (producer && producer.status === 1) {
        await producer.shutdown();
      }
      restoreEnv(baseEnv, original);
    }
  });
});

test('Producer static properties', { concurrency: 1 }, async (t) => {
  await t.test('SEND_RESULT static property', () => {
    assert.strictEqual(RocketMQProducer.SEND_RESULT.OK, 0);
    assert.strictEqual(RocketMQProducer.SEND_RESULT.FLUSH_DISK_TIMEOUT, 1);
    assert.strictEqual(RocketMQProducer.SEND_RESULT.FLUSH_SLAVE_TIMEOUT, 2);
    assert.strictEqual(RocketMQProducer.SEND_RESULT.SLAVE_NOT_AVAILABLE, 3);
  });
});

test('Producer C++ binding coverage tests', { concurrency: 1 }, async (t) => {
  const binding = require('../lib/binding');
  const baseEnv = {
    ROCKETMQ_STUB_PRODUCER_START_ERROR: undefined,
    ROCKETMQ_STUB_PRODUCER_SHUTDOWN_ERROR: undefined,
    ROCKETMQ_STUB_SEND_EXCEPTION: undefined,
    ROCKETMQ_STUB_SEND_THROW: undefined
  };

  await t.test('constructor with all options (covers SetOptions branches)', () => {
    const producer = new binding.Producer('test-group', 'instance-1', {
      nameServer: 'localhost:9876',
      groupName: 'override-group',
      maxMessageSize: 1024 * 1024,
      compressLevel: 5,
      sendMessageTimeout: 3000,
      logLevel: 3,
      logDir: '/tmp/logs',
      logFileSize: 1024 * 1024 * 10,
      logFileNum: 5
    });
    assert.ok(producer);
  });

  await t.test('constructor with logLevel boundary values', () => {
    const producerLow = new binding.Producer('test-group', 'instance-1', {
      logLevel: -1
    });
    const producerHigh = new binding.Producer('test-group', 'instance-1', {
      logLevel: 999
    });
    assert.ok(producerLow);
    assert.ok(producerHigh);
  });

  await t.test('setSessionCredentials with non-string accessKey throws', () => {
    const producer = new binding.Producer('test-group');
    assert.throws(() => {
      producer.setSessionCredentials(123, 'secret', 'channel');
    }, /All arguments must be strings/);
  });

  await t.test('setSessionCredentials with non-string secretKey throws', () => {
    const producer = new binding.Producer('test-group');
    assert.throws(() => {
      producer.setSessionCredentials('access', 123, 'channel');
    }, /All arguments must be strings/);
  });

  await t.test('setSessionCredentials with non-string channel throws', () => {
    const producer = new binding.Producer('test-group');
    assert.throws(() => {
      producer.setSessionCredentials('access', 'secret', 123);
    }, /All arguments must be strings/);
  });

  await t.test('setSessionCredentials with too few arguments throws', () => {
    const producer = new binding.Producer('test-group');
    assert.throws(() => {
      producer.setSessionCredentials('access', 'secret');
    }, /Wrong number of arguments/);
  });

  await t.test('start without callback throws', () => {
    const producer = new binding.Producer('test-group');
    assert.throws(() => {
      producer.start();
    }, /Function expected/);
  });

  await t.test('start with non-function throws', () => {
    const producer = new binding.Producer('test-group');
    assert.throws(() => {
      producer.start('not a function');
    }, /Function expected/);
  });

  await t.test('shutdown without callback throws', () => {
    const producer = new binding.Producer('test-group');
    assert.throws(() => {
      producer.shutdown();
    }, /Function expected/);
  });

  await t.test('shutdown with non-function throws', () => {
    const producer = new binding.Producer('test-group');
    assert.throws(() => {
      producer.shutdown('not a function');
    }, /Function expected/);
  });

  await t.test('send with too few arguments throws', () => {
    const original = setEnv(baseEnv);
    try {
      const producer = new binding.Producer('test-group');
      assert.throws(() => {
        producer.send('topic', 'body', {});
      }, /Wrong number of arguments/);
    } finally {
      restoreEnv(baseEnv, original);
    }
  });

  await t.test('send with non-string topic throws', async () => {
    const original = setEnv(baseEnv);
    try {
      const producer = new binding.Producer('test-group');
      await new Promise((resolve) => {
        producer.start(() => resolve());
      });
      assert.throws(() => {
        producer.send(123, 'body', {}, () => {});
      }, /Topic must be a string/);
      await new Promise((resolve) => {
        producer.shutdown(() => resolve());
      });
    } finally {
      restoreEnv(baseEnv, original);
    }
  });

  await t.test('send with non-function callback throws', async () => {
    const original = setEnv(baseEnv);
    try {
      const producer = new binding.Producer('test-group');
      await new Promise((resolve) => {
        producer.start(() => resolve());
      });
      assert.throws(() => {
        producer.send('topic', 'body', {}, 'not a function');
      }, /Callback must be a function/);
      await new Promise((resolve) => {
        producer.shutdown(() => resolve());
      });
    } finally {
      restoreEnv(baseEnv, original);
    }
  });

  await t.test('send with non-object options skips native options parsing', async () => {
    const original = setEnv(baseEnv);
    try {
      const producer = new binding.Producer('test-group');
      await new Promise((resolve) => {
        producer.start(() => resolve());
      });
      await new Promise((resolve, reject) => {
        producer.send('topic', 'body', 123, (err) => {
          if (err) return reject(err);
          resolve();
        });
      });
      await new Promise((resolve) => {
        producer.shutdown(() => resolve());
      });
    } finally {
      restoreEnv(baseEnv, original);
    }
  });

  await t.test('send with invalid body type throws', async () => {
    const original = setEnv(baseEnv);
    try {
      const producer = new binding.Producer('test-group');
      await new Promise((resolve) => {
        producer.start(() => resolve());
      });
      assert.throws(() => {
        producer.send('topic', 12345, {}, () => {});
      }, /Message body must be a string or buffer/);
      await new Promise((resolve) => {
        producer.shutdown(() => resolve());
      });
    } finally {
      restoreEnv(baseEnv, original);
    }
  });

  await t.test('send with ROCKETMQ_STUB_SEND_THROW triggers synchronous exception', async () => {
    const original = setEnv(baseEnv);
    try {
      const producer = new binding.Producer('test-group');
      await new Promise((resolve) => {
        producer.start(() => resolve());
      });
      const errEnv = { ROCKETMQ_STUB_SEND_THROW: '1' };
      const errOriginal = setEnv(errEnv);
      try {
        assert.throws(() => {
          producer.send('topic', 'body', {}, () => {});
        }, /producer send throw/);
      } finally {
        restoreEnv(errEnv, errOriginal);
      }
      await new Promise((resolve) => {
        producer.shutdown(() => resolve());
      });
    } finally {
      restoreEnv(baseEnv, original);
    }
  });

  await t.test('BlockingCall failure triggers cleanup (coverage branch)', async () => {
    const original = setEnv(baseEnv);
    try {
      const producer = new binding.Producer('test-group');
      await new Promise((resolve) => {
        producer.start(() => resolve());
      });
      const errEnv = { ROCKETMQ_STUB_PRODUCER_BLOCKING_FAIL: '1' };
      const errOriginal = setEnv(errEnv);
      try {
        await new Promise((resolve) => {
          producer.send('topic', 'body', {}, () => {
            resolve();
          });
          setTimeout(resolve, 100);
        });
      } finally {
        restoreEnv(errEnv, errOriginal);
      }
      await new Promise((resolve) => {
        producer.shutdown(() => resolve());
      });
    } finally {
      restoreEnv(baseEnv, original);
    }
  });

  await t.test('CallJs null env check (coverage branch)', async () => {
    const original = setEnv(baseEnv);
    try {
      const producer = new binding.Producer('test-group');
      await new Promise((resolve) => {
        producer.start(() => resolve());
      });
      const errEnv = { ROCKETMQ_STUB_PRODUCER_CALLJS_NULL_ENV: '1' };
      const errOriginal = setEnv(errEnv);
      try {
        await new Promise((resolve) => {
          producer.send('topic', 'body', {}, () => {
            resolve();
          });
          setTimeout(resolve, 100);
        });
      } finally {
        restoreEnv(errEnv, errOriginal);
      }
      await new Promise((resolve) => {
        producer.shutdown(() => resolve());
      });
    } finally {
      restoreEnv(baseEnv, original);
    }
  });

  await t.test('CallJs Napi::Error catch (coverage branch)', async () => {
    const original = setEnv(baseEnv);
    try {
      const producer = new binding.Producer('test-group');
      await new Promise((resolve) => {
        producer.start(() => resolve());
      });
      const errEnv = { ROCKETMQ_STUB_PRODUCER_CALLJS_THROW: '1' };
      const errOriginal = setEnv(errEnv);
      try {
        await new Promise((resolve) => {
          producer.send('topic', 'body', {}, () => {
            resolve();
          });
          setTimeout(resolve, 100);
        });
      } finally {
        restoreEnv(errEnv, errOriginal);
      }
      await new Promise((resolve) => {
        producer.shutdown(() => resolve());
      });
    } finally {
      restoreEnv(baseEnv, original);
    }
  });
});
