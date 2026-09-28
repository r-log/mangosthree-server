# The target architecture

Measured on master 0bbab743b and decided by the maintainer on 2026-09-28. A **Today:** line marks where the tree
differs from the target. Section 6 lists each divergence with the phase that closes it. Every number comes from a
command in the appendix.

## 1. Layers and the include direction

A layer includes only what its row allows, and nothing includes upward. The chain
`proto -> session -> entities/spells/combat/maps -> motion` is the path a packet takes, not the include direction:
proto is the wire library near the bottom that session and motion build on. The tree agrees: `src/proto` includes
only `src/shared`; session includes it 90 times, the motion layer 19 times (8 from the kernel's writers, 11 from
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
`AuctionHouseBot/` is left out of this table until the question in section 5 is settled.

These directories **dissolve**:
- `Object/` (152 files): 63 to entities, 28 to data, 25 to ai, 10 to social, 9 to spells, 9 to combat, 6 to economy
  and 2 to foundation (`ObjectGuid`).
- `WorldHandlers/` (180 files): 45 to session (the 35 handler files, `GossipDef`, `UpdateData`, `LFGPackets`,
  `AccountMgr`, `WorldSessionMgr`, `ChatMessage*`), 40 to maps, 34 to spells, 26 to social, 12 to scripts, 12 to
  data, 4 to economy, 4 to entities (achievements) and 3 to app.
- `Server/`: 11 files to session, 15 to data and `GameGlobals` to persistence. `References/`: threat to combat,
  maps to maps, groups to social. `Tools/`: the dump and the cleaner to persistence, `Language.h` to data.
- `MotionGenerators/` (the unit's shell over the kernel) goes to entities.

**Today:** 1,118 include lines go against the table.

| From -> to | Lines | Mostly | Three includers |
|---|---|---|---|
| domain -> proto | 267 | `WorldPacket.h` 150, `Opcodes.h` 114 | `Creature.cpp`, `Spell.cpp`, `Guild.cpp` |
| domain -> session | 244 | `WorldSession.h` 108, `UpdateData.h` 82, `GossipDef.h` 51 | `Item.cpp`, `Spell.cpp`, `Guild.cpp` |
| any -> app | 222 | `World.h` (config reads): domain 156, scripts 25, session 25, data 13, persistence 2, foundation 1 | `Unit.cpp`, `Map.cpp`, `WorldSession.cpp` |
| domain, data, session -> scripts | 178 | `Chat.h` 92, `ScriptMgr.h` 86 | `Creature.cpp`, `Spell.cpp`, `ChatHandler.cpp` |
| data -> domain | 161 | `MapManager.h` 13, `SpellMgr.h` 12, `ArenaTeam.h` 10 | `ObjectMgr.h`, `ObjectMgr.cpp`, `CharacterCache.cpp` |
| data -> session | 20 | `GossipDef.h` 11, `AccountMgr.h` 8 | `ObjectMgr.h`, `ObjectMgr.cpp`, `ObjectMgrCreatures.cpp` |
| motion -> domain, session | 12 | `Unit.h` 5, all from `game/movement/` | `MoveSpline.cpp`, `MoveSplineInit.cpp`, `WireParity.cpp` |
| rest | 14 | `ChatCommands` -> `AuctionHouseBot.h`/`Harness.h` 5; `PlayerDump`/`CharacterDatabaseCleaner` 7; `GameEventMgr` 1; `ObjectGuid.cpp` -> `ObjectMgr.h` 1 | |

The domain directories are peers. Include lines that cross between them are allowed, but they cannot grow: they are
a `CheckLayout` ratchet, keyed per includer file and header. Only two seams are gated, `entities/player` and
`spells/aura`.

**Today:** 2,354 such lines, the largest being entities -> maps 472, -> social 344, -> pvp 192 and -> spells 168,
and spells -> entities 295.

## 2. What each layer may know

Entities, and the whole domain tier, hold state and rules. They never build a packet and never write SQL. A manager
returns facts, the owner sends them, and `persistence/` writes them.

Only session builds packets, using `session/packets/<domain>/` together with proto's codecs. When a packet has to go
out in the middle of a domain call, the domain calls a typed fact callback at the old statement, so the packet keeps
its place in the order. The callback's type is declared in the domain, beside the manager, for example a
`std::function<void(Facts const&)>`. Session supplies the implementation, which builds the packet and sends it. The
domain therefore never names session or `WorldPacket`.

`persistence/` makes every `CharacterDatabase.`, `WorldDatabase.` and `LoginDatabase.` call.

**Today (packets):** 433 `WorldPacket data(` sites: session 181, the domain tier 239 (entities 166, 109 of them under
`entities/player/`; social 34, spells 21, combat 7, maps 6, pvp 5), scripts 7, app 4, motion 2. The shared sink
`ManagerPacketSink` is `std::function<void(WorldPacket const*)>`: it names the packet.

**Today (database):** 984 of the 1,008 calls are outside persistence: entities 240 (62 in
`entities/player/persistence/`), social 192, data 142, session 136, scripts 94, app 74, maps 62, economy 22,
spells 11, pvp 9, ai 2. Game `.cpp` files hold 996 calls in 126 files.

**Today (entities, the manager audit):**
- Seven managers build packets: `CurrencyMgr` (4 opcodes), `PetMgr` (1), `HonorMgr` (1), `ReputationMgr` (5),
  `SocialList`/`PlayerSocial` (2), `RuneMgr` (3) and `SpellCooldownMgr` (2).
- Seven call the database: `CurrencyMgr` (3 lines), `QuestStatusMgr` (6), `ReputationMgr` (2), `SocialList` (5),
  `SpellCooldownMgr` (2), `GlyphMgr` (6) and `TalentMgr` (8).
- Three are clean: `InventoryMgr`, `PlayerPetCache` and `QuestRewardRules`.
- `QuestCompletePacket` is a packet builder on the manager list, and the owner-side global `SocialMgr` sends packets.
- `entities/player/README.md` rule 4 ("packets are built in the manager") and `CheckStateOwnership`'s manager rows
  state the opposite of this section.

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
shares. Reputation, currency, honor, runes and achievements therefore stay in entities/player/. SpellCooldownMgr moves
to spells/ on Unit (Creature has its own copy today).

| Today | Target |
|---|---|
| `QuestStatusMgr`, `QuestRewardRules`, `TalentMgr`, `GlyphMgr`, `InventoryMgr`, `PetMgr`, `PlayerPetCache`, `SocialList`, `ReputationMgr`, `CurrencyMgr`, `HonorMgr`, `RuneMgr` (all in `entities/player/`) | stay |
| `AchievementMgr` (`WorldHandlers/`) | `entities/player/` |
| `QuestCompletePacket` | `session/packets/` (a packet builder) |
| `SpellCooldownMgr` | `spells/`, held by `Unit`. **Today:** `Creature` has its own copy (6 methods, `Object/CreatureSpellCooldown.cpp`). |
| `SocialMgr` (a realm-wide global that tells every friend lister about a status change; it holds no player's state) | `social/` |

**Today (`Unit`, 587 member functions, grouped roughly by defining file and name):** combat 146, auras 112, spell
casting 72, movement 72, stats and power 68, lifecycle and update 64, pets/charm/summons 49, visibility 4. The target
puts combat in `combat/`, auras and casting in `spells/` and movement in the motion shell; the rest stays on `Unit`.

**Today (player-only code in `Unit`):** Unit's 15 files hold 134 `(Player*)this` casts and 222 `TYPEID_PLAYER` tests;
5 virtuals are overridden only by `Player` (`IsInWater`, `IsUnderWater`, `ProhibitSpellSchool`, `SetSheath`,
`Uncharm`). The aura and combat bodies live in `Object/`, `WorldHandlers/` and `References/`; only the aura storage
(`spells/AuraContainer.h`) and the leaf math (`combat/`) are already home.

## 4. The finish line per layer: the layout gate matches this page

A layer is done when its gate enforces its rule on a clean clone. The counters are **ratchets against regression
only, not finish lines**: `method_count.py --all` (Player 953, Unit 587, WorldSession 591, ObjectMgr 255), R1, M1,
the downcast counts and the database-call count.

| Layer | Rule | Gate |
|---|---|---|
| every file | lives in a directory named here; includes by path; only the edges section 1 allows | `CheckLayout` (proposed, not built). It ratchets from today's measured edges (1,118 against the rule plus 2,354 across the domain tier), keyed per includer file and header. An edge leaves its allow-list in the PR that removes it, and a new one fails. `CheckIncludeCollisions` is in #174. |
| proto, foundation | nothing above | `CheckProtoBoundary` (holds, 0 lines out). Foundation has no gate yet (2 lines out, `ObjectGuid.cpp`). |
| motion | proto and foundation only | `CheckMotionBoundary`. It holds for `src/motion`; `game/movement/` (12 lines) is outside it. |
| persistence | the only place a `*Database.` call is spelled | `CheckSyncDb` today covers only blocking calls in converted files. #144 extends it, or `CheckLayout` takes the rule. |
| data | no domain, session or scripts header | `CheckLayout`; a `CheckHeaderReach` row for `ObjectMgr.h` |
| domain | no `WorldPacket.h`, `Opcodes.h`, `WorldSession.h` or `*Database.`; the directories are peers under the ratchet; gated seams `entities/player` and `spells/aura`; managers never name `Player` | `CheckLayout`, `CheckManagerIsolation`, `CheckHeaderReach`, `CheckSessionSeam`, `CheckSpatialBoundary`, `CheckBlockedMasks`; `CheckStateOwnership` re-pointed at the session and persistence files |
| session | builds the packets; never includes scripts or app | `CheckLayout`; `CheckStateOwnership` (who may spell an opcode) |
| scripts | SD3 reaches only the script surface and the hook interface | `CheckMotionMasterShim` (`MotionMaster` only); `CheckScriptSurface` (#83, proposed) |

## 5. Not decided

| Question | Options |
|---|---|
| `AuctionHouseBot/` (2 files; a non-blizzlike feature) | keep or remove. The user decides separately. Until then it is measured as app. |

## 6. Divergences

This page resolved four earlier questions: the proto arrow; #76's layout rule, replaced by the ratchet in section 4;
where reputation, currency, honor and runes live; and the rule for the domain tier.

| # | Today, against the target | Closed by |
|---|---|---|
| 1 | domain -> proto: 267 lines; 239 `WorldPacket data(` sites in the domain tier; `ManagerPacketSink` names `WorldPacket` | when content touches each domain; spells in D11 (#142) |
| 2 | domain -> session: 244 lines (`WorldSession.h`, `UpdateData.h`, `GossipDef.h`) | Unit reopen, D11, then when content touches it |
| 3 | `World.h` included 222 times below app | the configuration interface (#143), when content touches it |
| 4 | `Chat.h` (92) and `ScriptMgr.h` (86) included below scripts | the `Chat` split when content touches it; the hook interface (#83) |
| 5 | data -> domain: 161 lines (`ObjectMgr.h` names the entities) | #121, when content needs it |
| 6 | `game/movement/` includes `Unit.h`, transports and `OpcodeTable.h` (12 lines) | when content touches it |
| 7 | `PlayerDump`, `CharacterDatabaseCleaner` and `GameGlobals` (9 lines) and `ObjectGuid.cpp` (2: `World.h`, `ObjectMgr.h`) include above their layer | when content touches it |
| 8 | 984 database calls outside persistence | #144, when content needs it |
| 9 | **Entities, Today:** seven managers build packets (`CurrencyMgr`, `PetMgr`, `HonorMgr`, `ReputationMgr`, `SocialList`/`PlayerSocial`, `RuneMgr`, `SpellCooldownMgr`); seven call the database (`CurrencyMgr`, `QuestStatusMgr`, `ReputationMgr`, `SocialList`, `SpellCooldownMgr`, `GlyphMgr`, `TalentMgr`); three are clean (`InventoryMgr`, `PlayerPetCache`, `QuestRewardRules`) | when the domain is next touched |
| 10 | `QuestCompletePacket` in entities; README rule 4 and `CheckStateOwnership`'s manager rows | the first PR of row 9 |
| 11 | `Player` forwarders for quests, talents and inventory | D4i caller migration (#78) |
| 12 | `SpellCooldownMgr` is player-only, and `Creature` has its own cooldowns | Unit reopen |
| 13 | `Unit`: 134 `(Player*)this` casts, 222 player type tests, 5 Player-only virtuals | Unit reopen |
| 14 | Unit's aura and combat bodies are in `Object/`, `WorldHandlers/` and `References/` | Unit reopen (combat), D11 (spells) |
| 15 | `Object/` and `WorldHandlers/` exist; `session/`, `data/`, `ai/`, `social/`, `pvp/`, `economy/` do not | a move PR before each domain's first seam (#76) |
| 16 | `AchievementMgr` is in `WorldHandlers/`, and `SocialMgr` is under `entities/player/` | their move PRs, when content touches them |
| 17 | 2,354 cross lines inside the domain tier have no ratchet yet | the `CheckLayout` PR |
| 18 | No `CheckLayout`, and no gate for `*Database.` outside persistence | the first PR after this page (ratchet); #144 |

## Appendix: how each number was measured

From the repository root, with Python 3 and Git Bash. `layers.py` is the script below. Its `RULES` table is
section 1's directory table, written as code: `ObjectGuid.h`/`.cpp` are foundation (the 2026-09-28
decision), and `AuctionHouseBot/` stays measured as app until section 5 is settled.

- Where the files go: `python layers.py src files | grep ' game/Object/' | cut -d' ' -f1 | sort | uniq -c` (and
  likewise for `WorldHandlers/`, `Server/`, `References/` and `Tools/`).
- The edge tables: `python layers.py src against` (1,118, with the headers and the includers),
  `python layers.py src sideways` (2,354) and `python layers.py src edges` (every layer pair).
- Packets and the database: `python layers.py src packets` (433 by layer and directory) and `python layers.py src db`
  (1,008). For game `.cpp` only:
  `grep -rhoE '\b(Character|World|Login)Database\.' --include=*.cpp src/game | wc -l` (996), with `-l` for the files
  (126).
- Proto: `python layers.py src edges | grep -E '^(proto|session +-> proto|motion +-> proto)'` (proto -> proto and
  foundation only; session 90; motion 19), split by
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
  `grep -ohE '\(\s*Player\s*\*\s*\)\s*this|static_cast<\s*Player\s*\*\s*>\s*\(\s*this' $F | wc -l` (134) and
  `grep -ohE 'GetTypeId\(\)\s*[!=]=\s*TYPEID_PLAYER' $F | wc -l` (222).
  The Player-only virtuals are the `virtual` lines of the `--all --list` output whose name is declared again in
  `Player.h` and in none of `Creature.h`, `Pet.h`, `Totem.h`, `TemporarySummon.h` or `Vehicle.h` (5 of 34).
- Creature cooldowns: `grep -nE '^\w.*Creature::\w+\(' src/game/Object/CreatureSpellCooldown.cpp` (6).

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
