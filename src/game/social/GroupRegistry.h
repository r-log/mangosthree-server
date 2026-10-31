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

#ifndef MANGOS_H_GROUPREGISTRY
#define MANGOS_H_GROUPREGISTRY

#include "Platform/Define.h"

#include <unordered_map>

class Group;

/**
 * The world's groups by id: the ones loaded from the character database at startup and the ones
 * formed since. A group is listed when it is created or loaded and unlisted when it is disbanded;
 * the groups still listed are deleted with the registry. The world owns the registry and installs
 * it; the code below the world reaches it through sGroupRegistry.
 */
class GroupRegistry
{
    public:
        GroupRegistry() { }
        /// Deletes every group still listed.
        ~GroupRegistry();

        GroupRegistry(GroupRegistry const&) = delete;
        GroupRegistry& operator=(GroupRegistry const&) = delete;

        /// The installed registry; asserts when none is installed.
        static GroupRegistry& Installed();
        /// Installs `registry` and answers the one it replaces (NULL for none).
        static GroupRegistry* Install(GroupRegistry* registry);

        /// The group with the id, or NULL.
        Group* Find(uint32 id) const;
        void Add(Group* group);
        void Remove(Group* group);

        /// Renumbers the stored group ids from 1 up; runs before the group guid generator is seeded.
        static void PackIds();
        /// Loads the groups, their members and their instance binds, and disbands a group left with
        /// fewer than two members.
        void Load();

    private:
        typedef std::unordered_map<uint32, Group*> GroupMap;

        GroupMap m_groups;
};

#define sGroupRegistry GroupRegistry::Installed()

#endif
