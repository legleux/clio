#!/bin/sh

export CONFIG="/opt/ripple/etc/rippled-${NETWORK}.cfg"
/opt/ripple/bin/rippled --conf "${CONFIG}"
