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

#include "SpellEffectPackets.h"
#include "Object.h"
#include "Opcodes.h"
#include "Player.h"
#include "Unit.h"
#include "WorldPacket.h"
#include "spells/pipeline/Spell.h"

void BuildSpiritHealerConfirmPacket(WorldPacket& packet, SpiritHealerConfirmFact const& fact)
{
    packet.Initialize(SMSG_SPIRIT_HEALER_CONFIRM, 8);
    packet << ObjectGuid(fact.healer);
}

void BuildSummonRequestPacket(WorldPacket& packet, SummonRequestFact const& fact)
{
    packet.Initialize(SMSG_SUMMON_REQUEST, 8 + 4 + 4);
    packet << ObjectGuid(fact.summoner);
    packet << uint32(fact.zone);
    packet << uint32(fact.autoDeclineMs);
}

void BuildInstaKillLogPacket(WorldPacket& packet, ObjectGuid caster, ObjectGuid victim, uint32 spellId)
{
    packet.Initialize(SMSG_SPELLINSTAKILLLOG, 8 + 8 + 4);
    packet << caster;
    packet << victim;
    packet << uint32(spellId);
}

void BuildDispelLogPacket(WorldPacket& packet, bool steal, ObjectGuid victim, ObjectGuid caster, uint32 spellId, std::vector<uint32> const& spellIds)
{
    packet.Initialize(steal ? SMSG_SPELLSTEALLOG : SMSG_SPELLDISPELLOG, 8 + 8 + 4 + 1 + 4 + spellIds.size() * 5);
    packet << victim.WriteAsPacked();
    packet << caster.WriteAsPacked();
    packet << uint32(spellId);
    packet << uint8(0);                                     // not used
    packet << uint32(spellIds.size());
    for (std::vector<uint32>::const_iterator itr = spellIds.begin(); itr != spellIds.end(); ++itr)
    {
        packet << uint32(*itr);
        packet << uint8(0);                                 // 0 dispelled or stolen, otherwise cleansed or transferred
    }
}

void BuildDispelFailedPacket(WorldPacket& packet, ObjectGuid caster, ObjectGuid victim, uint32 spellId, std::vector<uint32> const& spellIds)
{
    packet.Initialize(SMSG_DISPEL_FAILED, 8 + 8 + 4 + 4 * spellIds.size());
    packet << caster;
    packet << victim;
    packet << uint32(spellId);
    for (std::vector<uint32>::const_iterator itr = spellIds.begin(); itr != spellIds.end(); ++itr)
    {
        packet << uint32(*itr);
    }
}

void BuildClearTargetPacket(WorldPacket& packet, ObjectGuid target)
{
    packet.Initialize(SMSG_CLEAR_TARGET, 8);
    packet << target;
}

void BuildDuelRequestedPacket(WorldPacket& packet, ObjectGuid flag, ObjectGuid challenger)
{
    packet.Initialize(SMSG_DUEL_REQUESTED, 8 + 8);
    packet << flag;
    packet << challenger;
}

void BuildTotemCreatedPacket(WorldPacket& packet, uint8 slot, ObjectGuid totem, uint32 duration, uint32 spellId)
{
    packet.Initialize(SMSG_TOTEM_CREATED, 1 + 8 + 4 + 4);
    packet << uint8(slot);
    packet << totem;
    packet << uint32(duration);
    packet << uint32(spellId);
}

void BuildNewTaxiPathPacket(WorldPacket& packet)
{
    packet.Initialize(SMSG_NEW_TAXI_PATH, 0);
}

void BuildTaxiNodeStatusPacket(WorldPacket& packet, ObjectGuid source, uint8 status)
{
    packet.Initialize(SMSG_TAXINODE_STATUS, 9);
    packet << source;
    packet << uint8(status);
}

void BuildBindPointUpdatePacket(WorldPacket& packet, WorldLocation const& home, uint32 areaId)
{
    packet.Initialize(SMSG_BINDPOINTUPDATE, 4 + 4 + 4 + 4 + 4);
    packet << float(home.coord_x);
    packet << float(home.coord_y);
    packet << float(home.coord_z);
    packet << uint32(home.mapid);
    packet << uint32(areaId);
}

void BuildPlayerBoundPacket(WorldPacket& packet, ObjectGuid binder, uint32 areaId)
{
    packet.Initialize(SMSG_PLAYERBOUND, 8 + 4);
    packet << binder;
    packet << uint32(areaId);
}

void SessionSpellEffectNotices::InstaKillLog(Spell const& spell, ObjectGuid casterGuid, Unit const& victim)
{
    WorldPacket data;
    BuildInstaKillLogPacket(data, casterGuid, victim.GetObjectGuid(), spell.m_spellInfo->ID);
    spell.GetCaster()->SendMessageToSet(&data, true);
}

void SessionSpellEffectNotices::DispelLog(Spell const& spell, Unit const& victim, std::vector<uint32> const& spellIds)
{
    WorldPacket data;
    BuildDispelLogPacket(data, false, victim.GetObjectGuid(), spell.GetCaster()->GetObjectGuid(), spell.m_spellInfo->ID, spellIds);
    spell.GetCaster()->SendMessageToSet(&data, true);
}

void SessionSpellEffectNotices::DispelFailed(Spell const& spell, Unit const& victim, std::vector<uint32> const& spellIds)
{
    WorldPacket data;
    BuildDispelFailedPacket(data, spell.GetCaster()->GetObjectGuid(), victim.GetObjectGuid(), spell.m_spellInfo->ID, spellIds);
    spell.GetCaster()->SendMessageToSet(&data, true);
}

void SessionSpellEffectNotices::StealLog(Spell const& spell, Unit const& victim, std::vector<uint32> const& spellIds)
{
    WorldPacket data;
    BuildDispelLogPacket(data, true, victim.GetObjectGuid(), spell.GetCaster()->GetObjectGuid(), spell.m_spellInfo->ID, spellIds);
    spell.GetCaster()->SendMessageToSet(&data, true);
}

void SessionSpellEffectNotices::ClearTarget(Unit const& target)
{
    WorldPacket data;
    BuildClearTargetPacket(data, target.GetObjectGuid());
    target.SendMessageToSet(&data, false);
}

void SessionSpellEffectNotices::DuelRequested(Player& challenger, Player& challenged, ObjectGuid flag)
{
    WorldPacket data;
    BuildDuelRequestedPacket(data, flag, challenger.GetObjectGuid());
    challenger.SendDirectMessage(&data);
    challenged.SendDirectMessage(&data);
}

void SessionSpellEffectNotices::TotemCreated(Player& owner, uint8 slot, ObjectGuid totem, uint32 duration, uint32 spellId)
{
    WorldPacket data;
    BuildTotemCreatedPacket(data, slot, totem, duration, spellId);
    owner.SendDirectMessage(&data);
}

void SessionSpellEffectNotices::TaxiNodeLearned(Player& player, ObjectGuid source)
{
    WorldPacket data;
    BuildNewTaxiPathPacket(data);
    player.SendDirectMessage(&data);

    BuildTaxiNodeStatusPacket(data, source, 1);
    player.SendDirectMessage(&data);
}

void SessionSpellEffectNotices::HomeBound(Player& player, ObjectGuid binder, WorldLocation const& home, uint32 areaId)
{
    WorldPacket data;
    BuildBindPointUpdatePacket(data, home, areaId);
    player.SendDirectMessage(&data);

    BuildPlayerBoundPacket(data, binder, areaId);
    player.SendDirectMessage(&data);
}
