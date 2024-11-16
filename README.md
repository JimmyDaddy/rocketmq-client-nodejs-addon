# Apache RocketMQ Client for Node.js

Node.js client for [Apache RocketMQ](https://rocketmq.apache.org/), implemented as native addons.

## Installation

```bash
npm install rocketmq-nodejs
```

## Prerequisites

- Node.js >= 12.0.0

## Usage

### RocketMQ

#### Producer
```javascript
const { Producer } = require('rocketmq-nodejs');

const producer = new Producer('GID_GROUP', {
    nameServer: '127.0.0.1:9876'
});

producer.start(() => {
    producer.send('TP_TOPIC', 'Hello RocketMQ', (err, result) => {
        if (err) return console.error(err);
        console.log(result);
    });
});
```

#### Consumer
```javascript
const { PushConsumer } = require('rocketmq-nodejs');

const consumer = new PushConsumer('GID_GROUP', {
    nameServer: '127.0.0.1:9876'
});

consumer.subscribe('TP_TOPIC', '*');
consumer.on('message', (msg, ack) => {
    console.log(msg);
    ack.done();
});

consumer.start(() => {
    console.log('Consumer started');
});
```

### Aliyun RocketMQ

#### Producer
```javascript
const producer = new Producer('GID_GROUP', {
    nameServer: 'onsaddr.cn-hangzhou.mq.aliyuncs.com:80'
});

// Set Aliyun credentials before starting
producer.setSessionCredentials(
    'YOUR_ACCESS_KEY',      // Access Key ID
    'YOUR_SECRET_KEY',      // Access Key Secret
    'ALIYUN'               // Channel
);

producer.start(() => {
    producer.send('TP_TOPIC', 'Hello Aliyun RocketMQ', (err, result) => {
        if (err) return console.error(err);
        console.log(result);
    });
});
```

#### Consumer
```javascript
const consumer = new PushConsumer('GID_GROUP', {
    nameServer: 'onsaddr.cn-hangzhou.mq.aliyuncs.com:80'
});

// Set Aliyun credentials before subscribing
consumer.setSessionCredentials(
    'YOUR_ACCESS_KEY',      // Access Key ID
    'YOUR_SECRET_KEY',      // Access Key Secret
    'ALIYUN'               // Channel
);

consumer.subscribe('TP_TOPIC', '*');
consumer.on('message', (msg, ack) => {
    console.log(msg);
    ack.done();
});

consumer.start(() => {
    console.log('Consumer started');
});
```

## API Reference

### Producer

#### new Producer(groupId, options)
- `groupId`: Producer group name
- `options`: Configuration object
  - `nameServer`: Name server address

#### producer.start(callback)
Start the producer.

#### producer.send(topic, message, [callback])
Send message to specified topic.
- `topic`: Topic name
- `message`: Message content (string)
- `callback`: Optional callback function(err, result)

#### producer.shutdown([callback])
Shutdown the producer.

### PushConsumer

#### new PushConsumer(groupId, options)
- `groupId`: Consumer group name
- `options`: Configuration object
  - `nameServer`: Name server address

#### consumer.subscribe(topic, subExpression)
Subscribe to specified topic.
- `topic`: Topic name
- `subExpression`: Subscribe expression, use '*' for all tags

#### consumer.on('message', callback)
Register message handler.
- `callback`: Function(message, ack)
  - `message`: Message object
  - `ack`: Acknowledgment object with done() method

#### consumer.start(callback)
Start the consumer.

#### consumer.shutdown([callback])
Shutdown the consumer.

## Advanced Features

### Session Credentials

For Aliyun RocketMQ:

```javascript
producer.setSessionCredentials(
    'AccessKey', 
    'SecretKey', 
    'Channel'  // Usually 'ALIYUN'
);
```

### Message Properties

```javascript
producer.send('TopicTest', {
    body: 'Hello',
    tags: 'TagA',
    keys: ['key1', 'key2'],
    properties: {
        'property1': 'value1'
    }
}, callback);
```