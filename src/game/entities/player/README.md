# The character (`Player`)

`Player.h` and `Player.cpp` hold the class itself: construction, the update loop, the fields and the glue between the parts below. `PlayerRegistry` finds the online characters by guid or name. Everything else the character does lives in one directory per concern:

| Directory | Concern |
|---|---|
| `persistence/` | loading the character from the database, saving it, and offline lookups through the character cache |
| `quests/` | the quest log, quest status and the rules for taking and completing a quest |
| `talents/` | talents, specs and glyphs |
| `spells/` | the spell book, spell modifiers, learning, action buttons, cooldowns and runes |
| `inventory/` | items: storage, equipping and its stat effects, enchants and gems, equipment sets, durability, gear score, and currencies |
| `interaction/` | what the character does with NPCs and the post: gossip, vendors, loot and mail |
| `combat/` | melee, combo points, death and resurrection, duels, derived stats, regeneration, and the XP and honor rewarded at kills and group events |
| `pvp/` | the PvP flag and contested state, battlegrounds and honor |
| `social/` | groups, channels, chat, reputation, and the friend and ignore lists |
| `world/` | movement, zones and areas, area triggers, instances, visibility, rested XP, breath and fatigue timers and environmental damage, and taxi routes |
| `pets/` | the character's pets, their action bars and the pet cache read at login |

Includes name the file only (`#include "Player.h"`, `#include "TalentMgr.h"`): every directory here is on the `game` target's include path, so a header's name must stay unique across that path.

## Managers and the owner side

`src/tests/CheckManagerIsolation.cmake` puts every file in this tree, subdirectories included, on exactly one of two lists; a file on neither fails `ctest`.

- **`MANAGER_FILES`** are managers: a piece of the character's state and the rules over it, built and tested in `mangos_tests` without a character. A manager never names the character class: no include of `Player.h`, no pointer, reference, member or alias of the class, and not the word itself, comments included. It takes everything it needs (guid, name, game time, template lookups) as parameters. `src/tests/CheckHeaderReach.cmake` also keeps each manager's includes from reaching `Player.h`, `Unit.h`, the session or the object manager.
- **`OWNER_FILES`** are the character's own side: `Player.h`, `Player.cpp`, every `Player*.cpp`, `PlayerRegistry`, `PlayerTaxi`, `PlayerPetCache` and this README, plus the older managers that still name the class until each one is converted.

The owner side of a concern lives in the same directory as its manager: `quests/PlayerQuest.cpp` drives `quests/QuestStatusMgr`, and `talents/PlayerTalent.cpp` drives `talents/TalentMgr`.
