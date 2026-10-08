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

#include "spells/handlers/SpellHandlerRegistry.h"
#include "spells/handlers/AuraDummyHandlers.h"
#include "spells/handlers/AuraShapeshiftHandlers.h"
#include "spells/handlers/AuraControlHandlers.h"
#include "spells/handlers/SpellEffectTailHandlers.h"
#include "spells/handlers/AuraPeriodicHandlers.h"
#include "spells/handlers/SpellEffectHealPowerHandlers.h"
#include "spells/handlers/SpellEffectObjectCombatHandlers.h"
#include "spells/handlers/SpellTargetingHandlers.h"
#include "spells/handlers/SpellChecksHandlers.h"
#include "spells/handlers/SpellCheckTargetHandlers.h"
#include "spells/handlers/SpellEffectSkillEnchantPetHandlers.h"
#include "spells/handlers/SpellEffectDamageTeleportHandlers.h"
#include "spells/handlers/SpellEffectDummyHandlers.h"
#include "Utilities/Errors.h"

SpellHandlerRegistry const& SpellHandlerRegistry::Game()
{
    // Built once, by the first call: World::SetInitialWorldSettings makes it at boot, after the spell
    // tables load, so a registration fault aborts the start and never a map tick. Read-only afterwards;
    // a function-local static is initialised exactly once even if a thread got here first.
    static SpellHandlerRegistry const registry = []()
    {
        SpellHandlerRegistry built;
        uint32 rows = RegisterAuraDummyHandlers(built);
        rows += RegisterAuraShapeshiftHandlers(built);
        rows += RegisterAuraControlHandlers(built);
        rows += RegisterSpellEffectTailHandlers(built);
        rows += RegisterAuraPeriodicHandlers(built);
        rows += RegisterSpellEffectHealPowerHandlers(built);
        rows += RegisterSpellEffectObjectCombatHandlers(built);
        rows += RegisterSpellTargetingHandlers(built);
        rows += RegisterSpellChecksHandlers(built);
        rows += RegisterSpellCheckTargetHandlers(built);
        rows += RegisterSpellEffectSkillEnchantPetHandlers(built);
        rows += RegisterSpellEffectDamageTeleportHandlers(built);
        rows += RegisterSpellEffectDummyHandlers(built);
        MANGOS_ASSERT(rows == built.Count() + built.CountDefaults()); // no key and no default registered twice
        return built;
    }();
    return registry;
}

std::size_t SpellHandlerRegistry::CountAt(uint32 site) const
{
    std::map<uint64, ErasedFunction>::const_iterator first = m_handlers.lower_bound(MakeKey(site, 0));
    std::map<uint64, ErasedFunction>::const_iterator last = m_handlers.lower_bound(MakeKey(site + 1, 0));
    std::size_t count = 0;
    for (; first != last; ++first)
    {
        ++count;
    }
    return count;
}

bool SpellHandlerRegistry::Insert(uint32 site, uint32 spellId, ErasedFunction function)
{
    if (!function)
    {
        return false;
    }
    return m_handlers.insert(std::make_pair(MakeKey(site, spellId), function)).second;
}

bool SpellHandlerRegistry::InsertDefault(uint32 site, ErasedFunction function)
{
    if (!function)
    {
        return false;
    }
    return m_defaults.insert(std::make_pair(site, function)).second;
}

SpellHandlerRegistry::ErasedFunction SpellHandlerRegistry::Lookup(uint32 site, uint32 spellId) const
{
    std::map<uint64, ErasedFunction>::const_iterator found = m_handlers.find(MakeKey(site, spellId));
    return found == m_handlers.end() ? NULL : found->second;
}

SpellHandlerRegistry::ErasedFunction SpellHandlerRegistry::LookupDefault(uint32 site) const
{
    std::map<uint32, ErasedFunction>::const_iterator found = m_defaults.find(site);
    return found == m_defaults.end() ? NULL : found->second;
}
