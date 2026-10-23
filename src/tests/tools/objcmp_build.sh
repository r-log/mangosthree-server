#!/usr/bin/env bash
# objcmp_build.sh CONFIG SHA SIDE TREE BUILD OUT GATE
#
# Checks out SHA in the git work tree TREE, deletes BUILD, configures it for CONFIG and builds every
# target, then copies the objects of the build to OUT/SIDE at their build-relative paths and writes
# OUT/SIDE.objects (their list), OUT/SIDE.deps (`ninja -t deps` of them), OUT/SIDE.compile_commands.json,
# OUT/SIDE.configure.log and OUT/SIDE.seconds. In the proof configuration, when OUT/changed-lines.txt
# exists, it also writes OUT/SIDE.lines: which changed lines of this side the build compiled.
#
#   proof   GCC, PCH off, every target (the servers, the scripts, the tests and the tools), with
#           GATE/cmake/MangosProof.cmake injected, so a commit without it builds the same way.
#   normal  the Linux GCC job's configuration (PCH on, tests, tools), every target.
#
# Both leave the revision out (WITHOUT_GIT), so the one object that prints it is the same. ccache,
# when present, runs in its preprocessor mode: its direct and depend modes cannot see a new header
# that would now be found first on the include path.
set -euo pipefail

config=$1 sha=$2 side=$3 tree=$4 build=$5 out=$6 gate=$7
start=$SECONDS

git -C "$tree" checkout --quiet --force --detach "$sha"
rm -rf "$build"
mkdir -p "$out"

export CCACHE_NODIRECT=1 CCACHE_NODEPEND=1
launcher=()
if command -v ccache > /dev/null; then
    launcher=(-DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache)
fi

common=(
    -S "$tree" -B "$build" -G Ninja
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++
    "${launcher[@]}"
    -DWITHOUT_GIT=1 -DSCRIPT_LIB_SD3=1 -DBUILD_MANGOSD=1 -DBUILD_REALMD=1 -DBUILD_TOOLS=1 -DWITH_TESTS=1
    -DWITH_CRYPTO_TESTS=1
)
case "$config" in
    proof)
        cmake "${common[@]}" -DWITH_NET_TESTS=1 -DPCH=0 \
            -DCMAKE_PROJECT_MaNGOS_INCLUDE="$gate/cmake/MangosProof.cmake" > "$out/$side.configure.log"
        ;;
    normal)
        cmake "${common[@]}" -DWITH_NET_TESTS=0 -DPCH=1 -DMANGOS_STRICT_TICK=ON > "$out/$side.configure.log"
        ;;
    *)
        echo "unknown configuration: $config" >&2
        exit 2
        ;;
esac

cmake --build "$build" --parallel

ninja -C "$build" -t inputs all | grep '\.o$' | sort -u > "$out/$side.objects"
mkdir -p "$out/$side"
(cd "$build" && xargs -a "$out/$side.objects" cp --parents -t "$out/$side")
(cd "$build" && xargs -a "$out/$side.objects" ninja -t deps) > "$out/$side.deps"
cp "$build/compile_commands.json" "$out/$side.compile_commands.json"
if [ "$config" = proof ] && [ -f "$out/changed-lines.txt" ]; then
    python3 "$gate/src/tests/tools/objcmp.py" --lines-compiled "$side" --changed-lines "$out/changed-lines.txt" \
        --deps "$out/$side.deps" --compile-db "$build/compile_commands.json" --root "$tree" --build "$build" \
        --out "$out/$side.lines"
fi
echo $((SECONDS - start)) > "$out/$side.seconds"
echo "$side: $(wc -l < "$out/$side.objects") objects from $sha in $((SECONDS - start)) s"
