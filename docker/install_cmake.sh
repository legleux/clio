#!/usr/bin/env bash
set -ex

CMAKE_MAJ_VERSION="3"
CMAKE_MIN_VERSION="27"
CMAKE_PATCH_VERSION="7"
CMAKE_VERSION="${CMAKE_MAJ_VERSION}.${CMAKE_MIN_VERSION}.${CMAKE_PATCH_VERSION}"
CMAKE_SCRIPT="cmake-${CMAKE_VERSION}-linux-x86_64.sh"

curl -OJL "https://cmake.org/files/v${CMAKE_MAJ_VERSION}.${CMAKE_MIN_VERSION}/${CMAKE_SCRIPT}"
chmod +x "${CMAKE_SCRIPT}"

./"${CMAKE_SCRIPT}" --skip-license --prefix=/usr/local
rm "${CMAKE_SCRIPT}"
