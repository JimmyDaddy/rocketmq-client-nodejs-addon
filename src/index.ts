export { RocketMQProducer, SendResultStatus, LogLevel as ProducerLogLevel, ProducerOptions, SendOptions, SendResult } from './producer';
export { RocketMQPushConsumer, LogLevel as ConsumerLogLevel, PushConsumerOptions, Message, ConsumerAck } from './consumer';

import { RocketMQProducer } from './producer';
import { RocketMQPushConsumer } from './consumer';

export const Producer = RocketMQProducer;
export const PushConsumer = RocketMQPushConsumer;
