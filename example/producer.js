"use strict";

const common = require("./common");
const Producer = require("../").Producer;
const readline = require("readline");

const rl = readline.createInterface({
    input: process.stdin,
    output: process.stdout
});

void function () {
    const producer = new Producer('GID_GROUP', {
        nameServer: common.nameServer,
    });
    producer.start(async function() {
        console.log('producer started');
        while(1) {
            let resolve, reject;
            const promise = new Promise((res, rej) => {
                resolve = res;
                reject = rej;
              });
            rl.question('Enter input: ', input => {
                producer.send('TP_TOPIC', input, function(err, ret) {
                    if (err) {
                        console.error(err);
                        return reject(err)
                    }
                    console.log(ret);
                    resolve(ret)
                });
            });
            await promise;
        }
    });

}();
