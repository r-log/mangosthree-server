#!/usr/bin/env python3
"""verbatim_sites.py: the spell handler moves verbatim.py proves; verbatim.py's docstring holds the rules.

BASE                the commit --check reads each file at: the parent of the latest move.
ORIGINAL            the original of every file whose entry names none (ORIGINALS in verbatim.py).
VOID_SUBSTITUTIONS  the outcome pairs every site in a function returning void ends its substitutions with.
SITES               one entry per file, its sites in the order they stand in it.
BLOCKS              one entry per block of type definitions moved from a source file into a header.

A move edits this file, never verbatim.py: its sites' entries, and BASE, or its block's entry. The file
holds assignments only, each to one of the five names or to a spelling aid of its own (a name beginning
with `_`), each value a literal (split_gate.py's value rule: constants, lists, tuples, dicts, + and *,
names assigned above it, dict(...) with keywords); split_gate.py refuses to run verbatim.py on any other
statement, name or value, and on a tool that binds or changes one of the five itself.
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

# The tree the moved bodies are checked against: the parent of the latest move. ORIGINAL (master
# before the first move) is the original of every file whose entry names none (ORIGINALS in verbatim.py).
BASE = 'e463e6415'
ORIGINAL = 'afdabc428'

VOID_SUBSTITUTIONS = [('return SpellHandlerOutcome<void>::Return();', 'return;'),
                      ('return SpellHandlerOutcome<void>::Continue();', 'break;')]

# One entry per file; each file lists its sites in the order they stand in it, and may name its
# `original` (ORIGINALS in verbatim.py).
SITES = {
    'src/game/WorldHandlers/SpellAuraDummy.cpp': {
        'handlers': 'src/game/spells/handlers/AuraDummyHandlers.cpp',
        'rows_function': 'RegisterAuraDummyRows',
        'added': ['#include "spells/handlers/AuraDummyHandlers.h"'],
        'tail_marker': '// Decoupling D11 (design/2026-09-28-unit-reopening.md 3(b)): the spell handler registry\'s',
        'cuts': [
            (['                    case 48025:                             // Headless Horseman\'s Mount'], 3),
            (['                    case 54729:                             // Winged Steed of the Ebon Blade'], 3),
            (['                    case 58600:                             // Restricted Flight Area'], 10),
            (['                    case 71342:                             // Big Love Rocket'], 3),
            (['                    case 72286:                             // Invincible'], 3),
            (['                    case 74856:                             // Blazing Hippogryph'], 3),
            (['                    case 75614:                             // Celestial Steed'], 3),
            (['                    case 75973:                             // X-53 Touring Rocket'], 3)],
        'sites': [{
            'name': 'HandleAuraDummy AT APPLY, SPELLFAMILY_WARRIOR (switch (GetId()))',
            'dispatch': [
                '                AuraDummyApplyContext ctx(this, target);',
                '                if (SpellHandlerRegistry::Game().Dispatch<AuraDummyApplyWarriorSite>(GetId(), ctx).IsReturn())',
                '                {',
                '                    return;',
                '                }'],
            'open': ['                switch (GetId())', '                {'],
            'close': ['                }'],
            'label_indent': 20,
            'table': 'warriorApply',
            'traits': 'AuraDummyApplyWarriorSite',
            'context': 'AuraDummyApplyContext',
            'live_outs': ['target'],
            'members_of': ('src/game/WorldHandlers/SpellAuras.h', 'Aura'),
            'substitutions': [('ctx.target', 'target'), ('ctx.aura', 'this')] + VOID_SUBSTITUTIONS,
            'labels': {
                41099: '                    case 41099:                             // Battle Stance',
                41100: '                    case 41100:                             // Berserker Stance',
                41101: '                    case 41101:                             // Defensive Stance',
                53790: '                    case 53790:                             // Defensive Stance',
                53791: '                    case 53791:                             // Berserker Stance',
                53792: '                    case 53792:                             // Battle Stance'},
        }, {
            'name': 'HandleAuraDummy AT APPLY, SPELLFAMILY_WARRIOR, Overpower\'s Unrelenting Assault '
                    '(switch ((*itr)->GetSpellProto()->ID), in the loop)',
            'dispatch': [
                '                            AuraDummyUnrelentingAssaultContext assaultCtx(target, itr);',
                '                            if (SpellHandlerRegistry::Game()'
                '.Dispatch<AuraDummyUnrelentingAssaultSite>(',
                '                                    (*itr)->GetSpellProto()->ID, assaultCtx).IsReturn())',
                '                            {',
                '                                return;',
                '                            }'],
            'open': ['                            switch ((*itr)->GetSpellProto()->ID)',
                     '                            {'],
            'close': ['                            }'],
            'label_indent': 32,
            'traits': 'AuraDummyUnrelentingAssaultSite',
            'default': '                                default:',
            'context': 'AuraDummyUnrelentingAssaultContext',
            'live_outs': ['target', 'itr'],
            'in_scope': ['apply', 'Real', 'classOptions', 'caster', 'modifierAuras'],
            'members_of': ('src/game/WorldHandlers/SpellAuras.h', 'Aura'),
            'substitutions': [('ctx.target', 'target'), ('ctx.itr', 'itr')] + VOID_SUBSTITUTIONS,
            'labels': {
                46859: '                                case 46859:                 // Unrelenting Assault, rank 1',
                46860: '                                case 46860:                 // Unrelenting Assault, rank 2'},
            'gone': (18, 3),
        }, {
            'name': 'HandleAuraDummy AT REMOVE, the hunter quest-tame block (switch (GetId()))',
            'dispatch': [
                '            AuraDummyQuestTameContext tameCtx(finalSpellId);',
                '            if (SpellHandlerRegistry::Game()'
                '.Dispatch<AuraDummyQuestTameSite>(GetId(), tameCtx).IsReturn())',
                '            {',
                '                return;',
                '            }'],
            'open': ['            switch (GetId())', '            {'],
            'close': ['            }'],
            'label_indent': 16,
            'traits': 'AuraDummyQuestTameSite',
            'context': 'AuraDummyQuestTameContext',
            'live_outs': ['finalSpellId'],
            'in_scope': ['apply', 'Real', 'target', 'classOptions', 'caster'],
            'members_of': ('src/game/WorldHandlers/SpellAuras.h', 'Aura'),
            'substitutions': [('ctx.finalSpellId', 'finalSpellId')] + VOID_SUBSTITUTIONS,
            'labels': {
                19548: '                case 19548: {body}',
                19674: '                case 19674: {body}',
                19687: '                case 19687: {body}',
                19688: '                case 19688: {body}',
                19689: '                case 19689: {body}',
                19692: '                case 19692: {body}',
                19693: '                case 19693: {body}',
                19694: '                case 19694: {body}',
                19696: '                case 19696: {body}',
                19697: '                case 19697: {body}',
                19699: '                case 19699: {body}',
                19700: '                case 19700: {body}',
                30646: '                case 30646: {body}',
                30653: '                case 30653: {body}',
                30654: '                case 30654: {body}',
                30099: '                case 30099: {body}',
                30102: '                case 30102: {body}',
                30105: '                case 30105: {body}'},
        }, {
            'name': 'HandleAuraDummy AT REMOVE (switch (GetId()), family-independent)',
            'dispatch': [
                '        AuraDummyRemoveContext ctx(this, target);',
                '        if (SpellHandlerRegistry::Game().Dispatch<AuraDummyRemoveSite>(GetId(), ctx).IsReturn())',
                '        {',
                '            return;',
                '        }',
                ''],
            'open': ['        switch (GetId())', '        {'],
            'close': ['        }'],
            'residual': True,
            'label_indent': 12,
            'traits': 'AuraDummyRemoveSite',
            'context': 'AuraDummyRemoveContext',
            'live_outs': ['target'],
            'in_scope': ['apply', 'Real', 'classOptions'],
            'members_of': ('src/game/WorldHandlers/SpellAuras.h', 'Aura'),
            'substitutions': [('ctx.target', 'target'), ('ctx.aura->GetRemoveMode()', 'm_removeMode'),
                              ('ctx.aura->GetCaster()', 'GetCaster()'),
                              ('ctx.aura->GetSpellProto()', 'GetSpellProto()'),
                              ('ctx.aura', 'this')] + VOID_SUBSTITUTIONS,
            'labels': {
                10255: '            case 10255:                                     // Stoned',
                12479: '            case 12479:                                     // Hex of Jammal\'an',
                12774: '            case 12774:                                     '
                       '// (DND) Belnistrasz Idol Shutdown Visual',
                28169: '            case 28169:                                     // Mutating Injection',
                32045: '            case 32045:                                     // Soul Charge',
                32051: '            case 32051:                                     // Soul Charge',
                32052: '            case 32052:                                     // Soul Charge',
                32286: '            case 32286:                                     // Focus Target Visual',
                35079: '            case 35079:                                     // Misdirection, triggered buff',
                59628: '            case 59628:                                     '
                       '// Tricks of the Trade, triggered buff',
                36730: '            case 36730:                                     // Flame Strike',
                41099: '            case 41099:                                     // Battle Stance',
                41100: '            case 41100:                                     // Berserker Stance',
                41101: '            case 41101:                                     // Defensive Stance',
                42454: '            case 42454:                                     // Captured Totem',
                42517: '            case 42517:                                     // Beam to Zelfrax',
                43681: '            case 43681:                                     // Inactive',
                43969: '            case 43969:                                     // Feathered Charm',
                44191: '            case 44191:                                     // Flame Strike',
                45934: '            case 45934:                                     // Dark Fiend',
                45963: '            case 45963:                                     // Call Alliance Deserter',
                46308: '            case 46308:                                     // Burning Winds',
                46637: '            case 46637:                                     // Break Ice',
                48385: '            case 48385:                                     // Create Spirit Fount Beam',
                50141: '            case 50141:                                     // Blood Oath',
                51405: '            case 51405:                                     // Digging for Treasure',
                51870: '            case 51870:                                     // Collect Hair Sample',
                52098: '            case 52098:                                     // Charge Up',
                53039: '            case 53039:                                     // Deploy Parachute',
                53790: '            case 53790:                                     // Defensive Stance',
                53791: '            case 53791:                                     // Berserker Stance',
                53792: '            case 53792:                                     // Battle Stance',
                56511: '            case 56511:                                     '
                       '// Towers of Certain Doom: Tower Bunny Smoke Flare Effect',
                58600: '            case 58600:                                     // Restricted Flight Area',
                61900: '            case 61900:                                     // Electrical Charge',
                68839: '            case 68839:                                     // Corrupt Soul'},
            'deleted': [43681, 58600],
        }, {
            'name': 'HandleAuraDummy AT APPLY & REMOVE, SPELLFAMILY_GENERIC (switch (GetId()))',
            'dispatch': [
                '            AuraDummyApplyRemoveContext ctx(this, target, apply);',
                '            if (SpellHandlerRegistry::Game()'
                '.Dispatch<AuraDummyApplyRemoveGenericSite>(GetId(), ctx).IsReturn())',
                '            {',
                '                return;',
                '            }',
                ''],
            'open': ['            switch (GetId())', '            {'],
            'close': ['            }'],
            'residual': True,
            'label_indent': 16,
            'traits': 'AuraDummyApplyRemoveGenericSite',
            'context': 'AuraDummyApplyRemoveContext',
            'live_outs': ['target', 'apply'],
            'in_scope': ['Real', 'classOptions'],
            'members_of': ('src/game/WorldHandlers/SpellAuras.h', 'Aura'),
            'substitutions': [('ctx.target', 'target'), ('ctx.apply', 'apply'),
                              ('ctx.aura->GetCasterGuid()', 'GetCasterGuid()'), ('ctx.aura->GetId()', 'GetId()'),
                              ('ctx.aura', 'this')] + VOID_SUBSTITUTIONS,
            'labels': {
                6606: '                case 6606:                                  '
                       '// Self Visual - Sleep Until Cancelled (DND)',
                11196: '                case 11196:                                 // Recently Bandaged',
                24658: '                case 24658:                                 // Unstable Power',
                24661: '                case 24661:                                 // Restless Strength',
                29266: '                case 29266:                                 // Permanent Feign Death',
                31261: '                case 31261:                                 // Permanent Feign Death (Root)',
                37493: '                case 37493:                                 // Feign Death',
                52593: '                case 52593:                                 // Bloated Abomination Feign Death',
                55795: '                case 55795:                                 // Falling Dragon Feign Death',
                57626: '                case 57626:                                 // Feign Death',
                57685: '                case 57685:                                 // Permanent Feign Death',
                58768: '                case 58768:                                 '
                       '// Permanent Feign Death (Freeze Jumpend)',
                58806: '                case 58806:                                 '
                       '// Permanent Feign Death (Drowned Anim)',
                58951: '                case 58951:                                 // Permanent Feign Death',
                64461: '                case 64461:                                 '
                       '// Permanent Feign Death (No Anim) (Root)',
                65985: '                case 65985:                                 '
                       '// Permanent Feign Death (Root Silence Pacify)',
                70592: '                case 70592:                                 // Permanent Feign Death',
                70628: '                case 70628:                                 // Permanent Feign Death',
                70630: '                case 70630:                                 // Frozen Aftermath - Feign Death',
                71598: '                case 71598:                                 // Feign Death',
                35356: '                case 35356:                                 // Spawn Feign Death',
                35357: '                case 35357:                                 // Spawn Feign Death',
                42557: '                case 42557:                                 // Feign Death',
                51329: '                case 51329:                                 // Feign Death',
                40133: '                case 40133:                                 // Summon Fire Elemental',
                40132: '                case 40132:                                 // Summon Earth Elemental',
                40214: '                case 40214:                                 // Dragonmaw Illusion',
                42515: '                case 42515:                                 // Jarl Beam',
                42583: '                case 42583:                                 // Claw Rage',
                68987: '                case 68987:                                 // Pursuit',
                43874: '                case 43874:                                 '
                       '// Scourge Mur\'gul Camp: Force Shield Arcane Purple x3',
                47178: '                case 47178:                                 // Plague Effect Self',
                56422: '                case 56422:                                 // Nerubian Submerge',
                70733: '                case 70733:                                 // Stoneform',
                58204: '                case 58204:                                 // LK Intro VO (1)',
                58205: '                case 58205:                                 // LK Intro VO (2)',
                27978: '                case 27978:',
                40131: '                case 40131:',
                66936: '                case 66936:                                     // Submerge',
                66948: '                case 66948:                                     // Submerge'},
        }, {
            'name': 'HandleAuraDummy AT APPLY & REMOVE, SPELLFAMILY_DRUID (switch (GetId()))',
            'dispatch': [
                '            AuraDummyApplyRemoveContext ctx(this, target, apply);',
                '            if (SpellHandlerRegistry::Game().Dispatch<AuraDummyDruidSite>(GetId(), ctx).IsReturn())',
                '            {',
                '                return;',
                '            }'],
            'open': ['            switch (GetId())', '            {'],
            'close': ['            }'],
            'label_indent': 16,
            'traits': 'AuraDummyDruidSite',
            'context': 'AuraDummyApplyRemoveContext',
            'live_outs': ['target', 'apply'],
            'in_scope': ['Real', 'classOptions'],
            'members_of': ('src/game/WorldHandlers/SpellAuras.h', 'Aura'),
            'substitutions': [('ctx.target', 'target'), ('ctx.apply', 'apply'),
                              ('ctx.aura->GetModifier()->', 'm_modifier.'), ('ctx.aura', 'this')] + VOID_SUBSTITUTIONS,
            'labels': {
                52610: '                case 52610:                                 // Savage Roar',
                61336: '                case 61336:                                 // Survival Instincts'},
        }, {
            'name': 'HandleAuraDummy AT APPLY & REMOVE, SPELLFAMILY_DRUID, Improved Moonkin Form (switch (GetId()))',
            'dispatch': [
                '                AuraDummyImprovedMoonkinContext imfCtx(this, spell_id);',
                '                if (SpellHandlerRegistry::Game()'
                '.Dispatch<AuraDummyImprovedMoonkinSite>(GetId(), imfCtx).IsReturn())',
                '                {',
                '                    return;',
                '                }'],
            'open': ['                switch (GetId())', '                {'],
            'close': ['                }'],
            'label_indent': 20,
            'traits': 'AuraDummyImprovedMoonkinSite',
            'default': '                    default:',
            'context': 'AuraDummyImprovedMoonkinContext',
            'live_outs': ['spell_id'],
            'in_scope': ['apply', 'Real', 'target', 'classOptions'],
            'members_of': ('src/game/WorldHandlers/SpellAuras.h', 'Aura'),
            'substitutions': [('ctx.spell_id', 'spell_id'), ('ctx.aura->GetId()', 'GetId()'),
                              ('ctx.aura', 'this')] + VOID_SUBSTITUTIONS,
            'labels': {
                48384: '                    case 48384: {body}    // Rank 1',
                48395: '                    case 48395: {body}    // Rank 2',
                48396: '                    case 48396: {body}    // Rank 3'},
            'gone': (5, 16),
        }],
    },
    'src/game/WorldHandlers/SpellAuraShapeshift.cpp': {
        'handlers': 'src/game/spells/handlers/AuraShapeshiftHandlers.cpp',
        'rows_function': 'RegisterAuraShapeshiftRows',
        'added': ['#include "spells/handlers/AuraShapeshiftHandlers.h"'],
        'sites': [{
            'name': 'HandleAuraTransform AT APPLY, no creature entry (switch (GetId()))',
            'dispatch': [
                '            AuraTransformContext ctx(GetId(), target);',
                '            if (SpellHandlerRegistry::Game().Dispatch<AuraTransformSite>(GetId(), ctx).IsReturn())',
                '            {',
                '                return;',
                '            }'],
            'open': ['            switch (GetId())', '            {'],
            'close': ['            }'],
            'label_indent': 16,
            'traits': 'AuraTransformSite',
            'default': '                default:',
            'context': 'AuraTransformContext',
            'live_outs': ['target'],
            'in_scope': ['apply', 'Real'],
            'members_of': ('src/game/WorldHandlers/SpellAuras.h', 'Aura'),
            'substitutions': [('ctx.target', 'target'), ('ctx.spellId', 'GetId()')] + VOID_SUBSTITUTIONS,
            'labels': {
                16739: '                case 16739:                                 // Orb of Deception',
                42365: '                case 42365:                                 // Murloc costume',
                50517: '                case 50517:                                 // Dread Corsair',
                51926: '                case 51926:                                 // Corsair Costume',
                65386: '                case 65386:                                 // Honor the Dead',
                65495: '                case 65495:',
                65528: '                case 65528:                                 '
                       "// Gossip NPC Appearance - Pirates' Day",
                65529: '                case 65529:                                 '
                       '// Gossip NPC Appearance - Day of the Dead (DotD)',
                71450: '                case 71450:                                 // Crown Parcel Service Uniform'},
        }],
    },
    'src/game/WorldHandlers/SpellAuraControl.cpp': {
        'handlers': 'src/game/spells/handlers/AuraControlHandlers.cpp',
        'rows_function': 'RegisterAuraControlRows',
        'added': ['#include "spells/handlers/AuraControlHandlers.h"'],
        'sites': [{
            'name': 'HandleModThreat (switch (GetId()))',
            'dispatch': [
                '    AuraThreatContext ctx(target, level_diff, multiplier);',
                '    if (SpellHandlerRegistry::Game().Dispatch<AuraThreatSite>(GetId(), ctx).IsReturn())',
                '    {',
                '        return;',
                '    }'],
            'open': ['    switch (GetId())', '    {'],
            'close': ['    }'],
            'label_indent': 8,
            'traits': 'AuraThreatSite',
            'context': 'AuraThreatContext',
            'live_outs': ['target', 'level_diff', 'multiplier'],
            'in_scope': ['apply', 'Real'],
            'members_of': ('src/game/WorldHandlers/SpellAuras.h', 'Aura'),
            'substitutions': [('ctx.target', 'target'), ('ctx.level_diff', 'level_diff'),
                              ('ctx.multiplier', 'multiplier')] + VOID_SUBSTITUTIONS,
            'labels': {
                26400: '        case 26400:',
                28862: '        case 28862:'},
        }],
    },
    'src/game/WorldHandlers/SpellEffectTail.cpp': {
        'handlers': 'src/game/spells/handlers/SpellEffectTailHandlers.cpp',
        'rows_function': 'RegisterSpellEffectTailRows',
        'added': ['#include "spells/handlers/SpellEffectTailHandlers.h"'],
        'sites': [{
            'name': 'EffectTransmitted (switch (m_spellInfo->ID))',
            'dispatch': [
                '    SpellEffectTransmittedContext ctx(m_caster, name_id);',
                '    if (SpellHandlerRegistry::Game().Dispatch<SpellEffectTransmittedSite>(m_spellInfo->ID, ctx).IsReturn())',
                '    {',
                '        return;',
                '    }'],
            'open': ['    switch (m_spellInfo->ID)', '    {'],
            'close': ['    }'],
            'label_indent': 8,
            'traits': 'SpellEffectTransmittedSite',
            'default': '        default:',
            'context': 'SpellEffectTransmittedContext',
            'live_outs': ['name_id'],
            'in_scope': ['effect'],
            'members_of': ('src/game/WorldHandlers/Spell.h', 'Spell'),
            'substitutions': [('ctx.m_caster', 'm_caster'), ('ctx.name_id', 'name_id')] + VOID_SUBSTITUTIONS,
            'labels': {
                29886: '        case 29886: // Create Soulwell'},
        }],
    },
    'src/game/WorldHandlers/SpellAuraPeriodic.cpp': {
        'handlers': 'src/game/spells/handlers/AuraPeriodicHandlers.cpp',
        'rows_function': 'RegisterAuraPeriodicRows',
        'added': ['#include "spells/handlers/AuraPeriodicHandlers.h"'],
        'sites': [{
            'name': 'HandleAuraProcTriggerSpell (switch (GetId()))',
            'dispatch': [
                '    AuraProcTriggerContext ctx(this, target, apply);',
                '    if (SpellHandlerRegistry::Game().Dispatch<AuraProcTriggerSite>(GetId(), ctx).IsReturn())',
                '    {',
                '        return;',
                '    }'],
            'open': ['    switch (GetId())', '    {'],
            'close': ['    }'],
            'label_indent': 8,
            'traits': 'AuraProcTriggerSite',
            'default': '        default:',
            'context': 'AuraProcTriggerContext',
            'live_outs': ['target', 'apply'],
            'in_scope': ['Real'],
            'members_of': ('src/game/WorldHandlers/SpellAuras.h', 'Aura'),
            'substitutions': [('ctx.target', 'target'), ('ctx.apply', 'apply'),
                              ('ctx.aura->GetCaster()', 'GetCaster()'),
                              ('ctx.aura->GetHolder()', 'GetHolder()')] + VOID_SUBSTITUTIONS,
            'labels': {
                28200: '        case 28200:                                         '
                       '// Ascendance (Talisman of Ascendance trinket)',
                50720: '        case 50720:                                         // Vigilance (threat transfering)'},
        }, {
            'name': 'HandlePeriodicTriggerSpell (switch (GetId()))',
            'dispatch': [
                '        AuraPeriodicTriggerContext ctx(this, target);',
                '        if (SpellHandlerRegistry::Game().Dispatch<AuraPeriodicTriggerSite>(GetId(), ctx).IsReturn())',
                '        {',
                '            return;',
                '        }'],
            'open': ['        switch (GetId())', '        {'],
            'close': ['        }'],
            'label_indent': 12,
            'traits': 'AuraPeriodicTriggerSite',
            'default': '            default:',
            'context': 'AuraPeriodicTriggerContext',
            'live_outs': ['target'],
            'in_scope': ['apply'],
            'members_of': ('src/game/WorldHandlers/SpellAuras.h', 'Aura'),
            'substitutions': [('ctx.target', 'target'), ('ctx.aura->GetRemoveMode()', 'm_removeMode'),
                              ('ctx.aura->GetSpellEffect()', 'm_spellEffect'), ('ctx.aura->GetCaster()', 'GetCaster()'),
                              ('ctx.aura->GetEffIndex()', 'GetEffIndex()'),
                              ('ctx.aura->GetSpellProto()', 'GetSpellProto()'),
                              ('ctx.aura', 'this')] + VOID_SUBSTITUTIONS,
            'labels': {
                66: '            case 66:                                        // Invisibility',
                42783: '            case 42783:                                     // Wrath of the Astrom...',
                46221: '            case 46221:                                     // Animal Blood',
                51912: '            case 51912:                                     '
                       '// Ultra-Advanced Proto-Typical Shortening Blaster'},
        }, {
            'name': 'HandlePeriodicEnergize (switch (GetId()))',
            'dispatch': [
                '        AuraPeriodicEnergizeContext ctx(this, target);',
                '        if (SpellHandlerRegistry::Game()'
                '.Dispatch<AuraPeriodicEnergizeSite>(GetId(), ctx).IsReturn())',
                '        {',
                '            return;',
                '        }'],
            'open': ['        switch (GetId())', '        {'],
            'close': ['        }'],
            'label_indent': 12,
            'traits': 'AuraPeriodicEnergizeSite',
            'default': '            default:',
            'context': 'AuraPeriodicEnergizeContext',
            'live_outs': ['target'],
            'in_scope': ['apply', 'Real', 'loading'],
            'members_of': ('src/game/WorldHandlers/SpellAuras.h', 'Aura'),
            'substitutions': [('ctx.target', 'target'), ('ctx.aura->GetModifier()->', 'm_modifier.'),
                              ('ctx.aura->GetCaster()', 'GetCaster()'),
                              ('ctx.aura->GetBasePoints()', 'GetBasePoints()'),
                              ('ctx.aura->GetAuraMaxTicks()', 'GetAuraMaxTicks()'),
                              ('ctx.aura', 'this')] + VOID_SUBSTITUTIONS,
            'labels': {
                54833: '            case 54833:                                     '
                       '// Glyph of Innervate (value%/2 of casters base mana)',
                29166: '            case 29166:                                     '
                       '// Innervate (value% of casters base mana)',
                48391: '            case 48391:                                     // Owlkin Frenzy 2% base mana',
                57669: '            case 57669:                                     // Replenishment (0.2% from max)',
                61782: '            case 61782:                                     // Infinite Replenishment'},
        }, {
            'name': 'HandleAuraPeriodicDummy (switch(GetSpellProto()->ID))',
            'dispatch': [
                '            AuraPeriodicDummyRogueContext ctx(this, target, apply);',
                '            if (SpellHandlerRegistry::Game()'
                '.Dispatch<AuraPeriodicDummyRogueSite>(GetSpellProto()->ID, ctx).IsReturn())',
                '            {',
                '                return;',
                '            }'],
            'open': ['            switch(GetSpellProto()->ID)', '            {'],
            'close': ['            }'],
            'label_indent': 16,
            'traits': 'AuraPeriodicDummyRogueSite',
            'context': 'AuraPeriodicDummyRogueContext',
            'live_outs': ['target', 'apply'],
            'in_scope': ['Real', 'loading'],
            'members_of': ('src/game/WorldHandlers/SpellAuras.h', 'Aura'),
            'substitutions': [('ctx.target', 'target'), ('ctx.apply', 'apply'),
                              ('ctx.aura->GetHolder()', 'GetHolder()')] + VOID_SUBSTITUTIONS,
            'labels': {
                31666: '                case 31666:'},
        }, {
            'name': 'HandleAuraModIncreaseHealth (switch (GetId()))',
            'dispatch': [
                '    AuraIncreaseHealthContext ctx(this, target, apply, Real);',
                '    if (SpellHandlerRegistry::Game().Dispatch<AuraIncreaseHealthSite>(GetId(), ctx).IsReturn())',
                '    {',
                '        return;',
                '    }'],
            'open': ['    switch (GetId())', '    {'],
            'close': ['    }'],
            'label_indent': 8,
            'traits': 'AuraIncreaseHealthSite',
            'default': '        default:',
            'context': 'AuraIncreaseHealthContext',
            'live_outs': ['target', 'apply', 'Real'],
            'members_of': ('src/game/WorldHandlers/SpellAuras.h', 'Aura'),
            'substitutions': [('ctx.target', 'target'), ('ctx.apply', 'apply'), ('ctx.real', 'Real'),
                              ('ctx.aura->GetModifier()->', 'm_modifier.')] + VOID_SUBSTITUTIONS,
            'labels': {
                54443: '        case 54443:                                         '
                       '// Demonic Empowerment (Voidwalker)',
                55233: '        case 55233:                                         // Vampiric Blood',
                61254: '        case 61254:                                         '
                       '// Will of Sartharion (Obsidian Sanctum)',
                12976: '        case 12976:                                         '
                       '// Warrior Last Stand triggered spell',
                28726: '        case 28726:                                         '
                       '// Nightmare Seed ( Nightmare Seed )',
                31616: "        case 31616:                                         // Nature's Guardian",
                34511: '        case 34511:                                         '
                       '// Valor (Bulwark of Kings, Bulwark of the Ancient Kings)',
                44055: '        case 44055: case 55915: case 55917: case 67596:     '
                       "// Tremendous Fortitude (Battlemaster's Alacrity)",
                55915: '        case 44055: case 55915: case 55917: case 67596:     '
                       "// Tremendous Fortitude (Battlemaster's Alacrity)",
                55917: '        case 44055: case 55915: case 55917: case 67596:     '
                       "// Tremendous Fortitude (Battlemaster's Alacrity)",
                67596: '        case 44055: case 55915: case 55917: case 67596:     '
                       "// Tremendous Fortitude (Battlemaster's Alacrity)",
                50322: '        case 50322:                                         // Survival Instincts',
                53479: '        case 53479:                                         // Hunter pet - Last Stand',
                59465: "        case 59465:                                         // Brood Rage (Ahn'Kahet)"},
        }],
    },
    'src/game/WorldHandlers/SpellEffectHealPower.cpp': {
        'original': 'dfb6969ac09ba68864d8d3a32477e512242cb89c',
        'handlers': 'src/game/spells/handlers/SpellEffectHealPowerHandlers.cpp',
        'rows_function': 'RegisterSpellEffectHealPowerRows',
        'added': ['#include "spells/handlers/SpellEffectHealPowerHandlers.h"'],
        'sites': [{
            'name': 'EffectEnergize (switch (m_spellInfo->ID))',
            'dispatch': [
                '    SpellEffectEnergizeContext ctx(m_caster, unitTarget, damage, level_diff, level_multiplier);',
                '    if (SpellHandlerRegistry::Game().Dispatch<SpellEffectEnergizeSite>(m_spellInfo->ID, ctx).IsReturn())',
                '    {',
                '        return;',
                '    }'],
            'open': ['    switch (m_spellInfo->ID)', '    {'],
            'close': ['    }'],
            'label_indent': 8,
            'traits': 'SpellEffectEnergizeSite',
            'default': '        default:',
            'context': 'SpellEffectEnergizeContext',
            'live_outs': ['level_diff', 'level_multiplier', 'damage'],
            'in_scope': ['effect', 'power'],
            'members_of': ('src/game/WorldHandlers/Spell.h', 'Spell'),
            'substitutions': [('ctx.m_caster', 'm_caster'), ('ctx.unitTarget', 'unitTarget'),
                              ('ctx.damage', 'damage'), ('ctx.level_diff', 'level_diff'),
                              ('ctx.level_multiplier', 'level_multiplier')] + VOID_SUBSTITUTIONS,
            'labels': {
                9512: '        case 9512:                                          // Restore Energy',
                24571: '        case 24571:                                         // Blood Fury',
                24532: '        case 24532:                                         // Burst of Energy',
                31930: '        case 31930:                                         // Judgements of the Wise',
                48542: '        case 48542:                                         // Revitalize (mana restore case)',
                63375: '        case 63375:                                         // Improved Stormstrike',
                68082: '        case 68082:                                         // Glyph of Seal of Command',
                67487: '        case 67487:                                         // Mana Potion Injector',
                67490: '        case 67490:                                         // Runic Mana Injector'},
        }],
    },
    'src/game/WorldHandlers/SpellEffectObjectCombat.cpp': {
        'handlers': 'src/game/spells/handlers/SpellEffectObjectCombatHandlers.cpp',
        'rows_function': 'RegisterSpellEffectObjectCombatRows',
        'added': ['#include "spells/handlers/SpellEffectObjectCombatHandlers.h"'],
        'sites': [{
            'name': 'EffectActivateObject, custom use (switch (m_spellInfo->ID))',
            'dispatch': [
                '            SpellEffectActivateObjectContext ctx(gameObjTarget, m_caster, m_spellInfo);',
                '            if (SpellHandlerRegistry::Game().Dispatch<SpellEffectActivateObjectSite>(m_spellInfo->ID, ctx).IsReturn())',
                '            {',
                '                return;',
                '            }'],
            'open': ['            switch (m_spellInfo->ID)', '            {'],
            'close': ['            }'],
            'label_indent': 16,
            'traits': 'SpellEffectActivateObjectSite',
            'context': 'SpellEffectActivateObjectContext',
            'live_outs': [],
            'in_scope': ['effect', 'misc_value'],
            'members_of': ('src/game/WorldHandlers/Spell.h', 'Spell'),
            'substitutions': [('ctx.gameObjTarget', 'gameObjTarget'), ('ctx.m_caster', 'm_caster'),
                              ('ctx.m_spellInfo', 'm_spellInfo')] + VOID_SUBSTITUTIONS,
            'labels': {
                24734: '                case 24734:         // Summon Templar Random',
                24744: '                case 24744:         // Summon Templar (fire)',
                24756: '                case 24756:         // Summon Templar (air)',
                24758: '                case 24758:         // Summon Templar (earth)',
                24760: '                case 24760:         // Summon Templar (water)',
                24763: '                case 24763:         // Summon Duke Random',
                24765: '                case 24765:         // Summon Duke (fire)',
                24768: '                case 24768:         // Summon Duke (air)',
                24770: '                case 24770:         // Summon Duke (earth)',
                24772: '                case 24772:         // Summon Duke (water)',
                24784: '                case 24784:         // Summon Royal Random',
                24786: '                case 24786:         // Summon Royal (fire)',
                24788: '                case 24788:         // Summon Royal (air)',
                24789: '                case 24789:         // Summon Royal (earth)',
                24790: '                case 24790:         // Summon Royal (water)',
                40176: '                case 40176:         // Simon Game pre-game Begin, blue',
                40177: '                case 40177:         // Simon Game pre-game Begin, green',
                40178: '                case 40178:         // Simon Game pre-game Begin, red',
                40179: '                case 40179:         // Simon Game pre-game Begin, yellow',
                40283: '                case 40283:         // Simon Game END, blue',
                40284: '                case 40284:         // Simon Game END, green',
                40285: '                case 40285:         // Simon Game END, red',
                40286: '                case 40286:         // Simon Game END, yellow',
                40494: '                case 40494:         // Simon Game, switched ON',
                40495: '                case 40495:         // Simon Game, switched OFF',
                40512: '                case 40512:         // Simon Game, switch...disable Off switch',
                40632: '                case 40632:         // Summon Gezzarak the Huntress',
                40640: '                case 40640:         // Summon Karrog',
                40642: '                case 40642:         // Summon Darkscreecher Akkarai',
                40644: '                case 40644:         // Summon Vakkiz the Windrager',
                41004: '                case 41004:         // Summon Terokk',
                46085: '                case 46085:         // Place Fake Fur',
                46592: '                case 46592:         // Summon Ahune Lieutenant'},
        }, {
            'name': 'EffectResurrect (switch (m_spellInfo->ID))',
            'dispatch': [
                '    SpellEffectResurrectContext ctx(m_caster, m_CastItem, m_spellInfo);',
                '    if (SpellHandlerRegistry::Game().Dispatch<SpellEffectResurrectSite>(m_spellInfo->ID, ctx).IsReturn())',
                '    {',
                '        return;',
                '    }'],
            'open': ['    switch (m_spellInfo->ID)', '    {'],
            'close': ['    }'],
            'label_indent': 8,
            'traits': 'SpellEffectResurrectSite',
            'default': '        default:',
            'context': 'SpellEffectResurrectContext',
            'live_outs': [],
            'members_of': ('src/game/WorldHandlers/Spell.h', 'Spell'),
            'substitutions': [('ctx.m_caster', 'm_caster'), ('ctx.m_CastItem', 'm_CastItem'),
                              ('ctx.m_spellInfo', 'm_spellInfo')] + VOID_SUBSTITUTIONS,
            'labels': {
                8342: '        case 8342:                                          // Defibrillate (Goblin Jumper Cables) has 33% chance on success',
                22999: '        case 22999:                                         // Defibrillate (Goblin Jumper Cables XL) has 50% chance on success',
                54732: '        case 54732:                                         // Defibrillate (Gnomish Army Knife) has 67% chance on success'},
        }],
    },
    'src/game/WorldHandlers/SpellTargeting.cpp': {
        'original': 'e463e64156898754a41237e69f06570df14369f4',
        'handlers': 'src/game/spells/handlers/SpellTargetingHandlers.cpp',
        'rows_function': 'RegisterSpellTargetingRows',
        'added': ['#include "spells/handlers/SpellTargetingHandlers.h"'],
        'sites': [{
            'name': 'SetTargetMap, TARGET_ALL_ENEMY_IN_AREA (switch (m_spellInfo->ID))',
            'dispatch': [
                '            {',
                '                SpellTargetAllEnemyInAreaContext ctx(m_caster, targetUnitMap, unMaxTargets);',
                '                if (SpellHandlerRegistry::Game().Dispatch<SpellTargetAllEnemyInAreaSite>(m_spellInfo->ID, ctx).IsReturn())',
                '                {',
                '                    return;',
                '                }',
                '            }'],
            'open': ['            switch (m_spellInfo->ID)', '            {'],
            'close': ['            }'],
            'label_indent': 16,
            'traits': 'SpellTargetAllEnemyInAreaSite',
            'default': '                default:',
            'context': 'SpellTargetAllEnemyInAreaContext',
            'live_outs': ['targetUnitMap', 'unMaxTargets'],
            'in_scope': ['effIndex', 'targetMode', 'spellEffect', 'classOpt', 'EffectChainTarget', 'radius',
                         'tempTargetGOList'],
            'members_of': ('src/game/WorldHandlers/Spell.h', 'Spell'),
            'substitutions': [('ctx.m_caster', 'm_caster'), ('ctx.targetUnitMap', 'targetUnitMap'),
                              ('ctx.unMaxTargets', 'unMaxTargets')] + VOID_SUBSTITUTIONS,
            'labels': {
                30769: '                case 30769:                                 // Pick Red Riding Hood',
                30843: '                case 30843:                                 // Enfeeble',
                31347: '                case 31347:                                 // Doom',
                37676: '                case 37676:                                 // Insidious Whisper',
                38028: '                case 38028:                                 // Watery Grave',
                40618: '                case 40618:                                 // Insignificance',
                41376: '                case 41376:                                 // Spite',
                62166: '                case 62166:                                 // Stone Grip',
                63981: '                case 63981:                                 // Stone Grip (h)',
                42005: '                case 42005:                                 // Bloodboil (spell hits only the 5 furthest away targets)'},
        }, {
            'name': 'SetTargetMap, TARGET_EFFECT_SELECT, SPELL_EFFECT_DUMMY (switch (m_spellInfo->ID))',
            'dispatch': [
                '                    SpellTargetEffectDummyContext ctx(this, m_caster, m_spellInfo, m_targets, targetUnitMap);',
                '                    if (SpellHandlerRegistry::Game().Dispatch<SpellTargetEffectDummySite>(m_spellInfo->ID, ctx).IsReturn())',
                '                    {',
                '                        return;',
                '                    }'],
            'open': ['                    switch (m_spellInfo->ID)', '                    {'],
            'close': ['                    }'],
            'label_indent': 24,
            'traits': 'SpellTargetEffectDummySite',
            'default': '                        default:',
            'context': 'SpellTargetEffectDummyContext',
            'live_outs': ['targetUnitMap'],
            'in_scope': ['effIndex', 'targetMode', 'spellEffect', 'classOpt', 'EffectChainTarget', 'unMaxTargets',
                         'radius', 'tempTargetGOList'],
            'members_of': ('src/game/WorldHandlers/Spell.h', 'Spell'),
            'substitutions': [('ctx.m_caster', 'm_caster'), ('ctx.m_spellInfo', 'm_spellInfo'),
                              ('ctx.m_targets', 'm_targets'), ('ctx.targetUnitMap', 'targetUnitMap'),
                              ('ctx.spell->FindCorpseUsing', 'FindCorpseUsing'),
                              ('ctx.spell->SendCastResult', 'SendCastResult'),
                              ('ctx.spell->finish', 'finish')] + VOID_SUBSTITUTIONS,
            'labels': {
                20577: '                        case 20577:                         // Cannibalize'},
        }],
    },
}

# One entry per block moved from a source file into a header: base, origin, header, first, lines, added
# (BLOCKS in verbatim.py).
BLOCKS = [{
    'base': '5f5b94ca226655fec0a31b9ce0a76805f262338c',
    'origin': 'src/game/WorldHandlers/SpellTargeting.cpp',
    'header': 'src/game/WorldHandlers/SpellTargetDistanceOrder.h',
    'first': '// Helper for targets furthest away to the spell target',
    'lines': 20,
    'added': ['#include "SpellTargetDistanceOrder.h"',
              'template WorldObject* Spell::FindCorpseUsing<MaNGOS::CannibalizeObjectCheck>();'],
}]
