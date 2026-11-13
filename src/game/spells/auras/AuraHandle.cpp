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

#include "spells/SpellScriptHooks.h"
#include "spells/auras/SpellAuras.h"

uint32 AuraHandle::Charges() const
{
    return m_holder.GetAuraCharges();
}

bool AuraHandle::DropCharge()
{
    return m_holder.DropAuraCharge();
}

int32 AuraHandle::Amount(SpellEffectIndex eff) const
{
    Aura const* aura = m_holder.GetAuraByEffectIndex(eff);
    return aura ? aura->GetModifier()->m_amount : 0;
}

void AuraHandle::SetAmount(SpellEffectIndex eff, int32 amount)
{
    if (Aura* aura = m_holder.GetAuraByEffectIndex(eff))
    {
        aura->GetModifier()->m_amount = amount;
    }
}

int32& AuraHandle::ScriptWord(SpellEffectIndex eff)
{
    return m_holder.ScriptWord(eff);
}

AuraRemoveMode AuraHandle::RemoveMode() const
{
    return m_holder.GetRemoveMode();
}

ObjectGuid AuraHandle::CasterGuid() const
{
    return m_holder.GetCasterGuid();
}

int32 AuraHandle::Duration() const
{
    return m_holder.GetAuraDuration();
}
