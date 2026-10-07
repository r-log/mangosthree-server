# The session (`session/`)

The session layer's own directory: the builders of the outgoing packets under `packets/<domain>/` and the opcode handler classes under `handlers/<domain>/`. The session's other files live outside it, and `src/tests/tools/layout_gate.py`'s `RULES` classify them as session too: `WorldSession`, `OpcodeTable`, `SessionMailbox`, `SessionProtocolPolicy`, `WorldGateway` and `WorldNetwork` in `Server/`, and in `WorldHandlers/` the opcode handler files (`SpellHandler` among them), `WorldSessionMgr`, `AccountMgr`, `GossipDef`, `UpdateData`, `LFGPackets` and the `ChatMessage*` files.

## `packets/`

One directory per domain. A builder is a free function, `void BuildXPacket(WorldPacket& packet, XFact const& fact)`, that initializes one outgoing packet from a typed fact and sends nothing. The code that reports the fact names no packet: it calls a `std::function` callback declared beside it (the six facts of a player's own client in `PlayerClientFacts.h`, `CooldownEventSink` and `CooldownsClearedSink` in `spells/SpellCooldownMgr.h`, `SpellModChangedSink` in `spells/SpellModMgr.h`), and the session supplies the callback, which builds the packet and sends it to the session that `GetSession()` returns at that send.

`PlayerPacketSinks` installs them: `InstallPlayerPacketSinks` gives a player every callback the session installs, the cooldown callbacks (through `InstallCooldownPacketSinks`), those of its own client, those of its group updates and that of its spell modifiers. Every place that creates a player calls it right after the construction, and `src/tests/tools/player_sinks.py` fails a `new Player(` with no install on the three lines after it.

`CheckStateOwnership` pins who may spell an opcode of the rows it lists: a file that spells a row's packet name without an allowance fails `ctest`. `spells/CooldownPackets.cpp` holds the cooldown row's allowance for `SMSG_COOLDOWN_EVENT` and `SMSG_CLEAR_COOLDOWNS`, and `spells/SpellModPackets.cpp` the spell modifier row's for `SMSG_SET_FLAT_SPELL_MODIFIER` and `SMSG_SET_PCT_SPELL_MODIFIER`.

## `handlers/`

One stateless class per domain, whose static entry points the rows of `Server/OpcodeTable.cpp` bind; `handlers/README.md` says what a handler class may keep and which gates hold it.

## What is here

| Directory | File | Holds |
|---|---|---|
| `packets/` | `PlayerPacketSinks` | `InstallPlayerPacketSinks`, and the templates it installs through: `FactToSession`, `SecurityLevelOfSession` and `ClientCallbacksToSession` |
| `packets/combat/` | `AttackSwingPackets` | `SMSG_ATTACKSWING_NOTINRANGE`, `SMSG_ATTACKSWING_BADFACING` and `SMSG_CANCEL_COMBAT`, from the swing out of reach, the swing facing away and the attack cancelled; no payload |
| `packets/entities/` | `PetGuidsPacket` | `SMSG_PET_GUIDS`, from the player's pet: the count, 1, then the pet's guid |
| `packets/entities/` | `StandStatePacket` | `SMSG_STANDSTATE_UPDATE`, from the player's stand state: one byte |
| `packets/spells/` | `AutoRepeatPackets` | `SMSG_CANCEL_AUTO_REPEAT`, from the auto-repeat spell cancelled: the target's packed guid |
| `packets/spells/` | `CooldownPackets` | `SMSG_COOLDOWN_EVENT` (the spell id, then the owner's guid) and `SMSG_CLEAR_COOLDOWNS` (the owner's bit-packed guid around the count and every spell id), from `SpellCooldownMgr`'s cooldown event and clear of every cooldown |
| `packets/spells/` | `CooldownPacketSinks` | `InstallCooldownPacketSinks`: the two cooldown callbacks, each building its packet with `CooldownPackets` and sending it to the player's session |
| `packets/spells/` | `SpellModPackets` | `SMSG_SET_FLAT_SPELL_MODIFIER` or `SMSG_SET_PCT_SPELL_MODIFIER`, from `SpellModMgr`'s modifier added or removed: the count of operations, 1, the count of pairs, the operation, then each effect bit and its sum as a float |
| `handlers/combat/` | `CombatHandlers` | the client's melee swing, attack stop, sheath and duel accept and cancel, and the `SMSG_ATTACKSTOP` the swing sends when it refuses a target |

## The layer and the threads

Session may include domain, data, motion, proto, persistence and foundation, and never scripts or app (`design/architecture.md`, section 1). `CheckLayout` classifies every source file here as session and fails an include from here that neither this rule nor a line on its allow-list, `src/tests/layout_allow.txt`, permits. `CheckHeaderReach` keeps each header here that has a rule from reaching the headers its rule names (`handlers/combat/CombatHandlers.h`'s names `WorldSession.h`, `Player.h`, `Unit.h`, `WorldPacket.h`, `ObjectMgr.h` and `World.h`). A header here is included by its path from outside its own directory (`#include "session/packets/PlayerPacketSinks.h"`): no directory here is on the `game` target's include path.

The network threads own the sockets and run the account lookup, never game state, and a decoded packet reaches the game only through the session's `SessionMailbox`. A handler runs on the world thread, except a `PROCESS_THREADSAFE` or `PROCESS_INPLACE` row of a player in the world, which may run on the map worker that owns the player's map; `design/architecture.md` section 5 gives the whole rule.
