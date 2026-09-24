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
the campaign's downcast counter; D5f turned subtype_sites into a ratchet (ctest entry
`subtype_downcasts`), so its regexes and output format are exact -- see the D5 brief before
changing either.

python3 src/tests/tools/downcast_count.py src/game
python3 src/tests/tools/downcast_count.py --self-test
python3 src/tests/tools/downcast_count.py --list subtype_sites src/game
python3 src/tests/tools/downcast_count.py --gate subtype_sites=0 src/game

--list <class> prints one `file:line: text` line per occurrence of that class (so the number of
printed lines equals the class's occurrence count) and still prints the counts.
--gate <class>=<max> (repeatable) exits 1 when that class's occurrence count exceeds <max>, after
naming the offending sites. It is how the campaign's ratchets are wired into ctest.
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
import bisect
import os
import re
import sys

# Order matters: it is the order printed, and c_style_player/c_style_creature are the two
# total_c_style adds together.
#
# Every class tolerates whitespace around the `*` and the `const`: `static_cast<Creature *>` and
# `(Creature const *)` are the same downcast as their tight spellings, and the pre-D5f regexes
# missed them (D5f fix round 1 widened all five and re-baselined the numbers). subtype_sites
# accepts any receiver expression inside the cast -- `((Creature*)GetTarget())->IsPet()` and
# `static_cast<Creature const*>(minion)->IsTotem()` count like `((Creature*)x)->IsPet()` -- and
# the `[^;]*?` keeps a match inside one statement.
#
# NOT_AN_OPERAND is what the whitespace tolerance costs: `bool f(Player* ) const` declares an
# unnamed parameter, it is not a cast, and only the tight `*)` spelling used to exclude it. A
# cast's operand never starts with one of these keywords.
NOT_AN_OPERAND = r"(?!const\b|override\b|final\b|noexcept\b)"
CLASSES = [
    ("c_style_player", re.compile(r"\(\s*Player(\s+const)?\s*\*\s*\)\s*" + NOT_AN_OPERAND + r"[A-Za-z_(]")),
    ("c_style_creature", re.compile(r"\(\s*Creature(\s+const)?\s*\*\s*\)\s*" + NOT_AN_OPERAND + r"[A-Za-z_(]")),
    ("static_cast", re.compile(r"static_cast<\s*(Player|Creature)(\s+const)?\s*\*\s*>")),
    ("dynamic_cast", re.compile(r"dynamic_cast<")),
    ("subtype_sites", re.compile(
        r"\(\s*\(\s*Creature(\s+const)?\s*\*\s*\)[^;]*?\)\s*->\s*(IsPet|IsTotem)\s*\(\s*\)"
        r"|static_cast<\s*Creature(\s+const)?\s*\*\s*>\s*\([^;]*?\)\s*->\s*(IsPet|IsTotem)\s*\(\s*\)")),
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


def line_starts(text):
    """Offsets of the first character of every line, for offset -> line number lookups."""
    starts = [0]
    i = text.find('\n')
    while i != -1:
        starts.append(i + 1)
        i = text.find('\n', i + 1)
    return starts


def find_match_lines(pattern, stripped):
    """1-based line numbers of every occurrence of pattern in an already-stripped blob.

    strip_comments preserves line structure (a // comment keeps its newline, a block comment
    is replaced by its newlines), so a line number in the stripped text is the line number in
    the original file.
    """
    starts = line_starts(stripped)
    return [bisect.bisect_right(starts, m.start()) for m in pattern.finditer(stripped)]


def parse_gate(spec):
    """'subtype_sites=0' -> ('subtype_sites', 0). Raises ValueError on anything else."""
    if "=" not in spec:
        raise ValueError(f"--gate wants <class>=<max>, got '{spec}'")
    cls, _, limit = spec.partition("=")
    cls = cls.strip()
    if cls not in CLASS_MAP:
        raise ValueError(f"--gate: unknown class '{cls}' (known: {', '.join(CLASS_MAP)})")
    try:
        maximum = int(limit.strip())
    except ValueError:
        raise ValueError(f"--gate: '{limit.strip()}' is not a number")
    if maximum < 0:
        raise ValueError(f"--gate: a maximum cannot be negative, got {maximum}")
    return cls, maximum


def gate_failures(totals, gates):
    """The message for every gate the counts blow; empty when every gate holds."""
    messages = []
    for cls, maximum in gates:
        count = totals.get(cls, 0)
        if count > maximum:
            messages.append(
                f"GATE FAILED: {cls} occurrences={count} exceeds the allowed maximum {maximum}"
            )
    return messages


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

    # The spellings the pre-D5f regexes missed (fix round 1).
    text = strip_comments("Creature* c = static_cast<Creature *>(possessed);\n")
    n = count_occurrences(CLASS_MAP["static_cast"], text)
    if n != 1:
        failures.append(f"static_cast<Creature *>: expected static_cast=1, got {n}")

    text = strip_comments("Player* p = (Player const *)unit;\n")
    n = count_occurrences(CLASS_MAP["c_style_player"], text)
    if n != 1:
        failures.append(f"(Player const *)unit: expected c_style_player=1, got {n}")

    # ...but an unnamed parameter is a declaration, not a cast (the cost of the whitespace
    # tolerance, paid by NOT_AN_OPERAND).
    text = strip_comments("bool CliHandler::needReportToTarget(Player* ) const\n")
    n = count_occurrences(CLASS_MAP["c_style_player"], text)
    if n != 0:
        failures.append(f"needReportToTarget(Player* ) const: expected c_style_player=0, got {n}")

    text = strip_comments("if (((Creature*)GetTarget())->IsPet())\n")
    n = count_occurrences(CLASS_MAP["subtype_sites"], text)
    if n != 1:
        failures.append(f"((Creature*)GetTarget())->IsPet(): expected subtype_sites=1, got {n}")

    text = strip_comments("return static_cast<Creature const*>(minion)->IsTotem();\n")
    n = count_occurrences(CLASS_MAP["subtype_sites"], text)
    if n != 1:
        failures.append(f"static_cast<Creature const*>(minion)->IsTotem(): expected subtype_sites=1, got {n}")
    n = count_occurrences(CLASS_MAP["static_cast"], text)
    if n != 1:
        failures.append(f"static_cast<Creature const*>(minion)->IsTotem(): expected static_cast=1, got {n}")

    # A subtype question on a receiver that is not the cast's operand must not count: the `)`
    # right before `->IsPet()` is what the regex keys on, and a plain pointer has none.
    text = strip_comments("if (((Creature*)a)->HasLootRecipient() && creature->IsPet())\n")
    n = count_occurrences(CLASS_MAP["subtype_sites"], text)
    if n != 0:
        failures.append(f"cast + unrelated creature->IsPet(): expected subtype_sites=0, got {n}")

    # One statement per match: a cast on one statement and a subtype question on the next is
    # not a subtype site (the `[^;]*?` cannot cross the `;`).
    text = strip_comments("Creature* c = (Creature*)u; bool b = c->IsPet();\n")
    n = count_occurrences(CLASS_MAP["subtype_sites"], text)
    if n != 0:
        failures.append(f"cast then a separate IsPet() statement: expected subtype_sites=0, got {n}")

    # --list: the line numbers are the original file's, not the stripped blob's.
    text = strip_comments(
        "// ((Creature*)a)->IsPet()\n/* two\n   lines */\n((Creature*)b)->IsTotem();\n"
    )
    lines = find_match_lines(CLASS_MAP["subtype_sites"], text)
    if lines != [4]:
        failures.append(f"--list line numbers: expected [4], got {lines}")

    # --gate: the spec parser and the pass/fail decision.
    try:
        got = parse_gate("subtype_sites=0")
        if got != ("subtype_sites", 0):
            failures.append(f"parse_gate('subtype_sites=0'): expected ('subtype_sites', 0), got {got}")
    except ValueError as exc:
        failures.append(f"parse_gate('subtype_sites=0') raised: {exc}")
    for bad in ("subtype_sites", "no_such_class=0", "subtype_sites=x", "subtype_sites=-1"):
        try:
            parse_gate(bad)
            failures.append(f"parse_gate('{bad}'): expected ValueError, got none")
        except ValueError:
            pass
    gates = [("subtype_sites", 0)]
    if gate_failures({"subtype_sites": 0}, gates):
        failures.append("gate subtype_sites=0 with 0 sites: expected pass, got a failure")
    msgs = gate_failures({"subtype_sites": 1}, gates)
    if len(msgs) != 1 or "subtype_sites" not in msgs[0]:
        failures.append(f"gate subtype_sites=0 with 1 site: expected one message, got {msgs}")
    if gate_failures({"c_style_creature": 400}, [("c_style_creature", 400)]):
        failures.append("gate at exactly the maximum: expected pass, got a failure")

    if failures:
        for f in failures:
            print(f"self-test FAILED: {f}", file=sys.stderr)
        return 1
    print("self-test OK")
    return 0


def main(argv):
    args = argv[1:]
    if "--self-test" in args:
        return self_test()

    list_class = None
    gates = []
    root = None
    i = 0
    while i < len(args):
        arg = args[i]
        if arg in ("--list", "--gate"):
            if i + 1 >= len(args):
                sys.stderr.write(f"{arg} wants a value\n")
                return 1
            value = args[i + 1]
            i += 2
        elif arg.startswith("--list=") or arg.startswith("--gate="):
            arg, _, value = arg.partition("=")
            i += 1
        elif arg.startswith("-"):
            sys.stderr.write(f"unknown option '{arg}'\n")
            sys.stderr.write(__doc__)
            return 1
        else:
            if root is not None:
                sys.stderr.write("only one <src-root> is accepted\n")
                return 1
            root = arg
            i += 1
            continue

        if arg == "--list":
            if value not in CLASS_MAP:
                sys.stderr.write(f"--list: unknown class '{value}' (known: {', '.join(CLASS_MAP)})\n")
                return 1
            list_class = value
        else:
            try:
                gates.append(parse_gate(value))
            except ValueError as exc:
                sys.stderr.write(f"{exc}\n")
                return 1

    if root is None:
        sys.stderr.write(__doc__)
        return 1

    totals = {name: 0 for name, _ in CLASSES}
    files_with = {name: 0 for name, _ in CLASSES}
    listed = []
    gated_sites = {cls: [] for cls, _ in gates}

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

            wanted = set(gated_sites)
            if list_class:
                wanted.add(list_class)
            if not wanted:
                continue
            lines = text.splitlines()
            shown = path.replace(os.sep, "/")
            for cls in wanted:
                if counts[cls] == 0:
                    continue
                for lineno in find_match_lines(CLASS_MAP[cls], stripped):
                    source = lines[lineno - 1].strip() if lineno <= len(lines) else ""
                    entry = f"{shown}:{lineno}: {source}"
                    if cls == list_class:
                        listed.append(entry)
                    if cls in gated_sites:
                        gated_sites[cls].append(entry)

    for entry in listed:
        print(entry)
    if listed:
        print(f"{list_class} sites={len(listed)}")

    for cls, _ in CLASSES:
        print(f"{cls} occurrences={totals[cls]} files={files_with[cls]}")
    print(f"total_c_style={totals['c_style_player'] + totals['c_style_creature']}")

    failures = gate_failures(totals, gates)
    if failures:
        for cls, maximum in gates:
            if totals[cls] > maximum:
                for entry in gated_sites[cls]:
                    print(f"  {entry}", file=sys.stderr)
        for message in failures:
            print(message, file=sys.stderr)
        return 1
    for cls, maximum in gates:
        print(f"gate {cls}<={maximum}: OK ({totals[cls]})")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
