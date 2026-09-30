#!/usr/bin/env python3
"""verbatim.py [--root <repo root>] [--base <ref>] --check | --self-test

The spell handler registry's verbatim proof (decoupling D11, design/2026-09-28-unit-reopening.md
3(b)): every case body that moved out of a per-spell-ID switch into a registered handler is pasted
back at its label, and the file must come back byte for byte as it was at BASE.

For each file in SITES, --check:
  1. reads the file at BASE (`git show <base>:<file>`) and in the working tree, and for each of the
     two its handlers: the handler file (HANDLERS) where that version has it, else the handler block
     appended to the sites' file (from TAIL_MARKER to the end of the file); a version holding both
     fails, and so does a working tree holding neither;
  2. drops the lines the moves added (ADDED: the handlers header's include) and that appended block;
  3. at each site, replaces the dispatch (the site's exact DISPATCH lines, found exactly once)
     with the switch it stood for: the switch's opening lines, then for each row of the site's
     registration table, in table order, the label line (LABELS) and -- once per run of rows that
     register the same function -- that function's body, reverse-substituted (SUBSTITUTIONS: the
     context accessors and the outcome) and re-indented to the label's body; then, if the site has
     a registered `default:` (DEFAULT, found through `registry.RegisterDefault<TRAITS>(&F);`), its
     label and F's body the same way; a site whose switch had no `default:` must have none registered
     (every site names its TRAITS for that). A label holding `{body}` is a one-line case
     (`case 1: x = 1; break;    // note`): the body's lines are joined there with one space.
     A partly moved site (RESIDUAL) keeps its switch, holding the labels not moved, directly after
     the dispatch: the dispatch is dropped and each run of rows goes back into that switch before
     its anchor (the line that followed the run: a label line found exactly once there, or the
     switch's close), the runs in table order; such a site registers no default (its `default:`,
     if any, stays in the switch);
  4. does the same to the file at BASE, for the sites a PR before this one moved (their dispatch
     is in the base; a site whose dispatch is not in the base is this PR's and must be a switch
     there), so BASE may be any commit from before the first move to the parent of this PR;
  5. compares the two, byte for byte, and names the first difference.
Because the bodies are pasted from the functions the table registers, a row pointing at the wrong
function, a lost row or default, a changed line, or a body moved in the wrong order all fail; a
body a PR before this one moved is proven again, against its own base's copy. Every
`RegisterDefault` in the handlers must be a line `registry.RegisterDefault<TRAITS>(&F);` of a site
with a `default:`, and every site's TRAITS must appear as `Dispatch<TRAITS>` in its own DISPATCH
lines, so no default is registered under a spelling the paste-back does not read; and every
`Register` stands inside ROWS_FUNCTION, which registers the tables' rows, so no labelled row is
registered outside the tables the paste-back reads.

Each handler body is also checked for what the paste-back cannot see: a live-out local used
bare (a case body's name the move did not route through the context), and so any other name in
scope at the site (IN_SCOPE: its function's parameters and locals); a member of the site's
class used bare (an implicit `this->` the move missed: in a free function the name could still
compile if a free function or global of that name exists, and then means something else) -- the
names are read from the class's own declaration (MEMBERS_OF), every member function and data
member at class scope; and a `return ...::Continue();` inside a loop or a nested switch (the
moved `break;` would have left that loop, not the case).

python src/tests/tools/verbatim.py --check        # against BASE (this PR's parent), reading git
python src/tests/tools/verbatim.py --check --base afdabc428   # against master before the first move
python src/tests/tools/verbatim.py --self-test    # fixtures only, no git
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
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from case_labels import blank  # noqa: E402  (the same comment/literal blanking as the ratchet)

# The tree the moved bodies are checked against: the parent of the latest move. `--base afdabc428`
# (before the first move) proves every site against the switches as they were.
BASE = 'fcaf75391'

VOID_SUBSTITUTIONS = [('return SpellHandlerOutcome<void>::Return();', 'return;'),
                      ('return SpellHandlerOutcome<void>::Continue();', 'break;')]

# One entry per file; each file lists its sites in the order they stand in it.
SITES = {
    'src/game/WorldHandlers/SpellAuraDummy.cpp': {
        'handlers': 'src/game/spells/handlers/AuraDummyHandlers.cpp',
        'rows_function': 'RegisterAuraDummyRows',
        'added': ['#include "spells/handlers/AuraDummyHandlers.h"'],
        'tail_marker': '// Decoupling D11 (design/2026-09-28-unit-reopening.md 3(b)): the spell handler registry\'s',
        'sites': [{
            'name': 'HandleAuraDummy AT APPLY, SPELLFAMILY_WARRIOR (switch (GetId()))',
            'dispatch': [
                '                AuraDummyApplyContext ctx(this, target);',
                '                if (SpellHandlerRegistry::Game().Dispatch<AuraDummyApplyWarriorSite>(GetId(), ctx).IsReturn())',
                '                {',
                '                    return;',
                '                }'],
            'open': ['                switch (GetId())', '                {'],
            'close': ['                }'],
            'label_indent': 20,
            'braced': True,
            'table': 'warriorApply',
            'traits': 'AuraDummyApplyWarriorSite',
            'context': 'AuraDummyApplyContext',
            'live_outs': ['target'],
            'members_of': ('src/game/WorldHandlers/SpellAuras.h', 'Aura'),
            'substitutions': [('ctx.target', 'target'), ('ctx.aura', 'this')] + VOID_SUBSTITUTIONS,
            'labels': {
                41099: '                    case 41099:                             // Battle Stance',
                41100: '                    case 41100:                             // Berserker Stance',
                41101: '                    case 41101:                             // Defensive Stance',
                53790: '                    case 53790:                             // Defensive Stance',
                53791: '                    case 53791:                             // Berserker Stance',
                53792: '                    case 53792:                             // Battle Stance'},
        }, {
            'name': 'HandleAuraDummy AT APPLY, SPELLFAMILY_WARRIOR, Overpower\'s Unrelenting Assault '
                    '(switch ((*itr)->GetSpellProto()->ID), in the loop)',
            'dispatch': [
                '                            AuraDummyUnrelentingAssaultContext assaultCtx(target, itr);',
                '                            if (SpellHandlerRegistry::Game()'
                '.Dispatch<AuraDummyUnrelentingAssaultSite>(',
                '                                    (*itr)->GetSpellProto()->ID, assaultCtx).IsReturn())',
                '                            {',
                '                                return;',
                '                            }'],
            'open': ['                            switch ((*itr)->GetSpellProto()->ID)',
                     '                            {'],
            'close': ['                            }'],
            'label_indent': 32,
            'braced': False,
            'table': 'unrelentingAssault',
            'traits': 'AuraDummyUnrelentingAssaultSite',
            'default': '                                default:',
            'context': 'AuraDummyUnrelentingAssaultContext',
            'live_outs': ['target', 'itr'],
            'in_scope': ['apply', 'Real', 'classOptions', 'caster', 'modifierAuras'],
            'members_of': ('src/game/WorldHandlers/SpellAuras.h', 'Aura'),
            'substitutions': [('ctx.target', 'target'), ('ctx.itr', 'itr')] + VOID_SUBSTITUTIONS,
            'labels': {
                46859: '                                case 46859:                 // Unrelenting Assault, rank 1',
                46860: '                                case 46860:                 // Unrelenting Assault, rank 2'},
        }, {
            'name': 'HandleAuraDummy AT REMOVE, the hunter quest-tame block (switch (GetId()))',
            'dispatch': [
                '            AuraDummyQuestTameContext tameCtx(finalSpellId);',
                '            if (SpellHandlerRegistry::Game()'
                '.Dispatch<AuraDummyQuestTameSite>(GetId(), tameCtx).IsReturn())',
                '            {',
                '                return;',
                '            }'],
            'open': ['            switch (GetId())', '            {'],
            'close': ['            }'],
            'label_indent': 16,
            'braced': False,
            'table': 'questTame',
            'traits': 'AuraDummyQuestTameSite',
            'context': 'AuraDummyQuestTameContext',
            'live_outs': ['finalSpellId'],
            'in_scope': ['apply', 'Real', 'target', 'classOptions', 'caster'],
            'members_of': ('src/game/WorldHandlers/SpellAuras.h', 'Aura'),
            'substitutions': [('ctx.finalSpellId', 'finalSpellId')] + VOID_SUBSTITUTIONS,
            'labels': {
                19548: '                case 19548: {body}',
                19674: '                case 19674: {body}',
                19687: '                case 19687: {body}',
                19688: '                case 19688: {body}',
                19689: '                case 19689: {body}',
                19692: '                case 19692: {body}',
                19693: '                case 19693: {body}',
                19694: '                case 19694: {body}',
                19696: '                case 19696: {body}',
                19697: '                case 19697: {body}',
                19699: '                case 19699: {body}',
                19700: '                case 19700: {body}',
                30646: '                case 30646: {body}',
                30653: '                case 30653: {body}',
                30654: '                case 30654: {body}',
                30099: '                case 30099: {body}',
                30102: '                case 30102: {body}',
                30105: '                case 30105: {body}'},
        }, {
            'name': 'HandleAuraDummy AT REMOVE (switch (GetId()), family-independent), its stance labels',
            'dispatch': [
                '        AuraDummyRemoveContext ctx(this, target);',
                '        if (SpellHandlerRegistry::Game().Dispatch<AuraDummyRemoveSite>(GetId(), ctx).IsReturn())',
                '        {',
                '            return;',
                '        }',
                ''],
            'open': ['        switch (GetId())', '        {'],
            'close': ['        }'],
            'residual': {
                41099: '            case 42454:                                     // Captured Totem',
                53790: '            case 56511:                                     '
                       '// Towers of Certain Doom: Tower Bunny Smoke Flare Effect'},
            'label_indent': 12,
            'braced': True,
            'table': 'stanceRemoval',
            'traits': 'AuraDummyRemoveSite',
            'context': 'AuraDummyRemoveContext',
            'live_outs': ['target'],
            'in_scope': ['apply', 'Real', 'classOptions'],
            'members_of': ('src/game/WorldHandlers/SpellAuras.h', 'Aura'),
            'substitutions': [('ctx.target', 'target'), ('ctx.aura', 'this')] + VOID_SUBSTITUTIONS,
            'labels': {
                41099: '            case 41099:                                     // Battle Stance',
                41100: '            case 41100:                                     // Berserker Stance',
                41101: '            case 41101:                                     // Defensive Stance',
                53790: '            case 53790:                                     // Defensive Stance',
                53791: '            case 53791:                                     // Berserker Stance',
                53792: '            case 53792:                                     // Battle Stance'},
        }, {
            'name': 'HandleAuraDummy AT APPLY & REMOVE, SPELLFAMILY_DRUID (switch (GetId()))',
            'dispatch': [
                '            AuraDummyApplyRemoveContext ctx(this, target, apply);',
                '            if (SpellHandlerRegistry::Game().Dispatch<AuraDummyDruidSite>(GetId(), ctx).IsReturn())',
                '            {',
                '                return;',
                '            }'],
            'open': ['            switch (GetId())', '            {'],
            'close': ['            }'],
            'label_indent': 16,
            'braced': True,
            'table': 'druid',
            'traits': 'AuraDummyDruidSite',
            'context': 'AuraDummyApplyRemoveContext',
            'live_outs': ['target', 'apply'],
            'in_scope': ['Real', 'classOptions'],
            'members_of': ('src/game/WorldHandlers/SpellAuras.h', 'Aura'),
            'substitutions': [('ctx.target', 'target'), ('ctx.apply', 'apply'),
                              ('ctx.aura->GetModifier()->', 'm_modifier.'), ('ctx.aura', 'this')] + VOID_SUBSTITUTIONS,
            'labels': {
                52610: '                case 52610:                                 // Savage Roar',
                61336: '                case 61336:                                 // Survival Instincts'},
        }, {
            'name': 'HandleAuraDummy AT APPLY & REMOVE, SPELLFAMILY_DRUID, Improved Moonkin Form (switch (GetId()))',
            'dispatch': [
                '                AuraDummyImprovedMoonkinContext imfCtx(this, spell_id);',
                '                if (SpellHandlerRegistry::Game()'
                '.Dispatch<AuraDummyImprovedMoonkinSite>(GetId(), imfCtx).IsReturn())',
                '                {',
                '                    return;',
                '                }'],
            'open': ['                switch (GetId())', '                {'],
            'close': ['                }'],
            'label_indent': 20,
            'braced': False,
            'table': 'improvedMoonkin',
            'traits': 'AuraDummyImprovedMoonkinSite',
            'default': '                    default:',
            'context': 'AuraDummyImprovedMoonkinContext',
            'live_outs': ['spell_id'],
            'in_scope': ['apply', 'Real', 'target', 'classOptions'],
            'members_of': ('src/game/WorldHandlers/SpellAuras.h', 'Aura'),
            'substitutions': [('ctx.spell_id', 'spell_id'), ('ctx.aura->GetId()', 'GetId()'),
                              ('ctx.aura', 'this')] + VOID_SUBSTITUTIONS,
            'labels': {
                48384: '                    case 48384: {body}    // Rank 1',
                48395: '                    case 48395: {body}    // Rank 2',
                48396: '                    case 48396: {body}    // Rank 3'},
        }],
    },
}


class Failure(Exception):
    pass


def table_rows(lines, table):
    """[(spell id, function)] of `static <Row type> const <table>[] = { { id, &Function }, ... };`."""
    starts = [i for i, l in enumerate(lines)
              if re.fullmatch(r'\s*static [\w:]+(<[\w:]+>)? const %s\[\] =' % re.escape(table), l)]
    if len(starts) != 1:
        raise Failure('registration table %s found %d times' % (table, len(starts)))
    rows = []
    i = starts[0] + 1
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


def handler_body(lines, function, context):
    """The lines between the braces of `static SpellHandlerOutcome<...> function(context& ctx)` (or
    `/*ctx*/`, a body that reads no context)."""
    head = re.compile(r'static SpellHandlerOutcome<[\w:]+> %s\(%s& (ctx|/\*ctx\*/)\)' % (re.escape(function),
                                                                                        re.escape(context)))
    starts = [i for i, l in enumerate(lines) if head.fullmatch(l)]
    if len(starts) != 1:
        raise Failure('handler %s(%s& ctx) found %d times' % (function, context, len(starts)))
    i = starts[0]
    if lines[i + 1] != '{':
        raise Failure('handler %s: no "{" on the line after its head' % function)
    j = next((k for k in range(i + 2, len(lines)) if lines[k] == '}'), None)
    if j is None:
        raise Failure('handler %s: no closing "}" in column 0' % function)
    return lines[i + 2:j]


def class_members(header, cls):
    """The names class `cls` declares at class scope in `header` (its text): member functions and
    data members, not its constructors, friends, nested types or what inline bodies call."""
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
        text = re.split(r'=|\[|:(?!:)', text)[0]
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


def check_body(function, body, site, members=()):
    """What the paste-back cannot see: a bare live-out, a bare member of the site's class, and a
    Continue inside a loop or switch."""
    text = blank('\n'.join(body))
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


def paste(site, function, body, label_lines):
    """The switch lines one registered function stood for: its label line(s) and its body."""
    restored = []
    for line in body:
        for a, b in site['substitutions']:
            line = line.replace(a, b)
        restored.append(line)
    if len(label_lines) == 1 and '{body}' in label_lines[0]:
        return [label_lines[0].replace('{body}', ' '.join(l.strip() for l in restored))]
    if any('{body}' in l for l in label_lines):
        raise Failure('%s: %s is a one-line case sharing its body with another label' % (site['name'], function))
    indent = ' ' * site['label_indent']
    restored = [indent + line if line else line for line in restored]
    if site['braced']:
        return label_lines + [indent + '{'] + restored + [indent + '}']
    return label_lines + restored


def put_back(rest, at, n, site, pieces):
    """A partly moved switch: `rest` with the dispatch at `at` (n lines) dropped and each run of moved
    rows pasted back into the switch that still stands directly after it, before the run's anchor."""
    first = at + n + len(site['open'])
    if rest[at + n:first] != site['open']:
        raise Failure('%s: the dispatch is not directly followed by the switch that still stands' % site['name'])
    end = next((k for k in range(first, len(rest)) if rest[k] == site['close'][0]), None)
    if end is None:
        raise Failure('%s: the switch that still stands has no closing line' % site['name'])
    anchors = site['residual']
    runs = []
    for first_id, lines in pieces:
        if first_id in anchors:
            runs.append((anchors[first_id], []))
        elif not runs:
            raise Failure('%s: the first row, %d, starts no run (it has no anchor)' % (site['name'], first_id))
        runs[-1][1].extend(lines)
    if len(runs) != len(anchors):
        raise Failure('%s: %d runs of rows for %d anchors' % (site['name'], len(runs), len(anchors)))
    positions = []
    for anchor, _ in runs:
        found = [end] if anchor == site['close'][0] else [k for k in range(first, end) if rest[k] == anchor]
        if len(found) != 1:
            raise Failure('%s: the anchor %r found %d times in the switch that still stands'
                          % (site['name'], anchor, len(found)))
        positions.append(found[0])
    if positions != sorted(set(positions)):
        raise Failure('%s: the runs of rows are not in the order of their anchors in the switch' % site['name'])
    for (_, lines), pos in reversed(list(zip(runs, positions))):
        rest[pos:pos] = lines
    del rest[at:at + n]


def rebuild(text, spec, headers, handler_text=None, strict=True):
    """`text` with every site's dispatch replaced by its switch, the added lines and the appended
    handler block dropped; the number of bodies pasted back, of labels, the names of the sites
    rebuilt, and the handler lines read. The handlers are `handler_text` (the handler file) when
    given, else the block appended to `text` from the tail marker. `strict` (the working tree)
    requires every site; otherwise (the base) a site whose dispatch is absent is left as it stands,
    and a file with neither handler source is returned as is. `headers` maps a site's members_of
    header path to its text."""
    new = text.split('\n')
    marker = [i for i, l in enumerate(new) if l == spec['tail_marker']]
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
        n = len(site['dispatch'])
        at = [i for i in range(len(rest) - n + 1) if rest[i:i + n] == site['dispatch']]
        if not strict and not at:
            continue
        if len(at) != 1:
            raise Failure('%s: the dispatch found %d times' % (site['name'], len(at)))
        if not site.get('traits'):
            raise Failure('%s: no traits named, so a default registered for it could not be seen' % site['name'])
        if 'Dispatch<%s>' % site['traits'] not in ''.join(site['dispatch']):
            raise Failure('%s: its traits %s do not appear as Dispatch<%s> in its dispatch lines'
                          % (site['name'], site['traits'], site['traits']))
        rows = table_rows(handlers, site['table'])
        ids = [r[0] for r in rows]
        if sorted(ids) != sorted(site['labels']) or len(set(ids)) != len(ids):
            raise Failure('%s: table rows %s, labels %s' % (site['name'], ids, sorted(site['labels'])))
        header, cls = site['members_of']
        members = class_members(headers[header], cls)
        pieces = []
        i = 0
        while i < len(rows):
            function = rows[i][1]
            j = i
            while j < len(rows) and rows[j][1] == function:
                j += 1
            if function in [r[1] for r in rows[:i]]:
                raise Failure('%s: %s registered in two runs of rows (its labels were not together)'
                              % (site['name'], function))
            body = handler_body(handlers, function, site['context'])
            check_body(function, body, site, members)
            pieces.append((rows[i][0], paste(site, function, body, [site['labels'][r[0]] for r in rows[i:j]])))
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
            body = handler_body(handlers, function, site['context'])
            check_body(function, body, site, members)
            switch += paste(site, function, body, [site['default']])
            pasted += 1
        elif any(re.fullmatch(r'\s*registry\.RegisterDefault<%s>\(.*' % re.escape(site['traits']), l)
                 for l in handlers):
            raise Failure('%s: a default registered for a site whose switch had none' % site['name'])
        if 'residual' in site:
            put_back(rest, at[0], n, site, pieces)
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


def first_difference(a, b):
    al, bl = a.split('\n'), b.split('\n')
    for n, (x, y) in enumerate(zip(al, bl), 1):
        if x != y:
            return 'line %d: base %r, rebuilt %r' % (n, x, y)
    return 'lengths differ: base %d lines, rebuilt %d' % (len(al), len(bl))


def verify(rel, old_text, new_text, spec, headers, out=print, old_handlers=None, new_handlers=None):
    """`old_handlers` and `new_handlers`: the handler file's text in that version, None where it has none."""
    try:
        rebuilt, pasted, labels, sites, handlers = rebuild(new_text, spec, headers, new_handlers)
        base, base_pasted, _, base_sites, _ = rebuild(old_text, spec, headers, old_handlers, strict=False)
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
        '(the base had %d of the sites moved: %d bodies pasted back there)' % (
            rel, pasted, len(defined), labels, len(sites), len(base_sites), base_pasted))
    return 0, pasted


def check(root, base, out=print):
    rc = 0
    for rel, spec in SITES.items():
        path = os.path.join(root, *rel.split('/'))
        with open(path, encoding='utf-8', newline='') as fh:
            new_text = fh.read()
        try:
            old_text = subprocess.run(['git', '-C', root, 'show', '%s:%s' % (base, rel)], capture_output=True,
                                      check=True).stdout.decode('utf-8')
        except (OSError, subprocess.CalledProcessError) as e:
            out('%s: FAILED: cannot read it at %s from git: %s' % (rel, base, e))
            rc = 1
            continue
        new_handlers = None
        handler_path = os.path.join(root, *spec['handlers'].split('/'))
        if os.path.isfile(handler_path):
            with open(handler_path, encoding='utf-8', newline='') as fh:
                new_handlers = fh.read()
        try:
            listed = subprocess.run(['git', '-C', root, 'ls-tree', '--name-only', base, '--', spec['handlers']],
                                    capture_output=True, check=True).stdout.decode('utf-8').split()
            old_handlers = None
            if listed:
                old_handlers = subprocess.run(['git', '-C', root, 'show', '%s:%s' % (base, spec['handlers'])],
                                              capture_output=True, check=True).stdout.decode('utf-8')
        except (OSError, subprocess.CalledProcessError) as e:
            out('%s: FAILED: cannot read %s at %s from git: %s' % (rel, spec['handlers'], base, e))
            rc = 1
            continue
        headers = {}
        for site in spec['sites']:
            header = site['members_of'][0]
            with open(os.path.join(root, *header.split('/')), encoding='utf-8', newline='') as fh:
                headers[header] = fh.read()
        got, _ = verify(rel, old_text, new_text, spec, headers, out, old_handlers, new_handlers)
        rc |= got
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
    static Row const rows[] =
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
# second site's move is checked against, whose own dispatch is pasted back the same way.
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
        'braced': True,
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
        'braced': False,
        'table': 'ranks',
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
        'residual': {2: '        case 3:                                 // Three', 4: '    }'},
        'label_indent': 8,
        'braced': True,
        'table': 'removed',
        'traits': 'RemoveSite',
        'context': 'RemoveContext',
        'live_outs': ['target'],
        'in_scope': ['apply'],
        'members_of': ('Thing.h', 'Thing'),
        'substitutions': [('ctx.target', 'target'), ('ctx.aura', 'this')] + VOID_SUBSTITUTIONS,
        'labels': {2: '        case 2:                                 // Two',
                   4: '        case 4:                                 // Four',
                   5: '        case 5:                                 // Five',
                   6: '        case 6:                                 // Six'},
    }],
}


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
        bool m_positive : 1;
        int m_table[4];
        enum { KIND_A, KIND_B };
    private:
        uint64 m_casterGuid = 0;
};
"""}


def self_test():
    failures = []
    got = sorted(class_members(SELF_HEADERS['Thing.h'], 'Thing'))
    want = ['GetCaster', 'GetId', 'Handle', 'IsPositive', 'm_casterGuid', 'm_modifier', 'm_positive', 'm_table']
    print('self-test: %-66s %s' % ('the class-scope member names are read', 'PASS' if got == want else 'FAIL'))
    if got != want:
        failures.append('class_members: got %r, expected %r' % (got, want))

    def run(label, want_rc, needle='', swap=None, old_text=SELF_OLD, spec=SELF_SPEC, sites=SELF_SITES,
            handlers=SELF_HANDLERS, old_handlers=None):
        """`swap` (a, b) replaces a by b in the sites' file and the handler file; a must be in one."""
        if swap:
            a, b = swap
            if a not in sites + (handlers or ''):
                failures.append('%s: the mutation %r matches nothing' % (label, a))
                print('self-test: %-66s %s' % (label, 'FAIL'))
                return
            sites = sites.replace(a, b)
            handlers = handlers.replace(a, b) if handlers is not None else None
        got = []
        rc, _ = verify('fixture', old_text, sites, spec, SELF_HEADERS, got.append, old_handlers, handlers)
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
    run('a run pasted back before the wrong label fails', 1, 'DIFFERS',
        **dict(part, spec=part_site(residual={2: '        case 1:                                 // One',
                                              4: '    }'})))
    run('a line between the dispatch and the switch that still stands fails', 1,
        'the dispatch is not directly followed by the switch that still stands',
        swap=('        return;\n    }\n    switch', '        return;\n    }\n    Log();\n    switch'), **part)
    run('a run\'s anchor gone from the switch that still stands fails', 1,
        'the anchor \'        case 3:                                 // Three\' found 0 times',
        swap=('        case 3:                                 // Three\n', '        case 7:\n'), **part)
    run('a moved label still standing in the switch fails', 1, 'DIFFERS',
        swap=('        case 3:                                 // Three\n',
              '        case 2:                                 // Two\n        {\n            target->Drop(2);\n'
              '            return;\n        }\n        case 3:                                 // Three\n'), **part)
    run('runs registered out of the order of their anchors fail', 1,
        'the runs of rows are not in the order of their anchors in the switch',
        swap=('        { 2, &Two },\n        { 4, &Four },\n        { 5, &Four },\n        { 6, &Six },',
              '        { 4, &Four },\n        { 5, &Four },\n        { 6, &Six },\n        { 2, &Two },'), **part)
    run('an anchor keyed on a row that starts no run fails', 1, '1 runs of rows for 2 anchors',
        **dict(part, spec=part_site(residual={2: '        case 3:                                 // Three',
                                              5: '    }'})))
    run('a default at a partly moved site fails (it stays in the switch that still stands)', 1,
        'a partly moved switch keeps its default: in the switch that still stands',
        **dict(part, spec=part_site(default='        default:')))
    for f in failures:
        print('SELF-TEST FAILED: ' + f)
    print('self-test: %s (%d failure(s))' % ('PASS' if not failures else 'FAIL', len(failures)))
    return 1 if failures else 0


def main(argv):
    ap = argparse.ArgumentParser(description='The spell handler registry\'s verbatim proof (decoupling D11).')
    ap.add_argument('--root', default=os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..')))
    ap.add_argument('--base', default=BASE)
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument('--check', action='store_true')
    g.add_argument('--self-test', action='store_true')
    args = ap.parse_args(argv[1:])
    if args.self_test:
        return self_test()
    return check(os.path.abspath(args.root), args.base)


if __name__ == '__main__':
    sys.exit(main(sys.argv))
