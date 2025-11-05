import Producer = require("./lib/producer");
import PushConsumer = require("./lib/push_consumer");

export { Producer, PushConsumer };

export type LogLevelName = Producer.LogLevelName;
export type LogLevel = Producer.LogLevel;
export type ProducerOptions = Producer.ProducerOptions;
export type ProducerSendOptions = Producer.ProducerSendOptions;
export type SendResult = Producer.SendResult;
export type SendCallback = Producer.SendCallback;

export type PushConsumerOptions = PushConsumer.PushConsumerOptions;
export type Message = PushConsumer.Message;
export type MessageAck = PushConsumer.MessageAck;
export type MessageListener = PushConsumer.MessageListener;
