FROM debian:buster

RUN apt-get update \
    && apt-get -y install e2tools python3 device-tree-compiler unzip p7zip-full jq vim

WORKDIR /data