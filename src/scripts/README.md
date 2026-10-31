# The scripts layer (`src/scripts/`)

Scripts are a layer of their own (`design/architecture.md`, sections 1, 6 and 7):

| Directory | Holds | Target |
|---|---|---|
| `surface/` | what a new script may call: the strong ids and handles (`Ids.h`) today, the views over handles as each family lands with its first caller; `surface/detail/` is the surface's own, never a script's | `scripts` |
| `dispatch/` | `Script::Dispatcher`, the one implementation of the core's hook interface (`data/ScriptHooks.h`), which `mangosd` installs: it hands each hook to the legacy library when it is built, loads the script texts and reports the bound script names no script serves | `scripts` |
| `content/` | new scripts, one directory per dungeon or zone; none yet | `scripts_content`, born with the first file |
| `legacy/` | SD3, which reaches the core through its compatibility layer: its precompiled header and the core headers its files include | `scripts_legacy`, optional behind `SCRIPT_LIB_SD3` |

**What a new script may include:** surface headers (not `surface/detail/`), the headers of its own `content/` directory, and the standard headers on the gate's list: containers, `<algorithm>`, `<functional>`, `<optional>`, `<string>` and the like. Threads, locks, atomics, `<random>`, the clocks and `rand()` are not on it; the map's time and seeded random numbers come through the surface.

**The gate:** `CheckScriptSurface` (`src/tests/tools/script_surface.py`, ctest `script_surface` and `script_surface_selftest`) fails when a content file or a surface header includes anything else, when a content object refers to a symbol a core library defines, and when a file outside `legacy/` includes a legacy header; `src/mangosd/LegacyScripts.cpp`, which hands the legacy library's hooks to the dispatcher, is the one exception. It also holds the legacy reach: `src/tests/script_legacy_reach.txt` lists one line per legacy file, kind (a call, a free call, an override, an include, a singleton, a cast) and core name, and a line the tree gains fails, as does a listed line it no longer has; `--generate` only ever drops lines, so a new reach is typed in by hand where a reviewer sees it. It refuses to pass when it finds no surface header, parses no core name, or reads no symbol from a core library. `CheckMotionMasterShim` holds legacy's movement calls.
