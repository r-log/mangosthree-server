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
     found there; a BYVALUE entry (below) is a CHANGED entry once its transformation is checked;
  5. an entry whose base line stands in no window is checked at its place only: an added entry
     stands once in the working tree, directly below its base line, which stands once in the file
     at BASE; a changed line stands once in the working tree, its base line once at BASE and
     nowhere in the working tree;
  6. requires no direct spelling of a FORM the file does not list, outside the added lines it
     dropped or found at their place; a FORM whose direct spelling also stands in code that never
     cast (`elsewhere`: True for every file, or the list of the files where it so stands) is only
     looked for in the files that list it; a file that `declares` the
     overrides (Player.h, whose own code calls its methods directly and never cast) lists no FORM
     and is not searched for one: only its entries are checked, at their place.
A window set for a base line that holds no site fails.

A BYVALUE entry (the line as it reads now, listed with the base line it replaced) is the known
transformation of a getter override that returns a guid by value: its base line declares a getter
whose return type, the line's first word, is `ObjectGuid const&` (once on the line, with no
`override`), and the line must read exactly as the base line with that `ObjectGuid const&` read as
`ObjectGuid` and ` override final` written before the body (before ` {`, or before the closing `;`
of a declaration), nothing else changed; any other difference fails, naming both lines. Accepted,
it is checked as a CHANGED entry. Player.h:2098, the selection guid, is the one listed.

A `moved` FORM is a whole statement `<direct>(<args>);` standing where a block of lines stood that
moved into the body of `header` in the file `to` (the lines between the `{` directly below that
header, which stands once there, and the next `}` at column 0); its arguments must be the header's
parameter names, in order. The block is written back in the statement's place: each body line at
the statement's indentation instead of the body's four spaces, and each EDIT's new spelling, which
must stand once in the body, read as its base spelling; its site is the line holding its cast
spelling, which must stand nowhere in the working tree, and its window must reach past the block's
last line. A body line that is not empty must start with four spaces, or it fails, named with its
line in `to`. Any other difference in the body, a body line lost or gained, or the statement
standing anywhere else, is a difference in that window. A `moved` FORM with a `dropped` line stood
for one more line, directly above the block: that line holds the cast spelling and stands nowhere
in the working tree; it is written back above the block at the statement's indentation, and it is
the site. A `dropped` line that does not hold the cast spelling, or that still stands, fails; a
changed one is a difference in the window. A moved body that a later seam changed is pinned as
text, like a CHANGED line: a `moved` FORM with a `body` (the body's lines as they read now) and a
`base_body` (the base lines it stands for) takes no EDITs; the body in `to` must read exactly as
`body`, or it fails, named with its first differing line in `to`, and `base_body` is written back
in the statement's place instead of the body, so a wrong `base_body` is a difference in the window.

A `reported` FORM is a whole statement `<direct><fact>{<args>});` standing inside its kept type guard
where a test of the player's group and its writes stood, which are now the body of a callback in
the file `to` (the lines between the `{` directly below the callback's head, `lambda`, which stands
once there, and the `};` at the head's indentation). The body is written back in the statement's
place: each line at the statement's indentation instead of the body's, its receiver (`owner->`)
read as the receiver the site cast and each `fact.<field>` as the statement's argument in that
place, so a wrong flag or a body line changed is a difference in the window; the body must use the
receiver and every field. A `dropped` line stands above the body as for a `moved` FORM. A
statement listed in its file's `folded` (by its order among its FORM's statements) stood for the
body's opening test folded into its guard: only the lines inside that test are written back, and
the guard two lines above must be listed as a CHANGED line whose base reads
`if ((<the guard's condition>) && <the test>)`, or `if (<the guard's condition> && <the test>)`
when the condition holds no `||`, no `?` and no assignment (a `=` that is not part of `==`, `!=`,
`<=` or `>=`). Its site is the line holding its cast spelling, which
must stand nowhere in the working tree, and each FORM's sites must be found in the pasted-back text
as often as the spec lists them. The receiver is the FORM's: the pet-owner arms read `owner->` as
`((Player*)owner)->`. A FORM whose cast spelling stands inside a longer one of another FORM the file
lists (the pet aura's flag line holds the owner stat's spelling) does not claim the lines holding
the longer one, at BASE or pasted back. A file's `deleted` names the header of each member
deleted whole: it stands once at BASE with a body and nowhere in the working tree, and the cast
spellings inside that body at BASE are no sites. An entry of `deleted` that is a list of lines names
a block deleted inside a member: it stands once at BASE and nowhere pasted back, and the cast
spellings inside it at BASE are no sites.

An `inlined` FORM is the mirror of a `moved` one: a block of lines standing where one statement
`<cast><args>);` stood, the call of another class's method whose body the block now is. The block
opens with the line `direct` and must read, line for line, as the base body of `header` in the file
`from` (read at BASE: the lines between the `{` directly below that header, which stands once there,
and the next `}` at column 0), each line at the block's indentation instead of the body's four
spaces, with the `dropped` line, which must stand once in that body, left out, and each `receiver`
deleted, and each EDIT's base spelling, which must stand once in that body, read as its new
spelling, which may hold more lines than one. It is written back as the one statement, its
arguments the header's parameter names in order, and that line is the site. A block line that
does not read as its body line fails, named with both (`m_motion->` written for `MotionState()` is
one). The cast spelling and the dropped line must stand nowhere in the file, and the method's name
followed by `(` nowhere in the files `gone` names, which held its declaration and its definition.

A `local` FORM is a pointer local turned into a flag: the line `direct`, `bool <local> = <test>;`,
stands where the pointer line `cast`, `Player* <local> = (<test>) ? (Player*)this : NULL;` or
`Player* <local> = <test> ? static_cast<Player*>(this): NULL;`, stood, and each FORM it lists is a
call that went through the pointer, a plain FORM whose cast spelling is its direct spelling called
through `<local>->`, or a call on a part of the player renamed (its cast spelling starts with
`<local>->`), so pasting it back gives the call its receiver again. The flag line is written back
as the pointer line, which is the site and must stand nowhere in the working tree. A local with a
`declared` pair was declared and assigned in its type test: `direct` is `<local> = true;`, standing
where `cast`, `<local> = static_cast<Player *>(this);`, stood, and three lines above it stands
`bool <local> = false;` where `Player* <local> = NULL;` stood; both are written back, and the
assignment is the site. A flag line that does not read as the pointer line's test under the same
name (a `!=`, another name, a parenthesis dropped), or a declaration that is not three lines above
its assignment, fails, and so does a listed FORM that is not listed in the file or does not call
through the local. From the flag line to the member's end (the next `}` at column 0) the local may
stand only as a test, `<local> &&`, `<local> ?` or `if (<local>)`, or in a `//` comment; any other
use, a `<local>->` left over among them, fails, named with its line. A call that passed the pointer
itself as an argument (`SetClientControl(player, 1)`, now `SetClientControl(this, 1)`) is a CHANGED
line.

A moved body and an inlined block are checked for the includes they depend on, not only for their
bytes: a line of either that calls an overloaded standard math function (cos, sin, tan, acos, asin,
atan, atan2, sqrt, pow, fabs, abs, floor, ceil, round, fmod, exp, log), unqualified or through
`std::` or `::`, with an argument that is neither a cast to double nor
a double literal fails, named with its line and the call: the overload such a call binds depends on
the declarations its file's includes bring in. Comments and string literals are not read.

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
below :435; UnitAura.cpp:383 and Unit.cpp:4418 on the type test of their own line. The reputation
sites in the Shattered Sun pendants' arms rely on the type return opening each arm:
UnitAuraProcHandler.cpp:995, :1038, :1063 and :1089 stand 6 lines below theirs (:989, :1032, :1057
and :1083), and :1002, :1045, :1070 and :1096, the Scryers' test below the Aldor's, 13, so those
four set their own window in their file's spec. The cooldown sites: UnitAuraProcHandler.cpp:1380
stands 6 lines below its type return (:1374), :4514 and :4604 2 below their type tests (:4512 and
:4602); UnitDynObject.cpp:224 7 below its type test (:217) and :280 7 below its (:273); two set
their own window: UnitAuraProcHandler.cpp:3242, in Lightning Overload's arm, 47 below the type
return opening it (:3195), and Unit.cpp:4406, ClearInCombat's `else` arm, 12 below the creature
test it is the other branch of (:4394). The combo-point sites stand within 3 lines of their
warrior and type tests: Unit.cpp:5533 2 below :5531, :6074 2 below :6072, and :6115 2 below
:6113 and 3 below the Overpower case label (:6112); their direct spelling of ClearComboPoints
carries its leading space, since Unit.cpp:6044 calls it on another player. The rage sites rely on
DealDamage's type and rage test (Unit.cpp:943): :960 stands 17 below it and :975 32 below it (each
11 below its case label, :949 and :964), so both set their own window; their direct spelling of
RewardRage carries its leading space, since Unit.cpp:917 and :1326 call it on the victim. The
kill-credit sites stand within 4 lines of their type tests: Unit.cpp:1459 4 below the critter and
type test at :1455, UnitAuraProcHandler.cpp:464 2 below the kill-flag and type test at :462, and
:4389 on the type test of its own line; KilledMonsterCredit is another name, which the direct
spelling of KilledMonster does not match. The faction site, Unit.cpp:6599, stands 2 lines below
its type test (:6597); its direct spelling of setFactionForRace carries its leading space, since
Unit.cpp:7334 calls it on the possessed player. The ghost speed site, UnitSpeed.cpp:240, is the
player arm of the creature test at :228, 12 lines above it (the corpse test at :238 2), so it sets
its own window; its direct spelling of InBattleGround carries the opening parenthesis of the
config lookup, since Unit.cpp:1246 calls it on the victim. The proc one-off sites set their own
windows: UnitAuraProcHandler.cpp:980, the say in the Aura of Madness arm, stands 44 lines below
the type return opening the arm (:936; the case label :934 46), :1010, the selection in the
Shattered Sun pendant's arm, 21 below its type return (:989), its window overlapping :1002's, and
:4840, the raid member, 17 below the type and caster test at :4823; the direct spelling of Say
carries its leading space, so MonsterSay, another name, does not match it. The talent rank site,
UnitSpellBonus.cpp:102, stands 2 lines below its type and death knight test (:100). The rune
cooldown site, UnitAuraProcHandler.cpp:4383, stands on the line below its type and class test
(:4382), inside the window of the kill-credit site at :4389. The aura-state passive casts,
Unit.cpp:3327, a `moved` block of 17 lines on the line below its type test (:3326), set their own
window: 17 reaches the block's last line (16 below) and the type test's closing brace. The
own-session packet sites stand within 4 lines of their type tests: Unit.cpp:2364, :3193, :3599
and :6708 2 below theirs (:2362, :3191, :3597 and :6706), and :5970, the last line of a `moved`
block of 3, 4 below its own (:5966). The swing error report, Unit.cpp:626, the `dropped` line of a
`moved` block of 12 lines, stands under no type test and sets its own window: 13 reaches one line
past the block's last line (12 below). The account security comparison, UnitVisibility.cpp:186,
spells its direct call twice on one line, on this unit and on the observer, where the base cast
this unit once, so no FORM counts it: it is a CHANGED line with no FORM, in no window (the
nearest, :203's, opens at :192), checked at its place. The position and moving sites stand within
2 lines of their type tests: Unit.cpp:550, the pending commit, 2 below :548, :7014, the end of a
spline, 2 below the `else` arm's test at :7012 (the boarded test 9 above), and :2211, the
auto-repeat movement test, on the type test of its own line. The feign-death flag write,
UnitSpeed.cpp:538, the player arm of the creature test at :532, stands 6 lines below it; its
direct spelling names Unit's own member, which the cast reached through Player. The client-control
sites in the fear and confuse states stand within 3 lines of their type tests: UnitSpeed.cpp:360,
a fear's take, 3 below :357, :431, its return, 2 below :429, and the confuse's take and return,
:463 3 below :460 and :504 2 below :502; their direct spelling of SetClientControl carries its
leading space, since Unit.cpp calls it seven times through Player pointers. The group update sites
stand within 4 lines of their type tests: Unit.cpp:2886, :4928 and :4960 and UnitPower.cpp:168,
:223 and :261 4 below theirs (:2882, :4924, :4956, :164, :219 and :257), :4904 2 below the guard
its group test is folded into (:4902), and :6302, the dropped line above a block of 5, 2 below its
own (:6300); UnitPower.cpp:294, the fourth cast of the stat form there, stands in the deleted
ApplyMaxPowerMod. The pet-owner arms stand 2 lines below their folded owner guards (Unit.cpp:2897,
:4939, :4971, :6005 and :6317; UnitPower.cpp:179, :234 and :272), 8 below the pet arm's test
(:6005 10, below the early return of a pet that is not controlled); UnitPower.cpp:272 sets a window
of 6, since 7 below it stands the comment of the deleted ApplyMaxPowerMod, and the pet arm it
stands in (:264) is inside :261's window. One CHANGED owner guard stands in the windows of several
sites: a CHANGED line is written back at each line of the windows where its base line stands.
The near-teleport, Unit.cpp:6519, a `moved` block of one line, stands 2 lines below its type test
(:6517); the direct spelling of TeleportNear does not stand inside NearTeleportTo, the name of the
member that calls it. The knockback, Unit.cpp:6629, an inlined block of 7 lines, stands 2 lines
below its type test (:6627); written back, the block is one line, so a window of 1 already reaches
past it. The damage credit, Unit.cpp:1057, the `dropped` line above a `moved` block of 13 lines,
stands 2 lines below its type test (:1055) and sets its own window: 14 reaches one line past the
block's last line (13 below); its cast spelling is the whole dropped line, since the cast alone
still stands elsewhere in the file, and its two achievement edits name their criteria type, so
each new spelling stands once in the body. The spell damage's player flag, Unit.cpp:4611, a `local`
FORM, sets its own window: its pointer's uses stand 4 to 103 lines below it (the mastery test :4615,
the armor specialization test :4624 and its call :4627, the combo points :4633, the combo target
:4714), and 104 reaches one line past the last. The armor specialization call stands 3 lines below
its test, the combo points and the combo target on the test of their own line; the direct spelling
of GetComboTargetGuid carries the comparison before it, since Unit.cpp:6042 calls it on another
player. The possess sites are three `local` FORMs that set their own windows: in TakePossessOf
(SpellEntry const*, ...), Unit.cpp:7114, the pointer's uses stand 21 to 36 lines below it (the
camera, the client control and the forced update :7135-:7137, the possess bar :7150), and 37 reaches
one line past the last; in TakePossessOf(Unit*), Unit.cpp:7176, the assignment, they stand 41 to 58
below it (:7217-:7219, :7234), and 59 reaches one past; in ResetControlState, Unit.cpp:7257, the
assignment, they stand 9 to 66 below it (the first release :7266-:7271, the second :7300-:7304,
the pet's removal :7312, the pet bar :7323), and 67 reaches one past. Each call stands within 11
lines of its own test. The direct spelling of SetClientControl in Unit.cpp is the fear and confuse
states' own, so each FORM is `elsewhere` in the other's file only; the two calls that passed the
pointer as their target
(:7271, :7304) are CHANGED lines. The direct spelling of IsTaxiFlying carries the `(!` before it,
since Unit.cpp calls it on other units. The dead pet case of ResetControlState's creature arm
(:7342-:7353), whose `player->RemovePet` ran only on a NULL pointer, is a deleted block.

A `branch` site writes 8 lines for its one; every line number printed is the working tree's.

What fails: a changed, swapped or dropped argument; a flag line that does not read as its pointer
line's test, or a use of the flag that is not a test; a changed, moved or dropped guard, or any other
changed line, inside a window; an inlined block line that does not read as its base body line; a
math call of a moved body or an inlined block whose argument does not spell its type; a site lost,
added or still cast; an added line inside a window that is not listed; a listed line that does not
stand at its place; a base line an entry names that stands twice in a window's base text, or, an
added entry's, in two windows at different lines. What passes: any edit outside every window,
unlisted. Windows may overlap; each is checked on its own.

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

# The by-value transformation: the return type a guid getter override read as, the one it reads as
# now, and what stands before its body.
BYVALUE_FROM = 'ObjectGuid const& '
BYVALUE_TO = 'ObjectGuid '
BYVALUE_FINAL = ' override final'

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
    'GetReputationRank': {'direct': 'GetReputationRank(',
                          'cast': '((Player*)this)->GetReputationRank(',
                          'suffix': None},
    'AddSpellAndCategoryCooldowns': {'direct': 'AddSpellAndCategoryCooldowns(',
                                     'cast': '((Player*)this)->AddSpellAndCategoryCooldowns(',
                                     'suffix': None},
    'SendCooldownEvent': {'direct': 'SendCooldownEvent(',
                          'cast': '((Player*)this)->SendCooldownEvent(',
                          'suffix': None},
    'RemoveSpellCooldown': {'direct': 'RemoveSpellCooldown(',
                            'cast': '((Player*)this)->RemoveSpellCooldown(',
                            'suffix': None},
    'RemoveSpellCategoryCooldown': {'direct': 'RemoveSpellCategoryCooldown(',
                                    'cast': '((Player*)this)->RemoveSpellCategoryCooldown(',
                                    'suffix': None},
    'UpdatePotionCooldown': {'direct': 'UpdatePotionCooldown(',
                             'cast': '((Player*)this)->UpdatePotionCooldown(',
                             'suffix': None},
    'AddComboPoints': {'direct': 'AddComboPoints(',
                       'cast': '((Player*)this)->AddComboPoints(',
                       'suffix': None},
    'ClearComboPoints': {'direct': ' ClearComboPoints(',
                         'cast': ' ((Player*)this)->ClearComboPoints(',
                         'suffix': None},
    'RewardRage': {'direct': ' RewardRage(',
                   'cast': ' ((Player*)this)->RewardRage(',
                   'suffix': None},
    'KilledMonster': {'direct': 'KilledMonster(',
                      'cast': '((Player*)this)->KilledMonster(',
                      'suffix': None},
    'isHonorOrXPTarget': {'direct': 'isHonorOrXPTarget(',
                          'cast': '((Player*)this)->isHonorOrXPTarget(',
                          'suffix': None},
    'setFactionForRace': {'direct': ' setFactionForRace(',
                          'cast': ' ((Player*)this)->setFactionForRace(',
                          'suffix': None},
    'InBattleGround': {'direct': '(InBattleGround(',
                       'cast': '(((Player*)this)->InBattleGround(',
                       'suffix': None},
    'Say': {'direct': ' Say(',
            'cast': ' ((Player*)this)->Say(',
            'suffix': None},
    'GetSelectionGuid': {'direct': 'GetSelectionGuid(',
                         'cast': '((Player*)this)->GetSelectionGuid(',
                         'suffix': None},
    'GetNextRandomRaidMember': {'direct': 'GetNextRandomRaidMember(',
                                'cast': '((Player*)this)->GetNextRandomRaidMember(',
                                'suffix': None},
    'GetKnownTalentRankById': {'direct': 'GetKnownTalentRankById(',
                               'cast': '((Player*)this)->GetTalentMgr().GetKnownTalentRankById(',
                               'suffix': None},
    'IsBaseRuneSlotsOnCooldown': {'direct': 'IsBaseRuneSlotsOnCooldown(',
                                  'cast': '((Player*)this)->GetRuneMgr().IsBaseRuneSlotsOnCooldown(',
                                  'suffix': None},
    'SendAttackSwingCancelAttack': {'direct': 'SendAttackSwingCancelAttack(',
                                    'cast': '((Player*)this)->SendAttackSwingCancelAttack(',
                                    'suffix': None},
    'SendAutoRepeatCancel': {'direct': 'SendAutoRepeatCancel(',
                             'cast': '((Player*)this)->SendAutoRepeatCancel(',
                             'suffix': None},
    'SendPetGUIDs': {'direct': 'SendPetGUIDs(',
                     'cast': '((Player*)this)->SendPetGUIDs(',
                     'suffix': None},
    'SetPosition': {'direct': 'SetPosition(',
                    'cast': '((Player*)this)->SetPosition(',
                    'suffix': None},
    'isMoving': {'direct': 'isMoving(',
                 'cast': '((Player*)this)->isMoving(',
                 'suffix': None},
    'm_movementInfo.SetMovementFlags': {'direct': 'm_movementInfo.SetMovementFlags(',
                                        'cast': '((Player*)this)->m_movementInfo.SetMovementFlags(',
                                        'suffix': None},
    'SetClientControl': {'direct': ' SetClientControl(',
                         'cast': ' ((Player*)this)->SetClientControl(',
                         'suffix': None, 'elsewhere': ['src/game/Object/Unit.cpp']},
    'FitArmorSpecializationRules': {'direct': 'FitArmorSpecializationRules(',
                                    'cast': 'unitPlayer->FitArmorSpecializationRules(',
                                    'suffix': None},
    'GetComboPoints': {'direct': 'GetComboPoints(',
                       'cast': 'unitPlayer->GetComboPoints(',
                       'suffix': None},
    'GetComboTargetGuid': {'direct': '== GetComboTargetGuid(',
                           'cast': '== unitPlayer->GetComboTargetGuid(',
                           'suffix': None},
    'unitPlayer': {
        'kind': 'local', 'local': 'unitPlayer', 'direct': 'bool unitPlayer = GetTypeId() == TYPEID_PLAYER;',
        'cast': 'Player* unitPlayer = (GetTypeId() == TYPEID_PLAYER) ? (Player*)this : NULL;',
        'forms': ['FitArmorSpecializationRules', 'GetComboPoints', 'GetComboTargetGuid']},
    'PossessClientControl': {'direct': ' SetClientControl(',
                             'cast': ' player->SetClientControl(',
                             'suffix': None, 'elsewhere': ['src/game/Object/UnitSpeed.cpp']},
    'SendForcedObjectUpdate': {'direct': 'SendForcedObjectUpdate(',
                               'cast': 'player->SendForcedObjectUpdate(',
                               'suffix': None},
    'IsTaxiFlying': {'direct': '(!IsTaxiFlying(',
                     'cast': '(!player->IsTaxiFlying(',
                     'suffix': None},
    'PossessSpellInitialize': {'direct': 'PossessSpellInitialize(',
                               'cast': 'player->PossessSpellInitialize(',
                               'suffix': None},
    'RemovePet': {'direct': 'RemovePet(',
                  'cast': 'player->RemovePet(',
                  'suffix': None},
    'RemovePetActionBar': {'direct': 'RemovePetActionBar(',
                           'cast': 'player->RemovePetActionBar(',
                           'suffix': None},
    'SetCameraView': {'direct': 'SetCameraView(',
                      'cast': 'player->GetCamera().SetView(',
                      'suffix': None},
    'ResetCameraView': {'direct': 'ResetCameraView(',
                        'cast': 'player->GetCamera().ResetView(',
                        'suffix': None},
    'player': {
        'kind': 'local', 'local': 'player', 'direct': 'bool player = GetTypeId() == TYPEID_PLAYER;',
        'cast': 'Player* player = GetTypeId() == TYPEID_PLAYER ? static_cast<Player*>(this): NULL;',
        'forms': ['SetCameraView', 'PossessClientControl', 'SendForcedObjectUpdate', 'PossessSpellInitialize']},
    'playerAssigned': {
        'kind': 'local', 'local': 'player', 'direct': 'player = true;',
        'cast': 'player = static_cast<Player *>(this);', 'declared': ('bool player = false;', 'Player* player = NULL;'),
        'forms': ['SetCameraView', 'ResetCameraView', 'PossessClientControl', 'SendForcedObjectUpdate', 'IsTaxiFlying',
                  'PossessSpellInitialize', 'RemovePet', 'RemovePetActionBar']},
    'CastPassiveSpellsForAuraState': {
        'kind': 'moved', 'direct': 'CastPassiveSpellsForAuraState(', 'cast': '((Player*)this)->GetSpellMap()',
        'to': 'src/game/entities/player/spells/PlayerSpell.cpp',
        'header': 'void Player::CastPassiveSpellsForAuraState(AuraState flag)',
        'edits': [('((Player*)this)->GetSpellMap()', 'GetSpellMap()')]},
    'TeleportNear': {
        'kind': 'moved', 'direct': 'TeleportNear(', 'cast': '((Player*)this)->TeleportTo(',
        'to': 'src/game/entities/player/Player.cpp',
        'header': 'void Player::TeleportNear(float x, float y, float z, float orientation, bool casting)',
        'edits': [('((Player*)this)->TeleportTo(', 'TeleportTo(')]},
    'CreditDamageDealt': {
        'kind': 'moved', 'direct': 'CreditDamageDealt(', 'cast': 'Player* killer = ((Player*)this);',
        'dropped': 'Player* killer = ((Player*)this);',
        'to': 'src/game/entities/player/combat/PlayerCombat.cpp',
        'header': 'void Player::CreditDamageDealt(Unit* pVictim, uint32 damage)',
        'edits': [('killer->GetBattleGround()', 'GetBattleGround()'),
                  ('UpdatePlayerScore(killer, ', 'UpdatePlayerScore(this, '),
                  ('killer->UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_DAMAGE_DONE, ',
                   'UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_DAMAGE_DONE, '),
                  ('killer->UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_HIGHEST_HIT_DEALT, ',
                   'UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_HIGHEST_HIT_DEALT, ')]},
    'SendStandStateUpdate': {
        'kind': 'moved', 'direct': 'SendStandStateUpdate(', 'cast': '((Player*)this)->GetSession()->SendPacket(&data)',
        'to': 'src/game/entities/player/Player.cpp',
        'header': 'void Player::SendStandStateUpdate(uint8 state)',
        'body': ['    StandStateFact fact;', '    fact.state = state;',
                 '    ReportClientFact(m_clientCallbacks.standState, fact);'],
        'base_body': ['    WorldPacket data(SMSG_STANDSTATE_UPDATE, 1);', '    data << (uint8)state;',
                      '    ((Player*)this)->GetSession()->SendPacket(&data);']},
    'ReportSwingError': {
        'kind': 'moved', 'direct': 'ReportSwingError(', 'cast': '(GetTypeId() == TYPEID_PLAYER ? (Player*)this : NULL)',
        'dropped': 'Player* player = (GetTypeId() == TYPEID_PLAYER ? (Player*)this : NULL);',
        'to': 'src/game/entities/player/combat/PlayerCombat.cpp',
        'header': 'void Player::ReportSwingError(uint8 swingError)',
        'edits': [('player->LastSwingErrorMsg()', 'LastSwingErrorMsg()'),
                  ('if (player && swingError != ', 'if (swingError != '),
                  ('player->SendAttackSwingNotInRange()', 'SendAttackSwingNotInRange()'),
                  ('player->SendAttackSwingBadFacingAttack()', 'SendAttackSwingBadFacingAttack()'),
                  ('player->SwingErrorMsg(swingError)', 'SwingErrorMsg(swingError)')]},
    'ReportGroupStat': {
        'kind': 'reported', 'direct': 'ReportGroupFact(InstalledGroupCallbacks(m_groupCallbacks).stat, ',
        'cast': '((Player*)this)->SetGroupUpdateFlag(', 'fact': 'GroupStatFact', 'fields': ['flag'],
        'to': 'src/game/WorldHandlers/Group.h', 'lambda': '    callbacks.stat = [owner](auto const& fact)',
        'receiver': ('owner->', '((Player*)this)->')},
    'ReportGroupAura': {
        'kind': 'reported', 'direct': 'ReportGroupFact(InstalledGroupCallbacks(m_groupCallbacks).aura, ',
        'cast': 'Player* player = (Player*)this;', 'dropped': 'Player* player = (Player*)this;',
        'fact': 'GroupAuraFact', 'fields': ['flag', 'slot'],
        'to': 'src/game/WorldHandlers/Group.h', 'lambda': '    callbacks.aura = [owner](auto const& fact)',
        'receiver': ('owner->', 'player->')},
    'ReportOwnerGroupStat': {
        'kind': 'reported', 'direct': 'ReportGroupFact(InstalledGroupCallbacks(owner->m_groupCallbacks).stat, ',
        'cast': '((Player*)owner)->SetGroupUpdateFlag(', 'fact': 'GroupStatFact', 'fields': ['flag'],
        'to': 'src/game/WorldHandlers/Group.h', 'lambda': '    callbacks.stat = [owner](auto const& fact)',
        'receiver': ('owner->', '((Player*)owner)->')},
    'ReportPetGroupAura': {
        'kind': 'reported', 'direct': 'ReportGroupFact(InstalledGroupCallbacks(owner->m_groupCallbacks).petAura, ',
        'cast': '((Player*)owner)->SetGroupUpdateFlag(GROUP_UPDATE_FLAG_PET_AURAS)', 'fact': 'PetGroupAuraFact',
        'fields': ['flag', 'slot', 'pet'], 'to': 'src/game/WorldHandlers/Group.h',
        'lambda': '    callbacks.petAura = [owner](auto const& fact)', 'receiver': ('owner->', '((Player*)owner)->')},
    'SendKnockBack': {
        'kind': 'inlined', 'direct': 'Motion::KnockBackParams params;',
        'cast': '((Player*)this)->GetSession()->SendKnockBack(', 'from': 'src/game/WorldHandlers/MovementHandler.cpp',
        'header': 'void WorldSession::SendKnockBack(float angle, float horizontalSpeed, float verticalSpeed)',
        'dropped': 'Player* player = GetPlayer();', 'receiver': 'player->',
        'gone': ['src/game/Server/WorldSession.h', 'src/game/WorldHandlers/MovementHandler.cpp'],
        'edits': [('    params.directionX = cos(angle);',
                   '    // The direction is computed in double and narrowed to float.\n'
                   '    params.directionX = float(cos(double(angle)));'),
                  ('sin(angle)', 'float(sin(double(angle)))')]},
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

UNIT_H_REPUTATION = '''
    protected:
        /**
         * The rank this unit holds with a faction; the Shattered Sun pendants' proc picks its spell
         * by the Aldor's or the Scryers' rank.
         * @param faction_id the faction asked about
         * @return REP_NEUTRAL here; Player returns its rank by its reputation with that faction
         */
        virtual ReputationRank GetReputationRank(uint32 /*faction_id*/) const { return REP_NEUTRAL; }

    public:'''

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

UNIT_H_COOLDOWNS = '''
    protected:
        /**
         * Starts a spell's cooldown and its category's; AddGameObject calls it for an object whose
         * spell stays disabled while the object stands.
         * Does nothing here; Player stores both cooldowns in its manager.
         */
        virtual void AddSpellAndCategoryCooldowns(SpellEntry const* /*spellInfo*/, uint32 /*itemId*/,
                                                  Spell* /*spell*/ = NULL, bool /*infinityCooldown*/ = false) { }
        /**
         * Starts a spell's cooldown and its category's and reports the cooldown event; RemoveGameObject
         * calls it when an object whose spell stayed disabled goes.
         * Does nothing here; Player stores both cooldowns and reports the event to the callback its
         * session installed, and the session sends it.
         */
        virtual void SendCooldownEvent(SpellEntry const* /*spellInfo*/, uint32 /*itemId*/ = 0,
                                       Spell* /*spell*/ = NULL) { }
        /**
         * Ends a spell's cooldown; the Lightning Overload proc ends the cooldown of the spell it casts.
         * Does nothing here; Player removes it from its manager and, with update, tells its client.
         */
        virtual void RemoveSpellCooldown(uint32 /*spell_id*/, bool /*update*/ = false) { }
        /**
         * Ends the cooldown of every spell of a category; the Glyph of Ice Block, Sword and Board
         * and Freezing Fog procs end one.
         * Does nothing here; Player removes them from its manager and, with update, tells its client.
         */
        virtual void RemoveSpellCategoryCooldown(uint32 /*cat*/, bool /*update*/ = false) { }
        /**
         * Starts the cooldown of the potion used in combat; ClearInCombat calls it when combat ends.
         * Does nothing here; Player, out of combat, sends the cooldown event of the last potion it used
         * and forgets the potion.
         */
        virtual void UpdatePotionCooldown(Spell* /*spell*/ = NULL) { }'''

UNIT_H_COMBO_POINTS = '''        /**
         * Adds combo points on a target; ProcDamageAndSpellFor adds one to a warrior whose target
         * dodged, with the Overpower window it opens.
         * Does nothing here; Player adds them on its combo target, or moves them to a new one, and
         * sends them to its client.
         */
        virtual void AddComboPoints(Unit* /*target*/, int8 /*count*/) { }
        /**
         * Clears the combo points; ClearAllReactives and the end of the Overpower window clear a
         * warrior's.
         * Does nothing here; Player clears its combo points and its combo target and sends them to
         * its client.
         */
        virtual void ClearComboPoints() { }'''

UNIT_H_RAGE = '''        /**
         * Awards rage from a hit dealt or taken; DealDamage awards it to a rage user for its
         * main-hand and off-hand weapon hits.
         * Does nothing here; Player converts the damage, and for a hit it dealt the weapon speed
         * factor, into rage and adds it to its power.
         */
        virtual void RewardRage(uint32 /*damage*/, uint32 /*weaponSpeedHitFactor*/, bool /*attacker*/) { }'''

UNIT_H_KILL_CREDIT = '''        /**
         * Credits a kill of a creature to the quests and achievements that count it; JustKilledCreature
         * credits a player that killed a critter.
         * Does nothing here; Player credits the creature's entry and each of its kill credit entries.
         */
        virtual void KilledMonster(CreatureInfo const* /*cInfo*/, ObjectGuid /*guid*/) { }
        /**
         * @return whether a kill of this victim grants honor or experience, which the kill procs and
         * Improved Blood Presence require: false here; Player answers by its level against the
         * victim's and the victim's kind
         */
        virtual bool isHonorOrXPTarget(Unit* /*pVictim*/) const { return false; }'''

UNIT_H_FACTION_GHOST_SPEED = '''        /**
         * Sets the team and faction of a race; RestoreOriginalFaction returns a player to its own
         * when a faction override ends.
         * Does nothing here; Player sets its team and the faction template of the race.
         */
        virtual void setFactionForRace(uint8 /*race*/) { }
        /**
         * @return whether the unit is in a battleground, which picks the ghost run speed UpdateSpeed
         * applies to a dead player: false here; Player answers by its battleground instance
         */
        virtual bool InBattleGround() const { return false; }'''

UNIT_H_PROC_ONE_OFFS = '''        /**
         * Says a text in a language to the units in say range; the Aura of Madness proc has a player
         * say "This is Madness!".
         * Does nothing here; Player sends the say message to the players in its listen range.
         */
        virtual void Say(const std::string& /*text*/, const uint32 /*language*/) { }
        /**
         * @return the guid of the unit this unit has selected, which the Shattered Sun pendant's
         * proc strikes when there is no victim: an empty guid here; Player returns its selection
         */
        virtual ObjectGuid GetSelectionGuid() const { return ObjectGuid(); }
        /**
         * The raid member Prayer of Mending jumps to next.
         * @param radius the distance the member stands within
         * @return NULL here; Player returns a random other member of its group within the radius
         * that is not invisible and not hostile to it, or NULL
         */
        virtual Player* GetNextRandomRaidMember(float /*radius*/) { return NULL; }'''

UNIT_H_TALENT_RANK = '''        /**
         * The rank of a talent this unit knows; SpellBonusWithCoeffs raises a death knight's attack
         * power bonus by the rank of Impurity it knows.
         * @param talentId the talent asked about
         * @return NULL here, a unit that is not a player knows no talent; Player returns the spell of
         * the rank its active spec knows, or NULL
         */
        virtual SpellEntry const* GetKnownTalentRankById(int32 /*talentId*/) const { return NULL; }'''

UNIT_H_RUNE_COOLDOWN = '''        /**
         * Whether every base rune of a type is on cooldown; Blade Barrier procs only for a death
         * knight whose base blood runes are all on cooldown.
         * @param runeType the rune type asked about
         * @return false here, a unit that is not a player has no runes; the proc's player and class
         * tests return before asking a unit that is not a player; Player returns what its rune
         * manager answers
         */
        virtual bool IsBaseRuneSlotsOnCooldown(RuneType /*runeType*/) const { return false; }'''

UNIT_H_AURA_STATE_CASTS = '''        /**
         * Casts the passive spells this unit knows whose caster aura state is the flag; ModifyAuraState
         * calls it on a player when it sets that aura state.
         * @param flag the aura state set
         * Does nothing here, a unit that is not a player knows no spells; Player casts on itself,
         * triggered, every passive spell in its spell map that is not removed and whose caster aura
         * state is the flag
         */
        virtual void CastPassiveSpellsForAuraState(AuraState /*flag*/) { }'''

UNIT_H_OWN_SESSION_PACKETS = '''        /**
         * Tells the client its melee and ranged attack is cancelled; CombatStop and StopAttackFaction
         * call it on a player.
         * Does nothing here; Player reports the cancel to the callback its session installed, and the
         * session sends SMSG_CANCEL_COMBAT.
         */
        virtual void SendAttackSwingCancelAttack() { }
        /**
         * Tells the client its auto-repeat spell is cancelled; InterruptSpell calls it on a player
         * when it interrupts the auto-repeat spell.
         * @param target the unit whose guid the packet carries
         * Does nothing here; Player reports the cancel and the target's guid to the callback its session
         * installed, and the session sends SMSG_CANCEL_AUTO_REPEAT.
         */
        virtual void SendAutoRepeatCancel(Unit* /*target*/) { }
        /**
         * Tells the client the guid of its pet; SetPet calls it on a player when it sets a pet.
         * Does nothing here; Player, when it has a pet, reports the pet's guid to the callback its session
         * installed, and the session sends SMSG_PET_GUIDS.
         */
        virtual void SendPetGUIDs() { }
        /**
         * Tells the client its stand state; SetStandState calls it on a player when it sets the state.
         * @param state the stand state set
         * Does nothing here; Player reports the state to the callback its session installed, and the
         * session sends SMSG_STANDSTATE_UPDATE.
         */
        virtual void SendStandStateUpdate(uint8 /*state*/) { }
        /**
         * Tells the client a changed melee swing error and remembers it; UpdateMeleeAttackingState
         * calls it after each melee attack update.
         * @param swingError 0 for none, 1 out of reach, 2 facing the wrong way
         * Does nothing here; Player, when the error differs from the last one it told, reports it to the
         * callback its session installed, and the session sends SMSG_ATTACKSWING_NOTINRANGE for 1 or
         * SMSG_ATTACKSWING_BADFACING for 2; Player remembers the error.
         */
        virtual void ReportSwingError(uint8 /*swingError*/) { }'''

UNIT_H_ACCOUNT_SECURITY = '''        /**
         * The security level of the account playing this unit; a game master in GM mode sees a player
         * whose level is not above its own.
         * @return 0 here, a player's level; IsVisibleForOrDetect asks only when both units are
         * players, so the default is not observed there; Player returns what the query its session
         * installed reads
         */
        virtual uint32 GetAccountSecurityLevel() const { return 0; }'''

UNIT_H_POSITION_AND_MOVING = '''        /**
         * Moves the unit to a position, telling its map; Update's pending commit and the end of a
         * spline call it on a player.
         * @param teleport true when the move is a teleport
         * @return false here, a unit that is not a player is relocated by its map's creature relocation
         * at the same call sites; Player relocates itself and its map's view of it, and returns false
         * only for a position outside the map
         */
        virtual bool SetPosition(float /*x*/, float /*y*/, float /*z*/, float /*orientation*/,
                                 bool /*teleport*/ = false) { return false; }
        /**
         * Whether the unit's movement flags hold a moving flag; the auto-repeat spell update asks it of
         * a player.
         * @return false here; Player answers whether its movement flags hold one of movementFlagsMask
         */
        virtual bool isMoving() const { return false; }'''

UNIT_H_CLIENT_CONTROL = '''        /**
         * Gives or takes the client's control of a unit's movement; the fear and confuse states call it
         * on a player when the first of them takes hold and when the last one ends.
         * @param target the unit whose movement is given or taken
         * @param allowMove 1 gives the control, 0 takes it
         * Does nothing here; Player hands the mover authority to or from its session and tells its
         * client.
         */
        virtual void SetClientControl(Unit* /*target*/, uint8 /*allowMove*/) { }'''

UNIT_H_NEAR_TELEPORT = '''        /**
         * Teleports the unit a short way on its own map, keeping its transport, its combat and its pet,
         * and marks the teleport as a spell's when a cast moves the unit itself; NearTeleportTo calls it
         * on a player.
         * @param x the destination x coordinate
         * @param y the destination y coordinate
         * @param z the destination z coordinate
         * @param orientation the facing at the destination
         * @param casting true when the unit's own spell cast moves it
         * Does nothing here, a unit that is not a player is relocated in place by NearTeleportTo's other
         * arm; Player teleports itself with its own teleport.
         */
        virtual void TeleportNear(float /*x*/, float /*y*/, float /*z*/, float /*orientation*/, bool /*casting*/) { }'''

UNIT_H_DAMAGE_CREDIT = '''        /**
         * Credits the damage this unit dealt to a victim: its battleground score when both are players,
         * and its achievement criteria; DealDamage calls it on a player that hit another unit.
         * @param pVictim the unit that took the damage
         * @param damage the damage dealt
         * Does nothing here, a unit that is not a player has no score and no criteria; Player credits its
         * battleground and its achievements.
         */
        virtual void CreditDamageDealt(Unit* /*pVictim*/, uint32 /*damage*/) { }'''

UNIT_H_SPELL_DAMAGE = '''        /**
         * Whether an armor specialization spell fits the unit's primary talent tree, its class and the
         * armor it wears; CalculateSpellDamage gives such a spell no points on a player it does not fit.
         * @param spellProto the spell asked about
         * @return true here, a unit that is not a player has no armor specialization to fail; the
         * spell damage asks only a player, so the default is not observed there; Player answers by its
         * active talent tree, its class's specialization spell and its equipped items
         */
        virtual bool FitArmorSpecializationRules(SpellEntry const* /*spellProto*/) const { return true; }
        /**
         * @return the combo points the unit holds on its combo target, which CalculateSpellDamage
         * multiplies by a spell's combo damage: 0 here, a unit that is not a player holds none; Player
         * returns its own
         */
        virtual uint8 GetComboPoints() const { return 0; }
        /**
         * @return the guid of the unit this unit's combo points are on, which CalculateSpellDamage
         * compares with the spell's target: an empty guid here; Player returns its combo target's
         */
        virtual ObjectGuid GetComboTargetGuid() const { return ObjectGuid(); }'''

UNIT_H_POSSESS = '''        /**
         * Tells the client the action bar of the unit this unit possesses; TakePossessOf calls it on a
         * player once the possession has taken hold.
         * Does nothing here, a unit that is not a player has no client to tell; Player sends its
         * charm's action bar.
         */
        virtual void PossessSpellInitialize() { }
        /**
         * Removes the unit's pet; ResetControlState calls it on a player whose possession of its own pet
         * ends with the pet out of reach.
         * @param mode how the pet is saved
         * Does nothing here; Player removes its pet and saves it in that mode.
         */
        virtual void RemovePet(PetSaveMode /*mode*/) { }
        /**
         * Removes the pet action bar from the client; ResetControlState calls it on a player whose
         * possession of a unit that is not its own pet ends.
         * Does nothing here, a unit that is not a player has no client to tell; Player's pet manager
         * clears the bar and tells its client.
         */
        virtual void RemovePetActionBar() { }
        /**
         * Sets the unit's camera to another unit's view; TakePossessOf calls it on a player as the
         * possession takes hold.
         * @param target the unit whose view the camera takes
         * Does nothing here, a unit that is not a player has no camera; Player sets its camera's view.
         */
        virtual void SetCameraView(Unit* /*target*/) { }
        /**
         * Sets the unit's camera back to its own view; ResetControlState calls it on a player as the
         * possession ends.
         * Does nothing here, a unit that is not a player has no camera; Player resets its camera's view.
         */
        virtual void ResetCameraView() { }

    public:'''

PLAYER_H_ITEM_BY_GUID = '''
        // The item Unit's proc handlers ask for by guid: the one the inventory holds, or NULL; private,
        // so only a call through Unit reaches it
        Item* GetItemByGuid(ObjectGuid guid) const override { return GetInventoryMgr().GetItemByGuid(guid); }'''

PLAYER_H_TALENT_RANK = '''
        // The rank of a talent Unit's spell bonus asks for: the one the talent manager knows, or NULL;
        // private, so only a call through Unit reaches it
        SpellEntry const* GetKnownTalentRankById(int32 talentId) const override final
        {
            return GetTalentMgr().GetKnownTalentRankById(talentId);
        }'''

PLAYER_H_RUNE_COOLDOWN = '''
        // Whether every base rune of a type is on cooldown, which Unit's Blade Barrier proc asks: what
        // the rune manager answers; private, so only a call through Unit reaches it
        bool IsBaseRuneSlotsOnCooldown(RuneType runeType) const override final
        {
            return GetRuneMgr().IsBaseRuneSlotsOnCooldown(runeType);
        }'''

PLAYER_H_AURA_STATE_CASTS = '''
        // Casts the passive spells the player knows whose caster aura state is the flag, which Unit's
        // ModifyAuraState asks for when it sets that aura state; private, so only a call through Unit
        // reaches it
        void CastPassiveSpellsForAuraState(AuraState flag) override;'''

PLAYER_H_OWN_SESSION_PACKETS = '''
        // Tells the client the stand state Unit's SetStandState set; private, so only a call through
        // Unit reaches it
        void SendStandStateUpdate(uint8 state) override;

        // Tells the client a melee swing error that differs from the last one told and remembers it,
        // which Unit's UpdateMeleeAttackingState reports; private, so only a call through Unit reaches it
        void ReportSwingError(uint8 swingError) override;'''

PLAYER_H_ACCOUNT_SECURITY = '''
        // The security level of the account playing this player, which Unit's GM visibility rule
        // compares: what the query its session installed reads at this call; private, so only a call
        // through Unit reaches it
        uint32 GetAccountSecurityLevel() const override final
        {
            MANGOS_ASSERT(m_clientCallbacks.securityLevel);
            return m_clientCallbacks.securityLevel();
        }'''

PLAYER_H_NEAR_TELEPORT = '''
        // Teleports the player a short way on its own map, keeping its transport, combat and pet, which
        // Unit's NearTeleportTo asks for; private, so only a call through Unit reaches it
        void TeleportNear(float x, float y, float z, float orientation, bool casting) override;'''

PLAYER_H_DAMAGE_CREDIT = '''
        // Credits the damage the player dealt to a victim, its battleground score and its achievement
        // criteria, which Unit's DealDamage asks for; private, so only a call through Unit reaches it
        void CreditDamageDealt(Unit* pVictim, uint32 damage) override;'''

PLAYER_H_CAMERA = '''
        // Sets the camera to a unit's view, which Unit's TakePossessOf asks for as a possession takes
        // hold; private, so only a call through Unit reaches it
        void SetCameraView(Unit* target) override;

        // Sets the camera back to the player's own view, which Unit's ResetControlState asks for as a
        // possession ends; private, so only a call through Unit reaches it
        void ResetCameraView() override;'''

# file -> the count of each FORM rewritten in it, the lines the rewrite added, each with the base
# line it follows, the lines it changed, each with the base line it replaced, the sites whose
# window is not WINDOW, by base line, whether it `declares` the overrides, and the lines that
# return a guid by value now (BYVALUE), each with the base line it replaced.
FILES = {
    'src/game/Object/Unit.h': {
        'forms': {},
        'added': [('#include "spells/SpellCooldownMgr.h"', '#include "spells/AuraContainer.h"'),
                  (UNIT_H_COMBAT_STATS, '        MeleeHitOutcome RollMeleeOutcomeAgainst(const Unit* pVictim, '
                                        'WeaponAttackType attType, int32 crit_chance, int32 miss_chance, '
                                        'int32 dodge_chance, int32 parry_chance, int32 block_chance) const;'),
                  (UNIT_H_ITEM_BY_GUID + '\n' + UNIT_H_REPUTATION,
                   '        bool IsTriggeredAtSpellProcEvent(Unit* pVictim, '
                   'SpellAuraHolder* holder, SpellEntry const* procSpell, uint32 procFlag, '
                   'uint32 procExtra, WeaponAttackType attType, bool isVictim, '
                   'SpellProcEventEntry const*& spellProcEvent);'),
                  (UNIT_H_MOUNT_PET, '        void Unmount(bool from_aura = false);'),
                  ('class Transport;', 'class Totem;'),
                  ('struct CreatureInfo;', 'struct SpellEntryExt;'),
                  (UNIT_H_VISIBILITY, '        bool canDetectInvisibilityOf(Unit const* u) const;'),
                  (UNIT_H_COOLDOWNS + '\n' + UNIT_H_COMBO_POINTS + '\n' + UNIT_H_RAGE + '\n' + UNIT_H_KILL_CREDIT
                   + '\n' + UNIT_H_FACTION_GHOST_SPEED + '\n' + UNIT_H_PROC_ONE_OFFS + '\n' + UNIT_H_TALENT_RANK
                   + '\n' + UNIT_H_RUNE_COOLDOWN + '\n' + UNIT_H_AURA_STATE_CASTS + '\n' + UNIT_H_OWN_SESSION_PACKETS
                   + '\n' + UNIT_H_ACCOUNT_SECURITY + '\n' + UNIT_H_POSITION_AND_MOVING
                   + '\n' + UNIT_H_CLIENT_CONTROL + '\n' + UNIT_H_NEAR_TELEPORT + '\n' + UNIT_H_DAMAGE_CREDIT
                   + '\n' + UNIT_H_SPELL_DAMAGE + '\n' + UNIT_H_POSSESS,
                   '        virtual void ProhibitSpellSchool(SpellSchoolMask /*idSchoolMask*/, '
                   'uint32 /*unTimeMs*/) { }'),
                  ('        SpellCooldownMgr m_spellCooldownMgr;', '        AuraContainer m_auras;')]},
    'src/game/entities/player/Player.h': {
        'forms': {},
        'declares': True,
        'added': [("        // The private GetItemByGuid override answers only Unit's lookup.",
                   '        // GetItemDisplayIdInSlot, IsValidPos and the static position checks are called on it '
                   'directly.'),
                  (PLAYER_H_ITEM_BY_GUID + '\n' + PLAYER_H_TALENT_RANK + '\n' + PLAYER_H_RUNE_COOLDOWN
                   + '\n' + PLAYER_H_AURA_STATE_CASTS + '\n' + PLAYER_H_OWN_SESSION_PACKETS
                   + '\n' + PLAYER_H_ACCOUNT_SECURITY + '\n' + PLAYER_H_NEAR_TELEPORT + '\n' + PLAYER_H_DAMAGE_CREDIT
                   + '\n' + PLAYER_H_CAMERA,
                   '        ManagerPacketSink SessionSink() const;')],
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
                     '        Transport* GetTransport() const { return m_transport; }'),
                    ('        ReputationRank GetReputationRank(uint32 faction_id) const override final;',
                     '        ReputationRank GetReputationRank(uint32 faction_id) const;'),
                    ('        void AddSpellAndCategoryCooldowns(SpellEntry const* spellInfo, uint32 itemId, '
                     'Spell* spell = NULL, bool infinityCooldown = false) override;',
                     '        void AddSpellAndCategoryCooldowns(SpellEntry const* spellInfo, uint32 itemId, '
                     'Spell* spell = NULL, bool infinityCooldown = false);'),
                    ('        void SendCooldownEvent(SpellEntry const* spellInfo, uint32 itemId = 0, '
                     'Spell* spell = NULL) override;',
                     '        void SendCooldownEvent(SpellEntry const* spellInfo, uint32 itemId = 0, '
                     'Spell* spell = NULL);'),
                    ('        void RemoveSpellCooldown(uint32 spell_id, bool update = false) override;',
                     '        void RemoveSpellCooldown(uint32 spell_id, bool update = false);'),
                    ('        void RemoveSpellCategoryCooldown(uint32 cat, bool update) override;',
                     '        void RemoveSpellCategoryCooldown(uint32 cat, bool update = false);'),
                    ('        void UpdatePotionCooldown(Spell* spell) override;',
                     '        void UpdatePotionCooldown(Spell* spell = NULL);'),
                    ('        void AddComboPoints(Unit* target, int8 count) override;',
                     '        void AddComboPoints(Unit* target, int8 count);'),
                    ('        void ClearComboPoints() override;', '        void ClearComboPoints();'),
                    ('        void RewardRage(uint32 damage, uint32 weaponSpeedHitFactor, bool attacker) override;',
                     '        void RewardRage(uint32 damage, uint32 weaponSpeedHitFactor, bool attacker);'),
                    ('        void KilledMonster(CreatureInfo const* cInfo, ObjectGuid guid) override;',
                     '        void KilledMonster(CreatureInfo const* cInfo, ObjectGuid guid);'),
                    ('        bool isHonorOrXPTarget(Unit* pVictim) const override final;',
                     '        bool isHonorOrXPTarget(Unit* pVictim) const;'),
                    ('        void setFactionForRace(uint8 race) override;',
                     '        void setFactionForRace(uint8 race);'),
                    ('        bool InBattleGround() const override final { return m_bgData.bgInstanceID != 0; }',
                     '        bool InBattleGround() const { return m_bgData.bgInstanceID != 0; }'),
                    ('        void Say(const std::string& text, const uint32 language) override;',
                     '        void Say(const std::string& text, const uint32 language);'),
                    ('        Player* GetNextRandomRaidMember(float radius) override final;',
                     '        Player* GetNextRandomRaidMember(float radius);'),
                    ('        void SendPetGUIDs() override;', '        void SendPetGUIDs();'),
                    ('        void SendAttackSwingCancelAttack() override;',
                     '        void SendAttackSwingCancelAttack();'),
                    ('        void SendAutoRepeatCancel(Unit* target) override;',
                     '        void SendAutoRepeatCancel(Unit* target);'),
                    ('        bool SetPosition(float x, float y, float z, float orientation, bool teleport = false) '
                     'override;',
                     '        bool SetPosition(float x, float y, float z, float orientation, bool teleport = false);'),
                    ('        bool isMoving() const override final { return m_movementInfo.HasMovementFlag('
                     'movementFlagsMask); }',
                     '        bool isMoving() const { return m_movementInfo.HasMovementFlag(movementFlagsMask); }'),
                    ('        void SetClientControl(Unit* target, uint8 allowMove) override;',
                     '        void SetClientControl(Unit* target, uint8 allowMove);'),
                    ('        uint8 GetComboPoints() const override final { return m_comboPoints; }',
                     '        uint8 GetComboPoints() const { return m_comboPoints; }'),
                    ('        bool FitArmorSpecializationRules(SpellEntry const * spellProto) const override final;',
                     '        bool FitArmorSpecializationRules(SpellEntry const * spellProto) const;'),
                    ('        void RemovePet(PetSaveMode mode) override;', '        void RemovePet(PetSaveMode mode);'),
                    ('        void PossessSpellInitialize() override;', '        void PossessSpellInitialize();'),
                    ('        void RemovePetActionBar() override { m_petMgr.RemoveActionBar(SessionSink()); }',
                     '        void RemovePetActionBar() { m_petMgr.RemoveActionBar(SessionSink()); }')],
        'byvalue': [('        ObjectGuid GetSelectionGuid() const override final { return m_curSelectionGuid; }',
                     '        ObjectGuid const& GetSelectionGuid() const { return m_curSelectionGuid; }'),
                    ('        ObjectGuid GetComboTargetGuid() const override final { return m_comboTargetGuid; }',
                     '        ObjectGuid const& GetComboTargetGuid() const { return m_comboTargetGuid; }')]},
    'src/game/Object/Unit.cpp': {
        'forms': {'HasSpell': 1, 'UnsummonPetTemporaryIfAny': 2, 'ResummonPetTemporaryUnSummonedIfAny': 1,
                  'InArena': 1, 'GetCollisionHeight': 2, 'isGameMaster': 1, 'UpdatePotionCooldown': 1,
                  'AddComboPoints': 1, 'ClearComboPoints': 2, 'RewardRage': 2, 'KilledMonster': 1,
                  'setFactionForRace': 1, 'CastPassiveSpellsForAuraState': 1, 'SendAttackSwingCancelAttack': 2,
                  'SendAutoRepeatCancel': 1, 'SendPetGUIDs': 1, 'SendStandStateUpdate': 1, 'ReportSwingError': 1,
                  'SetPosition': 2, 'isMoving': 1, 'ReportGroupStat': 4, 'ReportGroupAura': 1,
                  'ReportOwnerGroupStat': 4, 'ReportPetGroupAura': 1, 'TeleportNear': 1,
                  'SendKnockBack': 1, 'CreditDamageDealt': 1, 'FitArmorSpecializationRules': 1, 'GetComboPoints': 1,
                  'GetComboTargetGuid': 1, 'unitPlayer': 1, 'player': 1, 'playerAssigned': 2,
                  'SetCameraView': 2, 'ResetCameraView': 2, 'PossessClientControl': 5, 'SendForcedObjectUpdate': 2,
                  'IsTaxiFlying': 2, 'PossessSpellInitialize': 2, 'RemovePet': 1, 'RemovePetActionBar': 1},
        'added': [('    m_spellCooldownMgr(),', '    movespline(new Movement::MoveSpline()),')],
        'changed': [('    if (GetTypeId() == TYPEID_PLAYER)',
                     '    if ((GetTypeId() == TYPEID_PLAYER) && ((Player*)this)->GetGroup())'),
                    ('            if (owner && (owner->GetTypeId() == TYPEID_PLAYER))',
                     '            if (owner && (owner->GetTypeId() == TYPEID_PLAYER) && '
                     '((Player*)owner)->GetGroup())'),
                    ('        if (owner && (owner->GetTypeId() == TYPEID_PLAYER))',
                     '        if (owner && (owner->GetTypeId() == TYPEID_PLAYER) && ((Player*)owner)->GetGroup())'),
                    ('                player->SetClientControl(this, 1);',
                     '                player->SetClientControl(player, 1);'),
                    ('            player->SetClientControl(this, 1);',
                     '            player->SetClientControl(player, 1);')],
        'folded': {'ReportGroupStat': [1], 'ReportOwnerGroupStat': [0, 1, 2, 3], 'ReportPetGroupAura': [0]},
        'window': {626: 13, 960: 17, 975: 32, 1057: 14, 3327: 17, 4092: 50, 4095: 53, 4103: 61, 4152: 12, 4155: 15,
                   4406: 12, 4611: 104, 7114: 37, 7176: 59, 7257: 67},
        'deleted': [['        if (possessedCreature->IsPet() && possessedCreature->GetObjectGuid() == GetPetGuid())',
                     '        {', '            // out of range pet dismissed',
                     '            if (!InReach(*possessedCreature, *this, '
                     'possessedCreature->GetMap()->GetVisibilityDistance()))',
                     '            {', '                player->RemovePet(PET_SAVE_REAGENTS);', '            }',
                     '            else', '            {',
                     '                possessedCreature->GetMotionMaster()->MoveFollow(this, PET_FOLLOW_DIST, '
                     'PET_FOLLOW_ANGLE);', '            }', '        }']]},
    'src/game/Object/UnitDynObject.cpp': {
        'forms': {'AddSpellAndCategoryCooldowns': 1, 'SendCooldownEvent': 1},
        'added': []},
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
        'forms': {'getClass': 5, 'ReportGroupStat': 3, 'ReportOwnerGroupStat': 3},
        'added': [],
        'changed': [('            if (owner && (owner->GetTypeId() == TYPEID_PLAYER))',
                     '            if (owner && (owner->GetTypeId() == TYPEID_PLAYER) && '
                     '((Player*)owner)->GetGroup())')],
        'folded': {'ReportOwnerGroupStat': [0, 1, 2]},
        'window': {272: 6},
        'deleted': ['void Unit::ApplyMaxPowerMod(Powers power, uint32 val, bool apply)']},
    'src/game/Object/UnitSpellBonus.cpp': {
        'forms': {'GetBaseSpellPowerBonus': 2, 'GetKnownTalentRankById': 1},
        'added': []},
    'src/game/Object/UnitVisibility.cpp': {
        'forms': {'IsLoggingOut': 1, 'IsLoading': 1, 'GetTransport': 2, 'IsGroupVisibleFor': 1, 'GetDrunkValue': 1},
        'added': [],
        'changed': [('            return GetAccountSecurityLevel() <= u->GetAccountSecurityLevel();',
                     '            return ((Player*)this)->GetSession()->GetSecurity() <= '
                     '((Player*)u)->GetSession()->GetSecurity();')]},
    'src/game/Object/UnitAura.cpp': {
        'forms': {'IsLoading': 1},
        'added': []},
    'src/game/Object/UnitSpeed.cpp': {
        'forms': {'InBattleGround': 1, 'm_movementInfo.SetMovementFlags': 1, 'SetClientControl': 4},
        'added': [],
        'window': {240: 12}},
    'src/game/WorldHandlers/UnitAuraProcHandler.cpp': {
        'forms': {'HasSpellCooldown': 10, 'AddSpellCooldown': 8, 'GetItemByGuid': 8, 'GetReputationRank': 8,
                  'RemoveSpellCooldown': 1, 'RemoveSpellCategoryCooldown': 3, 'isHonorOrXPTarget': 2,
                  'Say': 1, 'GetSelectionGuid': 1, 'GetNextRandomRaidMember': 1, 'IsBaseRuneSlotsOnCooldown': 1},
        'added': [],
        'window': {980: 44, 1002: 13, 1010: 21, 1045: 13, 1070: 13, 1096: 13, 2881: 70, 3242: 47, 4840: 17}},
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
        starts.append((len(pasted), BRANCH_LINES))
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
    """The working tree's line index of pasted-back line index I: the lines a statement was written
    back as, STARTS' (first index, count), all name that statement; an entry (first index, 1, count)
    is one statement written back for COUNT lines of the working tree, and names the first."""
    shift = 0
    for start in starts:
        p, n = start[0], start[1]
        if i < p:
            break
        if i < p + n:
            return p - shift
        shift += n - (start[2] if len(start) > 2 else 1)
    return i - shift


MATH_CALL = re.compile(r'(?:(?<![\w.>:])std::|(?<![\w.>:])::|(?<![\w.>:]))(?:cos|sin|tan|acos|asin|atan|atan2|sqrt|pow|'
                       r'fabs|abs|floor|ceil|round|fmod|exp|log)\s*\(')
CAST_TYPE = r'(?:double)'


def unspelled_math(line):
    """The first call in LINE, outside its comment and string literals, of an overloaded standard
    math function with an argument that is neither a cast to double nor a double literal, or None."""
    code = re.sub(r'"[^"]*"', '""', line).split('//')[0]
    for m in MATH_CALL.finditer(code):
        depth, args, at = 1, [''], m.end()
        while depth and at < len(code):
            depth += {'(': 1, ')': -1}.get(code[at], 0)
            if depth == 1 and code[at] == ',':
                args.append('')
            elif depth:
                args[-1] += code[at]
            at += 1
        if depth or not all(spelled(a.strip()) for a in args):
            return code[m.start():at]
    return None


def spelled(arg):
    """Whether ARG is a cast to double or a double literal, so the overload it binds is the double one."""
    if re.match(r'^[-+]?(\d+\.\d*|\.\d+)([eE][-+]?\d+)?$', arg) or re.match(r'^\(%s\)\s*\w+$' % CAST_TYPE, arg):
        return True
    m = re.match(r'^(%s\s*|static_cast<double>\s*|\(%s\)\s*)\(' % (CAST_TYPE, CAST_TYPE), arg)
    depth = 0
    for k in range(m.end() - 1 if m else len(arg), len(arg)):
        depth += {'(': 1, ')': -1}.get(arg[k], 0)
        if not depth:
            return k == len(arg) - 1
    return False


UNSPELLED = "an unqualified overloaded call binds by the file's includes: spell the argument as double"


def paste_moved(rel, text, spec, out, read, forms):
    """(rc, text with every statement of a listed `moved` FORM written back as the block it stands
    for, the number written back, [(pasted-back index, line count)] of each block)."""
    moved = {}

    def fail(message):
        out(message)
        return 1, text, 0, []

    for name in sorted(n for n in spec['forms'] if forms[n].get('kind') == 'moved'):
        form = forms[name]
        body = read(form['to']).split('\n')
        heads = [i for i, line in enumerate(body) if line == form['header']]
        if len(heads) != 1 or body[heads[0] + 1:heads[0] + 2] != ['{'] or '}' not in body[heads[0]:]:
            return fail('%s: FAILED: %d definition(s) %r with a body, expected 1' % (form['to'], len(heads),
                                                                                   form['header']))
        end = body.index('}', heads[0])
        for i in range(heads[0] + 2, end):
            if body[i] and not body[i].startswith('    '):
                return fail('%s:%d: FAILED: the body line %r of %r does not start with four spaces'
                            % (form['to'], i + 1, body[i], form['header']))
            if unspelled_math(body[i]):
                return fail('%s:%d: FAILED: %s: %s' % (form['to'], i + 1, unspelled_math(body[i]), UNSPELLED))
        block = '\n'.join(body[heads[0] + 2:end])
        if 'body' in form or 'base_body' in form:
            if form.get('edits') or 'body' not in form or 'base_body' not in form:
                return fail('%s: FAILED: %r pins its body: it takes a body and a base_body and no edits'
                            % (form['to'], form['header']))
            pin, now = form['body'], body[heads[0] + 2:end]
            k = next((k for k in range(max(len(pin), len(now))) if pin[k:k + 1] != now[k:k + 1]), None)
            if k is not None:
                return fail('%s:%d: FAILED: the body of %r does not read as its pinned text'
                            % (form['to'], heads[0] + 3 + k, form['header']))
            block = '\n'.join(form['base_body'])
        for base, new in form.get('edits', ()):
            if block.count(new) != 1:
                return fail('%s: FAILED: the edit %r stands %d time(s) in the body of %r, expected once'
                            % (form['to'], new, block.count(new), form['header']))
            block = block.replace(new, base)
        lines = block.split('\n')
        if 'dropped' in form:
            if form['cast'] not in form['dropped']:
                return fail('%s: FAILED: the dropped line %r holds no %r' % (rel, form['dropped'], form['cast']))
            if form['dropped'] in text:
                return fail('%s: FAILED: the dropped line %r still stands' % (rel, form['dropped']))
            lines = ['    ' + form['dropped']] + lines
        if form['cast'] in text:
            return fail('%s: FAILED: %r still stands: a block the move missed' % (rel, form['cast']))
        params = form['header'][form['header'].index('(') + 1:form['header'].rindex(')')]
        moved[name] = (form, lines, ', '.join(p.split()[-1].lstrip('*&') for p in params.split(',') if p.strip()))
    for name in sorted(n for n in spec['forms'] if forms[n].get('kind') == 'inlined'):
        form = forms[name]
        body = read('base:' + form['from']).split('\n')
        heads = [i for i, line in enumerate(body) if line == form['header']]
        if len(heads) != 1 or body[heads[0] + 1:heads[0] + 2] != ['{'] or '}' not in body[heads[0]:]:
            return fail('%s: FAILED: %d definition(s) %r with a body at the base, expected 1'
                        % (form['from'], len(heads), form['header']))
        end = body.index('}', heads[0])
        kept = [(i + 1, b.replace(form['receiver'], '')) for i, b in enumerate(body[:end])
                if i > heads[0] + 1 and b != '    ' + form['dropped']]
        if end - heads[0] - 2 != len(kept) + 1 or any(b and not b.startswith('    ') for _, b in kept) \
                or not kept or kept[0][1] != '    ' + form['direct']:
            return fail('%s: FAILED: the base body of %r does not hold its dropped line once, opens with no %r, or '
                        'holds a line that does not start with four spaces'
                        % (form['from'], form['header'], form['direct']))
        for base, new in form.get('edits', ()):
            at = [k for k, (_, b) in enumerate(kept) if base in b]
            if len(at) != 1 or kept[at[0]][1].count(base) != 1:
                return fail('%s: FAILED: the edit %r stands %d time(s) in the base body of %r, expected once'
                            % (form['from'], base, sum(b.count(base) for _, b in kept), form['header']))
            n, b = kept[at[0]]
            kept[at[0]:at[0] + 1] = [(n, part) for part in b.replace(base, new).split('\n')]
        if form['cast'] in text or any(line.strip() == form['dropped'] for line in text.split('\n')):
            return fail('%s: FAILED: %r or the dropped line %r still stands' % (rel, form['cast'], form['dropped']))
        for where in form['gone']:
            if re.search(r'\b%s\(' % re.escape(name), read(where)):
                return fail('%s: FAILED: %s is still declared or defined there' % (where, name))
        params = form['header'][form['header'].index('(') + 1:form['header'].rindex(')')]
        moved[name] = (form, kept, form['cast'] + ', '.join(p.split()[-1].lstrip('*&') for p in params.split(','))
                       + ');')
    for name in sorted(n for n in spec['forms'] if forms[n].get('kind') == 'reported'):
        form = forms[name]
        body = read(form['to']).split('\n')
        heads = [i for i, line in enumerate(body) if line == form['lambda']]
        indent = form['lambda'][:len(form['lambda']) - len(form['lambda'].lstrip())]
        if len(heads) != 1 or body[heads[0] + 1:heads[0] + 2] != [indent + '{'] or indent + '};' not in body[heads[0]:]:
            return fail('%s: FAILED: %d callback(s) %r with a body, expected 1'
                        % (form['to'], len(heads), form['lambda']))
        end = body.index(indent + '};', heads[0])
        for i in range(heads[0] + 2, end):
            if body[i] and not body[i].startswith(indent + '    '):
                return fail('%s:%d: FAILED: the body line %r of %r does not start with four spaces'
                            % (form['to'], i + 1, body[i], form['lambda']))
        lines = ['    ' + b[len(indent) + 4:] if b else b for b in body[heads[0] + 2:end]]
        uses = [form['receiver'][0]] + ['fact.' + f for f in form['fields']]
        if any(u not in '\n'.join(lines) for u in uses):
            return fail('%s: FAILED: the body of %r does not use each of %s'
                        % (form['to'], form['lambda'], ', '.join(uses)))
        if 'dropped' in form:
            if form['cast'] not in form['dropped']:
                return fail('%s: FAILED: the dropped line %r holds no %r' % (rel, form['dropped'], form['cast']))
            if form['dropped'] in text:
                return fail('%s: FAILED: the dropped line %r still stands' % (rel, form['dropped']))
        if form['cast'] in text:
            return fail('%s: FAILED: %r still stands: a site the seam missed' % (rel, form['cast']))
        moved[name] = (form, lines, None)
    pasted, starts, found = [], [], dict((name, 0) for name in moved)
    tree = text.split('\n')
    skip = 0
    for at, line in enumerate(tree):
        if skip:
            skip -= 1
            continue
        hit = [n for n in moved if moved[n][0]['kind'] == 'inlined' and line.strip() == moved[n][0]['direct']]
        if hit:
            form, kept, call = moved[hit[0]]
            indent = line[:len(line) - len(line.lstrip())]
            for k, (n, b) in enumerate(kept):
                now = tree[at + k] if at + k < len(tree) else '(end of file)'
                if now != (indent + b[4:] if b else b):
                    return fail('%s:%d: FAILED: %r does not read as the base body of %r at %s:%d: %r'
                                % (rel, at + k + 1, now, form['header'], form['from'], n, b))
                if unspelled_math(now):
                    return fail('%s:%d: FAILED: %s: %s' % (rel, at + k + 1, unspelled_math(now), UNSPELLED))
            starts.append((len(pasted), 1, len(kept)))
            pasted.append(indent + call)
            found[hit[0]] += 1
            skip = len(kept) - 1
            continue
        hit = [name for name in moved if moved[name][0]['kind'] != 'inlined' and moved[name][0]['direct'] in line]
        if not hit:
            pasted.append(line)
            continue
        form, block, names = moved[hit[0]]
        if form['kind'] == 'reported':
            block = reported_block(rel, at, tree, form, block,
                                   found[hit[0]] in spec.get('folded', {}).get(hit[0], ()), spec, out)
            if block is None:
                return 1, text, 0, []
            starts.append((len(pasted), len(block)))
            pasted += [re.match(r'^(\s*)', line).group(1) + b[4:] if b else b for b in block]
            found[hit[0]] += 1
            continue
        m = re.match(r'^(\s*)' + re.escape(form['direct']) + r'(.*)\);$', line)
        if len(hit) > 1 or not m or m.group(2) != names:
            return fail("%s:%d: FAILED: %s( does not stand as a whole statement passing the parameters of %r"
                        % (rel, at + 1, hit[0], form['header']))
        starts.append((len(pasted), len(block)))
        pasted += [m.group(1) + b[4:] if b else b for b in block]
        found[hit[0]] += 1
    for name, n in sorted(found.items()):
        if n != spec['forms'][name]:
            return fail('%s: FAILED: %d call(s) of %s, expected %d' % (rel, n, name, spec['forms'][name]))
    return 0, '\n'.join(pasted), sum(found.values()), starts


def reported_block(rel, at, tree, form, body, folded, spec, out):
    """The lines (four-space relative) the reported statement at tree index AT stands for: the
    callback's BODY with its receiver read as the cast and each `fact.<field>` as the statement's
    argument, below the dropped line if any; FOLDED, only the lines inside the body's opening
    test, whose condition stood in the guard two lines above (a listed CHANGED line). None after
    naming why."""
    m = re.match(r'^(\s*)' + re.escape(form['direct'] + form['fact']) + r'\{(.*)\}\);$', tree[at])
    args = m.group(2).split(', ') if m else []
    if len(args) != len(form['fields']):
        out('%s:%d: FAILED: %s does not stand as a whole statement reporting a %s of %d argument(s)'
            % (rel, at + 1, form['direct'].rstrip(', '), form['fact'], len(form['fields'])))
        return None
    lines = []
    for b in body:
        b = b.replace(form['receiver'][0], form['receiver'][1])
        for field, arg in zip(form['fields'], args):
            b = b.replace('fact.' + field, arg)
        lines.append(b)
    if folded:
        test = re.match(r'^    if \((.*)\)$', lines[0]) if len(lines) > 3 else None
        guard = re.match(r'^(\s*)if \((.*)\)$', tree[at - 2]) if at >= 2 else None
        if not test or lines[1] != '    {' or lines[-1] != '    }' or not guard or tree[at - 1] != guard.group(1) + '{':
            out('%s:%d: FAILED: a folded %s does not stand under a guard, or its body opens with no test'
                % (rel, at + 1, form['direct'].rstrip(', ')))
            return None
        bases = ['%sif ((%s) && %s)' % (guard.group(1), guard.group(2), test.group(1))]
        if ('||' not in guard.group(2) and '?' not in guard.group(2)
                and not re.search(r'(?<![=!<>])=(?!=)', guard.group(2))):
            bases.append('%sif (%s && %s)' % (guard.group(1), guard.group(2), test.group(1)))
        if not any((tree[at - 2], base) in spec.get('changed', []) for base in bases):
            out('%s:%d: FAILED: the folded guard %r is not listed as changed from %s'
                % (rel, at - 1, tree[at - 2], ' or '.join(repr(base) for base in bases)))
            return None
        lines = ['    ' + b[8:] if b else b for b in lines[2:-1]]
    if 'dropped' in form:
        lines = ['    ' + form['dropped']] + lines
    return lines


LOCAL_POINTER = re.compile(r'^Player\* (\w+) = (?:\((.+)\) \? \(Player\*\)this : NULL'
                           r'|(.+) \? static_cast<Player\*>\(this\): NULL);$')
LOCAL_ASSIGNED = re.compile(r'^(\w+) = static_cast<Player \*>\(this\);$')


def paste_locals(rel, text, spec, out, forms, starts=()):
    """(rc, text with the flag line of each listed `local` FORM written back as its pointer line); STARTS are
    the moved blocks already written back, so a refused line is named by its working-tree number."""
    lines = text.split('\n')

    def fail(message):
        out(message)
        return 1, text

    for name in sorted(n for n in spec['forms'] if forms[n].get('kind') == 'local'):
        form, local, declared = forms[name], forms[name]['local'], forms[name].get('declared')
        m = (LOCAL_ASSIGNED if declared else LOCAL_POINTER).match(form['cast'])
        flag = '%s = true;' % local if declared else 'bool %s = %s;' % (local, m and (m.group(2) or m.group(3)))
        if not m or m.group(1) != local or form['direct'] != flag or \
                declared not in (None, ('bool %s = false;' % local, 'Player* %s = NULL;' % local)):
            return fail('%s: FAILED: the flag line %r is not the pointer line %r read as `bool %s = <its test>;` '
                        'or as `%s = true;` below `bool %s = false;`'
                        % (rel, form['direct'], form['cast'], local, local, local))
        for f in form['forms']:
            cast = forms[f]['cast'] if f in spec['forms'] else ''
            renamed = cast.replace(local + '->', '', 1) != forms[f]['direct']
            if local + '->' not in cast or renamed and not cast.startswith(local + '->'):
                return fail('%s: FAILED: %s is no listed FORM whose cast spelling is its direct spelling called '
                            'through %s->' % (rel, f, local))
        if form['cast'] in text:
            return fail('%s: FAILED: the pointer line %r still stands' % (rel, form['cast']))
        at = [i for i, line in enumerate(lines) if line.strip() == form['direct']]
        if len(at) != spec['forms'][name]:
            return fail('%s: FAILED: %d flag line(s) %r, expected %d'
                        % (rel, len(at), form['direct'], spec['forms'][name]))
        tests = r'\b%s (?=&&|\?)|(?<=if \()%s(?=\))' % (local, local)
        for i in at:
            if declared and lines[i - 3].strip() != declared[0]:
                return fail('%s:%d: FAILED: %r is not declared three lines above it as %r'
                            % (rel, tree_index(starts, i) + 1, form['direct'], declared[0]))
            for k in range(i + 1, lines.index('}', i)):
                if re.search(r'\b%s\b' % local, re.sub(tests, '', lines[k].split('//')[0])):
                    return fail('%s:%d: FAILED: the local %s stands as neither `%s &&`, `%s ?` nor `if (%s)`: %r'
                                % (rel, tree_index(starts, k) + 1, local, local, local, local, lines[k].strip()))
            lines[i] = lines[i][:len(lines[i]) - len(lines[i].lstrip())] + form['cast']
            if declared:
                lines[i - 3] = lines[i - 3].replace(declared[0], declared[1])
    return 0, '\n'.join(lines)


def paste_back(rel, text, spec, out, read=None, forms=None):
    """(rc, text with every FORM written back, the number of calls written back, the (first index,
    line count) of each `moved` block and of each `branch` site, as two lists, {branch form: the
    pasted-back index of the first line of each of its sites, in order})."""
    forms = FORMS if forms is None else forms
    rc, text, sites, moved = paste_moved(rel, text, spec, out, read, forms)
    if not rc:
        rc, text = paste_locals(rel, text, spec, out, forms, moved)
    if rc:
        return 1, text, sites, [], {}
    sites += sum(n for name, n in spec['forms'].items() if forms[name].get('kind') == 'local')
    for name, want in sorted(spec['forms'].items()):
        form = forms[name]
        if form.get('kind') in ('branch', 'moved', 'reported', 'inlined', 'local'):
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
    return rc, text, sites + found, [moved, starts], by_form


def lists_none(rel, lines, masked, spec, out, forms=None):
    """0 when no FORM the file does not list is spelled outside the MASKED line indices (the added
    lines dropped or found at their place), else 1; an `elsewhere` FORM is not looked for."""
    forms = FORMS if forms is None else forms
    rest = '\n'.join(line for i, line in enumerate(lines) if i not in masked)
    for name in forms:
        tolerated = forms[name].get('elsewhere') is True or rel in (forms[name].get('elsewhere') or ())
        if name not in spec['forms'] and not tolerated and forms[name]['direct'] in rest:
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
    old_lines, new_lines, gone = old_text.split('\n'), pasted.split('\n'), set()
    for header in spec.get('deleted', []):
        if isinstance(header, list):
            at, left = block_at(old_lines, header), len(block_at(new_lines, header))
            if len(at) != 1 or left:
                out('%s: FAILED: the deleted block %r stands %d time(s) at the base and %d in the working tree, '
                    'expected once and none' % (rel, header[0], len(at), left))
                return 1, pairs
            gone.update(range(at[0], at[0] + len(header)))
            continue
        if old_lines.count(header) != 1 or header in new_lines or '}' not in old_lines[old_lines.index(header):]:
            out('%s: FAILED: the deleted member %r stands %d time(s) at the base with a body and %d in the working '
                'tree, expected once and none' % (rel, header, old_lines.count(header), new_lines.count(header)))
            return 1, pairs
        gone.update(range(old_lines.index(header), old_lines.index('}', old_lines.index(header)) + 1))
    for name, want in sorted(spec['forms'].items()):
        cast = cast_of(name, forms[name], read, out)
        if cast is None:
            return 1, pairs
        wider = [forms[n]['cast'] for n in spec['forms']
                 if forms[n].get('kind') != 'branch' and forms[n]['cast'] != cast and cast in forms[n]['cast']]
        olds = [o for o in (line_of(old_text, i) - 1 for i in find_all(old_text, cast))
                if o not in gone and not any(w in old_lines[o] for w in wider)]
        if len(olds) != want:
            out('%s: FAILED: %d cast call(s) of %s at the base, the spec lists %d' % (rel, len(olds), name, want))
            return 1, pairs
        if forms[name].get('kind') == 'branch':
            news = [start + BRANCH_PLAYER for start in by_form.get(name, [])]
        else:
            news = [n for n in (line_of(pasted, i) - 1 for i in find_all(pasted, cast))
                    if not any(w in new_lines[n] for w in wider)]
        if len(news) != want:
            out('%s: FAILED: %d cast call(s) of %s pasted back, the spec lists %d' % (rel, len(news), name, want))
            return 1, pairs
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
        if len(at) > 1 and kind == 'added':
            out('%s: FAILED: the base line %r that the %s line %s names stands in windows at lines %s, '
                'expected one' % (rel, base, kind, label, ', '.join(':%d' % (j + 1) for j in sorted(at))))
            rc = 1
            continue
        if not at:
            (added_out if kind == 'added' else changed_out).append((line, base))
            continue
        table = added_in if kind == 'added' else changed_in
        for j in sorted(at):
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


def by_value(base):
    """The line BASE reads as under the by-value transformation, or None when BASE declares no
    getter whose return type, its first word, is `ObjectGuid const&` (once on the line, with no
    `override`): that return read as `ObjectGuid` and ` override final` written before the body
    (` {`), or before the closing `;` of a declaration."""
    if not base.lstrip().startswith(BYVALUE_FROM) or base.count(BYVALUE_FROM) != 1 or 'override' in base:
        return None
    line = base.replace(BYVALUE_FROM, BYVALUE_TO, 1)
    at = line.find(' {')
    if at < 0:
        if not line.endswith(';'):
            return None
        at = len(line) - 1
    return line[:at] + BYVALUE_FINAL + line[at:]


def by_value_entries(rel, spec, out):
    """(0, the BYVALUE entries as CHANGED entries) when each reads as its base line under the
    by-value transformation, else (1, [])."""
    changed = []
    for new, base in spec.get('byvalue', []):
        want = by_value(base)
        if want is None:
            out('%s: FAILED: the by-value base line %r declares no getter returning `ObjectGuid const&`' % (rel, base))
            return 1, []
        if new != want:
            out('%s: FAILED: the by-value line %r is not the base line %r with its `ObjectGuid const&` return read '
                'as `ObjectGuid` and ` override final` before its body, nothing else changed (that reads %r)'
                % (rel, new, base, want))
            return 1, []
        changed.append((new, base))
    return 0, changed


def prove(rel, old_text, new_text, spec, out, window=WINDOW, read=None, forms=None):
    """(rc, [(name, base line, new line, window)]): rc 0 when the window around every site pastes
    back to the base's byte for byte and every listed entry stands at its place. READ reads a file
    a `branch` FORM names; FORMS replaces the module's."""
    forms = FORMS if forms is None else forms
    rc, by_value_changed = by_value_entries(rel, spec, out)
    if rc:
        return 1, []
    if by_value_changed:
        spec = dict(spec, changed=spec.get('changed', []) + by_value_changed)
    rc, pasted, sites, starts, by_form = paste_back(rel, new_text, spec, out, read, forms)
    if rc:
        return 1, []
    rc, pairs = pair_sites(rel, old_text, pasted, spec, out, read, forms, by_form)
    if rc:
        return 1, []

    def tree(i):
        return tree_index(starts[0], tree_index(starts[1], i))

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
    if by_value_changed:
        outside += ' (%d by value)' % len(by_value_changed)
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
        if rel.startswith('base:'):
            return subprocess.run(['git', '-C', root, 'show', '%s:%s' % (base, rel[5:])], capture_output=True,
                                  check=True).stdout.decode('utf-8')
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
    ((Player*)this)->SendClearCooldown(id, this);
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
    ((Player*)this)->SendClearCooldown(id, this);
    int added = 1;
}
'''

SELF_SPEC = {'forms': {'HasSpellCooldown': 2, 'AddSpellCooldown': 1},
             'added': [('    int added = 1;', '    ((Player*)this)->SendClearCooldown(id, this);')]}

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


SELF_MOVED_OLD = '''void Unit::Mod(AuraState flag, bool apply)
{
    if (apply)
    {
        SetFlag(flag);
        if (GetTypeId() == TYPEID_PLAYER)
        {
            const Map& m = ((Player*)this)->GetMap();
            for (Map::const_iterator itr = m.begin(); itr != m.end(); ++itr)
            {
                if (itr->second == flag)
                {
                    CastSpell(this, itr->first, true, NULL);
                }
            }
        }
    }
}
'''

SELF_MOVED_NEW = '\n'.join(SELF_MOVED_OLD.split('\n')[:7] + ['            Cast(flag);']
                           + SELF_MOVED_OLD.split('\n')[15:])

SELF_MOVED_BODY = 'void Player::Cast(AuraState flag)\n{\n' + '\n'.join(
    line[8:] for line in SELF_MOVED_OLD.split('\n')[7:15]).replace('((Player*)this)->GetMap()', 'GetMap()') + '\n}\n'

SELF_MOVED_FORMS = {'Cast': {'kind': 'moved', 'direct': 'Cast(', 'cast': '((Player*)this)->GetMap()',
                             'to': 'Player.cpp',
                             'header': 'void Player::Cast(AuraState flag)',
                             'edits': [('((Player*)this)->GetMap()', 'GetMap()')]}}

SELF_DROPPED_OLD = '''bool Unit::Swing()
{
    uint8 error = Pick();

    Player* player = (GetTypeId() == TYPEID_PLAYER ? (Player*)this : NULL);
    if (player && error != player->LastError())
    {
        player->SendError(error);
        player->SetLastError(error);
    }

    return error == 0;
}
'''

SELF_DROPPED_NEW = '\n'.join(SELF_DROPPED_OLD.split('\n')[:4] + ['    Report(error);']
                             + SELF_DROPPED_OLD.split('\n')[10:])

SELF_DROPPED_BODY = '''void Player::Report(uint8 error)
{
    if (error != LastError())
    {
        SendError(error);
        SetLastError(error);
    }
}
'''

SELF_DROPPED_FORM = {'kind': 'moved', 'direct': 'Report(',
                     'cast': '(GetTypeId() == TYPEID_PLAYER ? (Player*)this : NULL)',
                     'dropped': 'Player* player = (GetTypeId() == TYPEID_PLAYER ? (Player*)this : NULL);',
                     'to': 'Player.cpp', 'header': 'void Player::Report(uint8 error)',
                     'edits': [('player->LastError()', 'LastError()'), ('if (player && error != ', 'if (error != '),
                               ('player->SendError(', 'SendError('), ('player->SetLastError(', 'SetLastError(')]}

SELF_REPORTED_OLD = '''void Unit::SetHealth(uint32 val)
{
    SetValue(val);

    if (GetTypeId() == TYPEID_PLAYER)
    {
        if (((Player*)this)->GetGroup())
        {
            ((Player*)this)->SetGroupUpdateFlag(FLAG_HP);
        }
    }
}

void Unit::SetLevel(uint32 lvl)
{
    SetValue(lvl);

    if ((GetTypeId() == TYPEID_PLAYER) && ((Player*)this)->GetGroup())
    {
        ((Player*)this)->SetGroupUpdateFlag(FLAG_LEVEL);
    }
}

void Unit::UpdateAura(uint8 slot)
{
    if (GetTypeId() == TYPEID_PLAYER)
    {
        Player* player = (Player*)this;
        if (player->GetGroup())
        {
            player->SetGroupUpdateFlag(FLAG_AURAS);
            player->SetAuraUpdateMask(slot);
        }
    }
}

void Unit::Unused(uint32 val)
{
    if (GetTypeId() == TYPEID_PLAYER)
    {
        if (((Player*)this)->GetGroup())
        {
            ((Player*)this)->SetGroupUpdateFlag(FLAG_MAX);
        }
    }
}
'''

SELF_REPORTED_NEW = '''void Unit::SetHealth(uint32 val)
{
    SetValue(val);

    if (GetTypeId() == TYPEID_PLAYER)
    {
        Report(Of(m).stat, Stat{FLAG_HP});
    }
}

void Unit::SetLevel(uint32 lvl)
{
    SetValue(lvl);

    if (GetTypeId() == TYPEID_PLAYER)
    {
        Report(Of(m).stat, Stat{FLAG_LEVEL});
    }
}

void Unit::UpdateAura(uint8 slot)
{
    if (GetTypeId() == TYPEID_PLAYER)
    {
        Report(Of(m).aura, Aura{FLAG_AURAS, slot});
    }
}
'''

SELF_REPORTED_BODY = '''template <class C, class O>
C For(O* owner)
{
    C c;
    c.stat = [owner](auto const& fact)
    {
        if (owner->GetGroup())
        {
            owner->SetGroupUpdateFlag(fact.flag);
        }
    };
    c.aura = [owner](auto const& fact)
    {
        if (owner->GetGroup())
        {
            owner->SetGroupUpdateFlag(fact.flag);
            owner->SetAuraUpdateMask(fact.slot);
        }
    };
    return c;
}
'''

SELF_REPORTED_FORMS = {
    'Stat': {'kind': 'reported', 'direct': 'Report(Of(m).stat, ', 'cast': '((Player*)this)->SetGroupUpdateFlag(',
             'fact': 'Stat', 'fields': ['flag'], 'to': 'Group.h', 'lambda': '    c.stat = [owner](auto const& fact)',
             'receiver': ('owner->', '((Player*)this)->')},
    'Aura': {'kind': 'reported', 'direct': 'Report(Of(m).aura, ', 'cast': 'Player* player = (Player*)this;',
             'dropped': 'Player* player = (Player*)this;', 'fact': 'Aura', 'fields': ['flag', 'slot'], 'to': 'Group.h',
             'lambda': '    c.aura = [owner](auto const& fact)', 'receiver': ('owner->', 'player->')}}

SELF_REPORTED_SPEC = {'forms': {'Stat': 2, 'Aura': 1}, 'added': [],
                      'changed': [('    if (GetTypeId() == TYPEID_PLAYER)',
                                   '    if ((GetTypeId() == TYPEID_PLAYER) && ((Player*)this)->GetGroup())')],
                      'folded': {'Stat': [1]}, 'deleted': ['void Unit::Unused(uint32 val)']}

SELF_INLINED_OLD = '''void Unit::Knock(float angle, float speed)
{
    if (GetTypeId() == TYPEID_PLAYER)
    {
        ((Player*)this)->GetSession()->Knock(angle, speed);
    }
    else
    {
        Jump(angle, speed);
    }
}
'''

SELF_INLINED_NEW = SELF_INLINED_OLD.replace('        ((Player*)this)->GetSession()->Knock(angle, speed);\n',
                                            '        Params params;\n        params.x = float(cos(double(angle)));\n'
                                            '        Send(MotionState().Apply(params, speed));\n')

SELF_INLINED_BASE = '''void WorldSession::Knock(float angle, float speed)
{
    Params params;
    params.x = cos(angle);
    Player* player = GetPlayer();
    player->Send(player->MotionState().Apply(params, speed));
}
'''

SELF_INLINED_FORM = {'kind': 'inlined', 'direct': 'Params params;', 'cast': '((Player*)this)->GetSession()->Knock(',
                     'from': 'Session.cpp', 'header': 'void WorldSession::Knock(float angle, float speed)',
                     'dropped': 'Player* player = GetPlayer();', 'receiver': 'player->',
                     'gone': ['Session.h', 'Session.cpp'], 'edits': [('cos(angle)', 'float(cos(double(angle)))')]}

SELF_LOCAL_OLD = '''int32 Unit::Points(SpellEntry const* spellProto)
{
    Player* unitPlayer = (GetTypeId() == TYPEID_PLAYER) ? (Player*)this : NULL;
    if (unitPlayer && spellProto->HasAttribute(1))
    {
        if (!unitPlayer->Fits(spellProto))
        {
            return 0;
        }
    }
    uint8 points = unitPlayer ? unitPlayer->GetPoints() : 0;
    return points;
}
'''

SELF_LOCAL_NEW = SELF_LOCAL_OLD.replace('Player* unitPlayer = (GetTypeId() == TYPEID_PLAYER) ? (Player*)this : NULL;',
                                        'bool unitPlayer = GetTypeId() == TYPEID_PLAYER;').replace('unitPlayer->', '')

SELF_LOCAL_FORMS = {
    'Fits': {'direct': 'Fits(', 'cast': 'unitPlayer->Fits(', 'suffix': None},
    'GetPoints': {'direct': 'GetPoints(', 'cast': 'unitPlayer->GetPoints(', 'suffix': None},
    'unitPlayer': {'kind': 'local', 'local': 'unitPlayer', 'direct': 'bool unitPlayer = GetTypeId() == TYPEID_PLAYER;',
                   'cast': 'Player* unitPlayer = (GetTypeId() == TYPEID_PLAYER) ? (Player*)this : NULL;',
                   'forms': ['Fits', 'GetPoints']}}

SELF_ASSIGNED_OLD = '''void Unit::Reset()
{
    Player* player = NULL;
    if (GetTypeId() == TYPEID_PLAYER)
    {
        player = static_cast<Player *>(this);
    }
    if (player)
    {
        // the player's own camera
        player->GetCamera().ResetView();
        player->Grant(player, 1);
        return;
    }
    if (IsPet())
    {
        player->Grant(player, 0);
    }
}
'''

SELF_ASSIGNED_NEW = SELF_ASSIGNED_OLD.replace('Player* player = NULL;', 'bool player = false;').replace(
    'player = static_cast<Player *>(this);', 'player = true;').replace(
    'player->GetCamera().ResetView(', 'ResetView(').replace('player->Grant(player, 1)', 'Grant(this, 1)').replace(
    '    if (IsPet())\n    {\n        player->Grant(player, 0);\n    }\n', '')

SELF_ASSIGNED_FORMS = {
    'ResetView': {'direct': 'ResetView(', 'cast': 'player->GetCamera().ResetView(', 'suffix': None},
    'Grant': {'direct': 'Grant(', 'cast': 'player->Grant(', 'suffix': None},
    'assigned': {'kind': 'local', 'local': 'player', 'direct': 'player = true;',
                 'cast': 'player = static_cast<Player *>(this);',
                 'declared': ('bool player = false;', 'Player* player = NULL;'), 'forms': ['ResetView', 'Grant']}}

OWNER_REPORTED_OLD = '''void Unit::SetHealth(uint32 val)
{
    SetValue(val);

    if (IsPet())
    {
        Unit* owner = GetOwner();
        if (owner && (owner->GetTypeId() == TYPEID_PLAYER) && ((Player*)owner)->GetGroup())
        {
            ((Player*)owner)->SetGroupUpdateFlag(FLAG_PET_HP);
        }
    }
}

void Unit::SetMaxHealth(uint32 val)
{
    SetValue(val);

    if (IsPet())
    {
        Unit* owner = GetOwner();
        if (owner && (owner->GetTypeId() == TYPEID_PLAYER) && ((Player*)owner)->GetGroup())
        {
            ((Player*)owner)->SetGroupUpdateFlag(FLAG_PET_MAX);
        }
    }
}

void Unit::UpdateAura(uint8 slot)
{
    if (IsPet())
    {
        Pet* pet = (Pet*)this;
        Unit* owner = GetOwner();
        if (owner && (owner->GetTypeId() == TYPEID_PLAYER) && ((Player*)owner)->GetGroup())
        {
            ((Player*)owner)->SetGroupUpdateFlag(FLAG_PET_AURAS);
            pet->SetAuraUpdateMask(slot);
        }
    }
}
'''

OWNER_REPORTED_NEW = '''void Unit::SetHealth(uint32 val)
{
    SetValue(val);

    if (IsPet())
    {
        Unit* owner = GetOwner();
        if (owner && (owner->GetTypeId() == TYPEID_PLAYER))
        {
            Report(Of(owner->m).stat, Stat{FLAG_PET_HP});
        }
    }
}

void Unit::SetMaxHealth(uint32 val)
{
    SetValue(val);

    if (IsPet())
    {
        Unit* owner = GetOwner();
        if (owner && (owner->GetTypeId() == TYPEID_PLAYER))
        {
            Report(Of(owner->m).stat, Stat{FLAG_PET_MAX});
        }
    }
}

void Unit::UpdateAura(uint8 slot)
{
    if (IsPet())
    {
        Pet* pet = (Pet*)this;
        Unit* owner = GetOwner();
        if (owner && (owner->GetTypeId() == TYPEID_PLAYER))
        {
            Report(Of(owner->m).petAura, PetAura{FLAG_PET_AURAS, slot, pet});
        }
    }
}
'''

OWNER_REPORTED_BODY = '''template <class C, class O>
C For(O* owner)
{
    C c;
    c.stat = [owner](auto const& fact)
    {
        if (owner->GetGroup())
        {
            owner->SetGroupUpdateFlag(fact.flag);
        }
    };
    c.petAura = [owner](auto const& fact)
    {
        if (owner->GetGroup())
        {
            owner->SetGroupUpdateFlag(fact.flag);
            fact.pet->SetAuraUpdateMask(fact.slot);
        }
    };
    return c;
}
'''

OWNER_REPORTED_FORMS = {
    'OwnerStat': {'kind': 'reported', 'direct': 'Report(Of(owner->m).stat, ',
                  'cast': '((Player*)owner)->SetGroupUpdateFlag(', 'fact': 'Stat', 'fields': ['flag'], 'to': 'Group.h',
                  'lambda': '    c.stat = [owner](auto const& fact)', 'receiver': ('owner->', '((Player*)owner)->')},
    'PetAura': {'kind': 'reported', 'direct': 'Report(Of(owner->m).petAura, ',
                'cast': '((Player*)owner)->SetGroupUpdateFlag(FLAG_PET_AURAS)', 'fact': 'PetAura',
                'fields': ['flag', 'slot', 'pet'], 'to': 'Group.h',
                'lambda': '    c.petAura = [owner](auto const& fact)',
                'receiver': ('owner->', '((Player*)owner)->')}}

OWNER_REPORTED_SPEC = {'forms': {'OwnerStat': 2, 'PetAura': 1}, 'added': [],
                       'changed': [('        if (owner && (owner->GetTypeId() == TYPEID_PLAYER))',
                                    '        if (owner && (owner->GetTypeId() == TYPEID_PLAYER) && '
                                    '((Player*)owner)->GetGroup())')],
                       'folded': {'OwnerStat': [0, 1], 'PetAura': [0]}}


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
              '    ((Player*)this)->SendClearCooldown(id, this);\n    int added = 1;\n}\n',
              'AddSpellCooldown(dummySpell->ID\n    ((Player*)this)->SendClearCooldown(id, this);\n'
              '    int added = 1;\n'))
    run('an added line moved fails', 1, 'DIFFERS from the base at line 15',
        swap=('    ((Player*)this)->SendClearCooldown(id, this);\n    int added = 1;\n',
              '    int added = 1;\n    ((Player*)this)->SendClearCooldown(id, this);\n'))
    run('an added line whose base line stands twice in a window fails', 1,
        "stands 2 time(s) in the window around the AddSpellCooldown site at :13",
        old_text=SELF_OLD + '    ((Player*)this)->SendClearCooldown(id, this);\n',
        new_text=SELF_NEW + '    ((Player*)this)->SendClearCooldown(id, this);\n')
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

    by_ref = '        ObjectGuid const& GetPick() const { return m_pick; }'
    by_val = '        ObjectGuid GetPick() const override final { return m_pick; }'

    def by_value_row(label, want_rc, needle, new_line=by_val, listed=by_val, base_line=by_ref):
        run(label, want_rc, needle,
            new_text=SELF_DECL_NEW.replace('        int m_a;\n', '        int m_a;\n' + new_line + '\n'),
            old_text=SELF_DECL_OLD.replace('        int m_a;\n', '        int m_a;\n' + base_line + '\n'),
            spec=dict(SELF_DECL_SPEC, declares=True, byvalue=[(listed, base_line)]))

    by_value_row('a guid getter returned by value, override final, is the known transformation', 0,
                 '3 added line(s) and 1 changed line(s) at their place outside every window (1 by value)')
    second = '        ObjectGuid GetPick() override final { return m_pick; }'
    by_value_row('a by-value line with a second difference fails, naming both lines', 1,
                 'the by-value line %r is not the base line %r' % (second, by_ref), new_line=second, listed=second)
    kept = '        ObjectGuid const& GetPick() const override final { return m_pick; }'
    by_value_row('a by-value line that keeps the reference fails', 1, 'the by-value line %r is not' % kept,
                 new_line=kept, listed=kept)
    no_final = '        ObjectGuid GetPick() const override { return m_pick; }'
    by_value_row('a by-value line without final fails', 1, 'the by-value line %r is not' % no_final,
                 new_line=no_final, listed=no_final)
    no_ref = '        uint64 GetPick() const { return m_pick; }'
    by_value_row('a by-value base line that returns no guid reference fails', 1,
                 'the by-value base line %r declares no getter' % no_ref, base_line=no_ref)
    by_value_row('a by-value line missing from the working tree fails', 1,
                 'the changed line %r stands 0 time(s)' % by_val, new_line=by_ref)
    by_value_row('a by-value declaration gains override final before its semicolon', 0, '(1 by value)',
                 new_line='        ObjectGuid GetPick() const override final;',
                 listed='        ObjectGuid GetPick() const override final;',
                 base_line='        ObjectGuid const& GetPick() const;')

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
    each = dict(GEN_SPEC, changed=[('        break;', '        return;')])
    gen('a changed line standing in three windows is written back at each', 0,
        '3 changed line(s) written back inside them', swap=[('        return;', '        break;')] * 3, spec=each)
    gen('... and one of them left as the base fails', 1, "does not stand in place of '        return;'",
        swap=[('        return;', '        break;')] * 2, spec=each)
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
                                 (False, 1, '... and fails without the flag'),
                                 (['other.cpp'], 1, '... and fails when `elsewhere` names another file only')):
        forms = dict(elsewhere_forms)
        forms['Elsewhere'] = dict(forms['Elsewhere'], elsewhere=flag)
        got = []
        rc = verify('fixture', SELF_BRANCH_OLD, SELF_BRANCH_NEW, SELF_BRANCH_SPEC, got.append,
                    read=dict(SELF_BRANCH_FILES, **{'Unit.h': SELF_BRANCH_NEW}).__getitem__, forms=forms)
        ok = rc == want_rc and (flag is True or 'a call of Elsewhere in a file that lists none' in '\n'.join(got))
        print('self-test: %-72s %s' % (label, 'PASS' if ok else 'FAIL'))
        if not ok:
            failures.append('%s: rc %d\n%s' % (label, rc, '\n'.join(got)))

    def moved(label, want_rc, needle, swap=('', ''), body=('', ''), window=8):
        got = []
        rc = verify('fixture', SELF_MOVED_OLD, SELF_MOVED_NEW.replace(*swap), {'forms': {'Cast': 1}, 'added': [],
                    'window': {8: window}}, got.append, read={'Player.cpp': SELF_MOVED_BODY.replace(*body)}.__getitem__,
                    forms=SELF_MOVED_FORMS)
        ok = rc == want_rc and needle in '\n'.join(got)
        print('self-test: %-72s %s' % (label, 'PASS' if ok else 'FAIL'))
        if not ok:
            failures.append('%s: rc %d (want %d)\n%s' % (label, rc, want_rc, '\n'.join(got)))

    moved('a moved block pastes back in place of its statement', 0, 'around 1/1 call(s) pasted back')
    moved('a moved line changed fails', 1, 'fixture:8: DIFFERS from the base at line 13', body=('true', 'false'))
    moved('a moved line missing fails', 1, 'DIFFERS from the base at line 11',
          body=('    if (itr->second == flag)\n', ''))
    moved('a third edit in the moved block fails', 1, 'DIFFERS from the base at line 9',
          body=('m.begin()', 'm.cbegin()'))
    moved('a moved line whose indentation holds a comment fails', 1,
          'Player.cpp:8: FAILED: the body line', body=('            CastSpell(', '//          CastSpell('))
    moved('an edit missing from the moved block fails', 1, "the edit 'GetMap()' stands 0 time(s)",
          body=('GetMap()', 'Map()'))
    moved('the statement standing outside the window fails', 1, 'DIFFERS from the base at line 7',
          swap=('            Cast(flag);\n        }\n    }\n', '        }\n    }\n    Cast(flag);\n'))
    moved('the statement missing fails', 1, '0 call(s) of Cast, expected 1', swap=('            Cast(flag);\n', ''))
    moved('the statement passing another argument fails', 1, 'does not stand as a whole statement',
          swap=('Cast(flag)', 'Cast(apply)'))
    moved('the moved block still standing fails', 1, 'a block the move missed',
          swap=('    }\n}\n', '    }\n    ((Player*)this)->GetMap();\n}\n'))
    moved('a line below the statement is named by its working tree line', 1,
          'fixture:9: DIFFERS from the base at line 16',
          swap=('            Cast(flag);\n        }', '            Cast(flag);\n        };'))
    moved('a window that stops short of the block passes a changed last line', 0, 'around 1/1',
          body=('        }\n    }\n}', '        }\n    } \n}'), window=6)
    moved('... and the window reaching it fails', 1, 'DIFFERS from the base at line 15',
          body=('        }\n    }\n}', '        }\n    } \n}'), window=7)
    moved('a moved body calling floor(itr->first) fails', 1, "Player.cpp:8: FAILED: floor(itr->first): an "
          "unqualified overloaded call binds by the file's includes",
          body=('this, itr->first', 'this, floor(itr->first)'))
    for line, want in [('x = std::cos(angle);', 'std::cos(angle)'), ('x = ::sqrt(d) + 1;', '::sqrt(d)'),
                       ('x = atan2(double(y), x);', 'atan2(double(y), x)'), ('x = float(cos(double(angle)));', None),
                       ('x = cos(float(angle));', 'cos(float(angle))'),
                       ('x = sin(static_cast<float>(angle));', 'sin(static_cast<float>(angle))'),
                       ('x = tan((int)a);', 'tan((int)a)'), ('x = cos(static_cast<double>(angle));', None),
                       ('x = pow(static_cast<double>(a), 2.0) + fabs((double)b); // cos(c)', None),
                       ('x = Cosine(a) + m.sin(a) + p->tan(a) + Foo::log(a) + "abs(a)";', None)]:
        ok = unspelled_math('    ' + line) == want
        print('self-test: %-72s %s' % ('the include check on %s' % line[:52], 'PASS' if ok else 'FAIL'))
        if not ok:
            failures.append('the include check on %r: %r (want %r)' % (line, unspelled_math('    ' + line), want))

    def pinned(label, want_rc, needle, body=('', ''), base=('', ''), edits=()):
        got = []
        form = dict(SELF_MOVED_FORMS['Cast'], body=['    Report(flag);'], edits=list(edits),
                    base_body=[b[8:].replace(*base) for b in SELF_MOVED_OLD.split('\n')[7:15]])
        tree = 'void Player::Cast(AuraState flag)\n{\n    Report(flag);\n}\n'.replace(*body)
        rc = verify('fixture', SELF_MOVED_OLD, SELF_MOVED_NEW, {'forms': {'Cast': 1}, 'added': [], 'window': {8: 8}},
                    got.append, read={'Player.cpp': tree}.__getitem__, forms={'Cast': form})
        ok = rc == want_rc and needle in '\n'.join(got)
        print('self-test: %-72s %s' % (label, 'PASS' if ok else 'FAIL'))
        if not ok:
            failures.append('%s: rc %d (want %d)\n%s' % (label, rc, want_rc, '\n'.join(got)))

    pinned('a pinned body pastes its base body back in place of its statement', 0, 'around 1/1 call(s) pasted back')
    pinned('a pinned body line changed fails', 1, 'Player.cpp:3: FAILED: the body of',
           body=('Report(flag)', 'Report(0)'))
    pinned('a base body line changed fails', 1, 'fixture:8: DIFFERS from the base at line 13', base=('true', 'false'))
    pinned('a pinned body with edits fails', 1, 'takes a body and a base_body and no edits',
           edits=[('((Player*)this)->GetMap()', 'GetMap()')])

    def inlined(label, want_rc, needle, swap=('', ''), old=('', ''), header='', window=11, edits=None):
        got = []
        form = dict(SELF_INLINED_FORM, edits=SELF_INLINED_FORM['edits'] if edits is None else edits)
        rc = verify('fixture', SELF_INLINED_OLD.replace(*old), SELF_INLINED_NEW.replace(*swap),
                    {'forms': {'Knock': 1}, 'added': [], 'window': {5: window}}, got.append,
                    read={'base:Session.cpp': SELF_INLINED_BASE, 'Session.cpp': 'void WorldSession::Other()\n{\n}\n',
                          'Session.h': '        void Other();\n' + header}.__getitem__,
                    forms={'Knock': form})
        ok = rc == want_rc and needle in '\n'.join(got)
        print('self-test: %-72s %s' % (label, 'PASS' if ok else 'FAIL'))
        if not ok:
            failures.append('%s: rc %d (want %d)\n%s' % (label, rc, want_rc, '\n'.join(got)))

    inlined('an inlined block spelled float(cos(double(angle))) is its one call', 0,
            'around 1/1 call(s) pasted back')
    inlined('an inlined body line changed fails', 1, 'fixture:6: FAILED: ', swap=('float(cos', 'float(sin'))
    inlined('an edit that stands nowhere in the base body fails', 1, "the edit 'tan(angle)' stands 0 time(s)",
            edits=[('tan(angle)', 'float(cos(double(angle)))')])
    inlined('an inlined cos(angle) fails, read as its base line', 1, 'fixture:6: FAILED: cos(angle): an unqualified '
            "overloaded call binds by the file's includes", swap=('float(cos(double(angle)))', 'cos(angle)'), edits=[])
    inlined('m_motion-> in place of MotionState() fails', 1, "'        Send(m_motion->Apply(params, speed));' does "
            "not read as the base body of 'void WorldSession::Knock(float angle, float speed)' at Session.cpp:6",
            swap=('MotionState().', 'm_motion->'))
    inlined('the dropped line still standing fails', 1, "the dropped line 'Player* player = GetPlayer();' still stands",
            swap=('        Params', '        Player* player = GetPlayer();\n        Params'))
    inlined('the session method still declared fails', 1, 'Session.h: FAILED: Knock is still declared',
            header='        void Knock(float angle, float speed);\n')
    inlined('an argument changed at the base fails', 1, 'fixture:5: DIFFERS from the base at line 5',
            old=('Knock(angle, speed)', 'Knock(-angle, speed)'))
    inlined('the block wrapped in a guard the base has not fails', 1, 'fixture:6: DIFFERS from the base at line 4',
            swap=('        Params params;\n        params.x = float(cos(double(angle)));\n'
                  '        Send(MotionState().Apply(params, speed));',
                  '        if (speed > 0)\n        {\n            Params params;\n'
                  '            params.x = float(cos(double(angle)));\n'
                  '            Send(MotionState().Apply(params, speed));\n        }'))
    inlined('a window of 0 passes a line appended below the block', 0, 'around 1/1',
            swap=('speed));\n', 'speed));\n        Jump(angle, speed);\n'), window=0)
    inlined('... and a window of 1 fails it', 1, 'fixture:8: DIFFERS from the base at line 6',
            swap=('speed));\n', 'speed));\n        Jump(angle, speed);\n'), window=1)

    def local(label, want_rc, needle, swap=('', ''), form=None, old=('', '')):
        got = []
        forms = dict(SELF_LOCAL_FORMS, **(form or {}))
        rc = verify('fixture', SELF_LOCAL_OLD.replace(*old), SELF_LOCAL_NEW.replace(*old).replace(*swap),
                    {'forms': {'Fits': 1, 'GetPoints': 1, 'unitPlayer': 1}, 'added': [], 'window': {3: 9}},
                    got.append, forms=forms)
        ok = rc == want_rc and needle in '\n'.join(got)
        print('self-test: %-72s %s' % (label, 'PASS' if ok else 'FAIL'))
        if not ok:
            failures.append('%s: rc %d (want %d)\n%s' % (label, rc, want_rc, '\n'.join(got)))

    local('a pointer local turned into a flag pastes back with its calls', 0, 'around 3/3 call(s) pasted back')
    local('the flag line spelled with != fails', 1, "0 flag line(s) 'bool unitPlayer = GetTypeId() == TYPEID_PLAYER;'",
          swap=('GetTypeId() == TYPEID_PLAYER;', 'GetTypeId() != TYPEID_PLAYER;'))
    local('a flag line pinned under another name fails', 1, 'is not the pointer line',
          form={'unitPlayer': dict(SELF_LOCAL_FORMS['unitPlayer'],
                                   direct='bool isPlayer = GetTypeId() == TYPEID_PLAYER;')})
    local('a flag line pinned with a parenthesis dropped fails', 1, 'is not the pointer line',
          form={'unitPlayer': dict(SELF_LOCAL_FORMS['unitPlayer'],
                                   direct='bool unitPlayer = (GetTypeId() == TYPEID_PLAYER;')})
    local('a unitPlayer-> left over fails', 1, "fixture:11: FAILED: the local unitPlayer stands as neither",
          swap=('unitPlayer ? GetPoints()', 'unitPlayer ? unitPlayer->GetPoints()'))
    local('a test the local reads as if (unitPlayer) passes', 0, 'around 3/3',
          old=('if (unitPlayer && spellProto->HasAttribute(1))', 'if (unitPlayer)'))
    local('a form whose receiver is not the local fails', 1, 'Fits is no listed FORM whose cast spelling',
          form={'Fits': {'direct': 'Fits(', 'cast': 'player->Fits(', 'suffix': None}})
    local('the pointer line still standing fails', 1, 'the pointer line',
          swap=('    bool unitPlayer',
                '    Player* unitPlayer = (GetTypeId() == TYPEID_PLAYER) ? (Player*)this : NULL;\n    bool unitPlayer'))

    def assigned(label, want_rc, needle, swap=('', ''), form=None, spec=None):
        got = []
        full = dict({'forms': {'ResetView': 1, 'Grant': 1, 'assigned': 1}, 'added': [], 'window': {6: 7},
                     'changed': [('        player->Grant(this, 1);', '        player->Grant(player, 1);')],
                     'deleted': [['    if (IsPet())', '    {', '        player->Grant(player, 0);', '    }']]},
                    **(spec or {}))
        rc = verify('fixture', SELF_ASSIGNED_OLD, SELF_ASSIGNED_NEW.replace(*swap), full, got.append, window=2,
                    forms=dict(SELF_ASSIGNED_FORMS, **(form or {})))
        ok = rc == want_rc and needle in '\n'.join(got)
        print('self-test: %-72s %s' % (label, 'PASS' if ok else 'FAIL'))
        if not ok:
            failures.append('%s: rc %d (want %d)\n%s' % (label, rc, want_rc, '\n'.join(got)))

    assigned('a flag assigned in its type test pastes back; a comment naming it passes', 0, 'around 3/3')
    assigned('the flag declared true fails', 1, 'is not declared three lines above it',
             swap=('bool player = false;', 'bool player = true;'))
    assigned('the flag assigned 1 fails', 1, "0 flag line(s) 'player = true;'", swap=('player = true;', 'player = 1;'))
    assigned('a declaration pinned under another type fails', 1, 'is not the pointer line',
             form={'assigned': dict(SELF_ASSIGNED_FORMS['assigned'],
                                    declared=('int player = 0;', 'Player* player = NULL;'))})
    assigned('the pointer argument without its changed line fails', 1, 'fixture:12: DIFFERS from the base at line 12',
             spec={'changed': []})
    assigned('a cast in a deleted block is no site', 1, '2 cast call(s) of Grant at the base, the spec lists 1',
             spec={'deleted': []})
    assigned('a deleted block that does not stand at the base fails', 1,
             "the deleted block '    if (IsPet())' stands 0",
             spec={'deleted': [['    if (IsPet())', '    {', '        Drop();', '    }']]})
    local('a pointer line spelled with static_cast pastes back', 0, 'around 3/3',
          old=('(GetTypeId() == TYPEID_PLAYER) ? (Player*)this : NULL',
               'GetTypeId() == TYPEID_PLAYER ? static_cast<Player*>(this): NULL'),
          form={'unitPlayer': dict(SELF_LOCAL_FORMS['unitPlayer'], cast='Player* unitPlayer = GetTypeId() == '
                                   'TYPEID_PLAYER ? static_cast<Player*>(this): NULL;')})

    def dropped(label, want_rc, needle, swap=('', ''), body=('', ''), line=None, window=6):
        got = []
        form = dict(SELF_DROPPED_FORM, dropped=line or SELF_DROPPED_FORM['dropped'])
        rc = verify('fixture', SELF_DROPPED_OLD, SELF_DROPPED_NEW.replace(*swap),
                    {'forms': {'Report': 1}, 'added': [], 'window': {5: window}}, got.append,
                    read={'Player.cpp': SELF_DROPPED_BODY.replace(*body)}.__getitem__, forms={'Report': form})
        ok = rc == want_rc and needle in '\n'.join(got)
        print('self-test: %-72s %s' % (label, 'PASS' if ok else 'FAIL'))
        if not ok:
            failures.append('%s: rc %d (want %d)\n%s' % (label, rc, want_rc, '\n'.join(got)))

    dropped('a dropped line and its block paste back, two edits on one line', 0, 'around 1/1 call(s) pasted back')
    dropped('a dropped line changed fails', 1, 'fixture:5: DIFFERS from the base at line 5',
            line='Player const* player = (GetTypeId() == TYPEID_PLAYER ? (Player*)this : NULL);')
    dropped('a dropped line without the cast spelling fails', 1, 'holds no', line='Player* player = NULL;')
    dropped('a dropped line still standing fails', 1, 'still stands',
            swap=('    Report(',
                  '    Player* player = (GetTypeId() == TYPEID_PLAYER ? (Player*)this : NULL);\n    Report('))
    dropped('a body line below the dropped line changed fails', 1, 'DIFFERS from the base at line 9',
            body=('SetLastError(error)', 'SetLastError(0)'))
    dropped('the statement wrapped in a guard the base has not fails', 1, 'DIFFERS from the base at line 4',
            swap=('    Report(error);', '    if (GetTypeId() == TYPEID_PLAYER)\n    {\n        Report(error);\n    }'))
    dropped('a window that stops at the block passes a statement appended to it', 0, 'around 1/1',
            body=('    }\n}', '    }\n    Pick();\n}'), window=5)
    dropped('... and the window one line past it fails', 1, 'DIFFERS from the base at line 11',
            body=('    }\n}', '    }\n    Pick();\n}'))

    def reported(label, want_rc, needle, swap=('', ''), body=('', ''), spec=None):
        got = []
        rc = verify('fixture', SELF_REPORTED_OLD, SELF_REPORTED_NEW.replace(*swap),
                    dict(SELF_REPORTED_SPEC, **(spec or {})), got.append, window=6,
                    read={'Group.h': SELF_REPORTED_BODY.replace(*body)}.__getitem__, forms=SELF_REPORTED_FORMS)
        ok = rc == want_rc and needle in '\n'.join(got)
        print('self-test: %-72s %s' % (label, 'PASS' if ok else 'FAIL'))
        if not ok:
            failures.append('%s: rc %d (want %d)\n%s' % (label, rc, want_rc, '\n'.join(got)))

    reported('reported facts paste back as their callbacks, one folded into its guard', 0,
             'around 3/3 call(s) pasted back, with 0 added line(s) dropped and 1 changed')
    reported('a callback body line changed fails', 1, 'fixture:7: DIFFERS from the base at line 9',
             body=('SetGroupUpdateFlag(fact.flag);\n        }\n    };\n    c.aura',
                   'SetGroupUpdateFlag(fact.flag | 1);\n        }\n    };\n    c.aura'))
    reported('the wrong flag at a site fails', 1, 'fixture:7: DIFFERS from the base at line 9',
             swap=('Stat{FLAG_HP}', 'Stat{FLAG_MAX}'))
    reported('the aura callback reporting a stat fails', 1, 'does not stand as a whole statement reporting a Aura',
             swap=('Report(Of(m).stat, Stat{FLAG_HP})', 'Report(Of(m).aura, Stat{FLAG_HP})'))
    reported('the type guard dropped fails', 1, 'fixture:5: DIFFERS from the base at line 8',
             swap=('    if (GetTypeId() == TYPEID_PLAYER)\n    {\n        Report(Of(m).stat, Stat{FLAG_HP});\n    }\n',
                   '    Report(Of(m).stat, Stat{FLAG_HP});\n'))
    reported('the statement outside its guard fails', 1, 'fixture:8: DIFFERS from the base at line 8',
             swap=('        Report(Of(m).stat, Stat{FLAG_HP});\n    }\n',
                   '    }\n    Report(Of(m).stat, Stat{FLAG_HP});\n'))
    reported('a folded guard not listed as changed fails', 1, 'is not listed as changed from', spec={'changed': []})
    reported('the folded site not listed as folded fails', 1, 'fixture:17: DIFFERS from the base at line 19',
             spec={'folded': {}})
    reported('a callback that does not use its fact fails', 1, 'does not use each of',
             body=('owner->SetAuraUpdateMask(fact.slot)', 'owner->SetAuraUpdateMask(0)'))
    reported('a cast pair left standing fails', 1, 'still stands: a site the seam missed',
             swap=('    SetValue(lvl);\n', '    SetValue(lvl);\n    ((Player*)this)->SetGroupUpdateFlag(0);\n'))
    reported('a dropped line left standing fails', 1, 'still stands',
             swap=('        Report(Of(m).aura', '        Player* player = (Player*)this;\n        Report(Of(m).aura'))
    reported('the casts of a member deleted whole are no sites', 1,
             '3 cast call(s) of Stat at the base, the spec lists 2', spec={'deleted': []})
    reported('a deleted member still standing fails', 1, 'the deleted member',
             swap=('void Unit::UpdateAura', 'void Unit::Unused(uint32 val)\n{\n}\n\nvoid Unit::UpdateAura'))

    def owner(label, want_rc, needle, swap=('', ''), body=('', ''), old=('', ''), spec=None):
        got = []
        rc = verify('fixture', OWNER_REPORTED_OLD.replace(*old), OWNER_REPORTED_NEW.replace(*swap),
                    dict(OWNER_REPORTED_SPEC, **(spec or {})), got.append, window=6,
                    read={'Group.h': OWNER_REPORTED_BODY.replace(*body)}.__getitem__, forms=OWNER_REPORTED_FORMS)
        ok = rc == want_rc and needle in '\n'.join(got)
        print('self-test: %-72s %s' % (label, 'PASS' if ok else 'FAIL'))
        if not ok:
            failures.append('%s: rc %d (want %d)\n%s' % (label, rc, want_rc, '\n'.join(got)))

    owner('owner reports paste back through the owner cast, one guard written back thrice', 0,
          'around 3/3 call(s) pasted back, with 0 added line(s) dropped and 3 changed')
    owner('the flag set on the pet and the slot on the owner fails', 1, '0 cast call(s) of PetAura pasted back',
          body=('owner->SetGroupUpdateFlag(fact.flag);\n            fact.pet->SetAuraUpdateMask(fact.slot);',
                'fact.pet->SetGroupUpdateFlag(fact.flag);\n            owner->SetAuraUpdateMask(fact.slot);'))
    owner('the slot reported on another pet fails', 1, 'fixture:37: DIFFERS from the base at line 38',
          swap=('slot, pet}', 'slot, this}'))
    owner('the pet aura reported through the stat callback fails', 1, 'does not stand as a whole statement',
          swap=('Of(owner->m).petAura', 'Of(owner->m).stat'))
    owner('an owner guard that lost its type test fails', 1, 'is not listed as changed from',
          swap=('        if (owner && (owner->GetTypeId() == TYPEID_PLAYER))\n        {\n'
                '            Report(Of(owner->m).stat, Stat{FLAG_PET_MAX});',
                '        if (owner)\n        {\n            Report(Of(owner->m).stat, Stat{FLAG_PET_MAX});'))
    owner('a guard holding || does not fold without its parentheses', 1, 'is not listed as changed from',
          old=('if (owner && (owner', 'if (owner || (owner'), swap=('if (owner && (owner', 'if (owner || (owner'),
          spec={'changed': [('        if (owner || (owner->GetTypeId() == TYPEID_PLAYER))',
                             '        if (owner || (owner->GetTypeId() == TYPEID_PLAYER) && '
                             '((Player*)owner)->GetGroup())')]})
    owner('a guard holding an assignment does not fold without its parentheses', 1,
          'is not listed as changed from',
          old=('if (owner && (owner', 'if ((owner = owner) && (owner'),
          swap=('if (owner && (owner', 'if ((owner = owner) && (owner'),
          spec={'changed': [('        if ((owner = owner) && (owner->GetTypeId() == TYPEID_PLAYER))',
                             '        if ((owner = owner) && (owner->GetTypeId() == TYPEID_PLAYER) && '
                             '((Player*)owner)->GetGroup())')]})
    owner('the pet aura form unlisted: its cast counts as the stat form\'s at the base', 1,
          '3 cast call(s) of OwnerStat at the base, the spec lists 2', spec={'forms': {'OwnerStat': 2}})

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
