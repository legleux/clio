# TODO:
1. Fix hardcoded ips
2. parameters for which network to join
3. Make configs inherit during docker build
4. rippled can't use DNS name in `[grpc_port]` section
5. No mounts


# Notes:
Set dosguard setting in clio config
Cass is connectable from host

# Useful host commands:
## Query rippled state

`docker exec rippled bash -c "/opt/ripple/bin/rippled --conf=/opt/ripple/etc/rippled-main.cfg server_info" | grep -E "complete_|state\"|peers|uptime"`

    Loading: "/opt/ripple/etc/rippled-main.cfg"
    2022-Jun-21 01:48:55.750449624 UTC HTTPClient:NFO Connecting to 127.0.0.1:5005

                "complete_ledgers" : "72479170-72479498",
                "peers" : 10,
                "server_state" : "full",
                "uptime" : 1595,

## Query running cassandra

`docker exec cassandra bash -c "cqlsh -e 'use clio; select * from ledgers;' "`

    sequence | header
    ----------+----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
    72479380 | 0x0451f29401633bdbe2254541588e3e7ff4dacd3e7db3f9dcf07015a0eb5199f0f9e2fe4ce85b646cefdb7a2826a31758801258162a5f5a301c14db232d268668e3ab70e43493f98b8f3b341b59228bc2847c76439a1f1bdb54e9b340f2d596c4c2a4424b7a92a7328e28d7222a43de362a43de370a00b8f7101d67be6fbb9bfaff50ff7a9e82d1197d1a7df28871fa23058c68f896e5

    (1 rows)
