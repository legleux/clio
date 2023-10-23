#!/usr/bin/env bash
set -ex

REF=$(git rev-parse --short HEAD)
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
RUN_DIR=$(basename $PWD)

echo "Script lives in $SCRIPT_DIR"
echo "Script run from: $PWD"
echo "Script run from dirname: $RUN_DIR"

if [[ ${RUN_DIR} == "docker" ]]; then
    cd ..
fi


echo "CLIO_GIT_REF=$REF" > $SCRIPT_DIR/.env
# build the clio builder image
#docker build . --target builder --tag clio-builder:$REF  --file docker/Dockerfile

# build clio
#docker build . --target build_clio --file docker/Dockerfile

# build the clio server image
#docker build .  --target clio --tag clio:$REF --file docker/Dockerfile

# tag it with the git tag if we're on a tagged commit
# git describe --exact-match --tags $(git log -n1 --pretty='%h')
# or
# git describe --tags # <-- returns last tag found... not quite what we want
