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

#include "spells/handlers/SpellEffectObjectCombatHandlers.h"
#include "spells/handlers/SpellHandlerRegistry.h"
#include "Object/Creature.h"
#include "Object/GameObject.h"
#include "Common/TimeConstants.h"
#include "Utilities/MathDefines.h"
#include "Utilities/Util.h"
#include "Server/SharedDefines.h"

/// 24734 to 24790, the fifteen Wind Stone summons: the stone summons the templar, duke or royal the spell
/// names (one of the four at random for the three Random spells), facing the caster, and is spent
static SpellHandlerOutcome<void> EffectActivateObjectWindStoneSummon(SpellEffectActivateObjectContext& ctx)
{
    uint32 npcEntry = 0;
    uint32 templars[] = {15209, 15211, 15212, 15307};
    uint32 dukes[] = {15206, 15207, 15208, 15220};
    uint32 royals[] = {15203, 15204, 15205, 15305};

    switch (ctx.m_spellInfo->ID)
    {
        case 24734: npcEntry = templars[urand(0, 3)]; break;
        case 24763: npcEntry = dukes[urand(0, 3)];    break;
        case 24784: npcEntry = royals[urand(0, 3)];   break;
        case 24744: npcEntry = 15209;                 break;
        case 24756: npcEntry = 15212;                 break;
        case 24758: npcEntry = 15307;                 break;
        case 24760: npcEntry = 15211;                 break;
        case 24765: npcEntry = 15206;                 break;
        case 24768: npcEntry = 15220;                 break;
        case 24770: npcEntry = 15208;                 break;
        case 24772: npcEntry = 15207;                 break;
        case 24786: npcEntry = 15203;                 break;
        case 24788: npcEntry = 15204;                 break;
        case 24789: npcEntry = 15205;                 break;
        case 24790: npcEntry = 15305;                 break;
    }

    ctx.gameObjTarget->SummonCreature(npcEntry, ctx.gameObjTarget->Where().X(), ctx.gameObjTarget->Where().Y(), ctx.gameObjTarget->Where().Z(), ctx.gameObjTarget->Where().BearingTo(ctx.m_caster->Where()), TEMPSPAWN_TIMED_OOC_OR_DEAD_DESPAWN, MINUTE * IN_MILLISECONDS);
    ctx.gameObjTarget->SetLootState(GO_JUST_DEACTIVATED);
    return SpellHandlerOutcome<void>::Continue();
}

/// 40176 to 40512, the Simon Game's begin, end and switch spells: the game object stops taking interaction
static SpellHandlerOutcome<void> EffectActivateObjectSimonGame(SpellEffectActivateObjectContext& ctx)
{
    ctx.gameObjTarget->SetFlag(GAMEOBJECT_FLAGS, GO_FLAG_NO_INTERACT);
    return SpellHandlerOutcome<void>::Continue();
}

/// 40632, 40640, 40642, 40644, 41004, the Skettis summons: the game object is spent
static SpellHandlerOutcome<void> EffectActivateObjectSkettisSummon(SpellEffectActivateObjectContext& ctx)
{
    ctx.gameObjTarget->SetLootState(GO_JUST_DEACTIVATED);
    return SpellHandlerOutcome<void>::Continue();
}

/// 46085: Place Fake Fur: a creature (25835) is summoned near the game object, despawning after 15 unbroken seconds
/// alive and out of combat or once its corpse is gone, and the object is marked in use
static SpellHandlerOutcome<void> EffectActivateObjectPlaceFakeFur(SpellEffectActivateObjectContext& ctx)
{
    float x, y, z;
    ClosePointNear(*ctx.gameObjTarget, x, y, z, ctx.gameObjTarget->Where().Extent(), 2 * INTERACTION_DISTANCE, frand(0, M_PI_F * 2));

    // Note: event script is implemented in script library
    ctx.gameObjTarget->SummonCreature(25835, x, y, z, ctx.gameObjTarget->Where().Facing(), TEMPSPAWN_TIMED_OOC_OR_DEAD_DESPAWN, 15000);
    ctx.gameObjTarget->SetFlag(GAMEOBJECT_FLAGS, GO_FLAG_IN_USE);
    return SpellHandlerOutcome<void>::Continue();
}

/// 46592: Summon Ahune Lieutenant: the stone summons the lieutenant its entry names, facing the caster, and is
/// spent
static SpellHandlerOutcome<void> EffectActivateObjectSummonAhuneLieutenant(SpellEffectActivateObjectContext& ctx)
{
    uint32 npcEntry = 0;

    switch (ctx.gameObjTarget->GetEntry())
    {
        case 188049: npcEntry = 26116; break;       // Frostwave Lieutenant (Ashenvale)
        case 188137: npcEntry = 26178; break;       // Hailstone Lieutenant (Desolace)
        case 188138: npcEntry = 26204; break;       // Chillwind Lieutenant (Stranglethorn)
        case 188148: npcEntry = 26214; break;       // Frigid Lieutenant (Searing Gorge)
        case 188149: npcEntry = 26215; break;       // Glacial Lieutenant (Silithus)
        case 188150: npcEntry = 26216; break;       // Glacial Templar (Hellfire Peninsula)
    }

    ctx.gameObjTarget->SummonCreature(npcEntry, ctx.gameObjTarget->Where().X(), ctx.gameObjTarget->Where().Y(), ctx.gameObjTarget->Where().Z(), ctx.gameObjTarget->Where().BearingTo(ctx.m_caster->Where()), TEMPSPAWN_TIMED_OOC_OR_DEAD_DESPAWN, MINUTE * IN_MILLISECONDS);
    ctx.gameObjTarget->SetLootState(GO_JUST_DEACTIVATED);
    return SpellHandlerOutcome<void>::Continue();
}

/// 8342, 22999, 54732: Defibrillate: a failed roll ends the resurrection, the caster casting the failure spell
/// with the item where the spell has one; a successful roll lets it go on
static SpellHandlerOutcome<void> EffectResurrectDefibrillate(SpellEffectResurrectContext& ctx)
{
    uint32 failChance = 0;
    uint32 failSpellId = 0;
    switch (ctx.m_spellInfo->ID)
    {
        case 8342:  failChance = 67; failSpellId = 8338;  break;
        case 22999: failChance = 50; failSpellId = 23055; break;
        case 54732: failChance = 33; failSpellId = 0; break;
    }

    if (roll_chance_i(failChance))
    {
        if (failSpellId)
        {
            ctx.m_caster->CastSpell(ctx.m_caster, failSpellId, true, ctx.m_CastItem);
        }
        return SpellHandlerOutcome<void>::Return();
    }
    return SpellHandlerOutcome<void>::Continue();
}

/// Any other id: the resurrection goes on
static SpellHandlerOutcome<void> EffectResurrectDefault(SpellEffectResurrectContext& /*ctx*/)
{
    return SpellHandlerOutcome<void>::Continue();
}

template <class Site>
struct SpellEffectObjectCombatRow
{
    uint32 spellId;
    typename SpellHandler<Site>::Function function;
};

/// Registers every row of one site's table on `registry`; answers the number of rows.
template <class Site, std::size_t N>
static uint32 RegisterSpellEffectObjectCombatRows(SpellHandlerRegistry& registry,
                                                  SpellEffectObjectCombatRow<Site> const (&rows)[N])
{
    for (SpellEffectObjectCombatRow<Site> const& row : rows)
    {
        registry.Register<Site>(row.spellId, row.function);
    }
    return uint32(N);
}

uint32 RegisterSpellEffectObjectCombatHandlers(SpellHandlerRegistry& registry)
{
    static SpellEffectObjectCombatRow<SpellEffectActivateObjectSite> const activateObject[] =
    {
        { 24734, &EffectActivateObjectWindStoneSummon },
        { 24744, &EffectActivateObjectWindStoneSummon },
        { 24756, &EffectActivateObjectWindStoneSummon },
        { 24758, &EffectActivateObjectWindStoneSummon },
        { 24760, &EffectActivateObjectWindStoneSummon },
        { 24763, &EffectActivateObjectWindStoneSummon },
        { 24765, &EffectActivateObjectWindStoneSummon },
        { 24768, &EffectActivateObjectWindStoneSummon },
        { 24770, &EffectActivateObjectWindStoneSummon },
        { 24772, &EffectActivateObjectWindStoneSummon },
        { 24784, &EffectActivateObjectWindStoneSummon },
        { 24786, &EffectActivateObjectWindStoneSummon },
        { 24788, &EffectActivateObjectWindStoneSummon },
        { 24789, &EffectActivateObjectWindStoneSummon },
        { 24790, &EffectActivateObjectWindStoneSummon },
        { 40176, &EffectActivateObjectSimonGame },
        { 40177, &EffectActivateObjectSimonGame },
        { 40178, &EffectActivateObjectSimonGame },
        { 40179, &EffectActivateObjectSimonGame },
        { 40283, &EffectActivateObjectSimonGame },
        { 40284, &EffectActivateObjectSimonGame },
        { 40285, &EffectActivateObjectSimonGame },
        { 40286, &EffectActivateObjectSimonGame },
        { 40494, &EffectActivateObjectSimonGame },
        { 40495, &EffectActivateObjectSimonGame },
        { 40512, &EffectActivateObjectSimonGame },
        { 40632, &EffectActivateObjectSkettisSummon },
        { 40640, &EffectActivateObjectSkettisSummon },
        { 40642, &EffectActivateObjectSkettisSummon },
        { 40644, &EffectActivateObjectSkettisSummon },
        { 41004, &EffectActivateObjectSkettisSummon },
        { 46085, &EffectActivateObjectPlaceFakeFur },
        { 46592, &EffectActivateObjectSummonAhuneLieutenant },
    };

    static SpellEffectObjectCombatRow<SpellEffectResurrectSite> const resurrect[] =
    {
        { 8342, &EffectResurrectDefibrillate },
        { 22999, &EffectResurrectDefibrillate },
        { 54732, &EffectResurrectDefibrillate },
    };

    uint32 rows = RegisterSpellEffectObjectCombatRows(registry, activateObject);
    rows += RegisterSpellEffectObjectCombatRows(registry, resurrect);
    registry.RegisterDefault<SpellEffectResurrectSite>(&EffectResurrectDefault);
    ++rows;
    return rows;
}
