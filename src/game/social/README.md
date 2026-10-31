# The social registries (`social/`)

The social peer's registries of live objects, built and installed by the world as the session registry is:

| File | Holds |
|---|---|
| `GroupRegistry` | the world's groups by id, loaded from the character database at startup (`Load`, after `PackIds` renumbered their ids before the guid generators were seeded) and listed as they are formed or disbanded; reached through `sGroupRegistry`, which asserts when no registry is installed |
| `ArenaTeamRegistry` | the world's arena teams by id, loaded at startup (`Load`) and listed as they are created or disbanded, found by id, name or captain, and walked whole (`All`) by the weekly reset and a team's rank; reached through `sArenaTeamRegistry` |

The domain reaches them through the class headers it already includes: `Group.h` includes `GroupRegistry.h` and `ArenaTeam.h` includes `ArenaTeamRegistry.h`. A test that touches a group or a team installs its own registry (`src/tests/InstalledGroupRegistry.h`, `src/tests/InstalledArenaTeamRegistry.h`).
