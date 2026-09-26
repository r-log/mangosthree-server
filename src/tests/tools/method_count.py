#!/usr/bin/env python3
"""method_count.py --class <ClassName> --header <header>

The method counter for the decoupling campaign (counter R1). It is the D5 plan's method regex
(design/2026-09-23-decoupling-d5-unit-decomposition-plan.md, "R1"), which was a shell pipeline
over a hand-found line range:

  sed -n '<first>,<last>p' <header> | grep -nE '\\(.*\\)' | grep -E '[;{]\\s*$'
      | grep -vE '^[0-9]+:\\s*(//|/\\*|\\*|#)' | grep -v typedef
      | grep -vE ':\\s*(if|for|while|switch|else|return|case|do|catch)\\b' | wc -l

This tool finds the range itself: from the line that holds `class <ClassName>` (the
definition, not a forward declaration) to the line that holds the brace closing its body,
both inclusive -- the range the D4 facts measured by hand (Player.h:1106-4385 at 392a8fd36,
672 methods; 663 at ab4d8bd53, after D4a). Braces are matched with comments and string/char literals blanked; the filters
then run on the raw lines, numbered from 1 inside the range as `grep -n` numbers them, so every
quirk of the pipeline is kept on purpose (the number means what the facts measured):

  - a line counts when it holds '(' ... ')' and ends in ';' or '{' (so a declaration with a
    trailing // comment is NOT counted, and neither is one whose parens span two lines);
  - it does not count when, after its number and ':', it starts with //, /*, * or #;
  - it does not count when it contains `typedef` anywhere;
  - it does not count when ANY ':' in it (the number's own included) is followed by optional
    whitespace and a control keyword as a whole word -- so `case X: return y();` is dropped too.

It over-counts a few inline-body statements and counts the methods of nested types; the same
bias before and after a change, so the delta is honest.

python3 src/tests/tools/method_count.py --class Player --header src/game/entities/player/Player.h
python3 src/tests/tools/method_count.py --list --class Player --header src/game/entities/player/Player.h
python3 src/tests/tools/method_count.py --self-test
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

# The four filters of the pipeline, in its order. Each runs on "<n>:<raw line>".
HAS_PARENS = re.compile(r'\(.*\)')
ENDS_STATEMENT = re.compile(r'[;{]\s*$')
COMMENT_OR_DIRECTIVE = re.compile(r'^[0-9]+:\s*(//|/\*|\*|#)')
CONTROL_FLOW = re.compile(r':\s*(if|for|while|switch|else|return|case|do|catch)\b')


def blank_comments_and_literals(text):
    """Blank comments and string/char literals, keeping every '\\n' (line numbers hold).

    A character literal ends at a newline even without its closing apostrophe: an apostrophe
    in a preprocessor line (`#error don't`) must not swallow the lines after it, braces
    included. An escaped newline inside a literal (a line splice) keeps its '\\n'."""
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
                if quote == "'" and text[i] == '\n':
                    break
                if text[i] == '\\':
                    i += 1
                    if i < n and text[i] == '\n':
                        out.append('\n')
                elif text[i] == '\n':
                    out.append('\n')
                i += 1
            out.append(quote)
            if i < n and text[i] == quote:
                i += 1
            continue
        out.append(c)
        i += 1
    return ''.join(out)


def find_class_range(text, name):
    """(first, last) 1-based inclusive lines of the definition of class/struct <name>.

    `first` is the line of the class keyword, `last` the line of the brace closing the body.
    Raises ValueError when there is no definition (forward declarations are skipped)."""
    clean = blank_comments_and_literals(text)
    pattern = re.compile(r'\b(class|struct)\s+(?:\w+\s+)*?' + re.escape(name) + r'\b([^;{]*)\{')
    for m in pattern.finditer(clean):
        tail = m.group(2).strip()
        # `class Foo;` never reaches a '{' without a ';' first; a base clause or `final` may.
        if tail and not tail.startswith(':') and not tail.startswith('final'):
            continue
        open_at = m.end() - 1
        depth = 0
        for i in range(open_at, len(clean)):
            if clean[i] == '{':
                depth += 1
            elif clean[i] == '}':
                depth -= 1
                if depth == 0:
                    first = clean.count('\n', 0, m.start()) + 1
                    last = clean.count('\n', 0, i) + 1
                    return first, last
        raise ValueError('class %s: the body is never closed' % name)
    raise ValueError('class %s not found' % name)


def count_methods(text, name):
    """(first, last, [(line, raw)]) for class <name>: its range and the lines R1 counts."""
    first, last = find_class_range(text, name)
    # '\n' only, as grep and the range finder count lines: splitlines() also breaks at \v, \f,
    # \x1c-\x1e, \x85 and U+2028/2029, which would shift every line after one of them.
    lines = text.split('\n')[first - 1:last]
    counted = []
    for rel, raw in enumerate(lines, 1):
        numbered = '%d:%s' % (rel, raw)
        if not HAS_PARENS.search(numbered):
            continue
        if not ENDS_STATEMENT.search(numbered):
            continue
        if COMMENT_OR_DIRECTIVE.search(numbered):
            continue
        if 'typedef' in numbered:
            continue
        if CONTROL_FLOW.search(numbered):
            continue
        counted.append((first + rel - 1, raw))
    return first, last, counted


SELF_TEST_SOURCE = '''class Target;
class TargetMenu { void NotMine(); };
/* class Target { void InAComment(); }; */
class Target : public Base
{
        friend void Pal::Touch(Target* t);
    public:
        Target();
        ~Target();
        void Method(int a, int b) const;
        int Inline() const { return m_a; }
        bool Open(int x)
        {
            if (x) { Close(); }
            return Close();
        }
        void Split(int a,
                   int b);
        typedef void (*Callback)(int);
        // void Commented(int);
        /* legacy */ void Block(int);
        * void Star(int);
#define TARGET_MACRO(x) (x);
        struct Inner { void Nested(); };
        void Braced(int x) {
            switch (x) { case 1: Call(x); }
            case 2: Call(x);
            default: Call(x);
        }
        void Pair() { a(); } void Next(int);
        char const* m_text = "{ not a brace (";
        void Label(); public: void Other(int);
        std::function<void(int)> m_fn;
        void Trailing(int); // a trailing comment hides a declaration from R1
        label: return Next();
        m_z = flag ? 1 : do_it(y);
        void Spaced(int)   ; \t
};
void Target::Outside();
'''

# The lines of SELF_TEST_SOURCE the pipeline counts (checked against the shell pipeline itself):
#   6 friend function, 8-10 ctor/dtor/method, 24 a nested type's line, 25 ends in '{',
#   28 `default:` is not a control keyword, 30 two statements ending in ';', 32 an access label
#   mid-line, 33 a member whose type has parens, 36 `do_it` is not the word `do`, 37 spaces
#   before the ';' and a space and a tab after it (spelled \t, so no editor strips it). Not counted: 11/14/26 end in '}', 12/13/16/29 end in neither,
#   15/27/35 a ':' then a control keyword, 17/18 the parens split over two lines, 19 typedef,
#   20/21/22/23 start with //, /*, * or #, 31 no ')', 34 a trailing comment.
SELF_TEST_EXPECTED = [6, 8, 9, 10, 24, 25, 28, 30, 32, 33, 36, 37]

# Characters that str.splitlines() breaks at and grep does not. They are built with chr() so
# no editor or tool turns an escape in this file into the character itself.
NOT_NEWLINES = [chr(0x0b), chr(0x0c), chr(0x1c), chr(0x1d), chr(0x1e), chr(0x85), chr(0x2028), chr(0x2029)]

# (label, source, class, expected (first, last, counted lines)).
EDGE_CASES = [
    # Line numbers are '\n' only: a comment full of the other separators is one line, so the two
    # methods after it are lines 4 and 5 and the body closes on line 6.
    ('separators that are not newlines',
     'class Odd\n{\n    // ' + ' '.join(NOT_NEWLINES) + '\n    void A(int);\n    void B(int);\n};\nvoid After(int);\n',
     'Odd', (1, 6, [4, 5])),
    # An apostrophe with no partner on its line (a preprocessor line) ends at the newline: it
    # must not swallow the closing brace and run on to the next apostrophe.
    ('an unpaired apostrophe ends at the newline',
     "class Quote\n{\n#define QUOTE_ME don't\n    void A(int);\n};\nvoid Outside(int);\nchar c = 'x';\n",
     'Quote', (1, 5, [4])),
    # A line splice inside a string literal keeps its newline, so the range still ends on the
    # brace's own line.
    ('a line splice inside a string literal',
     'class Spliced\n{\n    char const* m_s = "a' + chr(92) + '\nb";\n    void A(int);\n};\n',
     'Spliced', (1, 6, [5])),
    # CRLF: the '\r' stays on the line, as grep sees it, and `[;{]\s*$` still ends the statement.
    ('CRLF line endings',
     SELF_TEST_SOURCE.replace('\n', '\r\n'), 'Target', (4, 38, SELF_TEST_EXPECTED)),
]


def self_test():
    ok = True
    try:
        first, last, counted = count_methods(SELF_TEST_SOURCE, 'Target')
    except ValueError as err:
        print('self-test: %s' % err)
        return 1
    if (first, last) != (4, 38):
        print('self-test: range %d-%d, expected 4-38' % (first, last))
        ok = False
    got = [line for line, _ in counted]
    if got != SELF_TEST_EXPECTED:
        print('self-test: counted lines %r, expected %r' % (got, SELF_TEST_EXPECTED))
        for line, raw in counted:
            print('  %d: %s' % (line, raw))
        ok = False
    # A class keyword on its own line with the name on the next still opens the range there.
    first, last, counted = count_methods('\nclass\n    Wrapped\n{\n    void A();\n};\n', 'Wrapped')
    if (first, last, len(counted)) != (2, 6, 1):
        print('self-test: wrapped definition gave %d-%d, %d methods' % (first, last, len(counted)))
        ok = False
    for missing in ('Missing', 'TargetMen'):
        try:
            count_methods(SELF_TEST_SOURCE, missing)
            print('self-test: class %s did not raise' % missing)
            ok = False
        except ValueError:
            pass
    for label, source, name, expected in EDGE_CASES:
        try:
            got = count_methods(source, name)
            got = (got[0], got[1], [line for line, _ in got[2]])
        except ValueError as err:
            got = str(err)
        if got != expected:
            print('self-test (%s): got %r, expected %r' % (label, got, expected))
            ok = False
    print('self-test %s' % ('OK' if ok else 'FAILED'))
    return 0 if ok else 1


def main(argv):
    parser = argparse.ArgumentParser(description='R1: the D5 method regex over one class body.')
    parser.add_argument('--self-test', action='store_true', help='run the self-test and exit')
    parser.add_argument('--class', dest='name', help='the class to measure, e.g. Player')
    parser.add_argument('--header', help='the header that defines it')
    parser.add_argument('--list', action='store_true', help='print every counted line')
    args = parser.parse_args(argv[1:])
    if args.self_test:
        return self_test()
    if not args.name or not args.header:
        parser.print_usage()
        return 2
    # newline='': no translation, so a '\r' stays where grep sees it and only '\n' ends a line.
    with open(args.header, encoding='utf-8', errors='replace', newline='') as f:
        text = f.read()
    first, last, counted = count_methods(text, args.name)
    if args.list:
        for line, raw in counted:
            print('%s:%d: %s' % (args.header, line, raw.strip()))
    print('%s methods=%d (R1 over %s:%d-%d)' % (args.name, len(counted), args.header, first, last))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
