FROM gcc:11.3.0 AS deps
# FROM ubuntu:20.04 AS build

# ENV DEBIAN_FRONTEND=noninteractive

# RUN apt-get update && apt-get install -y build-essential software-properties-common pkg-config libssl-dev curl gpg git zlib1g-dev  bison flex autoconf
RUN apt-get update && apt-get install -y build-essential
# COPY install_compilers.sh .
# # ARG GCC_VERSION=11
# RUN ./install_compilers.sh gcc-11

ARG BOOST_VERSION_=1_75_0
ARG BOOST_VERSION=1.75.0

COPY build_boost.sh .
RUN ./build_boost.sh ${BOOST_VERSION}
ENV BOOST_ROOT=/boost
# RUN curl -OJLs "https://boostorg.jfrog.io/artifactory/main/release/${BOOST_VERSION}/source/boost_${BOOST_VERSION_}.tar.gz"
# RUN tar zxvf "boost_${BOOST_VERSION_}.tar.gz"
# WORKDIR boost_${BOOST_VERSION_}
# RUN ./bootstrap.sh && ./b2 -j$(nproc)
# RUN mkdir -p $BOOST_ROOT/boost && mv boost $BOOST_ROOT && mv stage $BOOST_ROOT && rm -rf /boost_${BOOST_VERSION_}

FROM gcc:11.3.0 AS build
ENV BOOST_ROOT=/boost
COPY --from=deps /boost /boost
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y build-essential software-properties-common pkg-config libssl-dev curl gpg git zlib1g-dev  bison flex autoconf
RUN apt-get clean
RUN git clone https://github.com/xrplf/clio.git

# ENV CC=/usr/bin/gcc-11
# ENV CXX=/usr/bin/g++-11
RUN apt-get install -y vim
ARG CMAKE_VERSION=3.16.3
COPY install_cmake.sh .
RUN ./install_cmake.sh ${CMAKE_VERSION}
ENV PATH="/opt/local/cmake/bin:$PATH"

RUN cmake -S clio -B clio/build && cmake --build clio/build --parallel $(nproc)
RUN mkdir output
RUN strip clio/build/clio_server
RUN strip clio/build/clio_tests
RUN cp /clio/build/clio_tests /output
RUN cp /clio/build/clio_server /output
RUN cp /clio/example-config.json /output

FROM ubuntu:20.04 AS clio
COPY --from=build /output /clio
CMD ["/clio/clio_server", "/clio/example-config.json"]
