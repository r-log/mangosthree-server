#!/usr/bin/env python3
"""cast_sites.py: the call sites cast_verbatim.py proves; cast_verbatim.py's docstring holds the rules.

BASE   the commit --check reads each file at: the parent of the rewrite.
FORMS  name -> the direct spelling of a rewritten call and the cast spelling it stands for (the
       keys: the comment above it and cast_verbatim.py).
FILES  file -> the count of each FORM rewritten in it and the lines the rewrite added or changed
       (the keys: the comment above it and cast_verbatim.py).

A rewrite edits this file, never cast_verbatim.py. The file holds assignments only, each to one of
the three names or to a spelling aid of its own (a name beginning with `_`: the blocks Unit.h and
Player.h gained, spelt once); split_gate.py refuses to run cast_verbatim.py on any other statement or
name, and on a tool that binds one of the three itself.

Player.h:2098, the selection guid, is the one BYVALUE entry.

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
The spell-mod sites read the player's spell modifiers through the holder getter. The radius site,
UnitAuraProcHandler.cpp:1921, stands 16 lines below its type return (:1905) and sets its own window;
its direct spelling carries its leading space, since :4838 calls the getter on the caster, a CHANGED
line in :4840's window. The pushback and global cooldown sites cast `m_caster`, not `this`:
Spell.cpp:653 stands 24 lines below Delayed's type return (:629) and :699 13 below DelayedChannel's
(:686), so both set their own window, and SpellCooldown.cpp:121 stands 2 below its type test (:119).
Those three calls dropped the unused spell argument, so each is a CHANGED line, as are Spell.cpp:1297
and :1298, which dropped it outside every window. SpellPower.cpp:178, the owner line that cast
`m_caster` before a Unit member, is a CHANGED line with no FORM, and so is Unit.cpp's owner line in
the combo target site's window (:4719). GetSpellModOwner is a deleted member. Spell.cpp calls
GetItemByGuid through its own `Player` pointers to the caster (:248, :775), so that FORM is
`elsewhere` there.
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

BASE = '26df9c56a'

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
                      'suffix': None, 'elsewhere': ['src/game/WorldHandlers/Spell.cpp']},
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
    'ApplySpellMod': {'direct': ' GetSpellMods()->ApplySpellMod(',
                      'cast': ' ((Player*)this)->ApplySpellMod(',
                      'suffix': None},
    'CasterApplySpellMod': {'direct': 'm_caster->GetSpellMods()->ApplySpellMod(',
                            'cast': '((Player*)m_caster)->ApplySpellMod(',
                            'suffix': None},
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

_UNIT_H_COMBAT_STATS = '''        /**
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

_UNIT_H_ITEM_BY_GUID = '''        /**
         * The item this unit holds under a guid; the proc handlers ask it for the item an aura was
         * cast from.
         * @return NULL here; Player returns the item its inventory holds under that guid, or NULL
         */
        virtual Item* GetItemByGuid(ObjectGuid /*guid*/) const { return NULL; }'''

_UNIT_H_REPUTATION = '''
    protected:
        /**
         * The rank this unit holds with a faction; the Shattered Sun pendants' proc picks its spell
         * by the Aldor's or the Scryers' rank.
         * @param faction_id the faction asked about
         * @return REP_NEUTRAL here; Player returns its rank by its reputation with that faction
         */
        virtual ReputationRank GetReputationRank(uint32 /*faction_id*/) const { return REP_NEUTRAL; }

    public:'''

_UNIT_H_MOUNT_PET = '''
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

_UNIT_H_VISIBILITY = '''
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

_UNIT_H_COOLDOWNS = '''
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

_UNIT_H_COMBO_POINTS = '''        /**
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

_UNIT_H_RAGE = '''        /**
         * Awards rage from a hit dealt or taken; DealDamage awards it to a rage user for its
         * main-hand and off-hand weapon hits.
         * Does nothing here; Player converts the damage, and for a hit it dealt the weapon speed
         * factor, into rage and adds it to its power.
         */
        virtual void RewardRage(uint32 /*damage*/, uint32 /*weaponSpeedHitFactor*/, bool /*attacker*/) { }'''

_UNIT_H_KILL_CREDIT = '''        /**
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

_UNIT_H_FACTION_GHOST_SPEED = '''        /**
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

_UNIT_H_PROC_ONE_OFFS = '''        /**
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

_UNIT_H_TALENT_RANK = '''        /**
         * The rank of a talent this unit knows; SpellBonusWithCoeffs raises a death knight's attack
         * power bonus by the rank of Impurity it knows.
         * @param talentId the talent asked about
         * @return NULL here, a unit that is not a player knows no talent; Player returns the spell of
         * the rank its active spec knows, or NULL
         */
        virtual SpellEntry const* GetKnownTalentRankById(int32 /*talentId*/) const { return NULL; }'''

_UNIT_H_RUNE_COOLDOWN = '''        /**
         * Whether every base rune of a type is on cooldown; Blade Barrier procs only for a death
         * knight whose base blood runes are all on cooldown.
         * @param runeType the rune type asked about
         * @return false here, a unit that is not a player has no runes; the proc's player and class
         * tests return before asking a unit that is not a player; Player returns what its rune
         * manager answers
         */
        virtual bool IsBaseRuneSlotsOnCooldown(RuneType /*runeType*/) const { return false; }'''

_UNIT_H_AURA_STATE_CASTS = '''        /**
         * Casts the passive spells this unit knows whose caster aura state is the flag; ModifyAuraState
         * calls it on a player when it sets that aura state.
         * @param flag the aura state set
         * Does nothing here, a unit that is not a player knows no spells; Player casts on itself,
         * triggered, every passive spell in its spell map that is not removed and whose caster aura
         * state is the flag
         */
        virtual void CastPassiveSpellsForAuraState(AuraState /*flag*/) { }'''

_UNIT_H_OWN_SESSION_PACKETS = '''        /**
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

_UNIT_H_ACCOUNT_SECURITY = '''        /**
         * The security level of the account playing this unit; a game master in GM mode sees a player
         * whose level is not above its own.
         * @return 0 here, a player's level; IsVisibleForOrDetect asks only when both units are
         * players, so the default is not observed there; Player returns what the query its session
         * installed reads
         */
        virtual uint32 GetAccountSecurityLevel() const { return 0; }'''

_UNIT_H_POSITION_AND_MOVING = '''        /**
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

_UNIT_H_CLIENT_CONTROL = '''        /**
         * Gives or takes the client's control of a unit's movement; the fear and confuse states call it
         * on a player when the first of them takes hold and when the last one ends.
         * @param target the unit whose movement is given or taken
         * @param allowMove 1 gives the control, 0 takes it
         * Does nothing here; Player hands the mover authority to or from its session and tells its
         * client.
         */
        virtual void SetClientControl(Unit* /*target*/, uint8 /*allowMove*/) { }'''

_UNIT_H_NEAR_TELEPORT = '''        /**
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

_UNIT_H_DAMAGE_CREDIT = '''        /**
         * Credits the damage this unit dealt to a victim: its battleground score when both are players,
         * and its achievement criteria; DealDamage calls it on a player that hit another unit.
         * @param pVictim the unit that took the damage
         * @param damage the damage dealt
         * Does nothing here, a unit that is not a player has no score and no criteria; Player credits its
         * battleground and its achievements.
         */
        virtual void CreditDamageDealt(Unit* /*pVictim*/, uint32 /*damage*/) { }'''

_UNIT_H_SPELL_DAMAGE = '''        /**
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

_UNIT_H_POSSESS = '''        /**
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

_PLAYER_H_ITEM_BY_GUID = '''
        // The item Unit's proc handlers ask for by guid: the one the inventory holds, or NULL; private,
        // so only a call through Unit reaches it
        Item* GetItemByGuid(ObjectGuid guid) const override { return GetInventoryMgr().GetItemByGuid(guid); }'''

_PLAYER_H_TALENT_RANK = '''
        // The rank of a talent Unit's spell bonus asks for: the one the talent manager knows, or NULL;
        // private, so only a call through Unit reaches it
        SpellEntry const* GetKnownTalentRankById(int32 talentId) const override final
        {
            return GetTalentMgr().GetKnownTalentRankById(talentId);
        }'''

_PLAYER_H_RUNE_COOLDOWN = '''
        // Whether every base rune of a type is on cooldown, which Unit's Blade Barrier proc asks: what
        // the rune manager answers; private, so only a call through Unit reaches it
        bool IsBaseRuneSlotsOnCooldown(RuneType runeType) const override final
        {
            return GetRuneMgr().IsBaseRuneSlotsOnCooldown(runeType);
        }'''

_PLAYER_H_AURA_STATE_CASTS = '''
        // Casts the passive spells the player knows whose caster aura state is the flag, which Unit's
        // ModifyAuraState asks for when it sets that aura state; private, so only a call through Unit
        // reaches it
        void CastPassiveSpellsForAuraState(AuraState flag) override;'''

_PLAYER_H_OWN_SESSION_PACKETS = '''
        // Tells the client the stand state Unit's SetStandState set; private, so only a call through
        // Unit reaches it
        void SendStandStateUpdate(uint8 state) override;

        // Tells the client a melee swing error that differs from the last one told and remembers it,
        // which Unit's UpdateMeleeAttackingState reports; private, so only a call through Unit reaches it
        void ReportSwingError(uint8 swingError) override;'''

_PLAYER_H_ACCOUNT_SECURITY = '''
        // The security level of the account playing this player, which Unit's GM visibility rule
        // compares: what the query its session installed reads at this call; private, so only a call
        // through Unit reaches it
        uint32 GetAccountSecurityLevel() const override final
        {
            MANGOS_ASSERT(m_clientCallbacks.securityLevel);
            return m_clientCallbacks.securityLevel();
        }'''

_PLAYER_H_NEAR_TELEPORT = '''
        // Teleports the player a short way on its own map, keeping its transport, combat and pet, which
        // Unit's NearTeleportTo asks for; private, so only a call through Unit reaches it
        void TeleportNear(float x, float y, float z, float orientation, bool casting) override;'''

_PLAYER_H_DAMAGE_CREDIT = '''
        // Credits the damage the player dealt to a victim, its battleground score and its achievement
        // criteria, which Unit's DealDamage asks for; private, so only a call through Unit reaches it
        void CreditDamageDealt(Unit* pVictim, uint32 damage) override;'''

_PLAYER_H_CAMERA = '''
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
                  (_UNIT_H_COMBAT_STATS, '        MeleeHitOutcome RollMeleeOutcomeAgainst(const Unit* pVictim, '
                                        'WeaponAttackType attType, int32 crit_chance, int32 miss_chance, '
                                        'int32 dodge_chance, int32 parry_chance, int32 block_chance) const;'),
                  (_UNIT_H_ITEM_BY_GUID + '\n' + _UNIT_H_REPUTATION,
                   '        bool IsTriggeredAtSpellProcEvent(Unit* pVictim, '
                   'SpellAuraHolder* holder, SpellEntry const* procSpell, uint32 procFlag, '
                   'uint32 procExtra, WeaponAttackType attType, bool isVictim, '
                   'SpellProcEventEntry const*& spellProcEvent);'),
                  (_UNIT_H_MOUNT_PET, '        void Unmount(bool from_aura = false);'),
                  ('class Transport;', 'class Totem;'),
                  ('struct CreatureInfo;', 'struct SpellEntryExt;'),
                  (_UNIT_H_VISIBILITY, '        bool canDetectInvisibilityOf(Unit const* u) const;'),
                  (_UNIT_H_COOLDOWNS + '\n' + _UNIT_H_COMBO_POINTS + '\n' + _UNIT_H_RAGE + '\n' + _UNIT_H_KILL_CREDIT
                   + '\n' + _UNIT_H_FACTION_GHOST_SPEED + '\n' + _UNIT_H_PROC_ONE_OFFS + '\n' + _UNIT_H_TALENT_RANK
                   + '\n' + _UNIT_H_RUNE_COOLDOWN + '\n' + _UNIT_H_AURA_STATE_CASTS + '\n' + _UNIT_H_OWN_SESSION_PACKETS
                   + '\n' + _UNIT_H_ACCOUNT_SECURITY + '\n' + _UNIT_H_POSITION_AND_MOVING
                   + '\n' + _UNIT_H_CLIENT_CONTROL + '\n' + _UNIT_H_NEAR_TELEPORT + '\n' + _UNIT_H_DAMAGE_CREDIT
                   + '\n' + _UNIT_H_SPELL_DAMAGE + '\n' + _UNIT_H_POSSESS,
                   '        virtual void ProhibitSpellSchool(SpellSchoolMask /*idSchoolMask*/, '
                   'uint32 /*unTimeMs*/) { }'),
                  ('        SpellCooldownMgr m_spellCooldownMgr;', '        AuraContainer m_auras;')]},
    'src/game/entities/player/Player.h': {
        'forms': {},
        'declares': True,
        'added': [("        // The private GetItemByGuid override answers only Unit's lookup.",
                   '        // GetItemDisplayIdInSlot, IsValidPos and the static position checks are called on it '
                   'directly.'),
                  (_PLAYER_H_ITEM_BY_GUID + '\n' + _PLAYER_H_TALENT_RANK + '\n' + _PLAYER_H_RUNE_COOLDOWN
                   + '\n' + _PLAYER_H_AURA_STATE_CASTS + '\n' + _PLAYER_H_OWN_SESSION_PACKETS
                   + '\n' + _PLAYER_H_ACCOUNT_SECURITY + '\n' + _PLAYER_H_NEAR_TELEPORT + '\n' + _PLAYER_H_DAMAGE_CREDIT
                   + '\n' + _PLAYER_H_CAMERA,
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
        'changed': [('    if (SpellModMgr* modOwner = GetSpellMods())',
                     '    if (Player* modOwner = GetSpellModOwner())'),
                    ('    if (GetTypeId() == TYPEID_PLAYER)',
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
        'deleted': ['Player* Unit::GetSpellModOwner() const',
                    ['        if (possessedCreature->IsPet() && possessedCreature->GetObjectGuid() == GetPetGuid())',
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
                  'Say': 1, 'GetSelectionGuid': 1, 'GetNextRandomRaidMember': 1, 'IsBaseRuneSlotsOnCooldown': 1,
                  'ApplySpellMod': 1},
        'added': [],
        'changed': [('            caster->GetSpellMods()->ApplySpellMod(spellProto->ID, SPELLMOD_RADIUS, radius);',
                     '            caster->ApplySpellMod(spellProto->ID, SPELLMOD_RADIUS, radius, NULL);')],
        'window': {980: 44, 1002: 13, 1010: 21, 1045: 13, 1070: 13, 1096: 13, 1921: 16, 2881: 70, 3242: 47,
                   4840: 17}},
    'src/game/WorldHandlers/Spell.cpp': {
        'forms': {'CasterApplySpellMod': 2},
        'added': [],
        'changed': [('    ((Player*)m_caster)->ApplySpellMod(m_spellInfo->ID, SPELLMOD_NOT_LOSE_CASTING_TIME, '
                     'delayReduce);',
                     '    ((Player*)m_caster)->ApplySpellMod(m_spellInfo->ID, SPELLMOD_NOT_LOSE_CASTING_TIME, '
                     'delayReduce, this);'),
                    ('            modOwner->ApplySpellMod(m_spellInfo->ID, SPELLMOD_RADIUS, radius);',
                     '            modOwner->ApplySpellMod(m_spellInfo->ID, SPELLMOD_RADIUS, radius, this);'),
                    ('            modOwner->ApplySpellMod(m_spellInfo->ID, SPELLMOD_JUMP_TARGETS, EffectChainTarget);',
                     '            modOwner->ApplySpellMod(m_spellInfo->ID, SPELLMOD_JUMP_TARGETS, EffectChainTarget, '
                     'this);')],
        'window': {653: 24, 699: 13}},
    'src/game/WorldHandlers/SpellCooldown.cpp': {
        'forms': {'CasterApplySpellMod': 1},
        'added': [],
        'changed': [('            ((Player*)m_caster)->ApplySpellMod(m_spellInfo->ID, SPELLMOD_GLOBAL_COOLDOWN, gcd);',
                     '            ((Player*)m_caster)->ApplySpellMod(m_spellInfo->ID, SPELLMOD_GLOBAL_COOLDOWN, gcd, '
                     'this);')]},
    'src/game/WorldHandlers/SpellPower.cpp': {
        'forms': {},
        'added': [],
        'changed': [('                            if (SpellModMgr* modOwner = m_caster->GetSpellMods())',
                     '                            if (Player* modOwner = ((Player*)m_caster)->GetSpellModOwner())')]},
}
