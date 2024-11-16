const { PushConsumer } = require('rocketmq-nodejs');

const consumer = new PushConsumer('GID_GROUP', {
    nameServer: '127.0.0.1:9876'
});

consumer.subscribe('TP_TOPIC', '*');

consumer.on('message', (msg, ack) => {
    console.log('Received:', msg);
    ack.done();
});

consumer.start(() => {
    console.log('Consumer started');
});
