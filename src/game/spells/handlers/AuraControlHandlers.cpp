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

#include "spells/handlers/AuraControlHandlers.h"
#include "spells/handlers/SpellHandlerRegistry.h"
#include "Object/Creature.h"

/// 26400: Arcane Shroud: two threat per target level above 60
static SpellHandlerOutcome<void> AuraThreatArcaneShroud(AuraThreatContext& ctx)
{
        // Arcane Shroud
    ctx.level_diff = ctx.target->getLevel() - 60;
    ctx.multiplier = 2;
    return SpellHandlerOutcome<void>::Continue();
    // The Eye of Diminution
}

/// 28862: The Eye of Diminution: one threat per target level above 60
static SpellHandlerOutcome<void> AuraThreatEyeOfDiminution(AuraThreatContext& ctx)
{
    ctx.level_diff = ctx.target->getLevel() - 60;
    ctx.multiplier = 1;
    return SpellHandlerOutcome<void>::Continue();
}

template <class Site>
struct AuraControlRow
{
    uint32 spellId;
    typename SpellHandler<Site>::Function function;
};

/// Registers every row of one site's table on `registry`; answers the number of rows.
template <class Site, std::size_t N>
static uint32 RegisterAuraControlRows(SpellHandlerRegistry& registry, AuraControlRow<Site> const (&rows)[N])
{
    for (AuraControlRow<Site> const& row : rows)
    {
        registry.Register<Site>(row.spellId, row.function);
    }
    return uint32(N);
}

uint32 RegisterAuraControlHandlers(SpellHandlerRegistry& registry)
{
    static AuraControlRow<AuraThreatSite> const threat[] =
    {
        { 26400, &AuraThreatArcaneShroud },
        { 28862, &AuraThreatEyeOfDiminution },
    };

    return RegisterAuraControlRows(registry, threat);
}
