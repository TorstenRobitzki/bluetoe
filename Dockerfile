FROM ubuntu:latest

MAINTAINER Oliver Bruns <obruns@gmail.com>

RUN apt update && \
    apt install --assume-yes \
        build-essential \
        clang-6.0 \
        cmake \
        libboost-all-dev \
        ninja-build
