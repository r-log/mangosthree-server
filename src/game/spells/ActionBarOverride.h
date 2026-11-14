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

/**
 * @file ActionBarOverride.h
 * @brief Which action-bar override buffs a cast uses up: a pure choice over plain views of the caster's auras of type
 * SPELL_AURA_OVERRIDE_ACTIONBAR_SPELLS (332) and SPELL_AURA_OVERRIDE_ACTIONBAR_SPELLS_2 (333).
 */

#ifndef MANGOS_SPELLS_ACTIONBAROVERRIDE_H
#define MANGOS_SPELLS_ACTIONBAROVERRIDE_H

#include "Platform/Define.h"

#include <vector>

namespace Spells
{
    /// One aura of the caster, as the choice reads it.
    struct OverrideAuraView
    {
        uint32 auraType;        ///< the aura's type
        uint32 spellId;         ///< the spell of the aura's holder
        int32 amount;           ///< the aura's amount: for an override aura, the spell the client casts in its place
        int32 maxDuration;      ///< the holder's maximum duration in milliseconds; -1 or 0 when it has none
    };

    /**
     * @brief The holders a cast uses up.
     *
     * An aura of type 332 or 333 whose amount is the spell cast and whose holder has a duration (above 0) is used up by
     * the cast; an aura of any other type, of another amount, or without a duration is not. An amount of 0 matches no
     * spell.
     *
     * @param auras The caster's auras, in the order they are walked.
     * @param castSpellId The spell cast.
     * @return The spell ids of the holders to remove, each once, in the order of their first aura.
     */
    std::vector<uint32> UsedUpOverrides(std::vector<OverrideAuraView> const& auras, uint32 castSpellId);
}

#endif
