const { Producer } = require('rocketmq-nodejs');

const producer = new Producer('GID_GROUP', {
    nameServer: '127.0.0.1:9876'
});

producer.start(() => {
    console.log('Producer started');
    
    producer.send('TP_TOPIC', 'Hello RocketMQ', (err, result) => {
        if (err) {
            console.error('Send failed:', err);
            return;
        }
        console.log('Send success:', result);
        producer.shutdown(() => console.log('shutdown'))
    });

});
