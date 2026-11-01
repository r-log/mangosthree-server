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

#ifndef MANGOS_H_MAPNOTICES
#define MANGOS_H_MAPNOTICES

#include "Platform/Define.h"
#include "ObjectGuid.h"
#include "SharedDefines.h"
#include "Weather.h"

class Map;
class Player;
class Unit;
struct CreatureInfo;

/**
 * What a map tells the clients in it, one call per notice; who is told is the map's own send to its
 * players. The session installs the notices that send them at start-up; this base sends nothing.
 */
class MapNotices
{
    public:
        virtual ~MapNotices();

        /// `player` is told it is bound to its instance for good.
        virtual void InstanceSaveCreated(Player& player);
        /// The players in `map` are told the encounter frame change `type` for the unit `source`,
        /// with the change's `param1` and `param2`.
        virtual void EncounterFrame(Map const& map, uint32 type, ObjectGuid source, uint8 param1, uint8 param2);
        /// `player` is told the weather `state` at the intensity `grade`.
        virtual void WeatherFor(Player& player, WeatherState state, float grade);
        /// The players of `map` in the zone `zoneId` are told the weather `state` at the intensity
        /// `grade`; answers whether any was.
        virtual bool WeatherInZone(Map const& map, uint32 zoneId, WeatherState state, float grade);
        /// The players of `map`, only those in the zone `zoneId` unless it is 0, play the sound `soundId`.
        virtual void SoundToMap(Map const& map, uint32 soundId, uint32 zoneId);
        /// The players of `map` hear the creature `cinfo`, as its spawn `senderLowGuid` (0 for none),
        /// yell the text `textId` in `language`, each in its own locale, naming `target`.
        virtual void MonsterYell(Map const& map, CreatureInfo const& cinfo, int32 textId, Language language, Unit const* target, uint32 senderLowGuid);

        /// The installed notices, or the base when none are installed.
        static MapNotices& Installed();
        /// Installs `notices` and answers the ones it replaces (NULL for the base). NULL installs the
        /// base. Installed once at start-up, before the world loads.
        static MapNotices* Install(MapNotices* notices);
};

#define sMapNotices MapNotices::Installed()

#endif
