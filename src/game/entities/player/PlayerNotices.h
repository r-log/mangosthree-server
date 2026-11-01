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

#ifndef MANGOS_H_PLAYERNOTICES
#define MANGOS_H_PLAYERNOTICES

#include "Player.h"

#include <array>
#include <ctime>
#include <string>

class Transport;

/**
 * What a player tells its own client about itself, one call per notice: its transfers between maps,
 * its experience and levels, its cinematics, its login state, its titles and its charters. The player
 * keeps when each is told. The session installs the notices that send them at start-up; this base
 * sends nothing.
 */
class PlayerNotices
{
    public:
        virtual ~PlayerNotices();

        /// `player` is shown the loading screen to the map `mapId`, leaving on `transport` when it has one.
        virtual void TransferPending(Player const& player, uint32 mapId, Transport const* transport);
        /// `player` is told to load the map `mapId` at `x`, `y`, `z` facing `o`.
        virtual void NewWorld(Player const& player, uint32 mapId, float x, float y, float z, float o);
        /// `player` is told it gained `givenXp` and `restXp` experience, for killing `victim` when it is not
        /// empty.
        virtual void XpGainLogged(Player const& player, uint32 givenXp, uint32 restXp, ObjectGuid victim);
        /// `player` is told it reached `level`, with what it gained in health, mana and each stat.
        virtual void LevelUpInfoShown(Player const& player, uint32 level, uint32 healthGain, uint32 manaGain, std::array<uint32, MAX_STATS> const& statGains);
        /// `player` plays the cinematic sequence `sequenceId`.
        virtual void CinematicStarted(Player const& player, uint32 sequenceId);
        /// `player` plays the movie `movieId`.
        virtual void MovieStarted(Player const& player, uint32 movieId);
        /// `player` is told `name` declined its guild invitation, automatically when `autoDecline`.
        virtual void GuildInviteDeclined(Player const& player, std::string const& name, bool autoDecline);
        /// `player` is asked to confirm binding its home at `binder`.
        virtual void BinderConfirmAsked(Player const& player, ObjectGuid binder);
        /// `player` is told it explored the area `areaId` for `xp` experience.
        virtual void ExplorationExperienceShown(Player const& player, uint32 areaId, uint32 xp);
        /// `player` is told its home is at `x`, `y`, `z` on the map `mapId`, in the area `areaId`.
        virtual void BindPointShown(Player const& player, float x, float y, float z, uint32 mapId, uint32 areaId);
        /// `player` is told the realm's last weekly reset and its map's `difficulty`.
        virtual void WorldServerInfoShown(Player const& player, uint32 lastWeeklyReset, uint32 difficulty);
        /// `player` is told the spells it must unlearn: none.
        virtual void UnlearnSpellsShown(Player const& player);
        /// `player` is told the game time `now` and the game speed.
        virtual void LoginTimeSpeedShown(Player const& player, time_t now);
        /// `player` is told it earned the title `maskId`, or lost it when `lost`.
        virtual void TitleEarned(Player const& player, uint32 maskId, bool lost);
        /// `player` is told the result `result` of `signer` signing the charter `petition`.
        virtual void PetitionSignResultShown(Player const& player, ObjectGuid petition, ObjectGuid signer, uint32 result);
        /// `player` is told the result `result` of turning in its charter.
        virtual void PetitionTurnInResultShown(Player const& player, uint32 result);

        /// The installed notices, or the base when none are installed.
        static PlayerNotices& Installed();
        /// Installs `notices` and answers the ones it replaces (NULL for the base). NULL installs the
        /// base. Installed once at start-up, before the world loads.
        static PlayerNotices* Install(PlayerNotices* notices);
};

#define sPlayerNotices PlayerNotices::Installed()

#endif
