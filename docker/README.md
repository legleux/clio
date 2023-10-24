Edit `docker/docker-compose.yml` with a volume containing your `config.json` for Clio.

From root of repo, `docker compose -f docker/docker-compose.yml`

## Query Clio

`curl 127.0.0.1:51233 --silent --data '{"method":"server_info"}' | jq '.result.info.cache'`

```
{
"size": 8155257,
"is_full": true,
"latest_ledger_seq": 30222329
}
```

    
## Query cassandra

The presence of the Clio keyspace confirms Clio can communicate with the database.

`docker exec cassandra bash -c "cqlsh -e 'DESC keyspaces;' "`

    clio    system_auth         system_schema  system_views
    system  system_distributed  system_traces  system_virtual_schema



`docker exec cassandra bash -c "cqlsh -e 'use clio; select * from ledger_range;' "`

Before Clio is ready, you should see:


    is_latest | sequence
    -----------+----------


When Clio finally syncs with `rippled`:

    is_latest | sequence
    -----------+----------
        False | 30202695
        True | 30207972

The sequence in `True` should match `rippled`'s latest ledger.


 
### TODO
- All the Conan dependencies need to be build with this CentOS container so they have an older GLIBC.
- Add the `rippled` dockerfile for `docker compose`
- Change `$NPROC` determination in `build_clio.sh`
- Build image during CI and upload to ghcr and/or Docker Hub tagged:
    - `master` as `clio:latest`, `clio:X.Y.Z` and `clio:X.Y`
    - `develop` as `clio:<gitref>` and `clio:develop`

- Configurable versions for Git/CMake/Conan/GCC (use config file)
- Script that uploads Conan dependencies to remote for CI step
- Run in user-defined bridge network (how to point `rippled` to Clio container without IP?)
- Target for building packages
