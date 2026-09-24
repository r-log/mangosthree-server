#!/usr/bin/env python3
"""downcast_count.py <src-root>

The downcast counter for the decoupling campaign (design section 8). It walks every *.cpp
and *.h file under <src-root>, strips comments, and counts five classes of Player/Creature
downcast at the call site:

  c_style_player    a C-style cast to Player* immediately followed by an operand
  c_style_creature  the same for Creature
  static_cast       static_cast<Player*> / static_cast<Creature*> (and the "const" spelling)
  dynamic_cast      any dynamic_cast<...>
  subtype_sites     ((Creature*)x)->IsPet() / ->IsTotem() -- a C-style cast used only to ask
                    a question Creature::GetSubtype() already answers without the cast

subtype_sites is a subset of c_style_creature: a matching site counts in both. This script is
the campaign's downcast counter; a later PR (D5f) uses subtype_sites as its ratchet, so its
regexes and output format are exact -- see the D5 brief before changing either.

python3 src/tests/tools/downcast_count.py src/game
python3 src/tests/tools/downcast_count.py --self-test
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
import os
import re
import sys

# Order matters: it is the order printed, and c_style_player/c_style_creature are the two
# total_c_style adds together.
CLASSES = [
    ("c_style_player", re.compile(r"\(Player( const)?\*\)\s*[A-Za-z_(]")),
    ("c_style_creature", re.compile(r"\(Creature( const)?\*\)\s*[A-Za-z_(]")),
    ("static_cast", re.compile(r"static_cast<(Player|Creature)( const)?\*>")),
    ("dynamic_cast", re.compile(r"dynamic_cast<")),
    ("subtype_sites", re.compile(r"\(\(Creature( const)?\*\)(this|[A-Za-z_]+)\)->(IsPet|IsTotem)\(\)")),
]
CLASS_MAP = dict(CLASSES)


def strip_comments(text):
    """Remove // line comments and /* ... */ block comments with a simple state machine.

    String literals are not tracked separately (the brief allows this simplification): a "//"
    or "/*" inside a string literal is stripped as if it were a real comment. That is rare in
    this tree's cast sites and never changes which *file* has a match, only occasionally an
    occurrence count in a file that mixes casts and string literals containing those tokens.
    """
    out = []
    i = 0
    n = len(text)
    while i < n:
        slash = text.find('/', i)
        if slash == -1:
            out.append(text[i:])
            break
        out.append(text[i:slash])
        nxt = text[slash + 1:slash + 2]
        if nxt == '/':
            j = text.find('\n', slash)
            if j == -1:
                i = n
            else:
                i = j
        elif nxt == '*':
            j = text.find('*/', slash + 2)
            if j == -1:
                i = n
            else:
                out.append('\n' * text.count('\n', slash, j + 2))
                i = j + 2
        else:
            # A lone '/' (division, a path separator inside a string, ...): not a comment
            # start. Keep it and advance past it -- this is the case the earlier, buggier
            # version of this function looped forever on.
            out.append('/')
            i = slash + 1
    return ''.join(out)


def count_occurrences(pattern, text):
    return sum(1 for _ in pattern.finditer(text))


def count_text(text):
    """Occurrence counts per class for one already-comment-stripped blob."""
    return {name: count_occurrences(pattern, text) for name, pattern in CLASSES}


def self_test():
    failures = []

    text = strip_comments("// (Player*)x\n")
    n = count_occurrences(CLASS_MAP["c_style_player"], text)
    if n != 0:
        failures.append(f"commented-out cast: expected c_style_player=0, got {n}")

    text = strip_comments(
        "void f()\n{\n    Creature* c = (Creature const*)x;\n    static_cast<Player*>(u);\n}\n"
    )
    n = count_occurrences(CLASS_MAP["c_style_creature"], text)
    if n != 1:
        failures.append(f"(Creature const*)x: expected c_style_creature=1, got {n}")
    n = count_occurrences(CLASS_MAP["static_cast"], text)
    if n != 1:
        failures.append(f"static_cast<Player*>(u): expected static_cast=1, got {n}")

    text = strip_comments("((Creature*)this)->IsPet();\n")
    n = count_occurrences(CLASS_MAP["subtype_sites"], text)
    if n != 1:
        failures.append(f"((Creature*)this)->IsPet(): expected subtype_sites=1, got {n}")
    n = count_occurrences(CLASS_MAP["c_style_creature"], text)
    if n != 1:
        failures.append(f"((Creature*)this)->IsPet(): expected c_style_creature=1, got {n}")

    if failures:
        for f in failures:
            print(f"self-test FAILED: {f}", file=sys.stderr)
        return 1
    print("self-test OK")
    return 0


def main(argv):
    if "--self-test" in argv:
        return self_test()

    if len(argv) != 2:
        sys.stderr.write(__doc__)
        return 1

    root = argv[1]
    totals = {name: 0 for name, _ in CLASSES}
    files_with = {name: 0 for name, _ in CLASSES}

    for dirpath, dirnames, names in os.walk(root):
        dirnames.sort()
        for name in sorted(names):
            if not name.endswith((".cpp", ".h")):
                continue
            path = os.path.join(dirpath, name)
            try:
                text = open(path, encoding="utf-8", errors="ignore").read()
            except OSError:
                continue
            stripped = strip_comments(text)
            counts = count_text(stripped)
            for cls, n in counts.items():
                totals[cls] += n
                if n > 0:
                    files_with[cls] += 1

    for cls, _ in CLASSES:
        print(f"{cls} occurrences={totals[cls]} files={files_with[cls]}")
    print(f"total_c_style={totals['c_style_player'] + totals['c_style_creature']}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
