#!/usr/bin/env python3
"""movement_writers.py [--root <repo root>] --check | --list | --self-test: a unit's movement record has one owner.

The record (Unit's m_movementInfo) is named only by Unit.h, where it is declared private, and by its
owner file, UnitMovementStatus.cpp. Everything else reads it through Unit::ClientStatus() and
Unit::WireStatus() and writes it through the owner's operations, each called only from the files of
its row in OPERATIONS. MovementInfo befriends only the two wire bridge functions, and no const_cast
names a MovementInfo.

Not caught: a pointer or reference to the record handed out by the owner file, and a macro that
spells the member's name.
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

SCOPE = ('src/game/', 'src/tests/', 'src/modules/', 'src/mangosd/')
EXTENSIONS = ('.h', '.hpp', '.cpp', '.inl', '.inc')
OWNER_HEADER = 'src/game/Object/Unit.h'
OWNER_FILE = 'src/game/Object/UnitMovementStatus.cpp'
MEMBER = 'm_movementInfo'
OPERATIONS = (
    ('AcceptClientStatus', ('src/game/session/handlers/entities/MovementHandlers.cpp', 'src/game/Harness/', 'src/tests/')),
    ('ResetClientWord', ('src/game/spells/auras/SpellAuraControl.cpp', 'src/game/Object/UnitSpeed.cpp',
                         'src/game/entities/player/Player.cpp', 'src/tests/')),
    ('ClearClientFall', ('src/game/entities/player/Player.cpp',)),
    ('SetTransportStatus', ('src/game/Object/Vehicle.cpp', 'src/game/entities/player/persistence/PlayerLoadFromDB.cpp')),
    ('ClearTransportStatus', ('src/game/Object/Vehicle.cpp', 'src/game/entities/player/Player.cpp',
                              'src/game/entities/player/persistence/PlayerLoadFromDB.cpp')),
)
FRIENDS = ('Movement::ToWire', 'Movement::FromWire')

MEMBER_TOKEN = re.compile(r'\b%s\b' % MEMBER)
ACCESS = re.compile(r'^\s*(public|protected|private)\s*:', re.M)
CLASS_HEAD = re.compile(r'\bclass\s+MovementInfo\s*\{')
FRIEND = re.compile(r'\bfriend\b([^;]*);')


def line_of(text, pos):
    return text.count('\n', 0, pos) + 1


def read_tree(root):
    """{relative path: code with comments and literals blanked} of every source file in SCOPE."""
    files = {}
    for scope in SCOPE:
        top = os.path.join(root, *scope.rstrip('/').split('/'))
        for d, dirs, names in os.walk(top):
            dirs.sort()
            for name in sorted(names):
                if not name.endswith(EXTENSIONS):
                    continue
                path = os.path.join(d, name)
                rel = os.path.relpath(path, root).replace(os.sep, '/')
                with open(path, encoding='utf-8', errors='replace', newline='') as fh:
                    files[rel] = case_labels.blank(fh.read())
    return files


def allowed(rel, places):
    return any(rel == p or (p.endswith('/') and rel.startswith(p)) for p in places)


def calls(code, name):
    """Lines of the calls to `name` in `code`; a definition (`Unit::name(`) is not a call."""
    found = []
    for m in re.finditer(r'\b%s\s*\(' % name, code):
        before = code[:m.start()].rstrip()
        if before.endswith('::'):
            continue
        found.append(line_of(code, m.start()))
    return found


def check(root, out=print):
    files = read_tree(root)
    if not files:
        out('MovementWriters: read no file under %s -- a gate that scans nothing passes nothing' % root)
        return 1
    failed = []
    header = files.get(OWNER_HEADER)
    owner = files.get(OWNER_FILE)
    if header is None or owner is None:
        out('MovementWriters: %s or %s is missing -- the owner moved, and the gate must move with it'
            % (OWNER_HEADER, OWNER_FILE))
        return 1

    declared = list(MEMBER_TOKEN.finditer(header))
    if len(declared) != 1:
        failed.append('%s: %s is named %d times; the header declares it once and names it nowhere else'
                      % (OWNER_HEADER, MEMBER, len(declared)))
    else:
        specs = [m for m in ACCESS.finditer(header) if m.start() < declared[0].start()]
        if not specs or specs[-1].group(1) != 'private':
            failed.append('%s:%d: %s is not under private:' % (OWNER_HEADER, line_of(header, declared[0].start()), MEMBER))

    for rel in sorted(files):
        if rel in (OWNER_HEADER, OWNER_FILE):
            continue
        for m in MEMBER_TOKEN.finditer(files[rel]):
            failed.append('%s:%d: names %s; read it through ClientStatus() or WireStatus(), write it through an '
                          'owner operation' % (rel, line_of(files[rel], m.start()), MEMBER))

    counts = []
    for name, places in OPERATIONS:
        hits = {}
        for rel in sorted(files):
            if rel in (OWNER_HEADER, OWNER_FILE):
                continue
            lines = calls(files[rel], name)
            if not lines:
                continue
            if allowed(rel, places):
                for place in places:
                    if allowed(rel, (place,)):
                        hits[place] = hits.get(place, 0) + len(lines)
            else:
                for ln in lines:
                    failed.append('%s:%d: calls %s, which only %s may call' % (rel, ln, name, ', '.join(places)))
        for place in places:
            if not hits.get(place):
                failed.append('%s: no call to %s from %s -- the row is stale; remove the place' % (name, name, place))
        counts.append((name, sum(hits.values())))

    heads = []
    for rel in sorted(files):
        if rel.endswith(('.h', '.hpp')):
            for m in CLASS_HEAD.finditer(files[rel]):
                heads.append((rel, m))
    if len(heads) != 1:
        failed.append('MovementInfo: %d class definitions found, expected one' % len(heads))
    else:
        rel, m = heads[0]
        code = files[rel]
        depth, i = 0, m.end() - 1
        while i < len(code):
            if code[i] == '{':
                depth += 1
            elif code[i] == '}':
                depth -= 1
                if depth == 0:
                    break
            i += 1
        body = code[m.end():i]
        friends = [f.group(1).strip() for f in FRIEND.finditer(body)]
        for f in friends:
            if not any(name in f for name in FRIENDS):
                failed.append('%s: MovementInfo befriends %s; only %s may be its friends' % (rel, f, ' and '.join(FRIENDS)))

    for rel in sorted(files):
        for ln_no, ln in enumerate(files[rel].split('\n'), 1):
            if 'const_cast' in ln and ('MovementInfo' in ln or MEMBER in ln):
                failed.append('%s:%d: a const_cast names a MovementInfo' % (rel, ln_no))

    if failed:
        for f in failed:
            out(f)
        return 1
    out('MovementWriters OK: %d files; %s private to Unit, named only by its owner; %s'
        % (len(files), MEMBER, ', '.join('%s %d' % c for c in counts)))
    return 0


def self_test():
    failures = []

    UNIT_H = 'class Unit\n{\n    public:\n        void AcceptClientStatus(int s);\n    private:\n        MovementInfo m_movementInfo;\n};\n'
    INFO_H = ('class MovementInfo\n{\n    public:\n        friend Wire::MovementStatus Movement::ToWire(MovementInfo const&);\n'
              '        friend void Movement::FromWire(Wire::MovementStatus const&, MovementInfo&);\n};\n')
    OWNER = 'void Unit::AcceptClientStatus(int s) { m_movementInfo = s; }\n'
    base = {
        OWNER_HEADER: UNIT_H,
        OWNER_FILE: OWNER,
        'src/game/movement/MovementInfo.h': INFO_H,
        'src/game/session/handlers/entities/MovementHandlers.cpp': 'void h(Unit* u) { u->AcceptClientStatus(1); }\n',
        'src/game/Harness/S.cpp': 'void s(Unit* u) { u->AcceptClientStatus(2); }\n',
        'src/tests/T.cpp': 'void t(Unit& u) { u.AcceptClientStatus(3); u.ResetClientWord(0); }\n',
        'src/game/spells/auras/SpellAuraControl.cpp': 'void a(Unit* u) { u->ResetClientWord(0); }\n',
        'src/game/Object/UnitSpeed.cpp': 'void Unit::f() { ResetClientWord(0); }\n',
        'src/game/entities/player/Player.cpp': 'void Player::g() { ResetClientWord(0); ClearClientFall(); ClearTransportStatus(); }\n',
        'src/game/Object/Vehicle.cpp': 'void v(Unit* u) { u->SetTransportStatus(1); u->ClearTransportStatus(); }\n',
        'src/game/entities/player/persistence/PlayerLoadFromDB.cpp': 'void l() { SetTransportStatus(1); ClearTransportStatus(); }\n',
    }

    def tree(files):
        root = tempfile.mkdtemp(prefix='movement_writers_')
        for rel, text in files.items():
            path = os.path.join(root, *rel.split('/'))
            os.makedirs(os.path.dirname(path), exist_ok=True)
            with open(path, 'w', encoding='utf-8', newline='') as fh:
                fh.write(text)
        return root

    def expect(label, files, want):
        lines = []
        root = tree(files)
        try:
            got = check(root, out=lines.append)
        finally:
            shutil.rmtree(root, ignore_errors=True)
        if got != want:
            failures.append('%s: rc %d, expected %d (%s)' % (label, got, want, ' | '.join(lines)))

    expect('the owner alone passes', base, 0)
    expect('a planted write fails', dict(base, **{'src/game/Object/Creature.cpp': 'void c() { m_movementInfo.SetMovementFlags(0); }\n'}), 1)
    expect('a comment mention passes', dict(base, **{'src/game/Object/Creature.cpp': '// m_movementInfo is private now\nint x;\n'}), 0)
    expect('a public member fails', dict(base, **{OWNER_HEADER: UNIT_H.replace('    private:\n', '    public:\n')}), 1)
    expect('a second mention in the header fails', dict(base, **{OWNER_HEADER: UNIT_H.replace('};', '        int f() { return m_movementInfo.x; }\n};')}), 1)
    expect('an operation called from elsewhere fails', dict(base, **{'src/game/Object/Pet.cpp': 'void p(Unit* u) { u->ResetClientWord(0); }\n'}), 1)
    expect('a stale row fails', dict(base, **{'src/game/Object/UnitSpeed.cpp': 'void Unit::f() {}\n'}), 1)
    expect('a third friend fails', dict(base, **{'src/game/movement/MovementInfo.h': INFO_H.replace('};', '        friend class Unit;\n};')}), 1)
    expect('a const_cast fails', dict(base, **{'src/game/Object/ObjectUpdate.cpp': 'void o(Unit const* u) { MovementInfo& m = const_cast<MovementInfo&>(u->ClientStatus()); }\n'}), 1)
    expect('a missing owner file fails', {k: v for k, v in base.items() if k != OWNER_FILE}, 1)
    expect('no MovementInfo class fails', {k: v for k, v in base.items() if k != 'src/game/movement/MovementInfo.h'}, 1)
    expect('an empty scan fails', {'src/other/A.cpp': 'int x;\n'}, 1)

    for f in failures:
        print('self-test: ' + f)
    print('self-test: %s (%d failure(s))' % ('PASS' if not failures else 'FAIL', len(failures)))
    return 1 if failures else 0


def main(argv):
    ap = argparse.ArgumentParser(description='CheckMovementWriters: a unit\'s movement record has one owner.')
    here = os.path.dirname(os.path.abspath(__file__))
    ap.add_argument('--root', default=os.path.abspath(os.path.join(here, '..', '..', '..')))
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument('--check', action='store_true')
    g.add_argument('--list', action='store_true')
    g.add_argument('--self-test', action='store_true')
    args = ap.parse_args(argv[1:])
    if args.self_test:
        return self_test()
    root = os.path.abspath(args.root)
    if args.list:
        files = read_tree(root)
        for name, places in OPERATIONS:
            for rel in sorted(files):
                for ln in calls(files[rel], name):
                    if rel not in (OWNER_HEADER, OWNER_FILE):
                        print('%s %s:%d' % (name, rel, ln))
        return 0
    return check(root)


if __name__ == '__main__':
    sys.exit(main(sys.argv))
