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

#include "Maps/MapNotices.h"

#include <atomic>

namespace
{
    std::atomic<MapNotices*> s_mapNoticesInstalled(NULL);

    MapNotices& BaseMapNotices()
    {
        static MapNotices notices;
        return notices;
    }
}

MapNotices::~MapNotices()
{
}

void MapNotices::InstanceSaveCreated(Player& /*player*/)
{
}

void MapNotices::EncounterFrame(Map const& /*map*/, uint32 /*type*/, ObjectGuid /*source*/, uint8 /*param1*/, uint8 /*param2*/)
{
}

void MapNotices::WeatherFor(Player& /*player*/, WeatherState /*state*/, float /*grade*/)
{
}

bool MapNotices::WeatherInZone(Map const& /*map*/, uint32 /*zoneId*/, WeatherState /*state*/, float /*grade*/)
{
    return false;
}

void MapNotices::SoundToMap(Map const& /*map*/, uint32 /*soundId*/, uint32 /*zoneId*/)
{
}

void MapNotices::MonsterYell(Map const& /*map*/, CreatureInfo const& /*cinfo*/, int32 /*textId*/, Language /*language*/, Unit const* /*target*/, uint32 /*senderLowGuid*/)
{
}

MapNotices& MapNotices::Installed()
{
    MapNotices* notices = s_mapNoticesInstalled.load(std::memory_order_acquire);
    return notices ? *notices : BaseMapNotices();
}

MapNotices* MapNotices::Install(MapNotices* notices)
{
    return s_mapNoticesInstalled.exchange(notices, std::memory_order_acq_rel);
}
