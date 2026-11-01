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

#ifndef MANGOS_H_MAPPACKETS
#define MANGOS_H_MAPPACKETS

#include "Maps/MapNotices.h"

class WorldPacket;

/// Initializes `packet` as the notice that the client is bound to its instance for good: 0.
void BuildInstanceSaveCreatedPacket(WorldPacket& packet);
/// Initializes `packet` as the encounter frame change `type`: the type, then, by type, the unit's
/// packed guid and `param1`, `param1` alone, `param1` and `param2`, or nothing.
void BuildEncounterFramePacket(WorldPacket& packet, uint32 type, ObjectGuid source, uint8 param1, uint8 param2);
/// Initializes `packet` as the weather `state` at the intensity `grade`: the state, the grade as a
/// float, then 0.
void BuildWeatherPacket(WorldPacket& packet, WeatherState state, float grade);

/// The map's notices, each built here and sent through the map's own sends to its players.
class SessionMapNotices : public MapNotices
{
    public:
        void InstanceSaveCreated(Player& player) override;
        void EncounterFrame(Map const& map, uint32 type, ObjectGuid source, uint8 param1, uint8 param2) override;
        void WeatherFor(Player& player, WeatherState state, float grade) override;
        bool WeatherInZone(Map const& map, uint32 zoneId, WeatherState state, float grade) override;
        void SoundToMap(Map const& map, uint32 soundId, uint32 zoneId) override;
};

#endif
