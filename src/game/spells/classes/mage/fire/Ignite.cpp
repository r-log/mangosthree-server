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
    typedef SpellHandlerOutcome<SpellAuraProcResult> ProcOutcome;

    /// The Fire hits that do not ignite: Molten Armor's hit, Living Bomb's explosion, Flame Orb's hits, Fire Power's
    /// explosion and the Frostfire Orb's hits of both ranks.
    uint32 const NOT_IGNITING[] = { 34913, 44461, 82739, 83619, 84721, 95969 };

    /// The ticks Ignite (12654) deals: its duration over its periodic effect's period; 0 when the spell store has no
    /// 12654 or its effect no period.
    uint32 IgniteTicks()
    {
        SpellEntry const* ignite = sSpellStore.LookupEntry(IGNITE_DOT);
        SpellEffectEntry const* effect = ignite ? ignite->GetSpellEffect(EFFECT_INDEX_0) : NULL;
        int32 const duration = GetSpellDuration(ignite);
        if (!effect || !effect->EffectAuraPeriod || duration <= 0)
        {
            return 0;
        }
        return uint32(duration) / effect->EffectAuraPeriod;
    }

    /// The script words of an Ignite (12654) holder that hold what IgniteProc banked in it last: the server update the
    /// share was banked in (GameTime::GetGameTimeMS), the damage before its caster's bonus that the Ignite it replaced
    /// had left, and the ticks the holder had right after the bank. A holder no crit banked in holds 0 in all three.
    SpellEffectIndex const IGNITE_BANK_UPDATE = EFFECT_INDEX_0;
    SpellEffectIndex const IGNITE_BANK_LEFTOVER = EFFECT_INDEX_1;
    SpellEffectIndex const IGNITE_BANK_TICKS = EFFECT_INDEX_2;

    /// The caster's Ignite (12654) on `victim`, its periodic damage read into `view`; NULL when the victim holds none
    /// of his.
    SpellAuraHolder* CastersIgnite(Unit& victim, ObjectGuid const& caster, PeriodicDamageView& view)
    {
        uint32 const dot[] = { IGNITE_DOT };
        std::vector<PeriodicDamageView> found;
        if (!FindPeriodicDamage(victim, caster, dot, 1, found))
        {
            return NULL;
        }
        view = found[0];
        return victim.GetSpellAuraHolder(IGNITE_DOT, caster);
    }

    /// 11119, 11120, 12846: Ignite: a critical strike of one of the mage's direct Fire spells puts the aura's percent of
    /// its damage on the victim as Ignite (12654), two ticks over its duration; the caster's Ignite it replaces rolls
    /// what it had left into it. The hit of Molten Armor (34913), the explosion of Living Bomb (44461), Flame Orb's
    /// hits (82739), Fire Power's explosion (83619) and the Frostfire Orb's hits (95969, 84721) do not ignite. A share
    /// below one point per tick banks nothing, so a running Ignite is left as it is.
    /// A crit that lands in the same server update as the crit whose share the caster's Ignite took last, before that
    /// Ignite ticked (Mage::IgniteMunches), replaces that share: the Ignite is first set to carry only what the Ignite
    /// before it had left (Mage::IgniteMunchedTick), so the new one rolls over that leftover and this share. When the
    /// cast puts no new Ignite on, the Ignite keeps its amount. The new holder keeps the bank in its script words.
    ProcOutcome IgniteProc(OnProcContext& ctx)
    {
        if (!Mage::ProcEffectIs(ctx, SPELL_AURA_DUMMY))
        {
            return ProcOutcome::Continue();
        }
        if (!(ctx.procEx & PROC_EX_CRITICAL_HIT))
        {
            return ProcOutcome::Return(SPELL_AURA_PROC_FAILED);
        }
        if (ctx.procSpell &&
            std::find(std::begin(NOT_IGNITING), std::end(NOT_IGNITING), ctx.procSpell->ID) != std::end(NOT_IGNITING))
        {
            return ProcOutcome::Return(SPELL_AURA_PROC_FAILED);
        }
        if (!ctx.victim || !ctx.victim->IsAlive() || ctx.triggerAmount <= 0)
        {
            return ProcOutcome::Return(SPELL_AURA_PROC_FAILED);
        }

        // the hit before the victim's damage-taken effects, divided by the multiplier of the DoT that ticks (the
        // Ignite row's own); the flat taken bonus stays in
        SpellEntry const* igniteDot = sSpellStore.LookupEntry(IGNITE_DOT);
        float const taken = igniteDot ? Spells::DamageTakenMultiplier(*ctx.victim, ctx.caster, *igniteDot) : 1.0f;
        int32 const tick = Combat::DotShareTick(float(Mage::ShareBase(ctx.damage, taken)), uint32(ctx.triggerAmount), IgniteTicks());
        if (tick <= 0)
        {
            return ProcOutcome::Return(SPELL_AURA_PROC_FAILED);
        }
        int32 const basePoints[MAX_EFFECT_INDEX] = { tick, 0, 0 };
        ObjectGuid const caster = ctx.caster.GetObjectGuid();
        uint32 const now = GameTime::GetGameTimeMS();

        // what the caster's Ignite on the victim has left before his bonus: the new Ignite rolls it over
        PeriodicDamageView earlier = PeriodicDamageView();
        SpellAuraHolder* const held = CastersIgnite(*ctx.victim, caster, earlier);
        int32 leftover = held ? earlier.amountBeforeDone * int32(earlier.remainingTicks) : 0;
        bool munched = false;
        if (held)
        {
            AuraHandle bank(*held);
            uint32 const bankUpdate = uint32(bank.ScriptWord(IGNITE_BANK_UPDATE));
            uint32 const bankTicks = uint32(bank.ScriptWord(IGNITE_BANK_TICKS));
            if (Mage::IgniteMunches(bankUpdate, bankTicks, now, earlier.remainingTicks))
            {
                leftover = bank.ScriptWord(IGNITE_BANK_LEFTOVER);
                bank.SetAmountBeforeDone(earlier.effIndex, Mage::IgniteMunchedTick(leftover, earlier.remainingTicks));
                munched = true;
            }
        }

        SpellAuraProcResult const result = Mage::CastProcSpell(ctx, *ctx.victim, IGNITE_DOT, basePoints);

        // a holder with no bank takes this one: the holder the cast put on, or an earlier one no crit banked in, whose
        // leftover is then its own; a holder with a bank is the earlier Ignite, which the cast did not replace
        PeriodicDamageView current = PeriodicDamageView();
        if (SpellAuraHolder* const after = CastersIgnite(*ctx.victim, caster, current))
        {
            AuraHandle bank(*after);
            if (!bank.ScriptWord(IGNITE_BANK_TICKS))
            {
                bank.ScriptWord(IGNITE_BANK_UPDATE) = int32(now);
                bank.ScriptWord(IGNITE_BANK_LEFTOVER) = leftover;
                bank.ScriptWord(IGNITE_BANK_TICKS) = int32(current.remainingTicks);
            }
            else if (munched)
            {
                bank.SetAmountBeforeDone(current.effIndex, earlier.amountBeforeDone);
            }
        }
        return ProcOutcome::Return(result);
    }

    uint32 const IGNITE[] = { 11119, 11120, 12846 };
}

uint32 Mage::ShareBase(uint32 dealt, float multiplier)
{
    if (multiplier <= 0.0f)
    {
        return dealt;
    }
    return uint32(float(dealt) / multiplier + 0.5f);
}

bool Mage::IgniteMunches(uint32 bankUpdate, uint32 bankTicks, uint32 now, uint32 ticksLeft)
{
    return bankTicks != 0 && bankUpdate == now && ticksLeft == bankTicks;
}

int32 Mage::IgniteMunchedTick(int32 leftover, uint32 ticksLeft)
{
    if (!ticksLeft)
    {
        return 0;
    }
    return leftover / int32(ticksLeft);
}

namespace Mage
{
    uint32 RegisterIgnite(SpellHandlerRegistry& registry)
    {
        return RegisterRanks<OnProcHook>(registry, IGNITE, &IgniteProc);
    }
}
