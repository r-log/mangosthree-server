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
#include "spells/handlers/UnitAuraProcHandlerHandlers.h"
#include "spells/data/SpellMgr.h"
#include "spells/data/SpellTables.h"
#include "spells/SpellModMgr.h"
#include "spells/SpellScriptQueries.h"
#include "spells/PeriodicTiming.h"
#include "combat/SpellBonus.h"
#include "combat/ThreatManager.h"
#include "entities/Unit.h"
#include "DBCStores.h"
#include "GameTime.h"
#include "Log.h"
#include "Utilities/Util.h"

#include <algorithm>
#include <iterator>
#include <vector>

namespace
{
    typedef SpellHandlerOutcome<SpellAuraProcResult> ProcOutcome;

    /// Fire Blast, whose cooldown Impact clears.
    uint32 const FIRE_BLAST = 2136;

    /// 11103, 12357: Impact: when its proc-trigger effect procs, Fire Blast's (2136) cooldown is cleared and the effect's
    /// trigger spell, Impact (64343), is cast on the caster.
    ProcOutcome ImpactProc(OnProcContext& ctx)
    {
        if (!Mage::ProcEffectIs(ctx, SPELL_AURA_PROC_TRIGGER_SPELL))
        {
            return ProcOutcome::Continue();
        }
        ctx.caster.RemoveSpellCooldown(FIRE_BLAST, true);
        return ProcOutcome::Return(Mage::CastProcTriggerSpell(ctx, Mage::EffectTriggerSpell(ctx), &ctx.caster));
    }

    /// 11103, 12357: Impact: the proc rolls on the caster's damaging spells only: a hit of a spell with no damage effect
    /// of its own (Slow, Polymorph, Counterspell, Deep Freeze) is refused; Living Bomb's cast, whose rows hold its
    /// damage over time, is accepted.
    SpellHandlerOutcome<bool> ImpactCheckProc(CheckProcContext& ctx)
    {
        if (ctx.procSpell && !Spells::DealsDamage(*ctx.procSpell))
        {
            return SpellHandlerOutcome<bool>::Return(false);
        }
        return SpellHandlerOutcome<bool>::Continue();
    }

    /// The caster's DoTs Impact copies, in the order it copies them: Pyroblast (11366), Pyroblast! (92315), Living Bomb
    /// (44457), Ignite (12654) and Combustion (83853).
    uint32 const IMPACT_SPREAD_DOTS[] = { 11366, 92315, 44457, 12654, 83853 };

    /// Living Bomb, which Impact copies to IMPACT_BOMB_TARGETS enemies at most.
    uint32 const IMPACT_LIVING_BOMB = 44457;
    int32 const IMPACT_BOMB_TARGETS = 2;

    /// 12355: Impact: for each enemy its script effect reaches other than the stunned target, copies the caster's
    /// Pyroblast, Living Bomb, Ignite and Combustion from the stunned target to it, Living Bomb to two enemies at most;
    /// an enemy under a confuse or fear is left alone, and so is one already holding the caster's aura of that spell.
    /// The stun is the spell's own effect 0. The script ends the effect.
    SpellHandlerOutcome<void> ImpactSpreadEffect(DummyEffectContext& ctx)
    {
        Unit* const stunned = ctx.castTarget;
        if (ctx.effIndex != EFFECT_INDEX_1 || !ctx.target || !stunned || ctx.target == stunned ||
            ctx.target->HasAuraType(SPELL_AURA_MOD_CONFUSE) || ctx.target->HasAuraType(SPELL_AURA_MOD_FEAR))
        {
            return SpellHandlerOutcome<void>::Return();
        }

        ObjectGuid const caster = ctx.caster.GetObjectGuid();
        for (uint32 spellId : IMPACT_SPREAD_DOTS)
        {
            // the cast's word counts the Living Bomb copies tried over all of its targets
            if (Mage::ImpactSpreads(spellId, ctx.word))
            {
                CopyAuraHolder(*stunned, *ctx.target, spellId, caster);
            }
        }
        return SpellHandlerOutcome<void>::Return();
    }

    uint32 const IMPACT[] = { 11103, 12357 };
    uint32 const IMPACT_SPREAD[] = { 12355 };
}

bool Mage::ImpactSpreads(uint32 spellId, int32& bombsSpread)
{
    uint32 const* const dotsEnd = std::end(IMPACT_SPREAD_DOTS);
    if (std::find(std::begin(IMPACT_SPREAD_DOTS), dotsEnd, spellId) == dotsEnd)
    {
        return false;
    }
    if (spellId != IMPACT_LIVING_BOMB)
    {
        return true;
    }
    if (bombsSpread >= IMPACT_BOMB_TARGETS)
    {
        return false;
    }
    ++bombsSpread;
    return true;
}

namespace Mage
{
    uint32 RegisterImpact(SpellHandlerRegistry& registry)
    {
        uint32 rows = RegisterRanks<OnProcHook>(registry, IMPACT, &ImpactProc);
        rows += RegisterRanks<CheckProcHook>(registry, IMPACT, &ImpactCheckProc);
        rows += RegisterRanks<DummyEffectHook>(registry, IMPACT_SPREAD, &ImpactSpreadEffect);
        return rows;
    }
}
