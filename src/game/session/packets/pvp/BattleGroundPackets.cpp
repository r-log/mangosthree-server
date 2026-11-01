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

#include "BattleGroundPackets.h"
#include "ArenaTeam.h"
#include "social/ArenaTeamRegistry.h"
#include "BattleGroundAB.h"
#include "BattleGroundAV.h"
#include "BattleGroundEY.h"
#include "BattleGroundWS.h"
#include "DBCStores.h"
#include "Log.h"
#include "Opcodes.h"
#include "Player.h"
#include "PlayerRegistry.h"
#include "WorldPacket.h"
#include "session/packets/maps/SoundPacket.h"
#include "session/packets/maps/WorldStatePackets.h"

#include <cstdlib>
#include <set>

/**
 * @brief Builds a battlefield status packet for sending to the player.
 *
 * Constructs the network packet for SMSG_BATTLEFIELD_STATUS that informs the player
 * of their queue status, position, estimated wait time, and other relevant information.
 * Handles different status types: waiting in queue, invited to join, and in progress.
 *
 * @param data The packet to initialize.
 * @param bg Pointer to the battleground (may be NULL for status clear).
 * @param player
 * @param QueueSlot The queue slot index (0-2, player can be in multiple queues).
 * @param StatusID The status identifier (0=clear, STATUS_WAIT_QUEUE, STATUS_WAIT_JOIN, STATUS_IN_PROGRESS).
 * @param Time1 Status-specific time value (wait time, invitation timeout, or auto-leave time).
 * @param Time2 Secondary time value (queue time or elapsed battle time).
 * @param ArenaType
 */
void BuildBattleGroundStatusPacket(WorldPacket& data, BattleGround* bg, Player* player, uint8 QueueSlot, uint8 StatusID, uint32 Time1, uint32 Time2, ArenaType arenatype)
{
    // we can be in 2 queues in same time...
    if (!bg)
    {
        StatusID = STATUS_NONE;
    }

    ObjectGuid playerGuid = player->GetObjectGuid();
    ObjectGuid bgGuid = bg ? bg->GetObjectGuid() : ObjectGuid();

    DEBUG_LOG("BuildBattleGroundStatusPacket bgGuid %s, playerguid %s, queueSlot %u, "
        "statusid %u time1 %u time2 %u arenatype %u",
        bgGuid.GetString().c_str(), playerGuid.GetString().c_str(), QueueSlot, StatusID, Time1, Time2, arenatype);

    switch (StatusID)
    {
        case STATUS_NONE:
        {
            data.Initialize(SMSG_BATTLEFIELD_STATUS);

            data.WriteGuidMask<0, 4, 7, 1, 6, 3, 5, 2>(playerGuid);
            data.WriteGuidBytes<5, 6, 7, 2>(playerGuid);
            data << uint32(QueueSlot);                 // not queue slot
            data.WriteGuidBytes<3, 1>(playerGuid);
            data << uint32(QueueSlot);                 // Queue slot
            data << uint32(bg->GetStartTime());
            data.WriteGuidBytes<0, 4>(playerGuid);
            break;
        }
        case STATUS_WAIT_QUEUE:                             // status_in_queue
        {
            data.Initialize(SMSG_BATTLEFIELD_STATUS_QUEUED);

            data.WriteGuidMask<3, 0>(playerGuid);
            data.WriteGuidMask<3>(bgGuid);
            data.WriteGuidMask<2>(playerGuid);
            data.WriteBit(1);                          // eligible in queue
            data.WriteBit(0);                          // Join Failed
            data.WriteGuidMask<2>(bgGuid);
            data.WriteGuidMask<1>(playerGuid);
            data.WriteGuidMask<0, 6, 4>(bgGuid);
            data.WriteGuidMask<6, 7>(playerGuid);
            data.WriteGuidMask<7, 5>(bgGuid);
            data.WriteGuidMask<4, 5>(playerGuid);
            data.WriteBit(bg->isRated());              // is rated bg or arena. if israted => cant exit (wtf?)
            data.WriteBit(0);                          // Waiting On Other Activity 4
            data.WriteGuidMask<1>(bgGuid);

            data.WriteGuidBytes<0>(playerGuid);
            data << uint32(QueueSlot);                 // unk 5 - received in battlefield port then
            data.WriteGuidBytes<5>(bgGuid);
            data.WriteGuidBytes<3>(playerGuid);
            data << uint32(Time1);                     // Estimated Wait Time 6 time1
            data.WriteGuidBytes<7, 1, 2>(bgGuid);
            data << uint8(0);                          // unk param
            data.WriteGuidBytes<4>(bgGuid);
            data.WriteGuidBytes<2>(playerGuid);
            //if (...)
            //    data << uint8(playerCount);          // player count (XvX) > 0 && rated: Rated XvX, !rated: Skirmish XvX. if ==0 >= Bg type
            //else
                data << uint8(arenatype);
            data.WriteGuidBytes<6>(bgGuid);
            data.WriteGuidBytes<7>(playerGuid);
            data.WriteGuidBytes<3>(bgGuid);
            data.WriteGuidBytes<6>(playerGuid);
            data.WriteGuidBytes<0>(bgGuid);
            data << uint32(0);                         // Time
            data << uint32(QueueSlot);                 // queueslot
            data << uint8(0);                          // maxlevel? seen 85 only in sniffs
            data << uint32(Time2);                     // unk 10 Time since started time2
            data.WriteGuidBytes<1, 5>(playerGuid);
            data << uint32(bg->GetClientInstanceID()); // client instance id
            data.WriteGuidBytes<4>(playerGuid);
            break;
        }
        case STATUS_WAIT_JOIN:                              // status_invite
        {
            data.Initialize(SMSG_BATTLEFIELD_STATUS_NEEDCONFIRMATION, 44);

            data << uint32(bg->GetClientInstanceID()); // Client Instance ID
            data << uint32(Time1);                     // Time until closed
            //data << uint8(bg->GetPlayersCountByTeam(bg->GetPlayerTeam(playerGuid)));
            data << uint8(0);                          // unk 0
            data << uint32(QueueSlot);                 // Queue slot
            data << uint32(0);                         // Time
            data << uint8(0);                          // Max Level
            data << uint32(QueueSlot);                 // not queueslot
            data << uint32(bg->GetMapId());            // Map Id
            //if (...)
            //    data << uint8(playerCount);          // player count (XvX) > 0 && rated: Rated XvX, !rated: Skirmish XvX. if ==0 >= Bg type
            //else
                data << uint8(arenatype);

            data.WriteGuidMask<5, 2, 1>(playerGuid);
            data.WriteGuidMask<2>(bgGuid);
            data.WriteGuidMask<4>(playerGuid);
            data.WriteGuidMask<6, 3>(bgGuid);
            data.WriteBit(bg->isRated());              // Is Rated
            data.WriteGuidMask<7, 3>(playerGuid);
            data.WriteGuidMask<7, 0, 4>(bgGuid);
            data.WriteGuidMask<6>(playerGuid);
            data.WriteGuidMask<5, 1>(bgGuid);
            data.WriteGuidMask<0>(playerGuid);

            data.WriteGuidBytes<6, 5, 7, 2>(bgGuid);
            data.WriteGuidBytes<0, 7>(playerGuid);
            data.WriteGuidBytes<4>(bgGuid);
            data.WriteGuidBytes<1>(playerGuid);
            data.WriteGuidBytes<0>(bgGuid);
            data.WriteGuidBytes<4>(playerGuid);
            data.WriteGuidBytes<1>(bgGuid);
            data.WriteGuidBytes<5>(playerGuid);
            data.WriteGuidBytes<3>(bgGuid);
            data.WriteGuidBytes<6, 2, 3>(playerGuid);
            break;
        }
        case STATUS_IN_PROGRESS:
        {
            data.Initialize(SMSG_BATTLEFIELD_STATUS_ACTIVE, 49);

            data.WriteGuidMask<2, 7>(playerGuid);
            data.WriteGuidMask<7, 1>(bgGuid);
            data.WriteGuidMask<5>(playerGuid);
            data.WriteBit(bg->GetPlayerTeam(playerGuid) == ALLIANCE);  // Bg faction bit
            data.WriteGuidMask<0>(bgGuid);
            data.WriteGuidMask<1>(playerGuid);
            data.WriteGuidMask<3>(bgGuid);
            data.WriteGuidMask<6>(playerGuid);
            data.WriteGuidMask<5>(bgGuid);
            data.WriteBit(bg->isRated());              // Unk Bit 64
            data.WriteGuidMask<4>(playerGuid);
            data.WriteGuidMask<6, 4, 2>(bgGuid);
            data.WriteGuidMask<3, 0>(playerGuid);

            data.WriteGuidBytes<4, 5>(bgGuid);
            data.WriteGuidBytes<5>(playerGuid);
            data.WriteGuidBytes<1, 6, 3, 7>(bgGuid);
            data.WriteGuidBytes<6>(playerGuid);

            data << uint32(0);                         // Time
            data << uint8(0);                          // unk

            data.WriteGuidBytes<4, 1>(playerGuid);

            data << uint32(QueueSlot);                 // Queue slot
            //if (...)
            //    data << uint8(playerCount);          // player count (XvX) > 0 && rated: Rated XvX, !rated: Skirmish XvX. if ==0 >= Bg type
            //else
                data << uint8(arenatype);

            data << uint32(QueueSlot);                 // not queue slot
            data << uint32(bg->GetMapId());            // Map Id
            data << uint8(0);                          // Max Level? seen 85
            data << uint32(Time2);                     // Time since started

            data.WriteGuidBytes<2>(playerGuid);

            data << uint32(Time1);                     // Time until closed

            data.WriteGuidBytes<0, 3>(playerGuid);
            data.WriteGuidBytes<2>(bgGuid);

            data << uint32(bg->GetClientInstanceID()); // Client Instance ID

            data.WriteGuidBytes<0>(bgGuid);
            data.WriteGuidBytes<7>(playerGuid);
            break;
        }
        case STATUS_WAIT_LEAVE:
        {
            // not used currently and not checked
            data.Initialize(SMSG_BATTLEFIELD_STATUS_WAITFORGROUPS, 48);

            data << uint8(0);                          // unk
            data << uint32(QueueSlot);                 // not queueSlot
            data << uint32(QueueSlot);                 // Queue slot
            data << uint32(bg->GetEndTime());          // Time until closed
            data << uint32(0);                         // unk
            data << uint8(0);                          // unk
            data << uint8(0);                          // unk
            data << uint8(bg->GetMinLevel());          // Min Level
            data << uint8(0);                          // unk
            data << uint8(0);                          // unk
            data << uint32(bg->GetMapId());            // Map Id
            data << uint32(0);                         // Time
            data << uint8(0);                          // unk

            data.WriteGuidMask<0, 1, 7>(bgGuid);
            data.WriteGuidMask<7, 0>(playerGuid);
            data.WriteGuidMask<4>(bgGuid);
            data.WriteGuidMask<6, 2, 3>(playerGuid);
            data.WriteGuidMask<3>(bgGuid);
            data.WriteGuidMask<4>(playerGuid);
            data.WriteGuidMask<5>(bgGuid);
            data.WriteGuidMask<5>(playerGuid);
            data.WriteGuidMask<2>(bgGuid);
            data.WriteBit(bg->isRated());              // Is Rated
            data.WriteGuidMask<1>(playerGuid);
            data.WriteGuidMask<6>(bgGuid);

            data.WriteGuidBytes<0>(playerGuid);
            data.WriteGuidBytes<4>(bgGuid);
            data.WriteGuidBytes<3>(playerGuid);
            data.WriteGuidBytes<1, 0, 2>(bgGuid);
            data.WriteGuidBytes<2>(playerGuid);
            data.WriteGuidBytes<7>(bgGuid);
            data.WriteGuidBytes<1, 6>(playerGuid);
            data.WriteGuidBytes<6, 5>(bgGuid);
            data.WriteGuidBytes<5, 4, 7>(playerGuid);
            data.WriteGuidBytes<3>(bgGuid);
            break;
        }
        default:
            sLog.outError("Unknown BG status %u!", StatusID);
            break;
    }
}

/**
 * @brief Builds a PvP log data packet with player statistics.
 *
 * Constructs the network packet for MSG_PVP_LOG_DATA that contains the battleground
 * statistics for all players, including scores, kills, deaths, and battleground-specific
 * objective data. Indicates whether the battleground has finished.
 *
 * @param data The packet to initialize.
 * @param bg Pointer to the battleground instance.
 */
void BuildPvpLogDataPacket(WorldPacket& data, BattleGround* bg)
{
    ByteBuffer buffer;

    // last check on 4.3.4
    data.Initialize(SMSG_PVP_LOG_DATA, (1 + 1 + 4 + 40 * bg->GetPlayerScoresSize()));
    data.WriteBit(bg->isArena());
    data.WriteBit(bg->isRated());

    if (bg->isArena())
    {
        // it seems this must be according to BG_WINNER_A/H and _NOT_ BG_TEAM_A/H
        for (int8 i = 0; i < PVP_TEAM_COUNT; ++i)
        {
            if (ArenaTeam* at = sArenaTeamRegistry.Find(bg->GetArenaTeamIdByIndex(PvpTeamIndex(i))))
            {
                data.WriteBits(at->GetName().length(), 8);
            }
            else
            {
                data.WriteBits(0, 8);
            }
        }
    }

    data.WriteBits(bg->GetPlayerScoresSize(), 21);

    for (BattleGround::BattleGroundScoreMap::const_iterator itr = bg->GetPlayerScoresBegin(); itr != bg->GetPlayerScoresEnd(); ++itr)
    {
        ObjectGuid memberGuid = itr->first;
        Player* player = sPlayerRegistry.Find(itr->first);
        const BattleGroundScore* score = itr->second;

        data.WriteBit(0);                  // unk1
        data.WriteBit(0);                  // unk2
        data.WriteGuidMask<2>(memberGuid);
        data.WriteBit(!bg->isArena());
        data.WriteBit(0);                  // unk4
        data.WriteBit(0);                  // unk5
        data.WriteBit(0);                  // unk6
        data.WriteGuidMask<3, 0, 5, 1, 6>(memberGuid);
        Team team = bg->GetPlayerTeam(itr->first);
        if (!team && player)
        {
            team = player->GetTeam();
        }
        data.WriteBit(team == ALLIANCE);   // unk7
        data.WriteGuidMask<7>(memberGuid);

        buffer << uint32(score->HealingDone);         // healing done
        buffer << uint32(score->DamageDone);          // damage done

        if (!bg->isArena())
        {
            buffer << uint32(score->BonusHonor);
            buffer << uint32(score->Deaths);
            buffer << uint32(score->HonorableKills);
        }

        buffer.WriteGuidBytes<4>(memberGuid);
        buffer << uint32(score->KillingBlows);
        // if (unk5) << uint32() unk
        buffer.WriteGuidBytes<5>(memberGuid);
        // if (unk6) << uint32() unk
        // if (unk2) << uint32() unk
        buffer.WriteGuidBytes<1, 6>(memberGuid);
        // TODO: store this in player score
        if (player)
        {
            buffer << uint32(player->GetTalentMgr().PrimaryTree(player->GetTalentMgr().ActiveSpec()));
        }
        else
        {
            buffer << uint32(0);
        }

        switch (bg->GetTypeID())                            // battleground specific things
        {
            case BATTLEGROUND_AV:
                data.WriteBits(5, 24);                     // count of next fields
                buffer << uint32(((BattleGroundAVScore*)score)->GraveyardsAssaulted); // GraveyardsAssaulted
                buffer << uint32(((BattleGroundAVScore*)score)->GraveyardsDefended);  // GraveyardsDefended
                buffer << uint32(((BattleGroundAVScore*)score)->TowersAssaulted);     // TowersAssaulted
                buffer << uint32(((BattleGroundAVScore*)score)->TowersDefended);      // TowersDefended
                buffer << uint32(((BattleGroundAVScore*)score)->SecondaryObjectives); // SecondaryObjectives - free some of the Lieutnants
                break;
            case BATTLEGROUND_WS:
                data.WriteBits(2, 24);                     // count of next fields
                buffer << uint32(((BattleGroundWGScore*)score)->FlagCaptures);        // flag captures
                buffer << uint32(((BattleGroundWGScore*)score)->FlagReturns);         // flag returns
                break;
            case BATTLEGROUND_AB:
                data.WriteBits(2, 24);                     // count of next fields
                buffer << uint32(((BattleGroundABScore*)score)->BasesAssaulted);      // bases asssulted
                buffer << uint32(((BattleGroundABScore*)score)->BasesDefended);       // bases defended
                break;
            case BATTLEGROUND_EY:
                data.WriteBits(1, 24);                     // count of next fields
                buffer << uint32(((BattleGroundEYScore*)score)->FlagCaptures);        // flag captures
                break;
            case BATTLEGROUND_NA:
            case BATTLEGROUND_BE:
            case BATTLEGROUND_AA:
            case BATTLEGROUND_RL:
            case BATTLEGROUND_SA:                           // wotlk
            case BATTLEGROUND_DS:                           // wotlk
            case BATTLEGROUND_RV:                           // wotlk
            case BATTLEGROUND_IC:                           // wotlk
            case BATTLEGROUND_RB:                           // wotlk
                data.WriteBits(0, 24);                     // count of next fields
                break;
            default:
                sLog.outError("Unhandled SMSG_PVP_LOG_DATA for BG id %u", bg->GetTypeID());
                data.WriteBits(0, 24);                     // count of next fields
                break;
        }

        data.WriteGuidMask<4>(memberGuid);

        buffer.WriteGuidBytes<0, 3>(memberGuid);
        // if (unk4) << uint32() unk
        buffer.WriteGuidBytes<7, 2>(memberGuid);
    }

    data.WriteBit(bg->GetStatus() == STATUS_WAIT_LEAVE);   // If Ended

    if (bg->isRated())                                      // arena
    {
        for (int8 i = 0; i < PVP_TEAM_COUNT; ++i)
        {
            uint32 pointsLost = bg->GetArenaTeamRatingChangeByIndex(PvpTeamIndex(i)) < 0 ? abs(bg->GetArenaTeamRatingChangeByIndex(PvpTeamIndex(i))) : 0;
            uint32 pointsGained = bg->GetArenaTeamRatingChangeByIndex(PvpTeamIndex(i)) > 0 ? bg->GetArenaTeamRatingChangeByIndex(PvpTeamIndex(i)) : 0;

            data << uint32(0);                             // Matchmaking Value
            data << uint32(pointsLost);                    // Rating Lost
            data << uint32(pointsGained);                  // Rating gained
            DEBUG_LOG("rating change: %d", bg->GetArenaTeamRatingChangeByIndex(PvpTeamIndex(i)));
        }
    }

    data.FlushBits();
    data.append(buffer);

    if (bg->isArena())
    {
        for (int8 i = 0; i < PVP_TEAM_COUNT; ++i)
        {
            if (ArenaTeam* at = sArenaTeamRegistry.Find(bg->GetArenaTeamIdByIndex(PvpTeamIndex(i))))
            {
                data.append(at->GetName().data(), at->GetName().length());
            }
        }
    }

    data << uint8(bg->GetPlayersCountByTeam(HORDE));

    if (bg->GetStatus() == STATUS_WAIT_LEAVE)
    {
        data << uint8(bg->GetWinner() == ALLIANCE);        // who win
    }

    data << uint8(bg->GetPlayersCountByTeam(ALLIANCE));
}

/**
 * @brief Builds the group battleground join result packet.
 *
 * Writes the battleground join status code returned to grouped players after a
 * join request is processed.
 *
 * @param data The packet to initialize.
 * @param status The battleground group join status code.
 */
void BuildBattleGroundStatusFailedPacket(WorldPacket& data, BattleGround* bg, Player* player, uint8 QueueSlot, GroupJoinBattlegroundResult result)
{
    ObjectGuid bgGuid = bg->GetObjectGuid();
    ObjectGuid unkGuid2 = ObjectGuid();
    ObjectGuid playerGuid = player ? player->GetObjectGuid() : ObjectGuid(); // player who caused the error

    DEBUG_LOG("BuildBattleGroundStatusFailedPacket slot %u result %u bgstatus %u player %s bg %s",
        QueueSlot, result, bg->GetStatus(), playerGuid.GetString().c_str(), bgGuid.GetString().c_str());

    data.Initialize(SMSG_BATTLEFIELD_STATUS_FAILED);

    data.WriteGuidMask<3>(bgGuid);
    data.WriteGuidMask<3>(playerGuid);
    data.WriteGuidMask<3>(unkGuid2);
    data.WriteGuidMask<0>(playerGuid);
    data.WriteGuidMask<6>(bgGuid);
    data.WriteGuidMask<5, 6, 4, 2>(unkGuid2);

    data.WriteGuidMask<1>(playerGuid);
    data.WriteGuidMask<1>(bgGuid);
    data.WriteGuidMask<5, 6>(playerGuid);
    data.WriteGuidMask<1>(unkGuid2);
    data.WriteGuidMask<7>(bgGuid);
    data.WriteGuidMask<4>(playerGuid);

    data.WriteGuidMask<2, 5>(bgGuid);
    data.WriteGuidMask<7>(playerGuid);
    data.WriteGuidMask<4, 0>(bgGuid);
    data.WriteGuidMask<0>(unkGuid2);
    data.WriteGuidMask<2>(playerGuid);
    data.WriteGuidMask<7>(unkGuid2);

    data.WriteGuidBytes<1>(bgGuid);

    data << uint32(QueueSlot);         // not queue slot
    data << uint32(QueueSlot);         // Queue slot

    data.WriteGuidBytes<6, 3, 7, 4>(unkGuid2);
    data.WriteGuidBytes<0>(bgGuid);
    data.WriteGuidBytes<5>(unkGuid2);
    data.WriteGuidBytes<7, 6, 2>(bgGuid);
    data.WriteGuidBytes<6, 3>(playerGuid);
    data.WriteGuidBytes<1>(unkGuid2);
    data.WriteGuidBytes<3>(bgGuid);
    data.WriteGuidBytes<0, 1, 4>(playerGuid);
    data.WriteGuidBytes<0>(unkGuid2);
    data.WriteGuidBytes<5>(bgGuid);
    data.WriteGuidBytes<7>(playerGuid);
    data.WriteGuidBytes<4>(bgGuid);
    data.WriteGuidBytes<2>(unkGuid2);

    data << uint32(result);            // Result

    data.WriteGuidBytes<2>(playerGuid);

    data << uint32(0);                 // unk Time

    data.WriteGuidBytes<5>(playerGuid);
}

/**
 * @brief Builds a packet for when a player leaves a battleground.
 *
 * Constructs the SMSG_BATTLEGROUND_PLAYER_LEFT packet that notifies other players
 * about a player leaving the battleground.
 *
 * @param data The packet to initialize.
 * @param guid The GUID of the player who left.
 */
void BuildPlayerLeftBattleGroundPacket(WorldPacket& data, ObjectGuid guid)
{
    data.Initialize(SMSG_BATTLEGROUND_PLAYER_LEFT, 8);
    data.WriteGuidMask<7, 6, 2, 4, 5, 1, 3, 0>(guid);
    data.WriteGuidBytes<4, 2, 5, 7, 0, 6, 1, 3>(guid);
}

/**
 * @brief Builds a packet for when a player joins a battleground.
 *
 * Constructs the SMSG_BATTLEGROUND_PLAYER_JOINED packet that notifies other players
 * about a new player joining the battleground.
 *
 * @param data The packet to initialize.
 * @param plr Pointer to the player who joined.
 */
void BuildPlayerJoinedBattleGroundPacket(WorldPacket& data, Player* plr)
{
    data.Initialize(SMSG_BATTLEGROUND_PLAYER_JOINED, 8);
    data.WriteGuidMask<0, 4, 3, 5, 7, 6, 2, 1>(plr->GetObjectGuid());
    data.WriteGuidBytes<1, 5, 3, 2, 0, 7, 4, 6>(plr->GetObjectGuid());
}

/**
 * @brief Builds the battleground instance list packet for a player.
 *
 * Enumerates the client-visible battleground instances available for the player's
 * bracket and writes them into the battlefield list response.
 *
 * @param data The packet to initialize.
 * @param guid The battlemaster GUID associated with the request.
 * @param plr The player receiving the list.
 * @param bgTypeId The battleground type being listed.
 */
void BuildBattleGroundListPacket(WorldPacket& data, ObjectGuid guid, Player* plr, BattleGroundTypeId bgTypeId)
{
    if (!plr)
    {
        return;
    }

    BattleGround* bgTemplate = sBattleGroundMgr.GetBattleGroundTemplate(bgTypeId);

    data.Initialize(SMSG_BATTLEFIELD_LIST);
    data << uint32(0);                     // 4.3.4 winConquest weekend
    data << uint32(0);                     // 4.3.4 winConquest random
    data << uint32(0);                     // 4.3.4 lossHonor weekend
    data << uint32(bgTypeId);              // battleground id
    data << uint32(0);                     // 4.3.4 lossHonor random
    data << uint32(0);                     // 4.3.4 winHonor random
    data << uint32(0);                     // 4.3.4 winHonor weekend
    data << uint8(bgTemplate->GetMaxLevel()); // max level
    data << uint8(bgTemplate->GetMinLevel()); // min level
    data.WriteGuidMask<0, 1, 7>(guid);
    data.WriteBit(false);                  // has holiday bg currency bonus ??
    data.WriteBit(false);                  // has random bg currency bonus ??

    uint32 count = 0;
    ByteBuffer buf;
    if (bgTemplate)
    {
        // expected bracket entry
        if (PvPDifficultyEntry const* bracketEntry = GetBattlegroundBracketByLevel(bgTemplate->GetMapId(), plr->getLevel()))
        {
            BattleGroundBracketId bracketId = bracketEntry->GetBracketId();
            std::set<uint32> const& ids = sBattleGroundMgr.GetClientVisibleInstanceIds(bgTypeId, bracketId);
            for (std::set<uint32>::const_iterator itr = ids.begin(); itr != ids.end(); ++itr)
            {
                buf << uint32(*itr);
                ++count;
            }
        }
    }

    data.WriteBits(count, 24);
    data.WriteGuidMask<6, 4, 2, 3>(guid);
    data.WriteBit(false);                   // unk
    data.WriteGuidMask<5>(guid);
    data.WriteBit(true);                   // hide battleground list window

    data.WriteGuidBytes<6, 1, 7, 5>(guid);
    data.FlushBits();
    if (count)
    {
        data.append(buf);
    }
    data.WriteGuidBytes<0, 2, 4, 3>(guid);
}

void BuildStartTimerPacket(WorldPacket& data, uint32 timeLeft, uint32 totalTime)
{
    data.Initialize(SMSG_START_TIMER, 4 + 4 + 4);
    data << uint32(0);
    data << uint32(timeLeft);
    data << uint32(totalTime);
}

void SessionBattleGroundNotices::Status(Player& player, BattleGround* bg, uint8 queueSlot, uint8 statusId, uint32 time1, uint32 time2, ArenaType arenaType)
{
    WorldPacket data;
    BuildBattleGroundStatusPacket(data, bg, &player, queueSlot, statusId, time1, time2, arenaType);
    player.GetSession()->SendPacket(&data);
}

void SessionBattleGroundNotices::PvpLog(Player& player, BattleGround& bg)
{
    WorldPacket data;
    BuildPvpLogDataPacket(data, &bg);
    player.GetSession()->SendPacket(&data);
}

void SessionBattleGroundNotices::StartTimer(Player& player, uint32 timeLeft, uint32 totalTime)
{
    WorldPacket data;
    BuildStartTimerPacket(data, timeLeft, totalTime);
    player.GetSession()->SendPacket(&data);
}

void SessionBattleGroundNotices::SoundToAll(BattleGround& bg, uint32 soundId)
{
    WorldPacket data;
    BuildPlaySoundPacket(data, soundId);
    bg.SendPacketToAll(&data);
}

void SessionBattleGroundNotices::Sound(Player& player, uint32 soundId)
{
    WorldPacket data;
    BuildPlaySoundPacket(data, soundId);
    player.GetSession()->SendPacket(&data);
}

void SessionBattleGroundNotices::PlayerJoined(BattleGround& bg, Team team, Player& player)
{
    WorldPacket data;
    BuildPlayerJoinedBattleGroundPacket(data, &player);
    bg.SendPacketToTeam(team, &data, &player, false);
}

void SessionBattleGroundNotices::PlayerLeft(BattleGround& bg, Team team, ObjectGuid guid, Player* sender)
{
    WorldPacket data;
    BuildPlayerLeftBattleGroundPacket(data, guid);
    bg.SendPacketToTeam(team, &data, sender, false);
}

void SessionBattleGroundNotices::WorldStateToAll(BattleGround& bg, uint32 state, uint32 value)
{
    WorldStateUpdateFact fact;
    fact.state = state;
    fact.value = value;
    WorldPacket data;
    BuildUpdateWorldStatePacket(data, fact);
    bg.SendPacketToAll(&data);
}
