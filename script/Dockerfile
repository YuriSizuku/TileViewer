ARG DOCKER_ARCH
FROM --platform=${DOCKER_ARCH} debian:buster
RUN apt-get -y update \
    && apt-get -y install make cmake tar p7zip git wget curl \
    && apt-get -y install build-essential \
    && apt-get -y install libgtk-3-dev libx11-dev