#!/usr/bin/env bash
set -ex

REF=$(git rev-parse --short HEAD)

# build the clio builder image
docker build . --target builder --tag clio-builder:10-17  --file docker/Dockerfile

# build the clio server image
docker build .  --target clio --tag clio:$REF --file docker/Dockerfile

# tag it with the git tag if we're on a tagged commit
# git describe --exact-match --tags $(git log -n1 --pretty='%h')
# or
# git describe --tags # <-- returns last tag found... not quite what we want
