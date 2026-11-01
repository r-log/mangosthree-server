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

#include "combat/CombatNotices.h"

#include <atomic>

namespace
{
    std::atomic<CombatNotices*> s_combatNoticesInstalled(NULL);

    CombatNotices& BaseCombatNotices()
    {
        static CombatNotices notices;
        return notices;
    }
}

CombatNotices::~CombatNotices()
{
}

void CombatNotices::MeleeAttackStart(Unit& /*attacker*/, Unit& /*victim*/)
{
}

void CombatNotices::MeleeAttackStop(Unit& /*attacker*/, Unit& /*victim*/)
{
}

void CombatNotices::SpellNonMeleeDamageLog(Unit& /*source*/, SpellNonMeleeDamage const& /*log*/)
{
}

void CombatNotices::PeriodicAuraLog(SpellPeriodicAuraLogInfo const& /*info*/)
{
}

void CombatNotices::SpellMiss(Unit& /*caster*/, Unit& /*target*/, uint32 /*spellId*/, SpellMissInfo /*missInfo*/)
{
}

void CombatNotices::SpellDamageResist(Unit& /*caster*/, Unit& /*target*/, uint32 /*spellId*/)
{
}

void CombatNotices::AttackStateUpdate(Unit& /*source*/, CalcDamageInfo const& /*damageInfo*/)
{
}

void CombatNotices::HealSpellLog(Unit& /*healer*/, Unit& /*victim*/, uint32 /*spellId*/, uint32 /*damage*/, uint32 /*overHeal*/, bool /*critical*/, uint32 /*absorb*/)
{
}

void CombatNotices::DamageShield(Unit& /*victim*/, Unit& /*attacker*/, uint32 /*spellId*/, uint32 /*damage*/, uint32 /*overkill*/, uint32 /*schoolMask*/, uint32 /*resist*/)
{
}

void CombatNotices::ThreatUpdate(Unit& /*owner*/)
{
}

void CombatNotices::HighestThreatUpdate(Unit& /*owner*/, HostileReference& /*highest*/)
{
}

void CombatNotices::ThreatClear(Unit& /*owner*/)
{
}

void CombatNotices::ThreatRemove(Unit& /*owner*/, HostileReference& /*removed*/)
{
}

CombatNotices& CombatNotices::Installed()
{
    CombatNotices* notices = s_combatNoticesInstalled.load(std::memory_order_acquire);
    return notices ? *notices : BaseCombatNotices();
}

CombatNotices* CombatNotices::Install(CombatNotices* notices)
{
    return s_combatNoticesInstalled.exchange(notices, std::memory_order_acq_rel);
}
