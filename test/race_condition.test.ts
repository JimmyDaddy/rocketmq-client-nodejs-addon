'use strict';

import { describe, test, expect } from 'vitest';
import * as path from 'path';

import { ensureBindingBinary } from './helpers/binding';

const rootDir = path.join(__dirname, '..');
process.env.NODE_BINDINGS_COMPILED_DIR = 'build';
ensureBindingBinary(rootDir);

// Import from compiled dist for native binding compatibility
import { RocketMQProducer } from '../dist/producer';
import { RocketMQPushConsumer } from '../dist/consumer';
import { Status } from '../dist/contants';

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

describe('Race Condition Fix Tests', () => {
  describe('Producer race condition tests', () => {
    test('concurrent start calls - second call should be rejected properly', async () => {
      const producer = new RocketMQProducer('test-group');
      
      // 启动两个并发的 start 调用
      const promise1 = producer.start();
      const promise2 = producer.start();
      
      // 第一个应该成功，第二个应该失败
      const results = await Promise.allSettled([promise1, promise2]);
      
      expect(results[0].status).toBe('fulfilled');
      expect(results[1].status).toBe('rejected');
      // 第二个调用可能看到 "already starting" 或 "already started"，取决于时序
      expect((results[1] as PromiseRejectedResult).reason.message).toMatch(/Producer is already (starting|started)/);
      
      // 最终状态应该是 STARTED
      expect(producer.status).toBe(Status.STARTED);
      
      await producer.shutdown();
    });

    test('start failure should allow retry', async () => {
      const env = { ROCKETMQ_STUB_PRODUCER_START_ERROR: '1' };
      const original = setEnv(env);
      
      try {
        const producer = new RocketMQProducer('test-group');
        
        // 第一次 start 应该失败
        await expect(producer.start()).rejects.toThrow(/producer start error/);
        expect(producer.status).toBe(Status.STOPPED);
        
        // 移除错误环境变量，允许成功
        restoreEnv(env, original);
        
        // 第二次 start 应该成功
        await producer.start();
        expect(producer.status).toBe(Status.STARTED);
        
        await producer.shutdown();
      } finally {
        restoreEnv(env, original);
      }
    });

    test('concurrent operations are properly serialized', async () => {
      const producer = new RocketMQProducer('test-group');
      
      // 启动多个并发操作
      const operations = [
        producer.start(),
        producer.start(), // 应该失败
        producer.start()  // 应该失败
      ];
      
      const results = await Promise.allSettled(operations);
      
      // 只有第一个应该成功
      expect(results[0].status).toBe('fulfilled');
      expect(results[1].status).toBe('rejected');
      expect(results[2].status).toBe('rejected');
      
      expect(producer.status).toBe(Status.STARTED);
      await producer.shutdown();
    });
  });

  describe('Consumer race condition tests', () => {
    test('concurrent start calls - second call should be rejected properly', async () => {
      const consumer = new RocketMQPushConsumer('test-group');
      
      // 启动两个并发的 start 调用
      const promise1 = consumer.start();
      const promise2 = consumer.start();
      
      // 第一个应该成功，第二个应该失败
      const results = await Promise.allSettled([promise1, promise2]);
      
      expect(results[0].status).toBe('fulfilled');
      expect(results[1].status).toBe('rejected');
      // 第二个调用可能看到 "already starting" 或 "already started"，取决于时序
      expect((results[1] as PromiseRejectedResult).reason.message).toMatch(/Consumer is already (starting|started)/);
      
      // 最终状态应该是 STARTED
      expect(consumer.status).toBe(Status.STARTED);
      
      await consumer.shutdown();
    });

    test('start failure should allow retry', async () => {
      const env = { ROCKETMQ_STUB_CONSUMER_START_ERROR: '1' };
      const original = setEnv(env);
      
      try {
        const consumer = new RocketMQPushConsumer('test-group');
        
        // 第一次 start 应该失败
        await expect(consumer.start()).rejects.toThrow(/consumer start error/);
        expect(consumer.status).toBe(Status.STOPPED);
        
        // 移除错误环境变量，允许成功
        restoreEnv(env, original);
        
        // 第二次 start 应该成功
        await consumer.start();
        expect(consumer.status).toBe(Status.STARTED);
        
        await consumer.shutdown();
      } finally {
        restoreEnv(env, original);
      }
    });

    test('operations are properly queued and serialized', async () => {
      const consumer = new RocketMQPushConsumer('test-group');
      
      // 测试操作队列的正确性
      await consumer.start();
      expect(consumer.status).toBe(Status.STARTED);
      
      // 并发调用 shutdown
      const shutdownPromise1 = consumer.shutdown();
      const shutdownPromise2 = consumer.shutdown(); // 应该失败
      
      const results = await Promise.allSettled([shutdownPromise1, shutdownPromise2]);
      
      expect(results[0].status).toBe('fulfilled');
      expect(results[1].status).toBe('rejected');
      expect(consumer.status).toBe(Status.STOPPED);
    });
  });
});