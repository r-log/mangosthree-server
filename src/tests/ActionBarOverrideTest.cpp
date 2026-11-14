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

/// Which action-bar override buffs a cast uses up (spells/ActionBarOverride.h), over the 4.3.4 client's aura-332 and
/// aura-333 rows: each row's spell, override spell (the effect's amount) and the holder's duration as SpellEffect.dbc
/// and SpellDuration.dbc hold them. Hot Streak 48108 (332, Pyroblast! 92315, 15000 ms), Demon Armor 687 (332, 6112,
/// permanent), Chakra: Serenity 81208 (332, 88684, permanent), Frostfire Orb 84726 (332, 92283, a passive with no
/// duration), Frostfire Orb Override 92294 (332, 92283, 12000 ms), Stampede 81022 and 81021 (332 and 333, Ravage!
/// 81170, 10000 ms), Tiger's Fury 108566 (332, 81170, 10000 ms), Surge of Light 88688 (333, Flash Heal 101062,
/// 10000 ms), Dark Simulacrum 77616 (333, amount 0, 20000 ms), Ron's Test Buff 4 66109 (332 Blizzard 10, 333 Starfire
/// 2912, 600000 ms).

#include "TestHarness.h"

#include "spells/ActionBarOverride.h"
#include "spells/data/SpellAuraDefines.h"

#include <vector>

namespace
{
    Spells::OverrideAuraView View(uint32 auraType, uint32 spellId, int32 amount, int32 maxDuration)
    {
        Spells::OverrideAuraView view = { auraType, spellId, amount, maxDuration };
        return view;
    }

    uint32 const OVERRIDE = SPELL_AURA_OVERRIDE_ACTIONBAR_SPELLS;
    uint32 const OVERRIDE_2 = SPELL_AURA_OVERRIDE_ACTIONBAR_SPELLS_2;
}

TEST(ActionBarOverride_PyroblastBangUsesUpHotStreak)
{
    std::vector<Spells::OverrideAuraView> const auras = { View(OVERRIDE, 48108, 92315, 15000) };
    std::vector<uint32> const usedUp = Spells::UsedUpOverrides(auras, 92315);
    CHECK_EQ(usedUp.size(), std::size_t(1));
    CHECK_EQ(usedUp[0], uint32(48108));

    // Another spell, the hard-cast Pyroblast among them, leaves Hot Streak up.
    CHECK(Spells::UsedUpOverrides(auras, 11366).empty());
    CHECK(Spells::UsedUpOverrides(auras, 133).empty());
}

TEST(ActionBarOverride_AnOverrideWithoutADurationIsNeverUsedUp)
{
    // Demon Armor and the Chakras are permanent (-1); the Frostfire Orb talent is a passive with no duration (0).
    CHECK(Spells::UsedUpOverrides({ View(OVERRIDE, 687, 6112, -1) }, 6112).empty());
    CHECK(Spells::UsedUpOverrides({ View(OVERRIDE, 81208, 88684, -1) }, 88684).empty());
    CHECK(Spells::UsedUpOverrides({ View(OVERRIDE, 84726, 92283, 0) }, 92283).empty());

    // Beside the passive, the timed Frostfire Orb Override naming the same spell is used up alone.
    std::vector<uint32> const usedUp =
        Spells::UsedUpOverrides({ View(OVERRIDE, 84726, 92283, 0), View(OVERRIDE, 92294, 92283, 12000) }, 92283);
    CHECK_EQ(usedUp.size(), std::size_t(1));
    CHECK_EQ(usedUp[0], uint32(92294));
}

TEST(ActionBarOverride_Aura333IsUsedUpAsAura332Is)
{
    std::vector<uint32> const surge = Spells::UsedUpOverrides({ View(OVERRIDE_2, 88688, 101062, 10000) }, 101062);
    CHECK_EQ(surge.size(), std::size_t(1));
    CHECK_EQ(surge[0], uint32(88688));

    // Ravage! uses up every timed buff that names it, of either type, in the order the auras are walked.
    std::vector<Spells::OverrideAuraView> const ravage =
    {
        View(OVERRIDE, 81022, 81170, 10000), View(OVERRIDE, 108566, 81170, 10000), View(OVERRIDE_2, 81021, 81170, 10000),
    };
    std::vector<uint32> const usedUp = Spells::UsedUpOverrides(ravage, 81170);
    CHECK_EQ(usedUp.size(), std::size_t(3));
    CHECK_EQ(usedUp[0], uint32(81022));
    CHECK_EQ(usedUp[1], uint32(108566));
    CHECK_EQ(usedUp[2], uint32(81021));
}

TEST(ActionBarOverride_AnAmountOf0MatchesNoSpell)
{
    CHECK(Spells::UsedUpOverrides({ View(OVERRIDE_2, 77616, 0, 20000) }, 0).empty());
    CHECK(Spells::UsedUpOverrides({ View(OVERRIDE_2, 77616, 0, 20000) }, 49576).empty());
}

TEST(ActionBarOverride_OnlyTheTwoOverrideTypesCount)
{
    // An aura of another type whose amount happens to be the spell cast is no override.
    CHECK(Spells::UsedUpOverrides({ View(SPELL_AURA_DUMMY, 44445, 92315, 15000) }, 92315).empty());
    CHECK(Spells::UsedUpOverrides({ View(SPELL_AURA_ADD_PCT_MODIFIER, 57761, 133, 15000) }, 133).empty());
}

TEST(ActionBarOverride_AHolderIsUsedUpOnceWhateverItsAuras)
{
    // Ron's Test Buff 4 names Blizzard and Shadow Bolt at 332 and Starfire at 333: each of the three uses it up. A
    // second holder of the spell (another caster's) matches too; the spell is named once, and its removal takes every
    // holder of it.
    std::vector<Spells::OverrideAuraView> const auras =
    {
        View(OVERRIDE, 66109, 10, 600000), View(OVERRIDE_2, 66109, 2912, 600000), View(OVERRIDE, 66109, 686, 600000),
        View(OVERRIDE, 66109, 10, 600000),
    };
    for (uint32 cast : { 10u, 2912u, 686u })
    {
        std::vector<uint32> const usedUp = Spells::UsedUpOverrides(auras, cast);
        CHECK_EQ(usedUp.size(), std::size_t(1));
        CHECK_EQ(usedUp[0], uint32(66109));
    }
    CHECK(Spells::UsedUpOverrides(auras, 133).empty());
}
