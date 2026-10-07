#!/usr/bin/env python3
"""split_gate.py: the split between a proof tool and the data file beside it.

A proof tool holds code only; the entries a move adds live in the tool's data file, a Python file in
this directory that the move edits and the tool never does:

  verbatim.py          verbatim_sites.py   BASE, ORIGINAL, VOID_SUBSTITUTIONS, SITES
  handler_verbatim.py  handler_moves.py    MOVES, RESIDUES
  cast_verbatim.py     cast_sites.py       BASE, FORMS, FILES

problems() reads the two files' source and names each break of the split:
  - in the data file: a statement other than an assignment to plain names (the module docstring, its
    first statement, aside: an import, a def, a class, a call); a name that is neither a data name
    nor a spelling aid of the file's own (a name beginning with `_`, which the tool never reads); a
    data name the file does not assign; and in an assigned value anything but constants, lists,
    tuples, dicts, `+` and `*`, a name the file assigned in an earlier statement, and `dict(...)`
    with keyword arguments only (so a value is a literal: no call, no import, no lambda, no walrus,
    nothing read from the environment or another file);
  - in the tool file: a data name bound anywhere (an assignment, a def or a class, a parameter, an
    import, a `global`, a `match` capture) other than by the `from <data module> import ...` that reads
    them; a data name changed without binding it (an item or attribute stored or deleted on it, a
    method called on it); that import naming anything but data names (a `*`, a spelling aid, a name
    read under another name); the data module imported whole; no such import; and a `main()` whose
    first statement is not `if split_gate.check(__file__, '<data module>', DATA_NAMES, globals()):`
    returning 1.
check() runs problems() on a tool's own file and the data file beside it (read as UTF-8, a byte order
mark allowed), refuses a missing data file and a data module loaded from anywhere else, evaluates the
data file's assignments by the value rule above (nothing in it runs) and refuses a tool whose data
names, read from its globals, are not equal to those values: data replaced, emptied or changed by the
tool's module-level code, or loaded from another file, fails there. It prints each problem as
`<tool>: REFUSED: <problem>`. Each tool calls it as the first statement of main(), before any mode
runs, and stops with 1 when it prints one. A tool imports its data inside `try: ... except Exception
as e: sys.exit(split_gate.unloaded(__file__, '<data module>', e))`, so a data file that does not import,
or raises while it runs, is refused by name too, with no traceback: unloaded() names the data file's line
that raised and the error. Not seen:
a change made after check() through another name bound to a data value (`x = SITES; x.clear()`);
that is a review item.
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
import shutil
import subprocess
import sys
import tempfile
import traceback
import types

VALUE_RULE = ('a value holds only constants, lists, tuples, dicts, + and *, names assigned above it and '
              'dict(...) with keyword arguments only')


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
    if isinstance(node, (ast.MatchAs, ast.MatchStar)) and node.name:
        return [(node.name, node.lineno)]
    if isinstance(node, ast.MatchMapping) and node.rest:
        return [(node.rest, node.lineno)]
    return []


def root_name(node):
    """The name a chain of subscripts and attributes starts at, or None."""
    while isinstance(node, (ast.Subscript, ast.Attribute)):
        node = node.value
    return node.id if isinstance(node, ast.Name) else None


def changed_names(node):
    """(name, line, how) of each name `node` changes without binding it."""
    if isinstance(node, (ast.Subscript, ast.Attribute)) and isinstance(node.ctx, (ast.Store, ast.Del)):
        return [(root_name(node), node.lineno, 'stores or deletes an item or attribute of')]
    if isinstance(node, ast.Call) and isinstance(node.func, ast.Attribute):
        return [(root_name(node.func.value), node.lineno, 'calls a method of')]
    return []


def is_dict_call(node):
    """Whether `node` is `dict(...)` with keyword arguments only."""
    return (isinstance(node, ast.Call) and isinstance(node.func, ast.Name) and node.func.id == 'dict'
            and not node.args and all(k.arg is not None for k in node.keywords))


def value_problems(node, known):
    """(line, text) of each part of an assigned value outside the value rule."""
    if isinstance(node, ast.Constant):
        return []
    if isinstance(node, (ast.List, ast.Tuple)):
        return [p for e in node.elts for p in value_problems(e, known)]
    if isinstance(node, ast.Dict) and all(k is not None for k in node.keys):
        return [p for kv in zip(node.keys, node.values) for n in kv for p in value_problems(n, known)]
    if isinstance(node, ast.BinOp) and isinstance(node.op, (ast.Add, ast.Mult)):
        return value_problems(node.left, known) + value_problems(node.right, known)
    if isinstance(node, ast.Name) and node.id in known:
        return []
    if is_dict_call(node):
        return [p for k in node.keywords for p in value_problems(k.value, known)]
    if isinstance(node, ast.Name):
        return [(node.lineno, '%s, a name not assigned above it' % node.id)]
    return [(node.lineno, 'a %s (%s)' % (type(node).__name__, ast.unparse(node)[:60]))]


def evaluate(node, values):
    """The value of `node`, which value_problems() passed."""
    if isinstance(node, ast.Constant):
        return node.value
    if isinstance(node, ast.List):
        return [evaluate(e, values) for e in node.elts]
    if isinstance(node, ast.Tuple):
        return tuple(evaluate(e, values) for e in node.elts)
    if isinstance(node, ast.Dict):
        return {evaluate(k, values): evaluate(v, values) for k, v in zip(node.keys, node.values)}
    if isinstance(node, ast.BinOp):
        left, right = evaluate(node.left, values), evaluate(node.right, values)
        return left + right if isinstance(node.op, ast.Add) else left * right
    if isinstance(node, ast.Name):
        return values[node.id]
    return {k.arg: evaluate(k.value, values) for k in node.keywords}


def data_values(data_module, data_source, names):
    """(problems, values): each break of the split in the data file, and its assignments evaluated when
    there is none."""
    data_file = data_module + '.py'
    found = []
    try:
        tree = ast.parse(data_source, data_file)
    except SyntaxError as e:
        return ['%s does not parse: %s' % (data_file, e)], {}
    assigned = set()
    for i, stmt in enumerate(tree.body):
        if i == 0 and isinstance(stmt, ast.Expr) and isinstance(stmt.value, ast.Constant) \
                and isinstance(stmt.value.value, str):
            continue
        if not isinstance(stmt, ast.Assign) or not all(isinstance(t, ast.Name) for t in stmt.targets):
            found.append('%s:%d: a %s statement: a data file holds assignments to plain names only'
                         % (data_file, stmt.lineno, type(stmt).__name__))
            continue
        for line, what in value_problems(stmt.value, assigned):
            found.append('%s:%d: %s in a value: %s' % (data_file, line, what, VALUE_RULE))
        for target in stmt.targets:
            assigned.add(target.id)
            if target.id not in names and not target.id.startswith('_'):
                found.append('%s:%d: %s is neither a data name (%s) nor a spelling aid of the file\'s own '
                             '(a name beginning with `_`)' % (data_file, stmt.lineno, target.id, ', '.join(names)))
    found += ['%s does not assign %s' % (data_file, name) for name in names if name not in assigned]
    values = {}
    if not found:
        for stmt in tree.body[1:] if tree.body and isinstance(tree.body[0], ast.Expr) else tree.body:
            value = evaluate(stmt.value, values)
            for target in stmt.targets:
                values[target.id] = value
    return found, values


def data_problems(data_module, data_source, names):
    """Each break of the split in the data file."""
    return data_values(data_module, data_source, names)[0]


def gate_call(stmt, data_module):
    """Whether `stmt` is `if split_gate.check(__file__, '<data_module>', DATA_NAMES, globals()): return 1`."""
    if not isinstance(stmt, ast.If) or stmt.orelse or len(stmt.body) != 1:
        return False
    call, ret = stmt.test, stmt.body[0]
    return (isinstance(call, ast.Call) and not call.keywords and len(call.args) == 4
            and ast.unparse(call.func) == 'split_gate.check'
            and ast.unparse(call.args[0]) == '__file__'
            and isinstance(call.args[1], ast.Constant) and call.args[1].value == data_module
            and ast.unparse(call.args[2]) == 'DATA_NAMES' and ast.unparse(call.args[3]) == 'globals()'
            and isinstance(ret, ast.Return) and isinstance(ret.value, ast.Constant) and ret.value.value == 1)


def tool_problems(tool_name, tool_source, data_module, names):
    """Each break of the split in the tool file: a data name bound or changed in it, its data read any other
    way, the gate not run first in its main()."""
    data_file = data_module + '.py'
    found = []
    reads = False
    tree = ast.parse(tool_source, tool_name)
    for node in ast.walk(tree):
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
        for name, line, how in changed_names(node):
            if name in names:
                found.append('%s:%d: %s %s, a data name of %s: the tool reads its data, never changes it'
                             % (tool_name, line, how, name, data_file))
    if not reads:
        found.append('%s does not read its data with `from %s import ...`' % (tool_name, data_module))
    mains = [s for s in tree.body if isinstance(s, ast.FunctionDef) and s.name == 'main']
    body = mains[0].body if mains else []
    if body and isinstance(body[0], ast.Expr) and isinstance(body[0].value, ast.Constant):
        body = body[1:]
    if len(mains) != 1 or not body or not gate_call(body[0], data_module):
        found.append("%s: main() does not run `if split_gate.check(__file__, '%s', DATA_NAMES, globals()): "
                     "return 1` as its first statement" % (tool_name, data_module))
    return found


def problems(tool_name, tool_source, data_module, data_source, names):
    """Each break of the split between the tool `tool_name` and its data file, data file first."""
    return data_problems(data_module, data_source, names) + tool_problems(tool_name, tool_source, data_module,
                                                                          names)


def check(tool_file, data_module, names, tool_values, out=print):
    """problems() on the tool `tool_file` and the data file beside it, and the tool's data names (its
    globals, `tool_values`) against the data file's values: 0, or 1 having printed each problem."""
    tool_path = os.path.abspath(tool_file)
    tool_name = os.path.basename(tool_path)
    data_path = os.path.join(os.path.dirname(tool_path), data_module + '.py')
    with open(tool_path, encoding='utf-8-sig') as f:
        tool_source = f.read()
    try:
        with open(data_path, encoding='utf-8-sig') as f:
            data_source = f.read()
    except (OSError, UnicodeDecodeError) as e:
        found = ['no readable data file %s beside the tool (%s)' % (data_path, e)]
    else:
        found, values = data_values(data_module, data_source, names)
        found += tool_problems(tool_name, tool_source, data_module, names)
        loaded = getattr(sys.modules.get(data_module), '__file__', None)
        if not loaded or os.path.normcase(os.path.abspath(loaded)) != os.path.normcase(data_path):
            found.append('%s was loaded from %s, not from %s beside the tool' % (data_module, loaded, data_path))
        for name in names if values else ():
            if name not in tool_values:
                found.append('%s runs without %s: %s did not load' % (tool_name, name, data_module))
            elif tool_values[name] != values[name]:
                found.append('%s runs with a %s other than the one %s assigns' % (tool_name, name, data_path))
    for problem in found:
        out('%s: REFUSED: %s' % (os.path.splitext(tool_name)[0], problem))
    return 1 if found else 0


def unloaded(tool_file, data_module, error, out=print):
    """1, having printed that the data module of the tool `tool_file` does not import: `error`, led by the
    data file's line and the error's type when the data file raised it while it ran."""
    data_file = data_module + '.py'
    lines = [f.lineno for f in traceback.extract_tb(error.__traceback__) if os.path.basename(f.filename) == data_file]
    what = '%s:%d: %s: %s' % (data_file, lines[-1], type(error).__name__, error) if lines else error
    out('%s: REFUSED: %s does not import from beside the tool: %s'
        % (os.path.splitext(os.path.basename(tool_file))[0], data_module, what))
    return 1


FIXTURE_TOOL = '''import split_gate
from {data} import {names}

DATA_NAMES = {tuple!r}


def check(given=None):
    return {bound} if given is None else given


def main(argv):
    if split_gate.check(__file__, '{data}', DATA_NAMES, globals()):
        return 1
    return check()
'''


def self_test(tool_file, data_module, names, bound):
    """The gate on fixtures shaped as the tool `tool_file` and its data file, `bound` one of the data names,
    on the tool's own source and on the tool run without its data file and with one that raises: [(row label,
    the row's failures as text)]."""
    tool_name = os.path.basename(tool_file)
    tool = FIXTURE_TOOL.format(data=data_module, names=', '.join(names), tuple=tuple(names), bound=bound)
    data = ('"""The data."""\n_AID = [(\'a\', \'b\')] + [1] * 2\n_DICT = dict(key=_AID, other={\'x\': (1, None)})\n'
            + ''.join('%s = _DICT\n' % name for name in names))
    main_gate = "    if split_gate.check(__file__, '%s', DATA_NAMES, globals()):\n        return 1\n" % data_module

    def source(label, tool_source, data_source, needle):
        try:
            got = problems(tool_name, tool_source, data_module, data_source, names)
        except Exception as e:                                  # a crash fails the row
            return ['split gate, %s: crashed: %r' % (label, e)]
        ok = not got if needle is None else any(needle in problem for problem in got)
        return [] if ok else ['split gate, %s: %s' % (label, got or 'no problem named')]

    def checked(label, data_source, values, needle, want_rc, loaded=True):
        """check() on a directory holding the fixture tool and `data_source`, with the tool's `values`."""
        fixture = 'split_gate_fixture_' + data_module
        where = tempfile.mkdtemp()
        saved = sys.modules.get(fixture)
        try:
            tool_path = os.path.join(where, tool_name)
            with open(tool_path, 'w', encoding='utf-8') as f:
                f.write(tool.replace(data_module, fixture))
            if data_source is not None:
                with open(os.path.join(where, fixture + '.py'), 'w', encoding='utf-8') as f:
                    f.write(data_source)
            module = types.ModuleType(fixture)
            module.__file__ = os.path.join(where if loaded else tempfile.gettempdir(), fixture + '.py')
            sys.modules[fixture] = module
            got = []
            try:
                rc = check(tool_path, fixture, names, values, got.append)
            except Exception as e:                              # a crash fails the row
                rc, got = 2, ['crashed: %r' % e]
        finally:
            if saved is None:
                sys.modules.pop(fixture, None)
            else:
                sys.modules[fixture] = saved
            shutil.rmtree(where, ignore_errors=True)
        text = '\n'.join(got)
        ok = rc == want_rc and (needle is None and not got or needle is not None and needle in text)
        return [] if ok else ['split gate, %s: rc %d (want %d) %s' % (label, rc, want_rc, text or 'no problem named')]

    def unloaded_case():
        got = []
        rc = unloaded(tool_file, data_module, ImportError('no module'), got.append)
        ok = rc == 1 and got == ['%s: REFUSED: %s does not import from beside the tool: no module'
                                 % (os.path.splitext(tool_name)[0], data_module)]
        return [] if ok else ['split gate, a data module that does not import: rc %d %s' % (rc, got)]

    def run_copy(data_text):
        """The tool run from a copy of its directory, its data file holding `data_text` (None: no data file)."""
        here = os.path.dirname(os.path.abspath(tool_file))
        where = tempfile.mkdtemp()
        try:
            for name in os.listdir(here):
                if name.endswith('.py') and name != data_module + '.py':
                    shutil.copy(os.path.join(here, name), where)
            if data_text is not None:
                with open(os.path.join(where, data_module + '.py'), 'w', encoding='utf-8') as f:
                    f.write(data_text)
            return subprocess.run([sys.executable, '-E', '-B', os.path.join(where, tool_name)], cwd=where,
                                  capture_output=True, text=True)
        finally:
            shutil.rmtree(where, ignore_errors=True)

    refused = '%s: REFUSED: %s does not import from beside the tool: ' % (os.path.splitext(tool_name)[0], data_module)

    def refused_by_name(what, data_text, want):
        """The tool run by run_copy(`data_text`) stops with 1, its output beginning with `want`, no traceback."""
        try:
            run = run_copy(data_text)
        except Exception as e:                                  # a crash fails the row
            return ['split gate, %s %s: crashed: %r' % (tool_name, what, e)]
        ok = run.returncode == 1 and run.stdout.startswith(want) and 'Traceback' not in run.stderr
        return [] if ok else ['split gate, %s %s: rc %d, stdout %r, stderr %r'
                              % (tool_name, what, run.returncode, run.stdout[-300:], run.stderr[-300:])]

    def missing_data_case():
        """The tool run from a copy of its directory without its data file."""
        return refused_by_name('with no data file', None, refused)

    def raising_data_case():
        """The tool run with its data file ending in a line that raises as it runs: refused at that line."""
        try:
            with open(os.path.join(os.path.dirname(os.path.abspath(tool_file)), data_module + '.py'),
                      encoding='utf-8-sig') as f:
                own = f.read().rstrip('\n') + '\n'
        except Exception as e:                                  # a crash fails the row
            return ['split gate, %s with a raising data file: crashed: %r' % (tool_name, e)]
        at = '%s%s.py:%d: ' % (refused, data_module, own.count('\n') + 1)
        return [p for line, error in (('_X = 1 / 0', 'ZeroDivisionError'), ('_X = UNSET', 'NameError'),
                                      ("_X = [1] * 'a'", 'TypeError'))
                for p in refused_by_name('with %r in its data' % line, own + line + '\n', at + error + ': ')]

    try:
        evaluated = data_values(data_module, data, names)[1]
    except Exception:                                           # the rows reading it fail
        evaluated = {}
    rows = []
    rows.append(('the split: %s bound in the tool, a stray data name: REFUSED' % bound, (
        source('the split', tool, data, None)
        + source('%s bound in the tool' % bound, tool + '%s = None\n' % bound, data,
                 'binds %s, a data name of %s.py' % (bound, data_module))
        + source('a stray name in the data', tool, data + 'STRAY = 1\n', 'STRAY is neither a data name')
        + source('an import in the data', tool, data + 'import os\n', 'a Import statement')
        + source('the data module imported whole', 'import %s\n' % data_module + tool, data,
                 'imports %s whole' % data_module))))
    not_literal = [("_X = __import__('re')\n", "a Call (__import__('re'))"),
                   ("%s = __import__('other_data').%s\n" % (bound, bound), "a Attribute (__import__('other_data')"),
                   ("_X = exec('STRAY = 1')\n", 'a Call (exec('),
                   ('_X = lambda: 1\n', 'a Lambda'),
                   ('_X = (STRAY := 1)\n', 'a NamedExpr'),
                   ('_X = 1 if _AID else 2\n', 'a IfExp'),
                   ('_X = UNSET\n', 'UNSET, a name not assigned above it'),
                   ('_X = _LATER\n_LATER = 1\n', '_LATER, a name not assigned above it'),
                   ('_X = dict(_AID)\n', 'a Call (dict(_AID))'),
                   ('_X = dict(**_DICT)\n', 'a Call (dict(**_DICT))'),
                   ('_X = {**_DICT}\n', 'a Dict'),
                   ('_X = _AID[0]\n', 'a Subscript'),
                   ('_X = _AID - _AID\n', 'a BinOp'),
                   ("_X = f'{_AID}'\n", 'a JoinedStr')]
    rows.append(('the split: a data value that is not a literal: REFUSED', [
        p for text, needle in not_literal for p in source('the value %r' % text, tool, data + text, needle)]))
    changes = [('%s[:] = []\n', 'stores or deletes an item or attribute of'),
               ("del %s['key']\n", 'stores or deletes an item or attribute of'),
               ('%s.attr = 1\n', 'stores or deletes an item or attribute of'),
               ('%s.clear()\n', 'calls a method of'),
               ("%s['key'].update({})\n", 'calls a method of'),
               ('match {}:\n    case %s:\n        pass\n', 'binds %s' % bound),
               ('match []:\n    case [*%s]:\n        pass\n', 'binds %s' % bound),
               ('match {}:\n    case {**%s}:\n        pass\n', 'binds %s' % bound),
               ('def f():\n    global %s\n', 'binds %s' % bound),
               ('_x = (%s := 1)\n', 'binds %s' % bound)]
    rows.append(('the split: %s changed in the tool, data replaced: REFUSED' % bound, (
        [p for text, needle in changes
         for p in source('the tool line %r' % text, tool + text.replace('%s', bound), data, needle)]
        + checked('check() on the split', data, evaluated, None, 0)
        + checked('check() on a tool running with other data', data, dict(evaluated, **{bound: {}}),
                  'runs with a %s other than the one' % bound, 1)
        + checked('check() on a tool running without its data', data,
                  {k: v for k, v in evaluated.items() if k != bound}, 'runs without %s' % bound, 1))))
    others = [('a missing data name', tool, data.replace('%s = _DICT\n' % bound, ''),
               'does not assign %s' % bound),
              ('a spelling aid imported', tool.replace('import %s' % ', '.join(names),
                                                       'import %s, _AID' % ', '.join(names)),
               data, 'imports _AID from'),
              ('a data name imported renamed', tool.replace('import %s' % ', '.join(names),
                                                            'import %s as RENAMED' % ', '.join(names)),
               data, 'as RENAMED from'),
              ('no import of the data', tool.replace('from %s import' % data_module, 'from os import'), data,
               'does not read its data'),
              ('a def named like data', tool + 'def %s():\n    pass\n' % bound, data, 'binds %s' % bound),
              ('a class named like data', tool + 'class %s:\n    pass\n' % bound, data, 'binds %s' % bound),
              ('a parameter named like data', tool + 'def f(%s=None):\n    return 1\n' % bound, data,
               'binds %s' % bound),
              ('a string statement after the first', tool, data + '"""More."""\n', 'a Expr statement'),
              ('main() without the gate', tool.replace(main_gate, ''), data, 'main() does not run'),
              ('main() with the gate second', tool.replace(main_gate, '    argv = argv\n' + main_gate), data,
               'main() does not run'),
              ('no main()', tool.replace('def main(argv):', 'def start(argv):'), data, 'main() does not run')]
    rows.append(('the split: each other rule of the gate: REFUSED by name', (
        [p for label, t, d, needle in others for p in source(label, t, d, needle)]
        + checked('check() with the data loaded from elsewhere', data, evaluated, 'was loaded from', 1, loaded=False)
        + checked('check() with no data file', None, evaluated, 'no readable data file', 1)
        + checked('check() on a data file with a byte order mark', '\ufeff' + data, evaluated, None, 0)
        + checked('check() on a data file that does not parse', data + 'X =\n', evaluated, 'does not parse', 1)
        + unloaded_case())))
    other_gate = tool.replace("split_gate.check(__file__, '%s'" % data_module,
                              "split_gate.check(__file__, 'other_data'")
    rows.append(('the split: main() gating another data module: REFUSED', (
        source('the gate run on other_data', other_gate, data, 'main() does not run')
        + ([] if other_gate != tool else ['split gate, the gate run on other_data: the mutation matches nothing']))))
    rows.append(('the split: this tool with no data file beside it: REFUSED by name', missing_data_case()))
    rows.append(('the split: this tool with a data file that raises: REFUSED by name', raising_data_case()))
    try:
        with open(os.path.abspath(tool_file), encoding='utf-8-sig') as f:
            own = tool_problems(tool_name, f.read(), data_module, names)
    except Exception as e:                                      # a crash fails the row
        own = ['crashed: %r' % e]
    rows.append(("the split: this tool's main() runs the gate first",
                 ['split gate, %s: %s' % (tool_name, p) for p in own]))
    return rows
