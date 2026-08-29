#!/bin/bash
set -e

here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
. "$here/../ci/codestyle-scope.sh"
repo_root=$(cd "$here/../../.." && pwd)

prune=()
for dir in "${codestyle_excludes[@]}"; do
    prune+=(-name "$dir" -prune -o)
done

find "$repo_root/$codestyle_root" "${prune[@]}" \
    \( -name '*.cpp' -o -name '*.h' \) -print0 |
    xargs -r0 sed -i -e 's/[[:blank:]]\+$//'
