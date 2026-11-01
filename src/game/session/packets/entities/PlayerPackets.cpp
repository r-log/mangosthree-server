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

#include "PlayerPackets.h"
#include "Opcodes.h"
#include "Transports.h"
#include "Util.h"
#include "WorldPacket.h"
#include "session/packets/spells/SpellEffectPackets.h"

void BuildPetitionSignResultPacket(WorldPacket& packet, ObjectGuid petition, ObjectGuid signer, uint32 result)
{
    packet.Initialize(SMSG_PETITION_SIGN_RESULTS, 8 + 8 + 4);
    packet << petition;
    packet << signer;
    packet << uint32(result);
}

void SessionPlayerNotices::TransferPending(Player const& player, uint32 mapId, Transport const* transport)
{
    WorldPacket data(SMSG_TRANSFER_PENDING, (4 + 4 + 4));
    data.WriteBit(0);       // unknown
    if (transport)
    {
        data.WriteBit(1);   // has transport
        // The map he is LEAVING, and that is the one the ship sails. Never her
        // own: the client has no terrain for it and dies in CMap::LoadWdt().
        data << uint32(transport->GetMapId());
        data << uint32(transport->GetEntry());
    }
    else
    {
        data.WriteBit(0);   // has transport
    }

    data << uint32(mapId);
    player.SendDirectMessage(&data);
}

void SessionPlayerNotices::NewWorld(Player const& player, uint32 mapId, float x, float y, float z, float o)
{
    // transfer finished, inform client to start load
    WorldPacket data(SMSG_NEW_WORLD, 20);
    data << float(x);
    data << float(o);
    data << float(y);
    data << uint32(mapId);
    data << float(z);
    player.SendDirectMessage(&data);
}

void SessionPlayerNotices::XpGainLogged(Player const& player, uint32 givenXp, uint32 restXp, ObjectGuid victim)
{
    WorldPacket data(SMSG_LOG_XPGAIN, 21);
    data << victim;                                         // guid
    data << uint32(givenXp + restXp);                       // given experience
    data << uint8(!victim.IsEmpty() ? 0 : 1);               // 00-kill_xp type, 01-non_kill_xp type
    if (!victim.IsEmpty())
    {
        data << uint32(givenXp);                            // experience without rested bonus
        data << float(1);                                   // 1 - none 0 - 100% group bonus output
    }
    data << uint8(0);                                       // new 2.4.0
    player.SendDirectMessage(&data);
}

void SessionPlayerNotices::LevelUpInfoShown(Player const& player, uint32 level, uint32 healthGain, uint32 manaGain, std::array<uint32, MAX_STATS> const& statGains)
{
    // send levelup info to client
    WorldPacket data(SMSG_LEVELUP_INFO, (4 + 4 + MAX_STORED_POWERS * 4 + MAX_STATS * 4));
    data << uint32(level);
    data << uint32(healthGain);
    // for(int i = 0; i < MAX_POWERS; ++i)                  // Powers loop (0-4)
    data << uint32(manaGain);
    data << uint32(0);
    data << uint32(0);
    data << uint32(0);
    data << uint32(0);
    // end for
    for (int i = STAT_STRENGTH; i < MAX_STATS; ++i)         // Stats loop (0-4)
    {
        data << uint32(statGains[i]);
    }

    player.SendDirectMessage(&data);
}

void SessionPlayerNotices::CinematicStarted(Player const& player, uint32 sequenceId)
{
    WorldPacket data(SMSG_TRIGGER_CINEMATIC, 4);
    data << uint32(sequenceId);
    player.SendDirectMessage(&data);
}

void SessionPlayerNotices::MovieStarted(Player const& player, uint32 movieId)
{
    WorldPacket data(SMSG_TRIGGER_MOVIE, 4);
    data << uint32(movieId);
    player.SendDirectMessage(&data);
}

void SessionPlayerNotices::GuildInviteDeclined(Player const& player, std::string const& name, bool autoDecline)
{
    WorldPacket data(SMSG_GUILD_DECLINE, 10);
    data << name;
    data << uint8(autoDecline);
    player.SendDirectMessage(&data);
}

void SessionPlayerNotices::BinderConfirmAsked(Player const& player, ObjectGuid binder)
{
    WorldPacket data(SMSG_BINDER_CONFIRM, 8);
    data << ObjectGuid(binder);
    player.SendDirectMessage(&data);
}

void SessionPlayerNotices::ExplorationExperienceShown(Player const& player, uint32 areaId, uint32 xp)
{
    WorldPacket data(SMSG_EXPLORATION_EXPERIENCE, 8);
    data << uint32(areaId);
    data << uint32(xp);
    player.SendDirectMessage(&data);
}

void SessionPlayerNotices::BindPointShown(Player const& player, float x, float y, float z, uint32 mapId, uint32 areaId)
{
    WorldPacket data;
    BuildBindPointUpdatePacket(data, WorldLocation(mapId, x, y, z), areaId);
    player.SendDirectMessage(&data);
}

void SessionPlayerNotices::WorldServerInfoShown(Player const& player, uint32 lastWeeklyReset, uint32 difficulty)
{
    WorldPacket data(SMSG_WORLD_SERVER_INFO, 1 + 1 + 4 + 4);
    data.WriteBit(0);                                               // HasRestrictedLevel
    data.WriteBit(0);                                               // HasRestrictedMoney
    data.WriteBit(0);                                               // IneligibleForLoot

    //if (IneligibleForLoot)
    //    data << uint32(0);                                        // EncounterMask

    data << uint8(0);                                               // IsOnTournamentRealm

    //if (HasRestrictedMoney)
    //    data << uint32(100000);                                   // RestrictedMoney (starter accounts)
    //if (HasRestrictedLevel)
    //    data << uint32(20);                                       // RestrictedLevel (starter accounts)

    data << uint32(lastWeeklyReset);                                // LastWeeklyReset (not instance reset)
    data << uint32(difficulty);
    player.SendDirectMessage(&data);
}

void SessionPlayerNotices::UnlearnSpellsShown(Player const& player)
{
    WorldPacket data(SMSG_SEND_UNLEARN_SPELLS, 4);
    data << uint32(0);                                      // count, for(count) uint32;
    player.SendDirectMessage(&data);
}

void SessionPlayerNotices::LoginTimeSpeedShown(Player const& player, time_t now)
{
    WorldPacket data(SMSG_LOGIN_SETTIMESPEED, 4 + 4 + 4);
    data << uint32(secsToTimeBitFields(now));
    data << (float)0.01666667f;                             // game speed
    data << uint32(0);                                      // added in 3.1.2
    player.SendDirectMessage(&data);
}

void SessionPlayerNotices::TitleEarned(Player const& player, uint32 maskId, bool lost)
{
    WorldPacket data(SMSG_TITLE_EARNED, 4 + 4);
    data << uint32(maskId);
    data << uint32(lost ? 0 : 1);                           // 1 - earned, 0 - lost
    player.SendDirectMessage(&data);
}

void SessionPlayerNotices::PetitionSignResultShown(Player const& player, ObjectGuid petition, ObjectGuid signer, uint32 result)
{
    WorldPacket data;
    BuildPetitionSignResultPacket(data, petition, signer, result);
    player.SendDirectMessage(&data);
}

void SessionPlayerNotices::PetitionTurnInResultShown(Player const& player, uint32 result)
{
    WorldPacket data(SMSG_TURN_IN_PETITION_RESULTS, 4);
    data << uint32(result);
    player.SendDirectMessage(&data);
}
