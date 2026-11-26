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
#include "spells/classes/mage/fire/FireShared.h"
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
    /// A Frostfire Bolt holder of a caster without Glyph of Frostfire that holds no slow, because its target is immune
    /// to the slow, has only the periodic damage the bolt does not deal: its time is set to run out, and the unit's own
    /// expiry removes it at its next update. Mage::PyromaniacTargets and Combustion pass over a holder with no time
    /// left.
    void ExpireWithoutSlow(AuraHandle& aura)
    {
        if (!aura.HasEffect(EFFECT_INDEX_0))
        {
            aura.Expire();
        }
    }

    /// 44614: Frostfire Bolt: before its holder is added, a caster with Glyph of Frostfire (61205) takes the slow
    /// (effect 0) off it, so the stacking checks never see a slow of his, and any other caster's holder is capped at
    /// one stack, so his own re-cast refreshes it. When the holder is added, the glyphed caster's loses the slow if it
    /// still has one, and any other caster's the periodic damage (effect 2), so the bolt slows or burns, never both; a
    /// holder left with the periodic damage alone by its target's immunity to the slow runs out (ExpireWithoutSlow).
    /// Then, with the holder as that leaves it, the caster's burning enemies are counted again for Pyromaniac, as
    /// PyromaniacDotApply counts them: a bolt without its periodic damage or its time makes no enemy burn
    /// (Mage::PyromaniacTargets). The holder's boosts still run.
    SpellHandlerOutcome<void> FrostfireBoltAuraApply(AuraApplyContext& ctx)
    {
        if (ctx.caster)
        {
            bool const glyphed = ctx.caster->HasAura(GLYPH_OF_FROSTFIRE);
            if (!ctx.real)
            {
                if (glyphed)
                {
                    ctx.aura.RemoveEffect(EFFECT_INDEX_0);
                }
                else
                {
                    ctx.aura.CapStacks(1);
                }
                return SpellHandlerOutcome<void>::Continue();
            }
            ctx.aura.RemoveEffect(glyphed ? EFFECT_INDEX_0 : EFFECT_INDEX_2);
            if (!glyphed)
            {
                ExpireWithoutSlow(ctx.aura);
            }
        }
        return Mage::PyromaniacDotApply(ctx);
    }

    /// 44614: Frostfire Bolt: after its hit lands, on the caster's own holder of it on the target. With Glyph of
    /// Frostfire (61205) the holder's periodic damage becomes the glyph's share of the hit: its effect 2, in percent,
    /// of the damage dealt and absorbed, taken before the caster's bonus at the target's low health, Molten Fury, and
    /// the target's damage-taken effects, which each tick takes again (Mage::ShareBase); spread over the ticks the
    /// holder's time left holds at its period, times the holder's stacks, with the caster's mastery on the spell
    /// (Mage::FrostfireGlyphTick). Without the glyph a holder that stacked is set back to one stack, so the slow keeps
    /// its own value, and one with no slow runs out (ExpireWithoutSlow).
    SpellHandlerOutcome<void> FrostfireBoltAfterHit(AfterHitContext& ctx)
    {
        if (ctx.heal)
        {
            return SpellHandlerOutcome<void>::Continue();
        }
        ObjectGuid const caster = ctx.caster.GetObjectGuid();
        SpellAuraHolder* holder = ctx.target.GetSpellAuraHolder(FROSTFIRE_BOLT_SPELL, caster);
        if (!holder)
        {
            return SpellHandlerOutcome<void>::Continue();
        }
        AuraHandle aura(*holder);
        if (!ctx.caster.HasAura(GLYPH_OF_FROSTFIRE))
        {
            if (aura.Stacks() > 1)
            {
                aura.SetStacks(1);
            }
            ExpireWithoutSlow(aura);
            return SpellHandlerOutcome<void>::Continue();
        }

        SpellEntry const* glyph = sSpellStore.LookupEntry(GLYPH_OF_FROSTFIRE);
        int32 const percent = glyph ? glyph->CalculateSimpleValue(EFFECT_INDEX_2) : 0;
        uint32 const bolt[] = { FROSTFIRE_BOLT_SPELL };
        std::vector<PeriodicDamageView> found;
        if (percent <= 0 || !FindPeriodicDamage(ctx.target, caster, bolt, 1, found))
        {
            return SpellHandlerOutcome<void>::Continue();
        }
        PeriodicDamageView const& dot = found[0];
        uint32 const ticks = Spells::TickCount(dot.remainingMs, int32(dot.periodMs), int32(dot.periodMs));

        // the caster's mastery on the DoT: the step SpellDamageBonusDone takes for a spell in the mastery's mask
        float mastery = 1.0f;
        if (SpellModMgr* mods = ctx.caster.GetSpellMods())
        {
            mods->ApplySpellMod(FROSTFIRE_BOLT_SPELL, SPELLMOD_DOT, mastery, true);
        }
        // the hit before the caster's bonus at the target's low health and the target's damage-taken effects, which each
        // tick of the DoT takes again; the flat taken bonus stays in
        float const lowHealth = Spells::LowHealthDamageMultiplier(ctx.caster, ctx.target, ctx.spell);
        float const taken = Spells::DamageTakenMultiplier(ctx.target, ctx.caster, ctx.spell);
        uint32 const hit = Mage::ShareBase(ctx.amount + ctx.absorbed, lowHealth * taken);
        aura.SetAmount(dot.effIndex, Mage::FrostfireGlyphTick(hit, uint32(percent), ticks, aura.Stacks(), mastery));
        aura.SetAmountBeforeDone(dot.effIndex, Mage::FrostfireGlyphTick(hit, uint32(percent), ticks, aura.Stacks(), 1.0f));
        return SpellHandlerOutcome<void>::Continue();
    }

    uint32 const FROSTFIRE_BOLT[] = { FROSTFIRE_BOLT_SPELL };
}

int32 Mage::FrostfireGlyphTick(uint32 hit, uint32 percent, uint32 ticks, uint32 stacks, float masteryMultiplier)
{
    return int32(float(int32(stacks) * Combat::DotShareTick(float(hit), percent, ticks)) * masteryMultiplier);
}

namespace Mage
{
    uint32 RegisterFrostfireBolt(SpellHandlerRegistry& registry)
    {
        uint32 rows = RegisterRanks<AuraApplyHook>(registry, FROSTFIRE_BOLT, &FrostfireBoltAuraApply);
        rows += RegisterRanks<AfterHitHook>(registry, FROSTFIRE_BOLT, &FrostfireBoltAfterHit);
        return rows;
    }
}
