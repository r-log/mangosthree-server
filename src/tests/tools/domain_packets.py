#!/usr/bin/env python3
"""domain_packets.py [--root <repo root>] --check | --self-test: the domain tier builds no packet.

Scope: every file under src/ that layout_gate.py's RULES give a domain layer (entities, spells, combat,
maps, ai, social, pvp, economy), and the domain's precompiled header, src/game/pchdomain.h. In them:

  - no #include of a header under src/proto/, resolved the way layout_gate.py resolves it;
  - no WorldPacket by value: the type name is followed, past whitespace and const or volatile, only by
    `*` or `&`, or is named after `class` or `struct` (a forward declaration). A packet built, held,
    returned or passed by value, a std::vector<WorldPacket> and WorldPacket::member all fail;
  - no opcode name (SMSG_, CMSG_ or MSG_ starting a word) and no Opcodes or OpcodesList.

Comments and string and character literals are ignored (case_labels.blank). The allow list is empty: a
file that needs any of these belongs in session/.

Not caught: a packet built through a macro or a template parameter that names WorldPacket elsewhere, and
a typedef of WorldPacket declared outside the scope.
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
import shutil
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import case_labels  # noqa: E402
import layout_gate  # noqa: E402

PCH = 'game/pchdomain.h'
PACKET_RX = re.compile(r'\bWorldPacket\b')
OPCODE_RX = re.compile(r'\b(?:SMSG|CMSG|MSG)_\w+|\bOpcodes(?:List)?\b')
QUALIFIER_RX = re.compile(r'(?:\s|\bconst\b|\bvolatile\b)*')
ELABORATED_RX = re.compile(r'\b(?:class|struct)\s+$')


def in_scope(rel):
    return rel == PCH or layout_gate.layer(rel) in layout_gate.DOMAIN


def line_of(text, offset):
    return text.count('\n', 0, offset) + 1


def by_value(clean):
    """Offsets of every WorldPacket named by value in the blanked text."""
    out = []
    for m in PACKET_RX.finditer(clean):
        if ELABORATED_RX.search(clean[max(0, m.start() - 16):m.start()]):
            continue
        rest = QUALIFIER_RX.match(clean, m.end()).end()
        if rest < len(clean) and clean[rest] in '*&':
            continue
        out.append(m.start())
    return out


def findings(rel, text, resolve):
    """[(line, message)] of the file's violations; resolve(rel, spelling) answers the header's path."""
    out = []
    clean = case_labels.blank(text)
    raw = text.split('\n')
    for no, line in enumerate(clean.split('\n'), 1):
        if not line.lstrip().startswith('#'):
            continue
        m = layout_gate.INCLUDE_RX.match(raw[no - 1])
        if not m:
            continue
        target = resolve(rel, m.group(1).replace('\\', '/'))
        if target and layout_gate.layer(target) == 'proto':
            out.append((no, '#include "%s" is a proto header (src/%s)' % (m.group(1), target)))
    for offset in by_value(clean):
        out.append((line_of(clean, offset), 'a WorldPacket by value; only session/ builds or holds a packet'))
    for m in OPCODE_RX.finditer(clean):
        out.append((line_of(clean, m.start()), '%s names an opcode; only session/ and proto/ do' % m.group(0)))
    return sorted(out)


def scan(root):
    """[(path, line, message)] of every violation, and the number of files read."""
    src = os.path.join(root, 'src')
    files = layout_gate.list_files(src) if os.path.isdir(src) else []
    src_lower = {x.lower(): x for x in files}
    src_index = layout_gate.Index(files)
    dep = os.path.join(root, 'dep')
    dep_index = layout_gate.Index(['dep/' + x for x in layout_gate.list_files(dep)] if os.path.isdir(dep) else [])
    templates = layout_gate.list_templates(src) if os.path.isdir(src) else []

    def resolve(rel, spelling):
        try:
            how, path = layout_gate.resolve(rel, spelling, src_index, src_lower, dep_index, templates)
        except layout_gate.GateError:
            return None
        return path if how == 'tree' else None

    out = []
    read = 0
    for rel in files:
        if not in_scope(rel):
            continue
        with open(os.path.join(src, rel), encoding='utf-8', errors='replace', newline='') as fh:
            text = fh.read()
        read += 1
        out.extend(('src/' + rel, line, message) for line, message in findings(rel, text, resolve))
    return out, read


def check(root, out=print):
    found, read = scan(root)
    if read == 0:
        out('DomainPackets: read no domain file under %s/src -- a gate that scans nothing passes nothing' % root)
        return 1
    for rel, line, message in found:
        out('%s:%d: %s' % (rel, line, message))
    if found:
        out('DomainPackets: %d violation(s) in the domain tier; move the packet code to session/ '
            '(design/architecture.md section 2)' % len(found))
        return 1
    out('DomainPackets OK: %d files of the domain tier and its precompiled header, no proto include, no WorldPacket by value, '
        'no opcode name' % read)
    return 0


def self_test():
    failures = []

    def expect_value(label, text, want):
        got = [line_of(text, o) for o in by_value(case_labels.blank(text))]
        if got != want:
            failures.append('by value %s: lines %r, expected %r' % (label, got, want))

    expect_value('pointers, references and forward declarations pass',
                 'class WorldPacket;\nstruct WorldPacket;\nvoid F(WorldPacket* p);\nvoid G(WorldPacket const& p);\n'
                 'void H(const WorldPacket &p);\nvoid I(WorldPacket const * p);\nvoid J(WorldPacket\n    * p);\n', [])
    expect_value('a built packet, a temporary, a member and a list fail',
                 'WorldPacket data(1, 4);\nSend(WorldPacket(1));\nWorldPacket m_packet;\n'
                 'std::vector<WorldPacket> v;\nWorldPacket const copy = data;\nWorldPacket::size_type n;\n',
                 [1, 2, 3, 4, 5, 6])
    expect_value('a parameter by value fails', 'void F(WorldPacket packet);\nvoid G(WorldPacket);\n', [1, 2])
    expect_value('comments and literals are ignored',
                 '// WorldPacket data(1);\n/* WorldPacket m;\n */\nconst char* s = "WorldPacket x;";\n', [])
    expect_value('a longer identifier is not the type', 'WorldPacketList l;\nMyWorldPacket p;\n', [])

    def expect_opcodes(label, text, want):
        got = [line for line, message in findings('game/Object/X.cpp', text, lambda r, s: None)
               if 'opcode' in message]
        if got != want:
            failures.append('opcodes %s: lines %r, expected %r' % (label, got, want))

    expect_opcodes('opcode names and the enum fail',
                   'x = SMSG_ATTACKSTART;\ny = CMSG_CAST_SPELL;\nz = MSG_MOVE_JUMP;\nOpcodes o;\nOpcodesList l;\n',
                   [1, 2, 3, 4, 5])
    expect_opcodes('a chat type, a literal and a comment pass',
                   'c = CHAT_MSG_SAY;\nconst char* s = "SMSG_X";\n// CMSG_Y\nOpcodesTable t;\n', [])

    tree = {
        'src/proto/WorldPacket.h': 'class WorldPacket {};\n',
        'src/proto/Opcodes.h': 'enum Opcodes {};\n',
        'src/game/Object/Unit.cpp': '#include "Unit.h"\nvoid Unit::F(WorldPacket* p) {}\n',
        'src/game/Object/Unit.h': 'class WorldPacket;\n',
        'src/game/session/Packets.cpp': '#include "WorldPacket.h"\nWorldPacket data(SMSG_X, 4);\n',
        'src/' + PCH: '#include "Unit.h"\n',
    }

    def expect_check(label, files, want):
        d = tempfile.mkdtemp(prefix='domain_packets_')
        try:
            for rel, text in files.items():
                path = os.path.join(d, *rel.split('/'))
                os.makedirs(os.path.dirname(path), exist_ok=True)
                with open(path, 'w', encoding='utf-8', newline='') as fh:
                    fh.write(text)
            got = check(d, out=lambda s: None)
        finally:
            shutil.rmtree(d, ignore_errors=True)
        if got != want:
            failures.append('check %s: %d, expected %d' % (label, got, want))

    expect_check('a clean domain and a session that builds packets pass', tree, 0)
    expect_check('a domain include of the packet fails',
                 dict(tree, **{'src/game/Object/Unit.cpp': '#include "WorldPacket.h"\n'}), 1)
    expect_check('a domain include of the opcodes by a longer path fails',
                 dict(tree, **{'src/game/spells/pipeline/Spell.cpp': '#include "proto/Opcodes.h"\n'}), 1)
    expect_check('an include in a comment passes',
                 dict(tree, **{'src/game/Object/Unit.cpp': '// #include "WorldPacket.h"\n/*\n#include "Opcodes.h"\n*/\n'}), 0)
    expect_check('the PCH including the opcodes fails', dict(tree, **{'src/game/pchdomain.h': '#include "Opcodes.h"\n'}), 1)
    expect_check('the other PCH is not read', dict(tree, **{'src/game/pchdef.h': '#include "Opcodes.h"\n'}), 0)
    expect_check('a packet built in the maps fails',
                 dict(tree, **{'src/game/Maps/Weather.cpp': 'void W()\n{\n    WorldPacket data(1, 4);\n}\n'}), 1)
    expect_check('an opcode named in the pvp fails',
                 dict(tree, **{'src/game/BattleGround/BattleGround.cpp': 'uint16 o = SMSG_BATTLEFIELD_STATUS;\n'}), 1)
    expect_check('a data file building a packet is not read',
                 dict(tree, **{'src/game/data/Store.cpp': 'WorldPacket data(SMSG_X);\n'}), 0)
    expect_check('an empty scope fails', {'src/game/session/Packets.cpp': 'WorldPacket data;\n'}, 1)

    for f in failures:
        print('self-test: ' + f)
    print('self-test: %s (%d failure(s))' % ('PASS' if not failures else 'FAIL', len(failures)))
    return 1 if failures else 0


def main(argv):
    ap = argparse.ArgumentParser(description='CheckDomainPackets: the domain tier builds no packet.')
    here = os.path.dirname(os.path.abspath(__file__))
    ap.add_argument('--root', default=os.path.abspath(os.path.join(here, '..', '..', '..')))
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument('--check', action='store_true')
    g.add_argument('--self-test', action='store_true')
    args = ap.parse_args(argv[1:])
    if args.self_test:
        return self_test()
    return check(os.path.abspath(args.root))


if __name__ == '__main__':
    sys.exit(main(sys.argv))
