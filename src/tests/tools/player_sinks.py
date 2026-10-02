#!/usr/bin/env python3
"""player_sinks.py [--root <repo root>] --check | --self-test: every player gets its cooldown sinks.

Every `new Player(` under src/ -- placement (`new (std::nothrow) Player(`), qualified (`new ::Player(`) and
make_unique/make_shared<Player> included, the SD3 scripts too -- assigns the player to a variable, and
`InstallCooldownPacketSinks(*<variable>)` is called on one of the next WINDOW lines; an install on the line
of the `new` itself does not count and fails.
The number of creation sites is fixed at SITES: a new site fails until it installs the sinks and SITES is
raised with it, and a removed site fails until SITES is lowered.

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
            install = re.compile(r'\bInstallCooldownPacketSinks\s*\(\s*\*\s*%s\s*\)' % re.escape(var.group(1)))
            installed = any(install.search(text_line) for text_line in lines[line:line + WINDOW])
        found.append((line, installed))
    return found


def scan(root):
    """[(path, line, installed)] of every creation under src/, and the number of files read."""
    out = []
    files = 0
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
            files += 1
            with open(path, encoding='utf-8', errors='replace', newline='') as fh:
                for line, installed in sites(fh.read()):
                    out.append((rel, line, installed))
    return out, files


def check(root, out=print, expected=SITES):
    found, files = scan(root)
    if files == 0:
        out('PlayerSinks: read no file under %s/%s -- a gate that scans nothing passes nothing' % (root, SCOPE))
        return 1
    failed = False
    for rel, line, installed in found:
        if not installed:
            out('%s:%d: a player is created without InstallCooldownPacketSinks(*<it>) in the next %d lines; '
                'call it right after the construction' % (rel, line, WINDOW))
            failed = True
    if len(found) != expected:
        out('PlayerSinks: %d player creation site(s), SITES is %d; set SITES to the count in the same change'
            % (len(found), expected))
        failed = True
    if failed:
        return 1
    out('PlayerSinks OK: %d files under %s, %d player creation sites, each installs its cooldown sinks'
        % (files, SCOPE, len(found)))
    return 0


def self_test():
    failures = []

    def expect(label, text, want):
        got = sites(text)
        if got != want:
            failures.append('%s: %r, expected %r' % (label, got, want))

    install = 'InstallCooldownPacketSinks(*p);\n'
    expect('installed on the next line', 'Player* p = new Player(s);\n' + install, [(1, True)])
    expect('installed within the window', 'Player* p = new Player(s);\na();\nb();\n' + install, [(1, True)])
    expect('installed past the window', 'Player* p = new Player(s);\na();\nb();\nc();\n' + install, [(1, False)])
    expect('not installed', 'Player* p = new Player(s);\np->Create();\n', [(1, False)])
    expect('another variable installed', 'Player* p = new Player(s);\nInstallCooldownPacketSinks(*q);\n', [(1, False)])
    expect('install commented out', 'Player* p = new Player(s);\n// ' + install, [(1, False)])
    expect('not assigned', 'Use(new Player(s));\n' + install, [(1, False)])
    expect('assigned across lines', 'Player* p =\n    new  Player (s);\nInstallCooldownPacketSinks( * p );\n',
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

    good = 'Player* p = new Player(s);\n' + install
    three = {'src/game/WorldHandlers/CharacterHandler.cpp': good + good, 'src/game/Harness/Scenario.cpp': good}
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

    for f in failures:
        print('self-test: ' + f)
    print('self-test: %s (%d failure(s))' % ('PASS' if not failures else 'FAIL', len(failures)))
    return 1 if failures else 0


def main(argv):
    ap = argparse.ArgumentParser(description='CheckPlayerSinks: every player creation installs its cooldown sinks.')
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
