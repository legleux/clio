#!/usr/bin/env sh

docker run --rm -it \
    --network=clio \
    -p 5005 \
    --name rippled4clio \
    rippled_clio_etl bash
