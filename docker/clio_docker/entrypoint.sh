#!/usr/bin/bash

CONFIG="${1:-"/opt/clio/etc/config.json"}"
echo "${CONFIG} file being used."

/opt/clio/bin/clio_server "${CONFIG}"
