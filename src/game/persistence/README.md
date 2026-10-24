# The repositories (`persistence/`)

A repository spells the SQL of one table family and reads its rows: the game calls it with plain values and gets plain rows back, so the code that owns the state never names a statement. One directory per database and domain, `persistence/<database>/<domain>/`. A repository includes only the database driver and the foundation (`CheckLayout` classifies every file here as persistence, `src/tests/tools/layout_gate.py`'s `RULES`), so its rows are its own structs and never an entity or a manager.

| Repository | Holds |
|---|---|
| `characters/social/ReputationRepository` | a character's reputation rows (`character_reputation`): `ReadRow` reads one row of the login holder's reputation result into a `ReputationRow` for `ReputationMgr::LoadRow`, and `Save` writes the rows `ReputationMgr::SaveToDB` hands it, each a DELETE of the old row and an INSERT of the new one, both queued |

Includes are path-qualified (`#include "persistence/characters/social/ReputationRepository.h"`): this directory is not on the `game` target's include path. Every file here is on `CheckSyncDb`'s list, and each table is in its owner's `CheckStateOwnership` row, with the repository's file allowed to spell it.
