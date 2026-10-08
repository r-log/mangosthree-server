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

/// The Eastern Plaguelands script's two tower counts: the continent's map thread changes them and an
/// instance's map thread reads them when a player enters an affected zone. The static_asserts pin
/// that each count is a std::atomic<uint8> (through an explicit instantiation, since both are private)
/// and that the type is lock-free. The test fills the initial world states from counts stored through
/// the same idiom.

#include "TestHarness.h"
#include "ObjectMgr.h"
#include "OutdoorPvPEP.h"
#include "WorldPacket.h"

#include <atomic>
#include <type_traits>

namespace
{
    /// Checks the type of the Alliance tower count it is instantiated with; the count is named only in
    /// the explicit instantiation below.
    template<class Member, Member member>
    struct PlaguelandsAllianceTowerCountDeclaration
    {
        static_assert(std::is_same<Member, std::atomic<uint8> OutdoorPvPEP::*>::value,
                      "OutdoorPvPEP::m_towersAlliance is a std::atomic<uint8>");
    };

    /// Checks the type of the Horde tower count it is instantiated with; the count is named only in
    /// the explicit instantiation below.
    template<class Member, Member member>
    struct PlaguelandsHordeTowerCountDeclaration
    {
        static_assert(std::is_same<Member, std::atomic<uint8> OutdoorPvPEP::*>::value,
                      "OutdoorPvPEP::m_towersHorde is a std::atomic<uint8>");
    };

    template struct PlaguelandsAllianceTowerCountDeclaration<decltype(&OutdoorPvPEP::m_towersAlliance),
                                                             &OutdoorPvPEP::m_towersAlliance>;
    template struct PlaguelandsHordeTowerCountDeclaration<decltype(&OutdoorPvPEP::m_towersHorde),
                                                          &OutdoorPvPEP::m_towersHorde>;

    static_assert(std::atomic<uint8>::is_always_lock_free,
                  "a tower count must be lock-free: it is read and written on different map threads");

    /// Hands out a pointer to a private member: an explicit instantiation may name it, and the
    /// friend the instantiation defines returns it.
    template<class Tag, typename Tag::type Member>
    struct PlaguelandsTowerCountAccess
    {
        friend typename Tag::type MemberOf(Tag)
        {
            return Member;
        }
    };

    struct AllianceTowerCount
    {
        typedef std::atomic<uint8> OutdoorPvPEP::* type;
        friend type MemberOf(AllianceTowerCount);
    };

    struct HordeTowerCount
    {
        typedef std::atomic<uint8> OutdoorPvPEP::* type;
        friend type MemberOf(HordeTowerCount);
    };

    template struct PlaguelandsTowerCountAccess<AllianceTowerCount, &OutdoorPvPEP::m_towersAlliance>;
    template struct PlaguelandsTowerCountAccess<HordeTowerCount, &OutdoorPvPEP::m_towersHorde>;
}

TEST(PlaguelandsTowerCount_InitialWorldStatesCarryTheCounts)
{
    // The constructor links the zone's graveyard to no team; with no graveyard loaded it finds no link
    // and adds none.
    CHECK(sObjectMgr.FindGraveYardData(GRAVEYARD_ID_EASTERN_PLAGUE, GRAVEYARD_ZONE_EASTERN_PLAGUE) == NULL);
    OutdoorPvPEP script;
    CHECK(sObjectMgr.FindGraveYardData(GRAVEYARD_ID_EASTERN_PLAGUE, GRAVEYARD_ZONE_EASTERN_PLAGUE) == NULL);

    (script.*MemberOf(AllianceTowerCount())).store(2);
    (script.*MemberOf(HordeTowerCount())).store(3);

    WorldPacket data;
    uint32 count = 0;
    script.FillInitialWorldStates(data, count);

    // The two counts, then the four towers' states, each a 4-byte state and a 4-byte value.
    CHECK_EQ(count, uint32(6));
    CHECK_EQ(data.size(), size_t(48));
    CHECK_EQ(data.read<uint32>(0), uint32(WORLD_STATE_EP_TOWER_COUNT_ALLIANCE));
    CHECK_EQ(data.read<uint32>(4), uint32(2));
    CHECK_EQ(data.read<uint32>(8), uint32(WORLD_STATE_EP_TOWER_COUNT_HORDE));
    CHECK_EQ(data.read<uint32>(12), uint32(3));
}
