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

#include "entities/player/combat/PlayerCombatNotices.h"

#include <atomic>
#include <cstddef>

namespace
{
    std::atomic<PlayerCombatNotices*> s_playerCombatNoticesInstalled(NULL);

    PlayerCombatNotices& BasePlayerCombatNotices()
    {
        static PlayerCombatNotices notices;
        return notices;
    }
}

PlayerCombatNotices::~PlayerCombatNotices()
{
}

void PlayerCombatNotices::SwingDeadTarget(Player const& /*player*/)
{
}

void PlayerCombatNotices::SwingCantAttack(Player const& /*player*/)
{
}

void PlayerCombatNotices::ComboPointsUpdated(Player const& /*player*/, Unit const& /*target*/, uint8 /*points*/)
{
}

void PlayerCombatNotices::PreResurrect(Player const& /*player*/)
{
}

void PlayerCombatNotices::SpiritHealerLocCleared(Player const& /*player*/)
{
}

void PlayerCombatNotices::SpiritHealerLocShown(Player const& /*player*/, uint32 /*mapId*/, float /*x*/, float /*y*/, float /*z*/)
{
}

void PlayerCombatNotices::CorpseReclaimDelay(Player const& /*player*/, uint32 /*delayMs*/)
{
}

void PlayerCombatNotices::DuelOutOfBounds(Player const& /*player*/)
{
}

void PlayerCombatNotices::DuelInBounds(Player const& /*player*/)
{
}

void PlayerCombatNotices::DuelCompleted(Player const& /*player*/, Player const& /*opponent*/, DuelCompleteType /*type*/)
{
}

void PlayerCombatNotices::DuelCountdown(Player const& /*player*/, uint32 /*seconds*/)
{
}

PlayerCombatNotices& PlayerCombatNotices::Installed()
{
    PlayerCombatNotices* notices = s_playerCombatNoticesInstalled.load(std::memory_order_acquire);
    return notices ? *notices : BasePlayerCombatNotices();
}

PlayerCombatNotices* PlayerCombatNotices::Install(PlayerCombatNotices* notices)
{
    return s_playerCombatNoticesInstalled.exchange(notices, std::memory_order_acq_rel);
}
