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

#ifndef MANGOS_H_SPELLEFFECTPACKETS
#define MANGOS_H_SPELLEFFECTPACKETS

#include "PlayerClientFacts.h"
#include "spells/pipeline/SpellEffectNotices.h"

class WorldPacket;

/// Initializes `packet` as the spirit healer's confirmation request: the healer's guid.
void BuildSpiritHealerConfirmPacket(WorldPacket& packet, SpiritHealerConfirmFact const& fact);
/// Initializes `packet` as a summon request: the summoner's guid, its zone, then the time the
/// client waits before it declines.
void BuildSummonRequestPacket(WorldPacket& packet, SummonRequestFact const& fact);
/// Initializes `packet` as the outright kill of `victim` by the spell `spellId`: the caster's guid
/// (empty for an object's cast), the victim's, then the spell.
void BuildInstaKillLogPacket(WorldPacket& packet, ObjectGuid caster, ObjectGuid victim, uint32 spellId);
/// Initializes `packet` as the dispel log of the spell `spellId`, the steal log when `steal` is true:
/// the victim's and the caster's packed guids, the spell, a zero byte, the count, then each spell
/// taken with a zero byte.
void BuildDispelLogPacket(WorldPacket& packet, bool steal, ObjectGuid victim, ObjectGuid caster, uint32 spellId, std::vector<uint32> const& spellIds);
/// Initializes `packet` as the dispel the spell `spellId` failed: the caster's guid, the victim's,
/// the spell, then each spell it failed to dispel.
void BuildDispelFailedPacket(WorldPacket& packet, ObjectGuid caster, ObjectGuid victim, uint32 spellId, std::vector<uint32> const& spellIds);
/// Initializes `packet` as the order to clear `target` as a target: its guid.
void BuildClearTargetPacket(WorldPacket& packet, ObjectGuid target);
/// Initializes `packet` as a duel request: the flag's guid, then the challenger's.
void BuildDuelRequestedPacket(WorldPacket& packet, ObjectGuid flag, ObjectGuid challenger);
/// Initializes `packet` as a totem created: its slot, its guid, its duration in milliseconds, then
/// the spell that summoned it.
void BuildTotemCreatedPacket(WorldPacket& packet, uint8 slot, ObjectGuid totem, uint32 duration, uint32 spellId);
/// Initializes `packet` as a new taxi path learned; no payload.
void BuildNewTaxiPathPacket(WorldPacket& packet);
/// Initializes `packet` as the status of the flight master `source`'s node: its guid, then the status.
void BuildTaxiNodeStatusPacket(WorldPacket& packet, ObjectGuid source, uint8 status);
/// Initializes `packet` as the player's bind point: `home`'s coordinates, its map, then `areaId`.
void BuildBindPointUpdatePacket(WorldPacket& packet, WorldLocation const& home, uint32 areaId);
/// Initializes `packet` as the player bound by `binder` in the area `areaId`: the binder's guid,
/// then the area.
void BuildPlayerBoundPacket(WorldPacket& packet, ObjectGuid binder, uint32 areaId);

/// The spell effects' notices, each built here: the logs go to the units around the caster, the
/// cleared target to the units around the target, the rest to the player's own client.
class SessionSpellEffectNotices : public SpellEffectNotices
{
    public:
        void InstaKillLog(Spell const& spell, ObjectGuid casterGuid, Unit const& victim) override;
        void DispelLog(Spell const& spell, Unit const& victim, std::vector<uint32> const& spellIds) override;
        void DispelFailed(Spell const& spell, Unit const& victim, std::vector<uint32> const& spellIds) override;
        void StealLog(Spell const& spell, Unit const& victim, std::vector<uint32> const& spellIds) override;
        void ClearTarget(Unit const& target) override;
        void DuelRequested(Player& challenger, Player& challenged, ObjectGuid flag) override;
        void TotemCreated(Player& owner, uint8 slot, ObjectGuid totem, uint32 duration, uint32 spellId) override;
        void TaxiNodeLearned(Player& player, ObjectGuid source) override;
        void HomeBound(Player& player, ObjectGuid binder, WorldLocation const& home, uint32 areaId) override;
};

#endif
