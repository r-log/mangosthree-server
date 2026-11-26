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

/// Tests of priest/holy/PrayerOfMending.cpp: the one row the file registers, in the game's table too, and no other
/// spell's; the amount of the heal, raised by the glyph's percent for the first heal of a cast alone; the charges a
/// jump leaves, and the five heals a cast gives; which units of the holder's group may take the aura; and the units
/// the walk finds none for, a bare creature, which is neither a player nor a pet and holds no glyph. The jump itself
/// needs a map, a group and a priest in the world: scenario 1231 records it, with a pet that holds and receives the
/// aura and the glyph's first heal.

#include "TestHarness.h"
#include "spells/classes/priest/holy/PrayerOfMending.h"
#include "spells/SpellScriptHooks.h"
#include "spells/handlers/SpellHandlerRegistry.h"
#include "Creature.h"

#include <cstddef>
#include <cstdint>

namespace
{
    uint32 const PRAYER_OF_MENDING = 33076;
    uint32 const MENDING_AURA = 41635;
    uint32 const MENDING_HEAL = 33110;
    uint32 const MENDING_JUMP_VISUAL = 41637;
    uint32 const GLYPH_OF_PRAYER_OF_MENDING = 55685;
    uint32 const CHAKRA = 14751;

    // The cast's whole count of charges, in the client's data.
    uint32 const CHARGES = 5;
    // The glyph's effect 0 percent, in the client's data.
    int32 const GLYPH = 60;

    // A creature with its update fields allocated and no world, map, template or AI.
    class BareCreature : public Creature
    {
        public:
            BareCreature() : Creature(CREATURE_SUBTYPE_GENERIC)
            {
                _InitValues();
            }

            ~BareCreature()
            {
                delete[] m_uint32Values;
                m_uint32Values = NULL;
            }
    };
}

TEST(PriestPrayerOfMending_TheFileHoldsOneOnProcRowOnTheAuraInTheGameToo)
{
    // The proc of aura type 225, which only 41635 uses: one row, one key, no default.
    SpellHandlerRegistry registry;
    CHECK_EQ(Priest::RegisterPrayerOfMending(registry), uint32(1));
    CHECK_EQ(registry.Count(), std::size_t(1));
    CHECK_EQ(registry.CountDefaults(), std::size_t(0));
    CHECK_EQ(registry.CountAt(OnProcHook::Key), std::size_t(1));
    CHECK(registry.Find<OnProcHook>(MENDING_AURA) != NULL);
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    CHECK(game.Find<OnProcHook>(MENDING_AURA) == registry.Find<OnProcHook>(MENDING_AURA));

    // The cast, the heal, the jump's visual, the glyph and Chakra hold no row of the file: the heal keeps its id and
    // reaches the priest's procs as before, and Chakra's refusal of it stays Chakra's.
    for (uint32 spellId : { PRAYER_OF_MENDING, MENDING_HEAL, MENDING_JUMP_VISUAL, GLYPH_OF_PRAYER_OF_MENDING, CHAKRA })
    {
        CHECK(registry.Find<OnProcHook>(spellId) == NULL);
        CHECK(registry.Find<CheckProcHook>(spellId) == NULL);
        CHECK(registry.Find<DummyEffectHook>(spellId) == NULL);
    }
    CHECK(registry.Find<CheckProcHook>(MENDING_AURA) == NULL);
}

TEST(PriestPrayerOfMending_OnlyTheFirstHealOfACastTakesTheGlyphsPercent)
{
    // The first heal, the holder's charges all of the cast's, takes the glyph's percent of the amount: 60 percent of
    // 5000 is 3000, of 7319 is 4391 (4391.4 cut), of 1 is 0.
    CHECK_EQ(Priest::MendingHealAmount(5000, 0, GLYPH, CHARGES, CHARGES), int32(8000));
    CHECK_EQ(Priest::MendingHealAmount(7319, 0, GLYPH, CHARGES, CHARGES), int32(11710));
    CHECK_EQ(Priest::MendingHealAmount(1, 0, GLYPH, CHARGES, CHARGES), int32(1));

    // The four heals after it, charges 4, 3, 2 and 1, take the amount alone: the heals of the chain are equal.
    for (uint32 charges : { uint32(4), uint32(3), uint32(2), uint32(1) })
    {
        CHECK_EQ(Priest::MendingHealAmount(5000, 0, GLYPH, charges, CHARGES), int32(5000));
    }

    // No glyph, or an amount of the glyph that is not above 0: the amount alone, the first heal's too.
    CHECK_EQ(Priest::MendingHealAmount(5000, 0, 0, CHARGES, CHARGES), int32(5000));
    CHECK_EQ(Priest::MendingHealAmount(5000, 0, -60, CHARGES, CHARGES), int32(5000));

    // A cast whose whole count the data does not hold has no first heal to tell: the amount alone.
    CHECK_EQ(Priest::MendingHealAmount(5000, 0, GLYPH, 0, 0), int32(5000));
    CHECK_EQ(Priest::MendingHealAmount(5000, 0, GLYPH, CHARGES, 0), int32(5000));

    // The glyph raises the whole heal: with 500 the priest's spell power adds after the amount, 60 percent of 5500 is
    // 3300 more, so the first heal (8300 + 500) is 1.6 times a later one (5000 + 500); a later heal takes no part of it.
    CHECK_EQ(Priest::MendingHealAmount(5000, 500, GLYPH, CHARGES, CHARGES), int32(8300));
    CHECK_EQ(Priest::MendingHealAmount(5000, 500, GLYPH, 4, CHARGES), int32(5000));

    // A cast that starts with 6 charges (the two pieces of Regalia of Faith add one) has its first heal at 6 of 6; the
    // heal at 5 of 6 is a later one.
    CHECK_EQ(Priest::MendingHealAmount(5000, 0, GLYPH, CHARGES + 1, CHARGES + 1), int32(8000));
    CHECK_EQ(Priest::MendingHealAmount(5000, 0, GLYPH, CHARGES, CHARGES + 1), int32(5000));

    // The percent is the glyph's amount: 100 doubles the amount, and the heal never wraps its 32 bits.
    CHECK_EQ(Priest::MendingHealAmount(5000, 0, 100, CHARGES, CHARGES), int32(10000));
    CHECK_EQ(Priest::MendingHealAmount(INT32_MAX, 0, GLYPH, CHARGES, CHARGES), int32(INT32_MAX));
}

TEST(PriestPrayerOfMending_ACastGivesFiveHealsAndAJumpLeavesOneChargeFewer)
{
    // One charge fewer for the next holder; the last charge, and none, jump nowhere.
    CHECK_EQ(Priest::MendingJumpCharges(5), uint32(4));
    CHECK_EQ(Priest::MendingJumpCharges(4), uint32(3));
    CHECK_EQ(Priest::MendingJumpCharges(2), uint32(1));
    CHECK_EQ(Priest::MendingJumpCharges(1), uint32(0));
    CHECK_EQ(Priest::MendingJumpCharges(0), uint32(0));

    // From the whole count a holder is healed, the aura jumps with one charge fewer, and so on to the last charge:
    // charges 5, 4, 3, 2, 1 are five heals, the retail 4.3.4 sniff's chain (build 15595, 2012-05-28).
    uint32 heals = 0;
    uint32 seen[8] = { 0 };
    for (uint32 charges = CHARGES; charges; charges = Priest::MendingJumpCharges(charges))
    {
        seen[heals++] = charges;
    }
    CHECK_EQ(heals, uint32(5));
    for (uint32 i = 0; i < 5; ++i)
    {
        CHECK_EQ(seen[i], CHARGES - i);
    }

    // Only the first of the five is the cast's whole count: the glyph raises one heal in five.
    uint32 raised = 0;
    for (uint32 i = 0; i < 5; ++i)
    {
        raised += Priest::MendingHealAmount(1000, 0, GLYPH, seen[i], CHARGES) != 1000 ? 1 : 0;
    }
    CHECK_EQ(raised, uint32(1));
}

TEST(PriestPrayerOfMending_AUnitMayTakeTheAuraWhenItIsAnotherAliveWithinReachVisibleAndNotHostile)
{
    // A pet or a member of the group, alive, in reach, visible and not hostile to the holder.
    CHECK(OnProcContext::GroupUnitMayTake(false, true, true, false, false));

    // Each of the five conditions missing alone: the holder itself, a dead unit, one out of reach (the priest 30 yards
    // from a friend and his pet is out of a 20 yard radius), an invisible one, a hostile one.
    CHECK(!OnProcContext::GroupUnitMayTake(true, true, true, false, false));
    CHECK(!OnProcContext::GroupUnitMayTake(false, false, true, false, false));
    CHECK(!OnProcContext::GroupUnitMayTake(false, true, false, false, false));
    CHECK(!OnProcContext::GroupUnitMayTake(false, true, true, true, false));
    CHECK(!OnProcContext::GroupUnitMayTake(false, true, true, false, true));

    // Every one missing at once.
    CHECK(!OnProcContext::GroupUnitMayTake(true, false, false, true, true));
}

TEST(PriestPrayerOfMending_ABareCreatureHoldsNoGlyph)
{
    BareCreature creature;

    // No glyph aura on a unit that holds none.
    CHECK_EQ(Priest::MendingGlyph(creature), int32(0));
}
