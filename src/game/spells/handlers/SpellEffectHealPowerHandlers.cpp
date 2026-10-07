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

#include "spells/handlers/SpellEffectHealPowerHandlers.h"
#include "spells/handlers/SpellHandlerRegistry.h"
#include "Object/Creature.h"
#include "entities/player/Player.h"
#include "Server/SharedDefines.h"

/// 9512: Restore Energy: the caster's levels above 40 shrink the energy by 2 each
static SpellHandlerOutcome<void> EffectEnergizeRestoreEnergy(SpellEffectEnergizeContext& ctx)
{
    ctx.level_diff = ctx.m_caster->getLevel() - 40;
    ctx.level_multiplier = 2;
    return SpellHandlerOutcome<void>::Continue();
}

/// 24571: Blood Fury: the caster's levels above 60 shrink the energy by 10 each
static SpellHandlerOutcome<void> EffectEnergizeBloodFury(SpellEffectEnergizeContext& ctx)
{
    ctx.level_diff = ctx.m_caster->getLevel() - 60;
    ctx.level_multiplier = 10;
    return SpellHandlerOutcome<void>::Continue();
}

/// 24532: Burst of Energy: the caster's levels above 60 shrink the energy by 4 each
static SpellHandlerOutcome<void> EffectEnergizeBurstOfEnergy(SpellEffectEnergizeContext& ctx)
{
    ctx.level_diff = ctx.m_caster->getLevel() - 60;
    ctx.level_multiplier = 4;
    return SpellHandlerOutcome<void>::Continue();
}

/// 31930, 48542, 63375, 68082: the amount is a percentage of the target's base mana
static SpellHandlerOutcome<void> EffectEnergizeBaseManaPercent(SpellEffectEnergizeContext& ctx)
{
    ctx.damage = ctx.damage * ctx.unitTarget->GetCreateMana() / 100;
    return SpellHandlerOutcome<void>::Continue();
}

/// 67487, 67490: Mana Potion Injector, Runic Mana Injector: a quarter more for a player target with Engineering
static SpellHandlerOutcome<void> EffectEnergizeManaInjector(SpellEffectEnergizeContext& ctx)
{
    if (ctx.unitTarget->GetTypeId() == TYPEID_PLAYER)
    {
        Player* player = (Player*)ctx.unitTarget;
        if (player->HasSkill(SKILL_ENGINEERING))
        {
            ctx.damage += int32(ctx.damage * 0.25);
        }
    }
    return SpellHandlerOutcome<void>::Continue();
}

/// Any other id: the effect's own amount, not shrunk by level
static SpellHandlerOutcome<void> EffectEnergizeDefault(SpellEffectEnergizeContext& /*ctx*/)
{
    return SpellHandlerOutcome<void>::Continue();
}

template <class Site>
struct SpellEffectHealPowerRow
{
    uint32 spellId;
    typename SpellHandler<Site>::Function function;
};

/// Registers every row of one site's table on `registry`; answers the number of rows.
template <class Site, std::size_t N>
static uint32 RegisterSpellEffectHealPowerRows(SpellHandlerRegistry& registry,
                                               SpellEffectHealPowerRow<Site> const (&rows)[N])
{
    for (SpellEffectHealPowerRow<Site> const& row : rows)
    {
        registry.Register<Site>(row.spellId, row.function);
    }
    return uint32(N);
}

uint32 RegisterSpellEffectHealPowerHandlers(SpellHandlerRegistry& registry)
{
    static SpellEffectHealPowerRow<SpellEffectEnergizeSite> const energize[] =
    {
        { 9512, &EffectEnergizeRestoreEnergy },
        { 24571, &EffectEnergizeBloodFury },
        { 24532, &EffectEnergizeBurstOfEnergy },
        { 31930, &EffectEnergizeBaseManaPercent },
        { 48542, &EffectEnergizeBaseManaPercent },
        { 63375, &EffectEnergizeBaseManaPercent },
        { 68082, &EffectEnergizeBaseManaPercent },
        { 67487, &EffectEnergizeManaInjector },
        { 67490, &EffectEnergizeManaInjector },
    };

    uint32 rows = RegisterSpellEffectHealPowerRows(registry, energize);
    registry.RegisterDefault<SpellEffectEnergizeSite>(&EffectEnergizeDefault);
    ++rows;
    return rows;
}
