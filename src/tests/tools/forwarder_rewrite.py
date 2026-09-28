#!/usr/bin/env python3
"""forwarder_rewrite.py [--root <repo root>] [--apply] [--check] | --self-test

Decoupling D4i: drop the character's pure forwarders to its managers and rewrite their callers to
call the manager. A forwarder is PURE when its body is `return m_xMgr.Method(args);` or
`m_xMgr.Method(args);` with the forwarder's own parameters passed through unchanged and in order
-- nothing added by the owner (no sink, clock, guid, spec, lookup or owner fact) -- AND its
signature is the manager method's: the same return type, the same parameter types (reference-ness
included) and the same defaults, and the manager declares that method once (no overload set,
unless the table names the overload by its parameter types). Such a call has the same meaning
whether the character forwards it or the caller makes it, so each rewrite below is the
forwarder's body inlined at the call.

The table (FORWARDERS) names every forwarder this tool drops, with its manager member, the
character's accessor to the manager and the manager's method (the names differ for the
currencies and the talents). For each one:

  1. The forwarder is found in Player.h: either one line defining it inline, or a one-line
     declaration there plus exactly one out-of-line definition `Player::Name(...) { ... }` in an
     owner file (the two must agree on the types). Its body must match the pure shape above,
     and its signature is compared with the manager class's declaration of the method (read from
     the manager's header, inside that class only: types normalised for spacing and `const T` ==
     `T const`, defaults compared as written). Anything else stops the tool (IMPURE or ERROR)
     before anything is written. A pure forwarder is deleted (the line; for an out-of-line one,
     the declaration line and the definition).
  2. The name must be UNIQUE: no class but Player and the forwarder's own manager class declares
     it anywhere under src/ (SD3, the tests, and the other classes of the manager's own files
     included; only the manager class's own body is skipped). A declaration is a statement whose
     text before the name is only a type (`uint8 GetFreeTalentPoints() {`), or a `Class::Name(`
     with a class that is neither Player nor the manager. A SHARED name is never rewritten: every
     call to it is listed (HAND) for a rewrite by hand with the receiver's type proven per site.
     The HAND sites that must stay (calls on the other class) are pinned in EXPECTED_HAND, per
     name and file; --check fails when the listed sites differ from the pins, so a new call of a
     shared name (say, one a rebase brings in) cannot pass unnoticed.
  3. Every call of a unique name is rewritten, comments and string literals left alone:
       EXPR->Name(   ->  EXPR->Accessor().Method(
       EXPR.Name(    ->  EXPR.Accessor().Method(
       this->Name(   ->  m_xMgr.Method(          (in an owner file)
       Name(         ->  m_xMgr.Method(          (a bare call, in an owner file)
     A receiver that already is the manager (`m_xMgr.Name(`, `Accessor().Name(`) is left alone.
     The owner files are CheckManagerIsolation.cmake's OWNER_FILES (read from the gate, so the
     lists cannot drift): the character's own members call the member directly. A bare call
     anywhere else, and any `Player::Name` outside the forwarder's own definition (a qualified
     call or a member pointer), is an ERROR listed for a hand rewrite.

Not scanned for calls: src/tests/ (mangos_tests builds the managers without a character; the
manager tests call the managers' methods of the same names) and the manager's own files.

Every change is printed as `path:line: [domain] Name (kind): before => after`. Without --apply
nothing is written. --check exits 1 when anything is still to change, a forwarder is impure, an
ERROR site exists or the HAND sites differ from the pins; a second run after --apply is the
control that changes nothing. The compiler is the other control: a stale call of a dropped
forwarder no longer builds.

KNOWN MISSES, stated rather than chased: the literal blanking does not understand raw string
literals (R"(...)") or a `//` comment continued onto the next line by a trailing backslash; code
inside `#if 0` (or any preprocessor branch) is rewritten like the rest, without a warning; a
same-named local lambda or function object called bare inside an owner file would be rewritten.
None of these occurs at the sites this tool rewrites; the compiler catches every other slip (see
the controls). A whole-tree run takes a minute or two.

python3 src/tests/tools/forwarder_rewrite.py --root .            # plan and list
python3 src/tests/tools/forwarder_rewrite.py --root . --apply    # write
python3 src/tests/tools/forwarder_rewrite.py --root . --check    # the control
python3 src/tests/tools/forwarder_rewrite.py --self-test
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

PLAYER_H = 'src/game/entities/player/Player.h'
ISOLATION_GATE = 'src/tests/CheckManagerIsolation.cmake'
SOURCE_EXTS = ('.cpp', '.h', '.hpp', '.inl', '.cc', '.cxx')
NOT_SCANNED = ('src/tests/',)
LOOK_BEHIND = 400           # characters before a match searched for its receiver

# domain -> (member, accessor, manager class, the manager's own files: its header first)
DOMAINS = {
    'rune': ('m_runeMgr', 'GetRuneMgr', 'RuneMgr',
             ('src/game/entities/player/spells/RuneMgr.h',
              'src/game/entities/player/spells/RuneMgr.cpp')),
    'cooldown': ('m_spellCooldownMgr', 'GetSpellCooldownMgr', 'SpellCooldownMgr',
                 ('src/game/entities/player/spells/SpellCooldownMgr.h',
                  'src/game/entities/player/spells/SpellCooldownMgr.cpp')),
    'pet': ('m_petMgr', 'GetPetMgr', 'PetMgr',
            ('src/game/entities/player/pets/PetMgr.h',
             'src/game/entities/player/pets/PetMgr.cpp')),
    'currency': ('m_currencyMgr', 'GetCurrencyMgr', 'CurrencyMgr',
                 ('src/game/entities/player/inventory/CurrencyMgr.h',
                  'src/game/entities/player/inventory/CurrencyMgr.cpp')),
    'talent': ('m_talentMgr', 'GetTalentMgr', 'TalentMgr',
               ('src/game/entities/player/talents/TalentMgr.h',
                'src/game/entities/player/talents/TalentMgr.cpp')),
}

# (the forwarder's name, domain, the manager's method[, the overload's parameter types])
FORWARDERS = [
    ('GetRunesState', 'rune', 'GetRunesState'),
    ('GetBaseRune', 'rune', 'GetBaseRune'),
    ('GetCurrentRune', 'rune', 'GetCurrentRune'),
    ('GetRuneCooldown', 'rune', 'GetRuneCooldown'),
    ('GetBaseRuneCooldown', 'rune', 'GetBaseRuneCooldown'),
    ('GetRuneCooldownFraction', 'rune', 'GetRuneCooldownFraction'),
    ('IsBaseRuneSlotsOnCooldown', 'rune', 'IsBaseRuneSlotsOnCooldown'),
    ('ClearLastUsedRuneMask', 'rune', 'ClearLastUsedRuneMask'),
    ('IsLastUsedRune', 'rune', 'IsLastUsedRune'),
    ('SetLastUsedRune', 'rune', 'SetLastUsedRune'),
    ('SetBaseRune', 'rune', 'SetBaseRune'),
    ('SetCurrentRune', 'rune', 'SetCurrentRune'),
    ('SetRuneCooldown', 'rune', 'SetRuneCooldown'),
    ('SetBaseRuneCooldown', 'rune', 'SetBaseRuneCooldown'),
    ('SetRuneConvertAura', 'rune', 'SetRuneConvertAura'),
    ('ActivateRunes', 'rune', 'ActivateRunes'),
    ('GetSpellCooldownMap', 'cooldown', 'GetSpellCooldownMap'),
    ('AddSpellCooldown', 'cooldown', 'AddSpellCooldown'),
    ('GetTemporaryUnsummonedPetNumber', 'pet', 'GetTemporaryUnsummonedPetNumber'),
    ('SetTemporaryUnsummonedPetNumber', 'pet', 'SetTemporaryUnsummonedPetNumber'),
    ('GetStableSlots', 'pet', 'GetStableSlots'),
    ('SetStableSlots', 'pet', 'SetStableSlots'),
    ('GetCurrencyCount', 'currency', 'GetCount'),
    ('GetCurrencySeasonCount', 'currency', 'GetSeasonCount'),
    ('GetCurrencyWeekCount', 'currency', 'GetWeekCount'),
    ('GetCurrencyTotalCap', 'currency', 'GetTotalCap'),
    ('SetCurrencyFlags', 'currency', 'SetFlags'),
    ('GetFreeTalentPoints', 'talent', 'FreePoints'),
    ('SetFreeTalentPoints', 'talent', 'SetFreePoints'),
    ('GetPrimaryTalentTree', 'talent', 'PrimaryTree'),
    ('GetActiveSpec', 'talent', 'ActiveSpec'),
    ('GetSpecsCount', 'talent', 'SpecsCount'),
    ('GetKnownTalentRankById', 'talent', 'GetKnownTalentRankById'),     # out of line (Player.cpp)
]

# The HAND sites that stay: calls of a shared name on the other class (Pet's own talent points).
# (name, file) -> the number of listed sites. --check fails on any other count.
EXPECTED_HAND = {
    ('GetFreeTalentPoints', 'src/game/entities/player/talents/PlayerTalent.cpp'): 2,   # pet->
    ('SetFreeTalentPoints', 'src/game/ChatCommands/PlayerStatsMods.cpp'): 1,            # ((Pet*)target)->
    ('SetFreeTalentPoints', 'src/game/Object/PetSpells.cpp'): 3,                        # Pet members, bare
}

KEYWORDS = {'return', 'else', 'case', 'throw', 'new', 'delete', 'co_return', 'do', 'goto',
            'sizeof', 'typeid', 'not', 'and', 'or', 'if', 'while', 'for', 'switch'}
SPECIFIERS = {'virtual', 'static', 'inline', 'explicit', 'constexpr', 'friend', 'extern'}
# The text before a declared name on its line: specifiers, a (qualified, templated) type, then
# whitespace or a pointer/reference declarator.
TYPE_PREFIX = re.compile(
    r'^(?:(?:virtual|static|inline|explicit|constexpr|friend|extern)\s+)*(?:const\s+)?'
    r'([A-Za-z_][\w:]*)(?:\s*<[^;(){}]*>)?(?:\s+const)?(?:\s*[\*&]+\s*|\s+)$')
TOKEN = re.compile(r'[A-Za-z_]\w*|\d+\w*|::|\S')


def blank(text, literals=True):
    """Comments (and, with literals, string/char literals) replaced by spaces, every other
    character (newlines included) kept, so offsets and line numbers hold."""
    out = list(text)
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        if c == '/' and i + 1 < n and text[i + 1] == '/':
            while i < n and text[i] != '\n':
                out[i] = ' '
                i += 1
            continue
        if c == '/' and i + 1 < n and text[i + 1] == '*':
            end = text.find('*/', i + 2)
            end = n if end < 0 else end + 2
            for k in range(i, end):
                if text[k] != '\n':
                    out[k] = ' '
            i = end
            continue
        if c in '"\'':
            # a digit separator (1'000) is not a literal
            if c == "'" and i > 0 and text[i - 1].isdigit() and i + 1 < n and text[i + 1].isalnum() \
                    and text[max(0, i - 2):i] != 'u8':
                i += 1
                continue
            quote = c
            i += 1
            while i < n and text[i] != quote:
                if quote == "'" and text[i] == '\n':
                    break
                if text[i] == '\\' and i + 1 < n:
                    if literals:
                        if text[i + 1] != '\n':
                            out[i + 1] = ' '
                        out[i] = ' '
                    i += 2
                    continue
                if literals and text[i] != '\n':
                    out[i] = ' '
                i += 1
            if i < n and text[i] == quote:
                i += 1
            continue
        i += 1
    return ''.join(out)


def match_close(text, at, opening, closing):
    """The offset of the bracket closing the one at <at>, or -1."""
    depth = 0
    for i in range(at, len(text)):
        if text[i] == opening:
            depth += 1
        elif text[i] == closing:
            depth -= 1
            if depth == 0:
                return i
    return -1


def statement_prefix(clean, pos):
    """The blanked text of the statement before <pos>: after the last newline, ';', '{', '}' or
    single ':' (an access label, a case label, a ternary)."""
    i = pos - 1
    while i >= 0:
        ch = clean[i]
        if ch in '\n;{}':
            break
        if ch == ':' and not (i > 0 and clean[i - 1] == ':') \
                and not (i + 1 < len(clean) and clean[i + 1] == ':'):
            break
        i -= 1
    return clean[i + 1:pos]


def is_declaration(clean, pos):
    """True when the statement's text before the name at <pos> is only a type."""
    p = statement_prefix(clean, pos).strip()
    return bool(p) and bool(TYPE_PREFIX.match(p + ' ')) and p.split()[0] not in KEYWORDS


def owner_files(cmake_text):
    """The `set(OWNER_FILES ...)` paths of the isolation gate, relative to the repo root."""
    files, inside = [], False
    for line in cmake_text.split('\n'):
        code = line.split('#', 1)[0]
        if not inside:
            m = re.match(r'\s*set\(\s*OWNER_FILES\b(.*)$', code)
            if not m:
                continue
            inside, code = True, m.group(1)
        closed = ')' in code
        for word in code.split(')', 1)[0].split():
            files.append('src/game/' + word)
        if closed:
            break
    return set(files)


def join_tokens(toks):
    out = ''
    for t in toks:
        if out and (out[-1].isalnum() or out[-1] == '_') and (t[0].isalnum() or t[0] == '_'):
            out += ' '
        out += t
    return out


def norm_type(text):
    """A type spelled one way: specifiers dropped, spacing normalised, `const T` -> `T const`."""
    toks = TOKEN.findall(text)
    while toks and toks[0] in SPECIFIERS:
        toks.pop(0)
    if toks and toks[0] == 'const':
        toks.pop(0)
        i = 1                                   # past the base type's first name
        while i + 1 < len(toks) and toks[i] == '::':
            i += 2
        if i < len(toks) and toks[i] == '<':
            depth = 0
            while i < len(toks):
                depth += {'<': 1, '>': -1}.get(toks[i], 0)
                i += 1
                if depth == 0:
                    break
        toks.insert(i, 'const')
    return join_tokens(toks)


def split_top(text, sep):
    """<text> split at <sep> outside (), [], {} and <>."""
    parts, depth, cur = [], 0, ''
    for ch in text:
        if ch in '([{<':
            depth += 1
        elif ch in ')]}>':
            depth -= 1
        if ch == sep and depth == 0:
            parts.append(cur)
            cur = ''
        else:
            cur += ch
    parts.append(cur)
    return parts


def parse_params(text):
    """[(normalised type, name or None, normalised default or None)] of a parameter list."""
    text = text.strip()
    if text in ('', 'void'):
        return []
    out = []
    for p in split_top(text, ','):
        decl = split_top(p, '=')
        default = ' '.join('='.join(decl[1:]).split()) if len(decl) > 1 else None
        toks = TOKEN.findall(decl[0])
        name = None
        if len(toks) >= 2 and re.match(r'[A-Za-z_]\w*$', toks[-1]) and toks[-2] != '::' \
                and toks[-1] not in ('const', 'volatile'):
            name = toks.pop()
        out.append((norm_type(join_tokens(toks)), name, default))
    return out


def body_verdict(ret, params, body, member, method):
    """None if <body> is the pure pass-through of <params> to member.method; else the reason."""
    call = re.match(r'^(?P<kw>return\s+)?' + re.escape(member) + r'\s*\.\s*' + re.escape(method) +
                    r'\s*\((?P<args>[^()]*)\)\s*;$', body)
    if not call:
        return 'the body is not one call of %s.%s: {%s}' % (member, method, body)
    names = [p[1] for p in params]
    args = [a.strip() for a in call.group('args').split(',')] if call.group('args').strip() else []
    if None in names or args != names:
        return 'the arguments (%s) are not the parameters (%s) passed through' % (
            ', '.join(args), ', '.join(str(n) for n in names))
    if (norm_type(ret) == 'void') == bool(call.group('kw')):
        return 'return/void mismatch: %s {%s}' % (ret, body)
    return None


def class_body(clean, cls):
    """(open, close) brace offsets of the definition of class/struct <cls>, or None."""
    for m in re.finditer(r'(?<![\w])(?:class|struct)\s+' + re.escape(cls) + r'\b[^;{}()]*\{', clean):
        close = match_close(clean, m.end() - 1, '{', '}')
        if close > 0:
            return (m.end() - 1, close)
    return None


def manager_decls(clean, nocom, span, method):
    """The declarations of <method> in the class body <span>, at the class's own scope:
    [(line, normalised return type, params)]."""
    open_, close = span
    pat = re.compile(r'(?<![\w])' + re.escape(method) + r'\s*\(')
    starts = {m.start() for m in pat.finditer(clean, open_ + 1, close)}
    out, depth, paren = [], 0, 0
    for i in range(open_ + 1, close):
        if i in starts and depth == 0 and paren == 0 and is_declaration(clean, i):
            popen = clean.index('(', i)
            pclose = match_close(clean, popen, '(', ')')
            ret = statement_prefix(clean, i).strip()
            out.append((line_of(clean, i), norm_type(ret), parse_params(nocom[popen + 1:pclose])))
        ch = clean[i]
        if ch == '{':
            depth += 1
        elif ch == '}':
            depth -= 1
        elif ch == '(':
            paren += 1
        elif ch == ')':
            paren -= 1
    return out


def signature_verdict(ret, params, mdecl, method):
    """None if the forwarder's signature is the manager declaration's; else the reason."""
    mline, mret, mparams = mdecl
    if norm_type(ret) != mret:
        return 'return type %s, the manager\'s %s is %s' % (norm_type(ret), method, mret)
    if len(params) != len(mparams):
        return '%d parameter(s), the manager\'s %s takes %d' % (len(params), method, len(mparams))
    for i, (p, q) in enumerate(zip(params, mparams)):
        if p[0] != q[0]:
            return 'parameter %d is %s, the manager\'s %s takes %s' % (i + 1, p[0], method, q[0])
        if p[2] != q[2]:
            return 'parameter %d default %s, the manager\'s %s has %s' % (i + 1, p[2], method, q[2])
    return None


def line_of(text, pos):
    return text.count('\n', 0, pos) + 1


def line_start(text, pos):
    return text.rfind('\n', 0, pos) + 1


def line_end(text, pos):
    """The offset just past the newline ending the line of <pos>."""
    end = text.find('\n', pos)
    return len(text) if end < 0 else end + 1


def line_text(text, pos):
    return text[line_start(text, pos):line_end(text, pos)].rstrip('\r\n')


def read(path):
    with open(path, 'r', encoding='latin-1', newline='') as f:
        return f.read()


def write(path, text):
    with open(path, 'w', encoding='latin-1', newline='') as f:
        f.write(text)


def source_files(root):
    out = []
    for dirpath, dirnames, filenames in os.walk(os.path.join(root, 'src')):
        dirnames[:] = sorted(d for d in dirnames if d not in ('.git', '__pycache__'))
        for fn in sorted(filenames):
            if fn.endswith(SOURCE_EXTS):
                full = os.path.join(dirpath, fn)
                out.append(os.path.relpath(full, root).replace(os.sep, '/'))
    return out


def find_forwarder(texts, clean, nocom, owners, name, member, method):
    """Locate the forwarder <name> on Player. Returns (info or None, error or None); info is
    None when Player no longer declares the name (dropped already)."""
    if PLAYER_H not in texts:
        return None, None
    pclean = clean[PLAYER_H]
    pat = re.compile(r'(?<![\w])' + re.escape(name) + r'\s*\(')
    decls = [m.start() for m in pat.finditer(pclean) if is_declaration(pclean, m.start())]
    if not decls:
        return None, None
    if len(decls) > 1:
        return None, '%s:%d: an overload set on Player (%d declarations)' % (
            PLAYER_H, line_of(pclean, decls[0]), len(decls))
    at = decls[0]
    where = '%s:%d' % (PLAYER_H, line_of(pclean, at))
    raw = line_text(texts[PLAYER_H], at)
    nc = line_text(nocom(PLAYER_H), at)
    decl_span = (PLAYER_H, line_start(pclean, at), line_end(pclean, at))
    head = r'^\s*(?P<ret>[^(){};=]*?)\s*\b' + re.escape(name) + r'\s*\((?P<params>[^()]*)\)\s*(?:const\s*)?'
    m = re.match(head + r'\{(?P<body>.*)\}\s*$', nc)
    if m:
        params = parse_params(m.group('params'))
        return dict(kind='inline', where=where, raw=raw.strip(), ret=m.group('ret'), params=params,
                    names_from=params, body=' '.join(m.group('body').split()),
                    drops=[decl_span], spans=[decl_span], defined=None), None
    m = re.match(head + r';\s*$', nc)
    if not m:
        return None, '%s: a declaration that is neither a one-line inline forwarder nor a one-line ' \
                     'declaration: %s' % (where, raw.strip())
    decl_params = parse_params(m.group('params'))
    qual = re.compile(r'(?<![\w])Player\s*::\s*' + re.escape(name) + r'\s*\(')
    defs = [(f, q.start()) for f in sorted(owners) if f in clean for q in qual.finditer(clean[f])]
    if len(defs) != 1:
        return None, '%s: %d out-of-line definition(s) of Player::%s in the owner files' % (
            where, len(defs), name)
    f, q = defs[0]
    c, t = clean[f], nocom(f)
    ls = line_start(c, q)
    def_ret = c[ls:q].strip()
    popen = c.index('(', q)
    pclose = match_close(c, popen, '(', ')')
    after = re.match(r'\s*(?:const\s*)?\{', c[pclose + 1:])
    if not def_ret or pclose < 0 or not after:
        return None, '%s:%d: the definition of Player::%s is not `<type> Player::%s(...) [const] {`' % (
            f, line_of(c, q), name, name)
    bopen = pclose + after.end()
    bclose = match_close(c, bopen, '{', '}')
    def_params = parse_params(t[popen + 1:pclose])
    if norm_type(def_ret) != norm_type(m.group('ret')) or \
            [p[0] for p in def_params] != [p[0] for p in decl_params]:
        return None, '%s:%d: the declaration and the definition of %s differ' % (f, line_of(c, q), name)
    end = line_end(c, bclose)
    if c[end:line_end(c, end)].strip() == '' and end < len(c):
        end = line_end(c, end)                   # the blank line after the definition
    def_span = (f, ls, end)
    return dict(kind='out-of-line', where=where, raw=raw.strip(), ret=m.group('ret'),
                params=decl_params, names_from=def_params,
                body=' '.join(t[bopen + 1:bclose].split()), drops=[decl_span, def_span],
                spans=[decl_span, (f, ls, bclose + 1)],
                defined='%s:%d' % (f, line_of(c, q))), None


def run(root, forwarders, domains, owners, apply, expected_hand=None):
    """Plan (and with apply, write) the rewrite.
    Returns (changes, errors, hands, report lines, unexpected hand counts)."""
    files = source_files(root)
    texts = {f: read(os.path.join(root, f)) for f in files}
    clean = {f: blank(t) for f, t in texts.items()}
    nocom_cache = {}

    def nocom(f):
        if f not in nocom_cache:
            nocom_cache[f] = blank(texts[f], literals=False)
        return nocom_cache[f]

    edits = {}           # file -> [(start, end, replacement, name, kind)]
    errors, hands, report = [], [], []
    hand_count = {}

    for entry in forwarders:
        name, domain, method = entry[:3]
        overload = entry[3] if len(entry) > 3 else None
        member, accessor, mgr_class, mgr_files = domains[domain]
        pat = re.compile(r'(?<![\w])' + re.escape(name) + r'\s*\(')
        mgr_h = mgr_files[0]
        mspan = class_body(clean[mgr_h], mgr_class) if mgr_h in clean else None

        # 1. the forwarder itself: the body, then the signature against the manager's
        info, err = find_forwarder(texts, clean, nocom, owners, name, member, method)
        if err:
            errors.append('[%s] %s: %s' % (domain, name, err))
            continue
        spans = []
        if info:
            reason = body_verdict(info['ret'], info['names_from'], info['body'], member, method)
            if reason is None:
                if mspan is None:
                    reason = 'class %s not found in %s' % (mgr_class, mgr_h)
                else:
                    mdecls = manager_decls(clean[mgr_h], nocom(mgr_h), mspan, method)
                    if overload is not None:
                        mdecls = [d for d in mdecls if tuple(p[0] for p in d[2]) == tuple(overload)]
                    if len(mdecls) != 1:
                        reason = '%s::%s has %d declaration(s)%s; name the overload in the table' % (
                            mgr_class, method, len(mdecls), '' if overload is None else ' matching')
                    else:
                        reason = signature_verdict(info['ret'], info['params'], mdecls[0], method)
            if reason is not None:
                errors.append('%s: [%s] %s: IMPURE: %s' % (info['where'], domain, name, reason))
                continue
            spans = info['spans']
            for f, s, e in info['drops']:
                edits.setdefault(f, []).append((s, e, '', name, 'drop'))
            if info['kind'] == 'inline':
                report.append('%s: [%s] %s: DROP forwarder `%s`; its body `%s`'
                              % (info['where'], domain, name, info['raw'], info['body']))
            else:
                report.append('%s: [%s] %s: DROP out-of-line forwarder `%s`, defined at %s; its body `%s`'
                              % (info['where'], domain, name, info['raw'], info['defined'], info['body']))

        def in_spans(f, pos):
            return any(f == sf and s <= pos < e for sf, s, e in spans)

        # 2. uniqueness: a declaration of the name by any other class
        shared = []
        for f in files:
            c = clean[f]
            for m in pat.finditer(c):
                if in_spans(f, m.start()):
                    continue
                if f == mgr_h and mspan and mspan[0] < m.start() < mspan[1]:
                    continue                    # the manager class's own body
                before = c[max(0, m.start() - LOOK_BEHIND):m.start()]
                q = re.search(r'([A-Za-z_]\w*)\s*::\s*$', before)
                if q:
                    if q.group(1) not in ('Player', mgr_class):
                        shared.append('%s:%d (%s::%s)' % (f, line_of(c, m.start()), q.group(1), name))
                    continue
                if re.search(r'(->|\.)\s*$', before):
                    continue
                if is_declaration(c, m.start()):
                    if f == PLAYER_H:
                        errors.append('%s:%d: [%s] %s: another declaration on Player: %s'
                                      % (f, line_of(c, m.start()), domain, name,
                                         line_text(texts[f], m.start()).strip()))
                    else:
                        shared.append('%s:%d (declared)' % (f, line_of(c, m.start())))
        if shared:
            report.append('[%s] %s: SHARED with %s: not rewritten by the script'
                          % (domain, name, '; '.join(shared)))

        # 3. the calls
        qualified = re.compile(r'(?<![\w])Player\s*::\s*' + re.escape(name) + r'(?![\w])')
        for f in files:
            if f.startswith(NOT_SCANNED) or f in mgr_files:
                continue
            c, t = clean[f], texts[f]
            for m in qualified.finditer(c):
                if in_spans(f, m.start()):
                    continue
                errors.append('%s:%d: [%s] %s: Player::%s: rewrite by hand: %s'
                              % (f, line_of(t, m.start()), domain, name, name,
                                 line_text(t, m.start()).strip()))
            for m in pat.finditer(c):
                if in_spans(f, m.start()):
                    continue
                before = c[max(0, m.start() - LOOK_BEHIND):m.start()]
                base = m.start() - len(before)
                where = '%s:%d' % (f, line_of(t, m.start()))
                src_line = line_text(t, m.start()).strip()
                name_end = m.start() + len(name)

                def hand(kind):
                    hands.append('%s: [%s] %s: HAND (%s): %s' % (where, domain, name, kind, src_line))
                    hand_count[(name, f)] = hand_count.get((name, f), 0) + 1

                if re.search(r'::\s*$', before):
                    continue        # Player:: is an error above; another class's makes it shared
                recv = re.search(r'(->|\.)\s*$', before)
                if recv:
                    head = before[:recv.start()]
                    this = re.search(r'\bthis\s*$', head)
                    if this and recv.group(1) == '->':
                        if shared:
                            hand('this->')
                            continue
                        if f not in owners:
                            errors.append('%s: [%s] %s: this-> outside the owner files: %s'
                                          % (where, domain, name, src_line))
                            continue
                        edits.setdefault(f, []).append((base + this.start(), name_end, member + '.' + method,
                                                        name, 'this'))
                        continue
                    if re.search(r'(?:\b' + re.escape(member) + r'|\b' + re.escape(accessor) +
                                 r'\s*\(\s*\))\s*$', head):
                        continue            # already the manager
                    if shared:
                        hand('receiver')
                        continue
                    edits.setdefault(f, []).append((m.start(), name_end, accessor + '().' + method,
                                                    name, 'receiver'))
                    continue
                if is_declaration(c, m.start()):
                    continue                # a declaration (counted above)
                if shared:
                    hand('bare')
                    continue
                if f not in owners:
                    errors.append('%s: [%s] %s: a bare call outside the owner files: %s'
                                  % (where, domain, name, src_line))
                    continue
                edits.setdefault(f, []).append((m.start(), name_end, member + '.' + method, name, 'bare'))

    unexpected = []
    if expected_hand is not None:
        for key in sorted(set(hand_count) | set(expected_hand)):
            got, want = hand_count.get(key, 0), expected_hand.get(key, 0)
            if got != want:
                unexpected.append('%s in %s: %d HAND site(s) listed, %d pinned' % (key[0], key[1], got, want))

    domain_of = {e[0]: e[1] for e in forwarders}
    changes = 0
    for f in sorted(edits):
        t = texts[f]
        new = t
        for start, end, repl, name, kind in sorted(edits[f], key=lambda e: e[0], reverse=True):
            new = new[:start] + repl + new[end:]
        # the listing, per site, on the old text
        for start, end, repl, name, kind in sorted(edits[f], key=lambda e: e[0]):
            if kind == 'drop':
                continue
            ls = line_start(t, start)
            le = t.find('\n', start)
            le = len(t) if le < 0 else le
            new_line = t[ls:start] + repl + t[end:le]
            report.append('%s:%d: [%s] %s (%s): %s => %s'
                          % (f, line_of(t, start), domain_of[name], name, kind,
                             line_text(t, start).strip(), new_line.strip()))
            changes += 1
        changes += sum(1 for e in edits[f] if e[4] == 'drop')
        if apply and new != t:
            write(os.path.join(root, f), new)
    return changes, errors, hands, report, unexpected


def main_run(root, apply, check, forwarders=None, domains=None, owners=None, expected_hand=None, out=print):
    forwarders = FORWARDERS if forwarders is None else forwarders
    domains = DOMAINS if domains is None else domains
    expected_hand = EXPECTED_HAND if expected_hand is None else expected_hand
    if owners is None:
        owners = owner_files(read(os.path.join(root, ISOLATION_GATE)))
    if not owners:
        out('ERROR: no OWNER_FILES in %s' % ISOLATION_GATE)
        return 1
    changes, errors, hands, report, unexpected = run(root, forwarders, domains, owners, False, expected_hand)
    for line in report:
        out(line)
    for line in hands:
        out(line)
    for line in errors:
        out('ERROR: ' + line)
    for line in unexpected:
        out('UNEXPECTED HAND: ' + line)
    out('SUMMARY: %d change(s), %d hand site(s), %d error(s)' % (changes, len(hands), len(errors)))
    if errors:
        return 1
    if apply and changes:
        run(root, forwarders, domains, owners, True, expected_hand)
        out('APPLIED')
    if check and (changes or unexpected):
        return 1
    return 0


# ----------------------------------------------------------------------------------------------
# self-test

SELF_DOMAINS = {
    'rune': ('m_runeMgr', 'GetRuneMgr', 'RuneMgr', ('src/game/p/RuneMgr.h',)),
    'talent': ('m_talentMgr', 'GetTalentMgr', 'TalentMgr', ('src/game/p/TalentMgr.h',)),
}
SELF_FORWARDERS = [
    ('GetRunesState', 'rune', 'GetRunesState'),
    ('SetRuneCooldown', 'rune', 'SetRuneCooldown'),
    ('GetInfo', 'rune', 'GetInfo'),
    ('Fill', 'rune', 'Fill'),
    ('Reset', 'rune', 'Reset'),
    ('GetFreeTalentPoints', 'talent', 'FreePoints'),
    ('GetActiveSpec', 'talent', 'ActiveSpec'),
    ('GetKnown', 'talent', 'GetKnown'),
]
SELF_PLAYER_H = '''class Player
{
    public:
        uint8 GetRunesState() const { return m_runeMgr.GetRunesState(); }
        void SetRuneCooldown(uint8 index, uint16 cooldown) { m_runeMgr.SetRuneCooldown(index, cooldown); }
        RuneInfo const& GetInfo() const { return m_runeMgr.GetInfo(); }
        void Fill(RuneInfo& out) const { m_runeMgr.Fill(out); }
        void Reset(bool full = false) { m_runeMgr.Reset(full); }
        uint32 GetFreeTalentPoints() const { return m_talentMgr.FreePoints(); }
        uint8 GetActiveSpec() { return m_talentMgr.ActiveSpec(); }
        SpellEntry const* GetKnown(int32 talentId) const;
        bool HasRunes() const { return GetRunesState() != 0; } // GetRunesState() in a comment
        RuneMgr m_runeMgr;
};
'''
SELF_RUNEMGR_H = '''struct RuneInfo { uint8 slot; };
class RuneMgr
{
    public:
        uint8 GetRunesState() const { return m_state; }
        void SetRuneCooldown(uint8 index, uint16 cooldown) { m_cd[index] = cooldown; }
        const RuneInfo& GetInfo() const { return m_info; }
        void Fill(RuneInfo & out) const;
        void Reset(bool full = false);
        void Tick() { SetRuneCooldown(0, 0); }
};
'''
SELF_TALENTMGR_H = ('class TalentMgr { public: uint8 ActiveSpec() const; uint32 FreePoints() const;\n'
                    '    SpellEntry const* GetKnown(int32 talentId) const; };\n')
SELF_PET_H = 'class Pet\n{\n    public:\n        uint8 GetFreeTalentPoints() { return 0; }\n};\n'
SELF_OWNER = '''void Player::Regen()
{
    uint8 s = GetRunesState();
    this->SetRuneCooldown(1, 2);
    m_runeMgr.SetRuneCooldown(1, 2);
    const char* t = "GetRunesState()";
    /* SetRuneCooldown(3, 4) */
    uint32 f = GetFreeTalentPoints();
    uint8 spec = GetActiveSpec();
}

SpellEntry const* Player::GetKnown(int32 id) const
{
    return m_talentMgr.GetKnown(id);
}

void Player::After() {}
'''
SELF_OUTSIDE = '''void Spell::Take(Player* plr, Player& ref, Unit* u)
{
    uint8 s = plr->GetRunesState();
    ((Player*)u)->SetRuneCooldown(i, cd);
    s = ref.GetRunesState();
    s = plr->GetRuneMgr().GetRunesState();
    s = plr ->  GetRunesState ();
    uint32 a = pet->GetFreeTalentPoints();
    uint32 b = plr->GetFreeTalentPoints();
    buf << uint32(plr->GetActiveSpec());
    SpellEntry const* k = plr->GetKnown(5);
}
'''
SELF_TEST_FILE = 'void T() { RuneMgr r; r.GetRunesState(); }\n'
SELF_OWNER_PATH = 'src/game/entities/player/PlayerRegen.cpp'


def _tree(files):
    root = tempfile.mkdtemp(prefix='fwd_rewrite_')
    for rel, text in files.items():
        full = os.path.join(root, rel)
        os.makedirs(os.path.dirname(full), exist_ok=True)
        write(full, text)
    return root


def _base_files():
    return {
        PLAYER_H: SELF_PLAYER_H,
        'src/game/p/RuneMgr.h': SELF_RUNEMGR_H,
        'src/game/p/TalentMgr.h': SELF_TALENTMGR_H,
        'src/game/Object/Pet.h': SELF_PET_H,
        SELF_OWNER_PATH: SELF_OWNER,
        'src/game/Spell.cpp': SELF_OUTSIDE,
        'src/tests/RuneMgrTest.cpp': SELF_TEST_FILE,
    }


SELF_OWNERS = {PLAYER_H, SELF_OWNER_PATH}


def _quiet(line):
    pass


def _run(root, apply=False, expected=None, forwarders=None):
    return run(root, SELF_FORWARDERS if forwarders is None else forwarders, SELF_DOMAINS, SELF_OWNERS,
               apply, expected)


def self_test():
    failures = []

    def expect(cond, what):
        if not cond:
            failures.append(what)

    # 1. the rewrite
    root = _tree(_base_files())
    try:
        changes, errors, hands, report, _ = _run(root, True)
        expect(errors == [], 'no errors on the clean tree: %r' % errors)
        ph = read(os.path.join(root, PLAYER_H))
        expect('GetRunesState() const {' not in ph, 'the rune getter forwarder is deleted')
        expect('void SetRuneCooldown' not in ph, 'the rune setter forwarder is deleted')
        expect('GetInfo' not in ph and 'Fill' not in ph and 'Reset' not in ph,
               'the const& / by-reference / defaulted forwarders with equal signatures are deleted')
        expect('GetFreeTalentPoints() const' not in ph, 'a shared name\'s forwarder is deleted too')
        expect('uint8 GetActiveSpec()' not in ph, 'the talent forwarder is deleted')
        expect('GetKnown' not in ph, 'the out-of-line forwarder\'s declaration is deleted')
        expect('{ return m_runeMgr.GetRunesState() != 0; } // GetRunesState() in a comment' in ph,
               'a bare call in Player.h goes to the member; the comment is kept')
        expect(ph.count('\n') == SELF_PLAYER_H.count('\n') - 8, 'exactly eight lines deleted')
        own = read(os.path.join(root, SELF_OWNER_PATH))
        expect('uint8 s = m_runeMgr.GetRunesState();' in own, 'owner bare call -> member')
        expect('    m_runeMgr.SetRuneCooldown(1, 2);\n    m_runeMgr.SetRuneCooldown(1, 2);' in own,
               'owner this-> call -> member; the member call left alone')
        expect('"GetRunesState()"' in own, 'a string literal is left alone')
        expect('/* SetRuneCooldown(3, 4) */' in own, 'a block comment is left alone')
        expect('uint32 f = GetFreeTalentPoints();' in own, 'a shared name is not rewritten, even bare')
        expect('uint8 spec = m_talentMgr.ActiveSpec();' in own, 'a renamed method, bare')
        expect('GetKnown' not in own and own.endswith('}\n\nvoid Player::After() {}\n'),
               'the out-of-line definition and its blank line are deleted: %r' % own[-80:])
        out = read(os.path.join(root, 'src/game/Spell.cpp'))
        expect('uint8 s = plr->GetRuneMgr().GetRunesState();' in out, '-> receiver')
        expect('((Player*)u)->GetRuneMgr().SetRuneCooldown(i, cd);' in out, 'cast receiver')
        expect('s = ref.GetRuneMgr().GetRunesState();' in out, '. receiver')
        expect('s = plr->GetRuneMgr().GetRunesState();\n    s = plr ->  GetRuneMgr().GetRunesState ();'
               in out, 'an accessor receiver left alone; spacing kept')
        expect('uint32 a = pet->GetFreeTalentPoints();' in out, 'the Pet call is not rewritten')
        expect('uint32 b = plr->GetFreeTalentPoints();' in out, 'a shared name\'s Player call is a HAND site')
        expect('buf << uint32(plr->GetTalentMgr().ActiveSpec());' in out, 'a renamed method, receiver')
        expect('SpellEntry const* k = plr->GetTalentMgr().GetKnown(5);' in out, 'an out-of-line one\'s caller')
        expect(len([h for h in hands if 'GetFreeTalentPoints' in h]) == 3,
               'three HAND sites for the shared name: %r' % hands)
        expect(any('SHARED with src/game/Object/Pet.h:4' in r for r in report), 'the shared declaration is named')
        expect(any('DROP out-of-line forwarder' in r and SELF_OWNER_PATH + ':12' in r for r in report),
               'the out-of-line drop is listed with its definition')
        mgr = read(os.path.join(root, 'src/game/p/RuneMgr.h'))
        expect(mgr == SELF_RUNEMGR_H, 'the manager\'s own file is untouched')
        expect(read(os.path.join(root, 'src/tests/RuneMgrTest.cpp')) == SELF_TEST_FILE, 'src/tests is not scanned')
        expect(changes == 9 + 10, 'nineteen changes (9 drops + 10 calls): %d' % changes)
        # 2. the control: a second run changes nothing
        changes2, errors2, hands2, _, _ = _run(root, True)
        expect(changes2 == 0 and errors2 == [], 'the re-run changes nothing: %d %r' % (changes2, errors2))
        expect(len(hands2) == 3, 'the re-run still lists the hand sites')
        # 2b. the pinned HAND list: exact pins pass, a new site or a missing one does not
        pins = {('GetFreeTalentPoints', 'src/game/Spell.cpp'): 2, ('GetFreeTalentPoints', SELF_OWNER_PATH): 1}
        expect(_run(root, False, pins)[4] == [], 'the exact pins pass')
        expect(len(_run(root, False, {**pins, ('GetFreeTalentPoints', 'src/game/Spell.cpp'): 1})[4]) == 1,
               'one more HAND site than pinned is reported')
        expect(len(_run(root, False, {**pins, ('GetFreeTalentPoints', 'src/game/X.cpp'): 1})[4]) == 1,
               'a pinned site that is gone is reported')
        code = main_run(root, False, True, SELF_FORWARDERS, SELF_DOMAINS, SELF_OWNERS,
                        {**pins, ('GetFreeTalentPoints', 'src/game/Spell.cpp'): 1}, _quiet)
        expect(code == 1, '--check exits 1 on an unexpected HAND site')
        code = main_run(root, False, True, SELF_FORWARDERS, SELF_DOMAINS, SELF_OWNERS, pins, _quiet)
        expect(code == 0, '--check exits 0 when the HAND sites are the pinned ones')
    finally:
        shutil.rmtree(root, ignore_errors=True)

    # 3. impure forwarders stop the tool before anything is written
    rune = 'src/game/p/RuneMgr.h'
    impure = [
        (PLAYER_H, 'uint8 GetRunesState() const { return m_runeMgr.GetRunesState(); }',
         'uint8 GetRunesState() const { return m_runeMgr.GetRunesState(SessionSink()); }', 'an added sink'),
        (PLAYER_H, '{ m_runeMgr.SetRuneCooldown(index, cooldown); }',
         '{ m_runeMgr.SetRuneCooldown(cooldown, index); }', 'swapped arguments'),
        (PLAYER_H, '{ m_runeMgr.SetRuneCooldown(index, cooldown); }',
         '{ m_runeMgr.SetRuneCooldown(index + 1, cooldown); }', 'an expression argument'),
        (PLAYER_H, '{ return m_runeMgr.GetRunesState(); }',
         '{ Log(); return m_runeMgr.GetRunesState(); }', 'a second statement'),
        (PLAYER_H, '{ return m_runeMgr.GetRunesState(); }',
         '{ return m_otherMgr.GetRunesState(); }', 'another member'),
        (PLAYER_H, '{ return m_talentMgr.ActiveSpec(); }',
         '{ return m_talentMgr.SpecsCount(); }', 'another method'),
        (PLAYER_H, 'uint8 GetRunesState() const { return m_runeMgr.GetRunesState(); }',
         'uint8 GetRunesState() const;', 'an out-of-line declaration without a definition'),
        # the signature against the manager's (I-1)
        (PLAYER_H, 'void SetRuneCooldown(uint8 index, uint16 cooldown)',
         'void SetRuneCooldown(uint8 index, uint16 cooldown = 0)', 'a default the manager lacks'),
        (PLAYER_H, 'void Reset(bool full = false)', 'void Reset(bool full = true)', 'a different default'),
        (PLAYER_H, 'void Reset(bool full = false)', 'void Reset(bool full)', 'a manager default dropped'),
        (PLAYER_H, 'void SetRuneCooldown(uint8 index,', 'void SetRuneCooldown(uint32 index,',
         'a different parameter type'),
        (PLAYER_H, 'uint8 GetRunesState() const {', 'uint32 GetRunesState() const {', 'a wider return type'),
        (PLAYER_H, 'uint8 GetRunesState() const {', 'bool GetRunesState() const {', 'another return type'),
        (PLAYER_H, 'void Fill(RuneInfo& out) const', 'void Fill(RuneInfo out) const',
         'by value over a by-reference manager parameter'),
        (PLAYER_H, 'RuneInfo const& GetInfo() const', 'RuneInfo GetInfo() const', 'a copy of a const&'),
        (rune, '        void Tick()', '        uint8 GetRunesState(uint8 slot) const;\n        void Tick()',
         'an overloaded manager method'),
        (rune, 'void Reset(bool full = false);', 'void Reset(bool full = false, int n = 1);',
         'a manager parameter more'),
        (SELF_OWNER_PATH, 'return m_talentMgr.GetKnown(id);', 'return m_talentMgr.GetKnown(id + 1);',
         'an impure out-of-line body'),
        (SELF_OWNER_PATH, 'Player::GetKnown(int32 id) const', 'Player::GetKnown(uint32 id) const',
         'an out-of-line definition that differs from its declaration'),
    ]
    for path, old, new, what in impure:
        files = _base_files()
        expect(old in files[path], 'impure (%s): the fixture holds the text to replace' % what)
        files[path] = files[path].replace(old, new, 1)
        root = _tree(files)
        try:
            err = _run(root)[1]
            expect(err != [], 'impure (%s) is an error' % what)
            code = main_run(root, True, False, SELF_FORWARDERS, SELF_DOMAINS, SELF_OWNERS, {}, _quiet)
            expect(code == 1, 'impure (%s): the tool exits 1' % what)
            expect(read(os.path.join(root, path)) == files[path] and
                   read(os.path.join(root, 'src/game/Spell.cpp')) == SELF_OUTSIDE,
                   'impure (%s): nothing written' % what)
        finally:
            shutil.rmtree(root, ignore_errors=True)

    # 3b. accepted: `const T&` == `T const&`, and the overload named in the table
    accepted = [
        (PLAYER_H, 'RuneInfo const& GetInfo() const', 'const RuneInfo & GetInfo() const', None,
         'const T& spelled the other way'),
        (rune, '        void Tick()', '        uint8 GetRunesState(uint8 slot) const;\n        void Tick()',
         ('GetRunesState', 'rune', 'GetRunesState', ()), 'an overload named in the table'),
    ]
    for path, old, new, entry, what in accepted:
        files = _base_files()
        files[path] = files[path].replace(old, new, 1)
        root = _tree(files)
        try:
            table = [entry if entry and e[0] == entry[0] else e for e in SELF_FORWARDERS]
            err = _run(root, False, None, table)[1]
            expect(err == [], 'accepted (%s): %r' % (what, err))
        finally:
            shutil.rmtree(root, ignore_errors=True)

    # 4. sites the script refuses
    refused = [
        ('src/game/Other.cpp', 'void F() { uint8 s = GetRunesState(); }\n', 'a bare call outside the owners'),
        ('src/game/Other.cpp', 'auto p = &Player::GetRunesState;\n', 'a member pointer'),
        ('src/game/Other.cpp', 'uint8 s = u->Player::GetRunesState();\n', 'a qualified call'),
        ('src/game/Other.cpp', 'void F() { this->GetRunesState(); }\n', 'this-> outside the owners'),
    ]
    for path, text, what in refused:
        files = _base_files()
        files[path] = text
        root = _tree(files)
        try:
            err = _run(root)[1]
            expect(any('GetRunesState' in e for e in err), 'refused: %s: %r' % (what, err))
        finally:
            shutil.rmtree(root, ignore_errors=True)

    # 5. a declaration elsewhere makes the name shared -- another class in the manager's own
    #    header included; the manager class's own declaration does not
    for path, text, shared in [
            ('src/modules/SD3/x.h', 'struct X { uint32 GetActiveSpec() const; };\n', True),
            ('src/game/Y.cpp', 'uint8 Vehicle::GetRunesState() { return 0; }\n', True),
            ('src/game/Z.cpp', 'uint8 v = GetActiveSpec() + 1;\n', False),
            (rune, SELF_RUNEMGR_H.replace('struct RuneInfo { uint8 slot; };',
                                          'struct RuneInfo { uint8 GetRunesState() const; };'), True),
            (rune, SELF_RUNEMGR_H, False),
    ]:
        files = _base_files()
        files[path] = text
        root = _tree(files)
        try:
            report = _run(root)[3]
            name = 'GetActiveSpec' if 'ActiveSpec' in text else 'GetRunesState'
            is_shared = any(('%s: SHARED' % name) in r for r in report)
            expect(is_shared == shared, 'shared(%s) == %s: %r' % (path, shared, report))
        finally:
            shutil.rmtree(root, ignore_errors=True)

    # 6. helpers: the owner list from the gate's block, the blanking, the type spelling
    gate = ('set(MANAGER_FILES\n    entities/player/A.h\n)\nset(OWNER_FILES\n    entities/player/Player.h\n'
            '    entities/player/B.cpp   # note\n)\nset(OTHER x)\n')
    expect(owner_files(gate) == {'src/game/entities/player/Player.h', 'src/game/entities/player/B.cpp'},
           'owner_files: %r' % owner_files(gate))
    src = 'a /* b\nc */ "d\\"e" \'f\' 1\'000 // g\nh'
    b = blank(src)
    expect(len(b) == len(src) and b.count('\n') == 2 and not any(ch in b for ch in 'bcdefg')
           and "1'000" in b and b.endswith('\nh'), 'blank keeps offsets and newlines: %r' % b)
    expect(blank('x = "s"; // c', literals=False) == 'x = "s";     ', 'blank without literals keeps strings')
    expect(norm_type('const CurrencyTypesEntry *') == norm_type('CurrencyTypesEntry const*')
           == 'CurrencyTypesEntry const*', 'const T* == T const*')
    expect(norm_type('const std::map<int, int>&') == 'std::map<int,int>const&', 'const templated T&: %r'
           % norm_type('const std::map<int, int>&'))
    expect(norm_type('uint8') != norm_type('uint32') and norm_type('T&') != norm_type('T'),
           'types that differ stay different')
    expect(parse_params('uint8 index, Aura const* aura = NULL') ==
           [('uint8', 'index', None), ('Aura const*', 'aura', 'NULL')], 'parse_params: %r'
           % parse_params('uint8 index, Aura const* aura = NULL'))

    for f in failures:
        print('SELF-TEST FAILED: ' + f)
    print('self-test: %s (%d failure(s))' % ('PASS' if not failures else 'FAIL', len(failures)))
    return 1 if failures else 0


def main(argv):
    ap = argparse.ArgumentParser()
    ap.add_argument('--root', default=os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..')))
    ap.add_argument('--apply', action='store_true')
    ap.add_argument('--check', action='store_true')
    ap.add_argument('--self-test', action='store_true')
    args = ap.parse_args(argv[1:])
    if args.self_test:
        return self_test()
    return main_run(args.root, args.apply, args.check)


if __name__ == '__main__':
    sys.exit(main(sys.argv))
