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
#include "entities/Unit.h"

/// 74396: absent from the client's spell data
static SpellHandlerOutcome<void> AuraBoostMage74396(AuraBoostContext& ctx)
{
    if (!ctx.apply)
    {
        ctx.spellId1 = 44544;
    }
    else
    {
        return SpellHandlerOutcome<void>::Return();
    }
    return SpellHandlerOutcome<void>::Continue();
}

/// 31687: Summon Water Elemental, which has no dummy effect in the client's spell data: a caster with Glyph of Mana
/// Shield (70937) casts 70908 on itself, any other caster 70907, both absent from the client's spell data; the effect
/// ends
static SpellHandlerOutcome<void> EffectDummyMageSummonWaterElemental(SpellEffectDummyMageContext& ctx)
{
    if (ctx.m_caster->HasAura(70937))
    {
        ctx.m_caster->CastSpell(ctx.m_caster, 70908, true);
    }
    else
    {
        ctx.m_caster->CastSpell(ctx.m_caster, 70907, true);
    }

    return SpellHandlerOutcome<void>::Return();
}

/// 74396: absent from the client's spell data: removes one stack of 74396
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcMage74396(DummyAuraProcContext& ctx)
{
    // Remove only single aura from stack and remove holder if its last stack
    ctx.unit->RemoveAuraHolderFromStack(74396);
    return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_OK);
}

uint32 Mage::RegisterFrost(SpellHandlerRegistry& registry)
{
    static Row<AuraBoostMageSite> const boostMage[] =
    {
        { 74396, &AuraBoostMage74396 },
    };

    static Row<SpellEffectDummyMageSite> const effectDummyMage[] =
    {
        { 31687, &EffectDummyMageSummonWaterElemental },
    };

    static Row<DummyAuraProcMageSite> const dummyAuraProcMage[] =
    {
        { 74396, &DummyAuraProcMage74396 },
    };

    uint32 rows = RegisterRows(registry, boostMage);
    rows += RegisterRows(registry, effectDummyMage);
    rows += RegisterRows(registry, dummyAuraProcMage);
    return rows;
}
