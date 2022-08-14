FROM gcc:11.3.0 AS deps
RUN apt-get update && apt-get install -y build-essential
ARG BOOST_VERSION_=1_75_0
ARG BOOST_VERSION=1.75.0
COPY docker/build_boost.sh .
RUN ./build_boost.sh ${BOOST_VERSION}
ENV BOOST_ROOT=/boost

FROM gcc:11.3.0 AS build
ENV BOOST_ROOT=/boost
COPY --from=deps /boost /boost
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y build-essential software-properties-common pkg-config libssl-dev curl gpg git zlib1g-dev  bison flex autoconf
COPY . /clio
## Install cmake
ARG CMAKE_VERSION=3.16.3
COPY ./docker/install_cmake.sh .
RUN ./install_cmake.sh ${CMAKE_VERSION}
ENV PATH="/opt/local/cmake/bin:$PATH"

## Build clio
RUN cmake -S clio -B clio/build && cmake --build clio/build --parallel $(nproc)
RUN mkdir output
RUN strip clio/build/clio_server && strip clio/build/clio_tests
RUN cp /clio/build/clio_tests /output && cp /clio/build/clio_server /output
RUN mv /clio/example-config.json /output/example-config.json

FROM ubuntu:20.04 AS clio
COPY --from=build /output /clio
RUN mkdir -p /opt/clio/etc && mv /clio/example-config.json /opt/clio/etc/config.json

CMD ["/clio/clio_server", "/opt/clio/etc/config.json"]
