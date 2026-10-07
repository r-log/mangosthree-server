#!/usr/bin/env python3
"""split_gate.py: the split between a proof tool and the data file beside it.

A proof tool holds code only; the entries a move adds live in the tool's data file, a Python file in
this directory that the move edits and the tool never does:

  verbatim.py          verbatim_sites.py   BASE, ORIGINAL, VOID_SUBSTITUTIONS, SITES
  handler_verbatim.py  handler_moves.py    MOVES, RESIDUES
  cast_verbatim.py     cast_sites.py       BASE, FORMS, FILES

problems() reads the two files' source and names each break of the split:
  - in the data file: a statement other than an assignment to plain names (the module docstring
    aside: an import, a def, a class, a call), a name that is neither a data name nor a spelling aid
    of the file's own (a name beginning with `_`, which the tool never reads), and a data name the
    file does not assign;
  - in the tool file: a data name bound anywhere (an assignment, a def or a class, a parameter, an
    import, a `global`) other than by the `from <data module> import ...` that reads them; that import
    naming anything but data names (a `*`, a spelling aid, a name read under another name); the data
    module imported whole; and no such import at all.
check() runs problems() on a tool's own file and the data file beside it, also refusing a data module
loaded from anywhere else, and prints each problem as `<tool>: REFUSED: <problem>`. Each tool calls
it before any mode runs and stops with 1 when it prints one.
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

import ast
import os
import sys


def bound_names(node):
    """(name, line) of each name `node` binds, imports aside."""
    if isinstance(node, ast.Name) and isinstance(node.ctx, (ast.Store, ast.Del)):
        return [(node.id, node.lineno)]
    if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef, ast.ClassDef)):
        return [(node.name, node.lineno)]
    if isinstance(node, ast.arg):
        return [(node.arg, node.lineno)]
    if isinstance(node, (ast.Global, ast.Nonlocal)):
        return [(name, node.lineno) for name in node.names]
    if isinstance(node, ast.ExceptHandler) and node.name:
        return [(node.name, node.lineno)]
    return []


def data_problems(data_module, data_source, names):
    """Each break of the split in the data file: a statement or a name it may not hold, a name it lacks."""
    data_file = data_module + '.py'
    found = []
    assigned = set()
    for i, stmt in enumerate(ast.parse(data_source, data_file).body):
        if i == 0 and isinstance(stmt, ast.Expr) and isinstance(stmt.value, ast.Constant) \
                and isinstance(stmt.value.value, str):
            continue
        if not isinstance(stmt, ast.Assign) or not all(isinstance(t, ast.Name) for t in stmt.targets):
            found.append('%s:%d: a %s statement: a data file holds assignments to plain names only'
                         % (data_file, stmt.lineno, type(stmt).__name__))
            continue
        for target in stmt.targets:
            if target.id in names:
                assigned.add(target.id)
            elif not target.id.startswith('_'):
                found.append('%s:%d: %s is neither a data name (%s) nor a spelling aid of the file\'s own '
                             '(a name beginning with `_`)' % (data_file, stmt.lineno, target.id, ', '.join(names)))
    found += ['%s does not assign %s' % (data_file, name) for name in names if name not in assigned]
    return found


def tool_problems(tool_name, tool_source, data_module, names):
    """Each break of the split in the tool file: a data name bound in it, its data read any other way."""
    data_file = data_module + '.py'
    found = []
    reads = False
    for node in ast.walk(ast.parse(tool_source, tool_name)):
        if isinstance(node, ast.ImportFrom) and node.module == data_module and not node.level:
            reads = True
            for alias in node.names:
                if alias.name not in names or (alias.asname and alias.asname != alias.name):
                    found.append('%s:%d: imports %s%s from %s, which is not a data name read as itself'
                                 % (tool_name, node.lineno, alias.name,
                                    ' as ' + alias.asname if alias.asname else '', data_module))
            continue
        if isinstance(node, (ast.Import, ast.ImportFrom)):
            for alias in node.names:
                if isinstance(node, ast.Import) and alias.name == data_module:
                    found.append('%s:%d: imports %s whole: a tool reads the data names only, with '
                                 '`from %s import ...`' % (tool_name, node.lineno, data_module, data_module))
                elif (alias.asname or alias.name.split('.')[0]) in names:
                    found.append('%s:%d: binds %s, a data name of %s: the data lives there, never in the tool'
                                 % (tool_name, node.lineno, alias.asname or alias.name, data_file))
            continue
        for name, line in bound_names(node):
            if name in names:
                found.append('%s:%d: binds %s, a data name of %s: the data lives there, never in the tool'
                             % (tool_name, line, name, data_file))
    if not reads:
        found.append('%s does not read its data with `from %s import ...`' % (tool_name, data_module))
    return found


def problems(tool_name, tool_source, data_module, data_source, names):
    """Each break of the split between the tool `tool_name` and its data file, data file first."""
    return data_problems(data_module, data_source, names) + tool_problems(tool_name, tool_source, data_module,
                                                                          names)


def check(tool_file, data_module, names, out=print):
    """problems() on the tool `tool_file` and the data file beside it: 0, or 1 having printed each."""
    tool_path = os.path.abspath(tool_file)
    tool_name = os.path.basename(tool_path)
    data_path = os.path.join(os.path.dirname(tool_path), data_module + '.py')
    with open(tool_path, encoding='utf-8') as f:
        tool_source = f.read()
    with open(data_path, encoding='utf-8') as f:
        data_source = f.read()
    found = problems(tool_name, tool_source, data_module, data_source, names)
    loaded = getattr(sys.modules.get(data_module), '__file__', None)
    if not loaded or os.path.normcase(os.path.abspath(loaded)) != os.path.normcase(data_path):
        found.append('%s was loaded from %s, not from %s beside the tool' % (data_module, loaded, data_path))
    for problem in found:
        out('%s: REFUSED: %s' % (os.path.splitext(tool_name)[0], problem))
    return 1 if found else 0


def self_test(tool_name, data_module, names, bound):
    """The gate on fixtures shaped as `tool_name` and its data file: the split passes, and `bound`, a data
    name, bound in the tool, a stray name in the data, an import in the data and the data module
    imported whole are each refused by name. The failures, as text."""
    tool = ('from %s import %s\n\n\ndef check(given=None):\n    return %s if given is None else given\n'
            % (data_module, ', '.join(names), bound))
    data = '"""The data."""\n_AID = 1\n' + ''.join('%s = _AID\n' % name for name in names)
    cases = [('the split', tool, data, None),
             ('%s bound in the tool' % bound, tool + '%s = None\n' % bound, data,
              'binds %s, a data name of %s.py' % (bound, data_module)),
             ('a stray name in the data', tool, data + 'STRAY = 1\n', 'STRAY is neither a data name'),
             ('an import in the data', tool, data + 'import os\n', 'a Import statement'),
             ('the data module imported whole', 'import %s\n' % data_module + tool, data,
              'imports %s whole' % data_module)]
    failures = []
    for label, tool_source, data_source, needle in cases:
        got = problems(tool_name, tool_source, data_module, data_source, names)
        ok = not got if needle is None else any(needle in problem for problem in got)
        if not ok:
            failures.append('split gate, %s: %s' % (label, got or 'no problem named'))
    return failures
