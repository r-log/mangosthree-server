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

#include "spells/handlers/SpellChecksHandlers.h"
#include "spells/handlers/SpellHandlerRegistry.h"
#include "entities/player/Player.h"

/// 34026: Kill Command: the caster must have a pet
static SpellHandlerOutcome<SpellCastResult> CheckCastKillCommand(SpellCheckCastAuraDummyContext& ctx)
{
    if (!ctx.m_caster->GetPet())
    {
        return SpellHandlerOutcome<SpellCastResult>::Return(SPELL_FAILED_NO_PET);
    }
    return SpellHandlerOutcome<SpellCastResult>::Continue();
}

/// 61336: Survival Instincts: the caster must be a player in a feral form
static SpellHandlerOutcome<SpellCastResult> CheckCastSurvivalInstincts(SpellCheckCastAuraDummyContext& ctx)
{
    if (ctx.m_caster->GetTypeId() != TYPEID_PLAYER || !((Player*)ctx.m_caster)->IsInFeralForm())
    {
        return SpellHandlerOutcome<SpellCastResult>::Return(SPELL_FAILED_ONLY_SHAPESHIFT);
    }
    return SpellHandlerOutcome<SpellCastResult>::Continue();
}

/// Any other id: no check of its own
static SpellHandlerOutcome<SpellCastResult> CheckCastAuraDummyDefault(SpellCheckCastAuraDummyContext& /*ctx*/)
{
    return SpellHandlerOutcome<SpellCastResult>::Continue();
}

template <class Site>
struct SpellChecksRow
{
    uint32 spellId;
    typename SpellHandler<Site>::Function function;
};

/// Registers every row of one site's table on `registry`; answers the number of rows.
template <class Site, std::size_t N>
static uint32 RegisterSpellChecksRows(SpellHandlerRegistry& registry, SpellChecksRow<Site> const (&rows)[N])
{
    for (SpellChecksRow<Site> const& row : rows)
    {
        registry.Register<Site>(row.spellId, row.function);
    }
    return uint32(N);
}

uint32 RegisterSpellChecksHandlers(SpellHandlerRegistry& registry)
{
    static SpellChecksRow<SpellCheckCastAuraDummySite> const castAuraDummy[] =
    {
        { 34026, &CheckCastKillCommand },
        { 61336, &CheckCastSurvivalInstincts },
    };

    uint32 rows = RegisterSpellChecksRows(registry, castAuraDummy);
    registry.RegisterDefault<SpellCheckCastAuraDummySite>(&CheckCastAuraDummyDefault);
    ++rows;
    return rows;
}
