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
            float val = 0.0f;
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
        if (m_keepInto)
        {
            KeepRemoved(aura, entry.op);
        }
        m_mods[entry.op].remove_if([aura](SpellModEntry const& listed) { return listed.aura == aura; });
    }
}

void SpellModMgr::KeepRemoved(Aura const* aura, int32 op)
{
    SpellEntry const* spellInfo = sSpellStore.LookupEntry(m_keepSpell);
    if (!spellInfo || !ReadAfterTheCast(op))
    {
        return;
    }

    for (SpellModEntry const& listed : m_mods[op])
    {
        if (listed.aura != aura || !spellInfo->IsFitToFamily(SpellFamily(listed.family), *listed.mask))
        {
            continue;
        }

        KeptSpellMod kept = { listed.spellId, listed.effIndex, listed.flat, *listed.amount, listed.op, listed.family,
                              listed.mask->Flags, listed.mask->Flags2, listed.mastery };
        m_keepInto->push_back(kept);
    }
}

bool SpellModMgr::ReadAfterTheCast(int32 op)
{
    switch (op)
    {
        case SPELLMOD_DAMAGE:
        case SPELLMOD_DURATION:
        case SPELLMOD_THREAT:
        case SPELLMOD_EFFECT1:
        case SPELLMOD_CHARGES:
        case SPELLMOD_RADIUS:
        case SPELLMOD_CRITICAL_CHANCE:
        case SPELLMOD_ALL_EFFECTS:
        case SPELLMOD_EFFECT2:
        case SPELLMOD_CRIT_DAMAGE_BONUS:
        case SPELLMOD_ACTIVATION_TIME:
        case SPELLMOD_EFFECT_PAST_FIRST:
        case SPELLMOD_DOT:
        case SPELLMOD_EFFECT3:
        case SPELLMOD_SPELL_BONUS_DAMAGE:
        case SPELLMOD_MULTIPLE_VALUE:
        case SPELLMOD_RESIST_DISPEL_CHANCE:
            return true;
        default:
            return false;
    }
}

namespace
{
    /// Adds one modifier to the sums ApplySpellMod takes: a flat amount always, a percentage unless
    /// the value is 0 or the cast-time case skips it.
    template <class T> void AddToSums(bool flat, float amount, int32 op, T basevalue, float& totalpct, float& totalflat)
    {
        if (flat)
        {
            totalflat += amount;
            return;
        }

        // skip percent mods for null basevalue (most important for spell mods with charges )
        if (basevalue == T(0))
        {
            return;
        }

        // special case (skip >10sec spell casts for instant cast setting)
        if (op == SPELLMOD_CASTING_TIME && basevalue >= T(10 * IN_MILLISECONDS) && amount <= -100)
        {
            return;
        }

        totalpct += amount;
    }

    /// Whether `kept` holds a modifier of aura spell `spellId` (not 0) and effect `effIndex`.
    bool KeptHolds(KeptSpellMods const& kept, uint32 spellId, uint8 effIndex)
    {
        if (!spellId)
        {
            return false;
        }

        for (KeptSpellMod const& mod : kept)
        {
            if (mod.spellId == spellId && mod.effIndex == effIndex)
            {
                return true;
            }
        }
        return false;
    }
}

template <class T> T SpellModMgr::ApplySpellMod(uint32 spellId, SpellModOp op, T& basevalue, bool masteryOnly)
{
    SpellEntry const* spellInfo = sSpellStore.LookupEntry(spellId);
    if (!spellInfo)
    {
        return 0;
    }

    KeptSpellMods const* kept = m_kept && m_keptSpell == spellId ? m_kept : NULL;

    float totalpct = 0.0f;
    float totalflat = 0.0f;
    for (std::list<SpellModEntry>::iterator itr = m_mods[op].begin(); itr != m_mods[op].end(); ++itr)
    {
        SpellModEntry const& entry = *itr;

        if (masteryOnly && !entry.mastery)
        {
            continue;
        }

        if (kept && KeptHolds(*kept, entry.spellId, entry.effIndex))
        {
            continue;
        }

        if (!spellInfo->IsFitToFamily(SpellFamily(entry.family), *entry.mask))
        {
            continue;
        }

        AddToSums(entry.flat, *entry.amount, entry.op, basevalue, totalpct, totalflat);
    }

    if (kept)
    {
        for (KeptSpellMod const& mod : *kept)
        {
            if (mod.op != int32(op) || (masteryOnly && !mod.mastery) ||
                !spellInfo->IsFitToFamily(SpellFamily(mod.family), mod.maskFlags, mod.maskFlags2))
            {
                continue;
            }

            AddToSums(mod.flat, mod.amount, mod.op, basevalue, totalpct, totalflat);
        }
    }

    float diff = (float)basevalue * totalpct / 100.0f + totalflat;
    basevalue = T((float)basevalue + diff);
    return T(diff);
}

template int32 SpellModMgr::ApplySpellMod<int32>(uint32 spellId, SpellModOp op, int32& basevalue, bool masteryOnly);
template uint32 SpellModMgr::ApplySpellMod<uint32>(uint32 spellId, SpellModOp op, uint32& basevalue, bool masteryOnly);
template float SpellModMgr::ApplySpellMod<float>(uint32 spellId, SpellModOp op, float& basevalue, bool masteryOnly);

SpellModMgr::KeepScope::KeepScope(SpellModMgr* mgr, uint32 spellId, KeptSpellMods& into)
    : m_mgr(mgr), m_previousSpell(0), m_previousInto(NULL)
{
    into.clear();
    if (!m_mgr)
    {
        return;
    }

    m_previousSpell = m_mgr->m_keepSpell;
    m_previousInto = m_mgr->m_keepInto;
    m_mgr->m_keepSpell = spellId;
    m_mgr->m_keepInto = &into;
}

SpellModMgr::KeepScope::~KeepScope()
{
    if (m_mgr)
    {
        m_mgr->m_keepSpell = m_previousSpell;
        m_mgr->m_keepInto = m_previousInto;
    }
}

SpellModMgr::KeptScope::KeptScope(SpellModMgr* mgr, uint32 spellId, KeptSpellMods const& kept)
    : m_mgr(mgr), m_previousSpell(0), m_previousKept(NULL)
{
    if (!m_mgr)
    {
        return;
    }

    m_previousSpell = m_mgr->m_keptSpell;
    m_previousKept = m_mgr->m_kept;
    m_mgr->m_keptSpell = spellId;
    m_mgr->m_kept = &kept;
}

SpellModMgr::KeptScope::~KeptScope()
{
    if (m_mgr)
    {
        m_mgr->m_keptSpell = m_previousSpell;
        m_mgr->m_kept = m_previousKept;
    }
}

void SpellModMgr::Clear()
{
    for (int i = 0; i < MAX_SPELLMOD; ++i)
    {
        m_mods[i].clear();
    }
}
