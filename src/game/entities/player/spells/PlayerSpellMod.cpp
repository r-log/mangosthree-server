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

#include "Player.h"
#include "SpellAuras.h"

/**
 * @brief Applies or removes a spell modifier and notifies the client.
 *
 * @param aura The aura whose modifier should be added or removed.
 * @param apply True to apply the modifier; false to remove it.
 */
void Player::AddSpellMod(Aura* aura, bool apply)
{
    Modifier const* mod = aura->GetModifier();
    SpellModEntry entry = { aura, mod->m_auraname == SPELL_AURA_ADD_FLAT_MODIFIER, &mod->m_amount, mod->m_miscvalue, uint32(aura->GetSpellProto()->GetSpellFamilyName()), &aura->GetAuraSpellClassMask() };
    m_spellModMgr.Change(entry, apply, m_spellModSink);
}
