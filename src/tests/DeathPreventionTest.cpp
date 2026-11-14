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

/// The death-prevention save (combat/DeathPrevention.h) against the 4.3.4 tooltips: Cheat Death "will instead reduce
/// you to 10% of your maximum health", Cauterize "will instead bring you to 40% of your maximum health", Guardian
/// Spirit "heals the target of 50% of their maximum health". The floor is the percent of the maximum health,
/// truncated; a 50000-health bearer has the floors 5000 (10%), 20000 (40%) and 25000 (50%). When several of these
/// auras meet one blow, the first one its save uses up saves (Guardian Spirit), else the first one.

#include "TestHarness.h"

#include "combat/DeathPrevention.h"

#include <cstddef>
#include <vector>

namespace
{
    /// The choice among auras given by spell id, in the order the unit holds them, each used up as its row says.
    std::size_t ChoiceAmong(std::vector<uint32> const& spellIds)
    {
        std::vector<bool> usedUp;
        for (uint32 spellId : spellIds)
        {
            DeathPreventionRow const* row = DeathPreventionFor(spellId);
            usedUp.push_back(row && row->consumed);
        }
        return Combat::DeathPreventionChoice(usedUp);
    }

    const uint32 kCheatDeath = 31230;
    const uint32 kGuardianSpirit = 47788;
    const uint32 kCauterize = 86949;
}

TEST(DeathPrevention_OneAuraSaves)
{
    CHECK_EQ(ChoiceAmong({ kCauterize }), std::size_t(0));
}

TEST(DeathPrevention_GuardianSpiritSavesBeforeCauterizeWhicheverComesFirst)
{
    // Guardian Spirit's save uses it up; Cauterize's lockout stays free
    CHECK_EQ(ChoiceAmong({ kCauterize, kGuardianSpirit }), std::size_t(1));
    CHECK_EQ(ChoiceAmong({ kGuardianSpirit, kCauterize }), std::size_t(0));
}

TEST(DeathPrevention_GuardianSpiritSavesBeforeCheatDeath)
{
    CHECK_EQ(ChoiceAmong({ kCheatDeath, kGuardianSpirit }), std::size_t(1));
}

TEST(DeathPrevention_OfTwoGuardianSpiritsTheFirstSaves)
{
    // two priests' Guardian Spirits on one unit
    CHECK_EQ(ChoiceAmong({ kGuardianSpirit, kGuardianSpirit }), std::size_t(0));
}

TEST(DeathPrevention_OfTwoAurasThatAreNotUsedUpTheFirstSaves)
{
    CHECK_EQ(ChoiceAmong({ kCheatDeath, kCauterize }), std::size_t(0));
}

TEST(DeathPrevention_ABlowOfAtLeastTheHealthKills)
{
    CHECK(Combat::WouldKill(50000, 50000));
    CHECK(Combat::WouldKill(50001, 50000));
}

TEST(DeathPrevention_ABlowBelowTheHealthOrOfNothingDoesNotKill)
{
    CHECK(!Combat::WouldKill(49999, 50000));
    CHECK(!Combat::WouldKill(0, 1));
}

TEST(DeathPrevention_CheatDeathFromFullHealthKeepsTheDamageDownToTheFloor)
{
    // 50000 - 5000 = 45000 of the blow is taken; nothing is healed
    const Combat::DeathPreventionOutcome outcome = Combat::SaveFromKillingBlow(DEATH_PREVENTION_FLOOR, 50000, 50000, 10);
    CHECK_EQ(outcome.damageKept, int32(45000));
    CHECK_EQ(outcome.heal, int32(0));
}

TEST(DeathPrevention_CheatDeathBelowTheFloorKeepsTheHealthAsItIs)
{
    // 3000 is below the floor of 5000: no damage and no heal
    const Combat::DeathPreventionOutcome outcome = Combat::SaveFromKillingBlow(DEATH_PREVENTION_FLOOR, 3000, 50000, 10);
    CHECK_EQ(outcome.damageKept, int32(0));
    CHECK_EQ(outcome.heal, int32(0));
}

TEST(DeathPrevention_CauterizeFromFullHealthKeepsTheDamageDownToTheFloor)
{
    // 50000 - 20000 = 30000 of the blow is taken; nothing is healed
    const Combat::DeathPreventionOutcome outcome = Combat::SaveFromKillingBlow(DEATH_PREVENTION_FLOOR_AND_HEAL, 50000, 50000, 40);
    CHECK_EQ(outcome.damageKept, int32(30000));
    CHECK_EQ(outcome.heal, int32(0));
}

TEST(DeathPrevention_CauterizeFromOneHealthHealsUpToTheFloor)
{
    // 20000 - 1 = 19999 healed; no damage
    const Combat::DeathPreventionOutcome outcome = Combat::SaveFromKillingBlow(DEATH_PREVENTION_FLOOR_AND_HEAL, 1, 50000, 40);
    CHECK_EQ(outcome.damageKept, int32(0));
    CHECK_EQ(outcome.heal, int32(19999));
}

TEST(DeathPrevention_CauterizeAtTheFloorNeitherDamagesNorHeals)
{
    const Combat::DeathPreventionOutcome outcome = Combat::SaveFromKillingBlow(DEATH_PREVENTION_FLOOR_AND_HEAL, 20000, 50000, 40);
    CHECK_EQ(outcome.damageKept, int32(0));
    CHECK_EQ(outcome.heal, int32(0));
}

TEST(DeathPrevention_GuardianSpiritAbsorbsTheBlowAndHealsThePercent)
{
    // 50% of 50000 = 25000 healed; the whole blow absorbed
    const Combat::DeathPreventionOutcome outcome = Combat::SaveFromKillingBlow(DEATH_PREVENTION_HEAL_PERCENT, 1, 50000, 50);
    CHECK_EQ(outcome.damageKept, int32(0));
    CHECK_EQ(outcome.heal, int32(25000));
}

TEST(DeathPrevention_TheFloorIsTruncated)
{
    // 12345 x 10 / 100 = 1234.5, truncated to 1234; 12345 - 1234 = 11111
    const Combat::DeathPreventionOutcome outcome = Combat::SaveFromKillingBlow(DEATH_PREVENTION_FLOOR, 12345, 12345, 10);
    CHECK_EQ(outcome.damageKept, int32(11111));
    CHECK_EQ(outcome.heal, int32(0));
}
