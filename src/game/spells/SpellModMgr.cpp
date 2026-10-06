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

#include "SpellModMgr.h"
#include "Utilities/Errors.h"
#include "Common/TimeConstants.h"
#include "DBCStores.h"
#include "DBCStructure.h"

void SpellModMgr::Change(SpellModEntry const& entry, bool apply, SpellModChangedSink const& report)
{
    SpellModChangedFact fact;
    fact.flat = entry.flat;
    fact.op = uint8(entry.op);
    for (int eff = 0; eff < 96; ++eff)
    {
        uint64 _mask = 0;
        uint32 _mask2 = 0;

        if (eff < 64)
        {
            _mask = uint64(1) << (eff - 0);
        }
        else
        {
            _mask2 = uint32(1) << (eff - 64);
        }

        if (entry.mask->IsFitToFamilyMask(_mask, _mask2))
        {
            int32 val = 0;
            for (std::list<SpellModEntry>::const_iterator itr = m_mods[entry.op].begin(); itr != m_mods[entry.op].end(); ++itr)
            {
                if (itr->flat == entry.flat && (itr->mask->IsFitToFamilyMask(_mask, _mask2)))
                {
                    val += *itr->amount;
                }
            }
            val += apply ? *entry.amount : -(*entry.amount);
            SpellModValue value;
            value.effect = uint8(eff);
            value.value = val;
            fact.values.push_back(value);
        }
    }
    MANGOS_ASSERT(report);
    report(fact);

    if (apply)
    {
        m_mods[entry.op].push_back(entry);
    }
    else
    {
        Aura const* aura = entry.aura;
        m_mods[entry.op].remove_if([aura](SpellModEntry const& listed) { return listed.aura == aura; });
    }
}

template <class T> T SpellModMgr::ApplySpellMod(uint32 spellId, SpellModOp op, T& basevalue)
{
    SpellEntry const* spellInfo = sSpellStore.LookupEntry(spellId);
    if (!spellInfo)
    {
        return 0;
    }

    int32 totalpct = 0;
    int32 totalflat = 0;
    for (std::list<SpellModEntry>::iterator itr = m_mods[op].begin(); itr != m_mods[op].end(); ++itr)
    {
        SpellModEntry const& entry = *itr;

        if (!spellInfo->IsFitToFamily(SpellFamily(entry.family), *entry.mask))
        {
            continue;
        }

        if (entry.flat)
        {
            totalflat += *entry.amount;
        }
        else
        {
            // skip percent mods for null basevalue (most important for spell mods with charges )
            if (basevalue == T(0))
            {
                continue;
            }

            // special case (skip >10sec spell casts for instant cast setting)
            if (entry.op == SPELLMOD_CASTING_TIME
                    && basevalue >= T(10 * IN_MILLISECONDS) && *entry.amount <= -100)
                continue;

            totalpct += *entry.amount;
        }
    }

    float diff = (float)basevalue * (float)totalpct / 100.0f + (float)totalflat;
    basevalue = T((float)basevalue + diff);
    return T(diff);
}

template int32 SpellModMgr::ApplySpellMod<int32>(uint32 spellId, SpellModOp op, int32& basevalue);
template uint32 SpellModMgr::ApplySpellMod<uint32>(uint32 spellId, SpellModOp op, uint32& basevalue);
template float SpellModMgr::ApplySpellMod<float>(uint32 spellId, SpellModOp op, float& basevalue);
