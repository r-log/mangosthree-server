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

#ifndef MANGOS_H_COMBATPACKETS
#define MANGOS_H_COMBATPACKETS

#include "combat/CombatNotices.h"

class WorldPacket;

/// Initializes `packet` as `attacker`'s melee attack start on `victim`: both guids.
void BuildAttackStartPacket(WorldPacket& packet, Unit const& attacker, Unit const& victim);
/// Initializes `packet` as `attacker`'s melee attack stop on `victim`: both packed guids, then 0.
void BuildAttackStopPacket(WorldPacket& packet, Unit const& attacker, Unit const& victim);
/// Initializes `packet` as the spell damage `log`: the target's and the attacker's packed guids, the
/// spell, the damage and the overkill beyond the target's health, the school, the absorbed, resisted
/// and blocked amounts and the hit flags.
void BuildSpellNonMeleeDamageLogPacket(WorldPacket& packet, SpellNonMeleeDamage const& log);
/// Initializes `packet` as the periodic aura tick `info`: the target's and the caster's packed guids,
/// the spell, then the tick's fields for the aura's type. False for an aura type with no tick log.
bool BuildPeriodicAuraLogPacket(WorldPacket& packet, SpellPeriodicAuraLogInfo const& info);
/// Initializes `packet` as `caster`'s spell `spellId` missing `target`, for the reason `missInfo`.
void BuildSpellMissPacket(WorldPacket& packet, Unit const& caster, Unit const& target, uint32 spellId, SpellMissInfo missInfo);
/// Initializes `packet` as `target` resisting all of `caster`'s spell `spellId`: both raw guids, the
/// spell, then the log format (0).
void BuildProcResistPacket(WorldPacket& packet, Unit const& caster, Unit const& target, uint32 spellId);
/// Initializes `packet` as the melee swing `damageInfo`: the hit flags, both packed guids, the damage
/// and the overkill, the sub-damage, then the fields the hit flags call for.
void BuildAttackerStateUpdatePacket(WorldPacket& packet, CalcDamageInfo const& damageInfo);
/// Initializes `packet` as `healer`'s heal of `victim`: both packed guids, the spell, the amount, the
/// overheal, the absorbed amount, the critical flag, then 0.
void BuildSpellHealLogPacket(WorldPacket& packet, Unit const& healer, Unit const& victim, uint32 spellId, uint32 damage, uint32 overHeal, bool critical, uint32 absorb);
/// Initializes `packet` as `victim`'s damage shield striking `attacker`: both guids, the spell, the
/// damage, the overkill, the school, then the resisted amount.
void BuildSpellDamageShieldPacket(WorldPacket& packet, Unit const& victim, Unit const& attacker, uint32 spellId, uint32 damage, uint32 overkill, uint32 schoolMask, uint32 resist);
/// Initializes `packet` as `owner`'s threat list: its packed guid, the count, then each unit's packed
/// guid and threat.
void BuildThreatUpdatePacket(WorldPacket& packet, Unit const& owner);
/// Initializes `packet` as `owner`'s threat list with `highest` at its top: its packed guid, the
/// highest's, the count, then each unit's packed guid and threat.
void BuildHighestThreatUpdatePacket(WorldPacket& packet, Unit const& owner, HostileReference const& highest);
/// Initializes `packet` as `owner`'s threat list cleared: its packed guid.
void BuildThreatClearPacket(WorldPacket& packet, Unit const& owner);
/// Initializes `packet` as `removed` leaving `owner`'s threat list: both packed guids.
void BuildThreatRemovePacket(WorldPacket& packet, Unit const& owner, HostileReference const& removed);

/// The fight's notices, each built here and told to the units around its source.
class SessionCombatNotices : public CombatNotices
{
    public:
        void MeleeAttackStart(Unit& attacker, Unit& victim) override;
        void MeleeAttackStop(Unit& attacker, Unit& victim) override;
        void SpellNonMeleeDamageLog(Unit& source, SpellNonMeleeDamage const& log) override;
        void PeriodicAuraLog(SpellPeriodicAuraLogInfo const& info) override;
        void SpellMiss(Unit& caster, Unit& target, uint32 spellId, SpellMissInfo missInfo) override;
        void SpellDamageResist(Unit& caster, Unit& target, uint32 spellId) override;
        void AttackStateUpdate(Unit& source, CalcDamageInfo const& damageInfo) override;
        void HealSpellLog(Unit& healer, Unit& victim, uint32 spellId, uint32 damage, uint32 overHeal, bool critical, uint32 absorb) override;
        void DamageShield(Unit& victim, Unit& attacker, uint32 spellId, uint32 damage, uint32 overkill, uint32 schoolMask, uint32 resist) override;
        void ThreatUpdate(Unit& owner) override;
        void HighestThreatUpdate(Unit& owner, HostileReference& highest) override;
        void ThreatClear(Unit& owner) override;
        void ThreatRemove(Unit& owner, HostileReference& removed) override;
};

#endif
