#!/usr/bin/env python3
"""method_count.py --class <ClassName> --header <header>

The method counter for the decoupling campaign (counter R1). It is the D5 plan's method regex
(design/2026-09-23-decoupling-d5-unit-decomposition-plan.md, "R1"), which was a shell pipeline
over a hand-found line range:

  sed -n '<first>,<last>p' <header> | grep -nE '\\(.*\\)' | grep -E '[;{]\\s*$'
      | grep -vE '^[0-9]+:\\s*(//|/\\*|\\*|#)' | grep -v typedef
      | grep -vE ':\\s*(if|for|while|switch|else|return|case|do|catch)\\b' | wc -l

This tool finds the range itself: from the line that holds `class <ClassName>` (the
definition, not a forward declaration) to the line that holds the brace closing its body,
both inclusive -- the range the D4 facts measured by hand (Player.h:1106-4385 at 392a8fd36,
672 methods; 663 at ab4d8bd53, after D4a). Braces are matched with comments and string/char literals blanked; the filters
then run on the raw lines, numbered from 1 inside the range as `grep -n` numbers them, so every
quirk of the pipeline is kept on purpose (the number means what the facts measured):

  - a line counts when it holds '(' ... ')' and ends in ';' or '{' (so a declaration with a
    trailing // comment is NOT counted, and neither is one whose parens span two lines);
  - it does not count when, after its number and ':', it starts with //, /*, * or #;
  - it does not count when it contains `typedef` anywhere;
  - it does not count when ANY ':' in it (the number's own included) is followed by optional
    whitespace and a control keyword as a whole word -- so `case X: return y();` is dropped too.

It over-counts a few inline-body statements and counts the methods of nested types; the same
bias before and after a change, so the delta is honest.

--all counts every member function instead (ruling 17: the D4 finish line counts every one,
one-liners and commented declarations included). Over the same range, with comments and
string/char literals blanked by the same function, it walks the braces of the class body and
splits the class's own scope (depth 1) into statements at ';', at an access label and after the
body of an inline function; bodies and nested types are skipped whole, preprocessor lines
belong to no statement (so every branch of an #if is walked). A statement's declaration part
is what comes before its first top-level '=', ':' or '{' (initialiser, `= 0`/`= default`/
`= delete`, bit-field, constructor initialiser list, braced initialiser), template arguments
dropped. Its first (...) there that is not an attribute, decltype, alignas or noexcept group
decides:

  - after the member's name it is a parameter list: a member function. So are declarations and
    inline definitions, constructors (`T m(args);` in a class body can only be a function),
    destructors, operators, conversions, static/virtual/override/pure/defaulted/deleted members,
    template members and signatures over several lines -- one per function, at its first line;
  - a pointer declarator in parentheses is a data member (`void (*fp)(int);`,
    `void (T::*pm)(int);`) unless its name has a parameter list inside (`void (*Pick(int))(int);`
    is a function returning a pointer, and counts);
  - no (...) there: a data member, including `int m_x = Foo(1);`, `int m_x{Foo(1)};` and
    `std::function<void(int)> f;`.

Never counted: friend declarations (a friend defined in the class ends at its body, like a
member function), typedef/using (aliases and using-declarations), static_assert, nested
enum/struct/class/union bodies (their methods are the nested type's), and macro invocations --
a `NAME(args)` with no type before it whose NAME is not the class's own. A macro with no ';'
does not hide the declaration after it: `DECLARE_SOMETHING(X)` then `void f();` lists the
macro (not counted) and counts f. A macro with no parentheses (`Q_OBJECT`) ends at the next
access label; otherwise it reads as part of the next declaration.

Known limits (no macro expansion, no preprocessor evaluation): those of the literal blanking
(a raw string literal, a digit separator), a member declared through a function typedef
(`Fn f;`), several function declarators in one statement (`void f(), g();` counts once), a
brace on a preprocessor line, `#if 0` blocks (they count) and a declaration repeated under
`#if`/`#else` (it counts twice), a function-like macro used as a return type
(`DECLARE_TYPE(int) J() const;` is not counted), and a bit-field named like a Qt label
(`unsigned slots : 4;` reads as a label: two data statements, the method count unaffected).
--list prints every class-scope statement, the ones not counted marked
`[not counted: <reason>]`, with literal contents blanked.

python3 src/tests/tools/method_count.py --class Player --header src/game/entities/player/Player.h
python3 src/tests/tools/method_count.py --list --class Player --header src/game/entities/player/Player.h
python3 src/tests/tools/method_count.py --all [--list] --class Player --header src/game/entities/player/Player.h
python3 src/tests/tools/method_count.py --self-test
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
import bisect
import re
import sys

# The four filters of the pipeline, in its order. Each runs on "<n>:<raw line>".
HAS_PARENS = re.compile(r'\(.*\)')
ENDS_STATEMENT = re.compile(r'[;{]\s*$')
COMMENT_OR_DIRECTIVE = re.compile(r'^[0-9]+:\s*(//|/\*|\*|#)')
CONTROL_FLOW = re.compile(r':\s*(if|for|while|switch|else|return|case|do|catch)\b')


def blank_comments_and_literals(text):
    """Blank comments and string/char literals, keeping every '\\n' (line numbers hold).

    A character literal ends at a newline even without its closing apostrophe: an apostrophe
    in a preprocessor line (`#error don't`) must not swallow the lines after it, braces
    included. An escaped newline inside a literal (a line splice) keeps its '\\n'."""
    out = []
    i = 0
    n = len(text)
    while i < n:
        c = text[i]
        if c == '/' and i + 1 < n and text[i + 1] == '/':
            while i < n and text[i] != '\n':
                i += 1
            continue
        if c == '/' and i + 1 < n and text[i + 1] == '*':
            end = text.find('*/', i + 2)
            end = n if end < 0 else end + 2
            out.append('\n' * text.count('\n', i, end))
            i = end
            continue
        if c in '"\'':
            quote = c
            out.append(quote)
            i += 1
            while i < n and text[i] != quote:
                if quote == "'" and text[i] == '\n':
                    break
                if text[i] == '\\':
                    i += 1
                    if i < n and text[i] == '\n':
                        out.append('\n')
                elif text[i] == '\n':
                    out.append('\n')
                i += 1
            out.append(quote)
            if i < n and text[i] == quote:
                i += 1
            continue
        out.append(c)
        i += 1
    return ''.join(out)


def find_class_span(clean, name):
    """(keyword, open, close) offsets in <clean> (already blanked) of the definition of
    class/struct <name>: its class keyword, the '{' opening its body and the '}' closing it.
    Raises ValueError when there is no definition (forward declarations are skipped)."""
    pattern = re.compile(r'\b(class|struct)\s+(?:\w+\s+)*?' + re.escape(name) + r'\b([^;{]*)\{')
    for m in pattern.finditer(clean):
        tail = m.group(2).strip()
        # `class Foo;` never reaches a '{' without a ';' first; a base clause or `final` may.
        if tail and not tail.startswith(':') and not tail.startswith('final'):
            continue
        open_at = m.end() - 1
        depth = 0
        for i in range(open_at, len(clean)):
            if clean[i] == '{':
                depth += 1
            elif clean[i] == '}':
                depth -= 1
                if depth == 0:
                    return m.start(), open_at, i
        raise ValueError('class %s: the body is never closed' % name)
    raise ValueError('class %s not found' % name)


def find_class_range(text, name):
    """(first, last) 1-based inclusive lines of the definition of class/struct <name>.

    `first` is the line of the class keyword, `last` the line of the brace closing the body.
    Raises ValueError when there is no definition (forward declarations are skipped)."""
    clean = blank_comments_and_literals(text)
    start, _, close = find_class_span(clean, name)
    return clean.count('\n', 0, start) + 1, clean.count('\n', 0, close) + 1


def count_methods(text, name):
    """(first, last, [(line, raw)]) for class <name>: its range and the lines R1 counts."""
    first, last = find_class_range(text, name)
    # '\n' only, as grep and the range finder count lines: splitlines() also breaks at \v, \f,
    # \x1c-\x1e, \x85 and U+2028/2029, which would shift every line after one of them.
    lines = text.split('\n')[first - 1:last]
    counted = []
    for rel, raw in enumerate(lines, 1):
        numbered = '%d:%s' % (rel, raw)
        if not HAS_PARENS.search(numbered):
            continue
        if not ENDS_STATEMENT.search(numbered):
            continue
        if COMMENT_OR_DIRECTIVE.search(numbered):
            continue
        if 'typedef' in numbered:
            continue
        if CONTROL_FLOW.search(numbered):
            continue
        counted.append((first + rel - 1, raw))
    return first, last, counted


# --all: every member function, found by walking the class body's braces.

TOKEN = re.compile(r'::|->|\.\.\.|[A-Za-z_]\w*|\d[\w.]*|\S')
IDENT = re.compile(r'[A-Za-z_]\w*$')
# An access label ends the statement before it; Qt's signal/slot labels are labels too.
ACCESS_LABEL = re.compile(r'(?<![\w:])(?:(?:public|protected|private)(?:\s+(?:slots|Q_SLOTS))?'
                          r'|signals|Q_SIGNALS|slots|Q_SLOTS)\s*$')
SPECIFIERS = {'explicit', 'inline', 'virtual', 'static', 'constexpr', 'consteval', 'constinit',
              'extern', 'mutable', 'thread_local', 'friend'}
TYPE_KEYWORDS = {'void', 'bool', 'char', 'char8_t', 'char16_t', 'char32_t', 'wchar_t', 'short',
                 'int', 'long', 'float', 'double', 'signed', 'unsigned', 'auto', 'const',
                 'volatile', 'typename'}
# A (...) after one of these is an attribute, a type or an exception spec, never a parameter list.
NOT_PARAMETERS = {'decltype', 'alignas', '_Alignas', 'alignof', 'sizeof', 'noexcept', 'throw',
                  '__attribute__', '__declspec', 'requires', 'explicit'}
NOT_NAMES = SPECIFIERS | TYPE_KEYWORDS | NOT_PARAMETERS | {
    'operator', 'static_assert', 'typedef', 'using', 'template', 'enum', 'struct', 'class',
    'union', 'return', 'new', 'delete', 'this'}


def _match(toks, at, opening, closing):
    """Index of the token closing the group toks[at] opens, or -1."""
    depth = 0
    for j in range(at, len(toks)):
        if toks[j] == opening:
            depth += 1
        elif toks[j] == closing:
            depth -= 1
            if depth == 0:
                return j
    return -1


def _match_angle(toks, at):
    """Index of the '>' closing the '<' at toks[at] (parens inside skipped), or -1."""
    angle = paren = 0
    for j in range(at, len(toks)):
        t = toks[j]
        if t in ('(', '['):
            paren += 1
        elif t in (')', ']'):
            paren -= 1
            if paren < 0:
                return -1
        elif paren == 0 and t == '<':
            angle += 1
        elif paren == 0 and t == '>':
            angle -= 1
            if angle == 0:
                return j
    return -1


def _is_name(tok):
    """A declarator-id: an identifier, a merged `operator...` or a destructor `~X`."""
    if tok.startswith('operator') and tok != 'operator':
        return True
    if tok.startswith('~'):
        return len(tok) > 1
    return bool(IDENT.match(tok)) and tok not in NOT_NAMES


def _starts_with_pointer(inner):
    """The tokens of a (...) group open a pointer declarator: `*fp`, `&r`, `Foo::*pmf`."""
    j = 0
    while j + 1 < len(inner) and IDENT.match(inner[j]) and inner[j + 1] == '::':
        j += 2
    return j < len(inner) and inner[j] in ('*', '&', '^')


def _declarator_group(inner, after):
    """The verdict on a parenthesised declarator `T (inner) after`: None (a member function)
    or the reason it is not one."""
    j = 0
    pointer = False
    while j < len(inner):
        t = inner[j]
        if t in ('*', '&', '^'):
            pointer = True
            j += 1
        elif t in ('const', 'volatile', '::'):
            j += 1
        elif IDENT.match(t) and j + 1 < len(inner) and inner[j + 1] == '::':
            j += 2
        else:
            break
    rest = inner[j:]
    if len(rest) >= 2 and _is_name(rest[0]) and rest[1] == '(':
        return None  # a function returning a pointer: `void (*Pick(int which))(int);`
    if not pointer and len(rest) == 1 and _is_name(rest[0]) and after[:1] == ['(']:
        return None  # a parenthesised name: `void (Name)(int);`
    if pointer and after[:1] == ['(']:
        return 'function-pointer data member'
    return 'no parameter list'


def _merge_names(toks):
    """`operator` + its symbol (or conversion type) and `~` + name become one token each."""
    out = []
    i = 0
    while i < len(toks):
        t = toks[i]
        if t == 'operator':
            if toks[i + 1:i + 3] == ['(', ')']:
                out.append('operator()')
                i += 3
                continue
            j = i + 1
            while j < len(toks) and toks[j] != '(':
                j += 1
            out.append('operator ' + ' '.join(toks[i + 1:j]))
            i = j
            continue
        if t == '~' and i + 1 < len(toks) and IDENT.match(toks[i + 1]):
            out.append('~' + toks[i + 1])
            i += 2
            continue
        out.append(t)
        i += 1
    return out


def _strip_angles(toks):
    """Drop the <...> template arguments at paren depth 0 (`std::function<void(int)> f` -> `std::function f`)."""
    out = []
    depth = 0
    i = 0
    while i < len(toks):
        t = toks[i]
        if t in ('(', '['):
            depth += 1
        elif t in (')', ']'):
            depth -= 1
        elif t == '<' and depth == 0 and out and IDENT.match(out[-1]):
            close = _match_angle(toks, i)
            if close > 0:
                i = close + 1
                continue
        out.append(t)
        i += 1
    return out


def _head_verdict(toks, name):
    """(reason, ctor_init) for one declaration's tokens (template arguments already dropped).

    The head is the part before the first top-level '=', ':' or '{' (initialiser, pure or
    defaulted specifier, bit-field width, constructor initialiser list, braced initialiser).
    Its first top-level (...) that is not an attribute or a type decides: after a declarator-id
    it is the parameter list (reason None: a member function); a group that holds a pointer
    declarator is a data member unless a parameter list follows the name inside it."""
    depth = 0
    cut = len(toks)
    for j, t in enumerate(toks):
        if t in ('(', '['):
            depth += 1
        elif t in (')', ']'):
            depth -= 1
        elif depth == 0 and t in ('=', ':', '{'):
            cut = j
            break
    head = toks[:cut]
    ctor_init = cut < len(toks) and toks[cut] == ':'
    j = 0
    while j < len(head):
        t = head[j]
        if t == '[':
            close = _match(head, j, '[', ']')
            if close < 0:
                return 'unbalanced brackets', False
            j = close + 1
            continue
        if t != '(':
            j += 1
            continue
        close = _match(head, j, '(', ')')
        if close < 0:
            return 'unbalanced parentheses', False
        prev = head[j - 1] if j else None
        if prev in NOT_PARAMETERS:
            j = close + 1
            continue
        inner = head[j + 1:close]
        if prev is None or not _is_name(prev) or _starts_with_pointer(inner):
            return _declarator_group(inner, head[close + 1:]), False
        q = j - 1
        while q >= 2 and head[q - 1] == '::' and IDENT.match(head[q - 2]):
            q -= 2
        if q >= 1 and head[q - 1] == '::':
            q -= 1
        typed = any(p not in SPECIFIERS for p in head[:q])
        if not typed and prev not in (name, '~' + name) and not prev.startswith(('~', 'operator')):
            return 'macro invocation', False
        return None, ctor_init
    return 'no parameter list', False


def classify_statement(toks, name):
    """([(first token, reason)], ctor_init) for one class-scope statement of class <name>.

    reason None marks a member function. A statement yields more than one entry only when it
    opens with macro invocations that have no ';' of their own (`DECLARE_SOMETHING(X)` on the
    line before a declaration): each is an entry of its own, never counted."""
    entries = []
    k = 0
    # A leading `NAME(...)` with no type before it, NAME not the class's own (a constructor),
    # and no pointer declarator inside, is a macro invocation.
    while (k + 1 < len(toks) and IDENT.match(toks[k]) and toks[k] not in NOT_NAMES
           and toks[k] != name and toks[k + 1] == '('):
        close = _match(toks, k + 1, '(', ')')
        if close < 0 or _starts_with_pointer(toks[k + 2:close]):
            break
        entries.append((k, 'macro invocation'))
        k = close + 1
    if k >= len(toks):
        return entries, False
    start = k
    while toks[k:k + 2] == ['template', '<']:
        close = _match_angle(toks, k + 1)
        if close < 0 or close + 1 >= len(toks):
            break
        k = close + 1
    first = toks[k]
    reason = None
    if first == 'friend':
        reason = 'friend'
    elif first in ('typedef', 'using'):
        reason = 'typedef/using'
    elif first == 'static_assert':
        reason = 'static_assert'
    elif first in ('enum', 'struct', 'class', 'union'):
        depth = 0
        for t in toks[k:]:
            if t in ('(', '['):
                depth += 1
            elif t in (')', ']'):
                depth -= 1
            elif depth == 0 and t == '=':
                break
            elif depth == 0 and t == '{':
                reason = 'nested type'
                break
    ctor_init = False
    if reason is None:
        reason, ctor_init = _head_verdict(_strip_angles(_merge_names(toks[k:])), name)
    entries.append((start, reason))
    return entries, ctor_init


def _match_brace(clean, at, stop):
    """Offset of the '}' closing the '{' at clean[at], or stop when it is never closed."""
    depth = 0
    for j in range(at, stop):
        if clean[j] == '{':
            depth += 1
        elif clean[j] == '}':
            depth -= 1
            if depth == 0:
                return j
    return stop


def scan_members(text, name):
    """(first, last, [(line, reason, signature)]) for class <name>: its range (as R1 finds it)
    and one entry per class-scope statement, reason None for a member function."""
    clean = blank_comments_and_literals(text)
    start, open_at, close_at = find_class_span(clean, name)
    newlines = [m.start() for m in re.finditer('\n', clean)]
    entries = []
    buf = []   # the statement's characters (bodies of nested blocks replaced by '{...}')
    offs = []  # the offset in <clean> of each of them

    def classify(upto=None):
        stmt = ''.join(buf[:upto])
        found = [(m.group(), m.start()) for m in TOKEN.finditer(stmt)]
        verdicts, ctor_init = classify_statement([t for t, _ in found], name)
        return stmt, found, verdicts, ctor_init

    def flush(end_mark, upto=None):
        stmt, found, verdicts, _ = classify(upto)
        for n, (k, reason) in enumerate(verdicts):
            pos = found[k][1]
            last = n + 1 == len(verdicts)
            end = len(stmt) if last else found[verdicts[n + 1][0]][1]
            signature = ' '.join(stmt[pos:end].split()) + (end_mark if last else '')
            entries.append((bisect.bisect_left(newlines, offs[pos]) + 1, reason, signature))
        del buf[:], offs[:]

    paren = 0
    line_start = False
    i = open_at + 1
    while i < close_at:
        c = clean[i]
        if c.isspace():
            line_start = line_start or c == '\n'
            buf.append(' ')
            offs.append(i)
            i += 1
            continue
        if line_start and c == '#':
            # A preprocessor line (splices included) belongs to no statement: the declarations
            # between `#if` and `#endif` are walked like any other (every branch of them).
            while i < close_at and clean[i] != '\n':
                if clean[i] == '\\':
                    j = i + 1 + (clean[i + 1:i + 2] == '\r')
                    if clean[j:j + 1] == '\n':
                        i = j + 1
                        continue
                i += 1
            continue
        line_start = False
        if c == '{':
            end = _match_brace(clean, i, close_at)
            body = False
            if paren == 0:
                stmt, _, verdicts, ctor_init = classify()
                # In a constructor's initialiser list a '{' after a member name is a braced
                # initialiser; the body's '{' follows the last initialiser's ')' or '}'. A
                # friend's '{' can only open a function body (a hidden friend): a friend
                # declaration has no initialiser and cannot define a class.
                body = bool(verdicts) and (verdicts[-1][1] == 'friend' or (
                    verdicts[-1][1] is None
                    and (not ctor_init or stmt.rstrip().endswith((')', '}', '...')))))
            if body:
                flush(' {...}')
            else:
                # A nested type, a braced initialiser or a braced default argument.
                buf.extend('{...}')
                offs.extend([i] * 5)
            i = end + 1
            continue
        if c == ';':
            flush(';')
            paren = 0
            i += 1
            continue
        if (c == ':' and paren == 0 and clean[i + 1:i + 2] != ':' and clean[i - 1] != ':'):
            label = ACCESS_LABEL.search(''.join(buf))
            if label:
                flush('', label.start())
                i += 1
                continue
        if c == '(':
            paren += 1
        elif c == ')':
            paren = max(0, paren - 1)
        elif c == '}':
            i += 1  # a '}' with no '{' in the walk (one hidden on a preprocessor line)
            continue
        buf.append(c)
        offs.append(i)
        i += 1
    flush('')
    first = clean.count('\n', 0, start) + 1
    last = clean.count('\n', 0, close_at) + 1
    return first, last, entries


SELF_TEST_SOURCE = '''class Target;
class TargetMenu { void NotMine(); };
/* class Target { void InAComment(); }; */
class Target : public Base
{
        friend void Pal::Touch(Target* t);
    public:
        Target();
        ~Target();
        void Method(int a, int b) const;
        int Inline() const { return m_a; }
        bool Open(int x)
        {
            if (x) { Close(); }
            return Close();
        }
        void Split(int a,
                   int b);
        typedef void (*Callback)(int);
        // void Commented(int);
        /* legacy */ void Block(int);
        * void Star(int);
#define TARGET_MACRO(x) (x);
        struct Inner { void Nested(); };
        void Braced(int x) {
            switch (x) { case 1: Call(x); }
            case 2: Call(x);
            default: Call(x);
        }
        void Pair() { a(); } void Next(int);
        char const* m_text = "{ not a brace (";
        void Label(); public: void Other(int);
        std::function<void(int)> m_fn;
        void Trailing(int); // a trailing comment hides a declaration from R1
        label: return Next();
        m_z = flag ? 1 : do_it(y);
        void Spaced(int)   ; \t
};
void Target::Outside();
'''

# The lines of SELF_TEST_SOURCE the pipeline counts (checked against the shell pipeline itself):
#   6 friend function, 8-10 ctor/dtor/method, 24 a nested type's line, 25 ends in '{',
#   28 `default:` is not a control keyword, 30 two statements ending in ';', 32 an access label
#   mid-line, 33 a member whose type has parens, 36 `do_it` is not the word `do`, 37 spaces
#   before the ';' and a space and a tab after it (spelled \t, so no editor strips it). Not counted: 11/14/26 end in '}', 12/13/16/29 end in neither,
#   15/27/35 a ':' then a control keyword, 17/18 the parens split over two lines, 19 typedef,
#   20/21/22/23 start with //, /*, * or #, 31 no ')', 34 a trailing comment.
SELF_TEST_EXPECTED = [6, 8, 9, 10, 24, 25, 28, 30, 32, 33, 36, 37]

# Characters that str.splitlines() breaks at and grep does not. They are built with chr() so
# no editor or tool turns an escape in this file into the character itself.
NOT_NEWLINES = [chr(0x0b), chr(0x0c), chr(0x1c), chr(0x1d), chr(0x1e), chr(0x85), chr(0x2028), chr(0x2029)]

# (label, source, class, expected (first, last, counted lines)).
EDGE_CASES = [
    # Line numbers are '\n' only: a comment full of the other separators is one line, so the two
    # methods after it are lines 4 and 5 and the body closes on line 6.
    ('separators that are not newlines',
     'class Odd\n{\n    // ' + ' '.join(NOT_NEWLINES) + '\n    void A(int);\n    void B(int);\n};\nvoid After(int);\n',
     'Odd', (1, 6, [4, 5])),
    # An apostrophe with no partner on its line (a preprocessor line) ends at the newline: it
    # must not swallow the closing brace and run on to the next apostrophe.
    ('an unpaired apostrophe ends at the newline',
     "class Quote\n{\n#define QUOTE_ME don't\n    void A(int);\n};\nvoid Outside(int);\nchar c = 'x';\n",
     'Quote', (1, 5, [4])),
    # A line splice inside a string literal keeps its newline, so the range still ends on the
    # brace's own line.
    ('a line splice inside a string literal',
     'class Spliced\n{\n    char const* m_s = "a' + chr(92) + '\nb";\n    void A(int);\n};\n',
     'Spliced', (1, 6, [5])),
    # CRLF: the '\r' stays on the line, as grep sees it, and `[;{]\s*$` still ends the statement.
    ('CRLF line endings',
     SELF_TEST_SOURCE.replace('\n', '\r\n'), 'Target', (4, 38, SELF_TEST_EXPECTED)),
]

# --all: one fixture holding every case the rules name. Line numbers are the fixture's own.
ALL_TEST_SOURCE = '''class Widget;
class WidgetBase { void NotMine(); int NotMineEither() { return 0; } };
/* class Widget { void InAComment(); }; */
class Widget : public WidgetBase
{
        friend class Pal;
        friend void Pal::Touch(Widget* w);
        Q_OBJECT
    public:
        Widget();                                   // a trailing comment hides it from R1
        explicit Widget(int size) : m_size(size), m_list{1, 2} { Init(); }
        ~Widget() override;
        Widget(Widget const&) = delete;
        Widget& operator=(Widget const&) = default;
        bool operator<(Widget const& other) const;
        void operator()(int x) { m_size = x; }
        explicit operator bool() const;
        virtual void Draw() const = 0;
        static Widget* Create(int size);
        int Size() const { return m_size; }
        bool Resize(int w,
                    int h) const;
        void Multi(int x)
        {
            if (x) { Draw(); }
            for (int i = 0; i < x; ++i) { Draw(); }
        }
        template<class T>
        T Get(std::map<int, std::vector<T>> const& table) const;
        void Log(char const* text = "a ( b ; c { d");
        char Sep() const { return '{'; }
        /* void Hidden(); foo(); */
#if WIDGET_EXTRA
        void Extra(int);
#endif
        DECLARE_SOMETHING(Widget)
        void AfterMacro();
        DECLARE_SOMETHING_ELSE(Widget);
        typedef void (*Callback)(int);
        using Handler = std::function<void(int)>;
        using WidgetBase::NotMine;
        static_assert(sizeof(int) == 4, "int");
        struct Inner { void Nested(); int Nested2() { return 1; } };
        enum Mode { MODE_A, MODE_B };
        struct Inner* FindInner(int id);
        void (*Pick(int which))(int);
        void Pair() { Draw(); } void Next(int);
        void Fill(std::vector<int> v = {});
        auto Trailing() -> int;
    private:
        void (*m_callback)(int);
        void (Widget::*m_member)(int) = nullptr;
        std::function<void(int)> m_fn;
        std::function<void()> m_later = [this]() { Draw(); };
        int m_size = Compute(1, 2);
        int m_other{Compute(3)};
        std::vector<int> m_list;
        char const* m_text = "void Fake();";
        static int s_count;
        unsigned m_flag : 1;
    protected: void Tail(int);
};
void Widget::Outside();
'''

# Every class-scope statement of ALL_TEST_SOURCE: (first line, reason), None = a member function.
# Counted (25): 10 ctor with a trailing comment, 11 ctor whose initialiser list holds a braced
# initialiser, 12 dtor, 13 `= delete`, 14 `= default` operator=, 15 operator<, 16 operator() inline,
# 17 conversion, 18 `= 0`, 19 static, 20 one-liner, 21 two-line signature, 23 multi-line body,
# 28 template member (listed at its `template` line), 30 a literal holding '(', ';' and '{',
# 31 a char literal '{', 34 inside #if/#endif, 37 the declaration after a macro with no ';',
# 45 an elaborated return type, 46 a function returning a function pointer, 47 twice (a
# one-liner and a declaration on one line), 48 a braced default argument, 49 a trailing return
# type, 61 after an access label on the same line. Line 32's comment and every body are not walked.
ALL_TEST_EXPECTED = [
    (6, 'friend'), (7, 'friend'), (8, 'no parameter list'),
    (10, None), (11, None), (12, None), (13, None), (14, None), (15, None), (16, None),
    (17, None), (18, None), (19, None), (20, None), (21, None), (23, None), (28, None),
    (30, None), (31, None), (34, None),
    (36, 'macro invocation'), (37, None), (38, 'macro invocation'),
    (39, 'typedef/using'), (40, 'typedef/using'), (41, 'typedef/using'), (42, 'static_assert'),
    (43, 'nested type'), (44, 'nested type'),
    (45, None), (46, None), (47, None), (47, None), (48, None), (49, None),
    (51, 'function-pointer data member'), (52, 'function-pointer data member'),
    (53, 'no parameter list'), (54, 'no parameter list'), (55, 'no parameter list'),
    (56, 'no parameter list'), (57, 'no parameter list'), (58, 'no parameter list'),
    (59, 'no parameter list'), (60, 'no parameter list'),
    (61, None),
]

# (label, source, class, expected (first, last, [(line, reason)])).
ALL_EDGE_CASES = [
    # A preprocessor line continued by a splice is skipped whole: its second line is no statement.
    ('a spliced preprocessor line',
     'class Spliced\n{\n#define SPLICED(x) ' + chr(92) + '\n    x;\n    void A(int);\n};\n',
     'Spliced', (1, 6, [(5, None)])),
    # A constructor's initialiser list on the lines after its signature, then its body.
    ('an initialiser list on its own line',
     'class Init\n{\n    Init(int a)\n        : m_a(a), m_b{a}\n    {\n    }\n    int m_a;\n    int m_b;\n};\n',
     'Init', (1, 9, [(3, None), (7, 'no parameter list'), (8, 'no parameter list')])),
    # A macro and a declaration on one line are two entries; a macro with no parentheses and no
    # access label after it reads as part of the next declaration (counted once, at its line).
    ('macros that run into a declaration',
     'class Macro\n{\n    DECLARE_SOMETHING(Macro) void A(int);\n    Q_OBJECT\n    void B();\n};\n',
     'Macro', (1, 6, [(3, 'macro invocation'), (3, None), (4, None)])),
    # Pointer declarators after a type that is a name (not `void`): data members, unless the
    # name inside the parentheses has a parameter list of its own.
    ('pointer declarators after a named type',
     'class Ptr\n{\n    Mode (*m_pick)(int);\n    Mode (Ptr::*m_member)(int);\n    Mode (*Pick(int which))(int);\n};\n',
     'Ptr', (1, 6, [(3, 'function-pointer data member'), (4, 'function-pointer data member'), (5, None)])),
    # A function type inside template arguments, after a name, is not a parameter list.
    ('template arguments holding a function type',
     'class Fn\n{\n    std::function<Fn(int)> m_make;\n    std::vector<std::function<Fn(int)>> m_makers;\n'
     '    Fn Make(std::function<Fn(int)> f);\n};\n',
     'Fn', (1, 6, [(3, 'no parameter list'), (4, 'no parameter list'), (5, None)])),
    # A template prefix is read past: a template friend and a nested template type are not members.
    ('template prefixes',
     'class Tpl\n{\n    template<class T> friend void Touch(T* t);\n    template<class T> struct Rebind { void Nested(); };\n'
     '    template<class T, class U = std::pair<T, T>>\n    U Pair(T a, T b) const { return U(a, b); }\n};\n',
     'Tpl', (1, 7, [(3, 'friend'), (4, 'nested type'), (5, None)])),
    # A friend defined in the class (a hidden friend) ends at its body: the member after it
    # is a statement of its own, not part of the friend.
    ('hidden friends with bodies',
     'class Pal\n{\n    friend bool operator==(Pal const& a, Pal const& b) { return true; }\n'
     '    void AfterFriend();\n    template<class T> friend void Touch(T* t) { t->x(); }\n'
     '    void AfterTemplateFriend();\n};\n',
     'Pal', (1, 7, [(3, 'friend'), (4, None), (5, 'friend'), (6, None)])),
    # A macro after a specifier or a template prefix (past the leading-macro check) is still one.
    ('macros after a specifier or a template prefix',
     'class Spec\n{\n    static DECLARE_STATIC(Spec);\n    template<class T> DECLARE_T(T);\n    void After();\n};\n',
     'Spec', (1, 6, [(3, 'macro invocation'), (4, 'macro invocation'), (5, None)])),
    ('CRLF line endings (--all)',
     ALL_TEST_SOURCE.replace('\n', '\r\n'), 'Widget', (4, 62, ALL_TEST_EXPECTED)),
]


def all_self_test():
    """The --all scanner's self-test; True when it passes."""
    ok = True
    first, last, entries = scan_members(ALL_TEST_SOURCE, 'Widget')
    got = [(line, reason) for line, reason, _ in entries]
    if (first, last) != (4, 62) or got != ALL_TEST_EXPECTED:
        print('self-test (--all): range %d-%d, expected 4-62' % (first, last))
        for line, reason, signature in entries:
            print('  %d: %s  [%s]' % (line, signature, reason or 'counted'))
        ok = False
    methods = sum(1 for _, reason, _ in entries if reason is None)
    if methods != 25:
        print('self-test (--all): %d methods, expected 25' % methods)
        ok = False
    for label, source, name, expected in ALL_EDGE_CASES:
        first, last, entries = scan_members(source, name)
        got = (first, last, [(line, reason) for line, reason, _ in entries])
        if got != expected:
            print('self-test (%s): got %r, expected %r' % (label, got, expected))
            ok = False
    try:
        scan_members(ALL_TEST_SOURCE, 'WidgetBas')
        print('self-test (--all): class WidgetBas did not raise')
        ok = False
    except ValueError:
        pass
    return ok


def self_test():
    ok = True
    try:
        first, last, counted = count_methods(SELF_TEST_SOURCE, 'Target')
    except ValueError as err:
        print('self-test: %s' % err)
        return 1
    if (first, last) != (4, 38):
        print('self-test: range %d-%d, expected 4-38' % (first, last))
        ok = False
    got = [line for line, _ in counted]
    if got != SELF_TEST_EXPECTED:
        print('self-test: counted lines %r, expected %r' % (got, SELF_TEST_EXPECTED))
        for line, raw in counted:
            print('  %d: %s' % (line, raw))
        ok = False
    # A class keyword on its own line with the name on the next still opens the range there.
    first, last, counted = count_methods('\nclass\n    Wrapped\n{\n    void A();\n};\n', 'Wrapped')
    if (first, last, len(counted)) != (2, 6, 1):
        print('self-test: wrapped definition gave %d-%d, %d methods' % (first, last, len(counted)))
        ok = False
    for missing in ('Missing', 'TargetMen'):
        try:
            count_methods(SELF_TEST_SOURCE, missing)
            print('self-test: class %s did not raise' % missing)
            ok = False
        except ValueError:
            pass
    for label, source, name, expected in EDGE_CASES:
        try:
            got = count_methods(source, name)
            got = (got[0], got[1], [line for line, _ in got[2]])
        except ValueError as err:
            got = str(err)
        if got != expected:
            print('self-test (%s): got %r, expected %r' % (label, got, expected))
            ok = False
    if not all_self_test():
        ok = False
    print('self-test %s' % ('OK' if ok else 'FAILED'))
    return 0 if ok else 1


def main(argv):
    parser = argparse.ArgumentParser(description='R1: the D5 method regex over one class body '
                                                 '(--all: every member function).')
    parser.add_argument('--self-test', action='store_true', help='run the self-test and exit')
    parser.add_argument('--class', dest='name', help='the class to measure, e.g. Player')
    parser.add_argument('--header', help='the header that defines it')
    parser.add_argument('--list', action='store_true',
                        help='print every counted line (with --all: every class-scope statement,'
                             ' the ones not counted marked)')
    parser.add_argument('--all', action='store_true',
                        help='count every member function (a brace-aware walk) instead of R1')
    args = parser.parse_args(argv[1:])
    if args.self_test:
        return self_test()
    if not args.name or not args.header:
        parser.print_usage()
        return 2
    # newline='': no translation, so a '\r' stays where grep sees it and only '\n' ends a line.
    with open(args.header, encoding='utf-8', errors='replace', newline='') as f:
        text = f.read()
    if args.all:
        first, last, entries = scan_members(text, args.name)
        methods = [e for e in entries if e[1] is None]
        if args.list:
            for line, reason, signature in entries:
                mark = '' if reason is None else '[not counted: %s] ' % reason
                print('%s:%d: %s%s' % (args.header, line, mark, signature))
        print('%s methods=%d (--all over %s:%d-%d; %d other class-scope statements)'
              % (args.name, len(methods), args.header, first, last, len(entries) - len(methods)))
        return 0
    first, last, counted = count_methods(text, args.name)
    if args.list:
        for line, raw in counted:
            print('%s:%d: %s' % (args.header, line, raw.strip()))
    print('%s methods=%d (R1 over %s:%d-%d)' % (args.name, len(counted), args.header, first, last))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
