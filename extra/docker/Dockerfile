# syntax=docker/dockerfile:1
# MaNGOS Three -- one build, two images.
#
#   docker build -f extra/docker/Dockerfile --target mangosd -t mangosthree-mangosd .
#   docker build -f extra/docker/Dockerfile --target realmd  -t mangosthree-realmd  .
#
# Both servers are compiled once in the `build` stage; each runtime target copies only
# what it needs. Every apt step is a single layer without `apt-get upgrade`: the package
# index and the packages are fetched together, so a mirror mid-sync cannot break the
# build later from a stale cached index (which is what took the old images down).

FROM ubuntu:22.04 AS build
ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential clang cmake ninja-build ca-certificates \
        libssl-dev libbz2-dev libmysqlclient-dev zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY . .
# Same feature set as the Linux CI build. WITHOUT_GIT because .dockerignore excludes .git;
# the OCI revision label carries the commit instead. CONF_INSTALL_DIR is explicit because
# its default `../etc` resolves to /etc when the prefix is /mangos.
RUN cmake -S . -B build -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
        -DCMAKE_INSTALL_PREFIX=/mangos -DCONF_INSTALL_DIR=/mangos/etc -DWITHOUT_GIT=1 \
        -DBUILD_MANGOSD=1 -DBUILD_REALMD=1 -DBUILD_TOOLS=0 \
        -DSOAP=1 -DSCRIPT_LIB_SD3=1 -DPCH=1 \
    && cmake --build build --parallel \
    && cmake --install build

FROM ubuntu:22.04 AS runtime-base
ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
        libmysqlclient21 libssl3 libbz2-1.0 zlib1g ca-certificates tzdata gosu \
    && rm -rf /var/lib/apt/lists/* \
    && groupadd -g 1000 mangos \
    && useradd -u 1000 -g mangos -d /mangos -s /usr/sbin/nologin mangos \
    && mkdir -p /mangos/bin /mangos/etc /mangos/data /mangos/logs /mangos/dist \
    && chown -R mangos:mangos /mangos
ENV TZ=Etc/UTC \
    MANGOS_DATA_DIR=/mangos/data \
    MANGOS_LOGS_DIR=/mangos/logs
COPY --chmod=755 extra/docker/docker-entrypoint.sh /usr/local/bin/docker-entrypoint.sh
VOLUME ["/mangos/etc", "/mangos/data", "/mangos/logs"]
WORKDIR /mangos/bin
ENTRYPOINT ["docker-entrypoint.sh"]

FROM runtime-base AS mangosd
COPY --from=build --chown=mangos:mangos /mangos/bin/mangosd /mangos/bin/
COPY --from=build /mangos/etc/mangosd.conf.dist /mangos/etc/ahbot.conf.dist /mangos/dist/
RUN if ldd /mangos/bin/mangosd | grep 'not found'; then echo 'mangosd: unresolved shared libraries' >&2; exit 1; fi
EXPOSE 8085
CMD ["mangosd"]

FROM runtime-base AS realmd
COPY --from=build --chown=mangos:mangos /mangos/bin/realmd /mangos/bin/
COPY --from=build /mangos/etc/realmd.conf.dist /mangos/dist/
RUN if ldd /mangos/bin/realmd | grep 'not found'; then echo 'realmd: unresolved shared libraries' >&2; exit 1; fi
EXPOSE 3724
CMD ["realmd"]
