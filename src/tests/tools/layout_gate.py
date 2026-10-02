#!/usr/bin/env python3
"""layout_gate.py [--root <repo root>] [--allow <list>] --check | --generate | --self-test

CheckLayout: the include direction of design/architecture.md, sections 1 and 4.

Files: every .h, .hpp, .cpp, .inl and .inc under src/ takes its layer from RULES, section 1's
directory table; the first matching row wins. A file no row matches fails. tools/, genrev/ and
game/pchdef.* are excluded: neither checked nor a target. realmd may include only foundation and
persistence; tests may include anything.

Edges: each `#include "..."` line is an edge from the includer to the header, one of:

  allowed   the includer's layer may include the header's (MAY, section 1's "may include"), or
            both are in one layer.
  against   an upward or forbidden edge.
  seam      an edge into a gated seam (entities/player, spells/aura) from outside it: from another
            domain peer, or from the seam's own peer (SEAM_SAME_PEER).
  sideways  any other edge between two domain peers; an edge out of a seam is sideways.

Headers: a spelling resolves relative to the includer's directory first; otherwise it must be a
path suffix, at a directory boundary and ignoring letter case, of exactly one file under src/.
Two or more candidates fail as ambiguous and are all named. A spelling found only under dep/
(third party) or as a "<spelling>.in" template under src/ (generated) is skipped; a spelling found
nowhere fails. The targets' include paths are not read, so a spelling the compiler resolves
elsewhere (a dep/, generated or system header of the same name; a directory off the target's
include path; a relative match that differs only in case) is taken as the src/ file named above.

Not counted: `#include <...>` and an include produced by a macro. Counted: an include inside a
comment or an #if 0 block.

The allow-list, src/tests/layout_allow.txt, holds one line per key, paths from the repository
root, in one sort order:

  against, seam   "<includer file> -> <header>", however many times the file includes it.
  sideways        "<includer directory> [<includer peer>] -> <header>", the includer's immediate
                  directory (src/game/spells/handlers is its own key, not part of src/game/spells)
                  and its peer layer. A new file of a listed peer in a listed directory including a
                  listed header adds no key. A file's peer comes from its name (RULES), so a file
                  named after another peer is a new key.
The file starts with a header block of "#" lines and one blank line; --generate replaces the block
with the tool's header and names each line it replaced. A block with no blank line after it fails,
and --generate writes nothing while such a block holds a line that is not the tool's. After the
header, a "#" line with text, directly above an entry, is that entry's reason: one line, kept by
--generate with its entry and dropped with it. A moved line's reason goes to the new line only
when the move is one to one: the only stale and the only new line of its layer pair and header;
with more lines moved under one layer pair and header, --generate drops their reasons and names
each one to be given back by hand. A "#" line after the header that is not directly above an
entry, or holds no text, fails, and --generate writes nothing while the list holds such a line.
A reason is optional here; that a line needs one is decided by the review of the list's diff.

--check fails on:
  - a key the tree has and the list does not;
  - an against or seam edge whose includer file changed: per (kind, layer pair, header) the
    includer files in the tree may not outnumber the listed lines, so a new edge that matches a
    listed line of its layer pair gone from the tree is reported as "moved: replace the old line ...
    with ..."; beyond that count, or into another layer pair, it is new;
  - a listed line whose key is gone from the tree or is allowed;
  - a malformed or duplicate line, an unclassified file, an unknown or ambiguous header, a seam
    that matches no file, and a scan that reads no file or no include.
It passes with a per-layer-pair summary in numbers. --generate rewrites the list from the tree and
prints what it added and removed. --self-test runs the fixtures.
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

# Section 1's directory table, paths relative to src/: the first matching row wins, None excludes.
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
MAY = {l: set(ORDER[i + 1:]) for i, l in enumerate(ORDER)}
MAY['domain'] -= {'proto'}; MAY['data'] -= {'motion', 'proto'}
MAY['motion'] -= {'persistence'}; MAY['proto'] -= {'persistence'}
MAY['realmd'] = {'foundation', 'persistence'}

SEAMS = [
    ('entities/player', r'^game/entities/player/'),
    ('spells/aura', r'^game/spells/(aura/|Aura)|^game/WorldHandlers/(SpellAura|UnitAuraProcHandler)|^game/Object/UnitAura\.'),
]
# True: an include into a seam from the seam's own peer, outside the seam, is a seam edge too.
SEAM_SAME_PEER = True

EXTENSIONS = ('.h', '.hpp', '.cpp', '.inl', '.inc')
INCLUDE_RX = re.compile(r'\s*#\s*include\s*"([^"]+)"')
ALLOW_REL = 'src/tests/layout_allow.txt'
ALLOW_HEADER = '''\
# CheckLayout's allow-list: src/tests/tools/layout_gate.py, design/architecture.md sections 1 and 4.
# Against-the-rule and seam edges: "<includer file> -> <header>".
# Sideways edges: "<includer directory> [<peer>] -> <header>". Paths from the repository root.
# The gate fails on an edge that is not listed and on a line whose edge is gone or allowed.
# A new edge is fixed, not listed. A line whose includer file changed is replaced by the new one.
# After removing or moving edges: python src/tests/tools/layout_gate.py --generate
# A "#" line directly above an entry is its reason; --generate keeps it with the entry.
'''
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
    """('tree' | 'dep' | 'generated', path); GateError when not found or ambiguous."""
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
    """Every file's layer and every quoted include's edge under root/src."""
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
    lines = collections.Counter()
    edges = {}
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
    """The list's entries; its malformed or duplicate lines, a header block with no blank line after
    it and misplaced or empty reason lines as errors; {entry: reason line} for the entries with a
    reason; the "#" lines --generate would not keep (neither a header line it replaces nor a
    reason); and the header block's lines that are not the tool's (--generate replaces them)."""
    entries, errors, seen, reasons, lost, replaced = set(), [], set(), {}, [], []
    if not os.path.isfile(path):
        return entries, ['the allow-list %s does not exist; run --generate' % path], reasons, lost, replaced
    with open(path, encoding='utf-8') as fh:
        lines = [ln.strip() for ln in fh]
    own = ALLOW_HEADER.rstrip('\n').split('\n')
    header = 0
    while header < len(lines) and lines[header].startswith('#'):
        header += 1
    foreign = [(n, l) for n, l in enumerate(lines[:header], 1) if l not in own]
    if header == 0 or header == len(lines) or lines[header]:
        errors.append('%s: the list does not start with a header block and one blank line (--generate writes them)'
                      % path)
        for n, l in foreign:
            errors.append('%s:%d: a "#" line in a header block with no blank line after it: %s -- add the blank '
                          'line after the header, then the line becomes a reason or an error' % (path, n, l))
            lost.append(l)
    else:
        replaced = [l for _, l in foreign]
    reason = None
    for n, s in enumerate(lines[header:], header + 1):
        if reason is not None and not (s and not s.startswith('#')):
            errors.append('%s:%d: a reason line not directly above an entry: %s' % (path, n - 1, reason))
            lost.append(reason)
            reason = None
        if s.startswith('#'):
            reason = s
            if not s[1:].strip():
                errors.append('%s:%d: a reason line with no text: %s' % (path, n, s))
                lost.append(s)
                reason = None
            continue
        if not s:
            continue
        parts = s.split(' -> ')
        if len(parts) != 2 or not all(p.strip() for p in parts):
            errors.append('%s:%d: not "<includer> -> <header>": %s' % (path, n, s))
            reason = None
            continue
        e = (parts[0].strip(), parts[1].strip())
        if e in seen:
            errors.append('%s:%d: listed twice: %s' % (path, n, s))
        seen.add(e)
        entries.add(e)
        if reason is not None:
            reasons[e] = reason
        reason = None
    if reason is not None:
        errors.append('%s:%d: a reason line not directly above an entry: %s' % (path, len(lines), reason))
        lost.append(reason)
    return entries, errors, reasons, lost, replaced


def list_key(edge, verdict):
    """The list key of an edge: its includer's directory and peer if sideways, else its includer file."""
    kind, key = verdict
    return ('%s [%s]' % (edge[0].rsplit('/', 1)[0], key[0]), edge[1]) if kind in KEY_BY_DIRECTORY else edge


def keys_of(edges):
    """{list key: (kind, [(includer, header), ...])} for a scan's edges."""
    keys = {}
    for e, v in sorted(edges.items()):
        keys.setdefault(list_key(e, v), (v[0], []))[1].append(e)
    return keys


def format_allow(entries, reasons=None):
    reasons = reasons or {}
    return ALLOW_HEADER + '\n' + ''.join(
        ('%s\n' % reasons[e] if e in reasons else '') + '%s -> %s\n' % e for e in sorted(entries))


def summary(result, out):
    lines, edges = result['lines'], result['edges']
    per_kind = collections.defaultdict(set)
    pairs = collections.defaultdict(set)
    for e, (kind, key) in edges.items():
        per_kind[(kind, key)].add(e)
        pairs[(kind, key)].add(list_key(e, (kind, key)))
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
            out('layout: %s: %d lines, %d edges, %d (includer directory, includer peer, header) keys listed'
                % (titles[kind], tl, te, tp))
        else:
            out('layout: %s: %d lines, %d (includer file, header) edges listed' % (titles[kind], tl, te))
        for k in keys:
            out('layout:   %-11s -> %-15s %5d lines %5d edges%s'
                % (k[0], k[1], lines[(kind, k)], len(per_kind[(kind, k)]),
                   ' %5d keys' % len(pairs[(kind, k)]) if kind in KEY_BY_DIRECTORY else ''))


def line_verdict(entry):
    """The (kind, layer pair) a per-file list line names; None if allowed or not per file."""
    a, b = entry
    if not (a.startswith('src/') and b.startswith('src/') and a.endswith(EXTENSIONS)):
        return None
    if layer(a[4:]) in (None, 'UNMAPPED') or layer(b[4:]) in (None, 'UNMAPPED'):
        return None
    return classify(a[4:], b[4:])


def line_kind(entry):
    v = line_verdict(entry)
    return v[0] if v else None


def compare(edges, entries):
    """The tree's keys against the list: (keys, new, moved, stale, (tree counts, list counts), sole),
    sole the old lines of the moves that are the only stale and the only new line of their layer
    pair and header."""
    keys = keys_of(edges)
    new = sorted(k for k in keys if k not in entries)
    stale = sorted(e for e in entries if e not in keys)
    tree_n, list_n = collections.Counter(), collections.Counter()
    for k, (kind, _) in keys.items():
        if kind not in KEY_BY_DIRECTORY:
            tree_n[(edges[k], k[1])] += 1
    for e in entries:
        v = line_verdict(e)
        if v and v[0] not in KEY_BY_DIRECTORY:
            list_n[(v, e[1])] += 1
    # A new key pairs with a stale line of its kind, layer pair and header; the rest is new or stale.
    new_by, stale_by = collections.defaultdict(list), collections.defaultdict(list)
    for k in new:
        if keys[k][0] not in KEY_BY_DIRECTORY:
            new_by[(edges[k], k[1])].append(k)
    for e in stale:
        v = line_verdict(e)
        if v and v[0] not in KEY_BY_DIRECTORY:
            stale_by[(v, e[1])].append(e)
    moved, sole = [], set()
    for hk in sorted(new_by):
        moved += list(zip(stale_by.get(hk, []), new_by[hk]))
        if len(new_by[hk]) == 1 and len(stale_by.get(hk, [])) == 1:
            sole.add(stale_by[hk][0])
    moved_new, moved_old = {m[1] for m in moved}, {m[0] for m in moved}
    new = [k for k in new if k not in moved_new]
    stale = [e for e in stale if e not in moved_old]
    return keys, new, moved, stale, (tree_n, list_n), sole


def check(root, allow_path, out=print):
    """The gate: 0 when the tree's violations are exactly the list's, 1 otherwise."""
    try:
        result = scan(root)
    except GateError as e:
        out('CheckLayout: FAIL: %s' % e)
        return 1
    entries, list_errors, reasons, _, _ = read_allow(allow_path)
    errors = result['errors'] + list_errors
    edges = result['edges']
    keys, new, moved, stale, (tree_n, list_n), sole = compare(edges, entries)
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
        'sideways': 'new sideways include key(s) (includer directory, includer peer, header) between two domain '
                    'directories that the allow-list does not hold; section 1 allows these lines but the keys '
                    'cannot grow (a new file of a listed peer in a listed directory including a listed header is '
                    'not new) -- fix them, do not list them:'}
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
                    % (k[0], k[1], kind, key[0], key[1], tree_n[(edges[k], k[1])], '%s -> %s' % key,
                       list_n[(edges[k], k[1])]))
    if moved:
        out('CheckLayout: %d against-the-rule or seam line(s) changed includer file inside one layer pair, the '
            'header\'s count did not grow -- moved: replace the old line with the new one in %s:'
            % (len(moved), allow_rel))
        for old, k in moved:
            kind, key = edges[k]
            note = ''
            if old in reasons:
                note = ('   (its reason moves with it: %s)' if old in sole else
                        '   (its reason, which --generate drops: more than one line moved under this layer pair and '
                        'header, so give each new line its reason by hand: %s)') % reasons[old]
            out('  moved: replace the old line %s -> %s with %s -> %s   [%s: %s -> %s]%s'
                % (old[0], old[1], k[0], k[1], kind, key[0], key[1], note))
    if stale:
        out('CheckLayout: %d allow-list line(s) name an edge that is gone from the tree or is allowed now -- '
            'delete them (%s):' % (len(stale), allow_rel))
        for e in stale:
            note = ''
            if line_kind(e) in KEY_BY_DIRECTORY:
                note = ('   [a sideways edge is listed per includer directory and peer: %s [%s] -> %s]'
                        % (e[0].rsplit('/', 1)[0], layer(e[0][4:]), e[1]))
            if e in reasons:
                note += '   (its reason goes with it: %s)' % reasons[e]
            out('  %s -> %s%s' % (e[0], e[1], note))
    if errors or new or moved or stale:
        out('CheckLayout: FAIL')
        return 1
    per = collections.Counter(kind for kind, _ in keys.values())
    out('CheckLayout: OK: %d list lines (%d against + %d seam per includer file and header, %d sideways per '
        'includer directory, peer and header), all %d on the allow-list'
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
    old, errors, reasons, lost, replaced = read_allow(allow_path)
    if lost:
        for e in errors:
            out('CheckLayout: ERROR: %s' % e)
        out('CheckLayout: the list is not written: it would lose these "#" lines: %s (make each one a reason '
            'directly above its entry, or delete it)' % ' | '.join(lost))
        return 1
    entries = set(keys_of(result['edges']))
    carried, dropped = [], []
    _, _, moves, _, _, sole = compare(result['edges'], old)
    for was, now in moves:
        if was in reasons and now not in reasons:
            if was in sole:
                reasons[now] = reasons[was]
                carried.append((was, now))
            else:
                dropped.append(was)
    with open(allow_path, 'w', encoding='utf-8', newline='\n') as fh:
        fh.write(format_allow(entries, reasons))
    added, removed = sorted(entries - old), sorted(old - entries)
    for e in added:
        out('  + %s -> %s' % e)
    for e in removed:
        out('  - %s -> %s%s' % (e[0], e[1], '   (and its reason: %s)' % reasons[e] if e in reasons else ''))
    for was, now in carried:
        out('  reason moved from %s -> %s to %s -> %s: %s' % (was + now + (reasons[now],)))
    for was in dropped:
        out('  reason NOT carried from %s -> %s (more than one line moved under its layer pair and header; give '
            'each new line its reason by hand): %s' % (was + (reasons[was],)))
    for line in replaced:
        out('  header line replaced by the tool\'s header: %s' % line)
    summary(result, out)
    out('CheckLayout: wrote %d lines to %s (+%d, -%d)' % (len(entries), allow_path, len(added), len(removed)))
    return 0


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
SELF_ALLOW = [('src/game/Maps [maps]', 'src/game/Object/Unit.h'),
              ('src/game/Maps/Map.cpp', 'src/game/entities/player/Player.h'),
              ('src/game/WorldHandlers [spells]', 'src/game/Object/Unit.h'),
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
         'directory, peer and header), all 4 on the allow-list', 'domain      -> proto               1 lines    '
         ' 1 edges',
         'maps        -> entities/player     1 lines     1 edges',
         'maps        -> entities            1 lines     1 edges     1 keys',
         'sideways inside the domain tier (by layer): 2 lines, 2 edges, 2 (includer directory, includer peer, '
         'header) keys',
         '(15 allowed, 4 against/seam/sideways, 1 skipped: tests -> tools/), 1 third-party under dep/, 1 generated'])
    run('a planted upward include fails (foundation -> entities)',
        with_file('src/shared/Common.cpp', '#include "Common.h"\n#include "Object/Unit.h"\n'), SELF_ALLOW, 1,
        ['1 new include edge(s) against section 1', 'an upward or forbidden include',
         'src/shared/Common.cpp -> src/game/Object/Unit.h   [against: foundation -> domain]'])
    run('a new sideways include fails (spells -> maps)',
        with_file('src/game/WorldHandlers/Spell.cpp', '#include "Unit.h"\n#include "WorldPacket.h"\n#include "Map.h"\n'),
        SELF_ALLOW, 1, ['1 new sideways include key(s) (includer directory, includer peer, header)',
                        'section 1 allows these lines but the keys cannot grow',
                        '  src/game/WorldHandlers [spells] -> src/game/Maps/Map.h   [sideways: spells -> maps] '
                        'included by '
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
    run('relative first picks the includer\'s own World.h',
        dict(SELF_TREE, **{'src/game/WorldHandlers/World.h': '', 'src/shared/World.h': '',
                           'src/game/WorldHandlers/Spell.cpp':
                               '#include "Unit.h"\n#include "WorldPacket.h"\n#include "World.h"\n'}),
        SELF_ALLOW, 1, 'src/game/WorldHandlers/Spell.cpp -> src/game/WorldHandlers/World.h   [against: domain -> app]')
    run('a spelling matches at a directory boundary only',
        dict(SELF_TREE, **{'src/game/GameObject/X.h': '',
                           'src/game/Maps/Map.h': '#include "Common.h"\n#include "Object/X.h"\n'}),
        SELF_ALLOW, 1, 'src/game/Maps/Map.h: #include "Object/X.h" names a header the tree does not have')
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
    run('a same-peer include into a seam follows SEAM_SAME_PEER',
        with_file('src/game/Object/Bag.cpp', '#include "Player.h"\n'), SELF_ALLOW, 1 if SEAM_SAME_PEER else 0,
        '  src/game/Object/Bag.cpp -> src/game/entities/player/Player.h   [seam: entities -> entities/player]'
        if SEAM_SAME_PEER else 'CheckLayout: OK: 4 list lines')
    run('duplicate violating lines collapse to one edge',
        with_file('src/game/Maps/Map.cpp', '#include "Map.h"\n#include "Unit.h"\n#include "Player.h"\n#include "Unit.h"\n'),
        SELF_ALLOW, 0, ['CheckLayout: OK: 4 list lines', 'all 4 on the allow-list',
                        'maps        -> entities            2 lines     1 edges     1 keys'])

    run('rule 1: a new file in a listed directory, listed header, passes',
        with_file('src/game/Maps/MapGrid.cpp', '#include "Unit.h"\n'), SELF_ALLOW, 0,
        ['CheckLayout: OK: 4 list lines', 'maps        -> entities            2 lines     2 edges     1 keys'])
    run('rule 1: a split beside SpellAuraDummy.cpp (out of the seam) passes',
        with_file('src/game/WorldHandlers/SpellAuraDummyWarrior.cpp', '#include "Unit.h"\n'), SELF_ALLOW, 0,
        ['CheckLayout: OK: 4 list lines', 'spells      -> entities            2 lines     2 edges     1 keys'])
    run('rule 1: a new (directory, header) pair fails (spells/handlers)',
        with_file('src/game/spells/handlers/AuraDummyWarrior.cpp', '#include "Unit.h"\n'), SELF_ALLOW, 1,
        ['1 new sideways include key(s)', '  src/game/spells/handlers [spells] -> src/game/Object/Unit.h   '
         '[sideways: spells '
         '-> entities] included by src/game/spells/handlers/AuraDummyWarrior.cpp'])
    run('rule 1: another peer in a listed directory, listed header, fails',
        with_file('src/game/WorldHandlers/GroupSplit.cpp', '#include "Unit.h"\n'), SELF_ALLOW, 1,
        '  src/game/WorldHandlers [social] -> src/game/Object/Unit.h   [sideways: social -> entities] included by '
        'src/game/WorldHandlers/GroupSplit.cpp')
    run('rule 1: a listed pair with no includer left is stale and fails',
        with_file('src/game/Maps/Map.cpp', '#include "Map.h"\n#include "Player.h"\n'), SELF_ALLOW, 1,
        ['1 allow-list line(s) name an edge that is gone', '  src/game/Maps [maps] -> src/game/Object/Unit.h'])
    run('rule 1: a pair stays while one includer in the directory is left',
        dict(SELF_TREE, **{'src/game/Maps/Map.cpp': '#include "Map.h"\n#include "Player.h"\n',
                           'src/game/Maps/MapGrid.cpp': '#include "Unit.h"\n'}), SELF_ALLOW, 0,
        'CheckLayout: OK: 4 list lines')
    run('rule 1: a sideways edge listed per file (the old key) fails',
        SELF_TREE,
        [e for e in SELF_ALLOW if e[0] != 'src/game/Maps [maps]']
        + [('src/game/Maps/Map.cpp', 'src/game/Object/Unit.h')],
        1, ['  src/game/Maps [maps] -> src/game/Object/Unit.h   [sideways: maps -> entities] included by',
            '  src/game/Maps/Map.cpp -> src/game/Object/Unit.h   [a sideways edge is listed per includer '
            'directory and peer: '
            'src/game/Maps [maps] -> src/game/Object/Unit.h]'])

    run('rule 2: a second file including a forbidden header fails',
        with_file('src/game/WorldHandlers/SpellEffects.cpp', '#include "WorldPacket.h"\n'), SELF_ALLOW, 1,
        ['1 new include edge(s) against section 1',
         '  src/game/WorldHandlers/SpellEffects.cpp -> src/proto/WorldPacket.h'
         '   [against: domain -> proto] the header has 2 domain -> proto includer file(s) in the tree, 1 '
         'listed'], 'moved:')
    run('rule 2: a second file including a seam header fails',
        with_file('src/game/Maps/MapGrid.cpp', '#include "Player.h"\n'), SELF_ALLOW, 1,
        ['1 new include edge(s) into a gated seam', '  src/game/Maps/MapGrid.cpp -> src/game/entities/player/Player.h'
         '   [seam: maps -> entities/player] the header has 2 maps -> entities/player includer file(s) in the '
         'tree, 1 listed'], 'moved:')

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
        ['  src/game/Maps/Map.cpp -> src/proto/WorldPacket.h   [against: domain -> proto] the header has 2 '
         'domain -> proto '
         'includer file(s) in the tree, 1 listed'], 'moved:')
    two_new = dict(moved_tree, **{
        'src/game/Object/Unit.cpp': '#include "Unit.h"\n#include "zlib.h"\n#include "WorldPacket.h"\n'})
    run('rule 3: two includers for one deleted line fail (list untouched)', two_new, SELF_ALLOW, 1,
        [moved_text, '  src/game/Object/Unit.cpp -> src/proto/WorldPacket.h   [against: domain -> proto] the header '
         'has 2 domain -> proto includer file(s) in the tree, 1 listed'])
    run('rule 3: two includers for one deleted line fail (line replaced)', two_new, moved_allow, 1,
        '  src/game/Object/Unit.cpp -> src/proto/WorldPacket.h   [against: domain -> proto] the header has 2 '
        'domain -> proto '
        'includer file(s) in the tree, 1 listed', 'moved:')
    old_pl = ('src/game/Maps/Map.cpp', 'src/game/entities/player/Player.h')
    new_pl = ('src/game/Maps/MapGrid.cpp', 'src/game/entities/player/Player.h')
    seam_tree = dict(SELF_TREE, **{
        'src/game/Maps/Map.cpp': '#include "Map.h"\n#include "Unit.h"\n',
        'src/game/Maps/MapGrid.cpp': '#include "Player.h"\n'})
    run('rule 3: a moved seam line, replaced in the list, passes', seam_tree,
        [e for e in SELF_ALLOW if e != old_pl] + [new_pl], 0, 'CheckLayout: OK: 4 list lines')
    run('rule 3: a moved seam line not replaced fails as moved', seam_tree, SELF_ALLOW, 1,
        '  moved: replace the old line src/game/Maps/Map.cpp -> src/game/entities/player/Player.h with '
        'src/game/Maps/MapGrid.cpp -> src/game/entities/player/Player.h   [seam: maps -> entities/player]',
        'new include edge(s) into a gated seam')
    run('rule 3: a move does not cross kinds (seam line, against includer)',
        dict(SELF_TREE, **{'src/game/Maps/Map.cpp': '#include "Map.h"\n#include "Unit.h"\n',
                           'src/game/Server/DBCStores.cpp': '#include "Player.h"\n'}), SELF_ALLOW, 1,
        ['  src/game/Server/DBCStores.cpp -> src/game/entities/player/Player.h   [against: data -> domain]',
         '  src/game/Maps/Map.cpp -> src/game/entities/player/Player.h'], 'moved:')
    run('rule 3: a move does not cross headers (stale H1 line, new H2 edge)',
        dict(SELF_TREE, **{'src/proto/Opcodes.h': '#include "Common.h"\n',
                           'src/game/WorldHandlers/Spell.cpp': '#include "Unit.h"\n',
                           'src/game/Maps/Map.cpp':
                               '#include "Map.h"\n#include "Unit.h"\n#include "Player.h"\n#include "Opcodes.h"\n'}),
        SELF_ALLOW, 1,
        ['1 new include edge(s) against', '  src/game/Maps/Map.cpp -> src/proto/Opcodes.h   [against: domain -> '
         'proto] the header has 1 domain -> proto includer file(s) in the tree, 0 listed',
         '1 allow-list line(s) name an edge that is gone',
         '  src/game/WorldHandlers/Spell.cpp -> src/proto/WorldPacket.h'],
        'moved:')
    run('rule 3: a move does not cross layer pairs (against)',
        dict(SELF_TREE, **{'src/game/WorldHandlers/Spell.cpp': '#include "Unit.h"\n',
                           'src/game/Server/ObjectMgr.cpp': '#include "WorldPacket.h"\n'}), SELF_ALLOW, 1,
        ['  src/game/Server/ObjectMgr.cpp -> src/proto/WorldPacket.h   [against: data -> proto] the header has 1 '
         'data -> proto includer file(s) in the tree, 0 listed',
         '  src/game/WorldHandlers/Spell.cpp -> src/proto/WorldPacket.h'], 'moved:')
    run('rule 3: a move does not cross layer pairs (seam)',
        dict(SELF_TREE, **{'src/game/Maps/Map.cpp': '#include "Map.h"\n#include "Unit.h"\n',
                           'src/game/WorldHandlers/Spell.cpp':
                               '#include "Unit.h"\n#include "WorldPacket.h"\n#include "Player.h"\n'}),
        SELF_ALLOW, 1,
        ['  src/game/WorldHandlers/Spell.cpp -> src/game/entities/player/Player.h   [seam: spells -> entities/player]',
         '  src/game/Maps/Map.cpp -> src/game/entities/player/Player.h'], 'moved:')
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

    def generated(tree, allow):
        """--generate on a fixture: its exit, the list it leaves and its output."""
        with tempfile.TemporaryDirectory() as tmp:
            ap = build(tmp, tree, allow)
            got = []
            rc = generate(tmp, ap, out=got.append)
            with open(ap, encoding='utf-8') as fh:
                text = fh.read()
            rc2 = check(tmp, ap, out=got.append)
            return rc, text, got, rc2, read_allow(ap)[2]

    def row(label, ok, detail):
        rows.append(label)
        print('self-test: %-66s %s' % (label, 'PASS' if ok else 'FAIL'))
        expect(ok, '%s: %s' % (label, detail))

    why, first = '# Map.cpp reads the player', '# the first entry, right below the header'
    reasoned = format_allow(SELF_ALLOW, {SELF_ALLOW[0]: first, SELF_ALLOW[1]: why})
    rc, text, got, rc2, kept = generated(SELF_TREE, reasoned)
    row('reason lines survive --generate (the first entry\'s too)',
        rc == 0 and rc2 == 0 and text == reasoned and '%s\n%s -> %s\n' % ((why,) + SELF_ALLOW[1]) in text
        and kept == {SELF_ALLOW[0]: first, SELF_ALLOW[1]: why}, '%d %d\n%s\n%s' % (rc, rc2, text, '\n'.join(got)))
    gone_tree = with_file('src/game/Maps/Map.cpp', '#include "Map.h"\n#include "Unit.h"\n')
    run('a reason whose entry is gone fails with the stale entry', gone_tree, reasoned, 1,
        ['1 allow-list line(s) name an edge that is gone',
         '  src/game/Maps/Map.cpp -> src/game/entities/player/Player.h   (its reason goes with it: %s)' % why])
    rc, text, got, _, _ = generated(gone_tree, reasoned)
    row('--generate drops a reason with its entry',
        rc == 0 and why not in text and text == format_allow(SELF_ALLOW[:1] + SELF_ALLOW[2:], {SELF_ALLOW[0]: first})
        and '  - src/game/Maps/Map.cpp -> src/game/entities/player/Player.h   (and its reason: %s)' % why in got,
        '%d\n%s\n%s' % (rc, text, '\n'.join(got)))
    moved_seam = [e for e in SELF_ALLOW if e != old_pl] + [new_pl]
    run('a moved seam line names its reason', seam_tree, reasoned, 1,
        '  moved: replace the old line src/game/Maps/Map.cpp -> src/game/entities/player/Player.h with '
        'src/game/Maps/MapGrid.cpp -> src/game/entities/player/Player.h   [seam: maps -> entities/player]'
        '   (its reason moves with it: %s)' % why)
    rc, text, got, rc2, kept = generated(seam_tree, reasoned)
    row('--generate carries a moved line\'s reason to the new line',
        rc == 0 and rc2 == 0 and text == format_allow(moved_seam, {SELF_ALLOW[0]: first, new_pl: why})
        and kept == {SELF_ALLOW[0]: first, new_pl: why},
        '%d %d\n%s\n%s' % (rc, rc2, text, '\n'.join(got)))
    rc, text, got, _, kept = generated(SELF_TREE, format_allow(SELF_ALLOW))
    row('the header\'s "#" lines are not reasons', rc == 0 and kept == {}, '%d %r' % (rc, kept))
    r1, r2 = '# Spell.cpp sends a packet', '# MapPacket.cpp sends a packet'
    two_line = ('src/game/Maps/MapPacket.cpp', 'src/proto/WorldPacket.h')
    two_allow = format_allow(SELF_ALLOW + [two_line], {SELF_ALLOW[0]: first, old_wp: r1, two_line: r2})
    two_before = dict(SELF_TREE, **{'src/game/Maps/MapPacket.cpp': '#include "WorldPacket.h"\n'})
    two_moved = dict(SELF_TREE, **{'src/game/WorldHandlers/Spell.cpp': '#include "Unit.h"\n',
                                   'src/game/Maps/AMapPacket.cpp': '#include "WorldPacket.h"\n',
                                   'src/game/Object/ZUnitPacket.cpp': '#include "WorldPacket.h"\n'})
    run('two moves, one header: the baseline list passes', two_before, two_allow, 0, 'CheckLayout: OK')
    run('two moves, one header: --check names both reasons, not moving', two_moved, two_allow, 1,
        ['with src/game/Maps/AMapPacket.cpp -> src/proto/WorldPacket.h   [against: domain -> proto]   (its reason, '
         'which --generate drops: more than one line moved under this layer pair and header, so give each new '
         'line its reason by hand: %s)' % r2, 'give each new line its reason by hand: %s)' % r1],
        'its reason moves with it')
    rc, text, got, rc2, kept = generated(two_moved, two_allow)
    row('two moves, one header: --generate carries no reason, names both',
        rc == 0 and rc2 == 0 and kept == {SELF_ALLOW[0]: first}
        and any('reason NOT carried from src/game/WorldHandlers/Spell.cpp -> src/proto/WorldPacket.h' in g
                and g.endswith(r1) for g in got)
        and any('reason NOT carried from src/game/Maps/MapPacket.cpp -> src/proto/WorldPacket.h' in g
                and g.endswith(r2) for g in got), '%d %d %r\n%s' % (rc, rc2, kept, '\n'.join(got)))
    no_blank = reasoned.replace(ALLOW_HEADER + '\n', ALLOW_HEADER, 1)
    run('a reason right below the header, no blank line, fails', SELF_TREE, no_blank, 1,
        ['the list does not start with a header block and one blank line',
         'a "#" line in a header block with no blank line after it: %s -- add the blank line after the header, '
         'then the line becomes a reason or an error' % first])
    rc, text, got, _, _ = generated(SELF_TREE, no_blank)
    row('--generate keeps that list as it is (the line is not deleted)', rc == 1 and text == no_blank,
        '%d\n%s' % (rc, '\n'.join(got)))
    above = '# above the header\n' + reasoned
    run('a "#" line above the header is part of the header block', SELF_TREE, above, 0, 'CheckLayout: OK')
    rc, text, got, rc2, _ = generated(SELF_TREE, above)
    row('--generate replaces that header block and names the line', rc == 0 and rc2 == 0 and text == reasoned
        and '  header line replaced by the tool\'s header: # above the header' in got, '%d\n%s' % (rc, '\n'.join(got)))
    reworded = reasoned.replace('After removing or moving edges', 'After removing, moving or splitting edges', 1)
    run('a reworded header line passes --check', SELF_TREE, reworded, 0, 'CheckLayout: OK')
    rc, text, got, rc2, kept = generated(SELF_TREE, reworded)
    row('--generate repairs a reworded header line, reasons kept', rc == 0 and rc2 == 0 and text == reasoned
        and kept == {SELF_ALLOW[0]: first, SELF_ALLOW[1]: why}
        and any(g.startswith('  header line replaced by the tool\'s header: # After removing, moving') for g in got),
        '%d\n%s' % (rc, '\n'.join(got)))
    old_header = ALLOW_HEADER + ''.join('%s -> %s\n' % e for e in sorted(SELF_ALLOW))
    run('a header with no blank line after it fails', SELF_TREE, old_header, 1,
        'the list does not start with a header block and one blank line', 'no blank line after it:')
    rc, text, got, rc2, _ = generated(SELF_TREE, old_header)
    row('--generate writes the header and its blank line', rc == 0 and rc2 == 0 and text == format_allow(SELF_ALLOW),
        '%d %d\n%s' % (rc, rc2, '\n'.join(got)))
    run('a reason with no text fails', SELF_TREE, reasoned.replace(why + '\n', '#\n'), 1,
        'a reason line with no text: #')
    run('a reason line not directly above an entry fails', SELF_TREE,
        reasoned.replace(why + '\n', why + '\n\n'), 1, 'a reason line not directly above an entry: %s' % why)
    run('two reason lines above one entry fail', SELF_TREE,
        reasoned.replace(why + '\n', '# first\n' + why + '\n'), 1, 'a reason line not directly above an entry: # first')
    run('a reason line at the end of the list fails', SELF_TREE, reasoned + '# trailing\n', 1,
        'a reason line not directly above an entry: # trailing')
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
                       ('game/session/packets/spells/CooldownPackets.cpp', 'proto/WorldPacket.h', None),
                       ('game/session/packets/spells/CooldownPacketSinks.cpp', 'game/entities/player/Player.h', None),
                       ('game/spells/SpellCooldownMgr.cpp', 'game/session/packets/spells/CooldownPackets.h',
                        ('against', ('domain', 'session'))),
                       ('game/Object/Bag.cpp', 'game/entities/player/Player.h',
                        ('seam', ('entities', 'entities/player')) if SEAM_SAME_PEER else None)]:
        got = classify(a, b)
        expect(got == want, 'classify(%s, %s) = %r, expected %r' % (a, b, got, want))
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
    ap = argparse.ArgumentParser(description=__doc__[__doc__.index('CheckLayout:'):],
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
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
