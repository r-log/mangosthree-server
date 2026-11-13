#!/usr/bin/env python3
"""class_logic.py [--root <repo root>] --measure | --check --gate <name>=<ceiling> ... | --self-test: the class logic
left in engine files, counted, with a ceiling per counter.

Six counters over src/game, read as text line by line, as grep reads it; comments count:

  legacy_sites     lines that dispatch a legacy handler site, `SpellHandlerRegistry::Game().Dispatch<...Site>`, in
                   the .cpp files. A script hook's dispatch (`Dispatch<...Hook>`) is not counted.
  legacy_rows      lines `{ <number>, &...` inside a legacy site's table, `static <Row><...Site> const <name>[] =` to
                   the brace that closes it, in the .cpp files under spells/ (handlers/ and classes/ alike). A row of
                   any other array, a script hook's table included, is not counted.
  family_mentions  lines that name `SPELLFAMILY_` in the .cpp and .h files, outside spells/handlers/,
                   spells/classes/, Harness/ and data/dbc/DBCEnums.h.
  icon_compares    lines that hold `SpellIconID ==` in the .cpp files, outside spells/handlers/ and spells/classes/.
  mask_literals    every `UI64LIT(0x...)` literal in the .cpp files under spells/ and combat/; the class files under
                   spells/classes/ count too, so a class script that tests a family mask literal raises it. Headers
                   are not read: a literal moved into a header constant lowers it.
  family_switches  every `switch (...GetSpellFamilyName()` and `switch (...SpellClassSet` in the .cpp files.

legacy_sites, legacy_rows, family_mentions and icon_compares count lines, so a second mention or compare added to a
line that is already counted is not seen.

--measure prints `name=value` per counter. --check takes a --gate for each of the six counters, prints every counter
against its ceiling and exits 0 only when every counter equals its ceiling. A counter above its ceiling fails after
its lines are listed, so the new one can be found; a counter below its ceiling fails with the number to lower the
ceiling to, so a count and its ceiling fall in the same change. Both exit 1 when src/game holds no .cpp file: a gate
that reads nothing passes nothing. Exit 2 is a usage error. --self-test builds a fixture tree, plants one line per
counter and expects that counter alone to rise by one and --check to fail, then plants one line per exclusion and
expects no counter to move.

Not caught: a dispatch through a stored reference to the registry, a row whose spell id is not a number literal, a
table declared in another shape, a legacy row registered by a direct SpellHandlerRegistry::Register call (the
hooks' RegisterScript and RegisterRanks refuse a legacy site at compile time), and anything spelled by a macro.
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
import os
import re
import shutil
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import case_labels  # noqa: E402

COUNTERS = ('legacy_sites', 'legacy_rows', 'family_mentions', 'icon_compares', 'mask_literals', 'family_switches')

SITE_DISPATCH_RX = re.compile(r'SpellHandlerRegistry::Game\(\)\.Dispatch<\w+Site>')
SITE_TABLE_RX = re.compile(r'static \w+<\w+Site> const \w+\[\] =')
ROW_RX = re.compile(r'^\s*\{\s*\d+\s*,\s*&')
FAMILY_RX = re.compile(r'SPELLFAMILY_')
ICON_RX = re.compile(r'SpellIconID\s*==')
MASK_RX = re.compile(r'UI64LIT\(0x[0-9A-Fa-f]+\)')
SWITCH_RX = re.compile(r'switch\s*\(.*GetSpellFamilyName\(\)|switch\s*\(.*SpellClassSet')

ROW_DIRS = ('spells/',)
FAMILY_SKIP_DIRS = ('spells/handlers/', 'spells/classes/', 'Harness/')
FAMILY_SKIP_FILES = ('data/dbc/DBCEnums.h',)
ICON_SKIP_DIRS = ('spells/handlers/', 'spells/classes/')
MASK_DIRS = ('spells/', 'combat/')

FAILURE_HINT = ('new class logic goes to spells/classes/<class>/ on the script hooks of spells/SpellScriptHooks.h '
                '(design/architecture.md section 9)')


def game_files(root):
    """[(path under src/game with '/', text)] of every .cpp and .h file under <root>/src/game, sorted by path."""
    game = os.path.join(root, 'src', 'game')
    out = []
    for folder, dirs, names in os.walk(game):
        dirs.sort()
        for name in sorted(names):
            if not (name.endswith('.cpp') or name.endswith('.h')):
                continue
            path = os.path.join(folder, name)
            with open(path, encoding='utf-8', errors='replace', newline='') as fh:
                out.append((os.path.relpath(path, game).replace(os.sep, '/'), fh.read()))
    return out


def site_table_lines(text):
    """The line numbers (from 1) of every legacy site table in the text, from its header to its closing brace."""
    clean = case_labels.blank(text)
    lines = set()
    for m in SITE_TABLE_RX.finditer(clean):
        start = clean.find('{', m.end())
        if start < 0 or clean[m.end():start].strip():
            continue
        depth, end = 0, len(clean)
        for i in range(start, len(clean)):
            if clean[i] == '{':
                depth += 1
            elif clean[i] == '}':
                depth -= 1
                if depth == 0:
                    end = i
                    break
        first = clean.count('\n', 0, m.start()) + 1
        lines.update(range(first, first + clean.count('\n', m.start(), end) + 1))
    return lines


def measure(root):
    """({counter: [(path under src/game, line, text)]} with one entry per count, number of .cpp files read)."""
    hits = dict((name, []) for name in COUNTERS)
    cpp_files = 0
    for rel, text in game_files(root):
        cpp = rel.endswith('.cpp')
        cpp_files += cpp
        tables = site_table_lines(text) if cpp and rel.startswith(ROW_DIRS) and 'Site>' in text else set()
        family = 'SPELLFAMILY_' in text and not (rel.startswith(FAMILY_SKIP_DIRS) or rel in FAMILY_SKIP_FILES)
        icons = cpp and 'SpellIconID' in text and not rel.startswith(ICON_SKIP_DIRS)
        masks = cpp and 'UI64LIT' in text and rel.startswith(MASK_DIRS)
        for no, line in enumerate(text.split('\n'), 1):
            found = []
            if cpp and SITE_DISPATCH_RX.search(line):
                found.append('legacy_sites')
            if no in tables and ROW_RX.search(line):
                found.append('legacy_rows')
            if family and FAMILY_RX.search(line):
                found.append('family_mentions')
            if icons and ICON_RX.search(line):
                found.append('icon_compares')
            if masks:
                found.extend(['mask_literals'] * len(MASK_RX.findall(line)))
            if cpp and 'switch' in line:
                found.extend(['family_switches'] * len(SWITCH_RX.findall(line)))
            for name in found:
                hits[name].append((rel, no, line.strip()))
    return hits, cpp_files


def parse_gates(specs):
    """{counter: ceiling} from the --gate values; ValueError unless every counter is gated once."""
    gates = {}
    for spec in specs:
        name, sep, value = spec.partition('=')
        name = name.strip()
        if not sep or name not in COUNTERS:
            raise ValueError("--gate wants <counter>=<ceiling> with a counter of %s, got '%s'"
                             % (', '.join(COUNTERS), spec))
        try:
            ceiling = int(value.strip())
        except ValueError:
            raise ValueError("--gate %s: '%s' is not a number" % (name, value.strip()))
        if ceiling < 0:
            raise ValueError('--gate %s: a ceiling cannot be negative, got %d' % (name, ceiling))
        if name in gates:
            raise ValueError('--gate %s is given twice' % name)
        gates[name] = ceiling
    missing = [name for name in COUNTERS if name not in gates]
    if missing:
        raise ValueError('--check gates every counter; no --gate for %s' % ', '.join(missing))
    return gates


def empty_tree(root, out):
    out('ClassLogic: read no .cpp file under %s -- a gate that reads nothing passes nothing'
        % os.path.join(root, 'src', 'game'))
    return 1


def run_measure(root, out=print):
    hits, cpp_files = measure(root)
    if cpp_files == 0:
        return empty_tree(root, out)
    for name in COUNTERS:
        out('%s=%d' % (name, len(hits[name])))
    return 0


def check(root, gates, out=print):
    hits, cpp_files = measure(root)
    if cpp_files == 0:
        return empty_tree(root, out)
    above, below = [], []
    for name in COUNTERS:
        count, ceiling = len(hits[name]), gates[name]
        if count > ceiling:
            above.append(name)
            out('%s=%d (ceiling %d): ABOVE the ceiling' % (name, count, ceiling))
        elif count < ceiling:
            below.append(name)
            out('%s=%d (ceiling %d): BELOW the ceiling' % (name, count, ceiling))
        else:
            out('%s=%d (ceiling %d)' % (name, count, ceiling))
    for name in above:
        out('%s, every counted line:' % name)
        for rel, no, text in hits[name]:
            out('  src/game/%s:%d: %s' % (rel, no, text))
    for name in above:
        out('GATE FAILED: %s=%d is above its ceiling %d; %s'
            % (name, len(hits[name]), gates[name], FAILURE_HINT))
    for name in below:
        out('GATE FAILED: %s=%d is below its ceiling %d; lower the ceiling to %d in src/tests/CMakeLists.txt in '
            'this change' % (name, len(hits[name]), gates[name], len(hits[name])))
    if above or below:
        return 1
    out('ClassLogic OK: every counter at its ceiling, %d .cpp files read' % cpp_files)
    return 0


SELF_TREE = {
    'src/game/spells/pipeline/SpellEffectDummy.cpp': (
        'void Spell::EffectDummy(SpellEffectEntry const* effect)\n'
        '{\n'
        '    SpellHandlerRegistry::Game().Dispatch<SpellEffectDummyMageSite>(m_spellInfo->Id, ctx);\n'
        '    SpellHandlerRegistry::Game().Dispatch<DummyEffectHook>(m_spellInfo->Id, hook);\n'
        '    switch (m_spellInfo->GetSpellFamilyName())\n'
        '    {\n'
        '        case SPELLFAMILY_MAGE:\n'
        '            if (m_spellInfo->SpellIconID == 12 || m_spellInfo->SpellIconID == 13)\n'
        '                mask = UI64LIT(0x10) | UI64LIT(0x20);\n'
        '            break;\n'
        '    }\n'
        '}\n'),
    'src/game/spells/handlers/AurasHandlers.cpp': (
        '    static AurasRow<AuraBoostMageSite> const boostMage[] =\n'
        '    {\n'
        '        { 11129, &AuraBoostMage11129 },\n'
        '        // } a brace in a comment\n'
        '        { 28682, &AuraBoostMage28682 },\n'
        '    };\n'
        '    static HookRow<OnProcHook> const igniteRanks[] =\n'
        '    {\n'
        '        { 11119, &Ignite },\n'
        '    };\n'
        '    static IdBody const pairs[] =\n'
        '    {\n'
        '        { 1, &PairFirst },\n'
        '    };\n'
        '    if (spell->GetSpellFamilyName() == SPELLFAMILY_MAGE && spell->SpellIconID == 7)\n'
        '        mask = UI64LIT(0x40);\n'),
    'src/game/spells/classes/mage/Fire.cpp': (
        '    static AurasRow<AuraBoostMageSite> const boostFire[] =\n'
        '    {\n'
        '        { 28682, &Combustion },\n'
        '    };\n'
        '    if (spell->IsFitToFamily(SPELLFAMILY_MAGE, UI64LIT(0x80)) && spell->SpellIconID == 937) {}\n'),
    'src/game/Harness/ScenariosSpell.cpp': 'uint32 family = SPELLFAMILY_MAGE;\n',
    'src/game/data/dbc/DBCEnums.h': 'enum SpellFamily { SPELLFAMILY_MAGE = 3 };\n',
    'src/game/combat/UnitDamage.cpp': (
        '    // SPELLFAMILY_WARRIOR in a comment\n'
        '    if (spell->IsFitToFamilyMask(UI64LIT(0x0000000000000100)))\n'
        '        switch(classOptions->SpellClassSet)\n'),
    'src/game/entities/Unit.h': 'bool HasMageArmor() const; // SPELLFAMILY_MAGE\n',
    'src/game/entities/Unit.cpp': 'uint64 const flags = UI64LIT(0x1);\n',
}

SELF_COUNTS = {'legacy_sites': 1, 'legacy_rows': 3, 'family_mentions': 3, 'icon_compares': 1, 'mask_literals': 5,
               'family_switches': 2}

SELF_PLANTS = (
    ('legacy_sites', 'a legacy dispatch', 'src/game/spells/auras/SpellAuraDummy.cpp',
     'SpellHandlerRegistry::Game().Dispatch<AuraDummyMageSite>(GetId(), ctx);\n'),
    ('legacy_rows', 'a row in a legacy table', 'src/game/spells/handlers/AurasHandlers.cpp',
     '    static AurasRow<AuraBoostPriestSite> const boostPriest[] =\n    {\n        { 552, &AuraBoostPriest552 },\n'
     '    };\n'),
    ('legacy_rows', 'a row in a legacy table of a class file', 'src/game/spells/classes/mage/Fire.cpp',
     '    static AurasRow<AuraBoostMageSite> const boostMore[] =\n    {\n        {44401,&PushingTheLimit},\n    };\n'),
    ('family_mentions', 'a family arm', 'src/game/combat/UnitDamage.cpp', '        case SPELLFAMILY_ROGUE:\n'),
    ('family_mentions', 'a mention in a comment', 'src/game/entities/Unit.cpp', '// only SPELLFAMILY_DRUID\n'),
    ('family_mentions', 'a mention in a header', 'src/game/spells/data/SpellMgr.h',
     'inline bool IsMage(uint32 f) { return f == SPELLFAMILY_MAGE; }\n'),
    ('icon_compares', 'an icon compare', 'src/game/spells/auras/SpellAuraDummy.cpp',
     'if (GetSpellProto()->SpellIconID==2120) {}\n'),
    ('icon_compares', 'an icon compare in Harness/', 'src/game/Harness/ScenariosSpell.cpp',
     'if (spell->SpellIconID == 2120) {}\n'),
    ('mask_literals', 'a mask literal in combat/', 'src/game/combat/UnitDamage.cpp',
     'uint64 const mask = UI64LIT(0x200);\n'),
    ('mask_literals', 'a mask literal in spells/classes/', 'src/game/spells/classes/mage/Fire.cpp',
     'uint64 const fireball = UI64LIT(0x1);\n'),
    ('family_switches', 'a family switch outside spells/', 'src/game/entities/Unit.cpp',
     'switch (spellProto->GetSpellFamilyName())\n'),
)

SELF_EXCLUDED = (
    ('a script hook dispatch', 'src/game/spells/auras/SpellAuraDummy.cpp',
     'SpellHandlerRegistry::Game().Dispatch<CheckProcHook>(GetId(), ctx);\n'),
    ('a legacy dispatch in a header', 'src/game/spells/handlers/SpellHandlerRegistry.h',
     'inline void D() { SpellHandlerRegistry::Game().Dispatch<AuraDummyMageSite>(1, ctx); }\n'),
    ('a row in a script hook table', 'src/game/spells/handlers/AurasHandlers.cpp',
     '    static HookRow<OnProcHook> const moreRanks[] =\n    {\n        { 12846, &Ignite },\n    };\n'),
    ('a row in an array of another type', 'src/game/spells/handlers/AurasHandlers.cpp',
     '    static IdBody const morePairs[] =\n    {\n        { 2, &PairSecond },\n    };\n'),
    ('a row after a table has closed', 'src/game/spells/handlers/AurasHandlers.cpp', '        { 3, &Loose },\n'),
    ('a row in the block after a table header that is not followed by its braces',
     'src/game/spells/handlers/AurasHandlers.cpp',
     '    static AurasRow<AuraBoostMageSite> const built[] = BuildRows();\n    void F()\n    {\n'
     '        { 5, &Built },\n    }\n'),
    ('a legacy table outside spells/', 'src/game/entities/UnitHandlers.cpp',
     '    static AurasRow<AuraBoostMageSite> const elsewhere[] =\n    {\n        { 4, &Elsewhere },\n    };\n'),
    ('a mention in spells/handlers/', 'src/game/spells/handlers/AurasHandlers.cpp', 'case SPELLFAMILY_ROGUE:\n'),
    ('a mention in spells/classes/', 'src/game/spells/classes/mage/Mage.h', '// SPELLFAMILY_MAGE\n'),
    ('a mention in Harness/', 'src/game/Harness/Scenario.cpp', 'uint32 f = SPELLFAMILY_PRIEST;\n'),
    ('a mention in DBCEnums.h', 'src/game/data/dbc/DBCEnums.h', '    SPELLFAMILY_PRIEST = 6,\n'),
    ('a second mention on a counted line', 'src/game/combat/UnitDamage.cpp',
     '    // SPELLFAMILY_WARRIOR and SPELLFAMILY_ROGUE in a comment\n'),
    ('an icon compare in spells/handlers/', 'src/game/spells/handlers/AurasHandlers.cpp',
     'if (s->SpellIconID == 1) {}\n'),
    ('an icon compare in spells/classes/', 'src/game/spells/classes/mage/Fire.cpp', 'if (s->SpellIconID == 2) {}\n'),
    ('an icon compare in a header', 'src/game/spells/data/SpellMgr.h',
     'inline bool I(S* s) { return s->SpellIconID == 3; }\n'),
    ('an icon assignment', 'src/game/spells/auras/SpellAuraDummy.cpp', 'uint32 icon = s->SpellIconID;\n'),
    ('a mask literal outside spells/ and combat/', 'src/game/entities/Unit.cpp', 'uint64 const more = UI64LIT(0x2);\n'),
    ('a mask literal in a header', 'src/game/spells/data/SpellMgr.h', 'uint64 const M = UI64LIT(0x4);\n'),
    ('a family switch in a header', 'src/game/spells/data/SpellMgr.h',
     'inline void S(S* s) { switch (s->GetSpellFamilyName()) {} }\n'),
    ('a switch on something else', 'src/game/entities/Unit.cpp', 'switch (GetTypeId())\n'),
)


def self_test():
    failures = []

    def with_tree(files, action):
        d = tempfile.mkdtemp(prefix='class_logic_')
        try:
            for rel, text in files.items():
                path = os.path.join(d, *rel.split('/'))
                os.makedirs(os.path.dirname(path), exist_ok=True)
                with open(path, 'w', encoding='utf-8', newline='') as fh:
                    fh.write(text)
            return action(d)
        finally:
            shutil.rmtree(d, ignore_errors=True)

    def counts(files):
        return with_tree(files, lambda d: dict((n, len(h)) for n, h in measure(d)[0].items()))

    def checked(files, gates):
        lines = []
        code = with_tree(files, lambda d: check(d, gates, out=lines.append))
        return code, lines

    def planted(rel, text):
        return dict(SELF_TREE, **{rel: SELF_TREE.get(rel, '') + text})

    got = counts(SELF_TREE)
    if got != SELF_COUNTS:
        failures.append('the fixture tree counts %r, expected %r' % (got, SELF_COUNTS))

    for name, label, rel, text in SELF_PLANTS:
        want = dict(SELF_COUNTS, **{name: SELF_COUNTS[name] + 1})
        tree = planted(rel, text)
        got = counts(tree)
        if got != want:
            failures.append('plant %s (%s): counts %r, expected %r' % (label, name, got, want))
        code, lines = checked(tree, SELF_COUNTS)
        if code != 1 or not any(ln.startswith('GATE FAILED: %s=' % name) for ln in lines):
            failures.append('plant %s: --check gave %d without naming %s at the ceilings' % (label, code, name))

    for label, rel, text in SELF_EXCLUDED:
        if rel == 'src/game/combat/UnitDamage.cpp':
            tree = dict(SELF_TREE, **{rel: SELF_TREE[rel].replace('    // SPELLFAMILY_WARRIOR in a comment\n', text)})
        else:
            tree = planted(rel, text)
        got = counts(tree)
        if got != SELF_COUNTS:
            failures.append('%s is counted: %r, expected %r' % (label, got, SELF_COUNTS))

    code, lines = checked(SELF_TREE, SELF_COUNTS)
    if code != 0 or len(lines) != len(COUNTERS) + 1:
        failures.append('the fixture tree at its ceilings: --check gave %d, %r' % (code, lines))
    code, lines = checked(SELF_TREE, dict((n, c + 1) for n, c in SELF_COUNTS.items()))
    if code != 1 or sum('lower the ceiling to' in ln for ln in lines) != len(COUNTERS):
        failures.append('the fixture tree below its ceilings: --check gave %d, %r' % (code, lines))
    for name in COUNTERS:
        code, lines = checked(SELF_TREE, dict(SELF_COUNTS, **{name: SELF_COUNTS[name] - 1}))
        failed = [ln for ln in lines if ln.startswith('GATE FAILED')]
        if code != 1 or len(failed) != 1 or not failed[0].startswith('GATE FAILED: %s=' % name):
            failures.append('a ceiling of %s one below its count: --check gave %d, %r' % (name, code, failed))
    for name in COUNTERS:
        code, lines = checked(SELF_TREE, dict(SELF_COUNTS, **{name: SELF_COUNTS[name] + 1}))
        failed = [ln for ln in lines if ln.startswith('GATE FAILED')]
        want = 'GATE FAILED: %s=%d is below its ceiling %d; lower the ceiling to %d ' % (
            name, SELF_COUNTS[name], SELF_COUNTS[name] + 1, SELF_COUNTS[name])
        if code != 1 or len(failed) != 1 or not failed[0].startswith(want):
            failures.append('a ceiling of %s one above its count: --check gave %d, %r' % (name, code, failed))

    for label, files in (('an empty tree', {}),
                         ('a tree of headers only', {'src/game/entities/Unit.h': 'case SPELLFAMILY_MAGE:\n'}),
                         ('a tree without src/game', {'src/shared/Util.cpp': 'case SPELLFAMILY_MAGE:\n'})):
        if checked(files, SELF_COUNTS)[0] != 1:
            failures.append('%s passes --check' % label)
        if checked(files, dict((n, 0) for n in COUNTERS))[0] != 1:
            failures.append('%s passes --check at ceilings of 0' % label)
        if with_tree(files, lambda d: run_measure(d, out=lambda s: None)) != 1:
            failures.append('%s passes --measure' % label)

    full = ['%s=%d' % (n, c) for n, c in SELF_COUNTS.items()]
    for label, specs in (('every counter', full),):
        try:
            if parse_gates(specs) != SELF_COUNTS:
                failures.append('parse_gates(%s) is not the counts' % label)
        except ValueError as err:
            failures.append('parse_gates(%s) raised: %s' % (label, err))
    for label, specs in (('a missing counter', full[1:]),
                         ('an unknown counter', full + ['class_mentions=1']),
                         ('a counter given twice', full + [full[0]]),
                         ('a ceiling that is not a number', full[1:] + ['legacy_sites=many']),
                         ('a negative ceiling', full[1:] + ['legacy_sites=-1']),
                         ('no equals sign', full[1:] + ['legacy_sites'])):
        try:
            parse_gates(specs)
            failures.append('parse_gates with %s did not raise' % label)
        except ValueError:
            pass

    for f in failures:
        print('self-test: ' + f)
    print('self-test: %s (%d failure(s))' % ('PASS' if not failures else 'FAIL', len(failures)))
    return 1 if failures else 0


def main(argv):
    ap = argparse.ArgumentParser(description='class_logic: the class logic left in engine files, counted and '
                                             'held under a ceiling per counter.')
    here = os.path.dirname(os.path.abspath(__file__))
    ap.add_argument('--root', default=os.path.abspath(os.path.join(here, '..', '..', '..')))
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument('--measure', action='store_true')
    g.add_argument('--check', action='store_true')
    g.add_argument('--self-test', action='store_true')
    ap.add_argument('--gate', action='append', default=[], metavar='NAME=CEILING',
                    help='with --check, once per counter: %s' % ', '.join(COUNTERS))
    args = ap.parse_args(argv[1:])
    if args.gate and not args.check:
        print('--gate is read by --check only')
        return 2
    if args.self_test:
        return self_test()
    root = os.path.abspath(args.root)
    if args.measure:
        return run_measure(root)
    try:
        gates = parse_gates(args.gate)
    except ValueError as err:
        print(err)
        return 2
    return check(root, gates)


if __name__ == '__main__':
    sys.exit(main(sys.argv))
