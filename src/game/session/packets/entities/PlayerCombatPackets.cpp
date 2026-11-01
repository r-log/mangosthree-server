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

#include "PlayerCombatPackets.h"
#include "Opcodes.h"
#include "Player.h"
#include "Unit.h"
#include "WorldPacket.h"

void BuildDeathReleaseLocPacket(WorldPacket& packet, uint32 mapId, float x, float y, float z)
{
    packet.Initialize(SMSG_DEATH_RELEASE_LOC, 4 * 4);
    packet << uint32(mapId);
    packet << float(x);
    packet << float(y);
    packet << float(z);
}

void SessionPlayerCombatNotices::SwingDeadTarget(Player const& player)
{
    WorldPacket data(SMSG_ATTACKSWING_DEADTARGET, 0);
    player.SendDirectMessage(&data);
}

void SessionPlayerCombatNotices::SwingCantAttack(Player const& player)
{
    WorldPacket data(SMSG_ATTACKSWING_CANT_ATTACK, 0);
    player.SendDirectMessage(&data);
}

void SessionPlayerCombatNotices::ComboPointsUpdated(Player const& player, Unit const& target, uint8 points)
{
    WorldPacket data(SMSG_UPDATE_COMBO_POINTS, target.GetPackGUID().size() + 1);
    data << target.GetPackGUID();
    data << uint8(points);
    player.SendDirectMessage(&data);
}

void SessionPlayerCombatNotices::PreResurrect(Player const& player)
{
    WorldPacket data(SMSG_PRE_RESURRECT, player.GetPackGUID().size());
    data << player.GetPackGUID();
    player.SendDirectMessage(&data);
}

void SessionPlayerCombatNotices::SpiritHealerLocCleared(Player const& player)
{
    WorldPacket data;
    BuildDeathReleaseLocPacket(data, uint32(-1), float(0), float(0), float(0));
    player.SendDirectMessage(&data);
}

void SessionPlayerCombatNotices::SpiritHealerLocShown(Player const& player, uint32 mapId, float x, float y, float z)
{
    WorldPacket data;
    BuildDeathReleaseLocPacket(data, mapId, x, y, z);
    player.SendDirectMessage(&data);
}

void SessionPlayerCombatNotices::CorpseReclaimDelay(Player const& player, uint32 delayMs)
{
    WorldPacket data(SMSG_CORPSE_RECLAIM_DELAY, 4);
    data << uint32(delayMs);
    player.SendDirectMessage(&data);
}

void SessionPlayerCombatNotices::DuelOutOfBounds(Player const& player)
{
    WorldPacket data(SMSG_DUEL_OUTOFBOUNDS, 0);
    player.SendDirectMessage(&data);
}

void SessionPlayerCombatNotices::DuelInBounds(Player const& player)
{
    WorldPacket data(SMSG_DUEL_INBOUNDS, 0);
    player.SendDirectMessage(&data);
}

void SessionPlayerCombatNotices::DuelCompleted(Player const& player, Player const& opponent, DuelCompleteType type)
{
    WorldPacket data(SMSG_DUEL_COMPLETE, (1));
    data << (uint8)((type != DUEL_INTERRUPTED) ? 1 : 0);
    player.SendDirectMessage(&data);
    opponent.SendDirectMessage(&data);

    if (type != DUEL_INTERRUPTED)
    {
        data.Initialize(SMSG_DUEL_WINNER, (1 + 20));          // we guess size
        data << (uint8)((type == DUEL_WON) ? 0 : 1);          // 0 = just won; 1 = fled
        data << opponent.GetName();
        data << player.GetName();
        player.SendMessageToSet(&data, true);
    }
}

void SessionPlayerCombatNotices::DuelCountdown(Player const& player, uint32 seconds)
{
    WorldPacket data(SMSG_DUEL_COUNTDOWN, 4);
    data << uint32(seconds);                                // seconds
    player.SendDirectMessage(&data);
}
