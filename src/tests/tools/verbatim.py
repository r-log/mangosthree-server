#!/usr/bin/env python3
"""verbatim.py [--root <repo root>] [--base <ref>] [--original [--anchor <sha>]] --check | --self-test

The spell handler registry's verbatim proof (decoupling D11, design/2026-09-28-unit-reopening.md
3(b)): every case body that moved out of a per-spell-ID switch into a registered handler is pasted
back at its label, and the file must come back byte for byte as it was at BASE.

BASE, ORIGINAL, VOID_SUBSTITUTIONS, SITES and BLOCKS live in verbatim_sites.py beside this file, which a
move edits (its sites' entries and BASE, or its block's entry); this file holds none of them and never
changes them, and split_gate.py (whose docstring holds the rules) refuses to run it when it binds or
changes one, when it runs with values other than the data file's, or when the data file holds anything
but literal values. The data file's rules are read from its source before it is imported, so a data
file that would run anything else (an exit among them) is refused before it runs.

For each file in SITES, --check:
  1. reads the file at BASE (`git show <base>:<file>`) and in the working tree, and for each of the
     two its handlers: the handler file (HANDLERS) where that version has it, else the handler block
     appended to the sites' file (from TAIL_MARKER to the end of the file; a file whose handlers
     were never appended names none); a version holding both fails, and so does a working tree
     holding neither;
  2. drops the lines the moves added (ADDED: the handlers header's include) and that appended block;
  3. at each site, replaces the dispatch (the site's exact DISPATCH lines, found exactly once)
     with the switch it stood for: the switch's opening lines, then for each row of the site's
     registration table, in table order, the label line (LABELS) and -- once per run of rows that
     register the same function -- that function's body, reverse-substituted (SUBSTITUTIONS: the
     context accessors and the outcome), re-indented to the label's body and wrapped in `{` `}`
     at the label indent where the run's body was (BRACES below); then, if the site has
     a registered `default:` (DEFAULT, found through `registry.RegisterDefault<TRAITS>(&F);`), its
     label and F's body the same way; a site whose switch had no `default:` must have none registered
     (every site names its TRAITS for that). A label holding `{body}` is a one-line case
     (`case 1: x = 1; break;    // note`): the body's lines are joined there with one space.
     A partly moved site (RESIDUAL) keeps its switch, holding the labels not moved, directly after
     the dispatch, and its LABELS list every label of that switch as it stood before any move, in
     that order (the original order). Each version read derives its own split from its own
     table: its moved labels are its table's rows, its standing labels the `case N:` lines at the
     label indent of the switch after its dispatch (each found once, each a spec label line
     unchanged, all in the original order); every spec label is in exactly one of the two (none
     in both, none in neither) and no table lists a label twice. The rows must be in the original
     order; a registered function's run of rows goes back before its anchor: the label after its
     last row in the original order, or, where that version moved that label too, the next label
     of the original order still standing in that version, or the switch's close when none
     follows; the dispatch is dropped. The runs of moved labels in the original order (a run ends
     at a standing label) must be as many as the anchors, and the anchors must stand in the switch
     in the original order: guards the checks before make unreachable. So the same spec proves a
     base with fewer labels moved than the working tree; each version proves the original order of
     the labels it still holds, so a moved label's place is proven against a base it stands in (or
     one from before the site's first move). Such a site registers no default (its `default:`, if
     any, stays in the switch).
     TABLE: a site's registration table is the one table whose rows are typed by the site's
     TRAITS (`static <Row><TRAITS> const <any name>[] =`), so a table renamed between versions is
     still found; each version holds exactly one such table per site (two fail, none fails). Only
     a version with no typed table at all (rows of an untyped `Row`) reads the table the site's
     `table` names; a version with typed tables never reads the name.
     BRACES: a run's shape is read from the version's file, the base's first, else the working
     tree's: the run's label lines (or a `default:` line), searched in the whole file, must stand
     together there exactly once (more than once fails; blank lines between them: B), and the line
     after them gives the shape.
     `{` at the label indent there means the body is wrapped, and that block must close (`}` at
     the label indent) directly before the next `case` or `default:` at the label indent or the
     switch's close: a block followed by more lines of the body is a shape the paste-back cannot
     reproduce and fails. Anything else (a `{` at a deeper indent included) is an unbraced body.
     A run standing in neither version (moved in both) is pasted back unbraced on both sides: its
     shape, like its place, is proven against a base it stands in;
  4. does the same to the file at BASE, for the sites a PR before this one moved (their dispatch
     is in the base; a site whose dispatch is not in the base is this PR's and must be a switch
     there), so BASE may be any commit from before the first move to the parent of this PR;
  5. cuts from the rebuilt base what the working tree deleted, each a whole label run: a `case N:`
     or `default:` line and the lines under it down to the next `case` or `default:` at its
     indent or the first line indented less that is not comment-only (the switch's close). A run
     fails when the code above it, past blank and comment-only lines, could reach it: a label at
     its indent with no body of its own, or a run whose last statement (inside the closing `}`
     of a braced run, after the colon of a one-line case) is not a whole `return ...;`,
     `break;`, `continue;` or `goto ...;` line, nor one under a control header (a code line
     ending in none of `;`, `{`, `}`, `:`, such as `if (x)` or `for (;;)`); a run above that
     cannot fall through in another shape (an if and else that both return, braced or not)
     fails too, on the safe side. What is cut: a
     residual site's DELETED labels (each one of the site's labels, found at most once); a site
     GONE whole (its dispatch, a table typed by its TRAITS or a default registered for it still
     in the working tree fail), its switch, opening directly above its first label, with the
     lines GONE names above and below it, inside the file; and the file's CUTS, each (the run's
     first lines, its number of lines), the first line a `case` (a `default:` goes only with its
     whole switch) at no site, found at most once, the number exactly the run's. What a base no
     longer holds (a base from after the deletion) is not cut there, and a deleted label may be
     neither registered nor standing in it. A site with a dispatch deletes labels only when it
     is a residual one; cuts may not overlap;
  6. compares the two, byte for byte, and names the first difference.
A file renamed since a commit it is read at (the base or its original) keeps its key, the working
tree's path, and names its old path in `renamed_from`: the working tree must hold the key and not the
old path, and the commit exactly one of the two, the one read; a commit holding neither or both
fails, naming the paths. A key or a members_of header the working tree does not hold fails by name.
Because the bodies are pasted from the functions the table registers, a row pointing at the wrong
function, a lost row or default, a changed line, or a body moved in the wrong order all fail; a
body a PR before this one moved is proven again, against its own base's copy. Every
`RegisterDefault` in the handlers must be a line `registry.RegisterDefault<TRAITS>(&F);` of a site
with a `default:`, and every site's TRAITS must appear as `Dispatch<TRAITS>` in its own DISPATCH
lines, so no default is registered under a spelling the paste-back does not read; and every
`Register` stands inside ROWS_FUNCTION, which registers the tables' rows, so no labelled row is
registered outside the tables the paste-back reads.

The run shapes beyond one label per line and a body that ends in its own terminator:
  T, comment lines after a block: a braced run's closing `}` may be followed, before the next label
     or the switch's close, by lines that are comments (`//`) at exactly the label indent. They belong
     to that run: the handler holds them after its last statement at column 0, and the paste-back puts
     them after the block's `}` at the label indent. A code line, a comment at another indent or a
     blank line there is a block followed by more lines, and fails.
  S, the shape read inside the site's switch: a run's label lines, `default:` included, are looked up
     only inside the switches the site's open lines start (each closed by the first close line after
     them) that hold one of the site's label lines, so a `default:` line standing in another switch of
     the file is never read; found twice there fails.
  V, a valued return: a site in a function returning T names `value` T, and its outcome pairs are built
     from it (valued_substitutions(T), after the site's own substitutions), so the data names only T:
     `return SpellHandlerOutcome<T>::Return(x);` pastes back as `return x;` (x on that line),
     `return SpellHandlerOutcome<T>::Continue();` as `break;`. Every handler the site registers must
     answer SpellHandlerOutcome<T> (a site with no `value` answers void and ends its substitutions with
     VOID_SUBSTITUTIONS); another outcome type fails. Each fails by name: a `value` that is not a plain
     type spelling (one or more word characters and `:`, the spelling a handler's head is read with); a
     site with `value` whose own substitutions list an outcome pair (a left side holding
     `SpellHandlerOutcome<`); a site with `value` none of whose handlers in the working tree returns a
     value (a `Return(x)`, comments aside; a default's counts), whatever the runs still standing in a
     partly moved switch return; a handler returning a value at a site with no `value`.
  M, several labels on one line: LABELS maps each id of a line `case 1: case 2: case 3:` to that same
     line, and every label line must hold exactly the ids mapped to it and nothing else. The ids of one line are rows
     of one run, so they share its function (an id of the line registered by another function
     fails); the line is pasted back once. In a partly moved switch every id on a standing line
     stands; a deleted id sharing its line with others fails.
  F, a run falling into the next: its handler ends in `    return G(ctx);`, G being the function of
     the next row of the table, whose first label is the label after the run's last in the original
     order (no label standing between them). The paste-back drops that line, so the run falls into
     the next one as it did. A call of any other function as the last line fails, and so does a
     default ending in one. Moving both runs as one body under all their labels is not this shape:
     the second run's labels stood between the bodies, and the paste-back differs.
  P, comment lines before the first label: lines between a whole site's switch `{` and its first
     label, read where the switch stands, must each be a `//` comment at the `{`'s indent or deeper
     (a code line, a blank line or a shallower comment there fails). They belong to the run of that
     label: its handler begins with them, each held at its indent less the `{`'s, and the paste-back
     writes them at the `{`'s indent plus that, before the label. A handler not beginning with that
     many comment lines fails; a comment held at the body's indent pastes back at another indent
     and differs. A partly moved switch keeps them in the switch that still stands.
  B, blank lines inside a run's labels: where the run stands, blank lines between two of its label
     lines are part of the run and paste back between those labels. Any other line between them is
     not, and the run then pastes back without it and differs. A blank line after a run's body is a
     line of that body, and its handler holds it.
  E, a default falling off the switch's end: where the switch stands, a `default:` that is its last
     run and whose last statement (inside its block's `}` when braced, comments after it aside) is
     not a whole `return ...;`, `break;`, `continue;` or `goto ...;` line has a handler ending in
     `return ...::Continue();`, and the paste-back drops that line. A default handler ending in
     anything else there fails. A labelled run in that place fails: only a default may fall off
     the end. A last statement that terminates only under a control header (`if (x)` over
     `return;`) is not this shape and differs.
Every handler's last statement (comments aside) is a `return` at the function's own level, so no
body falls off its end.

Each handler body is also checked for what the paste-back cannot see: a live-out local used
bare (a case body's name the move did not route through the context), and so any other name in
scope at the site (IN_SCOPE: its function's parameters and locals); a member of the site's
class used bare (an implicit `this->` the move missed: in a free function the name could still
compile if a free function or global of that name exists, and then means something else) -- the
names are read from the class's own declaration (MEMBERS_OF), every member function and data
member at class scope; a substitution's left side inside a string literal, plain or raw (the
paste-back rewrites it there too, so a literal changed by the move would come back as the base's;
comments are rewritten and read as they stand); and a `return ...::Continue();` inside a loop or a
nested switch (the moved `break;` would have left that loop, not the case). A site whose substitutions
hold no pair pasting back as `break;` fails by name before any Continue is read.

python src/tests/tools/verbatim.py --check        # against BASE (this PR's parent), reading git
python src/tests/tools/verbatim.py --check --original   # each file against its original
python src/tests/tools/verbatim.py --self-test    # fixtures only, no git

ORIGINALS: --check --original reads each file at its original instead of BASE: the entry's own
`original` where it names one, else ORIGINAL (master before the first move). A file's original is
the commit at which it last stood with every one of its sites in place: for a file moved once, the
parent of its move. The original is the proof that the file's text outside its sites has not changed
since before its first move, so it may only move forward when a PR names the reason. A base or
original is spelled as a commit's hex SHA, full or abbreviated, and is resolved once to the full SHA
of the one commit it names, which every later read uses. Refused, each by name: a base or original
not spelled as a hex SHA, one that names no commit, one that is ambiguous (more than one object
begins with it and none is the one commit), one that resolves to a commit it does not spell (a ref
of that name read in its place), an original that is not the base (BASE, or --base) or an ancestor
of it (an original is never ahead of the base), and an original that does not hold the file.
--anchor <sha> takes the base's place in that ancestry check and nowhere else; CI passes master's head
there (the first parent of the checkout it tests), so no original is ahead of master whatever BASE a PR sets.

BLOCKS: a block of type definitions moved from a source file into a header. Each entry names the commit
before the move (`base`, spelled and resolved as an original is), the file the block stood in (`origin`),
the header that holds it now (`header`), the block's first line (`first`, exact text), its number of
lines (`lines`), and the lines the move added to the origin (`added`, each exact text); an entry with
other keys, or a `lines` that is not a positive count, is refused. --check, with or without --original,
runs every entry after the sites, each against its own base, and prints one line for it:
  1. the block: the origin at the base holds `first` exactly once, and the block is the `lines` lines
     from there; its braces balance, its last line is `};`, and it defines a type (a column-0
     `struct N` or `class N` line not ending in `;`). The base must be HEAD or an ancestor of it, or,
     with --anchor, the anchor or an ancestor of it;
  2. the origin read: the working tree's while SITES has no entry for the origin, else the origin at
     that entry's `original`, which the entry must name and which must be the base or descend from it.
     So the entry proves the base against the merge of the move, and once the origin's sites move,
     their --check --original proves every later tree against that merge;
  3. the origin as read, less each `added` line (each found exactly once), equals the origin at the
     base less the block and the blank lines directly after it, byte for byte; the first difference
     is named;
  4. the working tree's header holds the block's lines contiguous and byte for byte, exactly once;
  5. no twin: each type the block defines is defined once in the header and in no other C or C++ file
     under src/ (`git ls-files src`), nor in the origin as read at an original. A definition is
     `struct` or `class`, the name, then a body (past `final` and a base clause), comments and
     literals aside; a declaration or a use is none. A copy left in a file that does not include the
     header is one no compiler sees, and this check is what refuses it.
Check 5 runs before check 3, so a block left in its origin as well is named as a twin. Each fails by
name.

CI (.github/workflows/core_verbatim.yml) runs --check against both bases on every pull request: the
parent proves this PR's own moves, the originals re-prove every body ever moved against the switches
as they first stood, so a slip merged earlier is never inherited as the next PR's base. Both bases
work because every later tree still holds the bodies to paste back. The same job runs the cast
proof (cast_verbatim.py --check), whose base is frozen: it compares a window around each site.
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
import ast
import os
import re
import shutil
import subprocess
import sys
import tempfile
import threading

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from case_labels import blank  # noqa: E402  (the same comment/literal blanking as the ratchet)
import split_gate  # noqa: E402

# The names verbatim_sites.py assigns; split_gate.py holds the split.
DATA_NAMES = ('BASE', 'ORIGINAL', 'VOID_SUBSTITUTIONS', 'SITES', 'BLOCKS')

if split_gate.refused_before_import(__file__, 'verbatim_sites', DATA_NAMES):
    sys.exit(1)
try:
    from verbatim_sites import BASE, BLOCKS, ORIGINAL, SITES, VOID_SUBSTITUTIONS  # noqa: E402
except Exception as e:
    sys.exit(split_gate.unloaded(__file__, 'verbatim_sites', e))


def valued_substitutions(value):
    """The outcome pairs of a site in a function returning `value`: `return x;` and `break;`."""
    return [(re.compile(r'return SpellHandlerOutcome<%s>::Return\((.*)\);' % re.escape(value)), r'return \1;'),
            ('return SpellHandlerOutcome<%s>::Continue();' % value, 'break;')]


class Failure(Exception):
    pass


def outcome_pairs(site):
    """The site with its substitutions followed by the outcome pairs built from its `value` (V); a site
    with no `value` as it is. A `value` that is not a plain type spelling fails, and so does a site with a
    `value` whose own substitutions list an outcome pair, and a site whose pairs hold none pasting back as
    `break;` (its Continue could not be read)."""
    if 'value' in site:
        value = site['value']
        if not isinstance(value, str) or not re.fullmatch(r'[\w:]+', value):
            raise Failure('%s: its value %r is not a plain type spelling' % (site['name'], value))
        listed = [a for a, _ in site['substitutions'] if isinstance(a, str) and 'SpellHandlerOutcome<' in a]
        if listed:
            raise Failure('%s: its value is %s, and its substitutions list the outcome pair %r, which the tool builds '
                          'from the value' % (site['name'], value, listed[0]))
        site = dict(site, substitutions=list(site['substitutions']) + valued_substitutions(value))
    if not any(b == 'break;' for _, b in site['substitutions']):
        raise Failure('%s: its substitutions hold no Continue pair (none pastes back as `break;`)' % site['name'])
    return site


VALUED_RETURN = re.compile(r'\bSpellHandlerOutcome<[\w:]+>::Return\(')


def returns_value(body):
    """Whether `body`, comments aside, holds a `SpellHandlerOutcome<...>::Return(` with something before its `)`."""
    text = '\n'.join(body)
    return any(not re.match(r'\s*\)', text[m.end():]) for m in VALUED_RETURN.finditer(blank(text)))


def in_literal(text, at):
    """Whether text[at] stands inside a string literal, as blank() reads `text`: a mark after text[:at]
    stays blanked behind a `*/` (no block comment holds it) and is code behind a `"` (a literal closes)
    or stays blanked behind a line end (a raw string holds it; a line comment would end)."""
    def code(tail):
        return blank(text[:at] + tail).endswith('@')
    return not code('*/@') and (code('"@') or not code('\n@'))


TABLE_HEAD = re.compile(r'\s*static [\w:]+(?:<([\w:]+)>)? const (\w+)\[\] =')


def named_table(lines, table):
    """The index of the one head `static <Row type> const <table>[] =`."""
    starts = [i for i, l in enumerate(lines) for m in [TABLE_HEAD.fullmatch(l)] if m and m.group(2) == table]
    if len(starts) != 1:
        raise Failure('registration table %s found %d times' % (table, len(starts)))
    return starts[0]


def site_table(lines, site):
    """[(spell id, function)] of the site's registration table: the one table whose rows are typed
    by the site's traits; a version with no typed table at all reads the table the site names."""
    heads = [(i, m.group(1), m.group(2)) for i, m in enumerate(TABLE_HEAD.fullmatch(l) for l in lines) if m]
    typed = [(i, name) for i, traits, name in heads if traits == site['traits']]
    if len(typed) > 1:
        raise Failure('%s: %d registration tables typed by %s: %s'
                      % (site['name'], len(typed), site['traits'], [name for _, name in typed]))
    if typed:
        return table_rows(lines, typed[0][0], typed[0][1])
    if any(traits for _, traits, _ in heads):
        raise Failure('%s: no registration table typed by %s' % (site['name'], site['traits']))
    if 'table' not in site:
        raise Failure('%s: no registration table is typed, and the site names no table to read' % site['name'])
    return table_rows(lines, named_table(lines, site['table']), site['table'])


def table_rows(lines, start, table):
    """[(spell id, function)] of the table whose head is lines[start]: `{ { id, &Function }, ... };`."""
    rows = []
    i = start + 1
    if lines[i].strip() != '{':
        raise Failure('registration table %s: no "{" after its head' % table)
    i += 1
    while lines[i].strip() != '};':
        m = re.fullmatch(r'\s*\{ (\d+), &(\w+) \},', lines[i])
        if not m:
            raise Failure('registration table %s: not a "{ id, &Function }," row: %r' % (table, lines[i]))
        rows.append((int(m.group(1)), m.group(2)))
        i += 1
    return rows


def default_function(lines, traits):
    """F of the one `registry.RegisterDefault<traits>(&F);` line, and that line's index."""
    found = [(m.group(1), i) for i, m in enumerate(
        re.fullmatch(r'\s*registry\.RegisterDefault<%s>\(&(\w+)\);' % re.escape(traits), l) for l in lines) if m]
    if len(found) != 1:
        raise Failure('the default of %s registered %d times' % (traits, len(found)))
    return found[0]


def handler_body(lines, function, context, value='void'):
    """The lines between the braces of `static SpellHandlerOutcome<value> function(context& ctx)` (or
    `/*ctx*/`, a body that reads no context)."""
    head = re.compile(r'static SpellHandlerOutcome<([\w:]+)> %s\(%s& (ctx|/\*ctx\*/)\)' % (re.escape(function),
                                                                                          re.escape(context)))
    starts = [(i, m.group(1)) for i, m in enumerate(head.fullmatch(l) for l in lines) if m]
    if len(starts) != 1:
        raise Failure('handler %s(%s& ctx) found %d times' % (function, context, len(starts)))
    i, answers = starts[0]
    if answers != value:
        raise Failure('handler %s answers SpellHandlerOutcome<%s>, and its site\'s value is %s'
                      % (function, answers, value))
    if lines[i + 1] != '{':
        raise Failure('handler %s: no "{" on the line after its head' % function)
    j = next((k for k in range(i + 2, len(lines)) if lines[k] == '}'), None)
    if j is None:
        raise Failure('handler %s: no closing "}" in column 0' % function)
    code = [l for l in lines[i + 2:j] if l.strip() and not comment_only(l)]
    if not code or not re.fullmatch(r'    return\b.*;', code[-1]) or (
            len(code) > 1 and not blank(code[-2]).rstrip().endswith((';', '{', '}', ':'))):
        raise Failure('handler %s: its last statement is not a return at its own level (it could fall off its end)'
                      % function)
    return lines[i + 2:j]


def class_members(header, cls):
    """The names class `cls` declares at class scope in `header` (its text): member functions and
    data members, not its constructors, friends, nested types or what inline bodies call. A data
    member's name is the last name before its initialiser, array bound or bit-field colon; a `::`
    is not such a colon, so the parts of a qualified type (`std::string`, `proto::SessionId`) are
    never member names and the member the line declares is."""
    t = blank(header)
    m = re.search(r'\bclass\s+%s\b[^;{]*\{' % re.escape(cls), t)
    if not m:
        raise Failure('class %s not found in its header' % cls)
    names, depth, stmt, i = set(), 1, [], m.end()

    def declared(text):
        text = re.sub(r'^\s*((public|protected|private)\s*:\s*)+', '', text).strip()
        if not text or re.match(r'(friend|typedef|using|enum|struct|class|union)\b', text):
            return None
        if '(' in text:
            head = text[:text.index('(')]
            found = re.findall(r'~?\w+', head)
            name = found[-1] if found else None
            if name in (cls, '~' + cls) or (name and name.startswith('operator')):
                return None
            return name
        text = re.split(r'=|\[|(?<!:):(?!:)', text)[0]
        found = re.findall(r'\w+', text)
        return found[-1] if found else None

    while i < len(t) and depth > 0:
        c = t[i]
        if c == '{':
            if depth == 1:
                name = declared(''.join(stmt))
                if name:
                    names.add(name)
                stmt = []
            depth += 1
        elif c == '}':
            depth -= 1
        elif depth == 1:
            if c == ';':
                name = declared(''.join(stmt))
                if name:
                    names.add(name)
                stmt = []
            else:
                stmt.append(c)
        i += 1
    if not names:
        raise Failure('class %s declares no member the check could read' % cls)
    return names


def check_returns(site, function, body):
    """Whether `body` returns a value (returns_value); one doing so at a site with no `value` fails."""
    if not returns_value(body):
        return False
    if 'value' not in site:
        raise Failure('%s: handler %s returns a value, and the site names no value (its void pairs paste back no '
                      '`return x;`)' % (site['name'], function))
    return True


def check_body(function, body, site, members=()):
    """What the paste-back cannot see: a bare live-out, a bare member of the site's class, a substitution's
    left side inside a string literal, and a Continue inside a loop or switch."""
    raw = '\n'.join(body)
    text = blank(raw)
    for n, line in enumerate(text.split('\n'), 1):
        for name in sorted(members):
            if re.search(r'(?<![\w.>:~])%s\b' % re.escape(name), line):
                raise Failure('handler %s, body line %d: "%s", a member of %s, used bare (an implicit this-> the '
                              'move missed): %r' % (function, n, name, site['members_of'][1], body[n - 1]))
    for name in site['live_outs']:
        for n, line in enumerate(text.split('\n'), 1):
            if re.search(r'(?<![\w.>])%s\b' % re.escape(name), line.replace('ctx.' + name, '')):
                raise Failure('handler %s, body line %d: live-out "%s" used without the context: %r'
                              % (function, n, name, body[n - 1]))
    for name in site.get('in_scope', []):
        for n, line in enumerate(text.split('\n'), 1):
            if re.search(r'(?<![\w.>])%s\b' % re.escape(name), line):
                raise Failure('handler %s, body line %d: "%s", a name in scope at the site and not in its context, '
                              'used: %r' % (function, n, name, body[n - 1]))
    for a, _ in site['substitutions']:
        found = a.finditer(raw) if isinstance(a, re.Pattern) else re.finditer(re.escape(a), raw)
        for m in found:
            if in_literal(raw, m.start()):
                n = raw.count('\n', 0, m.start())
                raise Failure('handler %s, body line %d: %r, the left side of a substitution, stands inside a string '
                              'literal, which the paste-back would rewrite: %r'
                              % (function, n + 1, a.pattern if isinstance(a, re.Pattern) else a, body[n]))
    continues = [a for a, b in site['substitutions'] if b == 'break;']
    stack, pending, header_parens = [], False, None
    tokens = re.compile(r'\b(for|while|do|switch)\b|[{}();]|' + '|'.join(re.escape(c) for c in continues))
    pos = 0
    while True:
        m = tokens.search(text, pos)
        if not m:
            break
        tok = m.group(0)
        pos = m.end()
        if tok in continues:
            if pending or any(stack):
                line = text.count('\n', 0, m.start())
                raise Failure('handler %s, body line %d: a Continue inside a loop or switch (the old `break;` left '
                              'that, not the case): %r' % (function, line + 1, body[line]))
        elif tok in ('for', 'while', 'switch'):
            header_parens = 0
        elif tok == 'do':
            pending = True
        elif tok == '(' and header_parens is not None:
            header_parens += 1
        elif tok == ')' and header_parens is not None:
            header_parens -= 1
            if header_parens == 0:
                header_parens = None
                pending = True
        elif tok == '{':
            stack.append(pending)
            pending = False
        elif tok == '}':
            if stack:
                stack.pop()
        elif tok == ';' and header_parens is None:
            pending = False


def one_line(label_lines):
    return len(label_lines) == 1 and '{body}' in label_lines[0]


def site_spans(lines, site):
    """(first, close) of each switch in `lines` opened by the site's open lines (closed by the first
    close line after them) that holds one of the site's label lines."""
    n = len(site['open'])
    heads = tuple(l.split('{body}')[0] for l in site['labels'].values())
    spans = []
    for i in range(len(lines) - n + 1):
        if lines[i:i + n] != site['open']:
            continue
        end = next((k for k in range(i + n, len(lines)) if lines[k] == site['close'][0]), None)
        if end is not None and any(lines[k].startswith(heads) for k in range(i + n, end)):
            spans.append((i + n, end))
    return spans


def run_at(origins, site, label_lines):
    """(lines, i, k, end) of the first of `origins` (each a version's whole file, as lines) holding the
    run's label lines together inside the site's switch, blank lines between two of them allowed (B):
    i its first label line, k the line after its last, end the switch's close; holding them more than
    once fails; None where none holds them."""
    for lines in origins:
        at = []
        for first, end in site_spans(lines, site):
            for i in range(first, end):
                if lines[i] != label_lines[0]:
                    continue
                k = i + 1
                for want in label_lines[1:]:
                    while k < end and lines[k] == '':
                        k += 1
                    if k >= end or lines[k] != want:
                        break
                    k += 1
                else:
                    at.append((lines, i, k, end))
        if len(at) > 1:
            raise Failure('%s: the labels %s stand %d times in one version' % (site['name'], label_lines, len(at)))
        if at:
            return at[0]
    return None


def label_gaps(origins, site, label_lines):
    """B: the number of blank lines after each label line of the run but its last, where it stands."""
    found = run_at(origins, site, label_lines)
    if found is None:
        return [0] * (len(label_lines) - 1)
    lines, i, k, _ = found
    gaps, n = [], 0
    for line in lines[i + 1:k]:
        if line == '':
            n += 1
        else:
            gaps.append(n)
            n = 0
    return gaps


def falls_off(origins, site, label_lines):
    """E: whether the run stands last in its switch, where it stands, and its last statement (inside the
    closing `}` of a braced run, comment lines after it aside) is not a whole `return ...;`, `break;`,
    `continue;` or `goto ...;` line, so it falls off the switch's end."""
    found = run_at(origins, site, label_lines)
    if found is None:
        return False
    lines, i, k, end = found
    indent = ' ' * site['label_indent']
    if any(l.startswith(indent + 'case ') or l.startswith(indent + 'default:') for l in lines[k:end]):
        return False
    last = code_above(lines, end - 1)
    if last >= k and lines[last] == indent + '}' and lines[k] == indent + '{':
        last = code_above(lines, last - 1)
    if last < k:
        return False
    text = blank(lines[last]).strip()
    return not TERMINATOR.fullmatch(text.rstrip(';').split(';')[-1].strip() + ';' if text.endswith(';') else text)


def preamble(origins, site, label_line):
    """P: the number of lines between the switch's `{` and its first label, read in the first of
    `origins` holding the site's switch with `label_line` its first label; each must be a `//` comment
    line no shallower than the `{`; 0 where none holds it."""
    brace = len(site['open'][-1]) - len(site['open'][-1].lstrip(' '))
    label = re.compile(r' {%d}(case |default:)' % site['label_indent'])
    for lines in origins:
        found = []
        for first, end in site_spans(lines, site):
            k = next((j for j in range(first, end) if label.match(lines[j])), end)
            if k < end and lines[k] == label_line:
                found.append((first, k))
        if len(found) > 1:
            raise Failure('%s: the switch opening with %r stands %d times in one version'
                          % (site['name'], label_line.strip(), len(found)))
        if not found:
            continue
        first, k = found[0]
        for line in lines[first:k]:
            if not line.strip().startswith('//') or len(line) - len(line.lstrip(' ')) < brace:
                raise Failure('%s: before the first label, a line that is not a `//` comment at the indent of the '
                              'switch\'s `{` or deeper: %r' % (site['name'], line))
        return k - first
    return 0


def braced(origins, site, label_lines):
    """Whether the body under the run of labels `label_lines` is wrapped in `{` `}` at the label
    indent, read in the first of `origins` holding the run (run_at); False where none holds it."""
    indent = ' ' * site['label_indent']
    found = run_at(origins, site, label_lines)
    if found is not None:
        lines, _, k, _ = found
        if lines[k] != indent + '{':
            return False
        close = next((j for j in range(k + 1, len(lines)) if lines[j] == indent + '}'), None)
        after = close + 1 if close is not None else len(lines)
        while after < len(lines) and lines[after].startswith(indent + '//'):
            after += 1
        follows = lines[after] if after < len(lines) else None
        if follows is None or not (follows == site['close'][0] or follows.startswith(indent + 'case ')
                                   or follows.startswith(indent + 'default:')):
            raise Failure('%s: where it stands, the body under %s is a block followed by more lines, a shape the '
                          'paste-back cannot reproduce'
                          % (site['name'], [l.split('//')[0].strip() for l in label_lines]))
        return True
    return False


def substituted(site, body):
    restored = []
    for line in body:
        for a, b in site['substitutions']:
            line = a.sub(b, line) if isinstance(a, re.Pattern) else line.replace(a, b)
        restored.append(line)
    return restored


def paste(site, function, body, label_lines, wrapped=False, gaps=(), lead=0):
    """The switch lines one registered function stood for: its label line(s) and its body, in
    `{` `}` at the label indent when `wrapped` (a one-line case is never wrapped); `gaps[i]` blank lines
    after label line i (B); the body's first `lead` lines are the preamble, written before the labels
    at the indent of the switch's `{` (P)."""
    restored = substituted(site, body)
    brace = ' ' * (len(site['open'][-1]) - len(site['open'][-1].lstrip(' ')))
    if lead:
        if one_line(label_lines) or any(not l.strip().startswith('//') for l in restored[:lead]):
            raise Failure('%s: the switch has %d comment lines before its first label, and %s does not begin with '
                          'them' % (site['name'], lead, function))
        head = [brace + l for l in restored[:lead]]
        return head + paste(site, function, body[lead:], label_lines, wrapped, gaps)
    if any(gaps):
        spaced = []
        for n, line in enumerate(label_lines):
            spaced += [line] + [''] * (gaps[n] if n < len(gaps) else 0)
        label_lines = spaced
    if one_line(label_lines):
        return [label_lines[0].replace('{body}', ' '.join(l.strip() for l in restored))]
    if any('{body}' in l for l in label_lines):
        raise Failure('%s: %s is a one-line case sharing its body with another label' % (site['name'], function))
    indent = ' ' * site['label_indent']
    trailing = len(restored)
    while wrapped and trailing > 0 and restored[trailing - 1].startswith('//'):
        trailing -= 1
    restored = [indent + line if line else line for line in restored]
    if wrapped:
        return label_lines + [indent + '{'] + restored[:trailing] + [indent + '}'] + restored[trailing:]
    return label_lines + restored


FALLS = re.compile(r'    return (\w+)\(ctx\);')


def falls_into(site, function, body, rows, j):
    """`body` less its last line when that is `return G(ctx);`, a run falling into the run after it:
    G must be the function of the next row, and that row's label the next in the switch."""
    m = FALLS.fullmatch(body[-1]) if body else None
    if not m:
        return body
    order = list(site['labels'])
    after = order.index(rows[j - 1][0]) + 1
    if j >= len(rows) or rows[j][1] != m.group(1) or after >= len(order) or order[after] != rows[j][0]:
        raise Failure('%s: %s ends in a call of %s, which is not the function of the run after it in the switch'
                      % (site['name'], function, m.group(1)))
    return body[:-1]


def label_ids(line):
    """The spell ids of the `case N:` labels on a label line, comments and the `{body}` mark aside."""
    return [int(i) for i in re.findall(r'\bcase\s+(\d+)\s*:', blank(line.replace('{body}', '')))]


def check_label_lines(site):
    """Every label line holds exactly the ids the site maps to it."""
    for line in set(site['labels'].values()):
        want = sorted(i for i, l in site['labels'].items() if l == line)
        if re.sub(r'\bcase\s+\d+\s*:', '', blank(line.split('{body}')[0])).strip():
            raise Failure('%s: the label line %r holds more than `case N:` labels' % (site['name'], line.strip()))
        if sorted(label_ids(line)) != want:
            raise Failure('%s: the label line %r holds case %s, and the site maps %s to it'
                          % (site['name'], line.strip(), label_ids(line), want))


def standing_labels(rest, at, n, site):
    """A partly moved switch: the index of its close and {label: line index} of the labels still
    standing in the switch directly after the dispatch at `at` (n lines)."""
    first = at + n + len(site['open'])
    if rest[at + n:first] != site['open']:
        raise Failure('%s: the dispatch is not directly followed by the switch that still stands' % site['name'])
    end = next((k for k in range(first, len(rest)) if rest[k] == site['close'][0]), None)
    if end is None:
        raise Failure('%s: the switch that still stands has no closing line' % site['name'])
    standing = {}
    label = re.compile(r' {%d}case (\d+):' % site['label_indent'])
    for k in range(first, end):
        if not label.match(rest[k]):
            continue
        for spell in label_ids(rest[k]):
            if spell not in site['labels']:
                raise Failure('%s: case %d stands in the switch but is not one of its labels' % (site['name'], spell))
            if spell in standing:
                raise Failure('%s: case %d stands twice in the switch' % (site['name'], spell))
            if rest[k] != site['labels'].get(spell):
                raise Failure('%s: case %d in the switch is not its label line: %r' % (site['name'], spell, rest[k]))
            standing[spell] = k
    if list(standing) != [i for i in site['labels'] if i in standing]:
        raise Failure('%s: the labels standing in the switch, %s, are not in the spec\'s order'
                      % (site['name'], list(standing)))
    return end, standing


def moved_split(site, ids, standing):
    """A partly moved switch: this version's rows `ids` and standing labels split the labels."""
    order = list(site['labels'])
    if len(set(ids)) != len(ids):
        raise Failure('%s: the table registers a label twice: %s' % (site['name'], ids))
    stray = [i for i in ids if i not in site['labels']]
    if stray:
        raise Failure('%s: rows %s are not labels of the switch' % (site['name'], stray))
    both = [i for i in order if i in ids and i in standing]
    if both:
        raise Failure('%s: %s both registered and still standing in the switch' % (site['name'], both))
    neither = [i for i in order if i not in ids and i not in standing]
    if neither:
        raise Failure('%s: %s neither registered nor standing in the switch' % (site['name'], neither))
    if ids != [i for i in order if i in ids]:
        raise Failure('%s: the rows %s are not in the switch\'s original order' % (site['name'], ids))


def put_back(rest, at, n, site, pieces, end, standing):
    """A partly moved switch: `rest` with the dispatch at `at` (n lines) dropped and each registered
    function's lines pasted back into the switch that still stands, before its anchor."""
    order = list(site['labels'])
    runs = sum(1 for k, i in enumerate(order) if i not in standing and (k == 0 or order[k - 1] in standing))
    groups = []
    for labels, lines in pieces:
        k = order.index(labels[-1]) + 1
        while k < len(order) and order[k] not in standing:
            k += 1
        anchor = order[k] if k < len(order) else None
        if groups and groups[-1][0] == anchor:
            groups[-1][1].extend(lines)
        else:
            groups.append((anchor, list(lines)))
    if len(groups) != runs:
        raise Failure('%s: %d runs of moved labels for %d anchors' % (site['name'], runs, len(groups)))
    positions = [end if anchor is None else standing[anchor] for anchor, _ in groups]
    if positions != sorted(set(positions)):
        raise Failure('%s: the anchors do not stand in the switch in the original order' % site['name'])
    for (_, lines), pos in reversed(list(zip(groups, positions))):
        rest[pos:pos] = lines
    del rest[at:at + n]


def rebuild(text, spec, headers, handler_text=None, strict=True, origins=()):
    """`text` with every site's dispatch replaced by its switch, the added lines and the appended
    handler block dropped; the number of bodies pasted back, of labels, the names of the sites
    rebuilt, and the handler lines read. The handlers are `handler_text` (the handler file) when
    given, else the block appended to `text` from the tail marker. `strict` (the working tree)
    requires every site; otherwise (the base) a site whose dispatch is absent is left as it stands,
    and a file with neither handler source is returned as is. `headers` maps a site's members_of
    header path to its text; `origins` are the versions' lines the brace shapes are read from."""
    new = text.split('\n')
    marker = [i for i, l in enumerate(new) if l == spec.get('tail_marker')]
    if handler_text is not None:
        if marker:
            raise Failure('the handlers are in %s and a handler block is still appended to the sites\' file'
                          % spec['handlers'])
        handlers = handler_text.split('\n')
        rest = new
    else:
        if not strict and not marker:
            return text, 0, 0, [], []
        if len(marker) != 1:
            raise Failure('no handler file %s, and the tail marker found %d times' % (spec['handlers'], len(marker)))
        handlers = new[marker[0]:]
        rest = new[:marker[0]]
        # the block ends the file after one blank line: what is left ends in "\n", as the old file did
        if not rest or rest[-1] != '':
            raise Failure('no blank line before the tail marker')
    for added in spec['added']:
        at = [i for i, l in enumerate(rest) if l == added]
        if len(at) != 1:
            raise Failure('added line %r found %d times' % (added, len(at)))
        del rest[at[0]]
    pasted, labels, rebuilt, defaults = 0, 0, [], set()
    for site in spec['sites']:
        site = outcome_pairs(site)
        check_label_lines(site)
        n = len(site['dispatch'])
        at = [i for i in range(len(rest) - n + 1) if rest[i:i + n] == site['dispatch']]
        unknown = [i for i in site.get('deleted', []) if i not in site['labels']]
        if unknown:
            raise Failure('%s: deleted %s, not labels of its switch' % (site['name'], unknown))
        if site.get('deleted') and 'residual' not in site:
            raise Failure('%s: deletes labels and keeps its dispatch, which only a residual site does' % site['name'])
        if strict and 'gone' in site:
            gone(site, at, rest, handlers)
            continue
        if not strict and not at:
            continue
        if strict and site.get('deleted'):
            site = dict(site, labels={i: l for i, l in site['labels'].items() if i not in site['deleted']})
        if len(at) != 1:
            raise Failure('%s: the dispatch found %d times' % (site['name'], len(at)))
        if not site.get('traits'):
            raise Failure('%s: no traits named, so a default registered for it could not be seen' % site['name'])
        if 'Dispatch<%s>' % site['traits'] not in ''.join(site['dispatch']):
            raise Failure('%s: its traits %s do not appear as Dispatch<%s> in its dispatch lines'
                          % (site['name'], site['traits'], site['traits']))
        rows = site_table(handlers, site)
        ids = [r[0] for r in rows]
        if 'residual' in site:
            end, standing = standing_labels(rest, at[0], n, site)
            absent = [i for i in site.get('deleted', []) if i not in ids and i not in standing]
            if absent:
                site = dict(site, labels={i: l for i, l in site['labels'].items() if i not in absent})
            moved_split(site, ids, standing)
        elif sorted(ids) != sorted(site['labels']) or len(set(ids)) != len(ids):
            raise Failure('%s: table rows %s, labels %s' % (site['name'], ids, sorted(site['labels'])))
        header, cls = site['members_of']
        members = class_members(headers[header], cls)
        pieces, returned, pasted_before = [], False, pasted
        i = 0
        while i < len(rows):
            function = rows[i][1]
            j = i
            while j < len(rows) and rows[j][1] == function:
                j += 1
            if function in [r[1] for r in rows[:i]]:
                raise Failure('%s: %s registered in two runs of rows (its labels were not together)'
                              % (site['name'], function))
            body = falls_into(site, function, handler_body(handlers, function, site['context'],
                                                           site.get('value', 'void')), rows, j)
            check_body(function, body, site, members)
            returned = check_returns(site, function, body) or returned
            label_lines = []
            for spell, _ in rows[i:j]:
                line = site['labels'][spell]
                shared = [s for s, l in site['labels'].items() if l == line and s not in [r[0] for r in rows[i:j]]]
                if shared:
                    raise Failure('%s: the label line %r holds case %s, which %s does not register'
                                  % (site['name'], line.strip(), shared, function))
                if line not in label_lines:
                    label_lines.append(line)
            wrapped = not one_line(label_lines) and braced(origins, site, label_lines)
            if not one_line(label_lines) and falls_off(origins, site, label_lines):
                raise Failure('%s: the run under %s falls off the switch\'s end, which only a `default:` may'
                              % (site['name'], [l.split('//')[0].strip() for l in label_lines]))
            lead = preamble(origins, site, label_lines[0]) if 'residual' not in site else 0
            pieces.append(([r[0] for r in rows[i:j]], paste(site, function, body, label_lines, wrapped,
                                                             label_gaps(origins, site, label_lines), lead)))
            pasted += 1
            labels += j - i
            i = j
        switch = list(site['open'])
        for _, lines in pieces:
            switch += lines
        if 'default' in site and 'residual' in site:
            raise Failure('%s: a partly moved switch keeps its default: in the switch that still stands' % site['name'])
        if 'default' in site:
            function, line = default_function(handlers, site['traits'])
            defaults.add(line)
            if function in [r[1] for r in rows]:
                raise Failure('%s: the default %s is also a labelled row' % (site['name'], function))
            body = handler_body(handlers, function, site['context'], site.get('value', 'void'))
            if FALLS.fullmatch(body[-1] if body else ''):
                raise Failure('%s: the default %s ends in a call, and no run follows it' % (site['name'], function))
            check_body(function, body, site, members)
            returned = check_returns(site, function, body) or returned
            if falls_off(origins, site, [site['default']]):
                if not body or body[-1].strip() not in [a for a, b in site['substitutions'] if b == 'break;']:
                    raise Failure('%s: the default falls off the switch\'s end, and %s does not end in Continue'
                                  % (site['name'], function))
                body = body[:-1]
            switch += paste(site, function, body, [site['default']], braced(origins, site, [site['default']]))
            pasted += 1
        elif any(re.fullmatch(r'\s*registry\.RegisterDefault<%s>\(.*' % re.escape(site['traits']), l)
                 for l in handlers):
            raise Failure('%s: a default registered for a site whose switch had none' % site['name'])
        if strict and 'value' in site and pasted > pasted_before and not returned:
            raise Failure('%s: its value is %s, and no handler it registers returns one (no `Return(x)` to paste back '
                          'as `return x;`)' % (site['name'], site['value']))
        if 'residual' in site:
            put_back(rest, at[0], n, site, pieces, end, standing)
        else:
            switch += site['close']
            rest[at[0]:at[0] + n] = switch
        rebuilt.append(site['name'])
    heads = [i for i, l in enumerate(handlers) if re.match(r'static [\w:]+ %s\(' % re.escape(spec['rows_function']), l)]
    rows_body = set()
    if len(heads) == 1:
        end = next((k for k in range(heads[0], len(handlers)) if handlers[k] == '}'), len(handlers))
        rows_body = set(range(heads[0], end))
    for i, line in enumerate(blank('\n'.join(handlers)).split('\n')):
        if re.search(r'\bRegisterDefault\b', line) and i not in defaults:
            raise Failure('a RegisterDefault that is not a site\'s `registry.RegisterDefault<TRAITS>(&F);` line: %r'
                          % handlers[i])
        if strict and re.search(r'(\.|->|::)\s*Register\s*[<(]|\bRegister\s*<', line) and i not in rows_body:
            raise Failure('a Register outside %s, where every labelled row is registered from its table: %r'
                          % (spec['rows_function'], handlers[i]))
    return '\n'.join(rest), pasted, labels, rebuilt, handlers


def gone(site, at, rest, handlers):
    """A site deleted whole: nothing of it may stand in the working tree."""
    if at or 'Dispatch<%s>' % site['traits'] in '\n'.join(rest):
        raise Failure('%s: deleted, and its dispatch still stands' % site['name'])
    if any(m and m.group(1) == site['traits'] for m in (TABLE_HEAD.fullmatch(l) for l in handlers)):
        raise Failure('%s: deleted, and a table typed by %s still stands' % (site['name'], site['traits']))
    if any(re.search(r'\bRegisterDefault\s*<\s*%s\s*>' % re.escape(site['traits']), l) for l in handlers):
        raise Failure('%s: deleted, and a default is still registered for it' % site['name'])


LABEL_LINE = re.compile(r'( *)(case [^:]+|default)\s*:')


TERMINATOR = re.compile(r'(return\b.*|break|continue|goto\s+\w+)\s*;')


def comment_only(line):
    return line.strip().startswith(('//', '/*', '*'))


def code_above(lines, k):
    """The index of the last line at or above lines[k] that is neither blank nor comment-only; -1 for none."""
    while k >= 0 and (not lines[k].strip() or comment_only(lines[k])):
        k -= 1
    return k


def label_run(lines, i, where, cases_only=False):
    """The end of the label run starting at lines[i] (a `case`/`default:` line, a `case` line when
    `cases_only`); fails when the code above it could reach it: a label at its indent with no body of
    its own, or a run above whose last statement is not a return, break, continue or goto."""
    m = LABEL_LINE.match(lines[i])
    if not m or (cases_only and not m.group(2).startswith('case')):
        raise Failure('%s: %r is not a `case`%s line' % (where, lines[i], '' if cases_only else ' or `default:`'))
    indent = m.group(1)
    k = code_above(lines, i - 1)
    above = LABEL_LINE.match(lines[k]) if k >= 0 else None
    if above and above.group(1) == indent:
        last = blank(lines[k][above.end():]).strip()
        if not last:
            raise Failure('%s: %r has no body of its own and would fall into another' % (where, lines[k].strip()))
    elif k >= 0 and blank(lines[k]).strip().endswith('{'):
        last = None
    else:
        if k >= 0 and lines[k] == indent + '}':
            k = code_above(lines, k - 1)
        last = blank(lines[k]).strip() if k >= 0 else ''
        h = code_above(lines, k - 1) if k >= 0 else -1
        header = blank(lines[h]).strip() if h >= 0 else ''
        if header and not header.endswith((';', '{', '}', ':')):
            raise Failure('%s: the run above may fall into it (%r ends under %r)'
                          % (where, lines[k].strip(), lines[h].strip()))
    if last is not None and not TERMINATOR.fullmatch(last.rstrip(';').split(';')[-1].strip() + ';'
                                                     if last.endswith(';') else last):
        raise Failure('%s: the run above may fall into it (%r)' % (where, lines[k].strip()))
    for j in range(i + 1, len(lines)):
        line = lines[j]
        if line.startswith(indent + 'case ') or line.startswith(indent + 'default:'):
            return j
        if line.strip() and not comment_only(line) and len(line) - len(line.lstrip(' ')) < len(indent):
            return j
    raise Failure('%s: the run under %r has no end' % (where, lines[i].strip()))


def cut(text, spec):
    """The rebuilt base less what the working tree deleted; the number of blocks and of lines cut."""
    lines = text.split('\n')

    def label_at(site, spell):
        label = site['labels'][spell]
        found = [i for i, l in enumerate(lines)
                 if (l.startswith(label.split('{body}')[0]) if '{body}' in label else l == label)]
        if len(found) > 1:
            raise Failure('%s: label %d found %d times in the base' % (site['name'], spell, len(found)))
        return found[0] if found else None

    ranges = []
    for site in spec['sites']:
        found = []
        if 'gone' in site:
            found = [i for i in (label_at(site, spell) for spell in site['labels']) if i is not None]
        if found:
            first = min(found)
            start = first - len(site['open'])
            if lines[start:first] != site['open']:
                raise Failure('%s: deleted, and its switch does not open above its first label' % site['name'])
            end = next((k for k in range(first, len(lines)) if lines[k] == site['close'][0]), None)
            if end is None:
                raise Failure('%s: deleted, and its switch has no closing line' % site['name'])
            above, below = site['gone']
            if start - above < 0 or end + 1 + below > len(lines):
                raise Failure('%s: deleted, and the lines GONE names run past the file' % site['name'])
            ranges.append((start - above, end + 1 + below))
        for spell in site.get('deleted', []):
            if len(label_ids(site['labels'][spell])) > 1:
                raise Failure('%s: deleted label %d shares its line with other labels' % (site['name'], spell))
            i = label_at(site, spell)
            if i is not None:
                ranges.append((i, label_run(lines, i, '%s: deleted label %d' % (site['name'], spell))))
    site_labels = [l for site in spec['sites'] for l in site['labels'].values()]
    for head, count in spec.get('cuts', []):
        where = 'the cut at %r' % head[0].strip()
        if any(head[0] == l or ('{body}' in l and head[0].startswith(l.split('{body}')[0])) for l in site_labels):
            raise Failure('%s: a label of a site, which its DELETED or GONE cuts' % where)
        at = [i for i in range(len(lines) - len(head) + 1) if lines[i:i + len(head)] == head]
        if len(at) > 1:
            raise Failure('%s: found %d times in the base' % (where, len(at)))
        if at:
            end = label_run(lines, at[0], where, cases_only=True)
            if at[0] + count != end:
                raise Failure('%s: %d lines, and its label run is %d' % (where, count, end - at[0]))
            ranges.append((at[0], end))
    ranges.sort()
    if any(a[1] > b[0] for a, b in zip(ranges, ranges[1:])):
        raise Failure('two cuts overlap')
    for a, b in reversed(ranges):
        del lines[a:b]
    return '\n'.join(lines), len(ranges), sum(b - a for a, b in ranges)


def first_difference(a, b, other='rebuilt'):
    """The first line where `a` (the base) and `b` (named `other`) differ, or their lengths."""
    al, bl = a.split('\n'), b.split('\n')
    for n, (x, y) in enumerate(zip(al, bl), 1):
        if x != y:
            return 'line %d: base %r, %s %r' % (n, x, other, y)
    return 'lengths differ: base %d lines, %s %d' % (len(al), other, len(bl))


def verify(rel, old_text, new_text, spec, headers, out=print, old_handlers=None, new_handlers=None):
    """`old_handlers` and `new_handlers`: the handler file's text in that version, None where it has none."""
    try:
        origins = [old_text.split('\n'), new_text.split('\n')]
        rebuilt, pasted, labels, sites, handlers = rebuild(new_text, spec, headers, new_handlers, origins=origins)
        base, base_pasted, _, base_sites, _ = rebuild(old_text, spec, headers, old_handlers, strict=False,
                                                      origins=origins)
        base, cuts, cut_lines = cut(base, spec)
    except Failure as e:
        out('%s: FAILED: %s' % (rel, e))
        return 1, 0
    defined = [m.group(1) for m in (re.match(r'static SpellHandlerOutcome<[\w:]+> (\w+)\(', l)
                                    for l in handlers) if m]
    if len(defined) != pasted:
        out('%s: FAILED: %d handlers defined, %d registered and pasted back' % (rel, len(defined), pasted))
        return 1, pasted
    if rebuilt != base:
        out('%s: DIFFERS from the base after pasting back %d bodies at %d sites: %s' % (
            rel, pasted, len(sites), first_difference(base, rebuilt)))
        return 1, pasted
    out('%s: IDENTICAL to the base, byte for byte, with %d/%d bodies pasted back at their %d labels in %d sites '
        '(the base had %d of the sites moved: %d bodies pasted back there), the base less %d cuts (%d lines)' % (
            rel, pasted, len(defined), labels, len(sites), len(base_sites), base_pasted, cuts, cut_lines))
    return 0, pasted


SHA = re.compile(r'[0-9a-f]{7,40}')


def git_runner(root):
    """git(*args) in `root`: (exit code, stdout, stderr)."""
    def git(*args):
        try:
            r = subprocess.run(['git', '-C', root] + list(args), capture_output=True)
        except OSError as e:
            return 1, '', str(e)
        return r.returncode, r.stdout.decode('utf-8'), r.stderr.decode('utf-8', 'replace').strip()
    return git


def resolve(what, spelling, git):
    """The full SHA of the one commit `spelling` (a commit's hex SHA, full or abbreviated) names."""
    if not SHA.fullmatch(spelling):
        raise Failure("the %s %r is not spelled as a commit's hex SHA" % (what, spelling))
    code, full, _ = git('rev-parse', '--verify', '--quiet', spelling + '^{commit}')
    full = full.strip()
    if code != 0 or not full:
        code, objects, _ = git('rev-parse', '--disambiguate=' + spelling)
        if code == 0 and len(objects.split()) > 1:
            raise Failure('the %s %s is ambiguous: %d objects of the repository begin with it'
                          % (what, spelling, len(objects.split())))
        raise Failure('the %s %s is not a commit of the repository' % (what, spelling))
    if not full.startswith(spelling):
        raise Failure('the %s %s names %s, not the commit it spells (a ref of that name)' % (what, spelling, full))
    return full


def original_base(rel, spec, base, git, what='base'):
    """The full SHA --original reads `rel` at: the entry's `original`, else ORIGINAL (ORIGINALS);
    `base` is the full SHA of the commit no original is ahead of, the base or the anchor (`what`)."""
    spelling = spec.get('original', ORIGINAL)
    ref = resolve('original', spelling, git)
    if git('merge-base', '--is-ancestor', ref, base)[0] != 0:
        raise Failure('the original %s is ahead of the %s %s (it is neither the %s nor an ancestor of it)'
                      % (spelling, what, base, what))
    if not any(git('cat-file', '-e', '%s:%s' % (ref, p))[0] == 0 for p in (rel, spec.get('renamed_from')) if p):
        raise Failure('the original %s does not hold the file' % spelling)
    return ref


def read_at(rel, spec, base, original, git, read, anchor=None):
    """(ref, path): the full SHA a SITES key is read at, the base or under --original its original,
    and the path there: the key, or `was`, its renamed_from. The working tree must hold the key and not
    `was`, and the commit exactly one of the two. `anchor`, a full SHA, replaces the base in the
    original's ancestry check."""
    full_base = resolve('base', base, git)
    if not original:
        ref = full_base
    elif anchor is not None:
        ref = original_base(rel, spec, anchor, git, 'anchor')
    else:
        ref = original_base(rel, spec, full_base, git)
    was = spec.get('renamed_from')

    def in_tree(p):
        try:
            read(p)
        except OSError:
            return False
        return True
    if not in_tree(rel):
        raise Failure('the working tree has no such file')
    if was and in_tree(was):
        raise Failure('its renamed_from %s is still in the working tree' % was)
    have = [p for p in (rel, was) if p and git('cat-file', '-e', '%s:%s' % (ref, p))[0] == 0]
    if len(have) != 1:
        raise Failure('%s at %s' % ('both it and its renamed_from %s are there' % was if have else
                                    'neither it nor its renamed_from %s is there' % was if was else 'it is not there',
                                    ref))
    return ref, have[0]


BLOCK_KEYS = ('base', 'origin', 'header', 'first', 'lines', 'added')

# The files a type is compiled from: a definition in any other file is no twin.
TYPE_SOURCES = ('.c', '.cc', '.cpp', '.cxx', '.h', '.hh', '.hpp', '.hxx', '.inc', '.inl', '.ipp')


def block_types(block):
    """The names of the types `block` defines: each column-0 `struct N` or `class N` line, comments and
    literals aside, that is not a declaration ending in `;`."""
    names = []
    for line in blank('\n'.join(block)).split('\n'):
        m = re.match(r'(?:struct|class)\s+(\w+)', line)
        if m and not line.rstrip().endswith(';'):
            names.append(m.group(1))
    return names


def definitions(text, name):
    """The number of definitions of the type `name` in `text`, comments and literals aside: `struct` or
    `class`, the name, then its body, past `final` and a base clause."""
    if name not in text:
        return 0
    return len(re.findall(r'\b(?:struct|class)\s+%s\b\s*(?:final\b\s*)?(?::(?!:)[^;{}()]*)?\{' % re.escape(name),
                          blank(text)))


def check_block(entry, sites, git, read, scan, anchor=None):
    """The line --check prints for one BLOCKS entry, its checks 1 to 5 passed (BLOCKS); `scan` reads a
    file under src/ for the twin search, `anchor` (a full SHA) takes HEAD's place in the base's ancestry."""
    if sorted(entry) != sorted(BLOCK_KEYS):
        raise Failure('the entry\'s keys are %s, and a block entry has exactly %s' % (sorted(entry), list(BLOCK_KEYS)))
    origin, header, n = entry['origin'], entry['header'], entry['lines']
    if not isinstance(n, int) or n < 1:
        raise Failure('its lines %r is not a positive count' % (n,))
    base = resolve('base', entry['base'], git)
    if git('merge-base', '--is-ancestor', base, anchor or 'HEAD')[0] != 0:
        raise Failure('the base %s is not an ancestor of %s' % (entry['base'], 'the anchor ' + anchor if anchor
                                                                else 'HEAD'))
    code, old, err = git('show', '%s:%s' % (base, origin))
    if code != 0:
        raise Failure('cannot read the origin at the base %s from git: %s' % (base, err))
    old = old.split('\n')
    at = [i for i, l in enumerate(old) if l == entry['first']]
    if len(at) != 1:
        raise Failure('the block\'s first line found %d times in the origin at the base' % len(at))
    block = old[at[0]:at[0] + n]
    if len(block) != n:
        raise Failure('the block\'s %d lines run past the end of the origin at the base' % n)
    code_text = blank('\n'.join(block))
    if code_text.count('{') != code_text.count('}'):
        raise Failure('the block\'s braces do not balance')
    if block[-1] != '};':
        raise Failure('the block\'s last line is not `};`, the close of a type: %r' % block[-1])
    types = block_types(block)
    if not types:
        raise Failure('the block defines no type (no column-0 `struct N` or `class N` line)')
    spec, others = sites.get(origin), []
    if spec is None:
        try:
            new = read(origin)
        except OSError:
            raise Failure('the working tree has no %s' % origin)
        where = 'in the working tree'
    else:
        if 'original' not in spec:
            raise Failure('the origin has a SITES entry, and the entry names no original to read it at')
        ref = resolve('original', spec['original'], git)
        if git('merge-base', '--is-ancestor', base, ref)[0] != 0:
            raise Failure('the original %s of the origin\'s SITES entry does not descend from the base %s'
                          % (spec['original'], entry['base']))
        code, new, err = git('show', '%s:%s' % (ref, origin))
        if code != 0:
            raise Failure('cannot read the origin at its original %s from git: %s' % (ref, err))
        where = 'at ' + ref
        others.append(('%s at %s' % (origin, ref), new))
    try:
        held = read(header)
    except OSError:
        raise Failure('the working tree has no header %s' % header)
    lines = held.split('\n')
    found = [k for k in range(len(lines) - n + 1) if lines[k:k + n] == block]
    if len(found) != 1:
        raise Failure('the header holds the block %d times (contiguous, byte for byte)' % len(found))
    code, listed, err = git('ls-files', '-z', '--', 'src')
    if code != 0:
        raise Failure('cannot list the files under src/: %s' % err)
    for path in listed.split('\0'):
        if path.endswith(TYPE_SOURCES) and path != header:
            try:
                others.append((path, scan(path)))
            except OSError:
                continue
    for name in types:
        if definitions(held, name) != 1:
            raise Failure('type %s is defined %d times in the header' % (name, definitions(held, name)))
        for place, text in others:
            if definitions(text, name):
                raise Failure('type %s is defined again in %s, a twin of the block\'s' % (name, place))
    rest = new.split('\n')
    for added in entry['added']:
        hits = [k for k, l in enumerate(rest) if l == added]
        if len(hits) != 1:
            raise Failure('added line %r found %d times in the origin as read' % (added, len(hits)))
        del rest[hits[0]]
    end = at[0] + n
    while end + 1 < len(old) and old[end] == '':
        end += 1
    want = old[:at[0]] + old[end:]
    if rest != want:
        raise Failure('the origin as read, less its added lines, is not the base\'s less the block: %s'
                      % first_difference('\n'.join(want), '\n'.join(rest), 'read'))
    return ('%s -> %s: IDENTICAL to the base %s, byte for byte: the block of %d lines at its line %d stands once in '
            'the header; the origin, read %s, is the base\'s less the block and the %d blank line(s) after it, '
            'with %d added line(s); its types %s are defined nowhere else under src/'
            % (origin, header, base, n, at[0] + 1, where, end - at[0] - n, len(entry['added']), ', '.join(types)))


def check(root, base, out=print, original=False, git=None, read=None, sites=None, anchor=None, blocks=None):
    """`git` and `read` (a path's text in the working tree) default to the repository at `root`;
    `anchor`, under --original, is the commit no original may be ahead of, in the base's place;
    `sites` and `blocks` default to SITES and BLOCKS."""
    git = git or git_runner(root)
    sites = SITES if sites is None else sites

    def read_tree(rel, errors='strict'):
        with open(os.path.join(root, *rel.split('/')), encoding='utf-8', errors=errors, newline='') as fh:
            return fh.read()
    scan = read or (lambda rel: read_tree(rel, 'replace'))
    read = read or read_tree
    if anchor is not None:
        try:
            anchor = resolve('anchor', anchor, git)
        except Failure as e:
            out('verbatim: FAILED: %s' % e)
            return 1
        out('verbatim: each original is measured against the anchor %s' % anchor)
    rc = 0
    for rel, spec in sites.items():
        try:
            ref, at = read_at(rel, spec, base, original, git, read, anchor)
            new_text = read(rel)
        except Failure as e:
            out('%s: FAILED: %s' % (rel, e))
            rc = 1
            continue
        code, old_text, err = git('show', '%s:%s' % (ref, at))
        if code != 0:
            out('%s: FAILED: cannot read it at %s from git: %s' % (rel, ref, err))
            rc = 1
            continue
        try:
            new_handlers = read(spec['handlers'])
        except OSError:
            new_handlers = None
        code, listed, err = git('ls-tree', '--name-only', ref, '--', spec['handlers'])
        old_handlers = None
        if code == 0 and listed.split():
            code, old_handlers, err = git('show', '%s:%s' % (ref, spec['handlers']))
        if code != 0:
            out('%s: FAILED: cannot read %s at %s from git: %s' % (rel, spec['handlers'], ref, err))
            rc = 1
            continue
        headers = {}
        for site in spec['sites']:
            header = site['members_of'][0]
            try:
                headers[header] = read(header)
            except OSError:
                out('%s: FAILED: the working tree has no %s, its members_of' % (rel, header))
                rc = 1
                break
        else:
            got, _ = verify(rel, old_text, new_text, spec, headers, out, old_handlers, new_handlers)
            rc |= got
    for entry in BLOCKS if blocks is None else blocks:
        try:
            out(check_block(entry, sites, git, read, scan, anchor))
        except Failure as e:
            out('%s -> %s: FAILED: %s' % (entry.get('origin'), entry.get('header'), e))
            rc = 1
    out('verbatim: %s' % ('OK' if rc == 0 else 'FAILED'))
    return rc


# ---- Self-test fixtures: a site with a shared body, a Continue, and a loop's own break; a second
# ---- site with one-line cases writing a live-out and a registered `default:`.
SELF_OLD = '''#include "A.h"

void Thing::Handle(bool apply)
{
    Unit* target = GetTarget();
    switch (GetId())
    {
        case 1:                                 // One
        case 2:                                 // Two
        {
            target->Cast(this);
            return;
        }
        case 3:                                 // Three
        {
            for (int i = 0; i < 3; ++i)
            {
                if (i == 1)
                    break;
            }
            break;
        }
    }
    uint32 rank;
    switch (GetId())
    {
        case 7: rank = 1; break;    // Rank 1
        case 8: rank = 2; break;    // Rank 2
        default:
            Log(GetId());
            return;
    }
    target->Tail(rank);
}
'''

SELF_SITES = '''#include "A.h"
#include "Handlers.h"

void Thing::Handle(bool apply)
{
    Unit* target = GetTarget();
    SelfContext handlerContext(this, target);
    if (Dispatch<SelfSite>(handlerContext).IsReturn())
    {
        return;
    }
    uint32 rank;
    RankContext rankContext(this, rank);
    if (Dispatch<RankSite>(rankContext).IsReturn())
    {
        return;
    }
    target->Tail(rank);
}
'''

SELF_BLOCK = '''static SpellHandlerOutcome<void> One(SelfContext& ctx)
{
    ctx.target->Cast(ctx.aura);
    return SpellHandlerOutcome<void>::Return();
}

static SpellHandlerOutcome<void> Three(SelfContext& ctx)
{
    for (int i = 0; i < 3; ++i)
    {
        if (i == 1)
            break;
    }
    return SpellHandlerOutcome<void>::Continue();
}

static SpellHandlerOutcome<void> Rank7(RankContext& ctx)
{
    ctx.rank = 1;
    return SpellHandlerOutcome<void>::Continue();
}

static SpellHandlerOutcome<void> Rank8(RankContext& ctx)
{
    ctx.rank = 2;
    return SpellHandlerOutcome<void>::Continue();
}

static SpellHandlerOutcome<void> RankDefault(RankContext& ctx)
{
    Log(ctx.aura->GetId());
    return SpellHandlerOutcome<void>::Return();
}

template <class Site, std::size_t N>
static uint32 RegisterRows(Registry& registry, Row<Site> const (&rows)[N])
{
    for (Row<Site> const& row : rows)
    {
        registry.Register<Site>(row.spellId, row.function);
    }
    return uint32(N);
}

void Register(Registry& registry)
{
    static Row<SelfSite> const rows[] =
    {
        { 1, &One },
        { 2, &One },
        { 3, &Three },
    };
    static Row<RankSite> const ranks[] =
    {
        { 7, &Rank7 },
        { 8, &Rank8 },
    };
    registry.RegisterDefault<RankSite>(&RankDefault);
}
'''

# The handler file, and the same handlers appended to the sites' file behind the tail marker.
SELF_HANDLERS = '#include "Handlers.h"\n\n' + SELF_BLOCK
SELF_BESIDE = SELF_SITES + '\n// Handlers:\n\n' + SELF_BLOCK

# The file after an earlier move of the first site only, its body beside the sites: a base the
# second site's move is checked against, whose own dispatch is pasted back the same way; its rows
# are of an untyped Row, so its table is read by the name the site gives.
SELF_MID = '''#include "A.h"
#include "Handlers.h"

void Thing::Handle(bool apply)
{
    Unit* target = GetTarget();
    SelfContext handlerContext(this, target);
    if (Dispatch<SelfSite>(handlerContext).IsReturn())
    {
        return;
    }
    uint32 rank;
    switch (GetId())
    {
        case 7: rank = 1; break;    // Rank 1
        case 8: rank = 2; break;    // Rank 2
        default:
            Log(GetId());
            return;
    }
    target->Tail(rank);
}

// Handlers:

static SpellHandlerOutcome<void> One(SelfContext& ctx)
{
    ctx.target->Cast(ctx.aura);
    return SpellHandlerOutcome<void>::Return();
}

static SpellHandlerOutcome<void> Three(SelfContext& ctx)
{
    for (int i = 0; i < 3; ++i)
    {
        if (i == 1)
            break;
    }
    return SpellHandlerOutcome<void>::Continue();
}

void Register(Registry& registry)
{
    struct Row
    {
        uint32 spellId;
        Function function;
    };

    static Row const rows[] =
    {
        { 1, &One },
        { 2, &One },
        { 3, &Three },
    };
}
'''

SELF_SPEC = {
    'handlers': 'Handlers.cpp',
    'rows_function': 'RegisterRows',
    'added': ['#include "Handlers.h"'],
    'tail_marker': '// Handlers:',
    'sites': [{
        'name': 'fixture',
        'dispatch': ['    SelfContext handlerContext(this, target);',
                     '    if (Dispatch<SelfSite>(handlerContext).IsReturn())', '    {', '        return;', '    }'],
        'open': ['    switch (GetId())', '    {'],
        'close': ['    }'],
        'label_indent': 8,
        'table': 'rows',
        'traits': 'SelfSite',
        'context': 'SelfContext',
        'live_outs': ['target'],
        'in_scope': ['apply'],
        'members_of': ('Thing.h', 'Thing'),
        'substitutions': [('ctx.target', 'target'), ('ctx.aura', 'this')] + VOID_SUBSTITUTIONS,
        'labels': {1: '        case 1:                                 // One',
                   2: '        case 2:                                 // Two',
                   3: '        case 3:                                 // Three'},
    }, {
        'name': 'fixture ranks',
        'dispatch': ['    RankContext rankContext(this, rank);', '    if (Dispatch<RankSite>(rankContext).IsReturn())',
                     '    {', '        return;', '    }'],
        'open': ['    switch (GetId())', '    {'],
        'close': ['    }'],
        'label_indent': 8,
        'traits': 'RankSite',
        'default': '        default:',
        'context': 'RankContext',
        'live_outs': ['rank'],
        'in_scope': ['apply', 'target'],
        'members_of': ('Thing.h', 'Thing'),
        'substitutions': [('ctx.rank', 'rank'), ('ctx.aura->GetId()', 'GetId()'), ('ctx.aura', 'this')]
        + VOID_SUBSTITUTIONS,
        'labels': {7: '        case 7: {body}    // Rank 1',
                   8: '        case 8: {body}    // Rank 2'},
    }],
}


# ---- A partly moved switch: two runs registered (the second anchored at the close), the rest still standing.
SELF_OLD_PART = '''#include "A.h"

void Thing::Remove(bool apply)
{
    Unit* target = GetTarget();
    switch (GetId())
    {
        case 1:                                 // One
        {
            target->Drop(1);
            return;
        }
        case 2:                                 // Two
        {
            target->Drop(2);
            return;
        }
        case 3:                                 // Three
        {
            target->Drop(3);
            return;
        }
        case 4:                                 // Four
        case 5:                                 // Five
        {
            target->Drop(4);
            return;
        }
        case 6:                                 // Six
        {
            break;
        }
    }
    target->Tail();
}
'''

SELF_SITES_PART = '''#include "A.h"
#include "Handlers.h"

void Thing::Remove(bool apply)
{
    Unit* target = GetTarget();
    RemoveContext removeContext(this, target);
    if (Dispatch<RemoveSite>(removeContext).IsReturn())
    {
        return;
    }
    switch (GetId())
    {
        case 1:                                 // One
        {
            target->Drop(1);
            return;
        }
        case 3:                                 // Three
        {
            target->Drop(3);
            return;
        }
    }
    target->Tail();
}
'''

SELF_HANDLERS_PART = '''#include "Handlers.h"

static SpellHandlerOutcome<void> Two(RemoveContext& ctx)
{
    ctx.target->Drop(2);
    return SpellHandlerOutcome<void>::Return();
}

static SpellHandlerOutcome<void> Four(RemoveContext& ctx)
{
    ctx.target->Drop(4);
    return SpellHandlerOutcome<void>::Return();
}

static SpellHandlerOutcome<void> Six(RemoveContext& /*ctx*/)
{
    return SpellHandlerOutcome<void>::Continue();
}

template <class Site, std::size_t N>
static uint32 RegisterRows(Registry& registry, Row<Site> const (&rows)[N])
{
    for (Row<Site> const& row : rows)
    {
        registry.Register<Site>(row.spellId, row.function);
    }
    return uint32(N);
}

void Register(Registry& registry)
{
    static Row<RemoveSite> const removed[] =
    {
        { 2, &Two },
        { 4, &Four },
        { 5, &Four },
        { 6, &Six },
    };
}
'''

SELF_SPEC_PART = {
    'handlers': 'Handlers.cpp',
    'rows_function': 'RegisterRows',
    'added': ['#include "Handlers.h"'],
    'tail_marker': '// Handlers:',
    'sites': [{
        'name': 'fixture part',
        'dispatch': ['    RemoveContext removeContext(this, target);',
                     '    if (Dispatch<RemoveSite>(removeContext).IsReturn())', '    {', '        return;', '    }'],
        'open': ['    switch (GetId())', '    {'],
        'close': ['    }'],
        'residual': True,
        'label_indent': 8,
        'traits': 'RemoveSite',
        'context': 'RemoveContext',
        'live_outs': ['target'],
        'in_scope': ['apply'],
        'members_of': ('Thing.h', 'Thing'),
        'substitutions': [('ctx.target', 'target'), ('ctx.aura', 'this')] + VOID_SUBSTITUTIONS,
        'labels': {1: '        case 1:                                 // One',
                   2: '        case 2:                                 // Two',
                   3: '        case 3:                                 // Three',
                   4: '        case 4:                                 // Four',
                   5: '        case 5:                                 // Five',
                   6: '        case 6:                                 // Six'},
    }],
}


# ---- A partly moved switch of 18 labels (9 and 10 share a body), written out for any split: the
# ---- rows a version registers, in table order, and the labels still standing in its switch.
GEN_ORDER = list(range(1, 19))
GEN_SPEC = dict(SELF_SPEC_PART, sites=[dict(SELF_SPEC_PART['sites'][0], name='generated part', labels={
    i: '        case %d:%s// Label %d' % (i, ' ' * (33 - len(str(i))), i) for i in GEN_ORDER})])


def gen_version(rows, standing=None, switch_order=None, unbraced=(), table='removed'):
    """(sites' file, handler file) of a version registering `rows` in the table `table`, with
    `standing` (by default every label not registered) in its switch in `switch_order` (by default
    the original order), the bodies under the labels in `unbraced` not wrapped in braces; no rows:
    the file before any move, and no handler file."""
    if standing is None:
        standing = [i for i in GEN_ORDER if i not in rows]
    labels = GEN_SPEC['sites'][0]['labels']
    out = ['#include "A.h"'] + (['#include "Handlers.h"'] if rows else []) + [
        '', 'void Thing::Remove(bool apply)', '{', '    Unit* target = GetTarget();']
    if rows:
        out += GEN_SPEC['sites'][0]['dispatch']
    out += ['    switch (GetId())', '    {']
    for i in switch_order or GEN_ORDER:
        if i in standing:
            out.append(labels[i])
            if i != 9:
                body = ['            target->Drop(%d);' % (9 if i == 10 else i), '            return;']
                out += body if i in unbraced else ['        {'] + body + ['        }']
    out += ['    }', '    target->Tail();', '}', '']
    if not rows:
        return '\n'.join(out), None
    handlers = ['#include "Handlers.h"', '']
    for i in sorted(set(9 if i == 10 else i for i in rows)):
        handlers += ['static SpellHandlerOutcome<void> Drop%d(RemoveContext& ctx)' % i, '{',
                     '    ctx.target->Drop(%d);' % i, '    return SpellHandlerOutcome<void>::Return();', '}', '']
    handlers += ['template <class Site, std::size_t N>',
                 'static uint32 RegisterRows(Registry& registry, Row<Site> const (&rows)[N])', '{',
                 '    for (Row<Site> const& row : rows)', '    {',
                 '        registry.Register<Site>(row.spellId, row.function);', '    }', '    return uint32(N);', '}',
                 '', 'void Register(Registry& registry)', '{', '    static Row<RemoveSite> const %s[] =' % table,
                 '    {']
    handlers += ['        { %d, &Drop%d },' % (i, 9 if i == 10 else i) for i in rows]
    handlers += ['    };', '}', '']
    return '\n'.join(out), '\n'.join(handlers)


SELF_HEADERS = {'Thing.h': """class Other { void Tail(); };
class  Thing
{
        friend struct Helper;
    public:
        Thing(int x) : m_x(x) {}
        ~Thing();
        void Handle(bool apply);
        uint32 GetId() const;
        Unit* GetCaster() const { return Lookup(m_casterGuid); }   // Lookup is not a member
        bool IsPositive() { return m_positive; }
        int& operator[](int i);
    protected:
        Modifier m_modifier;
        proto::Link m_link;
        bool m_positive : 1;
        int m_table[4];
        enum { KIND_A, KIND_B };
    private:
        uint64 m_casterGuid = 0;
};
"""}


# ---- BLOCKS: a source file before and after its comparator types moved into a header, the move adding
# ---- the header's include and an explicit instantiation; two blank lines stand after the block.
BLOCK_LINES = ['// Order of the far targets',
               'class Unit;',
               'template <class A>',
               'struct Pair',
               '{',
               '    typedef A first_type;',
               '};',
               '',
               'struct FarOrder : public Pair<int>',
               '{',
               '    // the far side first: a "{" here opens no body',
               '    bool operator()(int a, int b) const { return a > b; }',
               '};']

BLOCK_ORIGIN = '''#include "Far.h"

enum FarKind
{
    FAR_ONE,
};

void Far::Pick()
{
}

''' + '\n'.join(BLOCK_LINES) + '''


void Far::Sort()
{
    list.sort(FarOrder());
}
'''

BLOCK_MOVED = '''#include "Far.h"
#include "spells/FarOrder.h"

enum FarKind
{
    FAR_ONE,
};

void Far::Pick()
{
}
template void Far::Pick<int>();

void Far::Sort()
{
    list.sort(FarOrder());
}
'''

BLOCK_HEADER = '#pragma once\n\n' + '\n'.join(BLOCK_LINES) + '\n'

BLOCK_ENTRY = {'base': '2b00000', 'origin': 'src/game/Far.cpp', 'header': 'src/game/spells/FarOrder.h',
               'first': '// Order of the far targets', 'lines': 13,
               'added': ['#include "spells/FarOrder.h"', 'template void Far::Pick<int>();']}


def self_test():
    failures = []
    got = sorted(class_members(SELF_HEADERS['Thing.h'], 'Thing'))
    want = ['GetCaster', 'GetId', 'Handle', 'IsPositive', 'm_casterGuid', 'm_link', 'm_modifier', 'm_positive',
            'm_table']
    print('self-test: %-66s %s' % ('the class-scope member names are read', 'PASS' if got == want else 'FAIL'))
    if got != want:
        failures.append('class_members: got %r, expected %r' % (got, want))

    def run(label, want_rc, needle='', swap=None, old_text=SELF_OLD, spec=SELF_SPEC, sites=SELF_SITES,
            handlers=SELF_HANDLERS, old_handlers=None, bound=None):
        """`swap` (a, b) replaces a by b in the sites' file and the handler file; a must be in one. `bound`:
        the check runs in a thread given that many seconds, and one still running then fails the row as a
        hang (the thread is left to spin, and the process ends without it)."""
        if swap:
            a, b = swap
            if a not in sites + (handlers or ''):
                failures.append('%s: the mutation %r matches nothing' % (label, a))
                print('self-test: %-66s %s' % (label, 'FAIL'))
                return
            sites = sites.replace(a, b)
            handlers = handlers.replace(a, b) if handlers is not None else None
        got = []

        def checked():
            try:
                return verify('fixture', old_text, sites, spec, SELF_HEADERS, got.append, old_handlers, handlers)[0]
            except Exception as e:                              # a crash fails the row
                got.append('crashed: %r' % e)
                return 2
        if bound is None:
            rc = checked()
        else:
            ended = []
            thread = threading.Thread(target=lambda: ended.append(checked()), daemon=True)
            thread.start()
            thread.join(bound)
            rc = ended[0] if ended else 3
            if not ended:
                got.append('hung: still checking after %d s' % bound)
        text = '\n'.join(got)
        ok = rc == want_rc and needle in text
        print('self-test: %-66s %s' % (label, 'PASS' if ok else 'FAIL'))
        if not ok:
            failures.append('%s: rc %d (want %d)\n%s' % (label, rc, want_rc, text))

    run('the bodies in the handler file paste back byte for byte (5 labels)', 0,
        'IDENTICAL to the base, byte for byte, with 5/5 bodies pasted back at their 5 labels in 2 sites '
        '(the base had 0 of the sites moved')
    run('against a base whose bodies stood beside their sites: passes', 0,
        'with 5/5 bodies pasted back at their 5 labels in 2 sites (the base had 2 of the sites moved: '
        '5 bodies pasted back there)', old_text=SELF_BESIDE)
    run('against a base whose handlers are in the handler file: passes', 0,
        '(the base had 2 of the sites moved: 5 bodies pasted back there)', old_text=SELF_SITES,
        old_handlers=SELF_HANDLERS)
    run('a body that changed on the way into the handler file fails', 1, 'DIFFERS',
        old_text=SELF_BESIDE.replace('ctx.rank = 2;', 'ctx.rank = 4;'))
    run('a handler block left beside the sites fails', 1, 'a handler block is still appended to the sites\' file',
        sites=SELF_BESIDE)
    run('bodies beside their sites, no handler file, pass', 0, 'IDENTICAL', sites=SELF_BESIDE, handlers=None)
    run('no handler file and no handler block fails', 1, 'no handler file Handlers.cpp, and the tail marker found 0',
        handlers=None)
    run('a changed body line fails', 1, 'DIFFERS', swap=('if (i == 1)', 'if (i == 2)'))
    run('a fallthrough with code before its second label cannot be moved: fails', 1, 'DIFFERS',
        old_text=SELF_OLD.replace('        case 1:                                 // One\n',
                                  '        case 1:                                 // One\n            Log(1);\n'))
    run('two rows swapped fail', 1, 'registered in two runs of rows',
        swap=('        { 2, &One },\n        { 3, &Three },', '        { 3, &Three },\n        { 2, &One },'))
    run('a row pointing at the wrong body fails', 1, 'DIFFERS', swap=('{ 2, &One }', '{ 2, &Three }'))
    run('a lost row fails', 1, 'table rows [1, 3]', swap=('        { 2, &One },\n', ''))
    run('a bare live-out in a body fails', 1, 'live-out "target" used without the context',
        swap=('ctx.target->Cast', 'target->Cast'))
    run('a name in scope but not in the context fails', 1,
        '"apply", a name in scope at the site and not in its context', swap=('ctx.rank = 2;', 'ctx.rank = apply;'))
    run('a bare member of the site\'s class in a body fails', 1, '"GetCaster", a member of Thing, used bare',
        swap=('ctx.target->Cast(ctx.aura);', 'ctx.target->Cast(GetCaster());'))
    run('a bare data member (m_modifier.) in a body fails', 1, '"m_modifier", a member of Thing, used bare',
        swap=('ctx.rank = 2;', 'ctx.rank = m_modifier.m_amount;'))
    run('a qualified member bare fails, a local named its qualifier passes', 1,
        'body line 2: "m_link", a member of Thing, used bare',
        swap=('    ctx.target->Cast(ctx.aura);',
              '    Proto* proto = ctx.target->GetProto();\n    proto->Cast(m_link);'))
    run('an implicit GetId() the move missed fails', 1, '"GetId", a member of Thing, used bare',
        swap=('Log(ctx.aura->GetId());', 'Log(GetId());'))
    run('a member reached through the context passes the member check', 1, 'DIFFERS',
        swap=('ctx.target->Cast(ctx.aura);', 'ctx.target->Cast(ctx.aura->GetCaster());'))
    run('a Continue inside a loop fails', 1, 'a Continue inside a loop or switch',
        swap=('if (i == 1)\n            break;',
              'if (i == 1)\n            return SpellHandlerOutcome<void>::Continue();'))
    run('a lost dispatch fails', 1, 'the dispatch found 0 times',
        swap=('    if (Dispatch<SelfSite>(handlerContext).IsReturn())\n', ''))
    run('Return taken for Continue fails', 1, 'DIFFERS',
        swap=('return SpellHandlerOutcome<void>::Continue();', 'return SpellHandlerOutcome<void>::Return();'))
    run('a one-line case\'s body changed fails', 1,
        'line 27: base \'        case 7: rank = 1; break;    // Rank 1\'', swap=('ctx.rank = 1;', 'ctx.rank = 3;'))
    run('a lost default registration fails', 1, 'the default of RankSite registered 0 times',
        swap=('    registry.RegisterDefault<RankSite>(&RankDefault);\n', ''))
    run('a default pasted from the wrong body fails', 1, 'the default Rank8 is also a labelled row',
        swap=('RegisterDefault<RankSite>(&RankDefault)', 'RegisterDefault<RankSite>(&Rank8)'))
    run('a default registered where the switch had none fails', 1,
        'a default registered for a site whose switch had none',
        swap=('    registry.RegisterDefault<RankSite>',
              '    registry.RegisterDefault<SelfSite>(&Three);\n    registry.RegisterDefault<RankSite>'))
    for label, planted in [('spaced brackets', '    registry.RegisterDefault< SelfSite >(&Three);'),
                           ('a type alias',
                            '    typedef SelfSite Alias;\n    registry.RegisterDefault<Alias>(&Three);'),
                           ('another registry', '    other.RegisterDefault<SelfSite>(&Three);')]:
        run('a RegisterDefault through %s fails' % label, 1,
            'a RegisterDefault that is not a site\'s `registry.RegisterDefault<TRAITS>(&F);` line',
            swap=('    registry.RegisterDefault<RankSite>', planted + '\n    registry.RegisterDefault<RankSite>'))
    run('a RegisterDefault in a comment is not a registration', 0, 'IDENTICAL',
        swap=('    registry.RegisterDefault<RankSite>',
              '    // registry.RegisterDefault<SelfSite>(&Three);\n    registry.RegisterDefault<RankSite>'))
    run('a labelled row registered outside the tables fails', 1,
        'a Register outside RegisterRows, where every labelled row is registered from its table',
        swap=('    registry.RegisterDefault<RankSite>',
              '    registry.Register<SelfSite>(4, &Three);\n    registry.RegisterDefault<RankSite>'))
    run('a Register when the rows function is renamed fails', 1, 'a Register outside RegisterRows',
        swap=('static uint32 RegisterRows(', 'static uint32 RegisterTableRows('))
    no_traits = dict(SELF_SPEC, sites=[dict(SELF_SPEC['sites'][0]), SELF_SPEC['sites'][1]])
    del no_traits['sites'][0]['traits']
    run('a site that names no traits fails (its default guard could not run)', 1,
        'fixture: no traits named, so a default registered for it could not be seen', spec=no_traits)
    typo = dict(SELF_SPEC, sites=[dict(SELF_SPEC['sites'][0], traits='SelfSit'), SELF_SPEC['sites'][1]])
    run('a site whose traits its dispatch does not name fails', 1,
        'fixture: its traits SelfSit do not appear as Dispatch<SelfSit> in its dispatch lines', spec=typo)
    run('a handler defined but not registered fails', 1, '6 handlers defined, 5 registered and pasted back',
        swap=('void Register(Registry& registry)',
              'static SpellHandlerOutcome<void> Orphan(RankContext& ctx)\n{\n'
              '    return SpellHandlerOutcome<void>::Continue();\n}\n\nvoid Register(Registry& registry)'))
    run('against a base that had the first site moved: passes', 0,
        'with 5/5 bodies pasted back at their 5 labels in 2 sites (the base had 1 of the sites moved: '
        '2 bodies pasted back there)', old_text=SELF_MID)
    run('the base\'s own moved body is proven again: a change there fails', 1, 'DIFFERS',
        old_text=SELF_MID.replace('ctx.target->Cast(ctx.aura);', 'ctx.target->Cast(NULL);'))
    part = dict(old_text=SELF_OLD_PART, spec=SELF_SPEC_PART, sites=SELF_SITES_PART, handlers=SELF_HANDLERS_PART)

    def part_site(**changes):
        return dict(SELF_SPEC_PART, sites=[dict(SELF_SPEC_PART['sites'][0], **changes)])

    run('a partly moved switch pastes back byte for byte (4 labels, 2 runs)', 0,
        'IDENTICAL to the base, byte for byte, with 3/3 bodies pasted back at their 4 labels in 1 sites', **part)
    run('against a base that is the partly moved file itself: passes', 0,
        '(the base had 1 of the sites moved: 3 bodies pasted back there)',
        **dict(part, old_text=SELF_SITES_PART, old_handlers=SELF_HANDLERS_PART))
    wrong_order = {i: SELF_SPEC_PART['sites'][0]['labels'][i] for i in (2, 1, 3, 4, 5, 6)}
    run('a spec whose original order is wrong pastes a run before the wrong label: fails', 1, 'DIFFERS',
        **dict(part, spec=part_site(labels=wrong_order)))
    run('a line between the dispatch and the switch that still stands fails', 1,
        'the dispatch is not directly followed by the switch that still stands',
        swap=('        return;\n    }\n    switch', '        return;\n    }\n    Log();\n    switch'), **part)
    run('a label the switch never had standing in it fails', 1,
        'case 7 stands in the switch but is not one of its labels',
        swap=('        case 3:                                 // Three\n', '        case 7:\n'), **part)
    run('a standing label whose line changed fails', 1, 'case 3 in the switch is not its label line',
        swap=('// Three', '// Three!'), **part)
    run('a label standing twice in the switch fails', 1, 'case 1 stands twice in the switch',
        swap=('        case 3:                                 // Three\n',
              '        case 1:                                 // One\n'
              '        case 3:                                 // Three\n'), **part)
    run('a moved label still standing in the switch fails', 1, '[2] both registered and still standing in the switch',
        swap=('        case 3:                                 // Three\n',
              '        case 2:                                 // Two\n        {\n            target->Drop(2);\n'
              '            return;\n        }\n        case 3:                                 // Three\n'), **part)
    run('rows registered out of the switch\'s original order fail', 1,
        'the rows [4, 5, 6, 2] are not in the switch\'s original order',
        swap=('        { 2, &Two },\n        { 4, &Four },\n        { 5, &Four },\n        { 6, &Six },',
              '        { 4, &Four },\n        { 5, &Four },\n        { 6, &Six },\n        { 2, &Two },'), **part)
    run('a default at a partly moved site fails (it stays in the switch that still stands)', 1,
        'a partly moved switch keeps its default: in the switch that still stands',
        **dict(part, spec=part_site(default='        default:')))

    def versions(base, tree):
        """The run() arguments of a base and a tree, each (sites' file, handler file)."""
        return dict(old_text=base[0], old_handlers=base[1], sites=tree[0], handlers=tree[1], spec=GEN_SPEC)

    none, six = gen_version([]), gen_version([4, 5, 6, 13, 14, 15])
    twelve = gen_version([1, 2, 4, 5, 6, 9, 10, 13, 14, 15, 17, 18])
    run('base 0 moved, tree 6 moved in two runs: passes', 0,
        'with 6/6 bodies pasted back at their 6 labels in 1 sites (the base had 0 of the sites moved',
        **versions(none, six))
    run('base 6 moved, tree 12 moved (a second move at the site): passes', 0,
        'with 11/11 bodies pasted back at their 12 labels in 1 sites (the base had 1 of the sites moved: '
        '6 bodies pasted back there)', **versions(six, twelve))
    run('base 0 moved, tree 12 moved in five runs, the last at the close: passes', 0,
        'with 11/11 bodies pasted back at their 12 labels in 1 sites (the base had 0', **versions(none, twelve))
    run('the base\'s anchors 7 and 16 moved in the tree (re-anchored at 8 and 17): passes', 0,
        'with 8/8 bodies pasted back at their 8 labels in 1 sites (the base had 1 of the sites moved: 6 bodies',
        **versions(six, gen_version([4, 5, 6, 7, 13, 14, 15, 16])))
    run('the tree moved a label its base\'s table lists twice: fails', 1,
        'the table registers a label twice: [4, 5, 5, 6, 13, 14, 15]',
        **versions(gen_version([4, 5, 5, 6, 13, 14, 15]), twelve))
    both = gen_version([3, 4, 5, 6, 13, 14, 15], standing=[i for i in GEN_ORDER if i not in (4, 5, 6, 13, 14, 15)])
    run('a label both registered and standing, in base and tree alike: fails', 1,
        '[3] both registered and still standing in the switch', **versions(both, both))
    neither = gen_version([4, 5, 6, 13, 14, 15], standing=[i for i in GEN_ORDER if i not in (3, 4, 5, 6, 13, 14, 15)])
    run('a label neither registered nor standing, in base and tree alike: fails', 1,
        '[3] neither registered nor standing in the switch', **versions(neither, neither))
    run('a run registered out of the original order fails', 1,
        'the rows [13, 14, 15, 4, 5, 6] are not in the switch\'s original order',
        **versions(none, gen_version([13, 14, 15, 4, 5, 6])))
    swapped = [16 if i == 7 else 7 if i == 16 else i for i in GEN_ORDER]
    run('labels standing out of the original order in the switch fail', 1,
        'the labels standing in the switch, [1, 2, 3, 16, 8, 9, 10, 11, 12, 7, 17, 18], are not in the spec\'s order',
        **versions(none, gen_version([4, 5, 6, 13, 14, 15], switch_order=swapped)))

    def misordered(a, b):
        """GEN_SPEC with labels a and b swapped in its original order."""
        labels = GEN_SPEC['sites'][0]['labels']
        order = [b if i == a else a if i == b else i for i in GEN_ORDER]
        return dict(GEN_SPEC, sites=[dict(GEN_SPEC['sites'][0], labels={i: labels[i] for i in order})])

    eight = gen_version([4, 5, 6, 7, 13, 14, 15, 16])
    run('a spec order wrong between two labels both versions hold fails against the parent', 1,
        'the labels standing in the switch, [1, 2, 3, 8, 9', **dict(versions(six, eight), spec=misordered(1, 2)))
    run('a spec order wrong around a label the tree moved fails against the parent it stood in', 1,
        'the labels standing in the switch, [1, 2, 3, 6, 7, 8',
        **dict(versions(gen_version([4, 5]), gen_version([4, 5, 6])), spec=misordered(6, 7)))
    run('a spec order wrong around a label both versions moved fails against a base before the move', 1,
        'DIFFERS', **dict(versions(none, twelve), spec=misordered(6, 7)))

    def site_named(table):
        """GEN_SPEC with its site naming `table`."""
        return dict(GEN_SPEC, sites=[dict(GEN_SPEC['sites'][0], table=table)])

    def raises(label, needle, call):
        try:
            call()
            text = 'no failure'
        except Failure as e:
            text = str(e)
        ok = needle in text
        print('self-test: %-66s %s' % (label, 'PASS' if ok else 'FAIL'))
        if not ok:
            failures.append('%s: %s' % (label, text))

    renamed = gen_version([1, 2, 4, 5, 6, 9, 10, 13, 14, 15, 17, 18], table='removal')
    run('a table renamed between base and tree is found by its row type: passes', 0,
        'with 11/11 bodies pasted back at their 12 labels in 1 sites (the base had 1 of the sites moved: '
        '6 bodies pasted back there)', **dict(versions(six, renamed), spec=site_named('removed')))
    raises('the lookup by the name the base has fails on the renamed table', 'registration table removed found 0 times',
           lambda: named_table(renamed[1].split('\n'), 'removed'))
    two = (twelve[0], twelve[1].replace('    };\n}', '    };\n    static Row<RemoveSite> const more[] =\n    {\n'
                                                       '        { 3, &Drop3 },\n    };\n}'))
    run('two tables typed by one site\'s traits fail', 1,
        'generated part: 2 registration tables typed by RemoveSite: [\'removed\', \'more\']', **versions(six, two))
    other = (twelve[0], twelve[1].replace('Row<RemoveSite> const removed', 'Row<OtherSite> const removed'))
    run('no table typed by the site\'s traits fails, though one has its name', 1,
        'generated part: no registration table typed by RemoveSite',
        **dict(versions(six, other), spec=site_named('removed')))
    untyped = (twelve[0], twelve[1].replace('Row<RemoveSite> const removed', 'Row const removed'))
    run('an untyped table where the site names none fails', 1,
        'generated part: no registration table is typed, and the site names no table to read',
        **versions(six, untyped))

    mixed = (6, 14)
    moved = [4, 5, 6, 9, 10, 13, 14, 15]
    run('a braced and an unbraced run moved together: passes', 0,
        'with 7/7 bodies pasted back at their 8 labels in 1 sites (the base had 0',
        **versions(gen_version([], unbraced=mixed), gen_version(moved, unbraced=mixed)))
    run('against a base holding the unbraced runs, the braced ones moved: passes', 0,
        'with 7/7 bodies pasted back at their 8 labels in 1 sites (the base had 1 of the sites moved: 2 bodies',
        **versions(gen_version([4, 5], unbraced=mixed), gen_version(moved, unbraced=mixed)))
    run('braced runs the base moved and the tree holds take their shape from the tree: passes', 0,
        'with 6/6 bodies pasted back at their 6 labels in 1 sites (the base had 1 of the sites moved: 11 bodies',
        **versions(twelve, six))
    origin = gen_version([], unbraced=mixed)[0].split('\n')
    site = GEN_SPEC['sites'][0]
    for label, ids, n in [('an unbraced body pasted back with braces', [6], 2),
                          ('a braced body pasted back without braces', [9, 10], 4)]:
        lines = [site['labels'][i] for i in ids]
        body = ['    ctx.target->Drop(%d);' % ids[0], '    return SpellHandlerOutcome<void>::Return();']
        at = origin.index(lines[0])
        stood = origin[at:at + len(lines) + n]
        shape = braced([origin], site, lines)
        ok = paste(site, 'Drop', body, lines, shape) == stood and paste(site, 'Drop', body, lines, not shape) != stood
        print('self-test: %-66s %s' % (label + ' differs from its origin', 'PASS' if ok else 'FAIL'))
        if not ok:
            failures.append('%s: the shape read, %s, does not reproduce the origin alone' % (label, shape))
    tail = gen_version([], unbraced=mixed)[0].replace(
        '        {\n            target->Drop(5);\n            return;\n        }',
        '        {\n            target->Drop(5);\n        }\n        return;')
    run('a body that is a block followed by more lines fails', 1,
        'generated part: where it stands, the body under [\'case 5:\'] is a block followed by more lines',
        **versions((tail, None), gen_version(moved, unbraced=mixed)))
    nested = gen_version([], unbraced=mixed)[0].replace(
        '        {\n            target->Drop(5);\n            return;\n        }',
        '        {\n            target->Drop(5);\n        }\n            case 50:\n            return;')
    run('a block followed by a deeper case (a nested switch\'s) fails', 1,
        'generated part: where it stands, the body under [\'case 5:\'] is a block followed by more lines',
        **versions((nested, None), gen_version(moved, unbraced=mixed)))
    beside = (twelve[0], twelve[1].replace('    };\n}', '    };\n    static Row<RemoveSiteX> const other[] =\n    {\n'
                                                          '        { 3, &Drop3 },\n    };\n}'))
    run('a table typed by another traits sharing the name\'s start is not the site\'s: passes', 0,
        'with 11/11 bodies pasted back at their 12 labels in 1 sites (the base had 1', **versions(six, beside))
    elsewhere = ['void Thing::Other()', '{', '    switch (GetId())', '    {',
                 GEN_SPEC['sites'][0]['labels'][6], '        {', '            target->Drop(6);', '            return;',
                 '        }', '    }', '}', '']
    twice = gen_version([], unbraced=mixed)[0] + '\n'.join(elsewhere)
    tree = gen_version(moved, unbraced=mixed)
    run('a run whose label lines stand twice in one version fails', 1, 'stand 2 times in one version',
        **versions((twice, None), (tree[0] + '\n'.join(elsewhere), tree[1])))
    deep = gen_version([], unbraced=mixed)[0].replace(
        GEN_SPEC['sites'][0]['labels'][6] + '\n            target->Drop(6);\n',
        GEN_SPEC['sites'][0]['labels'][6] + '\n            {\n                target->Drop(6);\n            }\n')
    deep_tree = (tree[0], tree[1].replace('    ctx.target->Drop(6);\n',
                                          '    {\n        ctx.target->Drop(6);\n    }\n'))
    run('an unbraced body starting with a deeper { is read unbraced: passes', 0,
        'with 7/7 bodies pasted back at their 8 labels in 1 sites (the base had 0',
        **versions((deep, None), deep_tree))

    whole_site = {k: v for k, v in GEN_SPEC['sites'][0].items() if k != 'residual'}
    whole_site.update(name='generated whole', default='        default:',
                      labels={i: GEN_SPEC['sites'][0]['labels'][i] for i in (1, 2, 3)})
    whole = dict(GEN_SPEC, sites=[whole_site])
    body = {i: ['            target->Drop(%d);' % i, '            return;'] for i in (0, 1, 2, 3)}
    switch = ([whole_site['labels'][1], '        {'] + body[1] + ['        }']
              + [whole_site['labels'][2]] + body[2]
              + [whole_site['labels'][3], '        {'] + body[3] + ['        }']
              + ['        default:', '        {'] + body[0] + ['        }'])
    head = ['#include "A.h"', '', 'void Thing::Remove(bool apply)', '{', '    Unit* target = GetTarget();']
    whole_origin = '\n'.join(head + ['    switch (GetId())', '    {'] + switch
                             + ['    }', '    target->Tail();', '}', ''])
    whole_tree = '\n'.join(head[:1] + ['#include "Handlers.h"'] + head[1:] + whole_site['dispatch']
                           + ['    target->Tail();', '}', ''])
    whole_handlers = ['#include "Handlers.h"', '']
    for i, name in [(1, 'Drop1'), (2, 'Drop2'), (3, 'Drop3'), (0, 'DropDefault')]:
        whole_handlers += ['static SpellHandlerOutcome<void> %s(RemoveContext& ctx)' % name, '{',
                           '    ctx.target->Drop(%d);' % i, '    return SpellHandlerOutcome<void>::Return();', '}', '']
    whole_handlers += ['void Register(Registry& registry)', '{', '    static Row<RemoveSite> const removed[] =',
                       '    {',
                       '        { 1, &Drop1 },', '        { 2, &Drop2 },', '        { 3, &Drop3 },', '    };',
                       '    registry.RegisterDefault<RemoveSite>(&DropDefault);', '}', '']
    run('a braced default, a block closing before it: pastes back braced, passes', 0,
        'with 4/4 bodies pasted back at their 3 labels in 1 sites (the base had 0',
        old_text=whole_origin, sites=whole_tree, handlers='\n'.join(whole_handlers), spec=whole)

    def deleting(ids, cuts=()):
        """GEN_SPEC with `ids` deleted and the file's `cuts`."""
        return dict(GEN_SPEC, cuts=list(cuts), sites=[dict(GEN_SPEC['sites'][0], deleted=ids)])

    def without(ids, rows=(4, 5, 6, 13, 14, 15)):
        """A version registering `rows` whose switch holds every other label but `ids`."""
        return gen_version(list(rows), standing=[i for i in GEN_ORDER if i not in rows and i not in ids])

    run('a deleted label cut from a base it stands in (braced): passes', 0,
        'with 6/6 bodies pasted back at their 6 labels in 1 sites (the base had 0 of the sites moved: 0 bodies pasted '
        'back there), the base less 1 cuts (5 lines)', **dict(versions(none, without([7])), spec=deleting([7])))
    run('a deleted label cut from a base that registers it (unbraced there): passes', 0,
        '(the base had 1 of the sites moved: 7 bodies pasted back there), the base less 1 cuts (3 lines)',
        **dict(versions(gen_version([4, 5, 6, 7, 13, 14, 15]), without([7])), spec=deleting([7])))
    run('a deleted label falling into the next label\'s body: its line alone is cut, passes', 0,
        'the base less 1 cuts (1 lines)', **dict(versions(none, without([9])), spec=deleting([9])))
    gen_labels = GEN_SPEC['sites'][0]['labels']
    shared = 'has no body of its own and would fall into another'
    run('a deleted label sharing the body of the label above it fails', 1, shared,
        **dict(versions(none, without([10])), spec=deleting([10])))
    for label, between in [('a comment', '        // note\n'), ('a blank line', '\n')]:
        run('a deleted label under a bodyless label and %s fails' % label, 1, shared,
            **dict(versions((none[0].replace(gen_labels[10], between + gen_labels[10]), None), without([10])),
                   spec=deleting([10])))
    run('a deleted label a `default:` falls into fails', 1, '\'default:\' ' + shared,
        **dict(versions((none[0].replace(gen_labels[7], '        default:\n' + gen_labels[7]), None),
                        without([7])), spec=deleting([7])))
    run('a deleted label last before the switch\'s close: passes', 0, 'the base less 1 cuts (5 lines)',
        **dict(versions(none, without([18])), spec=deleting([18])))
    twice7 = ['void Thing::Again()', '{', '    switch (GetId())', '    {', gen_labels[7], '            return;',
              '    }', '}', '']
    run('a deleted label whose line stands twice in the base fails', 1, 'label 7 found 2 times in the base',
        **dict(versions((none[0] + '\n'.join(twice7), None), (without([7])[0] + '\n'.join(twice7), without([7])[1])),
               spec=deleting([7])))
    run('a deleted id that is no label of the site fails', 1, 'deleted [99], not labels of its switch',
        **dict(versions(none, without([7])), spec=deleting([7, 99])))
    run('a deleted label still registered in the tree fails', 1, 'rows [7] are not labels of the switch',
        **dict(versions(none, gen_version([4, 5, 6, 7, 13, 14, 15])), spec=deleting([7])))
    run('a deleted label still standing in the tree fails', 1,
        'case 7 stands in the switch but is not one of its labels',
        **dict(versions(none, six), spec=deleting([7])))
    run('a label gone from the tree but not deleted fails', 1, '[7] neither registered nor standing in the switch',
        **versions(none, without([7])))

    other = ['void Thing::Other()', '{', '    switch (GetId())', '    {',
             '        case 50:                                // Fifty', '            Drop(50);', '            return;',
             '        case 51:                                // Fifty-one', '        {', '            Drop(51);',
             '            return;', '        }',
             '        case 52:                                // Fifty-two', '            Drop(52);',
             '            return;', '    }', '}', '']

    def plus_other(version, drop=()):
        # `version` with the function Other appended, less the lines `drop` names (their indexes in it)
        return (version[0] + '\n'.join(l for k, l in enumerate(other) if k not in drop), version[1])

    base_other, tree_other = plus_other(none), plus_other(without([7]), range(7, 12))
    fifty_one = [other[7]]

    def file_cut(base, tree, cuts):
        return dict(versions(base, tree), spec=deleting([7], cuts))

    run('a file cut, a whole label run outside the sites: passes', 0, 'the base less 2 cuts (10 lines)',
        **file_cut(base_other, tree_other, [(fifty_one, 5)]))
    run('a file cut, the last run before the switch\'s close: passes', 0, 'the base less 2 cuts (8 lines)',
        **file_cut(base_other, plus_other(without([7]), range(12, 15)), [([other[12]], 3)]))
    run('a file cut a line long fails', 1, '6 lines, and its label run is 5',
        **file_cut(base_other, tree_other, [(fifty_one, 6)]))
    run('a file cut a line short fails', 1, '4 lines, and its label run is 5',
        **file_cut(base_other, tree_other, [(fifty_one, 4)]))
    run('a file cut of a body line of a standing label fails', 1, '\'            Drop(50);\' is not a `case`',
        **file_cut(base_other, plus_other(without([7]), [5]), [([other[5]], 1)]))
    run('a file cut of a return, a label falling into the next body, fails', 1,
        '\'            return;\' is not a `case`',
        **file_cut(base_other, plus_other(without([7]), [6]), [([other[6], other[7]], 1)]))
    run('a file cut at a site\'s label fails (its DELETED cuts it)', 1, 'a label of a site',
        **file_cut(none, without([7]), [([gen_labels[7]], 5)]))
    bodyless = plus_other(none, [5, 6])
    for label, between in [('', []), (' and a comment', ['        // note']), (' and a blank line', [''])]:
        lines = bodyless[0].split('\n')
        at = lines.index(other[7])
        lines[at:at] = between
        run('a file cut under a bodyless label%s fails' % label, 1, '%r %s' % (other[4].strip(), shared),
            **file_cut(('\n'.join(lines), None), tree_other, [(fifty_one, 5)]))
    run('a file cut whose first line the base does not hold cuts nothing there: fails', 1, 'DIFFERS',
        **file_cut(base_other, tree_other, [(['        case 53:'], 3)]))
    run('a file cut whose first lines stand twice in the base fails', 1, 'found 2 times in the base',
        **file_cut(plus_other((base_other[0], None)), tree_other, [(fifty_one, 5)]))
    run('two file cuts of one run fail', 1, 'two cuts overlap',
        **file_cut(base_other, tree_other, [(fifty_one, 5), (fifty_one, 5)]))
    run('a file cut the tree still holds fails', 1, 'DIFFERS',
        **file_cut(base_other, plus_other(without([7])), [(fifty_one, 5)]))
    run('against a base from after the deletions (nothing left to cut): passes', 0,
        '(the base had 1 of the sites moved: 6 bodies pasted back there), the base less 0 cuts (0 lines)',
        **file_cut(tree_other, tree_other, [(fifty_one, 5)]))

    text_other = '\n'.join(other)

    def cut_run(label, want, needle, text, head, count):
        # `text` in place of Other in the base; the tree drops `count` lines from `head`
        lines = text.split('\n')
        at = lines.index(head)
        tree = without([7])
        run(label, want, needle, **file_cut((none[0] + text, None),
                                            (tree[0] + '\n'.join(lines[:at] + lines[at + count:]), tree[1]),
                                            [([head], count)]))

    falls = 'the run above may fall into it'
    fifty = '            Drop(50);\n            return;\n'
    cut_run('a file cut under a run with no terminator fails', 1, falls,
            text_other.replace(fifty, '            Drop(50);\n'), other[7], 5)
    cut_run('a file cut under a run whose last code is followed by a comment fails', 1, falls,
            text_other.replace(fifty, '            Drop(50);\n// note\n'), other[7], 5)
    for last in ['return;', 'break;', 'continue;', 'goto done;', 'return Drop(0);']:
        cut_run('a file cut under a run ending in `%s`: passes' % last, 0, 'the base less 2 cuts (10 lines)',
                text_other.replace(fifty, '            Drop(50);\n            %s\n' % last), other[7], 5)
    cut_run('a file cut under a one-line case ending in break: passes', 0, 'the base less 2 cuts (10 lines)',
            text_other.replace('\n'.join(other[4:7]) + '\n', '        case 50: Drop(50); break;    // Fifty\n'),
            other[7], 5)
    cut_run('a file cut under a one-line case with no terminator fails', 1, falls,
            text_other.replace('\n'.join(other[4:7]) + '\n', '        case 50: Drop(50);    // Fifty\n'), other[7], 5)
    cut_run('a file cut under a braced run ending in return: passes', 0, 'the base less 2 cuts (8 lines)',
            text_other, other[12], 3)
    cut_run('a file cut under a braced run with no terminator fails', 1, falls,
            text_other.replace('            Drop(51);\n            return;\n        }',
                               '            Drop(51);\n        }'),
            other[12], 3)
    cut_run('a file cut of a `default:` run fails', 1, '\'        default:\' is not a `case` line',
            text_other.replace('            return;\n    }\n}', '            return;\n        default:\n'
                               '            return;\n    }\n}'), '        default:', 2)
    column0 = text_other.replace('            Drop(51);\n', '            Drop(51);\n// note\n')
    cut_run('a run holding a comment at column 0 is cut whole: passes', 0, 'the base less 2 cuts (11 lines)',
            column0, other[7], 6)
    cut_run('a cut stopping at a comment at column 0 fails', 1, '5 lines, and its label run is 6',
            column0, other[7], 5)
    nested = text_other.replace('            Drop(51);\n            return;\n        }',
                                '            switch (x)\n            {\n                case 1:\n'
                                '                    break;\n            }\n            return;\n        }')
    cut_run('a run holding a nested switch is cut whole: passes', 0, 'the base less 2 cuts (14 lines)',
            nested, other[7], 9)
    cut_run('a cut stopping at the nested switch\'s close fails', 1, '7 lines, and its label run is 9',
            nested, other[7], 7)
    cut_run('a run with a blank line inside, last before the close: passes', 0, 'the base less 2 cuts (9 lines)',
            text_other.replace('            Drop(52);\n', '            Drop(52);\n\n'), other[12], 4)
    tail = text_other.replace('            return;\n    }\n}', '            return;\n    // tail\n    }\n}')
    cut_run('a comment less indented before the close ends the last run with it: passes', 0,
            'the base less 2 cuts (9 lines)', tail, other[12], 4)
    cut_run('a last run cut short of that comment fails', 1, '3 lines, and its label run is 4', tail, other[12], 3)
    for label, shape in [('if (x)', '            if (x)\n                return;\n'),
                         ('for (;;)', '            for (;;)\n                break;\n')]:
        cut_run('a file cut under a run ending in `%s` and a terminator under it fails' % label, 1, 'ends under',
                text_other.replace(fifty, '            Drop(50);\n' + shape), other[7], 5)
    cut_run('a file cut under a braced run ending in `if (x)` and a return under it fails', 1, 'ends under',
            text_other.replace('            Drop(51);\n            return;\n        }',
                               '            if (x)\n                return;\n        }'), other[12], 3)
    cut_run('a file cut under a run ending in `if (x) return;` on one line fails', 1, falls,
            text_other.replace(fifty, '            Drop(50);\n            if (x) return;\n'), other[7], 5)
    cut_run('a file cut under a return after a closed block: passes', 0, 'the base less 2 cuts (10 lines)',
            text_other.replace(fifty, '            if (x)\n            {\n                Drop(50);\n            }\n'
                                      '            return;\n'), other[7], 5)
    cut_run('a file cut of the first label after the switch\'s `{`: passes', 0, 'the base less 2 cuts (8 lines)',
            text_other, other[4], 3)
    cut_run('a file cut under a run that is its label and a return: passes', 0, 'the base less 2 cuts (10 lines)',
            text_other.replace(fifty, '            return;\n'), other[7], 5)

    gone_spec = dict(whole, sites=[dict(whole_site, gone=(1, 1))])
    gone_tree = '\n'.join(head[:1] + ['#include "Handlers.h"'] + head[1:4] + ['}', ''])
    no_rows = '#include "Handlers.h"\n\nvoid Register(Registry& registry)\n{\n}\n'
    run('a site deleted whole, cut from a base before its move: passes', 0,
        'with 0/0 bodies pasted back at their 0 labels in 0 sites (the base had 0 of the sites moved: 0 bodies pasted '
        'back there), the base less 1 cuts (%d lines)' % (len(switch) + 5),
        old_text=whole_origin, sites=gone_tree, handlers=no_rows, spec=gone_spec)
    run('a site deleted whole, cut from a base that had it moved (unbraced there): passes', 0,
        '(the base had 1 of the sites moved: 4 bodies pasted back there), the base less 1 cuts', old_text=whole_tree,
        old_handlers='\n'.join(whole_handlers), sites=gone_tree, handlers=no_rows, spec=gone_spec)
    run('a site deleted whole, against a base from after its deletion: passes', 0,
        '(the base had 0 of the sites moved: 0 bodies pasted back there), the base less 0 cuts (0 lines)',
        old_text=gone_tree, old_handlers=no_rows, sites=gone_tree, handlers=no_rows, spec=gone_spec)
    run('a site deleted whole, a line below it kept that GONE names: fails', 1, 'DIFFERS', old_text=whole_origin,
        sites=gone_tree.replace('\n}\n', '\n    target->Tail();\n}\n'), handlers=no_rows, spec=gone_spec)
    run('a site deleted whole whose dispatch still stands fails', 1,
        'generated whole: deleted, and its dispatch still stands', old_text=whole_origin, sites=whole_tree,
        handlers=no_rows, spec=gone_spec)
    run('a site deleted whole whose table still stands fails', 1,
        'deleted, and a table typed by RemoveSite still stands',
        old_text=whole_origin, sites=gone_tree, handlers=no_rows.replace(
            '{\n}', '{\n    static Row<RemoveSite> const removed[] =\n    {\n    };\n}'), spec=gone_spec)
    run('a site deleted whole whose default is still registered fails', 1,
        'deleted, and a default is still registered for it', old_text=whole_origin, sites=gone_tree,
        handlers=no_rows.replace('{\n}', '{\n    registry.RegisterDefault<RemoveSite>(&DropDefault);\n}'),
        spec=gone_spec)
    run('a whole switch deleting some of its labels fails', 1, 'deletes labels and keeps its dispatch',
        old_text=whole_origin, sites=whole_tree, handlers='\n'.join(whole_handlers),
        spec=dict(whole, sites=[dict(whole_site, deleted=[1])]))
    first = whole_site['labels'][1]
    led = whole_origin.replace('    {\n' + first, '    {\n        // lead\n' + first)
    run('a site deleted whole whose switch does not open directly above its first label fails', 1,
        'deleted, and its switch does not open above its first label', old_text=led, sites=gone_tree,
        handlers=no_rows, spec=gone_spec)
    run('a site deleted whole whose GONE lines run above the file fails', 1, 'the lines GONE names run past the file',
        old_text=whole_origin, sites=gone_tree, handlers=no_rows,
        spec=dict(whole, sites=[dict(whole_site, gone=(100, 1))]))
    form_site = {k: v for k, v in GEN_SPEC['sites'][0].items() if k not in ('residual', 'labels')}

    def form(name, labels, switch, functions, rows, default=None, before=(), standing=(),
             signature='void Thing::Remove(bool apply)', tail=('    target->Tail();',), **changes):
        """run() arguments of a site moved from `switch` (its lines between the site's open and close)
        into `functions` [(name, value, body)] registered by `rows` [(id, name)]: whole, or partly with
        the switch `standing` (its lines, open and close included) after the dispatch; `before`
        stands above the site in both versions."""
        site = dict(form_site, name=name, labels=labels, **changes)
        if default:
            site['default'] = '        default:'
        head = ['#include "A.h"', '', signature, '{', '    Unit* target = GetTarget();'] + list(before)
        end = list(tail) + ['}', '']
        origin = head + site['open'] + switch + site['close'] + end
        tree = head[:1] + ['#include "Handlers.h"'] + head[1:] + site['dispatch'] + list(standing) + end
        handlers = ['#include "Handlers.h"', '']
        for function, value, body in functions:
            handlers += ['static SpellHandlerOutcome<%s> %s(%s& ctx)' % (value, function, site['context']), '{']
            handlers += body + ['}', '']
        handlers += ['void Register(Registry& registry)', '{', '    static Row<%s> const rows[] =' % site['traits'],
                     '    {'] + ['        { %d, &%s },' % row for row in rows] + ['    };']
        if default:
            handlers.append('    registry.RegisterDefault<%s>(&%s);' % (site['traits'], default))
        handlers += ['}', '']
        return dict(old_text='\n'.join(origin), sites='\n'.join(tree), handlers='\n'.join(handlers),
                    spec=dict(GEN_SPEC, sites=[site]))

    def mutated(args, key, a, b):
        """`args` with a replaced by b in its `key` text; a must stand there."""
        if a not in args[key]:
            failures.append('the mutation %r matches nothing in %s' % (a, key))
        return dict(args, **{key: args[key].replace(a, b)})

    lab = {i: gen_labels[i] for i in (1, 2, 3, 4)}
    ret, cont = '    return SpellHandlerOutcome<void>::Return();', '    return SpellHandlerOutcome<void>::Continue();'

    trailing = form('trailing comments', lab, [
        lab[1], '        {', '            target->Drop(1);', '            break;', '        }',
        '        // case 90:                             // Ninety', '        // break;', '        // case 91:',
        '        // break;',
        lab[2], '        {', '            target->Drop(2);', '            return;', '        }', '        // case 92:',
        '        // break;',
        lab[3], '            target->Drop(3);', '            return;',
        lab[4], '        {', '            target->Drop(4);', '            return;', '        }',
        '        // case 93:'], [
        ('One', 'void', ['    ctx.target->Drop(1);', cont, '// case 90:                             // Ninety',
                         '// break;', '// case 91:', '// break;']),
        ('Two', 'void', ['    ctx.target->Drop(2);', ret, '// case 92:', '// break;']),
        ('Three', 'void', ['    ctx.target->Drop(3);', ret]),
        ('Four', 'void', ['    ctx.target->Drop(4);', ret, '// case 93:'])],
        [(1, 'One'), (2, 'Two'), (3, 'Three'), (4, 'Four')])
    run('T: comment lines after a braced run, the last before the close: pass', 0,
        'with 4/4 bodies pasted back at their 4 labels in 1 sites', **trailing)
    run('T: a code line after a braced run, a comment on it: fails', 1, 'is a block followed by more lines',
        **mutated(trailing, 'old_text', '        // case 91:', '        Log(); // case 91:'))
    run('T: a comment line deeper than the label after a braced run: fails', 1, 'is a block followed by more lines',
        **mutated(trailing, 'old_text', '        // case 91:', '            // case 91:'))
    run('T: a blank line among the comment lines after a block: fails', 1, 'is a block followed by more lines',
        **mutated(trailing, 'old_text', '        // case 91:', '\n        // case 91:'))
    run('T: the comment lines kept at the body\'s indent in the handler: fails', 1, 'DIFFERS',
        **mutated(trailing, 'handlers', '// case 92:\n// break;', '    // case 92:\n    // break;'))

    other_switch = ['    switch (GetId())', '    {', '        case 90:                                // Ninety',
                    '            target->Drop(90);', '            break;', '        default:', '        {',
                    '            target->Log();', '            break;', '        }', '    }']
    scoped = form('scoped default', {1: lab[1]}, [
        lab[1], '            target->Drop(1);', '            return;',
        '        default:', '            target->Drop(0);', '            return;'], [
        ('One', 'void', ['    ctx.target->Drop(1);', ret]), ('Default', 'void', ['    ctx.target->Drop(0);', ret])],
        [(1, 'One')], default='Default', before=other_switch)
    run('S: a default line standing in another switch too: shape read in the site\'s', 0,
        'with 2/2 bodies pasted back at their 1 labels in 1 sites', **scoped)
    run('S: a run\'s label line standing in the other switch too: fails', 1, 'stand 2 times in one version',
        **mutated(mutated(scoped, 'old_text', other_switch[2], lab[1]), 'sites', other_switch[2], lab[1]))
    run('S: a default the site\'s switch lacks, standing in another: fails', 1, 'DIFFERS',
        **mutated(scoped, 'old_text', '        default:\n            target->Drop(0);\n            return;\n', ''))
    run('F: a default ending in a call (no run follows it): fails', 1, 'the default Default ends in a call',
        **mutated(scoped, 'handlers', '    ctx.target->Drop(0);\n' + ret, '    return One(ctx);'))

    valued_site = dict(value='SpellCastResult', traits='CheckSite', context='CheckContext',
                 dispatch=['    CheckContext checkContext(this, target);',
                           '    SpellHandlerOutcome<SpellCastResult> outcome = Dispatch<CheckSite>(checkContext);',
                           '    if (outcome.IsReturn())', '    {', '        return outcome.GetValue();', '    }'],
                 substitutions=[('ctx.target', 'target'), ('ctx.aura', 'this')])
    valued_rows = ([lab[1], '        {', '            if (target->IsDead())', '            {',
                    '                return SPELL_FAILED_TARGETS_DEAD;', '            }', '            break;',
                    '        }',
                    lab[2], '            return SPELL_FAILED_BAD_TARGETS;'],
                   [('One', 'SpellCastResult', [
                       '    if (ctx.target->IsDead())', '    {',
                       '        return SpellHandlerOutcome<SpellCastResult>::Return(SPELL_FAILED_TARGETS_DEAD);',
                       '    }',
                       '    return SpellHandlerOutcome<SpellCastResult>::Continue();']),
                    ('Two', 'SpellCastResult', [
                        '    return SpellHandlerOutcome<SpellCastResult>::Return(SPELL_FAILED_BAD_TARGETS);'])],
                   [(1, 'One'), (2, 'Two')])
    valued = form('valued', {1: lab[1], 2: lab[2]}, *valued_rows, signature='SpellCastResult Thing::Check(bool apply)',
                  tail=('    return SPELL_CAST_OK;',), **valued_site)
    run('V: a valued return and a break in a SpellCastResult site: pass', 0,
        'with 2/2 bodies pasted back at their 2 labels in 1 sites', **valued)
    void_site = dict(valued['spec']['sites'][0], substitutions=valued_site['substitutions'] + VOID_SUBSTITUTIONS)
    del void_site['value']
    run('V: a valued site whose spec names no value (void pairs): fails', 1,
        'handler One answers SpellHandlerOutcome<SpellCastResult>, and its site\'s value is void',
        **dict(valued, spec=dict(GEN_SPEC, sites=[void_site])))
    run('V: a handler answering void at a valued site: fails', 1,
        'handler Two answers SpellHandlerOutcome<void>, and its site\'s value is SpellCastResult',
        **mutated(valued, 'handlers', 'SpellHandlerOutcome<SpellCastResult> Two(', 'SpellHandlerOutcome<void> Two('))
    as_void = dict(valued['spec']['sites'][0], substitutions=valued_site['substitutions'] + [
        (re.compile(r'return SpellHandlerOutcome<SpellCastResult>::Return\((.*)\);'), 'return;')])
    run('V: a valued return pasted back as `return;`: fails', 1, 'DIFFERS',
        **dict(valued, spec=dict(GEN_SPEC, sites=[as_void])))
    run('V: a changed return value: fails', 1, 'DIFFERS',
        **mutated(valued, 'handlers', 'Return(SPELL_FAILED_BAD_TARGETS)', 'Return(SPELL_FAILED_BAD_IMPLICIT_TARGETS)'))
    run('V: a break taken as a return of the value after the switch: fails', 1, 'DIFFERS',
        **mutated(valued, 'handlers', 'SpellCastResult>::Continue()', 'SpellCastResult>::Return(SPELL_CAST_OK)'))
    for what, value in (('a semicolon', 'SpellCastResult;'), ('a newline', 'Spell\nCastResult'),
                        ('a brace', 'SpellCastResult{'), ('nothing', '')):
        run('V: a value holding %s: fails by name' % what, 1, 'its value %r is not a plain type spelling' % value,
            **dict(valued, spec=dict(GEN_SPEC, sites=[dict(valued['spec']['sites'][0], value=value)])))
    nothing_returned = mutated(mutated(valued, 'handlers', 'Return(SPELL_FAILED_TARGETS_DEAD)', 'Continue()'),
                               'handlers', 'Return(SPELL_FAILED_BAD_TARGETS)', 'Continue()')
    nothing_returned = mutated(mutated(nothing_returned, 'old_text', 'return SPELL_FAILED_TARGETS_DEAD;', 'break;'),
                               'old_text', 'return SPELL_FAILED_BAD_TARGETS;', 'break;')
    run('V: a valued site none of whose handlers returns a value: fails', 1,
        'valued: its value is SpellCastResult, and no handler it registers returns one', **nothing_returned)
    void_returning = mutated(mutated(valued, 'handlers', 'SpellHandlerOutcome<SpellCastResult> One(',
                                     'SpellHandlerOutcome<void> One('),
                             'handlers', 'SpellHandlerOutcome<SpellCastResult> Two(', 'SpellHandlerOutcome<void> Two(')
    run('V: a void handler returning a value at a site with no value: fails', 1,
        'valued: handler One returns a value, and the site names no value',
        **dict(void_returning, spec=dict(GEN_SPEC, sites=[void_site])))
    run('V: a valued Continue inside a loop: fails', 1, 'handler One, body line 4: a Continue inside a loop',
        **mutated(valued, 'handlers',
                  '        return SpellHandlerOutcome<SpellCastResult>::Return(SPELL_FAILED_TARGETS_DEAD);',
                  '        while (ctx.target->IsDead())\n'
                  '            return SpellHandlerOutcome<SpellCastResult>::Continue();\n'
                  '        return SpellHandlerOutcome<SpellCastResult>::Return(SPELL_FAILED_TARGETS_DEAD);'))
    commented = mutated(mutated(trailing, 'old_text', '        // break;',
                                '        // return SpellHandlerOutcome<int>::Return(1);'),
                        'handlers', '// break;', '// return SpellHandlerOutcome<int>::Return(1);')
    run('V: a valued Return in a comment at a site with no value: pass', 0,
        'with 4/4 bodies pasted back at their 4 labels in 1 sites', **commented)

    def respec(args, **changes):
        """`args` (form()'s) with its one site changed by `changes`."""
        return dict(args, spec=dict(args['spec'], sites=[dict(args['spec']['sites'][0], **changes)]))

    for what, value in (('holding a space', 'unsigned int'), ('holding a template argument', 'std::pair<int, int>'),
                        ('holding a trailing space', 'SpellCastResult '), ('that is not text', 7)):
        run('V: a value %s: fails by name' % what, 1, 'its value %r is not a plain type spelling' % (value,),
            **respec(valued, value=value))
    for what, pairs in (('the void pairs', VOID_SUBSTITUTIONS),
                        ('its own Continue pair', [('return SpellHandlerOutcome<SpellCastResult>::Continue();',
                                                    'break;')])):
        run('V: a valued site listing %s beside its value: fails by name' % what, 1,
            'valued: its value is SpellCastResult, and its substitutions list the outcome pair %r, which the tool '
            'builds from the value' % pairs[0][0], **respec(valued, substitutions=valued_site['substitutions'] + pairs))
    breaks = [lab[1], '            target->Drop(1);', '            break;']
    bad = '            return SPELL_FAILED_BAD_TARGETS;'
    breaking = ('One', 'SpellCastResult',
                ['    ctx.target->Drop(1);', '    return SpellHandlerOutcome<SpellCastResult>::Continue();'])
    checking = dict(valued_site, signature='SpellCastResult Thing::Check(bool apply)',
                    tail=('    return SPELL_CAST_OK;',))
    no_value = ['            target->Drop(2);', '            // return SPELL_FAILED_BAD_TARGETS;', '            break;']
    run('V: a partly moved valued site whose moved runs only break: fails by name', 1,
        'still: its value is SpellCastResult, and no handler it registers returns one (no `Return(x)` to paste back '
        'as `return x;`)',
        **form('still', {1: lab[1], 2: lab[2]}, breaks + [lab[2]] + no_value, [breaking], [(1, 'One')],
               standing=form_site['open'] + [lab[2]] + no_value + form_site['close'], residual=True, **checking))
    run('V: a valued site whose only returned value is its default\'s: passes', 0,
        'with 2/2 bodies pasted back at their 1 labels in 1 sites',
        **form('default only', {1: lab[1]}, breaks + ['        default:', bad],
               [breaking, ('Default', 'SpellCastResult',
                           ['    return SpellHandlerOutcome<SpellCastResult>::Return(SPELL_FAILED_BAD_TARGETS);'])],
               [(1, 'One')], default='Default', **checking))
    run('V: a base returning no value at a valued site is read, not refused: DIFFERS', 1,
        'DIFFERS from the base after pasting back 2 bodies at 1 sites',
        **dict(valued, old_text=nothing_returned['sites'], old_handlers=nothing_returned['handlers']))

    no_continue = 'trailing comments: its substitutions hold no Continue pair (none pastes back as `break;`)'
    raises('a site whose substitutions hold no Continue pair: its pairs refused by name', no_continue,
           lambda: outcome_pairs(dict(trailing['spec']['sites'][0], substitutions=[('ctx.target', 'target')])))
    run('a site whose substitutions hold no Continue pair: fails by name', 1, no_continue, bound=60,
        **respec(trailing, substitutions=[('ctx.target', 'target'), ('ctx.aura', 'this')]))
    literal = '\n            target->Drop(1);\n            break;\n'
    for what, base_line, handler_line in (
            ('a string literal', '            Log("target here");', '    Log("ctx.target here");'),
            ('a raw string literal', '            Log(R"(the target)");', '    Log(R"(the ctx.target)");')):
        run('a substitution\'s left side inside %s: fails by name' % what, 1,
            "handler One, body line 1: 'ctx.target', the left side of a substitution, stands inside a string literal, "
            "which the paste-back would rewrite: %r" % handler_line,
            **mutated(mutated(trailing, 'old_text', lab[1] + '\n        {' + literal,
                              lab[1] + '\n        {\n' + base_line + literal),
                      'handlers', '    ctx.target->Drop(1);\n', handler_line + '\n    ctx.target->Drop(1);\n'))
    run('a substitution\'s left side inside a string literal after a use in code: fails by name', 1,
        "handler One, body line 2: 'ctx.target', the left side of a substitution, stands inside a string literal",
        **mutated(mutated(trailing, 'old_text', lab[1] + '\n        {\n            target->Drop(1);\n',
                          lab[1] + '\n        {\n            target->Drop(1);\n            Log("target here");\n'),
                  'handlers', '    ctx.target->Drop(1);\n', '    ctx.target->Drop(1);\n    Log("ctx.target here");\n'))
    noted = 'const char* note = "%s";'
    run('a valued pair\'s left side inside a string literal: fails by name', 1,
        "handler Two, body line 1: %r, the left side of a substitution, stands inside a string literal"
        % valued_substitutions('SpellCastResult')[0][0].pattern,
        **mutated(mutated(valued, 'old_text', '            return SPELL_FAILED_BAD_TARGETS;',
                          '            %s\n            return SPELL_FAILED_BAD_TARGETS;' % (noted % 'return 1;')),
                  'handlers', '    return SpellHandlerOutcome<SpellCastResult>::Return(SPELL_FAILED_BAD_TARGETS);',
                  '    %s\n    return SpellHandlerOutcome<SpellCastResult>::Return(SPELL_FAILED_BAD_TARGETS);'
                  % (noted % 'return SpellHandlerOutcome<SpellCastResult>::Return(1);')))
    for what, base_line, handler_line in (
            ('a line comment', '            // the target here', '    // the ctx.target here'),
            ('a block comment', '            /* the target */ Log();', '    /* the ctx.target */ Log();')):
        run('a substitution\'s left side inside %s is rewritten: passes' % what, 0,
            'with 4/4 bodies pasted back at their 4 labels in 1 sites',
            **mutated(mutated(trailing, 'old_text', lab[1] + '\n        {' + literal,
                              lab[1] + '\n        {\n' + base_line + literal),
                      'handlers', '    ctx.target->Drop(1);\n', handler_line + '\n    ctx.target->Drop(1);\n'))

    trio = '        case 11: case 12: case 13:              // Trio'
    trio2 = '        case 21: case 22: case 23:              // Trio, unbraced'
    multi_labels = {11: trio, 12: trio, 13: trio, 21: trio2, 22: trio2, 23: trio2, 3: lab[3]}
    trio_block = [trio, '        {', '            target->Drop(11);', '            return;', '        }']
    multi_switch = trio_block + [trio2, '            target->Drop(21);', '            return;',
                                 lab[3], '            target->Drop(3);', '            return;']
    multi_functions = [('Trio', 'void', ['    ctx.target->Drop(11);', ret]),
                       ('TrioUnbraced', 'void', ['    ctx.target->Drop(21);', ret]),
                       ('Three', 'void', ['    ctx.target->Drop(3);', ret])]
    multi_rows = [(11, 'Trio'), (12, 'Trio'), (13, 'Trio'), (21, 'TrioUnbraced'), (22, 'TrioUnbraced'),
                  (23, 'TrioUnbraced'), (3, 'Three')]
    run('M: three-label lines over a braced and an unbraced body: pass', 0,
        'with 3/3 bodies pasted back at their 7 labels in 1 sites',
        **form('multi', multi_labels, multi_switch, multi_functions, multi_rows))
    run('M: an id of a multi-label line missing from LABELS and the table: fails', 1,
        'holds case [11, 12, 13], and the site maps [11, 13] to it',
        **form('multi', {i: l for i, l in multi_labels.items() if i != 12}, multi_switch, multi_functions,
               [r for r in multi_rows if r[0] != 12]))
    run('M: an id of a multi-label line registered to another function: fails', 1,
        'holds case [12, 13], which Trio does not register',
        **form('multi', multi_labels, multi_switch, multi_functions,
               [(11, 'Trio'), (12, 'Three'), (13, 'Trio')] + multi_rows[3:]))
    run('M: an id of a multi-label line missing from the table: fails', 1, 'table rows',
        **form('multi', multi_labels, multi_switch, multi_functions, [r for r in multi_rows if r[0] != 12]))
    open_close = form_site['open'] + trio_block + form_site['close']
    run('M: a multi-label line still standing in a partly moved switch: passes', 0,
        'with 2/2 bodies pasted back at their 4 labels in 1 sites',
        **form('multi part', multi_labels, multi_switch, multi_functions[1:], multi_rows[3:], standing=open_close,
               residual=True))
    for label, line in [('a `default:`', '        case 11: default:                      // Trio'),
                        ('a named case', '        case 11: case SPELL_X:                 // Trio')]:
        run('M: a label line holding %s beside its case: fails' % label, 1, 'holds more than `case N:` labels',
            **form('multi', {11: line}, [line, '            target->Drop(11);', '            return;'],
                   multi_functions[:1], [(11, 'Trio')]))
    rest_close = form_site['open'] + multi_switch[5:] + form_site['close']
    run('M: a deleted id sharing its line with others: fails', 1, 'deleted label 12 shares its line with other labels',
        **form('multi part', multi_labels, multi_switch, multi_functions[:1], [(11, 'Trio'), (13, 'Trio')],
               standing=rest_close, residual=True, deleted=[12]))

    fall_switch = [lab[1], '            target->Drop(1);', '            // no break here', '',
                   lab[2], '        {', '            target->Drop(2);', '            return;', '        }',
                   lab[3], '        {', '            target->Drop(3);', '            // no break here', '        }',
                   lab[4], '            target->Drop(4);', '            break;']
    fall_functions = [('One', 'void', ['    ctx.target->Drop(1);', '    // no break here', '', '    return Two(ctx);']),
                      ('Two', 'void', ['    ctx.target->Drop(2);', ret]),
                      ('Three', 'void', ['    ctx.target->Drop(3);', '    // no break here', '    return Four(ctx);']),
                      ('Four', 'void', ['    ctx.target->Drop(4);', cont])]
    fall_rows = [(1, 'One'), (2, 'Two'), (3, 'Three'), (4, 'Four')]
    falls = form('falls', lab, fall_switch, fall_functions, fall_rows)
    run('a sites\' file naming no tail marker, its handlers in the handler file: passes', 0,
        'with 4/4 bodies pasted back at their 4 labels in 1 sites',
        **dict(trailing, spec={k: v for k, v in trailing['spec'].items() if k != 'tail_marker'}))
    run('F: runs falling into the next (unbraced, braced) as calls of it: pass', 0,
        'with 4/4 bodies pasted back at their 4 labels in 1 sites', **falls)
    one_body = ('One', 'void', ['    ctx.target->Drop(1);', '    // no break here', '', '    ctx.target->Drop(2);',
                                ret])
    run('F: a fall-through moved as one body under both runs\' labels: fails', 1, 'DIFFERS',
        **form('falls', lab, fall_switch, [one_body] + fall_functions[2:], [(1, 'One'), (2, 'One')] + fall_rows[2:]))
    run('F: one body, the second run\'s ids missing: fails', 1, 'DIFFERS',
        **form('falls', {i: lab[i] for i in (1, 3, 4)}, fall_switch, [one_body] + fall_functions[2:],
               [(1, 'One')] + fall_rows[2:]))
    run('F: a call of the next run, that run\'s ids missing: fails', 1,
        'One ends in a call of Two, which is not the function of the run after it',
        **form('falls', {i: lab[i] for i in (1, 3, 4)}, fall_switch, fall_functions, [(1, 'One')] + fall_rows[2:]))
    run('F: a call of a run that is not the next: fails', 1,
        'One ends in a call of Three, which is not the function of the run after it',
        **mutated(falls, 'handlers', '    return Two(ctx);', '    return Three(ctx);'))
    run('F: the call dropped (the body falls off its end): fails', 1,
        'handler One: its last statement is not a return at its own level',
        **mutated(falls, 'handlers', '    return Two(ctx);\n', ''))
    run('F: a control header over the call of the next run: fails', 1,
        'handler One: its last statement is not a return at its own level',
        **mutated(falls, 'handlers', '    // no break here\n\n    return Two(ctx);',
                  '    // no break here\n    if (ctx.target->IsDead())\n    return Two(ctx);'))
    run('F: a call of a run still standing in the switch: fails', 1,
        'One ends in a call of Two, which is not the function of the run after it',
        **form('falls part', lab, fall_switch, fall_functions, [(1, 'One')] + fall_rows[2:], residual=True,
               standing=form_site['open'] + fall_switch[4:9] + form_site['close']))
    run('F: a call of the next row past a label still standing between them: fails', 1,
        'One ends in a call of Three, which is not the function of the run after it',
        **form('falls part', lab, fall_switch, [(n, v, ['    ctx.target->Drop(1);', '    // no break here', '',
                                                        '    return Three(ctx);']) if n == 'One' else (n, v, b)
                                                for n, v, b in fall_functions],
               [(1, 'One')] + fall_rows[2:], residual=True,
               standing=form_site['open'] + fall_switch[4:9] + form_site['close']))

    lab1, lab12, lab123 = ({i: lab[i] for i in ids} for ids in ((1,), (1, 2), (1, 2, 3)))
    drop1, drop3, drop9 = ('    ctx.target->Drop(%d);' % n for n in (1, 3, 9))
    two = [lab[1], '            target->Drop(1);', '            break;', lab[2], '            target->Drop(2);',
           '            break;']
    two_functions = [('One', 'void', [drop1, cont]), ('Two', 'void', ['    ctx.target->Drop(2);', cont])]
    two_rows = [(1, 'One'), (2, 'Two')]
    deeper = '            // One, deeper'
    lead = form('lead', lab12, [deeper] + two,
                [('One', 'void', ['        // One, deeper', drop1, cont])] + two_functions[1:], two_rows)
    not_comment = ("before the first label, a line that is not a `//` comment at the indent of the switch's `{` "
                   "or deeper: ")
    run('P: a comment before the first label, deeper than the label: passes', 0,
        'with 2/2 bodies pasted back at their 2 labels in 1 sites', **lead)
    run('P: three comments at the brace\'s indent and deeper: pass', 0,
        'with 2/2 bodies pasted back at their 2 labels in 1 sites',
        **form('lead', lab12, ['    // the switch', '            // One', '            // more of One'] + two,
               [('One', 'void', ['// the switch', '        // One', '        // more of One', drop1, cont])]
               + two_functions[1:], two_rows))
    run('P: a comment before a braced first run: passes', 0,
        'with 2/2 bodies pasted back at their 2 labels in 1 sites',
        **form('lead', lab12, ['        // One', lab[1], '        {', '            target->Drop(1);',
                               '            break;', '        }'] + two[3:],
               [('One', 'void', ['    // One', drop1, cont])] + two_functions[1:], two_rows))
    run('P: the comment kept at the body\'s indent in the handler: fails', 1,
        "DIFFERS from the base after pasting back 2 bodies at 1 sites: line 8: base '            // One, deeper'",
        **mutated(lead, 'handlers', '        // One, deeper', '    // One, deeper'))
    run('P: the comment after the first statement in the handler: fails', 1,
        'the switch has 1 comment lines before its first label, and One does not begin with them',
        **mutated(lead, 'handlers', '        // One, deeper\n' + drop1, drop1 + '\n        // One, deeper'))
    run('P: the comment dropped from the handler: fails', 1, 'One does not begin with them',
        **mutated(lead, 'handlers', '        // One, deeper\n', ''))
    run('P: a statement before the first label: fails', 1, not_comment + "'            target->Lead();'",
        **mutated(lead, 'old_text', deeper, deeper + '\n            target->Lead();'))
    run('P: a blank line among the comments before the first label: fails', 1, not_comment + "''",
        **mutated(lead, 'old_text', deeper, deeper + '\n'))
    run('P: a comment before the first label shallower than the switch\'s brace: fails', 1,
        not_comment + "'  // One, deeper'", **mutated(lead, 'old_text', deeper, '  // One, deeper'))

    gap_functions = [('One', 'void', [drop1, cont]), ('Three', 'void', [drop3, cont])]
    gap_rows = [(1, 'One'), (2, 'One'), (3, 'Three')]
    three = [lab[3], '            target->Drop(3);', '            break;']
    gap = form('gap', lab123, [lab[1], '', lab[2], '            target->Drop(1);', '            break;'] + three,
               gap_functions, gap_rows)
    run('B: a blank line between two labels of one run: passes', 0,
        'with 2/2 bodies pasted back at their 3 labels in 1 sites', **gap)
    run('B: two blank lines between two labels of one braced run: pass', 0,
        'with 2/2 bodies pasted back at their 3 labels in 1 sites',
        **form('gap', lab123, [lab[1], '', '', lab[2], '        {', '            target->Drop(1);',
                               '            break;', '        }'] + three, gap_functions, gap_rows))
    run('B: the blank line pasted after the body as well: fails', 1, "line 13: base '        case 3:",
        **mutated(gap, 'handlers', drop1 + '\n' + cont, drop1 + '\n' + cont + '\n'))
    run1 = '\n            target->Drop(1);\n            break;\n'
    after_body = mutated(gap, 'old_text', lab[1] + '\n\n' + lab[2] + run1, lab[1] + '\n' + lab[2] + run1 + '\n')
    run('B: the blank line after the body, the handler without it: fails', 1,
        "DIFFERS from the base after pasting back 2 bodies at 1 sites: line 12: base ''", **after_body)
    run('B: the blank line after the body, the handler holding it: passes', 0,
        'with 2/2 bodies pasted back at their 3 labels in 1 sites',
        **mutated(after_body, 'handlers', drop1 + '\n' + cont, drop1 + '\n' + cont + '\n'))
    run('B: a comment line between two labels of one run: fails', 1, "line 9: base '        // between'",
        **mutated(gap, 'old_text', lab[1] + '\n\n', lab[1] + '\n        // between\n'))

    falling = two_functions[:1] + [('Default', 'void', [drop9, cont])]
    off = form('off', lab1, two[:3] + ['        default:', '            target->Drop(9);'], falling, [(1, 'One')],
               default='Default')
    nine = '            target->Drop(9);'
    run('E: a default that falls off the switch\'s end: passes', 0,
        'with 2/2 bodies pasted back at their 1 labels in 1 sites', **off)
    run('E: a braced default that falls off the switch\'s end: passes', 0,
        'with 2/2 bodies pasted back at their 1 labels in 1 sites',
        **form('off', lab1, two[:3] + ['        default:', '        {', nine, '        }'], falling, [(1, 'One')],
               default='Default'))
    run('E: a default ending in break: passes', 0, 'with 2/2 bodies pasted back at their 1 labels in 1 sites',
        **mutated(off, 'old_text', nine, nine + '\n            break;'))
    run('E: a default ending in return: passes', 0, 'with 2/2 bodies pasted back at their 1 labels in 1 sites',
        **mutated(mutated(off, 'old_text', nine, nine + '\n            return;'),
                  'handlers', drop9 + '\n' + cont, drop9 + '\n' + ret))
    run('E: the falling default\'s handler ending in Return: fails', 1,
        'the default falls off the switch\'s end, and Default does not end in Continue',
        **mutated(off, 'handlers', drop9 + '\n' + cont, drop9 + '\n' + ret))
    run('E: a default whose return stands under an if: fails', 1,
        "line 14: base '    }', rebuilt '            break;'",
        **mutated(mutated(off, 'old_text', nine, '            if (target->IsDead())\n                return;'),
                  'handlers', drop9 + '\n' + cont,
                  '    if (ctx.target->IsDead())\n        return SpellHandlerOutcome<void>::Return();\n' + cont))
    run('E: the last labelled run falling off the switch\'s end: fails', 1,
        "the run under ['case 2:'] falls off the switch's end, which only a `default:` may",
        **form('off', lab12, two[:-1], two_functions, two_rows))

    # ORIGINALS: a repository of five commits in this order, ORIGINAL first, each named by its spelling
    # padded to a full SHA; the fixture file holds an unrelated line at ORIGINAL that it no longer holds at
    # its own original, and is missing at the second. Two commits off that line share the prefix 8000000.
    # Around that line: 1000000, ORIGINAL's parent; BASE, after the fifth; a000000, a side branch off the
    # second holding the file; b000000, a merge of the fourth (its first parent) and a000000.
    def full(spelling):
        return spelling.ljust(40, '0')
    commits = [full(c) for c in (ORIGINAL, '2000000', '3000000', '4000000', '5000000')]
    others = ['8000000a' + '0' * 32, '8000000b' + '0' * 32]
    root, side, merge = full('1000000'), full('a000000'), full('b000000')
    parents = {c: [p] for p, c in zip([root] + commits, commits + [full(BASE)])}
    parents.update({side: [commits[1]], merge: [commits[3], side]})
    unrelated = SELF_OLD.replace('#include "A.h"', '#include "Old.h"')
    repo = {(full(ORIGINAL), 'fixture'): unrelated, (full('3000000'), 'fixture'): SELF_OLD,
            (full('4000000'), 'fixture'): SELF_OLD, (full('5000000'), 'fixture'): SELF_OLD,
            (full(BASE), 'fixture'): SELF_OLD, (side, 'fixture'): SELF_OLD}
    tree = {'fixture': SELF_SITES, 'Handlers.cpp': SELF_HANDLERS, 'Thing.h': SELF_HEADERS['Thing.h']}
    tags = {}

    def ancestry(commit):
        """`commit` and every commit it descends from, through every parent."""
        found, todo = set(), [commit]
        while todo:
            c = todo.pop()
            if c not in found:
                found.add(c)
                todo += parents.get(c, [])
        return found

    def fake_git(*args):
        dag = list(parents) + [root]
        known = dag + others
        if args[:3] == ('rev-parse', '--verify', '--quiet') and args[3].endswith('^{commit}'):
            name = args[3][:-len('^{commit}')]
            if name in known:
                return 0, name + '\n', ''
            if name in tags:
                return 0, tags[name] + '\n', ''
            found = [c for c in known if c.startswith(name)]
            return (0, found[0] + '\n', '') if len(found) == 1 else (1, '', '')
        if args[0] == 'rev-parse' and args[1].startswith('--disambiguate='):
            prefix = args[1][len('--disambiguate='):]
            return 0, ''.join(c + '\n' for c in known if c.startswith(prefix)), ''
        if args[:2] == ('cat-file', '-e'):
            ref, _, path = args[2].partition(':')
            return (0 if (ref, path) in repo else 128), '', ''
        if args[:2] == ('merge-base', '--is-ancestor'):
            if args[2] not in dag or args[3] not in dag:
                return 128, '', 'fatal: not a valid commit'
            return (0 if args[2] in ancestry(args[3]) else 1), '', ''
        if args[0] == 'show':
            ref, _, path = args[1].partition(':')
            return (0, repo[(ref, path)], '') if (ref, path) in repo else (128, '', 'fatal: no such path')
        if args[0] == 'ls-tree':
            return 0, '', ''
        raise AssertionError('fake git: %r' % (args,))

    def originals(label, want_rc, needle, original=None, base='4000000', with_original=True, refs=None, argv=None,
                  anchor=None):
        """`refs`: tags of the fake repository, {name: spelling of the commit}; `argv`: run main() with it."""
        spec = dict(SELF_SPEC, original=original) if original else SELF_SPEC
        tags.clear()
        tags.update((name, full(c)) for name, c in (refs or {}).items())
        got = []
        real_check = check
        try:
            if argv:
                globals()['check'] = lambda root, base, original=False, anchor=None: real_check(
                    root, base, got.append, original, fake_git, tree.__getitem__, {'fixture': spec}, anchor, [])
                rc = main(argv)
            else:
                rc = check('.', base, got.append, with_original, fake_git, tree.__getitem__, {'fixture': spec},
                           anchor, [])
        except Exception as e:                                  # a crash fails the row
            rc = 2
            got.append('crashed: %r' % e)
        finally:
            globals()['check'] = real_check
            tags.clear()
        text = '\n'.join(got)
        ok = rc == want_rc and needle in text
        print('self-test: %-66s %s' % (label, 'PASS' if ok else 'FAIL'))
        if not ok:
            failures.append('%s: rc %d (want %d)\n%s' % (label, rc, want_rc, text))

    identical = 'fixture: IDENTICAL to the base, byte for byte, with 5/5 bodies'
    originals('ORIGINALS: the file at its own original: passes', 0, identical, original='3000000')
    originals('ORIGINALS: the same file at ORIGINAL: fails on the unrelated line', 1,
              "fixture: DIFFERS from the base after pasting back 5 bodies at 2 sites: line 1: base "
              "'#include \"Old.h\"', rebuilt '#include \"A.h\"'")
    originals('ORIGINALS: an original equal to the base: passes', 0, identical, original='4000000')
    originals('ORIGINALS: an original ahead of the base: fails', 1,
              'fixture: FAILED: the original 5000000 is ahead of the base 4000000', original='5000000')
    originals('ORIGINALS: an original not in the repository: fails', 1,
              'fixture: FAILED: the original 6000000 is not a commit of the repository', original='6000000')
    originals('ORIGINALS: an original that does not hold the file: fails', 1,
              'fixture: FAILED: the original 2000000 does not hold the file', original='2000000')
    originals('ORIGINALS: an original spelled as a ref name: fails', 1,
              "fixture: FAILED: the original 'HEAD' is not spelled as a commit's hex SHA", original='HEAD')
    originals('ORIGINALS: without --original the base is read: passes', 0, identical,
              original=ORIGINAL, with_original=False)
    originals('ORIGINALS: a base not in the repository: fails', 1,
              'fixture: FAILED: the base 6000000 is not a commit of the repository', base='6000000',
              with_original=False)
    originals('ORIGINALS: a base not in the repository, with --original: fails', 1,
              'fixture: FAILED: the base 6000000 is not a commit of the repository', original='3000000',
              base='6000000')
    originals('ORIGINALS: a tag spelled like a real commit\'s prefix: fails', 1,
              'fixture: FAILED: the original 2000000 names %s, not the commit it spells' % full('4000000'),
              original='2000000', refs={'2000000': '4000000'})
    originals('ORIGINALS: a tag spelled like no commit: fails', 1,
              'fixture: FAILED: the original 7000000 names %s, not the commit it spells' % full('3000000'),
              original='7000000', refs={'7000000': '3000000'})
    originals('ORIGINALS: an ambiguous abbreviation: fails', 1,
              'fixture: FAILED: the original 8000000 is ambiguous: 2 objects of the repository begin with it',
              original='8000000')
    originals('ORIGINALS: a base shadowed by a tag of its spelling: fails', 1,
              'fixture: FAILED: the base 4000000 names %s, not the commit it spells' % full('5000000'),
              refs={'4000000': '5000000'}, with_original=False)
    originals('ORIGINALS: an original spelled as a SHA and more: fails', 1,
              "fixture: FAILED: the original '4000000~1' is not spelled as a commit's hex SHA", original='4000000~1')
    originals('ORIGINALS: main() with --original reads the original: fails', 1,
              "fixture: DIFFERS from the base after pasting back 5 bodies at 2 sites: line 1: base "
              "'#include \"Old.h\"'", argv=['verbatim.py', '--check', '--original', '--base', '4000000'])
    originals('ORIGINALS: main() without --original reads the base: passes', 0, identical,
              argv=['verbatim.py', '--check', '--base', '4000000'])
    originals('ANCHOR: an original equal to the base, ahead of the anchor: fails', 1,
              'fixture: FAILED: the original 5000000 is ahead of the anchor 4000000', original='5000000',
              base='5000000', anchor='4000000')
    originals('ANCHOR: an original equal to the anchor: passes', 0,
              'verbatim: each original is measured against the anchor %s\n%s' % (full('4000000'), identical),
              original='4000000', base='5000000', anchor='4000000')
    originals('ANCHOR: an anchor not in the repository: fails', 1,
              'verbatim: FAILED: the anchor 6000000 is not a commit of the repository', original='3000000',
              anchor='6000000')
    originals('ANCHOR: an empty anchor: fails', 1,
              "verbatim: FAILED: the anchor '' is not spelled as a commit's hex SHA", original='3000000', anchor='')
    originals('ANCHOR: main() with --anchor measures against it: fails', 1,
              'fixture: FAILED: the original 5000000 is ahead of the anchor 4000000', original='5000000',
              argv=['verbatim.py', '--check', '--original', '--base', '5000000', '--anchor', '4000000'])
    originals('ANCHOR: an original on a side branch, an ancestor of neither: fails', 1,
              'fixture: FAILED: the original a000000 is ahead of the anchor %s' % full('4000000'), original='a000000',
              base='5000000', anchor='4000000')
    originals('ANCHOR: BASE ahead of the anchor, read from the data, an original set to it: fails', 1,
              'fixture: FAILED: the original %s is ahead of the anchor %s' % (BASE, full('4000000')), original=BASE,
              argv=['verbatim.py', '--check', '--original', '--anchor', '4000000'])
    originals('ANCHOR: the global ORIGINAL ahead of the anchor: fails', 1,
              'fixture: FAILED: the original %s is ahead of the anchor %s' % (ORIGINAL, root), anchor='1000000')
    originals('ANCHOR: a merge commit as the anchor, the original on its second parent: passes', 0,
              'verbatim: each original is measured against the anchor %s\n%s' % (merge, identical), original='a000000',
              anchor='b000000')

    # renamed_from: a file renamed from old_p to new_p between 3000000 and 4000000.
    old_p, new_p = 'src/game/WorldHandlers/Sites.cpp', 'src/game/spells/auras/Sites.cpp'

    def renamed(label, want_rc, needles, files, sites, base='4000000', work=None, original=False):
        """check() with `files` {(commit's spelling, path): text} and `work` as the working tree."""
        needles = [needles] if isinstance(needles, str) else needles
        work = dict(moved_tree) if work is None else work
        files = {(full(c), path): text for (c, path), text in files.items()}

        def git(*args):
            if args[:3] == ('rev-parse', '--verify', '--quiet'):
                found = [c for c in commits if c.startswith(args[3][:-len('^{commit}')])]
                return (0, found[0] + '\n', '') if len(found) == 1 else (1, '', '')
            if args[:2] == ('cat-file', '-e'):
                ref, _, path = args[2].partition(':')
                return (0 if (ref, path) in files else 128), '', ''
            if args[:2] == ('merge-base', '--is-ancestor'):
                return (0 if commits.index(args[2]) <= commits.index(args[3]) else 1), '', ''
            if args[0] == 'show':
                ref, _, path = args[1].partition(':')
                return (0, files[(ref, path)], '') if (ref, path) in files else (128, '', 'fatal: no such path')
            return 0, '', ''

        def read(path):
            if path not in work:
                raise FileNotFoundError(path)
            return work[path]
        got = []
        try:
            rc = check('.', base, got.append, original, git, read, sites, blocks=[])
        except Exception as e:                                  # a crash fails the row
            rc = 2
            got.append('crashed: %r' % e)
        text = '\n'.join(got)
        ok = rc == want_rc and all(n in text for n in needles)
        print('self-test: %-66s %s' % (label, 'PASS' if ok else 'FAIL'))
        if not ok:
            failures.append('%s: rc %d (want %d)\n%s' % (label, rc, want_rc, text))

    moved = {new_p: dict(SELF_SPEC, renamed_from=old_p)}
    moved_tree = {new_p: SELF_SITES, 'Handlers.cpp': SELF_HANDLERS, 'Thing.h': SELF_HEADERS['Thing.h']}
    before, after = {('3000000', old_p): SELF_OLD}, {('4000000', new_p): SELF_OLD}
    renamed('renamed_from: a commit holding the old path reads it: IDENTICAL', 0,
            [new_p + ': IDENTICAL to the base', 'verbatim: OK'], before, moved, base='3000000')
    renamed('renamed_from: a commit holding the new path reads it: IDENTICAL', 0,
            [new_p + ': IDENTICAL to the base', 'verbatim: OK'], after, moved)
    renamed('renamed_from: an original holding the old path reads it: IDENTICAL', 0,
            new_p + ': IDENTICAL to the base', {**before, **after}, dict(moved, **{new_p: dict(
                moved[new_p], original='3000000')}), original=True)
    renamed('renamed_from: changed content at the old path DIFFERS', 1, new_p + ': DIFFERS from the base',
            {('3000000', old_p): SELF_OLD.replace('if (i == 1)', 'if (i == 2)')}, moved, base='3000000')
    renamed('renamed_from: a wrong old path is refused by name', 1,
            new_p + ': FAILED: neither it nor its renamed_from src/game/Object/Sites.cpp is there at %s'
            % full('3000000'),
            before, {new_p: dict(SELF_SPEC, renamed_from='src/game/Object/Sites.cpp')}, base='3000000')
    renamed('renamed_from: a commit holding both paths is refused', 1,
            new_p + ': FAILED: both it and its renamed_from %s are there at %s' % (old_p, full('4000000')),
            {**after, ('4000000', old_p): SELF_OLD}, moved)
    renamed('renamed_from: the old path still in the working tree is refused', 1,
            new_p + ': FAILED: its renamed_from %s is still in the working tree' % old_p, before, moved,
            base='3000000', work=dict(moved_tree, **{old_p: SELF_SITES}))
    renamed('a key without renamed_from, not at the base: FAILED', 1,
            new_p + ': FAILED: it is not there at %s' % full('3000000'),
            before, {new_p: SELF_SPEC}, base='3000000')
    renamed('a key the working tree does not hold: FAILED by name', 1,
            [old_p + ': FAILED: the working tree has no such file', 'verbatim: FAILED'], before, {old_p: SELF_SPEC},
            base='3000000')
    renamed('a members_of header the working tree does not hold: FAILED', 1,
            new_p + ': FAILED: the working tree has no Thing.h, its members_of', before, moved, base='3000000',
            work={new_p: SELF_SITES, 'Handlers.cpp': SELF_HANDLERS})

    # BLOCKS: a repository of five commits in this order, the base second, the move's merge third, HEAD
    # fourth; the origin stands as it was before the move at the first, second and fifth.
    block_commits = [full(c) for c in ('1b00000', '2b00000', '3b00000', '4b00000', '5b00000')]
    origin, header = BLOCK_ENTRY['origin'], BLOCK_ENTRY['header']
    block_files = {(block_commits[0], origin): BLOCK_ORIGIN, (block_commits[1], origin): BLOCK_ORIGIN,
                   (block_commits[2], origin): BLOCK_MOVED, (block_commits[3], origin): BLOCK_MOVED,
                   (block_commits[4], origin): BLOCK_ORIGIN, (block_commits[1], 'fixture'): SELF_OLD}
    block_tree = {origin: BLOCK_MOVED, header: BLOCK_HEADER, 'src/game/Other.cpp': 'void Other() {}\n',
                  'fixture': SELF_SITES, 'Handlers.cpp': SELF_HANDLERS, 'Thing.h': SELF_HEADERS['Thing.h']}

    def block_run(entry=None, tree=None, files=None, sites=None, anchor=None, argv=None, original=None, listed=True,
                  disk=None):
        """(rc, output) of check_block() on `entry` (BLOCK_ENTRY changed by it) in the working tree `tree`
        (block_tree changed by it, None deleting a file), the commits holding `files` (block_files changed by
        it); with `argv` main(), with `original` check(), each with the sites `sites` and the block; with
        `disk` ({path: bytes} over the tree) check() reading the tree written to a directory."""
        entry = {k: v for k, v in dict(BLOCK_ENTRY, **(entry or {})).items() if v is not None}
        work = {k: v for k, v in dict(block_tree, **(tree or {})).items() if v is not None}
        have = {**block_files, **{(full(c), p): t for (c, p), t in (files or {}).items()}}
        have = {k: v for k, v in have.items() if v is not None}
        sites = {} if sites is None else sites

        def git(*args):
            if args[:3] == ('rev-parse', '--verify', '--quiet'):
                found = [c for c in block_commits if c.startswith(args[3][:-len('^{commit}')])]
                return (0, found[0] + '\n', '') if len(found) == 1 else (1, '', '')
            if args[0] == 'rev-parse':
                return 0, '', ''
            if args[:2] == ('merge-base', '--is-ancestor'):
                a, b = (block_commits[3] if c == 'HEAD' else c for c in args[2:4])
                return (0 if block_commits.index(a) <= block_commits.index(b) else 1), '', ''
            if args[:2] == ('cat-file', '-e'):
                return (0 if tuple(args[2].partition(':')[::2]) in have else 128), '', ''
            if args[0] == 'show':
                ref, _, path = args[1].partition(':')
                return (0, have[(ref, path)], '') if (ref, path) in have else (128, '', 'fatal: no such path')
            if args[0] == 'ls-files':
                if not listed:
                    return 128, '', 'fatal: not a git repository'
                specs = [d for d in args[args.index('--') + 1:] if d != '.'] if '--' in args else []
                paths = [p for p in sorted(work) + ['src/game/Gone.cpp']
                         if not specs or any(p == d or p.startswith(d + '/') for d in specs)]
                return 0, ''.join(p + ('\0' if '-z' in args else '\n') for p in paths), ''
            if args[0] == 'ls-tree':
                return 0, '', ''
            raise AssertionError('fake git: %r' % (args,))

        def read(path):
            if path not in work:
                raise FileNotFoundError(path)
            return work[path]
        got = []
        real_check = check
        try:
            if argv:
                globals()['check'] = lambda root, base, original=False, anchor=None: real_check(
                    root, base, got.append, original, git, read, sites, anchor, [entry])
                rc = main(argv)
            elif original is not None:
                rc = check('.', '2b00000', got.append, original, git, read, sites, anchor, [entry])
            elif disk is not None:
                where = tempfile.mkdtemp()
                try:
                    for path, data in dict({p: t.encode('utf-8') for p, t in work.items()}, **disk).items():
                        os.makedirs(os.path.join(where, os.path.dirname(path)), exist_ok=True)
                        with open(os.path.join(where, path), 'wb') as f:
                            f.write(data)
                    rc = check(where, '2b00000', got.append, False, git, None, sites, None, [entry])
                finally:
                    shutil.rmtree(where, ignore_errors=True)
            else:
                try:
                    got.append(check_block(entry, sites, git, read, read,
                                           anchor and full(anchor)))
                    rc = 0
                except Failure as e:
                    got.append('FAILED: %s' % e)
                    rc = 1
        except Exception as e:                                  # a crash fails the row
            rc = 2
            got.append('crashed: %r' % e)
        finally:
            globals()['check'] = real_check
        return rc, '\n'.join(got)

    def blocked(label, want_rc, needles, **run):
        """block_run(**run) giving `want_rc` and the `needles` in order."""
        needles = [needles] if isinstance(needles, str) else needles
        rc, text = block_run(**run)
        at, ok = 0, rc == want_rc
        for needle in needles:
            at = text.find(needle, at)
            ok = ok and at >= 0
        print('self-test: %-66s %s' % (label, 'PASS' if ok else 'FAIL'))
        if not ok:
            failures.append('%s: rc %d (want %d)\n%s' % (label, rc, want_rc, text))

    moved_line = ("src/game/Far.cpp -> src/game/spells/FarOrder.h: IDENTICAL to the base %s, byte for byte: the block "
                  "of 13 lines at its line 12 stands once in the header; the origin, read %%s, is the base's less the "
                  "block and the 2 blank line(s) after it, with 2 added line(s); its types Pair, FarOrder are defined "
                  "nowhere else under src/" % block_commits[1])
    twin = "FAILED: type FarOrder is defined again in src/game/Other.cpp, a twin of the block's"
    twin_file = {'src/game/Other.cpp': 'struct FarOrder : public Pair<int>\n{\n};\n'}
    blocked('BLOCKS: a block moved into a header byte for byte: IDENTICAL', 0, moved_line % 'in the working tree')
    blocked('BLOCKS: its first line twice in the origin at the base: fails', 1,
            "FAILED: the block's first line found 2 times in the origin at the base",
            files={('2b00000', origin): BLOCK_ORIGIN.replace('void Far::Sort', '// Order of the far targets\nvoid '
                                                                                'Far::Sort')})
    blocked('BLOCKS: its first line not in the origin at the base: fails', 1,
            "FAILED: the block's first line found 0 times in the origin at the base",
            entry={'first': '// Order of the near targets'})
    blocked('BLOCKS: a block running past the end of the origin: fails', 1,
            "FAILED: the block's 99 lines run past the end of the origin at the base", entry={'lines': 99})
    blocked('BLOCKS: a block whose braces do not balance: fails', 1, "FAILED: the block's braces do not balance",
            entry={'lines': 12})
    blocked('BLOCKS: a block whose last line is not `};`: fails', 1,
            "FAILED: the block's last line is not `};`, the close of a type: ''", entry={'lines': 14})
    blocked('BLOCKS: a block that defines no type: fails', 1,
            'FAILED: the block defines no type (no column-0 `struct N` or `class N` line)',
            entry={'first': 'enum FarKind', 'lines': 4})
    commented = BLOCK_LINES[:1] + ['/* the old order:', 'struct OldOrder', '*/'] + BLOCK_LINES[1:]
    blocked('BLOCKS: a struct line inside a comment in the block names no type: passes', 0,
            'the block of 16 lines at its line 12 stands once in the header',
            entry={'lines': 16}, files={('2b00000', origin): BLOCK_ORIGIN.replace('\n'.join(BLOCK_LINES),
                                                                                 '\n'.join(commented))},
            tree={header: '#pragma once\n\n' + '\n'.join(commented) + '\n'})
    blocked('BLOCKS: an entry with a key missing: fails', 1,
            "FAILED: the entry's keys are ['base', 'first', 'header', 'lines', 'origin'], and a block entry has "
            "exactly ['base', 'origin', 'header', 'first', 'lines', 'added']", entry={'added': None})
    blocked('BLOCKS: a lines of 0: fails', 1, 'FAILED: its lines 0 is not a positive count', entry={'lines': 0})
    blocked('BLOCKS: a lines spelled as text: fails', 1, "FAILED: its lines '13' is not a positive count",
            entry={'lines': '13'})
    blocked('BLOCKS: the header holding the block twice: fails', 1,
            'FAILED: the header holds the block 2 times (contiguous, byte for byte)',
            tree={header: BLOCK_HEADER + '\n'.join(BLOCK_LINES) + '\n'})
    blocked('BLOCKS: the header\'s copy one byte off: fails', 1,
            'FAILED: the header holds the block 0 times (contiguous, byte for byte)',
            tree={header: BLOCK_HEADER.replace('a > b', 'a >= b')})
    blocked('BLOCKS: no header in the working tree: fails', 1,
            'FAILED: the working tree has no header src/game/spells/FarOrder.h', tree={header: None})
    blocked('BLOCKS: the header\'s copy inside a comment: fails', 1,
            'FAILED: type Pair is defined 0 times in the header',
            tree={header: '#pragma once\n\n/*\n' + '\n'.join(BLOCK_LINES) + '\n*/\n'})
    blocked('BLOCKS: a second definition in the header: fails', 1,
            'FAILED: type FarOrder is defined 2 times in the header',
            tree={header: BLOCK_HEADER + '\nclass FarOrder final\n{\n};\n'})
    blocked('BLOCKS: a twin type in a third file under src/: fails', 1, twin, tree=twin_file)
    sources = ['.c', '.cc', '.cpp', '.cxx', '.h', '.hh', '.hpp', '.hxx', '.inc', '.inl', '.ipp']
    missed = [(ext, run) for ext in sources
              for run in [block_run(tree={'src/shared/Twin' + ext: twin_file['src/game/Other.cpp']})]
              if run != (1, twin.replace('src/game/Other.cpp', 'src/shared/Twin' + ext))]
    label = 'BLOCKS: a twin in each C or C++ file kind under src/: fails'
    print('self-test: %-66s %s' % (label, 'PASS' if not missed else 'FAIL'))
    if missed:
        failures.append('%s: not refused as a twin: %s' % (label, missed))
    blocked('BLOCKS: a definition outside src/ is no twin: passes', 0, moved_line % 'in the working tree',
            tree={'dep/Twin.cpp': twin_file['src/game/Other.cpp']})
    blocked('BLOCKS: the block left in the origin as well: fails as a twin', 1,
            "FAILED: type Pair is defined again in src/game/Far.cpp, a twin of the block's",
            tree={origin: BLOCK_ORIGIN.replace('#include "Far.h"\n', '#include "Far.h"\n#include "spells/FarOrder.h"\n')
                  .replace('{\n}\n\n', '{\n}\ntemplate void Far::Pick<int>();\n\n', 1)})
    blocked('BLOCKS: a declaration, a use and a comment elsewhere are no twin: passes', 0,
            moved_line % 'in the working tree',
            tree={'src/game/Other.cpp': 'struct FarOrder;\nclass Pair;\nvoid Use(struct FarOrder* order) {}\n'
                                        'template <class FarOrder> void Sort() {}\n// struct FarOrder { };\n'
                                        'struct FarOrderLess { };\nstruct FarOrder::Inner { };\n'})
    blocked('BLOCKS: a definition in a file that is not C or C++: no twin, passes', 0,
            moved_line % 'in the working tree', tree={'src/tests/tools/fixture.py': twin_file['src/game/Other.cpp']})
    blocked('BLOCKS: the origin with a change not listed in added: fails', 1,
            "FAILED: the origin as read, less its added lines, is not the base's less the block: line 14: base "
            "'    list.sort(FarOrder());', read '    list.sort(FarOrder(1));'",
            tree={origin: BLOCK_MOVED.replace('FarOrder()', 'FarOrder(1)')})
    ends = {('2b00000', origin): BLOCK_ORIGIN[:BLOCK_ORIGIN.index('\n\n\nvoid Far::Sort')] + '\n'}
    ends_moved = BLOCK_MOVED[:BLOCK_MOVED.index('void Far::Sort')]
    blocked('BLOCKS: a block that ends the origin: IDENTICAL', 0,
            "the origin, read in the working tree, is the base's less the block and the 0 blank line(s) after it",
            files=ends, tree={origin: ends_moved})
    blocked('BLOCKS: a block that ends the origin, the final newline dropped: fails', 1,
            "FAILED: the origin as read, less its added lines, is not the base's less the block: lengths differ: "
            "base 12 lines, read 11", files=ends, tree={origin: ends_moved[:-1]})
    blocked('BLOCKS: a listed added line missing from the origin: fails', 1,
            "FAILED: added line 'template void Far::Pick<int>();' found 0 times in the origin as read",
            tree={origin: BLOCK_MOVED.replace('template void Far::Pick<int>();\n', '')})
    blocked('BLOCKS: a listed added line standing twice: fails', 1,
            "FAILED: added line 'template void Far::Pick<int>();' found 2 times in the origin as read",
            tree={origin: BLOCK_MOVED + 'template void Far::Pick<int>();\n'})
    blocked('BLOCKS: a base that is not an ancestor of HEAD: fails', 1,
            'FAILED: the base 5b00000 is not an ancestor of HEAD', entry={'base': '5b00000'})
    blocked('BLOCKS: a base that is not an ancestor of the anchor: fails', 1,
            'FAILED: the base 3b00000 is not an ancestor of the anchor %s' % block_commits[1],
            entry={'base': '3b00000'}, anchor='2b00000')
    blocked('BLOCKS: a base spelled as a ref name: fails', 1,
            "FAILED: the base 'HEAD' is not spelled as a commit's hex SHA", entry={'base': 'HEAD'})
    blocked('BLOCKS: the origin missing from the working tree: fails', 1,
            'FAILED: the working tree has no src/game/Far.cpp', tree={origin: None})
    blocked('BLOCKS: the origin missing at the base: fails', 1,
            'FAILED: cannot read the origin at the base %s from git: fatal: no such path' % block_commits[1],
            files={('2b00000', origin): None})
    blocked('BLOCKS: the files under src/ not listed: fails', 1,
            'FAILED: cannot list the files under src/: fatal: not a git repository', listed=False)
    blocked('BLOCKS: the tree read from disk, a file not UTF-8 searched: its twin fails', 1,
            ['src/game/Far.cpp -> src/game/spells/FarOrder.h: ' + twin, 'verbatim: FAILED'], tree=twin_file,
            disk={'src/game/Other.cpp': b'// caf\xe9\n' + twin_file['src/game/Other.cpp'].encode('utf-8')})
    at_original = {origin: {'original': '3b00000'}}
    later = {origin: BLOCK_MOVED.replace('    list.sort(FarOrder());\n', '    Dispatch();\n')}
    blocked('BLOCKS: the origin read at its SITES entry\'s original: IDENTICAL', 0,
            moved_line % ('at ' + block_commits[2]), tree=later, sites=at_original)
    blocked('BLOCKS: the same, the working tree differing, without the entry: fails', 1,
            "is not the base's less the block: line 14: base '    list.sort(FarOrder());', read '    Dispatch();'",
            tree=later)
    blocked('BLOCKS: a SITES entry naming no original: fails', 1,
            'FAILED: the origin has a SITES entry, and the entry names no original to read it at', sites={origin: {}})
    blocked('BLOCKS: a SITES entry whose original is older than the base: fails', 1,
            "FAILED: the original 1b00000 of the origin's SITES entry does not descend from the base 2b00000",
            sites={origin: {'original': '1b00000'}})
    blocked('BLOCKS: the origin missing at its original: fails', 1,
            'FAILED: cannot read the origin at its original %s from git: fatal: no such path' % block_commits[2],
            sites=at_original, files={('3b00000', origin): None})
    blocked('BLOCKS: a twin in the origin read at its original: fails', 1,
            "FAILED: type FarOrder is defined again in src/game/Far.cpp at %s, a twin of the block's"
            % block_commits[2], sites=at_original, files={('3b00000', origin): BLOCK_MOVED + 'class FarOrder {};\n'},
            entry={'added': BLOCK_ENTRY['added'] + ['class FarOrder {};']})
    blocked('BLOCKS: --original runs the block checks as --check does', 0,
            [moved_line % 'in the working tree', 'verbatim: OK'], original=True)
    blocked('BLOCKS: main() runs them after the site checks, a refusal failing the run', 1,
            ['fixture: IDENTICAL to the base, byte for byte',
             'src/game/Far.cpp -> src/game/spells/FarOrder.h: ' + twin, 'verbatim: FAILED'],
            sites={'fixture': SELF_SPEC}, tree=twin_file, argv=['verbatim.py', '--check', '--base', '2b00000'])
    blocked('BLOCKS: main() with --original and --anchor: the anchor reaches them', 1,
            'FAILED: the base 2b00000 is not an ancestor of the anchor %s' % block_commits[0],
            argv=['verbatim.py', '--check', '--original', '--anchor', '1b00000'])

    here = os.path.dirname(os.path.abspath(__file__))

    def blocks_replaced(data_source, text):
        """The data file's source with its BLOCKS assignment's lines replaced by `text` (lines)."""
        found = [n for n in ast.parse(data_source).body
                 if isinstance(n, ast.Assign) and [getattr(t, 'id', None) for t in n.targets] == ['BLOCKS']]
        lines = data_source.split('\n')
        return '\n'.join(lines[:found[0].lineno - 1] + text + lines[found[0].end_lineno:])
    try:
        with open(os.path.join(here, 'verbatim.py'), encoding='utf-8-sig') as f:
            tool_source = f.read()
        with open(os.path.join(here, 'verbatim_sites.py'), encoding='utf-8-sig') as f:
            data_source = f.read()
        bound = split_gate.problems('verbatim.py', tool_source + 'BLOCKS = []\n', 'verbatim_sites', data_source,
                                    DATA_NAMES)
        unset = split_gate.problems('verbatim.py', tool_source, 'verbatim_sites', blocks_replaced(data_source, []),
                                    DATA_NAMES)
        ok = (any('binds BLOCKS, a data name of verbatim_sites.py' in p for p in bound)
              and 'verbatim_sites.py does not assign BLOCKS' in unset)
    except Exception as e:                                      # a crash fails the row
        ok, bound, unset = False, ['crashed: %r' % e], []
    label = 'the split: BLOCKS bound in the tool, or not assigned: REFUSED'
    print('self-test: %-66s %s' % (label, 'PASS' if ok else 'FAIL'))
    if not ok:
        failures.append('the split, BLOCKS: %s %s' % (bound, unset))

    # The self-test run again from a copy of this directory whose data holds a BLOCKS entry; that run (marked
    # by VERBATIM_SELF_TEST_NESTED) leaves this row out.
    if os.environ.get('VERBATIM_SELF_TEST_NESTED') != '1':
        where = tempfile.mkdtemp()
        try:
            for name in os.listdir(here):
                if name.endswith('.py'):
                    shutil.copy(os.path.join(here, name), where)
            with open(os.path.join(where, 'verbatim_sites.py'), 'w', encoding='utf-8', newline='') as f:
                f.write(blocks_replaced(data_source, [
                    "BLOCKS = [{'base': 'dc0af339ec81b2b2d7f70a05c145172dd3c80437',",
                    "           'origin': 'src/game/WorldHandlers/SpellTargeting.cpp',",
                    "           'header': 'src/game/WorldHandlers/SpellTargetDistanceOrder.h',",
                    "           'first': '// Helper for targets furthest away to the spell target',",
                    "           'lines': 20,",
                    "           'added': ['#include \"SpellTargetDistanceOrder.h\"',",
                    "                     'template WorldObject* '",
                    "                     'Spell::FindCorpseUsing<MaNGOS::CannibalizeObjectCheck>();']}]"]))
            nested = subprocess.run([sys.executable, '-W', 'error', '-B', os.path.join(where, 'verbatim.py'),
                                     '--self-test'], cwd=where, capture_output=True, text=True, timeout=600,
                                    env=dict(os.environ, VERBATIM_SELF_TEST_NESTED='1'))
            ok = nested.returncode == 0 and nested.stdout.rstrip('\n').endswith('self-test: PASS (0 failure(s))')
            text = nested.stdout[-2000:] + nested.stderr[-1000:]
        except Exception as e:                                  # a crash fails the row
            ok, text = False, 'crashed: %r' % e
        finally:
            shutil.rmtree(where, ignore_errors=True)
        label = 'the self-test with a BLOCKS entry in the data file: passes'
        print('self-test: %-66s %s' % (label, 'PASS' if ok else 'FAIL'))
        if not ok:
            failures.append('%s:\n%s' % (label, text))

    for label, bad in split_gate.self_test(__file__, 'verbatim_sites', DATA_NAMES, 'SITES', checked_first=True):
        print('self-test: %-66s %s' % (label, 'PASS' if not bad else 'FAIL'))
        failures += bad

    for f in failures:
        print('SELF-TEST FAILED: ' + f)
    print('self-test: %s (%d failure(s))' % ('PASS' if not failures else 'FAIL', len(failures)))
    return 1 if failures else 0


def main(argv):
    if split_gate.check(__file__, 'verbatim_sites', DATA_NAMES, globals()):
        return 1
    ap = argparse.ArgumentParser(description='The spell handler registry\'s verbatim proof (decoupling D11).')
    ap.add_argument('--root', default=os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..')))
    ap.add_argument('--base', default=BASE)
    ap.add_argument('--original', action='store_true', help='check each file against its original (ORIGINALS)')
    ap.add_argument('--anchor', help='under --original, the commit no original may be ahead of (ORIGINALS)')
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument('--check', action='store_true')
    g.add_argument('--self-test', action='store_true')
    args = ap.parse_args(argv[1:])
    if args.anchor is not None and not args.original:
        ap.error('--anchor applies to --original only')
    if args.self_test:
        return self_test()
    return check(os.path.abspath(args.root), args.base, original=args.original, anchor=args.anchor)


if __name__ == '__main__':
    sys.exit(main(sys.argv))
