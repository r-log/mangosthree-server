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
    uint32 const PYROMANIAC_RANK_1 = 34293;
    uint32 const PYROMANIAC_RANK_2 = 34295;
    uint32 const PYROMANIAC_HASTE = 83582;

    /// The DoTs Pyromaniac counts: Flamestrike's ground effect (2120), Pyroblast (11366), Ignite (12654), Living Bomb
    /// (44457), Frostfire Bolt (44614), Combustion (83853), the free Flamestrike's ground effect (88148) and Pyroblast!
    /// (92315).
    uint32 const PYROMANIAC_DOTS[] = { 2120, 11366, 12654, 44457, 44614, 83853, 88148, 92315 };

    /// When `caster`, the caster of a holder of a DoT Pyromaniac counts that was just added to `target` or taken off
    /// it, is a player in the world on `target`'s map who knows Pyromaniac (34295, else 34293), his burning enemies
    /// (Mage::PyromaniacTargets) are counted again: Pyromaniac's haste (83582) is cast on him with the rank's value
    /// when three or more burn and he lacks it, and taken off him when fewer burn and he has it. A caster anywhere else
    /// is left as he is.
    void PyromaniacRecount(Unit* caster, Unit& target)
    {
        if (!caster || caster->GetTypeId() != TYPEID_PLAYER || !caster->IsInWorld() || caster->FindMap() != target.FindMap())
        {
            return;
        }
        uint32 const talent = caster->HasAura(PYROMANIAC_RANK_2) ? PYROMANIAC_RANK_2
                            : caster->HasAura(PYROMANIAC_RANK_1) ? PYROMANIAC_RANK_1 : 0;
        if (!talent)
        {
            return;
        }

        bool const active = Mage::PyromaniacActive(Mage::PyromaniacTargets(*caster));
        bool const hasHaste = caster->HasAura(PYROMANIAC_HASTE);
        if (active && !hasHaste)
        {
            int32 const basePoints[MAX_EFFECT_INDEX] = { Mage::PyromaniacHaste(talent), 0, 0 };
            caster->CastCustomSpell(caster, PYROMANIAC_HASTE, &basePoints[EFFECT_INDEX_0], NULL, NULL, true);
        }
        else if (!active && hasHaste)
        {
            caster->RemoveAurasDueToSpell(PYROMANIAC_HASTE);
        }
    }

    /// The eight DoTs Pyromaniac counts, when their holder is removed: the caster's burning enemies are counted again
    /// with the holder gone (PyromaniacRecount). A removal by re-application (AURA_REMOVE_BY_STACK) changes nothing:
    /// the apply that follows counts. The holder's boosts still run.
    SpellHandlerOutcome<void> PyromaniacDotRemove(AuraRemoveContext& ctx)
    {
        if (ctx.mode != AURA_REMOVE_BY_STACK)
        {
            PyromaniacRecount(ctx.caster, ctx.target);
        }
        return SpellHandlerOutcome<void>::Continue();
    }

    /// 34293, 34295: Pyromaniac: when the talent's holder is added (a login, a respec, a new rank), Pyromaniac's haste
    /// (83582) is taken off the mage; the next DoT it counts puts the haste back when it is due. The call before the
    /// holder is added takes nothing off. The holder's boosts still run.
    SpellHandlerOutcome<void> PyromaniacTalentApply(AuraApplyContext& ctx)
    {
        if (ctx.real)
        {
            ctx.RemoveAura(ctx.target, PYROMANIAC_HASTE);
        }
        return SpellHandlerOutcome<void>::Continue();
    }

    /// 34293, 34295: Pyromaniac: when the talent's holder is removed, Pyromaniac's haste (83582) is taken off the mage.
    /// The holder's boosts still run.
    SpellHandlerOutcome<void> PyromaniacTalentRemove(AuraRemoveContext& ctx)
    {
        ctx.RemoveAura(ctx.target, PYROMANIAC_HASTE);
        return SpellHandlerOutcome<void>::Continue();
    }

    uint32 const PYROMANIAC_DOTS_APPLIED[] = { 2120, 11366, 12654, 44457, 83853, 88148, 92315 };
    uint32 const PYROMANIAC[] = { PYROMANIAC_RANK_1, PYROMANIAC_RANK_2 };
}

SpellHandlerOutcome<void> Mage::PyromaniacDotApply(AuraApplyContext& ctx)
{
    if (ctx.real)
    {
        PyromaniacRecount(ctx.caster, ctx.target);
    }
    return SpellHandlerOutcome<void>::Continue();
}

bool Mage::PyromaniacCounts(uint32 spellId)
{
    return std::find(std::begin(PYROMANIAC_DOTS), std::end(PYROMANIAC_DOTS), spellId) != std::end(PYROMANIAC_DOTS);
}

bool Mage::PyromaniacBurns(uint32 spellId, bool holdsPeriodicDamage)
{
    if (spellId == FROSTFIRE_BOLT_SPELL)
    {
        return holdsPeriodicDamage;
    }
    return PyromaniacCounts(spellId);
}

bool Mage::PyromaniacActive(uint32 targets)
{
    return targets >= 3;
}

uint32 Mage::PyromaniacTargets(Unit& mage)
{
    ObjectGuid const caster = mage.GetObjectGuid();
    std::vector<PeriodicDamageView> found;
    uint32 burning = 0;
    for (HostileReference* ref = mage.GetHostileRefManager().getFirst(); ref; ref = ref->next())
    {
        Unit* enemy = ref->getSource()->getOwner();
        if (!enemy || !enemy->IsAlive() || !enemy->IsInWorld() || enemy->FindMap() != mage.FindMap())
        {
            continue;
        }
        for (uint32 spellId : PYROMANIAC_DOTS)
        {
            if (!enemy->GetSpellAuraHolder(spellId, caster))
            {
                continue;
            }
            bool holdsPeriodicDamage = false;
            if (spellId == FROSTFIRE_BOLT_SPELL)
            {
                uint32 const held[] = { spellId };
                holdsPeriodicDamage = FindPeriodicDamage(*enemy, caster, held, 1, found) > 0 &&
                                      found[0].remainingMs != 0;
            }
            if (PyromaniacBurns(spellId, holdsPeriodicDamage))
            {
                ++burning;
                break;
            }
        }
    }
    return burning;
}

int32 Mage::PyromaniacHaste(uint32 talentSpell)
{
    if (std::find(std::begin(PYROMANIAC), std::end(PYROMANIAC), talentSpell) == std::end(PYROMANIAC))
    {
        return 0;
    }
    SpellEntry const* talent = sSpellStore.LookupEntry(talentSpell);
    return talent ? talent->CalculateSimpleValue(EFFECT_INDEX_0) : 0;
}

namespace Mage
{
    uint32 RegisterPyromaniac(SpellHandlerRegistry& registry)
    {
        uint32 rows = RegisterRanks<AuraApplyHook>(registry, PYROMANIAC_DOTS_APPLIED, &PyromaniacDotApply);
        rows += RegisterRanks<AuraRemoveHook>(registry, PYROMANIAC_DOTS, &PyromaniacDotRemove);
        rows += RegisterRanks<AuraApplyHook>(registry, PYROMANIAC, &PyromaniacTalentApply);
        rows += RegisterRanks<AuraRemoveHook>(registry, PYROMANIAC, &PyromaniacTalentRemove);
        return rows;
    }
}
