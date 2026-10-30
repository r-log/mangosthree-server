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

/// The aura holder updates that wait for the end of the map phase. Inside a map's update a caster
/// on another map waits, and one on the updated map, or on a deck of a vessel sailing it, does not;
/// on a thread inside the phase that updates no map every caster waits; outside the phase none
/// does. The queue hands back what was posted, in order, once, and takes posts from several
/// threads at once without losing any. The maps are stand-ins: MapPhase only stores and compares a
/// map's address.

#include "TestHarness.h"
#include "MapPhase.h"

#include <thread>
#include <vector>

namespace
{
    int s_updatedMap = 0;
    int s_otherMap = 0;
    int s_deckMap = 0;

    Map const* Updated()
    {
        return reinterpret_cast<Map const*>(&s_updatedMap);
    }

    Map const* Other()
    {
        return reinterpret_cast<Map const*>(&s_otherMap);
    }

    Map const* Deck()
    {
        return reinterpret_cast<Map const*>(&s_deckMap);
    }

    /// Opens the map phase for one test and closes it on every way out of the test.
    class OpenMapPhase
    {
        public:
            OpenMapPhase() { MapPhase::Begin(); }
            ~OpenMapPhase() { MapPhase::End(); }

        private:
            OpenMapPhase(OpenMapPhase const&) = delete;
            OpenMapPhase& operator=(OpenMapPhase const&) = delete;
    };

    OffMapAuraUpdate Update(uint32 spellId, uint32 diff)
    {
        OffMapAuraUpdate const update = { 1, 0, UI64LIT(0xF13000B400000010), spellId, UI64LIT(0x0000000000F00000), diff };
        return update;
    }
}

TEST(OffMapAuraUpdates_NothingWaitsOutsideTheMapPhase)
{
    REQUIRE(!MapPhase::Active());

    CHECK(!OffMapAuraUpdates::Waits(Updated(), NULL));
    CHECK(!OffMapAuraUpdates::Waits(Other(), NULL));
    CHECK(!OffMapAuraUpdates::Waits(Deck(), Other()));
}

TEST(OffMapAuraUpdates_InsideAMapsUpdateOnlyACasterOnAnotherMapWaits)
{
    OpenMapPhase phase;
    MapPhase::Scope scope(Updated());

    CHECK(!OffMapAuraUpdates::Waits(Updated(), NULL));
    CHECK(OffMapAuraUpdates::Waits(Other(), NULL));
    CHECK(!OffMapAuraUpdates::Waits(Deck(), Updated()));
    CHECK(OffMapAuraUpdates::Waits(Deck(), Other()));
}

TEST(OffMapAuraUpdates_OnAThreadThatUpdatesNoMapEveryCasterWaits)
{
    OpenMapPhase phase;

    CHECK(OffMapAuraUpdates::Waits(Updated(), NULL));
    CHECK(OffMapAuraUpdates::Waits(Other(), NULL));
}

TEST(OffMapAuraUpdates_TakeHandsBackThePostsInOrderOnce)
{
    OffMapAuraUpdates::Take();

    OffMapAuraUpdates::Post(Update(589, 100));
    OffMapAuraUpdates::Post(Update(774, 200));

    std::vector<OffMapAuraUpdate> const taken = OffMapAuraUpdates::Take();
    REQUIRE(taken.size() == 2u);
    CHECK_EQ(taken[0].spellId, uint32(589));
    CHECK_EQ(taken[0].diff, uint32(100));
    CHECK_EQ(taken[1].spellId, uint32(774));
    CHECK_EQ(taken[1].diff, uint32(200));
    CHECK(taken[0].target == UI64LIT(0xF13000B400000010));
    CHECK(taken[0].caster == UI64LIT(0x0000000000F00000));
    CHECK(OffMapAuraUpdates::Take().empty());
}

TEST(OffMapAuraUpdates_PostsFromSeveralThreadsAllArriveInEachThreadsOrder)
{
    OffMapAuraUpdates::Take();

    uint32 const threads = 4, posts = 250;
    std::vector<std::thread> posters;
    for (uint32 t = 0; t < threads; ++t)
    {
        posters.push_back(std::thread([t, posts]()
                                      {
                                          for (uint32 i = 0; i < posts; ++i)
                                          {
                                              OffMapAuraUpdates::Post(Update(t, i));
                                          }
                                      }));
    }
    for (size_t t = 0; t < posters.size(); ++t)
    {
        posters[t].join();
    }

    std::vector<OffMapAuraUpdate> const taken = OffMapAuraUpdates::Take();
    REQUIRE(taken.size() == threads * posts);
    std::vector<uint32> next(threads, 0);
    for (size_t i = 0; i < taken.size(); ++i)
    {
        REQUIRE(taken[i].spellId < threads);
        CHECK_EQ(taken[i].diff, next[taken[i].spellId]);
        ++next[taken[i].spellId];
    }
}