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
        periodic[eff] = aura->IsPeriodic() && made->GetModifier()->periodictime != 0;
        if (periodic[eff])
        {
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
