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
#include "Utilities/Util.h"

/// 66: Invisibility
static SpellHandlerOutcome<void> AuraTriggerSpellMage66(AuraTriggerSpellContext& /*ctx*/)
{
    // Here need periodic trigger reducing threat spell (or do it manually)
    return SpellHandlerOutcome<void>::Return();
}

/// every other id: nothing
static SpellHandlerOutcome<void> AuraTriggerSpellMageDefault(AuraTriggerSpellContext& /*ctx*/)
{
    return SpellHandlerOutcome<void>::Continue();
}

/// every other id: nothing
static SpellHandlerOutcome<void> AuraPeriodicDummyTickMageDefault(AuraPeriodicDummyTickContext& /*ctx*/)
{
    return SpellHandlerOutcome<void>::Continue();
}

/// every other id: nothing
static SpellHandlerOutcome<void> AuraBoostMageDefault(AuraBoostContext& /*ctx*/)
{
    return SpellHandlerOutcome<void>::Return();
}

/// 32826: Polymorph Cast Visual: a creature target casts one of six forms on itself at random (Squirrel, Giraffe,
/// Serpent, Dragonhawk, Worgen or Sheep Form: 32813, 32816 to 32820); the effect ends
static SpellHandlerOutcome<void> EffectDummyMagePolymorphCastVisual(SpellEffectDummyMageContext& ctx)
{
    if (ctx.unitTarget && ctx.unitTarget->GetTypeId() == TYPEID_UNIT)
    {
        // Polymorph Cast Visual Rank 1
        const uint32 spell_list[6] =
        {
            32813,                          // Squirrel Form
            32816,                          // Giraffe Form
            32817,                          // Serpent Form
            32818,                          // Dragonhawk Form
            32819,                          // Worgen Form
            32820                           // Sheep Form
        };
        ctx.unitTarget->CastSpell(ctx.unitTarget, spell_list[urand(0, 5)], true);
    }
    return SpellHandlerOutcome<void>::Return();
}

/// 38194: Blink: the caster casts Blink (38203) on the unit target, if any; the effect ends
static SpellHandlerOutcome<void> EffectDummyMageBlink(SpellEffectDummyMageContext& ctx)
{
    // Blink
    if (ctx.unitTarget)
    {
        ctx.m_caster->CastSpell(ctx.unitTarget, 38203, true);
    }

    return SpellHandlerOutcome<void>::Return();
}

/// 42955: Conjure Refreshment: the damage is set to 20 and the spell creates that many of the conjured food the
/// target's level calls for; the effect ends
static SpellHandlerOutcome<void> EffectDummyMageConjureRefreshment(SpellEffectDummyMageContext& ctx)
{
    uint32 item = 0;

    uint32 level = ctx.unitTarget->getLevel();

    if (level < 44)
    {
        item = 65500;                                 // Conjured Mana Cookie (lvl 34)
    }
    else if (level < 54)
    {
        item = 65515;                            // Conjured Mana Brownie (lvl 44)
    }
    else if (level < 64)
    {
        item = 65516;                            // Conjured Mana Cupcake (lvl 54)
    }
    else if (level < 65)
    {
        item = 65517;                            // Conjured Mana Lollipop (lvl 64)
    }
    else if (level < 74)
    {
        item = 34062;                            // Conjured Mana Biscuit (lvl 65)
    }
    else if (level < 80)
    {
        item = 43518;                            // Conjured Mana Pie (lvl 74)
    }
    else if (level < 85)
    {
        item = 43523;                            // Conjured Mana Strudel (lvl 80)
    }
    else
    {
        item = 65499;                                            // Conjured Mana Cake (lvl 85)
    }

    ctx.damage = 20; // Used to set stack size.
    ctx.spell->DoCreateItem(ctx.effect,item);

    return SpellHandlerOutcome<void>::Return();
}

/// 56372: Glyph of Ice Block: removes a player's cooldowns of category 35
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcMage56372(DummyAuraProcContext& ctx)
{
    if (ctx.unit->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }

    // not 100% safe with client version switches but for 3.1.3 no spells with cooldown that can have mage player except Frost Nova.
    ctx.unit->RemoveSpellCategoryCooldown(35, true);
    return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_OK);
// Glyph of Polymorph
}

/// 56375: Glyph of Polymorph: removes the periodic damage auras from a living victim
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcMage56375(DummyAuraProcContext& ctx)
{
    if (!ctx.pVictim || !ctx.pVictim->IsAlive())
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }

    ctx.pVictim->RemoveSpellsCausingAura(SPELL_AURA_PERIODIC_DAMAGE);
    ctx.pVictim->RemoveSpellsCausingAura(SPELL_AURA_PERIODIC_DAMAGE_PERCENT);
    return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_OK);
// Blessing of Ancient Kings
}

/// 64411: Blessing of Ancient Kings: after a positive spell, triggers 64413 for 15 percent of the damage
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcMage64411(DummyAuraProcContext& ctx)
{
    // for DOT procs
    if (!IsPositiveSpell(ctx.procSpell->ID))
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }

    ctx.triggered_spell_id = 64413;
    ctx.basepoints[0] = ctx.damage * 15 / 100;
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// Fingers of Frost
}

uint32 Mage::RegisterClassWide(SpellHandlerRegistry& registry)
{
    static Row<AuraTriggerSpellMageSite> const triggerSpellMage[] =
    {
        { 66, &AuraTriggerSpellMage66 },
    };

    static Row<SpellEffectDummyMageSite> const effectDummyMage[] =
    {
        { 32826, &EffectDummyMagePolymorphCastVisual },
        { 38194, &EffectDummyMageBlink },
        { 42955, &EffectDummyMageConjureRefreshment },
        { 1459, &EffectDummyRaidBuff<SpellEffectDummyMageContext> },
        { 61316, &EffectDummyRaidBuff<SpellEffectDummyMageContext> },
    };

    static Row<DummyAuraProcMageSite> const dummyAuraProcMage[] =
    {
        { 56372, &DummyAuraProcMage56372 },
        { 56375, &DummyAuraProcMage56375 },
        { 64411, &DummyAuraProcMage64411 },
    };

    uint32 rows = RegisterRows(registry, triggerSpellMage);
    rows += RegisterRows(registry, effectDummyMage);
    rows += RegisterRows(registry, dummyAuraProcMage);
    registry.RegisterDefault<AuraTriggerSpellMageSite>(&AuraTriggerSpellMageDefault);
    registry.RegisterDefault<AuraPeriodicDummyTickMageSite>(&AuraPeriodicDummyTickMageDefault);
    registry.RegisterDefault<AuraBoostMageSite>(&AuraBoostMageDefault);
    return rows + 3;
}

uint32 RegisterMageScripts(SpellHandlerRegistry& registry)
{
    uint32 registered = Mage::RegisterClassWide(registry);
    registered += Mage::RegisterFire(registry);
    registered += Mage::RegisterFrost(registry);
    registered += Mage::RegisterArcane(registry);
    return registered;
}
