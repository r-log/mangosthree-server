#!/usr/bin/env python3
"""member_count.py <header> <ClassName>

The data-member counter for the decoupling campaign's D4 phase (counter M1 in
design/2026-09-26-decoupling-d4-facts.md). It finds the definition of <ClassName> in
<header>, strips comments and string/char literals, walks the braces of the class body and
counts the statements at depth 1 (the class body itself) that

  - end in ';',
  - contain no '(' (so no method declaration, no function pointer, no constructor call),
  - do not start with typedef/using/friend/enum/struct/class/static_assert
    (after an access label such as `private:` is removed).

Two numbers are printed: `statements` (one per such statement) and `declarators` (a
statement's comma-separated declarators, counted at angle/brace/bracket depth 0, so
`float m_x, m_y, m_z;` is 3 and `std::map<uint32, Foo> m_map;` is 1).

Known blind spot, kept on purpose so the number means what the facts file measured: a
member whose type contains '(' (a function-pointer member) is not counted.

python3 src/tests/tools/member_count.py src/game/entities/player/Player.h Player
python3 src/tests/tools/member_count.py --list src/game/entities/player/Player.h Player
python3 src/tests/tools/member_count.py --self-test
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
import re
import sys

SKIP_RE = re.compile(r'(typedef|using|friend|enum|struct|class|union|static_assert)\b')
ACCESS_RE = re.compile(r'^(public|private|protected)\s*:\s*')


def strip_comments_and_literals(text):
    """Blank comments and string/char literals, keeping every newline (line numbers hold)."""
    out = []
    i = 0
    n = len(text)
    while i < n:
        c = text[i]
        if c == '/' and i + 1 < n and text[i + 1] == '/':
            while i < n and text[i] != '\n':
                i += 1
            continue
        if c == '/' and i + 1 < n and text[i + 1] == '*':
            end = text.find('*/', i + 2)
            end = n if end < 0 else end + 2
            out.append('\n' * text.count('\n', i, end))
            i = end
            continue
        if c in '"\'':
            quote = c
            out.append(quote)
            i += 1
            while i < n and text[i] != quote:
                if text[i] == '\\':
                    i += 1
                elif text[i] == '\n':
                    out.append('\n')
                i += 1
            out.append(quote)
            i += 1
            continue
        out.append(c)
        i += 1
    return ''.join(out)


def find_class_body(text, name):
    """Offset of the '{' that opens the definition of class/struct <name>, or -1."""
    pattern = re.compile(r'\b(class|struct)\s+(?:\w+\s+)*?' + re.escape(name) + r'\b([^;{]*)\{')
    for m in pattern.finditer(text):
        tail = m.group(2)
        # `class Foo;` never reaches a '{' without a ';' first; a base-clause or `final` may.
        if tail.strip() == '' or tail.strip().startswith(':') or tail.strip().startswith('final'):
            return m.end() - 1
    return -1


def split_declarators(statement):
    depth = 0
    count = 1
    for ch in statement:
        if ch in '<([{':
            depth += 1
        elif ch in '>)]}':
            depth -= 1
        elif ch == ',' and depth == 0:
            count += 1
    return count


def count_members(text, name):
    """[(line, statement, declarators)] for the data members of class <name> in <text>."""
    clean = strip_comments_and_literals(text)
    start = find_class_body(clean, name)
    if start < 0:
        raise ValueError('class %s not found' % name)
    line = clean.count('\n', 0, start) + 1
    depth = 0
    cur = []
    members = []
    i = start
    while i < len(clean):
        ch = clean[i]
        if ch == '\n':
            line += 1
        if ch == '{':
            depth += 1
            cur = []
        elif ch == '}':
            depth -= 1
            cur = []
            if depth == 0:
                break
        elif depth == 1:
            if ch == ';':
                s = ' '.join(''.join(cur).split())
                s = ACCESS_RE.sub('', s)
                while ACCESS_RE.match(s):
                    s = ACCESS_RE.sub('', s)
                if s and '(' not in s and not SKIP_RE.match(s):
                    members.append((line, s, split_declarators(s)))
                cur = []
            else:
                cur.append(ch)
        i += 1
    return members


SELF_TEST_SOURCE = '''
class Other { int m_notMine; };
class Target : public Base
{
    friend class Pal;
    public:
        typedef std::set<uint32> Set;           // a typedef is not a member
        using Alias = int;
        enum Kind { A, B };
        struct Inner { int m_inner; int m_inner2; };
        Target();
        void Method(int a, int b);
        int Inline() const { int local = 1; return local; }
        static_assert(sizeof(int) == 4, "x");
    private:
        int m_a;                                /* one */
        float m_x, m_y, m_z;                    // three declarators, one statement
        std::map<uint32, std::string> m_map;    // the comma is inside <>
        static uint32 s_count;
        char const* m_text = "a;b{c}";          // literal braces and ';' are blanked
        void (*m_callback)(int);                // blind spot: has '(' -- not counted
    protected: bool m_flag;
};
'''


def self_test():
    members = count_members(SELF_TEST_SOURCE, 'Target')
    statements = len(members)
    declarators = sum(d for _, _, d in members)
    names = [s for _, s, _ in members]
    ok = True
    if statements != 6:
        print('self-test: statements %d, expected 6: %r' % (statements, names))
        ok = False
    if declarators != 8:
        print('self-test: declarators %d, expected 8: %r' % (declarators, names))
        ok = False
    if any('m_inner' in s or 'm_notMine' in s for s in names):
        print('self-test: a nested or foreign member was counted: %r' % names)
        ok = False
    if not any(s == 'bool m_flag' for s in names):
        print('self-test: a member on the access-label line was missed: %r' % names)
        ok = False
    try:
        count_members(SELF_TEST_SOURCE, 'Missing')
        print('self-test: a missing class did not raise')
        ok = False
    except ValueError:
        pass
    print('self-test %s' % ('OK' if ok else 'FAILED'))
    return 0 if ok else 1


def main(argv):
    if argv[1:] == ['--self-test']:
        return self_test()
    args = argv[1:]
    listing = False
    if args and args[0] == '--list':
        listing = True
        args = args[1:]
    if len(args) != 2:
        print(__doc__)
        return 2
    path, name = args
    with open(path, encoding='utf-8', errors='replace') as f:
        text = f.read()
    members = count_members(text, name)
    if listing:
        for line, s, d in members:
            print('%s:%d: %s%s' % (path, line, s, '' if d == 1 else '  [%d declarators]' % d))
    statements = len(members)
    declarators = sum(d for _, _, d in members)
    static = sum(1 for _, s, _ in members if re.match(r'static\b', s))
    print('%s statements=%d declarators=%d static=%d' % (name, statements, declarators, static))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
