#!/usr/bin/env python3
"""cast_verbatim.py [--root <repo root>] [--base <ref>] --check | --self-test

The verbatim proof for call sites that stopped casting `this` to the character class: every
rewritten call is pasted back to the cast form it stands for, and the lines around it must read
byte for byte as they did at BASE.

For each file in FILES, --check:
  1. reads the file in the working tree and at BASE (`git show <base>:<file>`);
  2. finds every call of each FORM (its direct spelling, the argument list read to the matching
     parenthesis) and writes it back as the cast call it stands for: a FORM with a `suffix` must
     end its argument list with that suffix (the argument the rewrite appended), and the suffix
     is dropped; every other argument is kept as written, in its order; each FORM's count must be
     the file's listed count, in the working tree and (its cast spelling) at BASE, and no cast
     spelling of a FORM may be left in the working tree (a site the rewrite missed);
     a `branch` FORM is a whole statement `<lhs> <op> <call>(<args>);` that stood for a type test
     with two branches: it is written back as `<guard>` with the Player branch
     `<lhs> <op> <Player override's returned expression>;` and the `else` branch
     `<lhs> <op> <Unit default's returned expression>;`, both expressions read from the one-line
     definitions in `player` and `unit` (the working tree's), the Player one with its RENAMES
     written back to their cast spelling; the call's arguments must be the override's parameter
     names, in order; its site is the Player branch's line, and its cast spelling is that
     branch's expression; two listed `branch` FORMs whose Player expressions are equal fail (their
     sites could not be told apart at BASE); a `branch` FORM spelled in a listed added entry that
     stands once, directly below its base line, is the declaration the rewrite added, not a site;
  3. pairs the sites in order, the n-th written-back call of a FORM with the n-th cast call of it
     at BASE (a `branch` site by the Player line of its own n-th expansion; a base line a `branch`
     site claims may be claimed by no other site), and compares the WINDOW around each: the site's
     line and the WINDOW lines above and below it at BASE against the working tree's lines,
     aligned outward from the site, byte for
     byte; the first differing line is named with its file and line;
  4. inside a window, an ADDED entry (a line or a block of lines, listed with the base line it
     follows) is dropped where it stands directly below that line, and a CHANGED entry (the line
     as it reads once the FORMs are written back, listed with the base line it replaced) is
     written back where it stands in that line's place; the base line an entry names must stand
     once in the window's base text, and an entry whose base line stands in a window must be
     found there;
  5. an entry whose base line stands in no window is checked at its place only: an added entry
     stands once in the working tree, directly below its base line, which stands once in the file
     at BASE; a changed line stands once in the working tree, its base line once at BASE and
     nowhere in the working tree;
  6. requires no direct spelling of a FORM the file does not list, outside the added lines it
     dropped or found at their place; a FORM whose direct spelling also stands in code that never
     cast (`elsewhere`) is only looked for in the files that list it; a file that `declares` the
     overrides (Player.h, whose own code calls its methods directly and never cast) lists no FORM
     and is not searched for one: only its entries are checked, at their place.
A window set for a base line that holds no site fails.

WINDOW is 11 lines: measured over every site, the farthest guard or statement a site relies on
stands 11 lines away (UnitDamage.cpp:655 under the preventDeathSpell test at :644;
UnitAuraProcHandler.cpp:2822 under the type return at :2811); the rest stand within 6 lines
(the guard above an added call, the return below a tested one, the assignment below :342 that
:655 relies on, :655's case label, :3201's type return at :3195). A site that relies on a line
farther away sets its own window in its file's spec: UnitAuraProcHandler.cpp:2881 relies on the
same type return at :2811, 70 lines above it, and five mount sites (below) on their type test.
The combat-stats sites stand within 9 lines of theirs: UnitCombat.cpp:262 and :289 5 (the outer
dodge and parry tests at :257 and :284; the `else` branch ends 5 below), :413 5 (the
normalized-player flag at :408 its guard reads; the guard at :411; the range selection 3 below),
:748 9 (the `canDodge` test at :739; the clamp 6 below) and :773 7 (`canParry` at :766; the clamp 6
below); UnitDamage.cpp:83 7 (the player flag at :76; the guard at :81; the return 3 below);
UnitSpellBonus.cpp:704 and :1309 3 (their type tests); UnitPower.cpp:344, :350, :351, :365, :367
and Unit.cpp:4250 0 to 1 (the type test on the site's own line; :344's case label above it). The
cast-item sites, UnitAuraProcHandler.cpp:551, :633, :719, :3569, :4686, :4961, :5013 and :5073,
stand 1 line below theirs (the guid and type test the ternary's condition holds). The mount and
pet sites rely on the type test of Mount (Unit.cpp:4042) or Unmount (:4140): :4047 stands 5 lines
below Mount's; :4092, :4095 and :4103, in the mount aura's arm, stand 50, 53 and 61 below it, and
:4152 and :4155 12 and 15 below Unmount's, so those five set their own window in their file's spec.
The visibility and session sites stand within 3 lines of theirs: UnitVisibility.cpp:91, :92 and
:93 1 to 3 below the two type tests at :90 their condition holds, :203 2 below :201 and :437 2
below :435; UnitAura.cpp:383 and Unit.cpp:4418 on the type test of their own line.

A `branch` site writes 8 lines for its one; every line number printed is the working tree's.

What fails: a changed, swapped or dropped argument; a changed, moved or dropped guard, or any
other changed line, inside a window; a site lost, added or still cast; an added line inside a
window that is not listed; a listed line that does not stand at its place; a base line an entry
names that stands twice in a window's base text, or in two windows at different lines. What
passes: any edit outside every window, unlisted. Windows may overlap; each is checked on its own.

The file:line of every rewritten site, old and new, is printed with its window.

python src/tests/tools/cast_verbatim.py --check          # against BASE, the parent of the rewrite
python src/tests/tools/cast_verbatim.py --self-test      # fixtures only, no git
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
import subprocess
import sys

BASE = '26df9c56a'

# The lines compared above and below each site.
WINDOW = 11

# The lines a `branch` site is written back as: the guard, the Player branch and the `else` branch;
# the Player branch's statement is the third.
BRANCH_LINES = 8
BRANCH_PLAYER = 2

# name -> the direct spelling (up to its opening parenthesis), the cast spelling it stands for,
# and the argument the rewrite appended (None: the arguments are unchanged); a `branch` FORM names
# its guard, the files holding the Player override and the Unit default, and the RENAMES.
FORMS = {
    'HasSpellCooldown': {'direct': 'm_spellCooldownMgr.HasSpellCooldown(',
                         'cast': '((Player*)this)->HasSpellCooldown(',
                         'suffix': ', time(NULL)'},
    'AddSpellCooldown': {'direct': 'm_spellCooldownMgr.AddSpellCooldown(',
                         'cast': '((Player*)this)->GetSpellCooldownMgr().AddSpellCooldown(',
                         'suffix': None},
    'CalculateMinMaxDamage': {'direct': 'CalculateMinMaxDamage(',
                              'cast': '((Player*)this)->CalculateMinMaxDamage(',
                              'suffix': None},
    'GetArmorPenetrationPct': {'direct': 'GetArmorPenetrationPct(',
                               'cast': '((Player*)this)->GetArmorPenetrationPct(',
                               'suffix': None},
    'GetBaseSpellPowerBonus': {'direct': 'GetBaseSpellPowerBonus(',
                               'cast': '((Player*)this)->GetBaseSpellPowerBonus(',
                               'suffix': None},
    'getClass': {'direct': 'GetTypeId() == TYPEID_PLAYER && getClass(',
                 'cast': 'GetTypeId() == TYPEID_PLAYER && ((Player const*)this)->getClass(',
                 'suffix': None, 'elsewhere': True},
    'HasSpell': {'direct': 'TYPEID_PLAYER || !HasSpell(',
                 'cast': 'TYPEID_PLAYER || !(Player*)(this)->HasSpell(',
                 'suffix': None},
    'GetItemByGuid': {'direct': 'GetItemByGuid(',
                      'cast': '((Player*)this)->GetInventoryMgr().GetItemByGuid(',
                      'suffix': None},
    'UnsummonPetTemporaryIfAny': {'direct': 'UnsummonPetTemporaryIfAny(',
                                  'cast': '((Player*)this)->UnsummonPetTemporaryIfAny(',
                                  'suffix': None},
    'ResummonPetTemporaryUnSummonedIfAny': {'direct': 'ResummonPetTemporaryUnSummonedIfAny(',
                                            'cast': '((Player*)this)->ResummonPetTemporaryUnSummonedIfAny(',
                                            'suffix': None},
    'InArena': {'direct': 'InArena(',
                'cast': '((Player*)this)->InArena(',
                'suffix': None},
    'GetCollisionHeight': {'direct': 'GetCollisionHeight(',
                           'cast': '((Player*)this)->GetCollisionHeight(',
                           'suffix': None},
    'isGameMaster': {'direct': 'TYPEID_PLAYER && isGameMaster(',
                     'cast': 'TYPEID_PLAYER && ((Player*)this)->isGameMaster(',
                     'suffix': None},
    'IsLoading': {'direct': '!IsLoading(',
                  'cast': '!((Player*)this)->IsLoading(',
                  'suffix': None},
    'IsLoggingOut': {'direct': '!IsLoggingOut(',
                     'cast': '!((Player*)this)->IsLoggingOut(',
                     'suffix': None},
    'GetTransport': {'direct': ' GetTransport(',
                     'cast': ' ((Player*)this)->GetTransport(',
                     'suffix': None},
    'IsGroupVisibleFor': {'direct': 'IsGroupVisibleFor(',
                          'cast': '((Player*)this)->IsGroupVisibleFor(',
                          'suffix': None},
    'GetDrunkValue': {'direct': 'GetDrunkValue(',
                      'cast': '((Player*)this)->GetDrunkValue(',
                      'suffix': None},
    'GetMeleeRollExpertiseReduction': {
        'kind': 'branch', 'direct': 'GetMeleeRollExpertiseReduction(',
        'guard': 'if (GetTypeId() == TYPEID_PLAYER)',
        'player': 'src/game/entities/player/Player.h', 'unit': 'src/game/Object/Unit.h',
        'renames': [('GetExpertiseDodgeOrParryReduction(', '((Player*)this)->GetExpertiseDodgeOrParryReduction(')]},
    'GetMeleeSpellExpertiseReduction': {
        'kind': 'branch', 'direct': 'GetMeleeSpellExpertiseReduction(',
        'guard': 'if (GetTypeId() == TYPEID_PLAYER)',
        'player': 'src/game/entities/player/Player.h', 'unit': 'src/game/Object/Unit.h',
        'renames': [('GetExpertiseDodgeOrParryReduction(', '((Player*)this)->GetExpertiseDodgeOrParryReduction(')]},
}

UNIT_H_COMBAT_STATS = '''        /**
         * The dodge and parry chance an attacker's expertise takes off in RollMeleeOutcomeAgainst.
         * @return the SPELL_AURA_MOD_EXPERTISE total times 25 here; Player returns its expertise
         * reduction times 100
         */
        virtual int32 GetMeleeRollExpertiseReduction(WeaponAttackType /*attType*/) const { return GetTotalAuraModifier(SPELL_AURA_MOD_EXPERTISE) * 25; }
        /**
         * The dodge and parry chance an attacker's expertise takes off in MeleeSpellHitResult.
         * @return the SPELL_AURA_MOD_EXPERTISE total times 25 here; Player returns its expertise
         * reduction times 100.0f
         */
        virtual int32 GetMeleeSpellExpertiseReduction(WeaponAttackType /*attType*/) const { return GetTotalAuraModifier(SPELL_AURA_MOD_EXPERTISE) * 25; }
        /**
         * Fills the weapon damage range CalculateDamage uses for a normalized attack: does nothing
         * here; Player computes the range from its own weapon and stats.
         */
        virtual void CalculateMinMaxDamage(WeaponAttackType /*attType*/, bool /*normalized*/,
                                           float& /*min_damage*/, float& /*max_damage*/) { }
        /**
         * @return the armor penetration percent CalcArmorReducedDamage passes on: 0 here; Player
         * returns its own
         */
        virtual float GetArmorPenetrationPct() const { return 0.0f; }
        /**
         * @return the base spell power the advertised damage and healing bonuses add: 0 here;
         * Player returns its own
         */
        virtual uint32 GetBaseSpellPowerBonus() const { return 0; }'''

UNIT_H_ITEM_BY_GUID = '''        /**
         * The item this unit holds under a guid; the proc handlers ask it for the item an aura was
         * cast from.
         * @return NULL here; Player returns the item its inventory holds under that guid, or NULL
         */
        virtual Item* GetItemByGuid(ObjectGuid /*guid*/) const { return NULL; }'''

UNIT_H_MOUNT_PET = '''
    protected:
        /**
         * Puts this unit's pet away until ResummonPetTemporaryUnSummonedIfAny brings it back; Mount
         * calls it for a mount by a GM command, and for a temporary pet or one in an arena.
         * Does nothing here; Player unsummons its pet and keeps a permanent pet's number to resummon.
         */
        virtual void UnsummonPetTemporaryIfAny() { }
        /**
         * Brings back the pet UnsummonPetTemporaryIfAny put away; Unmount calls it when no pet is out.
         * Does nothing here; Player loads that pet again unless it still may not have one out.
         */
        virtual void ResummonPetTemporaryUnSummonedIfAny() { }
        /**
         * @return whether this unit is in an arena, where Mount puts a controlled pet away under
         * PetUnsummonAtMount: false here; Player answers from its battleground
         */
        virtual bool InArena() const { return false; }
        /**
         * The collision height Mount and Unmount send to the client; they send none when it is 0.
         * @param mounted true for the height on the mount, false for the unit's own
         * @return 0 here; Player returns its mount's or its native model's height
         */
        virtual float GetCollisionHeight(bool /*mounted*/) const { return 0.0f; }

    public:'''

UNIT_H_VISIBILITY = '''
    protected:
        /**
         * @return whether this unit is a game master, whom IsTargetableForAttack never offers as a
         * target: false here; Player answers from its game master flag
         */
        virtual bool isGameMaster() const { return false; }
        /**
         * @return whether this unit's session is still loading it: AddSpellAuraHolder then adds an
         * aura to it while it is dead, and IsVisibleForOrDetect does not see it by its transport;
         * false here; Player asks its session
         */
        virtual bool IsLoading() const { return false; }
        /**
         * @return whether this unit's session is logging it out: IsVisibleForOrDetect then does not
         * see it by its transport; false here; Player asks its session
         */
        virtual bool IsLoggingOut() const { return false; }
        /**
         * @return the transport this unit rides, on which IsVisibleForOrDetect sees a player that
         * rides the same one even out of the world: NULL here; Player returns its own
         */
        virtual Transport* GetTransport() const { return NULL; }
        /**
         * @param p the player that looks at this unit
         * @return whether P sees this unit through its stealth under the group visibility
         * setting: false here; Player answers by its group, its raid or its team
         */
        virtual bool IsGroupVisibleFor(Player* /*p*/) const { return false; }
        /**
         * @return the drunk value canDetectInvisibilityOf takes as this unit's detection level
         * against the drunk invisibility: 0 here; Player returns its own
         */
        virtual uint16 GetDrunkValue() const { return 0; }

    public:'''

PLAYER_H_ITEM_BY_GUID = '''
        // The item Unit's proc handlers ask for by guid: the one the inventory holds, or NULL; private,
        // so only a call through Unit reaches it
        Item* GetItemByGuid(ObjectGuid guid) const override { return GetInventoryMgr().GetItemByGuid(guid); }'''

# file -> the count of each FORM rewritten in it, the lines the rewrite added, each with the base
# line it follows, the lines it changed, each with the base line it replaced, the sites whose
# window is not WINDOW, by base line, and whether it `declares` the overrides.
FILES = {
    'src/game/Object/Unit.h': {
        'forms': {},
        'added': [('#include "spells/SpellCooldownMgr.h"', '#include "spells/AuraContainer.h"'),
                  (UNIT_H_COMBAT_STATS, '        MeleeHitOutcome RollMeleeOutcomeAgainst(const Unit* pVictim, '
                                        'WeaponAttackType attType, int32 crit_chance, int32 miss_chance, '
                                        'int32 dodge_chance, int32 parry_chance, int32 block_chance) const;'),
                  (UNIT_H_ITEM_BY_GUID, '        bool IsTriggeredAtSpellProcEvent(Unit* pVictim, '
                                        'SpellAuraHolder* holder, SpellEntry const* procSpell, uint32 procFlag, '
                                        'uint32 procExtra, WeaponAttackType attType, bool isVictim, '
                                        'SpellProcEventEntry const*& spellProcEvent);'),
                  (UNIT_H_MOUNT_PET, '        void Unmount(bool from_aura = false);'),
                  ('class Transport;', 'class Totem;'),
                  (UNIT_H_VISIBILITY, '        bool canDetectInvisibilityOf(Unit const* u) const;'),
                  ('        SpellCooldownMgr m_spellCooldownMgr;', '        AuraContainer m_auras;')]},
    'src/game/entities/player/Player.h': {
        'forms': {},
        'declares': True,
        'added': [("        // The private GetItemByGuid override answers only Unit's lookup.",
                   '        // GetItemDisplayIdInSlot, IsValidPos and the static position checks are called on it '
                   'directly.'),
                  (PLAYER_H_ITEM_BY_GUID, '        ManagerPacketSink SessionSink() const;')],
        'changed': [('        // The item slots: the lookups (GetItemByPos, GetItemByGuid, GetItemByEntry,',
                     '        // The item slots. Decoupling D4i: the lookups (GetItemByPos, GetItemByGuid, '
                     'GetItemByEntry,'),
                    ('        float GetCollisionHeight(bool mounted) const override;',
                     '        float GetCollisionHeight(bool mounted) const;'),
                    ('        bool InArena() const override;', '        bool InArena() const;'),
                    ('        // rows) and its rules live on PetMgr. The three that reach the live pet',
                     '        // rows) and its rules live on PetMgr. Decoupling D4k: the three that reach the '
                     'live pet'),
                    ('        // through SessionSink(). The temporary-unsummon pet number is read and',
                     '        // through SessionSink(). Decoupling D4i: the temporary-unsummon pet number is read '
                     'and'),
                    ('        void UnsummonPetTemporaryIfAny() override;', '        void UnsummonPetTemporaryIfAny();'),
                    ('        void ResummonPetTemporaryUnSummonedIfAny() override;',
                     '        void ResummonPetTemporaryUnSummonedIfAny();'),
                    ('        bool isGameMaster() const override final { return m_ExtraFlags & PLAYER_EXTRA_GM_ON; }',
                     '        bool isGameMaster() const { return m_ExtraFlags & PLAYER_EXTRA_GM_ON; }'),
                    ('        bool IsGroupVisibleFor(Player* p) const override final;',
                     '        bool IsGroupVisibleFor(Player* p) const;'),
                    ("        // Whether the player's session is loading it or logging it out; the aura and "
                     "spell packet",
                     '        // Decoupling D5a: the aura and spell packet code asks Player, not the session, whether'),
                    ('        // code asks Player, not the session.',
                     '        // the player is loading or logging out.'),
                    ('        bool IsLoading() const override final;', '        bool IsLoading() const;'),
                    ('        bool IsLoggingOut() const override final;', '        bool IsLoggingOut() const;'),
                    ('        uint16 GetDrunkValue() const override final { return GetByteValue(PLAYER_BYTES_3, 1); }',
                     '        uint16 GetDrunkValue() const { return GetByteValue(PLAYER_BYTES_3, 1); }'),
                    ('        Transport* GetTransport() const override final { return m_transport; }',
                     '        Transport* GetTransport() const { return m_transport; }')]},
    'src/game/Object/Unit.cpp': {
        'forms': {'HasSpell': 1, 'UnsummonPetTemporaryIfAny': 2, 'ResummonPetTemporaryUnSummonedIfAny': 1,
                  'InArena': 1, 'GetCollisionHeight': 2, 'isGameMaster': 1},
        'added': [('    m_spellCooldownMgr(),', '    movespline(new Movement::MoveSpline()),')],
        'window': {4092: 50, 4095: 53, 4103: 61, 4152: 12, 4155: 15}},
    'src/game/Object/UnitCombat.cpp': {
        'forms': {'GetMeleeRollExpertiseReduction': 2, 'GetMeleeSpellExpertiseReduction': 2,
                  'CalculateMinMaxDamage': 1},
        'added': [],
        'changed': [('    // CalculateMinMaxDamage runs only for a normalized player attack; the six',
                     '    // The Player downcast is E2b, so it stays here under its original guard; the six')]},
    'src/game/Object/UnitDamage.cpp': {
        'forms': {'HasSpellCooldown': 1, 'AddSpellCooldown': 1, 'GetArmorPenetrationPct': 1},
        'added': [],
        'changed': [('        armorPenetrationPct = ((Player*)this)->GetArmorPenetrationPct();',
                     '        armorPenetrationPct = ((Player*)this)->GetArmorPenetrationPct();'
                     + ' ' * 62 + '// E2b, same guard')]},
    'src/game/Object/UnitPower.cpp': {
        'forms': {'getClass': 5},
        'added': []},
    'src/game/Object/UnitSpellBonus.cpp': {
        'forms': {'GetBaseSpellPowerBonus': 2},
        'added': []},
    'src/game/Object/UnitVisibility.cpp': {
        'forms': {'IsLoggingOut': 1, 'IsLoading': 1, 'GetTransport': 2, 'IsGroupVisibleFor': 1, 'GetDrunkValue': 1},
        'added': []},
    'src/game/Object/UnitAura.cpp': {
        'forms': {'IsLoading': 1},
        'added': []},
    'src/game/WorldHandlers/UnitAuraProcHandler.cpp': {
        'forms': {'HasSpellCooldown': 10, 'AddSpellCooldown': 8, 'GetItemByGuid': 8},
        'added': [],
        'window': {2881: 70}},
}


def line_of(text, pos):
    return text.count('\n', 0, pos) + 1


def closing_paren(text, start):
    """The index of the ')' that closes the argument list opening just before `start`, or -1."""
    depth = 1
    i = start
    while i < len(text):
        c = text[i]
        if c == '(':
            depth += 1
        elif c == ')':
            depth -= 1
            if depth == 0:
                return i
        i += 1
    return -1


def find_all(text, needle):
    at = 0
    while True:
        hit = text.find(needle, at)
        if hit < 0:
            return
        yield hit
        at = hit + len(needle)


def one_line_body(read, rel, name, marker, out):
    """(parameter names, returned expression) of NAME's one-line definition in REL, the one line
    holding MARKER, `{ return ` and ending with `; }`; None (after naming why) when there is not
    exactly one."""
    try:
        text = read(rel)
    except (OSError, KeyError, TypeError) as e:
        out('%s: FAILED: cannot read it: %s' % (rel, e))
        return None
    key = ' %s(' % name
    hits = [line for line in text.split('\n')
            if key in line and marker in line and '{ return ' in line and line.endswith('; }')]
    if len(hits) != 1:
        out('%s: FAILED: %d one-line definition(s) of %s holding %r, expected 1' % (rel, len(hits), name, marker))
        return None
    line = hits[0]
    start = line.index(key) + len(key)
    params = line[start:closing_paren(line, start)].strip()
    names = [p.split()[-1].lstrip('*&') for p in params.split(',')] if params else []
    return names, line[line.index('{ return ') + len('{ return '):-len('; }')]


def branch_cast(read, name, form, out):
    """(the override's parameter names, its returned expression with the RENAMES written back to
    their cast spelling), or None."""
    player = one_line_body(read, form['player'], name, ' override ', out)
    if player is None:
        return None
    names, expr = player
    for direct, cast in form['renames']:
        if direct not in expr:
            out('%s: FAILED: the override of %s returns %r, which holds no %r' % (form['player'], name, expr, direct))
            return None
        expr = expr.replace(direct, cast)
    return names, expr


def cast_of(name, form, read, out):
    """The spelling a FORM's site stands for at BASE, or None."""
    if form.get('kind') != 'branch':
        return form['cast']
    got = branch_cast(read, name, form, out)
    return None if got is None else got[1]


def listed_lines(text, spec):
    """The line indices of TEXT inside a listed added entry that stands once, directly below the
    line it follows: a `branch` FORM spelled there is the declaration the rewrite added, not a
    call site."""
    lines = text.split('\n')
    masked = set()
    for added, after in spec['added']:
        block = added.split('\n')
        hits = block_at(lines, block)
        if len(hits) == 1 and hits[0] > 0 and lines[hits[0] - 1] == after:
            masked.update(range(hits[0], hits[0] + len(block)))
    return masked


def paste_branches(rel, text, spec, out, read, forms):
    """(rc, text with every statement of a listed `branch` FORM written back as its two branches,
    the number written back, the pasted-back index of the first line of each, {form: those
    indices, in order}). Two listed `branch` FORMs whose Player expressions are equal fail: their
    sites could not be told apart at BASE."""
    branches = {}
    for name in sorted(spec['forms']):
        form = forms[name]
        if form.get('kind') != 'branch':
            continue
        player = branch_cast(read, name, form, out)
        unit = one_line_body(read, form['unit'], name, 'virtual ', out)
        if player is None or unit is None:
            return 1, text, 0, [], {}
        if player[1] in text:
            out('%s: FAILED: %d branch(es) still spelled %r: a site the rewrite missed'
                % (rel, text.count(player[1]), player[1]))
            return 1, text, 0, [], {}
        statement = re.compile(r'^(\s*)(\w+) (=|-=|\+=) ' + re.escape(form['direct']) + r'(.*)\);$')
        for other, b in branches.items():
            if b[3] == player[1]:
                out('%s: FAILED: the overrides of %s and %s return the same expression %r: their sites cannot be '
                    'told apart at the base' % (rel, other, name, player[1]))
                return 1, text, 0, [], {}
        branches[name] = (form, statement, player[0], player[1], unit[1])
    if not branches:
        return 0, text, 0, [], {}
    pasted, starts = [], []
    by_form = dict((name, []) for name in branches)
    found = dict((name, 0) for name in branches)
    masked = listed_lines(text, spec)
    for at, line in enumerate(text.split('\n')):
        hit = [] if at in masked else [name for name, b in branches.items() if b[0]['direct'] in line]
        if not hit:
            pasted.append(line)
            continue
        name = hit[0]
        form, statement, names, player_expr, unit_expr = branches[name]
        m = statement.match(line)
        if len(hit) > 1 or not m:
            out('%s:%d: FAILED: %s( does not stand as a whole statement `<lhs> <op> %s(...);`'
                % (rel, at + 1, name, name))
            return 1, text, 0, [], {}
        if m.group(4) != ', '.join(names):
            out("%s:%d: FAILED: %s(%s) does not pass the override's parameters (%s)"
                % (rel, at + 1, name, m.group(4), ', '.join(names)))
            return 1, text, 0, [], {}
        indent, lhs, op = m.group(1), m.group(2), m.group(3)
        starts.append(len(pasted))
        by_form[name].append(len(pasted))
        pasted += [indent + form['guard'], indent + '{', '%s    %s %s %s;' % (indent, lhs, op, player_expr),
                   indent + '}', indent + 'else', indent + '{', '%s    %s %s %s;' % (indent, lhs, op, unit_expr),
                   indent + '}']
        found[name] += 1
    for name, n in sorted(found.items()):
        if n != spec['forms'][name]:
            out('%s: FAILED: %d call(s) of %s, expected %d' % (rel, n, name, spec['forms'][name]))
            return 1, text, 0, [], {}
    return 0, '\n'.join(pasted), sum(found.values()), starts, by_form


def tree_index(starts, i):
    """The working tree's line index of pasted-back line index I: a `branch` site's 8 lines all
    name the statement they stand for."""
    shift = 0
    for p in starts:
        if i < p:
            break
        if i < p + BRANCH_LINES:
            return p - shift
        shift += BRANCH_LINES - 1
    return i - shift


def paste_back(rel, text, spec, out, read=None, forms=None):
    """(rc, text with every FORM written back, the number of calls written back, the pasted-back
    index of the first line of each `branch` site, {branch form: those indices, in order})."""
    forms = FORMS if forms is None else forms
    sites = 0
    for name, want in sorted(spec['forms'].items()):
        form = forms[name]
        if form.get('kind') == 'branch':
            continue
        if form['cast'] in text:
            out('%s: FAILED: %d call(s) still spelled %r: a site the rewrite missed'
                % (rel, text.count(form['cast']), form['cast']))
            return 1, text, sites, [], {}
        found = 0
        pieces = []
        at = 0
        while True:
            hit = text.find(form['direct'], at)
            if hit < 0:
                break
            args_start = hit + len(form['direct'])
            close = closing_paren(text, args_start)
            if close < 0:
                out('%s:%d: FAILED: %s( has no closing parenthesis' % (rel, line_of(text, hit), name))
                return 1, text, sites, [], {}
            args = text[args_start:close]
            if form['suffix'] is not None:
                if not args.endswith(form['suffix']):
                    out('%s:%d: FAILED: %s(%s) does not end its arguments with %r'
                        % (rel, line_of(text, hit), name, args, form['suffix']))
                    return 1, text, sites, [], {}
                args = args[:-len(form['suffix'])]
            pieces.append(text[at:hit])
            pieces.append(form['cast'] + args + ')')
            sites += 1
            at = close + 1
            found += 1
        pieces.append(text[at:])
        text = ''.join(pieces)
        if found != want:
            out('%s: FAILED: %d call(s) of %s, expected %d' % (rel, found, name, want))
            return 1, text, sites, [], {}
    rc, text, found, starts, by_form = paste_branches(rel, text, spec, out, read, forms)
    return rc, text, sites + found, starts, by_form


def lists_none(rel, lines, masked, spec, out, forms=None):
    """0 when no FORM the file does not list is spelled outside the MASKED line indices (the added
    lines dropped or found at their place), else 1; an `elsewhere` FORM is not looked for."""
    forms = FORMS if forms is None else forms
    rest = '\n'.join(line for i, line in enumerate(lines) if i not in masked)
    for name in forms:
        if name not in spec['forms'] and not forms[name].get('elsewhere') and forms[name]['direct'] in rest:
            out('%s: FAILED: a call of %s in a file that lists none' % (rel, name))
            return 1
    return 0


def pair_sites(rel, old_text, pasted, spec, out, read=None, forms=None, by_form=None):
    """(rc, [(name, base index, pasted-back index)] sorted by base line): the n-th written-back call
    of a FORM with the n-th cast call of it at BASE (0-based line indices); a `branch` site is its
    own n-th expansion's Player line (BY_FORM: each expansion's first pasted-back index). A base
    line a `branch` site claims may be claimed by no other site."""
    forms = FORMS if forms is None else forms
    by_form = by_form or {}
    pairs = []
    for name, want in sorted(spec['forms'].items()):
        cast = cast_of(name, forms[name], read, out)
        if cast is None:
            return 1, pairs
        olds = [line_of(old_text, i) - 1 for i in find_all(old_text, cast)]
        if len(olds) != want:
            out('%s: FAILED: %d cast call(s) of %s at the base, the spec lists %d' % (rel, len(olds), name, want))
            return 1, pairs
        if forms[name].get('kind') == 'branch':
            news = [start + BRANCH_PLAYER for start in by_form.get(name, [])]
        else:
            news = [line_of(pasted, i) - 1 for i in find_all(pasted, cast)]
        pairs += [(name, o, n) for o, n in zip(olds, news)]
    claims = {}
    for name, o, n in pairs:
        claims.setdefault(o, []).append(name)
    for o, names in sorted(claims.items()):
        if len(names) > 1 and any(forms[name].get('kind') == 'branch' for name in names):
            out('%s: FAILED: base line %d is claimed by %d sites (%s): a branch site claims its line alone'
                % (rel, o + 1, len(names), ', '.join(names)))
            return 1, pairs
    return 0, sorted(pairs, key=lambda p: p[1])


def block_at(lines, block):
    """Every index where BLOCK's lines stand in LINES, together and in order."""
    return [i for i in range(len(lines) - len(block) + 1) if lines[i:i + len(block)] == block]


def entry_label(added):
    block = added.split('\n')
    return repr(added) if len(block) == 1 else '%r (a block of %d lines)' % (block[0], len(block))


def place_entries(rel, old_lines, windows, spec, out):
    """(rc, {anchor index: block lines}, {base index: changed line}, the added entries outside every
    window, the changed entries outside every window). An entry is inside a window when the base
    line it names stands in that window's base text."""
    added_in, changed_in, added_out, changed_out = {}, {}, [], []
    rc = 0
    entries = [('added', a, base) for a, base in spec['added']] + \
              [('changed', new, base) for new, base in spec.get('changed', [])]
    for kind, line, base in entries:
        label = entry_label(line) if kind == 'added' else repr(line)
        at = set()
        twice = False
        for name, b, lo, hi in windows:
            hits = [j for j in range(lo, hi + 1) if old_lines[j] == base]
            if len(hits) > 1:
                out('%s: FAILED: the base line %r that the %s line %s names stands %d time(s) in the window '
                    'around the %s site at :%d, expected once' % (rel, base, kind, label, len(hits), name, b + 1))
                twice = True
            at.update(hits)
        if twice:
            rc = 1
            continue
        if len(at) > 1:
            out('%s: FAILED: the base line %r that the %s line %s names stands in windows at lines %s, '
                'expected one' % (rel, base, kind, label, ', '.join(':%d' % (j + 1) for j in sorted(at))))
            rc = 1
            continue
        if not at:
            (added_out if kind == 'added' else changed_out).append((line, base))
            continue
        j = at.pop()
        table = added_in if kind == 'added' else changed_in
        if j in table:
            out('%s: FAILED: two %s entries name the base line %r: list them as one' % (rel, kind, base))
            rc = 1
        table[j] = line.split('\n') if kind == 'added' else line
    return rc, added_in, changed_in, added_out, changed_out


def walk_window(old_lines, lines, b, t, lo, hi, added, changed):
    """(the first difference as (base index, tree index) or None, {anchor index: tree index of the
    first line of the block dropped below it}, the base indices whose changed line was written
    back), aligning outward from the site at base index B and tree index T."""
    dropped, written = {}, set()

    def same(i, j):
        if not 0 <= i < len(lines):
            return False
        if lines[i] == old_lines[j]:
            return True
        if changed.get(j) == lines[i]:
            written.add(j)
            return True
        return False

    diffs = []
    if not same(t, b):
        diffs.append((b, t))
    i = t
    for j in range(b - 1, lo - 1, -1):
        i -= 1
        block = added.get(j)
        if block and i - len(block) + 1 >= 0 and lines[i - len(block) + 1:i + 1] == block:
            dropped[j] = i - len(block) + 1
            i -= len(block)
        if not same(i, j):
            diffs.append((j, i))
            break
    i = t
    for j in range(b + 1, hi + 1):
        i += 1
        block = added.get(j - 1)
        if block and lines[i:i + len(block)] == block:
            dropped[j - 1] = i
            i += len(block)
        if not same(i, j):
            diffs.append((j, i))
            break
    else:
        block = added.get(hi)
        if block and lines[i + 1:i + 1 + len(block)] == block:
            dropped[hi] = i + 1
    return (min(diffs) if diffs else None), dropped, written


def check_outside(rel, old_lines, lines, added_out, changed_out, out, tree=lambda i: i):
    """(0 when every entry outside the windows stands at its place, else 1, the pasted-back indices
    of the added lines found there); TREE maps a pasted-back index to the working tree's."""
    at_place = set()
    for added, after in added_out:
        block = added.split('\n')
        label = entry_label(added)
        hits = block_at(lines, block)
        if len(hits) != 1:
            out('%s: FAILED: the added line %s stands %d time(s), expected once' % (rel, label, len(hits)))
            return 1, at_place
        m = old_lines.count(after)
        if m != 1:
            out('%s: FAILED: the base line %r that %s follows stands %d time(s) at the base, expected once'
                % (rel, after, label, m))
            return 1, at_place
        if hits[0] == 0 or lines[hits[0] - 1] != after:
            out('%s:%d: FAILED: the added line %s does not stand directly below %r'
                % (rel, tree(hits[0]) + 1, label, after))
            return 1, at_place
        at_place.update(range(hits[0], hits[0] + len(block)))
    for new, base in changed_out:
        n = lines.count(new)
        if n != 1:
            out('%s: FAILED: the changed line %r stands %d time(s), expected once' % (rel, new, n))
            return 1, at_place
        m = old_lines.count(base)
        if m != 1 or base in lines:
            out('%s: FAILED: the base line %r that %r replaced stands %d time(s) at the base and %d in the '
                'working tree, expected once and none' % (rel, base, new, m, lines.count(base)))
            return 1, at_place
    return 0, at_place


def prove(rel, old_text, new_text, spec, out, window=WINDOW, read=None, forms=None):
    """(rc, [(name, base line, new line, window)]): rc 0 when the window around every site pastes
    back to the base's byte for byte and every listed entry stands at its place. READ reads a file
    a `branch` FORM names; FORMS replaces the module's."""
    forms = FORMS if forms is None else forms
    rc, pasted, sites, starts, by_form = paste_back(rel, new_text, spec, out, read, forms)
    if rc:
        return 1, []
    rc, pairs = pair_sites(rel, old_text, pasted, spec, out, read, forms, by_form)
    if rc:
        return 1, []

    def tree(i):
        return tree_index(starts, i)

    old_lines = old_text.split('\n')
    lines = pasted.split('\n')
    own = dict(spec.get('window', {}))
    for name, b, t in pairs:
        own.pop(b + 1, None)
    if own:
        out('%s: FAILED: a window is set for base line(s) %s, which hold no site'
            % (rel, ', '.join(':%d' % n for n in sorted(own))))
        return 1, []
    windows = []
    for name, b, t in pairs:
        k = spec.get('window', {}).get(b + 1, window)
        windows.append((name, b, t, max(0, b - k), min(len(old_lines) - 1, b + k), k))
    rc, added_in, changed_in, added_out, changed_out = place_entries(
        rel, old_lines, [(n, b, lo, hi) for n, b, t, lo, hi, k in windows], spec, out)
    if rc:
        return 1, []
    masked = set()
    for name, b, t, lo, hi, k in windows:
        added = {j: v for j, v in added_in.items() if lo <= j <= hi}
        changed = {j: v for j, v in changed_in.items() if lo <= j <= hi}
        diff, dropped, written = walk_window(old_lines, lines, b, t, lo, hi, added, changed)
        for j, start in dropped.items():
            masked.update(range(start, start + len(added[j])))
        where = 'in the window around the %s site at :%d -> :%d (%d line(s) each side)' % (
            name, b + 1, tree(t) + 1, k)
        if diff is not None:
            j, i = diff
            text = lines[i] if 0 <= i < len(lines) else '(end of file)'
            out('%s:%d: DIFFERS from the base at line %d %s:\n  base:        %s\n  pasted back: %s'
                % (rel, tree(i) + 1, j + 1, where, old_lines[j], text))
            rc = 1
            continue
        for j in sorted(set(added) - set(dropped)):
            out('%s: FAILED: the added line %s does not stand directly below %r %s'
                % (rel, entry_label('\n'.join(added[j])), old_lines[j], where))
            rc = 1
        for j in sorted(set(changed) - written):
            out('%s: FAILED: the changed line %r does not stand in place of %r %s'
                % (rel, changed[j], old_lines[j], where))
            rc = 1
    got, at_place = check_outside(rel, old_lines, lines, added_out, changed_out, out, tree)
    rc |= got
    if spec.get('declares'):
        if spec['forms']:
            out('%s: FAILED: a file that declares the overrides lists call sites' % rel)
            rc = 1
    else:
        rc |= lists_none(rel, lines, masked | at_place, spec, out, forms)
    if rc:
        return 1, []
    outside = '%d added line(s) and %d changed line(s) at their place outside every window' % (
        sum(len(a.split('\n')) for a, _ in added_out), len(changed_out))
    if not windows:
        out('%s: no call site; %s' % (rel, outside))
    else:
        out('%s: IDENTICAL to the base, byte for byte, in the windows around %d/%d call(s) pasted back, with %d added '
            'line(s) dropped and %d changed line(s) written back inside them; %s'
            % (rel, sites, sum(spec['forms'].values()), sum(len(v) for v in added_in.values()), len(changed_in),
               outside))
    return 0, [(name, b + 1, tree(t) + 1, k) for name, b, t, lo, hi, k in windows]


def verify(rel, old_text, new_text, spec, out, window=WINDOW, read=None, forms=None):
    """0 when the window around every site pastes back to the base's byte for byte, else 1."""
    return prove(rel, old_text, new_text, spec, out, window=window, read=read, forms=forms)[0]


def check(root, base, out=print):
    rc = 0
    total = 0

    def read(rel):
        with open(os.path.join(root, *rel.split('/')), encoding='utf-8', newline='') as fh:
            return fh.read()

    for rel, spec in FILES.items():
        new_text = read(rel)
        try:
            old_text = subprocess.run(['git', '-C', root, 'show', '%s:%s' % (base, rel)], capture_output=True,
                                      check=True).stdout.decode('utf-8')
        except (OSError, subprocess.CalledProcessError) as e:
            out('%s: FAILED: cannot read it at %s from git: %s' % (rel, base, e))
            rc = 1
            continue
        got, sites = prove(rel, old_text, new_text, spec, out, read=read)
        rc |= got
        for name, old_line, new_line, k in sorted(sites, key=lambda s: s[2]):
            out('  %s:%d -> :%d  %s  (window %d)' % (rel, old_line, new_line, name, k))
            total += 1
    out('cast_verbatim: %s (%d call site(s) in %d file(s), %d line(s) each side of a site)'
        % ('OK' if rc == 0 else 'FAILED', total, len(FILES), WINDOW))
    return rc


SELF_OLD = '''void Unit::Proc(uint32 id, SpellEntry const* dummySpell)
{
    if (cooldown && GetTypeId() == TYPEID_PLAYER && ((Player*)this)->HasSpellCooldown(id))
    {
        return;
    }
    if (((Player*)this)->HasSpellCooldown(Pick(id, 2)))
    {
        return;
    }
    if (cooldown && GetTypeId() == TYPEID_PLAYER)
    {
        ((Player*)this)->GetSpellCooldownMgr().AddSpellCooldown(dummySpell->ID, 0, time(NULL) + cooldown);
    }
    ((Player*)this)->RemoveSpellCooldown(id);
}
'''

SELF_NEW = '''void Unit::Proc(uint32 id, SpellEntry const* dummySpell)
{
    if (cooldown && GetTypeId() == TYPEID_PLAYER && m_spellCooldownMgr.HasSpellCooldown(id, time(NULL)))
    {
        return;
    }
    if (m_spellCooldownMgr.HasSpellCooldown(Pick(id, 2), time(NULL)))
    {
        return;
    }
    if (cooldown && GetTypeId() == TYPEID_PLAYER)
    {
        m_spellCooldownMgr.AddSpellCooldown(dummySpell->ID, 0, time(NULL) + cooldown);
    }
    ((Player*)this)->RemoveSpellCooldown(id);
    int added = 1;
}
'''

SELF_SPEC = {'forms': {'HasSpellCooldown': 2, 'AddSpellCooldown': 1},
             'added': [('    int added = 1;', '    ((Player*)this)->RemoveSpellCooldown(id);')]}

SELF_DECL_OLD = '''#include "A.h"
#include "B.h"

Unit::Unit() :
    a(1),
    b(2)
{
}

class Unit
{
    public:
        int m_a;
    protected:
        int m_b;
};
'''

SELF_DECL_NEW = '''#include "A.h"
#include "N.h"
#include "B.h"

Unit::Unit() :
    a(1),
    n(),
    b(2)
{
}

class Unit
{
    public:
        int m_a;
    protected:
        int m_b;
        int m_n;
};
'''

SELF_DECL_SPEC = {'forms': {}, 'added': [('#include "N.h"', '#include "A.h"'), ('    n(),', '    a(1),'),
                                         ('        int m_n;', '        int m_b;')]}


SELF_BRANCH_OLD = '''int32 Unit::Roll(WeaponAttackType attType) const
{
    int32 dodge = 10;
    // the expertise
    if (GetTypeId() == TYPEID_PLAYER)
    {
        dodge -= int32(((Player*)this)->GetExpertise(attType) * 100);
    }
    else
    {
        dodge -= GetTotalAuraModifier(SPELL_AURA_MOD_EXPERTISE) * 25;
    }
    return dodge;
}

class Unit
{
    public:
        int32 Roll(WeaponAttackType attType) const;
};
'''

SELF_BRANCH_NEW = '''int32 Unit::Roll(WeaponAttackType attType) const
{
    int32 dodge = 10;
    // the attacker's expertise
    dodge -= GetRollExpertise(attType);
    return dodge;
}

class Unit
{
    public:
        int32 Roll(WeaponAttackType attType) const;
        /**
         * The expertise.
         */
        virtual int32 GetRollExpertise(WeaponAttackType /*attType*/) const { return GetTotalAuraModifier(SPELL_AURA_MOD_EXPERTISE) * 25; }
};
'''

SELF_BRANCH_FILES = {
    'Player.h': '        int32 GetRollExpertise(WeaponAttackType attType) const override '
                '{ return int32(GetExpertise(attType) * 100); }\n',
}

SELF_BRANCH_FORMS = {
    'GetRollExpertise': {'kind': 'branch', 'direct': 'GetRollExpertise(', 'guard': 'if (GetTypeId() == TYPEID_PLAYER)',
                         'player': 'Player.h', 'unit': 'Unit.h',
                         'renames': [('GetExpertise(', '((Player*)this)->GetExpertise(')]},
}

SELF_BRANCH_SPEC = {'forms': {'GetRollExpertise': 1},
                    'added': [('''        /**
         * The expertise.
         */
        virtual int32 GetRollExpertise(WeaponAttackType /*attType*/) const { return GetTotalAuraModifier(SPELL_AURA_MOD_EXPERTISE) * 25; }''',
                               '        int32 Roll(WeaponAttackType attType) const;')],
                    'changed': [("    // the attacker's expertise", '    // the expertise')]}


def two_branches(player, unit, sites):
    """(base text, working tree text, files, forms, spec) of a function holding one branch site per
    SITES entry (a form name, in order), with the Player overrides returning PLAYER[name] and the
    Unit defaults UNIT[name]; the defaults stand in their own file, as on the tree."""
    old, new = ['int32 Unit::Roll(WeaponAttackType attType) const', '{', '    int32 dodge = 10;'], []
    new += old
    forms, spec, files = {}, {'forms': {}, 'added': []}, {'Player.h': '', 'Unit.h': ''}
    for name in sites:
        cast = player[name].replace('GetExpertise(', '((Player*)this)->GetExpertise(')
        old += ['    if (GetTypeId() == TYPEID_PLAYER)', '    {', '        dodge -= %s;' % cast, '    }', '    else',
                '    {', '        dodge -= %s;' % unit[name], '    }']
        new.append('    dodge -= %s(attType);' % name)
        spec['forms'][name] = spec['forms'].get(name, 0) + 1
    for name in sorted(player):
        forms[name] = {'kind': 'branch', 'direct': name + '(', 'guard': 'if (GetTypeId() == TYPEID_PLAYER)',
                       'player': 'Player.h', 'unit': 'Unit.h',
                       'renames': [('GetExpertise(', '((Player*)this)->GetExpertise(')]}
        files['Player.h'] += '        int32 %s(WeaponAttackType attType) const override { return %s; }\n' % (
            name, player[name])
        files['Unit.h'] += '        virtual int32 %s(WeaponAttackType) const { return %s; }\n' % (name, unit[name])
    tail = ['    return dodge;', '}', '']
    return '\n'.join(old + tail), '\n'.join(new + tail), files, forms, spec


def generated(sites):
    """(base text, working tree text, {label: 1-based base line}) of a generated function: filler
    lines `    int fNNN = NNN;`, each SITES entry (label, gap of filler lines before it, kind) placed
    after its gap, then 40 filler lines; a kind is 'test' (an `if` testing the cooldown, its
    return below) or 'add' (the guard above an added cooldown)."""
    old, new, at = ['void Unit::Proc(uint32 id)', '{'], ['void Unit::Proc(uint32 id)', '{'], {}
    n = [0]

    def filler(count):
        for _ in range(count):
            n[0] += 1
            line = '    int f%03d = %d;' % (n[0], n[0])
            old.append(line)
            new.append(line)

    for label, gap, kind in sites:
        filler(gap)
        if kind == 'test':
            old.append('    if (cooldown && ((Player*)this)->HasSpellCooldown(id + %d))' % n[0])
            new.append('    if (cooldown && m_spellCooldownMgr.HasSpellCooldown(id + %d, time(NULL)))' % n[0])
            at[label] = len(old)
            body = ['    {', '        return;', '    }']
        else:
            body = ['    if (cooldown)', '    {']
            old += body
            new += body
            old.append('        ((Player*)this)->GetSpellCooldownMgr().AddSpellCooldown(id, 0, %d);' % n[0])
            new.append('        m_spellCooldownMgr.AddSpellCooldown(id, 0, %d);' % n[0])
            at[label] = len(old)
            body = ['    }']
        old += body
        new += body
    filler(40)
    old.append('}')
    new.append('}')
    return '\n'.join(old) + '\n', '\n'.join(new) + '\n', at


# A at line 33, B at 69 and C at 76 (their windows overlap), D at 120.
GEN_OLD, GEN_NEW, GEN_AT = generated([('A', 30, 'test'), ('B', 30, 'add'), ('C', 5, 'test'), ('D', 40, 'test')])
GEN_SPEC = {'forms': {'HasSpellCooldown': 3, 'AddSpellCooldown': 1}, 'added': []}


def gen_line(n):
    """The generated file's line N."""
    return GEN_OLD.split('\n')[n - 1]


def self_test():
    failures = []

    def run(label, want_rc, needle='', swap=None, new_text=SELF_NEW, old_text=SELF_OLD, spec=SELF_SPEC):
        swaps = swap if isinstance(swap, list) else [swap] if swap else []
        for a, b in swaps:
            if a not in new_text:
                failures.append('%s: the mutation %r matches nothing' % (label, a))
                print('self-test: %-72s %s' % (label, 'FAIL'))
                return
            new_text = new_text.replace(a, b, 1)
        got = []
        rc = verify('fixture', old_text, new_text, spec, got.append, window=WINDOW)
        text = '\n'.join(got)
        ok = rc == want_rc and needle in text
        print('self-test: %-72s %s' % (label, 'PASS' if ok else 'FAIL'))
        if not ok:
            failures.append('%s: rc %d (want %d)\n%s' % (label, rc, want_rc, text))

    run('the rewritten calls paste back byte for byte (3 calls)', 0,
        'IDENTICAL to the base, byte for byte, in the windows around 3/3 call(s) pasted back, with 1 added line(s) '
        'dropped')
    run('an argument swapped fails', 1, 'DIFFERS from the base at line 13',
        swap=('AddSpellCooldown(dummySpell->ID, 0,', 'AddSpellCooldown(0, dummySpell->ID,'))
    run('an argument changed fails', 1, 'DIFFERS from the base at line 3',
        swap=('HasSpellCooldown(id, time(NULL))', 'HasSpellCooldown(id + 1, time(NULL))'))
    run('a guard dropped fails', 1, 'DIFFERS from the base at line 3',
        swap=('cooldown && GetTypeId() == TYPEID_PLAYER && m_spell', 'cooldown && m_spell'))
    run('a guard moved off an added call fails', 1, 'DIFFERS from the base at line 11',
        swap=('    if (cooldown && GetTypeId() == TYPEID_PLAYER)\n', '    if (cooldown)\n'))
    run('a site left as a cast fails', 1, 'still spelled',
        swap=('m_spellCooldownMgr.HasSpellCooldown(id, time(NULL))', '((Player*)this)->HasSpellCooldown(id)'))
    run('a site added fails', 1, 'call(s) of HasSpellCooldown, expected 2',
        swap=('    int added = 1;', '    int added = 1;\n    m_spellCooldownMgr.HasSpellCooldown(7, time(NULL));'))
    run('a missing clock argument fails', 1, 'does not end its arguments with',
        swap=('HasSpellCooldown(id, time(NULL))', 'HasSpellCooldown(id)'))
    run('another clock fails', 1, 'does not end its arguments with',
        swap=('HasSpellCooldown(id, time(NULL))', 'HasSpellCooldown(id, now)'))
    run('the clock appended to a form without one fails', 1, 'DIFFERS from the base at line 13',
        swap=('time(NULL) + cooldown);', 'time(NULL) + cooldown, time(NULL));'))
    run('an added line that is not listed fails', 1, 'DIFFERS from the base',
        spec={'forms': SELF_SPEC['forms'], 'added': []})
    run('a listed added line that is missing fails', 1, 'does not stand directly below',
        swap=('    int added = 1;\n', ''))
    run('a listed added line that stands twice fails', 1, 'DIFFERS from the base at line 16',
        swap=('    int added = 1;\n', '    int added = 1;\n    int added = 1;\n'))
    run('a call in a file that lists none of its form fails', 1, 'in a file that lists none',
        spec={'forms': {'HasSpellCooldown': 2}, 'added': SELF_SPEC['added']})
    run('a changed line inside a window fails', 1, 'fixture:1: DIFFERS from the base at line 1 ',
        swap=('void Unit::Proc(uint32 id,', 'void Unit::Proc(uint32 id2,'))
    run('a call with no closing parenthesis fails', 1, 'has no closing parenthesis',
        swap=('AddSpellCooldown(dummySpell->ID, 0, time(NULL) + cooldown);\n    }\n'
              '    ((Player*)this)->RemoveSpellCooldown(id);\n    int added = 1;\n}\n',
              'AddSpellCooldown(dummySpell->ID\n    ((Player*)this)->RemoveSpellCooldown(id);\n'
              '    int added = 1;\n'))
    run('an added line moved fails', 1, 'DIFFERS from the base at line 15',
        swap=('    ((Player*)this)->RemoveSpellCooldown(id);\n    int added = 1;\n',
              '    int added = 1;\n    ((Player*)this)->RemoveSpellCooldown(id);\n'))
    run('an added line whose base line stands twice in a window fails', 1,
        "stands 2 time(s) in the window around the AddSpellCooldown site at :13",
        old_text=SELF_OLD + '    ((Player*)this)->RemoveSpellCooldown(id);\n',
        new_text=SELF_NEW + '    ((Player*)this)->RemoveSpellCooldown(id);\n')
    run('added lines outside every window stand at their place (include, initialiser, member)', 0,
        'fixture: no call site; 3 added line(s) and 0 changed line(s) at their place outside every window',
        new_text=SELF_DECL_NEW, old_text=SELF_DECL_OLD, spec=SELF_DECL_SPEC)
    declared = '        bool Cooling() const { return m_spellCooldownMgr.HasSpellCooldown(1, time(NULL)); }'
    run('a form spelled in a listed added line is no call site', 0, '4 added line(s)',
        new_text=SELF_DECL_NEW.replace('        int m_a;\n', '        int m_a;\n' + declared + '\n'),
        old_text=SELF_DECL_OLD,
        spec=dict(SELF_DECL_SPEC, added=SELF_DECL_SPEC['added'] + [(declared, '        int m_a;')]))
    run('a form spelled in an unlisted line of a file that lists none fails', 1, 'in a file that lists none',
        new_text=SELF_DECL_NEW.replace('        int m_a;\n', '        int m_a;\n' + declared + '\n'),
        old_text=SELF_DECL_OLD, spec=SELF_DECL_SPEC)
    run('... and passes in a file that declares the overrides', 0, '3 added line(s) and 0 changed line(s)',
        new_text=SELF_DECL_NEW.replace('        int m_a;\n', '        int m_a;\n' + declared + '\n'),
        old_text=SELF_DECL_OLD, spec=dict(SELF_DECL_SPEC, declares=True))
    run('a listed added line missing from a file that declares the overrides fails', 1,
        "the added line '        int m_n;' stands 0 time(s)",
        new_text=SELF_DECL_NEW.replace('        int m_n;\n', ''), old_text=SELF_DECL_OLD,
        spec=dict(SELF_DECL_SPEC, declares=True))
    run('a file that declares the overrides and lists a call site fails', 1, 'declares the overrides lists call sites',
        new_text=SELF_DECL_NEW, old_text=SELF_DECL_OLD,
        spec=dict(SELF_DECL_SPEC, declares=True, forms={'HasSpellCooldown': 0}))
    run('an added line outside every window whose base line stands twice at the base fails', 1,
        'the base line \'#include "A.h"\' that \'#include "N.h"\' follows stands 2 time(s) at the base',
        new_text=SELF_DECL_NEW + '#include "A.h"\n', old_text=SELF_DECL_OLD + '#include "A.h"\n', spec=SELF_DECL_SPEC)
    run('an added line outside every window that stands twice fails', 1,
        'the added line \'#include "N.h"\' stands 2 time(s), expected once',
        new_text=SELF_DECL_NEW.replace('#include "N.h"\n', '#include "N.h"\n#include "N.h"\n'),
        old_text=SELF_DECL_OLD, spec=SELF_DECL_SPEC)
    decl_changed = dict(SELF_DECL_SPEC, changed=[('        int m_a2;', '        int m_a;')])
    run('a changed line outside every window is written back', 0,
        '3 added line(s) and 1 changed line(s) at their place',
        new_text=SELF_DECL_NEW.replace('        int m_a;\n', '        int m_a2;\n'),
        old_text=SELF_DECL_OLD, spec=decl_changed)
    run('a changed line outside every window that is missing fails', 1,
        "the changed line '        int m_a2;' stands 0 time(s)",
        new_text=SELF_DECL_NEW, old_text=SELF_DECL_OLD, spec=decl_changed)
    run('a changed line outside every window that stands twice fails', 1,
        "the changed line '        int m_a2;' stands 2 time(s)",
        new_text=SELF_DECL_NEW.replace('        int m_a;\n', '        int m_a2;\n        int m_a2;\n'),
        old_text=SELF_DECL_OLD, spec=decl_changed)
    run('a changed line outside every window whose base line still stands fails', 1,
        'stands 1 time(s) at the base and 1 in the working tree, expected once and none',
        new_text=SELF_DECL_NEW.replace('        int m_a;\n', '        int m_a;\n        int m_a2;\n'),
        old_text=SELF_DECL_OLD, spec=decl_changed)
    run('an added include moved fails', 1, 'fixture:3: FAILED: the added line \'#include "N.h"\'',
        new_text=SELF_DECL_NEW.replace('#include "N.h"\n#include "B.h"\n', '#include "B.h"\n#include "N.h"\n'),
        old_text=SELF_DECL_OLD, spec=SELF_DECL_SPEC)
    run('an added initialiser moved fails', 1, "fixture:6: FAILED: the added line '    n(),'",
        new_text=SELF_DECL_NEW.replace('    a(1),\n    n(),\n', '    n(),\n    a(1),\n'),
        old_text=SELF_DECL_OLD, spec=SELF_DECL_SPEC)
    run('an added member moved into another section fails', 1, "fixture:16: FAILED: the added line '        int m_n;'",
        new_text=SELF_DECL_NEW.replace('        int m_b;\n        int m_n;\n', '        int m_b;\n').replace(
            '        int m_a;\n', '        int m_a;\n        int m_n;\n'),
        old_text=SELF_DECL_OLD, spec=SELF_DECL_SPEC)

    a, b, c, d = GEN_AT['A'], GEN_AT['B'], GEN_AT['C'], GEN_AT['D']
    k = 11  # the measured WINDOW: the rows at its edge pin it

    def gen(label, want_rc, needle='', swap=None, spec=GEN_SPEC, new_text=GEN_NEW, old_text=GEN_OLD):
        run(label, want_rc, needle, swap, new_text, old_text, spec)

    def edit(n):
        """A swap that changes the generated file's line N."""
        return (gen_line(n) + '\n', gen_line(n).replace(';', ' + 1;') + '\n')

    def insert_below(n, text):
        return (gen_line(n) + '\n', gen_line(n) + '\n' + text + '\n')

    gen('the generated file pastes back around each site (4 calls, two windows overlapping)', 0,
        'in the windows around 4/4 call(s) pasted back')
    gen('an edit outside every window passes unlisted', 0, 'IDENTICAL',
        swap=[edit(3), edit(a + k + 2), edit(d + k + 3)])
    gen('an edit K+1 = 12 lines above a site passes', 0, 'IDENTICAL', swap=edit(a - k - 1))
    gen('an edit K = 11 lines above a site fails', 1,
        'fixture:%d: DIFFERS from the base at line %d in the window around the HasSpellCooldown site at :%d'
        % (a - k, a - k, a), swap=edit(a - k))
    gen('an edit K+1 = 12 lines below a site passes', 0, 'IDENTICAL', swap=edit(d + k + 1))
    gen('an edit K = 11 lines below a site fails', 1, 'DIFFERS from the base at line %d' % (d + k), swap=edit(d + k))
    gen('an edit where two windows overlap is named by the first', 1,
        'DIFFERS from the base at line %d in the window around the AddSpellCooldown site at :%d' % (b + 4, b),
        swap=edit(b + 4))
    gen('an edit where two windows overlap is named by the second too', 1,
        'DIFFERS from the base at line %d in the window around the HasSpellCooldown site at :%d' % (b + 4, c),
        swap=edit(b + 4))
    gen('a guard changed inside the window fails', 1, 'DIFFERS from the base at line %d' % (b - 2),
        swap=('    if (cooldown)\n', '    if (cooldown || id)\n'))
    gen('a site whose argument changed fails', 1, 'DIFFERS from the base at line %d' % c,
        swap=('id + 65, time(NULL)', 'id + 1, time(NULL)'))
    gen('a site lost fails', 1, 'call(s) of HasSpellCooldown, expected 3',
        swap=('m_spellCooldownMgr.HasSpellCooldown(', 'HasSpellCooldownAt('))
    gen('a site duplicated fails', 1, 'call(s) of HasSpellCooldown, expected 3',
        swap=insert_below(d + k + 5, '    m_spellCooldownMgr.HasSpellCooldown(id, time(NULL));'))
    gen('two sites of a form swapped fail', 1, 'DIFFERS from the base at line %d' % a,
        swap=[('HasSpellCooldown(id + 30,', 'HasSpellCooldown(id + XX,'),
              ('HasSpellCooldown(id + 65,', 'HasSpellCooldown(id + 30,'),
              ('HasSpellCooldown(id + XX,', 'HasSpellCooldown(id + 65,')])
    gen('an unlisted added line inside a window fails', 1,
        'fixture:%d: DIFFERS from the base at line %d' % (a + 6, a + 6), swap=insert_below(a + 5, '    int x = 0;'))
    gen('an unlisted added line outside every window passes', 0, 'IDENTICAL',
        swap=insert_below(a + k + 3, '    int x = 0;'))
    listed = dict(GEN_SPEC, added=[('    int x = 0;', gen_line(a + 5))])
    gen('a listed added line inside a window drops out', 0, 'with 1 added line(s) dropped',
        swap=insert_below(a + 5, '    int x = 0;'), spec=listed)
    gen('a listed added line below the window\'s last line drops out', 0, 'with 1 added line(s) dropped',
        swap=insert_below(a + k, '    int x = 0;'), spec=dict(GEN_SPEC, added=[('    int x = 0;', gen_line(a + k))]))
    gen('a listed added line elsewhere in its window fails', 1, 'DIFFERS from the base at line %d' % (a + 4),
        swap=insert_below(a + 3, '    int x = 0;'), spec=listed)
    gen('a listed added line outside its window fails', 1, 'does not stand directly below %r' % gen_line(a + 5),
        swap=insert_below(a + k + 3, '    int x = 0;'), spec=listed)
    gen('an anchor standing twice in a window\'s base text fails', 1,
        "the base line '    {' that the added line '    int x = 0;' names stands 2 time(s) in the window "
        "around the AddSpellCooldown site at :%d" % b, spec=dict(GEN_SPEC, added=[('    int x = 0;', '    {')]))
    gen('an anchor standing in two windows at different lines fails', 1, 'stands in windows at lines :%d, :%d, :%d'
        % (a + 2, c + 2, d + 2), spec=dict(GEN_SPEC, added=[('    int x = 0;', '        return;')]))
    base_twice = GEN_OLD.replace(gen_line(d + k + 8), '    int dup = 0;').replace(gen_line(a + 5), '    int dup = 0;')
    new_twice = GEN_NEW.replace(gen_line(d + k + 8), '    int dup = 0;').replace(gen_line(a + 5), '    int dup = 0;')
    gen('an anchor standing once in its window and again outside every window passes', 0,
        'with 1 added line(s) dropped', swap=('    int dup = 0;\n', '    int dup = 0;\n    int x = 0;\n'),
        spec=dict(GEN_SPEC, added=[('    int x = 0;', '    int dup = 0;')]), new_text=new_twice, old_text=base_twice)
    block = '    {\n        return;\n    }'
    gen('a multi-line block of common lines inside a window drops out', 0, 'with 3 added line(s) dropped',
        swap=insert_below(a + 7, block), spec=dict(GEN_SPEC, added=[(block, gen_line(a + 7))]))
    gen('a multi-line block missing a line fails', 1, 'DIFFERS from the base at line %d' % (a + 8),
        swap=insert_below(a + 7, '    {\n        return;'), spec=dict(GEN_SPEC, added=[(block, gen_line(a + 7))]))
    changed = dict(GEN_SPEC, changed=[(gen_line(a - 3).replace(';', ' + 1;'), gen_line(a - 3))])
    gen('a listed changed line inside a window is written back', 0, '1 changed line(s) written back inside them',
        swap=edit(a - 3), spec=changed)
    gen('a listed changed line that is missing fails', 1, 'does not stand in place of %r' % gen_line(a - 3),
        spec=changed)
    gen('a listed changed line on a site is written back', 0, '1 changed line(s) written back inside them',
        swap=('id + 30, time(NULL)))', 'id + 30, time(NULL)))  // the test'),
        spec=dict(GEN_SPEC, changed=[(gen_line(a) + '  // the test', gen_line(a))]))
    gen('a window set for a site reaches past K', 1, 'DIFFERS from the base at line %d' % (a - k - 5),
        swap=edit(a - k - 5), spec=dict(GEN_SPEC, window={a: k + 5}))
    gen('a window set for a base line with no site fails', 1, 'which hold no site',
        spec=dict(GEN_SPEC, window={a + 1: k + 5}))
    gen('a cast count at the base that is not the listed count fails', 1,
        'cast call(s) of HasSpellCooldown at the base',
        old_text=GEN_OLD.replace('((Player*)this)->HasSpellCooldown(id + 30', 'HasSpellCooldown(id + 30'))
    gen('a listed added line above a site inside a window drops out', 0, 'with 1 added line(s) dropped',
        swap=insert_below(a - 5, '    int x = 0;'), spec=dict(GEN_SPEC, added=[('    int x = 0;', gen_line(a - 5))]))
    gen('a working tree that ends inside a window fails', 1,
        'DIFFERS from the base at line %d in the window around the HasSpellCooldown site at :%d' % (d + 4, d),
        new_text='\n'.join(GEN_NEW.split('\n')[:d + 3]))
    run('... naming the end of the file', 1, 'pasted back: (end of file)',
        new_text='\n'.join(GEN_NEW.split('\n')[:d + 3]), old_text=GEN_OLD, spec=GEN_SPEC)

    one_old, one_new, one_at = generated([('A', 30, 'test'), ('D', 40, 'test')])
    far = one_at['D'] + k + 8
    for n in (one_at['A'] + 5, far):
        line = one_old.split('\n')[n - 1] + '\n'
        one_old, one_new = one_old.replace(line, '    int dup = 0;\n'), one_new.replace(line, '    int dup = 0;\n')
    spelled = '    m_spellCooldownMgr.AddSpellCooldown(id, 0, 0);'
    one_spec = {'forms': {'HasSpellCooldown': 2}, 'added': [(spelled, '    int dup = 0;')]}
    run('a form spelled in a listed added line inside a window is no call site', 0, 'with 1 added line(s) dropped',
        swap=('    int dup = 0;\n', '    int dup = 0;\n' + spelled + '\n'),
        new_text=one_new, old_text=one_old, spec=one_spec)
    run('an unlisted second copy of that line outside every window fails', 1,
        'a call of AddSpellCooldown in a file that lists none',
        new_text=one_new.replace('    int dup = 0;\n', '    int dup = 0;\n' + spelled + '\n'),
        old_text=one_old, spec=one_spec)

    def branch(label, want_rc, needle, swap=None, files=None, spec=SELF_BRANCH_SPEC):
        new_text = SELF_BRANCH_NEW
        files = dict(SELF_BRANCH_FILES, **(files or {}))
        if swap:
            a, b = swap
            if a not in new_text:
                failures.append('%s: the mutation %r matches nothing' % (label, a))
                print('self-test: %-72s %s' % (label, 'FAIL'))
                return
            new_text = new_text.replace(a, b, 1)
        files.setdefault('Unit.h', new_text)
        got = []
        rc = verify('fixture', SELF_BRANCH_OLD, new_text, spec, got.append, read=files.__getitem__,
                    forms=SELF_BRANCH_FORMS)
        text = '\n'.join(got)
        ok = rc == want_rc and needle in text
        print('self-test: %-72s %s' % (label, 'PASS' if ok else 'FAIL'))
        if not ok:
            failures.append('%s: rc %d (want %d)\n%s' % (label, rc, want_rc, text))

    branch('a branch statement pastes back as its two branches, block and line too', 0,
           'around 1/1 call(s) pasted back, with 0 added line(s) dropped and 1 changed line(s) written back inside '
           'them; 4 added line(s) and 0 changed line(s) at their place outside every window')
    branch('a changed default body fails', 1, 'DIFFERS from the base at line 11',
           files={'Unit.h': SELF_BRANCH_NEW.replace('EXPERTISE) * 25; }', 'EXPERTISE) * 100; }')})
    branch('a changed override body fails', 1, '0 cast call(s) of GetRollExpertise at the base',
           files={'Player.h': SELF_BRANCH_FILES['Player.h'].replace('* 100)', '* 100.0f)')})
    branch('an override that is missing fails', 1, '0 one-line definition(s) of GetRollExpertise',
           files={'Player.h': ''})
    branch('an override that no longer calls the renamed member fails', 1, 'which holds no',
           files={'Player.h': SELF_BRANCH_FILES['Player.h'].replace('GetExpertise(attType)', 'attType')})
    branch('another operator fails', 1, 'DIFFERS from the base at line 7',
           swap=('dodge -= GetRollExpertise', 'dodge += GetRollExpertise'))
    branch('another argument fails', 1, "does not pass the override's parameters",
           swap=('GetRollExpertise(attType);', 'GetRollExpertise(BASE_ATTACK);'))
    branch('a call that is not a whole statement fails', 1, 'does not stand as a whole statement',
           swap=('    dodge -= GetRollExpertise(attType);', '    dodge -= 1 + GetRollExpertise(attType);'))
    branch('a branch left standing fails', 1, 'still spelled',
           swap=('    return dodge;\n}', '    dodge -= int32(((Player*)this)->GetExpertise(attType) * 100);\n'
                 '    return dodge;\n}'))
    block = SELF_BRANCH_SPEC['added'][0][0] + '\n'
    branch('a block moved fails', 1, 'does not stand as a whole statement',
           swap=('        int32 Roll(WeaponAttackType attType) const;\n' + block,
                 block + '        int32 Roll(WeaponAttackType attType) const;\n'))
    branch('a block missing a line fails', 1, 'does not stand as a whole statement',
           swap=('         * The expertise.\n', ''))
    branch('a changed line that is not listed fails', 1, 'DIFFERS from the base at line 4',
           spec=dict(SELF_BRANCH_SPEC, changed=[]))
    branch('a listed changed line that is missing fails', 1, 'the changed line',
           swap=("    // the attacker's expertise", '    // the expertise'))
    branch('a line below a branch site is named by its working tree line', 1,
           'fixture:6: DIFFERS from the base at line 13', swap=('    return dodge;', '    return dodge + 1;'))
    branch('a branch statement is named by its working tree line', 1,
           "fixture:5: DIFFERS from the base at line 7 in the window around the GetRollExpertise site at :7 -> :5",
           swap=('dodge -= GetRollExpertise', 'dodge += GetRollExpertise'))
    got, sites = prove('fixture', SELF_BRANCH_OLD, SELF_BRANCH_NEW, SELF_BRANCH_SPEC, lambda _: None,
                       read=dict(SELF_BRANCH_FILES, **{'Unit.h': SELF_BRANCH_NEW}).__getitem__, forms=SELF_BRANCH_FORMS)
    want = [('GetRollExpertise', 7, 5, WINDOW)]
    label = 'a branch site is listed with its working tree line'
    print('self-test: %-72s %s' % (label, 'PASS' if sites == want else 'FAIL'))
    if sites != want:
        failures.append('prove: got %r, expected %r' % (sites, want))
    two_old = SELF_BRANCH_OLD.replace('    return dodge;\n', SELF_BRANCH_OLD.split('    int32 dodge = 10;\n')[1].split(
        '    return dodge;\n')[0].replace('// the expertise', '// again') + '    return dodge;\n', 1)
    two_new = SELF_BRANCH_NEW.replace('    return dodge;\n', '    // again\n    dodge -= GetRollExpertise(attType);\n'
                                      '    return dodge;\n', 1)
    two_spec = dict(SELF_BRANCH_SPEC, forms={'GetRollExpertise': 2})
    two_files = dict(SELF_BRANCH_FILES, **{'Unit.h': two_new})
    got = []
    rc = verify('fixture', two_old, two_new.replace('    return dodge;', '    return dodge + 1;'), two_spec, got.append,
                read=two_files.__getitem__, forms=SELF_BRANCH_FORMS)
    label = 'a line below two branch sites is named by its working tree line'
    ok = rc == 1 and 'fixture:8: DIFFERS from the base at line 22' in '\n'.join(got)
    print('self-test: %-72s %s' % (label, 'PASS' if ok else 'FAIL'))
    if not ok:
        failures.append('%s: rc %d\n%s' % (label, rc, '\n'.join(got)))

    def branches(label, want_rc, needle, player, unit, sites, edit=None):
        old_text, new_text, files, forms, spec = two_branches(player, unit, sites)
        if edit:
            new_text = new_text.replace(edit[0], edit[1], 1)
        got = []
        rc = verify('fixture', old_text, new_text, spec, got.append, read=files.__getitem__, forms=forms)
        text = '\n'.join(got)
        ok = rc == want_rc and needle in text
        print('self-test: %-72s %s' % (label, 'PASS' if ok else 'FAIL'))
        if not ok:
            failures.append('%s: rc %d (want %d)\n%s' % (label, rc, want_rc, text))

    roll, spell = 'int32(GetExpertise(attType) * 100)', 'int32(GetExpertise(attType) * 100.0f)'
    aura = 'GetTotalAuraModifier(SPELL_AURA_MOD_EXPERTISE) * 25'
    four = ['ExpRoll', 'ExpRoll', 'ExpSpell', 'ExpSpell']
    branches('two branch forms with their own expressions paste back', 0, 'around 4/4 call(s) pasted back',
             {'ExpRoll': roll, 'ExpSpell': spell}, {'ExpRoll': aura, 'ExpSpell': aura}, four)
    branches('two branch forms whose overrides return the same expression fail', 1, 'return the same expression',
             {'ExpRoll': roll, 'ExpSpell': roll}, {'ExpRoll': aura, 'ExpSpell': aura}, four)
    branches('... and the second form\'s sites are compared as its own', 1,
             'DIFFERS from the base at line 22 in the window around the ExpSpell site at :22 -> :6',
             {'ExpRoll': roll, 'ExpSpell': spell}, {'ExpRoll': aura, 'ExpSpell': aura}, four,
             edit=('    dodge -= ExpSpell(attType);', '    dodge += ExpSpell(attType);'))
    plus = {'ExpRoll': roll, 'ExpRollPlus': roll + ' + 0'}, {'ExpRoll': aura, 'ExpRollPlus': aura}
    claim_old = two_branches(plus[0], plus[1], ['ExpRoll', 'ExpRollPlus'])[0]
    _, claim_new, files, forms, spec = two_branches(plus[0], plus[1], ['ExpRoll', 'ExpRoll', 'ExpRollPlus'])
    got = []
    rc = verify('fixture', claim_old, claim_new, spec, got.append, read=files.__getitem__, forms=forms)
    label = 'a base line claimed by a branch site and another site fails'
    ok = rc == 1 and 'base line 14 is claimed by 2 sites (ExpRoll, ExpRollPlus)' in '\n'.join(got)
    print('self-test: %-72s %s' % (label, 'PASS' if ok else 'FAIL'))
    if not ok:
        failures.append('%s: rc %d\n%s' % (label, rc, '\n'.join(got)))
    cast_line = '    dodge -= ' + roll.replace('GetExpertise(', '((Player*)this)->GetExpertise(') + ';'
    base = '\n'.join(['    int32 a = 0;', cast_line, '    int32 b = 0;']) + '\n'
    pasted = '\n'.join([cast_line, '    int32 a = 0;', '    int32 b = 0;', '    int32 c = 0;']) + '\n'
    got = []
    _, _, files, forms, _ = two_branches({'ExpRoll': roll}, {'ExpRoll': aura}, ['ExpRoll'])
    rc, pairs = pair_sites('fixture', base, pasted, {'forms': {'ExpRoll': 1}, 'added': []}, got.append,
                           read=files.__getitem__, forms=forms, by_form={'ExpRoll': [1]})
    label = 'a branch site is paired by its own expansion, not by its expression'
    ok = rc == 0 and pairs == [('ExpRoll', 1, 1 + BRANCH_PLAYER)]
    print('self-test: %-72s %s' % (label, 'PASS' if ok else 'FAIL'))
    if not ok:
        failures.append('%s: rc %d, pairs %r\n%s' % (label, rc, pairs, '\n'.join(got)))

    gen_one_old, gen_one_new, gen_one_at = generated([('A', 30, 'test')])
    copy = '    bool cooling = m_spellCooldownMgr.HasSpellCooldown(1, time(NULL));'
    anchor = gen_one_old.split('\n')[gen_one_at['A'] + 4]
    far = gen_one_old.split('\n')[gen_one_at['A'] + 30]
    one_listed = {'forms': {'HasSpellCooldown': 1}, 'added': [(copy, anchor)]}
    run('a listed form spelled in a listed added line is a call site, and a second copy fails', 1,
        'call(s) of HasSpellCooldown, expected 1',
        swap=[(anchor + '\n', anchor + '\n' + copy + '\n'), (far + '\n', far + '\n' + copy + '\n')],
        new_text=gen_one_new, old_text=gen_one_old, spec=one_listed)
    branch('a second copy of a listed block spelling a branch form is no declaration', 1,
           'does not stand as a whole statement', swap=('};\n', '};\n' + SELF_BRANCH_SPEC['added'][0][0] + '\n'),
           files={'Unit.h': SELF_BRANCH_NEW})
    elsewhere_forms = dict(SELF_BRANCH_FORMS, Elsewhere={'direct': 'return dodge', 'cast': 'unused', 'suffix': None})
    for flag, want_rc, label in ((True, 0, 'an `elsewhere` FORM spelled in a file that lists none passes'),
                                 (False, 1, '... and fails without the flag')):
        forms = dict(elsewhere_forms)
        forms['Elsewhere'] = dict(forms['Elsewhere'], elsewhere=flag)
        got = []
        rc = verify('fixture', SELF_BRANCH_OLD, SELF_BRANCH_NEW, SELF_BRANCH_SPEC, got.append,
                    read=dict(SELF_BRANCH_FILES, **{'Unit.h': SELF_BRANCH_NEW}).__getitem__, forms=forms)
        ok = rc == want_rc and (flag or 'a call of Elsewhere in a file that lists none' in '\n'.join(got))
        print('self-test: %-72s %s' % (label, 'PASS' if ok else 'FAIL'))
        if not ok:
            failures.append('%s: rc %d\n%s' % (label, rc, '\n'.join(got)))

    got, sites = prove('fixture', SELF_OLD, SELF_NEW, SELF_SPEC, lambda _: None)
    want = [('HasSpellCooldown', 3, 3, k), ('HasSpellCooldown', 7, 7, k), ('AddSpellCooldown', 13, 13, k)]
    label = 'the sites are listed with their base and new lines and windows'
    print('self-test: %-72s %s' % (label, 'PASS' if sites == want else 'FAIL'))
    if sites != want:
        failures.append('prove: got %r, expected %r' % (sites, want))

    for f in failures:
        print('FAILED: ' + f)
    print('self-test: %s' % ('OK' if not failures else 'FAILED'))
    return 1 if failures else 0


def main(argv):
    ap = argparse.ArgumentParser(description='The verbatim proof for call sites that stopped casting this.')
    here = os.path.dirname(os.path.abspath(__file__))
    ap.add_argument('--root', default=os.path.abspath(os.path.join(here, '..', '..', '..')))
    ap.add_argument('--base', default=BASE)
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument('--check', action='store_true')
    g.add_argument('--self-test', action='store_true')
    args = ap.parse_args(argv[1:])
    if args.self_test:
        return self_test()
    return check(os.path.abspath(args.root), args.base)


if __name__ == '__main__':
    sys.exit(main(sys.argv))
