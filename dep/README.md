# Vendored dependencies

Plain in-tree copies of the third-party libraries the core builds. They were imported from
the `mangos/mangosDeps` repository at commit `e3156dcba0bfb6b199327529f7c032fedc359d44`
(2026-08-04) when this tree stopped using git submodules. Nothing here is modified
relative to that commit except line endings: this repository normalises text files to LF
(`.gitattributes`), which touched the CRLF sources StormLib ships. `cmake/Dependencies.cmake` is the single place that decides which
of them is configured and when — there is no `CMakeLists.txt` in this directory.

| Directory | Library | Version | Upstream | Used by |
|---|---|---|---|---|
| `zlib/` | zlib | 1.2.13 | https://zlib.net | packet compression (`shared`, `game`, tools) |
| `bzip2/` | bzip2 | 1.0.6 | https://sourceware.org/bzip2/ | StormLib (extractor tools) |
| `StormLib/` | StormLib | 9.26 | https://github.com/ladislav-zezula/StormLib | MPQ reading in the extractor tools (`BUILD_TOOLS`) |
| `recastnavigation/` | Recast & Detour | mangosDeps snapshot | https://github.com/recastnavigation/recastnavigation | navmesh baking (tools) and pathfinding (`game`) |
| `utf8cpp/` | utfcpp | 4.1.1 | https://github.com/nemtrif/utfcpp | UTF-8 validation for names and chat (`shared`) |

Libraries that mangosDeps also carried and this tree does not use were not imported:
gsoap (SOAP support was removed), lualib (the Eluna scripting engine was removed), and
the forum icons.

To update a library, replace its directory with the new upstream release and record the
version here; do not patch in place without saying so in this file.
