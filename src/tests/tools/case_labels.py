#!/usr/bin/env python3
"""case_labels.py --self-test: the C++ comment and literal blanker that raw_rand.py and player_sinks.py share.

blank(text) returns the text with comments and string and character literals (raw strings too, with or
without a u8, u, U or L prefix) replaced by spaces; line breaks and the text's length are kept.
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
import re
import sys


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
        elif c == 'R' and text.startswith('R"', i) and raw_prefix_start(text, i) is not None:
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


def raw_prefix_start(text, i):
    """Where the raw string whose R stands at i starts (its u8, u, U or L prefix included), or None when the R
    ends a longer name."""
    for p in ('u8', 'u', 'U', 'L', ''):
        s = i - len(p)
        if s >= 0 and text[s:i] == p and (s == 0 or not (text[s - 1].isalnum() or text[s - 1] == '_')):
            return s
    return None


def digit_separator(text, i):
    """A ' inside a number (1'000), not a character literal."""
    k = i
    while k > 0 and (text[k - 1].isalnum() or text[k - 1] in "_'"):
        k -= 1
    return k < i and text[k].isdigit()


def self_test():
    failures = []

    def expect(cond, what):
        if not cond:
            failures.append(what)

    for label, text, gone, seen in [
        ('a line comment is blanked', 'a // hidden\nb\n', ['hidden'], ['a', 'b']),
        ('a block comment is blanked across lines', 'a /* hid\nden */ b\n', ['hid', 'den'], ['a', 'b']),
        ('a string with an escaped quote is blanked', 'x = "q\\" hidden"; seen\n', ['hidden'], ['seen']),
        ('a character literal is blanked', "c = '}'; seen\n", ['}'], ['seen']),
        ('a digit separator is not a quote', "x = 1'000; seen; y = 2'000;\n", [], ["1'000", 'seen', "2'000"]),
        ('a raw string is blanked', 'a = R"x(hidden })x"; seen\n', ['hidden', '}'], ['seen']),
        ('a raw string with an encoding prefix ends at its delimiter',
         'a = LR"(")"; seen1 //")" gone1\nb = uR"(")"; seen2 //")" gone2\n'
         'c = UR"(")"; seen3 //")" gone3\nd = u8R"(")"; seen4 //")" gone4\n',
         ['gone1', 'gone2', 'gone3', 'gone4'], ['seen1', 'seen2', 'seen3', 'seen4']),
        ('an R ending a longer name opens no raw string', 'a = xLR"(" seen ")";\n', [], ['seen']),
        ('an R after a name ending in _L opens no raw string', 'a = _LR"(" seen ")";\n', [], ['seen']),
        ('U8 is no raw string prefix', 'a = U8R"(" seen ")";\n', [], ['seen']),
        ('a raw string at the start of the text is read', 'R"(")"; seen //")" gone\n', ['gone'], ['seen'])]:
        out = blank(text)
        ok = (len(out) == len(text) and out.count('\n') == text.count('\n')
              and all(g not in out for g in gone) and all(s in out for s in seen))
        print('self-test: %-58s %s' % (label, 'PASS' if ok else 'FAIL'))
        expect(ok, '%s: got %r' % (label, out))

    for f in failures:
        print('SELF-TEST FAILED: ' + f)
    print('self-test: %s (%d failure(s))' % ('PASS' if not failures else 'FAIL', len(failures)))
    return 1 if failures else 0


def main(argv):
    ap = argparse.ArgumentParser(description='The C++ comment and literal blanker raw_rand.py and player_sinks.py share.')
    ap.add_argument('--self-test', action='store_true', required=True)
    ap.parse_args(argv[1:])
    return self_test()


if __name__ == '__main__':
    sys.exit(main(sys.argv))
