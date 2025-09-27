ARG DOCKER_ARCH
FROM --platform=${DOCKER_ARCH} debian:buster
RUN sed -i 's/deb.debian.org/archive.debian.org/g' /etc/apt/sources.list && \
    sed -i 's/security.debian.org/archive.debian.org/g' /etc/apt/sources.list && \
    sed -i '/buster-updates/d' /etc/apt/sources.list
RUN apt-get -y -qq update \
    && apt-get -y -qq install make cmake tar p7zip-full git wget curl \
    && apt-get -y -qq install build-essential \
    && apt-get -y -qq install libgtk-3-dev libx11-dev