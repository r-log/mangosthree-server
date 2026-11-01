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

#ifndef MANGOS_H_PLAYERCOMBATPACKETS
#define MANGOS_H_PLAYERCOMBATPACKETS

#include "entities/player/combat/PlayerCombatNotices.h"

class WorldPacket;

/// Initializes `packet` as the spirit healer's place on the minimap: the map, then the coordinates;
/// the map 0xFFFFFFFF and zero coordinates clear it.
void BuildDeathReleaseLocPacket(WorldPacket& packet, uint32 mapId, float x, float y, float z);

/// A player's combat, death and duel notices, each built here: to the player's own client, a duel's
/// end to both duellists and its winner to the units around the player.
class SessionPlayerCombatNotices : public PlayerCombatNotices
{
    public:
        void SwingDeadTarget(Player const& player) override;
        void SwingCantAttack(Player const& player) override;
        void ComboPointsUpdated(Player const& player, Unit const& target, uint8 points) override;
        void PreResurrect(Player const& player) override;
        void SpiritHealerLocCleared(Player const& player) override;
        void SpiritHealerLocShown(Player const& player, uint32 mapId, float x, float y, float z) override;
        void CorpseReclaimDelay(Player const& player, uint32 delayMs) override;
        void DuelOutOfBounds(Player const& player) override;
        void DuelInBounds(Player const& player) override;
        void DuelCompleted(Player const& player, Player const& opponent, DuelCompleteType type) override;
        void DuelCountdown(Player const& player, uint32 seconds) override;
};

#endif
