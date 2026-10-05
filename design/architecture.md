# The target architecture

Measured on master 0bbab743b and decided by the maintainer on 2026-09-28. A **Today:** line marks where the tree
differs from the target. Section 8 lists each divergence with the phase that closes it. Every number comes from a
command in the appendix. Sections 5 and 6 (the threads, the build targets) were added on 2026-09-29, measured on
master 7b6a481ce.

## 1. Layers and the include direction

A layer includes only what its row allows, and nothing includes upward. The chain
`proto -> session -> entities/spells/combat/maps -> motion` is the path a packet takes, not the include direction:
proto is the wire library near the bottom that session and motion build on. The tree agrees: `src/proto` includes
only `src/shared`; session includes it 102 times, the motion layer 19 times (8 from the kernel's writers, 11 from
`game/movement`).

| Layer | Directories in the target | May include |
|---|---|---|
| app | `mangosd/`, the world tick (`World`), `Harness/` | everything |
| scripts | `modules/SD3/` (it implements the domain's script-hook interface, #83), `ChatCommands/`, the command-dispatch half of `Chat`, `CommandMgr`, `ScriptMgr` | everything below app |
| session | `session/`: `WorldSession`, `OpcodeTable`, gateway, `handlers/<domain>/`, `packets/<domain>/`, the message builders split from `Chat` | domain, data, motion, proto, persistence, foundation |
| domain | peers: `entities/`, `spells/`, `combat/`, `maps/`, `ai/`, `social/`, `pvp/`, `economy/`; the script-hook interface | data, motion, persistence, foundation (**not** proto) |
| data | `data/`: DBC/DB2 and SQL stores, `ObjectMgr`'s stores, `QuestDef`, `SharedDefines`, pools, events, the read-only configuration interface (#143) | persistence, foundation |
| motion | `src/motion` (the kernel), `movement/` (splines) | proto, foundation |
| proto | `src/proto` (the wire: `WorldPacket`, `Opcodes`, codecs) | foundation |
| persistence | `src/shared/Database` (the driver), `persistence/<domain>/` (repositories, #144) | foundation |
| foundation | the rest of `src/shared`, `Time/`, `ObjectGuid` (a value type) | nothing |

`realmd` is a separate program and uses only foundation and persistence. `tests` may include anything.
`AuctionHouseBot/` is app: it is kept (the 2026-09-28 decision, section 7), off by default, and reaches the domain only through `World`'s timer and one chat command.

These directories **dissolve**:
- `Object/` (152 files): 63 to entities, 28 to data, 25 to ai, 10 to social, 9 to spells, 9 to combat, 6 to economy
  and 2 to foundation (`ObjectGuid`).
- `WorldHandlers/` (180 files): 45 to session (the 35 handler files, `GossipDef`, `UpdateData`, `LFGPackets`,
  `AccountMgr`, `WorldSessionMgr`, `ChatMessage*`), 40 to maps, 34 to spells, 26 to social, 12 to scripts, 12 to
  data, 4 to economy, 4 to entities (achievements) and 3 to app.
- `Server/`: 11 files to session, 15 to data and `GameGlobals` to persistence. `References/`: threat to combat,
  maps to maps, groups to social. `Tools/`: the dump and the cleaner to persistence, `Language.h` to data.
- `MotionGenerators/` (the unit's shell over the kernel) goes to entities.

**Today:** 1,115 include lines go against the table.

| From -> to | Lines | Mostly | Three includers |
|---|---|---|---|
| domain -> proto | 265 | `WorldPacket.h` 149, `Opcodes.h` 113 | `Creature.cpp`, `Spell.cpp`, `Guild.cpp` |
| domain -> session | 243 | `WorldSession.h` 107, `UpdateData.h` 82, `GossipDef.h` 51 | `Item.cpp`, `Spell.cpp`, `Guild.cpp` |
| any -> app | 222 | `World.h` (config reads): domain 156, scripts 25, session 25, data 13, persistence 2, foundation 1 | `Unit.cpp`, `Map.cpp`, `WorldSession.cpp` |
| domain, data, session -> scripts | 178 | `Chat.h` 92, `ScriptMgr.h` 86 | `Creature.cpp`, `Spell.cpp`, `ChatHandler.cpp` |
| data -> domain | 161 | `MapManager.h` 13, `SpellMgr.h` 12, `ArenaTeam.h` 10 | `ObjectMgr.h`, `ObjectMgr.cpp`, `CharacterCache.cpp` |
| data -> session | 20 | `GossipDef.h` 11, `AccountMgr.h` 8 | `ObjectMgr.h`, `ObjectMgr.cpp`, `ObjectMgrCreatures.cpp` |
| motion -> domain, session | 12 | `Unit.h` 5, all from `game/movement/` | `MoveSpline.cpp`, `MoveSplineInit.cpp`, `WireParity.cpp` |
| rest | 14 | `ChatCommands` -> `AuctionHouseBot.h`/`Harness.h` 5; `PlayerDump`/`CharacterDatabaseCleaner` 7; `GameEventMgr` 1; `ObjectGuid.cpp` -> `ObjectMgr.h` 1 | |

The domain directories are peers. Include lines that cross between them are allowed, but they cannot spread: they are
a `CheckLayout` ratchet, keyed per includer directory, includer peer and header. The peer is the coupling unit; the
directory stands in for it until `Object/`, `WorldHandlers/` and `References/` dissolve, when the key collapses to
directory and header. A new file of a listed peer in a listed directory that includes a listed header adds no key, so
a file can split inside its directory; a new key fails. Only two seams are gated, `entities/player` and
`spells/aura`, from both sides: an include into either from another peer directory and from the seam's own peer
(`Object/Unit.cpp` -> `Player.h`, 34 lines; `SpellMgr.h` -> `SpellAuraDefines.h`, 16 lines; the 2026-09-28 decision)
is listed per includer file and header and cannot grow, and so is an include against the table. Such a line may
change its includer file in the PR that deletes the old one, only within the same layer pair and header; a change of
layer pair or header is a new edge.

**Today:** 2,356 such lines, the largest being entities -> maps 472, -> social 344, -> pvp 192 and -> spells 169,
and spells -> entities 296.

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

Two consequences. `entities -> social` (344 lines today, the largest after maps) is debt: a player asks its group
through a fact callback declared beside the player and supplied by social, the shape the group update flags cluster
waits for. And `entities -> pvp` (192) is the same shape: a battleground tells the player, not the reverse.

## 2. What each layer may know

Entities, and the whole domain tier, hold state and rules. They never build a packet and never write SQL. A manager
returns facts, the owner sends them, and `persistence/` writes them.

Only session builds packets, using `session/packets/<domain>/` together with proto's codecs. When a packet has to go
out in the middle of a domain call, the domain calls a typed fact callback at the old statement, so the packet keeps
its place in the order. The callback's type is declared in the domain, beside the manager, for example a
`std::function<void(Facts const&)>`. Session supplies the implementation, which builds the packet and sends it. The
domain therefore never names session or `WorldPacket`.

`persistence/` makes every `CharacterDatabase.`, `WorldDatabase.` and `LoginDatabase.` call.

**Today (packets):** 425 `WorldPacket data(` sites: session 181, the domain tier 231 (entities 158, 102 of them under
`entities/player/`; social 34, spells 21, combat 7, maps 6, pvp 5), scripts 7, app 4, motion 2. The shared sink
`ManagerPacketSink` is `std::function<void(WorldPacket const*)>`: it names the packet. What a player's own client is
told when its swing is out of reach or faces away, its attack or its auto-repeat spell is cancelled, its pet is set
or its stand state changes are six typed facts declared beside `Player` (`PlayerClientFacts.h`), built into packets by
`session/packets/combat/`, `spells/` and `entities/` and sent through the callbacks `InstallPlayerPacketSinks`
installs where a player is created.

**Today (database):** 984 of the 1,008 calls are outside persistence: entities 240 (62 in
`entities/player/persistence/`), social 192, data 142, session 136, scripts 94, app 74, maps 62, economy 22,
spells 11, pvp 9, ai 2. Game `.cpp` files hold 996 calls in 126 files.

**Today (entities, the manager audit):**
- Six managers build packets: `CurrencyMgr` (4 opcodes), `PetMgr` (1), `HonorMgr` (1), `ReputationMgr` (5),
  `SocialList`/`PlayerSocial` (2) and `RuneMgr` (3).
- Six call the database: `CurrencyMgr` (3 lines), `QuestStatusMgr` (6), `ReputationMgr` (2), `SocialList` (5),
  `GlyphMgr` (6) and `TalentMgr` (8).
- Three are clean: `InventoryMgr`, `PlayerPetCache` and `QuestRewardRules`.
- In `spells/`, `SpellCooldownMgr` calls the database (2 lines) and builds no packet: it reports a cooldown event
  and the clear of every cooldown as typed facts, and `session/packets/spells/` builds the two packets.
- `QuestCompletePacket` is a packet builder on the manager list, and the owner-side global `SocialMgr` sends packets.
- `CheckStateOwnership`'s manager rows allow each packet-building manager to spell its opcodes, the opposite of
  this section; the cooldown row allows them to the session's builder instead. That row holds `SMSG_COOLDOWN_EVENT`
  and the whole-map `SMSG_CLEAR_COOLDOWNS` only: `SMSG_SPELL_COOLDOWN` and `SMSG_ITEM_COOLDOWN` are the cast's and the
  item's notices, the one-spell clear stays with `Player::SendClearCooldown`, nothing builds `SMSG_MODIFY_COOLDOWN` or
  `SMSG_COOLDOWN_CHEAT`, and a builder of `SMSG_MODIFY_COOLDOWN` for a character's spell joins the row. The pet row
  holds the one packet `PetMgr` builds, `SMSG_PET_SPELLS` with an empty guid (`PetMgr::RemoveActionBar`, which clears
  the client's pet action bar): the same opcode's full forms, the spell bars of the pet, of a possessed unit and of a
  charmed unit, are the owner's (`PlayerPet.cpp`), and no other `PET` opcode is the manager's. The row names no table:
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

Each correction is one PR per manager, with the harness record byte-identical.

## 3. Shared versus player-only

entities/player/ holds every state only a player has; the domain directories (spells/, combat/, ...) hold what Unit
shares. Reputation, currency, honor, runes and achievements therefore stay in entities/player/. SpellCooldownMgr lives
in spells/ on Unit (Creature has a different cooldown model today).

| Today | Target |
|---|---|
| `QuestStatusMgr`, `QuestRewardRules`, `TalentMgr`, `GlyphMgr`, `InventoryMgr`, `PetMgr`, `PlayerPetCache`, `SocialList`, `ReputationMgr`, `CurrencyMgr`, `HonorMgr`, `RuneMgr` (all in `entities/player/`) | stay |
| `AchievementMgr` (`WorldHandlers/`) | `entities/player/` |
| `QuestCompletePacket` | `session/packets/` (a packet builder) |
| `SpellCooldownMgr` | `spells/`, held by `Unit`. **Today:** `Creature` has a different cooldown model (6 methods, `Object/CreatureSpellCooldown.cpp`). |
| `SocialMgr` (a realm-wide global that tells every friend lister about a status change; it holds no player's state) | `social/` |

**Today (`Unit`, 629 member functions, grouped roughly by defining file and name):** combat 155, auras 113, spell
casting 80, lifecycle and update 80, movement 75, stats and power 69, pets/charm/summons 53, visibility 4. The target
puts combat in `combat/`, auras and casting in `spells/` and movement in the motion shell; the rest stays on `Unit`.

**Today (player-only code in `Unit`):** Unit's 15 files hold 35 `(Player*)this` casts and 214 `TYPEID_PLAYER` tests;
49 virtuals are overridden only by `Player` (`IsInWater`, `IsUnderWater`, `ProhibitSpellSchool`, `SetSheath`,
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
test: `SetPosition`, `isMoving`; and the client's control of its own movement, taken and returned by fear and
confuse: `SetClientControl`; each with the non-player answer as `Unit`'s default).
The aura and combat bodies live in `Object/`, `WorldHandlers/` and `References/`; only the aura storage
(`spells/AuraContainer.h`) and the leaf math (`combat/`) are already home.

## 4. The finish line per layer: the layout gate matches this page

A layer is done when its gate enforces its rule on a clean clone, and it is finished when it is its own build target
(section 6): from that day the linker enforces the include direction and the gate only guards what the linker cannot
see. The counters are **ratchets against regression only, not finish lines**: `method_count.py --all` (Player 953, Unit 587, WorldSession 591, ObjectMgr 255), R1, M1,
the downcast counts and the database-call count.

| Layer | Rule | Gate |
|---|---|---|
| every file | lives in a directory named here; includes by path; only the edges section 1 allows | `CheckLayout`, built (#179). It ratchets from today's measured edges (1,118 against the rule plus 2,354 across the domain tier). Edges against the rule or into a gated seam are keyed per includer file and header; a line may change its includer file in the PR that deletes the old one, only within the same layer pair and header, and a change of layer pair or header is a new edge. Sideways edges are keyed per includer directory, includer peer and header: the peer is the coupling unit, the directory stands in for it until `Object/`, `WorldHandlers/` and `References/` dissolve, and the key then collapses to directory and header. A file can split inside its directory. An edge leaves its allow-list in the PR that removes it, and a new one fails. `CheckIncludeCollisions` is in #174. |
| proto, foundation | nothing above | `CheckProtoBoundary` (holds, 0 lines out). Foundation has no gate yet (2 lines out, `ObjectGuid.cpp`). |
| motion | proto and foundation only | `CheckMotionBoundary`. It holds for `src/motion`; `game/movement/` (12 lines) is outside it. |
| persistence | the only place a `*Database.` call is spelled | `CheckSyncDb` today covers only blocking calls in converted files. #144 extends it, or `CheckLayout` takes the rule. |
| data | no domain, session or scripts header | `CheckLayout`; a `CheckHeaderReach` row for `ObjectMgr.h` |
| domain | no `WorldPacket.h`, `Opcodes.h`, `WorldSession.h` or `*Database.`; the directories are peers under the ratchet; gated seams `entities/player` and `spells/aura`; managers never name `Player` | `CheckLayout`, `CheckManagerIsolation`, `CheckHeaderReach`, `CheckSessionSeam`, `CheckSpatialBoundary`, `CheckBlockedMasks`; `CheckStateOwnership` re-pointed at the session and persistence files |
| session | builds the packets; never includes scripts or app | `CheckLayout`; `CheckStateOwnership` (who may spell an opcode) |
| scripts | SD3 reaches only the script surface and the hook interface | `CheckMotionMasterShim` (`MotionMaster` only); `CheckScriptSurface` (#83, proposed) |

## 5. The threads, and who owns what

The layers say who may *include* what; this section says who may *touch* what at run time. The model is already
in the tree, and most of it is already enforced. It is written here because it is the part of the architecture
that is correct and invisible.

**One world thread owns the game.** `World::Update` runs the tick under `TickGuard::Scope`, in this order: the
timed managers (mail, auctions, the auction bot, LFG), then `UpdateSessions` (every session's queued packets are
handled here, on this thread, outside any map), then `MapManager::Update`, then the harness, battlegrounds and
outdoor PvP, then `ProcessResultQueue` for the three databases (every async query's continuation runs here), then the
console's queued commands. Nothing else touches game state.

**Map workers own one map at a time.** `MapManager::Update` opens the map phase (`MapPhase::Begin`), hands every
world map to the `MapUpdater` pool (`MapUpdateThreads`, default 2; each worker is a registered MySQL client thread
through `DbThreadGuard`), blocks at the barrier (`wait`), and closes the phase. `Map::Update` takes a
`MapPhase::Scope`, so the thread running it owns that map; a transport deck's update is nested inside the map it
sails and takes its own scope, handing ownership back on return. With the pool off, or under the harness's stepped
clock, the world thread updates the maps in turn, one scope at a time, which is what makes a harness run
deterministic. A vessel that reaches the end of a map decides so on that map's thread and *crosses* only after the
barrier, on the world thread, because the destination may be updating on another core.

**Network threads own sockets, never game state.** `net::Server` (epoll, kqueue, IOCP or io_uring behind one facade)
runs one poller per worker, one `ClientConnection` per client, and a decoded packet crosses to the game through
exactly one door: the session's `SessionMailbox`, a locked queue drained by `UpdateSessions`. Sends go the other way
through the same connection object.

**Three database delay threads** (login, characters, world) execute the async queries; their results wait in the
`SqlResultQueue` until the world thread calls `ProcessResultQueue`. **Three service threads** never touch the game:
the console reader queues commands for the world thread, the log's console writer, and the anti-freeze watchdog,
which aborts the process when the tick counter stops moving.

| Rule | Enforced by |
|---|---|
| Game state is touched by the world thread, or by the worker whose scope owns the map | `MapPhase::Owns` (counted process-wide, asserted under `MANGOS_DEBUG`), today at the movement kernel |
| Session handlers run on the world thread outside the map phase | the tick's order; `CheckSessionSeam` |
| No synchronous database acquisition under the tick | `TickGuard` (strict mode asserts; the CI job proves the four strict cases stand) and `CheckSyncDb` |
| Packets cross from the network only through `SessionMailbox` | the `proto` boundary: `IClientLink` / `IWorldGateway` |
| A map never writes into another map during the phase | transport crossings run after the barrier |
| A harness run is deterministic | the stepped clock runs every map inline, in order, on one thread |

**Today:**
- Ownership is checked at one seam only, the movement kernel. Every other part of a map's state (its grids, its
  object stores, the spell and aura code that D11 opens) relies on the phase structure and on nobody reaching across;
  the D11 spell seam is the natural place for the second check.
- The network pool is sized by `hardware_concurrency()` (`net/reactor/ReactorServer.cpp:110`), not by the
  configuration. `Network.Threads` said otherwise and was read by no source file; it was removed from
  `mangosd.conf.dist.in` on 2026-09-29. `Network.OutKBuff`, `Network.OutUBuff` and `Network.TcpNodelay` are read by
  no source file either; the engine sets its own socket options.
- The game draws from the seeded `RNG` only: its 7 C library `rand()` draws moved to it, and `CheckRawRand` holds
  `src/game` at 0 draws and one `srand`, `World.cpp`'s. SD3 is the exception until #83: its 21 `rand()` draws (20
  lines in 10 script files) still use the C library. On Windows the UCRT keeps `rand()` state per thread, so
  `World.cpp:241`'s `srand` seeds only the main thread, and SD3's draws on the pooled map workers
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
| data, domain, session, `persistence/<domain>/`, and the app and scripts files inside `src/game` | **`game`**, one library: 27 globbed directories, 388k lines, one precompiled header | `shared_db`, `terrain`, `geometry`, `proto`, `motion`, Detour, zlib, and `mangosscript` |
| scripts (SD3) | `mangosscript` | `game` |
| app | `mangosd` | `game`, `proto` |
| tests | `mangos_tests` | `game`, `motion`, `proto` |

Two things the table shows. `game` and `mangosscript` link *each other* (`game/CMakeLists.txt:170`,
`modules/SD3/CMakeLists.txt:361`): scripts are above the domain in the page and beside it in the build. And the only
layer boundary the linker checks today is the one around `motion` and `proto`; every include the gate counts as
against the rule in section 1 lives inside `game`, where the linker sees one target.

The sizes, by target layer (from `layers.py files`, lines of `.h`/`.cpp`): scripts 264k (SD3 and `ChatCommands`),
entities 98k, spells 60k, data 38k, session 38k, app 32k (with the harness), social 21k, pvp 21k, maps 19k,
persistence 10k, combat 9k, ai 7k, economy 5k.

**The rule for splitting.** A layer becomes its own target in the PR that brings its upward edges to zero, never
before: a library with an upward include does not link, so the split *is* that layer's finish line, and it cannot be
faked. The domain tier splits as one library first (its directories are peers with cycles: entities <-> spells
295 / 168) and into per-directory libraries only if a cycle is ever cut on purpose. The order follows section 8:

1. `data`: after rows 5 and 7 (161 lines into the domain, 20 into session).
2. `domain`: after rows 1, 2, 3 and 4 (proto, session, `World.h`, `Chat.h` / `ScriptMgr.h`).
3. `session`: after the `World.h` and script edges it holds.
4. `scripts`: `ChatCommands`, the command half of `Chat`, `ScriptMgr` join `mangosscript`, and the `game ->
   mangosscript` link is replaced by the hook interface (#83), which ends the cycle.
5. `app`: `World`, the harness and the auction bot move to `mangosd` or a `world` library; `game` no longer exists.

Each split PR moves the layer's `file(GLOB)` lines into the new target's `CMakeLists.txt`, gives it its own
precompiled header or none, and deletes that layer's rows from `layout_allow.txt`. `mangos_tests` links every
target, as it links three today.

## 7. Not decided

| Question | Options |
|---|---|
| `AuctionHouseBot/` (2 files; a non-blizzlike feature) | **Kept** (decided 2026-09-28): optional, off by default, measured as app. Not a decoupling concern; a harness family that needs a quiet auction house disables it in the configuration rather than removing it. |
| The script-hook interface (#83): designed from what scripts need, or from what `Creature` exposes | **From what scripts need** (decided 2026-10-02). SD3 is 264k lines, the largest layer, and the one the dungeon work lives in; an interface drawn from `Creature`'s current surface would gate the reach scripts have today and keep it. The interface is drawn from the calls scripts make, measured: `getVictim` 1,216, `SetData` 838, `SummonCreature` 609, `GetMap` 520, `SelectHostileTarget` 496, `GetSingleCreatureFromStorage` 471, `GetMotionMaster` 471, `SelectAttackingTarget` 455, `Where` 341, `GetData` 241, the gossip macros 574 over four names, `GetHealthPercent` 198, `SetStandState` 169, the flag pair 297, `RemoveAurasDueToSpell` 142, `ForcedDespawn` 141, `GetQuestStatusMgr` 128, `HandleEmote` 110, `GroupEventHappens` 94 (see the appendix). The twenty names above cover most of the 839 script files' reach; #83's first PR writes them as the interface, with `GetMap` and `GetMotionMaster` replaced by what the scripts do through them (a map query, a move request), since handing a script the map or the kernel is the reach the gate exists to end. Everything a script reaches that is not on the interface is a ratchet from the day the interface lands. |

## 8. Divergences

This page resolved four earlier questions: the proto arrow; #76's layout rule, replaced by the ratchet in section 4;
where reputation, currency, honor and runes live; and the rule for the domain tier.

| # | Today, against the target | Closed by |
|---|---|---|
| 1 | domain -> proto: 265 lines; 231 `WorldPacket data(` sites in the domain tier; `ManagerPacketSink` names `WorldPacket`; `Player::SetClientControl`, reached from `Unit` as an override, builds its control packet and grants or revokes its session's mover authority on a `Unit*`: a seam of its own kind, not a client fact | when content touches each domain; spells in D11 (#142); `SetClientControl`'s own seam |
| 2 | domain -> session: 243 lines (`WorldSession.h`, `UpdateData.h`, `GossipDef.h`) | Unit reopen, D11, then when content touches it |
| 3 | `World.h` included 222 times below app | the configuration interface (#143), when content touches it |
| 4 | `Chat.h` (92) and `ScriptMgr.h` (86) included below scripts | the `Chat` split when content touches it; the hook interface (#83) |
| 5 | data -> domain: 161 lines (`ObjectMgr.h` names the entities) | #121, when content needs it |
| 6 | `game/movement/` includes `Unit.h`, transports and `OpcodeTable.h` (12 lines) | when content touches it |
| 7 | `PlayerDump`, `CharacterDatabaseCleaner` and `GameGlobals` (9 lines) and `ObjectGuid.cpp` (2: `World.h`, `ObjectMgr.h`) include above their layer | when content touches it |
| 8 | 984 database calls outside persistence | #144, when content needs it |
| 9 | **Entities, Today:** six managers build packets (`CurrencyMgr`, `PetMgr`, `HonorMgr`, `ReputationMgr`, `SocialList`/`PlayerSocial`, `RuneMgr`); six call the database (`CurrencyMgr`, `QuestStatusMgr`, `ReputationMgr`, `SocialList`, `GlyphMgr`, `TalentMgr`); three are clean (`InventoryMgr`, `PlayerPetCache`, `QuestRewardRules`). **Spells, Today:** `SpellCooldownMgr` calls the database and builds no packet: its cooldown event and clear of every cooldown are typed facts, built into packets by `session/packets/spells/` and sent through the callbacks the session installs where a player is created | when the domain is next touched |
| 10 | `QuestCompletePacket`, a packet builder, in `entities/player/quests/`; `CheckStateOwnership`'s rows for the managers that still build their packets | the quest builder's own seam (its owner reports a fact) before its move to `session/packets/quests/`; each manager's row with that manager under row 9 |
| 11 | `Player` forwarders for quests, talents and inventory | D4i caller migration (#78) |
| 12 | `SpellCooldownMgr` is in `spells/` and held by `Unit`, but only players use it (the type guards at its `Unit` call sites stay), and `Creature` has its own cooldown model (scenario 938) | the Creature fold, only on 4.3.4 evidence |
| 13 | `Unit`: 35 `(Player*)this` casts, 214 player type tests, 49 Player-only virtuals | Unit reopen |
| 14 | Unit's aura and combat bodies are in `Object/`, `WorldHandlers/` and `References/` | Unit reopen (combat), D11 (spells) |
| 15 | `Object/` and `WorldHandlers/` exist; `data/`, `ai/`, `social/`, `pvp/`, `economy/` do not; `session/` holds only the builders under `session/packets/`, and the session's other files are in `Server/` and `WorldHandlers/` | a move PR before each domain's first seam (#76); a seam creates the new builder files it needs in their target directory and moves no existing file |
| 16 | `AchievementMgr` is in `WorldHandlers/`, and `SocialMgr` is under `entities/player/` | their move PRs, when content touches them |
| 17 | 2,356 cross lines inside the domain tier have no ratchet yet | built (#179); re-keyed per includer directory, peer and header on 2026-09-29 |
| 18 | No `CheckLayout`, and no gate for `*Database.` outside persistence | built (#179) (the ratchet; sideways lines re-keyed per includer directory, peer and header on 2026-09-29); #144 |
| 19 | Thread ownership (`MapPhase::Owns`) is checked at the movement kernel only | the D11 spell seam takes the second check |
| 20 | `Network.OutKBuff`, `Network.OutUBuff` and `Network.TcpNodelay` are read by no source file (`Network.Threads` was, and is deleted) | delete them or wire them through #143 |
| 21 | 7 raw `rand()` draws in 5 `src/game` files, seeded by `World.cpp`'s `srand`, share one generator across the map workers | closed on 2026-09-30 by D11's named change: the 7 draws use the seeded `RNG`, and `CheckRawRand` keeps `src/game` at 0 draws. The one generator held on glibc only: the Windows UCRT keeps `rand()` state per thread, so `World.cpp:241`'s `srand` seeded the main thread and the pooled map workers drew from an unseeded stream (seed 1). The `srand` stays for glibc, where SD3's draws share its wall-clock-seeded stream; #83 removes it with SD3's draws and drops the gate's allowance to 0 |
| 22 | `game` is one target holding data, domain, session, the domain repositories, and app and scripts files; the linker checks only the `motion` / `proto` boundary | one split per layer, in section 6's order, each in the PR that zeroes that layer's upward edges |
| 23 | `game` and `mangosscript` link each other | the hook interface (#83), section 6 step 4 |
| 24 | `Unit`: five casts return the player itself (`Unit.cpp:1046`, `:3403`, `:3419`, `:4584`, `:5746`); they call no Player method, so the standing shape does not apply | a shape of their own, after the clean clusters (D14) |

## Appendix: how each number was measured

The script-hook table (section 7): the calls SD3 makes on its creature, its player and its instance, counted by
name over the scripts' sources:

```
grep -rhoE "m_creature->[A-Za-z_]+\(|pPlayer->[A-Za-z_]+\(|m_pInstance->[A-Za-z_]+\(" \
    src/modules/SD3/scripts --include=*.cpp | sort | uniq -c | sort -rn | head -25
```

From the repository root, with Python 3 and Git Bash. `layers.py` is the script below. Its `RULES` table is
section 1's directory table, written as code: `ObjectGuid.h`/`.cpp` are foundation (the 2026-09-28
decision), and `AuctionHouseBot/` is app (kept, section 7).

- Where the files go: `python layers.py src files | grep ' game/Object/' | cut -d' ' -f1 | sort | uniq -c` (and
  likewise for `WorldHandlers/`, `Server/`, `References/` and `Tools/`).
- The edge tables: `python layers.py src against` (1,115, with the headers and the includers),
  `python layers.py src sideways` (2,356) and `python layers.py src edges` (every layer pair).
- Packets and the database: `python layers.py src packets` (425 by layer and directory) and `python layers.py src db`
  (1,008). For game `.cpp` only:
  `grep -rhoE '\b(Character|World|Login)Database\.' --include=*.cpp src/game | wc -l` (996), with `-l` for the files
  (126).
- Proto: `python layers.py src edges | grep -E '^(proto|session +-> proto|motion +-> proto)'` (proto -> proto and
  foundation only; session 102; motion 19), split by
  `grep -hE '#\s*include\s*"(wire/[A-Za-z]+\.h|WorldPacket\.h|Opcodes\.h)"' src/motion/* | wc -l` (8) and the
  same over `src/game/movement/*` (11).
- The manager audit, per manager `M`:
  `cat M.h M.cpp | grep -v '^\s*//' | grep -oE 'SMSG_[A-Z_]+' | sort -u` (opcodes built) and
  `cat M.h M.cpp | grep -cE '\b(Character|World|Login)Database\.'` (database lines).
- Method counts:
  `python src/tests/tools/method_count.py --class <C> --header <C.h> --all` for Player (`entities/player/Player.h`),
  Unit (`Object/Unit.h`), WorldSession (`Server/WorldSession.h`) and ObjectMgr (`Object/ObjectMgr.h`).
- Unit's families:
  `python src/tests/tools/method_count.py --class Unit --header src/game/Object/Unit.h --all --list | python layers.py src unit`.
- The player-only leaks, with `F="src/game/Object/Unit*.cpp src/game/WorldHandlers/UnitAuraProcHandler.cpp"`:
  `grep -ohE '\(\s*Player\s*\*\s*\)\s*this|static_cast<\s*Player\s*\*\s*>\s*\(\s*this' $F | wc -l` (35) and
  `grep -ohE 'GetTypeId\(\)\s*[!=]=\s*TYPEID_PLAYER' $F | wc -l` (214).
  The Player-only virtuals are the `virtual` lines of the `--all --list` output whose name is declared again in
  `Player.h` and in none of `Creature.h`, `Pet.h`, `Totem.h`, `TemporarySummon.h` or `Vehicle.h`, where only a
  unit's declaration counts: `Vehicle.h`'s `GetTransport` is `TransportInfo`'s (49 of 78).
- Creature cooldowns: `grep -nE '^\w.*Creature::\w+\(' src/game/Object/CreatureSpellCooldown.cpp` (6).
- The threads (section 5): the tick's order is `World::Update` (`WorldHandlers/World.cpp:966`, the `TickGuard::Scope` at the top of its
  body) read top to bottom; the map split is `MapManager::Update` (`WorldHandlers/MapManager.cpp:306`);
  the thread starts are `grep -rnE "std::thread|StartConsoleThread|HaltDelayThread" src/mangosd src/game src/shared`
  (the map pool in `Maps/MapUpdater.cpp`, the CLI reader and the watchdog in `mangosd/`, the console writer in
  `shared/Log`, one delay thread per database); the network pool's size is `net/reactor/ReactorServer.cpp:110`;
  `grep -rl "Network\.\(OutKBuff\|OutUBuff\|TcpNodelay\)" src` finds only `mangosd.conf.dist.in`; the strict-mode proof is the
  "Check strict tick mode is armed" step of `core_linux_build.yml`. Raw generators:
  `python src/tests/tools/raw_rand.py --list` (0 draws in `src/game`, one `srand` in `World.cpp`; comments and literals
  excluded) and `grep -rnE '\brand\s*\(' src/modules/SD3 | grep -vE 'urand|irand'` (20 lines, 21 draws).
- The build targets (section 6): `grep -rnE "^\s*add_(library|executable)" --include=CMakeLists.txt src` and each
  target's `target_link_libraries`; the `game` globs are `src/game/CMakeLists.txt:1-82` (27 directories); its size is
  `find src/game -name '*.cpp' -o -name '*.h' | xargs cat | wc -l` (388,249). Lines per target layer:
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
 (r'^shared/Database/|^game/Server/GameGlobals|^game/Tools/(PlayerDump|CharacterDatabaseCleaner)', 'persistence'),
 (r'^shared/|^game/Time/|^game/Object/ObjectGuid\.', 'foundation'), (r'^proto/', 'proto'), (r'^(motion|game/movement)/', 'motion'),
 (r'^modules/SD3/|^game/ChatCommands/|^game/WorldHandlers/(Chat\.|ChatArgExtract|ChatHelp|CommandMgr|ScriptMgr|ScriptAction)', 'scripts'),
 (r'^game/(Harness|AuctionHouseBot)/|^game/WorldHandlers/(World\.|WorldConfig)', 'app'),
 (r'^game/Server/(WorldSession|OpcodeTable|SessionMailbox|SessionProtocolPolicy|WorldGateway|WorldNetwork)|^game/WorldHandlers/SpellHandler', 'session'),
 (r'^game/session/', 'session'),
 (r'^game/Server/|^game/Tools/Language|^game/Object/(ObjectMgr|ItemPrototype|CharacterCache|Taxi)|^game/WorldHandlers/(QuestDef|DisableMgr|PoolManager|GameEventMgr|WaypointManager|CreatureLinkingMgr)', 'data'),
 (r'^game/WorldHandlers/(Spell|UnitAuraProcHandler)|^game/Object/(SpellMgr|UnitAura|UnitSpellBonus)|^game/spells/', 'spells'),
 (r'^game/WorldHandlers/(\w*Handler\w*|ChatMessage\w*|WorldSessionMgr|AccountMgr|GossipDef|UpdateData|LFGPackets)\.', 'session'),
 (r'^game/combat/|^game/References/(Hostile|Threat)|^game/Object/(Unit(Combat|Damage|MeleeDamage|Threat|Hostility|Diminishing)|Formulas|StatSystem|CreatureThreat)', 'combat'),
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
