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

#ifndef MANGOS_H_QUESTPOISTORE
#define MANGOS_H_QUESTPOISTORE

#include "Platform/Define.h"

#include <unordered_map>
#include <vector>

struct QuestPOIPoint
{
    int32 x;
    int32 y;

    QuestPOIPoint() : x(0), y(0) {}
    QuestPOIPoint(int32 _x, int32 _y) : x(_x), y(_y) {}
};

struct QuestPOI
{
    uint32 PoiId;
    int32  ObjectiveIndex;
    uint32 MapId;
    uint32 MapAreaId;
    uint32 FloorId;
    uint32 Unk3;
    uint32 Unk4;
    std::vector<QuestPOIPoint> points;

    QuestPOI() : PoiId(0), ObjectiveIndex(0), MapId(0), MapAreaId(0), FloorId(0), Unk3(0), Unk4(0) {}
    QuestPOI(uint32 poiId, int32 objIndex, uint32 mapId, uint32 mapAreaId, uint32 floorId, uint32 unk3, uint32 unk4) : PoiId(poiId), ObjectiveIndex(objIndex), MapId(mapId), MapAreaId(mapAreaId), FloorId(floorId), Unk3(unk3), Unk4(unk4) {}
};

typedef std::vector<QuestPOI> QuestPOIVector;
typedef std::unordered_map<uint32, QuestPOIVector> QuestPOIMap;

/**
 * The quest points of interest (`quest_poi`, with their points from `quest_poi_points`): per quest,
 * the POIs in the order they were added, each holding its points. A quest with no entry answers
 * NULL and clearing forgets every quest. Editing a quest returns its POIs for the points to be
 * added to, and creates an empty entry for a quest that has none, which a lookup then answers.
 */
class QuestPOIStore
{
    public:
        void Clear();
        void Add(uint32 questId, QuestPOI const& poi);
        QuestPOIVector& Edit(uint32 questId);
        QuestPOIVector const* Find(uint32 questId) const;

    private:
        QuestPOIMap m_pois;
};

#endif
