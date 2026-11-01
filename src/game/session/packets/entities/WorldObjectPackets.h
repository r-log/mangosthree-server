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

#ifndef MANGOS_H_WORLDOBJECTPACKETS
#define MANGOS_H_WORLDOBJECTPACKETS

#include "entities/WorldObjectNotices.h"

/// A world object's notices, each built here: to the one player a notice names, to the units around
/// the object otherwise.
class SessionWorldObjectNotices : public WorldObjectNotices
{
    public:
        void ObjectDespawnAnim(WorldObject const& sender, ObjectGuid guid) override;
        void GameObjectCustomAnim(WorldObject const& sender, ObjectGuid guid, uint32 animId) override;
        void DistanceSoundPlayed(WorldObject const& source, uint32 soundId, Player const* target) override;
        void SoundPlayed(WorldObject const& source, uint32 soundId, Player const* target) override;
        void MusicPlayed(WorldObject const& source, uint32 soundId, Player const* target) override;
        void Emoted(Unit const& unit, uint32 emoteId) override;
        void PowerUpdated(Unit const& unit, Powers power, uint32 value) override;
        void Energized(Unit const& caster, Unit const& victim, uint32 spellId, uint32 amount, Powers power) override;
        void Dismounted(Unit const& unit) override;
        void OneShotAnimKitPlayed(Unit const& unit, uint32 id) override;
        void VehicleRecIdChanged(Unit const& unit, uint32 entry) override;
        void AiReacted(Creature const& creature, AiReaction reaction) override;
        void BuildingDamaged(GameObject const& building, Unit& caster, uint32 damage, uint32 spell) override;
        void AreaSpiritHealerTime(Player const& player, ObjectGuid healer, uint32 nextResurrectMs) override;
        void GameObjectPageShown(Player const& player, ObjectGuid object) override;
        void FishEscaped(Player const& player) override;
        void BarberShopEnabled(Player const& player) override;
        void ItemTimeUpdated(Player const& owner, ObjectGuid item, uint32 durationLeft) override;
        void ItemSocketsUpdated(Player const& owner, Item const& item) override;
        void ObjectDestroyed(Player const& target, ObjectGuid object, bool anim) override;
        void SpellCooldownsTold(Player const& player, ObjectGuid unit, uint8 flags, std::vector<SpellCooldownTime> const& cooldowns) override;
        void ExpectedRideVehicleAuraCancelled(Player const& passenger) override;
};

#endif
