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

namespace
{
    uint32 const RITUAL_OF_REFRESHMENT[] = { 43987 };

    /// 43987: Ritual of Refreshment: the caster casts at himself, triggered, the rank his level allows
    /// (Mage::RitualOfRefreshmentRank), whose summon puts that rank's Refreshment Portal at his feet. The ritual and
    /// its three ranks share one cooldown category (1177, 300 s), which the ritual's cast starts before its effect
    /// runs, and a cast of a spell on cooldown is refused, triggered or not; so the rank's cooldown is taken off him
    /// first, his client not told, and the rank's cast starts the category's cooldown again. The script ends the
    /// effect.
    SpellHandlerOutcome<void> RitualOfRefreshmentEffect(DummyEffectContext& ctx)
    {
        uint32 const rank = Mage::RitualOfRefreshmentRank(ctx.caster.getLevel());
        ctx.caster.RemoveSpellCooldown(rank);
        ctx.CastTriggered(ctx.caster, rank);
        return SpellHandlerOutcome<void>::Return();
    }
}

uint32 Mage::RitualOfRefreshmentRank(uint32 level)
{
    // Each rank summons a portal whose table gives one conjured food. From 80 the rank is the highest whose food's
    // RequiredLevel the level reaches; below 80 it is the Biscuit table's at any level, though the Biscuit asks for 65.
    constexpr uint32 BISCUIT_TABLE_RANK = 74650;    // portal 186811: Conjured Mana Biscuit (34062), RequiredLevel 65
    constexpr uint32 STRUDEL_TABLE_RANK = 92824;    // portal 207385: Conjured Mana Strudel (43523), RequiredLevel 80
    constexpr uint32 CAKE_TABLE_RANK = 92827;       // portal 207388: Conjured Mana Cake (65499), RequiredLevel 85
    constexpr uint32 STRUDEL_LEVEL = 80;
    constexpr uint32 CAKE_LEVEL = 85;
    if (level >= CAKE_LEVEL)
    {
        return CAKE_TABLE_RANK;
    }
    if (level >= STRUDEL_LEVEL)
    {
        return STRUDEL_TABLE_RANK;
    }
    return BISCUIT_TABLE_RANK;
}

namespace Mage
{
    uint32 RegisterRitualOfRefreshment(SpellHandlerRegistry& registry)
    {
        return RegisterRanks<DummyEffectHook>(registry, RITUAL_OF_REFRESHMENT, &RitualOfRefreshmentEffect);
    }
}
