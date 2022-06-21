#!/bin/sh

NETWORK="${1:-main}"
export CONFIG="/opt/ripple/etc/rippled-${NETWORK}.cfg"
echo "${CONFIG} file being used."

/opt/ripple/bin/rippled --conf "${CONFIG}"
