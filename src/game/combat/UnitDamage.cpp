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

#include <algorithm>
#include "Unit.h"
#include "combat/ArmorReduction.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "GridMap.h"
#include "ObjectGuid.h"
#include "spells/data/SpellMgr.h"
#include "QuestDef.h"
#include "Player.h"
#include "Creature.h"
#include "CreatureAI.h"
#include "InstanceData.h"
#include "Map.h"
#include "spells/pipeline/Spell.h"
#include "spells/auras/SpellAuras.h"
#include "data/BattleGroundHooks.h"
#include "data/OutdoorPvPHooks.h"
#include "data/GroupHooks.h"
#include "Formulas.h"
#include "Util.h"
#include "GridNotifiersImpl.h"
#include "movement/MoveSpline.h"
#include "CreatureLinkingMgr.h"
#include "GameTime.h"

#include <math.h>
#include <stdarg.h>

/**
 * @brief Reduces physical damage by the victim's effective armor.
 *
 * @param pVictim The victim whose armor is used.
 * @param damage The incoming physical damage.
 * @return The reduced damage amount.
 */
uint32 Unit::CalcArmorReducedDamage(Unit* pVictim, const uint32 damage)
{
    uint32 const victimArmor = pVictim->GetArmor();
    int32 const targetResistanceMod = GetTotalAuraModifierByMiscMask(SPELL_AURA_MOD_TARGET_RESISTANCE, SPELL_SCHOOL_MASK_NORMAL);
    bool const isPlayer = GetTypeId() == TYPEID_PLAYER;
    uint32 const victimLevel = pVictim->getLevel(); // read twice by the body
    uint32 const attackerLevel = getLevel(); // read twice by the body

    float armorPenetrationPct = 0.0f;
    if (isPlayer)
    {
        armorPenetrationPct = GetArmorPenetrationPct();
    }

    return Combat::ArmorReducedDamage(damage, victimArmor, targetResistanceMod, isPlayer,
                                      attackerLevel, victimLevel, armorPenetrationPct);
}

/**
 * @brief Calculates resistance, absorbs, and split-damage effects for incoming damage.
 *
 * @param pCaster The attacking caster.
 * @param schoolMask The incoming damage school mask.
 * @param damagetype The damage effect type.
 * @param damage The incoming damage amount.
 * @param absorb Output absorbed amount.
 * @param resist Output resisted amount.
 * @param canReflect Unused reflection flag placeholder.
 */
void Unit::CalculateDamageAbsorbAndResist(Unit* pCaster, SpellSchoolMask schoolMask, DamageEffectType damagetype, const uint32 damage, uint32* absorb, uint32* resist, bool canReflect)
{
    if (!pCaster || !IsAlive() || !damage)
    {
        return;
    }

    // Magic damage, check for resists
    if ((schoolMask & SPELL_SCHOOL_MASK_NORMAL) == 0)
    {
        // Get base victim resistance for school
        float tmpvalue2 = (float)GetResistance(GetFirstSchoolInMask(schoolMask));
        // Ignore resistance by self SPELL_AURA_MOD_TARGET_RESISTANCE aura
        tmpvalue2 += (float)pCaster->GetTotalAuraModifierByMiscMask(SPELL_AURA_MOD_TARGET_RESISTANCE, schoolMask);

        if (Player* casterPlayer = pCaster->ToPlayer())
        {
            tmpvalue2 -= (float)casterPlayer->GetSpellPenetrationItemMod();
        }

        tmpvalue2 *= (float)(0.15f / getLevel());
        if (tmpvalue2 < 0.0f)
        {
            tmpvalue2 = 0.0f;
        }
        if (tmpvalue2 > 0.75f)
        {
            tmpvalue2 = 0.75f;
        }
        uint32 ran = urand(0, 100);
        float faq[4] = {24.0f, 6.0f, 4.0f, 6.0f};
        uint8 m = 0;
        float Binom = 0.0f;
        for (uint8 i = 0; i < 4; ++i)
        {
            Binom += 2400 * (powf(tmpvalue2, float(i)) * powf((1 - tmpvalue2), float(4 - i))) / faq[i];
            if (ran > Binom)
            {
                ++m;
            }
            else
            {
                break;
            }
        }
        if (damagetype == DOT && m == 4)
        {
            *resist += uint32(damage - 1);
        }
        else
        {
            *resist += uint32(damage * m / 4);
        }
        if (*resist > damage)
        {
            *resist = damage;
        }
    }
    else
    {
        *resist = 0;
    }

    int32 RemainingDamage = damage - *resist;

    // Get unit state (need for some absorb check)
    uint32 unitflag = GetUInt32Value(UNIT_FIELD_FLAGS);
    // Reflect damage spells (not cast any damage spell in aura lookup)
    uint32 reflectSpell = 0;
    int32  reflectDamage = 0;
    Aura*  reflectTriggeredBy = NULL;                       // expected as not expired at reflect as in current cases
    // Death Prevention Aura
    SpellEntry const*  preventDeathSpell = NULL;
    int32  preventDeathAmount = 0;

    // full absorb cases (by chance)
    AuraList const& vAbsorb = GetAurasByType(SPELL_AURA_SCHOOL_ABSORB);
    for (AuraList::const_iterator i = vAbsorb.begin(); i != vAbsorb.end() && RemainingDamage > 0; ++i)
    {
        // only work with proper school mask damage
        Modifier* i_mod = (*i)->GetModifier();
        if (!(i_mod->m_miscvalue & schoolMask))
        {
            continue;
        }

        SpellEntry const* i_spellProto = (*i)->GetSpellProto();
        SpellClassOptionsEntry const* adsClassOptions = i_spellProto->GetSpellClassOptions();
        // Fire Ward or Frost Ward
        if (adsClassOptions && adsClassOptions->SpellClassSet == SPELLFAMILY_MAGE && adsClassOptions->SpellClassMask & UI64LIT(0x0000000000000108))
        {
            int chance = 0;
            Unit::AuraList const& auras = GetAurasByType(SPELL_AURA_ADD_PCT_MODIFIER);
            for (Unit::AuraList::const_iterator itr = auras.begin(); itr != auras.end(); ++itr)
            {
                SpellEntry const* itr_spellProto = (*itr)->GetSpellProto();
                // Frost Warding (chance full absorb)
                if (itr_spellProto->GetSpellFamilyName() == SPELLFAMILY_MAGE && itr_spellProto->SpellIconID == 501)
                {
                    // chance stored in next dummy effect
                    chance = itr_spellProto->CalculateSimpleValue(EFFECT_INDEX_1);
                    break;
                }
            }
            if (roll_chance_i(chance))
            {
                int32 amount = RemainingDamage;
                RemainingDamage = 0;

                // Frost Warding (mana regen)
                CastCustomSpell(this, 57776, &amount, NULL, NULL, true, NULL, *i);
                break;
            }
        }
    }

    // Need remove expired auras after
    bool existExpired = false;

    // Incanter's Absorption, for converting to spell power
    int32 incanterAbsorption = 0;

    // absorb without mana cost
    AuraList const& vSchoolAbsorb = GetAurasByType(SPELL_AURA_SCHOOL_ABSORB);
    for (AuraList::const_iterator i = vSchoolAbsorb.begin(); i != vSchoolAbsorb.end() && RemainingDamage > 0; ++i)
    {
        Modifier* mod = (*i)->GetModifier();
        if (!(mod->m_miscvalue & schoolMask))
        {
            continue;
        }

        SpellEntry const* spellProto = (*i)->GetSpellProto();

        // Max Amount can be absorbed by this aura
        int32  currentAbsorb = mod->m_amount;

        // Found empty aura (impossible but..)
        if (currentAbsorb <= 0)
        {
            existExpired = true;
            continue;
        }

        // Handle custom absorb auras
        // TODO: try find better way
        SpellClassOptionsEntry const* classOptions = spellProto->GetSpellClassOptions();

        switch(spellProto->GetSpellFamilyName())
        {
            case SPELLFAMILY_GENERIC:
            {
                // Astral Shift
                if (spellProto->SpellIconID == 3066)
                {
                    // reduces all damage taken while stun, fear or silence
                    if (unitflag & (UNIT_FLAG_STUNNED | UNIT_FLAG_FLEEING | UNIT_FLAG_SILENCED))
                    {
                        RemainingDamage -= RemainingDamage * currentAbsorb / 100;
                    }
                    continue;
                }
                // Nerves of Steel
                if (spellProto->SpellIconID == 2115)
                {
                    // while affected by Stun and Fear
                    if (unitflag & (UNIT_FLAG_STUNNED | UNIT_FLAG_FLEEING))
                    {
                        RemainingDamage -= RemainingDamage * currentAbsorb / 100;
                    }
                    continue;
                }
                // Spell Deflection
                if (spellProto->SpellIconID == 3006)
                {
                    // You have a chance equal to your Parry chance
                    if (damagetype == SPELL_DIRECT_DAMAGE &&// Only for direct spell damage
                            roll_chance_f(GetUnitParryChance()))             // Roll chance
                        RemainingDamage -= RemainingDamage * currentAbsorb / 100;
                    continue;
                }
                // Reflective Shield (Lady Malande boss)
                if (spellProto->ID == 41475 && canReflect)
                {
                    if (RemainingDamage < currentAbsorb)
                    {
                        reflectDamage = RemainingDamage / 2;
                    }
                    else
                    {
                        reflectDamage = currentAbsorb / 2;
                    }
                    reflectSpell = 33619;
                    reflectTriggeredBy = *i;
                    reflectTriggeredBy->SetInUse(true);     // lock aura from final deletion until processing
                    break;
                }
                if (spellProto->ID == 39228 ||              // Argussian Compass
                        spellProto->ID == 60218)            // Essence of Gossamer
                {
                    // Max absorb stored in 1 dummy effect
                    int32 max_absorb = spellProto->CalculateSimpleValue(EFFECT_INDEX_1);
                    if (max_absorb < currentAbsorb)
                    {
                        currentAbsorb = max_absorb;
                    }
                    break;
                }
                break;
            }
            case SPELLFAMILY_DRUID:
            {
                // Primal Tenacity
                if (spellProto->SpellIconID == 2253)
                {
                    // reduces all damage taken while Stunned and in Cat Form
                    if (GetShapeshiftForm() == FORM_CAT && (unitflag & UNIT_FLAG_STUNNED))
                    {
                        RemainingDamage -= RemainingDamage * currentAbsorb / 100;
                    }
                    continue;
                }
                // Moonkin Form passive
                if (spellProto->ID == 69366)
                {
                    // reduces all damage taken while Stunned
                    if (unitflag & UNIT_FLAG_STUNNED)
                    {
                        RemainingDamage -= RemainingDamage * currentAbsorb / 100;
                    }
                    continue;
                }
                break;
            }
            case SPELLFAMILY_ROGUE:
            {
                // Cheat Death (make less prio with Guardian Spirit case)
                if (spellProto->SpellIconID == 2109)
                {
                    if (!preventDeathSpell &&
                            GetTypeId() == TYPEID_PLAYER && // Only players
                            !m_spellCooldownMgr.HasSpellCooldown(31231, time(NULL)) &&
                            // Only if no cooldown
                            roll_chance_i((*i)->GetModifier()->m_amount))
                        // Only if roll
                    {
                        preventDeathSpell = (*i)->GetSpellProto();
                    }
                    // always skip this spell in charge dropping, absorb amount calculation since it has chance as m_amount and doesn't need to absorb any damage
                    continue;
                }
                break;
            }
            case SPELLFAMILY_PRIEST:
            {
                // Guardian Spirit
                if (spellProto->SpellIconID == 2873)
                {
                    preventDeathSpell = (*i)->GetSpellProto();
                    preventDeathAmount = (*i)->GetModifier()->m_amount;
                    continue;
                }
                // Reflective Shield
                if (spellProto->IsFitToFamilyMask(UI64LIT(0x0000000000000001)) && canReflect)
                {
                    if (pCaster == this)
                    {
                        break;
                    }
                    Unit* caster = (*i)->GetCaster();
                    if (!caster)
                    {
                        break;
                    }
                    AuraList const& vOverRideCS = caster->GetAurasByType(SPELL_AURA_DUMMY);
                    for (AuraList::const_iterator k = vOverRideCS.begin(); k != vOverRideCS.end(); ++k)
                    {
                        switch ((*k)->GetModifier()->m_miscvalue)
                        {
                            case 5065:                      // Rank 1
                            case 5064:                      // Rank 2
                            {
                                if (RemainingDamage >= currentAbsorb)
                                {
                                    reflectDamage = (*k)->GetModifier()->m_amount * currentAbsorb / 100;
                                }
                                else
                                {
                                    reflectDamage = (*k)->GetModifier()->m_amount * RemainingDamage / 100;
                                }
                                reflectSpell = 33619;
                                reflectTriggeredBy = *i;
                                reflectTriggeredBy->SetInUse(true);// lock aura from final deletion until processing
                            } break;
                            default: break;
                        }
                    }
                    break;
                }
                break;
            }
            case SPELLFAMILY_SHAMAN:
            {
                // Astral Shift
                if (spellProto->SpellIconID == 3066)
                {
                    // reduces all damage taken while stun, fear or silence
                    if (unitflag & (UNIT_FLAG_STUNNED | UNIT_FLAG_FLEEING | UNIT_FLAG_SILENCED))
                    {
                        RemainingDamage -= RemainingDamage * currentAbsorb / 100;
                    }
                    continue;
                }
                break;
            }
            case SPELLFAMILY_DEATHKNIGHT:
            {
                // Shadow of Death
                if (spellProto->SpellIconID == 1958)
                {
                    // TODO: absorb only while transform
                    continue;
                }
                // Anti-Magic Shell (on self)
                if (spellProto->ID == 48707)
                {
                    // damage absorbed by Anti-Magic Shell energizes the DK with additional runic power.
                    // This, if I'm not mistaken, shows that we get back ~2% of the absorbed damage as runic power.
                    int32 absorbed = RemainingDamage * currentAbsorb / 100;
                    int32 regen = absorbed * 2 / 10;
                    CastCustomSpell(this, 49088, &regen, NULL, NULL, true, NULL, *i);
                    RemainingDamage -= absorbed;
                    continue;
                }
                // Anti-Magic Shell (on single party/raid member)
                if (spellProto->ID == 50462)
                {
                    RemainingDamage -= RemainingDamage * currentAbsorb / 100;
                    continue;
                }
                // Anti-Magic Zone
                if (spellProto->ID == 50461)
                {
                    Unit* caster = (*i)->GetCaster();
                    if (!caster)
                    {
                        continue;
                    }
                    int32 absorbed = RemainingDamage * currentAbsorb / 100;
                    int32 canabsorb = caster->GetHealth();
                    if (canabsorb < absorbed)
                    {
                        absorbed = canabsorb;
                    }

                    RemainingDamage -= absorbed;

                    uint32 ab_damage = absorbed;
                    pCaster->DealDamageMods(caster, ab_damage, NULL);
                    pCaster->DealDamage(caster, ab_damage, NULL, damagetype, schoolMask, 0, false);
                    continue;
                }
                break;
            }
            default:
                break;
        }

        // currentAbsorb - damage can be absorbed by shield
        // If need absorb less damage
        if (RemainingDamage < currentAbsorb)
        {
            currentAbsorb = RemainingDamage;
        }

        RemainingDamage -= currentAbsorb;

        // Fire Ward or Frost Ward or Ice Barrier (or Mana Shield)
        // for Incanter's Absorption converting to spell power
        if (spellProto->IsFitToFamily(SPELLFAMILY_MAGE, UI64LIT(0x0000000000000000), 0x00000008))
        {
            incanterAbsorption += currentAbsorb;
        }

        // Reduce shield amount
        mod->m_amount -= currentAbsorb;
        if ((*i)->GetHolder()->DropAuraCharge())
        {
            mod->m_amount = 0;
        }
        // Need remove it later
        if (mod->m_amount <= 0)
        {
            existExpired = true;
        }
    }

    // Remove all expired absorb auras
    if (existExpired)
    {
        for (AuraList::const_iterator i = vSchoolAbsorb.begin(); i != vSchoolAbsorb.end();)
        {
            if ((*i)->GetModifier()->m_amount <= 0)
            {
                RemoveAurasDueToSpell((*i)->GetId(), NULL, AURA_REMOVE_BY_SHIELD_BREAK);
                i = vSchoolAbsorb.begin();
            }
            else
            {
                ++i;
            }
        }
    }

    // Cast back reflect damage spell
    if (canReflect && reflectSpell)
    {
        CastCustomSpell(pCaster, reflectSpell, &reflectDamage, NULL, NULL, true, NULL, reflectTriggeredBy);
        reflectTriggeredBy->SetInUse(false);                // free lock from deletion
    }

    // absorb by mana cost
    AuraList const& vManaShield = GetAurasByType(SPELL_AURA_MANA_SHIELD);
    for (AuraList::const_iterator i = vManaShield.begin(), next; i != vManaShield.end() && RemainingDamage > 0; i = next)
    {
        next = i; ++next;

        // check damage school mask
        if (((*i)->GetModifier()->m_miscvalue & schoolMask) == 0)
        {
            continue;
        }

        int32 currentAbsorb;
        if (RemainingDamage >= (*i)->GetModifier()->m_amount)
        {
            currentAbsorb = (*i)->GetModifier()->m_amount;
        }
        else
        {
            currentAbsorb = RemainingDamage;
        }

        SpellEffectEntry const* spellEffect = (*i)->GetSpellProto()->GetSpellEffect((*i)->GetEffIndex());

        if (float manaMultiplier = (spellEffect ? spellEffect->EffectAmplitude : 0))
        {
            if (SpellModMgr* modOwner = GetSpellMods())
            {
                modOwner->ApplySpellMod((*i)->GetId(), SPELLMOD_MULTIPLE_VALUE, manaMultiplier);
            }

            int32 maxAbsorb = int32(GetPower(POWER_MANA) / manaMultiplier);
            if (currentAbsorb > maxAbsorb)
            {
                currentAbsorb = maxAbsorb;
            }

            int32 manaReduction = int32(currentAbsorb * manaMultiplier);
            ApplyPowerMod(POWER_MANA, manaReduction, false);
        }

        // Mana Shield (or Fire Ward or Frost Ward or Ice Barrier)
        // for Incanter's Absorption converting to spell power
        if ((*i)->GetSpellProto()->IsFitToFamily(SPELLFAMILY_MAGE, UI64LIT(0x0000000000000000), 0x000008))
        {
            incanterAbsorption += currentAbsorb;
        }

        (*i)->GetModifier()->m_amount -= currentAbsorb;
        if ((*i)->GetModifier()->m_amount <= 0)
        {
            RemoveAurasDueToSpell((*i)->GetId());
            next = vManaShield.begin();
        }

        RemainingDamage -= currentAbsorb;
    }

    // effects dependent from full absorb amount
    // Incanter's Absorption, if have affective absorbing
    if (incanterAbsorption)
    {
        Unit::AuraList const& auras = GetAurasByType(SPELL_AURA_DUMMY);
        for (Unit::AuraList::const_iterator itr = auras.begin(); itr != auras.end(); ++itr)
        {
            SpellEntry const* itr_spellProto = (*itr)->GetSpellProto();

            // Incanter's Absorption
            if (itr_spellProto->GetSpellFamilyName() == SPELLFAMILY_GENERIC &&
                itr_spellProto->SpellIconID == 2941)
            {
                int32 amount = int32(incanterAbsorption * (*itr)->GetModifier()->m_amount / 100);

                // apply normalized part of already accumulated amount in aura
                if (Aura* spdAura = GetAura(44413, EFFECT_INDEX_0))
                {
                    amount += spdAura->GetModifier()->m_amount * spdAura->GetAuraDuration() / spdAura->GetAuraMaxDuration();
                }

                // Incanter's Absorption (triggered absorb based spell power, will replace existing if any)
                CastCustomSpell(this, 44413, &amount, NULL, NULL, true);
                break;
            }
        }
    }

    // only split damage if not damaging yourself
    if (pCaster != this)
    {
        AuraList const& vSplitDamagePct = GetAurasByType(SPELL_AURA_SPLIT_DAMAGE_PCT);
        for (AuraList::const_iterator i = vSplitDamagePct.begin(), next; i != vSplitDamagePct.end() && RemainingDamage >= 0; i = next)
        {
            next = i; ++next;

            // check damage school mask
            if (((*i)->GetModifier()->m_miscvalue & schoolMask) == 0)
            {
                continue;
            }

            // Damage can be splitted only if aura has an alive caster
            Unit* caster = (*i)->GetCaster();
            if (!caster || caster == this || !caster->IsInWorld() || !caster->IsAlive())
            {
                continue;
            }

            uint32 splitted = uint32(RemainingDamage * (*i)->GetModifier()->m_amount / 100.0f);

            RemainingDamage -=  int32(splitted);

            uint32 split_absorb = 0;
            pCaster->DealDamageMods(caster, splitted, &split_absorb);

            pCaster->SendSpellNonMeleeDamageLog(caster, (*i)->GetSpellProto()->ID, splitted, schoolMask, split_absorb, 0, false, 0, false);

            CleanDamage cleanDamage = CleanDamage(splitted, BASE_ATTACK, MELEE_HIT_NORMAL);
            pCaster->DealDamage(caster, splitted, &cleanDamage, DIRECT_DAMAGE, schoolMask, (*i)->GetSpellProto(), false);
        }
    }

    // Apply death prevention spells effects
    if (preventDeathSpell && RemainingDamage >= (int32)GetHealth())
    {
        switch(preventDeathSpell->GetSpellFamilyName())
        {
                // Cheat Death
            case SPELLFAMILY_ROGUE:
            {
                // Cheat Death
                if (preventDeathSpell->SpellIconID == 2109)
                {
                    CastSpell(this, 31231, true);
                    m_spellCooldownMgr.AddSpellCooldown(31231, 0, time(NULL) + 60);
                    // with health > 10% lost health until health==10%, in other case no losses
                    uint32 health10 = GetMaxHealth() / 10;
                    RemainingDamage = GetHealth() > health10 ? GetHealth() - health10 : 0;
                }
                break;
            }
            // Guardian Spirit
            case SPELLFAMILY_PRIEST:
            {
                // Guardian Spirit
                if (preventDeathSpell->SpellIconID == 2873)
                {
                    int32 healAmount = GetMaxHealth() * preventDeathAmount / 100;
                    CastCustomSpell(this, 48153, &healAmount, NULL, NULL, true);
                    RemoveAurasDueToSpell(preventDeathSpell->ID);
                    RemainingDamage = 0;
                }
                break;
            }
        }
    }

    *absorb = damage - RemainingDamage - *resist;
}

/**
 * @brief Calculates block, absorb, and resist results for spell-based damage.
 *
 * @param pCaster The attacking caster.
 * @param damageInfo The mutable spell damage information.
 * @param spellProto The spell entry causing damage.
 * @param attType The associated attack type.
 */
void Unit::CalculateAbsorbResistBlock(Unit* pCaster, SpellNonMeleeDamage* damageInfo, SpellEntry const* spellProto, WeaponAttackType attType, bool periodic)
{
    bool blocked = false;
    // Get blocked status
    switch (spellProto->GetDmgClass())
    {
            // Melee and Ranged Spells
        case SPELL_DAMAGE_CLASS_RANGED:
        case SPELL_DAMAGE_CLASS_MELEE:
            blocked = IsSpellBlocked(pCaster, spellProto, attType);
            break;
        default:
            break;
    }

    if (blocked)
    {
        damageInfo->blocked = uint32(damageInfo->damage * GetShieldBlockDamageValue() / 100.0f);
        if (damageInfo->damage < damageInfo->blocked)
        {
            damageInfo->blocked = damageInfo->damage;
        }
        damageInfo->damage -= damageInfo->blocked;
    }

    uint32 absorb_affected_damage = pCaster->CalcNotIgnoreAbsorbDamage(damageInfo->damage, GetSpellSchoolMask(spellProto), spellProto);
    CalculateDamageAbsorbAndResist(pCaster, GetSpellSchoolMask(spellProto), SPELL_DIRECT_DAMAGE, absorb_affected_damage, &damageInfo->absorb, &damageInfo->resist, !spellProto->HasAttribute(SPELL_ATTR_EX_CANT_REFLECTED));
    damageInfo->damage -= damageInfo->absorb + damageInfo->resist;

    // Resistance stopped the whole thing, and nothing else contributed. That is
    // the one event the client's SPELL_RESIST combat text describes, and it is
    // not the same as a miss: SendSpellMiss reports a spell that never landed,
    // where this one landed and was resisted down to nothing.
    //
    // The test is on the damage that remains, not on absorb_affected_damage. Those
    // differ whenever the caster carries SPELL_AURA_MOD_IGNORE_ABSORB_SCHOOL or
    // ..._FOR_SPELL: CalcNotIgnoreAbsorbDamage then returns only a fraction of the
    // hit, and resisting all of *that* fraction still leaves real damage to land --
    // which SendSpellNonMeleeDamageLog reports for the same hit, so the client
    // would have been told a blow was resisted while it was busy taking it.
    //
    // Periodic ticks are excluded by the caller rather than by the resist maths.
    // The DOT carve-out in CalculateDamageAbsorbAndResist holds a tick at
    // damage - 1 so that it always reports something, but it keys off damagetype
    // DOT and this routine passes SPELL_DIRECT_DAMAGE for every caller -- so a
    // periodic power burn can reach a full resist, and without this would announce
    // one for a tick whose power drain had already succeeded.
    if (!periodic && damageInfo->damage == 0 && damageInfo->resist > 0 &&
        damageInfo->absorb == 0 && damageInfo->blocked == 0)
    {
        pCaster->SendSpellDamageResist(this, spellProto->ID);
    }
}

void Unit::CalculateHealAbsorb(const uint32 heal, uint32* absorb)
{
    if (!IsAlive() || !heal)
    {
        return;
    }

    int32 RemainingHeal = heal;

    // Need remove expired auras after
    bool existExpired = false;

    // absorb
    AuraList const& vHealAbsorb = GetAurasByType(SPELL_AURA_HEAL_ABSORB);
    for (AuraList::const_iterator i = vHealAbsorb.begin(); i != vHealAbsorb.end() && RemainingHeal > 0; ++i)
    {
        Modifier* mod = (*i)->GetModifier();

        // Max Amount can be absorbed by this aura
        int32  currentAbsorb = mod->m_amount;

        // Found empty aura (impossible but..)
        if (currentAbsorb <= 0)
        {
            existExpired = true;
            continue;
        }

        // currentAbsorb - heal can be absorbed
        // If need absorb less heal
        if (RemainingHeal < currentAbsorb)
        {
            currentAbsorb = RemainingHeal;
        }

        RemainingHeal -= currentAbsorb;

        // Reduce aura amount
        mod->m_amount -= currentAbsorb;
        if ((*i)->GetHolder()->DropAuraCharge())
        {
            mod->m_amount = 0;
        }
        // Need remove it later
        if (mod->m_amount <= 0)
        {
            existExpired = true;
        }
    }

    // Remove all expired absorb auras
    if (existExpired)
    {
        for (AuraList::const_iterator i = vHealAbsorb.begin(); i != vHealAbsorb.end();)
        {
            if ((*i)->GetModifier()->m_amount <= 0)
            {
                RemoveAurasDueToSpell((*i)->GetId(), NULL, AURA_REMOVE_BY_SHIELD_BREAK);
                i = vHealAbsorb.begin();
            }
            else
            {
                ++i;
            }
        }
    }

    *absorb = heal - RemainingHeal;
}

/**
 * @brief Applies script-driven damage modifications before damage is dealt.
 *
 * @param pVictim The victim taking damage.
 * @param damage The mutable damage amount.
 * @param absorb Optional absorbed damage accumulator.
 */
void Unit::DealDamageMods(Unit* pVictim, uint32& damage, uint32* absorb)
{
    if (!pVictim->IsAlive() || pVictim->IsTaxiFlying() || pVictim->IsInEvadeMode())
    {
        if (absorb)
        {
            *absorb += damage;
        }
        damage = 0;
        return;
    }

    // You don't lose health from damage taken from another player while in a sanctuary
    // You still see it in the combat log though
    if (!IsAllowedDamageInArea(pVictim))
    {
        if (absorb)
        {
            *absorb += damage;
        }
        damage = 0;
    }

    uint32 originalDamage = damage;

    // Script Event damage Deal
    if (GetTypeId() == TYPEID_UNIT && AI())
    {
        AI()->DamageDeal(pVictim, damage);
    }
    // Script Event damage taken
    if (pVictim->GetTypeId() == TYPEID_UNIT && pVictim->AI())
    {
        pVictim->AI()->DamageTaken(this, damage);
    }

    if (absorb && originalDamage > damage)
    {
        *absorb += (originalDamage - damage);
    }
}

// Obsolete func need remove, here only for comotability vs another patches
uint32 Unit::SpellNonMeleeDamageLog(Unit* pVictim, uint32 spellID, uint32 damage)
{
    SpellEntry const* spellInfo = sSpellStore.LookupEntry(spellID);
    SpellNonMeleeDamage damageInfo(this, pVictim, spellInfo->ID, SpellSchoolMask(spellInfo->SchoolMask));
    CalculateSpellDamage(&damageInfo, damage, spellInfo);
    damageInfo.target->CalculateAbsorbResistBlock(this, &damageInfo, spellInfo);
    DealDamageMods(damageInfo.target, damageInfo.damage, &damageInfo.absorb);
    SendSpellNonMeleeDamageLog(&damageInfo);
    DealSpellDamage(&damageInfo, true);
    return damageInfo.damage;
}

/**
 * @brief Computes final spell damage before application to a target.
 *
 * @param damageInfo The damage result structure to populate.
 * @param damage The raw base damage.
 * @param spellInfo The spell entry causing damage.
 * @param attackType The associated attack type.
 */
void Unit::CalculateSpellDamage(SpellNonMeleeDamage* damageInfo, int32 damage, SpellEntry const* spellInfo, WeaponAttackType attackType)
{
    SpellSchoolMask damageSchoolMask = damageInfo->schoolMask;
    Unit* pVictim = damageInfo->target;

    if (damage < 0)
    {
        return;
    }

    if (!pVictim)
    {
        return;
    }

    // units which are not alive cannot deal damage except for dying creatures
    if ((!this->IsAlive() || !pVictim->IsAlive()) && (this->GetTypeId() != TYPEID_UNIT || this->GetDeathState() != DEAD))
    {
        return;
    }

    // Check spell crit chance
    bool crit = IsSpellCrit(pVictim, spellInfo, damageSchoolMask, attackType);

    // damage bonus (per damage class)
    switch (spellInfo->GetDmgClass())
    {
            // Melee and Ranged Spells
        case SPELL_DAMAGE_CLASS_RANGED:
        case SPELL_DAMAGE_CLASS_MELEE:
        {
            // Calculate damage bonus
            damage = MeleeDamageBonusDone(pVictim, damage, attackType, spellInfo, SPELL_DIRECT_DAMAGE);
            damage = pVictim->MeleeDamageBonusTaken(this, damage, attackType, spellInfo, SPELL_DIRECT_DAMAGE);

            // if crit add critical bonus
            if (crit)
            {
                damageInfo->HitInfo |= SPELL_HIT_TYPE_CRIT;
                damage = SpellCriticalDamageBonus(spellInfo, damage, pVictim);
                // Cata 4.0.1: no separate crit-damage resilience reduction;
                // the single GetDamageReduction call below covers crit damage.
            }
        }
        break;
        // Magical Attacks
        case SPELL_DAMAGE_CLASS_NONE:
        case SPELL_DAMAGE_CLASS_MAGIC:
        {
            // Calculate damage bonus
            damage = SpellDamageBonusDone(pVictim, spellInfo, damage, SPELL_DIRECT_DAMAGE);
            damage = pVictim->SpellDamageBonusTaken(this, spellInfo, damage, SPELL_DIRECT_DAMAGE);

            // If crit add critical bonus
            if (crit)
            {
                damageInfo->HitInfo |= SPELL_HIT_TYPE_CRIT;
                damage = SpellCriticalDamageBonus(spellInfo, damage, pVictim);
                // Cata 4.0.1: no separate crit-damage resilience reduction;
                // the single GetDamageReduction call below covers crit damage.
            }
        }
        break;
    }

    // only from players
    if (GetTypeId() == TYPEID_PLAYER)
    {
        uint32 reduction_affected_damage = CalcNotIgnoreDamageReduction(damage, damageSchoolMask);
        damage -= pVictim->GetDamageReduction(reduction_affected_damage);
    }

    // damage mitigation
    if (damage > 0)
    {
        // physical damage => armor
        if (damageSchoolMask & SPELL_SCHOOL_MASK_NORMAL)
        {
            uint32 armor_affected_damage = CalcNotIgnoreDamageReduction(damage, damageSchoolMask);
            damage = damage - armor_affected_damage + CalcArmorReducedDamage(pVictim, armor_affected_damage);
        }
    }
    else
    {
        damage = 0;
    }
    damageInfo->damage = damage;
}

/**
 * @brief Applies prepared spell damage to a target.
 *
 * @param damageInfo The prepared non-melee damage information.
 * @param durabilityLoss True to apply durability loss rules.
 */
void Unit::DealSpellDamage(SpellNonMeleeDamage* damageInfo, bool durabilityLoss)
{
    if (!damageInfo)
    {
        return;
    }

    Unit* pVictim = damageInfo->target;

    if (!pVictim)
    {
        return;
    }

    if (!pVictim->IsAlive() || pVictim->IsTaxiFlying() || pVictim->IsInEvadeMode())
    {
        return;
    }

    SpellEntry const* spellProto = sSpellStore.LookupEntry(damageInfo->SpellID);
    if (spellProto == NULL)
    {
        sLog.outError("Unit::DealSpellDamage have wrong damageInfo->SpellID: %u", damageInfo->SpellID);
        return;
    }

    // You don't lose health from damage taken from another player while in a sanctuary
    // You still see it in the combat log though
    if (!IsAllowedDamageInArea(pVictim))
    {
        return;
    }

    // Call default DealDamage (send critical in hit info for threat calculation)
    CleanDamage cleanDamage(0, BASE_ATTACK, damageInfo->HitInfo & SPELL_HIT_TYPE_CRIT ? MELEE_HIT_CRIT : MELEE_HIT_NORMAL);
    DealDamage(pVictim, damageInfo->damage, &cleanDamage, SPELL_DIRECT_DAMAGE, damageInfo->schoolMask, spellProto, durabilityLoss);
}

uint32 Unit::CalcNotIgnoreAbsorbDamage(uint32 damage, SpellSchoolMask damageSchoolMask, SpellEntry const* spellInfo /*= NULL*/)
{
    float absorb_affected_rate = 1.0f;
    Unit::AuraList const& ignoreAbsorbSchool = GetAurasByType(SPELL_AURA_MOD_IGNORE_ABSORB_SCHOOL);
    for (Unit::AuraList::const_iterator i = ignoreAbsorbSchool.begin(); i != ignoreAbsorbSchool.end(); ++i)
        if ((*i)->GetMiscValue() & damageSchoolMask)
        {
            absorb_affected_rate *= (100.0f - (*i)->GetModifier()->m_amount) / 100.0f;
        }

    if (spellInfo)
    {
        Unit::AuraList const& ignoreAbsorbForSpell = GetAurasByType(SPELL_AURA_MOD_IGNORE_ABSORB_FOR_SPELL);
        for (Unit::AuraList::const_iterator citr = ignoreAbsorbForSpell.begin(); citr != ignoreAbsorbForSpell.end(); ++citr)
            if ((*citr)->isAffectedOnSpell(spellInfo))
            {
                absorb_affected_rate *= (100.0f - (*citr)->GetModifier()->m_amount) / 100.0f;
            }
    }

    return absorb_affected_rate <= 0.0f ? 0 : (absorb_affected_rate < 1.0f  ? uint32(damage * absorb_affected_rate) : damage);
}

uint32 Unit::CalcNotIgnoreDamageReduction(uint32 damage, SpellSchoolMask damageSchoolMask)
{
    float absorb_affected_rate = 1.0f;
    Unit::AuraList const& ignoreAbsorb = GetAurasByType(SPELL_AURA_MOD_IGNORE_DAMAGE_REDUCTION_SCHOOL);
    for (Unit::AuraList::const_iterator i = ignoreAbsorb.begin(); i != ignoreAbsorb.end(); ++i)
        if ((*i)->GetMiscValue() & damageSchoolMask)
        {
            absorb_affected_rate *= (100.0f - (*i)->GetModifier()->m_amount) / 100.0f;
        }

    return absorb_affected_rate <= 0.0f ? 0 : (absorb_affected_rate < 1.0f  ? uint32(damage * absorb_affected_rate) : damage);
}

float Unit::GetCombatRatingReduction(CombatRating cr) const
{
    if (GetTypeId() == TYPEID_PLAYER)
    {
        return GetRatingBonusValue(cr);
    }
    else if (IsPet())
    {
        // Player's pet get 100% resilience from owner
        if (Unit* owner = GetOwner())
            if (owner->GetTypeId() == TYPEID_PLAYER)
            {
                return owner->GetRatingBonusValue(cr);
            }
    }

    return 0.0f;
}

uint32 Unit::GetCombatRatingDamageReduction(CombatRating cr, float rate, float cap, uint32 damage) const
{
    float percent = GetCombatRatingReduction(cr) * rate;
    if (percent > cap)
    {
        percent = cap;
    }
    return uint32(percent * damage / 100.0f);
}

/**
 * @brief Applies final damage to a victim and handles combat side effects.
 *
 * @param pVictim The victim taking damage.
 * @param damage The final damage amount.
 * @param cleanDamage Optional clean-damage metadata.
 * @param damagetype The damage effect classification.
 * @param damageSchoolMask The damage school mask.
 * @param spellProto The responsible spell, if any.
 * @param durabilityLoss True to apply durability loss rules.
 * @return The damage amount effectively processed.
 */
uint32 Unit::DealDamage(Unit* pVictim, uint32 damage, CleanDamage const* cleanDamage, DamageEffectType damagetype, SpellSchoolMask damageSchoolMask, SpellEntry const* spellProto, bool durabilityLoss)
{
    Player* const victimPlayer = pVictim->ToPlayer();
    Creature* const victimCreature = pVictim->ToCreature();

    // remove affects from attacker at any non-DoT damage (including 0 damage)
    if (damagetype != DOT)
    {
        if (damagetype != SELF_DAMAGE_ROGUE_FALL)
        {
            RemoveSpellsCausingAura(SPELL_AURA_MOD_STEALTH);
        }
        RemoveSpellsCausingAura(SPELL_AURA_FEIGN_DEATH);

        if (victimPlayer && !pVictim->IsStandState() && !pVictim->Blocked(Motion::ReasonStunned))
        {
            pVictim->SetStandState(UNIT_STAND_STATE_STAND);
        }
    }

    if (!damage)
    {
        // Rage from physical damage received .
        if (cleanDamage && cleanDamage->damage && (damageSchoolMask & SPELL_SCHOOL_MASK_NORMAL) && victimPlayer && (pVictim->GetPowerType() == POWER_RAGE))
        {
            victimPlayer->RewardRage(cleanDamage->damage, 0, false);
        }

        return 0;
    }

    DEBUG_FILTER_LOG(LOG_FILTER_DAMAGE, "DealDamageStart");

    uint32 health = pVictim->GetHealth();
    DEBUG_FILTER_LOG(LOG_FILTER_DAMAGE, "deal dmg:%d to health:%d ", damage, health);

    // Clamp damage on unkillable creatures (training dummies, etc.) so the
    // death cascade below never fires. Damage is clamped to (health - 1),
    // matching the TC-Preservation pattern: the dummy stays at 1 HP after
    // a would-be-lethal blow, and subsequent damage simply does nothing.
    // Self-damage is NOT clamped — a creature suiciding on its own effect
    // (Hellfire-style) should still resolve normally if the flag is ever
    // set on such a unit.
    if (damage >= health && pVictim != this &&
        victimCreature &&
        victimCreature->IsUnkillable())
    {
        damage = health > 1 ? health - 1 : 0;
    }

    // Rage from Damage made (only from direct weapon damage)
    if (cleanDamage && damagetype == DIRECT_DAMAGE && this != pVictim && GetTypeId() == TYPEID_PLAYER && (GetPowerType() == POWER_RAGE))
    {
        uint32 weaponSpeedHitFactor;

        switch (cleanDamage->attackType)
        {
            case BASE_ATTACK:
            {
                if (cleanDamage->hitOutCome == MELEE_HIT_CRIT)
                {
                    weaponSpeedHitFactor = uint32(GetAttackTime(cleanDamage->attackType) / 1000.0f * 7);
                }
                else
                {
                    weaponSpeedHitFactor = uint32(GetAttackTime(cleanDamage->attackType) / 1000.0f * 3.5f);
                }

                RewardRage(damage, weaponSpeedHitFactor, true);

                break;
            }
            case OFF_ATTACK:
            {
                if (cleanDamage->hitOutCome == MELEE_HIT_CRIT)
                {
                    weaponSpeedHitFactor = uint32(GetAttackTime(cleanDamage->attackType) / 1000.0f * 3.5f);
                }
                else
                {
                    weaponSpeedHitFactor = uint32(GetAttackTime(cleanDamage->attackType) / 1000.0f * 1.75f);
                }

                RewardRage(damage, weaponSpeedHitFactor, true);

                break;
            }
            case RANGED_ATTACK:
                break;
        }
    }

    // no xp,health if type 8 /critters/
    if (victimCreature && pVictim->GetCreatureType() == CREATURE_TYPE_CRITTER)
    {
        // TODO: fix this part
        // Critter may not die of damage taken, instead expect it to run away (no fighting back)
        // If (this) is TYPEID_PLAYER, (this) will enter combat w/victim, but after some time, automatically leave combat.
        // It is unclear how it should work for other cases.

        DEBUG_FILTER_LOG(LOG_FILTER_DAMAGE, "DealDamage critter, critter dies");

        victimCreature->SetLootRecipient(this);

        JustKilledCreature(victimCreature, NULL);
        pVictim->SetHealth(0);

        return damage;
    }

    // share damage by auras
    AuraList const& vShareDamageAuras = pVictim->GetAurasByType(SPELL_AURA_SHARE_DAMAGE_PCT);
    for (AuraList::const_iterator itr = vShareDamageAuras.begin(); itr != vShareDamageAuras.end(); ++itr)
    {
        if (!spellProto)
        {
            break;
        }

        SpellEffectEntry const* spellEffect = spellProto->GetSpellEffect(EFFECT_INDEX_0);

        // if damage is done by another shared aura, then skip to avoid circular reference (aura 300 is only applied on effect_idx_0
        if (spellEffect && spellEffect->Effect == SPELL_EFFECT_APPLY_AURA &&
            spellEffect->EffectAura == SPELL_AURA_SHARE_DAMAGE_PCT)
            break;

        if (Unit* shareTarget = (*itr)->GetCaster())
        {
            if (shareTarget != pVictim && ((*itr)->GetMiscValue() & damageSchoolMask))
            {
                SpellEntry const* shareSpell = (*itr)->GetSpellProto();
                uint32 shareDamage = uint32(damage*(*itr)->GetModifier()->m_amount / 100.0f);
                DealDamageMods(shareTarget, shareDamage, NULL);
                DealDamage(shareTarget, shareDamage, NULL, damagetype, GetSpellSchoolMask(shareSpell), shareSpell, false);
            }
        }
    }

    // duel ends when player has 1 or less hp
    bool duel_hasEnded = false;
    if (victimPlayer && victimPlayer->duel && damage >= (health - 1))
    {
        // prevent kill only if killed in duel and killed by opponent or opponent controlled creature
        if (victimPlayer->duel->opponent == this || victimPlayer->duel->opponent->GetObjectGuid() == GetOwnerGuid())
        {
            damage = health - 1;
        }

        duel_hasEnded = true;
    }

    // Get in CombatState
    if (pVictim != this && damagetype != DOT)
    {
        SetInCombatWith(pVictim);
        pVictim->SetInCombatWith(this);

        if (Player* attackedPlayer = pVictim->GetCharmerOrOwnerPlayerOrPlayerItself())
        {
            SetContestedPvP(attackedPlayer);
        }
    }

    if (GetTypeId() == TYPEID_PLAYER && this != pVictim)
    {
        CreditDamageDealt(pVictim, damage);
    }

    if (victimPlayer)
    {
        victimPlayer->UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_HIGHEST_HIT_RECEIVED, damage);
    }

    if (victimCreature && !pVictim->IsPet() && !victimCreature->HasLootRecipient())
    {
        victimCreature->SetLootRecipient(this);
    }

    if (health <= damage)
    {
        DEBUG_FILTER_LOG(LOG_FILTER_DAMAGE, "DealDamage %s Killed %s", GetGuidStr().c_str(), pVictim->GetGuidStr().c_str());

        /*
         *                      Preparation: Who gets credit for killing whom, invoke SpiritOfRedemtion?
         */
        // for loot will be used only if group_tap == NULL
        Player* player_tap = GetCharmerOrOwnerPlayerOrPlayerItself();
        Group* group_tap = NULL;

        // in creature kill case group/player tap stored for creature
        if (victimCreature)
        {
            group_tap = victimCreature->GetGroupLootRecipient();

            if (Player* recipient = victimCreature->GetOriginalLootRecipient())
            {
                player_tap = recipient;
            }
        }
        // in player kill case group tap selected by player_tap (killer-player itself, or charmer, or owner, etc)
        else
        {
            if (player_tap)
            {
                group_tap = player_tap->GetGroup();
            }
        }

        // Spirit of Redemtion Talent
        bool damageFromSpiritOfRedemtionTalent = spellProto && spellProto->ID == 27795;
        // if talent known but not triggered (check priest class for speedup check)
        Aura* spiritOfRedemtionTalentReady = NULL;
        if (!damageFromSpiritOfRedemtionTalent &&           // not called from SPELL_AURA_SPIRIT_OF_REDEMPTION
            victimPlayer && pVictim->getClass() == CLASS_PRIEST)
        {
            AuraList const& vDummyAuras = pVictim->GetAurasByType(SPELL_AURA_DUMMY);
            for (AuraList::const_iterator itr = vDummyAuras.begin(); itr != vDummyAuras.end(); ++itr)
            {
                if ((*itr)->GetSpellProto()->SpellIconID == 1654)
                {
                    spiritOfRedemtionTalentReady = *itr;
                    break;
                }
            }
        }

        /*
         *                      Generic Actions (ProcEvents, Combat-Log, Kill Rewards, Stop Combat)
         */
        // call kill spell proc event (before real die and combat stop to triggering auras removed at death/combat stop)
        if (player_tap && player_tap != pVictim)
        {
            player_tap->ProcDamageAndSpell(pVictim, PROC_FLAG_KILL, PROC_FLAG_KILLED, PROC_EX_NONE, 0);

            PartyKillLogFact partyKill;
            partyKill.killer = player_tap->GetObjectGuid();
            partyKill.victim = pVictim->GetObjectGuid();
            partyKill.group = group_tap;
            ReportClientFact(InstalledClientCallbacks(player_tap->m_clientCallbacks).partyKillLog, partyKill);
        }
        else if (GetTypeId() == TYPEID_UNIT && this != pVictim)
        {
            ProcDamageAndSpell(pVictim, PROC_FLAG_KILL, PROC_FLAG_KILLED, PROC_EX_NONE, 0);
        }

        // Reward player, his pets, and group/raid members
        if (player_tap != pVictim)
        {
            if (group_tap)
            {
                sGroupHooks.RewardAtKill(*group_tap, *pVictim, player_tap);
            }
            else if (player_tap)
            {
                player_tap->RewardSinglePlayerAtKill(pVictim);
            }
        }

        // stop combat
        DEBUG_FILTER_LOG(LOG_FILTER_DAMAGE, "DealDamageAttackStop");
        pVictim->CombatStop();
        pVictim->GetHostileRefManager().deleteReferences();

        /*
         *                      Actions for the killer
         */
        if (spiritOfRedemtionTalentReady)
        {
            DEBUG_FILTER_LOG(LOG_FILTER_DAMAGE, "DealDamage: Spirit of Redemtion ready");

            // save value before aura remove
            uint32 ressSpellId = pVictim->GetUInt32Value(PLAYER_SELF_RES_SPELL);
            if (!ressSpellId)
            {
                ressSpellId = victimPlayer->GetResurrectionSpellId();
            }

            // Remove all expected to remove at death auras (most important negative case like DoT or periodic triggers)
            pVictim->RemoveAllAurasOnDeath();

            // restore for use at real death
            pVictim->SetUInt32Value(PLAYER_SELF_RES_SPELL, ressSpellId);

            // FORM_SPIRITOFREDEMPTION and related auras
            pVictim->CastSpell(pVictim, 27827, true, NULL, spiritOfRedemtionTalentReady);
        }
        else
        {
            pVictim->SetHealth(0);
        }

        // Call KilledUnit for creatures
        if (GetTypeId() == TYPEID_UNIT && AI())
        {
            AI()->KilledUnit(pVictim);
        }

        if (Creature* killer = ToCreature())
        {
        }

        // Call AI OwnerKilledUnit (for any current summoned minipet/guardian/protector)
        PetOwnerKilledUnit(pVictim);

        /*
         *                      Actions for the victim
         */
        if (victimPlayer)          // Killed player
        {
            // remember victim PvP death for corpse type and corpse reclaim delay
            // at original death (not at SpiritOfRedemtionTalent timeout)
            if (!damageFromSpiritOfRedemtionTalent)
            {
                victimPlayer->SetPvPDeath(player_tap != NULL);
            }

            // achievement stuff
            victimPlayer->UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_TOTAL_DAMAGE_RECEIVED, health);
            if (player_tap)
            {
                player_tap->UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_SPECIAL_PVP_KILL, 1, 0, pVictim);
            }
            if (GetTypeId() == TYPEID_UNIT)
            {
                victimPlayer->UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_KILLED_BY_CREATURE, GetEntry());
            }
            else if (GetTypeId() == TYPEID_PLAYER && pVictim != this)
            {
                victimPlayer->UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_KILLED_BY_PLAYER, 1, victimPlayer->GetTeam());
            }

            // 10% durability loss on death
            // only if not player and not controlled by player pet. And not at BG
            if (durabilityLoss && !player_tap && !victimPlayer->InBattleGround())
            {
                DEBUG_LOG("DealDamage: Killed %s, looing 10 percents durability", pVictim->GetGuidStr().c_str());
                victimPlayer->DurabilityLossAll(0.10f, false);
                ReportClientFact(InstalledClientCallbacks(victimPlayer->m_clientCallbacks).durabilityDeath, DurabilityDeathFact());
            }

            if (!spiritOfRedemtionTalentReady)              // Before informing Battleground
            {
                DEBUG_FILTER_LOG(LOG_FILTER_DAMAGE, "SET JUST_DIED");
                pVictim->SetDeathState(JUST_DIED);
            }

            // a victim in a duel: the duel is interrupted
            // last damage from non duel opponent or non opponent controlled creature
            if (duel_hasEnded)
            {
                victimPlayer->duel->opponent->CombatStopWithPets(true);
                victimPlayer->CombatStopWithPets(true);

                victimPlayer->DuelComplete(DUEL_INTERRUPTED);
            }

            if (player_tap)                                 // PvP kill
            {
                if (!sBattleGroundHooks.PlayerKilled(*victimPlayer, *player_tap) && pVictim != this)
                {
                    // selfkills are not handled in outdoor pvp scripts
                    sOutdoorPvPHooks.PlayerKilled(victimPlayer->GetCachedZoneId(), *player_tap, *victimPlayer);
                }
            }
        }
        else                                                // Killed creature
        {
            JustKilledCreature(victimCreature, player_tap);
        }
    }
    else                                                    // if (health <= damage)
    {
        DEBUG_FILTER_LOG(LOG_FILTER_DAMAGE, "DealDamageAlive");

        if (victimPlayer)
        {
            victimPlayer->UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_TOTAL_DAMAGE_RECEIVED, damage);
        }

        pVictim->ModifyHealth(- (int32)damage);

        if (damagetype != DOT)
        {
            if (!getVictim())
            {
                // if not have main target then attack state with target (including AI call)
                // start melee attacks only after melee hit
                Attack(pVictim, (damagetype == DIRECT_DAMAGE));
            }

            // if damage pVictim call AI reaction
            pVictim->AttackedBy(this);
        }

        if (!victimPlayer)
        {
            float threat = damage * sSpellMgr.GetSpellThreatMultiplier(spellProto);
            pVictim->AddThreat(this, threat, (cleanDamage && cleanDamage->hitOutCome == MELEE_HIT_CRIT), damageSchoolMask, spellProto);
        }
        else                                                // victim is a player
        {
            // Rage from damage received
            if (this != pVictim && pVictim->GetPowerType() == POWER_RAGE)
            {
                uint32 rage_damage = damage + (cleanDamage ? cleanDamage->damage : 0);
                victimPlayer->RewardRage(rage_damage, 0, false);
            }

            // random durability for items (HIT TAKEN)
            if (roll_chance_f(sGameConfig.getConfig(CONFIG_FLOAT_RATE_DURABILITY_LOSS_DAMAGE)))
            {
                EquipmentSlots slot = EquipmentSlots(urand(0, EQUIPMENT_SLOT_END - 1));
                victimPlayer->DurabilityPointLossForEquipSlot(slot);
            }
        }

        if (m_itemCallbacks)
        {
            // random durability for items (HIT DONE)
            if (roll_chance_f(sGameConfig.getConfig(CONFIG_FLOAT_RATE_DURABILITY_LOSS_DAMAGE)))
            {
                EquipmentSlots slot = EquipmentSlots(urand(0, EQUIPMENT_SLOT_END - 1));
                m_itemCallbacks->durabilityPointLoss(slot);
            }
        }

        if (damagetype != NODAMAGE && damage && victimPlayer)
        {
            if (damagetype != DOT)
            {
                for (uint32 i = CURRENT_FIRST_NON_MELEE_SPELL; i < CURRENT_MAX_SPELL; ++i)
                {
                    // skip channeled spell (processed differently below)
                    if (i == CURRENT_CHANNELED_SPELL)
                    {
                        continue;
                    }

                    if (Spell* spell = pVictim->GetCurrentSpell(CurrentSpellTypes(i)))
                    {
                        if (spell->getState() == SPELL_STATE_PREPARING)
                        {
                            if (spell->m_spellInfo->GetInterruptFlags() & SPELL_INTERRUPT_FLAG_ABORT_ON_DMG)
                            {
                                pVictim->InterruptSpell(CurrentSpellTypes(i));
                            }
                            else
                            {
                                spell->Delayed();
                            }
                        }
                    }
                }
            }

            if (Spell* spell = pVictim->m_currentSpells[CURRENT_CHANNELED_SPELL])
            {
                if (spell->getState() == SPELL_STATE_CASTING)
                {
                    uint32 channelInterruptFlags = spell->m_spellInfo->GetChannelInterruptFlags();
                    if (channelInterruptFlags & CHANNEL_FLAG_DELAY)
                    {
                        if (pVictim != this)                // don't shorten the duration of channeling if you damage yourself
                        {
                            spell->DelayedChannel();
                        }
                    }
                    else if ((channelInterruptFlags & (CHANNEL_FLAG_DAMAGE | CHANNEL_FLAG_DAMAGE2)))
                    {
                        DETAIL_LOG("Spell %u canceled at damage!", spell->m_spellInfo->ID);
                        pVictim->InterruptSpell(CURRENT_CHANNELED_SPELL);
                    }
                }
                else if (spell->getState() == SPELL_STATE_DELAYED)
                    // break channeled spell in delayed state on damage
                {
                    DETAIL_LOG("Spell %u canceled at damage!", spell->m_spellInfo->ID);
                    pVictim->InterruptSpell(CURRENT_CHANNELED_SPELL);
                }
            }
        }

        // last damage from duel opponent
        if (duel_hasEnded)
        {
            MANGOS_ASSERT(victimPlayer);
            MANGOS_ASSERT(victimPlayer->duel);

            victimPlayer->SetHealth(1);

            victimPlayer->duel->opponent->CombatStopWithPets(true);
            victimPlayer->CombatStopWithPets(true);

            victimPlayer->CastSpell(victimPlayer, 7267, true);                  // beg
            victimPlayer->DuelComplete(DUEL_WON);
        }
    }

    DEBUG_FILTER_LOG(LOG_FILTER_DAMAGE, "DealDamageEnd returned %d damage", damage);

    return damage;
}

struct PetOwnerKilledUnitHelper
{
    explicit PetOwnerKilledUnitHelper(Unit* pVictim) : m_victim(pVictim) {}
    void operator()(Unit* pTarget) const
    {
        if (pTarget->GetTypeId() == TYPEID_UNIT)
        {
            if (pTarget->AI())
            {
                pTarget->AI()->OwnerKilledUnit(m_victim);
            }
        }
    }

    Unit* m_victim;
};

/**
 * @brief Handles creature death notifications, bindings, and loot preparation.
 *
 * @param victim The creature that was killed.
 * @param responsiblePlayer The credited player, if any.
 */
void Unit::JustKilledCreature(Creature* victim, Player* responsiblePlayer)
{
    victim->m_deathState = DEAD;                            // so that IsAlive, IsDead return expected results in the called hooks of JustKilledCreature
                                                            // must be used only shortly before SetDeathState(JUST_DIED) and only for Creatures or Pets

    // some critters required for quests (need normal entry instead possible heroic in any cases)
    if (victim->GetCreatureType() == CREATURE_TYPE_CRITTER && GetTypeId() == TYPEID_PLAYER)
    {
        if (CreatureInfo const* normalInfo = ObjectMgr::GetCreatureTemplate(victim->GetEntry()))
        {
            KilledMonster(normalInfo, victim->GetObjectGuid());
        }
    }

    // Interrupt channeling spell when a Possessed Summoned is killed
    SpellEntry const* spellInfo = sSpellStore.LookupEntry(victim->GetUInt32Value(UNIT_CREATED_BY_SPELL));
    if (spellInfo && spellInfo->HasAttribute(SPELL_ATTR_EX_FARSIGHT) && spellInfo->HasAttribute(SPELL_ATTR_EX_CHANNELED_1))
    {
        Unit* creator = GetMap()->GetUnit(victim->GetCreatorGuid());
        if (creator && creator->GetCharmGuid() == victim->GetObjectGuid())
        {
            Spell* channeledSpell = creator->GetCurrentSpell(CURRENT_CHANNELED_SPELL);
            if (channeledSpell && channeledSpell->m_spellInfo->ID == spellInfo->ID)
            {
                creator->InterruptNonMeleeSpells(false);
            }
        }
    }

    /* ******************************* Inform various hooks ************************************ */
    // Inform victim's AI
    if (victim->AI())
    {
        victim->AI()->JustDied(this);
    }

    // Inform Owner
    Unit* pOwner = victim->GetCharmerOrOwner();
    if (victim->IsTemporarySummon())
    {
        if (victim->GetSummonerGuid().IsCreatureOrVehicle())
            if (Creature* pSummoner = victim->GetMap()->GetCreature(victim->GetSummonerGuid()))
                if (pSummoner->AI())
                {
                    pSummoner->AI()->SummonedCreatureJustDied(victim);
                }
    }
    else if (pOwner && pOwner->GetTypeId() == TYPEID_UNIT)
    {
        if (pOwner->AI())
        {
            pOwner->AI()->SummonedCreatureJustDied(victim);
        }
    }

    // Inform Instance Data and Linking
    if (InstanceData* mapInstance = victim->GetInstanceData())
    {
        mapInstance->OnCreatureDeath(victim);
    }

    if (responsiblePlayer)                                  // killedby Player, inform BG
    {
        sBattleGroundHooks.CreatureKilled(*victim, *responsiblePlayer);
    }

    // Notify the outdoor pvp script
    sOutdoorPvPHooks.CreatureDied(responsiblePlayer ? responsiblePlayer->GetCachedZoneId() : GetTerrain()->GetZoneId(Where().X(), Where().Y(), Where().Z()), *victim);

    // Start creature death script
    GetMap()->ScriptsStart(DBS_ON_CREATURE_DEATH, victim->GetEntry(), victim, responsiblePlayer ? responsiblePlayer : this);

    if (victim->IsLinkingEventTrigger())
    {
        victim->GetMap()->GetCreatureLinkingHolder()->DoCreatureLinkingEvent(LINKING_EVENT_DIE, victim);
    }

    // Dungeon specific stuff
    if (victim->GetInstanceId())
    {
        Map* m = victim->GetMap();
        Player* creditedPlayer = GetCharmerOrOwnerPlayerOrPlayerItself();
        // TODO: do instance binding anyway if the charmer/owner is offline

        if (m->IsDungeon() && creditedPlayer)
        {
            ((DungeonMap*)m)->CreditCreatureKill(victim, creditedPlayer);
        }
    }

    bool isPet = victim->IsPet();

    /* ********************************* Set Death finally ************************************* */
    DEBUG_FILTER_LOG(LOG_FILTER_DAMAGE, "SET JUST_DIED");
    victim->SetDeathState(JUST_DIED);                       // if !spiritOfRedemtionTalentReady always true for unit

    if (isPet)
    {
        return;                                              // Pets might have been unsummoned at this place, do not handle them further!
    }

    /* ******************************** Prepare loot if can ************************************ */
    victim->DeleteThreatList();
    // only lootable if it has loot or can drop gold
    victim->PrepareBodyLootState();
    // may have no loot, so update death timer if allowed, must be after SetDeathState(JUST_DIED)
    victim->AllLootRemovedFromCorpse();
}

/**
 * @brief Notifies controlled companions that their owner killed a unit.
 *
 * @param pVictim The killed unit.
 */
void Unit::PetOwnerKilledUnit(Unit* pVictim)
{
    // for minipet and guardians (including protector)
    CallForAllControlledUnits(PetOwnerKilledUnitHelper(pVictim), CONTROLLED_MINIPET | CONTROLLED_GUARDIANS);
}

/**
 * @brief Applies healing to a victim and sends the corresponding heal log.
 *
 * @param pVictim The healed unit.
 * @param addhealth The attempted healing amount.
 * @param spellProto The spell responsible for the heal.
 * @param critical True if the heal crit.
 * @return The effective health gain.
 */
int32 Unit::DealHeal(Unit* pVictim, uint32 addhealth, SpellEntry const* spellProto, bool critical, uint32 absorb)
{
    int32 gain = pVictim->ModifyHealth(int32(addhealth));

    Unit* unit = this;

    if (CreditsHealToOwner())
    {
        unit = GetOwner();
    }

    // overheal = addhealth - gain
    unit->SendHealSpellLog(pVictim, spellProto->ID, addhealth, addhealth - gain, critical, absorb);

    if (Player* unitPlayer = unit->ToPlayer())
    {
        sBattleGroundHooks.HealingDone(*unitPlayer, gain);

        // use the actual gain, as the overheal shall not be counted, skip gain 0 (it ignored anyway in to criteria)
        if (gain)
        {
            unitPlayer->UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_HEALING_DONE, gain, 0, pVictim);
        }

        unitPlayer->UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_HIGHEST_HEAL_CASTED, addhealth);
    }

    if (Player* victimPlayer = pVictim->ToPlayer())
    {
        victimPlayer->UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_TOTAL_HEALING_RECEIVED, gain);
        victimPlayer->UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_HIGHEST_HEALING_RECEIVED, addhealth);
    }

    // Script Event HealedBy
    if (pVictim->GetTypeId() == TYPEID_UNIT && pVictim->AI())
    {
        pVictim->AI()->HealedBy(this, addhealth);
    }

    return gain;
}
