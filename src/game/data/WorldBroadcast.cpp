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

#include "data/WorldBroadcast.h"

#include <atomic>
#include <cstddef>

namespace
{
    std::atomic<WorldBroadcast*> s_installed(NULL);

    WorldBroadcast& BaseBroadcast()
    {
        static WorldBroadcast broadcast;
        return broadcast;
    }
}

WorldBroadcast::~WorldBroadcast()
{
}

void WorldBroadcast::WorldTextV(int32 /*stringId*/, va_list* /*args*/)
{
}

void WorldBroadcast::WorldText(int32 stringId, ...)
{
    va_list ap;
    va_start(ap, stringId);
    WorldTextV(stringId, &ap);
    va_end(ap);
}

void WorldBroadcast::ZoneUnderAttack(uint32 /*zoneId*/, Team /*team*/)
{
}

void WorldBroadcast::DefenseMessage(uint32 /*zoneId*/, int32 /*textId*/)
{
}

bool WorldBroadcast::PostAfterMapPhase(AfterMapPhasePost const& /*post*/)
{
    return false;
}

void WorldBroadcast::RealmFirstAchievement(char const* /*name*/, ObjectGuid /*guid*/, uint32 /*achievementId*/)
{
}

void WorldBroadcast::WorldStateChanged(uint32 /*field*/, uint32 /*value*/)
{
}

WorldBroadcast& WorldBroadcast::Installed()
{
    WorldBroadcast* broadcast = s_installed.load(std::memory_order_acquire);
    return broadcast ? *broadcast : BaseBroadcast();
}

WorldBroadcast* WorldBroadcast::Install(WorldBroadcast* broadcast)
{
    return s_installed.exchange(broadcast, std::memory_order_acq_rel);
}

bool WorldBroadcast::IsInstalled()
{
    return s_installed.load(std::memory_order_acquire) != NULL;
}
