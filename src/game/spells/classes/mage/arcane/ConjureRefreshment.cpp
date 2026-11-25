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
#include "spells/handlers/AurasHandlers.h"
#include "spells/handlers/SpellEffectDummyHandlers.h"
#include "spells/handlers/UnitAuraProcHandlerHandlers.h"
#include "spells/pipeline/Spell.h"
#include "spells/data/SpellMgr.h"
#include "entities/Unit.h"
#include "entities/Pet.h"
#include "DBCStores.h"
#include "Log.h"
#include "Utilities/Util.h"

/// 42955: Conjure Refreshment: the damage is set to 20 and the spell creates that many of the conjured food the
/// target's level calls for; the effect ends
static SpellHandlerOutcome<void> EffectDummyMageConjureRefreshment(SpellEffectDummyMageContext& ctx)
{
    uint32 item = 0;

    uint32 level = ctx.unitTarget->getLevel();

    if (level < 44)
    {
        item = 65500;                                 // Conjured Mana Cookie (lvl 34)
    }
    else if (level < 54)
    {
        item = 65515;                            // Conjured Mana Brownie (lvl 44)
    }
    else if (level < 64)
    {
        item = 65516;                            // Conjured Mana Cupcake (lvl 54)
    }
    else if (level < 65)
    {
        item = 65517;                            // Conjured Mana Lollipop (lvl 64)
    }
    else if (level < 74)
    {
        item = 34062;                            // Conjured Mana Biscuit (lvl 65)
    }
    else if (level < 80)
    {
        item = 43518;                            // Conjured Mana Pie (lvl 74)
    }
    else if (level < 85)
    {
        item = 43523;                            // Conjured Mana Strudel (lvl 80)
    }
    else
    {
        item = 65499;                                            // Conjured Mana Cake (lvl 85)
    }

    ctx.damage = 20; // Used to set stack size.
    ctx.spell->DoCreateItem(ctx.effect,item);

    return SpellHandlerOutcome<void>::Return();
}

namespace Mage
{
    uint32 RegisterConjureRefreshment(SpellHandlerRegistry& registry)
    {
        static Row<SpellEffectDummyMageSite> const effectDummyMage[] =
        {
            { 42955, &EffectDummyMageConjureRefreshment },
        };

        return RegisterRows(registry, effectDummyMage);
    }
}
