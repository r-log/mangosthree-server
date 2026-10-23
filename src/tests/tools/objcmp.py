#!/usr/bin/env python3
"""objcmp.py --per-object | --by-symbol BASE HEAD [options] | --initializers DIR | --diff-lines TREE BASE HEAD OUT |
           --lines-compiled SIDE [options] | --self-test [--cc <compiler>] [--build-script <objcmp_build.sh>]

Compares the object files of two builds of the same configuration: BASE built from one commit,
HEAD from another, each a directory holding the objects at their build-relative paths (*.o, ELF64
little-endian, or *.obj, MSVC COFF). The verdict is IDENTICAL or DIFFERENT, with exit code 0 or 1
(2 for a usage or input error). There is no third verdict.

--per-object (ELF and COFF)
  Pairs each object with the one at the same path, or with the one --map names for its source
  (lines `old/path.cpp -> new/path.cpp`). Every section that is not free must match by name and
  order, with equal type, flags, alignment and bytes; every relocation must match by offset, type,
  addend and target, the target taken by symbol name; every symbol by name, binding, type,
  visibility, section, value and size; every COMDAT group by signature and members.
  Free to differ: debug sections, .comment, notes, the COFF time stamp and .chks64, and the ELF
  STT_FILE symbol of a renamed pair.
  A string that is a path may differ only where --literals names it (lines `old -> new`): the
  base string ends with /old and the head string with /new, with the same prefix (separators
  `\\` and `/` taken as one). Relocations into string sections are then compared by the string
  they reach, not by its offset.
  A pair whose paths differ (a moved file) fails when either object has a dynamic initializer
  (ELF .init_array, .ctors, .preinit_array; COFF .CRT$X*).

--by-symbol (ELF)
  Reads every object of both builds. Each allocated section is one item: a function
  (-ffunction-sections) or data object (-fdata-sections), keyed by the names it defines: its
  global and weak names, else its local names, else its section name. An item's content is its
  section's type, flags, alignment, size, bytes, COMDAT signature, the symbols it defines (name,
  offset, size, type, binding, visibility), its relocations and its unwind records (each .eh_frame
  FDE that starts in the section, at its offset, with its CIE, trailing DW_CFA_nop padding left
  out; a thunk GCC places after its function has its own). An unwind record that starts in no
  item is compared by content: the base and head must hold the same such records, in any
  object. A relocation is taken by offset, type and addend and by its target: a global or
  undefined symbol by name; a local item by its name and its own content (so a moved function
  that now reaches a different static of the same name differs); a string or constant in a
  mergeable section (-fmerge-constants) by the bytes it reaches, wherever they are in the pool.
  Two items are equal when their contents are; the base and head sets must hold the same contents
  under every key. Copies of one inline function or one header constant in several objects count
  once, so a function may move between objects but may not change.
  A key in only one of the two sets fails, unless --map renames it (lines `old -> new`, mangled
  names): the head's new name is then read as the old one.
  An object is moved when its path is in one build only, and changed when a global non-COMDAT
  definition moved into or out of it or its initializer list changed. A moved or changed object
  with a dynamic initializer in either build fails, whatever the initializer does: an .init_array,
  .ctors or .preinit_array entry, or a thread-local initialization (__tls_init, _ZTH*, _ZTW*). The
  report names the variables each such initializer reaches. A function GCC makes for initializers
  (_GLOBAL__sub_I_*, __static_initialization_and_destruction_*, the thread-local ones) that
  differs counts as an initializer difference.
  With --changes (`git diff --name-status -M BASE HEAD`) and --base-lines, --head-lines (the
  --lines-compiled verdicts of each build), every changed line of a C, C++ or assembler file must
  be one the build that has it compiled; a renamed or copied file must also be read by an object
  of each build (--base-deps, --head-deps: `ninja -t deps`; --root is the source directory they
  name). A line that is not compiled fails as its own kind. Without line verdicts the check is per
  file. A changed file that is not code is listed as not code. A change to the object proof's own
  files (objcmp.py, objcmp_build.sh, MangosProof.cmake, the workflow) fails: the proof cannot
  vouch for itself.

--diff-lines TREE BASE HEAD OUT
  Writes the changed lines of every C, C++ or assembler file between two commits of the git tree
  TREE: lines whose code differs once comments are taken out and runs of whitespace made one (a
  raw string keeps its exact text). Each row is `side<TAB>path<TAB>line`, base for a line of the
  old version, head for one of the new.

--lines-compiled SIDE --changed-lines F --deps F --compile-db F --root DIR --build DIR --out F
  Run after a build, with SIDE's commit checked out: preprocesses (`-E -dD -dI`) TUs that read the
  changed files, up to 24 per file, and maps the line markers back. A changed line is compiled
  when a TU emitted it (or the line of the backslash-continued line or the function-like macro call
  it is part of); a blank, comment or `#if 0` line needs nothing; the include guard and `#pragma
  once` need the file to be read. A line that is an argument of a macro the build defines without
  using that parameter is not compiled. A changed conditional directive must be the same text moved:
  the base's and the head's changed directives must pair up.

--initializers DIR
  Lists the objects under DIR that carry a dynamic initializer.

--self-test
  Builds synthetic ELF and COFF objects, applies mutants (a changed byte, a changed call target,
  a moved function, an added or a moved-into dynamic initializer, a renamed symbol with and
  without a map, a changed string, a moved function that binds another static of the same name,
  a second unwind record inside a function, an unwind record that starts in no function (the
  same, moved, added, changed), a path string with and without --literals, a thread-local
  initializer or wrapper in a moved object, a change to the proof's own files) and checks each
  verdict. Its line cases run --lines-compiled with a stand-in preprocessor: a change inside
  #ifdef _WIN32, uncommented code there, a changed conditional directive, an argument of a macro
  that drops it, a changed .asm or test file no build compiles fail; a compiled line, a comment,
  #if 0, a renamed include guard and a moved conditional block pass. With --cc it also compiles
  the same shapes with that GCC in the proof configuration (and _WIN32, MANGOS_STRICT_TICK, an
  assert, a macro call over two lines, a moved thread_local). With --build-script it builds a
  small git tree twice with that objcmp_build.sh: a header the head adds ahead of another on the
  include path, and a changed option default, must differ.
"""
import argparse
import collections
import hashlib
import json
import os
import posixpath
import re
import shlex
import shutil
import struct
import subprocess
import sys
import tempfile
import traceback

SHT_NULL, SHT_PROGBITS, SHT_SYMTAB, SHT_STRTAB, SHT_RELA = 0, 1, 2, 3, 4
SHT_NOTE, SHT_NOBITS, SHT_REL = 7, 8, 9
SHT_INIT_ARRAY, SHT_FINI_ARRAY, SHT_PREINIT_ARRAY, SHT_GROUP, SHT_SYMTAB_SHNDX = 14, 15, 16, 17, 18
SHF_WRITE, SHF_ALLOC, SHF_EXECINSTR, SHF_MERGE, SHF_STRINGS, SHF_GROUP = 0x1, 0x2, 0x4, 0x10, 0x20, 0x200
STB_LOCAL, STB_GLOBAL, STB_WEAK = 0, 1, 2
STT_NOTYPE, STT_OBJECT, STT_FUNC, STT_SECTION, STT_FILE = 0, 1, 2, 3, 4
SHN_UNDEF, SHN_LORESERVE, SHN_ABS, SHN_COMMON, SHN_XINDEX = 0, 0xff00, 0xfff1, 0xfff2, 0xffff
EM_X86_64, EM_AARCH64 = 62, 183
X86_PC_RELATIVE = {2, 4, 9, 26, 41, 42}
STRUCTURAL = {SHT_NULL, SHT_SYMTAB, SHT_STRTAB, SHT_RELA, SHT_REL, SHT_GROUP, SHT_SYMTAB_SHNDX}
INIT_TYPES = {SHT_INIT_ARRAY, SHT_FINI_ARRAY, SHT_PREINIT_ARRAY}
CODE_EXTENSIONS = ('.c', '.cc', '.cpp', '.cxx', '.h', '.hh', '.hpp', '.hxx', '.inl', '.ipp', '.tpp', '.inc', '.s',
                   '.asm')
GATE_FILES = ('src/tests/tools/objcmp.py', 'src/tests/tools/objcmp_build.sh', 'cmake/MangosProof.cmake',
              '.github/workflows/core_object_proof.yml')


def digest(*parts):
    h = hashlib.blake2b(digest_size=16)
    for p in parts:
        if isinstance(p, bytes):
            h.update(struct.pack('<Q', len(p)))
            h.update(p)
        else:
            r = repr(p).encode('utf-8', 'surrogateescape')
            h.update(struct.pack('<Q', len(r)))
            h.update(r)
    return h.digest()


def cstr(d, off):
    return d[off:d.index(b'\0', off)].decode('latin-1')


# ---------------------------------------------------------------------------------------------------
# ELF64

class Section(object):
    __slots__ = ('index', 'name', 'type', 'flags', 'off', 'size', 'link', 'info', 'align', 'entsize', 'data')


class Symbol(object):
    __slots__ = ('name', 'value', 'size', 'bind', 'type', 'vis', 'shndx')


class Elf(object):
    """One ELF64 little-endian relocatable object: sections, symbols, relocations by target section,
    COMDAT groups by member section."""

    def __init__(self, path, data):
        d = data
        if d[:4] != b'\x7fELF' or d[4] != 2 or d[5] != 1:
            raise ValueError('%s: not an ELF64 little-endian object' % path)
        (self.etype, self.machine, _, _, _, shoff, _, _, _, _, _, shnum, shstrndx) = struct.unpack_from(
            '<HHIQQQIHHHHHH', d, 16)
        if shnum == 0 and shoff:
            shnum = struct.unpack_from('<Q', d, shoff + 32)[0]
        if shstrndx == SHN_XINDEX:
            shstrndx = struct.unpack_from('<I', d, shoff + 40)[0]
        self.path = path
        self.secs = []
        names = []
        for i in range(shnum):
            s = Section()
            (nameoff, s.type, s.flags, _, s.off, s.size, s.link, s.info, s.align, s.entsize) = struct.unpack_from(
                '<IIQQQQIIQQ', d, shoff + 64 * i)
            s.index = i
            s.data = None if s.type in (SHT_NOBITS, SHT_NULL) else d[s.off:s.off + s.size]
            self.secs.append(s)
            names.append(nameoff)
        shstr = self.secs[shstrndx]
        for s, n in zip(self.secs, names):
            s.name = cstr(d, shstr.off + n)
        self.syms = []
        symtab = [s for s in self.secs if s.type == SHT_SYMTAB]
        if symtab:
            st = symtab[0]
            strtab = self.secs[st.link]
            xindex = None
            for s in self.secs:
                if s.type == SHT_SYMTAB_SHNDX and s.link == st.index:
                    xindex = struct.unpack_from('<%dI' % (s.size // 4), d, s.off)
            for k, (nm, info, other, shndx, value, size) in enumerate(struct.iter_unpack('<IBBHQQ', st.data)):
                y = Symbol()
                y.name = cstr(d, strtab.off + nm) if nm else ''
                y.value, y.size, y.bind, y.type, y.vis = value, size, info >> 4, info & 15, other & 3
                y.shndx = xindex[k] if shndx == SHN_XINDEX and xindex else shndx
                self.syms.append(y)
        self.relocs = {}
        for s in self.secs:
            if s.type == SHT_RELA:
                self.relocs[s.info] = [(o, i & 0xffffffff, i >> 32, a) for o, i, a in struct.iter_unpack('<QQq', s.data)]
            elif s.type == SHT_REL:
                self.relocs[s.info] = [(o, i & 0xffffffff, i >> 32, 0) for o, i in struct.iter_unpack('<QQ', s.data)]
        self.group_of = {}
        self.groups = []
        for s in self.secs:
            if s.type == SHT_GROUP:
                words = struct.unpack_from('<%dI' % (s.size // 4), s.data)
                sig = self.syms[s.info].name if s.info < len(self.syms) else ''
                self.groups.append((sig, words[0], [self.secs[m].name for m in words[1:]]))
                for m in words[1:]:
                    self.group_of[m] = sig


def elf_free(s):
    n = s.name
    return (n.startswith('.debug') or n.startswith('.zdebug') or n.startswith('.note') or n == '.comment'
            or s.type == SHT_NOTE or n == '.llvm_addrsig' or n.startswith('.gnu.lto') or n == '.gnu.attributes')


def elf_init(s):
    n = s.name
    return (s.type in INIT_TYPES or n in ('.ctors', '.dtors') or n.startswith('.ctors.') or n.startswith('.dtors.')
            or n.startswith('.init_array') or n.startswith('.fini_array') or n.startswith('.preinit_array'))


def tls_init_name(name):
    """Whether a symbol is one GCC makes for a thread-local variable's dynamic initialization."""
    return name == '__tls_init' or name.startswith('_ZTH') or name.startswith('_ZTW')


def elf_has_init(e):
    return (any(elf_init(s) and s.size for s in e.secs)
            or any(tls_init_name(y.name) and 0 < y.shndx < SHN_LORESERVE for y in e.syms))


def pool_ref(sec, offset, canon=None):
    """What a relocation into a mergeable section reaches: the string or constant at OFFSET, or the
    raw offset and the pool's hash when OFFSET is not the start of one."""
    d, w = sec.data, max(sec.entsize, 1)
    if sec.flags & SHF_STRINGS:
        if 0 <= offset < len(d) and offset % w == 0 and (offset == 0 or d[offset - w:offset] == b'\0' * w):
            end = offset
            while end + w <= len(d) and d[end:end + w] != b'\0' * w:
                end += w
            s = d[offset:end]
            return ('str', canon(s) if canon else s)
    elif 0 <= offset and offset + w <= len(d) and offset % w == 0:
        return ('cst', d[offset:offset + w])
    return ('pool', sec.name, digest(d), offset)


def ref_bias(e, rtype, referrer):
    """The distance from a relocation's addend to the offset it reaches inside its target, for a
    reference from an instruction (x86-64 PC-relative operands end 4 bytes after the field)."""
    if e.machine == EM_X86_64 and rtype in X86_PC_RELATIVE and referrer.flags & SHF_EXECINSTR:
        return 4
    return 0


def eh_body(d, off, hdr, end, rels):
    """A CIE or FDE without its length field and without the DW_CFA_nop bytes that pad it to the
    next record's alignment (never cutting into a relocated field)."""
    floor = off + hdr
    for o, t, _, _ in rels:
        floor = max(floor, o + (8 if t in (1, 24, 257) else 4))
    while end > floor and d[end - 1] == 0:
        end -= 1
    return d[off + hdr:end]


def split_strings(sec):
    d, w = sec.data, max(sec.entsize, 1)
    out, start = [], 0
    for i in range(0, len(d) - w + 1, w):
        if d[i:i + w] == b'\0' * w:
            out.append(d[start:i])
            start = i + w
    if start < len(d):
        out.append(d[start:])
    return out


# ---------------------------------------------------------------------------------------------------
# Path literals

class Literals(object):
    """The path strings a change may alter: base `<prefix>/<old>` against head `<prefix>/<new>`."""

    def __init__(self, pairs):
        self.pairs = [(o.replace('\\', '/').strip('/'), n.replace('\\', '/').strip('/')) for o, n in pairs]

    def canon(self, side):
        def f(s):
            try:
                t = s.decode('latin-1').replace('\\', '/')
            except Exception:
                return s
            for k, (o, n) in enumerate(self.pairs):
                p = o if side == 'base' else n
                if t == p or t.endswith('/' + p):
                    return ('literal', k, t[:len(t) - len(p)])
            return s
        return f


def read_pairs(path):
    pairs = []
    if not path:
        return pairs
    for line in open(path, encoding='utf-8'):
        line = line.strip()
        if not line or line.startswith('#'):
            continue
        if ' -> ' not in line:
            raise ValueError('%s: not a `old -> new` line: %s' % (path, line))
        a, b = line.split(' -> ', 1)
        pairs.append((a.strip(), b.strip()))
    return pairs


# ---------------------------------------------------------------------------------------------------
# Per-object, ELF

def elf_object_view(e, canon=None):
    canon = canon or (lambda x: x)
    secs = [s for s in e.secs if s.type not in STRUCTURAL and not elf_free(s)]
    names = {s.index: s.name for s in e.secs}

    def target(rsym, addend, referrer, rtype):
        y = e.syms[rsym]
        if y.shndx == SHN_UNDEF:
            return ('undef', y.name, addend)
        if y.shndx in (SHN_ABS, SHN_COMMON):
            return ('special', y.name, y.shndx, y.value, addend)
        t = e.secs[y.shndx]
        if t.flags & SHF_MERGE and t.data is not None:
            bias = ref_bias(e, rtype, referrer)
            return ('pool', y.type == STT_SECTION and '' or y.name, pool_ref(t, y.value + addend + bias, canon), bias)
        if y.type == STT_SECTION:
            return ('section', t.name, addend)
        return ('symbol', y.name, addend)

    view = []
    for s in secs:
        if s.flags & SHF_MERGE and s.flags & SHF_STRINGS and s.data is not None:
            body = ('strings', [canon(x) for x in split_strings(s)])
        else:
            body = ('bytes', s.size, s.data)
        rels = [(o, t, target(y, a, s, t)) for o, t, y, a in e.relocs.get(s.index, [])]
        view.append((s.name, (s.type, s.flags, s.align, s.entsize), body, rels))
    syms = []
    for y in e.syms:
        if y.type == STT_FILE:
            continue
        if 0 < y.shndx < SHN_LORESERVE and elf_free(e.secs[y.shndx]):
            continue
        where = names.get(y.shndx, y.shndx) if y.shndx < SHN_LORESERVE else y.shndx
        value = y.value
        if 0 < y.shndx < SHN_LORESERVE and e.secs[y.shndx].flags & SHF_MERGE and y.type != STT_SECTION:
            value = pool_ref(e.secs[y.shndx], y.value, canon)
        syms.append((y.name, y.bind, y.type, y.vis, where, value, y.size))
    files = [y.name for y in e.syms if y.type == STT_FILE]
    return view, syms, files, sorted(e.groups)


def compare_elf_pair(a, b, literals, renamed):
    A, B = elf_object_view(a, literals.canon('base')), elf_object_view(b, literals.canon('head'))
    Ar, Br = elf_object_view(a, None), elf_object_view(b, None)
    other, notes = [], []
    counts = dict(code=0, code_eq=0, data=0, data_eq=0, literals=0)
    if [v[0] for v in A[0]] != [v[0] for v in B[0]]:
        only_a = [v[0] for v in A[0] if v[0] not in set(x[0] for x in B[0])]
        only_b = [v[0] for v in B[0] if v[0] not in set(x[0] for x in A[0])]
        other.append('section list differs: %d -> %d sections; only in base %s; only in head %s' % (
            len(A[0]), len(B[0]), only_a[:3], only_b[:3]))
    else:
        for (sa, sb, ra, rb) in zip(A[0], B[0], Ar[0], Br[0]):
            kind = 'code' if sa[1][1] & SHF_EXECINSTR else 'data'
            counts[kind] += 1
            if sa[1] != sb[1]:
                other.append('section %s attributes differ: %s -> %s' % (sa[0], sa[1], sb[1]))
                continue
            if sa[2] != sb[2]:
                other.append('section %s (%s) contents differ' % (sa[0], kind))
                continue
            if sa[3] != sb[3]:
                bad = [(x, y) for x, y in zip(sa[3], sb[3]) if x != y]
                other.append('section %s relocations differ (%d -> %d): %s' % (sa[0], len(sa[3]), len(sb[3]), bad[:2]))
                continue
            if ra[2] != rb[2] or ra[3] != rb[3]:
                counts['literals'] += 1
                notes.append('section %s: a listed path string differs' % sa[0])
            counts[kind + '_eq'] += 1
    if A[1] != B[1]:
        bad = [(x, y) for x, y in zip(A[1], B[1]) if x != y]
        other.append('symbols differ (%d -> %d): %s' % (len(A[1]), len(B[1]), bad[:2]))
    if A[2] != B[2] and not renamed:
        other.append('file symbol %s -> %s' % (A[2], B[2]))
    if A[3] != B[3]:
        other.append('COMDAT groups differ')
    if renamed and (elf_has_init(a) or elf_has_init(b)):
        other.append('INIT: a moved object has a dynamic initializer')
    free_diff = sum(1 for x, y in zip(sorted((s.name, s.data) for s in a.secs if elf_free(s)),
                                      sorted((s.name, s.data) for s in b.secs if elf_free(s))) if x != y)
    return other, notes, counts, free_diff


# ---------------------------------------------------------------------------------------------------
# Per-object, COFF (MSVC)

def coff_parse(path, d):
    big = d[:4] == b'\x00\x00\xff\xff' and struct.unpack_from('<H', d, 4)[0] >= 2
    if big:
        ts = struct.unpack_from('<I', d, 8)[0]
        nsec, symptr, nsyms = struct.unpack_from('<III', d, 44)
        sec0, symsz = 56, 20
    else:
        nsec, ts, symptr, nsyms = struct.unpack_from('<HIII', d, 2)
        sec0, symsz = 20, 18
    secs = []
    for i in range(nsec):
        name, vsize, vaddr, rawsz, rawptr, relptr, lnptr, nrel, nln, flags = struct.unpack_from(
            '<8sIIIIIIHHI', d, sec0 + 40 * i)
        raw = d[rawptr:rawptr + rawsz] if rawptr and not flags & 0x80 else b''
        rels = [struct.unpack_from('<IIH', d, relptr + 10 * k) for k in range(nrel)]
        secs.append(dict(name=name, size=rawsz, nrel=nrel, flags=flags, raw=raw, rels=rels))
    strtab_off = symptr + symsz * nsyms
    strtab = d[strtab_off:strtab_off + struct.unpack_from('<I', d, strtab_off)[0]] if nsyms else b''

    def sname(raw8):
        if raw8[:4] == b'\0\0\0\0':
            off = struct.unpack_from('<I', raw8, 4)[0]
            return strtab[off:strtab.index(b'\0', off)].decode('latin-1')
        return raw8.rstrip(b'\0').decode('latin-1')
    for s in secs:
        n = s['name']
        if n.startswith(b'/') and n[1:].rstrip(b'\0').isdigit():
            off = int(n[1:].rstrip(b'\0'))
            s['name'] = strtab[off:strtab.index(b'\0', off)].decode('latin-1')
        else:
            s['name'] = n.rstrip(b'\0').decode('latin-1')
    syms = []
    k = 0
    while k < nsyms:
        off = symptr + symsz * k
        raw8 = d[off:off + 8]
        if big:
            value, secno, typ, cls, naux = struct.unpack_from('<IiHBB', d, off + 8)
        else:
            value, secno, typ, cls, naux = struct.unpack_from('<IhHBB', d, off + 8)
        aux = d[off + symsz:off + symsz * (1 + naux)]
        syms.append(dict(idx=k, name=sname(raw8), value=value, sec=secno, type=typ, cls=cls, aux=aux))
        k += 1 + naux
    return dict(path=path, ts=ts, secs=secs, syms=syms)


def coff_free(name):
    return name.startswith('.debug$') or name == '.chks64'


def coff_has_init(c):
    return any(s['name'].startswith('.CRT$X') and s['size'] for s in c['secs'])


def compare_coff_pair(A, B, literals, renamed):
    other, notes = [], []
    counts = dict(code=0, code_eq=0, data=0, data_eq=0, literals=0)
    ca, cb = literals.canon('base'), literals.canon('head')
    free_diff = 0
    if len(A['secs']) != len(B['secs']):
        return ['section count %d -> %d' % (len(A['secs']), len(B['secs']))], notes, counts, 0
    allowed = set()
    for i, (a, b) in enumerate(zip(A['secs'], B['secs']), 1):
        if a['name'] != b['name']:
            other.append('section #%d name %s -> %s' % (i, a['name'], b['name']))
            continue
        same = (a['size'], a['nrel'], a['flags'], a['raw']) == (b['size'], b['nrel'], b['flags'], b['raw'])
        if coff_free(a['name']):
            free_diff += not same
            continue
        kind = 'code' if a['flags'] & 0x20 else 'data'
        counts[kind] += 1
        if same:
            counts[kind + '_eq'] += 1
            continue
        lit = (kind == 'data' and a['nrel'] == 0 and b['nrel'] == 0 and a['flags'] == b['flags']
               and a['raw'].endswith(b'\0') and b['raw'].endswith(b'\0'))
        if lit:
            ka, kb = ca(a['raw'].rstrip(b'\0')), cb(b['raw'].rstrip(b'\0'))
            lit = isinstance(ka, tuple) and ka == kb
        if lit:
            counts['literals'] += 1
            notes.append('literal section #%d: %s -> %s' % (i, a['raw'].rstrip(b'\0').decode('latin-1'),
                                                             b['raw'].rstrip(b'\0').decode('latin-1')))
            allowed.add(i)
        else:
            other.append('section #%d %s (%s) differs: size %d -> %d, nrel %d -> %d, flags %x -> %x' % (
                i, a['name'], kind, a['size'], b['size'], a['nrel'], b['nrel'], a['flags'], b['flags']))
    an = {s['idx']: s['name'] for s in A['syms']}
    bn = {s['idx']: s['name'] for s in B['syms']}
    lit_names = set(s['name'] for s in A['syms'] + B['syms'] if s['sec'] in allowed and s['name'].startswith('??_C@'))
    for i, (a, b) in enumerate(zip(A['secs'], B['secs']), 1):
        if coff_free(a['name']) or a['name'] != b['name']:
            continue
        ra = [(o, t, an.get(x)) for o, x, t in a['rels']]
        rb = [(o, t, bn.get(x)) for o, x, t in b['rels']]
        if ra != rb:
            bad = [(x, y) for x, y in zip(ra, rb) if x != y]
            ok = len(ra) == len(rb) and all(x[:2] == y[:2] and x[2] in lit_names and y[2] in lit_names for x, y in bad)
            if not ok:
                other.append('section #%d %s relocations differ: %s' % (i, a['name'], bad[:3]))
    if len(A['syms']) != len(B['syms']):
        other.append('symbol count %d -> %d' % (len(A['syms']), len(B['syms'])))
    else:
        for a, b in zip(A['syms'], B['syms']):
            ka = (a['name'], a['value'], a['sec'], a['type'], a['cls'], a['aux'])
            kb = (b['name'], b['value'], b['sec'], b['type'], b['cls'], b['aux'])
            if ka == kb:
                continue
            sec_ok = a['sec'] == b['sec'] and 0 < a['sec'] <= len(A['secs']) and (
                a['sec'] in allowed or coff_free(A['secs'][a['sec'] - 1]['name']))
            if sec_ok and a['cls'] == 3 and a['name'] == b['name']:
                continue
            if sec_ok and a['sec'] in allowed and a['name'] in lit_names and b['name'] in lit_names and ka[1:5] == kb[1:5]:
                continue
            if renamed and a['cls'] == 103 and b['cls'] == 103:
                continue
            other.append('symbol #%d %s -> %s differs' % (a['idx'], a['name'], b['name']))
    if renamed and (coff_has_init(A) or coff_has_init(B)):
        other.append('INIT: a moved object has a dynamic initializer')
    return other, notes, counts, free_diff


# ---------------------------------------------------------------------------------------------------
# Object discovery and pairing

def list_objects(root):
    out = []
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames.sort()
        for f in sorted(filenames):
            if f.endswith('.o') or f.endswith('.obj'):
                out.append(posixpath.normpath(os.path.relpath(os.path.join(dirpath, f), root).replace('\\', '/')))
    return out


def object_source(rel):
    """A CMake object's source path: `src/game/CMakeFiles/game.dir/Object/Spell.cpp.o` is
    `src/game/Object/Spell.cpp`."""
    p = rel
    i = p.find('/CMakeFiles/')
    if i >= 0:
        j = p.find('.dir/', i)
        if j >= 0:
            p = p[:i] + '/' + p[j + 5:]
    elif p.startswith('CMakeFiles/'):
        j = p.find('.dir/')
        p = p[j + 5:]
    for ext in ('.o', '.obj'):
        if p.endswith(ext):
            p = p[:-len(ext)]
    return p.lstrip('/')


def pair_objects(base_list, head_list, renames):
    """Pairs by path, or by a rename of the object's source. Returns pairs and the unpaired."""
    head_set = set(head_list)
    head_by_source = {}
    for h in head_list:
        head_by_source.setdefault(object_source(h), []).append(h)
    pairs, used = [], set()
    unpaired_base = []
    for b in base_list:
        src = object_source(b)
        if src in renames:
            cand = head_by_source.get(renames[src], [])
            if len(cand) == 1 and cand[0] not in used:
                pairs.append((b, cand[0]))
                used.add(cand[0])
                continue
        if b in head_set and b not in used:
            pairs.append((b, b))
            used.add(b)
        else:
            unpaired_base.append(b)
    unpaired_head = [h for h in head_list if h not in used]
    return pairs, unpaired_base, unpaired_head


def read_file(path):
    with open(path, 'rb') as f:
        return f.read()


# ---------------------------------------------------------------------------------------------------
# --per-object

def run_per_object(base, head, map_path, literals_path, out):
    renames = dict(read_pairs(map_path))
    literals = Literals(read_pairs(literals_path))
    bl, hl = list_objects(base), list_objects(head)
    pairs, ub, uh = pair_objects(bl, hl, renames)
    moved_stems = set(posixpath.splitext(posixpath.basename(o))[0] for o, n in renames.items() if o != n)
    tot = dict(code=0, code_eq=0, data=0, data_eq=0, literals=0, free_diff=0, other=0, identical_files=0)
    status_count = collections.Counter()
    for b, h in pairs:
        db, dh = read_file(os.path.join(base, b)), read_file(os.path.join(head, h))
        renamed = b != h or ('/' not in b and object_source(b) in moved_stems)
        if db == dh and not renamed:
            tot['identical_files'] += 1
            status_count['EQUAL'] += 1
            continue
        if db[:4] == b'\x7fELF':
            other, notes, c, fd = compare_elf_pair(Elf(b, db), Elf(h, dh), literals, renamed)
        else:
            other, notes, c, fd = compare_coff_pair(coff_parse(b, db), coff_parse(h, dh), literals, renamed)
        for k in ('code', 'code_eq', 'data', 'data_eq', 'literals'):
            tot[k] += c[k]
        tot['free_diff'] += fd
        status = 'OTHER' if other else ('PATHS' if c['literals'] else ('EQUAL' if not fd else 'DEBUG-ONLY'))
        status_count[status] += 1
        name = b if not renamed else '%s -> %s' % (b, h)
        if status != 'EQUAL' or renamed:
            out('%-10s %s: code %d/%d equal, data %d/%d equal, free sections differing %d, path strings %d' % (
                status, name, c['code_eq'], c['code'], c['data_eq'], c['data'], fd, c['literals']))
        for n in notes:
            out('    ' + n)
        for o in other:
            out('    OTHER: ' + o)
        tot['other'] += len(other)
    for b in ub:
        out('OTHER      only in base: %s' % b)
    for h in uh:
        out('OTHER      only in head: %s' % h)
    tot['other'] += len(ub) + len(uh)
    out('TOTAL: %d pairs (%d byte-identical files), %s; sections compared: code %d (%d equal), data %d (%d equal); '
        'path strings %d; free sections differing %d; unpaired %d base, %d head; OTHER %d' % (
            len(pairs), tot['identical_files'], ', '.join('%s %d' % kv for kv in sorted(status_count.items())),
            tot['code'], tot['code_eq'], tot['data'], tot['data_eq'], tot['literals'], tot['free_diff'],
            len(ub), len(uh), tot['other']))
    return tot['other'] == 0


# ---------------------------------------------------------------------------------------------------
# --by-symbol

class Corpus(object):
    """Items of every distinct object file read, shared by the two builds."""

    def __init__(self):
        self.key = []
        self.d0 = []
        self.refs = []
        self.where = []
        self.strong = []
        self.code = []
        self.grefs = []
        self.objects = {}

    def add_object(self, path, data, rename):
        h = digest(data)
        cache_key = (h, tuple(sorted(rename.items())))
        if cache_key in self.objects:
            return self.objects[cache_key]
        e = Elf(path, data)
        info = self._items(e, rename)
        self.objects[cache_key] = info
        return info

    def _items(self, e, rename):
        rn = (lambda n: rename.get(n, n)) if rename else (lambda n: n)

        def rn_section(name):
            if not rename:
                return name
            i = name.find('.', 1)
            while i >= 0:
                head, dot, tail = name[i + 1:].partition('.')
                if head in rename:
                    return name[:i + 1] + rename[head] + dot + tail
                i = name.find('.', i + 1)
            return name
        secs, syms = e.secs, e.syms
        kind = {}
        for s in secs:
            if s.type in STRUCTURAL or elf_free(s) or not s.flags & SHF_ALLOC:
                kind[s.index] = 'skip'
            elif s.flags & SHF_MERGE and s.data is not None:
                kind[s.index] = 'merge'
            elif elf_init(s):
                kind[s.index] = 'init'
            elif s.name == '.eh_frame':
                kind[s.index] = 'eh'
            elif s.size == 0:
                kind[s.index] = 'empty'
            else:
                kind[s.index] = 'item'
        defined = collections.defaultdict(list)
        for y in syms:
            if 0 < y.shndx < SHN_LORESERVE and y.type not in (STT_SECTION, STT_FILE):
                defined[y.shndx].append(y)
        base_index = len(self.key)
        item_of = {}
        order = [s for s in secs if kind[s.index] == 'item']
        for n, s in enumerate(order):
            item_of[s.index] = base_index + n
        keys = {}
        for s in order:
            ds = defined.get(s.index, [])
            glob = sorted(rn(y.name) for y in ds if y.bind != STB_LOCAL)
            loc = sorted(rn(y.name) for y in ds if y.bind == STB_LOCAL)
            if glob:
                keys[s.index] = ('G',) + tuple(glob)
            elif loc:
                keys[s.index] = ('L',) + tuple(loc)
            else:
                keys[s.index] = ('S', rn_section(s.name))

        grefs = []

        def target(rsym, addend, referrer, rtype, refs):
            y = syms[rsym]
            if y.shndx == SHN_UNDEF or (y.bind != STB_LOCAL and y.type != STT_SECTION):
                if y.shndx != SHN_UNDEF:
                    grefs.append(rn(y.name))
                return ('G', rn(y.name), addend, y.shndx == SHN_UNDEF and y.bind == STB_WEAK)
            if y.shndx >= SHN_LORESERVE:
                return ('A', rn(y.name), y.shndx, y.value, addend)
            k = kind.get(y.shndx)
            t = secs[y.shndx]
            if k == 'merge':
                bias = ref_bias(e, rtype, referrer)
                return ('M', pool_ref(t, y.value + addend + bias), bias)
            if k == 'item':
                refs.append(item_of[y.shndx])
                if y.type == STT_SECTION:
                    return ('I', keys[y.shndx], addend)
                return ('L', rn(y.name), keys[y.shndx], addend)
            return ('X', k, rn_section(t.name), y.type == STT_SECTION and '' or rn(y.name), addend)

        fdes = collections.defaultdict(list)
        orphans = []
        for s in secs:
            if kind[s.index] != 'eh' or s.data is None:
                continue
            d = s.data
            rels = sorted(e.relocs.get(s.index, []))
            cies = {}
            off = 0
            ri = 0
            records = []
            while off + 4 <= len(d):
                length = struct.unpack_from('<I', d, off)[0]
                if length == 0:
                    break
                hdr = 4
                if length == 0xffffffff:
                    length = struct.unpack_from('<Q', d, off + 4)[0]
                    hdr = 12
                end = off + hdr + length
                cid = struct.unpack_from('<I', d, off + hdr)[0]
                rr = []
                while ri < len(rels) and rels[ri][0] < end:
                    rr.append(rels[ri])
                    ri += 1
                records.append((off, hdr, end, cid, rr))
                off = end
            for off, hdr, end, cid, rr in records:
                if cid == 0:
                    refs = []
                    norm = [(o - off, t, target(y, a, s, t, refs)) for o, t, y, a in rr]
                    cies[off] = (eh_body(d, off, hdr, end, rr), norm, refs)
            for off, hdr, end, cid, rr in records:
                if cid == 0:
                    continue
                cie = cies.get(off + hdr - cid, (b'', [], []))
                body = bytearray(eh_body(d, off, hdr, end, rr))
                body[0:4] = b'\0\0\0\0'
                pc = [r for r in rr if r[0] == off + hdr + 4]
                rest = [r for r in rr if r[0] != off + hdr + 4]
                refs = list(cie[2])
                norm = [(o - off, t, target(y, a, s, t, refs)) for o, t, y, a in rest]
                owner = start = None
                if len(pc) == 1:
                    y = syms[pc[0][2]]
                    if 0 < y.shndx < SHN_LORESERVE and kind.get(y.shndx) == 'item':
                        owner, start = y.shndx, y.value + pc[0][3]
                if owner is None:
                    where = [(t, target(y, a, s, t, refs)) for o, t, y, a in pc]
                    label = ', '.join(syms[y].name or secs[syms[y].shndx].name if syms[y].shndx < len(secs) else syms[y].name
                                      for o, t, y, a in pc) or 'no start address'
                    orphans.append(((where, cie[0], cie[1], bytes(body), norm), refs, label))
                    continue
                rec = (start, cie[0], cie[1], bytes(body), norm)
                fdes[owner].append((rec, refs))
        info = dict(path=e.path, items=[], init=[], has_init=False, strong=set(), orphans=orphans)
        for s in order:
            refs = []
            del grefs[:]
            rels = [(o, t, target(y, a, s, t, refs), ) for o, t, y, a in sorted(e.relocs.get(s.index, []))]
            ds = sorted((rn(y.name), y.value, y.size, y.type, y.bind, y.vis) for y in defined.get(s.index, []))
            group = rn(e.group_of.get(s.index, ''))
            records = sorted(fdes.get(s.index, []), key=lambda r: (r[0][0], digest(r[0])))
            fde = [r for r, _ in records]
            for _, r in records:
                refs.extend(r)
            header = (s.type, s.flags, s.align, s.entsize, s.size, rn_section(s.name), group, ds)
            d0 = digest(header, s.data if s.data is not None else b'', rels, fde)
            self.key.append(keys[s.index])
            self.d0.append(d0)
            self.refs.append(refs)
            self.where.append((e.path, s.index))
            is_strong = not (s.flags & SHF_GROUP) and any(y.bind == STB_GLOBAL for y in defined.get(s.index, []))
            self.strong.append(is_strong)
            self.code.append(bool(s.flags & SHF_EXECINSTR))
            self.grefs.append(tuple(grefs))
            info['items'].append(item_of[s.index])
            if is_strong:
                info['strong'].add(keys[s.index])
        for s in secs:
            if kind[s.index] == 'init':
                if s.size:
                    info['has_init'] = True
                refs = []
                rels = [(o, t, target(y, a, s, t, refs)) for o, t, y, a in sorted(e.relocs.get(s.index, []))]
                info['init'].append((s.name, s.size, rels, refs))
        tls = [i for i in info['items'] if any(tls_init_name(n) for n in self.key[i][1:])]
        if tls:
            info['has_init'] = True
            info['init'].append(('thread-local', len(tls), [], tls))
        return info

    def initialized(self, info):
        """The data an object's initializers reach: the variables its init functions (and the
        file-local functions they call) refer to."""
        by_name = {}
        for i in info['items']:
            for name in self.key[i][1:]:
                by_name.setdefault(name, i)
        seen, names = set(), set()
        todo = [t for _, _, _, refs in info['init'] for t in refs]
        depth = 0
        while todo and depth < 4:
            nxt = []
            for i in todo:
                if i in seen:
                    continue
                seen.add(i)
                if self.code[i]:
                    nxt.extend(self.refs[i])
                    nxt.extend(by_name[n] for n in self.grefs[i] if n in by_name)
                elif self.key[i][0] in ('G', 'L'):
                    names.add(self.key[i][1])
            todo = nxt
            depth += 1
        return sorted(names)

    def refine(self):
        cur = list(self.d0)
        classes = len(set(cur))
        rounds = 0
        with_refs = [i for i, r in enumerate(self.refs) if r]
        while with_refs:
            rounds += 1
            nxt = list(cur)
            for i in with_refs:
                nxt[i] = digest(cur[i], b''.join(cur[t] for t in self.refs[i]))
            n = len(set(nxt))
            cur = nxt
            if n == classes or rounds > 200:
                break
            classes = n
        self.final = cur
        return rounds


def parse_deps(path, root):
    """`ninja -t deps` output: object -> set of repo-relative files read."""
    out = {}
    cur = None
    root = posixpath.normpath(root.replace('\\', '/')) if root else ''
    for line in open(path, encoding='utf-8', errors='replace'):
        line = line.rstrip('\n')
        if not line.strip():
            continue
        if not line[0].isspace():
            cur = line.split(': #deps', 1)[0].strip()
            out[cur] = set()
            continue
        if cur is None:
            continue
        p = posixpath.normpath(line.strip().replace('\\', '/'))
        if root and p.startswith(root + '/'):
            p = p[len(root) + 1:]
        elif p.startswith('/'):
            continue
        out[cur].add(p)
    return out


def read_changes(path):
    out = []
    for line in open(path, encoding='utf-8'):
        parts = line.rstrip('\n').split('\t')
        if len(parts) < 2:
            continue
        st = parts[0][:1]
        if st in 'RC' and len(parts) >= 3:
            out.append((st, parts[1], parts[2]))
        else:
            out.append((st, parts[1], None))
    return out


def init_function(text):
    """Whether a difference line names a function GCC makes for a file's dynamic initializers."""
    name = text.split(' ', 1)[0]
    return (name.startswith('_GLOBAL__sub_I_') or name.startswith('_GLOBAL__sub_D_')
            or '__static_initialization_and_destruction_' in name or tls_init_name(name))


def short_key(k):
    if len(k) == 2:
        return k[1]
    return '%s (+%d names)' % (k[1], len(k) - 2)


def describe(corpus, i, j):
    """The first difference between two items of one key."""
    (pa, sa), (pb, sb) = corpus.where[i], corpus.where[j]
    ea, eb = Elf(pa, read_file(pa)), Elf(pb, read_file(pb))
    a, b = ea.secs[sa], eb.secs[sb]
    if (a.type, a.flags, a.align, a.size) != (b.type, b.flags, b.align, b.size):
        return 'section %s: type/flags/align/size %s -> %s' % (a.name, (a.type, hex(a.flags), a.align, a.size),
                                                               (b.type, hex(b.flags), b.align, b.size))
    if a.data != b.data:
        k = next(n for n in range(len(a.data)) if a.data[n] != b.data[n])
        return 'section %s: bytes differ from offset 0x%x' % (a.name, k)
    ra, rb = sorted(ea.relocs.get(sa, [])), sorted(eb.relocs.get(sb, []))
    if len(ra) != len(rb):
        return 'section %s: %d -> %d relocations' % (a.name, len(ra), len(rb))
    for x, y in zip(ra, rb):
        na, nb = ea.syms[x[2]], eb.syms[y[2]]
        ta = na.name or ea.secs[na.shndx].name if na.shndx < len(ea.secs) else na.name
        tb = nb.name or eb.secs[nb.shndx].name if nb.shndx < len(eb.secs) else nb.name
        if (x[0], x[1], x[3]) != (y[0], y[1], y[3]) or ta != tb:
            return 'section %s: relocation at 0x%x: %s%+d (type %d) -> %s%+d (type %d)' % (
                a.name, x[0], ta, x[3], x[1], tb, y[3], y[1])
    if corpus.d0[i] != corpus.d0[j]:
        return 'section %s: the strings, constants, symbols or unwind record it holds or reaches differ' % a.name
    return 'section %s: a local item it reaches differs' % a.name


def run_by_symbol(base, head, map_path, base_deps, head_deps, changes, root, list_init, out, base_lines=None,
                  head_lines=None):
    rename = {}
    for old, new in read_pairs(map_path):
        rename[new] = old
    corpus = Corpus()
    sides = {}
    for side, top in (('base', base), ('head', head)):
        objs = {}
        for rel in list_objects(top):
            p = os.path.join(top, rel)
            data = read_file(p)
            if data[:4] != b'\x7fELF':
                raise ValueError('%s: --by-symbol reads ELF objects only' % p)
            info = corpus.add_object(p, data, rename if side == 'head' else {})
            objs[rel] = info
        sides[side] = objs
    rounds = corpus.refine()
    final = corpus.final
    sets = {}
    for side in ('base', 'head'):
        m = collections.defaultdict(dict)
        for rel, info in sides[side].items():
            for i in info['items']:
                m[corpus.key[i]].setdefault(final[i], (rel, i))
        sets[side] = m
    B, H = sets['base'], sets['head']
    diffs = []
    only_b = sorted(k for k in B if k not in H)
    only_h = sorted(k for k in H if k not in B)
    changed = sorted(k for k in B if k in H and set(B[k]) != set(H[k]))
    for k in only_b:
        rel = next(iter(B[k].values()))[0]
        diffs.append('only in base: %s (%s)' % (short_key(k), rel))
    for k in only_h:
        rel = next(iter(H[k].values()))[0]
        diffs.append('only in head: %s (%s)' % (short_key(k), rel))
    for k in changed:
        xb = [v for d, v in B[k].items() if d not in H[k]]
        xh = [v for d, v in H[k].items() if d not in B[k]]
        ib = (xb or list(B[k].values()))[0][1]
        ih = (xh or list(H[k].values()))[0][1]
        detail = describe(corpus, ib, ih)
        if not xb:
            detail = 'the head holds a further version: ' + detail
        elif not xh:
            detail = 'the base holds a further version: ' + detail
        diffs.append('changed: %s (%s -> %s): %s' % (short_key(k), (xb or list(B[k].values()))[0][0],
                                                    (xh or list(H[k].values()))[0][0], detail))
    init_fns = [d for d in diffs if init_function(d.split(': ', 1)[1])]
    diffs = [d for d in diffs if d not in init_fns]
    orphan_sets = {}
    for side, objs in sides.items():
        c = collections.Counter()
        where = {}
        for rel, info in objs.items():
            for rec, refs, label in info['orphans']:
                d = digest(rec, b''.join(final[t] for t in refs))
                c[d] += 1
                where.setdefault(d, (rel, label))
        orphan_sets[side] = (c, where)
    (cb, wb), (ch, wh) = orphan_sets['base'], orphan_sets['head']
    for d in sorted(set(cb) | set(ch)):
        if cb[d] != ch[d]:
            rel, label = (wb if cb[d] > ch[d] else wh)[d]
            diffs.append('unwind record without a function, %d in base and %d in head: for %s (%s)' % (
                cb[d], ch[d], label, rel))
    base_objs, head_objs = sides['base'], sides['head']
    moved_objs = sorted(set(base_objs) ^ set(head_objs))

    def init_sig(info):
        return [(n, sz, rels, [final[t] for t in refs]) for n, sz, rels, refs in info['init']]
    changed_objs = []
    for rel in sorted(set(base_objs) & set(head_objs)):
        a, b = base_objs[rel], head_objs[rel]
        if a['strong'] != b['strong'] or init_sig(a) != init_sig(b):
            changed_objs.append(rel)
    hard = []
    for rel in moved_objs + changed_objs:
        flags = [side for side, objs in (('base', base_objs), ('head', head_objs)) if rel in objs and objs[rel]['has_init']]
        if flags:
            info = head_objs.get(rel) or base_objs[rel]
            hard.append('INIT: %s %s and has a dynamic initializer in %s (it reaches %s)' % (
                rel, 'is in one build only' if rel in moved_objs else 'changed', ' and '.join(flags),
                ', '.join(corpus.initialized(info)[:6]) or 'no variable found'))
    strong_where = {}
    for side, objs in (('base', base_objs), ('head', head_objs)):
        for rel, info in objs.items():
            for k in info['strong']:
                strong_where.setdefault(k, {}).setdefault(side, set()).add(rel)
    moved_fns = []
    for k, w in sorted(strong_where.items()):
        if 'base' in w and 'head' in w and w['base'] != w['head']:
            moved_fns.append((k, sorted(w['base']), sorted(w['head'])))
    local_where = {}
    for side, objs in (('base', base_objs), ('head', head_objs)):
        for rel, info in objs.items():
            for i in info['items']:
                if corpus.key[i][0] == 'L':
                    local_where.setdefault(corpus.key[i], {}).setdefault(side, set()).add(rel)
    moved_locals = []
    for k, w in sorted(local_where.items()):
        if len(w.get('base', ())) == 1 and len(w.get('head', ())) == 1 and w['base'] != w['head']:
            moved_locals.append((k, sorted(w['base']), sorted(w['head'])))
    coverage, not_code, gate = [], [], []
    by_line = base_lines is not None or head_lines is not None
    line_rows = {'base': read_line_verdicts(base_lines), 'head': read_line_verdicts(head_lines)}
    if changes:
        reads = {'base': set(), 'head': set()}
        for side, path in (('base', base_deps), ('head', head_deps)):
            if path:
                for files in parse_deps(path, root).values():
                    reads[side] |= files
        for st, p, q in read_changes(changes):
            if st == 'R':
                entries = [(p, ('base',)), (q, ('head',))]
            elif st == 'C':
                entries = [(q, ('head',))]
            elif st == 'A':
                entries = [(p, ('head',))]
            elif st == 'D':
                entries = [(p, ('base',))]
            else:
                entries = [(p, ('base', 'head'))]
            for f, where in entries:
                if f in GATE_FILES:
                    gate.append(f)
                if not code_file(f):
                    not_code.append(f)
                elif by_line and st not in 'RC':
                    continue
                elif not any(f in reads[side] for side in where):
                    coverage.append('not compiled by the proof build: %s (%s, %s)' % (f, st, ' or '.join(where)))
        if by_line:
            coverage += line_problems(line_rows['base'], line_rows['head'])
    gate_problems = ['the change edits the object proof itself (%s), so the proof cannot show it mechanical' % f
                     for f in sorted(set(gate))]
    n_items = len(corpus.key)
    nb = sum(len(v) for v in B.values())
    nh = sum(len(v) for v in H.values())
    out('objects: base %d, head %d (%d distinct files read); items read %d; refinement rounds %d' % (
        len(base_objs), len(head_objs), len(corpus.objects), n_items, rounds))
    out('distinct contents: base %d under %d keys, head %d under %d keys' % (nb, len(B), nh, len(H)))
    out('unwind records without a function: base %d, head %d' % (sum(cb.values()), sum(ch.values())))
    out('dynamic initializers: base %d objects, head %d objects' % (
        sum(1 for i in base_objs.values() if i['has_init']), sum(1 for i in head_objs.values() if i['has_init'])))
    if list_init:
        for side, objs in (('base', base_objs), ('head', head_objs)):
            for rel, info in sorted(objs.items()):
                if info['has_init']:
                    out('    initializer (%s): %s: %s' % (side, rel, ', '.join(corpus.initialized(info)[:6])))
    out('objects in one build only: %d; objects whose global definitions or initializers changed: %d' % (
        len(moved_objs), len(changed_objs)))
    for rel in moved_objs[:60]:
        out('    %s only: %s' % ('base' if rel in base_objs else 'head', rel))
    if len(moved_objs) > 60:
        out('    ... %d more' % (len(moved_objs) - 60))
    for rel in changed_objs[:60]:
        out('    changed: %s' % rel)
    if len(changed_objs) > 60:
        out('    ... %d more' % (len(changed_objs) - 60))
    out('global definitions that moved between objects: %d' % len(moved_fns))
    for k, wb, wh in moved_fns[:60]:
        out('    %s: %s -> %s' % (short_key(k), ', '.join(wb), ', '.join(wh)))
    if len(moved_fns) > 60:
        out('    ... %d more' % (len(moved_fns) - 60))
    out('file-local definitions (statics) that moved between objects: %d' % len(moved_locals))
    for k, wb, wh in moved_locals[:60]:
        out('    %s: %s -> %s' % (short_key(k), ', '.join(wb), ', '.join(wh)))
    if len(moved_locals) > 60:
        out('    ... %d more' % (len(moved_locals) - 60))
    if changes:
        out('changed files: %d not code' % len(set(not_code)))
        for f in sorted(set(not_code))[:40]:
            out('    not code: %s' % f)
    if by_line:
        for side in ('base', 'head'):
            c = collections.Counter(v for _, _, v, _ in line_rows[side])
            out('changed lines (%s): %d compiled, %d blank, comment or #if 0, %d conditional directives, '
                '%d not compiled' % (side, c['ok'], c['skip'], c['cond'], c['notc']))
    hard += ['INIT: initializer function ' + d for d in init_fns]
    problems = hard + diffs + coverage + gate_problems
    for p in problems[:300]:
        out('DIFF ' + p)
    if len(problems) > 300:
        out('DIFF ... %d more' % (len(problems) - 300))
    out('differences: %d (dynamic initializer %d, symbols %d, not compiled by the proof build %d, gate files %d)' % (
        len(problems), len(hard), len(diffs), len(coverage), len(gate_problems)))
    return not problems


# ---------------------------------------------------------------------------------------------------
# Changed lines, and which of them the proof build compiled

CONDITIONALS = {'if', 'ifdef', 'ifndef', 'elif', 'elifdef', 'elifndef', 'else', 'endif'}
LINES_PER_FILE_TUS = 24
MARKER = re.compile(r'^# (\d+) "((?:[^"\\]|\\.)*)"')
FUNCTION_MACRO = re.compile(r'^#define\s+([A-Za-z_]\w*)\(([^)]*)\)\s?(.*)$')
TOKEN = re.compile(r'''[A-Za-z_]\w*|"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*'|\d[\w.]*|\S''')
IDENT = re.compile(r'[A-Za-z_]\w*')


def code_lines(text):
    """Per physical line, its code with the comments taken out and runs of whitespace made one
    space; a line a raw string literal touches keeps its exact text."""
    src = SourceLines(text)
    return [src.code[k] if k in src.raw else ' '.join(src.code[k].split()) for k in range(1, src.n + 1)]


def code_diff_rows(old_path, old_text, new_path, new_text):
    """The lines whose code differs between two versions of a file: (side, path, line), base for
    the old version and head for the new. A comment or whitespace edit is no row; uncommenting
    code is."""
    import difflib
    a = code_lines(old_text) if old_text is not None else []
    b = code_lines(new_text) if new_text is not None else []
    rows = []
    for tag, i1, i2, j1, j2 in difflib.SequenceMatcher(None, a, b, autojunk=False).get_opcodes():
        if tag != 'equal':
            rows.extend(('base', old_path, i + 1) for i in range(i1, i2) if a[i])
            rows.extend(('head', new_path, j + 1) for j in range(j1, j2) if b[j])
    return rows


def git_text(root, rev, path):
    r = subprocess.run(['git', '-C', root, 'show', '%s:%s' % (rev, path)], stdout=subprocess.PIPE,
                       stderr=subprocess.DEVNULL)
    return r.stdout.decode('utf-8', 'surrogateescape') if r.returncode == 0 else None


def run_diff_lines(root, base, head, out_path, log=print):
    names = subprocess.run(['git', '-C', root, '-c', 'core.quotepath=off', 'diff', '--name-status', '-M', base, head],
                           check=True, stdout=subprocess.PIPE).stdout.decode('utf-8', 'surrogateescape')
    rows = []
    for line in names.splitlines():
        parts = line.split('\t')
        if len(parts) < 2:
            continue
        st = parts[0][:1]
        old = parts[1] if st != 'A' else None
        new = parts[-1] if st != 'D' else None
        if st == 'C':
            old = None
        if not any(x and code_file(x) for x in (old, new)):
            continue
        rows.extend(code_diff_rows(old, git_text(root, base, old) if old else None,
                                   new, git_text(root, head, new) if new else None))
    with open(out_path, 'w', encoding='utf-8') as f:
        for side, path, line in rows:
            f.write('%s\t%s\t%d\n' % (side, path, line))
    log('changed code lines: %d in base, %d in head' % (sum(1 for r in rows if r[0] == 'base'),
                                                         sum(1 for r in rows if r[0] == 'head')))


def code_file(path):
    p = path.lower()
    if p.endswith('.in'):
        p = p[:-3]
    return p.endswith(CODE_EXTENSIONS)


class SourceLines(object):
    """One C, C++ or assembler source, line by line: its code with the comments taken out, the first
    line of each logical line (backslash continuations), its directives, the lines an `#if 0` group
    holds, the include guard, and the lines each function-like macro call spans."""

    def __init__(self, text):
        phys = text.replace('\r\n', '\n').replace('\r', '\n').split('\n')
        n = len(phys)
        self.n = n
        code = [[] for _ in range(n + 2)]
        i, line, size = 0, 1, len(text)
        t = text.replace('\r\n', '\n').replace('\r', '\n')
        size = len(t)
        state = None
        raw = set()
        while i < size:
            c = t[i]
            if c == '\n':
                line += 1
                i += 1
                if state == 'line':
                    state = None
                continue
            if c == '\\' and i + 1 < size and t[i + 1] == '\n':
                if state != 'block':
                    code[line].append('\\')
                line += 1
                i += 2
                continue
            if state == 'line':
                i += 1
                continue
            if state == 'block':
                if t.startswith('*/', i):
                    state = None
                    i += 2
                else:
                    i += 1
                continue
            if t.startswith('//', i):
                state = 'line'
                i += 2
                continue
            if t.startswith('/*', i):
                state = 'block'
                code[line].append(' ')
                i += 2
                continue
            if c == 'R' and i + 1 < size and t[i + 1] == '"' and (i == 0 or not (t[i - 1].isalnum() or t[i - 1] == '_')
                                                                  or t[i - 1] in 'uU8L'):
                open_paren = t.find('(', i + 2)
                if open_paren > 0:
                    delim = ')' + t[i + 2:open_paren] + '"'
                    end = t.find(delim, open_paren)
                    end = size if end < 0 else end + len(delim)
                    raw.add(line)
                    for ch in t[i:end]:
                        if ch == '\n':
                            line += 1
                            raw.add(line)
                        else:
                            code[line].append(ch)
                    i = end
                    continue
            if c in '"\'':
                j = i + 1
                while j < size and t[j] != c and t[j] != '\n':
                    j += 2 if t[j] == '\\' and j + 1 < size and t[j + 1] != '\n' else 1
                code[line].append(t[i:j + 1])
                i = j + 1
                continue
            code[line].append(c)
            i += 1
        self.code = [''.join(parts) for parts in code]
        self.raw = raw
        self.first = [0] * (n + 2)
        for k in range(1, n + 1):
            self.first[k] = self.first[k - 1] if k > 1 and phys[k - 2].endswith('\\') else k
        self.has_code = [bool(self.code[k].replace('\\', '').strip()) for k in range(n + 2)]
        self.directive = {}
        logical = {}
        for k in range(1, n + 1):
            logical.setdefault(self.first[k], []).append(k)
        for start, members in sorted(logical.items()):
            joined = ' '.join(self.code[k].replace('\\', ' ') for k in members).strip()
            if joined.startswith('#'):
                rest = joined[1:].strip()
                m = IDENT.match(rest)
                name = m.group(0) if m else ''
                text_norm = '#' + ' '.join(rest.split())
                for k in members:
                    self.directive[k] = (name, text_norm, start)
        self.dead = [False] * (n + 2)
        stack = []
        order = []
        for start in sorted(logical):
            d = self.directive.get(start)
            outer = stack[-1][1] if stack else False
            if d and d[0] in ('if', 'ifdef', 'ifndef'):
                cond = d[1].split(' ', 1)[1].strip() if ' ' in d[1] else ''
                stack.append([outer, outer or (d[0] == 'if' and cond == '0')])
                state_line = outer
            elif d and d[0] in ('elif', 'elifdef', 'elifndef', 'else'):
                if stack:
                    cond = d[1].split(' ', 1)[1].strip() if ' ' in d[1] else ''
                    stack[-1][1] = stack[-1][0] or (d[0] == 'elif' and cond == '0')
                    state_line = stack[-1][0]
                else:
                    state_line = False
            elif d and d[0] == 'endif':
                state_line = stack.pop()[0] if stack else False
            else:
                state_line = outer
            for k in logical[start]:
                self.dead[k] = state_line
            if d:
                order.append((start, d))
        self.guard = set()
        order = [(k, d) for k, d in order if d[1] != '#pragma once']
        if len(order) >= 3:
            (l1, d1), (l2, d2), (l3, d3) = order[0], order[1], order[-1]
            g = None
            if d1[0] == 'ifndef':
                g = d1[1].split(' ', 1)[1].strip() if ' ' in d1[1] else None
            elif d1[0] == 'if':
                m = re.match(r'#if !\s*defined\s*\(?\s*([A-Za-z_]\w*)\s*\)?$', d1[1])
                g = m.group(1) if m else None
            if g and d2[0] == 'define' and d2[1] == '#define ' + g and d3[0] == 'endif':
                self.guard = set(logical[l1]) | set(logical[l2]) | set(logical[l3])
        self.spans = {}

    def macro_spans(self, fmacros):
        """For each line, the function-like macro calls it is part of: (macro name, line of the name)."""
        spans = collections.defaultdict(list)
        toks = []
        for k in range(1, self.n + 1):
            if k in self.directive:
                continue
            for m in TOKEN.finditer(self.code[k]):
                toks.append((m.group(0), k))
        i = 0
        while i < len(toks):
            tok, k = toks[i]
            if tok in fmacros and i + 1 < len(toks) and toks[i + 1][0] == '(':
                depth, j = 0, i + 1
                while j < len(toks):
                    if toks[j][0] == '(':
                        depth += 1
                    elif toks[j][0] == ')':
                        depth -= 1
                        if depth == 0:
                            break
                    j += 1
                end = toks[min(j, len(toks) - 1)][1]
                for line in range(k, end + 1):
                    spans[line].append((tok, k))
            i += 1
        return spans


def dropping(params, body):
    """Whether a function-like macro leaves one of its parameters out of its replacement."""
    names = []
    for p in params.split(','):
        p = p.strip()
        if not p:
            continue
        if p == '...':
            names.append('__VA_ARGS__')
        elif p.endswith('...'):
            names.append(p[:-3].strip())
        else:
            names.append(p)
    used = set(IDENT.findall(body))
    return any(name not in used for name in names)


def read_preprocessed(path, root, build):
    """What one preprocessed TU (`-E -dD -dI`) emitted: file -> lines that hold output, and the
    function-like macros it defined (name -> whether a definition drops a parameter)."""
    emitted = collections.defaultdict(set)
    macros = {}
    cur, line = None, 0
    with open(path, encoding='utf-8', errors='replace') as f:
        for raw in f:
            raw = raw.rstrip('\n')
            m = MARKER.match(raw)
            if m:
                cur = norm_path(m.group(2).encode('latin-1', 'replace').decode('unicode_escape'), root, build)
                line = int(m.group(1))
                continue
            if raw.strip():
                if cur is not None:
                    emitted[cur].add(line)
                if raw.startswith('#define'):
                    fm = FUNCTION_MACRO.match(raw)
                    if fm:
                        macros[fm.group(1)] = macros.get(fm.group(1), False) or dropping(fm.group(2), fm.group(3))
            line += 1
    return emitted, macros


def norm_path(p, root, build):
    p = posixpath.normpath(p.replace('\\', '/'))
    if root and p.startswith(root + '/'):
        return p[len(root) + 1:]
    if build and p.startswith(build + '/'):
        return '@build/' + p[len(build) + 1:]
    return p


def preprocess_command(entry, out_path):
    args = list(entry['arguments']) if 'arguments' in entry else shlex.split(entry['command'])
    keep, skip = [], False
    for k, a in enumerate(args):
        if skip:
            skip = False
            continue
        if a in ('-o', '-MF', '-MT', '-MQ'):
            skip = True
            continue
        if a in ('-c', '-MD', '-MMD') or a.startswith('-MF') or a.startswith('-MT') or a.startswith('-o'):
            continue
        keep.append(a)
    return keep + ['-E', '-dD', '-dI', '-o', out_path]


def run_lines_compiled(side, changed_path, deps_path, db_path, root, build, out_path, log=print):
    """Preprocesses TUs of one build that read the changed files of SIDE, and writes for every
    changed line whether that build compiled it: `path<TAB>line<TAB>verdict<TAB>detail`, verdict
    ok, skip (blank, comment or #if 0), cond (a conditional directive; detail is its text) or
    notc (detail says why)."""
    root = posixpath.normpath(root.replace('\\', '/'))
    build = posixpath.normpath(build.replace('\\', '/'))
    wanted = collections.defaultdict(set)
    for row in open(changed_path, encoding='utf-8'):
        s, p, n = row.rstrip('\n').split('\t')
        if s == side and code_file(p):
            wanted[p].add(int(n))
    deps = {}
    if deps_path and os.path.exists(deps_path):
        cur = None
        for line in open(deps_path, encoding='utf-8', errors='replace'):
            line = line.rstrip('\n')
            if not line.strip():
                continue
            if not line[0].isspace():
                cur = line.split(': #deps', 1)[0].strip()
                deps[cur] = set()
            elif cur is not None:
                deps[cur].add(norm_path(line.strip(), root, build))
    db = {}
    if db_path and os.path.exists(db_path):
        for e in json.load(open(db_path, encoding='utf-8')):
            o = e.get('output')
            if o:
                db[posixpath.normpath(o.replace('\\', '/'))] = e
    readers = collections.defaultdict(list)
    for obj, files in sorted(deps.items()):
        for f in files:
            readers[f].append(obj)

    def target_of(p):
        if p.endswith('.in'):
            base = posixpath.basename(p[:-3])
            gen = sorted(f for f in readers if f.startswith('@build/') and posixpath.basename(f) == base)
            return gen[0] if gen else p
        return p
    sources = {}
    verdicts = {}
    need = collections.defaultdict(set)
    for p, lines in wanted.items():
        full = os.path.join(root, p)
        try:
            src = SourceLines(open(full, encoding='utf-8', errors='replace').read())
        except OSError:
            for n in lines:
                verdicts[(p, n)] = ('notc', 'the file is not in the tree of this build')
            continue
        sources[p] = src
        t = target_of(p)
        for n in sorted(lines):
            if n > src.n or not src.has_code[n] or src.dead[n]:
                verdicts[(p, n)] = ('skip', '')
                continue
            d = src.directive.get(n)
            if d and (n in src.guard or d[1] == '#pragma once'):
                verdicts[(p, n)] = ('ok', '') if readers.get(t) else ('notc', 'no object reads the file')
            elif d and d[0] in CONDITIONALS:
                verdicts[(p, n)] = ('cond', d[1])
            elif d and d[0] == '':
                verdicts[(p, n)] = ('skip', '')
            else:
                need[p].add(n)
    emitted = collections.defaultdict(set)
    macros = {}
    tried = collections.Counter()
    used = set()
    tmp = tempfile.mkdtemp(prefix='objcmp-lines-')

    def covered(p, n, spans):
        src = sources[p]
        t = target_of(p)
        first = src.first[n]
        calls = spans.get(n, [])
        bad = sorted(set(name for name, _ in calls if macros.get(name)))
        if bad:
            return ('notc', 'it is an argument of %s, which the proof build defines without using it' % bad[0])
        if first in emitted[t] or any(start in emitted[t] for _, start in calls):
            return ('ok', '')
        return None

    try:
        workers = max(1, min(8, os.cpu_count() or 2))
        while True:
            for p in list(need):
                spans = sources[p].macro_spans(macros) if macros else {}
                for n in list(need[p]):
                    v = covered(p, n, spans)
                    if v:
                        verdicts[(p, n)] = v
                        need[p].discard(n)
                if not need[p]:
                    del need[p]
            batch = []
            for p in sorted(need):
                t = target_of(p)
                cands = [o for o in readers.get(t, []) if o in db and o not in used and o not in batch]
                own = [o for o in cands if posixpath.normpath(db[o]['file'].replace('\\', '/')).endswith('/' + p)]
                if tried[p] >= LINES_PER_FILE_TUS or not cands:
                    continue
                groups = collections.OrderedDict()
                for o in cands:
                    groups.setdefault(o.split('.dir/')[0], []).append(o)
                pick = own[0] if own else list(groups.values())[tried[p] % len(groups)][0]
                batch.append(pick)
                if len(batch) >= workers:
                    break
            if not batch:
                break
            procs = []
            for k, o in enumerate(batch):
                used.add(o)
                e = db[o]
                ii = os.path.join(tmp, '%d.ii' % k)
                procs.append((o, ii, subprocess.Popen(preprocess_command(e, ii), cwd=e['directory'],
                                                      stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)))
            for o, ii, proc in procs:
                rc = proc.wait()
                if rc == 0 and os.path.exists(ii):
                    em, ma = read_preprocessed(ii, root, build)
                    for f, ls in em.items():
                        emitted[f] |= ls
                    for name, drops in ma.items():
                        macros[name] = macros.get(name, False) or drops
                if os.path.exists(ii):
                    os.remove(ii)
                for p in list(need):
                    if target_of(p) in deps.get(o, ()):
                        tried[p] += 1
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    for p, lines in need.items():
        t = target_of(p)
        total = len(readers.get(t, []))
        why = ('no object of this build reads the file' if not total else
               'no TU emitted it (%d of the %d TUs that read the file preprocessed)' % (tried[p], total))
        for n in lines:
            verdicts[(p, n)] = ('notc', why)
    with open(out_path, 'w', encoding='utf-8') as f:
        for (p, n), (v, detail) in sorted(verdicts.items()):
            f.write('%s\t%d\t%s\t%s\n' % (p, n, v, detail))
    counts = collections.Counter(v for v, _ in verdicts.values())
    log('%s: changed lines %d: compiled %d, blank or comment %d, conditional directives %d, not compiled %d; '
          '%d TUs preprocessed' % (side, len(verdicts), counts['ok'], counts['skip'], counts['cond'], counts['notc'],
                                   len(used)))


def read_line_verdicts(path):
    rows = []
    if path and os.path.exists(path):
        for line in open(path, encoding='utf-8'):
            p, n, v, detail = line.rstrip('\n').split('\t', 3)
            rows.append((p, int(n), v, detail))
    return rows


def line_problems(base_rows, head_rows):
    """The changed lines the proof build did not compile, and the changed conditional directives
    that are not the same text moved."""
    problems = []
    for side, rows in (('base', base_rows), ('head', head_rows)):
        runs = collections.OrderedDict()
        for p, n, v, detail in rows:
            if v == 'notc':
                runs.setdefault((p, detail), []).append(n)
        for (p, detail), ns in runs.items():
            ns.sort()
            ranges, start, prev = [], ns[0], ns[0]
            for n in ns[1:] + [None]:
                if n is not None and n == prev + 1:
                    prev = n
                    continue
                ranges.append('%d' % start if start == prev else '%d-%d' % (start, prev))
                if n is not None:
                    start = prev = n
            problems.append('not compiled by the proof build (%s): %s:%s: %s' % (side, p, ','.join(ranges[:12])
                                                                                + (',...' if len(ranges) > 12 else ''),
                                                                                detail))
    cb = collections.Counter(d for _, _, v, d in base_rows if v == 'cond')
    ch = collections.Counter(d for _, _, v, d in head_rows if v == 'cond')
    for side, rows, mine, other in (('base', base_rows, cb, ch), ('head', head_rows, ch, cb)):
        extra = mine - other
        for p, n, v, d in rows:
            if v == 'cond' and extra[d] > 0:
                extra[d] -= 1
                problems.append('not compiled by the proof build (%s): %s:%d: a conditional directive that '
                                'changed, not moved: %s' % (side, p, n, d))
    return problems


# ---------------------------------------------------------------------------------------------------
# --self-test

def write_elf(sections, symbols, machine=EM_X86_64):
    """A relocatable ELF64 object. sections: dicts name, data, flags, type, align, entsize, relocs
    [(offset, type, 'sym:NAME' | 'sec:NAME', addend)], group. symbols: dicts name, bind, type,
    section (None: undefined), value, size."""
    shstr, strtab = bytearray(b'\0'), bytearray(b'\0')

    def add(tab, s):
        off = len(tab)
        tab.extend(s.encode('latin-1') + b'\0')
        return off
    user = list(sections)
    sec_index = {s['name']: 1 + i for i, s in enumerate(user)}
    groups = sorted(set(s['group'] for s in user if s.get('group')))
    nsec_user = len(user)
    group_index = {g: 1 + nsec_user + i for i, g in enumerate(groups)}
    rela_list = [s for s in user if s.get('relocs')]
    rela_index = {s['name']: 1 + nsec_user + len(groups) + i for i, s in enumerate(rela_list)}
    symtab_index = 1 + nsec_user + len(groups) + len(rela_list)
    syms = [(0, 0, 0, 0, 0, 0)]
    sym_index = {}
    for s in user:
        sym_index['sec:' + s['name']] = len(syms)
        syms.append((0, (STB_LOCAL << 4) | STT_SECTION, 0, sec_index[s['name']], 0, 0))
    locals_ = [y for y in symbols if y.get('bind', STB_GLOBAL) == STB_LOCAL]
    globals_ = [y for y in symbols if y.get('bind', STB_GLOBAL) != STB_LOCAL]
    for y in locals_ + globals_:
        sym_index['sym:' + y['name']] = len(syms)
        shndx = sec_index[y['section']] if y.get('section') else 0
        syms.append((add(strtab, y['name']), (y.get('bind', STB_GLOBAL) << 4) | y.get('type', STT_FUNC), 0, shndx,
                     y.get('value', 0), y.get('size', 0)))
    first_global = 1 + len(user) + len(locals_)
    blobs = []
    headers = [(0, 0, 0, 0, 0, 0, 0, 0, 0)]
    for s in user:
        flags = s.get('flags', SHF_ALLOC | SHF_EXECINSTR) | (SHF_GROUP if s.get('group') else 0)
        blobs.append(s.get('data', b''))
        headers.append((add(shstr, s['name']), s.get('type', SHT_PROGBITS), flags, len(blobs) - 1, len(s.get('data', b'')),
                        0, 0, s.get('align', 1), s.get('entsize', 0)))
    for g in groups:
        members = [sec_index[s['name']] for s in user if s.get('group') == g]
        blobs.append(struct.pack('<%dI' % (1 + len(members)), 1, *members))
        headers.append((add(shstr, '.group'), SHT_GROUP, 0, len(blobs) - 1, len(blobs[-1]), symtab_index,
                        sym_index['sym:' + g], 4, 4))
    for s in rela_list:
        data = b''.join(struct.pack('<QQq', o, (sym_index[t] << 32) | ty, a) for o, ty, t, a in s['relocs'])
        blobs.append(data)
        headers.append((add(shstr, '.rela' + s['name']), SHT_RELA, 0x40, len(blobs) - 1, len(data), symtab_index,
                        sec_index[s['name']], 8, 24))
    blobs.append(b''.join(struct.pack('<IBBHQQ', *y) for y in syms))
    headers.append((add(shstr, '.symtab'), SHT_SYMTAB, 0, len(blobs) - 1, len(blobs[-1]), symtab_index + 1,
                    first_global, 8, 24))
    blobs.append(bytes(strtab))
    headers.append((add(shstr, '.strtab'), SHT_STRTAB, 0, len(blobs) - 1, len(strtab), 0, 0, 1, 0))
    name_off = add(shstr, '.shstrtab')
    blobs.append(bytes(shstr))
    headers.append((name_off, SHT_STRTAB, 0, len(blobs) - 1, len(shstr), 0, 0, 1, 0))
    out = bytearray(64)
    offsets = []
    for b in blobs:
        while len(out) % 8:
            out.append(0)
        offsets.append(len(out))
        out.extend(b)
    while len(out) % 8:
        out.append(0)
    shoff = len(out)
    for n, (nm, ty, fl, bi, sz, ln, inf, al, es) in enumerate(headers):
        off = offsets[bi] if n else 0
        out.extend(struct.pack('<IIQQQQIIQQ', nm, ty, fl, 0, off, sz, ln, inf, al, es))
    ident = b'\x7fELF' + bytes([2, 1, 1, 0]) + b'\0' * 8
    out[0:64] = ident + struct.pack('<HHIQQQIHHHHHH', 1, machine, 1, 0, 0, shoff, 0, 64, 0, 0, 64, len(headers),
                                    len(headers) - 1)
    return bytes(out)


def write_coff(sections, symbols):
    """A small COFF object. sections: dicts name, data, flags, relocs [(offset, symbol name, type)].
    symbols: dicts name, section (1-based index or 0), value, cls."""
    nsec = len(sections)
    strtab = bytearray(4)

    def name8(n):
        b = n.encode('latin-1')
        if len(b) <= 8:
            return b.ljust(8, b'\0')
        off = len(strtab)
        strtab.extend(b + b'\0')
        return b'\0\0\0\0' + struct.pack('<I', off)
    sym_names = [y['name'] for y in symbols]
    raw = bytearray()
    sec_hdrs = []
    data_off = 20 + 40 * nsec
    for s in sections:
        d = s.get('data', b'')
        ptr = data_off + len(raw)
        raw.extend(d)
        relptr = data_off + len(raw) if s.get('relocs') else 0
        for o, sym, t in s.get('relocs', []):
            raw.extend(struct.pack('<IIH', o, sym_names.index(sym), t))
        sname = s['name'].encode('latin-1')
        sec_hdrs.append(struct.pack('<8sIIIIIIHHI', sname.ljust(8, b'\0') if len(sname) <= 8 else b'/0', 0, 0,
                                    len(d), ptr, relptr, 0, len(s.get('relocs', [])), 0, s.get('flags', 0x60500020)))
    symptr = data_off + len(raw)
    symt = bytearray()
    for y in symbols:
        symt.extend(name8(y['name']) + struct.pack('<IhHBB', y.get('value', 0), y.get('section', 0), 0x20,
                                                   y.get('cls', 2), 0))
    struct.pack_into('<I', strtab, 0, len(strtab))
    head = struct.pack('<HHIIIHH', 0x8664, nsec, 12345, symptr, len(symbols), 0, 0)
    return head + b''.join(sec_hdrs) + bytes(raw) + bytes(symt) + bytes(strtab)


def eh_frame(fdes):
    """An .eh_frame section: one CIE, then an FDE per (target, offset, length, instructions)."""
    cie = b'\0\0\0\0\x01zR\0\x01\x78\x10\x01\x1b\x0c\x07\x08\x90\x01'
    cie += b'\0' * (-(len(cie) + 4) % 8)
    data = bytearray(struct.pack('<I', len(cie)) + cie)
    relocs = []
    for target, offset, length, insns in fdes:
        at = len(data)
        body = struct.pack('<IiI', at + 4, 0, length) + b'\0' + insns
        body += b'\0' * (-(len(body) + 4) % 8)
        data += struct.pack('<I', len(body)) + body
        relocs.append((at + 8, 2, target, offset))
    return dict(name='.eh_frame', data=bytes(data), flags=SHF_ALLOC, align=8, relocs=relocs)


def synth(variant='base', init_in=None, rename_a=False, eh=None, eh_in='a'):
    """The objects of one self-test variant, by path. a.o defines f, g, m, a table and a static h;
    b.o defines k and its own static h; c.o defines n. INIT_IN adds a dynamic initializer to that
    object; RENAME_A moves a.o to another path. EH adds unwind records: f's own, and a second one
    inside f as a thunk has ('thunk'), or one whose start is no function of its object ('orphan',
    put in EH_IN); a name ending in 'changed' gives that record other instructions."""
    F = b'\x55\x48\x89\xe5\xe8\0\0\0\0\xe8\0\0\0\0\x48\x8d\x05\0\0\0\0\xe8\0\0\0\0\x5d\xc3'
    G = b'\x55\x48\x8d\x05\0\0\0\0\x5d\xc3'
    M = b'\xe8\0\0\0\0\xc3'
    K = b'\xe8\0\0\0\0\x48\x8d\x05\0\0\0\0\xc3'
    v = variant
    gname = 'g2' if v == 'rename' else 'g'
    cos = 'cosf' if v == 'cosf' else 'cos'
    hello = b'hellp' if v == 'string' else b'hello'
    f_bytes = F[:2] + b'\x8a' + F[3:] if v == 'byte' else F
    move_g, move_f = v == 'move', v == 'bind'

    def pool(strings):
        offs, o = {}, 0
        for s in strings:
            offs[s] = o
            o += len(s) + 1
        return b''.join(s + b'\0' for s in strings), offs
    a_strings = [b'world', hello] if v == 'layout' else [hello, b'world']
    if move_g:
        a_strings.remove(b'world')
    if move_f:
        a_strings.remove(hello)
    b_strings = [b'world'] + ([hello] if move_f else [])
    a_pool, a_off = pool(a_strings)
    b_pool, b_off = pool(b_strings)
    objs = dict(a=([], []), b=([], []), c=([], []))

    def put(obj, sec, sym=None):
        objs[obj][0].append(sec)
        if sym:
            objs[obj][1].append(sym)
    put('a', dict(name='.text.m', data=M, relocs=[(1, 4, 'sym:_ZL1hi', -4)]), dict(name='m', section='.text.m', size=len(M)))
    put('a', dict(name='.text._ZL1hi', data=b'\x8d\x47\x01\xc3'),
        dict(name='_ZL1hi', bind=STB_LOCAL, section='.text._ZL1hi', size=4))
    put('b', dict(name='.text.k', data=K, relocs=[(1, 4, 'sym:_ZL1hi', -4), (8, 2, 'sec:.rodata.str1.1', b_off[b'world'] - 4)]),
        dict(name='k', section='.text.k', size=len(K)))
    put('b', dict(name='.text._ZL1hi', data=b'\x8d\x47\x02\xc3'),
        dict(name='_ZL1hi', bind=STB_LOCAL, section='.text._ZL1hi', size=4))
    put('c', dict(name='.text.n', data=b'\x31\xc0\xc3'), dict(name='n', section='.text.n', size=3))
    fo = 'b' if move_f else 'a'
    offs = b_off if move_f else a_off
    put(fo, dict(name='.text.f', data=f_bytes, relocs=[
        (5, 4, 'sym:' + gname, -4), (10, 4, 'sym:' + cos, -4), (17, 2, 'sec:.rodata.str1.1', offs[hello] - 4),
        (22, 4, 'sym:_ZL1hi', -4)]), dict(name='f', section='.text.f', size=len(F)))
    go = 'b' if move_g else 'a'
    offs = b_off if move_g else a_off
    put(go, dict(name='.text.' + gname, data=G, relocs=[(4, 2, 'sec:.rodata.str1.1', offs[b'world'] - 4)]),
        dict(name=gname, section='.text.' + gname, size=len(G)))
    put('a', dict(name='.data.table', data=b'\0' * 8, flags=SHF_ALLOC | SHF_WRITE, align=8, relocs=[(0, 1, 'sym:' + gname, 0)]),
        dict(name='table', type=STT_OBJECT, section='.data.table', size=8))
    put('a', dict(name='.rodata.str1.1', data=a_pool, flags=SHF_ALLOC | SHF_MERGE | SHF_STRINGS, entsize=1))
    put('b', dict(name='.rodata.str1.1', data=b_pool, flags=SHF_ALLOC | SHF_MERGE | SHF_STRINGS, entsize=1))
    if init_in:
        put(init_in, dict(name='.text._GLOBAL__sub_I_x', data=M, relocs=[(1, 4, 'sym:n', -4)]),
            dict(name='_GLOBAL__sub_I_x', bind=STB_LOCAL, section='.text._GLOBAL__sub_I_x', size=len(M)))
        put(init_in, dict(name='.init_array', data=b'\0' * 8, type=SHT_INIT_ARRAY, flags=SHF_ALLOC | SHF_WRITE, align=8,
                          entsize=8, relocs=[(0, 1, 'sec:.text._GLOBAL__sub_I_x', 0)]))
    if eh:
        tail = b'\x42\x0e\x18' if eh.endswith('changed') else b'\x42\x0e\x10'
        records = [('sec:.text.f', 0, len(F), b'\x41\x0e\x10')]
        if eh.startswith('thunk'):
            records.append(('sec:.text.f', 20, 8, tail))
        orphan = [('sym:elsewhere', 0, 4, tail)] if eh.startswith('orphan') else []
        if eh_in == 'a':
            put('a', eh_frame(records + orphan))
        else:
            put('a', eh_frame(records))
            put(eh_in, eh_frame(orphan))
    out = {}
    for name, (secs, syms) in objs.items():
        defined = set(y['name'] for y in syms)
        for s in secs:
            for r in s.get('relocs', []):
                if r[2].startswith('sym:') and r[2][4:] not in defined:
                    defined.add(r[2][4:])
                    syms.append(dict(name=r[2][4:], section=None, type=STT_NOTYPE))
        path = 'game/%s.o' % ('moved/a' if name == 'a' and rename_a else name)
        out[path] = write_elf(secs, syms)
    return out


def synth_literal(path_string, init=False):
    S = b'\x48\x8d\x05\0\0\0\0\xc3'
    pool = path_string + b'\0hello\0'
    secs = [dict(name='.text.f', data=S, relocs=[(3, 2, 'sec:.rodata.str1.1', -4)]),
            dict(name='.rodata.str1.1', data=pool, flags=SHF_ALLOC | SHF_MERGE | SHF_STRINGS, entsize=1)]
    syms = [dict(name='f', section='.text.f', size=len(S))]
    if init:
        secs.append(dict(name='.init_array', data=b'\0' * 8, type=SHT_INIT_ARRAY, flags=SHF_ALLOC | SHF_WRITE,
                         align=8, entsize=8, relocs=[(0, 1, 'sec:.text.f', 0)]))
    return write_elf(secs, syms)


def synth_coff(literal, byte=0x90, init=False):
    secs = [dict(name='.text$mn', data=bytes([0x48, 0x8d, 0x05, 0, 0, 0, 0, byte, 0xc3]), relocs=[(3, '??_C@_0BA@X@', 4)]),
            dict(name='.rdata', data=literal + b'\0', flags=0x40301040)]
    if init:
        secs.append(dict(name='.CRT$XCU', data=b'\0' * 8, flags=0x40300040))
    syms = [dict(name='f', section=1, cls=2), dict(name='??_C@_0BA@X@', section=2, cls=2)]
    return write_coff(secs, syms)


def run_case(mode, base_objs, head_objs, map_lines=(), literal_lines=()):
    tmp = tempfile.mkdtemp(prefix='objcmp-')
    try:
        for side, objs in (('base', base_objs), ('head', head_objs)):
            os.makedirs(os.path.join(tmp, side), exist_ok=True)
            for rel, data in objs.items():
                p = os.path.join(tmp, side, rel)
                os.makedirs(os.path.dirname(p), exist_ok=True)
                with open(p, 'wb') as f:
                    f.write(data)
        mp = lp = None
        if map_lines:
            mp = os.path.join(tmp, 'map.txt')
            with open(mp, 'w') as f:
                f.write('\n'.join(map_lines) + '\n')
        if literal_lines:
            lp = os.path.join(tmp, 'literals.txt')
            with open(lp, 'w') as f:
                f.write('\n'.join(literal_lines) + '\n')
        lines = []
        if mode == 'per-object':
            ok = run_per_object(os.path.join(tmp, 'base'), os.path.join(tmp, 'head'), mp, lp, lines.append)
        else:
            ok = run_by_symbol(os.path.join(tmp, 'base'), os.path.join(tmp, 'head'), mp, None, None, None, None,
                               False, lines.append)
        return ok, lines
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


PROOF_FLAGS = ['-O0', '-g0', '-w', '-fno-inline', '-ffunction-sections', '-fdata-sections', '-fmerge-constants',
               '-fPIC', '-fno-ipa-stack-alignment', '-UNDEBUG', '-D__FILE__=""', '-D__LINE__=0']

COMPILED_A = '''#include <cmath>
struct B1 { virtual ~B1(); int b1 = 1; };
struct B2 { virtual ~B2(); int b2 = 2; };
struct D : B1, B2 { ~D() override; };
template <class T> struct DT : B1, B2 { ~DT() override {} };
B1::~B1() {}
B2::~B2() {}
D::~D() {}
B1* make_dt() { return new DT<int>(); }
static int h(int x) { return x + 1; }
int hg(int x) { return x * 3; }
int g(int x) { return hg(x) * 2; }
int e(int x) { try { if (x > 3) throw x; } catch (int y) { return y + 1; } return 0; }
int m(int x) { return h(x); }
double f(double x) { return std::cos(x) + h(1); }
const char* s() { static int calls = 0; ++calls; return "hello"; }
static int cb(int x) { return x - 1; }
typedef int (*Fn)(int);
Fn pick(int x) { return x ? &cb : &hg; }
'''
COMPILED_B = '''struct B1 { virtual ~B1(); int b1 = 1; };
struct B2 { virtual ~B2(); int b2 = 2; };
struct D : B1, B2 { ~D() override; };
template <class T> struct DT : B1, B2 { ~DT() override {} };
static int h(int x) { return x + 2; }
int k() { return h(7); }
'''
MOVED = '''D::~D() {}
B1* make_dt() { return new DT<int>(); }
int g(int x) { return hg(x) * 2; }
int e(int x) { try { if (x > 3) throw x; } catch (int y) { return y + 1; } return 0; }
const char* s() { static int calls = 0; ++calls; return "hello"; }
static int cb(int x) { return x - 1; }
typedef int (*Fn)(int);
Fn pick(int x) { return x ? &cb : &hg; }
'''


def compiled_sources(variant):
    a, b = COMPILED_A, COMPILED_B
    if variant == 'byte':
        a = a.replace('return hg(x) * 2;', 'return hg(x) * 3;')
    elif variant == 'cosf':
        a = a.replace('std::cos(x)', 'cosf(x)')
    elif variant == 'string':
        a = a.replace('"hello"', '"hellp"')
    elif variant == 'move':
        for line in MOVED.splitlines(True):
            a = a.replace(line, '')
        b = 'int hg(int x);\n' + b + MOVED
    elif variant == 'bind':
        line = 'double f(double x) { return std::cos(x) + h(1); }\n'
        a = a.replace(line, '')
        b = '#include <cmath>\n' + b + line
    elif variant == 'init':
        b = b + 'int kv = k();\n'
    elif variant == 'rename':
        a = a.replace('int g(int x)', 'int g2(int x)')
    return {'a.cpp': a, 'b.cpp': b}


def compile_variant(cc, variant, tmp):
    objs = {}
    d = os.path.join(tmp, 'src-' + variant)
    os.makedirs(d, exist_ok=True)
    for name, text in compiled_sources(variant).items():
        src = os.path.join(d, name)
        with open(src, 'w') as f:
            f.write(text)
        obj = os.path.join(d, name + '.o')
        subprocess.check_call([cc] + PROOF_FLAGS + ['-c', src, '-o', obj])
        objs['game/' + name + '.o'] = read_file(obj)
    return objs


FAKE_CPP = r'''import re
import sys
args = sys.argv[1:]
out = args[args.index('-o') + 1]
defs = set(a[2:].split('=')[0] for a in args if a.startswith('-D'))
src = [a for a in args if a.endswith(('.cpp', '.c', '.h', '.S'))][-1]
res = ['# 1 "%s"' % src]
stack = []
for line in open(src).read().split('\n'):
    s = line.strip()
    active = all(stack)
    if s.startswith(('#ifdef', '#ifndef', '#if ')):
        word = s.split()[1] if len(s.split()) > 1 else ''
        if s.startswith('#ifdef'):
            stack.append(word in defs)
        elif s.startswith('#ifndef'):
            stack.append(word not in defs)
        else:
            stack.append(word != '0')
        res.append('')
    elif s.startswith('#else'):
        stack[-1] = not stack[-1]
        res.append('')
    elif s.startswith('#endif'):
        stack.pop()
        res.append('')
    elif not active:
        res.append('')
    elif s.startswith('#define'):
        defs.add(s.split()[1].split('(')[0])
        res.append(s)
    else:
        res.append(re.sub(r'//.*', '', line))
open(out, 'w').write('\n'.join(res) + '\n')
'''


def write_tree(tree, files):
    for rel, text in files.items():
        p = os.path.join(tree, rel)
        os.makedirs(os.path.dirname(p), exist_ok=True)
        with open(p, 'w', newline='\n') as f:
            f.write(text)


def changed_lines_of(base_files, head_files, out_path):
    with open(out_path, 'w') as f:
        for rel in sorted(set(base_files) | set(head_files)):
            if base_files.get(rel) == head_files.get(rel) or not code_file(rel):
                continue
            for side, path, line in code_diff_rows(rel if rel in base_files else None, base_files.get(rel),
                                                   rel if rel in head_files else None, head_files.get(rel)):
                f.write('%s\t%s\t%d\n' % (side, path, line))


def lines_case(sides, compile_cmd, defines=(), readers=None):
    """Runs run_lines_compiled on both sides of a small tree and then run_by_symbol on two equal object
    sets. SIDES is {'base': {path: text}, 'head': {path: text}}; every .cpp is its own TU, and READERS
    names which TUs read which headers. COMPILE_CMD gives a TU's argument list."""
    tmp = tempfile.mkdtemp(prefix='objcmp-lines-')
    try:
        tree = os.path.join(tmp, 'tree').replace('\\', '/')
        build = os.path.join(tmp, 'build').replace('\\', '/')
        os.makedirs(build)
        changed = os.path.join(tmp, 'changed-lines.txt')
        changed_lines_of(sides['base'], sides['head'], changed)
        changes = os.path.join(tmp, 'changes.txt')
        with open(changes, 'w') as f:
            for rel in sorted(set(sides['base']) | set(sides['head'])):
                if sides['base'].get(rel) != sides['head'].get(rel):
                    st = 'A' if rel not in sides['base'] else 'D' if rel not in sides['head'] else 'M'
                    f.write('%s\t%s\n' % (st, rel))
        out = {}
        for side in ('base', 'head'):
            if os.path.isdir(tree):
                shutil.rmtree(tree)
            write_tree(tree, sides[side])
            db, deps = [], []
            for rel in sorted(sides[side]):
                if not rel.endswith('.cpp'):
                    continue
                obj = 'game/CMakeFiles/game.dir/%s.o' % rel
                src = tree + '/' + rel
                db.append(dict(directory=build, file=src, output=obj, arguments=compile_cmd(src, obj, defines)))
                files = [src] + [tree + '/' + h for h in (readers or {}).get(rel, [])]
                deps.append('%s: #deps %d, deps mtime 1 (VALID)' % (obj, len(files)))
                deps.extend('    ' + x for x in files)
            dbp = os.path.join(tmp, side + '.db.json')
            json.dump(db, open(dbp, 'w'))
            depp = os.path.join(tmp, side + '.deps')
            with open(depp, 'w') as f:
                f.write('\n'.join(deps) + '\n\n')
            linesp = os.path.join(tmp, side + '.lines')
            run_lines_compiled(side, changed, depp, dbp, tree, build, linesp, log=lambda s: None)
            out[side] = (depp, linesp)
        objs = synth()
        objdir = os.path.join(tmp, 'objs')
        for side in ('base', 'head'):
            for rel, data in objs.items():
                p = os.path.join(objdir, side, rel)
                os.makedirs(os.path.dirname(p), exist_ok=True)
                with open(p, 'wb') as f:
                    f.write(data)
        lines = []
        ok = run_by_symbol(os.path.join(objdir, 'base'), os.path.join(objdir, 'head'), None, out['base'][0],
                           out['head'][0], changes, tree, False, lines.append, out['base'][1], out['head'][1])
        return ok, lines
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def fake_compile(tmp_script):
    def cmd(src, obj, defines):
        return [sys.executable, tmp_script] + ['-D' + d for d in defines] + ['-c', src, '-o', obj]
    return cmd


def changes_case(changes, extra_objs=None):
    """run_by_symbol on two equal object sets with a change list and no deps or lines."""
    tmp = tempfile.mkdtemp(prefix='objcmp-changes-')
    try:
        objs = synth()
        for side in ('base', 'head'):
            for rel, data in objs.items():
                p = os.path.join(tmp, side, rel)
                os.makedirs(os.path.dirname(p), exist_ok=True)
                with open(p, 'wb') as f:
                    f.write(data)
        ch = os.path.join(tmp, 'changes.txt')
        with open(ch, 'w') as f:
            f.write(''.join('%s\t%s\n' % c for c in changes))
        empty = os.path.join(tmp, 'empty.lines')
        open(empty, 'w').close()
        lines = []
        ok = run_by_symbol(os.path.join(tmp, 'base'), os.path.join(tmp, 'head'), None, None, None, ch, tmp, False,
                           lines.append, empty, empty)
        return ok, lines
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def synth_tls(rename=False, wrapper_only=False):
    """An object that defines a thread-local variable's initialization (__tls_init, _ZTH) or only
    a wrapper (_ZTW), at one path or another."""
    M = b'\xe8\0\0\0\0\xc3'
    secs = [dict(name='.text.f', data=b'\x31\xc0\xc3')]
    syms = [dict(name='f', section='.text.f', size=3)]
    if wrapper_only:
        secs.append(dict(name='.text._ZTW1x', data=M, relocs=[(1, 4, 'sym:_ZTH1x', -4)], group='_ZTW1x'))
        syms.append(dict(name='_ZTW1x', bind=STB_WEAK, section='.text._ZTW1x', size=len(M)))
        syms.append(dict(name='_ZTH1x', bind=STB_WEAK, section=None, type=STT_NOTYPE))
    else:
        secs.append(dict(name='.text.__tls_init', data=M, relocs=[(1, 4, 'sym:f', -4)]))
        syms.append(dict(name='__tls_init', bind=STB_LOCAL, section='.text.__tls_init', size=len(M)))
        syms.append(dict(name='_ZTH1x', bind=STB_GLOBAL, section='.text.__tls_init', size=len(M)))
    return {'game/%s.o' % ('moved/t' if rename else 't'): write_elf(secs, syms)}


LINE_A = '''#ifndef A_GUARD
#define A_GUARD
#define TRACE(x) ((void)0)
int Pick(int x)
{
#ifdef _WIN32
    return x + 1;   // the Windows path
#else
    return x + 2;
#endif
}
#if 0
int Dead() { return 1; }
#endif
#ifdef FEATURE
int Feature() { return 3; }
#endif
int Traced(int a) { TRACE(a + 1); return a; }
#ifdef _WIN32
/*
int windowsThing = 1;
*/
#endif
#endif
'''


def line_variants():
    a = LINE_A
    return {
        'windows': a.replace('return x + 1;', 'return x - 1000;'),
        'compiled': a.replace('    return x + 2;', '    return x + 3;'),
        'comment': a.replace('// the Windows path', '// the Windows-only path'),
        'dead': a.replace('int Dead() { return 1; }', 'int Dead() { return 2; }'),
        'guard': a.replace('A_GUARD', 'B_GUARD'),
        'cond-changed': a.replace('#ifdef FEATURE', '#ifdef OTHER_FEATURE'),
        'cond-moved': a.replace('#ifdef FEATURE\nint Feature() { return 3; }\n#endif\n', '').replace(
            'int Pick(int x)', '#ifdef FEATURE\nint Feature() { return 3; }\n#endif\nint Pick(int x)'),
        'dropped': a.replace('TRACE(a + 1)', 'TRACE(a + 2)'),
        'uncomment': a.replace('/*\nint windowsThing = 1;\n*/\n', 'int windowsThing = 1;\n'),
    }


COMPILED_LINES = '''#include <cassert>
#include <string>
#define CHECK(x, y) ((x) + (y))
int Pick(int x)
{
#ifdef _WIN32
    return x + 1;
#else
    return x + 2;
#endif
}
int Tick(int x)
{
#ifdef MANGOS_STRICT_TICK
    return x * 3;
#endif
    return x;
}
int Checked(int x) { assert(x > 0); return x; }
int Spread(int a) { return CHECK(a,
                                 1)
    ; }
#define TRACE(x) ((void)0)
int Traced(int a) { TRACE(a + 1); return a; }
'''


def compiled_line_variants():
    a = COMPILED_LINES
    return {
        'windows': (a.replace('return x + 1;', 'return x - 1000;'), False, 'not compiled by the proof build'),
        'strict-tick': (a.replace('return x * 3;', 'return x * 4;'), False, 'not compiled by the proof build'),
        'assert': (a.replace('assert(x > 0)', 'assert(x > 1)'), False, None),
        'spread': (a.replace('                                 1)\n', '                                 1 )\n'), True,
                   None),
        'dropped': (a.replace('TRACE(a + 1)', 'TRACE(a + 2)'), False, 'which the proof build defines without using it'),
    }


def gcc_compile(cc):
    def cmd(src, obj, defines):
        return [cc] + PROOF_FLAGS + ['-D' + d for d in defines] + ['-c', src, '-o', obj]
    return cmd


def compile_tree(cc, tree, out_dir, files):
    """Compiles every .cpp of FILES under TREE into OUT_DIR at game/<path>.o."""
    for rel in sorted(files):
        if rel.endswith('.cpp'):
            obj = os.path.join(out_dir, 'game', rel + '.o')
            os.makedirs(os.path.dirname(obj), exist_ok=True)
            subprocess.check_call([cc] + PROOF_FLAGS + ['-c', os.path.join(tree, rel), '-o', obj], cwd=tree)


def compiled_lines_case(cc, base_files, head_files):
    """The compiled shape of lines_case: real objects and real preprocessing with CC."""
    tmp = tempfile.mkdtemp(prefix='objcmp-cclines-')
    try:
        tree = os.path.join(tmp, 'tree').replace('\\', '/')
        build = os.path.join(tmp, 'build').replace('\\', '/')
        os.makedirs(build)
        changed = os.path.join(tmp, 'changed-lines.txt')
        changed_lines_of(base_files, head_files, changed)
        changes = os.path.join(tmp, 'changes.txt')
        with open(changes, 'w') as f:
            for rel in sorted(set(base_files) | set(head_files)):
                if base_files.get(rel) != head_files.get(rel):
                    st = 'A' if rel not in base_files else 'D' if rel not in head_files else 'M'
                    f.write('%s\t%s\n' % (st, rel))
        res = {}
        for side, files in (('base', base_files), ('head', head_files)):
            if os.path.isdir(tree):
                shutil.rmtree(tree)
            write_tree(tree, files)
            objdir = os.path.join(tmp, side)
            compile_tree(cc, tree, objdir, files)
            db, deps = [], []
            for rel in sorted(files):
                if rel.endswith('.cpp'):
                    obj = 'game/%s.o' % rel
                    src = tree + '/' + rel
                    db.append(dict(directory=build, file=src, output=obj, arguments=gcc_compile(cc)(src, obj, ())))
                    deps.append('%s: #deps 1, deps mtime 1 (VALID)' % obj)
                    deps.append('    ' + src)
            dbp = os.path.join(tmp, side + '.db.json')
            json.dump(db, open(dbp, 'w'))
            depp = os.path.join(tmp, side + '.deps')
            with open(depp, 'w') as f:
                f.write('\n'.join(deps) + '\n\n')
            linesp = os.path.join(tmp, side + '.lines')
            run_lines_compiled(side, changed, depp, dbp, tree, build, linesp, log=lambda s: None)
            res[side] = (objdir, depp, linesp)
        lines = []
        ok = run_by_symbol(res['base'][0], res['head'][0], None, res['base'][1], res['head'][1], changes, tree,
                           False, lines.append, res['base'][2], res['head'][2])
        return ok, lines
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def build_script_case(script, gate, base_files, head_files):
    """Commits BASE_FILES and HEAD_FILES to a small git tree, builds both with SCRIPT in the proof
    configuration as the workflow does, and compares them by symbol."""
    tmp = tempfile.mkdtemp(prefix='objcmp-build-')
    try:
        tree = os.path.join(tmp, 'tree')
        proof = os.path.join(tmp, 'proof')
        os.makedirs(proof)
        env = dict(os.environ, CCACHE_DIR=os.path.join(tmp, 'ccache'), GIT_AUTHOR_NAME='objcmp',
                   GIT_AUTHOR_EMAIL='objcmp@localhost', GIT_COMMITTER_NAME='objcmp',
                   GIT_COMMITTER_EMAIL='objcmp@localhost')
        q = dict(cwd=tree, env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        os.makedirs(tree)
        subprocess.check_call(['git', 'init', '-q'], **q)
        shas = []
        for files in (base_files, head_files):
            for rel in os.listdir(tree):
                if rel != '.git':
                    p = os.path.join(tree, rel)
                    shutil.rmtree(p) if os.path.isdir(p) else os.remove(p)
            write_tree(tree, files)
            subprocess.check_call(['git', 'add', '-A'], **q)
            subprocess.check_call(['git', 'commit', '-q', '--allow-empty', '-m', 'side'], **q)
            shas.append(subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=tree).decode().strip())
        run_diff_lines(tree, shas[0], shas[1], os.path.join(proof, 'changed-lines.txt'), log=lambda s: None)
        with open(os.path.join(proof, 'changes.txt'), 'wb') as f:
            f.write(subprocess.check_output(['git', '-C', tree, 'diff', '--name-status', '-M', shas[0], shas[1]]))
        build = os.path.join(tmp, 'build')
        for sha, side in zip(shas, ('base', 'head')):
            subprocess.check_call(['bash', script, 'proof', sha, side, tree, build, proof, gate], env=env,
                                  stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        lines = []
        ok = run_by_symbol(os.path.join(proof, 'base'), os.path.join(proof, 'head'), None,
                           os.path.join(proof, 'base.deps'), os.path.join(proof, 'head.deps'),
                           os.path.join(proof, 'changes.txt'), tree, False, lines.append,
                           os.path.join(proof, 'base.lines'), os.path.join(proof, 'head.lines'))
        return ok, lines
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


TOY_CMAKE = '''cmake_minimum_required(VERSION 3.19)
project(MaNGOS C CXX)
option(WITH_FEATURE "a feature" %s)
if(WITH_FEATURE)
    add_compile_definitions(FEATURE=1)
endif()
add_library(game STATIC T.cpp U.cpp)
target_include_directories(game PRIVATE inc1 inc2)
'''
TOY_BASE = {
    'CMakeLists.txt': TOY_CMAKE % 'OFF',
    'inc2/Conf.h': '#define LIMIT 10\n',
    'T.cpp': '#include "Conf.h"\nint Limit() { return LIMIT; }\n',
    'U.cpp': 'int Feature()\n{\n#ifdef FEATURE\n    return 1;\n#else\n    return 0;\n#endif\n}\n',
}


def lines_self_tests(check, cc, build_script):
    tmp = tempfile.mkdtemp(prefix='objcmp-fake-')
    try:
        fake = os.path.join(tmp, 'fake_cpp.py')
        with open(fake, 'w') as f:
            f.write(FAKE_CPP)
        cmd = fake_compile(fake)
        v = line_variants()
        for name, expect, reason, label in (
                ('windows', False, 'not compiled by the proof build (base): a.cpp:7',
                 'lines: a change inside #ifdef _WIN32'),
                ('compiled', True, None, 'lines: a change to a compiled line'),
                ('comment', True, None, 'lines: a comment changed inside #ifdef _WIN32'),
                ('dead', True, None, 'lines: a change inside #if 0'),
                ('guard', True, None, 'lines: the include guard renamed'),
                ('cond-changed', False, 'a conditional directive that changed, not moved',
                 'lines: a conditional directive changed'),
                ('cond-moved', True, None, 'lines: a conditional block moved, its directives unchanged'),
                ('dropped', False, 'which the proof build defines without using it',
                 'lines: an argument of a macro the proof build drops'),
                ('uncomment', False, 'not compiled by the proof build (head)',
                 'lines: code uncommented inside #ifdef _WIN32')):
            check(label, expect, lines_case({'base': {'a.cpp': LINE_A}, 'head': {'a.cpp': v[name]}}, cmd,
                                            defines=('FEATURE', 'OTHER_FEATURE')), reason)
        check('lines: a changed MASM file (.asm) no build compiles', False,
              lines_case({'base': {'x.asm': 'mov eax, 1\n', 'a.cpp': LINE_A},
                          'head': {'x.asm': 'mov eax, 2\n', 'a.cpp': LINE_A}}, cmd), 'x.asm:1')
        check('lines: a changed test file the build does not compile', False,
              lines_case({'base': {'src/tests/SpellTest.h': 'int CheckSum(int x) { return x == 3; }\n',
                                   'a.cpp': LINE_A},
                          'head': {'src/tests/SpellTest.h': 'int CheckSum(int x) { return 1; }\n', 'a.cpp': LINE_A}},
                         cmd), 'src/tests/SpellTest.h:1')
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    check('gate: a change to objcmp.py', False, changes_case([('M', 'src/tests/tools/objcmp.py')]),
          'the change edits the object proof itself')
    check('gate: a change to the workflow', False, changes_case([('M', '.github/workflows/core_object_proof.yml')]),
          'the change edits the object proof itself')
    S = 'by-symbol'
    check('by-symbol: an object with a thread-local initializer moves to another path', False,
          run_case(S, synth_tls(), synth_tls(rename=True)), 'DIFF INIT: ')
    check('by-symbol: an object with a thread-local wrapper moves to another path', False,
          run_case(S, synth_tls(wrapper_only=True), synth_tls(rename=True, wrapper_only=True)), 'DIFF INIT: ')
    check('by-symbol: an object with a thread-local initializer, unchanged', True,
          run_case(S, synth_tls(), synth_tls()))
    if cc:
        for name, (text, expect, reason) in sorted(compiled_line_variants().items()):
            check('compiled lines: %s' % name, expect,
                  compiled_lines_case(cc, {'a.cpp': COMPILED_LINES}, {'a.cpp': text}), reason)
        tls = ('#include <string>\nstatic std::string Make() { return std::string("a") + "b"; }\n'
               'thread_local std::string tls_name = Make();\nstd::string const& Name() { return tls_name; }\n')
        check('compiled lines: a file with a thread_local std::string moves', False,
              compiled_lines_case(cc, {'Old/Tls.cpp': tls}, {'New/Tls.cpp': tls}), 'DIFF INIT: ')
    if build_script:
        gate = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(build_script)))))
        if not os.path.exists(os.path.join(gate, 'cmake', 'MangosProof.cmake')):
            gate = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
        tools = all(shutil.which(x) for x in ('cmake', 'ninja', 'git'))
        if not tools:
            print('self-test: build script cases skipped: cmake, ninja and git are needed')
            return
        shadow = dict(TOY_BASE)
        shadow['inc1/Conf.h'] = '#define LIMIT 20\n'
        check('build: the head adds a header that shadows one on the include path', False,
              build_script_case(build_script, gate, TOY_BASE, shadow), '_Z5Limitv')
        option = dict(TOY_BASE)
        option['CMakeLists.txt'] = TOY_CMAKE % 'ON'
        check('build: the head changes an option default', False,
              build_script_case(build_script, gate, TOY_BASE, option), '_Z7Featurev')
        check('build: the same tree twice', True, build_script_case(build_script, gate, TOY_BASE, TOY_BASE))


def self_test(cc, build_script=None):
    rows = []

    def check(label, expect, case, reason=None):
        got, lines = case
        ok = expect == got and (reason is None or any(reason in line for line in lines))
        rows.append(ok)
        print('self-test: %-78s %s (%s)' % (label, 'PASS' if ok else 'FAIL', 'IDENTICAL' if got else 'DIFFERENT'))
        if not ok:
            for line in lines:
                print('    | ' + line)
    base = synth()
    S = 'by-symbol'
    check('by-symbol: the same objects', True, run_case(S, base, synth()))
    check('by-symbol: one changed byte in a function', False, run_case(S, base, synth('byte')))
    check('by-symbol: a call to cos becomes a call to cosf', False, run_case(S, base, synth('cosf')))
    check('by-symbol: a string literal changes', False, run_case(S, base, synth('string')))
    check('by-symbol: the string pool is laid out in another order', True, run_case(S, base, synth('layout')))
    check('by-symbol: a function moves to another object', True, run_case(S, base, synth('move')))
    check('by-symbol: a moved function reaches another static of the same name', False, run_case(S, base, synth('bind')),
          'DIFF changed: f ')
    check('by-symbol: a dynamic initializer is added', False, run_case(S, base, synth(init_in='a')))
    check('by-symbol: a symbol is renamed, no map', False, run_case(S, base, synth('rename')))
    check('by-symbol: a symbol is renamed, with the map', True, run_case(S, base, synth('rename'), map_lines=['g -> g2']))
    check('by-symbol: an object moves to another path, no initializer', True, run_case(S, base, synth(rename_a=True)))
    check('by-symbol: an object with a dynamic initializer moves to another path', False,
          run_case(S, synth(init_in='a'), synth(init_in='a', rename_a=True)), 'DIFF INIT: ')
    check('by-symbol: a function moves into an object that has a dynamic initializer', False,
          run_case(S, synth(init_in='b'), synth('move', init_in='b')), 'DIFF INIT: game/b.o changed')
    check('by-symbol: a function moves; an untouched object has a dynamic initializer', True,
          run_case(S, synth(init_in='c'), synth('move', init_in='c')))
    check('by-symbol: a second unwind record inside a function (a thunk), the same', True,
          run_case(S, synth(eh='thunk'), synth(eh='thunk')))
    check('by-symbol: a second unwind record inside a function, other instructions', False,
          run_case(S, synth(eh='thunk'), synth(eh='thunk-changed')), 'DIFF changed: f ')
    check('by-symbol: an unwind record without a function, the same in both builds', True,
          run_case(S, synth(eh='orphan'), synth(eh='orphan')))
    check('by-symbol: an unwind record without a function, moved to another object', True,
          run_case(S, synth(eh='orphan'), synth(eh='orphan', eh_in='c')))
    check('by-symbol: an unwind record without a function, in the head only', False,
          run_case(S, synth(eh='thunk'), synth(eh='orphan')), 'unwind record without a function, 0 in base and 1')
    check('by-symbol: an unwind record without a function, other instructions', False,
          run_case(S, synth(eh='orphan'), synth(eh='orphan-changed')), 'unwind record without a function')
    P = 'per-object'
    check('per-object: the same objects', True, run_case(P, base, synth()))
    check('per-object: one changed byte', False, run_case(P, base, synth('byte')))
    check('per-object: a function moves to another object', False, run_case(P, base, synth('move')))
    lit_b = {'src/game/Old/A.cpp.o': synth_literal(b'/w/src/game/Old/A.cpp')}
    lit_h = {'src/game/New/A.cpp.o': synth_literal(b'/w/src/game/New/A.cpp')}
    mp = ['src/game/Old/A.cpp -> src/game/New/A.cpp']
    check('per-object: a moved file whose path string changed, not listed', False, run_case(P, lit_b, lit_h, map_lines=mp))
    check('per-object: a moved file whose path string changed, listed', True,
          run_case(P, lit_b, lit_h, map_lines=mp, literal_lines=mp))
    check('per-object: a path string other than the listed one', False,
          run_case(P, lit_b, {'src/game/New/A.cpp.o': synth_literal(b'/w/src/game/New/B.cpp')}, map_lines=mp,
                   literal_lines=mp))
    check('per-object: a moved file with a dynamic initializer', False,
          run_case(P, {'src/game/Old/A.cpp.o': synth_literal(b'x', init=True)},
                   {'src/game/New/A.cpp.o': synth_literal(b'x', init=True)}, map_lines=mp), 'OTHER: INIT')
    cb = {'Spell.obj': synth_coff(b'C:\\w\\src\\game\\Old\\Spell.cpp')}
    ch = {'Spell.obj': synth_coff(b'C:\\w\\src\\game\\New\\Spell.cpp')}
    check('per-object COFF: the same objects', True, run_case(P, cb, cb))
    check('per-object COFF: one changed byte', False,
          run_case(P, cb, {'Spell.obj': synth_coff(b'C:\\w\\src\\game\\Old\\Spell.cpp', byte=0x91)}))
    check('per-object COFF: a path string changed, not listed', False, run_case(P, cb, ch))
    check('per-object COFF: a path string changed, listed', True,
          run_case(P, cb, ch, literal_lines=['src/game/Old/Spell.cpp -> src/game/New/Spell.cpp']))
    check('per-object COFF: a separator flip in a listed path string', True,
          run_case(P, cb, {'Spell.obj': synth_coff(b'C:\\w\\src\\game/Old/Spell.cpp')},
                   literal_lines=['src/game/Old/Spell.cpp -> src/game/Old/Spell.cpp']))
    check('per-object COFF: a moved file with a dynamic initializer', False,
          run_case(P, {'Old/Spell.obj': synth_coff(b'x', init=True)}, {'New/Spell.obj': synth_coff(b'x', init=True)},
                   map_lines=['Old/Spell -> New/Spell']), 'OTHER: INIT')
    check('per-object COFF: a moved file with a dynamic initializer, flat object names', False,
          run_case(P, {'Spell.obj': synth_coff(b'x', init=True)}, {'Spell.obj': synth_coff(b'x', init=True)},
                   map_lines=['src/game/Old/Spell.cpp -> src/game/New/Spell.cpp']), 'OTHER: INIT')
    if cc:
        tmp = tempfile.mkdtemp(prefix='objcmp-cc-')
        try:
            cbase = compile_variant(cc, 'base', tmp)
            for v, expect, label, mp in (
                    ('base', True, 'compiled by-symbol: the same sources', ()),
                    ('byte', False, 'compiled by-symbol: one changed constant', ()),
                    ('cosf', False, 'compiled by-symbol: std::cos becomes cosf', ()),
                    ('string', False, 'compiled by-symbol: a string literal changes', ()),
                    ('move', True, 'compiled by-symbol: functions move (a throw, a static local, a string, addresses, a thunk)', ()),
                    ('bind', False, 'compiled by-symbol: a moved function binds the other file\'s static h', ()),
                    ('init', False, 'compiled by-symbol: a dynamic initializer is added', ()),
                    ('rename', False, 'compiled by-symbol: g renamed g2, no map', ()),
                    ('rename', True, 'compiled by-symbol: g renamed g2, with the map', ('_Z1gi -> _Z2g2i',))):
                check(label, expect, run_case(S, cbase, compile_variant(cc, v, tmp), map_lines=mp))
            check('compiled per-object: functions move to another file', False,
                  run_case(P, cbase, compile_variant(cc, 'move', tmp)))
        finally:
            shutil.rmtree(tmp, ignore_errors=True)
    lines_self_tests(check, cc, build_script)
    failures = rows.count(False)
    print('self-test: %s (%d cases, %d failure(s))' % ('PASS' if not failures else 'FAIL', len(rows), failures))
    return not failures


def run_initializers(top, out):
    found = 0
    for rel in list_objects(top):
        data = read_file(os.path.join(top, rel))
        if data[:4] == b'\x7fELF':
            e = Elf(rel, data)
            secs = ['%s (%d bytes)' % (s.name, s.size) for s in e.secs if elf_init(s) and s.size]
        else:
            c = coff_parse(rel, data)
            secs = ['%s (%d bytes)' % (s['name'], s['size']) for s in c['secs'] if s['name'].startswith('.CRT$X') and s['size']]
        if secs:
            found += 1
            out('%s: %s' % (rel, ', '.join(secs)))
    out('objects with a dynamic initializer: %d' % found)
    return found


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument('--per-object', nargs=2, metavar=('BASE', 'HEAD'))
    g.add_argument('--by-symbol', nargs=2, metavar=('BASE', 'HEAD'))
    g.add_argument('--self-test', action='store_true')
    g.add_argument('--initializers', metavar='DIR')
    g.add_argument('--diff-lines', nargs=4, metavar=('TREE', 'BASE', 'HEAD', 'OUT'))
    g.add_argument('--lines-compiled', metavar='SIDE', choices=('base', 'head'))
    ap.add_argument('--changed-lines')
    ap.add_argument('--deps')
    ap.add_argument('--compile-db')
    ap.add_argument('--build')
    ap.add_argument('--out')
    ap.add_argument('--base-lines')
    ap.add_argument('--head-lines')
    ap.add_argument('--build-script')
    ap.add_argument('--map')
    ap.add_argument('--literals')
    ap.add_argument('--base-deps')
    ap.add_argument('--head-deps')
    ap.add_argument('--changes')
    ap.add_argument('--root')
    ap.add_argument('--list-init', action='store_true')
    ap.add_argument('--cc')
    ap.add_argument('--report')
    a = ap.parse_args()
    try:
        if a.self_test:
            return 0 if self_test(a.cc, a.build_script) else 1
        if a.initializers:
            run_initializers(a.initializers, print)
            return 0
        if a.diff_lines:
            run_diff_lines(*a.diff_lines)
            return 0
        if a.lines_compiled:
            run_lines_compiled(a.lines_compiled, a.changed_lines, a.deps, a.compile_db, a.root, a.build, a.out)
            return 0
    except Exception:
        traceback.print_exc()
        print('objcmp: error: the run did not finish')
        return 2
    lines = []

    def out(line):
        print(line)
        lines.append(line)
    try:
        if a.per_object:
            ok = run_per_object(a.per_object[0], a.per_object[1], a.map, a.literals, out)
        else:
            ok = run_by_symbol(a.by_symbol[0], a.by_symbol[1], a.map, a.base_deps, a.head_deps, a.changes, a.root,
                               a.list_init, out, a.base_lines, a.head_lines)
    except Exception:
        traceback.print_exc()
        print('objcmp: error: the run did not finish')
        return 2
    verdict = 'objcmp: %s' % ('IDENTICAL' if ok else 'DIFFERENT')
    out(verdict)
    if a.report:
        with open(a.report, 'w', encoding='utf-8') as f:
            f.write('\n'.join(lines) + '\n')
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
