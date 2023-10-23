#!/usr/bin/env bash
set -ex

REF=$(git rev-parse --short HEAD)

if [[ $(basename $PWD) == 'docker' ]]; then
    cd ..
fi

# build the clio builder image
#docker build . --target builder --tag clio-builder:$REF  --file docker/Dockerfile

# build clio
docker build . --target build_clio --file docker/Dockerfile

# build the clio server image
docker build .  --target clio --tag clio:$REF --file docker/Dockerfile

echo "CLIO_GIT_REF=$REF" > .env
# tag it with the git tag if we're on a tagged commit
# git describe --exact-match --tags $(git log -n1 --pretty='%h')
# or
# git describe --tags # <-- returns last tag found... not quite what we want
