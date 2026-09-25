#!/usr/bin/env python3
"""sync_db_sites.py <src-root>

The synchronous-database-call inventory for decoupling D7. It walks every *.cpp and *.h
file under <src-root>, strips comments, and prints one line per call to a blocking
Database entry point on one of the three global databases:

  path:line  <db>.<api>  <enclosing function>  <hint>

The enclosing function is found by a brace-aware scan: the file's blocks are tracked as
they open and close, and the site is attributed to the outermost enclosing block whose
header reads as a function definition (`Class::Name(...)` or `Name(...)`), so a lambda body
or an `if` block inside a function is still attributed to that function.

The hint is a shape, NOT a verdict:

  handler-named   the function name matches Handle\\w+Opcode$
  command-named   it matches Handle\\w+Command$
  startup-named   it matches ^(Load\\w*|Initialize|Init\\w*|CleanupInstances|PackInstances|
                  CheckDatabaseVersion)$
  other           anything else

Decoupling D7i split command-named out of handler-named. A chat command's handler and an
opcode's handler are not the same kind of site: a command is dispatched by
ChatHandler::ExecuteCommand, which opens a TickGuard::AdminScope when the command's
required security is above SEC_PLAYER, so a blocking call inside one is
administrative work counted apart rather than a tick violation. An opcode handler has no
such cover. The hint cannot READ that security level (it lives in the hardcoded tables and
in the `command` table), so this is still only a shape -- and plenty of administrative
sites sit in helpers (HandleBanListHelper, LookupPlayerSearchCommand,
GetDeletedCharacterInfoList) that match nothing and stay "other".

A startup-named function may still be called from the tick, and a handler-named one may
run on a login thread. Which sites actually cost the tick is what the runtime counter
(TickGuard, `.server database`) measures; a human decides what to convert.

python3 src/tests/tools/sync_db_sites.py src/game
python3 src/tests/tools/sync_db_sites.py --self-test
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
import bisect
import os
import re
import sys

# The same set the static gate (src/tests/CheckSyncDb.cmake) keys on, and the same set the
# runtime counter sits behind. `P?Query` before `QueryNamed` is safe: the alternation
# backtracks when the trailing `\s*\(` does not follow.
SITE_RE = re.compile(
    r"(CharacterDatabase|WorldDatabase|LoginDatabase)"
    r"\.(P?Query|QueryNamed|PQueryNamed|DirectExecute|DirectPExecute|DirectExecuteStmt"
    r"|Ping|CommitTransactionChecked|escape_string)\s*\("
)

HANDLER_RE = re.compile(r"Handle\w+Opcode$")
COMMAND_RE = re.compile(r"Handle\w+Command$")
STARTUP_RE = re.compile(
    r"^(Load\w*|Initialize|Init\w*|CleanupInstances|PackInstances|CheckDatabaseVersion)$"
)

# A block header that starts with one of these is not a function definition.
NOT_A_FUNCTION = frozenset((
    "if", "for", "while", "switch", "do", "else", "catch", "try",
    "namespace", "class", "struct", "union", "enum", "extern", "return",
))

# `Class::Name(` or `Name(`, with the destructor spelling allowed.
HEADER_RE = re.compile(r"(?:(\w+)\s*::\s*)?(~?\w+)\s*\(")

APIS = [
    "Query", "PQuery", "QueryNamed", "PQueryNamed", "DirectExecute", "DirectPExecute",
    "DirectExecuteStmt", "Ping", "CommitTransactionChecked", "escape_string",
]
HINTS = ["handler-named", "command-named", "startup-named", "other"]


def strip_comments(text):
    """Remove // line comments and /* ... */ block comments, preserving line structure.

    String literals are not tracked (the same simplification downcast_count.py makes): a
    "//" inside a string literal is stripped as if it started a comment. Newlines are kept
    so an offset's line number in the stripped text is its line number in the file.
    """
    out = []
    i = 0
    n = len(text)
    while i < n:
        slash = text.find('/', i)
        if slash == -1:
            out.append(text[i:])
            break
        out.append(text[i:slash])
        nxt = text[slash + 1:slash + 2]
        if nxt == '/':
            j = text.find('\n', slash)
            if j == -1:
                i = n
            else:
                i = j
        elif nxt == '*':
            j = text.find('*/', slash + 2)
            if j == -1:
                i = n
            else:
                out.append('\n' * text.count('\n', slash, j + 2))
                i = j + 2
        else:
            out.append('/')
            i = slash + 1
    return ''.join(out)


def line_starts(text):
    """Offsets of the first character of every line, for offset -> line number lookups."""
    starts = [0]
    i = text.find('\n')
    while i != -1:
        starts.append(i + 1)
        i = text.find('\n', i + 1)
    return starts


def header_chunk(text, brace):
    """The text between the previous statement boundary and the `{` at `brace`."""
    start = 0
    for i in range(brace - 1, -1, -1):
        if text[i] in ';{}':
            start = i + 1
            break
    return ' '.join(text[start:brace].split())


def function_name(chunk):
    """`Class::Name` / `Name` if the chunk reads as a function definition header, else None.

    The FIRST `identifier(` in the chunk is the one that names it: a constructor's header
    carries its member initialisers after the parameter list, and the last `identifier(`
    there is a member, not the function.
    """
    if not chunk:
        return None
    first = chunk.split(None, 1)[0].lstrip('*&')
    if first in NOT_A_FUNCTION:
        return None
    # An initialiser (`static ChatCommand table[] = {`) is not a function definition.
    equals = chunk.find('=')
    paren = chunk.find('(')
    if paren == -1:
        return None
    if equals != -1 and equals < paren:
        return None
    match = HEADER_RE.search(chunk)
    if not match:
        return None
    if match.group(2) in NOT_A_FUNCTION:
        return None
    return "%s::%s" % (match.group(1), match.group(2)) if match.group(1) else match.group(2)


def enclosing_functions(text):
    """For every site in `text`, the enclosing function: a list of (offset, api, db, name).

    One forward pass. A stack of open braces carries each block's function name (or None);
    a site is attributed to the OUTERMOST enclosing block that names a function, so a
    lambda, an if or a try inside a definition still reports the definition.
    """
    sites = [(m.start(), m.group(1), m.group(2)) for m in SITE_RE.finditer(text)]
    if not sites:
        return []

    found = []
    stack = []
    nxt = 0
    for i, ch in enumerate(text):
        while nxt < len(sites) and sites[nxt][0] == i:
            name = next((n for n in stack if n), None)
            found.append((sites[nxt][0], sites[nxt][2], sites[nxt][1], name or "<file-scope>"))
            nxt += 1
        if ch == '{':
            stack.append(function_name(header_chunk(text, i)))
        elif ch == '}':
            if stack:
                stack.pop()
    while nxt < len(sites):
        found.append((sites[nxt][0], sites[nxt][2], sites[nxt][1], "<file-scope>"))
        nxt += 1
    return found


def hint_for(name):
    """The shape hint for an enclosing-function name. Never a verdict."""
    bare = name.rsplit("::", 1)[-1]
    if HANDLER_RE.search(bare):
        return "handler-named"
    if COMMAND_RE.search(bare):
        return "command-named"
    if STARTUP_RE.match(bare):
        return "startup-named"
    return "other"


def scan_text(text):
    """(line, db, api, function, hint) for every site in one file's text."""
    stripped = strip_comments(text)
    starts = line_starts(stripped)
    rows = []
    for offset, api, db, name in enclosing_functions(stripped):
        rows.append((bisect.bisect_right(starts, offset), db, api, name, hint_for(name)))
    return rows


def self_test():
    failures = []

    handler = (
        "void WorldSession::HandlePetitionBuyOpcode(WorldPacket& recv_data)\n"
        "{\n"
        "    QueryResult* r = CharacterDatabase.PQuery(\"SELECT 1\");\n"
        "    CharacterDatabase.escape_string(name);\n"
        "}\n"
    )
    rows = scan_text(handler)
    expected = [
        (3, "CharacterDatabase", "PQuery", "WorldSession::HandlePetitionBuyOpcode", "handler-named"),
        (4, "CharacterDatabase", "escape_string", "WorldSession::HandlePetitionBuyOpcode", "handler-named"),
    ]
    if rows != expected:
        failures.append("handler with two queries: expected %s, got %s" % (expected, rows))

    # Decoupling D7i: a chat command's handler is NOT an opcode handler. The two hints are
    # separate because ChatHandler::ExecuteCommand covers one of them with an AdminScope.
    command = (
        "bool ChatHandler::HandlePInfoCommand(char* args)\n"
        "{\n"
        "    QueryResult* r = CharacterDatabase.PQuery(\"SELECT 1\");\n"
        "}\n"
    )
    rows = scan_text(command)
    expected = [(3, "CharacterDatabase", "PQuery", "ChatHandler::HandlePInfoCommand", "command-named")]
    if rows != expected:
        failures.append("command handler: expected %s, got %s" % (expected, rows))

    # ...and a helper a command calls is neither: it matches nothing and stays "other".
    command_helper = (
        "void ChatHandler::HandleBanListHelper(QueryResult* result)\n"
        "{\n"
        "    QueryResult* r = LoginDatabase.PQuery(\"SELECT 1\");\n"
        "}\n"
    )
    rows = scan_text(command_helper)
    expected = [(3, "LoginDatabase", "PQuery", "ChatHandler::HandleBanListHelper", "other")]
    if rows != expected:
        failures.append("command helper: expected %s, got %s" % (expected, rows))

    startup = (
        "void ObjectMgr::LoadCreatures()\n"
        "{\n"
        "    QueryResult* r = WorldDatabase.Query(\"SELECT 1\");\n"
        "}\n"
    )
    rows = scan_text(startup)
    expected = [(3, "WorldDatabase", "Query", "ObjectMgr::LoadCreatures", "startup-named")]
    if rows != expected:
        failures.append("Load* function: expected %s, got %s" % (expected, rows))

    commented = (
        "void Foo::Bar()\n"
        "{\n"
        "    // LoginDatabase.PExecute(\"x\"); LoginDatabase.Query(\"y\");\n"
        "    /* WorldDatabase.Query(\"z\"); */\n"
        "}\n"
    )
    rows = scan_text(commented)
    if rows:
        failures.append("commented-out query: expected no sites, got %s" % (rows,))

    split = (
        "bool Foo::Bar()\n"
        "{\n"
        "    return LoginDatabase.DirectPExecute(\n"
        "        \"UPDATE `realmlist` SET `population` = %f\", value);\n"
        "}\n"
    )
    rows = scan_text(split)
    expected = [(3, "LoginDatabase", "DirectPExecute", "Foo::Bar", "other")]
    if rows != expected:
        failures.append("query split across lines: expected %s, got %s" % (expected, rows))

    # A lambda body belongs to the function that contains it, not to the call it is an
    # argument of; an `if` block likewise.
    nested = (
        "void Player::SaveToDB()\n"
        "{\n"
        "    if (x)\n"
        "    {\n"
        "        CharacterDatabase.CommitTransactionChecked();\n"
        "    }\n"
        "    Run([this]() { WorldDatabase.Ping(); });\n"
        "}\n"
    )
    rows = scan_text(nested)
    expected = [
        (5, "CharacterDatabase", "CommitTransactionChecked", "Player::SaveToDB", "other"),
        (7, "WorldDatabase", "Ping", "Player::SaveToDB", "other"),
    ]
    if rows != expected:
        failures.append("nested blocks: expected %s, got %s" % (expected, rows))

    # A definition inside a namespace is still a function; a table initialiser is not.
    in_namespace = (
        "namespace MaNGOS\n"
        "{\n"
        "    static int table[] =\n"
        "    {\n"
        "        1,\n"
        "    };\n"
        "    void Initialize()\n"
        "    {\n"
        "        WorldDatabase.DirectExecute(\"SET NAMES utf8\");\n"
        "    }\n"
        "}\n"
    )
    rows = scan_text(in_namespace)
    expected = [(9, "WorldDatabase", "DirectExecute", "Initialize", "startup-named")]
    if rows != expected:
        failures.append("definition inside a namespace: expected %s, got %s" % (expected, rows))

    # The named APIs, and only those: QueryNamed and PQueryNamed are not `Query` with a
    # suffix left over.
    named = (
        "void Foo::Bar()\n"
        "{\n"
        "    WorldDatabase.PQueryNamed(\"SELECT %u\", 1);\n"
        "    WorldDatabase.QueryNamed(\"SELECT 1\");\n"
        "    WorldDatabase.AsyncPQuery(cb, \"SELECT 1\");\n"
        "}\n"
    )
    rows = scan_text(named)
    expected = [
        (3, "WorldDatabase", "PQueryNamed", "Foo::Bar", "other"),
        (4, "WorldDatabase", "QueryNamed", "Foo::Bar", "other"),
    ]
    if rows != expected:
        failures.append("named APIs (and AsyncPQuery excluded): expected %s, got %s" % (expected, rows))

    if failures:
        for f in failures:
            print("self-test FAILED: %s" % f, file=sys.stderr)
        return 1
    print("self-test OK")
    return 0


def main(argv):
    args = argv[1:]
    if "--self-test" in args:
        return self_test()

    roots = [a for a in args if not a.startswith("-")]
    unknown = [a for a in args if a.startswith("-")]
    if unknown:
        sys.stderr.write("unknown option '%s'\n" % unknown[0])
        sys.stderr.write(__doc__)
        return 1
    if len(roots) != 1:
        sys.stderr.write(__doc__)
        return 1
    root = roots[0]

    per_file = {}
    per_api = {api: 0 for api in APIS}
    per_hint = {hint: 0 for hint in HINTS}
    total = 0

    for dirpath, dirnames, names in os.walk(root):
        dirnames.sort()
        for name in sorted(names):
            if not name.endswith((".cpp", ".h")):
                continue
            path = os.path.join(dirpath, name)
            try:
                text = open(path, encoding="utf-8", errors="ignore").read()
            except OSError:
                continue
            rows = scan_text(text)
            if not rows:
                continue
            shown = path.replace(os.sep, "/")
            per_file[shown] = len(rows)
            for line, db, api, func, hint in rows:
                print("%s:%d  %s.%s  %s  %s" % (shown, line, db, api, func, hint))
                per_api[api] = per_api.get(api, 0) + 1
                per_hint[hint] += 1
                total += 1

    print("")
    print("sites per file:")
    for path, count in sorted(per_file.items(), key=lambda kv: (-kv[1], kv[0])):
        print("  %5d  %s" % (count, path))

    print("")
    print("totals per API:")
    for api in APIS:
        print("  %5d  %s" % (per_api.get(api, 0), api))

    print("")
    print("totals per hint:")
    for hint in HINTS:
        print("  %5d  %s" % (per_hint[hint], hint))

    print("")
    print("total sites=%d files=%d" % (total, len(per_file)))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
