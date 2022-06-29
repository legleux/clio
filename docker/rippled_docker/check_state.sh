#!/usr/bin/env bash

/opt/ripple/bin/rippled \
    --conf "${CONFIG}" server_info | \
    jq '.result.info | {"Version":.build_version, "Complete ledgers": .complete_ledgers, "Server state":.server_state, "Peers":.peers, "uptime":.uptime}'
