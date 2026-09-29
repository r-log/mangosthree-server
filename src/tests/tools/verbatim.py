#!/usr/bin/env python3
"""verbatim.py [--root <repo root>] [--base <ref>] --check | --self-test

The spell handler registry's verbatim proof (decoupling D11, design/2026-09-28-unit-reopening.md
3(b)): every case body that moved out of a per-spell-ID switch into a registered handler is pasted
back at its label, and the file must come back byte for byte as it was at BASE.

For each file in SITES, --check:
  1. reads the file at BASE (`git show <base>:<file>`) and in the working tree;
  2. drops the lines the move added (ADDED: the handlers header's include) and the handler block
     the move appended (from TAIL_MARKER to the end of the file);
  3. at each site, replaces the dispatch (the site's exact DISPATCH lines, found exactly once)
     with the switch it stood for: the switch's opening lines, then for each row of the site's
     registration table, in table order, the label line (LABELS) and -- once per run of rows that
     register the same function -- that function's body, reverse-substituted (SUBSTITUTIONS: the
     context accessors and the outcome) and re-indented to the label's body;
  4. compares the result with the file at BASE, byte for byte, and names the first difference.
Because the bodies are pasted from the functions the table registers, a row pointing at the wrong
function, a lost row, a changed line, or a body moved in the wrong order all fail.

Each handler body is also checked for what the paste-back cannot see: a live-out local used
bare (a case body's name the move did not route through the context); a member of the site's
class used bare (an implicit `this->` the move missed: in a free function the name could still
compile if a free function or global of that name exists, and then means something else) -- the
names are read from the class's own declaration (MEMBERS_OF), every member function and data
member at class scope; and a `return ...::Continue();` inside a loop or a nested switch (the
moved `break;` would have left that loop, not the case).

python src/tests/tools/verbatim.py --check        # against BASE, reading git
python src/tests/tools/verbatim.py --self-test    # fixtures only, no git
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
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from case_labels import blank  # noqa: E402  (the same comment/literal blanking as the ratchet)

# The tree the moved bodies are checked against: master before the first move (D11 PR 3).
BASE = 'afdabc428'

VOID_SUBSTITUTIONS = [('return SpellHandlerOutcome<void>::Return();', 'return;'),
                      ('return SpellHandlerOutcome<void>::Continue();', 'break;')]

# One entry per file; each file lists its sites in the order they stand in it.
SITES = {
    'src/game/WorldHandlers/SpellAuraDummy.cpp': {
        'added': ['#include "spells/handlers/AuraDummyHandlers.h"'],
        'tail_marker': '// Decoupling D11 (design/2026-09-28-unit-reopening.md 3(b)): the spell handler registry\'s',
        'sites': [{
            'name': 'HandleAuraDummy AT APPLY, SPELLFAMILY_WARRIOR (switch (GetId()))',
            'dispatch': [
                '                AuraDummyApplyContext ctx(this, target);',
                '                if (SpellHandlerRegistry::Game().Dispatch<AuraDummyApplyWarriorSite>(GetId(), ctx).IsReturn())',
                '                {',
                '                    return;',
                '                }'],
            'open': ['                switch (GetId())', '                {'],
            'close': ['                }'],
            'label_indent': 20,
            'braced': True,
            'table': 'warriorApply',
            'context': 'AuraDummyApplyContext',
            'live_outs': ['target'],
            'members_of': ('src/game/WorldHandlers/SpellAuras.h', 'Aura'),
            'substitutions': [('ctx.target', 'target'), ('ctx.aura', 'this')] + VOID_SUBSTITUTIONS,
            'labels': {
                41099: '                    case 41099:                             // Battle Stance',
                41100: '                    case 41100:                             // Berserker Stance',
                41101: '                    case 41101:                             // Defensive Stance',
                53790: '                    case 53790:                             // Defensive Stance',
                53791: '                    case 53791:                             // Berserker Stance',
                53792: '                    case 53792:                             // Battle Stance'},
        }],
    },
}


class Failure(Exception):
    pass


def table_rows(lines, table):
    """[(spell id, function)] of `static Row const <table>[] = { { id, &Function }, ... };`."""
    starts = [i for i, l in enumerate(lines) if re.fullmatch(r'\s*static Row const %s\[\] =' % re.escape(table), l)]
    if len(starts) != 1:
        raise Failure('registration table %s found %d times' % (table, len(starts)))
    rows = []
    i = starts[0] + 1
    if lines[i].strip() != '{':
        raise Failure('registration table %s: no "{" after its head' % table)
    i += 1
    while lines[i].strip() != '};':
        m = re.fullmatch(r'\s*\{ (\d+), &(\w+) \},', lines[i])
        if not m:
            raise Failure('registration table %s: not a "{ id, &Function }," row: %r' % (table, lines[i]))
        rows.append((int(m.group(1)), m.group(2)))
        i += 1
    return rows


def handler_body(lines, function, context):
    """The lines between the braces of `static SpellHandlerOutcome<...> function(context& ctx)`."""
    head = re.compile(r'static SpellHandlerOutcome<[\w:]+> %s\(%s& ctx\)' % (re.escape(function), re.escape(context)))
    starts = [i for i, l in enumerate(lines) if head.fullmatch(l)]
    if len(starts) != 1:
        raise Failure('handler %s(%s& ctx) found %d times' % (function, context, len(starts)))
    i = starts[0]
    if lines[i + 1] != '{':
        raise Failure('handler %s: no "{" on the line after its head' % function)
    j = next((k for k in range(i + 2, len(lines)) if lines[k] == '}'), None)
    if j is None:
        raise Failure('handler %s: no closing "}" in column 0' % function)
    return lines[i + 2:j]


def class_members(header, cls):
    """The names class `cls` declares at class scope in `header` (its text): member functions and
    data members, not its constructors, friends, nested types or what inline bodies call."""
    t = blank(header)
    m = re.search(r'\bclass\s+%s\b[^;{]*\{' % re.escape(cls), t)
    if not m:
        raise Failure('class %s not found in its header' % cls)
    names, depth, stmt, i = set(), 1, [], m.end()

    def declared(text):
        text = re.sub(r'^\s*((public|protected|private)\s*:\s*)+', '', text).strip()
        if not text or re.match(r'(friend|typedef|using|enum|struct|class|union)\b', text):
            return None
        if '(' in text:
            head = text[:text.index('(')]
            found = re.findall(r'~?\w+', head)
            name = found[-1] if found else None
            if name in (cls, '~' + cls) or (name and name.startswith('operator')):
                return None
            return name
        text = re.split(r'=|\[|:(?!:)', text)[0]
        found = re.findall(r'\w+', text)
        return found[-1] if found else None

    while i < len(t) and depth > 0:
        c = t[i]
        if c == '{':
            if depth == 1:
                name = declared(''.join(stmt))
                if name:
                    names.add(name)
                stmt = []
            depth += 1
        elif c == '}':
            depth -= 1
        elif depth == 1:
            if c == ';':
                name = declared(''.join(stmt))
                if name:
                    names.add(name)
                stmt = []
            else:
                stmt.append(c)
        i += 1
    if not names:
        raise Failure('class %s declares no member the check could read' % cls)
    return names


def check_body(function, body, site, members=()):
    """What the paste-back cannot see: a bare live-out, a bare member of the site's class, and a
    Continue inside a loop or switch."""
    text = blank('\n'.join(body))
    for n, line in enumerate(text.split('\n'), 1):
        for name in sorted(members):
            if re.search(r'(?<![\w.>:~])%s\b' % re.escape(name), line):
                raise Failure('handler %s, body line %d: "%s", a member of %s, used bare (an implicit this-> the '
                              'move missed): %r' % (function, n, name, site['members_of'][1], body[n - 1]))
    for name in site['live_outs']:
        for n, line in enumerate(text.split('\n'), 1):
            if re.search(r'(?<![\w.>])%s\b' % re.escape(name), line.replace('ctx.' + name, '')):
                raise Failure('handler %s, body line %d: live-out "%s" used without the context: %r'
                              % (function, n, name, body[n - 1]))
    continues = [a for a, b in site['substitutions'] if b == 'break;']
    stack, pending, header_parens = [], False, None
    tokens = re.compile(r'\b(for|while|do|switch)\b|[{}();]|' + '|'.join(re.escape(c) for c in continues))
    pos = 0
    while True:
        m = tokens.search(text, pos)
        if not m:
            break
        tok = m.group(0)
        pos = m.end()
        if tok in continues:
            if pending or any(stack):
                line = text.count('\n', 0, m.start())
                raise Failure('handler %s, body line %d: a Continue inside a loop or switch (the old `break;` left '
                              'that, not the case): %r' % (function, line + 1, body[line]))
        elif tok in ('for', 'while', 'switch'):
            header_parens = 0
        elif tok == 'do':
            pending = True
        elif tok == '(' and header_parens is not None:
            header_parens += 1
        elif tok == ')' and header_parens is not None:
            header_parens -= 1
            if header_parens == 0:
                header_parens = None
                pending = True
        elif tok == '{':
            stack.append(pending)
            pending = False
        elif tok == '}':
            if stack:
                stack.pop()
        elif tok == ';' and header_parens is None:
            pending = False


def rebuild(old_text, new_text, spec, headers):
    """The new file with every site's dispatch replaced by its switch, the added lines and the
    appended handler block dropped; and the number of bodies pasted back. `headers` maps a
    site's members_of header path to its text."""
    new = new_text.split('\n')
    marker = [i for i, l in enumerate(new) if l == spec['tail_marker']]
    if len(marker) != 1:
        raise Failure('tail marker found %d times' % len(marker))
    handlers = new[marker[0]:]
    rest = new[:marker[0]]
    # the block ends the file after one blank line: what is left ends in "\n", as the old file did
    if not rest or rest[-1] != '':
        raise Failure('no blank line before the tail marker')
    for added in spec['added']:
        at = [i for i, l in enumerate(rest) if l == added]
        if len(at) != 1:
            raise Failure('added line %r found %d times' % (added, len(at)))
        del rest[at[0]]
    pasted = 0
    for site in spec['sites']:
        n = len(site['dispatch'])
        at = [i for i in range(len(rest) - n + 1) if rest[i:i + n] == site['dispatch']]
        if len(at) != 1:
            raise Failure('%s: the dispatch found %d times' % (site['name'], len(at)))
        rows = table_rows(handlers, site['table'])
        ids = [r[0] for r in rows]
        if sorted(ids) != sorted(site['labels']) or len(set(ids)) != len(ids):
            raise Failure('%s: table rows %s, labels %s' % (site['name'], ids, sorted(site['labels'])))
        switch = list(site['open'])
        indent = ' ' * site['label_indent']
        i = 0
        while i < len(rows):
            function = rows[i][1]
            j = i
            while j < len(rows) and rows[j][1] == function:
                switch.append(site['labels'][rows[j][0]])
                j += 1
            if function in [r[1] for r in rows[:i]]:
                raise Failure('%s: %s registered in two runs of rows (its labels were not together)'
                              % (site['name'], function))
            body = handler_body(handlers, function, site['context'])
            header, cls = site['members_of']
            check_body(function, body, site, class_members(headers[header], cls))
            restored = []
            for line in body:
                for a, b in site['substitutions']:
                    line = line.replace(a, b)
                restored.append(indent + (' ' * 4 if not site['braced'] else '') + line if line else line)
            if site['braced']:
                switch += [indent + '{'] + restored + [indent + '}']
            else:
                switch += restored
            pasted += 1
            i = j
        switch += site['close']
        rest[at[0]:at[0] + n] = switch
    return '\n'.join(rest), pasted


def first_difference(a, b):
    al, bl = a.split('\n'), b.split('\n')
    for n, (x, y) in enumerate(zip(al, bl), 1):
        if x != y:
            return 'line %d: base %r, rebuilt %r' % (n, x, y)
    return 'lengths differ: base %d lines, rebuilt %d' % (len(al), len(bl))


def verify(rel, old_text, new_text, spec, headers, out=print):
    try:
        rebuilt, pasted = rebuild(old_text, new_text, spec, headers)
    except Failure as e:
        out('%s: FAILED: %s' % (rel, e))
        return 1, 0, 0
    total = sum(len(set(r[1] for r in table_rows(new_text.split('\n'), s['table']))) for s in spec['sites'])
    if rebuilt != old_text:
        out('%s: DIFFERS from the base after pasting back %d/%d bodies: %s' % (rel, pasted, total,
                                                                              first_difference(old_text, rebuilt)))
        return 1, pasted, total
    out('%s: IDENTICAL to the base, byte for byte, with %d/%d bodies pasted back at their %d labels' % (
        rel, pasted, total, sum(len(s['labels']) for s in spec['sites'])))
    return 0, pasted, total


def check(root, base, out=print):
    rc = 0
    for rel, spec in SITES.items():
        path = os.path.join(root, *rel.split('/'))
        with open(path, encoding='utf-8', newline='') as fh:
            new_text = fh.read()
        try:
            old_text = subprocess.run(['git', '-C', root, 'show', '%s:%s' % (base, rel)], capture_output=True,
                                      check=True).stdout.decode('utf-8')
        except (OSError, subprocess.CalledProcessError) as e:
            out('%s: FAILED: cannot read it at %s from git: %s' % (rel, base, e))
            rc = 1
            continue
        headers = {}
        for site in spec['sites']:
            header = site['members_of'][0]
            with open(os.path.join(root, *header.split('/')), encoding='utf-8', newline='') as fh:
                headers[header] = fh.read()
        got, _, _ = verify(rel, old_text, new_text, spec, headers, out)
        rc |= got
    out('verbatim: %s' % ('OK' if rc == 0 else 'FAILED'))
    return rc


# ---- Self-test fixtures: a site with a shared body, a Continue, and a loop's own break. ----
SELF_OLD = '''#include "A.h"

void Thing::Handle(bool apply)
{
    Unit* target = GetTarget();
    switch (GetId())
    {
        case 1:                                 // One
        case 2:                                 // Two
        {
            target->Cast(this);
            return;
        }
        case 3:                                 // Three
        {
            for (int i = 0; i < 3; ++i)
            {
                if (i == 1)
                    break;
            }
            break;
        }
    }
    target->Tail();
}
'''

SELF_NEW = '''#include "A.h"
#include "Handlers.h"

void Thing::Handle(bool apply)
{
    Unit* target = GetTarget();
    SelfContext handlerContext(this, target);
    if (Dispatch(handlerContext).IsReturn())
    {
        return;
    }
    target->Tail();
}

// Handlers:

static SpellHandlerOutcome<void> One(SelfContext& ctx)
{
    ctx.target->Cast(ctx.aura);
    return SpellHandlerOutcome<void>::Return();
}

static SpellHandlerOutcome<void> Three(SelfContext& ctx)
{
    for (int i = 0; i < 3; ++i)
    {
        if (i == 1)
            break;
    }
    return SpellHandlerOutcome<void>::Continue();
}

void Register(Registry& registry)
{
    static Row const rows[] =
    {
        { 1, &One },
        { 2, &One },
        { 3, &Three },
    };
}
'''

SELF_SPEC = {
    'added': ['#include "Handlers.h"'],
    'tail_marker': '// Handlers:',
    'sites': [{
        'name': 'fixture',
        'dispatch': ['    SelfContext handlerContext(this, target);', '    if (Dispatch(handlerContext).IsReturn())',
                     '    {', '        return;', '    }'],
        'open': ['    switch (GetId())', '    {'],
        'close': ['    }'],
        'label_indent': 8,
        'braced': True,
        'table': 'rows',
        'context': 'SelfContext',
        'live_outs': ['target'],
        'members_of': ('Thing.h', 'Thing'),
        'substitutions': [('ctx.target', 'target'), ('ctx.aura', 'this')] + VOID_SUBSTITUTIONS,
        'labels': {1: '        case 1:                                 // One',
                   2: '        case 2:                                 // Two',
                   3: '        case 3:                                 // Three'},
    }],
}


SELF_HEADERS = {'Thing.h': """class Other { void Tail(); };
class  Thing
{
        friend struct Helper;
    public:
        Thing(int x) : m_x(x) {}
        ~Thing();
        void Handle(bool apply);
        Unit* GetCaster() const { return Lookup(m_casterGuid); }   // Lookup is not a member
        bool IsPositive() { return m_positive; }
        int& operator[](int i);
    protected:
        Modifier m_modifier;
        bool m_positive : 1;
        int m_table[4];
        enum { KIND_A, KIND_B };
    private:
        uint64 m_casterGuid = 0;
};
"""}


def self_test():
    failures = []
    got = sorted(class_members(SELF_HEADERS['Thing.h'], 'Thing'))
    want = ['GetCaster', 'Handle', 'IsPositive', 'm_casterGuid', 'm_modifier', 'm_positive', 'm_table']
    print('self-test: %-58s %s' % ('the class-scope member names are read', 'PASS' if got == want else 'FAIL'))
    if got != want:
        failures.append('class_members: got %r, expected %r' % (got, want))

    def run(label, new_text, want_rc, needle=''):
        got = []
        rc, _, _ = verify('fixture', SELF_OLD, new_text, SELF_SPEC, SELF_HEADERS, out=got.append)
        text = '\n'.join(got)
        ok = rc == want_rc and needle in text
        print('self-test: %-58s %s' % (label, 'PASS' if ok else 'FAIL'))
        if not ok:
            failures.append('%s: rc %d (want %d)\n%s' % (label, rc, want_rc, text))

    run('the moved bodies paste back byte for byte (3/3 labels)', SELF_NEW, 0,
        'IDENTICAL to the base, byte for byte, with 2/2 bodies pasted back at their 3 labels')
    run('a changed body line fails', SELF_NEW.replace('if (i == 1)', 'if (i == 2)'), 1, 'DIFFERS')
    run('two rows swapped fail', SELF_NEW.replace('        { 2, &One },\n        { 3, &Three },',
                                                  '        { 3, &Three },\n        { 2, &One },'), 1,
        'registered in two runs of rows')
    run('a row pointing at the wrong body fails', SELF_NEW.replace('{ 2, &One }', '{ 2, &Three }'), 1, 'DIFFERS')
    run('a lost row fails', SELF_NEW.replace('        { 2, &One },\n', ''), 1, 'table rows [1, 3]')
    run('a bare live-out in a body fails', SELF_NEW.replace('ctx.target->Cast', 'target->Cast'), 1,
        'live-out "target" used without the context')
    run('a bare member of the site\'s class in a body fails',
        SELF_NEW.replace('ctx.target->Cast(ctx.aura);', 'ctx.target->Cast(GetCaster());'), 1,
        '"GetCaster", a member of Thing, used bare')
    run('a member reached through the context passes the member check',
        SELF_NEW.replace('ctx.target->Cast(ctx.aura);', 'ctx.target->Cast(ctx.aura->GetCaster());'), 1, 'DIFFERS')
    run('a Continue inside a loop fails', SELF_NEW.replace('if (i == 1)\n            break;',
                                                           'if (i == 1)\n            return SpellHandlerOutcome<void>::Continue();'),
        1, 'a Continue inside a loop or switch')
    run('a lost dispatch fails', SELF_NEW.replace('    if (Dispatch(handlerContext).IsReturn())\n', ''), 1,
        'the dispatch found 0 times')
    run('Return taken for Continue fails', SELF_NEW.replace('return SpellHandlerOutcome<void>::Continue();',
                                                            'return SpellHandlerOutcome<void>::Return();'), 1,
        'DIFFERS')
    for f in failures:
        print('SELF-TEST FAILED: ' + f)
    print('self-test: %s (%d failure(s))' % ('PASS' if not failures else 'FAIL', len(failures)))
    return 1 if failures else 0


def main(argv):
    ap = argparse.ArgumentParser(description='The spell handler registry\'s verbatim proof (decoupling D11).')
    ap.add_argument('--root', default=os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..')))
    ap.add_argument('--base', default=BASE)
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument('--check', action='store_true')
    g.add_argument('--self-test', action='store_true')
    args = ap.parse_args(argv[1:])
    if args.self_test:
        return self_test()
    return check(os.path.abspath(args.root), args.base)


if __name__ == '__main__':
    sys.exit(main(sys.argv))
