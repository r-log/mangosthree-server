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
#include "DBCStores.h"
#include "Log.h"
#include "Utilities/Util.h"

/// 66: Invisibility
static SpellHandlerOutcome<void> AuraTriggerSpellMage66(AuraTriggerSpellContext& ctx)
{
    // Its trigger spell, 35009, is not in the client's spell data: the tick takes a tenth of the mage's threat from
    // every enemy's list instead and casts nothing
    ctx.target->GetHostileRefManager().addThreatPercent(Mage::InvisibilityThreatPercent(ctx.auraId));
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

    // Frost Nova is the mage's one spell of category 35: the glyph resets its cooldown
    ctx.unit->RemoveSpellCategoryCooldown(35, true);
    return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_OK);
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
}

namespace
{
    typedef SpellHandlerOutcome<SpellAuraProcResult> ProcOutcome;

    /// 38194: Blink: its effect keeps one of the units its target mode found, as the cut after the hook keeps them.
    SpellHandlerOutcome<void> BlinkSelectTargets(SelectTargetsContext& ctx)
    {
        ctx.maxTargets = 1;
        return SpellHandlerOutcome<void>::Continue();
    }

    /// 26467: Persistent Shield, the Scarab Brooch's aura: its proc on a heal casts 26470 instead of its own trigger
    /// spell, at the unit healed, with 15% of the heal as the shield's amount.
    ProcOutcome PersistentShieldProc(OnProcContext& ctx)
    {
        if (!Mage::ProcEffectIs(ctx, SPELL_AURA_PROC_TRIGGER_SPELL))
        {
            return ProcOutcome::Continue();
        }
        int32 const basePoints[MAX_EFFECT_INDEX] = { int32(ctx.damage * 15 / 100), 0, 0 };
        return ProcOutcome::Return(Mage::CastProcTriggerSpell(ctx, 26470, ctx.victim, basePoints));
    }

    /// 79684: Offensive State: a mage whose talents do not stop Arcane Missiles! (Mage::SuppressesArcaneMissiles:
    /// Hot Streak or Brain Freeze) rolls the aura's percent after each spell hit the proc accepts, and on success casts
    /// Arcane Missiles! (79683) on himself; a mage with one of them never does. Arcane Missiles and its missiles never
    /// roll (Mage::OffensiveStateRefuses).
    ProcOutcome OffensiveStateProc(OnProcContext& ctx)
    {
        if (!Mage::ProcEffectIs(ctx, SPELL_AURA_PROC_TRIGGER_SPELL))
        {
            return ProcOutcome::Continue();
        }
        if (ctx.procSpell && Mage::OffensiveStateRefuses(ctx.procSpell->ID))
        {
            return ProcOutcome::Return(SPELL_AURA_PROC_CANT_TRIGGER);
        }
        if (Mage::SuppressesArcaneMissiles(ctx.caster))
        {
            return ProcOutcome::Return(SPELL_AURA_PROC_CANT_TRIGGER);
        }
        if (!roll_chance_i(ctx.triggerAmount))
        {
            return ProcOutcome::Return(SPELL_AURA_PROC_FAILED);
        }
        return ProcOutcome::Return(Mage::CastProcTriggerSpell(ctx, Mage::EffectTriggerSpell(ctx), &ctx.caster));
    }

    uint32 const BLINK[] = { 38194 };
    uint32 const PERSISTENT_SHIELD[] = { 26467 };
    uint32 const OFFENSIVE_STATE[] = { 79684 };
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
    rows += RegisterRanks<SelectTargetsHook>(registry, BLINK, &BlinkSelectTargets);
    rows += RegisterRanks<OnProcHook>(registry, PERSISTENT_SHIELD, &PersistentShieldProc);
    rows += RegisterRanks<OnProcHook>(registry, OFFENSIVE_STATE, &OffensiveStateProc);
    registry.RegisterDefault<AuraTriggerSpellMageSite>(&AuraTriggerSpellMageDefault);
    registry.RegisterDefault<AuraPeriodicDummyTickMageSite>(&AuraPeriodicDummyTickMageDefault);
    registry.RegisterDefault<AuraBoostMageSite>(&AuraBoostMageDefault);
    return rows + 3;
}

bool Mage::ProcEffectIs(OnProcContext const& ctx, uint32 auraType)
{
    SpellEffectEntry const* effect = ctx.spell.GetSpellEffect(ctx.effIndex);
    return effect && effect->EffectAura == auraType;
}

int32 Mage::InvisibilityThreatPercent(uint32 spellId)
{
    constexpr uint32 INVISIBILITY_FADE = 66;
    constexpr int32 FADE_TICK_PERCENT = -10;
    return spellId == INVISIBILITY_FADE ? FADE_TICK_PERCENT : 0;
}

bool Mage::OffensiveStateRefuses(uint32 procSpellId)
{
    constexpr uint32 ARCANE_MISSILES = 5143;
    constexpr uint32 ARCANE_MISSILES_MISSILE = 7268;
    return procSpellId == ARCANE_MISSILES || procSpellId == ARCANE_MISSILES_MISSILE;
}

bool Mage::SuppressesArcaneMissiles(Unit const& mage)
{
    constexpr uint32 HOT_STREAK_TALENT = 44445;
    constexpr uint32 BRAIN_FREEZE_RANKS[] = { 44546, 44548, 44549 };
    if (mage.HasAura(HOT_STREAK_TALENT))
    {
        return true;
    }
    for (uint32 rank : BRAIN_FREEZE_RANKS)
    {
        if (mage.HasAura(rank))
        {
            return true;
        }
    }
    return false;
}

SpellAuraProcResult Mage::CastProcSpell(OnProcContext& ctx, Unit& to, uint32 spellId, int32 const* basePoints)
{
    if (!sSpellStore.LookupEntry(spellId))
    {
        sLog.outError("Mage proc script of spell %u: the spell it casts, %u, is not in the spell store", ctx.spell.ID, spellId);
        return SPELL_AURA_PROC_FAILED;
    }
    return ctx.CastTriggered(to, spellId, basePoints) ? SPELL_AURA_PROC_OK : SPELL_AURA_PROC_FAILED;
}

uint32 Mage::EffectTriggerSpell(OnProcContext const& ctx)
{
    SpellEffectEntry const* effect = ctx.spell.GetSpellEffect(ctx.effIndex);
    return effect ? effect->EffectTriggerSpell : 0;
}

SpellAuraProcResult Mage::CastProcTriggerSpell(OnProcContext& ctx, uint32 spellId, Unit* to, int32 const* basePoints)
{
    SpellEntry const* entry = sSpellStore.LookupEntry(spellId);
    if (!entry || (ctx.caster.m_extraAttacks && IsSpellHaveEffect(entry, SPELL_EFFECT_ADD_EXTRA_ATTACKS)))
    {
        return SPELL_AURA_PROC_FAILED;
    }
    Unit* target = to ? to : !(ctx.procFlag & PROC_FLAG_SUCCESSFUL_POSITIVE_SPELL) && IsPositiveSpell(spellId) ? &ctx.caster : ctx.victim;
    if (!target)
    {
        return SPELL_AURA_PROC_FAILED;
    }
    return ctx.CastTriggered(*target, spellId, basePoints) ? SPELL_AURA_PROC_OK : SPELL_AURA_PROC_FAILED;
}

uint32 RegisterMageScripts(SpellHandlerRegistry& registry)
{
    uint32 registered = Mage::RegisterClassWide(registry);
    registered += Mage::RegisterFire(registry);
    registered += Mage::RegisterFrost(registry);
    registered += Mage::RegisterArcane(registry);
    registered += Mage::RegisterOrb(registry);
    return registered;
}
