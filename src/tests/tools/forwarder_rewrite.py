#!/usr/bin/env python3
"""forwarder_rewrite.py [--root <repo root>] [--apply] [--check] | --self-test

Decoupling D4i: drop the character's pure forwarders to its managers and rewrite their callers to
call the manager. A forwarder is PURE when its body is `return m_xMgr.Method(args);` or
`m_xMgr.Method(args);` with the forwarder's own parameters passed through unchanged and in order
-- nothing added by the owner (no sink, clock, guid, spec, lookup or owner fact) -- AND the
forwarders' OVERLOAD SET is the manager method's, one to one: the same return types, parameter
types (reference-ness included), defaults and constness. Both sets are read from the class's own
scope (class Player in Player.h, the manager class in its header: not another class of the same
file, not an inline body or a nested class), and every token of the name at that scope must parse
as a declaration, else the tool stops: a declaration the parser cannot read never shrinks a set.
Two sets whose members would collide (two keys the same) are refused too. Constness may differ in
two shapes only, and only when NO forwarder of the name is const, so every old caller held a
non-const character: a non-const forwarder over the manager's const method with no non-const
twin, and a manager const/non-const pair under the non-const forwarder (`Map()`). Such a call
resolves to the same function whether the character forwards it or the caller makes it, so each
rewrite below is the forwarder's body inlined at the call.

A forwarder has one of three shapes:
  inline, on one line of Player.h      `ret Name(params) const { return m_xMgr.M(args); }`
  inline, over several lines           `ret Name(params)` / `{` / body / `}` or `};`
  out of line                          `ret Name(params) const;` in Player.h, and exactly one
                                       `ret Player::Name(params) const { ... }` definition (for an
                                       overload set: the one with the declaration's parameter
                                       types), in an owner file, agreeing on the return type, the
                                       parameter types and constness.
A STATIC forwarder (D4i-3: the inventory's position checks) has the same shapes with the body
`return Mgr::M(args);`; `static` is part of the set comparison (a static forwarder stands only
for a static manager method), and a set mixing static and member forwarders is refused.
A name's set is dropped whole or not at all. The dropped lines take with them the comment lines
standing directly above them (a doc block over a definition, a `//` line over a declaration) and
one blank line, so no orphaned comment and no double blank line is left. A comment over dropped
lines that does not stand alone with them (code directly above the comment, or kept code
directly below the dropped lines) would be left over that code, and a comment separated from
them by blank lines would be left heading nothing when only a closing brace, another comment or
the end follows them: in both cases the tool stops (ERROR).

The table (FORWARDERS) names every forwarder this tool drops, with its manager member, the
character's accessor to the manager and the manager's method (the names differ for the
currencies and the talents). For each one:

  1. The forwarders are found in class Player (every declaration of the name there, in one of
     the three shapes). Each body must match the pure shape above, and the set is compared with
     the manager class's set (types normalised for spacing and `const T` == `T const`, defaults
     compared as written). Anything else stops the tool (IMPURE or ERROR) before anything is
     written. A pure set is deleted (its declarations and out-of-line definitions).
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
     For a static set the bare and this-> calls take the body instead (`Mgr::Method(`, which a
     static member of the owner may call too), and so does a plain qualified call:
       Player::Name( ->  Mgr::Method(           (not after `->`, `.`, `::` or `&`)
     A receiver that already is the manager (`m_xMgr.Name(`, `Accessor().Name(`) is left alone.
     The owner files are CheckManagerIsolation.cmake's OWNER_FILES (read from the gate, so the
     lists cannot drift): the character's own members call the member directly. A bare call
     anywhere else, and any other `Player::Name` outside the forwarder's own definition (a
     member's qualified call, a pointer), is an ERROR listed for a hand rewrite.

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
same-named local lambda or function object called bare inside an owner file would be rewritten;
a manager `using Base::Name;` (or an inherited overload) is not a `Name(` token, so a base class's
overloads are invisible to the set comparison (no manager has a base class today).
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
                 ('src/game/spells/SpellCooldownMgr.h',
                  'src/game/spells/SpellCooldownMgr.cpp')),
    'pet': ('m_petMgr', 'GetPetMgr', 'PetMgr',
            ('src/game/entities/player/pets/PetMgr.h',
             'src/game/entities/player/pets/PetMgr.cpp')),
    'currency': ('m_currencyMgr', 'GetCurrencyMgr', 'CurrencyMgr',
                 ('src/game/entities/player/inventory/CurrencyMgr.h',
                  'src/game/entities/player/inventory/CurrencyMgr.cpp')),
    'talent': ('m_talentMgr', 'GetTalentMgr', 'TalentMgr',
               ('src/game/entities/player/talents/TalentMgr.h',
                'src/game/entities/player/talents/TalentMgr.cpp')),
    'quest': ('m_questStatusMgr', 'GetQuestStatusMgr', 'QuestStatusMgr',
              ('src/game/entities/player/quests/QuestStatusMgr.h',
               'src/game/entities/player/quests/QuestStatusMgr.cpp')),
    'inventory': ('m_inventoryMgr', 'GetInventoryMgr', 'InventoryMgr',
                  ('src/game/entities/player/inventory/InventoryMgr.h',
                   'src/game/entities/player/inventory/InventoryMgr.cpp')),
}

# (the forwarder's name, domain, the manager's method); an overload set is compared whole
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
    # D4i-2: the quest status forwarders (out of line in PlayerQuest.cpp, inline in Player.h)
    ('IsActiveQuest', 'quest', 'IsActiveQuest'),
    ('IsCurrentQuest', 'quest', 'IsCurrentQuest'),
    ('GetQuestStatus', 'quest', 'GetQuestStatus'),
    ('ResetWeeklyQuestStatus', 'quest', 'ResetWeeklyQuestStatus'),
    ('ResetMonthlyQuestStatus', 'quest', 'ResetMonthlyQuestStatus'),
    ('RemoveTimedQuest', 'quest', 'RemoveTimedQuest'),
    ('getQuestStatusMap', 'quest', 'Map'),
    # D4i-3: the inventory lookups and counts (out of line in PlayerItem*.cpp)
    ('IsValidPos', 'inventory', 'IsValidPos'),
    ('HasItemCount', 'inventory', 'HasItemCount'),
    ('GetItemCount', 'inventory', 'GetItemCount'),
    ('GetItemByEntry', 'inventory', 'GetItemByEntry'),
    ('GetItemByLimitedCategory', 'inventory', 'GetItemByLimitedCategory'),
    ('GetItemByPos', 'inventory', 'GetItemByPos'),
    ('GetItemDisplayIdInSlot', 'inventory', 'GetItemDisplayIdInSlot'),
    ('GetItemFromBuyBackSlot', 'inventory', 'GetItemFromBuyBackSlot'),
    # the static position checks (inline in Player.h, out of line in PlayerItem.cpp)
    ('IsInventoryPos', 'inventory', 'IsInventoryPos'),
    ('IsEquipmentPos', 'inventory', 'IsEquipmentPos'),
    ('IsBagPos', 'inventory', 'IsBagPos'),
    ('IsBankPos', 'inventory', 'IsBankPos'),
]

# The HAND sites that stay: calls of a shared name on the other class (Pet's own talent points,
# a vendor list's or a bag's own items).
# (name, file) -> the number of listed sites. --check fails on any other count.
EXPECTED_HAND = {
    ('GetFreeTalentPoints', 'src/game/entities/player/talents/PlayerTalent.cpp'): 2,   # pet->
    ('SetFreeTalentPoints', 'src/game/ChatCommands/PlayerStatsMods.cpp'): 1,            # ((Pet*)target)->
    ('SetFreeTalentPoints', 'src/game/Object/PetSpells.cpp'): 3,                        # Pet members, bare
    # D4i-3: the vendor lists' own count (VendorItemData const*) and a bag's own slots (Bag*)
    ('GetItemCount', 'src/game/Object/ObjectMgr.cpp'): 4,                               # vItems/tItems->
    ('GetItemCount', 'src/game/WorldHandlers/ItemHandlerVendor.cpp'): 2,                # vItems/tItems->
    ('GetItemCount', 'src/game/entities/player/interaction/PlayerVendor.cpp'): 4,       # vItems/tItems->
    ('GetItemByPos', 'src/game/ChatCommands/DebugCommands.cpp'): 2,                     # bag->
    ('GetItemByPos', 'src/game/entities/player/inventory/PlayerGearScore.cpp'): 2,      # pBag->, bag->
    ('GetItemByPos', 'src/game/entities/player/inventory/PlayerItemEnchant.cpp'): 1,    # pBag->
    ('GetItemByPos', 'src/game/entities/player/inventory/PlayerItemStorage.cpp'): 6,    # pBag->, fullBag->
}

KEYWORDS = {'return', 'else', 'case', 'throw', 'new', 'delete', 'co_return', 'do', 'goto',
            'sizeof', 'typeid', 'not', 'and', 'or', 'if', 'while', 'for', 'switch'}
SPECIFIERS = {'virtual', 'static', 'inline', 'explicit', 'constexpr', 'friend', 'extern'}
# words that end a type and are never a parameter's name (`unsigned int`, `long long`)
TYPE_WORDS = {'const', 'volatile', 'int', 'long', 'short', 'char', 'signed', 'unsigned', 'double',
              'float', 'bool', 'void', 'wchar_t', 'char8_t', 'char16_t', 'char32_t', 'auto'}
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
    """[(normalised type, name or None, normalised default or None)] of a parameter list. A
    parameter without a name (`uint32`, `unsigned int`, `Quest const*`) is all type."""
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
                and toks[-1] not in TYPE_WORDS:
            name = toks.pop()
        out.append((norm_type(join_tokens(toks)), name, default))
    return out


def is_static(ret):
    """True when a declaration's text before its name holds `static`."""
    return 'static' in TOKEN.findall(ret)


def body_verdict(ret, params, body, member, method, static_class=None):
    """None if <body> is the pure pass-through of <params> to member.method (to
    static_class::method for a static forwarder); else the reason."""
    target, shown = re.escape(member) + r'\s*\.\s*', member + '.'
    if static_class is not None:
        target, shown = re.escape(static_class) + r'\s*::\s*', static_class + '::'
    call = re.match(r'^(?P<kw>return\s+)?' + target + re.escape(method) +
                    r'\s*\((?P<args>[^()]*)\)\s*;$', body)
    if not call:
        return 'the body is not one call of %s%s: {%s}' % (shown, method, body)
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


DECL_TAIL = re.compile(r'\s*(const\b)?\s*(?:override\b\s*)?(?:=\s*(?:0|delete|default)\s*)?([;{])')


def class_scope_decls(text, clean, nocom, path, cls, name):
    """([{pos, where, ret, params, const}], error) of every declaration of <name> in class <cls>'s
    own scope in <path> (not another class of the file, not an inline body or a nested class).
    Every `name(` token at that scope must parse as a declaration (a type, then `(...)` [const]
    and `;` or `{`), else the error names it: a declaration the parser misses (an attribute, the
    return type on the line above, a macro) must not silently shrink the set."""
    span = class_body(clean, cls)
    if span is None:
        return None, 'no class %s in %s' % (cls, path)
    open_, close = span
    tok = re.compile(r'(?<![\w])' + re.escape(name) + r'\s*\(')
    starts = {m.start() for m in tok.finditer(clean, open_ + 1, close)}
    out, depth, paren = [], 0, 0
    for i in range(open_ + 1, close):
        if i in starts and depth == 0 and paren == 0:
            where = '%s:%d' % (path, line_of(text, i))
            popen = clean.index('(', i)
            pclose = match_close(clean, popen, '(', ')')
            tail = DECL_TAIL.match(clean, pclose + 1) if pclose > 0 else None
            if not is_declaration(clean, i) or not tail:
                return None, '%s::%s at %s is not a declaration this tool can read: %s' % (
                    cls, name, where, line_text(text, i).strip())
            out.append(dict(pos=i, where=where, ret=statement_prefix(clean, i).strip(),
                            params=parse_params(nocom[popen + 1:pclose]), const=bool(tail.group(1))))
        ch = clean[i]
        if ch == '{':
            depth += 1
        elif ch == '}':
            depth -= 1
        elif ch == '(':
            paren += 1
        elif ch == ')':
            paren -= 1
    return out, None


def _key(ret, params, is_const):
    return (norm_type(ret), tuple((p[0], p[2]) for p in params), bool(is_const), is_static(ret))


def _signature(k):
    return '%s%s(%s)%s' % ('static ' if k[3] else '', k[0],
                           ', '.join(ty + ('=' + d if d else '') for ty, d in k[1]), ' const' if k[2] else '')


def overload_set_mismatch(fwds, decls, mgr_class, method):
    """None when the forwarders' overload set is the manager method's, one to one: the same
    return types, parameter types, defaults and constness, so every call resolves to the same
    function whether it names the character or the manager; else the reason. <fwds> and <decls>
    are [(ret, params, is const)]; `static` is part of the key (read from ret), so a static
    forwarder stands only for a static manager method and a member one only for a member one.
    Constness may differ only when no forwarder of the name is
    const (every old caller then held a non-const character): a non-const forwarder may stand
    for the manager's const method when the manager has no non-const twin (the call reaches the
    only candidate), and a manager const/non-const pair may stand under a non-const forwarder
    (the call reaches the non-const one, as the forwarder's body did)."""
    fk = [_key(*f) for f in fwds]
    mk = [_key(*d) for d in decls]
    if len(set(fk)) != len(fk) or len(set(mk)) != len(mk):
        return 'two declarations of %s read the same (%s)' % (
            method, '; '.join(_signature(k) for k in (fk if len(set(fk)) != len(fk) else mk)))
    lenient = not any(k[2] for k in fk)
    matched = set()
    for k in fk:
        if k in mk:
            matched.add(k)
            continue
        as_const = (k[0], k[1], True, k[3])
        if lenient and as_const in mk:
            # a non-const twin of another return type is left unmatched: refused below
            matched.add(as_const)
            continue
        return 'no %s::%s declaration is the forwarder %s (the manager has %s)' % (
            mgr_class, method, _signature(k), '; '.join(_signature(m) for m in mk) or 'none')
    for m in mk:
        if m in matched:
            continue
        # the const half of a pair: a non-const forwarder with the same parameters matched the
        # non-const half (or it would have failed above)
        pair = lenient and m[2] and any(f[1] == m[1] and not f[2] for f in fk)
        if not pair:
            return '%s::%s has an overload the forwarders lack: %s' % (mgr_class, method, _signature(m))
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


def forwarder_shape(texts, clean, nocom, files, owners, name, pos):
    """The forwarder declared in Player.h at <pos>: a dict (kind, where, raw, ret, params,
    names_from, body, const, drops, defined), or a string saying why it is not a forwarder."""
    ptext, pclean, pnc = texts[PLAYER_H], clean[PLAYER_H], nocom(PLAYER_H)
    where = '%s:%d' % (PLAYER_H, line_of(ptext, pos))
    op = pclean.index('(', pos)
    cl = match_close(pclean, op, '(', ')')
    after = DECL_TAIL.match(pclean, cl + 1) if cl > 0 else None
    if not after:
        return 'a declaration that is not a forwarder: %s' % line_text(ptext, pos).strip()
    is_const = bool(after.group(1))
    prefix = statement_prefix(pclean, pos)
    decl_start = line_start(ptext, pos)
    if pclean[decl_start:pos].strip() != prefix.strip():
        return 'the declaration shares its line: %s' % line_text(ptext, pos).strip()
    ret = prefix.strip()
    params = parse_params(pnc[op + 1:cl])
    raw = ' '.join(ptext[decl_start:after.end()].split())
    if after.group(2) == '{':
        ob = after.end() - 1
        cb = match_close(pclean, ob, '{', '}')
        if cb < 0:
            return 'an unbalanced body'
        tail = re.compile(r'[ \t]*;?[ \t]*(\n|$)').match(pclean, cb + 1)
        if not tail:
            return 'text follows the definition on its line: %s' % line_text(ptext, cb).strip()
        return dict(kind='inline' if '\n' not in ptext[decl_start:cb] else 'inline (multi-line)',
                    where=where, raw=raw, ret=ret, params=params, names_from=params,
                    body=' '.join(pnc[ob + 1:cb].split()), const=is_const,
                    drops=[(PLAYER_H, decl_start, tail.end())], defined=None)
    tail = re.compile(r'[ \t]*(\n|$)').match(pclean, after.end())
    if not tail:
        return 'text follows the declaration on its line: %s' % line_text(ptext, pos).strip()
    defs = []
    dpat = re.compile(r'(?<![\w])Player\s*::\s*' + re.escape(name) + r'\s*\(')
    for f in files:
        if f.startswith(NOT_SCANNED):
            continue
        c = clean[f]
        for dm in dpat.finditer(c):
            dop = dm.end() - 1
            dcl = match_close(c, dop, '(', ')')
            if dcl < 0:
                continue
            bm = re.compile(r'\s*(const\b)?\s*\{').match(c, dcl + 1)
            if bm and is_declaration(c, dm.start()):
                defs.append((f, dm, dop, dcl, bm))
    if not defs:
        return 'declared out of line, but no `ret Player::%s(...) {` definition found' % name
    types = [p[0] for p in params]
    same = [d for d in defs if [p[0] for p in parse_params(nocom(d[0])[d[2] + 1:d[3]])] == types]
    if not same and len(defs) > 1:
        return 'no Player::%s definition takes (%s)' % (name, ', '.join(types))
    defs = same or defs                 # an overload set: the definition of this declaration
    if len(defs) > 1:
        return 'more than one Player::%s definition: %s' % (
            name, ', '.join('%s:%d' % (d[0], line_of(texts[d[0]], d[1].start())) for d in defs))
    f, dm, dop, dcl, bm = defs[0]
    c, t = clean[f], texts[f]
    defined = '%s:%d' % (f, line_of(t, dm.start()))
    if f not in owners:
        return 'the definition %s is not in an owner file' % defined
    def_params = parse_params(nocom(f)[dop + 1:dcl])
    def_ret = statement_prefix(c, dm.start()).strip()
    if norm_type(def_ret) != norm_type(ret) or [p[0] for p in def_params] != types \
            or bool(bm.group(1)) != is_const:
        return 'the declaration and the definition %s differ' % defined
    dstart = line_start(t, dm.start())
    if c[dstart:dm.start()].strip() != def_ret:
        return 'the definition %s shares its line' % defined
    ob = bm.end() - 1
    cb = match_close(c, ob, '{', '}')
    if cb < 0:
        return 'the definition %s has an unbalanced body' % defined
    dtail = re.compile(r'[ \t]*(\n|$)').match(c, cb + 1)
    if not dtail:
        return 'text follows the definition %s on its line' % defined
    return dict(kind='out-of-line', where=where, raw=raw, ret=ret, params=params, names_from=def_params,
                body=' '.join(nocom(f)[ob + 1:cb].split()), const=is_const,
                drops=[(PLAYER_H, decl_start, tail.end()), (f, dstart, dtail.end())], defined=defined)


def drop_edits(text, ranges):
    """The deletions for one file's dropped forwarders: each whole-line range widened over the
    comment lines directly above it (when they stand alone: a blank line or the block's opener
    above them and a blank line, a closing brace or the end below); ranges separated by nothing
    but blank lines merged; then one blank line taken with the merged range when it stood as its
    own paragraph, so neither an orphaned comment nor a double blank line is left. Returns
    ([(start, end)] character ranges, [(first, last comment line, the line below the range)]):
    the second list holds every comment directly over a range that does not stand alone with
    it (code above the comment, or kept code directly below the range), and every comment
    separated from a range by blank lines when only a closing brace, another comment or the end
    follows the range (it would be left heading nothing); dropping that range would orphan the
    comment, so the caller refuses it. A line starting with `*` is a comment line only inside a
    block opened by a `/*` line above it."""
    lines = text.splitlines(True)
    starts = [0]
    for ln in lines:
        starts.append(starts[-1] + len(ln))
    n = len(lines)

    def blank(i):
        return 0 <= i < n and lines[i].strip() == ''

    def opener(i):
        return i < 0 or (0 <= i < n and re.search(r'[{:]\s*$', lines[i]) is not None)

    def closer(i):
        return i >= n or lines[i].strip().startswith('}')

    def merge(spans):
        merged = []
        for a, b in sorted(spans):
            if merged and all(blank(i) for i in range(merged[-1][1], a)):
                merged[-1][1] = max(merged[-1][1], b)
            else:
                merged.append([a, b])
        return merged

    def comment_top(i):
        """The first line of the comment block ending at line i, or None when line i is not a
        comment. A line starting with `*` counts only inside a block opened by a `/*` line
        above it (else it is code: a dereference)."""
        top = None
        while 0 <= i < n:
            if re.match(r'\s*(//|/\*)', lines[i]):
                top, i = i, i - 1
                continue
            if re.match(r'\s*\*', lines[i]):
                j = i
                while j >= 0 and re.match(r'\s*\*', lines[j]) and not re.match(r'\s*/\*', lines[j]):
                    j -= 1
                if j >= 0 and re.match(r'\s*/\*', lines[j]):
                    top, i = j, j - 1
                    continue
            break
        return top

    spans = [[text.count('\n', 0, s), n if e == len(text) else text.count('\n', 0, e)] for s, e in ranges]
    widened, orphans = [], []
    for a, b in merge(spans):
        k = comment_top(a - 1)
        k = a if k is None else k
        if k < a and (blank(k - 1) or opener(k - 1)) and (blank(b) or closer(b)):
            a = k
        elif k < a:
            # the comment does not stand alone with the dropped lines: dropping them would leave
            # it over whatever follows (kept code), so the layout is refused
            orphans.append((k + 1, a, b + 1))
        widened.append([a, b])
    out = []
    for a, b in merge(widened):
        # a comment separated from the dropped lines by blank lines heads them only when nothing
        # but a closing brace, another comment or the end follows them: then it would be left
        # heading nothing, so the layout is refused (kept code below it is what it heads)
        j = a - 1
        while blank(j):
            j -= 1
        top = comment_top(j) if j < a - 1 else None
        m = b
        while blank(m):
            m += 1
        if top is not None and (closer(m) or re.match(r'\s*(//|/\*)', lines[m])):
            orphans.append((top + 1, j + 1, m + 1))
        if (blank(a - 1) or opener(a - 1)) and blank(b):
            b += 1
        elif blank(a - 1) and closer(b):
            a -= 1
        out.append((starts[a], starts[b]))
    return out, orphans


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
    drops = {}           # file -> [(start, end)]: the dropped declarations and definitions
    drop_count = 0
    failed = set()
    static_names = set()     # names whose forwarders are static: their calls go to Mgr::Method

    def in_drop(f, pos):
        return any(s <= pos < e for s, e in drops.get(f, []))

    # 1. the forwarders, every name first (a call inside a dropped range is not a call): the
    #    Player set, each body, then the set against the manager's; dropped whole or not at all
    for name, domain, method in forwarders:
        member, accessor, mgr_class, mgr_files = domains[domain]
        if PLAYER_H not in texts:
            continue
        pdecls, err = class_scope_decls(texts[PLAYER_H], clean[PLAYER_H], nocom(PLAYER_H), PLAYER_H,
                                        'Player', name)
        if err:
            errors.append('[%s] %s: %s' % (domain, name, err))
            failed.add(name)
            continue
        shapes = []
        for d in pdecls:
            shape = forwarder_shape(texts, clean, nocom, files, owners, name, d['pos'])
            if isinstance(shape, str):
                errors.append('%s: [%s] %s: %s' % (d['where'], domain, name, shape))
                continue
            reason = body_verdict(shape['ret'], shape['names_from'], shape['body'], member, method,
                                  mgr_class if is_static(shape['ret']) else None)
            if reason is not None:
                errors.append('%s: [%s] %s: IMPURE: %s' % (d['where'], domain, name, reason))
                continue
            shapes.append(shape)
        if len(shapes) != len(pdecls):
            failed.add(name)
            continue
        if not shapes:
            continue                            # dropped already: only stale calls remain
        if len({is_static(s['ret']) for s in shapes}) > 1:
            errors.append('%s: [%s] %s: a set mixing static and member forwarders'
                          % (shapes[0]['where'], domain, name))
            failed.add(name)
            continue
        mgr_h = mgr_files[0]
        if mgr_h not in texts:
            mdecls, reason = None, 'no manager header %s' % mgr_h
        else:
            mdecls, reason = class_scope_decls(texts[mgr_h], clean[mgr_h], nocom(mgr_h), mgr_h, mgr_class,
                                               method)
        if reason is None:
            reason = overload_set_mismatch([(s['ret'], s['params'], s['const']) for s in shapes],
                                           [(m['ret'], m['params'], m['const']) for m in mdecls],
                                           mgr_class, method)
        if reason is not None:
            errors.append('%s: [%s] %s: IMPURE: %s' % (shapes[0]['where'], domain, name, reason))
            failed.add(name)
            continue
        if is_static(shapes[0]['ret']):
            static_names.add(name)
        for s in shapes:
            for f, a, b in s['drops']:
                drops.setdefault(f, []).append((a, b))
                drop_count += 1
            report.append('%s: [%s] %s: DROP %s forwarder `%s`%s; its body `%s`'
                          % (s['where'], domain, name, s['kind'], s['raw'],
                             ' defined at %s' % s['defined'] if s['defined'] else '', s['body']))

    for name, domain, method in forwarders:
        if name in failed:
            continue
        member, accessor, mgr_class, mgr_files = domains[domain]
        pat = re.compile(r'(?<![\w])' + re.escape(name) + r'\s*\(')
        mgr_h = mgr_files[0]
        mspan = class_body(clean[mgr_h], mgr_class) if mgr_h in clean else None

        # 2. uniqueness: a declaration of the name by any other class
        shared = []
        for f in files:
            c = clean[f]
            for m in pat.finditer(c):
                if in_drop(f, m.start()):
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
        static = name in static_names
        # the forwarder's body at a bare, this-> or qualified call: the member, or the class
        body_call = mgr_class + '::' + method if static else member + '.' + method
        for f in files:
            if f.startswith(NOT_SCANNED) or f in mgr_files:
                continue
            c, t = clean[f], texts[f]
            for m in qualified.finditer(c):
                if in_drop(f, m.start()):
                    continue
                # a static forwarder's plain `Player::Name(` call is the body's `Mgr::Method(`;
                # through a receiver (`u->Player::Name(`), after a scope (`X::Player::`), after
                # `&` or without the call (a pointer) it stays an error
                head = c[max(0, m.start() - LOOK_BEHIND):m.start()]
                if static and not shared and re.match(r'\s*\(', c[m.end():]) \
                        and not re.search(r'(->|\.|::|&)\s*$', head):
                    edits.setdefault(f, []).append((m.start(), m.end(), body_call, name, 'qualified'))
                    continue
                errors.append('%s:%d: [%s] %s: Player::%s: rewrite by hand: %s'
                              % (f, line_of(t, m.start()), domain, name, name,
                                 line_text(t, m.start()).strip()))
            for m in pat.finditer(c):
                if in_drop(f, m.start()):
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
                        edits.setdefault(f, []).append((base + this.start(), name_end, body_call, name, 'this'))
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
                edits.setdefault(f, []).append((m.start(), name_end, body_call, name, 'bare'))

    unexpected = []
    if expected_hand is not None:
        for key in sorted(set(hand_count) | set(expected_hand)):
            got, want = hand_count.get(key, 0), expected_hand.get(key, 0)
            if got != want:
                unexpected.append('%s in %s: %d HAND site(s) listed, %d pinned' % (key[0], key[1], got, want))

    for f in drops:
        ranges, orphans = drop_edits(texts[f], drops[f])
        for first, last, below in orphans:
            errors.append('%s:%d-%d: the comment over dropped lines would be left over line %d: '
                          'rewrite by hand' % (f, first, last, below))
        for s, e in ranges:
            edits.setdefault(f, []).append((s, e, '', None, 'drop'))

    domain_of = {e[0]: e[1] for e in forwarders}
    changes = drop_count                    # one per dropped declaration or definition
    results = {}
    for f in sorted(edits):
        t = texts[f]
        ordered = sorted(edits[f], key=lambda e: e[0])
        for (s1, e1, _, n1, _), (s2, e2, _, n2, _) in zip(ordered, ordered[1:]):
            if s2 < e1:
                errors.append('%s:%d: overlapping edits (%s, %s): nothing written'
                              % (f, line_of(t, s2), n1, n2))
        new = t
        for start, end, repl, name, kind in reversed(ordered):
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
        results[f] = new
    for f in sorted(results):
        if apply and not errors and results[f] != texts[f]:
            write(os.path.join(root, f), results[f])
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

# D4i-2: the out-of-line and multi-line shapes and the signature check (the quest forwarders)
SELF_Q_DOMAINS = {
    'quest': ('m_questStatusMgr', 'GetQuestStatusMgr', 'QuestStatusMgr', ('src/game/q/QuestStatusMgr.h',)),
}
SELF_Q_FORWARDERS = [
    ('IsCurrentQuest', 'quest', 'IsCurrentQuest'),
    ('GetQuestStatus', 'quest', 'GetQuestStatus'),
    ('ResetWeeklyQuestStatus', 'quest', 'ResetWeeklyQuestStatus'),
    ('ResetMonthlyQuestStatus', 'quest', 'ResetMonthlyQuestStatus'),
    ('RemoveTimedQuest', 'quest', 'RemoveTimedQuest'),
    ('getQuestStatusMap', 'quest', 'Map'),
]
SELF_Q_PLAYER_H = '''class Player
{
    public:
        // Prepare the menu
        void PrepareQuestMenu(ObjectGuid guid);

        // Quest is taken and not yet rewarded
        // if completed_or_not = 1 - taken, not completed
        bool IsCurrentQuest(uint32 quest_id, uint8 completed_or_not = 0) const; // taken

        // Get the quest status
        QuestStatus GetQuestStatus(uint32 quest_id) const;

        // Set the daily quest status
        void SetDailyQuestStatus(uint32 quest_id);
        void ResetWeeklyQuestStatus();
        void ResetMonthlyQuestStatus();

        // Remove a timed quest
        void RemoveTimedQuest(uint32 quest_id) { m_questStatusMgr.RemoveTimedQuest(quest_id); }

        // Get the player's quest status map
        QuestStatusMap& getQuestStatusMap()
        {
            return m_questStatusMgr.Map();
        };

        // Get the reward status
        bool GetQuestRewardStatus(uint32 quest_id) const;
        QuestStatusMgr m_questStatusMgr;
};
'''
SELF_Q_PLAYER_H_AFTER = '''class Player
{
    public:
        // Prepare the menu
        void PrepareQuestMenu(ObjectGuid guid);

        // Set the daily quest status
        void SetDailyQuestStatus(uint32 quest_id);

        // Get the reward status
        bool GetQuestRewardStatus(uint32 quest_id) const;
        QuestStatusMgr m_questStatusMgr;
};
'''
SELF_Q_MGR_H = '''class QuestStatusMgr
{
    public:
        QuestStatusMap& Map() { return m_status; }
        QuestStatusMap const& Map() const { return m_status; }
        bool IsCurrentQuest(uint32 quest_id, uint8 completed_or_not = 0) const;
        QuestStatus GetQuestStatus(uint32 quest_id) const;
        bool GetQuestRewardStatus(uint32 quest_id, TemplateLookup const& lookup) const;
        void RemoveTimedQuest(uint32 quest_id) { m_timedQuests.erase(quest_id); }
        void ResetWeeklyQuestStatus();
        void ResetMonthlyQuestStatus();
};
'''
SELF_Q_OWNER = '''#include "Player.h"

/**
 * @brief Checks whether a quest is current.
 */
bool Player::IsCurrentQuest(uint32 quest_id, uint8 completed_or_not) const
{
    return m_questStatusMgr.IsCurrentQuest(quest_id, completed_or_not);
}

void Player::Take(uint32 quest_id)
{
    QuestStatus status = GetQuestStatus(quest_id);
    RemoveTimedQuest(quest_id);
    this->ResetWeeklyQuestStatus();
    bool cur = IsCurrentQuest(quest_id);
    uint32 t = ((Player*)giver)->getQuestStatusMap()[quest_id].m_timer;
}

/**
 * @brief Gets the quest status.
 */
QuestStatus Player::GetQuestStatus(uint32 quest_id) const
{
    return m_questStatusMgr.GetQuestStatus(quest_id);
}

void Player::ResetWeeklyQuestStatus()
{
    m_questStatusMgr.ResetWeeklyQuestStatus();
}

void Player::ResetMonthlyQuestStatus()
{
    m_questStatusMgr.ResetMonthlyQuestStatus();
}
'''
SELF_Q_OWNER_AFTER = '''#include "Player.h"

void Player::Take(uint32 quest_id)
{
    QuestStatus status = m_questStatusMgr.GetQuestStatus(quest_id);
    m_questStatusMgr.RemoveTimedQuest(quest_id);
    m_questStatusMgr.ResetWeeklyQuestStatus();
    bool cur = m_questStatusMgr.IsCurrentQuest(quest_id);
    uint32 t = ((Player*)giver)->GetQuestStatusMgr().Map()[quest_id].m_timer;
}
'''
SELF_Q_OUTSIDE = '''void Handler(Player* p, Player const* cp)
{
    if (p->GetQuestStatus(1) == 0 && cp->IsCurrentQuest(2, 1)) {}
    p->getQuestStatusMap()[3].m_rewarded = false;
    for (auto i = p->getQuestStatusMap().begin(); i != p->getQuestStatusMap().end(); ++i) {}
    p->GetQuestRewardStatus(4);
    sWorld.GetPlayer()->ResetMonthlyQuestStatus(); // p->GetQuestStatus(5) in a comment
}
'''
SELF_Q_OUTSIDE_AFTER = '''void Handler(Player* p, Player const* cp)
{
    if (p->GetQuestStatusMgr().GetQuestStatus(1) == 0 && cp->GetQuestStatusMgr().IsCurrentQuest(2, 1)) {}
    p->GetQuestStatusMgr().Map()[3].m_rewarded = false;
    for (auto i = p->GetQuestStatusMgr().Map().begin(); i != p->GetQuestStatusMgr().Map().end(); ++i) {}
    p->GetQuestRewardStatus(4);
    sWorld.GetPlayer()->GetQuestStatusMgr().ResetMonthlyQuestStatus(); // p->GetQuestStatus(5) in a comment
}
'''
SELF_Q_OWNER_PATH = 'src/game/entities/player/quests/PlayerQuest.cpp'
SELF_Q_OWNERS = {PLAYER_H, SELF_Q_OWNER_PATH}


def _q_files():
    return {
        PLAYER_H: SELF_Q_PLAYER_H,
        'src/game/q/QuestStatusMgr.h': SELF_Q_MGR_H,
        SELF_Q_OWNER_PATH: SELF_Q_OWNER,
        'src/game/Handlers/QuestHandler.cpp': SELF_Q_OUTSIDE,
    }


# D4i-3: static forwarders (the inventory's position checks) beside a member one
SELF_S_DOMAINS = {
    'inv': ('m_invMgr', 'GetInvMgr', 'InvMgr', ('src/game/i/InvMgr.h',)),
}
SELF_S_FORWARDERS = [
    ('IsBagPos', 'inv', 'IsBagPos'),
    ('IsInvPos', 'inv', 'IsInvPos'),
    ('GetItemAt', 'inv', 'GetItemAt'),
]
SELF_S_PLAYER_H = '''class Player
{
    public:
        // Check a bag position
        static bool IsBagPos(uint16 pos);

        // Check an inventory position
        static bool IsInvPos(uint16 pos) { return InvMgr::IsInvPos(pos); }
        static bool IsInvPos(uint8 bag, uint8 slot);

        Item* GetItemAt(uint8 slot) const;
        bool TwoHand() const { return IsInvPos(1) && this->IsBagPos(2); }
        InvMgr m_invMgr;
};
'''
SELF_S_PLAYER_H_AFTER = '''class Player
{
    public:
        bool TwoHand() const { return InvMgr::IsInvPos(1) && InvMgr::IsBagPos(2); }
        InvMgr m_invMgr;
};
'''
SELF_S_MGR_H = '''class InvMgr
{
    public:
        static bool IsBagPos(uint16 pos);
        static bool IsInvPos(uint16 pos) { return IsInvPos(pos >> 8, pos & 255); }
        static bool IsInvPos(uint8 bag, uint8 slot);
        Item* GetItemAt(uint8 slot) const;
};
'''
SELF_S_OWNER = '''#include "Player.h"

bool Player::IsBagPos(uint16 pos)
{
    return InvMgr::IsBagPos(pos);
}

bool Player::IsInvPos(uint8 bag, uint8 slot)
{
    return InvMgr::IsInvPos(bag, slot);
}

Item* Player::GetItemAt(uint8 slot) const
{
    return m_invMgr.GetItemAt(slot);
}

void Player::Store(uint16 pos)
{
    if (IsBagPos(pos) || this->IsInvPos(pos) || Player::IsInvPos(1, 2))
        Item* it = GetItemAt(3);
    bool b = InvMgr::IsBagPos(pos);
}
'''
SELF_S_OWNER_AFTER = '''#include "Player.h"

void Player::Store(uint16 pos)
{
    if (InvMgr::IsBagPos(pos) || InvMgr::IsInvPos(pos) || InvMgr::IsInvPos(1, 2))
        Item* it = m_invMgr.GetItemAt(3);
    bool b = InvMgr::IsBagPos(pos);
}
'''
SELF_S_OUTSIDE = '''void Handler(Player* p, Player const* cp)
{
    if (Player::IsBagPos(1) && p->IsInvPos(2) && cp->IsInvPos(3, 4)) {}
    Item* i = cp->GetItemAt(5); // Player::IsBagPos(6) in a comment
    const char* s = "Player::IsBagPos(7)";
}
'''
SELF_S_OUTSIDE_AFTER = '''void Handler(Player* p, Player const* cp)
{
    if (InvMgr::IsBagPos(1) && p->GetInvMgr().IsInvPos(2) && cp->GetInvMgr().IsInvPos(3, 4)) {}
    Item* i = cp->GetInvMgr().GetItemAt(5); // Player::IsBagPos(6) in a comment
    const char* s = "Player::IsBagPos(7)";
}
'''
SELF_S_OWNER_PATH = 'src/game/entities/player/inventory/PlayerItem.cpp'
SELF_S_OWNERS = {PLAYER_H, SELF_S_OWNER_PATH}


SELF_S_SPACED = ('        Item* GetItemAt(uint8 slot) const;\n',
                 '        Item* GetItemAt(uint8 slot) const;\n\n')


def _s_files(spaced=True):
    """The static fixture; spaced: with a blank line between the dropped block and the kept
    member below it (as written, the block's comment would be orphaned: refused)."""
    return {
        PLAYER_H: SELF_S_PLAYER_H.replace(*SELF_S_SPACED) if spaced else SELF_S_PLAYER_H,
        'src/game/i/InvMgr.h': SELF_S_MGR_H,
        SELF_S_OWNER_PATH: SELF_S_OWNER,
        'src/game/Handlers/ItemHandler.cpp': SELF_S_OUTSIDE,
    }


def _srun(root, apply=False):
    return run(root, SELF_S_FORWARDERS, SELF_S_DOMAINS, SELF_S_OWNERS, apply, None)


def _qrun(root, apply=False):
    return run(root, SELF_Q_FORWARDERS, SELF_Q_DOMAINS, SELF_Q_OWNERS, apply, None)


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

    # 3b. accepted: `const T&` == `T const&`. (D4i-1's second row, "an overload named in the
    #     table", went with the table's overload element: the whole set is compared, and that
    #     fixture -- a manager overload Player lacks -- is group 3's refused "an overloaded
    #     manager method".)
    accepted = [
        (PLAYER_H, 'RuneInfo const& GetInfo() const', 'const RuneInfo & GetInfo() const',
         'const T& spelled the other way'),
    ]
    for path, old, new, what in accepted:
        files = _base_files()
        files[path] = files[path].replace(old, new, 1)
        root = _tree(files)
        try:
            err = _run(root)[1]
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

    # 7. D4i-2: out-of-line and multi-line forwarders, their comments, an owner's call on another
    #    character, a writer through the map, and the re-run control
    root = _tree(_q_files())
    try:
        changes, errors, hands, report = _qrun(root, True)[:4]
        expect(errors == [] and hands == [], 'quest: no errors, no hand sites: %r %r' % (errors, hands))
        got = read(os.path.join(root, PLAYER_H))
        expect(got == SELF_Q_PLAYER_H_AFTER, 'quest: Player.h after the drops:\n%s' % got)
        got = read(os.path.join(root, SELF_Q_OWNER_PATH))
        expect(got == SELF_Q_OWNER_AFTER, 'quest: the owner file after the drops:\n%s' % got)
        got = read(os.path.join(root, 'src/game/Handlers/QuestHandler.cpp'))
        expect(got == SELF_Q_OUTSIDE_AFTER, 'quest: the outside file:\n%s' % got)
        expect(read(os.path.join(root, 'src/game/q/QuestStatusMgr.h')) == SELF_Q_MGR_H, 'quest: manager untouched')
        # 4 out-of-line forwarders (a declaration and a definition each) + 2 inline; 5 owner + 6 outside calls
        expect(changes == 4 * 2 + 2 + 5 + 6, 'quest: 21 changes: %d' % changes)
        expect(sum(1 for r in report if ': DROP out-of-line' in r) == 4
               and sum(1 for r in report if ': DROP inline (multi-line)' in r) == 1,
               'quest: the shapes are named: %r' % [r for r in report if 'DROP' in r])
        changes2, errors2, _, _ = _qrun(root, True)[:4]
        expect(changes2 == 0 and errors2 == [], 'quest: the re-run changes nothing: %d %r' % (changes2, errors2))
    finally:
        shutil.rmtree(root, ignore_errors=True)

    # 8. D4i-2: forwarders that are not pure, or not found whole, stop the tool; nothing is written
    body_q = 'return m_questStatusMgr.GetQuestStatus(quest_id);'
    decl_q = 'QuestStatus GetQuestStatus(uint32 quest_id) const;'
    def_q = 'QuestStatus Player::GetQuestStatus(uint32 quest_id) const\n{'
    decl_c = 'bool IsCurrentQuest(uint32 quest_id, uint8 completed_or_not = 0) const;'
    def_c = 'bool Player::IsCurrentQuest(uint32 quest_id, uint8 completed_or_not) const\n{'
    q_impure = [
        # (file, old, new, what)
        (SELF_Q_OWNER_PATH, body_q,
         'return m_questStatusMgr.GetQuestStatus(quest_id, ObjectMgr::QuestTemplateLookup());',
         'an added template lookup (the GetQuestRewardStatus shape)'),
        (SELF_Q_OWNER_PATH, '    m_questStatusMgr.ResetWeeklyQuestStatus();\n',
         '    m_questStatusMgr.ResetWeeklyQuestStatus();\n\n    UpdateForQuestWorldObjects();\n',
         'a second statement (the SetQuestStatus shape)'),
        (PLAYER_H, decl_c, decl_c.replace('= 0', '= 1'), 'another default than the manager\'s'),
        (PLAYER_H, 'QuestStatusMap& getQuestStatusMap()', 'QuestStatusMap getQuestStatusMap()',
         'a copy where the manager returns a reference'),
        (PLAYER_H, '            return m_questStatusMgr.Map();\n',
         '            Log();\n            return m_questStatusMgr.Map();\n', 'a second statement, multi-line inline'),
        (SELF_Q_OWNER_PATH, def_q, def_q.replace(') const', ')'), 'the definition is not const'),
        (SELF_Q_OWNER_PATH, 'QuestStatus Player::GetQuestStatus(uint32 quest_id) const\n{\n    ' + body_q + '\n}\n', '',
         'no out-of-line definition'),
        (SELF_Q_OWNER_PATH, 'void Player::Take', 'QuestStatus Player::GetQuestStatus(uint32 quest_id) const\n{\n    '
         + body_q + '\n}\n\nvoid Player::Take', 'two definitions'),
        ('src/game/q/QuestStatusMgr.h', '        void ResetWeeklyQuestStatus();\n', '',
         'the manager declares no such method'),
        ('src/game/q/QuestStatusMgr.h', 'QuestStatus GetQuestStatus(uint32 quest_id) const;',
         'uint8 GetQuestStatus(uint32 quest_id) const;', 'another return type on the manager'),
        ('src/game/q/QuestStatusMgr.h', 'uint8 completed_or_not = 0) const;', 'uint32 completed_or_not = 0) const;',
         'another parameter type on the manager'),
        ('src/game/q/QuestStatusMgr.h', 'QuestStatus GetQuestStatus(uint32 quest_id) const;',
         'QuestStatus GetQuestStatus(uint32 const& quest_id) const;', 'a reference parameter on the manager'),
        ('src/game/q/QuestStatusMgr.h', 'uint8 completed_or_not = 0) const;', 'uint8 completed_or_not = 2) const;',
         'another default on the manager'),
        (PLAYER_H, '        // Get the reward status\n',
         '        // Get the reward status\n'
         '        QuestStatus GetQuestStatus(Quest const* q) const { return Find(q); }\n',
         'an overload that is not a forwarder'),
    ]
    for path, old, new, what in q_impure:
        files = _q_files()
        if path not in files:
            files[path] = ''
        expect(files[path].count(old) == 1, 'quest impure (%s): the fixture has the text once' % what)
        files[path] = files[path].replace(old, new)
        root = _tree(files)
        try:
            _, err, _, _ = _qrun(root, True)[:4]
            expect(err != [], 'quest impure (%s) is an error' % what)
            for f, text in files.items():
                expect(read(os.path.join(root, f)) == text, 'quest impure (%s): %s not written' % (what, f))
        finally:
            shutil.rmtree(root, ignore_errors=True)

    # 11. fix round 1 (review I-1, M-1): the forwarders' overload set against the manager CLASS's
    #     whole set, one to one; an unparsed manager declaration fails loud
    mgr_h = 'src/game/q/QuestStatusMgr.h'
    mdecl_q = 'QuestStatus GetQuestStatus(uint32 quest_id) const;'
    set_rows = [
        # ([(file, old, new)], refused?, what)
        ([(mgr_h, 'uint8 completed_or_not = 0) const;', 'uint8 completed_or_not) const;')], True,
         'a default the manager lacks (out of line)'),
        ([(PLAYER_H, 'void RemoveTimedQuest(uint32 quest_id) {', 'void RemoveTimedQuest(uint32 quest_id = 0) {')], True,
         'a default the manager lacks (inline)'),
        ([(mgr_h, mdecl_q, mdecl_q + '\n        QuestStatus GetQuestStatus(float quest_id) const;')], True,
         'an extra manager overload of another type (float)'),
        ([(mgr_h, mdecl_q,
           mdecl_q + '\n        QuestStatus GetQuestStatus(uint64 guid, bool create = false) const;')], True,
         'an extra manager overload of another arity with a default (uint64)'),
        ([(mgr_h, mdecl_q, mdecl_q + '\n        QuestStatus GetQuestStatus(uint32 quest_id);')], True,
         'an extra non-const manager overload under a const forwarder'),
        ([(PLAYER_H, 'QuestStatusMap& getQuestStatusMap()\n',
           'QuestStatusMap const& getQuestStatusMap() const\n')], True,
         'a const forwarder over the manager\'s const/non-const pair'),
        ([(mgr_h, mdecl_q, 'QuestStatus GetQuestStatus(uint64 quest_id) const;'),
          (mgr_h, 'class QuestStatusMgr\n',
           'struct QuestStatusData\n{\n    QuestStatus GetQuestStatus(uint32 quest_id) const;\n};\n'
           'class QuestStatusMgr\n')], True,
         'the matching signature is in ANOTHER class of the manager header'),
        ([(mgr_h, mdecl_q, mdecl_q + '\n        [[nodiscard]] QuestStatus GetQuestStatus(float q) const;')], True,
         'an unparsed manager overload (an attribute)'),
        ([(mgr_h, mdecl_q, 'QuestStatus\n        GetQuestStatus(uint32 quest_id) const;')], True,
         'an unparsed manager declaration (the return type on the line above)'),
        ([(mgr_h, 'class QuestStatusMgr\n', 'class QuestStatusMgrX\n')], True, 'no manager class in the header'),
        # fix round 2 (delta review I-1): a const forwarder in the set turns both constness
        # exceptions off
        ([(PLAYER_H, 'QuestStatus GetQuestStatus(uint32 quest_id) const;',
           'QuestStatus GetQuestStatus(uint32 quest_id);\n'
           '        QuestStatus GetQuestStatus(float q) const { return m_questStatusMgr.GetQuestStatus(q); }'),
          (SELF_Q_OWNER_PATH, 'QuestStatus Player::GetQuestStatus(uint32 quest_id) const\n{',
           'QuestStatus Player::GetQuestStatus(uint32 quest_id)\n{'),
          (mgr_h, mdecl_q, mdecl_q + '\n        QuestStatus GetQuestStatus(float q) const;')], True,
         'L1: a non-const forwarder over a const-only method beside a const forwarder'),
        ([(mgr_h, 'QuestStatusMap& Map() { return m_status; }', 'QuestStatusMap& Map(uint32 k) { return m_status; }'),
          (mgr_h, 'QuestStatusMap const& Map() const { return m_status; }',
           'QuestStatusMap const& Map(uint32 k) const { return m_status; }\n'
           '        QuestStatusMap const& Map(float k) const { return m_status; }'),
          (PLAYER_H, 'QuestStatusMap& getQuestStatusMap()\n        {\n'
           '            return m_questStatusMgr.Map();\n        };',
           'QuestStatusMap& getQuestStatusMap(uint32 k)\n        {\n            return m_questStatusMgr.Map(k);\n'
           '        };\n'
           '        QuestStatusMap const& getQuestStatusMap(float k) const { return m_questStatusMgr.Map(k); }')],
         True, 'L2: a const/non-const pair under a non-const forwarder beside a const forwarder'),
        # I-2: two manager overloads that read the same, and a type word taken for a name
        ([(mgr_h, 'void RemoveTimedQuest(uint32 quest_id) { m_timedQuests.erase(quest_id); }',
           'void RemoveTimedQuest(unsigned int);\n        void RemoveTimedQuest(unsigned long);'),
          (PLAYER_H, 'void RemoveTimedQuest(uint32 quest_id) {', 'void RemoveTimedQuest(unsigned quest_id) {')],
         True, 'leak2: unnamed `unsigned int` / `unsigned long` manager overloads'),
        ([(mgr_h, mdecl_q, mdecl_q + '\n        QuestStatus GetQuestStatus(uint32 other) const;')], True,
         'two manager declarations that read the same'),
        # M-1: Player's set is read with the every-token-must-parse rule too
        ([(PLAYER_H, 'QuestStatus GetQuestStatus(uint32 quest_id) const;',
           'QuestStatus GetQuestStatus(uint32 quest_id) const;\n'
           '        QuestStatus\n        GetQuestStatus(float q) const;')],
         True, 'L3: a Player overload with its return type on the line above'),
        ([(PLAYER_H, 'QuestStatus GetQuestStatus(uint32 quest_id) const;',
           'QuestStatus GetQuestStatus(uint32 quest_id) const;\n'
           '        [[nodiscard]] QuestStatus GetQuestStatus(float q) const;')],
         True, 'L4: a Player overload with an attribute'),
        # accepted: another class's same name does not count against the manager's set (it makes
        # the name SHARED: its calls become HAND sites, the set still drops)
        ([(mgr_h, 'class QuestStatusMgr\n', 'struct QuestStatusData\n{\n    uint8 GetQuestStatus(float q) const;\n};\n'
           'class QuestStatusMgr\n')], False,
         'another class of the header declares the name; the manager\'s own set matches'),
        # accepted: a call of the method inside an inline body of the manager is not a declaration
        ([(mgr_h, '        void ResetMonthlyQuestStatus();\n',
           '        void ResetMonthlyQuestStatus();\n'
           '        bool Done() const { return GetQuestStatus(1) == QUEST_STATUS_NONE; }\n')],
         False, 'a call inside a manager inline body'),
        # accepted: a non-const forwarder over the manager's single const method
        ([(PLAYER_H, 'QuestStatus GetQuestStatus(uint32 quest_id) const;',
           'QuestStatus GetQuestStatus(uint32 quest_id);'),
          (SELF_Q_OWNER_PATH, 'QuestStatus Player::GetQuestStatus(uint32 quest_id) const\n{',
           'QuestStatus Player::GetQuestStatus(uint32 quest_id)\n{')], False,
         'a non-const forwarder over the only (const) manager method'),
    ]
    for edits, refused, what in set_rows:
        files = _q_files()
        for path, old, new in edits:
            expect(files[path].count(old) == 1, 'set row (%s): the fixture has the text once: %r' % (what, old))
            files[path] = files[path].replace(old, new)
        root = _tree(files)
        try:
            _, err, _, _ = _qrun(root, True)[:4]
            expect(bool(err) == refused, 'set row (%s): refused == %s: %r' % (what, refused, err))
            if refused:
                for f, text in files.items():
                    expect(read(os.path.join(root, f)) == text, 'set row (%s): %s not written' % (what, f))
            else:
                expect(read(os.path.join(root, PLAYER_H)) == SELF_Q_PLAYER_H_AFTER,
                       'set row (%s): the forwarders are dropped' % what)
        finally:
            shutil.rmtree(root, ignore_errors=True)

    # the definition outside the owner files
    files = _q_files()
    files[SELF_Q_OWNER_PATH] = files[SELF_Q_OWNER_PATH].replace(
        'QuestStatus Player::GetQuestStatus(uint32 quest_id) const\n{\n    ' + body_q + '\n}\n', '')
    files['src/game/Other.cpp'] = ('QuestStatus Player::GetQuestStatus(uint32 quest_id) const\n{\n    '
                                   + body_q + '\n}\n')
    root = _tree(files)
    try:
        _, err, _, _ = _qrun(root, False)[:4]
        expect(any('not in an owner file' in e for e in err), 'quest: a definition outside the owners: %r' % err)
    finally:
        shutil.rmtree(root, ignore_errors=True)

    # a member pointer to a dropped out-of-line forwarder is still refused
    files = _q_files()
    files['src/game/Other.cpp'] = 'auto f = &Player::GetQuestStatus;\n'
    root = _tree(files)
    try:
        _, err, _, _ = _qrun(root, False)[:4]
        expect(any('Other.cpp:1' in e and 'Player::GetQuestStatus' in e for e in err),
               'quest: a member pointer is refused: %r' % err)
    finally:
        shutil.rmtree(root, ignore_errors=True)

    # 9. D4i-2: an overload set of pure forwarders is dropped whole; each definition is matched by
    #    its parameter types
    files = _q_files()
    files['src/game/q/QuestStatusMgr.h'] = files['src/game/q/QuestStatusMgr.h'].replace(
        '        QuestStatus GetQuestStatus(uint32 quest_id) const;\n',
        '        QuestStatus GetQuestStatus(uint32 quest_id) const;\n'
        '        QuestStatus GetQuestStatus(Quest const* q) const;\n')
    files[PLAYER_H] = files[PLAYER_H].replace(
        '        QuestStatus GetQuestStatus(uint32 quest_id) const;\n',
        '        QuestStatus GetQuestStatus(uint32 quest_id) const;\n'
        '        QuestStatus GetQuestStatus(Quest const* q) const;\n')
    files[SELF_Q_OWNER_PATH] = files[SELF_Q_OWNER_PATH].replace(
        'void Player::ResetWeeklyQuestStatus()',
        'QuestStatus Player::GetQuestStatus(Quest const* q) const\n{\n'
        '    return m_questStatusMgr.GetQuestStatus(q);\n}\n\n'
        'void Player::ResetWeeklyQuestStatus()')
    root = _tree(files)
    try:
        changes, err, _, report = _qrun(root, True)[:4]
        expect(err == [], 'quest overloads: no errors: %r' % err)
        expect(read(os.path.join(root, PLAYER_H)) == SELF_Q_PLAYER_H_AFTER,
               'quest overloads: both declarations dropped')
        expect(read(os.path.join(root, SELF_Q_OWNER_PATH)) == SELF_Q_OWNER_AFTER,
               'quest overloads: both definitions dropped')
        expect(changes == 23, 'quest overloads: 23 changes: %d' % changes)
    finally:
        shutil.rmtree(root, ignore_errors=True)

    # 12. D4i-3: static forwarders. The set's `static` is part of the key; a static body calls
    #     Mgr::Method; its bare, this-> and plain Player:: calls take that body, its receiver
    #     calls take the accessor; the other spellings of Player::Name stay errors
    root = _tree(_s_files())
    try:
        changes, errors, hands, report = _srun(root, True)[:4]
        expect(errors == [] and hands == [], 'static: no errors, no hand sites: %r %r' % (errors, hands))
        got = read(os.path.join(root, PLAYER_H))
        expect(got == SELF_S_PLAYER_H_AFTER, 'static: Player.h after the drops:\n%s' % got)
        got = read(os.path.join(root, SELF_S_OWNER_PATH))
        expect(got == SELF_S_OWNER_AFTER, 'static: the owner file after the drops:\n%s' % got)
        got = read(os.path.join(root, 'src/game/Handlers/ItemHandler.cpp'))
        expect(got == SELF_S_OUTSIDE_AFTER, 'static: the outside file:\n%s' % got)
        expect(read(os.path.join(root, 'src/game/i/InvMgr.h')) == SELF_S_MGR_H, 'static: manager untouched')
        # 3 out-of-line (declaration + definition) + 1 inline drops; 6 owner + 4 outside calls
        expect(changes == 3 * 2 + 1 + 6 + 4, 'static: 17 changes: %d' % changes)
        expect(sum(1 for r in report if '(qualified)' in r) == 2, 'static: two qualified calls: %r' % report)
        changes2, errors2 = _srun(root, True)[:2]
        expect(changes2 == 0 and errors2 == [], 'static: the re-run changes nothing: %d %r' % (changes2, errors2))
    finally:
        shutil.rmtree(root, ignore_errors=True)

    # fix round 1 (review M-2): as written, the fixture's dropped block (a `//` comment, the
    # IsInvPos pair and GetItemAt) ends directly above the kept TwoHand(); dropping it would
    # leave the comment over TwoHand(), so the tool stops and writes nothing
    files = _s_files(spaced=False)
    root = _tree(files)
    try:
        err = _srun(root, True)[1]
        expect(any(PLAYER_H + ':7-7: the comment over dropped lines would be left over line 12' in e for e in err),
               'static: the orphaned comment is refused: %r' % err)
        for f, text in files.items():
            expect(read(os.path.join(root, f)) == text, 'static (orphaned comment): %s not written' % f)
    finally:
        shutil.rmtree(root, ignore_errors=True)

    def _drop_one(text, line='    int b;\n'):
        start = text.index(line)
        return drop_edits(text, [(start, start + len(line))])
    expect(_drop_one('{\n    int a;\n\n    // doc\n    int b;\n    int c;\n}\n')[1] == [(4, 4, 6)],
           'drop_edits: a comment over a range with kept code below is returned')
    expect(_drop_one('{\n    int a;\n    // doc\n    int b;\n\n    int c;\n}\n')[1] == [(3, 3, 5)],
           'drop_edits: a comment with code directly above it is returned')
    text = '{\n    int a;\n\n    // doc\n    int b;\n\n    int c;\n}\n'
    expect(_drop_one(text) == ([(text.index('    // doc'), text.index('    int c;'))], []),
           'drop_edits: a standalone comment goes with its range: %r' % (_drop_one(text),))
    # fix round 2 (delta review M-1): a comment separated from the range by a blank line
    text = 'class P\n{\n    int a;\n\n    // doc b\n\n    int b;\n\n    int c;\n};\n'
    expect(_drop_one(text) == ([(text.index('    int b;'), text.index('    int c;'))], []),
           'drop_edits: B1, kept code after a blank line: the comment stays over it: %r' % (_drop_one(text),))
    expect(_drop_one('class P\n{\n    int a;\n\n    // doc b\n\n    int b;\n};\n')[1] == [(5, 5, 8)],
           'drop_edits: B2, the range last in the class: the comment would head `};`: refused')
    expect(_drop_one('class P\n{\n    int a;\n\n    // ---- items ----\n\n    int b;\n\n    // ---- bank ----\n'
                     '    int c;\n};\n')[1] == [(5, 5, 9)],
           'drop_edits: B3, the range the whole section: the banner would head the next one: refused')
    expect(_drop_one('int a;\n\n// doc b\n\nint b;\n', 'int b;\n')[1] == [(3, 3, 6)],
           'drop_edits: the range last in the file: the comment would head nothing: refused')
    expect(_drop_one('class P\n{\n    int a;\n\n    /* doc\n     * b */\n\n    int b;\n};\n')[1] == [(5, 6, 9)],
           'drop_edits: B2 with a /* */ block: refused from its opening line')
    # N-2: a line starting with `*` is a comment only inside a /* block; else it is code
    text = 'void f()\n{\n\n    *p = 1;\n    int b;\n\n}\n'
    expect(_drop_one(text) == ([(text.index('    int b;'), text.index('    int b;') + 11)], []),
           'drop_edits: `*p = 1;` over the range is code, not deleted: %r' % (_drop_one(text),))
    text = '{\n    int a;\n\n    /**\n     * doc\n     */\n    int b;\n\n    int c;\n}\n'
    expect(_drop_one(text) == ([(text.index('    /**'), text.index('    int c;'))], []),
           'drop_edits: a /** */ doc block goes with its range: %r' % (_drop_one(text),))

    inv_h = 'src/game/i/InvMgr.h'
    s_def = 'bool Player::IsBagPos(uint16 pos)\n{\n    return InvMgr::IsBagPos(pos);'
    s_rows = [
        # ([(file, old, new)], what, the error it must give): each refused, nothing written
        ([(inv_h, 'static bool IsBagPos(uint16 pos);', 'bool IsBagPos(uint16 pos);')],
         'a static forwarder over a member manager method',
         'no InvMgr::IsBagPos declaration is the forwarder static bool(uint16)'),
        ([(PLAYER_H, 'static bool IsBagPos(uint16 pos);', 'bool IsBagPos(uint16 pos);'),
          (SELF_S_OWNER_PATH, s_def, 'bool Player::IsBagPos(uint16 pos)\n{\n    return m_invMgr.IsBagPos(pos);')],
         'a member forwarder over a static manager method',
         'no InvMgr::IsBagPos declaration is the forwarder bool(uint16) (the manager has static bool(uint16))'),
        ([(SELF_S_OWNER_PATH, s_def, s_def.replace('return InvMgr::', 'return OtherMgr::'))],
         'a static body calling another class', 'IMPURE: the body is not one call of InvMgr::IsBagPos'),
        ([(SELF_S_OWNER_PATH, s_def, s_def.replace('return InvMgr::', 'return m_invMgr.'))],
         'a static body calling through the member', 'IMPURE: the body is not one call of InvMgr::IsBagPos'),
        # each forwarder matches the manager's own; the static one comes first, so a qualified
        # Player::IsInvPos(1, 2) of the member one would silently become InvMgr::IsInvPos(1, 2)
        ([(inv_h, 'static bool IsInvPos(uint8 bag, uint8 slot);', 'bool IsInvPos(uint8 bag, uint8 slot) const;'),
          (PLAYER_H, 'static bool IsInvPos(uint8 bag, uint8 slot);', 'bool IsInvPos(uint8 bag, uint8 slot) const;'),
          (SELF_S_OWNER_PATH,
           'bool Player::IsInvPos(uint8 bag, uint8 slot)\n{\n    return InvMgr::IsInvPos(bag, slot);',
           'bool Player::IsInvPos(uint8 bag, uint8 slot) const\n{\n    return m_invMgr.IsInvPos(bag, slot);')],
         'a set mixing a static and a member forwarder (each matching the manager)',
         'a set mixing static and member forwarders'),
        ([('src/game/Other.cpp', None, 'bool b = u->Player::IsBagPos(1);\n')], 'a qualified call through a receiver',
         'Other.cpp:1: [inv] IsBagPos: Player::IsBagPos: rewrite by hand'),
        ([('src/game/Other.cpp', None, 'auto f = &Player::IsBagPos;\n')], 'a pointer to a static forwarder',
         'Other.cpp:1: [inv] IsBagPos: Player::IsBagPos: rewrite by hand'),
        # fix round 1 (review M-1): a pointer without `&`: no call follows the name
        ([('src/game/Other.cpp', None, 'bool (*f)(uint16) = Player::IsBagPos;\n')], 'a pointer without &',
         'Other.cpp:1: [inv] IsBagPos: Player::IsBagPos: rewrite by hand'),
        ([('src/game/Other.cpp', None, 'bool b = ::Player::IsBagPos(1);\n')], 'a globally qualified call',
         'Other.cpp:1: [inv] IsBagPos: Player::IsBagPos: rewrite by hand'),
        ([('src/game/Other.cpp', None, 'bool b = IsBagPos(1);\n')], 'a bare static call outside the owners',
         'Other.cpp:1: [inv] IsBagPos: a bare call outside the owner files'),
        ([('src/game/Object/Bag.h', None, 'class Bag { public: static bool IsBagPos(uint16 pos); };\n')],
         'a shared static name: its Player:: call is not rewritten',
         'ItemHandler.cpp:3: [inv] IsBagPos: Player::IsBagPos: rewrite by hand'),
    ]
    for edits, what, needle in s_rows:
        files = _s_files()
        for path, old, new in edits:
            if old is None:
                files[path] = new
                continue
            expect(files[path].count(old) == 1, 'static row (%s): the fixture has the text once: %r' % (what, old))
            files[path] = files[path].replace(old, new)
        root = _tree(files)
        try:
            err = _srun(root, True)[1]
            expect(any(needle in e for e in err), 'static row (%s) is refused for its reason: %r' % (what, err))
            for f, text in files.items():
                expect(read(os.path.join(root, f)) == text, 'static row (%s): %s not written' % (what, f))
        finally:
            shutil.rmtree(root, ignore_errors=True)
    expect(is_static('static inline bool') and not is_static('bool') and not is_static('StaticInfo const&'),
           'is_static reads the word only')

    # 10. the parameter and type helpers (fix round 2: I-2, a type word is never a name)
    expect(parse_params('uint32 a, std::map<uint32, uint8> const& m = {}, uint8 c = 0') ==
           [('uint32', 'a', None), ('std::map<uint32,uint8>const&', 'm', '{}'), ('uint8', 'c', '0')],
           'parse_params: %r' % parse_params('uint32 a, std::map<uint32, uint8> const& m = {}, uint8 c = 0'))
    expect(parse_params('uint32') == [('uint32', None, None)] and parse_params('void') == []
           and parse_params('Quest const*, uint8 const') == [('Quest const*', None, None), ('uint8 const', None, None)],
           'parse_params: unnamed / void: %r' % parse_params('Quest const*, uint8 const'))
    expect(parse_params('unsigned int, unsigned long, long long, unsigned q') ==
           [('unsigned int', None, None), ('unsigned long', None, None), ('long long', None, None),
            ('unsigned', 'q', None)], 'parse_params: type words: %r'
           % parse_params('unsigned int, unsigned long, long long, unsigned q'))
    expect(norm_type('inline QuestStatusMap &') == 'QuestStatusMap&',
           'norm_type: %r' % norm_type('inline QuestStatusMap &'))

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
