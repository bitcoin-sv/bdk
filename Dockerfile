FROM golang:1.21.0-bullseye

WORKDIR /download

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    checkinstall \
    zlib1g-dev \
    python3 python3-pip python3-dev python-is-python3  \
    openjdk-11-jdk \
    g++ \
    wget

# Finalize setup java jdk
ENV JAVA_HOME=/usr/lib/jvm/java-11-openjdk-amd64
ENV PATH="$JAVA_HOME/bin:$PATH"

# Install python packages for building documentation
RUN python -m pip install pytest mkdocs pymdown-extensions plantuml_markdown

# Install prebuilt CMake 3.30.0
RUN wget https://github.com/Kitware/CMake/releases/download/v3.30.0/cmake-3.30.0-linux-x86_64.tar.gz \
    && tar -xvzf cmake-3.30.0-linux-x86_64.tar.gz                                                    \
    && mv cmake-3.30.0-linux-x86_64 /usr/local/cmake-3.30.0

ENV PATH="/usr/local/cmake-3.30.0/bin:$PATH"

# Build from source Boost 1.78.0 - static only
RUN wget https://boostorg.jfrog.io/artifactory/main/release/1.78.0/source/boost_1_78_0.tar.gz  \
    && tar -xvzf boost_1_78_0.tar.gz && cd boost_1_78_0                                        \
    && ./bootstrap.sh && ./b2 link=static cxxflags="-fPIC" cflags="-fPIC" --with=all -j8 install

# Build from source OpenSSL 3.0.9 - static only
RUN wget https://www.openssl.org/source/openssl-3.0.9.tar.gz \
    && tar -xvzf openssl-3.0.9.tar.gz                        \
    && cd openssl-3.0.9                                      \
    && ./config no-shared --prefix=/usr/local/openssl-3.0.9  \
    && make -j4                                              \
    && make install_sw

ENV OPENSSL_ROOT_DIR=/usr/local/openssl-3.0.9

WORKDIR /development

## Config so git command work. It is needed to retrieve the source code version
RUN git config --global --add safe.directory '*'