#!/usr/bin/env python3
"""handler_classes.py [--root <repo root>] --check | --self-test

A handler class stores nothing: under src/game/session/handlers/ (every .h, .hpp, .inl and .cpp, at
any depth) a handler borrows the session for one call and keeps nothing past it, so no file there
holds state that outlives a call or that two map workers could share.

Every class and struct defined there is walked with method_count.py's class-scope scanner (its --all
mode, which separates member functions from data members), and the check refuses, by name with file
and line:
  - a data member, and a static data member;
  - a member function that is not static (a constructor, a destructor or an operator included),
    unless it is deleted (`= delete`);
  - a base class (a base can carry storage);
  - a variable declared with its type (`struct S { ... } s;`).
Outside the classes it refuses:
  - a `static` or `thread_local` inside a function body, a member function's or a free one's (a
    function-local static outlives the call and is shared between the threads that run it);
  - a variable at namespace scope, named or anonymous namespaces and `extern "C"` blocks read
    through (a static data member's out-of-class definition is one).
A `constexpr` variable is accepted at either scope: it is a constant fixed at compile time, which no
call can change, so it holds no state between calls and nothing two workers could race on. A `const`
that is not `constexpr` is refused (its initialiser may run at start-up; spell it `constexpr`).
Accepted, and stored nowhere: static member functions, deleted ones, friend declarations, typedef and
using, nested types, static_assert, free functions (static or not) and out-of-class definitions.

A missing directory passes: there is nothing to check until the first handler class lands.

KNOWN MISSES, stated rather than chased: a lambda capturing the session handed to an API that runs it
later, a pointer kept through a template parameter or `auto`, state reached through a call (a
singleton's member, a global of another directory), and method_count.py's own limits (its docstring).

python src/tests/tools/handler_classes.py --check
python src/tests/tools/handler_classes.py --self-test
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

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import method_count  # noqa: E402

HANDLERS_DIR = 'src/game/session/handlers'
SUFFIXES = ('.h', '.hpp', '.inl', '.cpp')
DATA_REASONS = ('no parameter list', 'function-pointer data member')
CLASS_DEF = re.compile(r'(?<![\w])(class|struct)\s+([A-Za-z_]\w*)\s*(?:final\s*)?(:[^;{}]*)?\{')
FUNCTION_HEAD = re.compile(r'\)\s*(?:(?:const|volatile|noexcept|override|final|&&?)\s*|->\s*[\w:<>*&, ]+)*$')
NAMESPACE_HEAD = re.compile(r'(?:\bnamespace\b[\w\s:]*|\bextern\s*"\s*")$')
FORWARD = re.compile(r'(class|struct|union|enum)\b[^{]*;$')
STORAGE = re.compile(r'[{};]|\b(?:static|thread_local)\b')


def words(signature):
    return set(re.findall(r'\w+', signature.split('(')[0]))


def walk(clean):
    """([offset of a static or thread_local in a function body], [(from, to) of each namespace's head and
    braces])."""
    local, braces, stack, start = [], [], [], 0
    for m in STORAGE.finditer(clean):
        t = m.group()
        if t == '{':
            head = clean[start:m.start()].strip()
            if 'body' in stack:
                stack.append('body')
            elif FUNCTION_HEAD.search(head):
                stack.append('body')
            elif NAMESPACE_HEAD.search(head):
                stack.append('namespace')
                braces.append((start, m.end()))
            else:
                stack.append('scope')
            start = m.end()
        elif t == '}':
            if stack and stack.pop() == 'namespace':
                braces.append((m.start(), m.end()))
            start = m.end()
        elif t == ';':
            start = m.end()
        elif 'body' in stack:
            local.append(m.start())
    return local, braces


def check_text(rel, text):
    """[(line, what)] for every refusal in one file."""
    clean = method_count.blank_comments_and_literals(text)
    found = []
    for m in CLASS_DEF.finditer(clean):
        if re.search(r'\benum\s*$', clean[:m.start()]):
            continue
        name = m.group(2)
        if m.group(3):
            found.append((clean.count('\n', 0, m.start()) + 1, 'class %s has a base class (%s)' % (
                name, ' '.join(m.group(3)[1:].split()))))
        for line, reason, sig in method_count.scan_members(text, name)[2]:
            w = words(sig)
            if FORWARD.match(sig):
                continue
            if reason is None and 'static' not in w and not re.search(r'=\s*delete\s*;$', sig):
                found.append((line, 'class %s: a member function that is not static: %s' % (name, sig)))
            elif reason in DATA_REASONS and 'constexpr' not in w:
                kind = 'a static data member' if 'static' in w else 'a data member'
                found.append((line, 'class %s: %s: %s' % (name, kind, sig)))
            elif reason == 'nested type' and re.search(r'\{\.\.\.\}\s*[*&]*\s*\w', sig):
                found.append((line, 'class %s: a variable declared with its type: %s' % (name, sig)))
    local, braces = walk(clean)
    for at in local:
        found.append((clean.count('\n', 0, at) + 1, 'a function-local static: %s' % (
            ' '.join(clean[at:clean.find(';', at) + 1].split()))))
    flat = list(clean)
    for a, b in braces:
        flat[a:b] = [c if c == '\n' else ' ' for c in clean[a:b]]
    wrapped = 'struct HandlerFileScope {\n' + ''.join(flat) + '\n};\n'
    for line, reason, sig in method_count.scan_members(wrapped, 'HandlerFileScope')[2]:
        if sig.startswith('namespace ') or FORWARD.match(sig):
            continue
        if reason in DATA_REASONS and 'constexpr' not in words(sig):
            found.append((line - 1, 'a variable at namespace scope: %s' % sig))
        elif reason == 'nested type' and re.search(r'\{\.\.\.\}\s*[*&]*\s*\w', sig):
            found.append((line - 1, 'a variable declared with its type: %s' % sig))
    return sorted(found)


def check(root, out=print):
    top = os.path.join(root, *HANDLERS_DIR.split('/'))
    if not os.path.isdir(top):
        out('handler_classes: %s does not exist: nothing to check; OK' % HANDLERS_DIR)
        return 0
    files, refused = 0, 0
    for here, dirs, names in os.walk(top):
        dirs.sort()
        for n in sorted(names):
            if not n.endswith(SUFFIXES):
                continue
            path = os.path.join(here, n)
            rel = os.path.relpath(path, root).replace(os.sep, '/')
            with open(path, encoding='utf-8', errors='replace', newline='') as fh:
                text = fh.read()
            files += 1
            for line, what in check_text(rel, text):
                out('%s:%d: %s' % (rel, line, what))
                refused += 1
    out('handler_classes: %d files under %s, %d refused; %s' % (files, HANDLERS_DIR, refused,
                                                                 'OK' if refused == 0 else 'FAILED'))
    return 1 if refused else 0


SELF_ACCEPTED = '''#include "Fixture.h"

class WorldSession;

namespace
{
    constexpr uint32 MAX_SHEATH = 3;                    // "static int x;" in a literal is not read
}

struct CombatHandlers
{
    public:
        CombatHandlers() = delete;
        static void HandleSwing(WorldSession& session, WorldPacket& recv_data);
        static void HandleStop(WorldSession& session, WorldPacket& /*recv_data*/);
        static constexpr uint32 PACKET_SIZE = 4 + 20;  // static int m_count;
        typedef void (*Entry)(WorldSession&, WorldPacket&);
        friend class OpcodeTable;
        struct Detail;
    private:
        static void SendStop(WorldSession& session, Unit const* enemy)
        {
            WorldPacket data(SMSG_ATTACKSTOP, PACKET_SIZE);
            session.SendPacket(&data);
        }
};

static void Helper(WorldSession& session)
{
    for (int i = 0; i < MAX_SHEATH; ++i)
        session.GetPlayer();
}

void CombatHandlers::HandleSwing(WorldSession& session, WorldPacket& recv_data)
{
    Helper(session);
}
'''

SELF_REFUSED = [
    ('a data member', 'class CombatHandlers: a data member: Player* m_player;',
     '        friend class OpcodeTable;', '        Player* m_player;'),
    ('a static data member', 'class CombatHandlers: a static data member: static uint32 s_count;',
     '        friend class OpcodeTable;', '        static uint32 s_count;'),
    ('a static const that is not constexpr',
     'class CombatHandlers: a static data member: static const uint32 LIMIT = 3;',
     '        friend class OpcodeTable;', '        static const uint32 LIMIT = 3;'),
    ('a member function that is not static', 'a member function that is not static: void Reset();',
     '        friend class OpcodeTable;', '        void Reset();'),
    ('a base class', 'class CombatHandlers has a base class (public Base)',
     'struct CombatHandlers\n', 'struct CombatHandlers : public Base\n'),
    ('a variable declared with its type', 'a variable declared with its type: struct Cache {...} s_cache;',
     '        friend class OpcodeTable;', '        struct Cache { } s_cache;'),
    ('a function-local static in a member function', 'a function-local static: static uint32 sent = 0;',
     '            session.SendPacket(&data);',
     '            static uint32 sent = 0;\n            session.SendPacket(&data);'),
    ('a function-local static in a free function', 'a function-local static: static WorldSession* last;',
     '    Helper(session);', '    static WorldSession* last;\n    Helper(session);'),
    ('a thread_local in a function body', 'a function-local static: thread_local Player* cached;',
     '    Helper(session);', '    thread_local Player* cached;\n    Helper(session);'),
    ('a variable at namespace scope', 'a variable at namespace scope: WorldSession* g_last = nullptr;',
     'static void Helper', 'WorldSession* g_last = nullptr;\n\nstatic void Helper'),
    ('a static variable in an anonymous namespace', 'a variable at namespace scope: static uint32 s_swings;',
     '    constexpr uint32 MAX_SHEATH = 3;', '    static uint32 s_swings;'),
    ('a static data member defined out of the class',
     'a variable at namespace scope: uint32 CombatHandlers::s_count = 0;',
     'static void Helper', 'uint32 CombatHandlers::s_count = 0;\n\nstatic void Helper'),
]


def self_test():
    failures = []

    def row(label, ok, detail):
        print('self-test: %-62s %s' % (label, 'PASS' if ok else 'FAIL'))
        if not ok:
            failures.append('%s: %s' % (label, detail))

    got = check_text('fixture', SELF_ACCEPTED)
    row('statics only, a deleted constructor, constexpr constants: accepted', got == [], got)
    for label, needle, a, b in SELF_REFUSED:
        if a not in SELF_ACCEPTED:
            row(label, False, 'the mutation %r matches nothing' % a)
            continue
        got = check_text('fixture', SELF_ACCEPTED.replace(a, b, 1))
        row(label + ': refused', len(got) == 1 and needle in got[0][1], got)
    lines = []
    rc = check(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'no-such-root'), lines.append)
    row('a missing directory: passes, saying so', rc == 0 and 'does not exist' in lines[0], lines)
    for f in failures:
        print('SELF-TEST FAILED: ' + f)
    print('self-test: %s (%d failure(s))' % ('PASS' if not failures else 'FAIL', len(failures)))
    return 1 if failures else 0


def main(argv):
    ap = argparse.ArgumentParser(description='A handler class stores nothing.')
    ap.add_argument('--root', default=os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                                                   '..', '..', '..')))
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument('--check', action='store_true')
    g.add_argument('--self-test', action='store_true')
    args = ap.parse_args(argv[1:])
    if args.self_test:
        return self_test()
    return check(os.path.abspath(args.root))


if __name__ == '__main__':
    sys.exit(main(sys.argv))
