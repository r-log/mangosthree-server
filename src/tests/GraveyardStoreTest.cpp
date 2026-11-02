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

/// The graveyards linked to each zone, added and looked up with no database: a zone's links in the
/// order added, a link found by graveyard, a team edited through the edit access, and a cleared store.

#include "TestHarness.h"
#include "data/GraveyardStore.h"

namespace
{
    GraveYardData MakeLink(uint32 safeLocId, Team team)
    {
        GraveYardData data;
        data.safeLocId = safeLocId;
        data.team = team;
        return data;
    }

    size_t Count(GraveYardMapBounds bounds)
    {
        size_t n = 0;
        for (; bounds.first != bounds.second; ++bounds.first)
        {
            ++n;
        }
        return n;
    }
}

TEST(GraveyardStore_AZoneHoldsItsLinksInTheOrderAdded)
{
    GraveyardStore store;
    CHECK_EQ(Count(store.Zone(12)), size_t(0));

    store.Add(12, MakeLink(4, ALLIANCE));
    store.Add(14, MakeLink(10, HORDE));
    store.Add(12, MakeLink(7, TEAM_NONE));

    GraveYardMapBounds zone = store.Zone(12);
    REQUIRE(zone.first != zone.second);
    CHECK_EQ(zone.first->second.safeLocId, uint32(4));
    ++zone.first;
    REQUIRE(zone.first != zone.second);
    CHECK_EQ(zone.first->second.safeLocId, uint32(7));
    CHECK_EQ(Count(store.Zone(14)), size_t(1));
}

TEST(GraveyardStore_ALinkIsFoundByGraveyardInItsZone)
{
    GraveyardStore store;
    store.Add(12, MakeLink(4, ALLIANCE));
    store.Add(14, MakeLink(10, HORDE));

    GraveYardData const* link = store.Find(4, 12);
    REQUIRE(link != NULL);
    CHECK(link->team == ALLIANCE);
    CHECK(store.Find(4, 14) == NULL);
    CHECK(store.Find(10, 12) == NULL);
    CHECK(store.Find(10, 14) != NULL);
}

TEST(GraveyardStore_ALinksTeamIsEditedInPlace)
{
    GraveyardStore store;
    store.Add(12, MakeLink(4, TEAM_NONE));
    CHECK(store.FindForEdit(5, 12) == NULL);

    GraveYardData* link = store.FindForEdit(4, 12);
    REQUIRE(link != NULL);
    link->team = HORDE;
    CHECK(store.Find(4, 12)->team == HORDE);
}

TEST(GraveyardStore_ClearingForgetsEveryZone)
{
    GraveyardStore store;
    store.Add(12, MakeLink(4, ALLIANCE));
    store.Add(14, MakeLink(10, HORDE));

    store.Clear();
    CHECK_EQ(Count(store.Zone(12)), size_t(0));
    CHECK(store.Find(10, 14) == NULL);
}
