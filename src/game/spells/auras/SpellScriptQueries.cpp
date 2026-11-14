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
