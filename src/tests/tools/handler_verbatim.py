#!/usr/bin/env python3
"""handler_verbatim.py [--root <repo root>] [--base <ref>] --check | --self-test

The handler classes' verbatim proof: a function moved whole from a `WorldSession` member into a handler
class's static (src/game/session/handlers/) is pasted back at its old place, and the old file must come
back byte for byte as it was at the entry's base; an old file still in the working tree (a residue,
holding the functions that stay) must be the old file less the moved functions.

MOVES and RESIDUES live in handler_moves.py beside this file, which a move edits; this file holds neither,
and never changes them, and split_gate.py (whose docstring holds the rules) refuses to run it when it binds
or changes one, when it runs with values other than the data file's, or when the data file holds anything
but literal values.

MOVES holds one entry per moved function:
  base, base_file, base_header  the commit the move is proven against (the parent of the change that
      moved it: each entry names its own, so moves from one file in several changes are proven apart),
      the file that held the function there and its definition line, `<type> WorldSession::<Name>(<p>)`;
  new_file, new_header  the file that holds it in the working tree (under src/game/session/handlers/, so
      handler_classes.py reads it) and its definition line, `<type> <Class>::<Name2>(WorldSession& session)`
      or `<type> <Class>::<Name2>(WorldSession& session, <p>)`, <Class> any class but WorldSession: any
      name, the base's return type and the base's parameters, text for text, but that a commented-out
      parameter may be spelt `/*name*/` or `/* name */` on either side (a named parameter is not a
      commented-out one). A handler, a sender and a helper returning a value are all this shape;
  substitutions  (new text, base text) pairs, each matching code at least once and only where no name,
      `.`, `->` or `::` runs into it (a sender's call `SendAttackStop(session, ` read back as `SendAttackStop(`);
  edits  (new line, base line) or (new line, base line, count) entries: a line changed beyond the
      substitutions, read back whole; it must be found exactly count times in the function (1 when not
      given; fewer or more fails, naming which), and every one of them is read back; a new line listed
      twice fails.
RESIDUES holds one entry per change that kept an old file: base, base_file and removed, the include lines
that change removed from it (`#include ...`, exact text), the only lines a residue may lose beyond the
moved functions.

For each entry, --check:
  1. reads the function in each version: the comment lines directly above its definition line (`/*`,
     ` *`, `//`: the doc comment moves with it), that line, and its body to the first `}` at column 0,
     each definition line found exactly once; the comment lines read above it must hold no code when
     read alone (a block comment opening above them, or a directive after a `*/`, fails);
  2. fails on a body line naming a member of `WorldSession` bare (an implicit `this->` the move missed:
     the static would compile against a free function or global of that name); the names are every
     member function and data member at class scope in the working tree's `WorldSession.h`, read by
     verbatim.py's MEMBERS_OF reader (class_members). A name followed by `::` is a namespace or class
     qualifier (`std::string`, `Motion::Reason`), not a member use, and passes; the same name alone fails;
  3. reverses the move: an edit's line becomes its base line; on every other line, in code only
     (comments and literals stay), `session.` not after a name, `.`, `->` or `::` is dropped and the
     substitutions are read back; the definition line becomes base_header. A base function that names
     `session` in code fails (a local of that name would make a `session.` the reversal drops mean
     something else), and so does a span, base or new, whose braces do not balance (a `}` at column
     0 inside a body would end the span early and hide the lines after it);
  4. pastes that at the function's place in base_file at base and compares the file byte for byte;
  5. fails when the working tree's base_file still holds base_header (the move deletes it).
For each base_file still in the working tree, and for each base its entries name, --check then rebuilds
the residue from base_file at that base: every entry's function that stands there (whatever its own
base, so the functions moved later are cut too and the ones moved before are not looked for) is cut,
its doc comment and the blank lines after its `}` with it; the include lines its RESIDUES entries at that
base list are removed, each found exactly once, and those listed at another base wherever they still
stand; each removal must be an include directive in code (not in a literal), neither ending in nor
following a backslash, so no removal splices two lines or un-splices one. The result must equal the
working tree's file byte for byte: a line changed outside the cut spans fails with its line, an include
removed and not listed fails by name, a listed removal not found, found twice or not an include line
fails, and a moved function the residue still defines fails. A deleted base_file has no
residue to prove, but at each base its entries name every `WorldSession::` definition it held must be
one an entry moves (otherwise deleting the file would skip the residue proof); a RESIDUES entry for a
deleted file, or at a base no entry of the file names, fails.
A definition line of another shape and an edit or substitution that matches nothing fail by name. The
header's `static` is handler_classes.py's to check (a handler class has static member functions only).

python src/tests/tools/handler_verbatim.py --check       # every entry against its base, reading git
python src/tests/tools/handler_verbatim.py --self-test   # fixtures only, no git
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

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from case_labels import blank  # noqa: E402
from verbatim import Failure, class_members, first_difference  # noqa: E402
import split_gate  # noqa: E402
try:
    from handler_moves import MOVES, RESIDUES  # noqa: E402
except ImportError as e:
    sys.exit(split_gate.unloaded(__file__, 'handler_moves', e))

SESSION_HEADER = 'src/game/Server/WorldSession.h'
HANDLERS_DIR = 'src/game/session/handlers'

# The names handler_moves.py assigns; split_gate.py holds the split.
DATA_NAMES = ('MOVES', 'RESIDUES')

NEW_HEADER = re.compile(r'(?P<type>\S.*?) (?P<cls>\w+)::\w+\(WorldSession& session(?:, (?P<params>.+))?\)$')
BASE_HEADER = re.compile(r'(?P<type>\S.*?) WorldSession::\w+\((?P<params>.*)\)$')
PARAM_COMMENT = re.compile(r'/\*\s*(\w+)\s*\*/')
INCLUDE = re.compile(r'#\s*include\s*(<[^<>]+>|"[^"]+")\s*(//.*)?$')
SESSION_DOT = re.compile(r'(?<![\w.>:])session\.')
COMMENT = re.compile(r'\s*(/\*|\*|//)')


def function_span(lines, header, where):
    """(first, at, end): the doc comment's first line, the definition line, the line after the `}`."""
    found = [i for i, line in enumerate(lines) if line.rstrip('\r') == header]
    if len(found) != 1:
        raise Failure('%s: the definition line found %d times: %r' % (where, len(found), header))
    at = found[0]
    first = at
    while first > 0 and COMMENT.match(lines[first - 1]):
        first -= 1
    if blank('\n'.join(lines[first:at])).strip():
        raise Failure('%s: the comment lines above %r hold code or open above them' % (where, header))
    end = at + 1
    while end < len(lines) and lines[end].rstrip('\r') != '}':
        end += 1
    if end == len(lines):
        raise Failure('%s: no `}` at column 0 closes %r' % (where, header))
    return first, at, end + 1


def check_members(body, members):
    """A member of WorldSession named bare in a body line (comments and literals blanked)."""
    if not members:
        return
    bare = re.compile(r'(?<![\w.>:~])(%s)\b(?!\s*::)' % '|'.join(re.escape(m) for m in sorted(members)))
    for n, line in enumerate(blank('\n'.join(body)).split('\n'), 1):
        m = bare.search(line)
        if m:
            raise Failure('body line %d: "%s", a member of WorldSession, used bare (an implicit this-> the '
                          'move missed): %r' % (n, m.group(1), body[n - 1]))


def check_shape(new_header, base_header):
    """The new definition line is the base's, `WorldSession::` replaced by a class and the session put first."""
    new, base = NEW_HEADER.match(new_header), BASE_HEADER.match(base_header)
    if not new:
        raise Failure('the definition line is not `<type> <Class>::<Name>(WorldSession& session[, ...])`')
    if new.group('cls') == 'WorldSession':
        raise Failure('the definition line names WorldSession as its class: a handler class is another class')
    if not base:
        raise Failure('the base definition line is not a WorldSession member\'s: %r' % base_header)
    if new.group('type') != base.group('type'):
        raise Failure('the definition line returns %r, the base %r' % (new.group('type'), base.group('type')))
    params = [PARAM_COMMENT.sub(r'/*\1*/', x or '') for x in (new.group('params'), base.group('params'))]
    if params[0] != params[1]:
        raise Failure('the definition line takes (%s) after the session, the base (%s)' % tuple(params))


def read_edits(entry):
    """{new line: base line} and {new line: how many identical lines it covers} of an entry's edits."""
    edits, counts = {}, {}
    for e in entry.get('edits', []):
        if e[0] in edits:
            raise Failure('one new line is listed as two edits')
        count = e[2] if len(e) > 2 else 1
        if not isinstance(count, int) or count < 1:
            raise Failure('the edit %r covers %r lines: a count is 1 or more' % (e[0], count))
        edits[e[0]], counts[e[0]] = e[1], count
    return edits, counts


def verify(entry, base_text, tree_base_text, new_text, members, out=print):
    """0 when the entry's function pastes back byte for byte; 1 with the reason printed."""
    name = '%s %s' % (entry['new_file'], entry['new_header'])
    try:
        check_shape(entry['new_header'], entry['base_header'])
        new_lines = new_text.split('\n')
        first, at, end = function_span(new_lines, entry['new_header'], entry['new_file'])
        check_members(new_lines[at + 1:end], members)
        span = new_lines[first:end]
        if not entry['new_file'].startswith(HANDLERS_DIR + '/'):
            raise Failure('the new file is not under %s, where handler_classes.py reads it' % HANDLERS_DIR)
        edits, counts = read_edits(entry)
        for line in edits:
            hits = sum(1 for x in span if x == line)
            if hits != counts[line]:
                raise Failure('the edit %r matches %d lines of the function, not the %d it covers: too %s found' % (
                    line, hits, counts[line], 'few' if hits < counts[line] else 'many'))
        rules = [(SESSION_DOT, '')] + [(re.compile(r'(?<![\w.>:])' + re.escape(a)), b)
                                       for a, b in entry.get('substitutions', [])]
        used = [0] * len(rules)
        pasted = []
        for k, (line, code) in enumerate(zip(span, blank('\n'.join(span)).split('\n'))):
            if k == at - first:
                line = entry['base_header'] + ('\r' if line.endswith('\r') else '')
            elif line in edits:
                line = edits[line]
            else:
                for r, (pattern, text) in enumerate(rules):
                    for m in reversed(list(pattern.finditer(code))):
                        used[r] += 1
                        line = line[:m.start()] + text + line[m.end():]
                        code = code[:m.start()] + text + code[m.end():]
            pasted.append(line)
        unused = [a for (a, _), n in zip(entry.get('substitutions', []), used[1:]) if n == 0]
        if unused:
            raise Failure('the substitution %r matches no code in the function' % unused[0])
        base_lines = base_text.split('\n')
        b_first, _, b_end = function_span(base_lines, entry['base_header'], '%s at %s' % (entry['base_file'],
                                                                                           entry['base']))
        b_code = blank('\n'.join(base_lines[b_first:b_end]))
        if re.search(r'\bsession\b', b_code):
            raise Failure('the base function names `session` in code: the reversal cannot tell it from the parameter')
        n_code = blank('\n'.join(span))
        if b_code.count('{') != b_code.count('}') or n_code.count('{') != n_code.count('}'):
            raise Failure('a `}` at column 0 closes the function before its braces balance')
        if tree_base_text is not None and any(x.rstrip('\r') == entry['base_header']
                                              for x in tree_base_text.split('\n')):
            raise Failure('%s still defines %r: the move deletes it' % (entry['base_file'], entry['base_header']))
    except Failure as e:
        out('%s: FAILED: %s' % (name, e))
        return 1
    rebuilt = '\n'.join(base_lines[:b_first] + pasted + base_lines[b_end:])
    if rebuilt != base_text:
        out('%s: DIFFERS from %s at %s: %s' % (name, entry['base_file'], entry['base'],
                                               first_difference(base_text, rebuilt)))
        return 1
    out('%s: IDENTICAL to %s at %s, byte for byte, with %d lines pasted back (%d edits)' % (
        name, entry['base_file'], entry['base'], len(pasted), sum(counts.values())))
    return 0


def verify_residue(base_file, base, base_text, tree_text, headers, removed, removed_elsewhere=(), out=print):
    """0 when the working tree's base_file is base_file at base with the moved functions cut and the listed
    includes removed, byte for byte; 1 with the reason printed."""
    name = '%s, the residue' % base_file
    tree = tree_text.split('\n')
    try:
        for header in headers:
            if any(x.rstrip('\r') == header for x in tree):
                raise Failure('it still defines %r: the move deletes it' % header)
        lines = base_text.split('\n')
        spans = []
        for header in headers:
            if any(x.rstrip('\r') == header for x in lines):
                first, _, end = function_span(lines, header, '%s at %s' % (base_file, base))
                while end < len(lines) and not lines[end].strip():
                    end += 1
                spans.append((first, end))
        for a, b in sorted(spans, reverse=True):
            del lines[a:b]
        for line in removed:
            if not INCLUDE.match(line):
                raise Failure('the listed removal %r is not an include line' % line)
        for line, own in [(x, True) for x in removed] + [(x, False) for x in removed_elsewhere]:
            hits = [i for i, x in enumerate(lines) if x.rstrip('\r') == line]
            if len(hits) > 1 or (own and not hits):
                raise Failure('the listed removal %r found %d times outside the moved functions at %s' % (
                    line, len(hits), base))
            if hits:
                code = blank('\n'.join(lines)).split('\n')
                i = hits[0]
                if (not code[i].lstrip().startswith('#') or lines[i].rstrip('\r').endswith('\\')
                        or (i and lines[i - 1].rstrip('\r').endswith('\\'))):
                    raise Failure('the listed removal %r is not an include directive standing alone' % line)
                del lines[hits[0]]
    except Failure as e:
        out('%s: FAILED: %s' % (name, e))
        return 1
    if lines != tree:
        k = next((i for i, (x, y) in enumerate(zip(lines, tree)) if x != y), min(len(lines), len(tree)))
        if k < len(lines) and INCLUDE.match(lines[k].rstrip('\r')) and lines[k + 1:k + 2] == tree[k:k + 1]:
            out('%s: FAILED: line %d, %r, is removed and not listed' % (name, k + 1, lines[k]))
        else:
            out('%s: DIFFERS from %s at %s with the moved functions cut: line %d: base %r, tree %r' % (
                name, base_file, base, k + 1, lines[k] if k < len(lines) else None, tree[k] if k < len(tree) else None))
        return 1
    out('%s: IDENTICAL to %s at %s, byte for byte, with %d moved functions cut and %d includes removed' % (
        name, base_file, base, len(spans), len(base_text.split('\n')) - len(lines) - sum(b - a for a, b in spans)))
    return 0


def read(root, rel):
    path = os.path.join(root, *rel.split('/'))
    if not os.path.isfile(path):
        return None
    with open(path, encoding='utf-8', newline='') as fh:
        return fh.read()


def git_show(root, ref, rel):
    return subprocess.run(['git', '-C', root, 'show', '%s:%s' % (ref, rel)],
                          capture_output=True, check=True).stdout.decode('utf-8')


def check(root, base=None, out=print, moves=None, residues=None):
    """--check: `moves` and `residues` replace MOVES and RESIDUES."""
    moves = MOVES if moves is None else moves
    residues_all = RESIDUES if residues is None else residues
    rc = 0
    members = class_members(read(root, SESSION_HEADER), 'WorldSession') if moves else set()
    for entry in moves:
        entry = dict(entry, base=base or entry['base'])
        try:
            base_text = git_show(root, entry['base'], entry['base_file'])
        except (OSError, subprocess.CalledProcessError) as e:
            out('%s: FAILED: cannot read it at %s from git: %s' % (entry['base_file'], entry['base'], e))
            rc = 1
            continue
        new_text = read(root, entry['new_file'])
        if new_text is None:
            out('%s: FAILED: not in the working tree' % entry['new_file'])
            rc = 1
            continue
        rc |= verify(entry, base_text, read(root, entry['base_file']), new_text, members, out)
    for rel in sorted({e['base_file'] for e in moves} | {r['base_file'] for r in residues_all}):
        entries = [e for e in moves if e['base_file'] == rel]
        residues = [dict(r, base=base or r['base']) for r in residues_all if r['base_file'] == rel]
        bases = sorted({base or e['base'] for e in entries})
        tree_text = read(root, rel)
        stray = [r['base'] for r in residues if tree_text is None or r['base'] not in bases]
        if stray:
            out('%s: FAILED: a residue entry at %s with no move from the file at that base, or the file deleted'
                % (rel, stray[0]))
            rc = 1
        if tree_text is None:
            headers = {e['base_header'] for e in entries}
            for b in bases:
                try:
                    kept = [x.rstrip('\r') for x in git_show(root, b, rel).split('\n')
                            if re.match(r'\S.*\bWorldSession::\w+\(', x) and x.rstrip('\r') not in headers]
                except (OSError, subprocess.CalledProcessError) as e:
                    kept = ['(cannot read it at %s: %s)' % (b, e)]
                if kept:
                    out('%s: FAILED: deleted, but at %s it defines %r, which no entry moves (keep the file as a '
                        'residue)' % (rel, b, kept[0]))
                    rc = 1
            continue
        for b in bases:
            try:
                base_text = git_show(root, b, rel)
            except (OSError, subprocess.CalledProcessError) as e:
                out('%s: FAILED: cannot read it at %s from git: %s' % (rel, b, e))
                rc = 1
                continue
            rc |= verify_residue(rel, b, base_text, tree_text, [e['base_header'] for e in entries],
                                 [x for r in residues if r['base'] == b for x in r['removed']],
                                 [x for r in residues if r['base'] != b for x in r['removed']], out)
    out('handler_verbatim: %d moved functions; %s' % (len(moves), 'OK' if rc == 0 else 'FAILED'))
    return rc


SELF_SESSION = '''class WorldSession
{
    public:
        Player* GetPlayer() const { return _player; }
        void SendPacket(WorldPacket const* packet);
    private:
        Player* _player;
        std::string m_name;
        Motion::Reason m_reason;
        proto::SessionId m_sessionId;
        uint32 std;
};'''

SELF_SWING = '''/**
 * @brief Swing.
 */
void WorldSession::HandleSwingOpcode(WorldPacket& recv_data)
{
    Unit* enemy = _player->GetMap()->GetUnit(recv_data.ReadGuid());
    if (!enemy)
    {
        SendStop(NULL);                 // "session." in a comment stays
        return;
    }
    GetPlayer()->Attack(enemy, true);
}
'''

SELF_STOP = '''void WorldSession::HandleStopOpcode(WorldPacket& /*recv_data*/)
{
    GetPlayer()->AttackStop();
}
'''

SELF_HELPERS = '''bool WorldSession::CheckBanker(ObjectGuid guid)
{
    if (!_player->IsBanker(guid, Motion::Reason::Bank))
        return false;
    if (!_player->IsBanker(guid, Motion::Reason::Bank))
        return false;
    return GetPlayer()->IsInWorld();
}

AuctionHouseEntry const* WorldSession::GetCheckedAuctionHouse(ObjectGuid guid)
{
    return sAuctionMgr.GetEntry(guid);
}

uint8 WorldSession::CheckName(ObjectGuid guid)
{
    std::string name;
    return GetPlayer()->GetName(guid, name) ? 1 : 0;
}

void WorldSession::HandleGuildLogOpcode(WorldPacket& /* recvPacket */)
{
    GetPlayer()->SendLog();
}
'''

SELF_BASE = '#include "WorldSession.h"\n\n' + SELF_SWING + '\n' + SELF_STOP + '\n' + SELF_HELPERS

SELF_NEW = (SELF_BASE.replace('WorldSession.h', 'session/handlers/combat/Fixture.h')
            .replace('void WorldSession::HandleSwingOpcode(WorldPacket& recv_data)',
                     'void Fixture::HandleSwing(WorldSession& session, WorldPacket& recv_data)')
            .replace('void WorldSession::HandleStopOpcode(WorldPacket& /*recv_data*/)',
                     'void Fixture::HandleStop(WorldSession& session, WorldPacket& /*recv_data*/)')
            .replace('_player->', 'session.GetPlayer()->').replace('    GetPlayer()', '    session.GetPlayer()')
            .replace('SendStop(', 'SendStop(session, ').replace('return GetPlayer()', 'return session.GetPlayer()')
            .replace(' WorldSession::CheckBanker(', ' Fixture::CheckBanker(WorldSession& session, ')
            .replace(' WorldSession::GetCheckedAuctionHouse(',
                     ' Fixture::GetCheckedAuctionHouse(WorldSession& session, ')
            .replace(' WorldSession::CheckName(', ' Fixture::CheckName(WorldSession& session, ')
            .replace('void WorldSession::HandleGuildLogOpcode(WorldPacket& /* recvPacket */)',
                     'void Fixture::HandleGuildLog(WorldSession& session, WorldPacket& /*recvPacket*/)'))

SELF_NEW_FILE = 'src/game/session/handlers/combat/Fixture.cpp'

SELF_BANKER = ('    if (!session.GetPlayer()->IsBanker(guid, Motion::Reason::Bank))',
               '    if (!_player->IsBanker(guid, Motion::Reason::Bank))')

SELF_MOVES = [
    dict(base='fixture', base_file='Fixture.cpp', new_file=SELF_NEW_FILE,
         base_header='void WorldSession::HandleSwingOpcode(WorldPacket& recv_data)',
         new_header='void Fixture::HandleSwing(WorldSession& session, WorldPacket& recv_data)',
         substitutions=[('SendStop(session, ', 'SendStop(')],
         edits=[('    Unit* enemy = session.GetPlayer()->GetMap()->GetUnit(recv_data.ReadGuid());',
                 '    Unit* enemy = _player->GetMap()->GetUnit(recv_data.ReadGuid());')]),
    dict(base='fixture', base_file='Fixture.cpp', new_file=SELF_NEW_FILE,
         base_header='void WorldSession::HandleStopOpcode(WorldPacket& /*recv_data*/)',
         new_header='void Fixture::HandleStop(WorldSession& session, WorldPacket& /*recv_data*/)'),
    dict(base='fixture', base_file='Fixture.cpp', new_file=SELF_NEW_FILE,
         base_header='bool WorldSession::CheckBanker(ObjectGuid guid)',
         new_header='bool Fixture::CheckBanker(WorldSession& session, ObjectGuid guid)',
         edits=[SELF_BANKER + (2,)]),
    dict(base='fixture', base_file='Fixture.cpp', new_file=SELF_NEW_FILE,
         base_header='AuctionHouseEntry const* WorldSession::GetCheckedAuctionHouse(ObjectGuid guid)',
         new_header='AuctionHouseEntry const* Fixture::GetCheckedAuctionHouse(WorldSession& session, ObjectGuid guid)'),
    dict(base='fixture', base_file='Fixture.cpp', new_file=SELF_NEW_FILE,
         base_header='uint8 WorldSession::CheckName(ObjectGuid guid)',
         new_header='uint8 Fixture::CheckName(WorldSession& session, ObjectGuid guid)'),
    dict(base='fixture', base_file='Fixture.cpp', new_file=SELF_NEW_FILE,
         base_header='void WorldSession::HandleGuildLogOpcode(WorldPacket& /* recvPacket */)',
         new_header='void Fixture::HandleGuildLog(WorldSession& session, WorldPacket& /*recvPacket*/)'),
]

SELF_HEADERS = [e['base_header'] for e in SELF_MOVES]
SELF_RESIDUE_BASE = '#include "Chat.h"\n#include "Log.h"\n' + SELF_BASE
SELF_RESIDUE_HEAD = '#include "Chat.h"\n#include "Log.h"\n#include "WorldSession.h"\n\n'
SELF_RESIDUES = {
    'swing': SELF_RESIDUE_HEAD + SELF_STOP + '\n' + SELF_HELPERS,
    'swing and stop': SELF_RESIDUE_HEAD + SELF_HELPERS,
    'swing, Log.h': SELF_RESIDUE_HEAD.replace('#include "Log.h"\n', '') + SELF_STOP + '\n' + SELF_HELPERS,
}


def self_test():
    failures = []
    members = class_members(SELF_SESSION, 'WorldSession')

    def run(label, want_rc, needle, entry=0, swap=None, tree_base='#include "WorldSession.h"\n', base=SELF_BASE,
            **change):
        new = SELF_NEW
        if swap:
            if swap[0] not in new:
                failures.append('%s: the mutation %r matches nothing' % (label, swap[0]))
                print('self-test: %-62s FAIL' % label)
                return
            new = new.replace(*swap)
        got = []
        try:
            rc = verify(dict(SELF_MOVES[entry], **change), base, tree_base, new, members, got.append)
        except Exception as e:                                  # a crash fails the row
            rc = 2
            got.append('crashed: %r' % e)
        text = '\n'.join(got)
        ok = rc == want_rc and needle in text
        print('self-test: %-62s %s' % (label, 'PASS' if ok else 'FAIL'))
        if not ok:
            failures.append('%s: rc %d (want %d)\n%s' % (label, rc, want_rc, text))

    run('a handler with a _player edit and a sender call pastes back', 0, 'IDENTICAL', 0)
    run('a handler with a commented-out packet pastes back', 0, 'IDENTICAL', 1)
    run('a bare member (an implicit this->) fails', 1, '"GetPlayer", a member of WorldSession, used bare', 1,
        swap=('    session.GetPlayer()->AttackStop();', '    GetPlayer()->AttackStop();'))
    run('a bare private member fails', 1, '"_player", a member of WorldSession, used bare', 0,
        swap=('    session.GetPlayer()->Attack(', '    _player->Attack('))
    run('a changed body line fails', 1, 'DIFFERS from Fixture.cpp at fixture: line 14', 0,
        swap=('Attack(enemy, true)', 'Attack(enemy, false)'))
    run('a changed doc comment line fails', 1, 'DIFFERS from Fixture.cpp at fixture: line 4', 0,
        swap=(' * @brief Swing.', ' * @brief Swing at the target.'))
    run('a handler of another shape fails', 1, 'the definition line is not `<type> <Class>::<Name>(', 1,
        new_header='void Fixture::HandleStop(WorldSession* session, WorldPacket& /*recv_data*/)')
    run('a definition line taking other parameters than its base fails', 1, 'takes (ObjectGuid other)', 2,
        new_header='bool Fixture::CheckBanker(WorldSession& session, ObjectGuid other)')
    run('the base function still in the working tree fails', 1, 'still defines', 1, tree_base=SELF_BASE)
    run('the base file deleted in the working tree passes', 0, 'IDENTICAL', 1, tree_base=None)
    run('an edit that matches no line fails', 1, 'matches 0 lines of the function', 0,
        swap=('session.GetPlayer()->GetMap()', 'session.GetPlayer()->GetMap( )'))
    run('a substitution that matches nothing fails', 1, 'matches no code in the function', 0,
        swap=('SendStop(session, NULL)', 'SendStop(NULL)'))
    run('the edit not listed fails', 1, 'DIFFERS from Fixture.cpp at fixture: line 8', 0, edits=[])
    run('a base header not found fails', 1, 'Fixture.cpp at fixture: the definition line found 0 times', 1,
        base_header='void WorldSession::HandleStop(WorldPacket& /*recv_data*/)')
    shadow = ('    GetPlayer()->AttackStop();\n',
              '    {\n        WorldSession& session = *Other();\n        session.Ping();\n    }\n'
              '    GetPlayer()->AttackStop();\n')
    run('a base function naming session in code fails', 1, 'the base function names `session` in code', 1,
        base=SELF_BASE.replace(*shadow), swap=('    session.GetPlayer()->AttackStop();\n',
                                               '    {\n        WorldSession& session = *Other();\n'
                                               '        session.Ping();\n    }\n'
                                               '    session.GetPlayer()->AttackStop();\n'))
    early = ('    GetPlayer()->AttackStop();\n', '    {\n    Prepare();\n}\n    GetPlayer()->AttackStop();\n')
    run('a column-0 brace inside a base body fails', 1, 'closes the function before its braces balance', 1,
        base=SELF_BASE.replace(*early), swap=('    session.GetPlayer()->AttackStop();\n',
                                              '    {\n    Prepare();\n}\n    Evil();\n'))
    run('a substitution inside a longer name is not read back: fails', 1, 'DIFFERS', 0,
        swap=('        SendStop(session, NULL);', '        SendStop(session, NULL); XSendStop(session, NULL);'),
        base=SELF_BASE.replace('        SendStop(NULL);', '        SendStop(NULL); XSendStop(NULL);'))
    run('one new line listed as two edits fails', 1, 'one new line is listed as two edits', 0,
        edits=SELF_MOVES[0]['edits'] * 2)
    run('a new file outside the handlers directory fails', 1, 'the new file is not under', 1,
        new_file='src/game/WorldHandlers/Fixture.cpp')

    run('std::string in a body is a qualifier, not a member: passes', 0, 'IDENTICAL', 4)
    run('Motion:: in a body is a qualifier, not a member: passes', 0, 'IDENTICAL', 2)
    run('a member named std used alone fails', 1, '"std", a member of WorldSession, used bare', 4,
        swap=('    std::string name;', '    std::string name = std;'),
        base=SELF_BASE.replace('    std::string name;', '    std::string name = std;'))
    proto = '    ItemPrototype const* proto = sObjectMgr.GetItemPrototype(guid);\n    std::string name = proto->Name1;'
    run('a local named proto through an arrow, proto no member: passes', 0, 'IDENTICAL', 4,
        swap=('    std::string name;', proto), base=SELF_BASE.replace('    std::string name;', proto))
    run('a bool helper pastes back', 0, 'IDENTICAL to Fixture.cpp at fixture', 2)
    run('a helper returning a pointer pastes back', 0, 'IDENTICAL', 3)
    run('a uint8 helper pastes back', 0, 'with 5 lines pasted back', 4)
    run('a helper whose base is not a WorldSession member fails', 1, 'is not a WorldSession member\'s', 2,
        base_header='bool Other::CheckBanker(ObjectGuid guid)')
    run('a helper returning another type than its base fails', 1, 'returns \'uint8\', the base \'bool\'', 2,
        new_header='uint8 Fixture::CheckBanker(WorldSession& session, ObjectGuid guid)')
    run('a /*name*/ head over a /* name */ base pastes back', 0, 'IDENTICAL', 5)
    run('a /* name */ head pastes back', 0, 'IDENTICAL', 5,
        swap=('WorldPacket& /*recvPacket*/)', 'WorldPacket& /* recvPacket */)'),
        new_header='void Fixture::HandleGuildLog(WorldSession& session, WorldPacket& /* recvPacket */)')
    run('a head naming a parameter its base comments out fails', 1,
        'takes (WorldPacket& recv_data) after the session, the base (WorldPacket& /*recv_data*/)', 1,
        new_header='void Fixture::HandleStop(WorldSession& session, WorldPacket& recv_data)')
    run('a head commenting out a parameter its base names fails', 1,
        'takes (WorldPacket& /*recv_data*/) after the session, the base (WorldPacket& recv_data)', 0,
        new_header='void Fixture::HandleSwing(WorldSession& session, WorldPacket& /*recv_data*/)')
    run('a head naming WorldSession as its class fails', 1, 'names WorldSession as its class', 1,
        new_header='void WorldSession::HandleStop(WorldSession& session, WorldPacket& /*recv_data*/)')
    run('a bare member on a line that names a qualifier later fails', 1,
        '"_player", a member of WorldSession, used bare', 1,
        swap=('    session.GetPlayer()->AttackStop();', '    _player->AttackStop(std::string());'))
    run('an edit with its count of 1 stated pastes back', 0, '(1 edits)', 0,
        edits=[SELF_MOVES[0]['edits'][0] + (1,)])
    run('an edit covering 2 identical lines pastes back', 0, '(2 edits)', 2)
    run('an edit of 2 identical lines listed as 1 fails', 1, 'not the 1 it covers: too many found', 2,
        edits=[SELF_BANKER + (1,)])
    run('an edit of 2 identical lines listed as 3 fails', 1, 'not the 3 it covers: too few found', 2,
        edits=[SELF_BANKER + (3,)])
    run('an edit covering 0 lines fails', 1, 'a count is 1 or more', 2, edits=[SELF_BANKER + (0,)])

    def residue(label, want_rc, needle, tree, headers=SELF_HEADERS[:1], removed=(), elsewhere=(),
                base=SELF_RESIDUE_BASE):
        got = []
        try:
            rc = verify_residue('Fixture.cpp', 'fixture', base, tree, headers, list(removed),
                                list(elsewhere), got.append)
        except Exception as e:                                  # a crash fails the row
            rc = 2
            got.append('crashed: %r' % e)
        text = '\n'.join(got)
        ok = rc == want_rc and needle in text
        print('self-test: %-62s %s' % (label, 'PASS' if ok else 'FAIL'))
        if not ok:
            failures.append('%s: rc %d (want %d)\n%s' % (label, rc, want_rc, text))

    swing = SELF_RESIDUES['swing']
    residue('a residue equal to the base less the moved function passes', 0,
            'IDENTICAL to Fixture.cpp at fixture, byte for byte, with 1 moved functions cut and 0', swing)
    residue('two moved functions adjacent in the base, cut in order: passes', 0, 'with 2 moved functions cut',
            SELF_RESIDUES['swing and stop'], headers=SELF_HEADERS[:2])
    residue('a listed include removal passes', 0, 'and 1 includes removed', SELF_RESIDUES['swing, Log.h'],
            removed=['#include "Log.h"'])
    residue('an include removal not listed fails', 1, 'line 2, \'#include "Log.h"\', is removed and not listed',
            SELF_RESIDUES['swing, Log.h'])
    residue('a changed residue line fails', 1, 'DIFFERS from Fixture.cpp at fixture with the moved functions cut: '
            'line 7', swing.replace('AttackStop();', 'AttackStop(true);'))
    residue('a moved function still in the residue fails', 1, 'it still defines', swing, headers=SELF_HEADERS[:2])
    residue('a listed removal not found fails', 1, 'found 0 times', swing, removed=['#include "Map.h"'])
    residue('a listed removal that is not an include line fails', 1, 'is not an include line',
            swing.replace('    GetPlayer()->SendLog();\n', ''), removed=['    GetPlayer()->SendLog();'])
    residue('a removal listed at another base, gone here, passes', 0, 'IDENTICAL', swing,
            elsewhere=['#include "Map.h"'])
    stop = 'void WorldSession::HandleStopOpcode('
    kept = SELF_RESIDUE_HEAD + SELF_SWING + '\n' + SELF_HELPERS
    residue('a cut that starts inside an open block comment fails', 1, 'hold code or open above them', kept,
            headers=SELF_HEADERS[1:2], base=SELF_RESIDUE_BASE.replace(stop, '/* the old helper\n   kept\n */\n' + stop))
    residue('code on the comment line above a moved function fails', 1, 'hold code or open above them', kept,
            headers=SELF_HEADERS[1:2], base=SELF_RESIDUE_BASE.replace(stop, '/* x */ #include "C.h"\n' + stop))
    residue('a listed removal ending in a line splice fails', 1, 'is not an include directive standing alone',
            swing.replace('#include "Chat.h"\n', ''), removed=['#include "Chat.h" // pulls Log in: \\'],
            base=SELF_RESIDUE_BASE.replace('#include "Chat.h"', '#include "Chat.h" // pulls Log in: \\'))
    residue('a listed removal after a line splice fails', 1, 'is not an include directive standing alone',
            '// note \\\n' + swing.replace('#include "Chat.h"\n', ''), removed=['#include "Chat.h"'],
            base='// note \\\n' + SELF_RESIDUE_BASE)
    residue('a listed removal inside a raw string literal fails', 1, 'is not an include directive standing alone',
            swing.replace('#include "WorldSession.h"\n', 'char const* k = R"(\n)";\n#include "WorldSession.h"\n'),
            removed=['#include "Z.h"'], base=SELF_RESIDUE_BASE.replace(
                '#include "WorldSession.h"\n', 'char const* k = R"(\n#include "Z.h"\n)";\n#include "WorldSession.h"\n'))
    residue('a listed removal standing twice fails', 1, 'found 2 times', SELF_RESIDUES['swing, Log.h'],
            removed=['#include "Log.h"'],
            base=SELF_RESIDUE_BASE.replace('#include "Log.h"\n', '#include "Log.h"\n#include "Log.h"\n'))
    residue('a listed removal of another directive fails', 1, 'is not an include line', swing,
            removed=['#define FIXTURE 1'], base=SELF_RESIDUE_BASE.replace('#include "Log.h"\n',
                                                                           '#include "Log.h"\n#define FIXTURE 1\n'))

    def checked(label, want_rc, needle, residues, tree=None, moves=None):
        b1 = SELF_RESIDUE_BASE
        b2 = SELF_RESIDUES['swing, Log.h']
        tree = SELF_RESIDUE_HEAD.replace('#include "Log.h"\n', '') + SELF_HELPERS if tree is None else tree
        shown = {('b1', 'O.cpp'): b1, ('b2', 'O.cpp'): b2}
        files = {SESSION_HEADER: SELF_SESSION, SELF_NEW_FILE: SELF_NEW, 'O.cpp': tree or None}
        two = [dict(SELF_MOVES[0], base='b1', base_file='O.cpp'), dict(SELF_MOVES[1], base='b2', base_file='O.cpp')]
        saved = globals()['git_show'], globals()['read']
        globals()['git_show'] = lambda root, ref, rel: shown[(ref, rel)]
        globals()['read'] = lambda root, rel: files.get(rel)
        got = []
        try:
            rc = check('.', out=got.append, moves=two if moves is None else moves, residues=residues)
        except Exception as e:                                  # a crash fails the row
            rc = 2
            got.append('crashed: %r' % e)
        finally:
            globals()['git_show'], globals()['read'] = saved
        text = '\n'.join(got)
        ok = rc == want_rc and needle in text
        print('self-test: %-62s %s' % (label, 'PASS' if ok else 'FAIL'))
        if not ok:
            failures.append('%s: rc %d (want %d)\n%s' % (label, rc, want_rc, text))

    log = [dict(base='b1', base_file='O.cpp', removed=['#include "Log.h"'])]
    checked('--check: a residue of two changes, proven at both bases', 0, 'at b1, byte for byte, with 2 moved', log)
    checked('--check: a residue line changed fails at both bases', 1, 'DIFFERS from O.cpp at b2', log,
            tree=SELF_RESIDUE_HEAD.replace('#include "Log.h"\n', '') + SELF_HELPERS.replace('IsInWorld', 'IsInMap'))
    checked('--check: an include listed at the wrong base fails', 1, 'found 0 times', [dict(log[0], base='b2')])
    checked('--check: a residue entry at a base no move names fails', 1, 'a residue entry at b3',
            log + [dict(base='b3', base_file='O.cpp', removed=[])])
    checked('--check: a deleted origin file, every function moved: passes', 0, '6 moved functions; OK', [], tree=False,
            moves=[dict(m, base='b1', base_file='O.cpp') for m in SELF_MOVES])
    checked('--check: a deleted origin file holding a function no entry moves fails', 1, 'which no entry moves',
            [], tree=False)

    for label, bad in split_gate.self_test(__file__, 'handler_moves', DATA_NAMES, 'MOVES'):
        print('self-test: %-62s %s' % (label, 'PASS' if not bad else 'FAIL'))
        failures += bad

    for f in failures:
        print('SELF-TEST FAILED: ' + f)
    print('self-test: %s (%d failure(s))' % ('PASS' if not failures else 'FAIL', len(failures)))
    return 1 if failures else 0


def main(argv):
    if split_gate.check(__file__, 'handler_moves', DATA_NAMES, globals()):
        return 1
    ap = argparse.ArgumentParser(description='The handler classes\' verbatim proof.')
    ap.add_argument('--root', default=os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                                                   '..', '..', '..')))
    ap.add_argument('--base', help='prove every entry against this commit instead of its own base')
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument('--check', action='store_true')
    g.add_argument('--self-test', action='store_true')
    args = ap.parse_args(argv[1:])
    if args.self_test:
        return self_test()
    return check(os.path.abspath(args.root), args.base)


if __name__ == '__main__':
    sys.exit(main(sys.argv))
