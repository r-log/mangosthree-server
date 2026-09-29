#!/usr/bin/env python3
"""verbatim.py [--root <repo root>] [--base <ref>] --check | --self-test

The spell handler registry's verbatim proof (decoupling D11, design/2026-09-28-unit-reopening.md
3(b)): every case body that moved out of a per-spell-ID switch into a registered handler is pasted
back at its label, and the file must come back byte for byte as it was at BASE.

For each file in SITES, --check:
  1. reads the file at BASE (`git show <base>:<file>`) and in the working tree;
  2. drops the lines the moves added (ADDED: the handlers header's include) and the handler block
     the moves appended (from TAIL_MARKER to the end of the file);
  3. at each site, replaces the dispatch (the site's exact DISPATCH lines, found exactly once)
     with the switch it stood for: the switch's opening lines, then for each row of the site's
     registration table, in table order, the label line (LABELS) and -- once per run of rows that
     register the same function -- that function's body, reverse-substituted (SUBSTITUTIONS: the
     context accessors and the outcome) and re-indented to the label's body; then, if the site has
     a registered `default:` (DEFAULT, found through `registry.RegisterDefault<TRAITS>(&F);`), its
     label and F's body the same way. A label holding `{body}` is a one-line case
     (`case 1: x = 1; break;    // note`): the body's lines are joined there with one space;
  4. does the same to the file at BASE, for the sites a PR before this one moved (their dispatch
     is in the base; a site whose dispatch is not in the base is this PR's and must be a switch
     there), so BASE may be any commit from before the first move to the parent of this PR;
  5. compares the two, byte for byte, and names the first difference.
Because the bodies are pasted from the functions the table registers, a row pointing at the wrong
function, a lost row or default, a changed line, or a body moved in the wrong order all fail; a
body a PR before this one moved is proven again, against its own base's copy.

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

# The tree the moved bodies are checked against: the parent of the PR that moved the latest site
# (D11 PR 3b: PR 3's tip). `--base afdabc428` (master before the first move, D11 PR 3) proves every
# site against the switches as they were.
BASE = '9fcec1b2a'

VOID_SUBSTITUTIONS = [('return SpellHandlerOutcome<void>::Return();', 'return;'),
                      ('return SpellHandlerOutcome<void>::Continue();', 'break;')]

# One entry per file; each file lists its sites in the order they stand in it.
SITES = {
    'src/game/WorldHandlers/SpellAuraDummy.cpp': {
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
                '                            if (SpellHandlerRegistry::Game().Dispatch<AuraDummyUnrelentingAssaultSite>(',
                '                                    (*itr)->GetSpellProto()->ID, assaultCtx).IsReturn())',
                '                            {',
                '                                return;',
                '                            }'],
            'open': ['                            switch ((*itr)->GetSpellProto()->ID)', '                            {'],
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
                '                if (SpellHandlerRegistry::Game().Dispatch<AuraDummyImprovedMoonkinSite>(GetId(), imfCtx).IsReturn())',
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
    """F of the one `registry.RegisterDefault<traits>(&F);` line."""
    found = [m.group(1) for m in (re.fullmatch(r'\s*registry\.RegisterDefault<%s>\(&(\w+)\);' % re.escape(traits), l)
                                  for l in lines) if m]
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


def rebuild(text, spec, headers, strict=True):
    """`text` with every site's dispatch replaced by its switch, the added lines and the appended
    handler block dropped; the number of bodies pasted back, of labels, and the names of the sites
    rebuilt. `strict` (the working tree) requires every site; otherwise (the base) a site whose
    dispatch is absent is left as it stands, and a file with no handler block is returned as is.
    `headers` maps a site's members_of header path to its text."""
    new = text.split('\n')
    marker = [i for i, l in enumerate(new) if l == spec['tail_marker']]
    if not strict and not marker:
        return text, 0, 0, []
    if len(marker) != 1:
        raise Failure('tail marker found %d times' % len(marker))
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
    pasted, labels, rebuilt = 0, 0, []
    for site in spec['sites']:
        n = len(site['dispatch'])
        at = [i for i in range(len(rest) - n + 1) if rest[i:i + n] == site['dispatch']]
        if not strict and not at:
            continue
        if len(at) != 1:
            raise Failure('%s: the dispatch found %d times' % (site['name'], len(at)))
        rows = table_rows(handlers, site['table'])
        ids = [r[0] for r in rows]
        if sorted(ids) != sorted(site['labels']) or len(set(ids)) != len(ids):
            raise Failure('%s: table rows %s, labels %s' % (site['name'], ids, sorted(site['labels'])))
        header, cls = site['members_of']
        members = class_members(headers[header], cls)
        switch = list(site['open'])
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
            switch += paste(site, function, body, [site['labels'][r[0]] for r in rows[i:j]])
            pasted += 1
            labels += j - i
            i = j
        if 'default' in site:
            function = default_function(handlers, site['traits'])
            if function in [r[1] for r in rows]:
                raise Failure('%s: the default %s is also a labelled row' % (site['name'], function))
            body = handler_body(handlers, function, site['context'])
            check_body(function, body, site, members)
            switch += paste(site, function, body, [site['default']])
            pasted += 1
        elif 'traits' in site and any(re.fullmatch(r'\s*registry\.RegisterDefault<%s>\(.*' % re.escape(site['traits']),
                                                   l) for l in handlers):
            raise Failure('%s: a default registered for a site whose switch had none' % site['name'])
        switch += site['close']
        rest[at[0]:at[0] + n] = switch
        rebuilt.append(site['name'])
    return '\n'.join(rest), pasted, labels, rebuilt


def first_difference(a, b):
    al, bl = a.split('\n'), b.split('\n')
    for n, (x, y) in enumerate(zip(al, bl), 1):
        if x != y:
            return 'line %d: base %r, rebuilt %r' % (n, x, y)
    return 'lengths differ: base %d lines, rebuilt %d' % (len(al), len(bl))


def verify(rel, old_text, new_text, spec, headers, out=print):
    try:
        rebuilt, pasted, labels, sites = rebuild(new_text, spec, headers)
        base, base_pasted, _, base_sites = rebuild(old_text, spec, headers, strict=False)
    except Failure as e:
        out('%s: FAILED: %s' % (rel, e))
        return 1, 0
    lines = new_text.split('\n')
    defined = [m.group(1) for m in (re.match(r'static SpellHandlerOutcome<[\w:]+> (\w+)\(', l)
                                    for l in lines[lines.index(spec['tail_marker']):]) if m]
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
        headers = {}
        for site in spec['sites']:
            header = site['members_of'][0]
            with open(os.path.join(root, *header.split('/')), encoding='utf-8', newline='') as fh:
                headers[header] = fh.read()
        got, _ = verify(rel, old_text, new_text, spec, headers, out)
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

SELF_NEW = '''#include "A.h"
#include "Handlers.h"

void Thing::Handle(bool apply)
{
    Unit* target = GetTarget();
    SelfContext handlerContext(this, target);
    if (Dispatch(handlerContext).IsReturn())
    {
        return;
    }
    uint32 rank;
    RankContext rankContext(this, rank);
    if (Dispatch(rankContext).IsReturn())
    {
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

# The file after an earlier PR moved the first site only: a base the second site's PR is checked
# against, whose own dispatch is pasted back the same way.
SELF_MID = '''#include "A.h"
#include "Handlers.h"

void Thing::Handle(bool apply)
{
    Unit* target = GetTarget();
    SelfContext handlerContext(this, target);
    if (Dispatch(handlerContext).IsReturn())
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
    'added': ['#include "Handlers.h"'],
    'tail_marker': '// Handlers:',
    'sites': [{
        'name': 'fixture',
        'dispatch': ['    SelfContext handlerContext(this, target);', '    if (Dispatch(handlerContext).IsReturn())',
                     '    {', '        return;', '    }'],
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
        'dispatch': ['    RankContext rankContext(this, rank);', '    if (Dispatch(rankContext).IsReturn())',
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

    def run(label, new_text, want_rc, needle='', old_text=SELF_OLD):
        got = []
        rc, _ = verify('fixture', old_text, new_text, SELF_SPEC, SELF_HEADERS, out=got.append)
        text = '\n'.join(got)
        ok = rc == want_rc and needle in text
        print('self-test: %-66s %s' % (label, 'PASS' if ok else 'FAIL'))
        if not ok:
            failures.append('%s: rc %d (want %d)\n%s' % (label, rc, want_rc, text))

    run('the moved bodies paste back byte for byte (5 labels, 2 sites)', SELF_NEW, 0,
        'IDENTICAL to the base, byte for byte, with 5/5 bodies pasted back at their 5 labels in 2 sites')
    run('a changed body line fails', SELF_NEW.replace('if (i == 1)', 'if (i == 2)'), 1, 'DIFFERS')
    run('two rows swapped fail', SELF_NEW.replace('        { 2, &One },\n        { 3, &Three },',
                                                  '        { 3, &Three },\n        { 2, &One },'), 1,
        'registered in two runs of rows')
    run('a row pointing at the wrong body fails', SELF_NEW.replace('{ 2, &One }', '{ 2, &Three }'), 1, 'DIFFERS')
    run('a lost row fails', SELF_NEW.replace('        { 2, &One },\n', ''), 1, 'table rows [1, 3]')
    run('a bare live-out in a body fails', SELF_NEW.replace('ctx.target->Cast', 'target->Cast'), 1,
        'live-out "target" used without the context')
    run('a name in scope but not in the context fails', SELF_NEW.replace('ctx.rank = 2;', 'ctx.rank = apply;'), 1,
        '"apply", a name in scope at the site and not in its context')
    run('a bare member of the site\'s class in a body fails',
        SELF_NEW.replace('ctx.target->Cast(ctx.aura);', 'ctx.target->Cast(GetCaster());'), 1,
        '"GetCaster", a member of Thing, used bare')
    run('a bare data member (m_modifier.) in a body fails',
        SELF_NEW.replace('ctx.rank = 2;', 'ctx.rank = m_modifier.m_amount;'), 1,
        '"m_modifier", a member of Thing, used bare')
    run('an implicit GetId() the move missed fails', SELF_NEW.replace('Log(ctx.aura->GetId());', 'Log(GetId());'), 1,
        '"GetId", a member of Thing, used bare')
    run('a member reached through the context passes the member check',
        SELF_NEW.replace('ctx.target->Cast(ctx.aura);', 'ctx.target->Cast(ctx.aura->GetCaster());'), 1, 'DIFFERS')
    run('a Continue inside a loop fails', SELF_NEW.replace('if (i == 1)\n            break;',
                                                           'if (i == 1)\n            return SpellHandlerOutcome<void>::Continue();'),
        1, 'a Continue inside a loop or switch')
    run('a lost dispatch fails', SELF_NEW.replace('    if (Dispatch(handlerContext).IsReturn())\n', ''), 1,
        'the dispatch found 0 times')
    run('Return taken for Continue fails', SELF_NEW.replace('return SpellHandlerOutcome<void>::Continue();',
                                                            'return SpellHandlerOutcome<void>::Return();'), 1,
        'DIFFERS')
    run('a one-line case\'s body changed fails', SELF_NEW.replace('ctx.rank = 1;', 'ctx.rank = 3;'), 1,
        'line 27: base \'        case 7: rank = 1; break;    // Rank 1\'')
    run('a lost default registration fails',
        SELF_NEW.replace('    registry.RegisterDefault<RankSite>(&RankDefault);\n', ''), 1,
        'the default of RankSite registered 0 times')
    run('a default pasted from the wrong body fails',
        SELF_NEW.replace('RegisterDefault<RankSite>(&RankDefault)', 'RegisterDefault<RankSite>(&Rank8)'), 1,
        'the default Rank8 is also a labelled row')
    run('a default registered where the switch had none fails',
        SELF_NEW.replace('    registry.RegisterDefault<RankSite>', '    registry.RegisterDefault<SelfSite>(&Three);\n'
                                                                   '    registry.RegisterDefault<RankSite>'), 1,
        'a default registered for a site whose switch had none')
    run('a handler defined but not registered fails',
        SELF_NEW.replace('void Register(Registry& registry)',
                         'static SpellHandlerOutcome<void> Orphan(RankContext& ctx)\n{\n'
                         '    return SpellHandlerOutcome<void>::Continue();\n}\n\nvoid Register(Registry& registry)'), 1,
        '6 handlers defined, 5 registered and pasted back')
    run('against a base that had the first site moved: passes', SELF_NEW, 0,
        'with 5/5 bodies pasted back at their 5 labels in 2 sites (the base had 1 of the sites moved: '
        '2 bodies pasted back there)', old_text=SELF_MID)
    run('the base\'s own moved body is proven again: a change there fails', SELF_NEW, 1, 'DIFFERS',
        old_text=SELF_MID.replace('ctx.target->Cast(ctx.aura);', 'ctx.target->Cast(NULL);'))
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
