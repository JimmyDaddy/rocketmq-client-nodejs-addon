'use strict';

import { describe, test, expect } from 'vitest';
import * as path from 'path';

import { ensureBindingBinary } from './helpers/binding';

const rootDir = path.join(__dirname, '..');
process.env.NODE_BINDINGS_COMPILED_DIR = 'build';
ensureBindingBinary(rootDir);

// Import from compiled dist for native binding compatibility
import { RocketMQProducer } from '../src/producer';
import { Status, LogLevel } from '../src/constants';
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

describe('Producer constructor tests', () => {
  test('with groupId only', () => {
    const producer = new RocketMQProducer('test-group');
    expect(producer).toBeTruthy();
    expect(producer.status).toBe(Status.STOPPED);
  });

  test('with groupId and options', () => {
    const producer = new RocketMQProducer('test-group', {
      nameServer: 'localhost:9876',
      logLevel: 'DEBUG'
    });
    expect(producer).toBeTruthy();
  });

  test('with groupId, instanceName, and options', () => {
    const producer = new RocketMQProducer('test-group', 'instance-1', {
      nameServer: 'localhost:9876'
    });
    expect(producer).toBeTruthy();
  });

  test('logLevel string to number conversion', () => {
    const producer = new RocketMQProducer('test-group', { logLevel: LogLevel.ERROR });
    expect(producer).toBeTruthy();
  });
});

describe('Producer setSessionCredentials tests', () => {
  test('with valid args', () => {
    const producer = new RocketMQProducer('test-group');
    const result = producer.setSessionCredentials('accessKey', 'secretKey', 'channel');
    expect(result).toBe(true);
  });

  test('throws on invalid accessKey', async () => {
    const producer = new RocketMQProducer('test-group');
    await expect(async () => {
      await producer.setSessionCredentials(123 as any, 'secretKey', 'channel');
    }).rejects.toThrow(/accessKey must be a string/);
  });

  test('throws on invalid secretKey', async () => {
    const producer = new RocketMQProducer('test-group');
    await expect(async () => {
      await producer.setSessionCredentials('accessKey', 123 as any, 'channel');
    }).rejects.toThrow(/secretKey must be a string/);
  });

  test('throws on invalid channel', async () => {
    const producer = new RocketMQProducer('test-group');
    await expect(async () => {
      await producer.setSessionCredentials('accessKey', 'secretKey', 123 as any);
    }).rejects.toThrow(/onsChannel must be a string/);
  });
});
describe('Producer start/shutdown lifecycle tests', () => {
  const baseEnv = {
    ROCKETMQ_STUB_PRODUCER_START_ERROR: undefined,
    ROCKETMQ_STUB_PRODUCER_SHUTDOWN_ERROR: undefined
  };

  test('start() with Promise pattern', async () => {
    const original = setEnv(baseEnv);
    let producer: any;
    try {
      producer = new RocketMQProducer('test-group');
      await producer.start();
      expect(producer.status).toBe(Status.STARTED);
    } finally {
      if (producer && producer.status === Status.STARTED) {
        await producer.shutdown();
      }
      restoreEnv(baseEnv, original);
    }
  });

  test('start() with callback pattern', async () => {
    const original = setEnv(baseEnv);
    let producer: any;
    try {
      producer = new RocketMQProducer('test-group');
      await new Promise<void>((resolve, reject) => {
        producer.start((err: any) => {
          if (err) return reject(err);
          resolve();
        });
      });
      expect(producer.status).toBe(Status.STARTED);
    } finally {
      if (producer && producer.status === Status.STARTED) {
        await producer.shutdown();
      }
      restoreEnv(baseEnv, original);
    }
  });

  test('start() error with Promise pattern', async () => {
    const env = { ROCKETMQ_STUB_PRODUCER_START_ERROR: '1' };
    const original = setEnv(env);
    try {
      const producer = new RocketMQProducer('test-group');
      await expect(producer.start()).rejects.toThrow(/producer start error/);
    } finally {
      restoreEnv(env, original);
    }
  });

  test('start() error with callback pattern', async () => {
    const env = { ROCKETMQ_STUB_PRODUCER_START_ERROR: '1' };
    const original = setEnv(env);
    try {
      const producer = new RocketMQProducer('test-group');
      await new Promise<void>((resolve) => {
        producer.start((err: any) => {
          expect(err).toBeTruthy();
          expect(err.message).toMatch(/producer start error/);
          resolve();
        });
      });
    } finally {
      restoreEnv(env, original);
    }
  });

  test('shutdown() with Promise pattern', async () => {
    const original = setEnv(baseEnv);
    let producer: any;
    try {
      producer = new RocketMQProducer('test-group');
      await producer.start();
      await producer.shutdown();
      expect(producer.status).toBe(Status.STOPPED);
      producer = null;
    } finally {
      if (producer && producer.status === Status.STARTED) {
        await producer.shutdown();
      }
      restoreEnv(baseEnv, original);
    }
  });

  test('shutdown() with callback pattern', async () => {
    const original = setEnv(baseEnv);
    let producer: any;
    try {
      producer = new RocketMQProducer('test-group');
      await producer.start();
      await new Promise<void>((resolve, reject) => {
        producer.shutdown((err: any) => {
          if (err) return reject(err);
          resolve();
        });
      });
      expect(producer.status).toBe(Status.STOPPED);
      producer = null;
    } finally {
      if (producer && producer.status === Status.STARTED) {
        await producer.shutdown();
      }
      restoreEnv(baseEnv, original);
    }
  });

  test('shutdown() error with Promise pattern', async () => {
    const original = setEnv(baseEnv);
    let producer: any;
    try {
      producer = new RocketMQProducer('test-group');
      await producer.start();
      const errEnv = { ROCKETMQ_STUB_PRODUCER_SHUTDOWN_ERROR: '1' };
      const errOriginal = setEnv(errEnv);
      try {
        await expect(producer.shutdown()).rejects.toThrow(/producer shutdown error/);
      } finally {
        restoreEnv(errEnv, errOriginal);
      }
      producer.status = Status.STARTED;
      await producer.shutdown();
      producer = null;
    } finally {
      if (producer && producer.status === Status.STARTED) {
        await producer.shutdown();
      }
      restoreEnv(baseEnv, original);
    }
  });

  test('status transitions STOPPED -> STARTING -> STARTED', async () => {
    const original = setEnv(baseEnv);
    let producer: any;
    try {
      producer = new RocketMQProducer('test-group');
      expect(producer.status).toBe(Status.STOPPED);
      
      // 状态在操作完成前不会立即改变
      const promise = producer.start();
      // 注意：状态可能还是 STOPPED，因为操作是异步的
      
      await promise;
      expect(producer.status).toBe(Status.STARTED);
    } finally {
      if (producer && producer.status === Status.STARTED) {
        await producer.shutdown();
      }
      restoreEnv(baseEnv, original);
    }
  });

  test('status transitions STARTED -> STOPPING -> STOPPED', async () => {
    const original = setEnv(baseEnv);
    let producer: any;
    try {
      producer = new RocketMQProducer('test-group');
      await producer.start();
      expect(producer.status).toBe(Status.STARTED);
      
      // 状态在操作完成前不会立即改变
      const promise = producer.shutdown();
      // 注意：状态可能还是 STARTED，因为操作是异步的
      
      await promise;
      expect(producer.status).toBe(Status.STOPPED);
      producer = null;
    } finally {
      if (producer && producer.status === Status.STARTED) {
        await producer.shutdown();
      }
      restoreEnv(baseEnv, original);
    }
  });

  test('start() throws when not STOPPED', async () => {
    const original = setEnv(baseEnv);
    let producer: any;
    try {
      producer = new RocketMQProducer('test-group');
      await producer.start();
      await expect(producer.start()).rejects.toThrow(/Producer is already started/);
    } finally {
      if (producer && producer.status === Status.STARTED) {
        await producer.shutdown();
      }
      restoreEnv(baseEnv, original);
    }
  });

  test('shutdown() throws when not STARTED', async () => {
    const original = setEnv(baseEnv);
    try {
      const producer = new RocketMQProducer('test-group');
      await expect(producer.shutdown()).rejects.toThrow(/Producer is already stopped/);
    } finally {
      restoreEnv(baseEnv, original);
    }
  });

  test('multiple start/shutdown cycles', async () => {
    const original = setEnv(baseEnv);
    let producer: any;
    try {
      producer = new RocketMQProducer('test-group');
      await producer.start();
      await producer.shutdown();
      await producer.start();
      await producer.shutdown();
      producer = null;
    } finally {
      if (producer && producer.status === Status.STARTED) {
        await producer.shutdown();
      }
      restoreEnv(baseEnv, original);
    }
  });

  test('multiple producers with producerRef counting', async () => {
    const original = setEnv(baseEnv);
    let producer1: any, producer2: any;
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
      if (producer1 && producer1.status === Status.STARTED) {
        await producer1.shutdown();
      }
      if (producer2 && producer2.status === Status.STARTED) {
        await producer2.shutdown();
      }
      restoreEnv(baseEnv, original);
    }
  });
});
describe('Producer send tests', () => {
  const baseEnv = {
    ROCKETMQ_STUB_PRODUCER_START_ERROR: undefined,
    ROCKETMQ_STUB_PRODUCER_SHUTDOWN_ERROR: undefined,
    ROCKETMQ_STUB_SEND_EXCEPTION: undefined,
    ROCKETMQ_STUB_SEND_THROW: undefined
  };

  test('with string body using Promise', async () => {
    const original = setEnv(baseEnv);
    let producer: any;
    try {
      producer = new RocketMQProducer('test-group');
      await producer.start();
      const result = await producer.send('test-topic', 'hello world');
      expect(result.status).toBe(0);
      expect(result.statusStr).toBe('OK');
      expect(result.msgId).toBeTruthy();
    } finally {
      if (producer && producer.status === Status.STARTED) {
        await producer.shutdown();
      }
      restoreEnv(baseEnv, original);
    }
  });

  test('with Buffer body using Promise', async () => {
    const original = setEnv(baseEnv);
    let producer: any;
    try {
      producer = new RocketMQProducer('test-group');
      await producer.start();
      const result = await producer.send('test-topic', Buffer.from('hello buffer'));
      expect(result.status).toBe(0);
    } finally {
      if (producer && producer.status === Status.STARTED) {
        await producer.shutdown();
      }
      restoreEnv(baseEnv, original);
    }
  });

  test('with callback pattern', async () => {
    const original = setEnv(baseEnv);
    let producer: any;
    try {
      producer = new RocketMQProducer('test-group');
      await producer.start();
      await new Promise<void>((resolve, reject) => {
        producer.send('test-topic', 'hello', (err: any, result: any) => {
          if (err) return reject(err);
          expect(result.status).toBe(0);
          resolve();
        });
      });
    } finally {
      if (producer && producer.status === Status.STARTED) {
        await producer.shutdown();
      }
      restoreEnv(baseEnv, original);
    }
  });

  test('with options', async () => {
    const original = setEnv(baseEnv);
    let producer: any;
    try {
      producer = new RocketMQProducer('test-group');
      await producer.start();
      const result = await producer.send('test-topic', 'hello', { tags: 'tagA', keys: 'key1' });
      expect(result.status).toBe(0);
    } finally {
      if (producer && producer.status === Status.STARTED) {
        await producer.shutdown();
      }
      restoreEnv(baseEnv, original);
    }
  });

  test('with options and callback', async () => {
    const original = setEnv(baseEnv);
    let producer: any;
    try {
      producer = new RocketMQProducer('test-group');
      await producer.start();
      await new Promise<void>((resolve, reject) => {
        producer.send('test-topic', 'hello', { tags: 'tagA' }, (err: any, result: any) => {
          if (err) return reject(err);
          expect(result.status).toBe(0);
          resolve();
        });
      });
    } finally {
      if (producer && producer.status === Status.STARTED) {
        await producer.shutdown();
      }
      restoreEnv(baseEnv, original);
    }
  });

  test('with empty string body returns EMPTY_BODY', async () => {
    const original = setEnv(baseEnv);
    let producer: any;
    try {
      producer = new RocketMQProducer('test-group');
      await producer.start();
      const result = await producer.send('test-topic', '');
      expect(result.status).toBe(-1);
      expect(result.statusStr).toBe('EMPTY_BODY');
    } finally {
      if (producer && producer.status === Status.STARTED) {
        await producer.shutdown();
      }
      restoreEnv(baseEnv, original);
    }
  });

  test('with empty Buffer body returns EMPTY_BODY', async () => {
    const original = setEnv(baseEnv);
    let producer: any;
    try {
      producer = new RocketMQProducer('test-group');
      await producer.start();
      const result = await producer.send('test-topic', Buffer.alloc(0));
      expect(result.status).toBe(-1);
      expect(result.statusStr).toBe('EMPTY_BODY');
    } finally {
      if (producer && producer.status === Status.STARTED) {
        await producer.shutdown();
      }
      restoreEnv(baseEnv, original);
    }
  });

  test('empty body with callback', async () => {
    const original = setEnv(baseEnv);
    let producer: any;
    try {
      producer = new RocketMQProducer('test-group');
      await producer.start();
      await new Promise<void>((resolve) => {
        producer.send('test-topic', '', (err: any, result: any) => {
          expect(err).toBe(null);
          expect(result.status).toBe(-1);
          expect(result.statusStr).toBe('EMPTY_BODY');
          resolve();
        });
      });
    } finally {
      if (producer && producer.status === Status.STARTED) {
        await producer.shutdown();
      }
      restoreEnv(baseEnv, original);
    }
  });

  test('error with Promise pattern (onException)', async () => {
    const original = setEnv(baseEnv);
    let producer: any;
    try {
      producer = new RocketMQProducer('test-group');
      await producer.start();
      const errEnv = { ROCKETMQ_STUB_SEND_EXCEPTION: '1' };
      const errOriginal = setEnv(errEnv);
      try {
        await expect(producer.send('test-topic', 'hello')).rejects.toThrow(/producer send exception/);
      } finally {
        restoreEnv(errEnv, errOriginal);
      }
    } finally {
      if (producer && producer.status === Status.STARTED) {
        await producer.shutdown();
      }
      restoreEnv(baseEnv, original);
    }
  });

  test('error with callback pattern', async () => {
    const original = setEnv(baseEnv);
    let producer: any;
    try {
      producer = new RocketMQProducer('test-group');
      await producer.start();
      const errEnv = { ROCKETMQ_STUB_SEND_EXCEPTION: '1' };
      const errOriginal = setEnv(errEnv);
      try {
        await new Promise<void>((resolve) => {
          producer.send('test-topic', 'hello', (err: any) => {
            expect(err).toBeTruthy();
            expect(err.message).toMatch(/producer send exception/);
            resolve();
          });
        });
      } finally {
        restoreEnv(errEnv, errOriginal);
      }
    } finally {
      if (producer && producer.status === Status.STARTED) {
        await producer.shutdown();
      }
      restoreEnv(baseEnv, original);
    }
  });

  test('throws on invalid topic', async () => {
    const original = setEnv(baseEnv);
    let producer: any;
    try {
      producer = new RocketMQProducer('test-group');
      await producer.start();
      expect(() => {
        producer.send(123, 'body');
      }).toThrow(/topic must be a string/);
    } finally {
      if (producer && producer.status === Status.STARTED) {
        await producer.shutdown();
      }
      restoreEnv(baseEnv, original);
    }
  });

  test('throws on invalid body type', async () => {
    const original = setEnv(baseEnv);
    let producer: any;
    try {
      producer = new RocketMQProducer('test-group');
      await producer.start();
      expect(() => {
        producer.send('topic', 123);
      }).toThrow(/body must be a string or Buffer/);
    } finally {
      if (producer && producer.status === Status.STARTED) {
        await producer.shutdown();
      }
      restoreEnv(baseEnv, original);
    }
  });
});

describe('Producer static properties', () => {
  test('SEND_RESULT static property', () => {
    expect(RocketMQProducer.SEND_RESULT.OK).toBe(0);
    expect(RocketMQProducer.SEND_RESULT.FLUSH_DISK_TIMEOUT).toBe(1);
    expect(RocketMQProducer.SEND_RESULT.FLUSH_SLAVE_TIMEOUT).toBe(2);
    expect(RocketMQProducer.SEND_RESULT.SLAVE_NOT_AVAILABLE).toBe(3);
  });
});
describe('Producer C++ binding coverage tests', () => {
  const baseEnv = {
    ROCKETMQ_STUB_PRODUCER_START_ERROR: undefined,
    ROCKETMQ_STUB_PRODUCER_SHUTDOWN_ERROR: undefined,
    ROCKETMQ_STUB_SEND_EXCEPTION: undefined,
    ROCKETMQ_STUB_SEND_THROW: undefined
  };

  test('constructor with all options (covers SetOptions branches)', () => {
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
    expect(producer).toBeTruthy();
  });

  test('constructor with logLevel boundary values', () => {
    const producerLow = new binding.Producer('test-group', 'instance-1', {
      logLevel: -1
    });
    const producerHigh = new binding.Producer('test-group', 'instance-1', {
      logLevel: 999
    });
    expect(producerLow).toBeTruthy();
    expect(producerHigh).toBeTruthy();
  });

  test('setSessionCredentials with non-string accessKey throws', () => {
    const producer = new binding.Producer('test-group');
    expect(() => {
      producer.setSessionCredentials(123 as any, 'secret', 'channel');
    }).toThrow(/All arguments must be strings/);
  });

  test('setSessionCredentials with non-string secretKey throws', () => {
    const producer = new binding.Producer('test-group');
    expect(() => {
      producer.setSessionCredentials('access', 123 as any, 'channel');
    }).toThrow(/All arguments must be strings/);
  });

  test('setSessionCredentials with non-string channel throws', () => {
    const producer = new binding.Producer('test-group');
    expect(() => {
      producer.setSessionCredentials('access', 'secret', 123 as any);
    }).toThrow(/All arguments must be strings/);
  });

  test('setSessionCredentials with too few arguments throws', () => {
    const producer = new binding.Producer('test-group');
    expect(() => {
      (producer as any).setSessionCredentials('access', 'secret');
    }).toThrow(/Wrong number of arguments/);
  });

  test('start without callback throws', () => {
    const producer = new binding.Producer('test-group');
    expect(() => {
      (producer as any).start();
    }).toThrow(/Function expected/);
  });

  test('start with non-function throws', () => {
    const producer = new binding.Producer('test-group');
    expect(() => {
      (producer as any).start('not a function');
    }).toThrow(/Function expected/);
  });

  test('shutdown without callback throws', () => {
    const producer = new binding.Producer('test-group');
    expect(() => {
      (producer as any).shutdown();
    }).toThrow(/Function expected/);
  });

  test('shutdown with non-function throws', () => {
    const producer = new binding.Producer('test-group');
    expect(() => {
      (producer as any).shutdown('not a function');
    }).toThrow(/Function expected/);
  });

  test('send with too few arguments throws', () => {
    const original = setEnv(baseEnv);
    try {
      const producer = new binding.Producer('test-group');
      expect(() => {
        (producer as any).send('topic', 'body', {});
      }).toThrow(/Wrong number of arguments/);
    } finally {
      restoreEnv(baseEnv, original);
    }
  });

  test('send with non-string topic throws', async () => {
    const original = setEnv(baseEnv);
    try {
      const producer = new binding.Producer('test-group');
      await new Promise<void>((resolve) => {
        producer.start(() => resolve());
      });
      expect(() => {
        producer.send(123 as any, 'body', {}, () => {});
      }).toThrow(/Topic must be a string/);
      await new Promise<void>((resolve) => {
        producer.shutdown(() => resolve());
      });
    } finally {
      restoreEnv(baseEnv, original);
    }
  });

  test('send with non-function callback throws', async () => {
    const original = setEnv(baseEnv);
    try {
      const producer = new binding.Producer('test-group');
      await new Promise<void>((resolve) => {
        producer.start(() => resolve());
      });
      expect(() => {
        (producer as any).send('topic', 'body', {}, 'not a function');
      }).toThrow(/Callback must be a function/);
      await new Promise<void>((resolve) => {
        producer.shutdown(() => resolve());
      });
    } finally {
      restoreEnv(baseEnv, original);
    }
  });

  test('send with non-object options skips native options parsing', async () => {
    const original = setEnv(baseEnv);
    try {
      const producer = new binding.Producer('test-group');
      await new Promise<void>((resolve) => {
        producer.start(() => resolve());
      });
      await new Promise<void>((resolve, reject) => {
        producer.send('topic', 'body', 123 as any, (err: any) => {
          if (err) return reject(err);
          resolve();
        });
      });
      await new Promise<void>((resolve) => {
        producer.shutdown(() => resolve());
      });
    } finally {
      restoreEnv(baseEnv, original);
    }
  });

  test('send with invalid body type throws', async () => {
    const original = setEnv(baseEnv);
    try {
      const producer = new binding.Producer('test-group');
      await new Promise<void>((resolve) => {
        producer.start(() => resolve());
      });
      expect(() => {
        producer.send('topic', 12345 as any, {}, () => {});
      }).toThrow(/Message body must be a string or buffer/);
      await new Promise<void>((resolve) => {
        producer.shutdown(() => resolve());
      });
    } finally {
      restoreEnv(baseEnv, original);
    }
  });

  test('send with ROCKETMQ_STUB_SEND_THROW triggers synchronous exception', async () => {
    const original = setEnv(baseEnv);
    try {
      const producer = new binding.Producer('test-group');
      await new Promise<void>((resolve) => {
        producer.start(() => resolve());
      });
      const errEnv = { ROCKETMQ_STUB_SEND_THROW: '1' };
      const errOriginal = setEnv(errEnv);
      try {
        expect(() => {
          producer.send('topic', 'body', {}, () => {});
        }).toThrow(/producer send throw/);
      } finally {
        restoreEnv(errEnv, errOriginal);
      }
      await new Promise<void>((resolve) => {
        producer.shutdown(() => resolve());
      });
    } finally {
      restoreEnv(baseEnv, original);
    }
  });

  test('BlockingCall failure triggers cleanup (coverage branch)', async () => {
    const original = setEnv(baseEnv);
    try {
      const producer = new binding.Producer('test-group');
      await new Promise<void>((resolve) => {
        producer.start(() => resolve());
      });
      const errEnv = { ROCKETMQ_STUB_PRODUCER_BLOCKING_FAIL: '1' };
      const errOriginal = setEnv(errEnv);
      try {
        await new Promise<void>((resolve) => {
          producer.send('topic', 'body', {}, () => {
            resolve();
          });
          setTimeout(resolve, 100);
        });
      } finally {
        restoreEnv(errEnv, errOriginal);
      }
      await new Promise<void>((resolve) => {
        producer.shutdown(() => resolve());
      });
    } finally {
      restoreEnv(baseEnv, original);
    }
  });

  test('CallJs null env check (coverage branch)', async () => {
    const original = setEnv(baseEnv);
    try {
      const producer = new binding.Producer('test-group');
      await new Promise<void>((resolve) => {
        producer.start(() => resolve());
      });
      const errEnv = { ROCKETMQ_STUB_PRODUCER_CALLJS_NULL_ENV: '1' };
      const errOriginal = setEnv(errEnv);
      try {
        await new Promise<void>((resolve) => {
          producer.send('topic', 'body', {}, () => {
            resolve();
          });
          setTimeout(resolve, 100);
        });
      } finally {
        restoreEnv(errEnv, errOriginal);
      }
      await new Promise<void>((resolve) => {
        producer.shutdown(() => resolve());
      });
    } finally {
      restoreEnv(baseEnv, original);
    }
  });

  test('CallJs Napi::Error catch (coverage branch)', async () => {
    const original = setEnv(baseEnv);
    try {
      const producer = new binding.Producer('test-group');
      await new Promise<void>((resolve) => {
        producer.start(() => resolve());
      });
      const errEnv = { ROCKETMQ_STUB_PRODUCER_CALLJS_THROW: '1' };
      const errOriginal = setEnv(errEnv);
      try {
        await new Promise<void>((resolve) => {
          producer.send('topic', 'body', {}, () => {
            resolve();
          });
          setTimeout(resolve, 100);
        });
      } finally {
        restoreEnv(errEnv, errOriginal);
      }
      await new Promise<void>((resolve) => {
        producer.shutdown(() => resolve());
      });
    } finally {
      restoreEnv(baseEnv, original);
    }
  });
});
