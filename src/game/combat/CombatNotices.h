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

#ifndef MANGOS_H_COMBATNOTICES
#define MANGOS_H_COMBATNOTICES

#include "Platform/Define.h"
#include "SharedDefines.h"

class HostileReference;
class Unit;
struct CalcDamageInfo;
struct SpellNonMeleeDamage;
struct SpellPeriodicAuraLogInfo;

/**
 * What a fight tells the clients that see it, one call per notice; `source` is the unit whose
 * neighbourhood is told, itself included unless the notice says otherwise. The session installs
 * the notices that send them at start-up; this base sends nothing.
 */
class CombatNotices
{
    public:
        virtual ~CombatNotices();

        /// `attacker` starts its melee attack on `victim`.
        virtual void MeleeAttackStart(Unit& attacker, Unit& victim);
        /// `attacker` stops its melee attack on `victim`.
        virtual void MeleeAttackStop(Unit& attacker, Unit& victim);
        /// The spell damage `log` of its attacker on its target.
        virtual void SpellNonMeleeDamageLog(Unit& source, SpellNonMeleeDamage const& log);
        /// The tick `info` of a periodic aura, told around the aura's target.
        virtual void PeriodicAuraLog(SpellPeriodicAuraLogInfo const& info);
        /// `caster`'s spell `spellId` missed `target`, for the reason `missInfo`.
        virtual void SpellMiss(Unit& caster, Unit& target, uint32 spellId, SpellMissInfo missInfo);
        /// `target` resisted all the damage of `caster`'s spell `spellId`.
        virtual void SpellDamageResist(Unit& caster, Unit& target, uint32 spellId);
        /// The melee swing `damageInfo` of its attacker on its target.
        virtual void AttackStateUpdate(Unit& source, CalcDamageInfo const& damageInfo);
        /// `healer`'s spell `spellId` healed `victim` for `damage`, `overHeal` of it beyond full health
        /// and `absorb` of it absorbed; `critical` for a critical heal.
        virtual void HealSpellLog(Unit& healer, Unit& victim, uint32 spellId, uint32 damage, uint32 overHeal, bool critical, uint32 absorb);
        /// `victim`'s damage shield, the spell `spellId` of the school `schoolMask`, struck `attacker`
        /// for `damage`, `overkill` of it beyond the attacker's health, with `resist` resisted.
        virtual void DamageShield(Unit& victim, Unit& attacker, uint32 spellId, uint32 damage, uint32 overkill, uint32 schoolMask, uint32 resist);
        /// `owner`'s threat list, told to the units around it but not to itself.
        virtual void ThreatUpdate(Unit& owner);
        /// `owner`'s threat list with `highest` at its top, told to the units around it but not to itself.
        virtual void HighestThreatUpdate(Unit& owner, HostileReference& highest);
        /// `owner`'s threat list is cleared, told to the units around it but not to itself.
        virtual void ThreatClear(Unit& owner);
        /// `removed` left `owner`'s threat list, told to the units around it but not to itself.
        virtual void ThreatRemove(Unit& owner, HostileReference& removed);

        /// The installed notices, or the base when none are installed.
        static CombatNotices& Installed();
        /// Installs `notices` and answers the ones it replaces (NULL for the base). NULL installs the
        /// base. Installed once at start-up, before the world loads.
        static CombatNotices* Install(CombatNotices* notices);
};

#define sCombatNotices CombatNotices::Installed()

#endif
