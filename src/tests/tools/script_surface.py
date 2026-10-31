#!/usr/bin/env python3
"""script_surface.py [--root <repo root>] --check [--symbols <nm|dumpbin> <tool>] [--core-lib <lib>]... [--content-object <obj>]...
script_surface.py [--root <repo root>] --generate
script_surface.py --self-test

CheckScriptSurface: what a new script may reach (design/architecture.md, sections 4 and 7).

  1. A file under src/scripts/content/ includes only surface headers (src/scripts/surface/*.h, not
     surface/detail/), headers under src/scripts/content/, and the allowed standard headers.
  2. A surface header includes only surface headers, the vocabulary headers (Platform/Define.h,
     SharedDefines.h, DBCEnums.h) and the allowed standard headers.
  3. A content object refers to no symbol a core library defines: the undefined symbols of the
     content objects against the defined symbols of the core libraries, read with nm (-u, and
     --defined-only -g) or dumpbin (/symbols, and /linkermember:1).
  4. No file outside src/scripts/legacy/ includes a legacy header, except src/mangosd/LegacyScripts.cpp,
     which hands the legacy library's hooks to the dispatcher.
  5. The legacy reach only shrinks: src/tests/script_legacy_reach.txt holds one line per (legacy file,
     kind, core name), and a line the tree does not have yet fails, as does a listed line the tree no
     longer has. The kinds: call (`->n(` or `.n(`), free (`n(`), override (a declaration marked
     override), include (a header outside src/scripts/legacy/), singleton (`s...` defined by the core)
     and downcast (a cast to a pointer to a core class). The core names are those its headers under
     src/game, src/shared, src/proto and src/motion call with parentheses, declare as classes or
     define as singletons, without the C++ keywords and the common standard library names. Matching is
     by name, so a legacy name the core also has counts; a line goes with its file. --generate writes
     the whole list when there is none, and otherwise drops the lines the tree no longer has and never
     adds one, so a new reach has to be typed into the list by hand.
  6. It refuses when it finds no surface header, when it parses no core name, when the list is
     missing or empty while legacy files exist, and when it is given core libraries and reads no
     defined symbol from one of them. An empty list with no legacy file is the end state, and passes.

Quoted includes resolve as layout_gate.py resolves them; a quoted include it cannot resolve fails.
With no content files and no content objects, rules 1 and 3 have nothing to check, which the
summary says.
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
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import case_labels  # noqa: E402
import layout_gate  # noqa: E402

SURFACE = 'scripts/surface/'
SURFACE_DETAIL = 'scripts/surface/detail/'
CONTENT = 'scripts/content/'
LEGACY = 'scripts/legacy/'
LEGACY_HANDOVER = 'mangosd/LegacyScripts.cpp'
VOCABULARY = ('Platform/Define.h', 'SharedDefines.h', 'DBCEnums.h')
ALLOWED_STD = frozenset((
    'algorithm', 'array', 'bitset', 'cassert', 'climits', 'cmath', 'cstddef', 'cstdint', 'cstring',
    'deque', 'functional', 'initializer_list', 'iterator', 'limits', 'list', 'map', 'memory', 'numeric',
    'optional', 'set', 'string', 'string_view', 'tuple', 'type_traits', 'unordered_map', 'unordered_set',
    'utility', 'vector'))
ANGLE_RX = re.compile(r'\s*#\s*include\s*<([^>]+)>')

RATCHET_REL = 'src/tests/script_legacy_reach.txt'
CORE_DIRS = ('game/', 'shared/', 'proto/', 'motion/')
HEADER_EXTENSIONS = ('.h', '.hpp', '.inl')
KINDS = ('call', 'free', 'override', 'include', 'singleton', 'downcast')
NOT_NAMES = frozenset((
    'if', 'for', 'while', 'switch', 'return', 'sizeof', 'catch', 'case', 'new', 'delete', 'defined',
    'static_cast', 'dynamic_cast', 'reinterpret_cast', 'const_cast', 'operator', 'typeid', 'decltype',
    'alignof', 'noexcept', 'static_assert', 'void', 'bool', 'char', 'int', 'long', 'short', 'unsigned',
    'signed', 'float', 'double', 'int8', 'int16', 'int32', 'int64', 'uint8', 'uint16', 'uint32', 'uint64',
    'size_t', 'time_t',
    'begin', 'end', 'rbegin', 'rend', 'size', 'empty', 'clear', 'find', 'insert', 'erase', 'at', 'count',
    'front', 'back', 'push_back', 'push_front', 'pop_back', 'pop_front', 'emplace', 'emplace_back',
    'resize', 'reserve', 'swap', 'data', 'c_str', 'length', 'substr', 'append', 'compare', 'reset', 'get',
    'release', 'first', 'second', 'make_pair', 'sort', 'min', 'max', 'abs', 'fabs', 'sqrt', 'pow', 'floor',
    'ceil', 'sin', 'cos', 'atan2', 'memset', 'memcpy', 'strcmp', 'strlen', 'sprintf', 'snprintf', 'printf',
    'assert', 'rand', 'srand', 'time', 'lower_bound', 'upper_bound', 'equal_range', 'remove', 'unique'))
NAME_CALL_RX = re.compile(r'\b([A-Za-z_]\w*)\s*\(')
MEMBER_CALL_RX = re.compile(r'(?:->|\.)\s*([A-Za-z_]\w*)\s*\(')
FREE_CALL_RX = re.compile(r'(?<![\w.:>~])([A-Za-z_]\w*)\s*\(')
OVERRIDE_RX = re.compile(r'\b([A-Za-z_]\w*)\s*\([^;{}()]*\)[^;{}()]*\boverride\b')
CLASS_RX = re.compile(r'\b(?:class|struct)\s+(?:MANGOS_DLL_\w+\s+)?([A-Za-z_]\w*)\s*(?:final\s*)?[:{]')
SINGLETON_DEFINE_RX = re.compile(r'^[ \t]*#[ \t]*define[ \t]+(s[A-Z]\w*)\b', re.M)
SINGLETON_RX = re.compile(r'\b(s[A-Z]\w*)\b')
CAST_RX = re.compile(r'\(\s*(?:const\s+)?([A-Z]\w*)\s*\*\s*\)|\b(?:static|dynamic|reinterpret|const)_cast\s*<\s*(?:const\s+)?([A-Z]\w*)\s*\*')


class GateError(Exception):
    pass


def includes(path):
    """The (kind, spelling) of each #include line in the file: kind is 'quoted' or 'angle'."""
    out = []
    with open(path, encoding='utf-8', errors='replace') as fh:
        for ln in fh:
            m = layout_gate.INCLUDE_RX.match(ln)
            if m:
                out.append(('quoted', m.group(1).replace('\\', '/')))
                continue
            m = ANGLE_RX.match(ln)
            if m:
                out.append(('angle', m.group(1).strip()))
    return out


def is_surface_header(path):
    return path.startswith(SURFACE) and not path.startswith(SURFACE_DETAIL) and path.endswith('.h')


def check_includes(root):
    """The errors of rules 1, 2 and 4 and the counts the summary prints."""
    src = os.path.join(root, 'src')
    if not os.path.isdir(src):
        raise GateError('no src/ directory under %s: pass --root <the absolute repo root>' % root)
    files = layout_gate.list_files(src)
    dep = os.path.join(root, 'dep')
    dep_files = ['dep/' + x for x in layout_gate.list_files(dep)] if os.path.isdir(dep) else []
    src_lower = {x.lower(): x for x in files}
    src_index, dep_index = layout_gate.Index(files), layout_gate.Index(dep_files)
    templates = layout_gate.list_templates(src)

    def target(includer, spelling):
        how, path = layout_gate.resolve(includer, spelling, src_index, src_lower, dep_index, templates)
        return path if how == 'tree' else None

    vocabulary = set()
    for spelling in VOCABULARY:
        found = src_index.candidates(spelling)
        if len(found) != 1:
            raise GateError('the vocabulary header %s resolves to %d files under src/' % (spelling, len(found)))
        vocabulary.add(found[0])

    surface = [f for f in files if is_surface_header(f)]
    content = [f for f in files if f.startswith(CONTENT)]
    errors = []
    if not surface:
        errors.append('found no surface header under src/%s -- a gate that sees no surface passes nothing' % SURFACE)

    def rule(f, allowed, what):
        for kind, spelling in includes(os.path.join(src, f)):
            if kind == 'angle':
                if spelling not in ALLOWED_STD:
                    errors.append('src/%s: #include <%s>: %s include only the allowed standard headers'
                                  % (f, spelling, what))
                continue
            try:
                t = target(f, spelling)
            except layout_gate.GateError as e:
                errors.append(str(e))
                continue
            if t is None or not allowed(t):
                errors.append('src/%s: #include "%s" reaches %s: %s' % (f, spelling, 'src/' + t if t else 'outside src/', what))

    for f in content:
        rule(f, lambda t: is_surface_header(t) or t.startswith(CONTENT),
             'a new script includes only surface headers, its content headers and the allowed standard headers')
    for f in surface:
        rule(f, lambda t: is_surface_header(t) or t in vocabulary,
             'a surface header includes only surface headers, the vocabulary headers and the allowed standard headers')
    for f in files:
        if f.startswith(LEGACY) or f == LEGACY_HANDOVER:
            continue
        for kind, spelling in includes(os.path.join(src, f)):
            if kind != 'quoted':
                continue
            try:
                t = target(f, spelling)
            except layout_gate.GateError:
                continue
            if t and t.startswith(LEGACY):
                errors.append('src/%s: #include "%s" reaches the legacy library (src/%s); only src/%s and src/%s do'
                              % (f, spelling, t, LEGACY, LEGACY_HANDOVER))
    return errors, len(surface), len(content)


def core_vocabulary(root):
    """The names the core declares in its headers: what is called with parentheses, the classes, and
    the singleton accessors (`#define s...`)."""
    src = os.path.join(root, 'src')
    functions, classes, singletons = set(), set(), set()
    for f in layout_gate.list_files(src):
        if not f.startswith(CORE_DIRS) or not f.endswith(HEADER_EXTENSIONS):
            continue
        with open(os.path.join(src, f), encoding='utf-8', errors='replace') as fh:
            raw = fh.read()
        clean = case_labels.blank(raw)
        functions.update(m.group(1) for m in NAME_CALL_RX.finditer(clean))
        classes.update(m.group(1) for m in CLASS_RX.finditer(clean))
        singletons.update(m.group(1) for m in SINGLETON_DEFINE_RX.finditer(raw))
    functions -= NOT_NAMES
    return functions, classes, singletons


def legacy_reach(root):
    """One line per (legacy file, kind, name): how each legacy file reaches the core, and the number
    of legacy files."""
    src = os.path.join(root, 'src')
    functions, classes, singletons = core_vocabulary(root)
    if not functions or not classes or not singletons:
        raise GateError('parsed no core vocabulary from the headers under src/%s -- a ratchet that knows no '
                        'core name holds nothing' % ', src/'.join(CORE_DIRS))
    files = layout_gate.list_files(src)
    dep = os.path.join(root, 'dep')
    dep_files = ['dep/' + x for x in layout_gate.list_files(dep)] if os.path.isdir(dep) else []
    src_lower = {x.lower(): x for x in files}
    src_index, dep_index = layout_gate.Index(files), layout_gate.Index(dep_files)
    templates = layout_gate.list_templates(src)
    legacy = [f for f in files if f.startswith(LEGACY)]
    lines = set()
    for f in legacy:
        name = 'legacy/' + f[len(LEGACY):]
        with open(os.path.join(src, f), encoding='utf-8', errors='replace') as fh:
            raw = fh.read()
        clean = case_labels.blank(raw)
        found = set()
        for kind, spelling in includes(os.path.join(src, f)):
            if kind != 'quoted':
                continue
            try:
                how, t = layout_gate.resolve(f, spelling, src_index, src_lower, dep_index, templates)
            except layout_gate.GateError:
                continue
            if how == 'tree' and not t.startswith(LEGACY):
                found.add(('include', t))
        overridden = set()
        for ln in clean.splitlines():
            for m in OVERRIDE_RX.finditer(ln):
                if m.group(1) in functions:
                    found.add(('override', m.group(1)))
                    overridden.add((ln, m.group(1)))
            for m in MEMBER_CALL_RX.finditer(ln):
                if m.group(1) in functions:
                    found.add(('call', m.group(1)))
            for m in FREE_CALL_RX.finditer(ln):
                if m.group(1) in functions and (ln, m.group(1)) not in overridden:
                    found.add(('free', m.group(1)))
        found.update(('singleton', m.group(1)) for m in SINGLETON_RX.finditer(clean) if m.group(1) in singletons)
        for m in CAST_RX.finditer(clean):
            cls = m.group(1) or m.group(2)
            if cls in classes:
                found.add(('downcast', cls))
        lines.update('%s %s %s' % (name, kind, n) for kind, n in found)
    return lines, len(legacy)


def read_ratchet(path):
    if not os.path.exists(path):
        return None
    with open(path, encoding='utf-8') as fh:
        return set(ln.strip() for ln in fh if ln.strip() and not ln.startswith('#'))


def write_ratchet(path, lines):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'w', encoding='utf-8', newline='\n') as fh:
        for ln in sorted(lines):
            fh.write(ln + '\n')


def ratchet_totals(lines):
    kinds = {k: 0 for k in KINDS}
    files = set()
    for ln in lines:
        f, kind, _ = ln.split(' ', 2)
        kinds[kind] = kinds.get(kind, 0) + 1
        files.add(f)
    return '%d reach lines over %d legacy files (%s)' % (len(lines), len(files), ', '.join(
        '%s %d' % (k, kinds[k]) for k in KINDS))


def ratchet_errors(current, listed, legacy_count):
    """Rule 5: the legacy reach can only shrink."""
    errors = []
    if listed is None:
        return ['%s does not exist: run script_surface.py --generate once' % RATCHET_REL]
    if not listed and legacy_count:
        return ['%s is empty while %d legacy files exist -- a ratchet that lists nothing holds nothing'
                % (RATCHET_REL, legacy_count)]
    listed_files = set(ln.split(' ', 1)[0] for ln in listed)
    for ln in sorted(current - listed):
        if ln.split(' ', 1)[0] in listed_files:
            errors.append('legacy reach grew: %s -- reach the core through the surface instead' % ln)
        else:
            errors.append('a legacy file not on the list reaches the core: %s -- new code goes under '
                          'src/scripts/content/' % ln)
    for ln in sorted(listed - current):
        errors.append('stale ratchet line (the reach is gone): %s -- delete it, or run --generate' % ln)
    return errors


def parse_nm(text, undefined):
    """The symbols nm prints: with -u the undefined ones, with --defined-only -g the defined ones but the
    weak ones, which are inline and template instantiations every object may carry."""
    out = set()
    for ln in text.splitlines():
        parts = ln.split()
        if undefined and len(parts) >= 2 and parts[-2] == 'U':
            out.add(parts[-1])
        elif not undefined and len(parts) >= 3 and parts[-2] not in ('U', 'w', 'v', 'W', 'V'):
            out.add(parts[-1])
    return out


def parse_dumpbin_symbols(text):
    """The undefined external symbols of `dumpbin /symbols`."""
    out = set()
    for ln in text.splitlines():
        if ' UNDEF ' in ln and ' External ' in ln and '|' in ln:
            out.add(ln.split('|', 1)[1].split()[0])
    return out


def parse_dumpbin_linkermember(text):
    """The public symbols of `dumpbin /linkermember:1`."""
    out = set()
    listing = False
    for ln in text.splitlines():
        if re.match(r'\s*[0-9]+ public symbols', ln):
            listing = True
            continue
        if listing:
            m = re.match(r'\s*[0-9A-Fa-f]+ (\S+)\s*$', ln)
            if m:
                out.add(m.group(1))
            elif ln.strip():
                listing = False
    return out


def read_symbols(fmt, tool, path, undefined):
    if fmt == 'nm':
        args = [tool, '-u', path] if undefined else [tool, '--defined-only', '-g', path]
        parse = lambda text: parse_nm(text, undefined)
    else:
        args = [tool, '/symbols' if undefined else '/linkermember:1', path]
        parse = parse_dumpbin_symbols if undefined else parse_dumpbin_linkermember
    r = subprocess.run(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True, errors='replace')
    if r.returncode != 0:
        raise GateError('%s failed on %s: %s' % (' '.join(args[:-1]), path, r.stderr.strip()))
    return parse(r.stdout)


def symbol_errors(content_undefined, core_defined):
    """Rule 3: each content object's undefined symbols that a core library defines."""
    errors = []
    defined = set()
    for symbols in core_defined.values():
        defined |= symbols
    for obj in sorted(content_undefined):
        for symbol in sorted(content_undefined[obj] & defined):
            errors.append('%s refers to %s, which a core library defines: a new script calls the core only '
                          'through the surface' % (obj, symbol))
    return errors


def check(root, symbols, core_libs, content_objects, out=print, ratchet=None):
    errors, surface_count, content_count = check_includes(root)
    reach, legacy_count = legacy_reach(root)
    path = ratchet or os.path.join(root, *RATCHET_REL.split('/'))
    errors += ratchet_errors(reach, read_ratchet(path), legacy_count)
    core_defined = {}
    content_undefined = {}
    if core_libs:
        if not symbols:
            errors.append('core libraries were given but no symbol tool (--symbols nm|dumpbin <tool>)')
        else:
            fmt, tool = symbols
            for lib in core_libs:
                core_defined[lib] = read_symbols(fmt, tool, lib, undefined=False)
                if not core_defined[lib]:
                    errors.append('read no defined symbol from %s -- a symbol check that reads nothing passes nothing'
                                  % lib)
            for obj in content_objects:
                content_undefined[obj] = read_symbols(fmt, tool, obj, undefined=True)
    errors += symbol_errors(content_undefined, core_defined)
    for e in errors:
        out('CheckScriptSurface: ' + e)
    out('script_surface: %d surface headers, %d content files, %d content objects against %d core libraries '
        '(%d defined symbols); legacy: %s' % (surface_count, content_count, len(content_objects), len(core_defined),
                                             sum(len(s) for s in core_defined.values()), ratchet_totals(reach)))
    return 1 if errors else 0


def generate(root, out=print, ratchet=None):
    """Writes the ratchet: the whole reach when no list exists, otherwise the listed lines that are
    still true, so it only ever shrinks."""
    reach, legacy_count = legacy_reach(root)
    path = ratchet or os.path.join(root, *RATCHET_REL.split('/'))
    listed = read_ratchet(path)
    lines = reach if listed is None else listed & reach
    write_ratchet(path, lines)
    out('script_surface: wrote %s: %s%s' % (path, ratchet_totals(lines),
                                           '' if listed is None else ' (%d stale lines dropped, %d new ones not added)'
                                           % (len(listed - reach), len(reach - listed))))
    return 0


def self_test():
    failures = []

    def tree(files):
        root = tempfile.mkdtemp(prefix='script_surface_')
        for rel, text in files.items():
            path = os.path.join(root, *rel.split('/'))
            os.makedirs(os.path.dirname(path), exist_ok=True)
            with open(path, 'w', encoding='utf-8', newline='') as fh:
                fh.write(text)
        return root

    base = {
        'src/shared/Platform/Define.h': '#include <cstdint>\n',
        'src/game/Server/SharedDefines.h': '#include "Platform/Define.h"\n',
        'src/game/Server/DBCEnums.h': '',
        'src/game/Object/ObjectGuid.h': '#include "Platform/Define.h"\n',
        'src/game/Object/Unit.h': '#include "ObjectGuid.h"\nclass Unit\n{\n    public:\n        void Kill();\n};\n#define sUnits Units()\n',
        'src/scripts/surface/Ids.h': '#include "Platform/Define.h"\n#include <functional>\n',
        'src/scripts/surface/Units.h': '#include "surface/Ids.h"\n#include "SharedDefines.h"\n',
        'src/scripts/surface/detail/Guids.h': '#include "surface/Ids.h"\n#include "ObjectGuid.h"\n',
        'src/scripts/surface/Ids.cpp': '#include "surface/detail/Guids.h"\n',
        'src/scripts/dispatch/Dispatcher.cpp': '#include "Unit.h"\n',
        'src/scripts/legacy/system/ScriptDevMgr.h': '#include "Unit.h"\n',
        'src/scripts/legacy/include/sc_creature.h': '#include "system/ScriptDevMgr.h"\n',
        'src/mangosd/LegacyScripts.cpp': '#include "system/ScriptDevMgr.h"\n',
        'src/scripts/content/deadmines/boss.h': '#include "surface/Units.h"\n',
        'src/scripts/content/deadmines/boss.cpp': '#include "boss.h"\n#include "surface/Ids.h"\n#include <vector>\n',
    }

    def expect(label, changes, code, needle=None):
        files = dict(base)
        files.update(changes)
        files = {k: v for k, v in files.items() if v is not None}
        root = tree(files)
        lines = []
        try:
            write_ratchet(os.path.join(root, *RATCHET_REL.split('/')), legacy_reach(root)[0])
            got = check(root, None, [], [], out=lines.append)
        except GateError as e:
            got, lines = 1, [str(e)]
        finally:
            shutil.rmtree(root, ignore_errors=True)
        text = '\n'.join(lines)
        if got != code or (needle and needle not in text):
            failures.append('%s: exit %d (want %d)%s\n%s' % (label, got, code, ', missing "%s"' % needle if needle else '', text))

    expect('the clean tree passes', {}, 0)
    expect('a core header in content fails', {'src/scripts/content/deadmines/boss.cpp': '#include "Unit.h"\n'}, 1,
           'content/deadmines/boss.cpp: #include "Unit.h"')
    expect('a forbidden standard header in content fails', {'src/scripts/content/deadmines/boss.cpp': '#include <thread>\n'}, 1,
           '#include <thread>')
    expect('a surface detail header in content fails',
           {'src/scripts/content/deadmines/boss.cpp': '#include "surface/detail/Guids.h"\n'}, 1, 'surface/detail/Guids.h')
    expect('a surface header reaching ObjectGuid.h fails', {'src/scripts/surface/Units.h': '#include "ObjectGuid.h"\n'}, 1,
           'surface/Units.h: #include "ObjectGuid.h"')
    expect('a surface header reaching a legacy header fails',
           {'src/scripts/surface/Units.h': '#include "sc_creature.h"\n'}, 1, 'reaches src/scripts/legacy/include/sc_creature.h')
    expect('the dispatcher including a legacy header fails',
           {'src/scripts/dispatch/Dispatcher.cpp': '#include "system/ScriptDevMgr.h"\n'}, 1,
           'dispatch/Dispatcher.cpp: #include "system/ScriptDevMgr.h" reaches the legacy library')
    expect('no surface header refuses', {'src/scripts/surface/Ids.h': None, 'src/scripts/surface/Units.h': None,
                                         'src/scripts/surface/detail/Guids.h': None, 'src/scripts/surface/Ids.cpp': None,
                                         'src/scripts/content/deadmines/boss.h': '',
                                         'src/scripts/content/deadmines/boss.cpp': ''}, 1, 'found no surface header')
    expect('an include the tree does not have fails', {'src/scripts/content/deadmines/boss.cpp': '#include "nowhere.h"\n'}, 1,
           'names a header the tree does not have')

    ratchet_base = dict(base)
    ratchet_base.update({
        'src/game/Object/Unit.h': '#include "ObjectGuid.h"\nclass Unit\n{\n    public:\n        void Kill();\n        void Heal();\n};\n#define sUnits Units()\n',
        'src/game/Object/Creature.h': 'class Creature : public Unit\n{\n};\n',
        'src/scripts/legacy/scripts/boss_a.cpp': '#include "Unit.h"\nvoid A(Unit* u)\n{\n    u->Kill();\n    ((Creature*)u)->Kill();\n    sUnits;\n}\n',
        'src/scripts/legacy/scripts/boss_b.cpp': 'void B(Unit* u)\n{\n    u->Heal();\n}\n',
    })

    def ratchet_case(label, listed, changes, want, needle=None, gen=False):
        files = dict(ratchet_base)
        files.update(changes)
        root = tree({k: v for k, v in files.items() if v is not None})
        path = os.path.join(root, *RATCHET_REL.split('/'))
        if listed is not None:
            write_ratchet(path, listed)
        lines = []
        try:
            if gen:
                generate(root, out=lines.append)
                got = sorted(read_ratchet(path))
            else:
                got = check(root, None, [], [], out=lines.append)
        except GateError as e:
            got, lines = 1, [str(e)]
        finally:
            shutil.rmtree(root, ignore_errors=True)
        text = '\n'.join(lines)
        if got != want or (needle and needle not in text):
            failures.append('%s: got %r (want %r)%s\n%s' % (label, got, want, ', missing "%s"' % needle if needle else '', text))

    full = ['legacy/scripts/boss_a.cpp call Kill', 'legacy/scripts/boss_a.cpp downcast Creature',
            'legacy/scripts/boss_a.cpp include game/Object/Unit.h', 'legacy/scripts/boss_a.cpp singleton sUnits',
            'legacy/scripts/boss_b.cpp call Heal', 'legacy/system/ScriptDevMgr.h include game/Object/Unit.h']
    ratchet_case('the listed reach passes', full, {}, 0, 'legacy: 6 reach lines over 3 legacy files')
    ratchet_case('a grown reach fails', full, {'src/scripts/legacy/scripts/boss_b.cpp': 'void B(Unit* u)\n{\n    u->Heal();\n    u->Kill();\n}\n'},
                 1, 'legacy reach grew: legacy/scripts/boss_b.cpp call Kill')
    ratchet_case('a stale line fails', full, {'src/scripts/legacy/scripts/boss_b.cpp': 'void B()\n{\n}\n'}, 1,
                 'stale ratchet line (the reach is gone): legacy/scripts/boss_b.cpp call Heal')
    ratchet_case('a new legacy file fails', full, {'src/scripts/legacy/scripts/boss_c.cpp': 'void C(Unit* u)\n{\n    u->Kill();\n}\n'},
                 1, 'a legacy file not on the list reaches the core: legacy/scripts/boss_c.cpp call Kill')
    ratchet_case('an empty ratchet with legacy files refuses', [], {}, 1, 'is empty while 4 legacy files exist')
    ratchet_case('a missing ratchet fails', None, {}, 1, 'does not exist')
    ratchet_case('an empty ratchet with no legacy files passes', [], {'src/scripts/legacy/scripts/boss_a.cpp': None,
                                                                       'src/scripts/legacy/scripts/boss_b.cpp': None,
                                                                       'src/scripts/legacy/system/ScriptDevMgr.h': None,
                                                                       'src/scripts/legacy/include/sc_creature.h': None,
                                                                       'src/mangosd/LegacyScripts.cpp': ''}, 0)
    ratchet_case('--generate drops a stale line and never adds a new one', full + ['legacy/scripts/boss_b.cpp call Gone'],
                 {'src/scripts/legacy/scripts/boss_b.cpp': 'void B(Unit* u)\n{\n    u->Heal();\n    u->Kill();\n}\n'},
                 full, gen=True)
    ratchet_case('--generate with no list writes the whole reach', None, {}, sorted(full), gen=True)

    nm_u = '                 U _ZN4Unit4KillEv\n                 U _Znwm\n'
    nm_defined = ('0000000000000010 T _ZN4Unit4KillEv\n0000000000000000 B g_counter\n                 U memcpy\n'
                  '0000000000000000 W _ZNSt6vectorIiSaIiEE9push_backERKi\n')
    if parse_nm(nm_u, True) != {'_ZN4Unit4KillEv', '_Znwm'}:
        failures.append('nm -u parse: %r' % parse_nm(nm_u, True))
    if parse_nm(nm_defined, False) != {'_ZN4Unit4KillEv', 'g_counter'}:
        failures.append('nm --defined-only parse: %r' % parse_nm(nm_defined, False))
    dumpbin_symbols = ('008 00000000 UNDEF  notype ()    External     | ?Kill@Unit@@QEAAXXZ (public: void __cdecl Unit::Kill(void))\n'
                       '009 00000000 SECT3  notype ()    External     | ?Boss@@YAXXZ (void __cdecl Boss(void))\n')
    if parse_dumpbin_symbols(dumpbin_symbols) != {'?Kill@Unit@@QEAAXXZ'}:
        failures.append('dumpbin /symbols parse: %r' % parse_dumpbin_symbols(dumpbin_symbols))
    dumpbin_lib = ('Archive member name at 8: /\n\n         2 public symbols\n\n      1A3C ?Kill@Unit@@QEAAXXZ\n'
                   '      1A3C ?Heal@Unit@@QEAAXXZ\n\n  Archive member name at 1A3C: Unit.obj/\n')
    if parse_dumpbin_linkermember(dumpbin_lib) != {'?Kill@Unit@@QEAAXXZ', '?Heal@Unit@@QEAAXXZ'}:
        failures.append('dumpbin /linkermember:1 parse: %r' % parse_dumpbin_linkermember(dumpbin_lib))

    core = {'game': {'_ZN4Unit4KillEv'}}
    if symbol_errors({'boss.o': {'_Znwm', 'memcpy'}}, core):
        failures.append('a standard-library-only script failed the symbol check')
    found = symbol_errors({'boss.o': {'_Znwm', '_ZN4Unit4KillEv'}}, core)
    if len(found) != 1 or '_ZN4Unit4KillEv' not in found[0] or 'boss.o' not in found[0]:
        failures.append('a hand-declared core call was not named: %r' % found)

    for f in failures:
        print('script_surface self-test FAILED: ' + f)
    if not failures:
        print('script_surface self-test: all cases pass')
    return 1 if failures else 0


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__[__doc__.index('CheckScriptSurface:'):],
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--root', default=os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..')))
    ap.add_argument('--symbols', nargs=2, metavar=('FORMAT', 'TOOL'), help='nm or dumpbin, and the tool to run')
    ap.add_argument('--core-lib', action='append', default=[], help='a core library whose defined symbols a script may not use')
    ap.add_argument('--content-object', action='append', default=[], help='an object file of a new script')
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument('--check', action='store_true')
    g.add_argument('--generate', action='store_true', help='write the legacy ratchet: only ever shrinks it')
    g.add_argument('--self-test', action='store_true')
    args = ap.parse_args(argv[1:])
    if args.self_test:
        return self_test()
    if args.generate:
        try:
            return generate(os.path.abspath(args.root))
        except (GateError, layout_gate.GateError) as e:
            print('CheckScriptSurface: ' + str(e))
            return 1
    if args.symbols and args.symbols[0] not in ('nm', 'dumpbin'):
        ap.error('--symbols takes nm or dumpbin')
    try:
        return check(os.path.abspath(args.root), args.symbols, args.core_lib, args.content_object)
    except (GateError, layout_gate.GateError) as e:
        print('CheckScriptSurface: ' + str(e))
        return 1


if __name__ == '__main__':
    sys.exit(main(sys.argv))
