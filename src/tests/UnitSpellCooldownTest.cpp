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

/// Unit holds the spell cooldown manager, default-constructed; a Creature's own cooldown maps
/// (CreatureSpellCooldown.cpp) stand beside it and neither reads nor writes the other.
///
/// The probe is a bare Creature (no map, no AI) that makes Unit's protected manager readable.
/// Spells 94701 and 94702 have no spell store row in this binary, so the Creature's category
/// check finds no category and answers from its spell map alone.

#include "TestHarness.h"
#include "Creature.h"
#include "Unit.h"
#include "spells/SpellCooldownMgr.h"

#include <ctime>
#include <type_traits>

namespace
{
    class CooldownProbe : public Creature
    {
        public:
            CooldownProbe() : Creature(CREATURE_SUBTYPE_GENERIC) { }
            using Unit::m_spellCooldownMgr;
    };

    static_assert(std::is_same<decltype(&CooldownProbe::m_spellCooldownMgr), SpellCooldownMgr Unit::*>::value,
                  "the spell cooldown manager is a member of Unit");

    const uint32 kUnitSpell = 94701;
    const uint32 kCreatureSpell = 94702;
}

TEST(UnitSpellCooldown_AUnitHoldsADefaultConstructedManager)
{
    CooldownProbe probe;
    time_t const now = time(NULL);

    CHECK(probe.m_spellCooldownMgr.GetSpellCooldownMap().empty());
    CHECK(!probe.m_spellCooldownMgr.HasSpellCooldown(kUnitSpell, now));
    CHECK_EQ(probe.m_spellCooldownMgr.GetSpellCooldownDelay(kUnitSpell, now), time_t(0));
}

TEST(UnitSpellCooldown_ACreaturesOwnCooldownsAreUntouched)
{
    CooldownProbe probe;
    time_t const now = time(NULL);
    time_t const end = now + 3600;

    probe.m_spellCooldownMgr.AddSpellCooldown(kUnitSpell, 0, end);
    CHECK(!probe.HasSpellCooldown(kUnitSpell));
    CHECK_EQ(probe.GetCreatureSpellCooldownDelay(kUnitSpell), uint32(0));

    probe._AddCreatureSpellCooldown(kCreatureSpell, end);
    CHECK(probe.HasSpellCooldown(kCreatureSpell));
    CHECK(probe.GetCreatureSpellCooldownDelay(kCreatureSpell) > uint32(3500));

    CHECK_EQ(probe.m_spellCooldownMgr.GetSpellCooldownMap().size(), size_t(1));
    CHECK(probe.m_spellCooldownMgr.HasSpellCooldown(kUnitSpell, now));
    CHECK(!probe.m_spellCooldownMgr.HasSpellCooldown(kCreatureSpell, now));
}
