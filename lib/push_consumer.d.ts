/// <reference types="node" />

import { EventEmitter } from "events";
import type Producer = require("./producer");

declare class PushConsumer extends EventEmitter {
    constructor(groupId: string, options?: PushConsumer.PushConsumerOptions);
    constructor(groupId: string, instanceName: string | null, options?: PushConsumer.PushConsumerOptions);

    setSessionCredentials(accessKey: string, secretKey: string, onsChannel: string): boolean;

    start(): Promise<void>;
    start(callback: (error: Error | null | undefined, result?: unknown) => void): void;

    shutdown(): Promise<void>;
    shutdown(callback: (error: Error | null | undefined, result?: unknown) => void): void;

    subscribe(topic: string, expression?: string): void;

    addListener(event: "message", listener: PushConsumer.MessageListener): this;
    on(event: "message", listener: PushConsumer.MessageListener): this;
    once(event: "message", listener: PushConsumer.MessageListener): this;
    prependListener(event: "message", listener: PushConsumer.MessageListener): this;
    prependOnceListener(event: "message", listener: PushConsumer.MessageListener): this;
    removeListener(event: "message", listener: PushConsumer.MessageListener): this;
    off(event: "message", listener: PushConsumer.MessageListener): this;

    addListener(event: string, listener: (...args: any[]) => void): this;
    on(event: string, listener: (...args: any[]) => void): this;
    once(event: string, listener: (...args: any[]) => void): this;
    prependListener(event: string, listener: (...args: any[]) => void): this;
    prependOnceListener(event: string, listener: (...args: any[]) => void): this;
    removeListener(event: string, listener: (...args: any[]) => void): this;
    off(event: string, listener: (...args: any[]) => void): this;
}

declare namespace PushConsumer {
    type LogLevelName = Producer.LogLevelName;
    type LogLevel = Producer.LogLevel;

    interface PushConsumerOptions {
        nameServer?: string;
        groupName?: string;
        threadCount?: number;
        maxBatchSize?: number;
        maxReconsumeTimes?: number;
        logDir?: string;
        logFileNum?: number;
        logFileSize?: number;
        logLevel?: LogLevel;
    }

    interface Message {
        topic: string;
        tags: string;
        keys: string;
        body: string;
        msgId: string;
    }

    interface MessageAck {
        done(success?: boolean): void;
    }

    type MessageListener = (message: Message, ack: MessageAck) => void;
}

export = PushConsumer;
