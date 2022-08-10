# Running Clio in Docker


To build, just run docker build . -t clio_server in this directory.
By default, running thi container will require a running cassandra instance and rippled.
To override the configuration in the container, mount the directory with your config at `/etc/opt/clio`
i.e. `docker run --network=host -v $PWD/cfg:/etc/opt/clio clio_server`

Use the docke-compose.yml to get a clio instance running with rippled, cassandra.
`docker compose up`
This will create a docker network where the services can all connect to each other.

## Query rippled state

`docker exec rippled bash -c "/opt/ripple/bin/rippled --conf=/opt/ripple/etc/rippled-main.cfg server_info" | grep -E "complete_|state\"|peers|uptime"`

    Loading: "/opt/ripple/etc/rippled-main.cfg"
    2022-Jun-21 01:48:55.750449624 UTC HTTPClient:NFO Connecting to 127.0.0.1:5005

                "complete_ledgers" : "72479170-72479498",
                "peers" : 10,
                "server_state" : "full",
                "uptime" : 1595,

## Query running cassandra

`docker exec cassandra bash -c "cqlsh -e 'use clio; select * from ledger_range;' "`


 is_latest | sequence
-----------+----------
     False | 30202695
      True | 30207415
