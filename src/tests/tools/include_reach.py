"""include_reach.py <src root> <header> [<header> ...]

Transitive include weight and reach for the decoupling campaign's counters (design section 8).
For each header: the number of headers in its transitive closure, and the number of game .cpp
files whose own closure contains it, i.e. the translation units a change to it rebuilds.
Includes are resolved the way the flat include directories of the game target resolve them:
relative to the including file first, then path-qualified against each top-level directory
(game, motion, shared, proto, mangosd, realmd), then as a full path, then by basename -- against
any file type, not just headers. The header-reach gate (CheckHeaderReach.cmake) globs only the
first four of these roots (game, motion, shared, proto); it does not need mangosd or realmd
because no rule or game-target header reaches a header defined there, so the two tools agree on
every measured number.
"""
import collections
import os
import re
import sys

ROOT = sys.argv[1]
WANT = sys.argv[2:]
DIRS = ["game", "motion", "shared", "proto", "mangosd", "realmd"]
INCLUDE_RE = re.compile(r'^\s*#\s*include\s*[<"]([^">]+)[">]', re.M)

by_name = {}
all_files = []
for d in DIRS:
    for dirpath, dirnames, names in os.walk(os.path.join(ROOT, d)):
        dirnames.sort()
        for name in sorted(names):
            rel = os.path.relpath(os.path.join(dirpath, name), ROOT).replace("\\", "/")
            # Index every file type the gate's resolve_include() would find (it has no
            # extension filter), so a header that includes a .inc resolves the same way here.
            by_name.setdefault(name, rel)
            by_name.setdefault(rel, rel)
            if name.endswith((".h", ".hpp", ".cpp")):
                all_files.append(rel)


def resolve_target(inc, local):
    if local in by_name:
        return by_name[local]
    if "/" in inc:
        # A path-qualified include (e.g. "Auth/Sha1.h") is tried against each top-level
        # directory before falling back to a basename match: the tree has Auth/ vs Crypto/
        # Sha1.h/Md5.h collisions, and basename-first-wins would be ambiguous between them.
        for d in DIRS:
            candidate = by_name.get(f"{d}/{inc}")
            if candidate:
                return candidate
    return by_name.get(inc) or by_name.get(os.path.basename(inc))


edges = collections.defaultdict(set)
for rel in all_files:
    try:
        text = open(os.path.join(ROOT, rel), encoding="utf-8", errors="ignore").read()
    except OSError:
        continue
    here = os.path.dirname(rel)
    for match in INCLUDE_RE.finditer(text):
        inc = match.group(1)
        local = os.path.normpath(os.path.join(here, inc)).replace("\\", "/")
        target = resolve_target(inc, local)
        if target and target != rel:
            edges[rel].add(target)

memo = {}


def closure(start):
    if start in memo:
        return memo[start]
    seen, stack = set(), [start]
    while stack:
        current = stack.pop()
        for nxt in edges.get(current, ()):
            if nxt not in seen:
                seen.add(nxt)
                stack.append(nxt)
    memo[start] = seen
    return seen


game_cpps = [f for f in all_files if f.endswith(".cpp") and f.startswith("game/")]


def resolve(name):
    return by_name.get(name) or by_name.get(os.path.basename(name)) or name


print(f"{'header':44s} {'closure':>7s} {'game cpps reaching it':>22s}")
for wanted in WANT:
    header = resolve(wanted)
    size = len(closure(header))
    reach = sum(1 for cpp in game_cpps if header in closure(cpp))
    print(f"{header:44s} {size:7d} {reach:22d}")
