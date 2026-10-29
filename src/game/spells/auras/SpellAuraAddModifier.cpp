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
 * @file SpellAuras.cpp
 * @brief Spell aura implementation
 *
 * This file implements the SpellAura class which handles spell auras:
 * - Aura application and removal
 * - Aura effect processing (stat modifiers, DoTs, HoTs, etc.)
 * - Aura stacking rules
 * - Aura dispelling mechanics
 * - Aura periodic effects
 * - Aura duration management
 * - Aura visual effects
 *
 * Auras are persistent effects applied by spells that modify
 * unit stats, deal damage over time, or provide other benefits.
 *
 * @see SpellAura for the aura class
 * @see Spell for spell casting
 */

#include "Platform/Define.h"
#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "spells/data/SpellMgr.h"
#include "Player.h"
#include "spells/pipeline/Spell.h"
#include "spells/auras/SpellAuras.h"
#include "Policies/Singleton.h"
#include "Util.h"
#include "GridNotifiersImpl.h"
#include "Language.h"
#include "spells/data/SpellTables.h"

#define NULL_AURA_SLOT 0xFF

void Aura::HandleAddModifier(bool apply, bool Real)
{
    if (GetTarget()->GetTypeId() != TYPEID_PLAYER || !Real)
    {
        return;
    }

    if (m_modifier.m_miscvalue >= MAX_SPELLMOD)
    {
        return;
    }

    if (apply)
    {
        SpellEntry const* spellProto = GetSpellProto();

        // Add custom charges for some mod aura
        if (IsOneChargeModifierAura(spellProto->ID))
        {
            GetHolder()->SetAuraCharges(1);
        }

        SpellClassOptionsEntry const * opt = spellProto->GetSpellClassOptions();
        // Everlasting Affliction, overwrite wrong data, if will need more better restore support of spell_affect table
        if (opt && spellProto->GetSpellFamilyName() == SPELLFAMILY_WARLOCK && spellProto->SpellIconID == 3169)
        {
            // Corruption and Unstable Affliction
            const_cast<SpellClassOptionsEntry*>(opt)->SpellClassMask = ClassFamilyMask(UI64LIT(0x0000010000000002));
        }
        // Improved Flametongue Weapon, overwrite wrong data, maybe time re-add table
        else if (opt && spellProto->ID == 37212)
        {
            // Flametongue Weapon (Passive)
            const_cast<SpellClassOptionsEntry*>(opt)->SpellClassMask = ClassFamilyMask(UI64LIT(0x0000000000200000));
        }
    }

    ((Player*)GetTarget())->AddSpellMod(this, apply);

    ReapplyAffectedPassiveAuras();
}
