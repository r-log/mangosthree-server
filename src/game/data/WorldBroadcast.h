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

#ifndef MANGOS_H_WORLDBROADCAST
#define MANGOS_H_WORLDBROADCAST

#include "ObjectGuid.h"
#include "Platform/Define.h"
#include "SharedDefines.h"

#include <cstdarg>

/// World-thread work raised inside the map phase, applied after the barrier.
struct AfterMapPhasePost
{
    enum Kind { DEFENSE_MESSAGE, ZONE_UNDER_ATTACK, GRAVEYARD_LINK_TEAM };

    Kind kind;
    uint32 zoneId;
    int32 textId;
    Team team;
    uint32 graveyardId;
};

/**
 * What the code below the world tells every client in it: a system text, a zone under attack, a world
 * defense message, a realm's first achievement and a world state; and the work the map phase leaves
 * for the world thread. The world installs itself at construction; the code calls the installed one
 * through sWorldBroadcast. This base, installed when nothing else is, tells nobody and posts nothing.
 */
class WorldBroadcast
{
    public:
        virtual ~WorldBroadcast();

        /// Sends the mangos string `stringId`, formatted with `args`, to every player in the world.
        virtual void WorldTextV(int32 stringId, va_list* args);
        /// Sends the mangos string `stringId`, formatted with the arguments that follow, to every
        /// player in the world.
        void WorldText(int32 stringId, ...);
        /// Tells the players of `team` outside an instance that `zoneId` is under attack.
        virtual void ZoneUnderAttack(uint32 zoneId, Team team);
        /// Sends the world defense message `textId` about `zoneId` to every player outside an instance.
        virtual void DefenseMessage(uint32 zoneId, int32 textId);
        /// Queues `post` for the world thread while the map phase runs. False outside it: the caller
        /// does the work itself.
        virtual bool PostAfterMapPhase(AfterMapPhasePost const& post);
        /// Tells every client that the player `name` (`guid`) is the realm's first to earn `achievementId`.
        virtual void RealmFirstAchievement(char const* name, ObjectGuid guid, uint32 achievementId);
        /// Tells every client that the world state `field` is now `value`.
        virtual void WorldStateChanged(uint32 field, uint32 value);

        /// The installed broadcast, or the base when none is installed.
        static WorldBroadcast& Installed();
        /// Installs `broadcast` and answers the one it replaces (NULL for the base). NULL installs
        /// the base.
        static WorldBroadcast* Install(WorldBroadcast* broadcast);
        /// Whether a broadcast is installed.
        static bool IsInstalled();
};

#define sWorldBroadcast WorldBroadcast::Installed()

#endif
