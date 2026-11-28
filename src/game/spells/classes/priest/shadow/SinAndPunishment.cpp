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

#include "spells/classes/priest/shadow/SinAndPunishment.h"
#include "spells/data/SpellMgr.h"
#include "entities/Unit.h"
#include "DBCStores.h"
#include "Utilities/Util.h"

#include <cstdint>

namespace
{
    /// The talent's two ranks, the first rank first. Effect 0 of each is a dummy effect that holds the chance in
    /// percent of the fear (50 and 100) and effect 1 an aura that holds the seconds the Shadowfiend's cooldown is cut
    /// by (5 and 10): "$s1% chance", "reduced by $s2 sec" in the client's text.
    uint32 const SIN_AND_PUNISHMENT_RANK_1 = 87099;
    uint32 const SIN_AND_PUNISHMENT_RANK_2 = 87100;
    uint32 const SIN_AND_PUNISHMENT_RANKS[] = { SIN_AND_PUNISHMENT_RANK_1, SIN_AND_PUNISHMENT_RANK_2 };

    /// Vampiric Touch: the aura whose dispel gives the fear.
    uint32 const VAMPIRIC_TOUCH[] = { 34914 };

    /// The fear: Sin and Punishment, a spell that takes the units friendly to its caster within 6 yards of him
    /// (radius index 29) and feared them for 3 s.
    uint32 const SIN_AND_PUNISHMENT_FEAR = 87204;

    /// The spell whose critical ticks cut the cooldown: Mind Flay.
    uint32 const MIND_FLAY = 15407;

    /// The Shadowfiend, whose cooldown (300 s in the client's data) a critical tick cuts.
    uint32 const SHADOWFIEND = 34433;

    /// A cooldown is cut in milliseconds, a rank holds seconds.
    int32 const MS_PER_SECOND = 1000;

    typedef SpellHandlerOutcome<SpellAuraProcResult> ProcOutcome;

    /// The value of effect `index` of `spellId` in the client's data, for a rank of the talent only; 0 for any other
    /// spell and when the store lacks it.
    int32 RankValue(uint32 spellId, SpellEffectIndex index)
    {
        if (spellId != SIN_AND_PUNISHMENT_RANK_1 && spellId != SIN_AND_PUNISHMENT_RANK_2)
        {
            return 0;
        }
        SpellEntry const* rank = sSpellStore.LookupEntry(spellId);
        return rank ? rank->CalculateSimpleValue(index) : 0;
    }

    /// 34914: Vampiric Touch: when a dispel takes the holder (Priest::SinAndPunishmentDispelFears) and its caster
    /// holds a rank of the talent, a roll at the rank's chance (a chance of 100 draws none) has the dispeller cast the
    /// fear at himself, triggered, with the priest as the original caster (AuraRemoveContext::CastTriggeredFrom):
    /// the spell's own targets are the units friendly to the dispeller within its radius, the dispeller among them.
    /// Nothing for a removal that is no dispel, for a dispel with no dispeller and for a priest with no rank. The
    /// holder's own boosts run after it.
    SpellHandlerOutcome<void> VampiricTouchRemoved(AuraRemoveContext& ctx)
    {
        if (!Priest::SinAndPunishmentDispelFears(ctx.mode, ctx.dispeller != NULL, ctx.caster != NULL))
        {
            return SpellHandlerOutcome<void>::Continue();
        }
        int32 const chance = Priest::SinAndPunishmentChance(Priest::SinAndPunishmentRank(*ctx.caster));
        if (chance > 0 && (chance >= 100 || Priest::SinAndPunishmentFears(chance, irand(0, 99))))
        {
            ctx.CastTriggeredFrom(*ctx.dispeller, *ctx.dispeller, SIN_AND_PUNISHMENT_FEAR);
        }
        return SpellHandlerOutcome<void>::Continue();
    }

    /// 87099, 87100: Sin and Punishment: its proc flags take the priest's periodic damage, and it has no
    /// spell_proc_event row, so the script's answer is the whole check: true for a critical tick of Mind Flay
    /// (Priest::SinAndPunishmentTakes), false for every other event, which no longer reaches the aura's effects.
    SpellHandlerOutcome<bool> SinAndPunishmentCheckProc(CheckProcContext& ctx)
    {
        uint32 const spellId = ctx.procSpell ? ctx.procSpell->ID : 0;
        return SpellHandlerOutcome<bool>::Return(Priest::SinAndPunishmentTakes(spellId, ctx.procFlag, ctx.procEx));
    }

    /// 87099, 87100: Sin and Punishment: a critical tick of Mind Flay (the check-proc row has let only that through,
    /// and the script asks again) cuts the Shadowfiend's cooldown by the rank's effect 1 seconds
    /// (Priest::SinAndPunishmentCut), in his store and to his client (OnProcContext::ShortenCooldown). CANT_TRIGGER
    /// for any other event; FAILED for an effect other than the aura's second, which holds the cut; OK for the cut,
    /// made or not (nothing to cut while the Shadowfiend is ready).
    ProcOutcome SinAndPunishmentProc(OnProcContext& ctx)
    {
        uint32 const spellId = ctx.procSpell ? ctx.procSpell->ID : 0;
        if (!Priest::SinAndPunishmentTakes(spellId, ctx.procFlag, ctx.procEx))
        {
            return ProcOutcome::Return(SPELL_AURA_PROC_CANT_TRIGGER);
        }
        if (ctx.effIndex != EFFECT_INDEX_1)
        {
            return ProcOutcome::Return(SPELL_AURA_PROC_FAILED);
        }
        int32 const seconds = Priest::SinAndPunishmentCut(ctx.spell.ID);
        if (seconds > 0)
        {
            ctx.ShortenCooldown(SHADOWFIEND, uint32(seconds) * uint32(MS_PER_SECOND));
        }
        return ProcOutcome::Return(SPELL_AURA_PROC_OK);
    }
}

uint32 Priest::RegisterSinAndPunishment(SpellHandlerRegistry& registry)
{
    uint32 rows = RegisterRanks<AuraRemoveHook>(registry, VAMPIRIC_TOUCH, &VampiricTouchRemoved);
    rows += RegisterRanks<CheckProcHook>(registry, SIN_AND_PUNISHMENT_RANKS, &SinAndPunishmentCheckProc);
    rows += RegisterRanks<OnProcHook>(registry, SIN_AND_PUNISHMENT_RANKS, &SinAndPunishmentProc);
    return rows;
}

uint32 Priest::SinAndPunishmentRank(Unit const& priest)
{
    if (priest.HasAura(SIN_AND_PUNISHMENT_RANK_2))
    {
        return SIN_AND_PUNISHMENT_RANK_2;
    }
    return priest.HasAura(SIN_AND_PUNISHMENT_RANK_1) ? SIN_AND_PUNISHMENT_RANK_1 : 0;
}

int32 Priest::SinAndPunishmentChance(uint32 rank)
{
    return RankValue(rank, EFFECT_INDEX_0);
}

int32 Priest::SinAndPunishmentCut(uint32 rank)
{
    return RankValue(rank, EFFECT_INDEX_1);
}

bool Priest::SinAndPunishmentFears(int32 chance, int32 roll)
{
    return chance >= 100 || (chance > 0 && roll < chance);
}

bool Priest::SinAndPunishmentDispelFears(AuraRemoveMode mode, bool dispeller, bool caster)
{
    return mode == AURA_REMOVE_BY_DISPEL && dispeller && caster;
}

bool Priest::SinAndPunishmentTakes(uint32 spellId, uint32 procFlag, uint32 procEx)
{
    return spellId == MIND_FLAY && (procFlag & PROC_FLAG_ON_DO_PERIODIC) && (procEx & PROC_EX_CRITICAL_HIT);
}
