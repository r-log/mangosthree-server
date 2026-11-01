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

#ifndef MANGOS_H_GROUPNOTICES
#define MANGOS_H_GROUPNOTICES

#include "Group.h"

#include <string>
#include <vector>

class Player;

/**
 * What a group tells its members' clients, one call per notice; who is told is the group's own send
 * to its members. The session installs the notices that send them at start-up; this base sends
 * nothing.
 */
class GroupNotices
{
    public:
        virtual ~GroupNotices();

        /// The members of `group` are told which raid markers are placed.
        virtual void RaidMarkersChanged(Group& group);
        /// The members of `group` are told its leader is now the member named `name`.
        virtual void LeaderChanged(Group& group, std::string const& name);
        /// `player` is told it was removed from its group.
        virtual void Uninvited(Player& player);
        /// `player` is told its group is disbanded.
        virtual void Destroyed(Player& player);
        /// `player` is told it is in no group.
        virtual void NoGroup(Player& player);
        /// `player` is told the loot roll `roll` starts, on the map `mapId`, with `countDown`
        /// milliseconds to choose among the choices `mask`.
        virtual void LootStartRoll(Player& player, Roll const& roll, uint32 countDown, uint32 mapId, RollVoteMask mask);
        /// The voters of `roll` in `group` are told the player `target` rolled `rollNumber` as `rollType`.
        virtual void LootRoll(Group& group, Roll const& roll, ObjectGuid target, uint32 rollNumber, uint8 rollType);
        /// The voters of `roll` in `group` are told the player `target` won it with `rollNumber` as `rollType`.
        virtual void LootRollWon(Group& group, Roll const& roll, ObjectGuid target, uint32 rollNumber, RollVote rollType);
        /// The voters of `roll` in `group` are told every one of them passed.
        virtual void LootAllPassed(Group& group, Roll const& roll);
        /// Each of `recipients` is told the master looter may give the loot to `candidates`.
        virtual void MasterLootList(std::vector<ObjectGuid> const& candidates, std::vector<Player*> const& recipients);
        /// The members of `group` are told `who` set the target icon `id` on `target`.
        virtual void TargetIconSet(Group& group, ObjectGuid who, uint8 id, ObjectGuid target);
        /// `player`, the member `slot` of `group`, is told the group's roster, with its own dungeon
        /// finder state `lfgState` and dungeon `lfgDungeon` and the roster version `sequence`.
        virtual void GroupList(Group const& group, Player& player, Group::MemberSlot const& slot, uint8 lfgState, uint32 lfgDungeon, uint32 sequence);
        /// The members of `group` that do not see `member` are told its changed stats.
        virtual void PartyMemberStatsChanged(Group& group, Player& member);
        /// The members of `group` are told the offline member `member` is not ready.
        virtual void ReadyCheckOffline(Group& group, ObjectGuid member);

        /// The installed notices, or the base when none are installed.
        static GroupNotices& Installed();
        /// Installs `notices` and answers the ones it replaces (NULL for the base). NULL installs the
        /// base. Installed once at start-up, before the world loads.
        static GroupNotices* Install(GroupNotices* notices);
};

#define sGroupNotices GroupNotices::Installed()

#endif
