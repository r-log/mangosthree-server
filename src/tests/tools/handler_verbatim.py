#!/usr/bin/env python3
"""handler_verbatim.py [--root <repo root>] [--base <ref>] --check | --self-test

The handler classes' verbatim proof: a function moved whole from a `WorldSession` member into a handler
class's static (src/game/session/handlers/) is pasted back at its old place, and the old file must come
back byte for byte as it was at the entry's base.

MOVES holds one entry per moved function:
  base, base_file, base_header  the commit the move is proven against (the parent of the change that
      moved it: each entry names its own, so moves from one file in several changes are proven apart),
      the file that held the function there and its definition line, `void WorldSession::<Name>(...)`;
  new_file, new_header  the file that holds it in the working tree and its definition line: a handler,
      `void <Class>::Handle<X>(WorldSession& session, WorldPacket& <p>)`, <p> the base's own parameter
      name or that name commented out, or a sender, `void <Class>::Send<X>(WorldSession& session, ...)`;
  substitutions  (new text, base text) pairs, each matching code at least once (a sender's call
      `SendAttackStop(session, ` read back as `SendAttackStop(`);
  edits  (new line, base line) pairs: a line changed beyond the substitutions, found once and read back whole.

For each entry, --check:
  1. reads the function in each version: the comment lines directly above its definition line (`/*`,
     ` *`, `//`: the doc comment moves with it), that line, and its body to the first `}` at column 0,
     each definition line found exactly once;
  2. fails on a body line naming a member of `WorldSession` bare (an implicit `this->` the move missed:
     the static would compile against a free function or global of that name); the names are every
     member function and data member at class scope in the working tree's `WorldSession.h`, read by
     verbatim.py's MEMBERS_OF reader (class_members);
  3. reverses the move: an edit's line becomes its base line; on every other line, in code only
     (comments and literals stay), `session.` not after a name, `.`, `->` or `::` is dropped and the
     substitutions are read back; the definition line becomes base_header;
  4. pastes that at the function's place in base_file at base and compares the file byte for byte;
  5. fails when the working tree's base_file still holds base_header (the move deletes it).
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

SESSION_HEADER = 'src/game/Server/WorldSession.h'

MOVES = []

HANDLER = re.compile(r'void \w+::Handle\w+\(WorldSession& session, WorldPacket& (\w+|/\*\w+\*/)\)$')
SENDER = re.compile(r'void \w+::Send\w+\(WorldSession& session(, [^()]+)?\)$')
BASE_HEADER = re.compile(r'void WorldSession::\w+\(.*\)$')
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
    bare = re.compile(r'(?<![\w.>:~])(%s)\b' % '|'.join(re.escape(m) for m in sorted(members)))
    for n, line in enumerate(blank('\n'.join(body)).split('\n'), 1):
        m = bare.search(line)
        if m:
            raise Failure('body line %d: "%s", a member of WorldSession, used bare (an implicit this-> the '
                          'move missed): %r' % (n, m.group(1), body[n - 1]))


def verify(entry, base_text, tree_base_text, new_text, members, out=print):
    """0 when the entry's function pastes back byte for byte; 1 with the reason printed."""
    name = '%s %s' % (entry['new_file'], entry['new_header'])
    try:
        if not (HANDLER.match(entry['new_header']) or SENDER.match(entry['new_header'])):
            raise Failure('the definition line is neither a handler\'s nor a sender\'s shape')
        if not BASE_HEADER.match(entry['base_header']):
            raise Failure('the base definition line is not a WorldSession member\'s: %r' % entry['base_header'])
        new_lines = new_text.split('\n')
        first, at, end = function_span(new_lines, entry['new_header'], entry['new_file'])
        check_members(new_lines[at + 1:end], members)
        span = new_lines[first:end]
        edits = dict(entry.get('edits', []))
        for line in edits:
            hits = sum(1 for x in span if x == line)
            if hits != 1:
                raise Failure('the edit %r matches %d lines of the function' % (line, hits))
        rules = [(SESSION_DOT, '')] + [(re.compile(re.escape(a)), b) for a, b in entry.get('substitutions', [])]
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
        name, entry['base_file'], entry['base'], len(pasted), len(edits)))
    return 0


def read(root, rel):
    path = os.path.join(root, *rel.split('/'))
    if not os.path.isfile(path):
        return None
    with open(path, encoding='utf-8', newline='') as fh:
        return fh.read()


def check(root, base=None, out=print):
    rc = 0
    members = class_members(read(root, SESSION_HEADER), 'WorldSession') if MOVES else set()
    for entry in MOVES:
        entry = dict(entry, base=base or entry['base'])
        try:
            base_text = subprocess.run(['git', '-C', root, 'show', '%s:%s' % (entry['base'], entry['base_file'])],
                                       capture_output=True, check=True).stdout.decode('utf-8')
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
    out('handler_verbatim: %d moved functions; %s' % (len(MOVES), 'OK' if rc == 0 else 'FAILED'))
    return rc


SELF_SESSION = '''class WorldSession
{
    public:
        Player* GetPlayer() const { return _player; }
        void SendPacket(WorldPacket const* packet);
    private:
        Player* _player;
};'''

SELF_BASE = '''#include "WorldSession.h"

/**
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

void WorldSession::HandleStopOpcode(WorldPacket& /*recv_data*/)
{
    GetPlayer()->AttackStop();
}
'''

SELF_NEW = (SELF_BASE.replace('WorldSession.h', 'session/handlers/combat/Fixture.h')
            .replace('void WorldSession::HandleSwingOpcode(WorldPacket& recv_data)',
                     'void Fixture::HandleSwing(WorldSession& session, WorldPacket& recv_data)')
            .replace('void WorldSession::HandleStopOpcode(WorldPacket& /*recv_data*/)',
                     'void Fixture::HandleStop(WorldSession& session, WorldPacket& /*recv_data*/)')
            .replace('_player->', 'session.GetPlayer()->').replace('    GetPlayer()', '    session.GetPlayer()')
            .replace('SendStop(', 'SendStop(session, '))

SELF_MOVES = [
    dict(base='fixture', base_file='Fixture.cpp', new_file='Fixture2.cpp',
         base_header='void WorldSession::HandleSwingOpcode(WorldPacket& recv_data)',
         new_header='void Fixture::HandleSwing(WorldSession& session, WorldPacket& recv_data)',
         substitutions=[('SendStop(session, ', 'SendStop(')],
         edits=[('    Unit* enemy = session.GetPlayer()->GetMap()->GetUnit(recv_data.ReadGuid());',
                 '    Unit* enemy = _player->GetMap()->GetUnit(recv_data.ReadGuid());')]),
    dict(base='fixture', base_file='Fixture.cpp', new_file='Fixture2.cpp',
         base_header='void WorldSession::HandleStopOpcode(WorldPacket& /*recv_data*/)',
         new_header='void Fixture::HandleStop(WorldSession& session, WorldPacket& /*recv_data*/)'),
]


def self_test():
    failures = []
    members = class_members(SELF_SESSION, 'WorldSession')

    def run(label, want_rc, needle, entry=0, swap=None, tree_base='#include "WorldSession.h"\n', **change):
        new = SELF_NEW
        if swap:
            if swap[0] not in new:
                failures.append('%s: the mutation %r matches nothing' % (label, swap[0]))
                print('self-test: %-62s FAIL' % label)
                return
            new = new.replace(*swap)
        got = []
        try:
            rc = verify(dict(SELF_MOVES[entry], **change), SELF_BASE, tree_base, new, members, got.append)
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
    run('a handler of another shape fails', 1, 'neither a handler\'s nor a sender\'s shape', 1,
        new_header='void Fixture::HandleStop(WorldSession* session, WorldPacket& /*recv_data*/)')
    run('a sender\'s definition line passes the shape check', 1, 'the definition line found 0 times', 1,
        new_header='void Fixture::SendStop(WorldSession& session, Unit const* enemy)')
    run('the base function still in the working tree fails', 1, 'still defines', 1, tree_base=SELF_BASE)
    run('the base file deleted in the working tree passes', 0, 'IDENTICAL', 1, tree_base=None)
    run('an edit that matches no line fails', 1, 'matches 0 lines of the function', 0,
        swap=('session.GetPlayer()->GetMap()', 'session.GetPlayer()->GetMap( )'))
    run('a substitution that matches nothing fails', 1, 'matches no code in the function', 0,
        swap=('SendStop(session, NULL)', 'SendStop(NULL)'))
    run('the edit not listed fails', 1, 'DIFFERS from Fixture.cpp at fixture: line 8', 0, edits=[])
    run('a base header not found fails', 1, 'Fixture.cpp at fixture: the definition line found 0 times', 1,
        base_header='void WorldSession::HandleStop(WorldPacket& /*recv_data*/)')

    for f in failures:
        print('SELF-TEST FAILED: ' + f)
    print('self-test: %s (%d failure(s))' % ('PASS' if not failures else 'FAIL', len(failures)))
    return 1 if failures else 0


def main(argv):
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
