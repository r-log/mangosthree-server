# The data stores (`data/`)

The stores here hold world data loaded once from the world database at start-up (and again on a `.reload`, for a table that has one) and then only read, through lookups. A store is the container and its lookups: it names no entity, session, packet or database type, so the stores build and are tested in `mangos_tests` with no database. The loader that reads the table and validates its rows stays with `ObjectMgr` until the loaders move; `ObjectMgr` owns each store and its getter forwards to the lookup.

| Store | Holds |
|---|---|
| `MailLevelRewardStore` | the mail a character is sent on reaching a level (`mail_level_reward`): per level, the rewards in the order loaded, each for the races in its mask; `ObjectMgr::LoadMailLevelRewards` fills it and `ObjectMgr::GetMailLevelReward` forwards to its lookup |
| `ExplorationBaseXpStore` | the base XP of exploring an area of a given level (`exploration_basexp`): one value per area level, a second value for a level overwriting the first; `ObjectMgr::LoadExplorationBaseXP` fills it at start-up only (no `.reload` reads the table) and `ObjectMgr::GetBaseXP` forwards to its lookup |

Includes are path-qualified (`#include "data/MailLevelRewardStore.h"`): this directory is not on the `game` target's include path. `CheckLayout` classifies every file here as data (`src/tests/tools/layout_gate.py`'s `RULES`), and `CheckHeaderReach` keeps each store's header from reaching `ObjectMgr.h`, the entities, the session, the world, the database and the packets.
