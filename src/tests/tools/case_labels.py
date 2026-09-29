#!/usr/bin/env python3
"""case_labels.py [--root <repo root>] [--baseline <file>] --check | --generate | --list | --self-test

CheckCaseLabels: the switch-aware spell-ID case-label ratchet of decoupling D11
(design/2026-09-28-unit-reopening.md, section 3(c)'s proof table, row (b), and its appendix
"Switch-aware labels"). The per-spell-ID switches inside the spell handlers move, one family per
PR, into the spell handler registry (src/game/spells/handlers/); this gate keeps the count of the
labels still in switches from growing, and makes each PR that moves a family lower it.

The lexer:
  - blanks comments, string and character literals (raw strings too), keeping line breaks;
  - tracks every `switch (<key>) {` with its key expression and the brace depth of its body;
  - gives every `case` label the key of its INNERMOST enclosing switch, so a switch nested in a
    case body owns its own labels (an effect switch inside a spell-ID case counts nothing, and a
    spell-ID switch inside an effect case counts in full).
A label counts when that key IS a spell id:
  - the key ends in an `ID` or `Id` member read (`m_spellInfo->ID`, `(*itr)->GetSpellProto()->ID`);
  - or it is a `GetId()` call (`GetId()`, `(*i)->GetId()`);
  - or it is one of SPELL_ID_LOCALS, the locals the tree switches on that hold a spell id.
Keys naming another kind of ID are EXCLUDED_KEYS (the note's `achievement->ID` and
`seTalent->ID`, plus `house->ID` and `currency->ID`). A key that only passes an id to a function
(`GetSpellSpecific(spellInfo->ID)`) is not a spell id. `enchant_spell_id` is a stat type (its
labels are ITEM_MOD_*) and is not in SPELL_ID_LOCALS.

Which files: every .h, .hpp, .cpp, .inl and .inc under src/game, except the registry's own
directory src/game/spells/handlers/ (SKIP). src/modules/SD3 is #83's scope and is not counted.

src/tests/case_labels.txt holds the count per file, "<path> <count>", paths from the repository
root. --check (the gate) fails on a file whose count grew or that the list does not hold (a new
label), on a file whose count fell below its line or is gone (a stale baseline: the PR that moves
a family lowers its line), on a malformed or duplicate line, and on a scan that reads nothing.
--generate rewrites the list from the tree; run it after moving labels, never to make an added one
pass. --list prints every counted label as "<path>:<line>: <key>".

KNOWN LIMITS, stated rather than chased: text inside `#if 0` counts; a switch whose body is not a
braced block is not tracked (its labels go to the enclosing switch); a `case` produced by a macro
is not seen. None of them hides a spell-ID label in src/game today.

python src/tests/tools/case_labels.py --check        # the gate (CheckCaseLabels.cmake runs this)
python src/tests/tools/case_labels.py --generate     # rewrite src/tests/case_labels.txt
python src/tests/tools/case_labels.py --self-test
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
import sys
import tempfile

SCOPE = 'src/game/'
SKIP = ('src/game/spells/handlers/',)
EXTENSIONS = ('.h', '.hpp', '.cpp', '.inl', '.inc')
BASELINE_REL = 'src/tests/case_labels.txt'

SPELL_ID_LOCALS = {'auraId', 'spellid', 'spellId', 'trigger_spell_id', 'triggered_spell_id'}
EXCLUDED_KEYS = {'achievement->ID', 'seTalent->ID', 'house->ID', 'currency->ID'}
ID_MEMBER = re.compile(r'.*(->|\.)\s*(ID|Id)')
GET_ID = re.compile(r'(.*(->|\.)\s*)?GetId\s*\(\s*\)')


def blank(text):
    """Comments and string/character literals as spaces; line breaks kept."""
    out = []
    i, n = 0, len(text)

    def spaces(s):
        return re.sub(r'[^\n]', ' ', s)

    while i < n:
        c = text[i]
        if c == '/' and text.startswith('//', i):
            j = text.find('\n', i)
            j = n if j < 0 else j
            out.append(spaces(text[i:j]))
            i = j
        elif c == '/' and text.startswith('/*', i):
            j = text.find('*/', i + 2)
            j = n if j < 0 else j + 2
            out.append(spaces(text[i:j]))
            i = j
        elif c == 'R' and text.startswith('R"', i) and (i == 0 or not (text[i - 1].isalnum() or text[i - 1] == '_')):
            m = re.match(r'R"([^()\\\s]{0,16})\(', text[i:])
            if not m:
                out.append(c)
                i += 1
                continue
            end = ')' + m.group(1) + '"'
            j = text.find(end, i + m.end())
            j = n if j < 0 else j + len(end)
            out.append(spaces(text[i:j]))
            i = j
        elif c == '"' or (c == "'" and not digit_separator(text, i)):
            j = i + 1
            while j < n and text[j] != c and text[j] != '\n':
                j += 2 if text[j] == '\\' else 1
            j = min(j + 1, n)
            out.append(spaces(text[i:j]))
            i = j
        else:
            out.append(c)
            i += 1
    return ''.join(out)


def digit_separator(text, i):
    """A ' inside a number (1'000), not a character literal."""
    k = i
    while k > 0 and (text[k - 1].isalnum() or text[k - 1] in "_'"):
        k -= 1
    return k < i and text[k].isdigit()


TOKEN = re.compile(r'\bswitch\b|\bcase\b|[{}]')


def labels(text):
    """[(line, key)] for every `case` label, key = the innermost enclosing switch's key (None
    outside any switch)."""
    t = blank(text)
    depth = 0
    stack = []                                                  # (key, body depth)
    found = []
    pos = 0
    while True:
        m = TOKEN.search(t, pos)
        if not m:
            break
        tok = m.group(0)
        pos = m.end()
        if tok == '{':
            depth += 1
        elif tok == '}':
            depth -= 1
            while stack and stack[-1][1] > depth:
                stack.pop()
        elif tok == 'switch':
            k = pos
            while k < len(t) and t[k].isspace():
                k += 1
            if k >= len(t) or t[k] != '(':
                continue
            level, j = 0, k
            while j < len(t):
                if t[j] == '(':
                    level += 1
                elif t[j] == ')':
                    level -= 1
                    if level == 0:
                        break
                j += 1
            key = ' '.join(t[k + 1:j].split())
            b = j + 1
            while b < len(t) and t[b].isspace():
                b += 1
            if b < len(t) and t[b] == '{':
                depth += 1
                stack.append((key, depth))
                pos = b + 1
            else:
                pos = j + 1
        else:
            found.append((t.count('\n', 0, m.start()) + 1, stack[-1][0] if stack else None))
    return found


def is_spell_id_key(key):
    if key is None or key in EXCLUDED_KEYS:
        return False
    return bool(ID_MEMBER.fullmatch(key) or GET_ID.fullmatch(key) or key in SPELL_ID_LOCALS)


def scan(root):
    """{path: [(line, key)]} of the counted labels, and the number of files read."""
    counted = {}
    files = 0
    top = os.path.join(root, *SCOPE.rstrip('/').split('/'))
    for d, dirs, names in os.walk(top):
        dirs.sort()
        for name in sorted(names):
            if not name.endswith(EXTENSIONS):
                continue
            path = os.path.join(d, name)
            rel = os.path.relpath(path, root).replace(os.sep, '/')
            if rel.startswith(SKIP):
                continue
            files += 1
            with open(path, encoding='utf-8', errors='replace', newline='') as fh:
                hits = [(line, key) for line, key in labels(fh.read()) if is_spell_id_key(key)]
            if hits:
                counted[rel] = hits
    return counted, files


def read_baseline(path):
    """({path: count}, [errors])."""
    entries, errors = {}, []
    if not os.path.isfile(path):
        return entries, ['%s does not exist' % path]
    with open(path, encoding='utf-8') as fh:
        for n, raw in enumerate(fh, 1):
            line = raw.strip()
            if not line or line.startswith('#'):
                continue
            m = re.fullmatch(r'(\S+) (\d+)', line)
            if not m:
                errors.append('%s:%d: not "<path> <count>": %s' % (path, n, line))
            elif m.group(1) in entries:
                errors.append('%s:%d: %s is listed twice' % (path, n, m.group(1)))
            else:
                entries[m.group(1)] = int(m.group(2))
    return entries, errors


def format_baseline(counts):
    head = ('# CheckCaseLabels (src/tests/tools/case_labels.py): spell-ID case labels per file under src/game,\n'
            '# outside src/game/spells/handlers/. A PR that moves labels into the registry lowers its lines\n'
            '# (--generate); a new label fails the gate. Total: %d labels in %d files.\n'
            % (sum(counts.values()), len(counts)))
    return head + ''.join('%s %d\n' % (p, counts[p]) for p in sorted(counts))


def check(root, baseline_path, out=print):
    counted, files = scan(root)
    if files == 0:
        out('CheckCaseLabels: FAILED: found no C/C++ file under %s' % SCOPE)
        return 1
    counts = dict((p, len(h)) for p, h in counted.items())
    base, errors = read_baseline(baseline_path)
    for p in sorted(set(counts) | set(base)):
        have, listed = counts.get(p, 0), base.get(p)
        if listed is None:
            errors.append('new spell-ID label(s): %s has %d, the baseline lists none' % (p, have))
            errors.extend('  %s:%d: case in switch (%s)' % (p, line, key) for line, key in counted[p])
        elif have > listed:
            errors.append('new spell-ID label(s): %s has %d, the baseline %d' % (p, have, listed))
        elif have < listed:
            errors.append('stale baseline: %s has %d, the baseline %d -- lower its line (--generate) in this PR'
                          % (p, have, listed))
    if errors:
        out('CheckCaseLabels: FAILED (%d spell-ID labels in %d files):' % (sum(counts.values()), len(counts)))
        for e in errors:
            out('  ' + e)
        return 1
    out('CheckCaseLabels: OK: %d spell-ID labels in %d files (of %d read), equal to the baseline'
        % (sum(counts.values()), len(counts), files))
    return 0


def generate(root, baseline_path, out=print):
    counted, files = scan(root)
    if files == 0:
        out('CheckCaseLabels: found no C/C++ file under %s' % SCOPE)
        return 1
    counts = dict((p, len(h)) for p, h in counted.items())
    old, _ = read_baseline(baseline_path)
    for p in sorted(set(counts) | set(old)):
        if counts.get(p, 0) != old.get(p, 0):
            out('  %s: %d -> %d' % (p, old.get(p, 0), counts.get(p, 0)))
    with open(baseline_path, 'w', encoding='utf-8', newline='\n') as fh:
        fh.write(format_baseline(counts))
    out('wrote %s: %d labels in %d files' % (baseline_path, sum(counts.values()), len(counts)))
    return 0


def self_test():
    failures = []

    def expect(cond, what):
        if not cond:
            failures.append(what)

    # The lexer, on fixtures: (text, the counted labels' lines).
    for label, text, want in [
        ('an ID-keyed switch counts every label',
         'void f() {\n switch (m_spellInfo->ID) {\n case 1: break;\n case 2: case 3: break;\n default: break;\n }\n}\n',
         [3, 4, 4]),
        ('GetId(), an Id member and a spell-id local count',
         'switch (GetId()) { case 1: break; }\nswitch (x->Id) {\n case 2: break; }\n'
         'switch (trigger_spell_id)\n{\n case 3: break;\n}\n',
         [1, 3, 6]),
        ('a nested non-ID switch owns its labels',
         'switch (GetId())\n{\n case 10:\n {\n  switch (eff)\n  {\n   case 1: break;\n   case 2: break;\n  }\n'
         '  break;\n }\n case 11: break;\n}\n',
         [3, 12]),
        ('an ID switch nested in a non-ID case counts in full',
         'switch (GetSpellFamilyName())\n{\n case SPELLFAMILY_WARRIOR:\n {\n  switch (GetId())\n  {\n'
         '   case 41099: break;\n  }\n  break;\n }\n case SPELLFAMILY_MAGE: break;\n}\n',
         [7]),
        ('another key counts nothing',
         'switch (GetEntry()) { case 1: break; }\nswitch (GetSpellSpecific(spellInfo->ID)) { case 2: break; }\n'
         'switch (achievement->ID) { case 3: break; }\nswitch (enchant_spell_id) { case 4: break; }\n',
         []),
        ('a commented or quoted label counts nothing',
         'switch (GetId())\n{\n // case 1:\n /* case 2:\n case 3: */\n case 4: s = "case 5:"; c = \'}\'; break;\n}\n'
         '// switch (GetId()) { case 6: }\n',
         [6]),
        ('a digit separator is not a quote',
         "switch (GetId())\n{\n case 1'000: break;\n case 2: break;\n}\n",
         [3, 4]),
        ('a raw string is blanked',
         'switch (GetId())\n{\n case 1: s = R"x(case 9: })x"; break;\n case 2: break;\n}\n',
         [3, 4])]:
        got = [line for line, key in labels(text) if is_spell_id_key(key)]
        ok = got == want
        print('self-test: %-58s %s' % (label, 'PASS' if ok else 'FAIL'))
        expect(ok, 'lexer: %s: got %r, expected %r' % (label, got, want))

    # The gate, on trees.
    base_tree = {
        'src/game/WorldHandlers/SpellAuraDummy.cpp':
            'void Aura::HandleAuraDummy()\n{\n    switch (GetId())\n    {\n        case 1: return;\n        case 2: return;\n    }\n}\n',
        'src/game/Object/Unit.cpp': 'void f() { switch (procSpell->ID) { case 3: break; } }\n',
        'src/game/Object/Other.cpp': 'void g() { switch (GetEntry()) { case 3: break; } }\n',
        'src/game/spells/handlers/SpellHandlerRegistry.h': 'void h() { switch (GetId()) { case 9: break; } }\n',
        'src/modules/SD3/x.cpp': 'void s() { switch (GetId()) { case 7: break; } }\n'}
    base_list = {'src/game/WorldHandlers/SpellAuraDummy.cpp': 2, 'src/game/Object/Unit.cpp': 1}

    def run(label, tree, baseline, want_rc, needles=()):
        with tempfile.TemporaryDirectory() as tmp:
            for rel, text in tree.items():
                path = os.path.join(tmp, *rel.split('/'))
                os.makedirs(os.path.dirname(path), exist_ok=True)
                with open(path, 'w', encoding='utf-8', newline='') as fh:
                    fh.write(text)
            bl = os.path.join(tmp, 'case_labels.txt')
            with open(bl, 'w', encoding='utf-8', newline='') as fh:
                fh.write(baseline if isinstance(baseline, str) else format_baseline(baseline))
            got = []
            rc = check(tmp, bl, out=got.append)
            text = '\n'.join(got)
            ok = rc == want_rc and all(n in text for n in ([needles] if isinstance(needles, str) else needles))
            print('self-test: %-58s %s' % (label, 'PASS' if ok else 'FAIL'))
            expect(ok, '%s: rc %d (want %d)\n%s' % (label, rc, want_rc, text))

    def with_file(rel, text):
        tree = dict(base_tree)
        tree[rel] = text
        return tree

    run('the baseline tree passes (handlers/ and SD3 not counted)', base_tree, base_list, 0,
        'OK: 3 spell-ID labels in 2 files')
    run('a planted new label fails',
        with_file('src/game/Object/Unit.cpp', 'void f() { switch (procSpell->ID) { case 3: break; case 4: break; } }\n'),
        base_list, 1, 'new spell-ID label(s): src/game/Object/Unit.cpp has 2, the baseline 1')
    run('a new label in a file the baseline lacks fails',
        with_file('src/game/Object/Other.cpp', 'void g() { switch (GetId()) { case 3: break; } }\n'),
        base_list, 1, ['new spell-ID label(s): src/game/Object/Other.cpp has 1, the baseline lists none',
                       'src/game/Object/Other.cpp:1: case in switch (GetId())'])
    run('a removed label with a stale baseline fails',
        with_file('src/game/WorldHandlers/SpellAuraDummy.cpp',
                  'void Aura::HandleAuraDummy()\n{\n    switch (GetId())\n    {\n        case 2: return;\n    }\n}\n'),
        base_list, 1, 'stale baseline: src/game/WorldHandlers/SpellAuraDummy.cpp has 1, the baseline 2')
    run('a baseline line for a file with no label left fails',
        with_file('src/game/Object/Unit.cpp', 'void f() {}\n'), base_list, 1,
        'stale baseline: src/game/Object/Unit.cpp has 0, the baseline 1')
    run('a label moved into spells/handlers/ with the baseline lowered passes',
        with_file('src/game/Object/Unit.cpp', 'void f() {}\n'), {'src/game/WorldHandlers/SpellAuraDummy.cpp': 2}, 0,
        'OK: 2 spell-ID labels in 1 files')
    run('a malformed baseline line fails', base_tree, format_baseline(base_list) + 'src/x.cpp\n', 1,
        'not "<path> <count>"')
    run('a line listed twice fails', base_tree, format_baseline(base_list) + 'src/game/Object/Unit.cpp 1\n', 1,
        'listed twice')
    run('a zero scan fails', {'src/README': 'x'}, {}, 1, 'found no C/C++ file')
    with tempfile.TemporaryDirectory() as tmp:
        for rel, text in base_tree.items():
            path = os.path.join(tmp, *rel.split('/'))
            os.makedirs(os.path.dirname(path), exist_ok=True)
            with open(path, 'w', encoding='utf-8', newline='') as fh:
                fh.write(text)
        bl = os.path.join(tmp, 'case_labels.txt')
        got = []
        rc = generate(tmp, bl, out=got.append)
        rc2 = check(tmp, bl, out=got.append)
        ok = rc == 0 and rc2 == 0 and read_baseline(bl)[0] == base_list
        print('self-test: %-58s %s' % ('--generate writes what --check then passes', 'PASS' if ok else 'FAIL'))
        expect(ok, 'generate/check round trip: %d %d\n%s' % (rc, rc2, '\n'.join(got)))

    for f in failures:
        print('SELF-TEST FAILED: ' + f)
    print('self-test: %s (%d failure(s))' % ('PASS' if not failures else 'FAIL', len(failures)))
    return 1 if failures else 0


def main(argv):
    ap = argparse.ArgumentParser(description='CheckCaseLabels: the spell-ID case-label ratchet (decoupling D11).')
    ap.add_argument('--root', default=os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..')))
    ap.add_argument('--baseline', help='the per-file counts (default: <root>/%s)' % BASELINE_REL)
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument('--check', action='store_true')
    g.add_argument('--generate', action='store_true')
    g.add_argument('--list', action='store_true')
    g.add_argument('--self-test', action='store_true')
    args = ap.parse_args(argv[1:])
    if args.self_test:
        return self_test()
    root = os.path.abspath(args.root)
    baseline = args.baseline or os.path.join(root, *BASELINE_REL.split('/'))
    if args.list:
        counted, _ = scan(root)
        for p in sorted(counted):
            for line, key in counted[p]:
                print('%s:%d: %s' % (p, line, key))
        print('%d labels in %d files' % (sum(len(h) for h in counted.values()), len(counted)))
        return 0
    return check(root, baseline) if args.check else generate(root, baseline)


if __name__ == '__main__':
    sys.exit(main(sys.argv))
