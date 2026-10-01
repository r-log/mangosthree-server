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
/// A Unit that is not a Player holds no item: a bare Creature (no map, no AI, no auras) is asked
/// through a Unit reference. A Player cannot be built in this binary (it needs a WorldSession and
/// a map). Player's override is private: the static_asserts pin Unit's declaration, that a call
/// through a Unit reaches the lookup and that a call through a Player does not compile; Player.h
/// marks the override `override`, so it fails to build unless it matches Unit's declaration.

#include "TestHarness.h"
#include "Creature.h"
#include "Player.h"
#include "Unit.h"

#include <type_traits>
#include <utility>

namespace
{
    template<class T>
    auto CallsItemByGuid(int) -> decltype(std::declval<T const&>().GetItemByGuid(ObjectGuid()), std::true_type());

    template<class T>
    std::false_type CallsItemByGuid(...);
}

static_assert(std::is_same<decltype(&Unit::GetItemByGuid), Item* (Unit::*)(ObjectGuid) const>::value,
              "Unit declares the item lookup by guid");
static_assert(decltype(CallsItemByGuid<Unit>(0))::value, "a call through a Unit reaches the item lookup");
static_assert(!decltype(CallsItemByGuid<Player>(0))::value,
              "Player's item lookup is private: a call through a Player does not compile");

TEST(UnitItemByGuid_ACreatureHoldsNoItemUnderAnyGuid)
{
    Creature creature(CREATURE_SUBTYPE_GENERIC);
    Unit const& unit = creature;

    CHECK(unit.GetItemByGuid(ObjectGuid()) == NULL);
    CHECK(unit.GetItemByGuid(ObjectGuid(HIGHGUID_ITEM, uint32(1))) == NULL);
    CHECK(unit.GetItemByGuid(ObjectGuid(HIGHGUID_ITEM, uint32(0xFFFFFFFF))) == NULL);
    CHECK(unit.GetItemByGuid(ObjectGuid(HIGHGUID_PLAYER, uint32(7))) == NULL);
}
