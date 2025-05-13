"use strict";

if (typeof Promise.withResolvers === 'undefined') {
    Promise.withResolvers = function () {
        let resolve, reject
        const promise = new Promise((res, rej) => {
            resolve = res
            reject = rej
        })
        return { promise, resolve, reject }
    }
}

const readline = require('readline');
const common = require('./common');
const Producer = require('../').Producer;

const rl = readline.createInterface({
    input: process.stdin,
    output: process.stdout
});

void async function () {
    const producer = new Producer('GID_GROUP', {
        nameServer: common.nameServer,
    });

    // producer.setSessionCredentials('accessKey', 'secretKey', 'ALIYUN')

    await producer.start();

    while(1) {
        const { promise, resolve, reject } = Promise.withResolvers()
        rl.question('Enter input: ', async input => {
            await producer.send('TP_TOPIC', input).catch(reject);
            resolve();
        });
        await promise;
    }

    // producer.start(async function() {
    //     console.log('producer started');
    //     while(1) {
    //         const { promise, resolve, reject } = Promise.withResolvers()
    //         rl.question('Enter input: ', input => {
    //             producer.send('TP_TOPIC', input, function(err, ret) {
    //                 if (err) {
    //                     console.error(err);
    //                     reject(err)
    //                 }
    //                 console.log(ret);
    //                 resolve(ret)
    //             });
    //         });
    //         await promise.catch(console.error);
    //     }
    // });

}();
