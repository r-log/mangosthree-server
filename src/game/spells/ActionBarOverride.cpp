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

#include "spells/ActionBarOverride.h"
#include "spells/data/SpellAuraDefines.h"

#include <algorithm>

std::vector<uint32> Spells::UsedUpOverrides(std::vector<OverrideAuraView> const& auras, uint32 castSpellId)
{
    std::vector<uint32> usedUp;
    for (OverrideAuraView const& aura : auras)
    {
        bool const overrides = aura.auraType == SPELL_AURA_OVERRIDE_ACTIONBAR_SPELLS ||
                               aura.auraType == SPELL_AURA_OVERRIDE_ACTIONBAR_SPELLS_2;
        if (overrides && aura.amount > 0 && uint32(aura.amount) == castSpellId && aura.maxDuration > 0 &&
            std::find(usedUp.begin(), usedUp.end(), aura.spellId) == usedUp.end())
        {
            usedUp.push_back(aura.spellId);
        }
    }
    return usedUp;
}
