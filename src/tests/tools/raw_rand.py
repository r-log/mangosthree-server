#!/usr/bin/env python3
"""raw_rand.py [--root <repo root>] --check | --list | --self-test: no C library rand() or srand() in src/game.

Not caught: a call built by a macro, a call through a function pointer or a namespace alias,
`(rand)()`, and the other C generators (rand_r, random, drand48).
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

SCOPE = 'src/game/'
EXTENSIONS = ('.h', '.hpp', '.cpp', '.inl', '.inc')
SEED_FILE = 'src/game/WorldHandlers/World.cpp'
SEED_LIMIT = 1

TOKEN = re.compile(r'\b(srand|rand)\s*\(')
QUALIFIER = re.compile(r'(\w*)\s*::\s*$')


def tokens(text):
    """[(line, name)] of every C library rand() call and srand( call, comments and literals ignored."""
    clean = case_labels.blank(text)
    found = []
    for m in TOKEN.finditer(clean):
        before = clean[:m.start()].rstrip()
        if before.endswith('.') or before.endswith('->'):
            continue
        q = QUALIFIER.search(before)
        if q and q.group(1) not in ('', 'std'):
            continue
        name = m.group(1)
        if name == 'rand' and not re.match(r'\s*\)', clean[m.end():]):
            continue
        found.append((clean.count('\n', 0, m.start()) + 1, name))
    return found


def scan(root):
    """{path: [(line, name)]} of the tokens under src/game, and the number of files read."""
    hits = {}
    files = 0
    top = os.path.join(root, *SCOPE.rstrip('/').split('/'))
    for d, dirs, names in os.walk(top):
        dirs.sort()
        for name in sorted(names):
            if not name.endswith(EXTENSIONS):
                continue
            path = os.path.join(d, name)
            rel = os.path.relpath(path, root).replace(os.sep, '/')
            files += 1
            with open(path, encoding='utf-8', errors='replace', newline='') as fh:
                found = tokens(fh.read())
            if found:
                hits[rel] = found
    return hits, files


def check(root, out=print):
    hits, files = scan(root)
    if files == 0:
        out('RawRand: read no file under %s%s -- a gate that scans nothing passes nothing' % (root, '/' + SCOPE))
        return 1
    draws = [(p, line) for p in sorted(hits) for line, name in hits[p] if name == 'rand']
    seeds = [(p, line) for p in sorted(hits) for line, name in hits[p] if name == 'srand']
    stray = [(p, line) for p, line in seeds if p != SEED_FILE]
    kept = [(p, line) for p, line in seeds if p == SEED_FILE]
    failed = False
    for p, line in draws:
        out('%s:%d: rand() draws from the C library; draw from the seeded RNG (urand, irand, rand32, rand_norm)'
            % (p, line))
        failed = True
    for p, line in stray:
        out('%s:%d: srand( seeds the C library; only %s seeds it, for SD3\'s draws' % (p, line, SEED_FILE))
        failed = True
    if len(kept) > SEED_LIMIT:
        for p, line in kept:
            out('%s:%d: srand( is called %d times in %s; at most %d' % (p, line, len(kept), SEED_FILE, SEED_LIMIT))
        failed = True
    if failed:
        return 1
    out('RawRand OK: %d files under %s, 0 rand() draws, %d srand( (%s)' % (files, SCOPE, len(kept), SEED_FILE))
    return 0


def self_test():
    failures = []

    def expect(label, text, want):
        got = [name for _, name in tokens(text)]
        if got != want:
            failures.append('%s: tokens %r, expected %r' % (label, got, want))

    expect('planted draw', 'int x = rand() % 4;\n', ['rand'])
    expect('spaced draw', 'int x = rand ( ) % 4;\n', ['rand'])
    expect('std draw', 'int x = std::rand();\n', ['rand'])
    expect('global draw', 'int x = ::rand();\n', ['rand'])
    expect('seed', 'srand(time(NULL));\n', ['srand'])
    expect('std seed', 'std::srand (1);\n', ['srand'])
    expect('urand', 'uint32 x = urand(0, 3);\n', [])
    expect('irand and rand32', 'int32 x = irand(0, 3) + rand32() + rand_norm();\n', [])
    expect('member call', 'uint32 x = RNG::instance()->rand() + gen.rand();\n', [])
    expect('other qualifier', 'uint32 x = RNGen::rand();\n', [])
    expect('longer names', 'uint32 x = m_rand() + grand() + Rand() + operand();\n', [])
    expect('line comment', '// rand() and srand(1)\nint x = 0;\n', [])
    expect('block comment', '/* rand()\n srand(2) */\nint x = 0;\n', [])
    expect('string', 'const char* s = "rand() srand(";\n', [])
    expect('rand with an argument', 'int x = rand(7);\n', [])
    lines = tokens('int a;\n// rand()\nint b = rand();\n')
    if lines != [(3, 'rand')]:
        failures.append('line numbers: %r, expected [(3, \'rand\')]' % lines)

    def tree(files):
        root = tempfile.mkdtemp(prefix='raw_rand_')
        for rel, text in files.items():
            path = os.path.join(root, *rel.split('/'))
            os.makedirs(os.path.dirname(path), exist_ok=True)
            with open(path, 'w', encoding='utf-8', newline='') as fh:
                fh.write(text)
        return root

    def expect_check(label, files, want):
        lines = []
        root = tree(files)
        try:
            got = check(root, out=lines.append)
        finally:
            shutil.rmtree(root, ignore_errors=True)
        if got != want:
            failures.append('check %s: rc %d, expected %d (%s)' % (label, got, want, ' | '.join(lines)))

    seed = {SEED_FILE: 'void f()\n{\n    srand(1);\n}\n'}
    expect_check('the seed alone passes', seed, 0)
    expect_check('no seed passes', {'src/game/A.cpp': 'int x = urand(0, 1);\n'}, 0)
    expect_check('a planted draw fails', dict(seed, **{'src/game/Spells/A.cpp': 'int x = rand() % 2;\n'}), 1)
    expect_check('a draw in the seed file fails', {SEED_FILE: 'void f()\n{\n    srand(1);\n    x = rand();\n}\n'}, 1)
    expect_check('a stray seed fails', dict(seed, **{'src/game/Harness/H.cpp': 'void g() { srand(2); }\n'}), 1)
    expect_check('a second seed fails', {SEED_FILE: 'void f()\n{\n    srand(1);\n    srand(2);\n}\n'}, 1)
    expect_check('a comment mention passes', dict(seed, **{'src/game/A.h': '/// draws from rand() no more\n'}), 0)
    expect_check('urand passes', dict(seed, **{'src/game/A.inl': 'uint32 x = urand(0, 3);\n'}), 0)
    expect_check('outside src/game is not read', dict(seed, **{'src/modules/SD3/a.cpp': 'int x = rand();\n'}), 0)
    expect_check('an empty scan fails', {'src/other/A.cpp': 'int x;\n'}, 1)
    expect_check('another extension is not read', dict(seed, **{'src/game/A.txt': 'rand()\n'}), 0)

    for f in failures:
        print('self-test: ' + f)
    print('self-test: %s (%d failure(s))' % ('PASS' if not failures else 'FAIL', len(failures)))
    return 1 if failures else 0


def main(argv):
    ap = argparse.ArgumentParser(description='CheckRawRand: no C library rand() draw or srand() seed in src/game.')
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
        hits, files = scan(root)
        for p in sorted(hits):
            for line, name in hits[p]:
                print('%s:%d: %s' % (p, line, name))
        print('%d tokens in %d files of %d' % (sum(len(h) for h in hits.values()), len(hits), files))
        return 0
    return check(root)


if __name__ == '__main__':
    sys.exit(main(sys.argv))
