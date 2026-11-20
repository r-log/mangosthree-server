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

#include "spells/SpellScriptQueries.h"
#include "spells/auras/SpellAuras.h"
#include "combat/SpellBonus.h"
#include "spells/data/SpellMgr.h"
#include "spells/data/SpellTables.h"
#include "spells/handlers/SpellHandlerRegistry.h"
#include "spells/handlers/UnitSpellBonusHandlers.h"

size_t FindPeriodicDamage(Unit const& target, ObjectGuid const& caster, uint32 const* spellIds, size_t count,
                          std::vector<PeriodicDamageView>& out)
{
    out.clear();
    for (size_t i = 0; i < count; ++i)
    {
        Unit::SpellAuraHolderConstBounds const bounds = target.GetSpellAuraHolderBounds(spellIds[i]);
        for (Unit::SpellAuraHolderMap::const_iterator itr = bounds.first; itr != bounds.second; ++itr)
        {
            SpellAuraHolder const* holder = itr->second;
            if (holder->IsDeleted() || holder->GetCasterGuid() != caster)
            {
                continue;
            }
            for (int32 eff = 0; eff < MAX_EFFECT_INDEX; ++eff)
            {
                Aura const* aura = holder->GetAuraByEffectIndex(SpellEffectIndex(eff));
                if (!aura || aura->GetModifier()->m_auraname != SPELL_AURA_PERIODIC_DAMAGE)
                {
                    continue;
                }
                uint32 const maxTicks = aura->GetAuraMaxTicks();
                uint32 const ticksDone = aura->GetAuraTicks();

                PeriodicDamageView view;
                view.spellId = spellIds[i];
                view.effIndex = SpellEffectIndex(eff);
                view.amount = aura->GetModifier()->m_amount;
                view.periodMs = aura->GetModifier()->periodictime;
                view.remainingMs = holder->GetAuraDuration();
                view.remainingTicks = maxTicks > ticksDone ? maxTicks - ticksDone : 0;
                view.amountBeforeDone = aura->GetTickAmountBeforeDone();
                view.stacks = holder->GetStackAmount();
                SpellEffectEntry const* effect = aura->GetSpellEffect();
                view.basePeriodMs = effect ? effect->EffectAuraPeriod : 0;
                out.push_back(view);
            }
        }
    }
    return out.size();
}

bool CopyAuraHolder(Unit& from, Unit& to, uint32 spellId, ObjectGuid const& caster)
{
    SpellAuraHolder const* source = from.GetSpellAuraHolder(spellId, caster);
    if (!source || source->IsDeleted() || to.GetSpellAuraHolder(spellId, caster))
    {
        return false;
    }
    Unit* casterUnit = source->GetCaster();
    if (!casterUnit)
    {
        return false;
    }

    // everything the copy takes from the source is read before the add, which may remove the source (a target limit)
    SpellEntry const* spellProto = source->GetSpellProto();
    int32 const left = source->GetAuraDuration();
    bool periodic[MAX_EFFECT_INDEX] = { false, false, false };
    int32 amounts[MAX_EFFECT_INDEX] = { 0, 0, 0 };

    // the source's caster, item, stack and charges, and the time it has left as the copy's duration and full duration
    SpellAuraHolder* copy = CreateSpellAuraHolder(spellProto, &to, casterUnit);
    copy->SetLoadedState(caster, source->GetCastItemGuid(), source->GetStackAmount(), source->GetAuraCharges(), left,
                         left);
    for (int32 eff = 0; eff < MAX_EFFECT_INDEX; ++eff)
    {
        Aura const* aura = source->GetAuraByEffectIndex(SpellEffectIndex(eff));
        if (!aura)
        {
            continue;
        }
        int32 basePoints = aura->GetBasePoints();
        Aura* made = CreateAura(spellProto, SpellEffectIndex(eff), &basePoints, copy, &to, casterUnit);
        periodic[eff] = aura->IsPeriodic() && aura->GetModifier()->periodictime != 0;
        if (periodic[eff])
        {
            // the source's period as it ticks, not the one the caster's haste gives now
            made->GetModifier()->periodictime = aura->GetModifier()->periodictime;
            made->CarryTickTimer(aura->GetPeriodicTimer());
            made->GetModifier()->m_amount = aura->GetTickAmountBeforeDone();
        }
        amounts[eff] = aura->GetModifier()->m_amount;
        copy->AddAura(made, SpellEffectIndex(eff));
    }

    if (!to.AddSpellAuraHolder(copy))
    {
        return false;
    }

    // a periodic aura's apply adds its caster's bonus to the amount; the source's amount holds that bonus already
    if (SpellAuraHolder* added = to.GetSpellAuraHolder(spellId, caster))
    {
        for (int32 eff = 0; eff < MAX_EFFECT_INDEX; ++eff)
        {
            Aura* aura = added->GetAuraByEffectIndex(SpellEffectIndex(eff));
            if (aura && periodic[eff] && aura->GetModifier()->m_amount != amounts[eff])
            {
                aura->ChangeAmount(amounts[eff]);
            }
        }
    }
    return true;
}

namespace
{
    /// The class-script ids whose bonus holds while the victim is under 35% health: Molten Fury's three ranks (31679,
    /// 31680, 86880) and Death's Embrace's three (47198, 47199, 47200).
    int32 const LOW_HEALTH_SCRIPTS[] = { 4919, 4920, 12368, 6917, 6926, 6928 };

    bool IsLowHealthScript(int32 script)
    {
        for (int32 id : LOW_HEALTH_SCRIPTS)
        {
            if (id == script)
            {
                return true;
            }
        }
        return false;
    }

    /// The proc flags the proc loop reads for `spell` (Unit::IsTriggeredAtSpellProcEvent): its spell_proc_event row's
    /// when that row names any, else the spell's own.
    uint32 ProcFlagsOf(SpellEntry const& spell)
    {
        SpellProcEventEntry const* event = sSpellMgr.GetSpellProcEvent(spell.ID);
        return event && event->procFlags ? event->procFlags : spell.GetProcFlags();
    }
}

float Spells::LowHealthDamageMultiplier(Unit const& caster, Unit const& victim, SpellEntry const& spell)
{
    if (!victim.HasAuraState(AURA_STATE_HEALTHLESS_35_PERCENT))
    {
        return 1.0f;
    }

    // the talents of a pet's or a totem's owner count for its spells
    Unit const* owner = caster.GetOwner();
    if (!owner)
    {
        owner = &caster;
    }

    float multiplier = 1.0f;
    Unit::AuraList const& scripts = owner->GetAurasByType(SPELL_AURA_OVERRIDE_CLASS_SCRIPTS);
    for (Unit::AuraList::const_iterator i = scripts.begin(); i != scripts.end(); ++i)
    {
        Modifier const* mod = (*i)->GetModifier();
        if (IsLowHealthScript(mod->m_miscvalue) && (*i)->isAffectedOnSpell(&spell))
        {
            multiplier *= (100.0f + mod->m_amount) / 100.0f;
        }
    }
    return multiplier;
}

float Spells::DamageTakenMultiplier(Unit& victim, Unit const& caster, SpellEntry const& spell)
{
    SpellEntry const* spellProto = &spell;
    uint32 const schoolMask = spell.SchoolMask;

    float takenTotalMod = victim.GetTotalAuraMultiplierByMiscMask(SPELL_AURA_MOD_DAMAGE_PERCENT_TAKEN, schoolMask);

    Unit::AuraList const& dummyAuras = victim.GetAurasByType(SPELL_AURA_DUMMY);
    for (Unit::AuraList::const_iterator i = dummyAuras.begin(); i != dummyAuras.end(); ++i)
    {
        SpellDamageTakenContext ctx(&victim, spellProto, takenTotalMod, i);
        if (SpellHandlerRegistry::Game().Dispatch<SpellDamageTakenSite>((*i)->GetId(), ctx).IsLoopContinue())
        {
            continue;
        }
    }

    Unit::AuraList const& ownerTaken = victim.GetAurasByType(SPELL_AURA_MOD_DAMAGE_FROM_CASTER);
    for (Unit::AuraList::const_iterator i = ownerTaken.begin(); i != ownerTaken.end(); ++i)
    {
        if ((*i)->GetCasterGuid() == caster.GetObjectGuid() && (*i)->isAffectedOnSpell(spellProto))
        {
            takenTotalMod *= ((*i)->GetModifier()->m_amount + 100.0f) / 100.0f;
        }
    }

    float const mechanicDamageTakenMultiplier = victim.GetTotalAuraMultiplierByMiscValueForMask(
        SPELL_AURA_MOD_MECHANIC_DAMAGE_TAKEN_PERCENT, GetAllSpellMechanicMask(spellProto));

    bool const isAreaOfEffectSpell = IsAreaOfEffectSpell(spellProto);
    float aoeDamageAvoidanceMultiplier = 1.0f;
    bool isPet = false;
    float petAoeDamageAvoidanceMultiplier = 1.0f;
    if (isAreaOfEffectSpell)
    {
        aoeDamageAvoidanceMultiplier = victim.GetTotalAuraMultiplierByMiscMask(SPELL_AURA_MOD_AOE_DAMAGE_AVOIDANCE, schoolMask);
        isPet = victim.IsPet();
        if (isPet)
        {
            petAoeDamageAvoidanceMultiplier = victim.GetTotalAuraMultiplierByMiscMask(SPELL_AURA_MOD_PET_AOE_DAMAGE_AVOIDANCE, schoolMask);
        }
    }

    return Combat::SpellDamageTakenPercent(takenTotalMod, mechanicDamageTakenMultiplier, isAreaOfEffectSpell,
                                           aoeDamageAvoidanceMultiplier, isPet, petAoeDamageAvoidanceMultiplier);
}

bool Spells::DealsDamage(SpellEntry const& spell)
{
    for (uint32 i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        SpellEffectEntry const* const effect = spell.GetSpellEffect(SpellEffectIndex(i));
        if (!effect)
        {
            continue;
        }
        switch (effect->Effect)
        {
            case SPELL_EFFECT_SCHOOL_DAMAGE:
            case SPELL_EFFECT_ENVIRONMENTAL_DAMAGE:
            case SPELL_EFFECT_HEALTH_LEECH:
            case SPELL_EFFECT_WEAPON_DAMAGE_NOSCHOOL:
            case SPELL_EFFECT_WEAPON_PERCENT_DAMAGE:
            case SPELL_EFFECT_WEAPON_DAMAGE:
            case SPELL_EFFECT_NORMALIZED_WEAPON_DMG:
                return true;
            default:
                break;
        }
        if (effect->EffectAura == SPELL_AURA_PERIODIC_DAMAGE || effect->EffectAura == SPELL_AURA_PERIODIC_DAMAGE_PERCENT ||
            effect->EffectAura == SPELL_AURA_PERIODIC_LEECH)
        {
            return true;
        }
    }
    return false;
}

bool Spells::HeldByControlDamageBreaks(Unit const& unit)
{
    for (AuraType const type : { SPELL_AURA_MOD_CONFUSE, SPELL_AURA_MOD_STUN })
    {
        Unit::AuraList const& auras = unit.GetAurasByType(type);
        for (Unit::AuraList::const_iterator i = auras.begin(); i != auras.end(); ++i)
        {
            SpellAuraHolder const* holder = (*i)->GetHolder();
            SpellEntry const* spell = (*i)->GetSpellProto();
            if (holder && !holder->IsPositive() &&
                (BreaksOnAnyDamage(spell->GetAuraInterruptFlags(), uint32(type)) ||
                 BreaksOnFirstDamageByCharge(holder->GetAuraCharges(), ProcFlagsOf(*spell), uint32(type))))
            {
                return true;
            }
        }
    }
    return false;
}

int32* ScriptWordOf(Unit& unit, uint32 spellId, SpellEffectIndex eff)
{
    Unit::SpellAuraHolderBounds const bounds = unit.GetSpellAuraHolderBounds(spellId);
    for (Unit::SpellAuraHolderMap::iterator itr = bounds.first; itr != bounds.second; ++itr)
    {
        if (!itr->second->IsDeleted())
        {
            return &itr->second->ScriptWord(eff);
        }
    }
    return NULL;
}
