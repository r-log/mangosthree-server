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

#include "spells/data/SpellMgr.h"
#include "spells/data/SpellTables.h"

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
    for (uint32 spellId : { 17941u, 22008u, 34936u, 48108u, 51124u, 54741u, 57761u, 64823u })
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
    for (uint32 spellId : { 45284u, 45297u, 77451u })
    {
        CHECK(IsOverloadIgnoringCastTime(spellId));
    }
    for (uint32 spellId : { 36573u, 42316u, 42385u })
    {
        CHECK(MayTargetTaxiFlyer(spellId));
    }

    // The switches' ids that are not 4.3.4 spells are in no set, and neither is an id no switch held.
    for (uint32 spellId : { 31834u, 34754u, 44401u, 12345u })
    {
        CHECK(!IsOneChargeModifierAura(spellId));
    }
    for (uint32 spellId : { 34027u, 58914u, 74396u, 12345u })
    {
        CHECK(!IsMaxStackAtCreateAura(spellId));
    }
    for (uint32 spellId : { 45286u, 45296u, 49239u, 49240u, 45298u, 45302u, 49268u, 49269u, 12345u })
    {
        CHECK(!IsOverloadIgnoringCastTime(spellId));
    }
    CHECK(!IsHealthToMaxAura(12345));
    CHECK(!MayTargetTaxiFlyer(12345));
}

TEST(SpellTables_TheProcRankTablesAnswerTheirSwitchesLiveRowsAndNoOthers)
{
    CHECK_EQ(GetArcanePotencyProcSpell(31571), uint32(57529));
    CHECK_EQ(GetArcanePotencyProcSpell(31572), uint32(57531));
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
    for (uint32 spellId : { 0u, 12345u })
    {
        CHECK_EQ(GetArcanePotencyProcSpell(spellId), uint32(0));
        CHECK_EQ(GetSecondWindProcSpell(spellId), uint32(0));
        CHECK_EQ(GetRapidRecuperationProcSpell(spellId), uint32(0));
        CHECK_EQ(GetBlessedRecoveryProcSpell(spellId), uint32(0));
        CHECK_EQ(GetDeepWoundsWeaponDamagePercent(spellId), uint32(0));
        CHECK(!IsBlazingSpeedRank(spellId));
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
