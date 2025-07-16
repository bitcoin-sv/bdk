FROM golang:1.21.0-bullseye

# This docker file prepare environment to build the full bdk inside a docker
#
# To build it using one of these approaches
#
#      docker build -t bdk:dev .
#      docker buildx build --platform linux/amd64 -t bdkbuilder:amd . # uname -m
#      docker buildx build --platform linux/arm64 -t bdkbuilder:arm . # uname -m
#      docker buildx build --platform linux/amd64,linux/arm64 -t bdkbuilder:cross .
#
# To run and test it, assuming there are the bitcoin-sv source code along side with bdk
#
#      # $PWD is the parent/parent directory containing source code of both bitcoin-sv and bdk
#      docker run -it --name buildbdk --rm --mount type=bind,source="${PWD}",target=/development bdk:dev /bin/bash
#
# Then inside the docker
#
#      cd /development && mkdir dockerbuild && cd dockerbuild && cmake ../bitcoin-sv/bdk/ -DCUSTOM_SYSTEM_OS_NAME=docker && make -j8 && make install && make test && cpack -G TGZ
#
#

WORKDIR /download

# Install build dependencies
RUN apt-get update && apt-get install -y \
    wget p7zip-full \
    libssl-dev \
    build-essential \
    checkinstall \
    zlib1g-dev \
    python3 python3-pip python3-dev python-is-python3

# Install python packages for building documentation
RUN python -m pip install pytest mkdocs pymdown-extensions plantuml_markdown


# Build/Install from source CMake 3.30.3
RUN wget https://github.com/Kitware/CMake/releases/download/v3.30.3/cmake-3.30.3.tar.gz \
    && tar -xzvf cmake-3.30.3.tar.gz \
    && cd cmake-3.30.3 \
    && ./bootstrap --parallel=$(nproc) --no-qt-gui --prefix=/usr/local/cmake-3.30.3 \
    && make -j$(nproc) && make install

ENV PATH="/usr/local/cmake-3.30.3/bin:$PATH"

# Build from source OpenSSL 3.0.9 - static only
RUN wget https://www.openssl.org/source/openssl-3.0.9.tar.gz \
    && tar -xvzf openssl-3.0.9.tar.gz                        \
    && cd openssl-3.0.9                                      \
    && ./config no-shared --prefix=/usr/local/openssl-3.0.9  \
    && make -j$(nproc)                                       \
    && make install_sw

ENV OPENSSL_ROOT_DIR=/usr/local/openssl-3.0.9

# Build from source Boost 1.85.0 - static only
# We still have to build with ./b2 (instead of cmake) because bullseye only have g++10
# Which make issue when compiling boost with cmake
RUN wget https://boostorg.jfrog.io/artifactory/main/release/1.85.0/source/boost_1_85_0.tar.gz  \
    && tar -xvzf boost_1_85_0.tar.gz && cd boost_1_85_0                                        \
    && ./bootstrap.sh --prefix="/usr/local/boost_1_85_0" && ./b2 link=static cxxflags="-fPIC" cflags="-fPIC" --without-stacktrace -j$(nproc) --prefix="/usr/local/boost_1_85_0" install

ENV BOOST_ROOT=/usr/local/boost_1_85_0

## Cleanup the download
RUN rm -fR /download/*

## Config so git command work. It is needed to retrieve the source code version
RUN git config --global --add safe.directory '*'

WORKDIR /development