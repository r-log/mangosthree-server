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

#ifndef MANGOS_H_BATTLEGROUNDPACKETS
#define MANGOS_H_BATTLEGROUNDPACKETS

#include "BattleGroundMgr.h"

class WorldPacket;

/// Initializes `data` as `player`'s status in its queue slot `QueueSlot`: cleared, queued, invited,
/// in progress or waiting to leave, by `StatusID`.
void BuildBattleGroundStatusPacket(WorldPacket& data, BattleGround* bg, Player* player, uint8 QueueSlot, uint8 StatusID, uint32 Time1, uint32 Time2, ArenaType arenatype);
/// Initializes `data` as the score board of `bg`.
void BuildPvpLogDataPacket(WorldPacket& data, BattleGround* bg);
/// Initializes `data` as the refusal `result` of a join to `bg`, caused by `player`.
void BuildBattleGroundStatusFailedPacket(WorldPacket& data, BattleGround* bg, Player* player, uint8 QueueSlot, GroupJoinBattlegroundResult result);
/// Initializes `data` as the notice that the player `guid` left the battleground.
void BuildPlayerLeftBattleGroundPacket(WorldPacket& data, ObjectGuid guid);
/// Initializes `data` as the notice that `plr` joined the battleground.
void BuildPlayerJoinedBattleGroundPacket(WorldPacket& data, Player* plr);
/// Initializes `data` as the instances of `bgTypeId` the battlemaster `guid` lists for `plr`.
void BuildBattleGroundListPacket(WorldPacket& data, ObjectGuid guid, Player* plr, BattleGroundTypeId bgTypeId);
/// Initializes `data` as the countdown to a battleground's start: 0, `timeLeft`, then `totalTime`.
void BuildStartTimerPacket(WorldPacket& data, uint32 timeLeft, uint32 totalTime);

/// The battlegrounds' notices: a player's own are sent to its client, a battleground's audience
/// through the battleground's own sends.
class SessionBattleGroundNotices : public BattleGroundNotices
{
    public:
        void Status(Player& player, BattleGround* bg, uint8 queueSlot, uint8 statusId, uint32 time1, uint32 time2, ArenaType arenaType) override;
        void PvpLog(Player& player, BattleGround& bg) override;
        void StartTimer(Player& player, uint32 timeLeft, uint32 totalTime) override;
        void SoundToAll(BattleGround& bg, uint32 soundId) override;
        void Sound(Player& player, uint32 soundId) override;
        void PlayerJoined(BattleGround& bg, Team team, Player& player) override;
        void PlayerLeft(BattleGround& bg, Team team, ObjectGuid guid, Player* sender) override;
        void WorldStateToAll(BattleGround& bg, uint32 state, uint32 value) override;
        void MessageToAll(BattleGround& bg, ChatMsg msgtype, int32 textId, Player const* source, va_list* args) override;
        void YellToAll(BattleGround& bg, int32 textId, Language language, Creature const& source) override;
        void Message2ToAll(BattleGround& bg, ChatMsg msgtype, int32 textId, Player const* source, int32 arg1, int32 arg2) override;
        void Yell2ToAll(BattleGround& bg, int32 textId, uint32 language, Creature const& source, int32 arg1, int32 arg2) override;
};

#endif
