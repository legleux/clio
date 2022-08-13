#!/usr/bin/env bash
set -exu

BOOST_VERSION=$1
BOOST_VERSION_=$(echo ${BOOST_VERSION} | tr . _)
echo "BOOST_VERSION: ${BOOST_VERSION}"
echo "BOOST_VERSION_: ${BOOST_VERSION_}"
curl -OJLs "https://boostorg.jfrog.io/artifactory/main/release/${BOOST_VERSION}/source/boost_${BOOST_VERSION_}.tar.gz"
tar zxvf "boost_${BOOST_VERSION_}.tar.gz"
cd boost_${BOOST_VERSION_} && ./bootstrap.sh && ./b2 -j$(nproc)
mkdir -p /boost && mv boost /boost && mv stage /boost
