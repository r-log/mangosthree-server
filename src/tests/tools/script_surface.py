#!/usr/bin/env python3
"""script_surface.py [--root <repo root>] --check [--symbols <nm|dumpbin> <tool>] [--core-lib <lib>]... [--content-object <obj>]...
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
  5. It refuses when it finds no surface header, and when it is given core libraries and reads no
     defined symbol from one of them.

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


def check(root, symbols, core_libs, content_objects, out=print):
    errors, surface_count, content_count = check_includes(root)
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
        '(%d defined symbols)' % (surface_count, content_count, len(content_objects), len(core_defined),
                                  sum(len(s) for s in core_defined.values())))
    return 1 if errors else 0


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
        'src/game/Object/Unit.h': '#include "ObjectGuid.h"\n',
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
    g.add_argument('--self-test', action='store_true')
    args = ap.parse_args(argv[1:])
    if args.self_test:
        return self_test()
    if args.symbols and args.symbols[0] not in ('nm', 'dumpbin'):
        ap.error('--symbols takes nm or dumpbin')
    try:
        return check(os.path.abspath(args.root), args.symbols, args.core_lib, args.content_object)
    except (GateError, layout_gate.GateError) as e:
        print('CheckScriptSurface: ' + str(e))
        return 1


if __name__ == '__main__':
    sys.exit(main(sys.argv))
