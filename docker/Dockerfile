# FROM gcc:11.3.0
FROM ubuntu:20.04

ENV DEBIAN_FRONTEND=noninteractive

ARG BOOST_VERSION=1.75.0
ARG BOOST_VERSION_=1_75_0

ARG CMAKE_MAJ_VERSION=3.16
ARG CMAKE_POINT_VERSION=3
ARG CMAKE_VERSION="${CMAKE_MAJ_VERSION}.${CMAKE_POINT_VERSION}"
RUN apt-get update && apt-get install -y build-essential software-properties-common pkg-config libssl-dev curl gpg git zlib1g-dev  bison flex autoconf
## Why isn't this working?
# RUN curl -OJLs https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/cmake-${CMAKE_VERSION}-linux-x86_64.sh
# RUN chmod +x ./cmake-${CMAKE_VERSION}-Linux-x86_64.sh && ./cmake-${CMAKE_VERSION}-Linux-x86_64.sh --exclude-subdir --skip-license #&& ln -s /usr/bin/bash /bin/sh
RUN curl -OJLs https://cmake.org/files/v${CMAKE_MAJ_VERSION}/cmake-${CMAKE_VERSION}-Linux-x86_64.tar.gz
RUN tar xzvf cmake-${CMAKE_VERSION}-Linux-x86_64.tar.gz
RUN ln -s /cmake-${CMAKE_VERSION}-Linux-x86_64/bin/cmake /usr/local/bin/cmake

RUN add-apt-repository -y ppa:ubuntu-toolchain-r/test
RUN apt-get update && apt-get install -y gcc-11 g++-11
RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-11 110 \
                         --slave /usr/bin/g++ g++ /usr/bin/g++-11 \
                         --slave /usr/bin/gcov gcov /usr/bin/gcov-11 \
                         --slave /usr/bin/gcc-ar gcc-ar /usr/bin/gcc-ar-11 \
                         --slave /usr/bin/gcc-ranlib gcc-ranlib /usr/bin/gcc-ranlib-11

RUN curl -OJLs "https://boostorg.jfrog.io/artifactory/main/release/${BOOST_VERSION}/source/boost_${BOOST_VERSION_}.tar.gz"

RUN tar zxvf "boost_${BOOST_VERSION_}.tar.gz"
RUN cd boost_${BOOST_VERSION_} && ./bootstrap.sh && ./b2 -j$(nproc)
ENV BOOST_ROOT=/boost_${BOOST_VERSION_}
RUN git clone https://github.com/xrplf/clio.git

RUN apt-get install -y
ENV CC=/usr/bin/gcc-11
ENV CXX=/usr/bin/g++-11
RUN cmake -S clio -B clio/build && cmake --build clio/build --parallel $(nproc)
CMD ["/clio/build/clio_server", "/clio/example-config.json"]
