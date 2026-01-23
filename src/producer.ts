import binding, { NativeProducer } from './binding';

const START_OR_SHUTDOWN = Symbol('RocketMQProducer#startOrShutdown');

export enum SendResultStatus {
  OK = 0,
  FLUSH_DISK_TIMEOUT = 1,
  FLUSH_SLAVE_TIMEOUT = 2,
  SLAVE_NOT_AVAILABLE = 3,
}

const SEND_RESULT_STATUS_STR: Record<number, string> = {
  0: 'OK',
  1: 'FLUSH_DISK_TIMEOUT',
  2: 'FLUSH_SLAVE_TIMEOUT',
  3: 'SLAVE_NOT_AVAILABLE',
};

enum StartStatus {
  STOPPED = 0,
  STARTED = 1,
  STOPPING = 2,
  STARTING = 3,
}

export enum LogLevel {
  FATAL = 1,
  ERROR = 2,
  WARN = 3,
  INFO = 4,
  DEBUG = 5,
  TRACE = 6,
  NUM = 7,
}

export interface ProducerOptions {
  nameServer?: string;
  groupName?: string;
  maxMessageSize?: number;
  compressLevel?: number;
  sendMessageTimeout?: number;
  logLevel?: LogLevel | keyof typeof LogLevel;
  logDir?: string;
  logFileSize?: number;
  logFileNum?: number;
}

export interface SendOptions {
  tags?: string;
  keys?: string;
}

export interface SendResult {
  status: number;
  statusStr: string;
  msgId: string;
  offset: number;
}

type Callback<T = void> = (err?: Error | null, result?: T) => void;

let producerRef = 0;
let timer: NodeJS.Timeout | undefined;

export class RocketMQProducer {
  private core: NativeProducer;
  private status: StartStatus;
  private operationQueue: Promise<void>;

  /**
   * RocketMQ Producer constructor
   * @param groupId the group id
   * @param instanceName the instance name
   * @param options the options
   */
  constructor(groupId: string, instanceName?: string | ProducerOptions, options?: ProducerOptions) {
    let actualInstanceName: string | null = null;
    let actualOptions: ProducerOptions = {};

    if (typeof instanceName !== 'string') {
      actualOptions = instanceName || {};
    } else {
      actualInstanceName = instanceName;
      actualOptions = options || {};
    }

    if (actualOptions.logLevel && typeof actualOptions.logLevel === 'string') {
      actualOptions.logLevel = LogLevel[actualOptions.logLevel.toUpperCase() as keyof typeof LogLevel] || LogLevel.INFO;
    }

    this.core = new binding.Producer(groupId, actualInstanceName, actualOptions);
    this.status = StartStatus.STOPPED;
    this.operationQueue = Promise.resolve();
  }

  /**
   * Set session credentials (usually used in Alibaba MQ)
   * @param accessKey the access key
   * @param secretKey the secret key
   * @param onsChannel the ons channel
   * @return the result
   */
  setSessionCredentials(accessKey: string, secretKey: string, onsChannel: string): boolean {
    if (typeof accessKey !== 'string') throw new TypeError('accessKey must be a string');
    if (typeof secretKey !== 'string') throw new TypeError('secretKey must be a string');
    if (typeof onsChannel !== 'string') throw new TypeError('onsChannel must be a string');
    
    this.core.setSessionCredentials(accessKey, secretKey, onsChannel);
    return true;
  }

  private [START_OR_SHUTDOWN](method: 'start' | 'shutdown'): Promise<void>;
  private [START_OR_SHUTDOWN](method: 'start' | 'shutdown', callback: Callback): void;
  private [START_OR_SHUTDOWN](method: 'start' | 'shutdown', callback?: Callback): void | Promise<void> {
    let promise: Promise<void> | undefined;
    let resolve: (value?: void) => void;
    let reject: (err: Error) => void;

    if (!callback) {
      promise = new Promise<void>((_resolve, _reject) => {
        resolve = _resolve;
        reject = _reject;
      });
    } else {
      resolve = () => callback(null);
      reject = callback;
    }

    // 将操作加入队列，确保串行执行
    this.operationQueue = this.operationQueue
      .then(() => {
        return new Promise<void>((queueResolve) => {
          this.core[method]((err) => {
            if (err) {
              // 回滚状态
              if (method === 'start') {
                this.status = StartStatus.STOPPED;
              } else {
                this.status = StartStatus.STARTED;
              }
              queueResolve();
              return reject(err);
            }

            if (method === 'start') {
              this.status = StartStatus.STARTED;
              if (!producerRef) timer = setInterval(() => {}, 24 * 3600 * 1000);
              producerRef++;
            } else {
              this.status = StartStatus.STOPPED;
              producerRef--;
              if (!producerRef && timer) {
                clearInterval(timer);
                timer = undefined;
              }
            }

            queueResolve();
            resolve();
          });
        });
      })
      .catch(() => {}); // 防止队列中断

    return promise;
  }

  /**
   * Start the producer
   * @param callback the callback function
   * @return returns a Promise if no callback
   */
  start(): Promise<void>;
  start(callback: Callback): void;
  start(callback?: Callback): void | Promise<void> {
    if (this.status === StartStatus.STARTING) {
      const err = new Error('Producer is already starting');
      return callback ? callback(err) : Promise.reject(err);
    }
    if (this.status === StartStatus.STARTED) {
      const err = new Error('Producer is already started');
      return callback ? callback(err) : Promise.reject(err);
    }
    if (this.status === StartStatus.STOPPING) {
      const err = new Error('Producer is stopping, please wait for shutdown to complete');
      return callback ? callback(err) : Promise.reject(err);
    }
    this.status = StartStatus.STARTING;
    return this[START_OR_SHUTDOWN]('start', callback as any);
  }

  /**
   * Shutdown the producer
   * @param callback the callback function
   * @return returns a Promise if no callback
   */
  shutdown(): Promise<void>;
  shutdown(callback: Callback): void;
  shutdown(callback?: Callback): void | Promise<void> {
    if (this.status === StartStatus.STOPPED) {
      const err = new Error('Producer is already stopped');
      return callback ? callback(err) : Promise.reject(err);
    }
    if (this.status === StartStatus.STOPPING) {
      const err = new Error('Producer is already stopping');
      return callback ? callback(err) : Promise.reject(err);
    }
    if (this.status === StartStatus.STARTING) {
      const err = new Error('Producer is starting, please wait for start to complete');
      return callback ? callback(err) : Promise.reject(err);
    }
    this.status = StartStatus.STOPPING;
    return this[START_OR_SHUTDOWN]('shutdown', callback as any);
  }

  /**
   * Send a message
   * @param topic the topic
   * @param body the body
   * @param options the options
   * @param callback the callback function
   * @return returns a Promise if no callback
   */
  send(topic: string, body: string | Buffer, options?: SendOptions): Promise<SendResult>;
  send(topic: string, body: string | Buffer, callback: Callback<SendResult>): void;
  send(topic: string, body: string | Buffer, options: SendOptions, callback: Callback<SendResult>): void;
  send(
    topic: string,
    body: string | Buffer,
    options?: SendOptions | Callback<SendResult>,
    callback?: Callback<SendResult>
  ): void | Promise<SendResult> {
    if (typeof topic !== 'string') throw new TypeError('topic must be a string');
    if (typeof body !== 'string' && !Buffer.isBuffer(body)) {
      throw new TypeError('body must be a string or Buffer');
    }

    let actualOptions: SendOptions = {};
    let actualCallback: Callback<SendResult> | undefined;

    if (typeof options === 'function') {
      actualCallback = options;
    } else {
      actualOptions = options || {};
      actualCallback = callback;
    }

    // 检查 Producer 状态
    if (this.status !== StartStatus.STARTED) {
      const statusName =
        this.status === StartStatus.STOPPED
          ? 'STOPPED'
          : this.status === StartStatus.STARTING
          ? 'STARTING'
          : 'STOPPING';
      const err = new Error(`Producer must be started before sending messages. Current status: ${statusName}`);
      return actualCallback ? actualCallback(err) : Promise.reject(err);
    }

    if (!body.length) {
      const ret: SendResult = { status: -1, statusStr: 'EMPTY_BODY', msgId: '', offset: 0 };
      return actualCallback ? actualCallback(null, ret) : Promise.resolve(ret);
    }

    let promise: Promise<SendResult> | undefined;
    let resolve: (value: SendResult) => void;
    let reject: (err: Error) => void;

    if (!actualCallback) {
      promise = new Promise<SendResult>((_resolve, _reject) => {
        resolve = _resolve;
        reject = _reject;
      });
    } else {
      resolve = (result: SendResult) => actualCallback!(null, result);
      reject = actualCallback;
    }

    this.core.send(topic, body, actualOptions, (err, status, msgId, offset) => {
      if (err) {
        return reject(err);
      }

      const ret: SendResult = {
        status: status!,
        statusStr: SEND_RESULT_STATUS_STR[status!] || 'UNKNOWN',
        msgId: msgId!,
        offset: offset!,
      };
      resolve(ret);
    });

    return promise;
  }

  static SEND_RESULT = SendResultStatus;
}

module.exports = RocketMQProducer;
