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

#include "spells/handlers/SpellCheckTargetHandlers.h"
#include "spells/handlers/SpellHandlerRegistry.h"
#include "Object/Creature.h"

/// 37433: Spout (The Lurker Below): only a player out of the water is a target
static SpellHandlerOutcome<bool> CheckTargetSpout(SpellCheckTargetContext& ctx)
{
    if (ctx.target->GetTypeId() != TYPEID_PLAYER || ctx.target->IsInWater())
    {
        return SpellHandlerOutcome<bool>::Return(false);
    }
    return SpellHandlerOutcome<bool>::Continue();
}

/// 68921, 69049: Soulstorm: refuses a target in the caster's frame less than 10 yards from the caster, edge to edge
/// in 2D
static SpellHandlerOutcome<bool> CheckTargetSoulstorm(SpellCheckTargetContext& ctx)
{
    if (ctx.m_caster->Where().WithinDist(ctx.target->Where(), 10.0f, false))
    {
        return SpellHandlerOutcome<bool>::Return(false);
    }
    return SpellHandlerOutcome<bool>::Continue();
}

/// Any other id: no check of its own
static SpellHandlerOutcome<bool> CheckTargetDefault(SpellCheckTargetContext& /*ctx*/)
{
    return SpellHandlerOutcome<bool>::Continue();
}

template <class Site>
struct SpellCheckTargetRow
{
    uint32 spellId;
    typename SpellHandler<Site>::Function function;
};

/// Registers every row of one site's table on `registry`; answers the number of rows.
template <class Site, std::size_t N>
static uint32 RegisterSpellCheckTargetRows(SpellHandlerRegistry& registry, SpellCheckTargetRow<Site> const (&rows)[N])
{
    for (SpellCheckTargetRow<Site> const& row : rows)
    {
        registry.Register<Site>(row.spellId, row.function);
    }
    return uint32(N);
}

uint32 RegisterSpellCheckTargetHandlers(SpellHandlerRegistry& registry)
{
    static SpellCheckTargetRow<SpellCheckTargetSite> const target[] =
    {
        { 37433, &CheckTargetSpout },
        { 68921, &CheckTargetSoulstorm },
        { 69049, &CheckTargetSoulstorm },
    };

    uint32 rows = RegisterSpellCheckTargetRows(registry, target);
    registry.RegisterDefault<SpellCheckTargetSite>(&CheckTargetDefault);
    ++rows;
    return rows;
}
