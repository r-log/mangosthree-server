# The target architecture

Measured on master 0bbab743b and decided by the maintainer on 2026-09-28. A **Today:** line marks where the tree
differs from the target. Section 8 lists each divergence with the phase that closes it. Every number comes from a
command in the appendix. Sections 5 and 6 (the threads, the build targets) were added on 2026-09-29, measured on
master 7b6a481ce.

## 1. Layers and the include direction

A layer includes only what its row allows, and nothing includes upward. The chain
`proto -> session -> entities/spells/combat/maps -> motion` is the path a packet takes, not the include direction:
proto is the wire library near the bottom that session and motion build on. The tree agrees: `src/proto` includes
only `src/shared`; session includes it 116 times, the motion layer 23 times (8 from the kernel's writers, 15 from
`game/movement`).

| Layer | Directories in the target | May include |
|---|---|---|
| app | `mangosd/`, the world tick (`World`), `Harness/` | everything |
| scripts | `scripts/` (#83): `surface/`, what a script may call; `dispatch/`, the one implementation of the hook interface in `data/` through which the core reaches the scripts; `content/`, the new scripts; `legacy/`, SD3 (today `modules/SD3/`). `ChatCommands/`, the command-dispatch half of `Chat`, `CommandMgr`, `ScriptMgr` | everything below app; a file in `content/` only the surface |
| session | `session/`: `WorldSession`, `OpcodeTable`, gateway, `handlers/<domain>/` and `handlers/connection/`, `packets/<domain>/`, the message builders split from `Chat` | domain, data, motion, proto, persistence, foundation |
| domain | peers: `entities/`, `spells/`, `combat/`, `maps/`, `ai/`, `social/`, `pvp/`, `economy/` | data, motion, persistence, foundation (**not** proto) |
| data | `data/`: DBC/DB2 and SQL stores, `ObjectMgr`'s stores, `QuestDef`, `SharedDefines`, pools, events, the read-only configuration interface (#143), the script hook interface (`ScriptHooks`, what the core calls on the script library, #83), the command hook interface (`CommandHooks`, what the core asks of the GM command system, #83), the database script hook interface (`DbScriptHooks`, what the domain asks of the database script engine, #83), the chat packet builder the world installs (`ChatPacketBuilder`, #83) | persistence, foundation |
| motion | `src/motion` (the kernel), `movement/` (splines and the movement record) | proto, foundation |
| proto | `src/proto` (the wire: `WorldPacket`, `Opcodes`, codecs) | foundation |
| persistence | `src/shared/Database` (the driver), `persistence/<domain>/` (repositories: namespaces of free functions that return plain rows) | foundation |
| foundation | the rest of `src/shared`, `Time/`, `ObjectGuid` (a value type) | nothing |

`realmd` is a separate program and uses only foundation and persistence. `tests` may include anything.
`AuctionHouseBot/` is app: it is kept (the 2026-09-28 decision, section 7), off by default, and reaches the domain only through `World`'s timer and one chat command.

These directories **dissolve**:
- `Object/` (137 files): 64 to entities, 28 to data, 25 to ai, 10 to social, 6 to economy, 2 to combat and 2 to
  foundation (`ObjectGuid`).
- `WorldHandlers/` (133 files): 32 to session (the 24 handler files, `GossipDef`, `UpdateData`, `LFGPackets`,
  `AccountMgr`, `WorldSessionMgr`), 40 to maps, 26 to social, 12 to scripts, 12 to data, 4 to
  economy, 4 to entities (achievements) and 3 to app.
- `Server/`: 11 files to session, 15 to data and `GameGlobals` to persistence. `References/`: maps to maps,
  groups to social; the threat managers are in `combat/`. `Tools/`: the dump and the cleaner to persistence, `Language.h` to data.
- `MotionGenerators/` (the unit's shell over the kernel) goes to entities.

**Today:** 305 include lines go against the table.

| From -> to | Lines | Mostly | Three includers |
|---|---|---|---|
| domain -> proto | 127 | `WorldPacket.h` 79, `Opcodes.h` 47 | `Unit.cpp`, `Player.cpp`, `Creature.cpp` |
| domain -> session | 82 | `WorldSession.h` 66, `UpdateData.h` 10, `GossipDef.h` 5 | `Player.cpp`, `PlayerQuest.cpp`, `Unit.cpp` |
| any -> app | 26 | `World.h`: scripts 10, session 7, domain 5, data 3, foundation 1 | `Creature.cpp`, `WorldGateway.cpp`, `ObjectMgr.cpp` |
| data -> domain | 58 | `MapManager.h` 5, `MapPersistentStateMgr.h` 5, `SpellMgr.h` 5, `Creature.h` 5 | `ObjectMgr.cpp`, `GameEventMgr.cpp`, `CreatureLinkingMgr.cpp` |
| rest | 12 | `ChatCommands` -> `AuctionHouseBot.h`/`Harness.h` 3; `PlayerDump`/`CharacterDatabaseCleaner` 7; `GameEventMgr` -> `WorldPacket.h` 1; `ObjectGuid.cpp` -> `ObjectMgr.h` 1 | |

The domain directories are peers. Include lines that cross between them are allowed, but they cannot spread: they are
a `CheckLayout` ratchet, keyed per includer directory, includer peer and header. The peer is the coupling unit; the
directory stands in for it until `Object/`, `WorldHandlers/` and `References/` dissolve, when the key collapses to
directory and header. A new file of a listed peer in a listed directory that includes a listed header adds no key, so
a file can split inside its directory; a new key fails. Only two seams are gated, `entities/player` and
`spells/aura`, from both sides: an include into either from another peer directory and from the seam's own peer
(`Object/Unit.cpp` -> `Player.h`, 34 lines; `SpellMgr.h` -> `SpellAuraDefines.h`, 16 lines; the 2026-09-28 decision)
is listed per includer file and header and cannot grow, and so is an include against the table. Such a line may
change its includer file in the PR that deletes the old one or removes the include from it, only within the same layer pair and header; a change of
layer pair or header is a new edge. A file that reached a header only through a header a PR cuts may include it directly in that
PR: the line is listed with the reason `reached through <header> before #<PR>`, and the PR shows that the file's include closure
reached the header at the parent commit. Any other new edge is fixed, not listed.

**Today:** 849 such lines, the largest being spells -> entities 175, entities -> maps 151, -> spells 50, -> social 38
and -> pvp 28, spells -> maps 63 and pvp -> entities 55.

**Which peers may know which (decided 2026-10-02).** The ratchet above keeps the cross lines from growing; it does
not say which of them are the design and which are debt. This table does. A pair not listed is debt: its ratchet
aims at zero, and the domain tier splits into per-directory libraries along these rows once the unlisted pairs are
gone, not before (section 6 splits the tier as one library first).

| Peer | May include | Why |
|---|---|---|
| entities | spells (the aura storage and the cooldown facts), combat (what a unit's attack loop needs), maps (where it stands) | a unit is a thing in the world that casts, fights and stands somewhere |
| spells | entities, combat, maps | a spell reads its caster and target, deals its damage, and finds its targets on a map |
| combat | entities, spells | threat and damage read units and auras |
| maps | entities | a map holds objects; it never knows what they do |
| ai | entities, spells, combat, maps, motion | an AI reads everything a creature can see and asks for moves; nothing includes ai but scripts and session |
| social | entities | a group or a guild is made of players; nothing in the domain includes social (session does) |
| pvp | entities, social, maps, combat | a battleground is a map of groups fighting |
| economy | entities, social | an auction or a trade is between players |

Two consequences. `entities -> social` (336 lines today, the largest after maps) is debt: a player asks its group
through a fact callback declared beside the player and supplied by social, the shape the group update flags cluster
waits for. And `entities -> pvp` (188) is the same shape: a battleground tells the player, not the reverse.

## 2. What each layer may know

Entities, and the whole domain tier, hold state and rules. They never build a packet and never write SQL. A manager
returns facts, the owner sends them, and `persistence/` writes them.

Only session builds packets, using `session/packets/<domain>/` together with proto's codecs. When a packet has to go
out in the middle of a domain call, the domain calls a typed fact callback at the old statement, so the packet keeps
its place in the order. The callback's type is declared in the domain, beside the manager, for example a
`std::function<void(Facts const&)>`. Session supplies the implementation, which builds the packet and sends it. The
domain therefore never names session or `WorldPacket`.

`persistence/` makes every `CharacterDatabase.`, `WorldDatabase.` and `LoginDatabase.` call.

**Today (packets):** 404 `WorldPacket data(` sites: session 178, the domain tier 215 (entities 145, 89 of them under
`entities/player/`; social 34, spells 21, combat 7, maps 6, pvp 2), scripts 7, app 4. What a player's own client is
told when its swing is out of reach or faces away, its attack or its auto-repeat spell is cancelled, its pet is set,
its pet action bar is cleared, its stand state changes, it is credited honor for a kill, it completes a quest or it
is sent a system message are ten typed facts declared beside `Player` (`PlayerClientFacts.h`); a rune converted, the runes resynchronised and rune power added are
three declared beside `RuneMgr`; a currency's count changed, its week limit, the weekly reset and every currency are
four declared beside `CurrencyMgr`; the forced reactions, the changed standings, the reputation table, a faction made
visible and a faction's at-war flag are five declared beside `ReputationMgr`; the contact list and a contact's change
are two declared beside `PlayerSocial` (`SocialList.h`). All twenty-four are built into packets by
`session/packets/combat/`, `spells/`, `entities/`, `quests/` and `chat/` and sent through the callbacks `InstallPlayerPacketSinks` installs where a player is created.

**Today (database):** none of the 857 calls is outside persistence; every layer reaches the databases
through the repositories in `persistence/`. Game `.cpp` files hold 856 calls in 65 files.

**Today (entities, the manager audit):**
- No manager builds packets. `CurrencyMgr`, `ReputationMgr` and `SocialList`/`PlayerSocial` report their four, five
  and one client notices as typed facts that `session/packets/entities/` builds.
- None calls the database.
- All twelve are clean: `InventoryMgr`, `PlayerPetCache`, `QuestRewardRules`, `HonorMgr`, which reports a kill's honor
  credit as a typed fact that `session/packets/entities/` builds into its packet, `PetMgr` (the clear of the pet
  action bar is a fact of the player's own client), `RuneMgr`, which reports its three rune facts the same way, and
  `ReputationMgr`, `CurrencyMgr`, `SocialList`, `GlyphMgr`, `TalentMgr` and `QuestStatusMgr`, whose rows their
  repositories read and write (`persistence/characters/`).
- In `spells/`, `SpellCooldownMgr` calls no database and builds no packet: its rows go through its repository
  (`persistence/characters/spells/`), and it reports a cooldown event and the clear of every cooldown as typed facts,
  which `session/packets/spells/` builds into the two packets.
- `QuestCompletePacket` builds the quest-complete fact the player reports, in `session/packets/quests/`. The
  owner-side global `SocialMgr` reports each
  friend status fact to the own client of each player it goes to, and `session/packets/entities/` builds the packet.
- `CheckStateOwnership`'s cooldown, honor, pet, rune, currency, reputation and social rows allow the opcodes to the
  session's builders, not to the managers; `SocialList.cpp` keeps a debug log line that names its contact list's. The cooldown row holds `SMSG_COOLDOWN_EVENT`
  and the whole-map `SMSG_CLEAR_COOLDOWNS` only: `SMSG_SPELL_COOLDOWN` and `SMSG_ITEM_COOLDOWN` are the cast's and the
  item's notices, the one-spell clear stays with `Player::SendClearCooldown`, nothing builds `SMSG_MODIFY_COOLDOWN` or
  `SMSG_COOLDOWN_CHEAT`, and a builder of `SMSG_MODIFY_COOLDOWN` for a character's spell joins the row. The pet row
  holds `SMSG_PET_SPELLS`: its empty-guid form, which clears the client's pet action bar, is the session's builder's
  (`session/packets/entities/PetActionBarPacket`, from the player's fact), and the same opcode's full forms, the spell
  bars of the pet, of a possessed unit and of a charmed unit, are the owner's (`PlayerPet.cpp`); no other `PET`
  opcode is on the row. The row names no table:
  the stable-slot count is a column of the character row, and the five pet tables are written by the pet code and
  mirrored by `PlayerPetCache`, which writes none of them.

Each manager is fixed when its domain is next touched.

**The D4k manager shape is incomplete, not wrong.** The maintainer agreed on 2026-09-28. D4k already gave every
manager what this page asks for: state of its own, no `Player`, reads passed in as parameters, writes made through
call-scoped callbacks, per-row loads, and a test that runs without a `Player`. The packet and the SQL stayed inside
the managers so that the harness record could prove each move verbatim. Each of them already sits behind one narrow
seam: the sink parameter, and `LoadRow`/`Save(guid)`. The fix is therefore a move, not a redesign.

The smallest correction per manager:
- **Packets:** the builder becomes a free function in `session/packets/<domain>`, in the shape `QuestCompletePacket`
  already has. The sink parameter becomes the typed fact callback described above.
- **Database:** the row decoding and the SQL move to `persistence/characters/<domain>`. `LoadRow` takes a plain row,
  and the save reads the manager's state by const reference.

Each correction is one PR per manager, with the harness record byte-identical. A body that moves between files with
different includes is not byte-identical if an unqualified call to an overloaded function binds a different overload
in its new file (on libstdc++, `cos` on a `float` binds `cosf` under `<math.h>` and `cos(double)` under `<cmath>`
alone).

## 3. Shared versus player-only

entities/player/ holds every state only a player has; the domain directories (spells/, combat/, ...) hold what Unit
shares. Reputation, currency, honor, runes and achievements therefore stay in entities/player/. SpellCooldownMgr lives
in spells/ on Unit (Creature has a different cooldown model today).

| Today | Target |
|---|---|
| `QuestStatusMgr`, `QuestRewardRules`, `TalentMgr`, `GlyphMgr`, `InventoryMgr`, `PetMgr`, `PlayerPetCache`, `SocialList`, `ReputationMgr`, `CurrencyMgr`, `HonorMgr`, `RuneMgr` (all in `entities/player/`) | stay |
| `AchievementMgr` (`WorldHandlers/`) | `entities/player/` |
| `QuestCompletePacket` (`session/packets/quests/`, the builder of the player's quest-complete fact) | stays |
| `SpellCooldownMgr` | `spells/`, held by `Unit`. **Today:** `Creature` has a different cooldown model (6 methods, `Object/CreatureSpellCooldown.cpp`). |
| `SocialMgr` (a realm-wide global that tells every friend lister about a status change; it holds no player's state) | `social/` |

**Today (`Unit`, 652 member functions, grouped roughly by defining file and name):** combat 175, auras 113, spell
casting 72, lifecycle and update 80, movement 81, stats and power 72, pets/charm/summons 55, visibility 4. The target
puts combat in `combat/`, auras and casting in `spells/` and movement in the motion shell; the rest stays on `Unit`.

**Today (player-only code in `Unit`):** Unit's 16 files hold 11 `(Player*)this` casts and 196 `TYPEID_PLAYER` tests;
59 virtuals are overridden only by `Player` (`IsInWater`, `IsUnderWater`, `ProhibitSpellSchool`, `SetSheath`,
`Uncharm`; the combat stats `Unit` asks a player for: `GetMeleeRollExpertiseReduction`,
`GetMeleeSpellExpertiseReduction`, `CalculateMinMaxDamage`, `GetArmorPenetrationPct`, `GetBaseSpellPowerBonus`;
`GetItemByGuid`, the item the proc handlers ask for by guid; what `Mount` and `Unmount` ask of a mounting player:
`UnsummonPetTemporaryIfAny`, `ResummonPetTemporaryUnSummonedIfAny`, `InArena`, `GetCollisionHeight`; and what the
visibility, targeting and aura checks ask of a player: `isGameMaster`, `IsLoading`, `IsLoggingOut`, `GetTransport`,
`IsGroupVisibleFor`, `GetDrunkValue`; `GetReputationRank`, what the proc handlers ask of a player's reputation;
the cooldowns the proc handlers, the gameobject bookkeeping and `ClearInCombat` start or end on a player:
`AddSpellAndCategoryCooldowns`, `SendCooldownEvent`, `RemoveSpellCooldown`, `RemoveSpellCategoryCooldown`,
`UpdatePotionCooldown`; the combo points `ProcDamageAndSpellFor` and the reactive timers award or clear on a
warrior: `AddComboPoints`, `ClearComboPoints`; the rage `DealDamage` rewards a player's hit: `RewardRage`; the
kill credit and the honor-or-experience test `JustKilledCreature` and the proc checks ask of a player:
`KilledMonster`, `isHonorOrXPTarget`; the faction a player returns to and the ghost run speed the battleground
sets: `setFactionForRace`, `InBattleGround`; what the three one-off procs ask of a player: `Say`,
`GetSelectionGuid`, `GetNextRandomRaidMember`; the known rank of the Impurity talent `SpellBonusWithCoeffs` asks
of a death knight: `GetKnownTalentRankById`; whether a death knight's blood runes are all on cooldown, Blade
Barrier's test: `IsBaseRuneSlotsOnCooldown`; the passive spells a player's new aura state casts,
`ModifyAuraState`'s loop: `CastPassiveSpellsForAuraState`; what `Unit` tells a player's own client: its attack
cancelled, `SendAttackSwingCancelAttack`; its auto-repeat spell cancelled, `SendAutoRepeatCancel`; its pet's guid,
`SendPetGUIDs`; its stand state, `SendStandStateUpdate`; a changed melee swing error, `ReportSwingError`; the
account security level the GM visibility rule compares: `GetAccountSecurityLevel`; the position `Update`'s
pending commit and a spline's end write on a player, and whether a player is moving, the auto-repeat update's
test: `SetPosition`, `isMoving`; the client's control of its own movement, taken and returned by fear and
confuse: `SetClientControl`; a player's short teleport on its own map, `NearTeleportTo`'s player arm:
`TeleportNear`; the damage a player's hit credits to its battleground score and its achievements,
`DealDamage`'s player arm: `CreditDamageDealt`; what `CalculateSpellDamage` reads of a player, whether an
armor specialization spell fits it, its combo points and its combo target: `FitArmorSpecializationRules`,
`GetComboPoints`, `GetComboTargetGuid`; and what the possess members ask of a possessing player, its camera set to
the possessed unit's view and back, the possess action bar told to its client, its own pet removed when the
possession ends out of reach, and the pet action bar removed otherwise: `SetCameraView`, `ResetCameraView`,
`PossessSpellInitialize`, `RemovePet`, `RemovePetActionBar`; each with the non-player answer as `Unit`'s default).
The aura bodies live in `spells/auras/`, beside the aura storage (`spells/AuraContainer.h`).
The combat bodies, the threat managers and the leaf math live in `combat/`; `Object/Unit.cpp` still
holds `DealDamage`, `DealHeal`, the kill credit and the combat bodies that walk a unit's pets, which need headers
`combat/` does not include.

## 4. The finish line per layer: the layout gate matches this page

A layer is done when its gate enforces its rule on a clean clone, and it is finished when it is its own build target
(section 6): from that day the linker enforces the include direction and the gate only guards what the linker cannot
see. The counters are **ratchets against regression only, not finish lines**: `method_count.py --all` (Player 928, Unit 652, WorldSession 189, ObjectMgr 255), R1, M1,
the downcast counts and the database-call count.

| Layer | Rule | Gate |
|---|---|---|
| every file | lives in a directory named here; includes by path; only the edges section 1 allows | `CheckLayout`, built (#179). It ratchets from the measured edges listed in `layout_allow.txt`; `layout_gate.py --check` prints the count per bucket. Edges against the rule or into a gated seam are keyed per includer file and header; a line may change its includer file in the PR that deletes the old one or removes the include from it, only within the same layer pair and header, and a change of layer pair or header is a new edge. Sideways edges are keyed per includer directory, includer peer and header: the peer is the coupling unit, the directory stands in for it until `Object/`, `WorldHandlers/` and `References/` dissolve, and the key then collapses to directory and header. A file can split inside its directory. An edge leaves its allow-list in the PR that removes it, and a new one fails. `CheckIncludeCollisions` is in #174. |
| proto, foundation | nothing above | `CheckProtoBoundary` (holds, 0 lines out). Foundation has no gate yet (2 lines out, `ObjectGuid.cpp`). |
| motion | proto and foundation only | `CheckMotionBoundary`. It holds for `src/motion` and `game/movement/`, and only the kernel may not log. A unit's movement record has one owner: `CheckMovementWriters` (private to `Unit`, written only through its named operations, each from its own files). |
| persistence | the only place a `*Database.` call is spelled and a result or a field is read | `CheckDatabaseCalls`: no `CharacterDatabase`, `WorldDatabase` or `LoginDatabase` call in `src/game`, `src/modules`, `src/shared` or `src/mangosd` outside persistence and the driver; `CheckRowTypes`: no `QueryResult`, `QueryNamedResult` or `Field` there; `CheckSyncDb`: the blocking calls in the files it lists |
| data | no domain, session or scripts header | `CheckLayout`; a `CheckHeaderReach` row for `ObjectMgr.h` |
| domain | no `WorldPacket.h`, `Opcodes.h`, `WorldSession.h` or `*Database.`; the directories are peers under the ratchet; gated seams `entities/player` and `spells/aura`; managers never name `Player` | `CheckLayout`, `CheckManagerIsolation`, `CheckHeaderReach`, `CheckSessionSeam`, `CheckSpatialBoundary`, `CheckBlockedMasks`; `CheckStateOwnership` re-pointed at the session and persistence files |
| session | builds the packets; never includes scripts or app | `CheckLayout`; `CheckStateOwnership` (who may spell an opcode) |
| scripts | new content reaches only the surface; legacy reaches the core only through its compatibility layer, listed line by line, which can only shrink | `CheckMotionMasterShim` (legacy `MotionMaster` calls); `CheckScriptSurface` (#83, with the seam after #121) |

## 5. The threads, and who owns what

The layers say who may *include* what; this section says who may *touch* what at run time. The model is already
in the tree, and most of it is already enforced. It is written here because it is the part of the architecture
that is correct and invisible.

**One world thread owns the game.** `World::Update` runs the tick under `TickGuard::Scope`, in this order: the
timed managers (mail, auctions, the auction bot, LFG), then `UpdateSessions` (each session's queued packets are
handled here, on this thread, outside any map, until the first one only a map's update may take (a
`PROCESS_THREADSAFE` packet of a player in the world): the session rule below),
then `MapManager::Update`, then the posts the map phase left for the world thread (`ApplyAfterMapPhasePosts`), then the harness, battlegrounds and
outdoor PvP, then `ProcessResultQueue` for the three databases (every async query's continuation runs here), then the
console's queued commands. Nothing else touches game state.

**Map workers own one world map at a time.** `MapManager::Update` opens the map phase (`MapPhase::Begin`), hands
every world map to the `MapUpdater` pool (`MapUpdateThreads`, default 2; each worker is a registered MySQL client
thread through `DbThreadGuard`), blocks at the barrier (`wait`), and closes the phase. `Map::Update` takes a
`MapPhase::Scope`, so the thread running it owns that map; a transport deck's update is nested inside the map it
sails and takes its own scope, which owns the deck and the map whose update it interrupted, and on return the thread
owns that map alone again. A unit on a deck is also owned by the update of the map its vessel sails
(`MapPhase::OwnsUnitOn`): a world map and the decks of the vessels sailing it are one world, updated on one thread.
With the pool off, or under the harness's stepped clock, the world thread updates the maps in turn, one scope at a
time, which is what makes a harness run deterministic. A vessel that reaches the end of a map decides so on that map's
thread and *crosses* only after the barrier, on the world thread, because the destination may be updating on another
core.

**Network threads own sockets, never game state.** `net::Server` (epoll, kqueue, IOCP or io_uring behind one facade)
runs one poller per worker (on Windows the workers share one completion port), one `ClientConnection` per socket (a
client in the world holds two, its two streams), and a decoded packet crosses to the game through exactly one door:
the session's `SessionMailbox`, a locked queue drained by `UpdateSessions` and by the update of the player's map. Sends
go the other way through the session's `proto::SessionLinks`, which routes each opcode to one of the two streams.
During account lookup and attach the network thread runs the login and character queries synchronously and reads
world configuration (`sWorld.getConfig`, `sWorld.GetPlayerSecurityLimit` and the session constructor's locale reads,
`WorldGateway.cpp:101-317`), and the login request crosses to the world as a struct handed to `Attach` and queued
through `World::AddSession` (`WorldSessionMgr.cpp:104-109`), never as a packet, which is why the rule holds.

**Three database delay threads** (login, characters, world) execute the async queries; their results wait in the
`SqlResultQueue` until the world thread calls `ProcessResultQueue`. **Three service threads** never touch the game:
the console reader queues commands for the world thread, the log's console writer, and the anti-freeze watchdog,
which aborts the process when the tick counter stops moving.

**Four locks between map threads are leaves.** A script's outdoor PvP zone player set (`ZonePlayerSet`), each map's
team-buff queue (`TeamBuffQueue`), the capture-point slider store (`CapturePointSliderStore`) and the world's
after-phase queue (`AfterMapPhaseQueue`) each guard game state reached from more than one map thread. Each is taken
under nothing on a map thread, and nothing is called while it is held but its own container, so no lock order through
it can cycle; each has a test that holds it and shows its operations pending. Three locks held across game code can
be held when one of the four is taken. Two are the queue locks, both held on the world thread outside the map phase:
the console command queue's, across each console command in `World::ProcessCliCommands`, and the session-add queue's,
across `World::AddSession_` in `World::UpdateSessions`. A leaf reached from code the world thread runs can be taken
under either (the zone player set, the slider store); a leaf taken only inside a map's update or in `World::Update`
itself never is (the team-buff queue, the after-phase queue). The third is the map manager's lock, held while
`MapManager::CreateMap` builds an instance and loads its active objects and while `MapManager::DeleteInstance`
unloads one; it sits above a leaf only through a capture point on an instance map, of which the shipped data has
none.

| Rule | Enforced by |
|---|---|
| Game state is touched by the world thread, or by the worker whose scope owns the map. A deck's scope also owns the map whose update it interrupted, and a unit on a deck is also owned by the update of the map its vessel sails: one world, one thread. A unit on any other map is not | `MapPhase::Owns`, and for a unit `Unit::IsOwnedByThisThread` through `MapPhase::OwnsUnitOn` (counted process-wide, asserted under `MANGOS_DEBUG`), at two places: the movement kernel (`Unit::AssertMotionOwner`) and the spell seam (`Unit::AssertSpellSeamOwner`, run when each of the five registry contexts that hold a unit is built: `DummyAuraProcContext`, `HasteAuraProcContext`, `ProcTriggerSpellAuraProcContext`, `SpellDamageTakenContext` and `MeleeDamageTakenContext`) |
| Session handlers run on the world thread in `UpdateSessions`, outside the map phase, or on the thread updating the player's map: `Map::Update` drains each in-world player's queue through `MapSessionFilter` under the map phase, on a map worker when the pool runs and, when no worker pool runs, on the world thread, which updates the maps itself, map by map, under the map phase (`MapManager.cpp:318`, `:333-341`), taking the rows marked `PROCESS_THREADSAFE` and `PROCESS_INPLACE`, 73 of the 437 bound rows (attack swing, attack stop and sheathing among them). Each drain stops at the first packet its filter refuses, so a `PROCESS_THREADSAFE` row of an in-world player runs only on the map's thread, and a `PROCESS_INPLACE` row on whichever drain reaches it (`World.cpp:1625`, `Map.cpp:919-928`, `WorldSession.cpp:210-269`, `LockedQueue.h:82-99`) | not yet built: `CheckSessionSeam` checks four named files for session calls and no threading |
| No synchronous database acquisition under the tick | `TickGuard` (strict mode asserts; the CI job proves the four strict cases stand) and `CheckSyncDb` |
| Packets cross from the network only through `SessionMailbox` | the `proto` boundary: `IClientLink` / `IWorldGateway` |
| A map never writes into another map during the phase, except within one world: a world map and the decks of the vessels sailing it write into each other, on that map's thread | transport crossings run after the barrier; a team buff for a player on a map the calling thread does not own is posted to that map's `TeamBuffQueue` and applied by that map's own update, the first statement after its `MapPhase::Scope` (`Map::ApplyTeamBuffs`, pinned by `CheckTeamBuffDrain`), and a deck's update owns the map it interrupted, so a buff for a player there is applied at once; a defense or zone-under-attack message and a missing graveyard link raised during the phase are posted to the world's `AfterMapPhaseQueue` and applied by the world thread after the barrier, the statement after `sMapMgr.Update` (`World::ApplyAfterMapPhasePosts`, pinned by `CheckAfterMapPhaseDrain`) |
| State every map thread can reach (outdoor PvP's scripts) is touched under a leaf lock, as an atomic, or after the barrier | the zone player set and the capture-point slider store are leaf locks; Eastern Plaguelands' tower counts are atomics, each read once by the script's zone-entry override; the team buffs and the after-phase posts are the routes of the row above. A caller that bypasses a route or reads a count twice passes every test: review holds them |
| A harness run is deterministic | the stepped clock runs every map inline, in order, on one thread |

**Today:**
- Ownership is checked at two seams, the movement kernel (`Unit::AssertMotionOwner`) and the spell seam
  (`Unit::AssertSpellSeamOwner`), where a unit's per-spell handlers enter the registry: 13 of the 15 dispatches of a
  `Unit` site build one of the five contexts that hold the unit, and building it checks the unit. The other two,
  `Unit::HandleSpellCritChanceAuraProc` and Lifebloom in `Unit::SpellHealingBonusDone`, give their handlers no unit
  and are not checked. Both checks count by the one-world rule (`Unit::IsOwnedByThisThread`): a world map and the
  decks of the vessels sailing it are updated on one thread, each deck's update nested inside the world map's, so the
  update of either owns the units of both. Every other touch is counted: a unit on another world map or an instance
  and, once a vessel has crossed to another map, a unit on the map it left, touched from its deck's update, or a unit
  on its deck, touched from that map's update. Every other part of a map's state (its grids, its object stores, the
  spell and aura code outside those 13 dispatches) relies on the phase structure and on nobody reaching across.
- The network pool is sized by `hardware_concurrency()` (`net/reactor/ReactorServer.cpp:110`), not by the
  configuration. `Network.Threads` said otherwise and was read by no source file; it was removed from
  `mangosd.conf.dist.in` on 2026-09-29. `Network.OutKBuff`, `Network.OutUBuff` and `Network.TcpNodelay` are read by
  no source file either; the engine sets its own socket options.
- The game draws from the seeded `RNG` only: its 7 C library `rand()` draws moved to it, and `CheckRawRand` holds
  `src/game` at 0 draws and one `srand`, `World.cpp`'s. SD3 is the exception until #83: its 21 `rand()` draws (20
  lines in 10 script files) still use the C library. On Windows the UCRT keeps `rand()` state per thread, so
  `World.cpp:242`'s `srand` seeds only the main thread, and SD3's draws on the pooled map workers
  (`MapUpdateThreads` 2) come from an unseeded per-thread stream (seed 1, the same on every worker and every boot);
  on glibc they share one stream, seeded from the wall clock by that `srand`.

## 6. Build targets

The include direction is enforced by a grep gate (`CheckLayout`). The end state enforces it with the linker: one
static library per layer, linked in the direction of section 1's table, so an include against the rule does not
compile-and-pass, it fails to link. A library cannot lie about its dependencies.

**Today:** four of the nine layers are already targets, and they are exactly the ones whose gates hold.

| Layer | Target today | Links |
|---|---|---|
| foundation | `shared`, `mangos_crypto`, `terrain`, `geometry` | `Threads`, `utf8`; crypto stands alone |
| persistence (driver) | `shared_db` | `shared`, MySQL |
| proto | `proto` | `shared`, `mangos_crypto` |
| motion | `motion` | `proto` |
| data, domain, session, `persistence/<domain>/`, and the app and scripts files inside `src/game` | **`game`**, one library: 40 globbed directories, 399k lines, one precompiled header | `shared_db`, `terrain`, `geometry`, `proto`, `motion`, Detour and zlib |
| scripts (SD3) | `mangosscript` | `game` |
| app | `mangosd` | `game`, `proto`, and `mangosscript` when the scripts are built, whose hooks it installs |
| tests | `mangos_tests` | `game`, `motion`, `proto` |

The only layer boundary the linker checks today is the one around `motion` and `proto`; every include the gate counts
as against the rule in section 1 lives inside `game`, where the linker sees one target. `game` does not link
`mangosscript`: the core calls the scripts through the hook interface (`data/ScriptHooks.h`), and `mangosd` links
`mangosscript` and installs SD3's hooks at start-up.

The sizes, by target layer (from `layers.py files`, lines of `.h`/`.cpp`): scripts 264k (SD3 and `ChatCommands`),
entities 98k, spells 65k, session 39k, data 39k, app 37k (with the harness), social 21k, pvp 20k, maps 19k,
persistence 10k, combat 8k, ai 7k, economy 5k.

**The rule for splitting.** A layer becomes its own target in the PR that brings its upward edges to zero, never
before: a library with an upward include does not link, so the split *is* that layer's finish line, and it cannot be
faked. The domain tier splits as one library first (its directories are peers with cycles: entities <-> spells
173 / 53) and into per-directory libraries only if a cycle is ever cut on purpose. The order follows section 8:

1. `data`: after rows 5 and 7 (77 lines into the domain, 3 into session).
2. `domain`: after rows 1, 2 and 3 (proto, session, `World.h`).
3. `session`: after the `World.h` and script edges it holds.
4. `scripts`: the `scripts` target is born at the seam (#83) with the surface and the dispatcher, new code with no
   upward edge; `ChatCommands`, the command half of `Chat`, `CommandMgr` and `ScriptMgr` join it as `commands/` and
   `dbscripts/` once #143 removes their `World.h` lines. New scripts build in `scripts_content`, which links only
   `scripts` and has no precompiled header. SD3 builds as `scripts_legacy` (today `mangosscript`), optional as now,
   and hands its hooks to the dispatcher. `game` already reaches the scripts only through the hook interface (#83).
5. `app`: `World`, the harness and the auction bot move to `mangosd` or a `world` library; `game` no longer exists.

Each split PR moves the layer's `file(GLOB)` lines into the new target's `CMakeLists.txt`, gives it its own
precompiled header or none, and deletes that layer's rows from `layout_allow.txt`. `mangos_tests` links every
target, as it links three today.

## 7. Not decided

| Question | Options |
|---|---|
| `AuctionHouseBot/` (2 files; a non-blizzlike feature) | **Kept** (decided 2026-09-28): optional, off by default, measured as app. Not a decoupling concern; a harness family that needs a quiet auction house disables it in the configuration rather than removing it. |
| The script surface (#83), what scripts call on the core | **Decided 2026-10-29.** Scripts are a layer of their own under `src/scripts/`: the surface, which is what a script may call, as views over handles and strong ids implemented on the core; the dispatcher, the one implementation of the core's hook interface; new content; and the old SD3 library as legacy. A new script includes only the surface, and its object file refers to no symbol another project library defines; a gate checks both and refuses to pass when it finds no surface. Legacy reaches the core through a compatibility layer listed line by line that can only shrink, and is deleted with the last legacy file. Scripts hold handles across ticks and bind a view once per hook. Movement verbs join the surface when the kernel owns every movement flag; per-spell behaviour belongs to the spell registry. The surface sits in the scripts layer because it builds gossip menus and vendor lists, which are the session's, and nothing below scripts includes it. Each family lands whole, with tests, when the first new script needs it; the authoring model is built after the finish line. Only content that cannot exist in 4.3.4 is deleted: the Classic Zul'Gurub raid scripts went; Zul'Aman, the empty placeholders and every script tied to 4.3.4 content stay. |

## 8. Divergences

This page resolved four earlier questions: the proto arrow; #76's layout rule, replaced by the ratchet in section 4;
where reputation, currency, honor and runes live; and the rule for the domain tier.

| # | Today, against the target | Closed by |
|---|---|---|
| 1 | domain -> proto: 127 lines; 215 `WorldPacket data(` sites in the domain tier; `Player::SetClientControl`, reached from `Unit` as an override, builds its control packet and grants or revokes its session's mover authority on a `Unit*`: a seam of its own kind, not a client fact | when content touches each domain; spells in D11 (#142); `SetClientControl`'s own seam |
| 2 | domain -> session: 82 lines (`WorldSession.h`, `UpdateData.h`, `GossipDef.h`) | Unit reopen, D11, then when content touches it |
| 3 | `World.h` included 27 times below app | the configuration interface (#143), when content touches it |
| 4 | `ScriptMgr.h` and `Chat.h` included below scripts 0 times (`Chat.h` 23 times when #83 began): a map runs its database script steps and the domain starts its events through `data/DbScriptHooks`, which `mangosd` installs; the session builds the chat packets and system messages, the domain builds its chat packets through `data/ChatPacketBuilder` and reports a player's system messages as a client fact, and the GM commands are reached through `data/CommandHooks` | closed for the includes (#83); the domain's chat packets become facts with row 1 |
| 5 | data -> domain: 58 lines, in the loaders and managers that act on live objects; `ObjectMgr.h` reaches no domain header (its `CheckHeaderReach` row) | #121, when content needs it |
| 6 | `game/movement/` includes nothing above motion (0 lines) | closed (#81): `CheckMotionBoundary` covers `src/motion` and `game/movement/` |
| 7 | `PlayerDump` and `CharacterDatabaseCleaner` (7 lines) and `ObjectGuid.cpp` (2: `World.h`, `ObjectMgr.h`) include above their layer | when content touches it |
| 8 | 0 database calls outside persistence | closed: `CheckDatabaseCalls` keeps it at 0 |
| 9 | **Entities, Today:** no manager builds packets (`CurrencyMgr`'s four, `ReputationMgr`'s five and `SocialList`/`PlayerSocial`'s contact list are typed facts built by `session/packets/entities/`); none calls the database; all twelve are clean (`InventoryMgr`, `PlayerPetCache`, `QuestRewardRules`, `HonorMgr`, whose kill's honor credit is a typed fact built by `session/packets/entities/`, `PetMgr`, the clear of the pet action bar now a fact of the player's own client, `RuneMgr`, whose three rune facts the same directory builds, and `ReputationMgr`, `CurrencyMgr`, `SocialList`, `GlyphMgr`, `TalentMgr` and `QuestStatusMgr`, whose rows their repositories in `persistence/characters/` read and write). **Spells, Today:** `SpellCooldownMgr` calls no database (its rows go through its repository in `persistence/characters/spells/`) and builds no packet: its cooldown event and clear of every cooldown are typed facts, built into packets by `session/packets/spells/` and sent through the callbacks the session installs where a player is created. `SpellModMgr` builds no packet and calls no database: its modifier change is a typed fact built by `session/packets/spells/` | closed for the database (#144: no manager calls it, each is tested from plain rows and its repository over the fake connection); the rest when the domain is next touched |
| 10 | `CheckStateOwnership`'s achievement row: its owner, `AchievementMgr`, still builds its packets; `QuestCompletePacket` is in `session/packets/quests/` and builds the player's quest-complete fact | with `AchievementMgr`'s move (row 16) |
| 11 | `Player` forwarders for quests, talents and inventory | D4i caller migration (#78) |
| 12 | `SpellCooldownMgr` is in `spells/` and held by `Unit`, but only players use it (the type guards at its `Unit` call sites stay), and `Creature` has its own cooldown model (scenario 938); the spell modifiers' manager, `SpellModMgr`, is in `spells/`, owned by `Player`, `Unit` holding a pointer | the Creature fold, only on 4.3.4 evidence |
| 13 | `Unit`: 11 `(Player*)this` casts, 196 player type tests, 59 Player-only virtuals | Unit reopen |
| 14 | Some of Unit's combat bodies are in `Object/Unit.cpp`: `DealDamage`, `DealHeal`, the kill credit and the pet walks | Unit reopen (combat) |
| 15 | `Object/` and `WorldHandlers/` exist; `data/` exists and holds eighteen stores (its README lists them), the generic record stores `KeyedRecordStore` and `MultiRecordStore`, `TextPool`, `GameConfig`, the script hook interface `ScriptHooks`, the command hook interface `CommandHooks` and the database script hook interface `DbScriptHooks`, while `ObjectMgr` is still in `Object/` and holds the conditions, the creature templates and their spells, the game object addons and the item templates in `KeyedRecordStore`s; `ai/`, `social/`, `pvp/`, `economy/` do not; `session/` holds the builders under `session/packets/`, the combat handler class under `session/handlers/combat/`, the client and warden handler classes under `session/handlers/connection/`, the vendor, loot, auction and trade handler classes under `session/handlers/economy/`, the skill, enchant, pet, taxi, query, item, NPC, quest, character, character customize, death and player handler classes under `session/handlers/entities/`, the map handler class under `session/handlers/maps/`, the pvp handler class under `session/handlers/pvp/` and the LFG, contact list, GM ticket, petition, chat channel, voice chat, chat, mail, calendar, group, guild and who handler classes under `session/handlers/social/`, and the session's other files are in `Server/` and `WorldHandlers/` | a move PR before each domain's first seam (#76); a seam creates the new builder files it needs in their target directory and moves no existing file |
| 16 | `AchievementMgr` is in `WorldHandlers/`, and `SocialMgr` is under `entities/player/` | their move PRs, when content touches them |
| 17 | 2,356 cross lines inside the domain tier have no ratchet yet | built (#179); re-keyed per includer directory, peer and header on 2026-09-29 |
| 18 | No `CheckLayout`, and no gate for `*Database.` outside persistence | built (#179) (the ratchet; sideways lines re-keyed per includer directory, peer and header on 2026-09-29); the database gate built (`CheckDatabaseCalls`, #144) |
| 19 | Thread ownership is checked at the movement kernel and at the spell seam, both by the one-world rule (`Unit::IsOwnedByThisThread`: `MapPhase::Owns` for the unit's map and, for a unit on a deck, for the map its vessel sails too): each of the five registry contexts that hold a unit checks it when built (`Unit::AssertSpellSeamOwner`), so 13 of the 15 dispatches of a `Unit` site check the unit they run for; the other two (`Unit::HandleSpellCritChanceAuraProc` and Lifebloom in `Unit::SpellHealingBonusDone`) give their handlers no unit, and the `Aura`, `SpellAuraHolder`, `Spell` and `SpellMgr` sites are not checked | closed for the `Unit` sites whose handlers are given the unit |
| 20 | `Network.OutKBuff`, `Network.OutUBuff` and `Network.TcpNodelay` are read by no source file (`Network.Threads` was, and is deleted) | delete them or wire them through #143 |
| 21 | 7 raw `rand()` draws in 5 `src/game` files, seeded by `World.cpp`'s `srand`, share one generator across the map workers | closed on 2026-09-30 by D11's named change: the 7 draws use the seeded `RNG`, and `CheckRawRand` keeps `src/game` at 0 draws. The one generator held on glibc only: the Windows UCRT keeps `rand()` state per thread, so `World.cpp:242`'s `srand` seeded the main thread and the pooled map workers drew from an unseeded stream (seed 1). The `srand` stays for glibc, where SD3's draws share its wall-clock-seeded stream; #83 removes it with SD3's draws and drops the gate's allowance to 0 |
| 22 | `game` is one target holding data, domain, session, the domain repositories, and app and scripts files; the linker checks only the `motion` / `proto` boundary | one split per layer, in section 6's order, each in the PR that zeroes that layer's upward edges |
| 23 | `game` and `mangosscript` linked each other | closed: the core calls the scripts through the hook interface (#83), `game` no longer links `mangosscript`, and `mangosd` links it and installs its hooks |
| 24 | `Unit`: two casts of `this` to the player, closed by the successor the next column names. They are the two accessors, `GetCharmerOrOwnerPlayerOrPlayerItself` and its const twin (`Unit.cpp:3421`, `:3437`; the twin's `(Player const*)` cast is outside row 13's count), which stay as counted casts: their 53 callers ask one question, "which player gets credit or control for this unit", and its answer becomes a fact on the charm and ownership model. The spell-mod owner is closed: spell computations read the player's spell modifiers through `Unit::GetSpellMods`, the getter of the holder in `spells/SpellModMgr`, and `GetSpellModOwner` is gone. | the charm and ownership model's own peer (`social/` or `entities/creature/`) |
| 25 | **Session handlers.** Before the first handler moved, `WorldSession` declared 418 `Handle*` members (589 methods) and the opcode table bound 372 of them through a pointer to a `WorldSession` member, from 36 files: 34 in `WorldHandlers/`, `BattleGround/BattleGroundHandler.cpp` and `Server/WorldSession.cpp`. **The target:** `session/handlers/<domain>/`, one stateless class per origin file in the domain's directory (combat's, made from two files, and pvp's, the class of both the arena team and the battleground files, two exceptions, and the miscellaneous handlers a third, split by topic into their domains' classes) with static entry points, each `static void HandleX(WorldSession& session, WorldPacket& packet)`; the table stores `void (*)(WorldSession&, WorldPacket&)`, the address of the named template `OpcodeThunk<&Class::HandleX>`; status and processing stay table metadata. **Who owns what:** the table and its thunks are code: they own nothing and live as long as the program. `World` owns every client session and deletes it only on the world thread, in `UpdateSessions` (`World.cpp:1631`, and `WorldSessionMgr.cpp:123`, `:143` in `AddSession_` at its top) or at shutdown (`World.cpp:213`, `:223`), never during a handler call; a session owns its player. A handler borrows the session by reference for one call, on the world thread or on the map worker that owns the player's map (section 5), and keeps nothing: no data member, no static variable but a prepared statement's cached id (`static SqlStatementID`), no session or player pointer in anything that outlives the call. The player pointer is taken fresh after any call that can log out; the two such calls are `LogoutPlayer` (the logout request, `WorldSession::HandleLogoutRequestOpcode`) and `PlayerRegistry::Kick` (`PlayerRegistry.cpp:79`, reached from account deletion). Work that finishes later captures ids and values only, never a session or player pointer, and finds what it needs again when it runs by those ids, or by ids a query keyed on them returns: the requester by its account id and session id through `WorldSession::FindRequesterSession` (`CharacterHandler.cpp:144-153`; the petition query's continuation makes the same check inline) or, with the guid checked too, `WorldSession::FindRequesterPlayer` (`:174-190`), which drop the answer when a reconnect has replaced the session (`WorldSessionMgr.cpp:132-143`), and any other player by guid through the player registry, a charter's owner by the owner guid the decline's query reads. | the table seam (no handler moves), done; then combat (`CombatHandlers`: the five handlers and `SendAttackStop`, WorldSession 589 -> 583), done; then economy's vendor file (`VendorHandlers`: the eight vendor and bank handlers, `SendListInventory` and `CheckBanker` staying on `WorldSession` in the origin file, WorldSession 555 -> 547), done; then pvp's arena team file (`PvpHandlers`: the eleven arena team handlers, `SendArenaTeamCommandResult` and `SendNotInArenaTeamPacket` staying on `WorldSession` in the origin file, WorldSession 547 -> 536), done; then pvp's battleground file into the same class (`PvpHandlers`: the sixteen battleground handlers, `SendBattlegGroundList` staying on `WorldSession` in the origin file, WorldSession 536 -> 520), done, and with it pvp; then economy's loot file (`LootHandlers`: the five loot handlers, `DoLootRelease` staying on `WorldSession` in the origin file, WorldSession 520 -> 515), done; then economy's auction file (`AuctionHandlers`: the eight auction house handlers, `GetCheckedAuctionHouseForAuctioneer` and `SendAuctionCancelledToBidderMail` as its private statics, the six auction senders staying on `WorldSession` in the origin file, `SendAuctionCommandResult` and `SendAuctionRemovedNotification` among them as packet senders whose callers all moved, WorldSession 515 -> 505), done; then economy's trade file (`TradeHandlers`: the ten trade handlers, `moveItems` as its private static, `SendTradeStatus`, `SendUpdateTrade` and `SendCancelTrade` staying on `WorldSession` in the origin file as packet senders that code outside the file also calls, the class `Player`'s friend for the six writes of the player's trade data, which has no setter, WorldSession 505 -> 494), done; then entities' skill file (`SkillHandlers`: the four talent and skill handlers, no member staying on `WorldSession`, the origin file deleted, WorldSession 494 -> 490), done; then entities' enchant file (`EnchantHandlers`: the three wrap, socket and temporary enchantment handlers, `SendEnchantmentLog` and `SendItemEnchantTimeUpdate` staying on `WorldSession` in the origin file as packet senders that code outside the file calls, WorldSession 490 -> 487), done; then entities' pet file, its command half (`PetHandlers`: the five pet command handlers, the name query, rename, abandon, talent and critter handlers and the two pet name senders staying on `WorldSession` in the origin file, WorldSession 487 -> 482), done; then entities' pet file, its second half into the same class (`PetHandlers`: the name query, rename, abandon, the two pet talent handlers and the critter dismissal, `SendPetNameQuery` and `SendPetNameInvalid` staying on `WorldSession` in the origin file as packet senders whose callers all moved, WorldSession 482 -> 476), done, and with it the pet file; then social's LFG file (`LfgHandlers`: the nine Dungeon Finder handlers, the twelve Dungeon Finder senders staying on `WorldSession` in the origin file as packet senders, ten of them called by the LFG manager and `SendLfgPlayerLockInfo` and `SendLfgPartyLockInfo` by the class alone, WorldSession 476 -> 467), done; then social's contact list file (`ContactHandlers`: the six contact list handlers, `CompleteAddFriend` and `CompleteAddIgnore` as its private statics, no member staying on `WorldSession`, the origin file deleted, WorldSession 467 -> 459), done; then social's GM ticket file (`TicketHandlers`: the seven ticket handlers, the two ticket senders `SendGMTicketGetTicket` and `SendGMTicketStatusUpdate` staying on `WorldSession` in the origin file as packet senders, both also called by the ticket manager, WorldSession 459 -> 452), done; then social's petition file, its signing, offer and turn-in half (`PetitionHandlers`: the show-signatures, sign, offer and turn-in handlers, their five continuations as its private statics and `QueuePetitionSignHolder` and `PetitionSignOutcome`, which the petition tests drive, as its public statics, the file-local slot enums and holder queue moving with them, the charter purchase, rename, decline, query and list request handlers, `SendPetitionShowList`, `SendPetitionQueryOpcode` and `SendPetitionQueryCallback` staying on `WorldSession` in the origin file, WorldSession 452 -> 441), done; then social's petition file, its second half into the same class (`PetitionHandlers`: the charter purchase, query, rename, decline and list request handlers, `QueuePetitionBuyRead`, which the petition tests drive, as a public static and the purchase, rename and decline continuations as private statics, `SendPetitionShowList` staying on `WorldSession` in the origin file as a packet sender the petitioner's gossip also calls, and `SendPetitionQueryOpcode` with its callback `SendPetitionQueryCallback` staying as the query the removal of a character's charter signatures (`Player::RemovePetitionsAndSigns`) also calls, WorldSession 441 -> 432), done, and with it the petition file; then social's chat channel file (`ChannelHandlers`: the seventeen chat channel handlers, no member staying on `WorldSession`, the origin file deleted, WorldSession 432 -> 415), done; then social's voice chat file (`VoiceChatHandlers`: the three voice chat handlers, no member staying on `WorldSession`, the origin file deleted, WorldSession 415 -> 412), done; then the connection's warden file (`WardenHandlers`: the Warden data drain, no member staying on `WorldSession`, the origin file deleted, WorldSession 412 -> 411), done; then entities' taxi file (`TaxiHandlers`: the five flight master handlers, the five taxi senders staying on `WorldSession` in the origin file as packet senders, `SendTaxiStatus` called by the class alone and `SendTaxiMenu`, `SendDoFlight`, `SendLearnNewTaxiNode` and `SendActivateTaxiReply` also by the flight master's gossip and the player's flight, WorldSession 411 -> 406), done; then social's chat file (`ChatHandlers`: the five chat handlers, `processChatmessageFurtherAfterSecurityChecks` as its private static and the text emote's builder moving with them, `SendPlayerNotFoundNotice` and `SendWrongFactionNotice` staying on `WorldSession` in the origin file as packet senders whose callers all moved, WorldSession 406 -> 400), done; then entities' query file (`QueryHandlers`: the ten query handlers, the name query's three senders and `SendQueryTimeResponse` staying on `WorldSession` in the origin file as packet senders, the last also called by the ticket handlers, WorldSession 400 -> 390), done; then entities' item file (`ItemHandlers`: the ten inventory and reforge handlers, `SendItemDb2Reply`, `SendItemSparseDb2Reply` and `SendReforgeResult` staying on `WorldSession` in the origin file as packet senders, the first two also called by the hotfix request, WorldSession 390 -> 380), done; then social's mail file (`MailHandlers`: the nine mail handlers, `MailSendRequest` and `QueueSendMailboxCountRead`, which the mail tests use, as public members and `CheckMailBox`, the send's continuation and `ProcessSendMail` as private statics, no member staying on `WorldSession`, the origin file deleted, WorldSession 380 -> 367), done; then entities' NPC file (`NpcHandlers`: the twelve NPC service handlers, `CheckStableMaster` as its private static and the stable's result codes moving with them, the NPC senders staying on `WorldSession` in the origin file as packet senders, most of them also called by the gossip, the chat commands or the scripts, WorldSession 367 -> 354), done; then entities' quest file (`QuestHandlers`: the thirteen quest handlers, `CanInteractWithQuestGiver` as its private static, `getDialogStatus` staying on `WorldSession` in the origin file, which the player's quest giver updates also call, WorldSession 354 -> 340), done; then social's calendar file (`CalendarHandlers`: the sixteen calendar handlers, `CalendarInviteRequest`, `ResolveCalendarInvitee` and `QueueCalendarInviteIgnoreRead`, which the calendar tests use, as public members and the invite's continuation and `FinishCalendarEventInvite` as private statics, `CalendarMgr`'s senders staying in the origin file, no member staying on `WorldSession`, WorldSession 340 -> 320), done; then entities' character file (`CharacterHandlers`: the eleven character screen and character handlers, `CharCreateRequest` and the three staging statics, which the character tests use, as public members and the create checks and the five continuations as private statics, the login, `HandlePlayerLoginOpcode` and `HandlePlayerLogin`, staying on `WorldSession` in the origin file with the logout, since it sets the session's loading state and seats the player it creates, and `FindRequesterSession`, `FindRequesterPlayer` and the list's sender `HandleCharEnum` staying with it, WorldSession 320 -> 302), done; then entities' character customize file (`CharacterCustomizeHandlers`: the ten customization and client-settings handlers, `CharCustomizeRequest` and `QueueCharCustomizeReads`, which the character tests use, as public members and the rename's and the customize's continuations as private statics, no member staying on `WorldSession`, the origin file deleted, WorldSession 302 -> 289), done; then social's group file (`GroupHandlers`: the twenty-five group handlers, the role change's builder moving with them, `SendPartyResult`, `SendGroupInvite` and `BuildPartyMemberStatsChangedPacket` staying on `WorldSession` in the origin file as packet senders, the last also called by the group's member updates, WorldSession 289 -> 264), done; then social's guild file (`GuildHandlers`: the thirty-five guild and guild bank handlers, `SendGuildCommandResult` and `SendSaveGuildEmblem` staying on `WorldSession` in the origin file as packet senders, the first also called by the guild charter's turn-in, WorldSession 264 -> 229), done; then the misc files, split by topic: maps' class (`MapHandlers`: the zone change, the area trigger and the world teleport of `MiscHandler.cpp`, and the instance reset and the dungeon and raid difficulty of `MiscHandlerInstance.cpp`, which is deleted, WorldSession 229 -> 223), done; then entities' death class (`DeathHandlers`: the five death and resurrection handlers of `MiscHandler.cpp`, WorldSession 223 -> 218), done; then entities' player class (`PlayerHandlers`: the fourteen handlers of `MiscHandler.cpp` about the player's own settings and the players it inspects, `SendLoadCUFProfiles` staying on `WorldSession` as the profiles' sender, WorldSession 218 -> 204), done; then the pet information request of `MiscHandler.cpp` into entities' pet class (`PetHandlers`, its body empty, WorldSession 204 -> 203), done; then the failed object update of `MiscHandler.cpp` into entities' query class (`QueryHandlers`, answered with the object's create block again, WorldSession 203 -> 202), done; then social's who class (`WhoHandlers`: the who list and the whois of `MiscHandler.cpp`, the whois continuation its private static, WorldSession 202 -> 199), done; then the bug report and the spam complaint of `MiscHandler.cpp` into social's ticket class (`TicketHandlers`, WorldSession 199 -> 197), done; then the PvP flag's toggle of `MiscHandler.cpp` into pvp's class (`PvpHandlers`, WorldSession 197 -> 196), done; then connection's client class (`ClientHandlers`: the account data, realm split, UI time, hotfix and taxi benchmark handlers of `MiscHandler.cpp`, `SendItemDb2Reply` and `SendItemSparseDb2Reply` staying on `WorldSession` as the hotfix's senders, WorldSession 196 -> 189), done; then the rest of `MiscHandler.cpp`, which is deleted, staying on `WorldSession`: the logout request and cancel and the time-sync reply into `Server/WorldSession.cpp` beside the ping and the keep-alive, as the session's own, `SendLoadCUFProfiles` beside the account data times, and the skipped move time into `MovementHandler.cpp` with the movement handlers, done; then one origin file per PR, never `SpellHandler.cpp` (D11), the movement and vehicle handlers after D9 |

## Appendix: how each number was measured

From the repository root, with Python 3 and Git Bash. `layers.py` is the script below. Its `RULES` table is
section 1's directory table, written as code: `ObjectGuid.h`/`.cpp` are foundation (the 2026-09-28
decision), and `AuctionHouseBot/` is app (kept, section 7).

- Where the files go: `python layers.py src files | grep ' game/Object/' | cut -d' ' -f1 | sort | uniq -c` (and
  likewise for `WorldHandlers/`, `Server/`, `References/` and `Tools/`).
- The edge tables: `python layers.py src against` (305, with the headers and the includers),
  `python layers.py src sideways` (2,357) and `python layers.py src edges` (every layer pair).
- Packets and the database: `python layers.py src packets` (404 by layer and directory) and `python layers.py src db`
  (1,006). For game `.cpp` only:
  `grep -rhoE '\b(Character|World|Login)Database\.' --include=*.cpp src/game | wc -l` (994), with `-l` for the files
  (127).
- Proto: `python layers.py src edges | grep -E '^(proto|session +-> proto|motion +-> proto)'` (proto -> proto and
  foundation only; session 116; motion 23), split by
  `grep -hE '#\s*include\s*"(wire/[A-Za-z]+\.h|WorldPacket\.h|Opcodes\.h)"' src/motion/* | wc -l` (8) and the
  same over `src/game/movement/*` (15).
- The manager audit, per manager `M`:
  `cat M.h M.cpp | grep -v '^\s*//' | grep -oE 'SMSG_[A-Z_]+' | sort -u` (opcodes built) and
  `cat M.h M.cpp | grep -cE '\b(Character|World|Login)Database\.'` (database lines).
- Method counts:
  `python src/tests/tools/method_count.py --class <C> --header <C.h> --all` for Player (`entities/player/Player.h`),
  Unit (`Object/Unit.h`), WorldSession (`Server/WorldSession.h`) and ObjectMgr (`Object/ObjectMgr.h`).
- Unit's families:
  `python src/tests/tools/method_count.py --class Unit --header src/game/Object/Unit.h --all --list | python layers.py src unit`.
- The player-only leaks, with
  `F="src/game/Object/Unit*.cpp src/game/combat/Unit*.cpp src/game/spells/auras/Unit*.cpp src/game/spells/pipeline/Unit*.cpp"`:
  `grep -ohE '\(\s*Player\s*\*\s*\)\s*this|static_cast<\s*Player\s*\*\s*>\s*\(\s*this' $F | wc -l` (11) and
  `grep -ohE 'GetTypeId\(\)\s*[!=]=\s*TYPEID_PLAYER' $F | wc -l` (196).
  The Player-only virtuals are the `virtual` lines of the `--all --list` output whose name is declared again in
  `Player.h` and in none of `Creature.h`, `Pet.h`, `Totem.h`, `TemporarySummon.h` or `Vehicle.h`, where only a
  unit's declaration counts: `Vehicle.h`'s `GetTransport` is `TransportInfo`'s (59 of 88).
- Creature cooldowns: `grep -nE '^\w.*Creature::\w+\(' src/game/Object/CreatureSpellCooldown.cpp` (6).
- The threads (section 5): the tick's order is `World::Update` (`WorldHandlers/World.cpp:971`, the `TickGuard::Scope` at the top of its
  body) read top to bottom; the map split is `MapManager::Update` (`WorldHandlers/MapManager.cpp:306`);
  the thread starts are `grep -rnE "std::thread|StartConsoleThread|HaltDelayThread" src/mangosd src/game src/shared`
  (the map pool in `Maps/MapUpdater.cpp`, the CLI reader and the watchdog in `mangosd/`, the console writer in
  `shared/Log`, one delay thread per database); the network pool's size is `net/reactor/ReactorServer.cpp:110`;
  the map-phase rows are
  `grep -E '^\s*OPCODE\(' src/game/Server/OpcodeTable.cpp | grep -vE 'Handle_(ServerSide|NULL|EarlyProccess|Deprecated)\b' | grep -cE 'PROCESS_(THREADSAFE|INPLACE)'`
  (73; without the last filter, 437 bound rows);
  `grep -rl "Network\.\(OutKBuff\|OutUBuff\|TcpNodelay\)" src` finds only `mangosd.conf.dist.in`; the strict-mode proof is the
  "Check strict tick mode is armed" step of `core_linux_build.yml`. Raw generators:
  `python src/tests/tools/raw_rand.py --list` (0 draws in `src/game`, one `srand` in `World.cpp`; comments and literals
  excluded) and `grep -rnE '\brand\s*\(' src/modules/SD3 | grep -vE 'urand|irand'` (20 lines, 21 draws).
- The session handlers (section 8, row 25): `Handle*` members are the `--all --list` lines of WorldSession matching
  `\bHandle\w*\s*\(` (418); the bound handlers are
  `grep -E '^\s*OPCODE\(' src/game/Server/OpcodeTable.cpp | grep -oE 'WorldSession::\w+' | grep -vE '::Handle_(ServerSide|NULL|EarlyProccess|Deprecated)$' | sort -u > bound.txt`
  (372), and the files defining them
  `grep -rlE '^\w.*\bWorldSession::\w+\s*\(' src/game --include=*.cpp | while read f; do grep -oE '^\w.*\bWorldSession::\w+\s*\(' "$f" | grep -oE 'WorldSession::\w+' | grep -qxFf bound.txt && echo "$f"; done`
  (36, 34 of them in `WorldHandlers/`).
- The build targets (section 6): `grep -rnE "^\s*add_(library|executable)" --include=CMakeLists.txt src` and each
  target's `target_link_libraries`; the `game` globs are `src/game/CMakeLists.txt:1-118` (40 directories); its size is
  `find src/game -name '*.cpp' -o -name '*.h' | xargs cat | wc -l` (399,004). Lines per target layer:
  `python layers.py src files`, summing `wc -l` per file by layer.

<details><summary>layers.py</summary>

```python
#!/usr/bin/env python3
# layers.py <src> files|edges|against|sideways|packets|db|unit  -- design/architecture.md's measurements.
# Every file under src/ gets its target layer (first matching rule wins); an #include resolves relative
# to the includer first, then by path suffix over the tree (system and dep headers are skipped).
import os, re, sys, collections
RULES = [
 (r'^(tools|genrev)/|^game/pchdef', None), (r'^tests/', 'tests'), (r'^realmd/', 'realmd'), (r'^mangosd/', 'app'),
 (r'^shared/Database/|^game/persistence/|^game/Server/GameGlobals|^game/Tools/(PlayerDump|CharacterDatabaseCleaner)', 'persistence'),
 (r'^shared/|^game/Time/|^game/Object/ObjectGuid\.', 'foundation'), (r'^proto/', 'proto'), (r'^(motion|game/movement)/', 'motion'),
 (r'^modules/SD3/|^game/ChatCommands/|^game/WorldHandlers/(Chat\.|ChatArgExtract|ChatHelp|CommandMgr|ScriptMgr|ScriptAction)', 'scripts'),
 (r'^game/(Harness|AuctionHouseBot)/|^game/WorldHandlers/(World\.|WorldConfig)', 'app'),
 (r'^game/Server/(WorldSession|OpcodeTable|SessionMailbox|SessionProtocolPolicy|WorldGateway|WorldNetwork)|^game/WorldHandlers/SpellHandler', 'session'),
 (r'^game/session/', 'session'),
 (r'^game/data/', 'data'),
 (r'^game/Server/|^game/Tools/Language|^game/Object/(ObjectMgr|ItemPrototype|CharacterCache|Taxi)|^game/WorldHandlers/(QuestDef|DisableMgr|PoolManager|GameEventMgr|WaypointManager|CreatureLinkingMgr)', 'data'),
 (r'^game/spells/', 'spells'),
 (r'^game/WorldHandlers/(\w*Handler\w*|ChatMessage\w*|WorldSessionMgr|AccountMgr|GossipDef|UpdateData|LFGPackets)\.', 'session'),
 (r'^game/combat/|^game/Object/(Formulas|StatSystem)', 'combat'),
 (r'^game/Maps/|^game/References/Map|^game/WorldHandlers/(Map|Grid|Cell|ObjectGridLoader|MoveMap|Transport|InstanceData|DynamicCollision|GameObjectModel|LineOfSight|Path\.|BareMap|Weather)', 'maps'),
 (r'^game/Object/[A-Za-z]*AI[A-Za-z]*\.', 'ai'),
 (r'^game/References/Group|^game/Object/(ArenaTeam|Calendar|Guild|GMTicketMgr)|^game/WorldHandlers/(Channel|Group|GuildMgr|Mail|MassMailMgr|LFG)', 'social'),
 (r'^game/(BattleGround|OutdoorPvP)/', 'pvp'),
 (r'^game/Object/(AuctionHouseMgr|LootMgr|ItemEnchantmentMgr)|^game/WorldHandlers/Skill(Discovery|ExtraItems)', 'economy'),
 (r'^game/(entities|Object|MotionGenerators)/|^game/WorldHandlers/Achievement', 'entities')]
DOMAIN = {'entities', 'spells', 'combat', 'maps', 'ai', 'social', 'pvp', 'economy'}
ORDER = ['app', 'scripts', 'session', 'domain', 'data', 'motion', 'proto', 'persistence', 'foundation']
MAY = {l: set(ORDER[i + 1:]) for i, l in enumerate(ORDER)}          # a layer may include every layer below it,
MAY['domain'] -= {'proto'}; MAY['data'] -= {'motion', 'proto'}         # except: no packets below session,
MAY['motion'] -= {'persistence'}; MAY['proto'] -= {'persistence'}      # and the two wire libraries stay pure
def layer(p): return next((l for rx, l in RULES if re.search(rx, p)), 'UNMAPPED')
def tier(l): return 'domain' if l in DOMAIN else l
def files(root):
    return sorted(os.path.relpath(os.path.join(d, f), root).replace(os.sep, '/') for d, _, fs in os.walk(root)
                  for f in fs if f.endswith(('.h', '.hpp', '.cpp', '.inl', '.inc')))
def lines(root, f): return open(os.path.join(root, f), encoding='utf-8', errors='replace')
def includes(root):
    idx = collections.defaultdict(list)
    for f in files(root): idx[os.path.basename(f).lower()].append(f)
    for f in (f for f in files(root) if layer(f)):
        for ln in lines(root, f):
            m = re.match(r'\s*#\s*include\s*["<]([^">]+)[">]', ln)
            if not m: continue
            n = m.group(1).replace(chr(92), '/')
            c = [x for x in idx[os.path.basename(n).lower()] if x.lower().endswith(n.lower().lstrip('./'))]
            rel = os.path.normpath(os.path.join(os.path.dirname(f), n)).replace(os.sep, '/')
            same = [x for x in c if x.split('/')[0] == f.split('/')[0]]
            if c: yield f, (rel if rel in c else (same or c)[0])
def main():
    root, mode = sys.argv[1], sys.argv[2]
    if mode == 'files':
        for f in files(root):
            if layer(f): print(layer(f), f)
    elif mode in ('edges', 'against', 'sideways'):
        n, ex, eg = collections.Counter(), collections.defaultdict(collections.Counter), collections.defaultdict(list)
        for f, t in includes(root):
            a, b = layer(f), layer(t)
            if not b or b == 'tests' or a in ('tests', 'realmd'): continue
            ta, tb = tier(a), tier(b)
            if mode == 'edges': key, bad = (a, b), not (ta == tb or tb in MAY[ta])
            elif mode == 'against': key, bad = (ta, tb), not (ta == tb or tb in MAY[ta])
            else: key, bad = (a, b), ta == tb == 'domain' and a != b
            if mode == 'edges' or bad:
                n[key] += 1; ex[key][os.path.basename(t) if mode != 'edges' else f] += 1
                if f not in eg[key]: eg[key].append(f)
        for k, v in sorted(n.items(), key=lambda x: -x[1]):
            print(f'{k[0]:11} -> {k[1]:11} {v:5}', ', '.join(f'{h} {c}' for h, c in ex[k].most_common(3 if mode == 'edges' else 5)),
                  '' if mode == 'edges' else '| e.g. ' + ', '.join(eg[k][:3]))
        print('TOTAL', sum(n.values()))
    elif mode in ('packets', 'db'):
        rx = re.compile(r'WorldPacket\s+data\s*\(' if mode == 'packets' else r'\b(Character|World|Login)Database\.')
        n = collections.Counter()
        for f in files(root):
            if not layer(f) or f.startswith(('tests/', 'realmd/')): continue
            k = sum(len(rx.findall(ln)) for ln in lines(root, f) if not ln.lstrip().startswith(('//', '*', '/*')))
            if k: n[(layer(f), 'modules/SD3' if f.startswith('modules/SD3') else f.rsplit('/', 1)[0])] += k
        for (l, d), v in sorted(n.items(), key=lambda x: -x[1]): print(v, l, d)
        by = collections.Counter()
        for (l, _), v in n.items(): by[l] += v
        print('TOTAL', sum(by.values()), dict(by.most_common()))
    elif mode == 'unit':   # stdin: method_count.py --class Unit --header <Unit.h> --all --list
        defs = collections.defaultdict(set)
        for f in files(root):
            if f.startswith('game/') and f.endswith('.cpp'):
                for m in re.finditer(r'\bUnit::(~?\w+)\s*\(', ''.join(lines(root, f))): defs[m.group(1)].add(f.rsplit('/', 1)[-1])
        byfile = {'UnitAura': 'auras', 'UnitAuraProcHandler': 'auras', 'UnitSpellBonus': 'spell casting',
                  'UnitDynObject': 'spell casting', 'UnitSpeed': 'movement', 'UnitPower': 'stats and power',
                  'UnitStatModifier': 'stats and power', 'StatSystem': 'stats and power', 'UnitVisibility': 'visibility',
                  **{x: 'combat' for x in ('UnitCombat', 'UnitDamage', 'UnitMeleeDamage', 'UnitThreat', 'UnitHostility', 'UnitDiminishing')}}
        pat = [('auras', r'Aura|Holder|Dispel|Immun|Proc|Charges'), ('spell casting', r'Spell|Cast|Channel|Interrupt|Cooldown|School'),
               ('combat', r'Attack|Combat|Victim|Damage|Melee|Threat|Hostil|Hit|Crit|Dodge|Parry|Block|Resist|Absorb|Evade|Kill|Deal|Weapon|Armor|Heal|Taunt|Death|Dead|Alive|Friendly|Faction|Reaction|Enemy|PvP|Combo|Duel|Target'),
               ('movement', r'Move|Speed|Fall|Jump|Root|Fly|Swim|Walk|Mount|Motion|Position|Orient|Knock|Teleport|Stop|Fear|Confus|Flee|Transport|Vehicle|Stun|Possess|Control|Spline|Levitat|Hover|Water|Taxi|Follow|Chase|Latch|Distance|Stand|Sheath|Mobility'),
               ('stats and power', r'Stat|Power|Health|Mana|Energy|Rage|Level|Regen|Resistance|Modifier|Create|Display|Scale|Bounding|Class|Race|Gender'),
               ('pets, charm, summons', r'Pet|Owner|Charm|Minion|Guardian|Totem|Summon|Creator|Critter|Master')]
        fam = collections.Counter()
        for ln in sys.stdin:
            if 'not counted' in ln or not re.match(r'\S+:\d+:', ln): continue
            m = re.search(r'(~?\w+)\s*\(', ln.split(': ', 1)[1].split('{')[0]); name = m.group(1) if m else '?'
            f = next((byfile[x[:-4]] for x in sorted(defs[name]) if x[:-4] in byfile), None)
            fam[f or next((k for k, rx in pat if re.search(rx, name)), 'lifecycle, update, other')] += 1
        print(sum(fam.values()), dict(fam.most_common()))
main()
```
</details>
