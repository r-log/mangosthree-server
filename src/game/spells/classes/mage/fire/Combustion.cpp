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
    /// Combustion's DoT, 83853.
    uint32 const COMBUSTION_DOT = 83853;

    /// The caster's DoTs Combustion combines besides Ignite: Pyroblast (11366) and Pyroblast! (92315), Living Bomb
    /// (44457) and Frostfire Bolt (44614), whose periodic effect Glyph of Frostfire gives. Flamestrike's DoT is not
    /// combined.
    uint32 const COMBUSTION_SHARES[] = { 11366, 92315, 44457, 44614 };

    /// Whether Combustion builds the share of `spellId` again at its cast: every one of COMBUSTION_SHARES but Frostfire
    /// Bolt, whose glyph tick is cut from a hit and stays as the aura holds it, before the caster's mastery.
    bool CombustionRebuildsShare(uint32 spellId)
    {
        return spellId != COMBUSTION_SHARES[3];
    }

    /// 11129: Combustion: its script effect puts Combustion (83853) on the target with a tick equal to the per-second
    /// damage of the caster's Pyroblast (11366, 92315), Living Bomb (44457), Frostfire Bolt (44614) and Ignite (12654)
    /// on the target at this moment. Pyroblast's and Living Bomb's parts are the DoT's tick as it would be cast now:
    /// the amount before the caster's bonus through the caster's done damage of a DoT (spell power, percent bonuses and
    /// mastery as they are now), over the period the spell gives before haste. The first three take the caster's bonus
    /// at the target's low health (Molten Fury) that the done damage of a DoT lacks. Frostfire Bolt's and Ignite's
    /// parts are the ticks the auras hold before the caster's mastery, over the period the aura holds. The engine
    /// applies the caster's Flashburn once to the new DoT, so every part holds it once: the rebuilt parts are divided
    /// by it, the parts held before mastery are not. Flamestrike is not combined, nor is a holder with no time left.
    /// With nothing to combine the DoT is still applied, with a tick of 0. The script ends the effect, so the mage
    /// family switch, which has nothing for 11129, does not run.
    SpellHandlerOutcome<void> CombustionEffect(DummyEffectContext& ctx)
    {
        if (ctx.effIndex != EFFECT_INDEX_0 || !ctx.target)
        {
            return SpellHandlerOutcome<void>::Return();
        }

        ObjectGuid const caster = ctx.caster.GetObjectGuid();

        // the caster's mastery on the new DoT: the step SpellDamageBonusDone takes for a spell without damage modifiers
        float flashburn = 1.0f;
        if (SpellModMgr* mods = ctx.caster.GetSpellMods())
        {
            mods->ApplySpellMod(COMBUSTION_DOT, SPELLMOD_DOT, flashburn, true);
        }

        std::vector<PeriodicDamageView> found;
        std::vector<Mage::DotShare> shares;
        FindPeriodicDamage(*ctx.target, caster, COMBUSTION_SHARES, sizeof(COMBUSTION_SHARES) / sizeof(COMBUSTION_SHARES[0]),
                           found);
        for (PeriodicDamageView const& view : found)
        {
            if (view.remainingMs == 0)
            {
                continue;
            }
            Mage::DotShare share = { view.amount, view.periodMs };
            share.heldMastery = flashburn;
            if (SpellEntry const* dotSpell = sSpellStore.LookupEntry(view.spellId))
            {
                share.lowHealthMultiplier = Spells::LowHealthDamageMultiplier(ctx.caster, *ctx.target, *dotSpell);
                if (CombustionRebuildsShare(view.spellId))
                {
                    if (view.amountBeforeDone > 0 && view.basePeriodMs > 0)
                    {
                        share.tick = int32(ctx.caster.SpellDamageBonusDone(ctx.target, dotSpell,
                                                                           uint32(view.amountBeforeDone), DOT,
                                                                           view.stacks ? view.stacks : 1));
                    }
                    if (view.basePeriodMs > 0)
                    {
                        share.periodMs = view.basePeriodMs;
                    }
                }
                else if (view.amountBeforeDone > 0)
                {
                    share.tick = view.amountBeforeDone;
                    share.heldMastery = 1.0f;
                }
            }
            shares.push_back(share);
        }

        uint32 const igniteDot[] = { IGNITE_DOT };
        Mage::DotShare ignite = { 0, 0 };
        bool const hasIgnite = FindPeriodicDamage(*ctx.target, caster, igniteDot, 1, found) > 0;
        if (hasIgnite)
        {
            ignite.tick = found[0].amountBeforeDone;
            ignite.periodMs = found[0].periodMs;
        }

        int32 const tick = Mage::CombustionTick(shares.empty() ? NULL : &shares[0], shares.size(), hasIgnite ? &ignite : NULL);
        int32 const basePoints[MAX_EFFECT_INDEX] = { tick, 0, 0 };
        ctx.CastTriggered(*ctx.target, COMBUSTION_DOT, basePoints);
        return SpellHandlerOutcome<void>::Return();
    }

    uint32 const COMBUSTION[] = { 11129 };
}

int32 Mage::CombustionTick(DotShare const* shares, size_t count, DotShare const* ignite)
{
    double perSecond = 0.0;
    for (size_t i = 0; i < count; ++i)
    {
        if (shares[i].periodMs)
        {
            double const multiplier = shares[i].lowHealthMultiplier > 0.0f ? double(shares[i].lowHealthMultiplier) : 1.0;
            double const held = shares[i].heldMastery > 0.0f ? double(shares[i].heldMastery) : 1.0;
            perSecond += double(shares[i].tick) * 1000.0 / double(shares[i].periodMs) * multiplier / held;
        }
    }
    if (ignite && ignite->periodMs)
    {
        perSecond += double(ignite->tick) * 1000.0 / double(ignite->periodMs);
    }
    return perSecond > 0.0 ? int32(perSecond) : 0;
}

namespace Mage
{
    uint32 RegisterCombustion(SpellHandlerRegistry& registry)
    {
        return RegisterRanks<DummyEffectHook>(registry, COMBUSTION, &CombustionEffect);
    }
}
