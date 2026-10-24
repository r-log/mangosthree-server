# The repositories (`persistence/`)

A repository spells the SQL of one table family and reads its rows: the game calls it with plain values and gets plain rows back, so the code that owns the state never names a statement. One directory per database and domain, `persistence/<database>/<domain>/`. A repository includes only the database driver and the foundation (`CheckLayout` classifies every file here as persistence, `src/tests/tools/layout_gate.py`'s `RULES`), so its rows are its own structs and never an entity or a manager.

| Repository | Holds |
|---|---|
| `characters/inventory/CurrencyRepository` | a character's currency rows (`character_currencies`): `ReadRow` reads one row of the login holder's currency result into a `CurrencyRow` for `CurrencyMgr::LoadRow`; `Update` and `Insert` write the row `CurrencyMgr::Save` hands them for a changed or a new currency; `DeleteEveryRowOf` deletes every character's row of a currency id the load does not know, all queued |
| `characters/social/ReputationRepository` | a character's reputation rows (`character_reputation`): `ReadRow` reads one row of the login holder's reputation result into a `ReputationRow` for `ReputationMgr::LoadRow`, and `Save` writes the rows `ReputationMgr::SaveToDB` hands it, each a DELETE of the old row and an INSERT of the new one, both queued |
| `characters/social/SocialRepository` | a character's friend and ignore list (`character_social`): `ReadRow` reads one row of the login holder's social result into a `SocialRow` for `PlayerSocial::LoadRow`; `PlayerSocial` calls `AddFlag` or `Insert` when it lists a character, `RemoveFlag` or `Delete` when it takes a flag off, and `SetNote` for a note, all queued |
| `characters/spells/SpellCooldownRepository` | a character's spell cooldowns (`character_spell_cooldown`): `ReadRow` reads one row of the login holder's cooldown result into a `SpellCooldownRow` for `SpellCooldownMgr::LoadRow`; `SpellCooldownMgr::SaveToDB` calls `DeleteAll`, then `Insert` for each cooldown it saves, all queued |

Includes are path-qualified (`#include "persistence/characters/social/ReputationRepository.h"`): this directory is not on the `game` target's include path. Every file here is on `CheckSyncDb`'s list, and each table is in its owner's `CheckStateOwnership` row, with the repository's file allowed to spell it.
