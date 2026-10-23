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
#include "MapPhase.h"

#include <thread>

namespace
{
    /// Three objects whose addresses stand for three maps: MapPhase only stores and compares a map's address, and
    /// never reads through it.
    int s_sailedMap = 0;
    int s_deckMap = 0;
    int s_thirdMap = 0;

    /// The world map a vessel sails.
    Map const* Sailed()
    {
        return reinterpret_cast<Map const*>(&s_sailedMap);
    }

    /// That vessel's deck.
    Map const* Deck()
    {
        return reinterpret_cast<Map const*>(&s_deckMap);
    }

    /// Any other map: another world map, an instance, or the map the vessel sails after it crossed.
    Map const* Third()
    {
        return reinterpret_cast<Map const*>(&s_thirdMap);
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

    /// What `question` answers on a thread of its own, which opens a scope over `scoped` first (none when NULL).
    template <typename Question>
    bool AnswerOnAnotherThread(Map const* scoped, Question question)
    {
        bool answer = false;
        std::thread other([&]()
                          {
                              if (scoped)
                              {
                                  MapPhase::Scope scope(scoped);
                                  answer = question();
                              }
                              else
                              {
                                  answer = question();
                              }
                          });
        other.join();
        return answer;
    }
}

TEST(MapPhase_WithThePhaseClosedEveryThreadOwnsEveryMapAndEveryUnit)
{
    REQUIRE(!MapPhase::Active());

    CHECK(MapPhase::Owns(Sailed()));
    CHECK(MapPhase::Owns(Third()));
    CHECK(MapPhase::OwnsUnitOn(Deck(), Sailed()));
    CHECK(MapPhase::OwnsUnitOn(Third(), NULL));
    CHECK(AnswerOnAnotherThread(NULL, []() { return MapPhase::Owns(Third()) && MapPhase::OwnsUnitOn(Deck(), Sailed()); }));
}

/// A deck's update runs nested inside the update of the map its vessel sails, on that map's thread:
/// TransportMap::Update opens the deck's scope, and the Map::Update it runs opens a second one.
TEST(MapPhase_ADecksNestedScopeOwnsTheDeckAndTheMapItInterruptedButNotAThird)
{
    OpenMapPhase phase;
    MapPhase::Scope sailed(Sailed());
    CHECK(MapPhase::Owns(Sailed()));
    CHECK(!MapPhase::Owns(Deck()));
    CHECK(!MapPhase::Owns(Third()));

    {
        MapPhase::Scope deck(Deck());
        MapPhase::Scope deckUpdate(Deck());
        CHECK(MapPhase::Owns(Deck()));
        CHECK(MapPhase::Owns(Sailed()));
        CHECK(!MapPhase::Owns(Third()));
    }
}

TEST(MapPhase_AScopesEndHandsTheThreadBackToTheMapItInterrupted)
{
    OpenMapPhase phase;
    {
        MapPhase::Scope sailed(Sailed());
        {
            MapPhase::Scope deck(Deck());
            CHECK(MapPhase::Owns(Deck()));
        }
        CHECK(MapPhase::Owns(Sailed()));
        CHECK(!MapPhase::Owns(Deck()));
    }
    CHECK(!MapPhase::Owns(Sailed()));
    CHECK(!MapPhase::Owns(Deck()));
}

/// With the worker pool on, another world map is updated on another worker at the same time.
TEST(MapPhase_AThreadOwnsOnlyTheMapsOfTheScopesOpenOnIt)
{
    OpenMapPhase phase;
    MapPhase::Scope sailed(Sailed());
    MapPhase::Scope deck(Deck());

    CHECK(!AnswerOnAnotherThread(NULL, []() { return MapPhase::Owns(Sailed()) || MapPhase::Owns(Deck()); }));
    CHECK(!AnswerOnAnotherThread(Third(), []() { return MapPhase::Owns(Sailed()) || MapPhase::Owns(Deck()); }));
    CHECK(AnswerOnAnotherThread(Third(), []() { return MapPhase::Owns(Third()); }));
    CHECK(!MapPhase::Owns(Third()));
}

TEST(MapPhase_AUnitOnADeckIsOwnedByTheUpdateOfTheMapItsVesselSails)
{
    OpenMapPhase phase;
    {
        // The sailed map's own update, before its vessels tick, and the deck's update nested in it.
        MapPhase::Scope sailed(Sailed());
        CHECK(MapPhase::OwnsUnitOn(Deck(), Sailed()));
        CHECK(MapPhase::OwnsUnitOn(Sailed(), NULL));

        MapPhase::Scope deck(Deck());
        CHECK(MapPhase::OwnsUnitOn(Deck(), Sailed()));
        CHECK(MapPhase::OwnsUnitOn(Sailed(), NULL));
    }
    {
        // A third map's update owns neither of them.
        MapPhase::Scope third(Third());
        CHECK(!MapPhase::OwnsUnitOn(Deck(), Sailed()));
        CHECK(!MapPhase::OwnsUnitOn(Sailed(), NULL));
    }

    // A thread that updates no map, and a thread that updates a third map, own neither of them.
    CHECK(!MapPhase::OwnsUnitOn(Deck(), Sailed()));
    CHECK(!AnswerOnAnotherThread(NULL, []() { return MapPhase::OwnsUnitOn(Deck(), Sailed()); }));
    CHECK(!AnswerOnAnotherThread(Third(), []() { return MapPhase::OwnsUnitOn(Deck(), Sailed()); }));
}

TEST(MapPhase_AUnitOnAThirdMapStaysUnowned)
{
    OpenMapPhase phase;
    {
        // From the sailed map's update and from the deck's.
        MapPhase::Scope sailed(Sailed());
        CHECK(!MapPhase::OwnsUnitOn(Third(), NULL));

        MapPhase::Scope deck(Deck());
        CHECK(!MapPhase::OwnsUnitOn(Third(), NULL));
    }
    {
        // The vessel crossed and sails the third map, whose update runs the deck nested inside it: a unit on the map
        // it left is not owned from the deck's update.
        MapPhase::Scope sails(Third());
        MapPhase::Scope deck(Deck());
        CHECK(MapPhase::OwnsUnitOn(Deck(), Third()));
        CHECK(!MapPhase::OwnsUnitOn(Sailed(), NULL));
    }
    {
        // Nor is a unit on the deck owned from the update of the map the vessel left.
        MapPhase::Scope left(Sailed());
        CHECK(!MapPhase::OwnsUnitOn(Deck(), Third()));
    }
}
