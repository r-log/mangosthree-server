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

/// The item Unit's proc handlers ask for by guid: the item an aura was cast from.
///
/// Unit answers through the item callbacks a player installs for itself, and NULL for a unit that
/// holds none: a bare Creature (no map, no AI, no auras) is asked through a Unit reference. A player on
/// a socketless session, in no world, answers through its own callbacks, from its empty inventory. The
/// static_asserts pin Unit's declaration and that Player declares none of its own.

#include "TestHarness.h"
#include "Auth/BigNumber.h"
#include "Creature.h"
#include "Player.h"
#include "Unit.h"
#include "WorldSession.h"

#include <type_traits>

namespace
{
    /// A player with its update fields allocated, on a session with no socket, in no world.
    class StandingPlayer : public Player
    {
        public:
            explicit StandingPlayer(WorldSession& session) : Player(&session)
            {
                Object::_Create(1, 0, HIGHGUID_PLAYER);
            }

            ~StandingPlayer()
            {
                delete[] m_uint32Values;
                m_uint32Values = NULL;
            }
    };
}

static_assert(std::is_same<decltype(&Unit::GetItemByGuid), Item* (Unit::*)(ObjectGuid) const>::value,
              "Unit declares the item lookup by guid");
static_assert(std::is_same<decltype(&Player::GetItemByGuid), Item* (Unit::*)(ObjectGuid) const>::value,
              "Player declares no item lookup of its own: its member pointer is Unit's");

TEST(UnitItemByGuid_ACreatureHoldsNoItemUnderAnyGuid)
{
    Creature creature(CREATURE_SUBTYPE_GENERIC);
    Unit const& unit = creature;

    CHECK(unit.GetItemByGuid(ObjectGuid()) == NULL);
    CHECK(unit.GetItemByGuid(ObjectGuid(HIGHGUID_ITEM, uint32(1))) == NULL);
    CHECK(unit.GetItemByGuid(ObjectGuid(HIGHGUID_ITEM, uint32(0xFFFFFFFF))) == NULL);
    CHECK(unit.GetItemByGuid(ObjectGuid(HIGHGUID_PLAYER, uint32(7))) == NULL);
}

TEST(UnitItemByGuid_APlayerAnswersFromItsInventory)
{
    WorldSession session(1, "items", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    StandingPlayer player(session);
    Unit const& unit = player;

    CHECK(unit.GetItemByGuid(ObjectGuid(HIGHGUID_ITEM, uint32(1))) == NULL);
    CHECK(unit.GetItemByGuid(ObjectGuid(HIGHGUID_ITEM, uint32(0xFFFFFFFF))) == NULL);
}
