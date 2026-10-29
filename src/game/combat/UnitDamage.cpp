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
#include "spells/auras/SpellAuras.h"
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
    uint32 const victimArmor = pVictim->GetArmor();                                                                                   // E2a
    int32 const targetResistanceMod = GetTotalAuraModifierByMiscMask(SPELL_AURA_MOD_TARGET_RESISTANCE, SPELL_SCHOOL_MASK_NORMAL);     // E2a
    bool const isPlayer = GetTypeId() == TYPEID_PLAYER;                                                                               // E2a
    uint32 const victimLevel = pVictim->getLevel();                                                                                   // E2a, read twice by the body
    uint32 const attackerLevel = getLevel();                                                                                          // E2a, read twice by the body

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

        if (pCaster->GetTypeId() == TYPEID_PLAYER)
        {
            tmpvalue2 -= (float)((Player*)pCaster)->GetSpellPenetrationItemMod();
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
    if (!pVictim->IsAlive() || pVictim->IsTaxiFlying() || (pVictim->GetTypeId() == TYPEID_UNIT && ((Creature*)pVictim)->IsInEvadeMode()))
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
    if (GetTypeId() == TYPEID_UNIT && ((Creature*)this)->AI())
    {
        ((Creature*)this)->AI()->DamageDeal(pVictim, damage);
    }
    // Script Event damage taken
    if (pVictim->GetTypeId() == TYPEID_UNIT && ((Creature*)pVictim)->AI())
    {
        ((Creature*)pVictim)->AI()->DamageTaken(this, damage);
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

    if (!pVictim->IsAlive() || pVictim->IsTaxiFlying() || (pVictim->GetTypeId() == TYPEID_UNIT && ((Creature*)pVictim)->IsInEvadeMode()))
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
        return ((Player const*)this)->GetRatingBonusValue(cr);
    }
    else if (IsPet())
    {
        // Player's pet get 100% resilience from owner
        if (Unit* owner = GetOwner())
            if (owner->GetTypeId() == TYPEID_PLAYER)
            {
                return ((Player*)owner)->GetRatingBonusValue(cr);
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
