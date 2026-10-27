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

#include "data/DbScriptStore.h"
#include "DBCStores.h"

DbScriptStore::DbScriptStore()
{
    m_dbScripts.resize(DBS_END);
}

ScriptChainMap const* DbScriptStore::GetScriptChainMap(DBScriptType type)
{
    std::lock_guard<std::mutex> _guard(m_lock);
    if ((type != DBS_INTERNAL) && type < DBS_END)
    {
        return &m_dbScripts[type];
    }

    return NULL;
}

ScriptChainMap& DbScriptStore::GetChainsForLoad(DBScriptType type)
{
    return m_dbScripts[type];
}

std::mutex& DbScriptStore::GetLoadLock()
{
    return m_lock;
}

// returns priority (0 == can not start script)
uint8 DbScriptStore::GetSpellStartDBScriptPriority(SpellEntry const* spellinfo, SpellEffectIndex effIdx)
{
    SpellEffectEntry const* spellEffect = spellinfo->GetSpellEffect(effIdx);
    if (!spellEffect)
    {
        return 0;
    }
    if (spellEffect->Effect == SPELL_EFFECT_SCRIPT_EFFECT)
    {
        return 10;
    }

    if (spellEffect->Effect == SPELL_EFFECT_DUMMY)
    {
        return 9;
    }

    // NonExisting triggered spells can also start DB-Spell-Scripts
    if (spellEffect->Effect == SPELL_EFFECT_TRIGGER_SPELL && !sSpellStore.LookupEntry(spellEffect->EffectTriggerSpell))
    {
        return 5;
    }

    // NonExisting trigger missile spells can also start DB-Spell-Scripts
    if (spellEffect->Effect == SPELL_EFFECT_TRIGGER_MISSILE && !sSpellStore.LookupEntry(spellEffect->EffectTriggerSpell))
    {
        return 4;
    }

    // Can not start script
    return 0;
}

// Priorize: SCRIPT_EFFECT before DUMMY before Non-Existing triggered spell, for same priority the first effect with the priority triggers
bool DbScriptStore::CanSpellEffectStartDBScript(SpellEntry const* spellinfo, SpellEffectIndex effIdx)
{
    uint8 priority = GetSpellStartDBScriptPriority(spellinfo, effIdx);
    if (!priority)
    {
        return false;
    }

    for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        uint8 currentPriority = GetSpellStartDBScriptPriority(spellinfo, SpellEffectIndex(i));
        if (currentPriority < priority)                     // lower priority, continue checking
        {
            continue;
        }
        if (currentPriority > priority)                     // take other index with higher priority
        {
            return false;
        }
        if (i < effIdx)                                     // same priority at lower index
        {
            return false;
        }
    }

    return true;
}
