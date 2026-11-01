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

#ifndef MANGOS_H_GROUPPACKETS
#define MANGOS_H_GROUPPACKETS

#include "social/GroupNotices.h"

class WorldPacket;

/// Initializes `packet` as the placed raid markers: their mask.
void BuildRaidMarkersChangedPacket(WorldPacket& packet, uint32 markerMask);
/// Initializes `packet` as the group's new leader: the leader's name.
void BuildGroupSetLeaderPacket(WorldPacket& packet, std::string const& name);
/// Initializes `packet` as the notice that the client was removed from its group: no payload.
void BuildGroupUninvitePacket(WorldPacket& packet);
/// Initializes `packet` as the notice that the client's group is disbanded: no payload.
void BuildGroupDestroyedPacket(WorldPacket& packet);
/// Initializes `packet` as the roster of no group: 0x10, three zero bytes, then zeros for the group
/// guid, the roster version, the member count and the leader guid.
void BuildEmptyGroupListPacket(WorldPacket& packet);
/// Initializes `packet` as `slot`'s roster of `group`: the group's type, the slot's subgroup, flags and
/// role, the dungeon finder state and dungeon for a dungeon finder group, the group guid, the roster
/// version `sequence`, then every other member's name, guid, online state, subgroup, flags and role,
/// then the leader and, with any other member, the loot settings and the difficulties.
void BuildGroupListPacket(WorldPacket& packet, Group const& group, Group::MemberSlot const& slot, uint8 lfgState, uint32 lfgDungeon, uint32 sequence);
/// Initializes `packet` as the start of the loot roll `roll`: the looted object, the map, the item's
/// slot, entry, random suffix and property and count, the countdown, the choices `mask` and the
/// number of rollers.
void BuildLootStartRollPacket(WorldPacket& packet, Roll const& roll, uint32 countDown, uint32 mapId, RollVoteMask mask);
/// Initializes `packet` as the player `target`'s roll `rollNumber` of the type `rollType` on `roll`.
void BuildLootRollPacket(WorldPacket& packet, Roll const& roll, ObjectGuid target, uint32 rollNumber, uint8 rollType);
/// Initializes `packet` as the player `target`'s win of `roll` with `rollNumber` of the type `rollType`.
void BuildLootRollWonPacket(WorldPacket& packet, Roll const& roll, ObjectGuid target, uint32 rollNumber, RollVote rollType);
/// Initializes `packet` as every roller of `roll` passing on it.
void BuildLootAllPassedPacket(WorldPacket& packet, Roll const& roll);
/// Initializes `packet` as the master looter's candidates: their count, then each guid.
void BuildMasterLootListPacket(WorldPacket& packet, std::vector<ObjectGuid> const& candidates);
/// Initializes `packet` as `who`'s target icon `id` set on `target`: 0, `who`, the icon, then `target`.
void BuildTargetIconSetPacket(WorldPacket& packet, ObjectGuid who, uint8 id, ObjectGuid target);
/// Initializes `packet` as `group`'s target icons: 1, then each set icon and its target.
void BuildTargetIconListPacket(WorldPacket& packet, Group const& group);
/// Initializes `packet` as the stats of `player` its group update mask names: the packed guid, the
/// mask, then each named field.
void BuildPartyMemberStatsChangedPacket(WorldPacket& packet, Player& player);
/// Initializes `packet` as the ready check answer `ready` of the member `member`.
void BuildReadyCheckConfirmPacket(WorldPacket& packet, ObjectGuid member, uint8 ready);

/// The group's notices, each built here: a player's own goes to its client, the group's to its
/// members through the group's own sends.
class SessionGroupNotices : public GroupNotices
{
    public:
        void RaidMarkersChanged(Group& group) override;
        void LeaderChanged(Group& group, std::string const& name) override;
        void Uninvited(Player& player) override;
        void Destroyed(Player& player) override;
        void NoGroup(Player& player) override;
        void LootStartRoll(Player& player, Roll const& roll, uint32 countDown, uint32 mapId, RollVoteMask mask) override;
        void LootRoll(Group& group, Roll const& roll, ObjectGuid target, uint32 rollNumber, uint8 rollType) override;
        void LootRollWon(Group& group, Roll const& roll, ObjectGuid target, uint32 rollNumber, RollVote rollType) override;
        void LootAllPassed(Group& group, Roll const& roll) override;
        void MasterLootList(std::vector<ObjectGuid> const& candidates, std::vector<Player*> const& recipients) override;
        void TargetIconSet(Group& group, ObjectGuid who, uint8 id, ObjectGuid target) override;
        void GroupList(Group const& group, Player& player, Group::MemberSlot const& slot, uint8 lfgState, uint32 lfgDungeon, uint32 sequence) override;
        void PartyMemberStatsChanged(Group& group, Player& member) override;
        void ReadyCheckOffline(Group& group, ObjectGuid member) override;
};

#endif
