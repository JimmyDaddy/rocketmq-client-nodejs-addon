"use strict";

const common = require('./common');
const PushConsumer = require('../').PushConsumer;

void function () {
    const consumer = new PushConsumer('GID_GROUP', {
        nameServer: common.nameServer,
    });

    consumer.setSessionCredentials('accessKey', 'secretKey', 'ALIYUN');

    consumer.subscribe('TP_TOPIC', '*');
    consumer.on('message', function(msg, ack) {
            console.log(msg)
            ack.done();
    })

    consumer.start(function() {
        console.log('consumer started');
    });
}();
