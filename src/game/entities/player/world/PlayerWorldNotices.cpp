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

#include "entities/player/world/PlayerWorldNotices.h"

#include <atomic>
#include <cstddef>

namespace
{
    std::atomic<PlayerWorldNotices*> s_playerWorldNoticesInstalled(NULL);

    PlayerWorldNotices& BasePlayerWorldNotices()
    {
        static PlayerWorldNotices notices;
        return notices;
    }
}

PlayerWorldNotices::~PlayerWorldNotices()
{
}

void PlayerWorldNotices::DungeonDifficultyShown(Player const& /*player*/, uint32 /*difficulty*/, bool /*inGroup*/)
{
}

void PlayerWorldNotices::RaidDifficultyShown(Player const& /*player*/, uint32 /*difficulty*/, bool /*inGroup*/)
{
}

void PlayerWorldNotices::InstanceResetFailedNotify(Player const& /*player*/, uint32 /*mapId*/)
{
}

void PlayerWorldNotices::InstanceReset(Player const& /*player*/, uint32 /*mapId*/)
{
}

void PlayerWorldNotices::InstanceResetFailed(Player const& /*player*/, uint32 /*reason*/, uint32 /*mapId*/)
{
}

void PlayerWorldNotices::RaidInstanceMessage(Player const& /*player*/, uint32 /*type*/, uint32 /*mapId*/, Difficulty /*difficulty*/, uint32 /*time*/)
{
}

void PlayerWorldNotices::RaidInfoShown(Player const& /*player*/, std::vector<DungeonPersistentState*> const& /*permBinds*/)
{
}

void PlayerWorldNotices::SavedInstancesShown(Player const& /*player*/, std::vector<uint32> const& /*permMapIds*/)
{
}

void PlayerWorldNotices::MirrorTimerStarted(Player const& /*player*/, MirrorTimerType /*type*/, uint32 /*max*/, uint32 /*current*/, int32 /*regen*/)
{
}

void PlayerWorldNotices::MirrorTimerStopped(Player const& /*player*/, MirrorTimerType /*type*/)
{
}

void PlayerWorldNotices::EnvironmentalDamageLogged(Player const& /*player*/, EnviromentalDamage /*type*/, uint32 /*damage*/, uint32 /*absorb*/, uint32 /*resist*/)
{
}

void PlayerWorldNotices::InebriationThresholdCrossed(Player const& /*player*/, uint32 /*state*/, uint32 /*itemId*/)
{
}

void PlayerWorldNotices::NewTaxiPath(Player const& /*player*/)
{
}

void PlayerWorldNotices::RaidGroupOnly(Player const& /*player*/, uint32 /*timerMs*/, uint32 /*error*/)
{
}

void PlayerWorldNotices::PvpRewardsShown(Player const& /*player*/)
{
}

void PlayerWorldNotices::RatedBgStatsShown(Player const& /*player*/)
{
}

PlayerWorldNotices& PlayerWorldNotices::Installed()
{
    PlayerWorldNotices* notices = s_playerWorldNoticesInstalled.load(std::memory_order_acquire);
    return notices ? *notices : BasePlayerWorldNotices();
}

PlayerWorldNotices* PlayerWorldNotices::Install(PlayerWorldNotices* notices)
{
    return s_playerWorldNoticesInstalled.exchange(notices, std::memory_order_acq_rel);
}
