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

#ifndef MANGOS_H_GRAVEYARDSTORE
#define MANGOS_H_GRAVEYARDSTORE

#include "Platform/Define.h"
#include "SharedDefines.h"

#include <map>
#include <utility>

struct GraveYardData
{
    uint32 safeLocId;
    Team team;
};
typedef std::multimap < uint32 /*zoneId*/, GraveYardData > GraveYardMap;
typedef std::pair<GraveYardMap::const_iterator, GraveYardMap::const_iterator> GraveYardMapBounds;

/**
 * The graveyards linked to each zone (`game_graveyard_zone`): per zone id, the links in the order
 * added, each a graveyard safe location and the team it serves. A graveyard is linked to a zone at
 * most once; finding a link answers the first one for that graveyard.
 */
class GraveyardStore
{
    public:
        void Clear() { m_links.clear(); }
        void Add(uint32 zoneId, GraveYardData const& data) { m_links.insert(GraveYardMap::value_type(zoneId, data)); }
        GraveYardMapBounds Zone(uint32 zoneId) const { return m_links.equal_range(zoneId); }
        GraveYardData const* Find(uint32 safeLocId, uint32 zoneId) const;
        /// The link of `safeLocId` in `zoneId` for an edit, or NULL.
        GraveYardData* FindForEdit(uint32 safeLocId, uint32 zoneId);

    private:
        GraveYardMap m_links;
};

#endif
