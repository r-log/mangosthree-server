#!/usr/bin/env bash
# Entry point for the mangosd / realmd images.
#
# Starts as root only long enough to seed the configuration into /mangos/etc -- which is
# usually a bind mount the host created, owned by root or by the host user -- and to make
# the logs directory writable; then drops to the unprivileged `mangos` user with gosu.
#
#   docker-entrypoint.sh mangosd [extra mangosd args]
#   docker-entrypoint.sh realmd  [extra realmd args]
#   docker-entrypoint.sh <anything else>   -> exec'd unchanged (debug shells, one-offs)
#
# Environment overrides (applied on every start, only when non-empty):
#   MANGOS_LOGIN_DB   -> LoginDatabaseInfo      (mangosd, realmd)
#   MANGOS_WORLD_DB   -> WorldDatabaseInfo      (mangosd)
#   MANGOS_CHAR_DB    -> CharacterDatabaseInfo  (mangosd)
#   MANGOS_DATA_DIR   -> DataDir                (mangosd; image default /mangos/data)
#   MANGOS_LOGS_DIR   -> LogsDir                (mangosd, realmd; image default /mangos/logs)
set -euo pipefail

MANGOS_HOME="${MANGOS_HOME:-/mangos}"
daemon="${1:-}"

case "$daemon" in
    mangosd|realmd) ;;
    *) exec "$@" ;;
esac

etc="$MANGOS_HOME/etc"
conf="$etc/$daemon.conf"
mkdir -p "$etc"

# 1. Seed every shipped *.conf.dist that has no *.conf yet. Never overwrite an existing
#    file. Seeded files take the owner of the etc directory, so a host user who mounted
#    their own directory can edit them without sudo.
for dist in "$MANGOS_HOME"/dist/*.conf.dist; do
    [ -e "$dist" ] || continue
    target="$etc/$(basename "${dist%.dist}")"
    if [ ! -e "$target" ]; then
        cp "$dist" "$target"
        chown --reference="$etc" "$target" 2>/dev/null || true
        echo "entrypoint: seeded $(basename "$target")"
    fi
done

# 2. Environment overrides. The value travels through the environment, not through
#    awk -v, so backslashes and other special characters in passwords survive intact.
#    The rewrite goes through a temp file and `cat` so a single-file bind mount keeps
#    its inode.
set_key() {   # set_key <file> <Key> <value>
    local file="$1"
    KEY="$2" VALUE="$3" awk '
        BEGIN { k = ENVIRON["KEY"]; v = ENVIRON["VALUE"]; done = 0 }
        !done && substr($0, 1, length(k)) == k && substr($0, length(k) + 1) ~ /^[[:space:]]*=/ {
            print k " = \"" v "\""; done = 1; next
        }
        { print }
        END { if (!done) print k " = \"" v "\"" }
    ' "$file" > "$file.tmp"
    cat "$file.tmp" > "$file"
    rm -f "$file.tmp"
}
apply() {   # apply <ENV_VAR> <Key>
    local value="${!1:-}"
    if [ -n "$value" ]; then
        set_key "$conf" "$2" "$value"
    fi
}
apply MANGOS_LOGIN_DB LoginDatabaseInfo
apply MANGOS_LOGS_DIR LogsDir
if [ "$daemon" = mangosd ]; then
    apply MANGOS_WORLD_DB WorldDatabaseInfo
    apply MANGOS_CHAR_DB  CharacterDatabaseInfo
    apply MANGOS_DATA_DIR DataDir
fi

# 3. The daemon must be able to write its logs. A bind mount Compose just created is
#    root-owned; hand it to mangos. Data is never chowned (large, may be read-only).
logs="${MANGOS_LOGS_DIR:-$MANGOS_HOME/logs}"
mkdir -p "$logs"
if [ "$(stat -c %u "$logs")" = 0 ]; then
    chown mangos:mangos "$logs"
fi

shift
if [ "$(id -u)" = 0 ] && command -v gosu >/dev/null; then
    exec gosu mangos "$MANGOS_HOME/bin/$daemon" -c "$conf" "$@"
fi
exec "$MANGOS_HOME/bin/$daemon" -c "$conf" "$@"
