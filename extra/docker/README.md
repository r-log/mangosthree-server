# Running MaNGOS Three with Docker

Two images are published for every push to `master` and for every release:

| Image | Tags |
|---|---|
| `ghcr.io/r-log/mangosthree-mangosd` | `nightly` (latest master build), `sha-<commit>`, `0.22.0`, `0.22`, `latest` |
| `ghcr.io/r-log/mangosthree-realmd` | same |

Both run as the unprivileged user `mangos` (uid 1000). Configuration lives in `/mangos/etc`,
client data (dbc/db2/maps/vmaps/mmaps) in `/mangos/data`, logs in `/mangos/logs`. On first start
the entrypoint copies the shipped `*.conf.dist` to `*.conf` if they are missing, then applies
these environment variables (only when set): `MANGOS_LOGIN_DB`, `MANGOS_WORLD_DB`,
`MANGOS_CHAR_DB` (`host;port;user;password;database`), `MANGOS_DATA_DIR`, `MANGOS_LOGS_DIR`.

## Quick start (compose)

1. `cp .env.example .env` and edit passwords, ports and the image tag.
2. Start the database: `docker compose up -d db`
3. Populate it from the [mangosthree-database](https://github.com/r-log/mangosthree-database)
   repository. From a checkout of that repository, run its installer against the container
   (`127.0.0.1`, port `DB_PORT`, user `root`, password `DB_ROOT_PASSWORD`):
   `./InstallDatabases.sh` — it creates the `realmd`, `character3` and `mangos3` databases and the
   `mangos` user. Alternatively `SOURCE` the files under `Realm/Setup`, `Character/Setup`,
   `World/Setup` and the `Updates` folders with `docker compose exec -T db mariadb -uroot -p<pw>`.
4. Extract the client data with the tools from a release zip and put the resulting `dbc`, `db2`,
   `maps`, `vmaps`, `mmaps` folders under `./data`.
5. `docker compose up -d`, then `docker compose logs -f mangosd`.
6. Point clients at the realm: in the `realmd` database set `realmlist.address` to the host's
   reachable IP (the default `127.0.0.1` only works on the same machine).

Configs are in `./etc` after the first start; edit them and `docker compose restart mangosd`.
Update: `docker compose pull && docker compose up -d`. Pin a version with `MANGOS_TAG=0.22.0`.
Attach to the world console: `docker attach mangosthree-mangosd-1` (detach with `Ctrl-P Ctrl-Q`).

## Building locally

`docker compose -f docker-compose.yml -f docker-compose.build.yml up -d --build` builds both
images from this checkout (`extra/docker/Dockerfile`, targets `mangosd` and `realmd`). A cold build
compiles the whole server once (both binaries share the build stage).
