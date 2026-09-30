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

#include "spells/handlers/AuraDummyHandlers.h"
#include "spells/handlers/SpellHandlerRegistry.h"
#include "Object/Creature.h"
#include "WorldHandlers/SpellAuras.h"
#include "Log/Log.h"

/// SPELLFAMILY_WARRIOR 41099: Battle Stance
static SpellHandlerOutcome<void> AuraDummyApplyWarrior41099(AuraDummyApplyContext& ctx)
{
    if (ctx.target->GetTypeId() != TYPEID_UNIT)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // Stance Cooldown
    ctx.target->CastSpell(ctx.target, 41102, true, NULL, ctx.aura);

    // Battle Aura
    ctx.target->CastSpell(ctx.target, 41106, true, NULL, ctx.aura);

    // equipment
    ((Creature*)ctx.target)->SetVirtualItem(VIRTUAL_ITEM_SLOT_0, 32614);
    ((Creature*)ctx.target)->SetVirtualItem(VIRTUAL_ITEM_SLOT_1, 0);
    ((Creature*)ctx.target)->SetVirtualItem(VIRTUAL_ITEM_SLOT_2, 0);
    return SpellHandlerOutcome<void>::Return();
}

/// SPELLFAMILY_WARRIOR 41100: Berserker Stance
static SpellHandlerOutcome<void> AuraDummyApplyWarrior41100(AuraDummyApplyContext& ctx)
{
    if (ctx.target->GetTypeId() != TYPEID_UNIT)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // Stance Cooldown
    ctx.target->CastSpell(ctx.target, 41102, true, NULL, ctx.aura);

    // Berserker Aura
    ctx.target->CastSpell(ctx.target, 41107, true, NULL, ctx.aura);

    // equipment
    ((Creature*)ctx.target)->SetVirtualItem(VIRTUAL_ITEM_SLOT_0, 32614);
    ((Creature*)ctx.target)->SetVirtualItem(VIRTUAL_ITEM_SLOT_1, 0);
    ((Creature*)ctx.target)->SetVirtualItem(VIRTUAL_ITEM_SLOT_2, 0);
    return SpellHandlerOutcome<void>::Return();
}

/// SPELLFAMILY_WARRIOR 41101: Defensive Stance
static SpellHandlerOutcome<void> AuraDummyApplyWarrior41101(AuraDummyApplyContext& ctx)
{
    if (ctx.target->GetTypeId() != TYPEID_UNIT)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // Stance Cooldown
    ctx.target->CastSpell(ctx.target, 41102, true, NULL, ctx.aura);

    // Defensive Aura
    ctx.target->CastSpell(ctx.target, 41105, true, NULL, ctx.aura);

    // equipment
    ((Creature*)ctx.target)->SetVirtualItem(VIRTUAL_ITEM_SLOT_0, 32604);
    ((Creature*)ctx.target)->SetVirtualItem(VIRTUAL_ITEM_SLOT_1, 31467);
    ((Creature*)ctx.target)->SetVirtualItem(VIRTUAL_ITEM_SLOT_2, 0);
    return SpellHandlerOutcome<void>::Return();
}

/// SPELLFAMILY_WARRIOR 53790: Defensive Stance
static SpellHandlerOutcome<void> AuraDummyApplyWarrior53790(AuraDummyApplyContext& ctx)
{
    if (ctx.target->GetTypeId() != TYPEID_UNIT)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // Stance Cooldown
    ctx.target->CastSpell(ctx.target, 59526, true, NULL, ctx.aura);

    // Defensive Aura
    ctx.target->CastSpell(ctx.target, 41105, true, NULL, ctx.aura);

    // equipment
    ((Creature*)ctx.target)->SetVirtualItem(VIRTUAL_ITEM_SLOT_0, 43625);
    ((Creature*)ctx.target)->SetVirtualItem(VIRTUAL_ITEM_SLOT_1, 39384);
    ((Creature*)ctx.target)->SetVirtualItem(VIRTUAL_ITEM_SLOT_2, 0);
    return SpellHandlerOutcome<void>::Return();
}

/// SPELLFAMILY_WARRIOR 53791: Berserker Stance
static SpellHandlerOutcome<void> AuraDummyApplyWarrior53791(AuraDummyApplyContext& ctx)
{
    if (ctx.target->GetTypeId() != TYPEID_UNIT)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // Stance Cooldown
    ctx.target->CastSpell(ctx.target, 59526, true, NULL, ctx.aura);

    // Berserker Aura
    ctx.target->CastSpell(ctx.target, 41107, true, NULL, ctx.aura);

    // equipment
    ((Creature*)ctx.target)->SetVirtualItem(VIRTUAL_ITEM_SLOT_0, 43625);
    ((Creature*)ctx.target)->SetVirtualItem(VIRTUAL_ITEM_SLOT_1, 43625);
    ((Creature*)ctx.target)->SetVirtualItem(VIRTUAL_ITEM_SLOT_2, 0);
    return SpellHandlerOutcome<void>::Return();
}

/// SPELLFAMILY_WARRIOR 53792: Battle Stance
static SpellHandlerOutcome<void> AuraDummyApplyWarrior53792(AuraDummyApplyContext& ctx)
{
    if (ctx.target->GetTypeId() != TYPEID_UNIT)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // Stance Cooldown
    ctx.target->CastSpell(ctx.target, 59526, true, NULL, ctx.aura);

    // Battle Aura
    ctx.target->CastSpell(ctx.target, 41106, true, NULL, ctx.aura);

    // equipment
    ((Creature*)ctx.target)->SetVirtualItem(VIRTUAL_ITEM_SLOT_0, 43623);
    ((Creature*)ctx.target)->SetVirtualItem(VIRTUAL_ITEM_SLOT_1, 0);
    ((Creature*)ctx.target)->SetVirtualItem(VIRTUAL_ITEM_SLOT_2, 0);
    return SpellHandlerOutcome<void>::Return();
}

/// SPELLFAMILY_WARRIOR, Overpower's Unrelenting Assault 46859: rank 1
static SpellHandlerOutcome<void> AuraDummyUnrelentingAssault46859(AuraDummyUnrelentingAssaultContext& ctx)
{
    ctx.target->CastSpell(ctx.target, 64849, true, NULL, (*ctx.itr));
    return SpellHandlerOutcome<void>::Continue();
}

/// SPELLFAMILY_WARRIOR, Overpower's Unrelenting Assault 46860: rank 2
static SpellHandlerOutcome<void> AuraDummyUnrelentingAssault46860(AuraDummyUnrelentingAssaultContext& ctx)
{
    ctx.target->CastSpell(ctx.target, 64850, true, NULL, (*ctx.itr));
    return SpellHandlerOutcome<void>::Continue();
}

/// SPELLFAMILY_WARRIOR, Overpower's Unrelenting Assault switch: its `default:`
static SpellHandlerOutcome<void> AuraDummyUnrelentingAssaultDefault(AuraDummyUnrelentingAssaultContext& /*ctx*/)
{
    return SpellHandlerOutcome<void>::Continue();
}

/// SPELLFAMILY_DRUID 52610: Savage Roar
static SpellHandlerOutcome<void> AuraDummyDruid52610(AuraDummyApplyRemoveContext& ctx)
{
    if (ctx.apply)
    {
        if (ctx.target->GetShapeshiftForm() != FORM_CAT)
        {
            return SpellHandlerOutcome<void>::Return();
        }

        ctx.target->CastSpell(ctx.target, 62071, true);
    }
    else
    {
        ctx.target->RemoveAurasDueToSpell(62071);
    }
    return SpellHandlerOutcome<void>::Return();
}

/// SPELLFAMILY_DRUID 61336: Survival Instincts
static SpellHandlerOutcome<void> AuraDummyDruid61336(AuraDummyApplyRemoveContext& ctx)
{
    if (ctx.apply)
    {
        if (!ctx.target->IsInFeralForm())
        {
            return SpellHandlerOutcome<void>::Return();
        }

        int32 bp0 = int32(ctx.target->GetMaxHealth() * ctx.aura->GetModifier()->m_amount / 100);
        ctx.target->CastCustomSpell(ctx.target, 50322, &bp0, NULL, NULL, true);
    }
    else
    {
        ctx.target->RemoveAurasDueToSpell(50322);
    }
    return SpellHandlerOutcome<void>::Return();
}

/// SPELLFAMILY_DRUID, Improved Moonkin Form 48384: rank 1
static SpellHandlerOutcome<void> AuraDummyImprovedMoonkin48384(AuraDummyImprovedMoonkinContext& ctx)
{
    ctx.spell_id = 50170;
    return SpellHandlerOutcome<void>::Continue();
}

/// SPELLFAMILY_DRUID, Improved Moonkin Form 48395: rank 2
static SpellHandlerOutcome<void> AuraDummyImprovedMoonkin48395(AuraDummyImprovedMoonkinContext& ctx)
{
    ctx.spell_id = 50171;
    return SpellHandlerOutcome<void>::Continue();
}

/// SPELLFAMILY_DRUID, Improved Moonkin Form 48396: rank 3
static SpellHandlerOutcome<void> AuraDummyImprovedMoonkin48396(AuraDummyImprovedMoonkinContext& ctx)
{
    ctx.spell_id = 50172;
    return SpellHandlerOutcome<void>::Continue();
}

/// SPELLFAMILY_DRUID, Improved Moonkin Form's rank switch: its `default:`
static SpellHandlerOutcome<void> AuraDummyImprovedMoonkinDefault(AuraDummyImprovedMoonkinContext& ctx)
{
    sLog.outError("HandleAuraDummy: Not handled rank of IMF (Spell: %u)", ctx.aura->GetId());
    return SpellHandlerOutcome<void>::Return();
}

template <class Site>
struct AuraDummyRow
{
    uint32 spellId;
    typename SpellHandler<Site>::Function function;
};

/// Registers every row of one site's table on `registry`; answers the number of rows.
template <class Site, std::size_t N>
static uint32 RegisterAuraDummyRows(SpellHandlerRegistry& registry, AuraDummyRow<Site> const (&rows)[N])
{
    for (AuraDummyRow<Site> const& row : rows)
    {
        registry.Register<Site>(row.spellId, row.function);
    }
    return uint32(N);
}

uint32 RegisterAuraDummyHandlers(SpellHandlerRegistry& registry)
{
    static AuraDummyRow<AuraDummyApplyWarriorSite> const warriorApply[] =
    {
        { 41099, &AuraDummyApplyWarrior41099 },
        { 41100, &AuraDummyApplyWarrior41100 },
        { 41101, &AuraDummyApplyWarrior41101 },
        { 53790, &AuraDummyApplyWarrior53790 },
        { 53791, &AuraDummyApplyWarrior53791 },
        { 53792, &AuraDummyApplyWarrior53792 },
    };

    static AuraDummyRow<AuraDummyUnrelentingAssaultSite> const unrelentingAssault[] =
    {
        { 46859, &AuraDummyUnrelentingAssault46859 },
        { 46860, &AuraDummyUnrelentingAssault46860 },
    };

    static AuraDummyRow<AuraDummyDruidSite> const druid[] =
    {
        { 52610, &AuraDummyDruid52610 },
        { 61336, &AuraDummyDruid61336 },
    };

    static AuraDummyRow<AuraDummyImprovedMoonkinSite> const improvedMoonkin[] =
    {
        { 48384, &AuraDummyImprovedMoonkin48384 },
        { 48395, &AuraDummyImprovedMoonkin48395 },
        { 48396, &AuraDummyImprovedMoonkin48396 },
    };

    uint32 rows = RegisterAuraDummyRows(registry, warriorApply);
    rows += RegisterAuraDummyRows(registry, unrelentingAssault);
    registry.RegisterDefault<AuraDummyUnrelentingAssaultSite>(&AuraDummyUnrelentingAssaultDefault);
    ++rows;
    rows += RegisterAuraDummyRows(registry, druid);
    rows += RegisterAuraDummyRows(registry, improvedMoonkin);
    registry.RegisterDefault<AuraDummyImprovedMoonkinSite>(&AuraDummyImprovedMoonkinDefault);
    ++rows;
    return rows;
}
