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

#include "PlayerWorldPackets.h"
#include "MapPersistentStateMgr.h"
#include "Opcodes.h"
#include "WorldPacket.h"
#include "session/packets/spells/SpellEffectPackets.h"

#include <ctime>

void SessionPlayerWorldNotices::DungeonDifficultyShown(Player const& player, uint32 difficulty, bool inGroup)
{
    uint8 val = 0x00000001;
    WorldPacket data(MSG_SET_DUNGEON_DIFFICULTY, 12);
    data << uint32(difficulty);
    data << uint32(val);
    data << uint32(inGroup);
    player.SendDirectMessage(&data);
}

void SessionPlayerWorldNotices::RaidDifficultyShown(Player const& player, uint32 difficulty, bool inGroup)
{
    uint8 val = 0x00000001;
    WorldPacket data(MSG_SET_RAID_DIFFICULTY, 12);
    data << uint32(difficulty);
    data << uint32(val);
    data << uint32(inGroup);
    player.SendDirectMessage(&data);
}

void SessionPlayerWorldNotices::InstanceResetFailedNotify(Player const& player, uint32 mapId)
{
    WorldPacket data(SMSG_RESET_FAILED_NOTIFY, 4);
    data << uint32(mapId);
    player.SendDirectMessage(&data);
}

void SessionPlayerWorldNotices::InstanceReset(Player const& player, uint32 mapId)
{
    WorldPacket data(SMSG_INSTANCE_RESET, 4);
    data << uint32(mapId);
    player.SendDirectMessage(&data);
}

void SessionPlayerWorldNotices::InstanceResetFailed(Player const& player, uint32 reason, uint32 mapId)
{
    // TODO: find what other fail reasons there are besides players in the instance
    WorldPacket data(SMSG_INSTANCE_RESET_FAILED, 4);
    data << uint32(reason);
    data << uint32(mapId);
    player.SendDirectMessage(&data);
}

void SessionPlayerWorldNotices::RaidInstanceMessage(Player const& player, uint32 type, uint32 mapId, Difficulty difficulty, uint32 time)
{
    WorldPacket data(SMSG_RAID_INSTANCE_MESSAGE, 4 + 4 + 4 + 4);
    data << uint32(type);
    data << uint32(mapId);
    data << uint32(difficulty);                             // difficulty
    data << uint32(time);
    if (type == RAID_INSTANCE_WELCOME)
    {
        data << uint8(0);                                   // is your (1)
        data << uint8(0);                                   // is extended (1), ignored if prev field is 0
    }
    player.SendDirectMessage(&data);
}

void SessionPlayerWorldNotices::RaidInfoShown(Player const& player, std::vector<DungeonPersistentState*> const& permBinds)
{
    WorldPacket data(SMSG_RAID_INSTANCE_INFO, 4);
    data << uint32(permBinds.size());

    time_t now = time(NULL);

    for (std::vector<DungeonPersistentState*>::const_iterator itr = permBinds.begin(); itr != permBinds.end(); ++itr)
    {
        DungeonPersistentState* state = *itr;
        data << uint32(state->GetMapId());          // map id
        data << uint32(state->GetDifficulty());     // difficulty
        data << ObjectGuid(state->GetInstanceGuid());// instance guid
        data << uint8(1);                           // expired = 0
        data << uint8(0);                           // extended = 1
        data << uint32(state->GetResetTime() - now);// reset time
        data << uint32(state->GetCompletedEncountersMask());// completed encounter mask
    }
    player.SendDirectMessage(&data);
}

void SessionPlayerWorldNotices::SavedInstancesShown(Player const& player, std::vector<uint32> const& permMapIds)
{
    WorldPacket data;

    // Send opcode 811. true or false means, whether you have current raid/heroic instances
    data.Initialize(SMSG_UPDATE_INSTANCE_OWNERSHIP);
    data << uint32(!permMapIds.empty());
    player.SendDirectMessage(&data);

    for (std::vector<uint32>::const_iterator itr = permMapIds.begin(); itr != permMapIds.end(); ++itr)
    {
        data.Initialize(SMSG_UPDATE_LAST_INSTANCE);
        data << uint32(*itr);
        player.SendDirectMessage(&data);
    }
}

void SessionPlayerWorldNotices::MirrorTimerStarted(Player const& player, MirrorTimerType type, uint32 max, uint32 current, int32 regen)
{
    WorldPacket data(SMSG_START_MIRROR_TIMER, (21));
    data << (uint32)type;
    data << current;
    data << max;
    data << regen;
    data << (uint8)0;
    data << (uint32)0; // Spell ID
    player.SendDirectMessage(&data);
}

void SessionPlayerWorldNotices::MirrorTimerStopped(Player const& player, MirrorTimerType type)
{
    WorldPacket data(SMSG_STOP_MIRROR_TIMER, 4);
    data << (uint32)type;
    player.SendDirectMessage(&data);
}

void SessionPlayerWorldNotices::EnvironmentalDamageLogged(Player const& player, EnviromentalDamage type, uint32 damage, uint32 absorb, uint32 resist)
{
    WorldPacket data(SMSG_ENVIRONMENTALDAMAGELOG, (21));
    data << player.GetObjectGuid();
    data << uint8(type != DAMAGE_FALL_TO_VOID ? type : DAMAGE_FALL);
    data << uint32(damage);
    data << uint32(absorb);
    data << uint32(resist);
    player.SendMessageToSet(&data, true);
}

void SessionPlayerWorldNotices::InebriationThresholdCrossed(Player const& player, uint32 state, uint32 itemId)
{
    // Send a packet to update the drunken state
    WorldPacket data(SMSG_CROSSED_INEBRIATION_THRESHOLD, (8 + 4 + 4));
    data << player.GetObjectGuid();
    data << uint32(state);
    data << uint32(itemId);

    player.SendMessageToSet(&data, true);
}

void SessionPlayerWorldNotices::NewTaxiPath(Player const& player)
{
    WorldPacket data;
    BuildNewTaxiPathPacket(data);
    player.SendDirectMessage(&data);
}

void SessionPlayerWorldNotices::RaidGroupOnly(Player const& player, uint32 timerMs, uint32 error)
{
    WorldPacket data(SMSG_RAID_GROUP_ONLY, 4 + 4);
    data << uint32(timerMs);
    data << uint32(error);
    player.SendDirectMessage(&data);
}

void SessionPlayerWorldNotices::PvpRewardsShown(Player const& player)
{
    // Placeholder

    WorldPacket data(SMSG_PVP_REWARDS, 6 * 4);
    data << uint32(1650);   // rbg conquest cap
    data << uint32(0);      // total conquest earned
    data << uint32(1350);   // arena conquest cap
    data << uint32(0);      // rbg conquest earned
    data << uint32(0);      // arena conquest earned
    data << uint32(1650);   // total conquest cap

    player.SendDirectMessage(&data);
}

void SessionPlayerWorldNotices::RatedBgStatsShown(Player const& player)
{
    // Placeholder

    WorldPacket data(SMSG_RATED_BG_STATS, 18 * 4);
    for (int i = 0; i < 18; ++i)
    {
        data << uint32(0);
    }

    player.SendDirectMessage(&data);
}
