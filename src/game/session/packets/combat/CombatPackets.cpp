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

#include "CombatPackets.h"
#include "Log.h"
#include "Opcodes.h"
#include "Unit.h"
#include "WorldPacket.h"
#include "combat/ThreatManager.h"
#include "spells/auras/SpellAuras.h"
#include "spells/data/SpellMgr.h"

void BuildAttackStartPacket(WorldPacket& packet, Unit const& attacker, Unit const& victim)
{
    packet.Initialize(SMSG_ATTACKSTART, 8 + 8);
    packet << attacker.GetObjectGuid();
    packet << victim.GetObjectGuid();
}

void BuildAttackStopPacket(WorldPacket& packet, Unit const& attacker, Unit const& victim)
{
    packet.Initialize(SMSG_ATTACKSTOP, 4 + 16);
    packet << attacker.GetPackGUID();
    packet << victim.GetPackGUID();                         // can be 0x00...
    packet << uint32(0);                                    // can be 0x1
}

void BuildSpellNonMeleeDamageLogPacket(WorldPacket& packet, SpellNonMeleeDamage const& log)
{
    uint32 targetHealth = log.target->GetHealth();
    uint32 overkill = log.damage > targetHealth ? log.damage - targetHealth : 0;

    packet.Initialize(SMSG_SPELLNONMELEEDAMAGELOG, 16 + 4 + 4 + 4 + 1 + 4 + 4 + 1 + 1 + 4 + 4 + 1);
    packet << log.target->GetPackGUID();
    packet << log.attacker->GetPackGUID();
    packet << uint32(log.SpellID);
    packet << uint32(log.damage);                           // damage amount
    packet << uint32(overkill);                             // overkill
    packet << uint8(log.schoolMask);                        // damage school
    packet << uint32(log.absorb);                           // AbsorbedDamage
    packet << uint32(log.resist);                           // resist
    packet << uint8(log.physicalLog);                       // if 1, then client show spell name (example: %s's ranged shot hit %s for %u school or %s suffers %u school damage from %s's spell_name
    packet << uint8(log.unused);                            // unused
    packet << uint32(log.blocked);                          // blocked
    packet << uint32(log.HitInfo);
    packet << uint8(0);                                     // flag to use extend data
}

bool BuildPeriodicAuraLogPacket(WorldPacket& packet, SpellPeriodicAuraLogInfo const& info)
{
    Aura* aura = info.aura;
    Modifier const* mod = aura->GetModifier();

    packet.Initialize(SMSG_PERIODICAURALOG, 30);
    packet << aura->GetTarget()->GetPackGUID();
    packet << aura->GetCasterGuid().WriteAsPacked();
    packet << uint32(aura->GetId());                        // spellId
    packet << uint32(1);                                    // count
    packet << uint32(mod->m_auraname);                      // auraId
    switch (mod->m_auraname)
    {
        case SPELL_AURA_PERIODIC_DAMAGE:
        case SPELL_AURA_PERIODIC_DAMAGE_PERCENT:
            packet << uint32(info.damage);                  // damage
            packet << uint32(info.overDamage);              // overkill?
            packet << uint32(GetSpellSchoolMask(aura->GetSpellProto()));
            packet << uint32(info.absorb);                  // absorb
            packet << uint32(info.resist);                  // resist
            packet << uint8(info.critical ? 1 : 0);         // new 3.1.2 critical flag
            return true;
        case SPELL_AURA_PERIODIC_HEAL:
        case SPELL_AURA_OBS_MOD_HEALTH:
            packet << uint32(info.damage);                  // damage
            packet << uint32(info.overDamage);              // overheal?
            packet << uint32(info.absorb);                  // absorb
            packet << uint8(info.critical ? 1 : 0);         // new 3.1.2 critical flag
            return true;
        case SPELL_AURA_OBS_MOD_MANA:
        case SPELL_AURA_PERIODIC_ENERGIZE:
            packet << uint32(mod->m_miscvalue);             // power type
            packet << uint32(info.damage);                  // damage
            return true;
        case SPELL_AURA_PERIODIC_MANA_LEECH:
            packet << uint32(mod->m_miscvalue);             // power type
            packet << uint32(info.damage);                  // amount
            packet << float(info.multiplier);               // gain multiplier
            return true;
        default:
            return false;
    }
}

void BuildSpellMissPacket(WorldPacket& packet, Unit const& caster, Unit const& target, uint32 spellId, SpellMissInfo missInfo)
{
    packet.Initialize(SMSG_SPELLLOGMISS, 4 + 8 + 1 + 4 + 8 + 1);
    packet << uint32(spellId);
    packet << caster.GetObjectGuid();
    packet << uint8(0);                                     // can be 0 or 1, flag
    packet << uint32(1);                                    // target count
    packet << target.GetObjectGuid();                       // target GUID
    packet << uint8(missInfo);
}

void BuildProcResistPacket(WorldPacket& packet, Unit const& caster, Unit const& target, uint32 spellId)
{
    // Two flat uint64 guids, not packed: the client's reader for the opcode reads eight bytes for
    // each, then the log format selector, which it reads and discards.
    packet.Initialize(SMSG_PROCRESIST, 8 + 8 + 4 + 1);
    packet << uint64(caster.GetObjectGuid().GetRawValue());
    packet << uint64(target.GetObjectGuid().GetRawValue());
    packet << uint32(spellId);
    packet << uint8(0);                                     // log format: 0 default, 1 debug
}

void BuildAttackerStateUpdatePacket(WorldPacket& packet, CalcDamageInfo const& damageInfo)
{
    uint32 targetHealth = damageInfo.target->GetHealth();
    uint32 overkill = damageInfo.damage > targetHealth ? damageInfo.damage - targetHealth : 0;

    uint32 count = 1;
    packet.Initialize(SMSG_ATTACKERSTATEUPDATE, 16 + 45);
    packet << (uint32)damageInfo.HitInfo;
    packet << damageInfo.attacker->GetPackGUID();
    packet << damageInfo.target->GetPackGUID();
    packet << (uint32)(damageInfo.damage);                  // Full damage
    packet << uint32(overkill);                             // overkill value
    packet << uint8(count);                                 // Sub damage count

    for (uint32 i = 0; i < count; ++i)
    {
        packet << uint32(damageInfo.damageSchoolMask);      // School of sub damage
        packet << float(damageInfo.damage);                 // sub damage
        packet << uint32(damageInfo.damage);                // Sub Damage
    }

    if (damageInfo.HitInfo & (HITINFO_ABSORB | HITINFO_ABSORB2))
    {
        for (uint32 i = 0; i < count; ++i)
        {
            packet << uint32(damageInfo.absorb);            // Absorb
        }
    }

    if (damageInfo.HitInfo & (HITINFO_RESIST | HITINFO_RESIST2))
    {
        for (uint32 i = 0; i < count; ++i)
        {
            packet << uint32(damageInfo.resist);            // Resist
        }
    }

    packet << uint8(damageInfo.TargetState);
    packet << uint32(0);                                    // unknown, usually seen with -1, 0 and 1000
    packet << uint32(0);                                    // spell id, seen with heroic strike and disarm as examples.
    // HITINFO_NOACTION normally set if spell

    if (damageInfo.HitInfo & HITINFO_BLOCK)
    {
        packet << uint32(damageInfo.blocked_amount);
    }

    if (damageInfo.HitInfo & HITINFO_UNK22)
    {
        packet << uint32(0);                                // count of some sort?
    }

    if (damageInfo.HitInfo & HITINFO_UNK0)
    {
        packet << uint32(0);
        packet << float(0);
        packet << float(0);
        packet << float(0);
        packet << float(0);
        packet << float(0);
        packet << float(0);
        packet << float(0);
        packet << float(0);
        for (uint8 i = 0; i < 5; ++i)
        {
            packet << float(0);
            packet << float(0);
        }
        packet << uint32(0);
    }
}

void BuildSpellHealLogPacket(WorldPacket& packet, Unit const& healer, Unit const& victim, uint32 spellId, uint32 damage, uint32 overHeal, bool critical, uint32 absorb)
{
    packet.Initialize(SMSG_SPELLHEALLOG, 8 + 8 + 4 + 4 + 1);
    packet << victim.GetPackGUID();
    packet << healer.GetPackGUID();
    packet << uint32(spellId);
    packet << uint32(damage);
    packet << uint32(overHeal);
    packet << uint32(absorb);
    packet << uint8(critical ? 1 : 0);
    packet << uint8(0);                                     // unused in client?
}

void BuildSpellDamageShieldPacket(WorldPacket& packet, Unit const& victim, Unit const& attacker, uint32 spellId, uint32 damage, uint32 overkill, uint32 schoolMask, uint32 resist)
{
    packet.Initialize(SMSG_SPELLDAMAGESHIELD, 8 + 8 + 4 + 4 + 4 + 4);
    packet << victim.GetObjectGuid();
    packet << attacker.GetObjectGuid();
    packet << uint32(spellId);
    packet << uint32(damage);                               // Damage
    packet << uint32(overkill);                             // Overkill
    packet << uint32(schoolMask);
    packet << uint32(resist);                               // Resist
}

void BuildThreatUpdatePacket(WorldPacket& packet, Unit const& owner)
{
    ThreatList const& tlist = owner.GetThreatManager().getThreatList();
    uint32 count = tlist.size();
    packet.Initialize(SMSG_THREAT_UPDATE, 8 + count * 8);
    packet << owner.GetPackGUID();
    packet << uint32(count);
    for (ThreatList::const_iterator itr = tlist.begin(); itr != tlist.end(); ++itr)
    {
        packet << (*itr)->getUnitGuid().WriteAsPacked();
        packet << uint32((*itr)->getThreat());
    }
}

void BuildHighestThreatUpdatePacket(WorldPacket& packet, Unit const& owner, HostileReference const& highest)
{
    ThreatList const& tlist = owner.GetThreatManager().getThreatList();
    uint32 count = tlist.size();
    packet.Initialize(SMSG_HIGHEST_THREAT_UPDATE, 8 + 8 + count * 8);
    packet << owner.GetPackGUID();
    packet << highest.getUnitGuid().WriteAsPacked();
    packet << uint32(count);
    for (ThreatList::const_iterator itr = tlist.begin(); itr != tlist.end(); ++itr)
    {
        packet << (*itr)->getUnitGuid().WriteAsPacked();
        packet << uint32((*itr)->getThreat());
    }
}

void BuildThreatClearPacket(WorldPacket& packet, Unit const& owner)
{
    packet.Initialize(SMSG_THREAT_CLEAR, 8);
    packet << owner.GetPackGUID();
}

void BuildThreatRemovePacket(WorldPacket& packet, Unit const& owner, HostileReference const& removed)
{
    packet.Initialize(SMSG_THREAT_REMOVE, 8 + 8);
    packet << owner.GetPackGUID();
    packet << removed.getUnitGuid().WriteAsPacked();
}

void SessionCombatNotices::MeleeAttackStart(Unit& attacker, Unit& victim)
{
    WorldPacket data;
    BuildAttackStartPacket(data, attacker, victim);
    attacker.SendMessageToSet(&data, true);
    DEBUG_FILTER_LOG(LOG_FILTER_COMBAT, "WORLD: Sent SMSG_ATTACKSTART: %s -> %s", attacker.GetGuidStr().c_str(), victim.GetGuidStr().c_str());
}

void SessionCombatNotices::MeleeAttackStop(Unit& attacker, Unit& victim)
{
    WorldPacket data;
    BuildAttackStopPacket(data, attacker, victim);
    attacker.SendMessageToSet(&data, true);
}

void SessionCombatNotices::SpellNonMeleeDamageLog(Unit& source, SpellNonMeleeDamage const& log)
{
    WorldPacket data;
    BuildSpellNonMeleeDamageLogPacket(data, log);
    source.SendMessageToSet(&data, true);
}

void SessionCombatNotices::PeriodicAuraLog(SpellPeriodicAuraLogInfo const& info)
{
    WorldPacket data;
    if (!BuildPeriodicAuraLogPacket(data, info))
    {
        sLog.outError("Unit::SendPeriodicAuraLog: unknown aura %u", uint32(info.aura->GetModifier()->m_auraname));
        return;
    }
    info.aura->GetTarget()->SendMessageToSet(&data, true);
}

void SessionCombatNotices::SpellMiss(Unit& caster, Unit& target, uint32 spellId, SpellMissInfo missInfo)
{
    WorldPacket data;
    BuildSpellMissPacket(data, caster, target, spellId, missInfo);
    caster.SendMessageToSet(&data, true);
}

void SessionCombatNotices::SpellDamageResist(Unit& caster, Unit& target, uint32 spellId)
{
    WorldPacket data;
    BuildProcResistPacket(data, caster, target, spellId);
    caster.SendMessageToSet(&data, true);
}

void SessionCombatNotices::AttackStateUpdate(Unit& source, CalcDamageInfo const& damageInfo)
{
    DEBUG_FILTER_LOG(LOG_FILTER_COMBAT, "WORLD: Sending SMSG_ATTACKERSTATEUPDATE");
    WorldPacket data;
    BuildAttackerStateUpdatePacket(data, damageInfo);
    source.SendMessageToSet(&data, true);
}

void SessionCombatNotices::HealSpellLog(Unit& healer, Unit& victim, uint32 spellId, uint32 damage, uint32 overHeal, bool critical, uint32 absorb)
{
    WorldPacket data;
    BuildSpellHealLogPacket(data, healer, victim, spellId, damage, overHeal, critical, absorb);
    healer.SendMessageToSet(&data, true);
}

void SessionCombatNotices::DamageShield(Unit& victim, Unit& attacker, uint32 spellId, uint32 damage, uint32 overkill, uint32 schoolMask, uint32 resist)
{
    WorldPacket data;
    BuildSpellDamageShieldPacket(data, victim, attacker, spellId, damage, overkill, schoolMask, resist);
    victim.SendMessageToSet(&data, true);
}

void SessionCombatNotices::ThreatUpdate(Unit& owner)
{
    DEBUG_FILTER_LOG(LOG_FILTER_COMBAT, "WORLD: Send SMSG_THREAT_UPDATE Message");
    WorldPacket data;
    BuildThreatUpdatePacket(data, owner);
    owner.SendMessageToSet(&data, false);
}

void SessionCombatNotices::HighestThreatUpdate(Unit& owner, HostileReference& highest)
{
    DEBUG_FILTER_LOG(LOG_FILTER_COMBAT, "WORLD: Send SMSG_HIGHEST_THREAT_UPDATE Message");
    WorldPacket data;
    BuildHighestThreatUpdatePacket(data, owner, highest);
    owner.SendMessageToSet(&data, false);
}

void SessionCombatNotices::ThreatClear(Unit& owner)
{
    DEBUG_FILTER_LOG(LOG_FILTER_COMBAT, "WORLD: Send SMSG_THREAT_CLEAR Message");
    WorldPacket data;
    BuildThreatClearPacket(data, owner);
    owner.SendMessageToSet(&data, false);
}

void SessionCombatNotices::ThreatRemove(Unit& owner, HostileReference& removed)
{
    DEBUG_FILTER_LOG(LOG_FILTER_COMBAT, "WORLD: Send SMSG_THREAT_REMOVE Message");
    WorldPacket data;
    BuildThreatRemovePacket(data, owner, removed);
    owner.SendMessageToSet(&data, false);
}
