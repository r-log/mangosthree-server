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

#include "entities/player/PlayerNotices.h"

#include <atomic>
#include <cstddef>

namespace
{
    std::atomic<PlayerNotices*> s_playerNoticesInstalled(NULL);

    PlayerNotices& BasePlayerNotices()
    {
        static PlayerNotices notices;
        return notices;
    }
}

PlayerNotices::~PlayerNotices()
{
}

void PlayerNotices::TransferPending(Player const& /*player*/, uint32 /*mapId*/, Transport const* /*transport*/)
{
}

void PlayerNotices::NewWorld(Player const& /*player*/, uint32 /*mapId*/, float /*x*/, float /*y*/, float /*z*/, float /*o*/)
{
}

void PlayerNotices::XpGainLogged(Player const& /*player*/, uint32 /*givenXp*/, uint32 /*restXp*/, ObjectGuid /*victim*/)
{
}

void PlayerNotices::LevelUpInfoShown(Player const& /*player*/, uint32 /*level*/, uint32 /*healthGain*/, uint32 /*manaGain*/, std::array<uint32, MAX_STATS> const& /*statGains*/)
{
}

void PlayerNotices::CinematicStarted(Player const& /*player*/, uint32 /*sequenceId*/)
{
}

void PlayerNotices::MovieStarted(Player const& /*player*/, uint32 /*movieId*/)
{
}

void PlayerNotices::GuildInviteDeclined(Player const& /*player*/, std::string const& /*name*/, bool /*autoDecline*/)
{
}

void PlayerNotices::BinderConfirmAsked(Player const& /*player*/, ObjectGuid /*binder*/)
{
}

void PlayerNotices::ExplorationExperienceShown(Player const& /*player*/, uint32 /*areaId*/, uint32 /*xp*/)
{
}

void PlayerNotices::BindPointShown(Player const& /*player*/, float /*x*/, float /*y*/, float /*z*/, uint32 /*mapId*/, uint32 /*areaId*/)
{
}

void PlayerNotices::WorldServerInfoShown(Player const& /*player*/, uint32 /*lastWeeklyReset*/, uint32 /*difficulty*/)
{
}

void PlayerNotices::UnlearnSpellsShown(Player const& /*player*/)
{
}

void PlayerNotices::LoginTimeSpeedShown(Player const& /*player*/, time_t /*now*/)
{
}

void PlayerNotices::TitleEarned(Player const& /*player*/, uint32 /*maskId*/, bool /*lost*/)
{
}

void PlayerNotices::PetitionSignResultShown(Player const& /*player*/, ObjectGuid /*petition*/, ObjectGuid /*signer*/, uint32 /*result*/)
{
}

void PlayerNotices::PetitionTurnInResultShown(Player const& /*player*/, uint32 /*result*/)
{
}

PlayerNotices& PlayerNotices::Installed()
{
    PlayerNotices* notices = s_playerNoticesInstalled.load(std::memory_order_acquire);
    return notices ? *notices : BasePlayerNotices();
}

PlayerNotices* PlayerNotices::Install(PlayerNotices* notices)
{
    return s_playerNoticesInstalled.exchange(notices, std::memory_order_acq_rel);
}
