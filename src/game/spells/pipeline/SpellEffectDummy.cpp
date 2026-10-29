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
#include <vector>
#include <list>
#include <algorithm>
#include <ctime>
#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "spells/data/SpellMgr.h"
#include "Player.h"
#include "Unit.h"
#include "spells/pipeline/Spell.h"
#include "spells/auras/SpellAuras.h"
#include "SharedDefines.h"
#include "Creature.h"
#include "Totem.h"
#include "Language.h"
#include "Util.h"
#include "data/DbScriptStore.h"
#include "data/ScriptHooks.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "Geometry/Vector3.h"
#include "MotionMaster.h"
#include <random>
#include "spells/handlers/SpellEffectDummyHandlers.h"
#include "spells/data/SpellTables.h"

/**
 * @brief Executes spell-specific dummy effect behavior.
 *
 * @param effect The dummy effect index.
 */
void Spell::EffectDummy(SpellEffectEntry const* effect)
{
    if (!unitTarget && !gameObjTarget && !itemTarget)
    {
        return;
    }

    // selection by spell family
    switch(m_spellInfo->GetSpellFamilyName())
    {
        case SPELLFAMILY_GENERIC:
        {
            SpellEffectDummyGenericContext ctx(this, m_spellInfo, m_currentBasePoints, m_CastItem, m_caster,
                                               m_originalCasterGUID, m_originalCaster, m_duration, unitTarget,
                                               itemTarget, damage, m_UniqueTargetInfo, m_triggeredByAuraSpell, effect);
            if (SpellHandlerRegistry::Game().Dispatch<SpellEffectDummyGenericSite>(m_spellInfo->ID, ctx).IsReturn())
            {
                return;
            }
            break;
        }
        case SPELLFAMILY_MAGE:
        {
            SpellEffectDummyMageContext ctx(this, m_caster, unitTarget, damage, effect);
            if (SpellHandlerRegistry::Game().Dispatch<SpellEffectDummyMageSite>(m_spellInfo->ID, ctx).IsReturn())
            {
                return;
            }

            // Conjure Mana Gem
            if (effect->EffectIndex == EFFECT_INDEX_1 && m_spellInfo->GetSpellEffectIdByIndex(EFFECT_INDEX_0) == SPELL_EFFECT_CREATE_ITEM)
            {
                if (m_caster->GetTypeId() != TYPEID_PLAYER)
                {
                    return;
                }

                // checked in create item check, avoid unexpected
                if (Item* item = ((Player*)m_caster)->GetInventoryMgr().GetItemByLimitedCategory(ITEM_LIMIT_CATEGORY_MANA_GEM))
                    if (item->HasMaxCharges())
                    {
                        return;
                    }

                unitTarget->CastSpell( unitTarget, effect->CalculateSimpleValue(), true, m_CastItem);
                return;
            }
            break;
        }
        case SPELLFAMILY_WARRIOR:
        {
            SpellClassOptionsEntry const* warClassOptions = m_spellInfo->GetSpellClassOptions();
            // Charge
            if (warClassOptions && (warClassOptions->SpellClassMask & UI64LIT(0x1)) && m_spellInfo->SpellVisualID[0] == 867)
            {
                int32 chargeBasePoints0 = damage;
                m_caster->CastCustomSpell(m_caster, 34846, &chargeBasePoints0, NULL, NULL, true);
                return;
            }
            // Execute
            if (warClassOptions && warClassOptions->SpellClassMask & UI64LIT(0x20000000))
            {
                if (!unitTarget)
                {
                    return;
                }

                uint32 rage = m_caster->GetPower(POWER_RAGE);

                // up to max 30 rage cost
                if (rage > 300)
                {
                    rage = 300;
                }

                // Glyph of Execution bonus
                uint32 rage_modified = rage;

                if (Aura* aura = m_caster->GetDummyAura(58367))
                {
                    rage_modified +=  aura->GetModifier()->m_amount * 10;
                }

                int32 basePoints0 = damage+int32(rage_modified * effect->EffectChainAmplitude +
                                                 m_caster->GetTotalAttackPowerValue(BASE_ATTACK)*0.2f);

                m_caster->CastCustomSpell(unitTarget, 20647, &basePoints0, NULL, NULL, true, 0);

                // Sudden Death
                if (m_caster->HasAura(52437))
                {
                    Unit::AuraList const& auras = m_caster->GetAurasByType(SPELL_AURA_PROC_TRIGGER_SPELL);
                    for (Unit::AuraList::const_iterator itr = auras.begin(); itr != auras.end(); ++itr)
                    {
                        // Only Sudden Death have this SpellIconID with SPELL_AURA_PROC_TRIGGER_SPELL
                        if ((*itr)->GetSpellProto()->SpellIconID == 1989)
                        {
                            // saved rage top stored in next affect
                            uint32 lastrage = (*itr)->GetSpellProto()->CalculateSimpleValue(EFFECT_INDEX_1) * 10;
                            if (lastrage < rage)
                            {
                                rage -= lastrage;
                            }
                            break;
                        }
                    }
                }

                m_caster->SetPower(POWER_RAGE, m_caster->GetPower(POWER_RAGE) - rage);
                return;
            }
            // Slam
            if (warClassOptions && warClassOptions->SpellClassMask & UI64LIT(0x0000000000200000))
            {
                if (!unitTarget)
                {
                    return;
                }

                // dummy cast itself ignored by client in logs
                m_caster->CastCustomSpell(unitTarget, 50782, &damage, NULL, NULL, true);
                return;
            }
            // Concussion Blow
            if (warClassOptions && warClassOptions->SpellClassMask & UI64LIT(0x0000000004000000))
            {
                m_damage += uint32(damage * m_caster->GetTotalAttackPowerValue(BASE_ATTACK) / 100);
                return;
            }

            SpellEffectDummyWarriorContext ctx(m_caster, unitTarget, damage);
            if (SpellHandlerRegistry::Game().Dispatch<SpellEffectDummyWarriorSite>(m_spellInfo->ID, ctx).IsReturn())
            {
                return;
            }
            break;
        }
        case SPELLFAMILY_WARLOCK:
        {
            SpellClassOptionsEntry const* wrlClassOptions = m_spellInfo->GetSpellClassOptions();
            // Life Tap
            if (wrlClassOptions && wrlClassOptions->SpellClassMask & UI64LIT(0x0000000000040000))
            {
                if (unitTarget && (int32(unitTarget->GetHealth()) > damage))
                {
                    // Shouldn't Appear in Combat Log
                    unitTarget->ModifyHealth(-damage);

                    int32 spell_power = m_caster->SpellBaseDamageBonusDone(GetSpellSchoolMask(m_spellInfo));
                    int32 mana = damage + spell_power / 2;

                    // Improved Life Tap mod
                    Unit::AuraList const& auraDummy = m_caster->GetAurasByType(SPELL_AURA_DUMMY);
                    for(Unit::AuraList::const_iterator itr = auraDummy.begin(); itr != auraDummy.end(); ++itr)
                    {
                        if ((*itr)->GetSpellProto()->GetSpellFamilyName()==SPELLFAMILY_WARLOCK && (*itr)->GetSpellProto()->SpellIconID == 208)
                        {
                            mana = ((*itr)->GetModifier()->m_amount + 100)* mana / 100;
                        }
                    }
                    m_caster->CastCustomSpell(unitTarget, 31818, &mana, NULL, NULL, true);

                    // Mana Feed
                    int32 manaFeedVal = 0;
                    Unit::AuraList const& mod = m_caster->GetAurasByType(SPELL_AURA_ADD_FLAT_MODIFIER);
                    for (Unit::AuraList::const_iterator itr = mod.begin(); itr != mod.end(); ++itr)
                    {
                        if ((*itr)->GetSpellProto()->GetSpellFamilyName()==SPELLFAMILY_WARLOCK && (*itr)->GetSpellProto()->SpellIconID == 1982)
                        {
                            manaFeedVal+= (*itr)->GetModifier()->m_amount;
                        }
                    }
                    if (manaFeedVal > 0)
                    {
                        manaFeedVal = manaFeedVal * mana / 100;
                        m_caster->CastCustomSpell(m_caster, 32553, &manaFeedVal, NULL, NULL, true, NULL);
                    }
                }
                else
                {
                    SendCastResult(SPELL_FAILED_FIZZLE);
                }

                return;
            }
            break;
        }
        case SPELLFAMILY_PRIEST:
        {
            SpellClassOptionsEntry const* prtsClassOptions = m_spellInfo->GetSpellClassOptions();
            // Penance
            if (prtsClassOptions && prtsClassOptions->SpellClassMask & UI64LIT(0x0080000000000000))
            {
                if (!unitTarget)
                {
                    return;
                }

                SpellHurtHeal const* penance = FindPenanceSpells(m_spellInfo->ID);
                if (!penance)
                {
                    sLog.outError("Spell::EffectDummy: Spell %u Penance need set correct heal/damage spell", m_spellInfo->ID);
                    return;
                }

                // prevent interrupted message for main spell
                finish(true);

                // replace cast by selected spell, this also make it interruptible including target death case
                if (m_caster->IsFriendlyTo(unitTarget))
                {
                    m_caster->CastSpell(unitTarget, penance->heal, false);
                }
                else
                {
                    m_caster->CastSpell(unitTarget, penance->hurt, false);
                }

                return;
            }
            break;
        }
        case SPELLFAMILY_DRUID:
        {
            // Starfall
            if (m_spellInfo->IsFitToFamilyMask(UI64LIT(0x0000000000000000), 0x00000100))
            {
                // Shapeshifting into an animal form or mounting cancels the effect.
                if (m_caster->GetCreatureType() == CREATURE_TYPE_BEAST || m_caster->IsMounted())
                {
                    if (m_triggeredByAuraSpell)
                    {
                        m_caster->RemoveAurasDueToSpell(m_triggeredByAuraSpell->ID);
                    }
                    return;
                }

                // Any effect which causes you to lose control of your character will supress the starfall effect.
                // The same two-part test CanFreeMove() runs (Unit.h): kNoFreeMoveReasons plus the
                // published feign (IsFeigningDeath); CanFreeMove() itself does not apply here, as its
                // extra owner-guid check has no place in this caster-state test.
                if ((m_caster->GetMotionMaster()->Mobility().reasons & Motion::kNoFreeMoveReasons) ||
                    m_caster->IsFeigningDeath())
                {
                    return;
                }

                if (uint32 star = GetStarfallStarSpell(m_spellInfo->ID))
                {
                    m_caster->CastSpell(unitTarget, star, true);
                    return;
                }
                sLog.outError("Spell::EffectDummy: Unhandeled Starfall spell rank %u", m_spellInfo->ID);
                return;
            }
            break;
        }
        case SPELLFAMILY_ROGUE:
        {
            SpellEffectDummyRogueContext ctx(m_caster, unitTarget);
            if (SpellHandlerRegistry::Game().Dispatch<SpellEffectDummyRogueSite>(m_spellInfo->ID, ctx).IsReturn())
            {
                return;
            }
            break;
        }
        case SPELLFAMILY_HUNTER:
        {
            SpellClassOptionsEntry const* huntClassOptions = m_spellInfo->GetSpellClassOptions();
            // Steady Shot
            if (huntClassOptions && huntClassOptions->SpellClassMask & UI64LIT(0x100000000))
            {
                if (!unitTarget || !unitTarget->IsAlive())
                {
                    return;
                }

                bool found = false;

                // check dazed affect
                Unit::AuraList const& decSpeedList = unitTarget->GetAurasByType(SPELL_AURA_MOD_DECREASE_SPEED);
                for (Unit::AuraList::const_iterator iter = decSpeedList.begin(); iter != decSpeedList.end(); ++iter)
                {
                    if ((*iter)->GetSpellProto()->SpellIconID==15 && (*iter)->GetSpellProto()->GetDispel()==0)
                    {
                        found = true;
                        break;
                    }
                }

                if (found)
                {
                    m_damage += damage;
                }
                return;
            }

            // Disengage
            if (huntClassOptions && huntClassOptions->SpellClassMask & UI64LIT(0x0000400000000000))
            {
                Unit* target = unitTarget;
                uint32 spellid = GetDisengageSpell(m_spellInfo->ID);
                if (!spellid)
                {
                    sLog.outError("Spell %u not handled propertly in EffectDummy(Disengage)", m_spellInfo->ID);
                    return;
                }
                if (!target || !target->IsAlive())
                {
                    return;
                }
                m_caster->CastSpell(target, spellid, true, NULL);
            }

            SpellEffectDummyHunterContext ctx(m_caster, unitTarget, effect);
            if (SpellHandlerRegistry::Game().Dispatch<SpellEffectDummyHunterSite>(m_spellInfo->ID, ctx).IsReturn())
            {
                return;
            }
            break;
        }
        case SPELLFAMILY_PALADIN:
        {
            switch (m_spellInfo->SpellIconID)
            {
                case 156:                                   // Holy Shock
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    SpellHurtHeal const* holyShock = FindHolyShockSpells(m_spellInfo->ID);
                    if (!holyShock)
                    {
                        sLog.outError("Spell::EffectDummy: Spell %u not handled in HS", m_spellInfo->ID);
                        return;
                    }

                    if (m_caster->IsFriendlyTo(unitTarget))
                    {
                        m_caster->CastSpell(unitTarget, holyShock->heal, true);
                    }
                    else
                    {
                        m_caster->CastSpell(unitTarget, holyShock->hurt, true);
                    }

                    return;
                }
                case 561:                                   // Judgement of command
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    uint32 spell_id = m_currentBasePoints[effect->EffectIndex];
                    SpellEntry const* spell_proto = sSpellStore.LookupEntry(spell_id);
                    if (!spell_proto)
                    {
                        return;
                    }

                    m_caster->CastSpell(unitTarget, spell_proto, true, NULL);
                    return;
                }
            }

            SpellEffectDummyPaladinContext ctx(this, m_caster, unitTarget, m_spellInfo,
                                               m_currentBasePoints, m_UniqueTargetInfo, effect);
            if (SpellHandlerRegistry::Game().Dispatch<SpellEffectDummyPaladinSite>(m_spellInfo->ID, ctx).IsReturn())
            {
                return;
            }
            break;
        }
        case SPELLFAMILY_SHAMAN:
        {
            SpellClassOptionsEntry const* shamClassOptions = m_spellInfo->GetSpellClassOptions();
            // Cleansing Totem
            if (shamClassOptions && (shamClassOptions->SpellClassMask & UI64LIT(0x0000000004000000)) && m_spellInfo->SpellIconID==1673)
            {
                if (unitTarget)
                {
                    m_caster->CastSpell(unitTarget, 52025, true);
                }
                return;
            }
            // Healing Stream Totem
            if (shamClassOptions && shamClassOptions->SpellClassMask & UI64LIT(0x0000000000002000))
            {
                if (unitTarget)
                {
                    if (Unit* owner = m_caster->GetOwner())
                    {
                        // spell have SPELL_DAMAGE_CLASS_NONE and not get bonuses from owner, use main spell for bonuses
                        if (m_triggeredBySpellInfo)
                        {
                            damage = int32(owner->SpellHealingBonusDone(unitTarget, m_triggeredBySpellInfo, damage, HEAL));
                            damage = int32(unitTarget->SpellHealingBonusTaken(owner, m_triggeredBySpellInfo, damage, HEAL));
                        }

                        // Restorative Totems
                        Unit::AuraList const& mDummyAuras = owner->GetAurasByType(SPELL_AURA_DUMMY);
                        for (Unit::AuraList::const_iterator i = mDummyAuras.begin(); i != mDummyAuras.end(); ++i)
                        {
                            // only its have dummy with specific icon
                            if ((*i)->GetSpellProto()->GetSpellFamilyName() == SPELLFAMILY_SHAMAN && (*i)->GetSpellProto()->SpellIconID == 338)
                            {
                                damage += (*i)->GetModifier()->m_amount * damage / 100;
                            }
                        }

                        // Glyph of Healing Stream Totem
                        if (Aura* dummy = owner->GetDummyAura(55456))
                        {
                            damage += dummy->GetModifier()->m_amount * damage / 100;
                        }
                    }
                    m_caster->CastCustomSpell(unitTarget, 52042, &damage, NULL, NULL, true, 0, 0, m_originalCasterGUID);
                }
                return;
            }
            // Mana Spring Totem
            if (shamClassOptions && shamClassOptions->SpellClassMask & UI64LIT(0x0000000000004000))
            {
                if (!unitTarget || unitTarget->GetPowerType() != POWER_MANA)
                {
                    return;
                }
                m_caster->CastCustomSpell(unitTarget, 52032, &damage, 0, 0, true, 0, 0, m_originalCasterGUID);
                return;
            }
            // Flametongue Weapon Proc, Ranks
            if (shamClassOptions && shamClassOptions->SpellClassMask & UI64LIT(0x0000000000200000))
            {
                if (!m_CastItem)
                {
                    sLog.outError("Spell::EffectDummy: spell %i requires cast Item", m_spellInfo->ID);
                    return;
                }
                // found spelldamage coefficients of 0.381% per 0.1 speed and 15.244 per 4.0 speed
                // but own calculation say 0.385 gives at most one point difference to published values
                int32 spellDamage = m_caster->SpellBaseDamageBonusDone(GetSpellSchoolMask(m_spellInfo));
                float weaponSpeed = (1.0f / IN_MILLISECONDS) * m_CastItem->GetProto()->Delay;
                int32 totalDamage = int32((damage + 3.85f * spellDamage) * 0.01 * weaponSpeed);

                m_caster->CastCustomSpell(unitTarget, 10444, &totalDamage, NULL, NULL, true, m_CastItem);
                return;
            }
            if (m_spellInfo->ID == 39610)                   // Mana Tide Totem effect
            {
                if (!unitTarget || unitTarget->GetPowerType() != POWER_MANA)
                {
                    return;
                }

                // Glyph of Mana Tide
                if (Unit* owner = m_caster->GetOwner())
                    if (Aura* dummy = owner->GetDummyAura(55441))
                    {
                        damage += dummy->GetModifier()->m_amount;
                    }
                // Regenerate 6% of Total Mana Every 3 secs
                int32 EffectBasePoints0 = unitTarget->GetMaxPower(POWER_MANA)  * damage / 100;
                m_caster->CastCustomSpell(unitTarget, 39609, &EffectBasePoints0, NULL, NULL, true, NULL, NULL, m_originalCasterGUID);
                return;
            }
            // Lava Lash
            if (m_spellInfo->IsFitToFamilyMask(UI64LIT(0x0000000000000000), 0x00000004))
            {
                if (m_caster->GetTypeId() != TYPEID_PLAYER)
                {
                    return;
                }
                Item* item = ((Player*)m_caster)->GetInventoryMgr().GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND);
                if (item)
                {
                    // Damage is increased if your off-hand weapon is enchanted with Flametongue.
                    Unit::AuraList const& auraDummy = m_caster->GetAurasByType(SPELL_AURA_DUMMY);
                    for (Unit::AuraList::const_iterator itr = auraDummy.begin(); itr != auraDummy.end(); ++itr)
                    {
                        if ((*itr)->GetSpellProto()->IsFitToFamily(SPELLFAMILY_SHAMAN, UI64LIT(0x0000000000200000)) &&
                            (*itr)->GetCastItemGuid() == item->GetObjectGuid())
                        {
                            m_damage += m_damage * damage / 100;
                            return;
                        }
                    }
                }
                return;
            }
            // Fire Nova
            if (m_spellInfo->SpellIconID == 33)
            {
                // fire totems slot
                Totem* totem = m_caster->GetTotem(TOTEM_SLOT_FIRE);
                if (!totem)
                {
                    return;
                }

                uint32 triggered_spell_id = GetFireNovaSpell(m_spellInfo->ID);
                if (!triggered_spell_id)
                {
                    return;
                }

                totem->CastSpell(totem, triggered_spell_id, true, NULL, NULL, m_caster->GetObjectGuid());

                // Fire Nova Visual
                totem->CastSpell(totem, 19823, true, NULL, NULL, m_caster->GetObjectGuid());
                return;
            }
            break;
        }
        case SPELLFAMILY_DEATHKNIGHT:
        {
            SpellClassOptionsEntry const* dkClassOptions = m_spellInfo->GetSpellClassOptions();
            // Death Coil
            if (dkClassOptions && dkClassOptions->SpellClassMask & UI64LIT(0x002000))
            {
                if (m_caster->IsFriendlyTo(unitTarget))
                {
                    if (!unitTarget || unitTarget->GetCreatureType() != CREATURE_TYPE_UNDEAD)
                    {
                        return;
                    }

                    int32 bp = int32(damage * 1.5f);
                    m_caster->CastCustomSpell(unitTarget, 47633, &bp, NULL, NULL, true);
                }
                else
                {
                    int32 bp = damage;
                    m_caster->CastCustomSpell(unitTarget, 47632, &bp, NULL, NULL, true);
                }
                return;
            }
            // Hungering Cold
            else if (dkClassOptions && dkClassOptions->SpellClassMask & UI64LIT(0x0000100000000000))
            {
                m_caster->CastSpell(m_caster, 51209, true);
                return;
            }
            // Death Strike
            else if (dkClassOptions && dkClassOptions->SpellClassMask & UI64LIT(0x0000000000000010))
            {
                uint32 count = 0;
                Unit::SpellAuraHolderMap const& auras = unitTarget->GetSpellAuraHolderMap();
                for (Unit::SpellAuraHolderMap::const_iterator itr = auras.begin(); itr != auras.end(); ++itr)
                {
                    if (itr->second->GetSpellProto()->GetDispel() == DISPEL_DISEASE &&
                        itr->second->GetCasterGuid() == m_caster->GetObjectGuid())
                    {
                        ++count;
                        // max. 15%
                        if (count == 3)
                        {
                            break;
                        }
                    }
                }

                SpellEffectEntry const* dsSpellEffect = m_spellInfo->GetSpellEffect(EFFECT_INDEX_0);
                int32 bp = int32(count * m_caster->GetMaxHealth() * (dsSpellEffect ? dsSpellEffect->EffectChainAmplitude : 0.0f) / 100);

                // Improved Death Strike (percent stored in nonexistent EFFECT_INDEX_2 effect base points)
                Unit::AuraList const& auraMod = m_caster->GetAurasByType(SPELL_AURA_ADD_FLAT_MODIFIER);
                for (Unit::AuraList::const_iterator iter = auraMod.begin(); iter != auraMod.end(); ++iter)
                {
                    // only required spell have spellicon for SPELL_AURA_ADD_FLAT_MODIFIER
                    if ((*iter)->GetSpellProto()->SpellIconID == 2751 && (*iter)->GetSpellProto()->GetSpellFamilyName() == SPELLFAMILY_DEATHKNIGHT)
                    {
                        bp += (*iter)->GetSpellProto()->CalculateSimpleValue(EFFECT_INDEX_2) * bp / 100;
                        break;
                    }
                }

                m_caster->CastCustomSpell(m_caster, 45470, &bp, NULL, NULL, true);
                return;
            }
            // Death Grip
            else if (m_spellInfo->ID == 49576)
            {
                if (!unitTarget)
                {
                    return;
                }

                m_caster->CastSpell(unitTarget, 49560, true);
                return;
            }
            // Death Grip
            else if (m_spellInfo->ID == 49560)
            {
                if (!unitTarget || unitTarget == m_caster)
                {
                    return;
                }

                uint32 spellId = m_spellInfo->CalculateSimpleValue(EFFECT_INDEX_0);
                float dest_x, dest_y;
                const Geometry::Vector3 near_ = PointNear(*m_caster, m_caster->Where().Extent() + unitTarget->Where().Extent(), m_caster->Where().Facing());
                dest_x = near_.x;
                dest_y = near_.y;
                unitTarget->CastSpell(dest_x, dest_y, m_caster->Where().Z() + 0.5f, spellId, true, NULL, NULL, m_caster->GetObjectGuid(), m_spellInfo);
                return;
            }
            // Obliterate
            else if (dkClassOptions && dkClassOptions->SpellClassMask & UI64LIT(0x0002000000000000))
            {
                // search for Annihilation
                Unit::AuraList const& dummyList = m_caster->GetAurasByType(SPELL_AURA_DUMMY);
                for (Unit::AuraList::const_iterator itr = dummyList.begin(); itr != dummyList.end(); ++itr)
                {
                    if ((*itr)->GetSpellProto()->GetSpellFamilyName() == SPELLFAMILY_DEATHKNIGHT && (*itr)->GetSpellProto()->SpellIconID == 2710)
                    {
                        if (roll_chance_i((*itr)->GetModifier()->m_amount)) // don't consume if found
                        {
                            return;
                        }
                        else
                        {
                            break;
                        }
                    }
                }

                // consume diseases
                unitTarget->RemoveAurasWithDispelType(DISPEL_DISEASE, m_caster->GetObjectGuid());
            }
            break;
        }
    }

    // pet auras
    if (PetAura const* petSpell = sSpellMgr.GetPetAura(m_spellInfo->ID, SpellEffectIndex(effect->EffectIndex)))
    {
        m_caster->AddPetAura(petSpell);
        return;
    }

    // Script based implementation. Must be used only for not good for implementation in core spell effects
    // So called only for not processed cases
    bool libraryResult = false;
    if (gameObjTarget)
    {
        libraryResult = sScriptHooks.OnEffectDummy(m_caster, m_spellInfo->ID, SpellEffectIndex(effect->EffectIndex), gameObjTarget, m_originalCasterGUID);
    }
    else if (unitTarget && unitTarget->GetTypeId() == TYPEID_UNIT)
    {
        libraryResult = sScriptHooks.OnEffectDummy(m_caster, m_spellInfo->ID, SpellEffectIndex(effect->EffectIndex), (Creature*)unitTarget, m_originalCasterGUID);
    }
    else if (itemTarget)
    {
        libraryResult = sScriptHooks.OnEffectDummy(m_caster, m_spellInfo->ID, SpellEffectIndex(effect->EffectIndex), itemTarget, m_originalCasterGUID);
    }

    if (libraryResult || !unitTarget)
    {
        return;
    }

    // Previous effect might have started script
    if (!DbScriptStore::CanSpellEffectStartDBScript(m_spellInfo, SpellEffectIndex(effect->EffectIndex)))
    {
        return;
    }

    DEBUG_FILTER_LOG(LOG_FILTER_SPELL_CAST, "Spell ScriptStart spellid %u in EffectDummy", m_spellInfo->ID);
    m_caster->GetMap()->ScriptsStart(DBS_ON_SPELL, m_spellInfo->ID, m_caster, unitTarget);
}
