ARG DOCKER_ARCH
FROM --platform=${DOCKER_ARCH} debian:buster
RUN apt-get -y -qq update \
    && apt-get -y -qq install make cmake tar p7zip-full git wget curl \
    && apt-get -y -qq install build-essential \
    && apt-get -y -qq install libgtk-3-dev libx11-dev