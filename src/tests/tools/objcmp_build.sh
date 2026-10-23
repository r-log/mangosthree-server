#!/usr/bin/env bash
# objcmp_build.sh CONFIG SHA SIDE TREE BUILD OUT GATE
#
# Checks out SHA in the git work tree TREE, configures BUILD for CONFIG and builds it, then copies
# the objects of the built targets to OUT/SIDE at their build-relative paths and writes
# OUT/SIDE.objects (their list), OUT/SIDE.deps (`ninja -t deps` of them) and OUT/SIDE.seconds.
#
#   proof   GCC, PCH off, mangosd and realmd (game, the scripts and what they link), with
#           GATE/cmake/MangosProof.cmake injected, so a commit without it builds the same way.
#   normal  the Linux GCC job's configuration (PCH on, tests, tools), every target.
#
# Both leave the revision out (WITHOUT_GIT), so the one object that prints it is the same.
set -euo pipefail

config=$1 sha=$2 side=$3 tree=$4 build=$5 out=$6 gate=$7
start=$SECONDS

git -C "$tree" checkout --quiet --force --detach "$sha"

common=(
    -S "$tree" -B "$build" -G Ninja
    -DCMAKE_BUILD_TYPE=Release
    -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++
    -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
    -DWITHOUT_GIT=1 -DSCRIPT_LIB_SD3=1 -DBUILD_MANGOSD=1 -DBUILD_REALMD=1
)
case "$config" in
    proof)
        cmake "${common[@]}" -DBUILD_TOOLS=0 -DWITH_TESTS=0 -DWITH_NET_TESTS=0 -DPCH=0 \
            -DCMAKE_PROJECT_MaNGOS_INCLUDE="$gate/cmake/MangosProof.cmake" > "$out/$side.configure.log"
        targets=(mangosd realmd)
        ;;
    normal)
        cmake "${common[@]}" -DBUILD_TOOLS=1 -DWITH_TESTS=1 -DWITH_CRYPTO_TESTS=1 -DWITH_NET_TESTS=0 -DPCH=1 \
            -DMANGOS_STRICT_TICK=ON > "$out/$side.configure.log"
        targets=(all)
        ;;
    *)
        echo "unknown configuration: $config" >&2
        exit 2
        ;;
esac

cmake --build "$build" --target "${targets[@]}" --parallel

ninja -C "$build" -t inputs "${targets[@]}" | grep '\.o$' | sort -u > "$out/$side.objects"
mkdir -p "$out/$side"
(cd "$build" && xargs -a "$out/$side.objects" cp --parents -t "$out/$side")
(cd "$build" && xargs -a "$out/$side.objects" ninja -t deps) > "$out/$side.deps"
cp "$build/compile_commands.json" "$out/$side.compile_commands.json"
echo $((SECONDS - start)) > "$out/$side.seconds"
echo "$side: $(wc -l < "$out/$side.objects") objects from $sha in $((SECONDS - start)) s"
