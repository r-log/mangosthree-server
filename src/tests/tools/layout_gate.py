#!/usr/bin/env python3
"""layout_gate.py [--root <repo root>] [--allow <list>] --check | --generate | --self-test

CheckLayout: the include-direction ratchet of design/architecture.md (sections 1 and 4). Every
C/C++ file under src/ gets its target layer from RULES, the page's appendix table (`layers.py`)
written as code; section 1's "may include" column is MAY. Each `#include "..."` line is an edge
from the includer to the header it names, and each edge is one of:

  allowed   the includer's layer may include the header's (section 1), or both are in the same
            layer; tests may include anything.
  against   an upward or forbidden edge (section 1's "Today: 1,118"), keyed by tier pair.
  sideways  both ends in the domain tier, in two different peer directories (section 1's
            "Today: 2,354"): allowed by the page but ratcheted, keyed by layer pair.
  seam      a sideways edge INTO one of the two gated seams, entities/player and spells/aura
            (sections 1 and 4): counted as against the rule, not sideways. With SEAM_SAME_PEER
            off only the peer-crossing edge is a seam edge; an edge from inside the same peer but
            outside the seam (Object/Unit.cpp to entities/player/Player.h) is not sideways and
            stays allowed. SEAM_SAME_PEER on (today, the 2026-09-28 decision) makes that edge a
            seam edge too.

src/tests/layout_allow.txt holds the violations in the tree, one line per key, paths from the
repository root, one sort order. The key depends on the kind (the 2026-09-29 re-key):

  sideways        "<includer directory> -> <header>": the includer file's own directory (its
                  immediate one: src/game/WorldHandlers, src/game/Object, src/game/spells/handlers
                  are three keys; a subdirectory is never folded into its parent). A new file in a
                  listed directory including a listed header is not a new edge, so a file can split
                  inside its directory; a new (directory, header) pair fails. An edge OUT of a gated
                  seam (entities/player/Player.cpp -> Maps/Map.h) is sideways; only an edge INTO a
                  seam is a seam edge.
  against, seam   "<includer file> -> <header>", one line per includer file and header, however
                  many times the file includes it. The line may change its includer: per (kind,
                  header) the number of includer files in the tree must not exceed the number of
                  listed lines, so a new (file, header) edge whose kind and header match a listed
                  line that is gone from the tree is a MOVE -- the gate fails until the old line is
                  replaced by the new one ("moved: replace the old line ... with ..."); a new edge
                  beyond the listed count is new and fails.

--check (the gate) fails on a key the list does not hold (a new one), on a move whose line is not
yet replaced, and on a list line whose key is not in the tree or is no longer a violation (the
page: "an edge leaves its allow-list in the PR that removes it"). It passes with a per-layer-pair
summary, in numbers. --generate rewrites the list from the tree and prints what it added and
removed; run it after removing or moving edges, never to make a new edge pass. The list is a text
file: that a line was not simply added for a new edge is the review's to see, as before.

Which files: every .h, .hpp, .cpp, .inl and .inc under src/. A file no RULES row matches fails
the gate; tools/, genrev/ and game/pchdef.* are excluded by a row of their own (separate
programs and the precompiled header, as on the page), and are neither checked nor a target.
realmd is checked against section 1's line: it uses only foundation and persistence (the page's
script skipped realmd includers; nothing in realmd is against that line today).

How a header resolves: relative to the includer's directory first (the compiler's first place
for a quoted include too); otherwise the spelling must be a path suffix, at a directory boundary,
of exactly ONE file under src/ ("Unit.h" or "Object/Unit.h" for src/game/Object/Unit.h). Two or
more such files and no relative hit: the include is ambiguous and fails, naming every candidate
-- include it by a longer path or relative to the includer. Matching ignores letter case, as the
page did. A spelling not under src/ is looked up the same way under dep/ (third-party: zlib, Detour, utf8cpp; skipped) and as a configure_file()
template "<spelling>.in" under src/ (BuildInfo.h; generated, skipped). A quoted spelling found
nowhere fails, as CheckHeaderReach fails on a nonexistent header.

The gate does NOT model the targets' include paths (which directories each target searches, in
which order; MSVC's extra search of the parent includers' directories). Because a relative hit is
the compiler's own first choice and any other hit must be unique under src/, the gate and the
compiler can differ on only three kinds of spelling: (1) one the compiler finds first outside
src/ -- a dep/, generated or system header with the same spelling as the unique src/ file the
gate takes; (2) one that names a unique src/ file in a directory that is not on the includer's
include path (today only src/game/movement is off game's), which the gate resolves and the
compiler does not -- that include does not build, so the build fails, not the gate; and (3) a
relative hit that only matches when case is ignored: the gate takes the file next to the
includer, while GCC on Linux misses it and resolves the spelling elsewhere through the include
directories (the tree has the shape: shared/Auth/HMACSHA1.h and shared/Crypto/HmacSha1.h).
None occurs today: a per-target emulation of the compiler's search matched the gate on every
include the 25 Windows projects compile (10,146 of 10,158 pairs; the 12 others are Linux-only
foundation and tests files), with 0 pairs of kind (3) (the CheckLayout review, 2026-09-28).

KNOWN LIMITS, stated rather than chased: `#include <...>` is ignored (system headers; today four
angle-bracket includes name a tree header -- ScriptMgr*.cpp -> DBCStores.h three times, Config.h
-> Policies/Singleton.h -- all allowed edges); an include is read as text, so one inside a
comment or an #if 0 counts, as it did on the page; a macro include (#include MACRO) is not seen.
A sideways key names a directory, not a layer: in a directory that holds several peers (Object/,
WorldHandlers/, until they dissolve) a new file of one peer including a header that a file of
another peer there already includes is not new (45 of the 434 pairs have includers from two or
more peers today).

python src/tests/tools/layout_gate.py --check        # the gate (CheckLayout.cmake runs this)
python src/tests/tools/layout_gate.py --generate     # rewrite src/tests/layout_allow.txt
python src/tests/tools/layout_gate.py --self-test
"""
#
# SPDX-License-Identifier: GPL-3.0-or-later
#
# MaNGOS is a full featured server for World of Warcraft, supporting
# the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
#
# Copyright (C) 2005-2026 MaNGOS <https://www.getmangos.eu>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <https://www.gnu.org/licenses/>.
#
# World of Warcraft, and all World of Warcraft or Warcraft art, images,
# and lore are copyrighted by Blizzard Entertainment, Inc.
#

import argparse
import collections
import os
import re
import sys
import tempfile

# design/architecture.md's appendix, `layers.py` RULES, verbatim: paths relative to src/, the first
# matching row wins, None excludes. ObjectGuid.h/.cpp are foundation and AuctionHouseBot/ is app
# (the 2026-09-28 decisions). A change here is a change to the page's section 1: change both.
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
MAY['realmd'] = {'foundation', 'persistence'}                          # section 1: realmd uses only these two

# The two gated seams (sections 1 and 4), as the directories they are or will be. entities/player
# is today's directory. spells/aura does not exist yet: it is the aura half of the spell code
# (the campaign design's spells/aura/: SpellAuras, SpellAura*, UnitAura, UnitAuraProcHandler,
# SpellAuraDefines, and the aura storage spells/AuraContainer.h) wherever it lives today.
SEAMS = [
    ('entities/player', r'^game/entities/player/'),
    ('spells/aura', r'^game/spells/(aura/|Aura)|^game/WorldHandlers/(SpellAura|UnitAuraProcHandler)|^game/Object/UnitAura\.'),
]
# Which edges into a seam are gated. False: only an edge from ANOTHER domain directory
# (spells -> entities/player). True (the 2026-09-28 decision, the page's section 1): also an edge
# from the seam's own peer but outside the seam (Object/Unit.cpp -> entities/player/Player.h,
# 34 lines; SpellMgr.h -> SpellAuraDefines.h, 16 lines), so the seam holds from both sides.
SEAM_SAME_PEER = True

EXTENSIONS = ('.h', '.hpp', '.cpp', '.inl', '.inc')
INCLUDE_RX = re.compile(r'\s*#\s*include\s*"([^"]+)"')
ALLOW_REL = 'src/tests/layout_allow.txt'
ALLOW_HEADER = '''\
# CheckLayout's allow-list: src/tests/tools/layout_gate.py, design/architecture.md sections 1 and 4.
# The include edges in the tree that go against section 1's table or cross into a gated seam
# (entities/player, spells/aura), one line per includer file and header: "<includer> -> <header>";
# and the ones that cross between two domain directories, one line per includer directory and
# header: "<includer directory> -> <header>". Paths from the repository root.
# The gate fails on an edge that is not listed and on a line whose edge is gone or allowed:
# delete the line in the PR that removes the edge. A new edge is not added here; it is fixed.
# An against-the-rule or seam line may change its includer: replace the old line with the new one.
# After removing or moving edges, regenerate: python src/tests/tools/layout_gate.py --generate
'''
# The kinds listed per includer directory; the others are listed per includer file.
KEY_BY_DIRECTORY = ('sideways',)


class GateError(Exception):
    pass


def layer(p):
    return next((l for rx, l in RULES if re.search(rx, p)), 'UNMAPPED')


def tier(l):
    return 'domain' if l in DOMAIN else l


def seam(p):
    return next((name for name, rx in SEAMS if re.search(rx, p)), None)


def list_files(top):
    """Every C/C++ file under top, as sorted '/'-paths relative to top."""
    out = []
    for d, dirs, fs in os.walk(top):
        dirs.sort()
        for f in fs:
            if f.endswith(EXTENSIONS):
                out.append(os.path.relpath(os.path.join(d, f), top).replace(os.sep, '/'))
    return sorted(out)


def list_templates(top):
    out = []
    for d, dirs, fs in os.walk(top):
        for f in fs:
            if f.endswith('.in'):
                out.append(os.path.relpath(os.path.join(d, f[:-3]), top).replace(os.sep, '/'))
    return out


class Index:
    """Spelling lookup over a list of '/'-paths: basename, lower case -> paths."""
    def __init__(self, paths):
        self.by_name = collections.defaultdict(list)
        for p in paths:
            self.by_name[p.rsplit('/', 1)[-1].lower()].append(p)

    def candidates(self, spelling):
        s = spelling.lower()
        while s.startswith('./') or s.startswith('../'):
            s = s[2:] if s.startswith('./') else s[3:]
        return [p for p in self.by_name.get(s.rsplit('/', 1)[-1], []) if ('/' + p.lower()).endswith('/' + s)]


def normpath(p):
    parts = []
    for x in p.split('/'):
        if x in ('', '.'):
            continue
        if x == '..':
            if not parts:
                return None
            parts.pop()
        else:
            parts.append(x)
    return '/'.join(parts)


def resolve(includer, spelling, src_index, src_lower, dep_index, templates):
    """('tree', path) | ('dep', path) | ('generated', path); raises GateError when not found or ambiguous.
    src_lower maps every src/ file's lower-case path to its path."""
    rel = normpath(includer.rsplit('/', 1)[0] + '/' + spelling if '/' in includer else spelling)
    if rel is not None and rel.lower() in src_lower:
        return 'tree', src_lower[rel.lower()]
    c = src_index.candidates(spelling)
    if len(c) > 1:
        raise GateError('%s: #include "%s" is ambiguous, %d files under src/ end with it (%s); include it by '
                        'a longer path or relative to the includer'
                        % ('src/' + includer, spelling, len(c), ', '.join('src/' + x for x in sorted(c))))
    if c:
        return 'tree', c[0]
    d = dep_index.candidates(spelling)
    if d:
        return 'dep', d[0]
    g = Index(templates).candidates(spelling)
    if g:
        return 'generated', g[0]
    raise GateError('%s: #include "%s" names a header the tree does not have (not under src/, dep/ '
                    'or a .in template)' % ('src/' + includer, spelling))


def classify(a_path, b_path):
    """None (allowed) or (kind, key): kind in against/seam/sideways."""
    a, b = layer(a_path), layer(b_path)
    if a == 'tests':
        return None
    ta, tb = tier(a), tier(b)
    if ta == tb == 'domain':
        s = seam(b_path)
        if s and seam(a_path) != s and (a != b or SEAM_SAME_PEER):
            return 'seam', (a, s)
        if a != b:
            return 'sideways', (a, b)
    if ta == tb or tb in MAY.get(ta, set()):
        return None
    return 'against', (ta, tb)


def scan(root):
    """Classify every file and every quoted include under root/src. Returns a dict of results."""
    src = os.path.join(root, 'src')
    if not os.path.isdir(src):
        raise GateError('no src/ directory under %s: pass --root <the absolute repo root>' % root)
    files = list_files(src)
    if not files:
        raise GateError('found no C/C++ file under %s -- a gate that scans nothing passes nothing' % src)
    errors = []
    layers = collections.Counter()
    for f in files:
        l = layer(f)
        if l == 'UNMAPPED':
            errors.append('src/%s: no RULES row classifies this file; give its directory a layer '
                          '(design/architecture.md section 1)' % f)
        layers[l] += 1
    for name, rx in SEAMS:
        if not any(re.search(rx, f) for f in files):
            errors.append('the gated seam %s matches no file (moved?)' % name)
    dep = os.path.join(root, 'dep')
    dep_files = ['dep/' + x for x in list_files(dep)] if os.path.isdir(dep) else []
    src_lower = {x.lower(): x for x in files}
    src_index, dep_index, templates = Index(files), Index(dep_files), list_templates(src)
    lines = collections.Counter()     # (kind, key) -> include lines
    edges = {}                        # (includer, header) -> (kind, key), repo-root paths
    resolved = collections.Counter()
    for f in files:
        if layer(f) in (None, 'UNMAPPED'):
            continue
        with open(os.path.join(src, f), encoding='utf-8', errors='replace') as fh:
            for ln in fh:
                m = INCLUDE_RX.match(ln)
                if not m:
                    continue
                try:
                    how, t = resolve(f, m.group(1).replace('\\', '/'), src_index, src_lower, dep_index, templates)
                except GateError as e:
                    errors.append(str(e))
                    continue
                resolved[how] += 1
                if how != 'tree':
                    continue
                if layer(t) is None:
                    if layer(f) != 'tests':
                        errors.append('src/%s: #include "%s" reaches src/%s, which RULES excludes (tools/, genrev/, '
                                      'the PCH)' % (f, m.group(1), t))
                    else:
                        lines['skipped'] += 1
                    continue
                verdict = classify(f, t)
                if verdict is None:
                    lines['allowed'] += 1
                    continue
                lines[verdict] += 1
                edges[('src/' + f, 'src/' + t)] = verdict
    if sum(resolved.values()) == 0:
        errors.append('found no #include "..." line under %s -- a gate that scans nothing passes nothing' % src)
    return {'files': files, 'layers': layers, 'errors': errors, 'lines': lines, 'edges': edges, 'resolved': resolved}


def read_allow(path):
    """The list's entries as a set of (includer, header), and the malformed/duplicate lines as errors."""
    entries, errors, seen = set(), [], set()
    if not os.path.isfile(path):
        return entries, ['the allow-list %s does not exist; run --generate' % path]
    with open(path, encoding='utf-8') as fh:
        for n, ln in enumerate(fh, 1):
            s = ln.strip()
            if not s or s.startswith('#'):
                continue
            parts = s.split(' -> ')
            if len(parts) != 2 or not all(p.strip() for p in parts):
                errors.append('%s:%d: not "<includer> -> <header>": %s' % (path, n, s))
                continue
            e = (parts[0].strip(), parts[1].strip())
            if e in seen:
                errors.append('%s:%d: listed twice: %s' % (path, n, s))
            seen.add(e)
            entries.add(e)
    return entries, errors


def list_key(edge, kind):
    """The allow-list key of an edge (includer, header): the includer's own directory for a
    sideways edge, the includer file for the others."""
    return (edge[0].rsplit('/', 1)[0], edge[1]) if kind in KEY_BY_DIRECTORY else edge


def keys_of(edges):
    """{list key: (kind, [(includer, header), ...])} for a scan's edges."""
    keys = {}
    for e, (kind, _) in sorted(edges.items()):
        keys.setdefault(list_key(e, kind), (kind, []))[1].append(e)
    return keys


def format_allow(entries):
    return ALLOW_HEADER + ''.join('%s -> %s\n' % e for e in sorted(entries))


def summary(result, out):
    lines, edges = result['lines'], result['edges']
    per_kind = collections.defaultdict(set)      # (kind, layer key) -> edges
    pairs = collections.defaultdict(set)         # (kind, layer key) -> list keys
    for e, (kind, key) in edges.items():
        per_kind[(kind, key)].add(e)
        pairs[(kind, key)].add(list_key(e, kind))
    lay = result['layers']
    out('layout: %d files classified: %s' % (len(result['files']), ', '.join(
        '%s %d' % (l if l else 'excluded', n) for l, n in sorted(lay.items(), key=lambda x: (-x[1], str(x[0]))))))
    r = result['resolved']
    listed = sum(v for k, v in lines.items() if isinstance(k, tuple))
    out('layout: %d quoted includes resolved: %d in the tree (%d allowed, %d against/seam/sideways, '
        '%d skipped: tests -> tools/), %d third-party under dep/, %d generated'
        % (sum(r.values()), r['tree'], lines['allowed'], listed, lines['skipped'], r['dep'], r['generated']))
    titles = {'against': 'against the rule (by tier)', 'seam': 'against the rule: into a gated seam',
              'sideways': 'sideways inside the domain tier (by layer)'}
    for kind in ('against', 'seam', 'sideways'):
        keys = sorted((x[1] for x in lines if isinstance(x, tuple) and x[0] == kind),
                      key=lambda k: (-lines[(kind, k)], k))
        tl = sum(lines[(kind, k)] for k in keys)
        te = sum(len(per_kind[(kind, k)]) for k in keys)
        if kind in KEY_BY_DIRECTORY:
            tp = len(set().union(*(pairs[(kind, k)] for k in keys)))
            ts = sum(len(pairs[(kind, k)]) for k in keys)
            out('layout: %s: %d lines, %d edges, %d (includer directory, header) pairs listed; the pairs column '
                'below counts a pair under each includer peer of a mixed directory (sum %d)'
                % (titles[kind], tl, te, tp, ts))
        else:
            out('layout: %s: %d lines, %d (includer file, header) edges listed' % (titles[kind], tl, te))
        for k in keys:
            out('layout:   %-11s -> %-15s %5d lines %5d edges%s'
                % (k[0], k[1], lines[(kind, k)], len(per_kind[(kind, k)]),
                   ' %5d pairs' % len(pairs[(kind, k)]) if kind in KEY_BY_DIRECTORY else ''))


def line_kind(entry):
    """The kind a per-file allow-list line names today, from its paths alone (the file may be
    gone): against, seam, sideways or None (allowed, or not a per-file line)."""
    a, b = entry
    if not (a.startswith('src/') and b.startswith('src/') and a.endswith(EXTENSIONS)):
        return None
    if layer(a[4:]) in (None, 'UNMAPPED') or layer(b[4:]) in (None, 'UNMAPPED'):
        return None
    v = classify(a[4:], b[4:])
    return v[0] if v else None


def compare(edges, entries):
    """The tree's list keys against the list's lines. Returns (keys, new, moved, stale, counts):
    the list keys the list does not hold; (old line, new key) moves of an against or seam line to
    another includer file of the same header; the list lines the tree does not have; and, per
    (kind, header) of the per-file kinds, the includer files in the tree and the listed lines."""
    keys = keys_of(edges)
    new = sorted(k for k in keys if k not in entries)
    stale = sorted(e for e in entries if e not in keys)
    tree_n, list_n = collections.Counter(), collections.Counter()
    for k, (kind, _) in keys.items():
        if kind not in KEY_BY_DIRECTORY:
            tree_n[(kind, k[1])] += 1
    for e in entries:
        kind = line_kind(e)
        if kind and kind not in KEY_BY_DIRECTORY:
            list_n[(kind, e[1])] += 1
    # Rule 3: per (kind, header) the includer files may change but their number may not grow. A new
    # key pairs with a stale line of the same kind and header, both in sorted order; what is left
    # over is new (the count grew) or stale.
    new_by, stale_by = collections.defaultdict(list), collections.defaultdict(list)
    for k in new:
        if keys[k][0] not in KEY_BY_DIRECTORY:
            new_by[(keys[k][0], k[1])].append(k)
    for e in stale:
        kind = line_kind(e)
        if kind and kind not in KEY_BY_DIRECTORY:
            stale_by[(kind, e[1])].append(e)
    moved = []
    for hk in sorted(new_by):
        moved += list(zip(stale_by.get(hk, []), new_by[hk]))
    moved_new, moved_old = {m[1] for m in moved}, {m[0] for m in moved}
    new = [k for k in new if k not in moved_new]
    stale = [e for e in stale if e not in moved_old]
    return keys, new, moved, stale, (tree_n, list_n)


def check(root, allow_path, out=print):
    """The gate: 0 when the tree's violations are exactly the list's, 1 otherwise."""
    try:
        result = scan(root)
    except GateError as e:
        out('CheckLayout: FAIL: %s' % e)
        return 1
    entries, list_errors = read_allow(allow_path)
    errors = result['errors'] + list_errors
    edges = result['edges']
    keys, new, moved, stale, (tree_n, list_n) = compare(edges, entries)
    allow_rel = os.path.relpath(allow_path, root).replace(os.sep, '/')
    if not errors:
        summary(result, out)
    for e in errors:
        out('CheckLayout: ERROR: %s' % e)
    headings = {
        'against': 'new include edge(s) against section 1 of design/architecture.md (an upward or forbidden '
                   'include) that the allow-list does not hold, keyed per includer file and header -- fix them, '
                   'do not list them:',
        'seam': 'new include edge(s) into a gated seam (entities/player, spells/aura; section 1: "only two '
                'seams are gated") that the allow-list does not hold, keyed per includer file and header -- '
                'fix them, do not list them:',
        'sideways': 'new sideways include pair(s) (includer directory, header) between two domain directories '
                    'that the allow-list does not hold; section 1 allows these lines but the pairs cannot grow '
                    '(a new file in a listed directory including a listed header is not new) -- fix them, do '
                    'not list them:'}
    for kind in ('against', 'seam', 'sideways'):
        mine = [k for k in new if keys[k][0] == kind]
        if mine:
            out('CheckLayout: %d %s' % (len(mine), headings[kind]))
        for k in mine:
            key = edges[keys[k][1][0]][1]
            if kind in KEY_BY_DIRECTORY:
                out('  %s -> %s   [%s: %s -> %s] included by %s'
                    % (k[0], k[1], kind, key[0], key[1], ', '.join(e[0] for e in keys[k][1])))
            else:
                out('  %s -> %s   [%s: %s -> %s] the header has %d %s includer file(s) in the tree, %d listed'
                    % (k[0], k[1], kind, key[0], key[1], tree_n[(kind, k[1])], kind, list_n[(kind, k[1])]))
    if moved:
        out('CheckLayout: %d against-the-rule or seam line(s) changed includer file, the header\'s count did '
            'not grow -- moved: replace the old line with the new one in %s:' % (len(moved), allow_rel))
        for old, k in moved:
            kind, key = edges[k]
            out('  moved: replace the old line %s -> %s with %s -> %s   [%s: %s -> %s]'
                % (old[0], old[1], k[0], k[1], kind, key[0], key[1]))
    if stale:
        out('CheckLayout: %d allow-list line(s) name an edge that is gone from the tree or is allowed now -- '
            'delete them (%s):' % (len(stale), allow_rel))
        for e in stale:
            note = ''
            if line_kind(e) in KEY_BY_DIRECTORY:
                note = ('   [a sideways edge is listed per includer directory: %s -> %s]'
                        % (e[0].rsplit('/', 1)[0], e[1]))
            out('  %s -> %s%s' % (e[0], e[1], note))
    if errors or new or moved or stale:
        out('CheckLayout: FAIL')
        return 1
    per = collections.Counter(kind for kind, _ in keys.values())
    out('CheckLayout: OK: %d list lines (%d against + %d seam per includer file and header, %d sideways per '
        'includer directory and header), all %d on the allow-list'
        % (len(keys), per['against'], per['seam'], per['sideways'], len(entries)))
    return 0


def generate(root, allow_path, out=print):
    try:
        result = scan(root)
    except GateError as e:
        out('CheckLayout: FAIL: %s' % e)
        return 1
    if result['errors']:
        for e in result['errors']:
            out('CheckLayout: ERROR: %s' % e)
        out('CheckLayout: the list is not written while the tree has errors')
        return 1
    old = read_allow(allow_path)[0] if os.path.isfile(allow_path) else set()
    entries = set(keys_of(result['edges']))
    with open(allow_path, 'w', encoding='utf-8', newline='\n') as fh:
        fh.write(format_allow(entries))
    added, removed = sorted(entries - old), sorted(old - entries)
    for e in added:
        out('  + %s -> %s' % e)
    for e in removed:
        out('  - %s -> %s' % e)
    summary(result, out)
    out('CheckLayout: wrote %d lines to %s (+%d, -%d)' % (len(entries), allow_path, len(added), len(removed)))
    return 0


# self-test ---------------------------------------------------------------------------------------

SELF_TREE = {
    'src/shared/Common.h': '#include <vector>\n#include "Define.h"\n',
    'src/shared/Define.h': '#include <cstdint>\n',
    'src/shared/Common.cpp': '#include "Common.h"\n#include "BuildInfo.h"\n',
    'src/shared/BuildInfo.h.in': '',
    'src/proto/WorldPacket.h': '#include "Common.h"\n',
    'src/game/Object/Unit.h': '#include "Common.h"\n',
    'src/game/Object/Unit.cpp': '#include "Unit.h"\n#include "zlib.h"\n',
    'src/game/entities/player/Player.h': '#include "Unit.h"\n',
    'src/game/entities/player/Player.cpp': '#include "Player.h"\n#include "Object/Unit.h"\n',
    'src/game/spells/AuraContainer.h': '#include "Common.h"\n',
    'src/game/WorldHandlers/Spell.cpp': '#include "Unit.h"\n#include "WorldPacket.h"\n',
    'src/game/Maps/Map.cpp': '#include "Map.h"\n#include "Unit.h"\n#include "Player.h"\n#include "Map.h"\n',
    'src/game/Maps/Map.h': '#include "Common.h"\n',
    'src/game/pchdef.h': '#include "Unit.h"\n',
    'src/tests/UnitTest.cpp': '#include "Unit.h"\n#include "Player.h"\n#include "WorldPacket.h"\n#include "ToolDefs.h"\n',
    'src/tools/Extractor.cpp': '#include "NoSuchHeader.h"\n',
    'src/tools/ToolDefs.h': '',
    'dep/zlib/zlib.h': '',
}
# The tree above: Spell.cpp -> WorldPacket.h (domain -> proto) is against; Map.cpp -> Unit.h and
# Spell.cpp -> Unit.h are sideways (maps -> entities, spells -> entities); Map.cpp -> Player.h
# crosses into the entities/player seam; Player.cpp -> Player.h (inside the seam) and Player.h ->
# Unit.h (one peer, not into a seam) are allowed whatever SEAM_SAME_PEER says;
# tests include anything (UnitTest.cpp -> tools/ToolDefs.h is skipped and counted); tools/ is not
# scanned (Extractor.cpp's missing header is never read). The two sideways edges are listed by
# their includer's directory, the against and the seam edge by their includer file.
SELF_ALLOW = [('src/game/Maps', 'src/game/Object/Unit.h'),
              ('src/game/Maps/Map.cpp', 'src/game/entities/player/Player.h'),
              ('src/game/WorldHandlers', 'src/game/Object/Unit.h'),
              ('src/game/WorldHandlers/Spell.cpp', 'src/proto/WorldPacket.h')]


def self_test():
    global SEAM_SAME_PEER
    failures, rows = [], []

    def expect(cond, what):
        if not cond:
            failures.append(what)

    def build(tmp, tree, allow):
        for rel, text in tree.items():
            p = os.path.join(tmp, *rel.split('/'))
            os.makedirs(os.path.dirname(p), exist_ok=True)
            with open(p, 'w', encoding='utf-8') as fh:
                fh.write(text)
        ap = os.path.join(tmp, *ALLOW_REL.split('/'))
        os.makedirs(os.path.dirname(ap), exist_ok=True)
        with open(ap, 'w', encoding='utf-8') as fh:
            fh.write(allow if isinstance(allow, str) else format_allow(allow))
        return ap

    def run(label, tree, allow, want, needles=(), absent=()):
        needles = [needles] if isinstance(needles, str) else list(needles)
        absent = [absent] if isinstance(absent, str) else list(absent)
        with tempfile.TemporaryDirectory() as tmp:
            ap = build(tmp, tree, allow)
            got = []
            rc = check(tmp, ap, out=got.append)
            text = '\n'.join(got)
            missing = [n for n in needles if n not in text]
            present = [n for n in absent if n in text]
            ok = rc == want and not missing and not present
            rows.append(label)
            print('self-test: %-66s %s (exit %d)' % (label, 'PASS' if ok else 'FAIL', rc))
            expect(ok, '%s: exit %d, expected %d%s%s\n%s' % (label, rc, want, ''.join(
                ', missing "%s"' % n for n in missing), ''.join(', unexpected "%s"' % n for n in present), text))
            return text

    def with_file(rel, text):
        t = dict(SELF_TREE)
        t[rel] = text
        return t

    run('the clean fixture passes (tools/ is not scanned)', SELF_TREE, SELF_ALLOW, 0,
        ['CheckLayout: OK: 4 list lines (1 against + 1 seam per includer file and header, 2 sideways per includer '
         'directory and header), all 4 on the allow-list', 'domain      -> proto               1 lines     1 edges',
         'maps        -> entities/player     1 lines     1 edges',
         'maps        -> entities            1 lines     1 edges     1 pairs',
         'sideways inside the domain tier (by layer): 2 lines, 2 edges, 2 (includer directory, header) pairs',
         '(15 allowed, 4 against/seam/sideways, 1 skipped: tests -> tools/), 1 third-party under dep/, 1 generated'])
    run('a planted upward include fails (foundation -> entities)',
        with_file('src/shared/Common.cpp', '#include "Common.h"\n#include "Object/Unit.h"\n'), SELF_ALLOW, 1,
        ['1 new include edge(s) against section 1', 'an upward or forbidden include',
         'src/shared/Common.cpp -> src/game/Object/Unit.h   [against: foundation -> domain]'])
    run('a new sideways include fails (spells -> maps)',
        with_file('src/game/WorldHandlers/Spell.cpp', '#include "Unit.h"\n#include "WorldPacket.h"\n#include "Map.h"\n'),
        SELF_ALLOW, 1, ['1 new sideways include pair(s) (includer directory, header)',
                        'section 1 allows these lines but the pairs cannot grow',
                        '  src/game/WorldHandlers -> src/game/Maps/Map.h   [sideways: spells -> maps] included by '
                        'src/game/WorldHandlers/Spell.cpp'])
    run('a new include into a gated seam fails as against',
        with_file('src/game/WorldHandlers/Spell.cpp', '#include "WorldPacket.h"\n#include "Player.h"\n'),
        SELF_ALLOW, 1, ['1 new include edge(s) into a gated seam', '[seam: spells -> entities/player]'])
    run('a removed edge with a stale list entry fails',
        with_file('src/game/WorldHandlers/Spell.cpp', '#include "Unit.h"\n'), SELF_ALLOW, 1,
        'allow-list line(s) name an edge that is gone')
    run('an allowed edge added is fine (session, same peer)',
        dict(SELF_TREE, **{'src/game/Server/WorldSession.cpp': '#include "Player.h"\n#include "WorldPacket.h"\n',
                           'src/game/Object/Creature.cpp': '#include "Unit.h"\n#include "Object/Unit.h"\n'}),
        SELF_ALLOW, 0, 'CheckLayout: OK')
    run('an unclassified file fails', with_file('src/weird/Thing.cpp', '#include "Common.h"\n'), SELF_ALLOW, 1,
        'src/weird/Thing.cpp: no RULES row classifies this file')
    run('a system include is ignored (<Object/Unit.h> too)',
        with_file('src/shared/Define.h', '#include <cstdint>\n#include <Object/Unit.h>\n#include <map>\n'),
        SELF_ALLOW, 0, 'CheckLayout: OK')
    run('a quoted header the tree does not have fails',
        with_file('src/game/Maps/Map.h', '#include "Common.h"\n#include "Gone.h"\n'), SELF_ALLOW, 1,
        '#include "Gone.h" names a header the tree does not have')
    run('an ambiguous spelling fails', dict(SELF_TREE, **{'src/game/Maps/Grid.h': '', 'src/game/Object/Grid.h': '',
                                                          'src/shared/Use.cpp': '#include "Grid.h"\n'}),
        SELF_ALLOW, 1, 'is ambiguous')
    # Relative first decides between two World.h: the includer's own directory holds the app's
    # (domain -> app, against), shared/ holds an allowed one. Picking shared/'s would pass.
    run('relative first picks the includer\'s own World.h',
        dict(SELF_TREE, **{'src/game/WorldHandlers/World.h': '', 'src/shared/World.h': '',
                           'src/game/WorldHandlers/Spell.cpp':
                               '#include "Unit.h"\n#include "WorldPacket.h"\n#include "World.h"\n'}),
        SELF_ALLOW, 1, 'src/game/WorldHandlers/Spell.cpp -> src/game/WorldHandlers/World.h   [against: domain -> app]')
    # No relative hit and two candidates, one under the includer's own top directory (game/): no
    # tie-break, the include fails and names both (the review's proto/ + game/movement/ twin).
    # The suffix match stops at a directory boundary: "Object/X.h" is not game/GameObject/X.h.
    run('a spelling matches at a directory boundary only',
        dict(SELF_TREE, **{'src/game/GameObject/X.h': '',
                           'src/game/Maps/Map.h': '#include "Common.h"\n#include "Object/X.h"\n'}),
        SELF_ALLOW, 1, 'src/game/Maps/Map.h: #include "Object/X.h" names a header the tree does not have')
    # Candidates are matched ignoring case, so Foo.h and foo.h in two directories are two candidates.
    run('candidates differing only in case are ambiguous',
        dict(SELF_TREE, **{'src/game/Maps/Foo.h': '', 'src/game/Object/foo.h': '',
                           'src/shared/Use.cpp': '#include "Foo.h"\n'}),
        SELF_ALLOW, 1, 'src/shared/Use.cpp: #include "Foo.h" is ambiguous, 2 files under src/ end with it '
                       '(src/game/Maps/Foo.h, src/game/Object/foo.h)')
    run('two candidates, one in the includer\'s top dir, fail',
        dict(SELF_TREE, **{'src/proto/NewCodec.h': '', 'src/game/movement/NewCodec.h': '',
                           'src/game/Maps/Map.h': '#include "Common.h"\n#include "NewCodec.h"\n'}),
        SELF_ALLOW, 1, 'src/game/Maps/Map.h: #include "NewCodec.h" is ambiguous, 2 files under src/ end with it '
                       '(src/game/movement/NewCodec.h, src/proto/NewCodec.h)')
    run('a malformed list line fails', SELF_TREE, format_allow(SELF_ALLOW) + 'src/x.cpp src/y.h\n', 1,
        'not "<includer> -> <header>"')
    run('a line listed twice fails', SELF_TREE, format_allow(SELF_ALLOW) + '%s -> %s\n' % SELF_ALLOW[0], 1,
        'listed twice')
    run('a list entry for an edge that is allowed now fails',
        SELF_TREE, SELF_ALLOW + [('src/game/entities/player/Player.cpp', 'src/game/entities/player/Player.h')], 1,
        'src/game/entities/player/Player.cpp -> src/game/entities/player/Player.h')
    # M-1's switch: an include into a seam from its own peer (Object/Bag.cpp -> Player.h) is
    # allowed while SEAM_SAME_PEER is off and fails as a seam edge when it is on.
    run('a same-peer include into a seam follows SEAM_SAME_PEER',
        with_file('src/game/Object/Bag.cpp', '#include "Player.h"\n'), SELF_ALLOW, 1 if SEAM_SAME_PEER else 0,
        '  src/game/Object/Bag.cpp -> src/game/entities/player/Player.h   [seam: entities -> entities/player]'
        if SEAM_SAME_PEER else 'CheckLayout: OK: 4 list lines')
    run('duplicate violating lines collapse to one edge',
        with_file('src/game/Maps/Map.cpp', '#include "Map.h"\n#include "Unit.h"\n#include "Player.h"\n#include "Unit.h"\n'),
        SELF_ALLOW, 0, ['CheckLayout: OK: 4 list lines', 'all 4 on the allow-list',
                        'maps        -> entities            2 lines     1 edges     1 pairs'])

    # Rule 1 (the 2026-09-29 re-key): a sideways edge is keyed by its includer's own directory.
    run('rule 1: a new file in a listed directory, listed header, passes',
        with_file('src/game/Maps/MapGrid.cpp', '#include "Unit.h"\n'), SELF_ALLOW, 0,
        ['CheckLayout: OK: 4 list lines', 'maps        -> entities            2 lines     2 edges     1 pairs'])
    # The case that asked for the re-key: a split beside SpellAuraDummy.cpp (inside the spells/aura
    # seam; an edge OUT of a seam is sideways) including headers WorldHandlers/ already includes.
    run('rule 1: a split beside SpellAuraDummy.cpp (out of the seam) passes',
        with_file('src/game/WorldHandlers/SpellAuraDummyWarrior.cpp', '#include "Unit.h"\n'), SELF_ALLOW, 0,
        ['CheckLayout: OK: 4 list lines', 'spells      -> entities            2 lines     2 edges     1 pairs'])
    # spells/handlers is a directory key of its own, not part of spells/: its first file is new.
    run('rule 1: a new (directory, header) pair fails (spells/handlers)',
        with_file('src/game/spells/handlers/AuraDummyWarrior.cpp', '#include "Unit.h"\n'), SELF_ALLOW, 1,
        ['1 new sideways include pair(s)', '  src/game/spells/handlers -> src/game/Object/Unit.h   [sideways: spells '
         '-> entities] included by src/game/spells/handlers/AuraDummyWarrior.cpp'])
    run('rule 1: a listed pair with no includer left is stale and fails',
        with_file('src/game/Maps/Map.cpp', '#include "Map.h"\n#include "Player.h"\n'), SELF_ALLOW, 1,
        ['1 allow-list line(s) name an edge that is gone', '  src/game/Maps -> src/game/Object/Unit.h'])
    run('rule 1: a pair stays while one includer in the directory is left',
        dict(SELF_TREE, **{'src/game/Maps/Map.cpp': '#include "Map.h"\n#include "Player.h"\n',
                           'src/game/Maps/MapGrid.cpp': '#include "Unit.h"\n'}), SELF_ALLOW, 0,
        'CheckLayout: OK: 4 list lines')
    run('rule 1: a sideways edge listed per file (the old key) fails',
        SELF_TREE,
        [e for e in SELF_ALLOW if e[0] != 'src/game/Maps'] + [('src/game/Maps/Map.cpp', 'src/game/Object/Unit.h')],
        1, ['  src/game/Maps -> src/game/Object/Unit.h   [sideways: maps -> entities] included by',
            '  src/game/Maps/Map.cpp -> src/game/Object/Unit.h   [a sideways edge is listed per includer directory: '
            'src/game/Maps -> src/game/Object/Unit.h]'])

    # Rule 2: against-the-rule and seam edges stay keyed per includer file, so a second file of a
    # directory that already has the line is new.
    run('rule 2: a second file including a forbidden header fails',
        with_file('src/game/WorldHandlers/SpellEffects.cpp', '#include "WorldPacket.h"\n'), SELF_ALLOW, 1,
        ['1 new include edge(s) against section 1',
         '  src/game/WorldHandlers/SpellEffects.cpp -> src/proto/WorldPacket.h'
         '   [against: domain -> proto] the header has 2 against includer file(s) in the tree, 1 listed'], 'moved:')
    run('rule 2: a second file including a seam header fails',
        with_file('src/game/Maps/MapGrid.cpp', '#include "Player.h"\n'), SELF_ALLOW, 1,
        ['1 new include edge(s) into a gated seam', '  src/game/Maps/MapGrid.cpp -> src/game/entities/player/Player.h'
         '   [seam: maps -> entities/player] the header has 2 seam includer file(s) in the tree, 1 listed'], 'moved:')

    # Rule 3: an against or seam line may change its includer file; per (kind, header) the count of
    # includer files may not grow. WorldPacket.h moves from Spell.cpp to Map.cpp (against).
    old_wp = ('src/game/WorldHandlers/Spell.cpp', 'src/proto/WorldPacket.h')
    new_wp = ('src/game/Maps/Map.cpp', 'src/proto/WorldPacket.h')
    moved_tree = dict(SELF_TREE, **{
        'src/game/WorldHandlers/Spell.cpp': '#include "Unit.h"\n',
        'src/game/Maps/Map.cpp':
            '#include "Map.h"\n#include "Unit.h"\n#include "Player.h"\n#include "WorldPacket.h"\n'})
    moved_allow = [e for e in SELF_ALLOW if e != old_wp] + [new_wp]
    moved_text = ('  moved: replace the old line src/game/WorldHandlers/Spell.cpp -> src/proto/WorldPacket.h with '
                  'src/game/Maps/Map.cpp -> src/proto/WorldPacket.h   [against: domain -> proto]')
    run('rule 3: a moved against line, replaced in the list, passes', moved_tree, moved_allow, 0,
        'CheckLayout: OK: 4 list lines')
    run('rule 3: a moved against line not replaced fails as moved', moved_tree, SELF_ALLOW, 1,
        ['1 against-the-rule or seam line(s) changed includer file', moved_text],
        ['new include edge(s) against', 'allow-list line(s) name an edge that is gone'])
    run('rule 3: the old line kept beside the new one fails', moved_tree, SELF_ALLOW + [new_wp], 1,
        ['1 allow-list line(s) name an edge that is gone',
         '  src/game/WorldHandlers/Spell.cpp -> src/proto/WorldPacket.h'],
        'moved:')
    run('rule 3: the old include kept, a second includer: count grew, fails',
        with_file('src/game/Maps/Map.cpp', moved_tree['src/game/Maps/Map.cpp']), SELF_ALLOW, 1,
        ['  src/game/Maps/Map.cpp -> src/proto/WorldPacket.h   [against: domain -> proto] the header has 2 against '
         'includer file(s) in the tree, 1 listed'], 'moved:')
    two_new = dict(moved_tree, **{
        'src/game/Object/Unit.cpp': '#include "Unit.h"\n#include "zlib.h"\n#include "WorldPacket.h"\n'})
    run('rule 3: two includers for one deleted line fail (list untouched)', two_new, SELF_ALLOW, 1,
        [moved_text, '  src/game/Object/Unit.cpp -> src/proto/WorldPacket.h   [against: domain -> proto] the header '
         'has 2 against includer file(s) in the tree, 1 listed'])
    run('rule 3: two includers for one deleted line fail (line replaced)', two_new, moved_allow, 1,
        '  src/game/Object/Unit.cpp -> src/proto/WorldPacket.h   [against: domain -> proto] the header has 2 against '
        'includer file(s) in the tree, 1 listed', 'moved:')
    # The same for a seam line: Player.h's seam include moves from Maps/Map.cpp to Spell.cpp.
    old_pl = ('src/game/Maps/Map.cpp', 'src/game/entities/player/Player.h')
    new_pl = ('src/game/WorldHandlers/Spell.cpp', 'src/game/entities/player/Player.h')
    seam_tree = dict(SELF_TREE, **{
        'src/game/Maps/Map.cpp': '#include "Map.h"\n#include "Unit.h"\n',
        'src/game/WorldHandlers/Spell.cpp': '#include "Unit.h"\n#include "WorldPacket.h"\n#include "Player.h"\n'})
    run('rule 3: a moved seam line, replaced in the list, passes', seam_tree,
        [e for e in SELF_ALLOW if e != old_pl] + [new_pl], 0, 'CheckLayout: OK: 4 list lines')
    run('rule 3: a moved seam line not replaced fails as moved', seam_tree, SELF_ALLOW, 1,
        '  moved: replace the old line src/game/Maps/Map.cpp -> src/game/entities/player/Player.h with '
        'src/game/WorldHandlers/Spell.cpp -> src/game/entities/player/Player.h   [seam: spells -> entities/player]',
        'new include edge(s) into a gated seam')
    # A move keeps its kind: the seam line of Player.h does not pay for an against include of it.
    run('rule 3: a move does not cross kinds (seam line, against includer)',
        dict(SELF_TREE, **{'src/game/Maps/Map.cpp': '#include "Map.h"\n#include "Unit.h"\n',
                           'src/game/Server/DBCStores.cpp': '#include "Player.h"\n'}), SELF_ALLOW, 1,
        ['  src/game/Server/DBCStores.cpp -> src/game/entities/player/Player.h   [against: data -> domain]',
         '  src/game/Maps/Map.cpp -> src/game/entities/player/Player.h'], 'moved:')
    # A move keeps its header: a stale WorldPacket.h line does not pay for a new Opcodes.h include.
    run('rule 3: a move does not cross headers (stale H1 line, new H2 edge)',
        dict(SELF_TREE, **{'src/proto/Opcodes.h': '#include "Common.h"\n',
                           'src/game/WorldHandlers/Spell.cpp': '#include "Unit.h"\n',
                           'src/game/Maps/Map.cpp':
                               '#include "Map.h"\n#include "Unit.h"\n#include "Player.h"\n#include "Opcodes.h"\n'}),
        SELF_ALLOW, 1,
        ['1 new include edge(s) against', '  src/game/Maps/Map.cpp -> src/proto/Opcodes.h   [against: domain -> '
         'proto] the header has 1 against includer file(s) in the tree, 0 listed',
         '1 allow-list line(s) name an edge that is gone',
         '  src/game/WorldHandlers/Spell.cpp -> src/proto/WorldPacket.h'],
        'moved:')
    run('a zero scan fails', {'src/README': 'x', 'dep/zlib/zlib.h': ''}, [], 1, 'found no C/C++ file')
    with tempfile.TemporaryDirectory() as tmp:
        ap = build(tmp, SELF_TREE, [])
        got = []
        rc = generate(tmp, ap, out=got.append)
        rc2 = check(tmp, ap, out=got.append)
        ok = rc == 0 and rc2 == 0 and read_allow(ap)[0] == set(SELF_ALLOW)
        rows.append('generate')
        print('self-test: %-66s %s' % ('--generate writes what --check then passes', 'PASS' if ok else 'FAIL'))
        expect(ok, 'generate/check round trip: %d %d\n%s' % (rc, rc2, '\n'.join(got)))
    # classify, the table itself: one row per rule of section 1.
    for a, b, want in [('game/Object/Unit.cpp', 'proto/WorldPacket.h', ('against', ('domain', 'proto'))),
                       ('game/Server/ObjectMgr.cpp', 'motion/State.h', ('against', ('data', 'motion'))),
                       ('motion/Kernel.cpp', 'shared/Database/Database.h', ('against', ('motion', 'persistence'))),
                       ('realmd/Main.cpp', 'proto/WorldPacket.h', ('against', ('realmd', 'proto'))),
                       ('realmd/Main.cpp', 'shared/Database/Database.h', None),
                       ('game/Object/Unit.cpp', 'game/WorldHandlers/SpellAuras.h', ('seam', ('entities', 'spells/aura'))),
                       ('game/WorldHandlers/SpellAuras.cpp', 'game/Object/SpellMgr.h', None),
                       ('game/Maps/Map.cpp', 'game/Object/Creature.h', ('sideways', ('maps', 'entities'))),
                       ('game/Object/ObjectGuid.cpp', 'game/Object/ObjectMgr.h', ('against', ('foundation', 'data'))),
                       ('game/AuctionHouseBot/AuctionHouseBot.cpp', 'game/Object/Unit.h', None),
                       ('tests/X.cpp', 'game/Harness/Harness.h', None),
                       ('game/Server/WorldSession.cpp', 'game/WorldHandlers/World.h', ('against', ('session', 'app'))),
                       ('game/Object/Bag.cpp', 'game/entities/player/Player.h',
                        ('seam', ('entities', 'entities/player')) if SEAM_SAME_PEER else None)]:
        got = classify(a, b)
        expect(got == want, 'classify(%s, %s) = %r, expected %r' % (a, b, got, want))
    # SEAM_SAME_PEER, the switch for the same-peer reading: both positions give what they promise.
    saved = SEAM_SAME_PEER
    for flag in (False, True):
        SEAM_SAME_PEER = flag
        for a, b, want in [('game/Object/Bag.cpp', 'game/entities/player/Player.h',
                            ('seam', ('entities', 'entities/player')) if flag else None),
                           ('game/Object/SpellMgr.h', 'game/WorldHandlers/SpellAuraDefines.h',
                            ('seam', ('spells', 'spells/aura')) if flag else None),
                           ('game/entities/player/Player.cpp', 'game/entities/player/Player.h', None),
                           ('game/WorldHandlers/SpellAuras.cpp', 'game/WorldHandlers/SpellAuras.h', None),
                           ('game/Maps/Map.cpp', 'game/entities/player/Player.h', ('seam', ('maps', 'entities/player')))]:
            got = classify(a, b)
            expect(got == want, 'classify, SEAM_SAME_PEER %s: (%s, %s) = %r, expected %r' % (flag, a, b, got, want))
    SEAM_SAME_PEER = saved
    rows.append('classify')
    print('self-test: %-66s %s' % ('classify: section 1 table, SEAM_SAME_PEER both ways', 'PASS' if not [
        f for f in failures if f.startswith('classify')] else 'FAIL'))

    for f in failures:
        print('SELF-TEST FAILED: ' + f)
    print('self-test: %s (%d rows, %d failure(s))' % ('PASS' if not failures else 'FAIL', len(rows), len(failures)))
    return 1 if failures else 0


def main(argv):
    ap = argparse.ArgumentParser(description='CheckLayout: the include-direction ratchet (design/architecture.md).')
    ap.add_argument('--root', default=os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..')))
    ap.add_argument('--allow', help='the allow-list (default: <root>/%s)' % ALLOW_REL)
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument('--check', action='store_true')
    g.add_argument('--generate', action='store_true')
    g.add_argument('--self-test', action='store_true')
    args = ap.parse_args(argv[1:])
    if args.self_test:
        return self_test()
    root = os.path.abspath(args.root)
    allow = args.allow or os.path.join(root, *ALLOW_REL.split('/'))
    return check(root, allow) if args.check else generate(root, allow)


if __name__ == '__main__':
    sys.exit(main(sys.argv))
