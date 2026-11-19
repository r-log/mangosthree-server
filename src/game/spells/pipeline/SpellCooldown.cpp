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

/**
 * @file Spell.cpp
 * @brief Spell casting and effect implementation
 *
 * This file implements the Spell class which handles spell casting:
 * - Spell validation and casting requirements
 * - Spell effect execution (damage, healing, summon, etc.)
 * - Spell targeting and area effects
 * - Spell cooldowns and resource costs
 * - Spell interruption and pushback
 * - Spell aura application
 * - Spell hit/miss calculations
 *
 * Spells are the primary combat mechanic in WoW, encompassing
 * abilities, talents, and item effects.
 *
 * @see Spell for the spell class
 * @see SpellAura for spell auras
 * @see SpellMgr for spell management
 */

#include "spells/pipeline/Spell.h"
#include "Database/DatabaseEnv.h"
#include "GridNotifiersImpl.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "spells/data/SpellMgr.h"
#include "spells/pipeline/SpellRules.h"
#include "Player.h"
#include "Unit.h"
#include "Policies/Singleton.h"
#include "SharedDefines.h"
#include "Util.h"
#include "DB2Stores.h"
#include "DisableMgr.h"

/**
 * @brief Checks whether the spell is currently blocked by the global cooldown.
 *
 * @return True if global cooldown is active; otherwise, false.
 */
bool Spell::HasGlobalCooldown()
{
    // global cooldown have only player or controlled units
    if (m_caster->GetCharmInfo())
    {
        return m_caster->GetCharmInfo()->GetGlobalCooldownMgr().HasGlobalCooldown(m_spellInfo);
    }
    else if (Player* casterPlayer = m_caster->ToPlayer())
    {
        return casterPlayer->GetGlobalCooldownMgr().HasGlobalCooldown(m_spellInfo);
    }
    else
    {
        return false;
    }
}

/**
 * @brief Starts the global cooldown for the caster when applicable.
 */
void Spell::TriggerGlobalCooldown()
{
    int32 gcd = m_spellInfo->GetStartRecoveryTime();
    if (!gcd)
    {
        return;
    }

    // a player's global cooldown modifiers first, then the haste and the 1.0 to 1.5 s hold only above one second, in
    // the client's order (Spell_C_StartSpellCooldown)
    if (m_caster->GetTypeId() == TYPEID_PLAYER)
    {
        m_caster->GetSpellMods()->ApplySpellMod(m_spellInfo->ID, SPELLMOD_GLOBAL_COOLDOWN, gcd);
    }

    const bool physical = Spells::IsPhysicalAttack(m_spellInfo->GetDmgClass(), m_spellInfo->Attributes);
    const bool hasted = Spells::GlobalCooldownHasted(m_spellInfo->GetStartRecoveryCategory(), physical, gcd);
    gcd = Spells::GlobalCooldown(gcd, hasted, m_caster->GetFloatValue(UNIT_MOD_CAST_HASTE));
    if (gcd <= 0)
    {
        return;
    }

    // global cooldown have only player or controlled units
    if (m_caster->GetCharmInfo())
    {
        m_caster->GetCharmInfo()->GetGlobalCooldownMgr().AddGlobalCooldown(m_spellInfo, gcd);
    }
    else if (Player* casterPlayer = m_caster->ToPlayer())
    {
        casterPlayer->GetGlobalCooldownMgr().AddGlobalCooldown(m_spellInfo, gcd);
    }
}

/**
 * @brief Cancels the global cooldown started by the current generic spell cast.
 */
void Spell::CancelGlobalCooldown()
{
    if (!m_spellInfo->GetStartRecoveryTime())
    {
        return;
    }

    // cancel global cooldown when interrupting current cast
    if (m_caster->GetCurrentSpell(CURRENT_GENERIC_SPELL) != this)
    {
        return;
    }

    // global cooldown have only player or controlled units
    if (m_caster->GetCharmInfo())
    {
        m_caster->GetCharmInfo()->GetGlobalCooldownMgr().CancelGlobalCooldown(m_spellInfo);
    }
    else if (Player* casterPlayer = m_caster->ToPlayer())
    {
        casterPlayer->GetGlobalCooldownMgr().CancelGlobalCooldown(m_spellInfo);
    }
}
