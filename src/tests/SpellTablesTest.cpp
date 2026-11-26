/**
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2026 MaNGOS <https://www.getmangos.eu>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

#include "TestHarness.h"

#include "Auth/BigNumber.h"
#include "Creature.h"
#include "DBCStores.h"
#include "Player.h"
#include "WorldSession.h"
#include "data/ServerSpellStore.h"
#include "spells/auras/SpellAuras.h"
#include "spells/data/SpellMgr.h"
#include "spells/data/SpellTables.h"

#include <algorithm>
#include <cstddef>
#include <vector>

TEST(SpellTables_TheGooberSpellsAnswerTheirReplacementsAndAnyOtherIdItself)
{
    CHECK_EQ(GetGooberCastSpell(34448), uint32(26566));
    CHECK_EQ(GetGooberCastSpell(34452), uint32(26572));
    CHECK_EQ(GetGooberCastSpell(37639), uint32(36326));
    CHECK_EQ(GetGooberCastSpell(45367), uint32(45371));
    CHECK_EQ(GetGooberCastSpell(45370), uint32(45368));
    CHECK_EQ(GetGooberCastSpell(12345), uint32(12345));
    CHECK_EQ(GetGooberCastSpell(0), uint32(0));
}

TEST(SpellTables_FerociousInspirationAnswersItsPetAuraAndNothingElse)
{
    CHECK_EQ(GetFerociousInspirationPetAura(34460), uint32(75447));
    // The two lower ranks are not 4.3.4 spells: no player knows them.
    CHECK_EQ(GetFerociousInspirationPetAura(34455), uint32(0));
    CHECK_EQ(GetFerociousInspirationPetAura(34459), uint32(0));
    CHECK_EQ(GetFerociousInspirationPetAura(12345), uint32(0));
}

TEST(SpellTables_TheFiveCoreHandledMasteryProcsAreTheSetAndNoOther)
{
    for (uint32 spellId : { 76659u, 76669u, 76672u, 76838u, 77222u })
    {
        CHECK(IsCoreHandledMasteryProc(spellId));
    }
    for (uint32 spellId : { 0u, 76658u, 76671u, 77223u, 12345u })
    {
        CHECK(!IsCoreHandledMasteryProc(spellId));
    }
}

TEST(SpellTables_SoulstoneAnswersItsResurrectionAndNothingElse)
{
    CHECK_EQ(GetSoulstoneResurrectionSpell(20707), uint32(3026));
    // The higher ranks are not 4.3.4 spells: no player carries their aura.
    for (uint32 spellId : { 20762u, 20763u, 20764u, 20765u, 27239u, 47883u, 12345u })
    {
        CHECK_EQ(GetSoulstoneResurrectionSpell(spellId), uint32(0));
    }
}

namespace
{
    // The max-targets switch's labels, grouped by the amount each group set.
    uint32 const MAX_TARGETS_1[] =
    {
        802, 804, 23138, 28560, 30541, 30572, 30769, 30835, 31347, 32312, 33711, 37388, 38794, 39338, 39342, 40834,
        41537, 44869, 45391, 45785, 45863, 45892, 45976, 46372, 47669, 48278, 50742, 50988, 51003, 51146, 52438,
        52449, 53457, 54148, 55479, 56140, 57578, 59870, 62016, 62042, 62166, 62301, 62374, 62488, 62577, 62603,
        62797, 62978, 63018, 63024, 63545, 63744, 63745, 63747, 63795, 63820, 63830, 64218, 64234, 64402, 64425,
        64465, 64543, 64623, 65121, 65301, 65872, 65950, 66001, 66152, 66153, 66332, 66336, 66339, 67077, 67281,
        67282, 67283, 67296, 67297, 67298, 68912, 68950, 68987, 69048, 69057, 72088, 73142, 73144, 69140, 69674,
        70450, 70837, 70882, 70920, 71224, 71445, 71471, 71837, 71861, 72091, 72254, 73022, 73023
    };
    uint32 const MAX_TARGETS_2[] =
    {
        10258, 28542, 62476, 63802, 66013, 67755, 67756, 68509, 69055, 69278, 70341, 71336, 71390
    };
    uint32 const MAX_TARGETS_2_TO_3[] =
    {
        61916
    };
    uint32 const MAX_TARGETS_3[] =
    {
        28796, 29213, 30004, 31298, 39341, 39344, 39992, 40869, 41303, 41376, 51904, 54522, 60936, 61693, 62477,
        63981, 64598, 64620, 70814, 72095, 72089, 70826, 73143, 73145
    };
    uint32 const MAX_TARGETS_3_TO_5[] =
    {
        46771
    };
    uint32 const MAX_TARGETS_3_TO_6[] =
    {
        63482
    };
    uint32 const MAX_TARGETS_4[] =
    {
        37676, 38028, 46650, 67757, 71221
    };
    uint32 const MAX_TARGETS_5[] =
    {
        30843, 40243, 42005, 45641, 55665, 58917, 64604, 67076, 67078, 67700, 68510
    };
    uint32 const MAX_TARGETS_7[] =
    {
        61694
    };
    uint32 const MAX_TARGETS_8[] =
    {
        38054
    };
    uint32 const MAX_TARGETS_10[] =
    {
        54098, 54835
    };
    uint32 const MAX_TARGETS_15[] =
    {
        25991
    };
}

TEST(SpellTables_EveryMaxTargetsRowAnswersWhatItsSwitchGroupSet)
{
    for (uint32 spellId : MAX_TARGETS_1)
    {
        SpellMaxTargetsRange const* range = FindSpellMaxTargets(spellId);
        REQUIRE(range != NULL);
        CHECK_EQ(range->lo, uint32(1));
        CHECK_EQ(range->hi, uint32(1));
    }
    for (uint32 spellId : MAX_TARGETS_2)
    {
        SpellMaxTargetsRange const* range = FindSpellMaxTargets(spellId);
        REQUIRE(range != NULL);
        CHECK_EQ(range->lo, uint32(2));
        CHECK_EQ(range->hi, uint32(2));
    }
    for (uint32 spellId : MAX_TARGETS_2_TO_3)
    {
        SpellMaxTargetsRange const* range = FindSpellMaxTargets(spellId);
        REQUIRE(range != NULL);
        CHECK_EQ(range->lo, uint32(2));
        CHECK_EQ(range->hi, uint32(3));
    }
    for (uint32 spellId : MAX_TARGETS_3)
    {
        SpellMaxTargetsRange const* range = FindSpellMaxTargets(spellId);
        REQUIRE(range != NULL);
        CHECK_EQ(range->lo, uint32(3));
        CHECK_EQ(range->hi, uint32(3));
    }
    for (uint32 spellId : MAX_TARGETS_3_TO_5)
    {
        SpellMaxTargetsRange const* range = FindSpellMaxTargets(spellId);
        REQUIRE(range != NULL);
        CHECK_EQ(range->lo, uint32(3));
        CHECK_EQ(range->hi, uint32(5));
    }
    for (uint32 spellId : MAX_TARGETS_3_TO_6)
    {
        SpellMaxTargetsRange const* range = FindSpellMaxTargets(spellId);
        REQUIRE(range != NULL);
        CHECK_EQ(range->lo, uint32(3));
        CHECK_EQ(range->hi, uint32(6));
    }
    for (uint32 spellId : MAX_TARGETS_4)
    {
        SpellMaxTargetsRange const* range = FindSpellMaxTargets(spellId);
        REQUIRE(range != NULL);
        CHECK_EQ(range->lo, uint32(4));
        CHECK_EQ(range->hi, uint32(4));
    }
    for (uint32 spellId : MAX_TARGETS_5)
    {
        SpellMaxTargetsRange const* range = FindSpellMaxTargets(spellId);
        REQUIRE(range != NULL);
        CHECK_EQ(range->lo, uint32(5));
        CHECK_EQ(range->hi, uint32(5));
    }
    for (uint32 spellId : MAX_TARGETS_7)
    {
        SpellMaxTargetsRange const* range = FindSpellMaxTargets(spellId);
        REQUIRE(range != NULL);
        CHECK_EQ(range->lo, uint32(7));
        CHECK_EQ(range->hi, uint32(7));
    }
    for (uint32 spellId : MAX_TARGETS_8)
    {
        SpellMaxTargetsRange const* range = FindSpellMaxTargets(spellId);
        REQUIRE(range != NULL);
        CHECK_EQ(range->lo, uint32(8));
        CHECK_EQ(range->hi, uint32(8));
    }
    for (uint32 spellId : MAX_TARGETS_10)
    {
        SpellMaxTargetsRange const* range = FindSpellMaxTargets(spellId);
        REQUIRE(range != NULL);
        CHECK_EQ(range->lo, uint32(10));
        CHECK_EQ(range->hi, uint32(10));
    }
    for (uint32 spellId : MAX_TARGETS_15)
    {
        SpellMaxTargetsRange const* range = FindSpellMaxTargets(spellId);
        REQUIRE(range != NULL);
        CHECK_EQ(range->lo, uint32(15));
        CHECK_EQ(range->hi, uint32(15));
    }

    // The labels the registry and the family switches hold, and an id no switch held, fix nothing here.
    for (uint32 spellId : { 74452u, 38194u, 12345u, 0u })
    {
        CHECK(FindSpellMaxTargets(spellId) == NULL);
    }
}

TEST(SpellTables_TheFixedRadiiAreTheSwitchesLabelsAndNoOthers)
{
    uint32 const instanceVisibility[] =
    {
        69057, 70826, 72088, 72089, 73142, 73143, 73144, 73145, 72350, 72351, 72706, 72830
    };
    for (uint32 spellId : instanceVisibility)
    {
        CHECK(HasInstanceVisibilityRadius(spellId));
        CHECK(!HasContactRadius(spellId));
    }
    CHECK(HasContactRadius(49376));
    CHECK(!HasInstanceVisibilityRadius(49376));

    // The radius bodies the registry holds, and an id no switch held, take no fixed radius.
    for (uint32 spellId : { 24811u, 28241u, 56438u, 66881u, 12345u })
    {
        CHECK(!HasInstanceVisibilityRadius(spellId));
        CHECK(!HasContactRadius(spellId));
    }
}

TEST(SpellTables_TheFiveFlagSetsHoldTheirSwitchesLiveLabelsAndNoOthers)
{
    for (uint32 spellId : { 17941u, 22008u, 34936u, 51124u, 57761u, 64823u })
    {
        CHECK(IsOneChargeModifierAura(spellId));
    }
    for (uint32 spellId : { 60430u, 64193u, 65737u })
    {
        CHECK(IsHealthToMaxAura(spellId));
    }
    for (uint32 spellId : { 24575u, 24659u, 24662u, 26464u, 55166u, 62519u, 63050u, 64455u, 65294u, 71564u })
    {
        CHECK(IsMaxStackAtCreateAura(spellId));
    }
    for (uint32 spellId : { 45284u, 45297u, 77451u, 88148u })
    {
        CHECK(IsOverloadIgnoringCastTime(spellId));
    }
    for (uint32 spellId : { 36573u, 42316u, 42385u })
    {
        CHECK(MayTargetTaxiFlyer(spellId));
    }

    // The switches' ids that are not 4.3.4 spells are in no set, and neither is an id no switch held. Hot Streak's
    // buff (48108) is in none: in 4.3.4 it holds no modifier effect and no charge. Nor is 54741, a Firestarter buff
    // that no 4.3.4 talent or spell casts.
    for (uint32 spellId : { 31834u, 34754u, 44401u, 48108u, 54741u, 12345u })
    {
        CHECK(!IsOneChargeModifierAura(spellId));
    }
    for (uint32 spellId : { 34027u, 58914u, 74396u, 12345u })
    {
        CHECK(!IsMaxStackAtCreateAura(spellId));
    }
    for (uint32 spellId : { 45286u, 45296u, 49239u, 49240u, 45298u, 45302u, 49268u, 49269u, 2120u, 12345u })
    {
        CHECK(!IsOverloadIgnoringCastTime(spellId));
    }
    CHECK(!IsHealthToMaxAura(12345));
    CHECK(!MayTargetTaxiFlyer(12345));
}

TEST(SpellTables_TheAuraTargetLimitsAreTheTablesRowsAndNoOther)
{
    // Living Bomb: "Limit 3 targets."
    CHECK_EQ(AuraTargetLimit(44457), uint32(3));

    // Its explosion's three is how many enemies the explosion hits, not a limit on the caster's auras; Pyroblast, Focus
    // Magic ("Limit 1 target", a single-target spell) and an id no table holds have none.
    for (uint32 spellId : { 44461u, 11366u, 54646u, 12345u, 0u })
    {
        CHECK_EQ(AuraTargetLimit(spellId), uint32(0));
    }
}

TEST(SpellTables_TheChillsLivingBombsExplosionMoltenArmorsHitAndDivineTouchsHealAreTheTriggeredCastsThatStartProcs)
{
    // Ice Shards' Chilled at both ranks and Piercing Chill's chill, which the talents' auras cast, Living Bomb's
    // explosion, which the bomb's aura casts when it runs out, Molten Armor's hit, and Divine Touch's heal, which the
    // talent's aura casts at the target of the priest's Renew
    for (uint32 spellId : { 12484u, 12485u, 83154u, 44461u, 34913u, 63544u })
    {
        CHECK(TriggeredCastStartsProcs(spellId));
    }

    // Frost Armor's Chilled, which shares Ice Shards' Chilled's class bit, Shattered Barrier's freeze, Living Bomb
    // itself, Ignite's DoT, Fire Power's explosion, Flame Orb's hit and an id no table holds do not
    for (uint32 spellId : { 7321u, 83073u, 44457u, 12654u, 83619u, 82739u, 30482u, 12345u, 0u })
    {
        CHECK(!TriggeredCastStartsProcs(spellId));
    }

    // Divine Touch's talent ranks (63534, 63542) and Renew (139) do not
    for (uint32 spellId : { 63534u, 63542u, 139u })
    {
        CHECK(!TriggeredCastStartsProcs(spellId));
    }

    // The table holds exactly its six rows
    uint32 rows = 0;
    for (uint32 spellId = 0; spellId < 1000000; ++spellId)
    {
        if (TriggeredCastStartsProcs(spellId))
        {
            ++rows;
        }
    }
    CHECK_EQ(rows, uint32(6));
}

TEST(SpellTables_MoltenArmorsHitAndTheChillsStartOnlyTheProcsOfAurasThatCanProcFromProcs)
{
    // Molten Armor's hit, Ice Shards' Chilled and Piercing Chill's chill start an aura's procs only when the aura
    // carries the bit, as Fingers of Frost (44543, Ex3 0x04000000) does; no other cast is held back
    for (uint32 spellId : { 34913u, 12484u, 12485u, 83154u })
    {
        CHECK(TriggeredCastStartsProcsOf(spellId, SPELL_ATTR_EX3_CAN_PROC_FROM_PROCS));
        CHECK(TriggeredCastStartsProcsOf(spellId, SPELL_ATTR_EX3_CAN_PROC_FROM_PROCS | 0x1));
        CHECK(!TriggeredCastStartsProcsOf(spellId, 0));
        CHECK(!TriggeredCastStartsProcsOf(spellId, ~uint32(SPELL_ATTR_EX3_CAN_PROC_FROM_PROCS)));
    }
    CHECK_EQ(uint32(SPELL_ATTR_EX3_CAN_PROC_FROM_PROCS), uint32(0x04000000));
    CHECK(TriggeredCastStartsProcsOf(83154, 0x04000000));
    // Brain Freeze's Ex3 (44546: 0x04080000) holds the bit too
    CHECK(TriggeredCastStartsProcsOf(12485, 0x04080000));
    for (uint32 spellId : { 44461u, 63544u, 7321u, 83073u, 82739u, 30482u, 12345u, 0u })
    {
        CHECK(TriggeredCastStartsProcsOf(spellId, 0));
        CHECK(TriggeredCastStartsProcsOf(spellId, SPELL_ATTR_EX3_CAN_PROC_FROM_PROCS));
    }
}

TEST(SpellTables_DamageOfAReactiveDamageProcDoesNotBreakAuras)
{
    CHECK(!DamageBreaksAuras(SPELL_ATTR_EX4_REACTIVE_DAMAGE_PROC));
    CHECK(!DamageBreaksAuras(0xc080));
    CHECK(DamageBreaksAuras(0));
    CHECK(DamageBreaksAuras(0x8080));
    CHECK(DamageBreaksAuras(~uint32(SPELL_ATTR_EX4_REACTIVE_DAMAGE_PROC)));
}

namespace
{
    // the aura interrupt flags of Polymorph (118), Wyvern Sting (19386) and Ring of Frost (82691), SpellInterrupts.dbc
    const uint32 kPolymorphFlags = 0x08080000;
    const uint32 kWyvernStingFlags = 0x00480002;
    const uint32 kRingOfFrostFlags = 0x08480000;
    // the Ex4 attributes of Molten Armor's hit (34913), Spell.dbc
    const uint32 kMoltenArmorHitEx4 = 0xc080;
}

TEST(SpellTables_PolymorphBreaksOnAnyDamageThatGetsThrough)
{
    // a direct hit or a periodic tick that deals damage
    CHECK(BreaksOnDamageProc(1, true, 0, false, 0, kPolymorphFlags, SPELL_AURA_MOD_CONFUSE));
    CHECK(BreaksOnDamageProc(355, true, 0, false, 0, kPolymorphFlags, SPELL_AURA_MOD_CONFUSE));

    // a hit whose Ex4 attributes hold bits other than SPELL_ATTR_EX4_REACTIVE_DAMAGE_PROC
    CHECK(BreaksOnDamageProc(355, true, 0, false, 0x8080, kPolymorphFlags, SPELL_AURA_MOD_CONFUSE));

    // a hit the absorbs take whole, or one that deals nothing
    CHECK(!BreaksOnDamageProc(0, true, 0, false, 0, kPolymorphFlags, SPELL_AURA_MOD_CONFUSE));
    CHECK(!BreaksOnDamageProc(0, false, 0, false, 0, kPolymorphFlags, SPELL_AURA_MOD_CONFUSE));

    // a heal is not damage taken, whatever its amount
    CHECK(!BreaksOnDamageProc(5000, false, 0, false, 0, kPolymorphFlags, SPELL_AURA_MOD_CONFUSE));

    // Molten Armor's hit and any other reactive damage proc
    CHECK(!BreaksOnDamageProc(244, true, 0, false, kMoltenArmorHitEx4, kPolymorphFlags, SPELL_AURA_MOD_CONFUSE));
    CHECK(!BreaksOnDamageProc(244, true, 0, false, SPELL_ATTR_EX4_REACTIVE_DAMAGE_PROC, kPolymorphFlags,
                              SPELL_AURA_MOD_CONFUSE));

    // the aura's own spell
    CHECK(!BreaksOnDamageProc(500, true, 0, true, 0, kPolymorphFlags, SPELL_AURA_MOD_CONFUSE));
}

TEST(SpellTables_AConfuseWithChargesKeepsItsChargeModel)
{
    // Dragon's Breath (one charge): the charge goes on the hit, not this rule
    CHECK(!BreaksOnDamageProc(500, true, 1, false, 0, 0, SPELL_AURA_MOD_CONFUSE));
    CHECK(!BreaksOnDamageProc(500, true, 2, false, 0, kPolymorphFlags, SPELL_AURA_MOD_CONFUSE));
}

TEST(SpellTables_AStunBreaksOnDamageOnlyWhenItsInterruptFlagsSaySo)
{
    // Wyvern Sting: "Any damage will cancel the effect"
    CHECK(BreaksOnDamageProc(500, true, 0, false, 0, kWyvernStingFlags, SPELL_AURA_MOD_STUN));
    CHECK(BreaksOnDamageProc(500, true, 0, false, 0, AURA_INTERRUPT_FLAG_DAMAGE, SPELL_AURA_MOD_STUN));
    CHECK(!BreaksOnDamageProc(0, true, 0, false, 0, kWyvernStingFlags, SPELL_AURA_MOD_STUN));
    CHECK(!BreaksOnDamageProc(500, true, 0, false, kMoltenArmorHitEx4, kWyvernStingFlags, SPELL_AURA_MOD_STUN));

    // Ring of Frost (no AURA_INTERRUPT_FLAG_DAMAGE, no charges: it breaks by a chance, CountsPreventedDamage) and a
    // stun with no flags at all
    CHECK(!BreaksOnDamageProc(500, true, 0, false, 0, kRingOfFrostFlags, SPELL_AURA_MOD_STUN));
    CHECK(!BreaksOnDamageProc(500, true, 0, false, 0, 0, SPELL_AURA_MOD_STUN));

    // Gouge and Repentance (one charge) keep their charge
    CHECK(!BreaksOnDamageProc(500, true, 1, false, 0, AURA_INTERRUPT_FLAG_DAMAGE, SPELL_AURA_MOD_STUN));
}

TEST(SpellTables_NoOtherAuraTypeBreaksOnAnyDamage)
{
    // Polymorph's and Hex's transform, Hex's pacify-silence, a fear and a root: none breaks through this rule
    for (uint32 auraType : { uint32(SPELL_AURA_TRANSFORM), uint32(SPELL_AURA_MOD_PACIFY_SILENCE),
                             uint32(SPELL_AURA_MOD_FEAR), uint32(SPELL_AURA_MOD_ROOT), uint32(SPELL_AURA_MOD_PACIFY),
                             uint32(SPELL_AURA_NONE) })
    {
        CHECK(!BreaksOnDamageProc(500, true, 0, false, 0, kPolymorphFlags, auraType));
        CHECK(!BreaksOnDamageProc(500, true, 0, false, 0, kWyvernStingFlags, auraType));
    }
}

namespace
{
    // the aura interrupt flags of Dragon's Breath (31661), Sap (6770), Hammer of Justice (853) and Frost Nova (122),
    // SpellInterrupts.dbc
    const uint32 kDragonsBreathFlags = 0x00480000;
    const uint32 kSapFlags = 0x00480002;
    const uint32 kHammerOfJusticeFlags = 0x00480000;
    const uint32 kFrostNovaFlags = 0x00480000;
}

TEST(SpellTables_AnyDamageBreaksAConfuseAndAStunWhoseFlagsSaySoAtOnce)
{
    // Polymorph's and Dragon's Breath's confuse, whose flags hold no AURA_INTERRUPT_FLAG_DAMAGE
    CHECK(BreaksOnAnyDamage(kPolymorphFlags, SPELL_AURA_MOD_CONFUSE));
    CHECK(BreaksOnAnyDamage(kDragonsBreathFlags, SPELL_AURA_MOD_CONFUSE));

    // a stun with AURA_INTERRUPT_FLAG_DAMAGE (Sap, Wyvern Sting) and one without it (Hammer of Justice, Ring of Frost)
    CHECK(BreaksOnAnyDamage(kSapFlags, SPELL_AURA_MOD_STUN));
    CHECK(BreaksOnAnyDamage(kWyvernStingFlags, SPELL_AURA_MOD_STUN));
    CHECK(!BreaksOnAnyDamage(kHammerOfJusticeFlags, SPELL_AURA_MOD_STUN));
    CHECK(!BreaksOnAnyDamage(kRingOfFrostFlags, SPELL_AURA_MOD_STUN));

    // Frost Nova's root, Polymorph's transform and Fear's fear, whatever their flags
    CHECK(!BreaksOnAnyDamage(kFrostNovaFlags, SPELL_AURA_MOD_ROOT));
    CHECK(!BreaksOnAnyDamage(kSapFlags, SPELL_AURA_MOD_ROOT));
    CHECK(!BreaksOnAnyDamage(kPolymorphFlags, SPELL_AURA_TRANSFORM));
    CHECK(!BreaksOnAnyDamage(kSapFlags, SPELL_AURA_MOD_FEAR));

    // the same answer BreaksOnDamageProc gives a hit of another spell that gets through, on a holder with no charges
    for (uint32 flags : { kPolymorphFlags, kDragonsBreathFlags, kSapFlags, kHammerOfJusticeFlags, kRingOfFrostFlags })
    {
        for (uint32 auraType : { uint32(SPELL_AURA_MOD_CONFUSE), uint32(SPELL_AURA_MOD_STUN),
                                 uint32(SPELL_AURA_MOD_ROOT) })
        {
            CHECK(BreaksOnAnyDamage(flags, auraType) == BreaksOnDamageProc(500, true, 0, false, 0, flags, auraType));
        }
    }
}

namespace
{
    // the aura interrupt flags of Gouge (1776) and Repentance (20066), SpellInterrupts.dbc; the proc flags of Gouge,
    // Repentance, Dragon's Breath (31661), Polymorph (118) and Ring of Frost (82691), SpellAuraOptions.dbc, whose proc
    // charges are 1, 1, 1, 0 and 0
    const uint32 kGougeFlags = 0x08000000;
    const uint32 kRepentanceFlags = 0x08000000;
    const uint32 kGougeProcFlags = 0x000A22A8;
    const uint32 kRepentanceProcFlags = 0x000AAAA8;
    const uint32 kDragonsBreathProcFlags = 0x000222A8;
    const uint32 kPolymorphProcFlags = 0x000AAAA8;
    const uint32 kRingOfFrostProcFlags = 0x000A2AA8;
}

TEST(SpellTables_AStunWhoseOneChargeEveryDamageSpendsBreaksOnTheFirst)
{
    // Gouge's and Repentance's stun: one charge, and every kind of damage taken among their proc flags; their aura
    // interrupt flags (AURA_INTERRUPT_FLAG_DAMAGE2 alone) do not say so
    CHECK(BreaksOnFirstDamageByCharge(1, kGougeProcFlags, SPELL_AURA_MOD_STUN));
    CHECK(BreaksOnFirstDamageByCharge(1, kRepentanceProcFlags, SPELL_AURA_MOD_STUN));
    CHECK(!BreaksOnAnyDamage(kGougeFlags, SPELL_AURA_MOD_STUN));
    CHECK(!BreaksOnAnyDamage(kRepentanceFlags, SPELL_AURA_MOD_STUN));

    // the same flags on a holder with no charge left, or with two
    CHECK(!BreaksOnFirstDamageByCharge(0, kGougeProcFlags, SPELL_AURA_MOD_STUN));
    CHECK(!BreaksOnFirstDamageByCharge(2, kRepentanceProcFlags, SPELL_AURA_MOD_STUN));

    // Polymorph and Ring of Frost hold no charge; Dragon's Breath holds one, but no periodic damage among its flags
    // (its confuse breaks on any damage by BreaksOnAnyDamage)
    CHECK(!BreaksOnFirstDamageByCharge(0, kPolymorphProcFlags, SPELL_AURA_MOD_CONFUSE));
    CHECK(!BreaksOnFirstDamageByCharge(0, kRingOfFrostProcFlags, SPELL_AURA_MOD_STUN));
    CHECK(!BreaksOnFirstDamageByCharge(1, kDragonsBreathProcFlags, SPELL_AURA_MOD_CONFUSE));
    CHECK(BreaksOnAnyDamage(kDragonsBreathFlags, SPELL_AURA_MOD_CONFUSE));

    // one charge on a confuse whose flags take every damage, and Gouge's flags short of a melee swing or a harmful
    // spell hit
    CHECK(BreaksOnFirstDamageByCharge(1, kGougeProcFlags, SPELL_AURA_MOD_CONFUSE));
    CHECK(!BreaksOnFirstDamageByCharge(1, kGougeProcFlags & ~uint32(PROC_FLAG_TAKEN_MELEE_HIT), SPELL_AURA_MOD_STUN));
    CHECK(!BreaksOnFirstDamageByCharge(1, kGougeProcFlags & ~uint32(PROC_FLAG_TAKEN_NEGATIVE_SPELL_HIT),
                                       SPELL_AURA_MOD_STUN));

    // Gouge's charge and flags on a fear, a root, a transform or a pacify-silence
    for (uint32 auraType : { uint32(SPELL_AURA_MOD_FEAR), uint32(SPELL_AURA_MOD_ROOT), uint32(SPELL_AURA_TRANSFORM),
                             uint32(SPELL_AURA_MOD_PACIFY_SILENCE) })
    {
        CHECK(!BreaksOnFirstDamageByCharge(1, kGougeProcFlags, auraType));
    }
}

TEST(SpellTables_TheThresholdEffectsAreFearRootAndPacifySilenceAndNoOther)
{
    // Fear, Entangling Roots and Frost Nova, and Hex's pacify-silence, whatever their flags and charges
    for (uint32 auraType : { uint32(SPELL_AURA_MOD_FEAR), uint32(SPELL_AURA_MOD_ROOT),
                             uint32(SPELL_AURA_MOD_PACIFY_SILENCE) })
    {
        CHECK(CountsPreventedDamage(auraType, 0, 0));
        CHECK(CountsPreventedDamage(auraType, kWyvernStingFlags, 0));
        CHECK(CountsPreventedDamage(auraType, kRingOfFrostFlags, 1));
    }

    // Polymorph's confuse (whose flags hold AURA_INTERRUPT_FLAG_DAMAGE2 alone too) and transform, a pacify and no aura
    for (uint32 auraType : { uint32(SPELL_AURA_MOD_CONFUSE), uint32(SPELL_AURA_TRANSFORM),
                             uint32(SPELL_AURA_MOD_PACIFY), uint32(SPELL_AURA_NONE) })
    {
        CHECK(!CountsPreventedDamage(auraType, kPolymorphFlags, 0));
        CHECK(!CountsPreventedDamage(auraType, kRingOfFrostFlags, 0));
    }
}

TEST(SpellTables_AStunWhoseFlagsHoldOnlyTheSecondDamageFlagIsAThresholdEffect)
{
    // Ring of Frost's freeze: AURA_INTERRUPT_FLAG_DAMAGE2 and not AURA_INTERRUPT_FLAG_DAMAGE, no charges
    CHECK(CountsPreventedDamage(SPELL_AURA_MOD_STUN, kRingOfFrostFlags, 0));
    CHECK(CountsPreventedDamage(SPELL_AURA_MOD_STUN, AURA_INTERRUPT_FLAG_DAMAGE2, 0));

    // the same stun on a holder with charges spends them; Wyvern Sting's stun and a stun that holds both flags break
    // on any damage (BreaksOnDamageProc); a stun with neither flag never breaks on damage
    CHECK(!CountsPreventedDamage(SPELL_AURA_MOD_STUN, kRingOfFrostFlags, 1));
    CHECK(!CountsPreventedDamage(SPELL_AURA_MOD_STUN, kWyvernStingFlags, 0));
    CHECK(!CountsPreventedDamage(SPELL_AURA_MOD_STUN, AURA_INTERRUPT_FLAG_DAMAGE | AURA_INTERRUPT_FLAG_DAMAGE2, 0));
    CHECK(!CountsPreventedDamage(SPELL_AURA_MOD_STUN, 0, 0));
}

TEST(SpellTables_AThresholdEffectCountsTheDamageAbsorbsAndReductionEffectsTook)
{
    // no absorb and no reduction effect: the damage that got through
    CHECK_EQ(ThresholdDamage(500, 0, 1.0f), 500u);
    CHECK_EQ(ThresholdDamage(0, 0, 1.0f), 0u);

    // a shield that took the hit whole, and one that took part of it
    CHECK_EQ(ThresholdDamage(0, 3518, 1.0f), 3518u);
    CHECK_EQ(ThresholdDamage(1518, 2000, 1.0f), 3518u);

    // Pain Suppression (-40%), and Pain Suppression with Shield Wall (-40%, -40%), after a shield
    CHECK_EQ(ThresholdDamage(600, 0, 0.6f), 1000u);
    CHECK_EQ(ThresholdDamage(160, 200, 0.6f * 0.6f), 1000u);

    // a multiplier that is no reduction, or that left nothing to take back, counts the sum
    CHECK_EQ(ThresholdDamage(500, 200, 1.0f), 700u);
    CHECK_EQ(ThresholdDamage(500, 200, 1.25f), 700u);
    CHECK_EQ(ThresholdDamage(500, 200, 0.0f), 700u);

    // a count past the largest uint32 stops there
    CHECK_EQ(ThresholdDamage(4294967295u, 4294967295u, 0.5f), 4294967295u);
}

TEST(SpellTables_TheProcRankTablesAnswerTheirSwitchesLiveRowsAndNoOthers)
{
    CHECK_EQ(GetArcanePotencyProcSpell(31571), uint32(57529));
    CHECK_EQ(GetArcanePotencyProcSpell(31572), uint32(57531));
    CHECK_EQ(GetArcanePotencyHiddenAura(57529), uint32(33421));
    CHECK_EQ(GetArcanePotencyHiddenAura(57531), uint32(33713));
    CHECK_EQ(GetSecondWindProcSpell(29838), uint32(29842));
    CHECK_EQ(GetSecondWindProcSpell(29834), uint32(29841));
    CHECK_EQ(GetSecondWindProcSpell(42770), uint32(42771));
    CHECK_EQ(GetRapidRecuperationProcSpell(53228), uint32(56654));
    CHECK_EQ(GetRapidRecuperationProcSpell(53232), uint32(58882));
    CHECK_EQ(GetFlametongueWeaponProcSpell(10400), uint32(8026));
    CHECK_EQ(GetBlessedRecoveryProcSpell(27811), uint32(27813));
    CHECK_EQ(GetBlessedRecoveryProcSpell(27815), uint32(27817));
    CHECK_EQ(GetBlessedRecoveryProcSpell(27816), uint32(27818));
    CHECK_EQ(GetLightningOverloadSpell(403), uint32(45284));
    CHECK_EQ(GetLightningOverloadSpell(421), uint32(45297));
    CHECK_EQ(GetLightningShieldDamageSpell(324), uint32(26364));
    CHECK_EQ(GetThreatOfThassarianOffHandSpell(49020), uint32(66198));
    CHECK_EQ(GetThreatOfThassarianOffHandSpell(49143), uint32(66196));
    CHECK_EQ(GetThreatOfThassarianOffHandSpell(45462), uint32(66216));
    CHECK_EQ(GetThreatOfThassarianOffHandSpell(49998), uint32(66188));
    CHECK_EQ(GetThreatOfThassarianOffHandSpell(56815), uint32(66217));
    CHECK_EQ(GetThreatOfThassarianOffHandSpell(45902), uint32(66215));
    CHECK_EQ(GetHolyShockOfHeal(25914), uint32(20473));
    CHECK_EQ(GetDeepWoundsWeaponDamagePercent(12834), uint32(16));
    CHECK_EQ(GetDeepWoundsWeaponDamagePercent(12849), uint32(32));
    CHECK_EQ(GetDeepWoundsWeaponDamagePercent(12867), uint32(48));
    CHECK(IsBlazingSpeedRank(31641));
    CHECK(IsBlazingSpeedRank(31642));

    // Ranks that are not 4.3.4 spells answer nothing, and neither does an id no switch held.
    CHECK_EQ(GetFlametongueWeaponProcSpell(15567), uint32(0));
    CHECK_EQ(GetLightningOverloadSpell(529), uint32(0));
    CHECK_EQ(GetLightningOverloadSpell(930), uint32(0));
    CHECK_EQ(GetLightningShieldDamageSpell(325), uint32(0));
    CHECK_EQ(GetLightningShieldDamageSpell(49281), uint32(0));
    CHECK_EQ(GetThreatOfThassarianOffHandSpell(51423), uint32(0));
    CHECK_EQ(GetHolyShockOfHeal(25913), uint32(0));

    // The hidden Arcane Potency auras answer for the buffs alone: not for the talent's ranks, the channels, or the
    // hidden auras themselves.
    for (uint32 spellId : { 31571u, 31572u, 5143u, 10u, 33421u, 33713u })
    {
        CHECK_EQ(GetArcanePotencyHiddenAura(spellId), uint32(0));
    }
    for (uint32 spellId : { 0u, 12345u })
    {
        CHECK_EQ(GetArcanePotencyProcSpell(spellId), uint32(0));
        CHECK_EQ(GetArcanePotencyHiddenAura(spellId), uint32(0));
        CHECK_EQ(GetSecondWindProcSpell(spellId), uint32(0));
        CHECK_EQ(GetRapidRecuperationProcSpell(spellId), uint32(0));
        CHECK_EQ(GetBlessedRecoveryProcSpell(spellId), uint32(0));
        CHECK_EQ(GetDeepWoundsWeaponDamagePercent(spellId), uint32(0));
        CHECK(!IsBlazingSpeedRank(spellId));
    }
}

TEST(SpellTables_IncantersAbsorptionCountsManaShieldAndMageWardAlone)
{
    CHECK(CountsForIncantersAbsorption(543));
    CHECK(CountsForIncantersAbsorption(1463));

    // Ice Barrier (11426), the creatures' Mana Shields (17740, 17741, 56778), Incanter's Absorption's own buff (44413)
    // and any other id count nothing.
    for (uint32 spellId : { 11426u, 17740u, 17741u, 56778u, 44413u, 0u, 12345u })
    {
        CHECK(!CountsForIncantersAbsorption(spellId));
    }
}

TEST(SpellTables_DivineAegisIsTheShieldAHitSpendsFirst)
{
    CHECK_EQ(Spells::AbsorbSpendOrder(47753), uint32(0));

    // Power Word: Shield (17), Ice Barrier (11426), Mage Ward (543), Mana Shield (1463) and any other id share the
    // place after it.
    for (uint32 spellId : { 17u, 11426u, 543u, 1463u, 0u, 12345u })
    {
        CHECK_EQ(Spells::AbsorbSpendOrder(spellId), uint32(1));
    }

    // Sorted stably by it, the shields a unit holds in the order they were applied put Divine Aegis first, however
    // late it came, and keep the rest in their order.
    std::vector<uint32> held = { 17, 11426, 47753, 543 };
    std::stable_sort(held.begin(), held.end(), [](uint32 first, uint32 second)
    {
        return Spells::AbsorbSpendOrder(first) < Spells::AbsorbSpendOrder(second);
    });
    const std::vector<uint32> spent = { 47753, 17, 11426, 543 };
    CHECK(held == spent);
}

TEST(SpellTables_EachHasteSpellAnswersItsOwnLockoutAndNoOtherSpellAnswersOne)
{
    // The four pairs, as the haste spells' texts name them: Bloodlust's Sated, Heroism's Exhaustion, Time Warp's
    // Temporal Displacement, Ancient Hysteria's Insanity.
    CHECK_EQ(LustLockoutOf(2825), uint32(57724));
    CHECK_EQ(LustLockoutOf(32182), uint32(57723));
    CHECK_EQ(LustLockoutOf(80353), uint32(80354));
    CHECK_EQ(LustLockoutOf(90355), uint32(95809));

    // Frostbolt (116), the lockouts themselves and any other id answer none.
    for (uint32 spellId : { 116u, 57724u, 57723u, 80354u, 95809u, 0u, 12345u })
    {
        CHECK_EQ(LustLockoutOf(spellId), uint32(0));
    }

    // The table's haste spells by ascending id, then 0 past its end.
    CHECK_EQ(LustSpellAt(0), uint32(2825));
    CHECK_EQ(LustSpellAt(1), uint32(32182));
    CHECK_EQ(LustSpellAt(2), uint32(80353));
    CHECK_EQ(LustSpellAt(3), uint32(90355));
    CHECK_EQ(LustSpellAt(4), uint32(0));
    CHECK_EQ(LustSpellAt(1000), uint32(0));
}

TEST(SpellTables_TheLockoutsAreTheLustTablesAnswersAndNoOtherSpell)
{
    // Sated, Exhaustion, Temporal Displacement and Insanity are lockouts.
    for (uint32 spellId : { 57724u, 57723u, 80354u, 95809u })
    {
        CHECK(IsLustLockout(spellId));
    }

    // The haste spells themselves, Frostbolt (116) and any other id are none.
    for (uint32 spellId : { 2825u, 32182u, 80353u, 90355u, 116u, 0u, 12345u })
    {
        CHECK(!IsLustLockout(spellId));
    }
}

TEST(SpellTables_TheDeathPreventionRowsAreCheatDeathGuardianSpiritAndCauterize)
{
    // Cauterize: effect 0 holds the chance (50/100), effect 1 the health percent (40); 87023 heals through its effect 1;
    // "once per minute"
    DeathPreventionRow const* cauterize = DeathPreventionFor(86949);
    CHECK(cauterize != NULL);
    if (cauterize)
    {
        CHECK_EQ(int32(cauterize->chanceEffect), int32(0));
        CHECK_EQ(int32(cauterize->percentEffect), int32(1));
        CHECK(cauterize->shape == DEATH_PREVENTION_FLOOR_AND_HEAL);
        CHECK_EQ(cauterize->followUpSpell, uint32(87023));
        CHECK_EQ(int32(cauterize->healEffect), int32(1));
        CHECK_EQ(cauterize->lockoutSeconds, uint32(60));
        CHECK(!cauterize->consumed);
    }
    DeathPreventionRow const* cauterizeRank1 = DeathPreventionFor(86948);
    CHECK(cauterizeRank1 != NULL);
    if (cauterizeRank1)
    {
        CHECK_EQ(int32(cauterizeRank1->chanceEffect), int32(0));
    }

    // Cheat Death: effect 0 holds the chance (33/66/100), the floor is 10%; "once per 90 seconds"
    DeathPreventionRow const* cheatDeath = DeathPreventionFor(31230);
    CHECK(cheatDeath != NULL);
    if (cheatDeath)
    {
        CHECK_EQ(int32(cheatDeath->percentEffect), int32(-1));
        CHECK_EQ(cheatDeath->percent, uint32(10));
        CHECK(cheatDeath->shape == DEATH_PREVENTION_FLOOR);
        CHECK_EQ(cheatDeath->followUpSpell, uint32(31231));
        CHECK_EQ(int32(cheatDeath->healEffect), int32(-1));
        CHECK_EQ(cheatDeath->lockoutSeconds, uint32(90));
        CHECK(!cheatDeath->consumed);
    }

    // Guardian Spirit: always saves, effect 1 holds the percent (50), 48153 heals through its effect 0, and the save
    // uses the aura up
    DeathPreventionRow const* guardianSpirit = DeathPreventionFor(47788);
    CHECK(guardianSpirit != NULL);
    if (guardianSpirit)
    {
        CHECK_EQ(int32(guardianSpirit->chanceEffect), int32(-1));
        CHECK_EQ(int32(guardianSpirit->percentEffect), int32(1));
        CHECK(guardianSpirit->shape == DEATH_PREVENTION_HEAL_PERCENT);
        CHECK_EQ(guardianSpirit->followUpSpell, uint32(48153));
        CHECK_EQ(int32(guardianSpirit->healEffect), int32(0));
        CHECK_EQ(guardianSpirit->lockoutSeconds, uint32(0));
        CHECK(guardianSpirit->consumed);
    }

    // The follow-ups, Ignite's DoT and an id no table holds have no row
    for (uint32 spellId : { 87023u, 31231u, 48153u, 12654u, 12345u, 0u })
    {
        CHECK(DeathPreventionFor(spellId) == NULL);
    }

    // The table holds exactly its six rows
    uint32 rows = 0;
    for (uint32 spellId = 0; spellId < 1000000; ++spellId)
    {
        if (DeathPreventionFor(spellId))
        {
            ++rows;
        }
    }
    CHECK_EQ(rows, uint32(6));
    for (uint32 spellId : { 31228u, 31229u, 31230u, 47788u, 86948u, 86949u })
    {
        DeathPreventionRow const* row = DeathPreventionFor(spellId);
        CHECK(row != NULL && row->spellId == spellId);
    }
}

TEST(SpellTables_TheEffectCastAuraAndPacketTablesAnswerTheirSwitchesLiveRows)
{
    SpellHurtHeal const* penance = FindPenanceSpells(47540);
    REQUIRE(penance != NULL);
    CHECK_EQ(penance->hurt, uint32(47758));
    CHECK_EQ(penance->heal, uint32(47757));
    SpellHurtHeal const* holyShock = FindHolyShockSpells(20473);
    REQUIRE(holyShock != NULL);
    CHECK_EQ(holyShock->hurt, uint32(25912));
    CHECK_EQ(holyShock->heal, uint32(25914));
    CHECK_EQ(GetShadowflameDotSpell(47897), uint32(47960));
    CHECK_EQ(GetStarfallStarSpell(50286), uint32(50288));
    CHECK_EQ(GetDisengageSpell(57635), uint32(57636));
    CHECK_EQ(GetDisengageSpell(61507), uint32(61508));
    CHECK_EQ(GetFireNovaSpell(1535), uint32(8349));
    CHECK_EQ(GetWyvernStingDotSpell(19386), uint32(24131));
    CHECK_EQ(GetHolyNovaHealSpell(15237), uint32(23455));
    CHECK_EQ(GetJudgementDebuffSpell(31804), uint32(31803));
    CHECK_EQ(GetJudgementDebuffSpell(53733), uint32(53742));
    CHECK_EQ(GetJudgementSelfSpell(20271), uint32(20185));
    CHECK_EQ(GetJudgementSelfSpell(57774), uint32(20185));
    CHECK_EQ(GetLavaFlowsHasteSpell(51480), uint32(64694));
    CHECK_EQ(GetLavaFlowsHasteSpell(51481), uint32(65263));
    CHECK_EQ(GetLavaFlowsHasteSpell(51482), uint32(65264));
    CHECK_EQ(GetSpellRequiredAreaId(41617), uint32(3905));
    CHECK_EQ(GetSpellRequiredAreaId(41619), uint32(3905));
    CHECK_EQ(GetSpellRequiredAreaId(41618), uint32(3842));
    CHECK_EQ(GetSpellRequiredAreaId(41620), uint32(3842));
    CHECK_EQ(GetSpellRequiredAreaId(45373), uint32(4075));

    // Ranks that are not 4.3.4 spells answer nothing, and neither does an id no switch held.
    CHECK(FindPenanceSpells(53005) == NULL);
    CHECK(FindHolyShockSpells(20929) == NULL);
    CHECK_EQ(GetShadowflameDotSpell(61290), uint32(0));
    CHECK_EQ(GetStarfallStarSpell(53196), uint32(0));
    CHECK_EQ(GetFireNovaSpell(8498), uint32(0));
    CHECK_EQ(GetWyvernStingDotSpell(24132), uint32(0));
    CHECK_EQ(GetHolyNovaHealSpell(15430), uint32(0));
    CHECK_EQ(GetJudgementSelfSpell(53407), uint32(0));
    CHECK_EQ(GetJudgementSelfSpell(53408), uint32(0));
    CHECK_EQ(GetSpellRequiredAreaId(12345), uint32(0));
    CHECK_EQ(GetLavaFlowsHasteSpell(12345), uint32(0));
    CHECK_EQ(GetDisengageSpell(12345), uint32(0));
    CHECK_EQ(GetJudgementDebuffSpell(12345), uint32(0));
}

TEST(SpellTables_IsPositiveEffectsSetsHoldTheirSwitchesLabelsAndNoOthers)
{
    for (uint32 spellId : { 10258u, 18153u, 32312u, 37388u, 45863u, 49634u, 54530u, 56099u, 58533u, 58552u, 62105u, 63745u, 63747u, 64402u })
    {
        CHECK(HasPositiveDummyEffect(spellId));
        CHECK(!HasNegativeDummyEffect(spellId));
    }
    for (uint32 spellId : { 28441u })
    {
        CHECK(HasNegativeDummyEffect(spellId));
        CHECK(!HasPositiveDummyEffect(spellId));
    }
    for (uint32 spellId : { 42436u, 42492u, 46650u, 62488u, 64503u })
    {
        CHECK(HasPositiveScriptEffect(spellId));
    }
    for (uint32 spellId : { 27184u, 27190u, 27191u, 27201u, 27202u, 27203u, 47669u, 64996u })
    {
        CHECK(HasPositiveDummyAura(spellId));
        CHECK(!HasNegativeDummyAura(spellId));
    }
    for (uint32 spellId : { 11196u, 13139u, 23182u, 23445u, 25040u, 35679u, 37695u, 38637u, 38638u, 38639u, 44689u, 58600u })
    {
        CHECK(HasNegativeDummyAura(spellId));
        CHECK(!HasPositiveDummyAura(spellId));
    }
    for (uint32 spellId : { 24740u, 47585u })
    {
        CHECK(HasPositivePacifySilenceAura(spellId));
    }
    for (uint32 spellId : { 36897u, 36899u, 37097u })
    {
        CHECK(HasNegativeTransformAura(spellId));
    }
    for (uint32 spellId : { 802u, 38449u, 50312u })
    {
        CHECK(HasPositiveScaleAura(spellId));
        CHECK(!HasNegativeScaleAura(spellId));
    }
    for (uint32 spellId : { 36893u, 36895u, 36900u, 36901u })
    {
        CHECK(HasNegativeScaleAura(spellId));
        CHECK(!HasPositiveScaleAura(spellId));
    }
    for (uint32 spellId : { 38449u })
    {
        CHECK(HasPositiveMeleeHasteAura(spellId));
    }
    for (uint32 spellId : { 42792u, 46221u })
    {
        CHECK(HasNegativeForceReactionAura(spellId));
    }
    for (uint32 spellId : { 57508u, 57509u, 57510u, 57511u, 57512u })
    {
        CHECK(HasNegativePhaseAura(spellId));
    }
    for (uint32 spellId : { 0u, 12345u })
    {
        CHECK(!HasPositiveDummyEffect(spellId));
        CHECK(!HasNegativeDummyEffect(spellId));
        CHECK(!HasPositiveScriptEffect(spellId));
        CHECK(!HasPositiveDummyAura(spellId));
        CHECK(!HasNegativeDummyAura(spellId));
        CHECK(!HasPositivePacifySilenceAura(spellId));
        CHECK(!HasNegativeTransformAura(spellId));
        CHECK(!HasPositiveScaleAura(spellId));
        CHECK(!HasNegativeScaleAura(spellId));
        CHECK(!HasPositiveMeleeHasteAura(spellId));
        CHECK(!HasNegativeForceReactionAura(spellId));
        CHECK(!HasNegativePhaseAura(spellId));
    }
}

// The speed-buff marker. The queries read Spell.dbc rows built as the loader builds them, each with only its id and
// its SpellAuraRestrictions row id, beside SpellAuraRestrictions rows put into their store: the client's rows of the
// fourteen spells whose ExcludeCasterAuraSpell is 96223, of Invisibility (81) and of Divine Shield (6), and a row of
// the test's own that names 96223 in every spell column but ExcludeCasterAuraSpell. The holder arm runs on a bare
// Creature (update fields allocated; no map, template or AI): its holders are made through their constructor with no
// aura, on rows put into the spell store, and put in its holder map as Unit::AddSpellAuraHolder files them, so no add
// runs the arm; each leaves through Unit::RemoveSpellAuraHolder, which runs SpellAuraHolder::HandleSpellSpecificBoosts.
namespace
{
    const uint32 kRunSpeedMarker = 96223;

    struct MarkerRowStorage
    {
        alignas(SpellEntry) unsigned char bytes[sizeof(SpellEntry)];
    };

    /// The Spell.dbc row in `storage`, with only its id and its SpellAuraRestrictions row id.
    SpellEntry& RestrictedSpell(MarkerRowStorage& storage, uint32 spellId, uint32 restrictionsId)
    {
        SpellEntry* row = reinterpret_cast<SpellEntry*>(storage.bytes);
        row->ID = spellId;
        row->AuraRestrictionsID = restrictionsId;
        return *row;
    }

    /// A spell of the client's data whose SpellAuraRestrictions row excludes its caster on the marker.
    struct MarkerCarrier
    {
        uint32 spellId;
        uint32 restrictionsId;
        uint32 casterAuraSpell;     ///< the aura its row asks of the caster (a druid form); 0 for none
    };

    MarkerCarrier const MARKER_CARRIERS[] =
    {
        { 1850, 1094, 768 },        // Dash, in Cat Form
        { 2983, 1180, 0 },          // Sprint
        { 8892, 1373, 0 },          // Goblin Rocket Boots
        { 13141, 1371, 0 },         // Gnomish Rocket Boots
        { 23451, 1376, 0 },         // Speed
        { 46989, 1368, 0 },         // Improved Blink, the second rank's buff
        { 47000, 1369, 0 },         // Improved Blink, the first rank's buff
        { 51582, 1374, 0 },         // Rocket Boots Engaged
        { 55004, 1372, 0 },         // Nitro Boosts
        { 64128, 1375, 0 },         // Body and Soul
        { 68992, 1179, 0 },         // Darkflight
        { 77761, 1367, 5487 },      // Stampeding Roar, in Bear Form
        { 77764, 1366, 768 },       // Stampeding Roar, in Cat Form
        { 87833, 1370, 0 },         // Invisibility Speed
    };
    constexpr std::size_t kMarkerCarriers = sizeof(MARKER_CARRIERS) / sizeof(MARKER_CARRIERS[0]);

    // SpellAuraRestrictions.dbc rows: { CasterAuraState, TargetAuraState, ExcludeCasterAuraState,
    // ExcludeTargetAuraState, CasterAuraSpell, TargetAuraSpell, ExcludeCasterAuraSpell, ExcludeTargetAuraSpell }.
    SpellAuraRestrictionsEntry s_carrierRestrictions[kMarkerCarriers] = {};
    SpellAuraRestrictionsEntry s_invisibilityRestrictions = { 0, 0, 12, 0, 0, 0, 0, 0 };
    SpellAuraRestrictionsEntry s_divineShieldRestrictions = { 0, 0, 0, 0, 0, 0, 61988, 0 };
    SpellAuraRestrictionsEntry s_otherColumnsRestrictions = { 0, 0, 0, 0, 96223, 96223, 0, 96223 };
    const uint32 kInvisibilityRestrictions = 81;
    const uint32 kDivineShieldRestrictions = 6;
    const uint32 kOtherColumnsRestrictions = 94330;
    const uint32 kMissingRestrictions = 94331;      // a row id the store never holds

    /// Puts the restriction rows into their store once: each carrier's as the client has it, and the three others.
    void StoreMarkerRestrictions()
    {
        static bool stored = false;
        if (stored)
        {
            return;
        }
        stored = true;
        for (std::size_t i = 0; i < kMarkerCarriers; ++i)
        {
            s_carrierRestrictions[i].CasterAuraSpell = MARKER_CARRIERS[i].casterAuraSpell;
            s_carrierRestrictions[i].ExcludeCasterAuraSpell = kRunSpeedMarker;
            sSpellAuraRestrictionsStore.SetEntry(MARKER_CARRIERS[i].restrictionsId, &s_carrierRestrictions[i]);
        }
        sSpellAuraRestrictionsStore.SetEntry(kInvisibilityRestrictions, &s_invisibilityRestrictions);
        sSpellAuraRestrictionsStore.SetEntry(kDivineShieldRestrictions, &s_divineShieldRestrictions);
        sSpellAuraRestrictionsStore.SetEntry(kOtherColumnsRestrictions, &s_otherColumnsRestrictions);
    }

    const uint32 kCarrierSpell = 94332;             // a speed buff on Sprint's row (1180)
    const uint32 kOtherCarrierSpell = 94333;        // another, on Stampeding Roar's row (1366)
    const uint32 kNonCarrierSpell = 94334;          // a buff on Divine Shield's row (6)

    MarkerRowStorage s_markerRow = {};
    MarkerRowStorage s_carrierRow = {};
    MarkerRowStorage s_otherCarrierRow = {};
    MarkerRowStorage s_nonCarrierRow = {};

    /// Puts the marker, the two carriers and the buff that carries none into the spell store once.
    void StoreHolderSpells()
    {
        static bool stored = false;
        if (stored)
        {
            return;
        }
        stored = true;
        StoreMarkerRestrictions();
        sSpellStore.SetEntry(kRunSpeedMarker, &RestrictedSpell(s_markerRow, kRunSpeedMarker, 0));
        sSpellStore.SetEntry(kCarrierSpell, &RestrictedSpell(s_carrierRow, kCarrierSpell, 1180));
        sSpellStore.SetEntry(kOtherCarrierSpell, &RestrictedSpell(s_otherCarrierRow, kOtherCarrierSpell, 1366));
        sSpellStore.SetEntry(kNonCarrierSpell,
                             &RestrictedSpell(s_nonCarrierRow, kNonCarrierSpell, kDivineShieldRestrictions));
    }

    /// A creature with its update fields allocated and no world, map, template or AI.
    class MarkerBearer : public Creature
    {
        public:
            MarkerBearer()
            {
                _InitValues();
            }

            // Unit::CleanupsBeforeDelete tears down an in-world unit only while the update fields exist; a holder still
            // in the map is the test's own and leaves the map first.
            ~MarkerBearer()
            {
                m_auras.Holders().clear();
                delete[] m_uint32Values;
                m_uint32Values = NULL;
            }

            /// A holder of the stored spell `spellId` on this creature, made through its constructor with no aura and
            /// put in the holder map as Unit::AddSpellAuraHolder files it. Its caster is this creature, whose guid is
            /// empty, so its removal looks up no other unit.
            SpellAuraHolder* Hold(uint32 spellId)
            {
                SpellAuraHolder* holder = new SpellAuraHolder(sSpellStore.LookupEntry(spellId), this, NULL, NULL, NULL);
                m_auras.AddHolder(spellId, holder);
                return holder;
            }
    };
}

TEST(SpellTables_TheSpeedBuffMarkerIsTheRunSpeedMarker)
{
    CHECK_EQ(SpeedBuffMarker(), kRunSpeedMarker);
}

TEST(SpellTables_EverySpellWhoseRestrictionsExcludeItsCasterOnTheMarkerCarriesIt)
{
    StoreMarkerRestrictions();
    MarkerRowStorage rows[kMarkerCarriers] = {};
    for (std::size_t i = 0; i < kMarkerCarriers; ++i)
    {
        MarkerCarrier const& carrier = MARKER_CARRIERS[i];
        CHECK(CarriesSpeedBuffMarker(RestrictedSpell(rows[i], carrier.spellId, carrier.restrictionsId)));
    }
    CHECK_EQ(kMarkerCarriers, std::size_t(14));

    // The row decides, not the id: Sprint's id on no restrictions row carries nothing.
    MarkerRowStorage bare = {};
    CHECK(!CarriesSpeedBuffMarker(RestrictedSpell(bare, 2983, 0)));

    // Body and Soul's second rank alone carries it by its id, on no restrictions row, as retail marks its holders.
    MarkerRowStorage bodyAndSoul = {};
    CHECK(CarriesSpeedBuffMarker(RestrictedSpell(bodyAndSoul, 65081, 0)));
}

TEST(SpellTables_ASpellCarriesNoMarkerWhenItsRestrictionsDoNotExcludeItsCasterOnIt)
{
    StoreMarkerRestrictions();
    MarkerRowStorage blink = {};
    MarkerRowStorage invisibility = {};
    MarkerRowStorage divineShield = {};
    MarkerRowStorage otherColumns = {};
    MarkerRowStorage missing = {};

    // Blink has no restrictions row; Invisibility's excludes a caster aura state; Divine Shield's excludes its caster
    // on another aura (61988).
    CHECK(!CarriesSpeedBuffMarker(RestrictedSpell(blink, 1953, 0)));
    CHECK(!CarriesSpeedBuffMarker(RestrictedSpell(invisibility, 32612, kInvisibilityRestrictions)));
    CHECK(!CarriesSpeedBuffMarker(RestrictedSpell(divineShield, 642, kDivineShieldRestrictions)));

    // The marker in every other spell column of the row, and a row the store does not hold.
    CHECK(!CarriesSpeedBuffMarker(RestrictedSpell(otherColumns, 94335, kOtherColumnsRestrictions)));
    CHECK(!CarriesSpeedBuffMarker(RestrictedSpell(missing, 94336, kMissingRestrictions)));
}

TEST(SpellTables_ABuffThatCarriesTheMarkerTakesItAwayHoweverItEnds)
{
    StoreHolderSpells();
    // AURA_REMOVE_BY_STACK is a same-caster re-application's removal of the holder it replaces.
    for (AuraRemoveMode mode : { AURA_REMOVE_BY_EXPIRE, AURA_REMOVE_BY_CANCEL, AURA_REMOVE_BY_DISPEL,
                                 AURA_REMOVE_BY_DEATH, AURA_REMOVE_BY_STACK, AURA_REMOVE_BY_DEFAULT })
    {
        MarkerBearer unit;
        SpellAuraHolder* marker = unit.Hold(kRunSpeedMarker);
        SpellAuraHolder* buff = unit.Hold(kCarrierSpell);
        CHECK(unit.GetSpellAuraHolder(kRunSpeedMarker) == marker);
        unit.RemoveSpellAuraHolder(buff, mode);
        CHECK(unit.GetSpellAuraHolder(kCarrierSpell) == NULL);
        CHECK(unit.GetSpellAuraHolder(kRunSpeedMarker) == NULL);
        CHECK(unit.GetSpellAuraHolderMap().empty());
    }
}

TEST(SpellTables_TheMarkerStaysWhileAnotherBuffThatCarriesItStands)
{
    StoreHolderSpells();
    MarkerBearer unit;
    SpellAuraHolder* marker = unit.Hold(kRunSpeedMarker);
    SpellAuraHolder* first = unit.Hold(kCarrierSpell);
    SpellAuraHolder* second = unit.Hold(kOtherCarrierSpell);

    unit.RemoveSpellAuraHolder(first, AURA_REMOVE_BY_EXPIRE);
    CHECK(unit.GetSpellAuraHolder(kRunSpeedMarker) == marker);
    CHECK(unit.GetSpellAuraHolder(kOtherCarrierSpell) == second);

    unit.RemoveSpellAuraHolder(second, AURA_REMOVE_BY_EXPIRE);
    CHECK(unit.GetSpellAuraHolder(kRunSpeedMarker) == NULL);
    CHECK(unit.GetSpellAuraHolderMap().empty());
}

TEST(SpellTables_ABuffThatCarriesNoMarkerLeavesIt)
{
    StoreHolderSpells();
    MarkerBearer unit;
    SpellAuraHolder* marker = unit.Hold(kRunSpeedMarker);
    SpellAuraHolder* other = unit.Hold(kNonCarrierSpell);

    unit.RemoveSpellAuraHolder(other, AURA_REMOVE_BY_EXPIRE);
    CHECK(unit.GetSpellAuraHolder(kRunSpeedMarker) == marker);

    // The marker's own end takes nothing else with it.
    unit.RemoveSpellAuraHolder(marker, AURA_REMOVE_BY_EXPIRE);
    CHECK(unit.GetSpellAuraHolderMap().empty());
}

// A shield whose points ran out leaves through RemoveDepletedShield, which takes that holder alone: another caster's
// holder of the same spell stays, where the removal by spell id takes both. The holders are made as the marker tests
// make theirs, on a spell row of the test's own put into the spell store; the second caster is a creature guid that
// the bare creature, never in a world, looks up as no unit.
namespace
{
    const uint32 kShieldSpell = 94337;

    MarkerRowStorage s_shieldRow = {};

    void StoreShieldSpell()
    {
        static bool stored = false;
        if (stored)
        {
            return;
        }
        stored = true;
        sSpellStore.SetEntry(kShieldSpell, &RestrictedSpell(s_shieldRow, kShieldSpell, 0));
    }
}

TEST(SpellTables_ADepletedShieldLeavesAndTheOtherCastersHolderStays)
{
    StoreShieldSpell();
    const ObjectGuid otherCaster(HIGHGUID_UNIT, uint32(50249), uint32(7));

    MarkerBearer unit;
    SpellAuraHolder* own = unit.Hold(kShieldSpell);
    SpellAuraHolder* other = unit.Hold(kShieldSpell);
    other->SetCasterGuid(otherCaster);
    CHECK_EQ(unit.GetSpellAuraHolderMap().size(), size_t(2));

    RemoveDepletedShield(own);
    CHECK_EQ(unit.GetSpellAuraHolderMap().size(), size_t(1));
    CHECK(unit.GetSpellAuraHolder(kShieldSpell) == other);
    CHECK(unit.GetSpellAuraHolder(kShieldSpell, otherCaster) == other);

    unit.RemoveSpellAuraHolder(other, AURA_REMOVE_BY_EXPIRE);
    CHECK(unit.GetSpellAuraHolderMap().empty());
}

TEST(SpellTables_TheRemovalBySpellIdTakesEveryCastersShield)
{
    StoreShieldSpell();
    MarkerBearer unit;
    unit.Hold(kShieldSpell);
    SpellAuraHolder* other = unit.Hold(kShieldSpell);
    other->SetCasterGuid(ObjectGuid(HIGHGUID_UNIT, uint32(50249), uint32(7)));

    unit.RemoveAurasDueToSpell(kShieldSpell, NULL, AURA_REMOVE_BY_SHIELD_BREAK);
    CHECK(unit.GetSpellAuraHolderMap().empty());
}

// Divine Aegis and Power Word: Shield stand together. The rows are the client's shapes, built in the stores: each a
// priest spell (class set 6) whose one effect applies a school absorb of every school (6/69/127), Power Word: Shield
// on mask 0x1 and icon 566, Divine Aegis on mask 0x1 << 56 and icon 2820, beside a third priest absorb of that shape,
// which the generic rule (same class set, same effects) keeps from standing beside either. The test takes the spell
// rows out of the spell store again; the effects stay with the static rows they point at.
namespace
{
    const uint32 kPowerWordShield = 17;
    const uint32 kDivineAegis = 47753;
    const uint32 kOtherPriestAbsorb = 94338;
    const uint32 kOptionsPowerWordShield = 94460;
    const uint32 kOptionsDivineAegis = 94461;
    const uint32 kOptionsOtherAbsorb = 94462;

    SpellClassOptionsEntry s_optionsPowerWordShield;
    SpellClassOptionsEntry s_optionsDivineAegis;
    SpellClassOptionsEntry s_optionsOtherAbsorb;
    ServerSpell s_priestAbsorbRows[3];

    /// Puts the three priest absorbs into the spell store, their class options into theirs and their effects beside
    /// them; the class options and the effects once.
    void StorePriestAbsorbs()
    {
        static bool stored = false;
        struct Row
        {
            uint32 spellId;
            uint32 icon;
            uint32 options;
            SpellClassOptionsEntry* optionsRow;
            uint64 mask;
        };
        Row const rows[3] =
        {
            { kPowerWordShield, 566, kOptionsPowerWordShield, &s_optionsPowerWordShield, UI64LIT(0x1) },
            { kDivineAegis, 2820, kOptionsDivineAegis, &s_optionsDivineAegis, UI64LIT(0x1) << 56 },
            { kOtherPriestAbsorb, 0, kOptionsOtherAbsorb, &s_optionsOtherAbsorb, UI64LIT(0x2) },
        };
        for (std::size_t i = 0; i < 3; ++i)
        {
            SpellEntry& row = s_priestAbsorbRows[i].Entry();
            row.ID = rows[i].spellId;
            row.SpellIconID = rows[i].icon;
            row.ClassOptionsID = rows[i].options;
            sSpellStore.SetEntry(rows[i].spellId, &row);
            if (stored)
            {
                continue;
            }
            rows[i].optionsRow->SpellClassSet = SPELLFAMILY_PRIEST;
            rows[i].optionsRow->SpellClassMask = ClassFamilyMask(rows[i].mask);
            sSpellClassOptionsStore.SetEntry(rows[i].options, rows[i].optionsRow);
            SpellEffectEntry& effect = s_priestAbsorbRows[i].effect;
            effect.SpellID = rows[i].spellId;
            effect.EffectIndex = EFFECT_INDEX_0;
            effect.Effect = SPELL_EFFECT_APPLY_AURA;
            effect.EffectAura = SPELL_AURA_SCHOOL_ABSORB;
            effect.EffectMiscValue_0 = SPELL_SCHOOL_MASK_ALL;
            AddSpellEffectEntry(&effect);
        }
        stored = true;
    }

    /// Takes the three priest absorbs out of the spell store.
    void ForgetPriestAbsorbs()
    {
        for (uint32 spellId : { kPowerWordShield, kDivineAegis, kOtherPriestAbsorb })
        {
            sSpellStore.SetEntry(spellId, NULL);
        }
    }
}

TEST(SpellTables_DivineAegisAndPowerWordShieldStandTogetherInEitherOrder)
{
    StorePriestAbsorbs();

    // Either arriving beside the other keeps it.
    CHECK(!sSpellMgr.IsNoStackSpellDueToSpell(kDivineAegis, kPowerWordShield));
    CHECK(!sSpellMgr.IsNoStackSpellDueToSpell(kPowerWordShield, kDivineAegis));

    // The generic rule stands for every other priest absorb of their shape, against each of them both ways.
    CHECK(sSpellMgr.IsNoStackSpellDueToSpell(kOtherPriestAbsorb, kPowerWordShield));
    CHECK(sSpellMgr.IsNoStackSpellDueToSpell(kPowerWordShield, kOtherPriestAbsorb));
    CHECK(sSpellMgr.IsNoStackSpellDueToSpell(kOtherPriestAbsorb, kDivineAegis));
    CHECK(sSpellMgr.IsNoStackSpellDueToSpell(kDivineAegis, kOtherPriestAbsorb));

    ForgetPriestAbsorbs();
    CHECK(sSpellStore.LookupEntry(kDivineAegis) == NULL);
}

// Strength of Soul's immunities and Weakened Soul stand together. The rows are the client's shapes, built in the
// stores: each a priest spell (class set 6) of icon 177, Weakened Soul (6788) on mask 0x20000000, the two immunities
// (96266, 96267) on mask 0, beside a third priest aura of that icon and mask, which the generic same-icon rule keeps
// from standing beside any of them. The rows hold no effect: the rule reads the effects only for a spell modifier,
// which none of the client's three holds. The test takes the spell rows out of the spell store again.
namespace
{
    const uint32 kWeakenedSoul = 6788;
    const uint32 kSoulImmunities[] = { 96266, 96267 };
    const uint32 kOtherSoulIconAura = 94339;
    const uint32 kOptionsWeakenedSoul = 94463;
    const uint32 kOptionsSoulImmunity = 94464;

    SpellClassOptionsEntry s_optionsWeakenedSoul;
    SpellClassOptionsEntry s_optionsSoulImmunity;
    ServerSpell s_soulIconRows[4];

    /// Puts Weakened Soul, the two immunities and the third aura into the spell store and their class options into
    /// theirs; the class options once.
    void StoreSoulIconRows()
    {
        static bool stored = false;
        if (!stored)
        {
            stored = true;
            s_optionsWeakenedSoul.SpellClassSet = SPELLFAMILY_PRIEST;
            s_optionsWeakenedSoul.SpellClassMask = ClassFamilyMask(UI64LIT(0x20000000));
            s_optionsSoulImmunity.SpellClassSet = SPELLFAMILY_PRIEST;
            s_optionsSoulImmunity.SpellClassMask = ClassFamilyMask(UI64LIT(0));
            sSpellClassOptionsStore.SetEntry(kOptionsWeakenedSoul, &s_optionsWeakenedSoul);
            sSpellClassOptionsStore.SetEntry(kOptionsSoulImmunity, &s_optionsSoulImmunity);
        }
        uint32 const ids[4] = { kWeakenedSoul, kSoulImmunities[0], kSoulImmunities[1], kOtherSoulIconAura };
        for (std::size_t i = 0; i < 4; ++i)
        {
            SpellEntry& row = s_soulIconRows[i].Entry();
            row.ID = ids[i];
            row.SpellIconID = 177;
            row.ClassOptionsID = ids[i] == kWeakenedSoul ? kOptionsWeakenedSoul : kOptionsSoulImmunity;
            sSpellStore.SetEntry(ids[i], &row);
        }
    }

    /// Takes them out of the spell store.
    void ForgetSoulIconRows()
    {
        for (uint32 spellId : { kWeakenedSoul, kSoulImmunities[0], kSoulImmunities[1], kOtherSoulIconAura })
        {
            sSpellStore.SetEntry(spellId, NULL);
        }
    }
}

TEST(SpellTables_StrengthOfSoulsImmunitiesAndWeakenedSoulStandTogetherInEitherOrder)
{
    StoreSoulIconRows();

    // Either immunity arriving beside Weakened Soul keeps it, and Weakened Soul arriving beside either keeps it.
    for (uint32 immunity : kSoulImmunities)
    {
        CHECK(!sSpellMgr.IsNoStackSpellDueToSpell(immunity, kWeakenedSoul));
        CHECK(!sSpellMgr.IsNoStackSpellDueToSpell(kWeakenedSoul, immunity));
    }

    // The generic rule stands for the icon's other pairs: the third aura against each of them both ways, and the two
    // immunities against each other.
    for (uint32 spellId : { kWeakenedSoul, kSoulImmunities[0], kSoulImmunities[1] })
    {
        CHECK(sSpellMgr.IsNoStackSpellDueToSpell(kOtherSoulIconAura, spellId));
        CHECK(sSpellMgr.IsNoStackSpellDueToSpell(spellId, kOtherSoulIconAura));
    }
    CHECK(sSpellMgr.IsNoStackSpellDueToSpell(kSoulImmunities[0], kSoulImmunities[1]));
    CHECK(sSpellMgr.IsNoStackSpellDueToSpell(kSoulImmunities[1], kSoulImmunities[0]));

    // Divine Aegis and Power Word: Shield still stand together beside them, and the generic rule still keeps the third
    // priest absorb from either.
    StorePriestAbsorbs();
    CHECK(!sSpellMgr.IsNoStackSpellDueToSpell(kDivineAegis, kPowerWordShield));
    CHECK(!sSpellMgr.IsNoStackSpellDueToSpell(kPowerWordShield, kDivineAegis));
    CHECK(sSpellMgr.IsNoStackSpellDueToSpell(kOtherPriestAbsorb, kPowerWordShield));
    CHECK(sSpellMgr.IsNoStackSpellDueToSpell(kOtherPriestAbsorb, kDivineAegis));
    ForgetPriestAbsorbs();

    ForgetSoulIconRows();
    CHECK(sSpellStore.LookupEntry(kWeakenedSoul) == NULL);
}

TEST(SpellTables_DivineHymnsHealAndLightwellsRenewAreTheClassNoneHealsThatActAsSpellHeals)
{
    // Divine Hymn's heal (64844) and Lightwell's Renew (7001), whose damage class is none in the client's data
    CHECK(Spells::HealActsAsSpellHeal(64844));
    CHECK(Spells::HealActsAsSpellHeal(7001));

    // Divine Hymn's channel (64843), Glyph of Power Word: Shield's heal (56160), Echo of Light's heal over time
    // (77489), Flash Heal (2061), Renew (139) and an id no table holds do not
    for (uint32 spellId : { 64843u, 56160u, 77489u, 2061u, 139u, 12345u, 0u })
    {
        CHECK(!Spells::HealActsAsSpellHeal(spellId));
    }

    // No heal is both made a spell heal and kept from the done bonus
    for (uint32 spellId : { 64844u, 7001u, 56160u })
    {
        CHECK(!(Spells::HealActsAsSpellHeal(spellId) && Spells::HealTakesNoDoneBonus(spellId)));
    }

    // The table holds exactly its two rows
    uint32 rows = 0;
    for (uint32 spellId = 0; spellId < 1000000; ++spellId)
    {
        if (Spells::HealActsAsSpellHeal(spellId))
        {
            ++rows;
        }
    }
    CHECK_EQ(rows, uint32(2));
}

TEST(SpellTables_TheHealRulesTakeTheTwoSpellHealsAsMagicAndEveryOtherSpellAsItsOwnClass)
{
    // Class none in the client's data: the two heals are taken as magic, Echo of Light's heal and any other spell as
    // none
    CHECK_EQ(Spells::HealRuleDamageClass(64844, SPELL_DAMAGE_CLASS_NONE), uint32(SPELL_DAMAGE_CLASS_MAGIC));
    CHECK_EQ(Spells::HealRuleDamageClass(7001, SPELL_DAMAGE_CLASS_NONE), uint32(SPELL_DAMAGE_CLASS_MAGIC));
    for (uint32 spellId : { 77489u, 56160u, 12345u, 0u })
    {
        CHECK_EQ(Spells::HealRuleDamageClass(spellId, SPELL_DAMAGE_CLASS_NONE), uint32(SPELL_DAMAGE_CLASS_NONE));
    }

    // Any other class stays as it is, for the two heals too
    for (uint32 damageClass : { uint32(SPELL_DAMAGE_CLASS_MAGIC), uint32(SPELL_DAMAGE_CLASS_MELEE),
                                uint32(SPELL_DAMAGE_CLASS_RANGED) })
    {
        for (uint32 spellId : { 64844u, 7001u, 2061u, 12345u })
        {
            CHECK_EQ(Spells::HealRuleDamageClass(spellId, damageClass), damageClass);
        }
    }
}

// The two effects of the spell-heal table past the done bonus, on units: a heal stored as Divine Hymn's (64844), of
// damage class none, beside a class-none heal of the test's own that no table names, both of the priest family with
// Divine Hymn's class bit (C 0x4), and two auras of the test's own whose effect masks name that bit, standing for Grace
// (aura 283, 8%) and Holy Word: Serenity's crit aura (aura 308). The healer is a player, so his crit rolls, with the
// sheet of his holy crit set by call; the healed unit and the other caster are bare creatures (update fields
// allocated; no map, template or AI) with guids of their own. The auras are made through their constructors on rows
// put into the spell store, their holders naming their caster by guid, and put in the healed unit's list of their
// type.
namespace
{
    const uint32 kSpellHymnHeal = 64844;
    const uint32 kOtherClassNoneHeal = 94540;
    const uint32 kGraceLike = 94541;
    const uint32 kSerenityLike = 94542;
    const uint32 kOptionsHymnBit = 94543;
    const uint32 kOptionsPriestAura = 94544;

    SpellClassOptionsEntry s_optionsHymnBit;
    SpellClassOptionsEntry s_optionsPriestAura;
    ServerSpell s_healRuleRows[4];

    /// Puts the two heals and the two auras into the spell store, their class options into theirs and their effects
    /// beside them; the class options and the effects once.
    void StoreHealRuleRows()
    {
        static bool stored = false;
        struct Row
        {
            uint32 spellId;
            uint32 options;
            uint32 effect;
            uint32 aura;
            int32 points;
        };
        Row const rows[4] =
        {
            { kSpellHymnHeal, kOptionsHymnBit, SPELL_EFFECT_HEAL, 0, 1000 },
            { kOtherClassNoneHeal, kOptionsHymnBit, SPELL_EFFECT_HEAL, 0, 1000 },
            { kGraceLike, kOptionsPriestAura, SPELL_EFFECT_APPLY_AURA, SPELL_AURA_MOD_HEALING_RECEIVED, 8 },
            { kSerenityLike, kOptionsPriestAura, SPELL_EFFECT_APPLY_AURA, SPELL_AURA_MOD_CRIT_FROM_CASTER, 100 },
        };
        for (std::size_t i = 0; i < 4; ++i)
        {
            SpellEntry& row = s_healRuleRows[i].Entry();
            row.ID = rows[i].spellId;
            row.ClassOptionsID = rows[i].options;
            row.SchoolMask = SPELL_SCHOOL_MASK_HOLY;
            sSpellStore.SetEntry(rows[i].spellId, &row);
            if (stored)
            {
                continue;
            }
            SpellEffectEntry& effect = s_healRuleRows[i].effect;
            effect.SpellID = rows[i].spellId;
            effect.EffectIndex = EFFECT_INDEX_0;
            effect.Effect = rows[i].effect;
            effect.EffectAura = rows[i].aura;
            effect.EffectBasePoints = rows[i].points;
            effect.EffectSpellClassMask = rows[i].aura ? ClassFamilyMask(0, 0x4) : ClassFamilyMask();
            AddSpellEffectEntry(&effect);
        }
        if (!stored)
        {
            s_optionsHymnBit.SpellClassSet = SPELLFAMILY_PRIEST;
            s_optionsHymnBit.SpellClassMask = ClassFamilyMask(0, 0x4);
            s_optionsPriestAura.SpellClassSet = SPELLFAMILY_PRIEST;
            sSpellClassOptionsStore.SetEntry(kOptionsHymnBit, &s_optionsHymnBit);
            sSpellClassOptionsStore.SetEntry(kOptionsPriestAura, &s_optionsPriestAura);
        }
        stored = true;
    }

    /// Takes the four rows out of the spell store.
    void ForgetHealRuleRows()
    {
        for (uint32 spellId : { kSpellHymnHeal, kOtherClassNoneHeal, kGraceLike, kSerenityLike })
        {
            sSpellStore.SetEntry(spellId, NULL);
        }
    }

    /// A creature of guid `guidLow` with its update fields allocated and no world, map, template or AI.
    class HealRuleUnit : public Creature
    {
        public:
            explicit HealRuleUnit(uint32 guidLow)
            {
                Object::_Create(guidLow, 50249, HIGHGUID_UNIT);
            }

            // The lists ListAura filled are emptied before the unit goes; their auras are the test's own.
            ~HealRuleUnit()
            {
                for (AuraType type : m_listedTypes)
                {
                    m_auras.ByType(type).clear();
                }
                delete[] m_uint32Values;
                m_uint32Values = NULL;
            }

            /// Puts `aura` in the list of `type`, so the unit's reads of that aura type read it.
            void ListAura(AuraType type, Aura* aura)
            {
                m_auras.ByType(type).push_back(aura);
                m_listedTypes.push_back(type);
            }

            /// Empties the list of `type`.
            void Unlist(AuraType type)
            {
                m_auras.ByType(type).clear();
            }

        private:
            std::vector<AuraType> m_listedTypes;
    };

    /// A player of guid `guidLow`, never in a world.
    class HealRulePlayer : public Player
    {
        public:
            HealRulePlayer(WorldSession& session, uint32 guidLow) : Player(&session)
            {
                Object::_Create(guidLow, 0, HIGHGUID_PLAYER);
            }

            ~HealRulePlayer()
            {
                delete[] m_uint32Values;
                m_uint32Values = NULL;
            }
    };

    /// An aura of the stored spell `spellId` at `points` on `healed`, whose holder names `caster` as its caster, put
    /// in the healed unit's list of `type`; its holder, which the caller deletes.
    SpellAuraHolder* ListedAura(HealRuleUnit& healed, uint32 spellId, AuraType type, int32 points, ObjectGuid caster)
    {
        SpellEntry const* spell = sSpellStore.LookupEntry(spellId);
        SpellAuraHolder* holder = new SpellAuraHolder(spell, &healed, NULL, NULL, NULL);
        holder->SetCasterGuid(caster);
        holder->AddAura(CreateAura(spell, EFFECT_INDEX_0, &points, holder, &healed), EFFECT_INDEX_0);
        healed.ListAura(type, holder->GetAuraByEffectIndex(EFFECT_INDEX_0));
        return holder;
    }
}

TEST(SpellTables_DivineHymnsHealTakesItsHealersGraceAsASpellHealDoesAndAnotherClassNoneHealDoesNot)
{
    StoreHealRuleRows();
    {
        WorldSession session(1, "heal", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
        HealRulePlayer healer(session, 1);
        HealRuleUnit other(2);
        HealRuleUnit healed(3);
        SpellEntry const* hymn = sSpellStore.LookupEntry(kSpellHymnHeal);
        SpellEntry const* otherHeal = sSpellStore.LookupEntry(kOtherClassNoneHeal);
        REQUIRE(hymn != NULL && otherHeal != NULL);
        CHECK_EQ(hymn->GetDmgClass(), uint32(SPELL_DAMAGE_CLASS_NONE));
        CHECK_EQ(otherHeal->GetDmgClass(), uint32(SPELL_DAMAGE_CLASS_NONE));
        CHECK(healer.GetObjectGuid() != other.GetObjectGuid());

        // No aura on the healed unit: 1000 of either heal is taken as 1000.
        CHECK_EQ(healed.SpellHealingBonusTaken(&healer, hymn, 1000, HEAL, 1), uint32(1000));
        CHECK_EQ(healed.SpellHealingBonusTaken(&healer, otherHeal, 1000, HEAL, 1), uint32(1000));

        // His Grace, 8%: the Hymn's heal from him, and its tick, take it; from the other caster they do not; the other
        // class-none heal, though Grace's mask names its class bit, returns before it.
        SpellAuraHolder* grace = ListedAura(healed, kGraceLike, SPELL_AURA_MOD_HEALING_RECEIVED, 8,
                                            healer.GetObjectGuid());
        CHECK_EQ(healed.SpellHealingBonusTaken(&healer, hymn, 1000, HEAL, 1), uint32(1080));
        CHECK_EQ(healed.SpellHealingBonusTaken(&healer, hymn, 1000, DOT, 1), uint32(1080));
        CHECK_EQ(healed.SpellHealingBonusTaken(&other, hymn, 1000, HEAL, 1), uint32(1000));
        CHECK_EQ(healed.SpellHealingBonusTaken(&healer, otherHeal, 1000, HEAL, 1), uint32(1000));
        healed.Unlist(SPELL_AURA_MOD_HEALING_RECEIVED);
        delete grace;
    }
    ForgetHealRuleRows();
    CHECK(sSpellStore.LookupEntry(kSpellHymnHeal) == NULL);
}

TEST(SpellTables_DivineHymnsHealRollsItsCritAsAMagicHealDoesAndAnotherClassNoneHealNever)
{
    StoreHealRuleRows();
    {
        WorldSession session(1, "heal", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
        HealRulePlayer healer(session, 1);
        HealRuleUnit other(2);
        HealRuleUnit healed(3);
        SpellEntry const* hymn = sSpellStore.LookupEntry(kSpellHymnHeal);
        SpellEntry const* otherHeal = sSpellStore.LookupEntry(kOtherClassNoneHeal);
        REQUIRE(hymn != NULL && otherHeal != NULL);
        uint16 const holySheet = uint16(PLAYER_SPELL_CRIT_PERCENTAGE1 + SPELL_SCHOOL_HOLY);

        // His holy crit at 100: the Hymn's heal crits; the other class-none heal never rolls.
        healer.SetFloatValue(holySheet, 100.0f);
        CHECK(healer.IsSpellCrit(&healed, hymn, SPELL_SCHOOL_MASK_HOLY, BASE_ATTACK, HEAL));
        CHECK(!healer.IsSpellCrit(&healed, otherHeal, SPELL_SCHOOL_MASK_HOLY, BASE_ATTACK, HEAL));

        // At 0, nothing crits.
        healer.SetFloatValue(holySheet, 0.0f);
        CHECK(!healer.IsSpellCrit(&healed, hymn, SPELL_SCHOOL_MASK_HOLY, BASE_ATTACK, HEAL));

        // His crit-from-caster aura at 100 on the healed unit, its mask naming the class bit: the Hymn's heal crits;
        // the other class-none heal still never rolls; a hit that is not a heal does not read the aura.
        SpellAuraHolder* own = ListedAura(healed, kSerenityLike, SPELL_AURA_MOD_CRIT_FROM_CASTER, 100,
                                          healer.GetObjectGuid());
        CHECK(healer.IsSpellCrit(&healed, hymn, SPELL_SCHOOL_MASK_HOLY, BASE_ATTACK, HEAL));
        CHECK(!healer.IsSpellCrit(&healed, otherHeal, SPELL_SCHOOL_MASK_HOLY, BASE_ATTACK, HEAL));
        CHECK(!healer.IsSpellCrit(&healed, hymn, SPELL_SCHOOL_MASK_HOLY, BASE_ATTACK, SPELL_DIRECT_DAMAGE));
        healed.Unlist(SPELL_AURA_MOD_CRIT_FROM_CASTER);
        delete own;

        // The other caster's: nothing crits.
        SpellAuraHolder* others = ListedAura(healed, kSerenityLike, SPELL_AURA_MOD_CRIT_FROM_CASTER, 100,
                                             other.GetObjectGuid());
        CHECK(!healer.IsSpellCrit(&healed, hymn, SPELL_SCHOOL_MASK_HOLY, BASE_ATTACK, HEAL));
        healed.Unlist(SPELL_AURA_MOD_CRIT_FROM_CASTER);
        delete others;
    }
    ForgetHealRuleRows();
    CHECK(sSpellStore.LookupEntry(kSpellHymnHeal) == NULL);
}

TEST(SpellTables_EchoOfLightsHealIsTheHealThatIgnoresTheHealingTakenPercent)
{
    CHECK(Spells::HealIgnoresTakenPercent(77489));

    // Echo of Light's mastery (77485), Divine Hymn's heal (64844), Lightwell's Renew (7001), Guardian Spirit (47788),
    // Flash Heal (2061) and an id no table holds do not
    for (uint32 spellId : { 77485u, 64844u, 7001u, 47788u, 2061u, 12345u, 0u })
    {
        CHECK(!Spells::HealIgnoresTakenPercent(spellId));
    }

    // The table holds exactly its one row
    uint32 rows = 0;
    for (uint32 spellId = 0; spellId < 1000000; ++spellId)
    {
        if (Spells::HealIgnoresTakenPercent(spellId))
        {
            ++rows;
        }
    }
    CHECK_EQ(rows, uint32(1));
}

// Echo of Light's heal (77489) and Divine Plea (54428) stand together. The rows are the client's shapes, built in the
// stores: both of icon 2821, the heal a spell of no class options whose effect 0 is a periodic heal, Divine Plea a
// paladin spell (class set 10, mask 0 / 0x1) whose effect 0 restores mana over time and whose effect 1 is a percent
// spell modifier, at an index the heal has no effect at, so the generic same-icon rule would keep them apart; beside a
// third aura of that icon and the heal's shape (the encounters' Fountain of Light and Twilight Renewal are such
// auras), which the rule keeps from standing beside either. The test takes the spell rows out of the spell store
// again; the class options and the effects stay with the static rows they point at.
namespace
{
    const uint32 kEchoOfLightHeal = 77489;
    const uint32 kDivinePlea = 54428;
    const uint32 kOtherEchoIconAura = 94345;
    const uint32 kOptionsDivinePlea = 94466;
    const uint32 kEchoIcon = 2821;

    SpellClassOptionsEntry s_optionsDivinePlea;
    ServerSpell s_echoIconRows[3];
    SpellEffectEntry s_divinePleaModifier;

    /// Puts the heal, Divine Plea and the third aura into the spell store, Divine Plea's class options into theirs and
    /// the effects beside them; the class options and the effects once.
    void StoreEchoIconRows()
    {
        static bool stored = false;
        uint32 const ids[3] = { kEchoOfLightHeal, kDivinePlea, kOtherEchoIconAura };
        for (std::size_t i = 0; i < 3; ++i)
        {
            SpellEntry& row = s_echoIconRows[i].Entry();
            row.ID = ids[i];
            row.SpellIconID = kEchoIcon;
            row.ClassOptionsID = ids[i] == kDivinePlea ? kOptionsDivinePlea : 0;
            sSpellStore.SetEntry(ids[i], &row);
            if (stored)
            {
                continue;
            }
            SpellEffectEntry& effect = s_echoIconRows[i].effect;
            effect.SpellID = ids[i];
            effect.EffectIndex = EFFECT_INDEX_0;
            effect.Effect = SPELL_EFFECT_APPLY_AURA;
            effect.EffectAura = ids[i] == kDivinePlea ? SPELL_AURA_OBS_MOD_MANA : SPELL_AURA_PERIODIC_HEAL;
            AddSpellEffectEntry(&effect);
        }
        if (!stored)
        {
            s_optionsDivinePlea.SpellClassSet = SPELLFAMILY_PALADIN;
            s_optionsDivinePlea.SpellClassMask = ClassFamilyMask(UI64LIT(0), 0x1);
            sSpellClassOptionsStore.SetEntry(kOptionsDivinePlea, &s_optionsDivinePlea);
            s_divinePleaModifier.SpellID = kDivinePlea;
            s_divinePleaModifier.EffectIndex = EFFECT_INDEX_1;
            s_divinePleaModifier.Effect = SPELL_EFFECT_APPLY_AURA;
            s_divinePleaModifier.EffectAura = SPELL_AURA_ADD_PCT_MODIFIER;
            AddSpellEffectEntry(&s_divinePleaModifier);
        }
        stored = true;
    }

    /// Takes them out of the spell store.
    void ForgetEchoIconRows()
    {
        for (uint32 spellId : { kEchoOfLightHeal, kDivinePlea, kOtherEchoIconAura })
        {
            sSpellStore.SetEntry(spellId, NULL);
        }
    }
}

TEST(SpellTables_EchoOfLightsHealAndDivinePleaStandTogetherInEitherOrder)
{
    StoreEchoIconRows();
    CHECK(sSpellStore.LookupEntry(kDivinePlea)->GetSpellFamilyName() == SPELLFAMILY_PALADIN);
    CHECK(sSpellStore.LookupEntry(kEchoOfLightHeal)->GetSpellClassOptions() == NULL);

    // The heal arriving beside Divine Plea keeps it, and Divine Plea arriving beside the heal keeps it.
    CHECK(!sSpellMgr.IsNoStackSpellDueToSpell(kEchoOfLightHeal, kDivinePlea));
    CHECK(!sSpellMgr.IsNoStackSpellDueToSpell(kDivinePlea, kEchoOfLightHeal));

    // The generic rule stands for the icon's other pairs: the third aura against each of them, both ways.
    for (uint32 spellId : { kEchoOfLightHeal, kDivinePlea })
    {
        CHECK(sSpellMgr.IsNoStackSpellDueToSpell(kOtherEchoIconAura, spellId));
        CHECK(sSpellMgr.IsNoStackSpellDueToSpell(spellId, kOtherEchoIconAura));
    }

    // Divine Aegis and Power Word: Shield still stand together beside them.
    StorePriestAbsorbs();
    CHECK(!sSpellMgr.IsNoStackSpellDueToSpell(kDivineAegis, kPowerWordShield));
    CHECK(!sSpellMgr.IsNoStackSpellDueToSpell(kPowerWordShield, kDivineAegis));
    ForgetPriestAbsorbs();

    ForgetEchoIconRows();
    CHECK(sSpellStore.LookupEntry(kEchoOfLightHeal) == NULL);
    CHECK(sSpellStore.LookupEntry(kDivinePlea) == NULL);
}

// Holy Word: Sanctuary's caster aura (88685) and Divine Hymn's healing buff (64844) stand together. The rows are the
// client's shapes, built in the stores: each a priest spell (class set 6) of icon 2845, Sanctuary on mask 0x200000 /
// 0 / 0x400000 and the buff on mask 0 / 0 / 0x4, beside a third priest aura of that icon, which the generic same-icon
// rule keeps from standing beside either. The rows hold no effect: the rule reads the effects only for a spell
// modifier, which neither of the client's two holds. The test takes the spell rows out of the spell store again.
namespace
{
    const uint32 kSanctuaryAura = 88685;
    const uint32 kHymnBuff = 64844;
    const uint32 kOtherSanctuaryIconAura = 94343;
    const uint32 kOptionsSanctuary = 94467;
    const uint32 kOptionsHymnBuff = 94468;
    const uint32 kSanctuaryIcon = 2845;

    SpellClassOptionsEntry s_optionsSanctuary;
    SpellClassOptionsEntry s_optionsHymnBuff;
    ServerSpell s_sanctuaryIconRows[3];

    /// Puts Sanctuary's aura, the Hymn's buff and the third aura into the spell store and their class options into
    /// theirs; the class options once.
    void StoreSanctuaryIconRows()
    {
        static bool stored = false;
        if (!stored)
        {
            stored = true;
            s_optionsSanctuary.SpellClassSet = SPELLFAMILY_PRIEST;
            s_optionsSanctuary.SpellClassMask = ClassFamilyMask(UI64LIT(0x200000), 0x400000);
            s_optionsHymnBuff.SpellClassSet = SPELLFAMILY_PRIEST;
            s_optionsHymnBuff.SpellClassMask = ClassFamilyMask(UI64LIT(0), 0x4);
            sSpellClassOptionsStore.SetEntry(kOptionsSanctuary, &s_optionsSanctuary);
            sSpellClassOptionsStore.SetEntry(kOptionsHymnBuff, &s_optionsHymnBuff);
        }
        uint32 const ids[3] = { kSanctuaryAura, kHymnBuff, kOtherSanctuaryIconAura };
        for (std::size_t i = 0; i < 3; ++i)
        {
            SpellEntry& row = s_sanctuaryIconRows[i].Entry();
            row.ID = ids[i];
            row.SpellIconID = kSanctuaryIcon;
            row.ClassOptionsID = ids[i] == kSanctuaryAura ? kOptionsSanctuary : kOptionsHymnBuff;
            sSpellStore.SetEntry(ids[i], &row);
        }
    }

    /// Takes them out of the spell store.
    void ForgetSanctuaryIconRows()
    {
        for (uint32 spellId : { kSanctuaryAura, kHymnBuff, kOtherSanctuaryIconAura })
        {
            sSpellStore.SetEntry(spellId, NULL);
        }
    }
}

TEST(SpellTables_SanctuarysAuraAndDivineHymnsBuffStandTogetherInEitherOrder)
{
    StoreSanctuaryIconRows();

    // Either arriving beside the other keeps it.
    CHECK(!sSpellMgr.IsNoStackSpellDueToSpell(kSanctuaryAura, kHymnBuff));
    CHECK(!sSpellMgr.IsNoStackSpellDueToSpell(kHymnBuff, kSanctuaryAura));

    // The generic rule stands for the icon's other pairs: the third aura against each of them, both ways.
    for (uint32 spellId : { kSanctuaryAura, kHymnBuff })
    {
        CHECK(sSpellMgr.IsNoStackSpellDueToSpell(kOtherSanctuaryIconAura, spellId));
        CHECK(sSpellMgr.IsNoStackSpellDueToSpell(spellId, kOtherSanctuaryIconAura));
    }

    // The two pairs of the priest's earlier waves still stand together beside them, and the generic rule still keeps
    // the third priest absorb from either of the first.
    StorePriestAbsorbs();
    CHECK(!sSpellMgr.IsNoStackSpellDueToSpell(kDivineAegis, kPowerWordShield));
    CHECK(!sSpellMgr.IsNoStackSpellDueToSpell(kPowerWordShield, kDivineAegis));
    CHECK(sSpellMgr.IsNoStackSpellDueToSpell(kOtherPriestAbsorb, kPowerWordShield));
    CHECK(sSpellMgr.IsNoStackSpellDueToSpell(kOtherPriestAbsorb, kDivineAegis));
    ForgetPriestAbsorbs();
    StoreEchoIconRows();
    CHECK(!sSpellMgr.IsNoStackSpellDueToSpell(kEchoOfLightHeal, kDivinePlea));
    CHECK(!sSpellMgr.IsNoStackSpellDueToSpell(kDivinePlea, kEchoOfLightHeal));
    ForgetEchoIconRows();

    ForgetSanctuaryIconRows();
    CHECK(sSpellStore.LookupEntry(kSanctuaryAura) == NULL);
    CHECK(sSpellStore.LookupEntry(kHymnBuff) == NULL);
}
