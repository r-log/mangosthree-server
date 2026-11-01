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

#include "spells/handlers/SpellMgrHandlers.h"
#include "spells/handlers/SpellHandlerRegistry.h"
#include "BattleGround/BattleGround.h"
#include "data/dbc/DBCStores.h"
#include "entities/player/Player.h"

/// 22564, 22563: Recall: only on Alterac Valley's map (30), in its battleground once that has begun
static SpellHandlerOutcome<SpellCastResult> AllowedInLocationAlteracValleyRecall(SpellAllowedInLocationContext& ctx)
{
        // a trinket in alterac valley allows to teleport to the boss
    if (!ctx.player)
    {
        return SpellHandlerOutcome<SpellCastResult>::Return(SPELL_FAILED_REQUIRES_AREA);
    }
    BattleGround* bg = ctx.player->GetBattleGround();
    return SpellHandlerOutcome<SpellCastResult>::Return(ctx.map_id == 30 && bg
           && bg->GetStatus() != STATUS_WAIT_JOIN ? SPELL_CAST_OK : SPELL_FAILED_REQUIRES_AREA);
}

/// 23333, 23335: Horde Flag and Alliance Flag: only on Warsong Gulch's map (489), in a battleground
static SpellHandlerOutcome<SpellCastResult> AllowedInLocationWarsongFlag(SpellAllowedInLocationContext& ctx)
{
    return SpellHandlerOutcome<SpellCastResult>::Return(ctx.map_id == 489 && ctx.player && ctx.player->InBattleGround() ? SPELL_CAST_OK : SPELL_FAILED_REQUIRES_AREA);
}

/// 34976: Netherstorm Flag: only on Eye of the Storm's map (566), in a battleground
static SpellHandlerOutcome<SpellCastResult> AllowedInLocationNetherstormFlag(SpellAllowedInLocationContext& ctx)
{
    return SpellHandlerOutcome<SpellCastResult>::Return(ctx.map_id == 566 && ctx.player && ctx.player->InBattleGround() ? SPELL_CAST_OK : SPELL_FAILED_REQUIRES_AREA);
}

/// 2584, 42792, 43681: Waiting to Resurrect, Recently Dropped Flag and Inactive: only in a battleground
static SpellHandlerOutcome<SpellCastResult> AllowedInLocationInBattleground(SpellAllowedInLocationContext& ctx)
{
    return SpellHandlerOutcome<SpellCastResult>::Return(ctx.player && ctx.player->InBattleGround() ? SPELL_CAST_OK : SPELL_FAILED_ONLY_BATTLEGROUNDS);
}

/// 22011, 22012, 24171, 44535: Spirit Heal Channel, Spirit Heal, Resurrection Impact Visual and Spirit Heal: only on
/// a battleground map
static SpellHandlerOutcome<SpellCastResult> AllowedInLocationSpiritHeal(SpellAllowedInLocationContext& ctx)
{
    MapEntry const* mapEntry = sMapStore.LookupEntry(ctx.map_id);
    if (!mapEntry)
    {
        return SpellHandlerOutcome<SpellCastResult>::Return(SPELL_FAILED_INCORRECT_AREA);
    }
    return SpellHandlerOutcome<SpellCastResult>::Return(mapEntry->IsBattleGround() ? SPELL_CAST_OK : SPELL_FAILED_ONLY_BATTLEGROUNDS);
}

/// 44521: Preparation: only in a battleground that has not begun
static SpellHandlerOutcome<SpellCastResult> AllowedInLocationBattlegroundPreparation(SpellAllowedInLocationContext& ctx)
{
    if (!ctx.player)
    {
        return SpellHandlerOutcome<SpellCastResult>::Return(SPELL_FAILED_REQUIRES_AREA);
    }

    BattleGround* bg = ctx.player->GetBattleGround();
    return SpellHandlerOutcome<SpellCastResult>::Return(bg && bg->GetStatus() == STATUS_WAIT_JOIN ? SPELL_CAST_OK : SPELL_FAILED_ONLY_BATTLEGROUNDS);
}

/// 32724, 32725, 35774, 35775: Gold Team and Green Team: only in an arena
static SpellHandlerOutcome<SpellCastResult> AllowedInLocationArenaTeam(SpellAllowedInLocationContext& ctx)
{
    return SpellHandlerOutcome<SpellCastResult>::Return(ctx.player && ctx.player->InArena() ? SPELL_CAST_OK : SPELL_FAILED_ONLY_IN_ARENA);
}

/// 32727: Arena Preparation: only in an arena that has not begun
static SpellHandlerOutcome<SpellCastResult> AllowedInLocationArenaPreparation(SpellAllowedInLocationContext& ctx)
{
    if (!ctx.player)
    {
        return SpellHandlerOutcome<SpellCastResult>::Return(SPELL_FAILED_REQUIRES_AREA);
    }
    if (!ctx.player->InArena())
    {
        return SpellHandlerOutcome<SpellCastResult>::Return(SPELL_FAILED_REQUIRES_AREA);
    }

    BattleGround* bg = ctx.player->GetBattleGround();
    return SpellHandlerOutcome<SpellCastResult>::Return(bg && bg->GetStatus() == STATUS_WAIT_JOIN ? SPELL_CAST_OK : SPELL_FAILED_ONLY_IN_ARENA);
}

/// 74410: Arena - Dampening: only in an arena
static SpellHandlerOutcome<SpellCastResult> AllowedInLocationArenaDampening(SpellAllowedInLocationContext& ctx)
{
    return SpellHandlerOutcome<SpellCastResult>::Return(ctx.player && ctx.player->InArena() ? SPELL_CAST_OK : SPELL_FAILED_ONLY_IN_ARENA);
}

/// 74411: Battleground - Dampening: only in a battleground that is not an arena
static SpellHandlerOutcome<SpellCastResult> AllowedInLocationBattlegroundDampening(SpellAllowedInLocationContext& ctx)
{
    if (!ctx.player)
    {
        return SpellHandlerOutcome<SpellCastResult>::Return(SPELL_FAILED_ONLY_BATTLEGROUNDS);
    }

    BattleGround* bg = ctx.player->GetBattleGround();
    return SpellHandlerOutcome<SpellCastResult>::Return(bg && !bg->isArena() ? SPELL_CAST_OK : SPELL_FAILED_ONLY_BATTLEGROUNDS);
}

template <class Site>
struct SpellMgrRow
{
    uint32 spellId;
    typename SpellHandler<Site>::Function function;
};

/// Registers every row of one site's table on `registry`; answers the number of rows.
template <class Site, std::size_t N>
static uint32 RegisterSpellMgrRows(SpellHandlerRegistry& registry, SpellMgrRow<Site> const (&rows)[N])
{
    for (SpellMgrRow<Site> const& row : rows)
    {
        registry.Register<Site>(row.spellId, row.function);
    }
    return uint32(N);
}

uint32 RegisterSpellMgrHandlers(SpellHandlerRegistry& registry)
{
    static SpellMgrRow<SpellAllowedInLocationSite> const allowedInLocation[] =
    {
        { 22564, &AllowedInLocationAlteracValleyRecall },
        { 22563, &AllowedInLocationAlteracValleyRecall },
        { 23333, &AllowedInLocationWarsongFlag },
        { 23335, &AllowedInLocationWarsongFlag },
        { 34976, &AllowedInLocationNetherstormFlag },
        { 2584, &AllowedInLocationInBattleground },
        { 42792, &AllowedInLocationInBattleground },
        { 43681, &AllowedInLocationInBattleground },
        { 22011, &AllowedInLocationSpiritHeal },
        { 22012, &AllowedInLocationSpiritHeal },
        { 24171, &AllowedInLocationSpiritHeal },
        { 44535, &AllowedInLocationSpiritHeal },
        { 44521, &AllowedInLocationBattlegroundPreparation },
        { 32724, &AllowedInLocationArenaTeam },
        { 32725, &AllowedInLocationArenaTeam },
        { 35774, &AllowedInLocationArenaTeam },
        { 35775, &AllowedInLocationArenaTeam },
        { 32727, &AllowedInLocationArenaPreparation },
        { 74410, &AllowedInLocationArenaDampening },
        { 74411, &AllowedInLocationBattlegroundDampening },
    };

    return RegisterSpellMgrRows(registry, allowedInLocation);
}
