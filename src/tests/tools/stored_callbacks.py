#!/usr/bin/env python3
"""stored_callbacks.py [--source-root <repo root>] | --self-test

Decoupling D4k: a character manager never stores a callback. The managers (the headers on
CheckManagerIsolation.cmake's MANAGER_FILES, read from that file so the two lists cannot drift)
take what they write to the character as callbacks -- std::function parameters, or a small
struct of them -- and call them where the old code wrote. A callback kept in a member would let
a manager write to the character at any later point, from any statement, which is exactly what
the per-call shape rules out (src/game/entities/player/README.md, "Manager shape", rule 3).

Every class and struct defined in a manager header is walked with method_count.py's class-scope
scanner (its --all mode, which separates member functions from data members), and so is every
anonymous struct, class or union body in one (its members are the enclosing object's storage).
A CALLBACK TYPE is `function<...>` (std::function, however qualified) or an alias of one: a
typedef or using in any manager header whose aliased type names a callback type, aliases of
aliases included. A HOLDER is a class or struct defined there with a data member or a base class
(any access) whose type names a callback type or another holder.

A CALL-SCOPED BUNDLE is a holder that is a PLAIN struct defined inside a class whose own member
function names it in its declaration: the struct is handed to the manager at the call
(`ApplySinks const& sinks`), because three or more callbacks or facts go in one small struct
(README rule 2). Plain means: no base class, no access label but `public:` (a `class` must open
with it), no nested type, no member function but constructors, destructors and defaulted or
deleted ones, no friend, static_assert or macro. Nothing else makes a holder a bundle -- not its
own members (a copy constructor, an assignment), not a function of some other class, not a
struct with methods or private members -- so a manager class that holds a callback is never one,
even nested in a class that takes it.

Every class that is not a bundle then fails the check for each data member or base class whose
type names a callback type (a stored or inherited callback) or a holder (a stored or inherited
bundle is a stored callback); the message names the last holder the declaration spells
(`T::S m_s` stores S). A bundle's own members are not checked: they live as long as the call. A
holder that is not a bundle is flagged at its own callback member, even when only a bundle uses
it: make it a bundle or flatten it into one.

Allowed, and not data members: a callback parameter, a callback returned, a local variable in an
inline body (bodies are not walked), a typedef or using of a callback type.

KNOWN MISSES, stated rather than chased: a callback held by a variable that is not a class data
member (a namespace-scope or function-local static, in a header or a .cpp); a class defined in a
manager's .cpp (it cannot be a by-value member of a class the header defines); a callback type
reached through an alias defined outside the manager headers, or through a template parameter,
`auto` or `decltype`; a base class reached through a template parameter (CRTP); the scanner's own
limits (method_count.py's docstring). Aliases and holders are matched by name, whatever their
scope, so a non-callback alias that shares a name with a callback alias in another manager reads
as a callback (a false alarm, never a miss).

python3 src/tests/tools/stored_callbacks.py --source-root .
python3 src/tests/tools/stored_callbacks.py --self-test
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
import sys

# The scanner is method_count.py's. No bytecode cache: this runs from the source tree under ctest.
sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import method_count  # noqa: E402

ISOLATION_GATE = 'src/tests/CheckManagerIsolation.cmake'
DATA_REASONS = ('no parameter list', 'function-pointer data member')

# A class or struct definition: the keyword, the name, an optional `final` and base clause, then
# the brace. `enum class`/`enum struct` are excluded by the caller.
CLASS_DEF = re.compile(r'(?<![\w])(class|struct)\s+([A-Za-z_]\w*)\s*(?:final\s*)?(?::([^;{}]*))?\{')
# An anonymous struct, class or union body: the keyword directly followed by its brace.
ANON_DEF = re.compile(r'(?<![\w])(class|struct|union)\s*\{')
ACCESS = re.compile(r'(?<![\w:])(public|private|protected)\s*:(?!:)')
USING_ALIAS = re.compile(r'(?<![\w])using\s+([A-Za-z_]\w*)\s*=([^;]*);')
TYPEDEF = re.compile(r'(?<![\w])typedef\b([^;]*);')


def manager_files(cmake_text):
    """The paths (relative to src/game) in the `set(MANAGER_FILES ...)` block of the isolation
    gate: one or more per line, each line's `#` comment dropped, up to the closing parenthesis."""
    files = []
    inside = False
    closed = False
    for line in cmake_text.split('\n'):
        code = line.split('#', 1)[0]
        if not inside:
            m = re.match(r'\s*set\(\s*MANAGER_FILES\b(.*)$', code)
            if not m:
                continue
            inside = True
            code = m.group(1)
        if ')' in code:
            files.extend(code.split(')', 1)[0].split())
            closed = True
            break
        files.extend(code.split())
    if not inside or not closed:
        raise ValueError('no complete set(MANAGER_FILES ...) block')
    if not files:
        raise ValueError('MANAGER_FILES is empty')
    return files


def tokens(text):
    return [t.group() for t in method_count.TOKEN.finditer(text)]


def names_callback(toks, callback_names):
    """The callback type a token list names, or None: `function<` (std::function, however
    qualified) or one of callback_names (aliases and holders) as a whole token."""
    for i, t in enumerate(toks):
        if t == 'function' and i + 1 < len(toks) and toks[i + 1] == '<':
            return 'std::function'
        if t in callback_names:
            return t
    return None


def typedef_name(body_toks):
    """The name a `typedef <body>;` declares: inside a pointer declarator group
    (`typedef void (*Name)(int);`) the last identifier in that group, otherwise the last
    identifier outside every (), [] and <>."""
    depth = 0
    last = None
    i = 0
    while i < len(body_toks):
        t = body_toks[i]
        if t == '(' and depth == 0:
            close = method_count._match(body_toks, i, '(', ')')
            inner = body_toks[i + 1:close] if close > 0 else []
            if inner[:1] and inner[0] in ('*', '&', '^'):
                idents = [x for x in inner if method_count.IDENT.match(x)]
                if idents:
                    return idents[-1]
            depth += 1
        elif t in ('(', '[', '<'):
            depth += 1
        elif t in (')', ']', '>'):
            depth -= 1
        elif depth == 0 and method_count.IDENT.match(t):
            last = t
        i += 1
    return last


def aliases(clean):
    """[(name, aliased tokens)] for every typedef and using-alias in a blanked text."""
    found = []
    for m in USING_ALIAS.finditer(clean):
        found.append((m.group(1), tokens(m.group(2))))
    for m in TYPEDEF.finditer(clean):
        body = tokens(m.group(1))
        name = typedef_name(body)
        if name:
            found.append((name, body))
    return found


def class_defs(clean):
    """[(keyword, name, base tokens, offset of the keyword)] for the classes and structs defined in a
    blanked text, in order, once each (`enum class` / `enum struct` excluded)."""
    defs = []
    seen = set()
    for m in CLASS_DEF.finditer(clean):
        before = clean[:m.start()].rstrip()
        if re.search(r'(?<![\w])enum$', before):
            continue
        if m.group(2) in seen:
            continue
        seen.add(m.group(2))
        defs.append((m.group(1), m.group(2), tokens(m.group(3) or ''), m.start()))
    return defs


def class_names(clean):
    """The names of the classes and structs defined in a blanked text, in order, once each."""
    return [name for _, name, _, _ in class_defs(clean)]


def top_level_body(clean, open_at, close_at):
    """The text of a class body at its own depth: nested braces' contents dropped."""
    out = []
    depth = 0
    for c in clean[open_at + 1:close_at]:
        if c == '{':
            depth += 1
        elif c == '}':
            depth -= 1
        elif depth == 0:
            out.append(c)
    return ''.join(out)


def is_plain(keyword, name, bases, body, entries):
    """A plain struct: no base, every access label public (and a `class` opens with `public:`),
    no nested type, no member function but constructors, destructors and defaulted or deleted
    ones, no friend, static_assert or macro."""
    if bases:
        return False
    labels = [m.group(1) for m in ACCESS.finditer(body)]
    if any(label != 'public' for label in labels):
        return False
    if keyword == 'class' and entries and not (labels and body.find(labels[0]) < body.find(';')):
        return False
    for _, reason, signature in entries:
        if reason is None:
            toks = tokens(signature)
            special = (name in toks and toks[toks.index(name) + 1:toks.index(name) + 2] == ['(']) \
                or ('~' in toks and name in toks) \
                or any(toks[i:i + 2] in (['=', 'default'], ['=', 'delete']) for i in range(len(toks)))
            if not special:
                return False
        elif reason not in DATA_REASONS and reason != 'typedef/using':
            return False
    return True


def anonymous_members(clean):
    """[(keyword, line, synthetic text)] for every anonymous struct, class or union body in a
    blanked text: its body wrapped as a named struct on the same line, so the scanner walks it."""
    found = []
    for m in ANON_DEF.finditer(clean):
        before = clean[:m.start()].rstrip()
        if re.search(r'(?<![\w])enum$', before):
            continue
        open_at = m.end() - 1
        close_at = method_count._match_brace(clean, open_at, len(clean))
        line = clean.count('\n', 0, m.start()) + 1
        text = '\n' * (line - 1) + 'struct AnonymousMember {' + clean[open_at + 1:close_at] + '};\n'
        found.append((m.group(1), line, text))
    return found


def head_tokens(signature):
    """A data member's declaration part: its tokens before the first top-level '=', '{' or ':'
    (initialiser, braced initialiser, bit-field width)."""
    toks = tokens(signature)
    depth = 0
    for i, t in enumerate(toks):
        if t in ('(', '['):
            depth += 1
        elif t in (')', ']'):
            depth -= 1
        elif depth == 0 and t in ('=', '{', ':'):
            return toks[:i]
    return toks


def nested_names(entries):
    """The names of the classes and structs defined directly inside a class, from its scanner entries."""
    names = []
    for _, reason, signature in entries:
        if reason != 'nested type':
            continue
        toks = tokens(signature)
        k = 0
        while toks[k:k + 2] == ['template', '<']:
            close = method_count._match_angle(toks, k + 1)
            if close < 0:
                break
            k = close + 1
        if k + 1 < len(toks) and toks[k] in ('class', 'struct') and method_count.IDENT.match(toks[k + 1]):
            names.append(toks[k + 1])
    return names


def check(sources):
    """sources: [(label, text)] -- the manager headers. Returns (violations, summary), each
    violation "<label>:<line>: <message>", sorted by label and line."""
    classes = []   # dicts: label, name (the key), shown, line, bases, plain, entries
    alias_list = []
    for label, text in sources:
        clean = method_count.blank_comments_and_literals(text)
        alias_list.extend(aliases(clean))
        for keyword, name, bases, at in class_defs(clean):
            _, open_at, close_at = method_count.find_class_span(clean, name)
            _, _, entries = method_count.scan_members(text, name)
            body = top_level_body(clean, open_at, close_at)
            classes.append({'label': label, 'name': name, 'shown': name, 'line': clean.count('\n', 0, at) + 1,
                            'bases': bases, 'plain': is_plain(keyword, name, bases, body, entries),
                            'entries': entries})
        for keyword, line, synthetic in anonymous_members(clean):
            _, _, entries = method_count.scan_members(synthetic, 'AnonymousMember')
            classes.append({'label': label, 'name': '%s:%d:anonymous' % (label, line),
                            'shown': 'an anonymous %s' % keyword, 'line': line, 'bases': [], 'plain': False,
                            'entries': entries})

    # Callback aliases, to a fixed point (an alias of an alias).
    callback_aliases = set()
    changed = True
    while changed:
        changed = False
        for name, body in alias_list:
            if name not in callback_aliases and names_callback(body, callback_aliases):
                callback_aliases.add(name)
                changed = True

    # Holders, to a fixed point: a class with a data member or a base naming a callback type or
    # another holder.
    holders = set()
    changed = True
    while changed:
        changed = False
        for c in classes:
            if c['name'] in holders:
                continue
            marks = callback_aliases | holders
            if names_callback(c['bases'], marks) or any(
                    reason in DATA_REASONS and names_callback(head_tokens(signature), marks)
                    for _, reason, signature in c['entries']):
                holders.add(c['name'])
                changed = True

    # Call-scoped bundles: a plain struct holding callbacks, defined inside a class whose own
    # member function names it.
    bundles = set()
    for c in classes:
        nested = [n for n in nested_names(c['entries']) if n in holders]
        for _, reason, signature in c['entries']:
            if reason is not None:
                continue
            toks = tokens(signature)
            for holder in nested:
                if holder in toks and any(d['name'] == holder and d['plain'] for d in classes):
                    bundles.add(holder)

    def last_holder(toks):
        held = [tok for tok in toks if tok in holders]
        return held[-1] if held else None

    violations = []
    data_members = 0
    for c in classes:
        exempt = c['name'] in bundles
        if not exempt:
            held = last_holder(c['bases'])
            if held:
                violations.append((c['label'], c['line'], '%s inherits %s, which holds a callback: `: %s`'
                                   % (c['shown'], held, ' '.join(c['bases']))))
            else:
                direct = names_callback(c['bases'], callback_aliases)
                if direct:
                    violations.append((c['label'], c['line'], '%s inherits a callback (%s): `: %s`'
                                       % (c['shown'], direct, ' '.join(c['bases']))))
        for line, reason, signature in c['entries']:
            if reason not in DATA_REASONS:
                continue
            data_members += 1
            if exempt:
                continue
            head = head_tokens(signature)
            held = last_holder(head)
            if held:
                violations.append((c['label'], line, '%s stores %s, which holds a callback: `%s`'
                                   % (c['shown'], held, signature)))
                continue
            direct = names_callback(head, callback_aliases)
            if direct:
                violations.append((c['label'], line, '%s stores a callback (%s): `%s`' % (c['shown'], direct, signature)))
    violations.sort(key=lambda v: (v[0], v[1]))
    summary = ('%d classes, %d data members, callback aliases [%s], call-scoped bundles [%s]'
               % (len(classes), data_members, ', '.join(sorted(callback_aliases)), ', '.join(sorted(bundles))))
    return ['%s:%d: %s' % v for v in violations], summary


# The self-test fixture: every case the rules name. Line numbers are the fixture's own.
SELF_TEST_HEADER = '''#include <functional>
class Field;
typedef std::function<void(int)> GlobalSink;              // an alias at namespace scope
typedef GlobalSink OtherSink;                              // an alias of an alias
typedef void (*RawCallback)(int);                          // a function pointer: not a std::function
typedef std::vector<int> Numbers;                          // an alias of something else
class Manager
{
    public:
        typedef std::function<void(uint32 spellId)> SpellSink;
        using FieldSink = std::function<void(uint16, uint32)>;
        struct Sinks                                       // a bundle: Apply takes it
        {
            SpellSink cast;
            FieldSink setField;
            int count = 0;
        };
        struct Facts                                       // no callback: not a holder
        {
            bool nonPassive = false;
        };
        struct Unpassed                                    // a holder nobody takes
        {
            SpellSink lost;
        };
        void Apply(uint8 slot, Sinks const& sinks);        // a bundle parameter
        void Load(Field* fields, SpellSink const& onRow); // a callback parameter
        FieldSink Make() const;                            // a callback returned
        void Inline(int x)
        {
            std::function<void()> local = [x]() { };       // a local in an inline body
            local();
        }
        Manager(Manager const& other);                     // its own copy: not a bundle
    private:
        std::function<void(int)> m_stored;                 // stored std::function
        SpellSink m_alias;                                 // stored alias
        OtherSink m_aliasOfAlias;                          // stored alias of an alias
        std::vector<GlobalSink> m_many;                    // a container of callbacks
        static FieldSink s_shared;                         // a static member is stored too
        Sinks m_bundle;                                    // a stored bundle
        Facts m_facts;                                     // no callback
        RawCallback m_raw;                                 // a function pointer (not this check's)
        Numbers m_numbers;
        std::function<void()> m_later = [this]() { Inline(1); };   // an initialiser does not hide it
};
struct Owned
{
    ::std::function<int()> m_read;                         // fully qualified
};
'''

# (line, fragment of the message) for every violation in SELF_TEST_HEADER, in order.
SELF_TEST_EXPECTED = [
    (24, 'Unpassed stores a callback (SpellSink)'),
    (36, 'Manager stores a callback (std::function)'),
    (37, 'Manager stores a callback (SpellSink)'),
    (38, 'Manager stores a callback (OtherSink)'),
    (39, 'Manager stores a callback (GlobalSink)'),
    (40, 'Manager stores a callback (FieldSink)'),
    (41, 'Manager stores Sinks, which holds a callback'),
    (45, 'Manager stores a callback (std::function)'),
    (49, 'Owned stores a callback (std::function)'),
]

SELF_TEST_CMAKE = '''set(OTHER_FILES a.h)
set(MANAGER_FILES
    entities/player/A.h                 # decoupling D4a
    entities/player/A.cpp               # a comment with (parentheses)? no: comments end the line
    entities/player/B.h
)
set(OWNER_FILES
    entities/player/Owner.cpp
)
'''


def self_test():
    ok = True
    violations, summary = check([('fixture.h', SELF_TEST_HEADER)])
    got = []
    for v in violations:
        m = re.match(r'fixture\.h:(\d+): (.*)$', v)
        got.append((int(m.group(1)), m.group(2)) if m else (0, v))
    if len(got) != len(SELF_TEST_EXPECTED) or any(
            line != want_line or not text.startswith(want_text)
            for (line, text), (want_line, want_text) in zip(got, SELF_TEST_EXPECTED)):
        print('self-test: violations differ')
        for line, text in got:
            print('  got      %d: %s' % (line, text))
        for line, text in SELF_TEST_EXPECTED:
            print('  expected %d: %s' % (line, text))
        ok = False
    want_summary = ('classes, 16 data members, callback aliases [FieldSink, GlobalSink, OtherSink, SpellSink], '
                    'call-scoped bundles [Sinks]')
    if not summary.endswith(want_summary) or not summary.startswith('5 classes'):
        print('self-test: summary %r' % summary)
        ok = False
    # Each allowed spelling alone, in a class of its own: no violation.
    allowed = [
        'class A { void Load(std::function<void(int)> const& onRow); };',
        'class A { std::function<void()> Make() const; };',
        'class A { void Run() { std::function<void()> local; local(); } };',
        'class A { typedef std::function<void()> Sink; using Other = std::function<void()>; };',
        'class A { struct S { std::function<void()> f; }; void Take(S const& s); };',
        'class A { void (*m_raw)(int); int m_count = Count(std::function<void()>()); };',
        'enum class E { X }; class A { int m_x; };',
        # plain bundles: a constructor, a defaulted member, typedefs, `public:`, a class opening with it
        'class A { struct S { S() {} S(S const&) = default; typedef int T; std::function<void()> f; }; void Take(S const& s); };',
        'class A { class S { public: std::function<void()> f; bool b = false; }; void Take(S const& s); };',
        # an anonymous struct and a base with no callback
        'class A { struct { int x; } m_anon; }; class B : public A { };',
    ]
    for text in allowed:
        violations, _ = check([('allowed.h', text)])
        if violations:
            print('self-test (allowed): %r gave %r' % (text, violations))
            ok = False
    # Each stored spelling alone, with the number of violations it gives.
    stored = [
        ('class A { std::function<void()> m_f; };', 1),
        ('class A { typedef std::function<void()> Sink; Sink m_s; };', 1),
        ('typedef std::function<void()> Sink; class A { Sink* m_s; };', 1),
        # a bundle that is also stored: flagged where it is stored
        ('class A { struct S { std::function<void()> f; }; void Take(S const& s); S m_s; };', 1),
        # its own copy constructor and assignment do not make a class a bundle
        ('class A { A(A const& o); A& operator=(A const& o); std::function<void()> m_f; };', 1),
        # nor does a function of another class: a bundle is nested in the class that takes it
        ('class A { std::function<void()> m_f; }; class B { void Take(A const& a); };', 1),
        # a holder only a bundle holds is not a bundle itself (S.f), and a stored bundle (m_t)
        ('class A { struct S { std::function<void()> f; }; struct T { S s; }; void Take(T const& t); T m_t; };', 2),
        # a struct with a nested type is not plain, so not a bundle: both its member and the nested holder's
        ('class A { struct T { struct S { std::function<void()> f; }; S s; }; void Take(T const& t); };', 2),
        # M-1, inheritance: a manager deriving from a callback type (any access), from an alias of one,
        # from a holder; a stored struct deriving from one; a class inheriting a bundle's callback
        ('class M : private std::function<void()> { void Do(); };', 1),
        ('class M { typedef std::function<void()> Sink; }; class N : public Sink { };', 1),
        ('struct H { std::function<void()> f; }; class M : protected H { };', 2),
        ('class M { struct W : std::function<void()> { }; void Take(W const& w); W m_w; };', 2),
        ('class M { struct S { std::function<void()> f; }; void Take(S const& s); }; '
         'class N : public M::S { }; class O { N m_n; };', 2),
        # M-2, a bundle is a plain struct: one with a member function, or a private member, or a base,
        # is walked like any class; and a manager nested in a class that takes it is no bundle either
        ('class M { struct S { std::function<void()> f; void Run(); }; void Take(S const& s); };', 1),
        ('class M { struct S { private: std::function<void()> f; }; void Take(S const& s); };', 1),
        ('class M { class S { std::function<void()> f; }; void Take(S const& s); };', 1),
        ('class Outer { class Mgr { public: void Do(); private: std::function<void()> m_f; }; void Use(Mgr& m); };', 1),
        # M-3, anonymous struct and union members
        ('class M { struct { std::function<void()> f; } m_anon; };', 1),
        ('class M { union { int a; std::function<void()>* p; }; };', 1),
    ]
    for text, count in stored:
        violations, _ = check([('stored.h', text)])
        if len(violations) != count:
            print('self-test (stored): %r gave %r, expected %d' % (text, violations, count))
            ok = False
    # A callback alias declared in one manager header and stored in another.
    violations, _ = check([('sink.h', 'typedef std::function<void()> SharedSink;'),
                           ('user.h', 'class A { SharedSink m_s; };')])
    if len(violations) != 1 or not violations[0].startswith('user.h:1: A stores a callback (SharedSink)'):
        print('self-test (alias across headers): %r' % violations)
        ok = False
    # The MANAGER_FILES block, comments dropped, other lists ignored.
    try:
        files = manager_files(SELF_TEST_CMAKE)
    except ValueError as err:
        files = str(err)
    if files != ['entities/player/A.h', 'entities/player/A.cpp', 'entities/player/B.h']:
        print('self-test (MANAGER_FILES): %r' % files)
        ok = False
    for bad in ('set(OWNER_FILES a.h)\n', 'set(MANAGER_FILES\n    # nothing\n)\n', 'set(MANAGER_FILES\n    a.h\n'):
        try:
            manager_files(bad)
            print('self-test (MANAGER_FILES): %r did not raise' % bad)
            ok = False
        except ValueError:
            pass
    print('self-test %s' % ('OK' if ok else 'FAILED'))
    return 0 if ok else 1


def main(argv):
    parser = argparse.ArgumentParser(description='A character manager never stores a callback.')
    parser.add_argument('--self-test', action='store_true', help='run the self-test and exit')
    parser.add_argument('--source-root', default=os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..'),
                        help='the repository root (default: three levels above this script)')
    args = parser.parse_args(argv[1:])
    if args.self_test:
        return self_test()
    if self_test() != 0:
        return 1
    root = os.path.abspath(args.source_root)
    with open(os.path.join(root, ISOLATION_GATE), encoding='utf-8') as f:
        try:
            files = manager_files(f.read())
        except ValueError as err:
            print('stored callbacks: %s in %s' % (err, ISOLATION_GATE))
            return 1
    missing = [rel for rel in files if not os.path.isfile(os.path.join(root, 'src', 'game', rel))]
    if missing:
        print('stored callbacks: MANAGER_FILES lists files that do not exist: %s' % ', '.join(missing))
        return 1
    headers = [rel for rel in files if rel.endswith('.h')]
    if not headers:
        print('stored callbacks: MANAGER_FILES lists no header -- the check would read nothing')
        return 1
    sources = []
    for rel in headers:
        with open(os.path.join(root, 'src', 'game', rel), encoding='utf-8', errors='replace', newline='') as f:
            sources.append(('src/game/' + rel, f.read()))
    violations, summary = check(sources)
    if violations:
        print('A character manager stores a callback (decoupling D4k: callbacks are parameters, never members):')
        for v in violations:
            print('  ' + v)
        print('Pass it at the call instead (README "Manager shape", rule 3).')
        return 1
    print('stored callbacks: %d manager headers clean; %s' % (len(headers), summary))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
