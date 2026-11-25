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
#include "entities/Pet.h"
#include "DBCStores.h"
#include "Log.h"
#include "Utilities/Util.h"

#include <vector>

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

    uint32 const BLINK[] = { 38194 };
    uint32 const PERSISTENT_SHIELD[] = { 26467 };
}

uint32 Mage::RegisterClassWide(SpellHandlerRegistry& registry)
{
    static Row<SpellEffectDummyMageSite> const effectDummyMage[] =
    {
        { 32826, &EffectDummyMagePolymorphCastVisual },
        { 38194, &EffectDummyMageBlink },
    };

    static Row<DummyAuraProcMageSite> const dummyAuraProcMage[] =
    {
        { 64411, &DummyAuraProcMage64411 },
    };

    uint32 rows = RegisterRows(registry, effectDummyMage);
    rows += RegisterRows(registry, dummyAuraProcMage);
    rows += RegisterRanks<SelectTargetsHook>(registry, BLINK, &BlinkSelectTargets);
    rows += RegisterRanks<OnProcHook>(registry, PERSISTENT_SHIELD, &PersistentShieldProc);
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

namespace Mage
{
    // Each mechanic file's registration, Mage::Register<Mechanic> (MageScriptFiles.inc, src/game/CMakeLists.txt).
#define MAGE_SCRIPT_FILE(mechanic) uint32 Register##mechanic(SpellHandlerRegistry& registry);
#include "MageScriptFiles.inc"
#undef MAGE_SCRIPT_FILE
}

std::vector<Mage::ScriptFile> const& Mage::ScriptFiles()
{
    static std::vector<ScriptFile> const files =
    {
#define MAGE_SCRIPT_FILE(mechanic) { #mechanic, &Mage::Register##mechanic },
#include "MageScriptFiles.inc"
#undef MAGE_SCRIPT_FILE
    };
    return files;
}

uint32 Mage::RegisterFiles(SpellHandlerRegistry& registry, std::vector<ScriptFile> const& files)
{
    uint32 registered = 0;
    for (ScriptFile const& file : files)
    {
        registered += file.registerRows(registry);
    }
    return registered;
}

uint32 RegisterMageScripts(SpellHandlerRegistry& registry)
{
    uint32 registered = Mage::RegisterClassWide(registry);
    registered += Mage::RegisterFiles(registry, Mage::ScriptFiles());
    return registered;
}
