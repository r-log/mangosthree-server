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

#include <iterator>
#include "Platform/Define.h"
#include "Common/TimeConstants.h"
#include "Utilities/MathDefines.h"
#include <cstdlib>
#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "spells/data/SpellMgr.h"
#include "Unit.h"
#include "spells/pipeline/Spell.h"
#include "spells/auras/SpellAuras.h"
#include "Group.h"
#include "SharedDefines.h"
#include "Creature.h"
#include "Language.h"
#include "Util.h"
#include "data/DbScriptStore.h"
#include "data/ScriptHooks.h"
#include "GridNotifiersImpl.h"
#include "Geometry/Vector3.h"
#include <random>
#include "spells/handlers/SpellEffectScriptHandlers.h"
#include "spells/data/SpellTables.h"

/**
 * @brief Executes script-driven spell effect behavior for special cases.
 *
 * @param effect The script effect index.
 */
void Spell::EffectScriptEffect(SpellEffectEntry const* effect)
{
    // TODO: we must implement hunter pet summon at login there (spell 6962)
    switch(m_spellInfo->GetSpellFamilyName())
    {
        case SPELLFAMILY_GENERIC:
        {
            SpellEffectScriptContext ctx(this, m_spellInfo, m_targets, m_caster, m_originalCasterGUID, m_originalCaster,
                                         unitTarget, itemTarget, damage, effect);
            if (SpellHandlerRegistry::Game().Dispatch<SpellEffectScriptGenericSite>(m_spellInfo->ID, ctx).IsReturn())
            {
                return;
            }
            break;
        }
        case SPELLFAMILY_WARLOCK:
        {
            SpellEffectScriptContext ctx(this, m_spellInfo, m_targets, m_caster, m_originalCasterGUID, m_originalCaster,
                                         unitTarget, itemTarget, damage, effect);
            if (SpellHandlerRegistry::Game().Dispatch<SpellEffectScriptWarlockSite>(m_spellInfo->ID, ctx).IsReturn())
            {
                return;
            }
            break;
        }
        case SPELLFAMILY_PRIEST:
        {
            SpellEffectScriptContext ctx(this, m_spellInfo, m_targets, m_caster, m_originalCasterGUID, m_originalCaster,
                                         unitTarget, itemTarget, damage, effect);
            if (SpellHandlerRegistry::Game().Dispatch<SpellEffectScriptPriestSite>(m_spellInfo->ID, ctx).IsReturn())
            {
                return;
            }
            break;
        }
        case SPELLFAMILY_HUNTER:
        {
            SpellEffectScriptContext ctx(this, m_spellInfo, m_targets, m_caster, m_originalCasterGUID, m_originalCaster,
                                         unitTarget, itemTarget, damage, effect);
            if (SpellHandlerRegistry::Game().Dispatch<SpellEffectScriptHunterSite>(m_spellInfo->ID, ctx).IsReturn())
            {
                return;
            }
            break;
        }
        case SPELLFAMILY_PALADIN:
        {
            // Judgement (seal trigger)
            if (m_spellInfo->GetCategory() == SPELLCATEGORY_JUDGEMENT)
            {
                if (!unitTarget || !unitTarget->IsAlive())
                {
                    return;
                }

                // Judgement self add
                uint32 spellId1 = GetJudgementSelfSpell(m_spellInfo->ID);
                uint32 spellId2 = 0;
                if (!spellId1)
                {
                    sLog.outError("Unsupported Judgement (seal trigger) spell (Id: %u) in Spell::EffectScriptEffect", m_spellInfo->ID);
                    return;
                }

                // offensive seals have aura dummy in 2 effect
                Unit::AuraList const& m_dummyAuras = m_caster->GetAurasByType(SPELL_AURA_DUMMY);
                for (Unit::AuraList::const_iterator itr = m_dummyAuras.begin(); itr != m_dummyAuras.end(); ++itr)
                {
                    // search seal (offensive seals have judgement's aura dummy spell id in 2 effect
                    if ((*itr)->GetEffIndex() != EFFECT_INDEX_2 || !IsSealSpell((*itr)->GetSpellProto()))
                    {
                        continue;
                    }
                    spellId2 = (*itr)->GetModifier()->m_amount;
                    SpellEntry const* judge = sSpellStore.LookupEntry(spellId2);
                    if (!judge)
                    {
                        continue;
                    }
                    break;
                }

                // if there were no offensive seals than there is seal with proc trigger aura
                if (!spellId2)
                {
                    Unit::AuraList const& procTriggerAuras = m_caster->GetAurasByType(SPELL_AURA_PROC_TRIGGER_SPELL);
                    for (Unit::AuraList::const_iterator itr = procTriggerAuras.begin(); itr != procTriggerAuras.end(); ++itr)
                    {
                        if ((*itr)->GetEffIndex() != EFFECT_INDEX_0 || !IsSealSpell((*itr)->GetSpellProto()))
                        {
                            continue;
                        }
                        spellId2 = 20271;
                        break;
                    }
                }

                if (spellId1)
                {
                    m_caster->CastSpell(unitTarget, spellId1, true);
                }

                if (spellId2)
                {
                    m_caster->CastSpell(unitTarget, spellId2, true);
                }

                return;
            }
            break;
        }
        case SPELLFAMILY_POTION:
        {
            SpellEffectScriptContext ctx(this, m_spellInfo, m_targets, m_caster, m_originalCasterGUID, m_originalCaster,
                                         unitTarget, itemTarget, damage, effect);
            if (SpellHandlerRegistry::Game().Dispatch<SpellEffectScriptPotionSite>(m_spellInfo->ID, ctx).IsReturn())
            {
                return;
            }
            break;
        }
        case SPELLFAMILY_DEATHKNIGHT:
        {
            SpellEffectScriptContext ctx(this, m_spellInfo, m_targets, m_caster, m_originalCasterGUID, m_originalCaster,
                                         unitTarget, itemTarget, damage, effect);
            if (SpellHandlerRegistry::Game().Dispatch<SpellEffectScriptDeathKnightSite>(m_spellInfo->ID, ctx).IsReturn())
            {
                return;
            }
            break;
        }
    }

    // normal DB scripted effect
    if (!unitTarget)
    {
        return;
    }

    // Script based implementation. Must be used only for not good for implementation in core spell effects
    // So called only for not processed cases
    if (unitTarget->GetTypeId() == TYPEID_UNIT)
    {
        if (sScriptHooks.OnEffectScriptEffect(m_caster, m_spellInfo->ID, SpellEffectIndex(effect->EffectIndex), (Creature*)unitTarget, m_originalCasterGUID))
        {
            return;
        }
    }

    // Previous effect might have started script
    if (!DbScriptStore::CanSpellEffectStartDBScript(m_spellInfo, SpellEffectIndex(effect->EffectIndex)))
    {
        return;
    }

    DEBUG_FILTER_LOG(LOG_FILTER_SPELL_CAST, "Spell ScriptStart spellid %u in EffectScriptEffect", m_spellInfo->ID);
    m_caster->GetMap()->ScriptsStart(DBS_ON_SPELL, m_spellInfo->ID, m_caster, unitTarget);
}
