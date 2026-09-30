#!/usr/bin/env python3
"""cast_verbatim.py [--root <repo root>] [--base <ref>] --check | --self-test

The verbatim proof for call sites that stopped casting `this` to the character class: every
rewritten call is pasted back to the cast form it stands for, every line the rewrite added is
dropped, and the file must come back byte for byte as it was at BASE.

For each file in FILES, --check:
  1. reads the file in the working tree and at BASE (`git show <base>:<file>`);
  2. drops each of the file's ADDED lines: each is listed with the base line it follows, and
     must stand in the working tree exactly once, as a whole line, directly below that line,
     which must stand exactly once in the file at BASE;
  3. finds every call of each FORM (its direct spelling, the argument list read to the matching
     parenthesis) and writes it back as the cast call it stands for: a FORM with a `suffix` must
     end its argument list with that suffix (the argument the rewrite appended), and the suffix
     is dropped; every other argument is kept as written, in its order;
  4. requires each FORM's count to be the file's listed count, and no cast spelling of a FORM to
     be left in the working tree (a site the rewrite missed);
  5. compares the result with the file at BASE, byte for byte, and names the first difference.
Because only the call's spelling is rewritten back, a changed, swapped or dropped argument, a
dropped or moved guard, a changed line around a site, a site added or lost, an added line that
is not listed, and an added line that stands anywhere but below its listed base line all fail.

The file:line of every rewritten site, old and new, is printed with the result.

python src/tests/tools/cast_verbatim.py --check          # against BASE, this change's parent
python src/tests/tools/cast_verbatim.py --self-test      # fixtures only, no git
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
import subprocess
import sys

BASE = '26df9c56a'

# name -> the direct spelling (up to its opening parenthesis), the cast spelling it stands for,
# and the argument the rewrite appended (None: the arguments are unchanged).
FORMS = {
    'HasSpellCooldown': {'direct': 'm_spellCooldownMgr.HasSpellCooldown(',
                         'cast': '((Player*)this)->HasSpellCooldown(',
                         'suffix': ', time(NULL)'},
    'AddSpellCooldown': {'direct': 'm_spellCooldownMgr.AddSpellCooldown(',
                         'cast': '((Player*)this)->GetSpellCooldownMgr().AddSpellCooldown(',
                         'suffix': None},
}

# file -> the count of each FORM rewritten in it and the lines the rewrite added, each with the
# base line it follows.
FILES = {
    'src/game/Object/Unit.h': {
        'forms': {},
        'added': [('#include "spells/SpellCooldownMgr.h"', '#include "spells/AuraContainer.h"'),
                  ('        SpellCooldownMgr m_spellCooldownMgr;', '        AuraContainer m_auras;')]},
    'src/game/Object/Unit.cpp': {
        'forms': {},
        'added': [('    m_spellCooldownMgr(),', '    movespline(new Movement::MoveSpline()),')]},
    'src/game/Object/UnitDamage.cpp': {
        'forms': {'HasSpellCooldown': 1, 'AddSpellCooldown': 1},
        'added': []},
    'src/game/WorldHandlers/UnitAuraProcHandler.cpp': {
        'forms': {'HasSpellCooldown': 10, 'AddSpellCooldown': 8},
        'added': []},
}


def line_of(text, pos):
    return text.count('\n', 0, pos) + 1


def closing_paren(text, start):
    """The index of the ')' that closes the argument list opening just before `start`, or -1."""
    depth = 1
    i = start
    while i < len(text):
        c = text[i]
        if c == '(':
            depth += 1
        elif c == ')':
            depth -= 1
            if depth == 0:
                return i
        i += 1
    return -1


def paste_back(rel, text, spec, out):
    """(rc, text with every FORM written back, the number of calls written back)."""
    sites = 0
    for name, want in sorted(spec['forms'].items()):
        form = FORMS[name]
        if form['cast'] in text:
            out('%s: FAILED: %d call(s) still spelled %r: a site the rewrite missed'
                % (rel, text.count(form['cast']), form['cast']))
            return 1, text, sites
        found = 0
        pieces = []
        at = 0
        while True:
            hit = text.find(form['direct'], at)
            if hit < 0:
                break
            args_start = hit + len(form['direct'])
            close = closing_paren(text, args_start)
            if close < 0:
                out('%s:%d: FAILED: %s( has no closing parenthesis' % (rel, line_of(text, hit), name))
                return 1, text, sites
            args = text[args_start:close]
            if form['suffix'] is not None:
                if not args.endswith(form['suffix']):
                    out('%s:%d: FAILED: %s(%s) does not end its arguments with %r'
                        % (rel, line_of(text, hit), name, args, form['suffix']))
                    return 1, text, sites
                args = args[:-len(form['suffix'])]
            pieces.append(text[at:hit])
            pieces.append(form['cast'] + args + ')')
            sites += 1
            at = close + 1
            found += 1
        pieces.append(text[at:])
        text = ''.join(pieces)
        if found != want:
            out('%s: FAILED: %d call(s) of %s, expected %d' % (rel, found, name, want))
            return 1, text, sites
    for name in FORMS:
        if name not in spec['forms'] and FORMS[name]['direct'] in text:
            out('%s: FAILED: a call of %s in a file that lists none' % (rel, name))
            return 1, text, sites
    return 0, text, sites


def drop_added(rel, old_text, text, spec, out):
    old_lines = old_text.split('\n')
    lines = text.split('\n')
    for added, after in spec['added']:
        n = lines.count(added)
        if n != 1:
            out('%s: FAILED: the added line %r stands %d time(s), expected once' % (rel, added, n))
            return 1, text
        m = old_lines.count(after)
        if m != 1:
            out('%s: FAILED: the base line %r that %r follows stands %d time(s) at the base, expected once'
                % (rel, after, added, m))
            return 1, text
        at = lines.index(added)
        if at == 0 or lines[at - 1] != after:
            out('%s:%d: FAILED: the added line %r does not stand directly below %r'
                % (rel, at + 1, added, after))
            return 1, text
        del lines[at]
    return 0, '\n'.join(lines)


def first_difference(a, b):
    la, lb = a.split('\n'), b.split('\n')
    for i in range(max(len(la), len(lb))):
        x = la[i] if i < len(la) else '(end of file)'
        y = lb[i] if i < len(lb) else '(end of file)'
        if x != y:
            return i + 1, x, y
    return None


def verify(rel, old_text, new_text, spec, out):
    """0 when the working tree's file pastes back to the base's byte for byte, else 1."""
    rc, text = drop_added(rel, old_text, new_text, spec, out)
    if rc:
        return 1
    rc, text, sites = paste_back(rel, text, spec, out)
    if rc:
        return 1
    if text != old_text:
        diff = first_difference(old_text, text)
        if diff is None:
            out('%s: DIFFERS from the base (line endings or a trailing byte)' % rel)
        else:
            out('%s: DIFFERS from the base at line %d:\n  base:        %s\n  pasted back: %s'
                % (rel, diff[0], diff[1], diff[2]))
        return 1
    out('%s: IDENTICAL to the base, byte for byte, with %d/%d call(s) pasted back and %d added line(s) dropped'
        % (rel, sites, sum(spec['forms'].values()), len(spec['added'])))
    return 0


def site_lines(rel, old_text, new_text, spec):
    """[(name, base line, new line)] for every rewritten site, matched in order."""
    out = []
    for name in sorted(spec['forms']):
        form = FORMS[name]
        olds = [line_of(old_text, i) for i in find_all(old_text, form['cast'])]
        news = [line_of(new_text, i) for i in find_all(new_text, form['direct'])]
        out += [(name, o, n) for o, n in zip(olds, news)]
    return sorted(out, key=lambda x: x[2])


def find_all(text, needle):
    at = 0
    while True:
        hit = text.find(needle, at)
        if hit < 0:
            return
        yield hit
        at = hit + len(needle)


def check(root, base, out=print):
    rc = 0
    total = 0
    for rel, spec in FILES.items():
        with open(os.path.join(root, *rel.split('/')), encoding='utf-8', newline='') as fh:
            new_text = fh.read()
        try:
            old_text = subprocess.run(['git', '-C', root, 'show', '%s:%s' % (base, rel)], capture_output=True,
                                      check=True).stdout.decode('utf-8')
        except (OSError, subprocess.CalledProcessError) as e:
            out('%s: FAILED: cannot read it at %s from git: %s' % (rel, base, e))
            rc = 1
            continue
        got = verify(rel, old_text, new_text, spec, out)
        rc |= got
        if not got:
            for name, old_line, new_line in site_lines(rel, old_text, new_text, spec):
                out('  %s:%d -> :%d  %s' % (rel, old_line, new_line, name))
                total += 1
    out('cast_verbatim: %s (%d call site(s) in %d file(s))' % ('OK' if rc == 0 else 'FAILED', total, len(FILES)))
    return rc


SELF_OLD = '''void Unit::Proc(uint32 id, SpellEntry const* dummySpell)
{
    if (cooldown && GetTypeId() == TYPEID_PLAYER && ((Player*)this)->HasSpellCooldown(id))
    {
        return;
    }
    if (((Player*)this)->HasSpellCooldown(Pick(id, 2)))
    {
        return;
    }
    if (cooldown && GetTypeId() == TYPEID_PLAYER)
    {
        ((Player*)this)->GetSpellCooldownMgr().AddSpellCooldown(dummySpell->ID, 0, time(NULL) + cooldown);
    }
    ((Player*)this)->RemoveSpellCooldown(id);
}
'''

SELF_NEW = '''void Unit::Proc(uint32 id, SpellEntry const* dummySpell)
{
    if (cooldown && GetTypeId() == TYPEID_PLAYER && m_spellCooldownMgr.HasSpellCooldown(id, time(NULL)))
    {
        return;
    }
    if (m_spellCooldownMgr.HasSpellCooldown(Pick(id, 2), time(NULL)))
    {
        return;
    }
    if (cooldown && GetTypeId() == TYPEID_PLAYER)
    {
        m_spellCooldownMgr.AddSpellCooldown(dummySpell->ID, 0, time(NULL) + cooldown);
    }
    ((Player*)this)->RemoveSpellCooldown(id);
    int added = 1;
}
'''

SELF_SPEC = {'forms': {'HasSpellCooldown': 2, 'AddSpellCooldown': 1},
             'added': [('    int added = 1;', '    ((Player*)this)->RemoveSpellCooldown(id);')]}

SELF_DECL_OLD = '''#include "A.h"
#include "B.h"

Unit::Unit() :
    a(1),
    b(2)
{
}

class Unit
{
    public:
        int m_a;
    protected:
        int m_b;
};
'''

SELF_DECL_NEW = '''#include "A.h"
#include "N.h"
#include "B.h"

Unit::Unit() :
    a(1),
    n(),
    b(2)
{
}

class Unit
{
    public:
        int m_a;
    protected:
        int m_b;
        int m_n;
};
'''

SELF_DECL_SPEC = {'forms': {}, 'added': [('#include "N.h"', '#include "A.h"'), ('    n(),', '    a(1),'),
                                         ('        int m_n;', '        int m_b;')]}


def self_test():
    failures = []

    def run(label, want_rc, needle='', swap=None, new_text=SELF_NEW, old_text=SELF_OLD, spec=SELF_SPEC):
        if swap:
            a, b = swap
            if a not in new_text:
                failures.append('%s: the mutation %r matches nothing' % (label, a))
                print('self-test: %-66s %s' % (label, 'FAIL'))
                return
            new_text = new_text.replace(a, b, 1)
        got = []
        rc = verify('fixture', old_text, new_text, spec, got.append)
        text = '\n'.join(got)
        ok = rc == want_rc and needle in text
        print('self-test: %-66s %s' % (label, 'PASS' if ok else 'FAIL'))
        if not ok:
            failures.append('%s: rc %d (want %d)\n%s' % (label, rc, want_rc, text))

    run('the rewritten calls paste back byte for byte (3 calls)', 0,
        'IDENTICAL to the base, byte for byte, with 3/3 call(s) pasted back and 1 added line(s) dropped')
    run('an argument swapped fails', 1, 'DIFFERS from the base at line 13',
        swap=('AddSpellCooldown(dummySpell->ID, 0,', 'AddSpellCooldown(0, dummySpell->ID,'))
    run('an argument changed fails', 1, 'DIFFERS from the base at line 3',
        swap=('HasSpellCooldown(id, time(NULL))', 'HasSpellCooldown(id + 1, time(NULL))'))
    run('a guard dropped fails', 1, 'DIFFERS from the base at line 3',
        swap=('cooldown && GetTypeId() == TYPEID_PLAYER && m_spell', 'cooldown && m_spell'))
    run('a guard moved off an added call fails', 1, 'DIFFERS from the base at line 11',
        swap=('    if (cooldown && GetTypeId() == TYPEID_PLAYER)\n', '    if (cooldown)\n'))
    run('a site left as a cast fails', 1, 'still spelled',
        swap=('m_spellCooldownMgr.HasSpellCooldown(id, time(NULL))', '((Player*)this)->HasSpellCooldown(id)'))
    run('a site added fails', 1, 'call(s) of HasSpellCooldown, expected 2',
        swap=('    int added = 1;', '    int added = 1;\n    m_spellCooldownMgr.HasSpellCooldown(7, time(NULL));'))
    run('a missing clock argument fails', 1, 'does not end its arguments with',
        swap=('HasSpellCooldown(id, time(NULL))', 'HasSpellCooldown(id)'))
    run('another clock fails', 1, 'does not end its arguments with',
        swap=('HasSpellCooldown(id, time(NULL))', 'HasSpellCooldown(id, now)'))
    run('the clock appended to a form without one fails', 1, 'DIFFERS from the base at line 13',
        swap=('time(NULL) + cooldown);', 'time(NULL) + cooldown, time(NULL));'))
    run('an added line that is not listed fails', 1, 'DIFFERS from the base',
        spec={'forms': SELF_SPEC['forms'], 'added': []})
    run('a listed added line that is missing fails', 1, 'stands 0 time(s)',
        swap=('    int added = 1;\n', ''))
    run('a listed added line that stands twice fails', 1, 'stands 2 time(s)',
        swap=('    int added = 1;\n', '    int added = 1;\n    int added = 1;\n'))
    run('a call in a file that lists none of its form fails', 1, 'in a file that lists none',
        spec={'forms': {'HasSpellCooldown': 2}, 'added': SELF_SPEC['added']})
    run('a changed line away from any site fails', 1, 'DIFFERS from the base at line 1:',
        swap=('void Unit::Proc(uint32 id,', 'void Unit::Proc(uint32 id2,'))
    run('a call with no closing parenthesis fails', 1, 'has no closing parenthesis',
        swap=('AddSpellCooldown(dummySpell->ID, 0, time(NULL) + cooldown);\n    }\n'
              '    ((Player*)this)->RemoveSpellCooldown(id);\n    int added = 1;\n}\n',
              'AddSpellCooldown(dummySpell->ID\n    ((Player*)this)->RemoveSpellCooldown(id);\n'
              '    int added = 1;\n'))
    run('an added line moved fails', 1, 'does not stand directly below',
        swap=('    ((Player*)this)->RemoveSpellCooldown(id);\n    int added = 1;\n',
              '    int added = 1;\n    ((Player*)this)->RemoveSpellCooldown(id);\n'))
    run('an added line whose base line is not unique fails', 1, 'stands 2 time(s) at the base',
        old_text=SELF_OLD + '    ((Player*)this)->RemoveSpellCooldown(id);\n',
        new_text=SELF_NEW + '    ((Player*)this)->RemoveSpellCooldown(id);\n')
    run('added lines below their base lines drop out (include, initialiser, member)', 0,
        'IDENTICAL to the base, byte for byte, with 0/0 call(s) pasted back and 3 added line(s) dropped',
        new_text=SELF_DECL_NEW, old_text=SELF_DECL_OLD, spec=SELF_DECL_SPEC)
    run('an added include moved fails', 1, 'fixture:3: FAILED: the added line \'#include "N.h"\'',
        new_text=SELF_DECL_NEW.replace('#include "N.h"\n#include "B.h"\n', '#include "B.h"\n#include "N.h"\n'),
        old_text=SELF_DECL_OLD, spec=SELF_DECL_SPEC)
    run('an added initialiser moved fails', 1, "fixture:5: FAILED: the added line '    n(),'",
        new_text=SELF_DECL_NEW.replace('    a(1),\n    n(),\n', '    n(),\n    a(1),\n'),
        old_text=SELF_DECL_OLD, spec=SELF_DECL_SPEC)
    run('an added member moved into another section fails', 1, "fixture:14: FAILED: the added line '        int m_n;'",
        new_text=SELF_DECL_NEW.replace('        int m_b;\n        int m_n;\n', '        int m_b;\n').replace(
            '        int m_a;\n', '        int m_a;\n        int m_n;\n'),
        old_text=SELF_DECL_OLD, spec=SELF_DECL_SPEC)

    sites = site_lines('fixture', SELF_OLD, SELF_NEW, SELF_SPEC)
    want = [('HasSpellCooldown', 3, 3), ('HasSpellCooldown', 7, 7), ('AddSpellCooldown', 13, 13)]
    label = 'the sites are listed with their base and new lines'
    print('self-test: %-66s %s' % (label, 'PASS' if sites == want else 'FAIL'))
    if sites != want:
        failures.append('site_lines: got %r, expected %r' % (sites, want))

    for f in failures:
        print('FAILED: ' + f)
    print('self-test: %s' % ('OK' if not failures else 'FAILED'))
    return 1 if failures else 0


def main(argv):
    ap = argparse.ArgumentParser(description='The verbatim proof for call sites that stopped casting this.')
    here = os.path.dirname(os.path.abspath(__file__))
    ap.add_argument('--root', default=os.path.abspath(os.path.join(here, '..', '..', '..')))
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
