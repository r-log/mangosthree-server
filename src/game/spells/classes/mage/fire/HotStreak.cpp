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

    uint32 const HOT_STREAK_BUFF = 48108;
    uint32 const PYROBLAST_INSTANT = 92315;
    uint32 const IMPROVED_HOT_STREAK_RANK_1 = 44446;
    uint32 const IMPROVED_HOT_STREAK_RANK_2 = 44448;

    /// 44445: Hot Streak: each critical strike of Fireball, Frostfire Bolt, Scorch, Pyroblast or Fire Blast (the proc
    /// row's spells; the script leaves Pyroblast! 92315 out) rolls HotStreakChance of the caster's sheet crit for the
    /// school of the spell; when that fails and the caster knows Improved Hot Streak (44448: 100, 44446: 50), two such
    /// crits in a row roll its percent. A non-crit of those spells starts the count again. Success casts Hot Streak
    /// (48108) on the caster. The count lives in this holder's script word of effect 0.
    ProcOutcome HotStreakProc(OnProcContext& ctx)
    {
        if (!Mage::ProcEffectIs(ctx, SPELL_AURA_DUMMY))
        {
            return ProcOutcome::Continue();
        }
        if (ctx.effIndex != EFFECT_INDEX_0)
        {
            return ProcOutcome::Return(SPELL_AURA_PROC_OK);
        }
        if (!ctx.procSpell)
        {
            return ProcOutcome::Return(SPELL_AURA_PROC_FAILED);
        }
        if (ctx.procSpell->ID == PYROBLAST_INSTANT)
        {
            return ProcOutcome::Return(SPELL_AURA_PROC_OK);
        }

        bool const crit = (ctx.procEx & PROC_EX_CRITICAL_HIT) != 0;
        float const sheetCrit = ctx.caster.GetTypeId() == TYPEID_PLAYER
            ? ctx.caster.GetFloatValue(PLAYER_SPELL_CRIT_PERCENTAGE1 + GetFirstSchoolInMask(GetSpellSchoolMask(ctx.procSpell)))
            : 0.0f;
        int32 const improved = ctx.caster.HasAura(IMPROVED_HOT_STREAK_RANK_2) ? 100
                             : ctx.caster.HasAura(IMPROVED_HOT_STREAK_RANK_1) ? 50 : 0;
        bool const baseRoll = crit && roll_chance_f(Mage::HotStreakChance(sheetCrit));
        bool const improvedRoll = crit && !baseRoll && improved > 0 && roll_chance_i(improved);
        if (!Mage::HotStreakStep(ctx.aura.ScriptWord(EFFECT_INDEX_0), crit, baseRoll, improvedRoll))
        {
            return ProcOutcome::Return(SPELL_AURA_PROC_OK);
        }
        return ProcOutcome::Return(Mage::CastProcSpell(ctx, ctx.caster, HOT_STREAK_BUFF));
    }

    uint32 const ITEM_MAGE_T10_2P_BONUS = 70752;
    uint32 const PUSHING_THE_LIMIT = 70753;

    /// 48108: Hot Streak (the buff): when it is used up, removed in the default mode as casting Pyroblast! removes it,
    /// and its caster holds Item - Mage T10 2P Bonus (70752), the mage casts Pushing the Limit (70753) on himself for
    /// that caster. Its running out, a dispel or a cancel gives nothing. The script ends the removal's boosts.
    SpellHandlerOutcome<void> HotStreakBuffRemove(AuraRemoveContext& ctx)
    {
        if (Mage::PushingTheLimitOnRemove(ctx.mode, ctx.caster && ctx.caster->HasAura(ITEM_MAGE_T10_2P_BONUS)))
        {
            ctx.CastTriggered(ctx.target, PUSHING_THE_LIMIT);
        }
        return SpellHandlerOutcome<void>::Return();
    }

    uint32 const HOT_STREAK[] = { 44445 };
    uint32 const HOT_STREAK_BUFFS[] = { HOT_STREAK_BUFF };
}

bool Mage::CountCritInARow(int32& crits, bool crit)
{
    if (!crit)
    {
        crits = 0;
        return false;
    }
    if (++crits < 2)
    {
        return false;
    }
    crits = 0;
    return true;
}

float Mage::HotStreakChance(float sheetCritPercent)
{
    float const crit = sheetCritPercent > 0.0f ? sheetCritPercent : 0.0f;
    float const chance = 95.0f - 2.73f * crit;
    return chance > 0.0f ? chance : 0.0f;
}

bool Mage::PushingTheLimitOnRemove(AuraRemoveMode mode, bool casterHasT10TwoPiece)
{
    return mode == AURA_REMOVE_BY_DEFAULT && casterHasT10TwoPiece;
}

bool Mage::HotStreakStep(int32& crits, bool crit, bool baseRollPassed, bool improvedRollPassed)
{
    if (crit && baseRollPassed)
    {
        crits = 0;
        return true;
    }
    return CountCritInARow(crits, crit) && improvedRollPassed;
}

namespace Mage
{
    uint32 RegisterHotStreak(SpellHandlerRegistry& registry)
    {
        uint32 rows = RegisterRanks<OnProcHook>(registry, HOT_STREAK, &HotStreakProc);
        rows += RegisterRanks<AuraRemoveHook>(registry, HOT_STREAK_BUFFS, &HotStreakBuffRemove);
        return rows;
    }
}
