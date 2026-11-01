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

#ifndef MANGOS_H_PLAYERPACKETS
#define MANGOS_H_PLAYERPACKETS

#include "entities/player/PlayerNotices.h"

class WorldPacket;

/// Initializes `packet` as the result `result` of `signer` signing the charter `petition`: the
/// charter's guid, the signer's, then the result.
void BuildPetitionSignResultPacket(WorldPacket& packet, ObjectGuid petition, ObjectGuid signer, uint32 result);

/// A player's own notices, each built here and sent to the player's own client.
class SessionPlayerNotices : public PlayerNotices
{
    public:
        void TransferPending(Player const& player, uint32 mapId, Transport const* transport) override;
        void NewWorld(Player const& player, uint32 mapId, float x, float y, float z, float o) override;
        void XpGainLogged(Player const& player, uint32 givenXp, uint32 restXp, ObjectGuid victim) override;
        void LevelUpInfoShown(Player const& player, uint32 level, uint32 healthGain, uint32 manaGain, std::array<uint32, MAX_STATS> const& statGains) override;
        void CinematicStarted(Player const& player, uint32 sequenceId) override;
        void MovieStarted(Player const& player, uint32 movieId) override;
        void GuildInviteDeclined(Player const& player, std::string const& name, bool autoDecline) override;
        void BinderConfirmAsked(Player const& player, ObjectGuid binder) override;
        void ExplorationExperienceShown(Player const& player, uint32 areaId, uint32 xp) override;
        void BindPointShown(Player const& player, float x, float y, float z, uint32 mapId, uint32 areaId) override;
        void WorldServerInfoShown(Player const& player, uint32 lastWeeklyReset, uint32 difficulty) override;
        void UnlearnSpellsShown(Player const& player) override;
        void LoginTimeSpeedShown(Player const& player, time_t now) override;
        void TitleEarned(Player const& player, uint32 maskId, bool lost) override;
        void PetitionSignResultShown(Player const& player, ObjectGuid petition, ObjectGuid signer, uint32 result) override;
        void PetitionTurnInResultShown(Player const& player, uint32 result) override;
};

#endif
