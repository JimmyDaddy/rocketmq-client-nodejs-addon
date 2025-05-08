FROM alpine:latest

WORKDIR /root

COPY . /root

RUN apk update && apk add nodejs && apk add npm
