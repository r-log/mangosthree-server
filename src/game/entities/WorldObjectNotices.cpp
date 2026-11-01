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

#include "entities/WorldObjectNotices.h"

#include <atomic>
#include <cstddef>

namespace
{
    std::atomic<WorldObjectNotices*> s_worldObjectNoticesInstalled(NULL);

    WorldObjectNotices& BaseWorldObjectNotices()
    {
        static WorldObjectNotices notices;
        return notices;
    }
}

WorldObjectNotices::~WorldObjectNotices()
{
}

void WorldObjectNotices::ObjectDespawnAnim(WorldObject const& /*sender*/, ObjectGuid /*guid*/)
{
}

void WorldObjectNotices::GameObjectCustomAnim(WorldObject const& /*sender*/, ObjectGuid /*guid*/, uint32 /*animId*/)
{
}

void WorldObjectNotices::DistanceSoundPlayed(WorldObject const& /*source*/, uint32 /*soundId*/, Player const* /*target*/)
{
}

void WorldObjectNotices::SoundPlayed(WorldObject const& /*source*/, uint32 /*soundId*/, Player const* /*target*/)
{
}

void WorldObjectNotices::MusicPlayed(WorldObject const& /*source*/, uint32 /*soundId*/, Player const* /*target*/)
{
}

void WorldObjectNotices::Emoted(Unit const& /*unit*/, uint32 /*emoteId*/)
{
}

void WorldObjectNotices::PowerUpdated(Unit const& /*unit*/, Powers /*power*/, uint32 /*value*/)
{
}

void WorldObjectNotices::Energized(Unit const& /*caster*/, Unit const& /*victim*/, uint32 /*spellId*/, uint32 /*amount*/, Powers /*power*/)
{
}

void WorldObjectNotices::Dismounted(Unit const& /*unit*/)
{
}

void WorldObjectNotices::OneShotAnimKitPlayed(Unit const& /*unit*/, uint32 /*id*/)
{
}

void WorldObjectNotices::VehicleRecIdChanged(Unit const& /*unit*/, uint32 /*entry*/)
{
}

void WorldObjectNotices::AiReacted(Creature const& /*creature*/, AiReaction /*reaction*/)
{
}

void WorldObjectNotices::BuildingDamaged(GameObject const& /*building*/, Unit& /*caster*/, uint32 /*damage*/, uint32 /*spell*/)
{
}

WorldObjectNotices& WorldObjectNotices::Installed()
{
    WorldObjectNotices* notices = s_worldObjectNoticesInstalled.load(std::memory_order_acquire);
    return notices ? *notices : BaseWorldObjectNotices();
}

WorldObjectNotices* WorldObjectNotices::Install(WorldObjectNotices* notices)
{
    return s_worldObjectNoticesInstalled.exchange(notices, std::memory_order_acq_rel);
}
