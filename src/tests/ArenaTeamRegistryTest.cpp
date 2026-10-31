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

/// The arena team registry: a listed team is found by its id, its name and its captain until it is
/// removed, every listed team is in All, and the registry a guard installs is the one
/// sArenaTeamRegistry answers while the guard lives. The teams are loaded from rows, as the
/// start-up load builds them.

#include "TestHarness.h"
#include "InstalledArenaTeamRegistry.h"
#include "ArenaTeam.h"
#include "ObjectGuid.h"
#include "persistence/characters/social/ArenaTeamRepository.h"

#include <string>

namespace
{
    /// A team loaded from the row of id `id`; the registry the test adds it to deletes it.
    ArenaTeam* LoadedTeam(uint32 id, std::string const& name, uint32 captainGuidLow)
    {
        ArenaTeamRow row;
        row.id = id;
        row.name = name;
        row.captainGuid = captainGuidLow;
        row.type = 2;
        ArenaTeam* team = new ArenaTeam;
        CHECK(team->LoadArenaTeamFromDB(row));
        return team;
    }
}

TEST(ArenaTeamRegistry_AListedTeamIsFoundByItsIdNameAndCaptainUntilItIsRemoved)
{
    InstalledArenaTeamRegistry teams;
    ArenaTeamRegistry& registry = teams.registry;

    ArenaTeam* first = LoadedTeam(7, "Aldors", 5101);
    ArenaTeam* second = LoadedTeam(8, "Scryers", 5102);
    registry.Add(first);
    registry.Add(second);

    CHECK(registry.Find(7) == first);
    CHECK(registry.FindByName("Scryers") == second);
    CHECK(registry.FindByCaptain(ObjectGuid(HIGHGUID_PLAYER, uint32(5101))) == first);
    CHECK(registry.Find(9) == NULL);
    CHECK(registry.FindByName("Consortium") == NULL);
    CHECK_EQ(registry.All().size(), size_t(2));

    registry.Remove(7);

    CHECK(registry.Find(7) == NULL);
    CHECK(registry.FindByName("Aldors") == NULL);
    CHECK(registry.Find(8) == second);
    CHECK_EQ(registry.All().size(), size_t(1));

    delete first;
}

TEST(ArenaTeamRegistry_TheGuardsRegistryIsTheInstalledOneWhileTheGuardLives)
{
    InstalledArenaTeamRegistry outer;
    {
        InstalledArenaTeamRegistry inner;
        CHECK(&sArenaTeamRegistry == &inner.registry);
    }
    CHECK(&sArenaTeamRegistry == &outer.registry);
}
