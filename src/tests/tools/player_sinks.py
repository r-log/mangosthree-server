#!/usr/bin/env python3
"""player_sinks.py [--root <repo root>] --check | --self-test: every player gets the session's callbacks, and
the player's two pointers (its group callbacks, its spell modifiers) are set last in its constructor and cleared
last in its destructor.

Every `new Player(` under src/ -- placement (`new (std::nothrow) Player(`), qualified (`new ::Player(`) and
make_unique/make_shared<Player> included, the SD3 scripts too -- assigns the player to a variable, and
`InstallPlayerPacketSinks(*<variable>)` is called on one of the next WINDOW lines; an install on the line
of the `new` itself does not count and fails.
The number of creation sites is fixed at SITES: a new site fails until it installs the callbacks and SITES is
raised with it, and a removed site fails until SITES is lowered.

In src/game/entities/player/Player.cpp the last two statements of the body of `Player::Player(` (its
definition header at column 0, the body read by brace matching from the `{` that follows) are `m_groupCallbacks
= &m_groupSinks;` and `m_spellMods = &m_spellModMgr;`, in either order, and the last two statements of
`Player::~Player(`'s body are `m_groupCallbacks = NULL;` and `m_spellMods = NULL;`, in either order: after them
only whitespace and the body's closing brace at column 0 follow. No other `m_groupCallbacks =` or `m_spellMods =`
stands under src/; an initialiser `m_groupCallbacks(NULL)` is not an assignment. Comments and literals are ignored.

Not caught: a player created through a macro or a factory function, and an install made on another path
before the player is used.
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

SCOPE = 'src/'
EXCLUDED = ('src/tests/tools/',)
EXTENSIONS = ('.h', '.hpp', '.cpp', '.inl', '.inc')
SITES = 3
WINDOW = 3

CREATE = re.compile(r'\bnew\b\s*(?:\([^()]*\)\s*)?(?:::\s*)?Player\s*\(|\bmake_(?:unique|shared)\s*<\s*Player\s*>\s*\(')
ASSIGNED = re.compile(r'(\w+)\s*(?:=|\()\s*(?:std\s*::\s*)?$')

PLAYER_CPP = 'src/game/entities/player/Player.cpp'
POINTER = re.compile(r'\b(?:m_groupCallbacks|m_spellMods)\s*=(?!=)')
PINS = (
    ('Player::Player(', (
        (re.compile(r'\bm_groupCallbacks\s*=\s*&\s*m_groupSinks\s*;'), 'm_groupCallbacks = &m_groupSinks;'),
        (re.compile(r'\bm_spellMods\s*=\s*&\s*m_spellModMgr\s*;'), 'm_spellMods = &m_spellModMgr;'))),
    ('Player::~Player(', (
        (re.compile(r'\bm_groupCallbacks\s*=\s*NULL\s*;'), 'm_groupCallbacks = NULL;'),
        (re.compile(r'\bm_spellMods\s*=\s*NULL\s*;'), 'm_spellMods = NULL;'))),
)


def sites(text):
    """[(line, installed)] for each player creation, comments and literals ignored."""
    clean = case_labels.blank(text)
    lines = clean.split('\n')
    found = []
    for m in CREATE.finditer(clean):
        line = clean.count('\n', 0, m.start()) + 1
        start = max(clean.rfind(c, 0, m.start()) for c in ';{}') + 1
        var = ASSIGNED.search(clean[start:m.start()])
        installed = False
        if var:
            install = re.compile(r'\bInstallPlayerPacketSinks\s*\(\s*\*\s*%s\s*\)' % re.escape(var.group(1)))
            installed = any(install.search(text_line) for text_line in lines[line:line + WINDOW])
        found.append((line, installed))
    return found


def pins(text):
    """([(line, message)] failures, {lines of the pinned statements that stand last}) for Player.cpp's text."""
    clean = case_labels.blank(text)
    failures = []
    pinned = set()
    for header, statements in PINS:
        name = header[:-1]
        m = re.search(r'^' + re.escape(header), clean, re.M)
        if not m:
            failures.append((1, 'no definition header `%s` at column 0' % header))
            continue
        head_line = clean.count('\n', 0, m.start()) + 1
        opened = clean.find('{', m.end())
        closed = -1
        depth = 0
        for i in range(max(opened, 0), len(clean)):
            if clean[i] == '{':
                depth += 1
            elif clean[i] == '}':
                depth -= 1
                if depth == 0:
                    closed = i
                    break
        if opened < 0 or closed < 0:
            failures.append((head_line, '%s has no brace-matched body' % name))
            continue
        if clean[closed - 1] != '\n':
            failures.append((clean.count('\n', 0, closed) + 1,
                             "the closing brace of %s's body is not at column 0" % name))
            continue
        lasts = []
        for statement, spelled in statements:
            found = list(statement.finditer(clean, opened + 1, closed))
            if not found:
                failures.append((head_line, "%s's body does not end with `%s`" % (name, spelled)))
            else:
                lasts.append((found[-1], spelled))
        end = closed
        tail = []
        broken = False
        for last, spelled in sorted(lasts, key=lambda x: -x[0].start()):
            line = clean.count('\n', 0, last.start()) + 1
            if broken or clean[last.end():end].strip():
                broken = True
                failures.append((line, "`%s` is not among the last statements of %s's body; set it last"
                                 % (spelled, name)))
            else:
                tail.append(line)
            end = last.start()
        before = clean[opened + 1:end].rstrip()
        if before and before[-1] not in ';{}':
            failures.append((clean.count('\n', 0, end) + 1,
                             "the last statements of %s's body are not statements of their own" % name))
        else:
            pinned.update(tail)
    return failures, pinned


def assignments(text):
    """[line] of every assignment to m_groupCallbacks or m_spellMods, comments and literals ignored."""
    clean = case_labels.blank(text)
    return [clean.count('\n', 0, m.start()) + 1 for m in POINTER.finditer(clean)]


def sources(root):
    """(path, text) of every scanned file under src/."""
    top = os.path.join(root, *SCOPE.rstrip('/').split('/'))
    for d, dirs, names in os.walk(top):
        dirs.sort()
        for name in sorted(names):
            if not name.endswith(EXTENSIONS):
                continue
            path = os.path.join(d, name)
            rel = os.path.relpath(path, root).replace(os.sep, '/')
            if rel.startswith(EXCLUDED):
                continue
            with open(path, encoding='utf-8', errors='replace', newline='') as fh:
                yield rel, fh.read()


def scan(root):
    """[(path, line, installed)] of every creation under src/, the number of files read, and
    [(path, line, message)] of every player pointer failure."""
    out = []
    files = 0
    pointer = []
    player_cpp = None
    assigned = []
    for rel, text in sources(root):
        files += 1
        for line, installed in sites(text):
            out.append((rel, line, installed))
        if rel == PLAYER_CPP:
            player_cpp = text
        for line in assignments(text):
            assigned.append((rel, line))
    pinned = set()
    if player_cpp is None:
        if files:
            pointer.append((PLAYER_CPP, 0, 'not found; the player pointers cannot be checked'))
    else:
        failures, pinned = pins(player_cpp)
        pointer.extend((PLAYER_CPP, line, message) for line, message in failures)
    for rel, line in assigned:
        if rel != PLAYER_CPP or line not in pinned:
            pointer.append((rel, line, 'a player pointer is assigned here; only the last statements of '
                            "Player::Player's and Player::~Player's bodies assign it"))
    return out, files, pointer


def check(root, out=print, expected=SITES):
    found, files, pointer = scan(root)
    if files == 0:
        out('PlayerSinks: read no file under %s/%s -- a gate that scans nothing passes nothing' % (root, SCOPE))
        return 1
    failed = False
    for rel, line, installed in found:
        if not installed:
            out('%s:%d: a player is created without InstallPlayerPacketSinks(*<it>) in the next %d lines; '
                'call it right after the construction' % (rel, line, WINDOW))
            failed = True
    if len(found) != expected:
        out('PlayerSinks: %d player creation site(s), SITES is %d; set SITES to the count in the same change'
            % (len(found), expected))
        failed = True
    for rel, line, message in pointer:
        out('%s:%d: %s' % (rel, line, message))
        failed = True
    if failed:
        return 1
    out('PlayerSinks OK: %d files under %s, %d player creation sites, each installs the session callbacks; '
        'the group-callback and spell-modifier pointers are set and cleared last in %s'
        % (files, SCOPE, len(found), PLAYER_CPP))
    return 0


def self_test():
    failures = []

    def expect(label, text, want):
        got = sites(text)
        if got != want:
            failures.append('%s: %r, expected %r' % (label, got, want))

    install = 'InstallPlayerPacketSinks(*p);\n'
    expect('installed on the next line', 'Player* p = new Player(s);\n' + install, [(1, True)])
    expect('installed within the window', 'Player* p = new Player(s);\na();\nb();\n' + install, [(1, True)])
    expect('installed past the window', 'Player* p = new Player(s);\na();\nb();\nc();\n' + install, [(1, False)])
    expect('not installed', 'Player* p = new Player(s);\np->Create();\n', [(1, False)])
    expect('another variable installed', 'Player* p = new Player(s);\nInstallPlayerPacketSinks(*q);\n', [(1, False)])
    expect('the cooldown installer alone', 'Player* p = new Player(s);\nInstallCooldownPacketSinks(*p);\n',
           [(1, False)])
    expect('install commented out', 'Player* p = new Player(s);\n// ' + install, [(1, False)])
    expect('not assigned', 'Use(new Player(s));\n' + install, [(1, False)])
    expect('assigned across lines', 'Player* p =\n    new  Player (s);\nInstallPlayerPacketSinks( * p );\n',
           [(2, True)])
    expect('make_unique', 'auto p = std::make_unique<Player>(s);\n' + install, [(1, True)])
    expect('comment and string', '// new Player(s)\nconst char* t = "new Player(";\n', [])
    expect('other classes', 'Player* p = new PlayerTaxi(s);\nx = new MyPlayer(s);\nnewPlayer(s);\n', [])
    expect('placement new installed', 'Player* p = new (std::nothrow) Player(s);\n' + install, [(1, True)])
    expect('placement new not installed', 'Player* p = new(std::nothrow)Player(s);\np->Create();\n', [(1, False)])
    expect('qualified new installed', 'Player* p = new ::Player(s);\n' + install, [(1, True)])
    expect('qualified new not installed', 'Player* p = new::Player(s);\np->Create();\n', [(1, False)])
    expect('install on the line of the new', 'Player* p = new Player(s); ' + install + 'p->Create();\n', [(1, False)])

    def tree(files):
        root = tempfile.mkdtemp(prefix='player_sinks_')
        for rel, text in files.items():
            path = os.path.join(root, *rel.split('/'))
            os.makedirs(os.path.dirname(path), exist_ok=True)
            with open(path, 'w', encoding='utf-8', newline='') as fh:
                fh.write(text)
        return root

    def expect_check(label, files, want, expected=SITES):
        lines = []
        root = tree(files)
        try:
            got = check(root, out=lines.append, expected=expected)
        finally:
            shutil.rmtree(root, ignore_errors=True)
        if got != want:
            failures.append('check %s: rc %d, expected %d (%s)' % (label, got, want, ' | '.join(lines)))

    ctor = 'Player::Player(WorldSession* s): Unit(), m_camera(this)\n{\n    m_slot = 255;\n\n    %s\n}\n\n'
    dtor = 'Player::~Player()\n{\n    CleanupsBeforeDelete();\n    %s\n}\n'
    sets = 'm_groupCallbacks = &m_groupSinks;\n    m_spellMods = &m_spellModMgr;'
    clears = '// the cleanups report\n    m_groupCallbacks = NULL;\n    m_spellMods = NULL;'
    player = ctor % sets + dtor % clears

    def expect_pins(label, text, want):
        got = [line for line, message in pins(text)[0]]
        if got != want:
            failures.append('pins %s: failures at %r, expected %r' % (label, got, want))

    expect_pins('the set and the clear last', player, [])
    expect_pins('the clear followed by a statement', ctor % sets + dtor % (clears + '\n    delete m_x;'), [14, 13])
    expect_pins('the clear at the top',
                ctor % sets + 'Player::~Player()\n{\n    m_groupCallbacks = NULL;\n    m_spellMods = NULL;\n'
                '    CleanupsBeforeDelete();\n}\n', [12, 11])
    expect_pins('the clear missing', ctor % sets + dtor % '', [9, 9])
    expect_pins('the set missing', ctor % '' + dtor % clears, [1, 1])
    expect_pins('the set inside a nested block',
                ctor % ('if (a)\n    {\n        %s\n    }' % sets) + dtor % clears, [8, 7])
    expect_pins('the clear commented out', ctor % sets + dtor % '// m_groupCallbacks = NULL;\n    m_spellMods = NULL;',
                [9])
    expect_pins('the clear as nullptr', ctor % sets + dtor % 'm_groupCallbacks = NULL;\n    m_spellMods = nullptr;',
                [9, 12])
    expect_pins('no destructor', ctor % sets, [1])
    expect_pins('an indented closing brace',
                ctor % sets + 'Player::~Player()\n{\n    m_groupCallbacks = NULL;\n    m_spellMods = NULL;\n    }\n',
                [13])
    expect_pins('the two in the other order',
                ctor % 'm_spellMods = &m_spellModMgr;\n    m_groupCallbacks = &m_groupSinks;'
                + dtor % 'm_spellMods = NULL;\n    m_groupCallbacks = NULL;', [])
    expect_pins('a statement between the two',
                ctor % sets + dtor % 'm_groupCallbacks = NULL;\n    delete m_x;\n    m_spellMods = NULL;', [12])

    def expect_assignments(label, text, want):
        got = assignments(text)
        if got != want:
            failures.append('assignments %s: %r, expected %r' % (label, got, want))

    expect_assignments('the initialiser, comparisons, a comment and a literal',
                       'm_groupCallbacks(NULL),\nif (m_groupCallbacks == NULL)\nx = m_groupCallbacks != NULL;\n'
                       '// m_groupCallbacks = q;\nconst char* t = "m_groupCallbacks = q;";\n', [])
    expect_assignments('qualified and unspaced', 'this->m_groupCallbacks = q;\nUnit::m_groupCallbacks=NULL;\n', [1, 2])
    expect_assignments('the spell-modifier pointer', 'm_spellMods(NULL),\nm_spellMods = p;\nm_spellModsX = p;\n', [2])

    good = 'Player* p = new Player(s);\n' + install
    three = {'src/game/WorldHandlers/CharacterHandler.cpp': good + good, 'src/game/Harness/Scenario.cpp': good,
             PLAYER_CPP: player,
             'src/game/Object/Unit.cpp': 'Unit::Unit() :\n    m_groupCallbacks(NULL),\n    m_x(0)\n{\n}\n'}
    expect_check('the three installed sites pass', three, 0)
    expect_check('a fourth site without the install fails',
                 dict(three, **{'src/modules/SD3/a.cpp': 'Player* p = new Player(s);\np->Create();\n'}), 1)
    expect_check('a fourth installed site fails until SITES is raised',
                 dict(three, **{'src/modules/SD3/a.cpp': good}), 1)
    expect_check('a fourth installed site passes with SITES raised',
                 dict(three, **{'src/modules/SD3/a.cpp': good}), 0, expected=4)
    expect_check('a removed site fails until SITES is lowered', {'src/game/Harness/Scenario.cpp': good}, 1)
    expect_check('an empty scan fails', {'other/A.cpp': good}, 1)
    expect_check('the tools are not read', dict(three, **{'src/tests/tools/x.inc': 'Player* p = new Player(s);\n'}), 0)
    expect_check('the set and the clear in place, the initialiser in Unit.cpp, pass', three, 0)
    expect_check('the clear followed by another statement fails',
                 dict(three, **{PLAYER_CPP: ctor % sets + dtor % (clears + '\n    delete m_x;')}), 1)
    expect_check('the clear missing fails', dict(three, **{PLAYER_CPP: ctor % sets + dtor % ''}), 1)
    expect_check('the set missing fails', dict(three, **{PLAYER_CPP: ctor % '' + dtor % clears}), 1)
    expect_check('an assignment in another file fails',
                 dict(three, **{'src/game/Object/Unit.cpp': 'Unit::~Unit()\n{\n    m_groupCallbacks = NULL;\n}\n'}), 1)
    expect_check('the spell-modifier pointer assigned in another file fails',
                 dict(three, **{'src/game/Object/Unit.cpp': 'Unit::~Unit()\n{\n    m_spellMods = NULL;\n}\n'}), 1)
    expect_check('a second assignment in Player.cpp fails',
                 dict(three, **{PLAYER_CPP: player + 'void Player::F()\n{\n    m_groupCallbacks = NULL;\n}\n'}), 1)
    expect_check('Player.cpp missing fails', {k: v for k, v in three.items() if k != PLAYER_CPP}, 1)
    expect_check('an assignment in the tools is not read',
                 dict(three, **{'src/tests/tools/x.inc': 'm_groupCallbacks = NULL;\n'}), 0)

    for f in failures:
        print('self-test: ' + f)
    print('self-test: %s (%d failure(s))' % ('PASS' if not failures else 'FAIL', len(failures)))
    return 1 if failures else 0


def main(argv):
    ap = argparse.ArgumentParser(description='CheckPlayerSinks: every player creation installs the session callbacks.')
    here = os.path.dirname(os.path.abspath(__file__))
    ap.add_argument('--root', default=os.path.abspath(os.path.join(here, '..', '..', '..')))
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument('--check', action='store_true')
    g.add_argument('--self-test', action='store_true')
    args = ap.parse_args(argv[1:])
    if args.self_test:
        return self_test()
    return check(os.path.abspath(args.root))


if __name__ == '__main__':
    sys.exit(main(sys.argv))
