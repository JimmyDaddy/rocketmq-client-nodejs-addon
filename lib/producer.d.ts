/// <reference types="node" />

declare class Producer {
    static readonly SEND_RESULT: {
        readonly OK: 0;
        readonly FLUSH_DISK_TIMEOUT: 1;
        readonly FLUSH_SLAVE_TIMEOUT: 2;
        readonly SLAVE_NOT_AVAILABLE: 3;
    };

    constructor(groupId: string, options?: Producer.ProducerOptions);
    constructor(groupId: string, instanceName: string | null, options?: Producer.ProducerOptions);

    setSessionCredentials(accessKey: string, secretKey: string, onsChannel: string): boolean;

    start(): Promise<void>;
    start(callback: (error: Error | null | undefined, result?: unknown) => void): void;

    shutdown(): Promise<void>;
    shutdown(callback: (error: Error | null | undefined, result?: unknown) => void): void;

    send(topic: string, body: string | Buffer): Promise<Producer.SendResult>;
    send(topic: string, body: string | Buffer, callback: Producer.SendCallback): void;
    send(topic: string, body: string | Buffer, options: Producer.ProducerSendOptions): Promise<Producer.SendResult>;
    send(
        topic: string,
        body: string | Buffer,
        options: Producer.ProducerSendOptions,
        callback: Producer.SendCallback
    ): void;
}

declare namespace Producer {
    type LogLevelName = "fatal" | "error" | "warn" | "info" | "debug" | "trace" | "num";

    type LogLevel = LogLevelName | number;

    interface ProducerOptions {
        nameServer?: string;
        groupName?: string;
        maxMessageSize?: number;
        compressLevel?: number;
        sendMessageTimeout?: number;
        logDir?: string;
        logFileNum?: number;
        logFileSize?: number;
        logLevel?: LogLevel;
    }

    interface ProducerSendOptions {
        tags?: string;
        keys?: string;
    }

    interface SendResult {
        status: number;
        statusStr: string;
        msgId: string;
        offset: number;
    }

    type SendCallback = (error: Error | null | undefined, result?: SendResult) => void;
}

export = Producer;
