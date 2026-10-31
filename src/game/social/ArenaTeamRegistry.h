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

#ifndef MANGOS_H_ARENATEAMREGISTRY
#define MANGOS_H_ARENATEAMREGISTRY

#include "ObjectGuid.h"
#include "Platform/Define.h"

#include <string>
#include <unordered_map>

class ArenaTeam;

/**
 * The world's arena teams by id: the ones loaded from the character database at startup and the
 * ones created since. A team is listed when it is created or loaded and unlisted when it is
 * disbanded; the teams still listed are deleted with the registry. The world owns the registry and
 * installs it; the code below the world reaches it through sArenaTeamRegistry.
 */
class ArenaTeamRegistry
{
    public:
        typedef std::unordered_map<uint32, ArenaTeam*> ArenaTeamMap;

        ArenaTeamRegistry() { }
        /// Deletes every team still listed.
        ~ArenaTeamRegistry();

        ArenaTeamRegistry(ArenaTeamRegistry const&) = delete;
        ArenaTeamRegistry& operator=(ArenaTeamRegistry const&) = delete;

        /// The installed registry; asserts when none is installed.
        static ArenaTeamRegistry& Installed();
        /// Installs `registry` and answers the one it replaces (NULL for none).
        static ArenaTeamRegistry* Install(ArenaTeamRegistry* registry);

        /// The team with the id, or NULL.
        ArenaTeam* Find(uint32 arenateamid) const;
        /// The team with the name, or NULL.
        ArenaTeam* FindByName(std::string const& arenateamname) const;
        /// The team the player captains, or NULL.
        ArenaTeam* FindByCaptain(ObjectGuid guid) const;
        void Add(ArenaTeam* arenaTeam);
        void Remove(uint32 Id);
        ArenaTeamMap const& All() const { return m_teams; }

        /// Loads the teams and their members, and disbands a team that fails to load.
        void Load();

    private:
        ArenaTeamMap m_teams;
};

#define sArenaTeamRegistry ArenaTeamRegistry::Installed()

#endif
