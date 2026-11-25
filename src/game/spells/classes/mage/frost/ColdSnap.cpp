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

#include "spells/classes/mage/Mage.h"
#include "spells/pipeline/Spell.h"
#include "spells/data/SpellMgr.h"
#include "spells/SpellModMgr.h"
#include "entities/Unit.h"
#include "entities/Creature.h"
#include "entities/Pet.h"
#include "DBCStores.h"
#include "Utilities/Util.h"

#include <algorithm>
#include <iterator>
#include <limits>
#include <utility>
#include <vector>

namespace
{
    uint32 const COLD_SNAP = 11958;
    uint32 const FROSTFIRE_ORB = 92283;
}

bool Mage::ColdSnapResets(uint32 spellId, uint32 family, uint32 schoolMask, uint32 recoveryMs)
{
    if (family != SPELLFAMILY_MAGE || spellId == COLD_SNAP || recoveryMs == 0)
    {
        return false;
    }
    return (schoolMask & SPELL_SCHOOL_MASK_FROST) != 0 || spellId == FROSTFIRE_ORB;
}

namespace Mage
{
    /// Cold Snap's dummy-effect row is a handler file's (spells/handlers/SpellEffectDummyHandlers.cpp); this file
    /// holds the leaf that row reads, and registers nothing.
    uint32 RegisterColdSnap(SpellHandlerRegistry& /*registry*/)
    {
        return 0;
    }
}
