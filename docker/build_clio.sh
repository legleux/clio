#!/usr/bin/env bash
set -ex

BUILD_DIR=build
BUILD_CONFIG=Release
TESTS=False
NPROC=24

source /opt/rh/devtoolset-11/enable
source /opt/rh/rh-python38/enable

pip install "conan<2"
conan remote add --insert 0 conan-non-prod http://18.143.149.228:8081/artifactory/api/conan/conan-non-prod || true
# TODO: adjust --build parameters when remote is updated with binaries built from
conan install . \
    --build b2  \
    --build missing \
    --output-folder ${BUILD_DIR} \
    --options tests=${TESTS} \
    --options "xrpl/*:rocksdb=False" \
    --settings build_type=${BUILD_CONFIG}

cmake . -B ${BUILD_DIR}    \
    -DCMAKE_TOOLCHAIN_FILE:FILEPATH=${BUILD_DIR}/build/generators/conan_toolchain.cmake \
    -DCMAKE_BUILD_TYPE=${BUILD_CONFIG}

cmake --build ${BUILD_DIR} --target install/strip --parallel $NPROC
