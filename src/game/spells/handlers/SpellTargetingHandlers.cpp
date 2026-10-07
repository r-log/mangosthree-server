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

#include "spells/handlers/SpellTargetingHandlers.h"
#include "spells/handlers/SpellHandlerRegistry.h"
#include "Object/Creature.h"
#include "WorldHandlers/GridNotifiers.h"
#include "WorldHandlers/Spell.h"
#include "WorldHandlers/SpellTargetDistanceOrder.h"
#include "entities/player/Player.h"
#include "entities/player/PlayerRegistry.h"

/// 30769, 30843, 31347, 37676, 38028, 40618, 41376, 62166, 63981: the caster's victim is not a target
static SpellHandlerOutcome<void> TargetAllEnemyInAreaSkipVictim(SpellTargetAllEnemyInAreaContext& ctx)
{
    // Do not target current victim
    if (Unit* pVictim = ctx.m_caster->getVictim())
    {
        ctx.targetUnitMap.remove(pVictim);
    }
    return SpellHandlerOutcome<void>::Continue();
// Other special cases
}

/// 42005: Bloodboil: only the targets furthest from the caster, as many as the effect may hit
static SpellHandlerOutcome<void> TargetAllEnemyInAreaBloodboil(SpellTargetAllEnemyInAreaContext& ctx)
{
    if (ctx.targetUnitMap.size() > ctx.unMaxTargets)
    {
        ctx.targetUnitMap.sort(TargetDistanceOrderFarAway(ctx.m_caster));
        ctx.targetUnitMap.resize(ctx.unMaxTargets);
    }
    return SpellHandlerOutcome<void>::Continue();
}

/// Any other id: the area's targets as filled
static SpellHandlerOutcome<void> TargetAllEnemyInAreaDefault(SpellTargetAllEnemyInAreaContext& /*ctx*/)
{
    return SpellHandlerOutcome<void>::Continue();
}

/// 20577: Cannibalize: the first match the corpse search finds in range is the target, a dead unit or a corpse and
/// its owner; with none, a player caster's cooldown is cleared and the cast fails
static SpellHandlerOutcome<void> TargetEffectDummyCannibalize(SpellTargetEffectDummyContext& ctx)
{
    WorldObject* result = ctx.spell->FindCorpseUsing<MaNGOS::CannibalizeObjectCheck> ();

    if (result)
    {
        switch (result->GetTypeId())
        {
            case TYPEID_UNIT:
            case TYPEID_PLAYER:
                ctx.targetUnitMap.push_back((Unit*)result);
                break;
            case TYPEID_CORPSE:
                ctx.m_targets.setCorpseTarget((Corpse*)result);
                if (Player* owner = sPlayerRegistry.Find(((Corpse*)result)->GetOwnerGuid()))
                {
                    ctx.targetUnitMap.push_back(owner);
                }
                break;
        }
    }
    else
    {
        // clear cooldown at fail
        if (ctx.m_caster->GetTypeId() == TYPEID_PLAYER)
        {
            ((Player*)ctx.m_caster)->RemoveSpellCooldown(ctx.m_spellInfo->ID, true);
        }
        ctx.spell->SendCastResult(SPELL_FAILED_NO_EDIBLE_CORPSES);
        ctx.spell->finish(false);
    }
    return SpellHandlerOutcome<void>::Continue();
}

/// Any other id: the unit target, if any
static SpellHandlerOutcome<void> TargetEffectDummyDefault(SpellTargetEffectDummyContext& ctx)
{
    if (ctx.m_targets.getUnitTarget())
    {
        ctx.targetUnitMap.push_back(ctx.m_targets.getUnitTarget());
    }
    return SpellHandlerOutcome<void>::Continue();
}

template <class Site>
struct SpellTargetingRow
{
    uint32 spellId;
    typename SpellHandler<Site>::Function function;
};

/// Registers every row of one site's table on `registry`; answers the number of rows.
template <class Site, std::size_t N>
static uint32 RegisterSpellTargetingRows(SpellHandlerRegistry& registry, SpellTargetingRow<Site> const (&rows)[N])
{
    for (SpellTargetingRow<Site> const& row : rows)
    {
        registry.Register<Site>(row.spellId, row.function);
    }
    return uint32(N);
}

uint32 RegisterSpellTargetingHandlers(SpellHandlerRegistry& registry)
{
    static SpellTargetingRow<SpellTargetAllEnemyInAreaSite> const allEnemyInArea[] =
    {
        { 30769, &TargetAllEnemyInAreaSkipVictim },
        { 30843, &TargetAllEnemyInAreaSkipVictim },
        { 31347, &TargetAllEnemyInAreaSkipVictim },
        { 37676, &TargetAllEnemyInAreaSkipVictim },
        { 38028, &TargetAllEnemyInAreaSkipVictim },
        { 40618, &TargetAllEnemyInAreaSkipVictim },
        { 41376, &TargetAllEnemyInAreaSkipVictim },
        { 62166, &TargetAllEnemyInAreaSkipVictim },
        { 63981, &TargetAllEnemyInAreaSkipVictim },
        { 42005, &TargetAllEnemyInAreaBloodboil },
    };

    static SpellTargetingRow<SpellTargetEffectDummySite> const effectDummy[] =
    {
        { 20577, &TargetEffectDummyCannibalize },
    };

    uint32 rows = RegisterSpellTargetingRows(registry, allEnemyInArea);
    registry.RegisterDefault<SpellTargetAllEnemyInAreaSite>(&TargetAllEnemyInAreaDefault);
    ++rows;
    rows += RegisterSpellTargetingRows(registry, effectDummy);
    registry.RegisterDefault<SpellTargetEffectDummySite>(&TargetEffectDummyDefault);
    ++rows;
    return rows;
}
