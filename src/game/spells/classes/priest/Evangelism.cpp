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

#include "spells/classes/priest/Evangelism.h"
#include "spells/data/SpellMgr.h"
#include "entities/Unit.h"
#include "DBCStores.h"

#include <cstddef>
#include <cstdint>

namespace
{
    uint32 const SMITE = 585;
    uint32 const HOLY_FIRE = 14914;

    /// Penance's bolt at an enemy, which each tick of the channel at an enemy casts.
    uint32 const PENANCE_BOLT = 47666;

    /// The talent's two ranks, and the buff a stack of each is, in the same order.
    uint32 const EVANGELISM_RANKS[] = { 81659, 81662 };
    uint32 const EVANGELISM_STACKS[] = { 81660, 81661 };

    /// Dark Evangelism's stacks, of the first and the second rank, which Mind Flay gives.
    uint32 const DARK_EVANGELISM_STACKS[] = { 87117, 87118 };

    /// The marker Archangel asks of its caster (its caster aura spell in the client's data).
    uint32 const EVANGELISM_MARKER = 87154;

    uint32 const ARCHANGEL[] = { 87151 };

    /// Archangel's healing done bonus and its mana, the percent of his maximum mana it gives.
    uint32 const ARCHANGEL_HEALING = 81700;
    uint32 const ARCHANGEL_MANA = 87152;

    typedef SpellHandlerOutcome<SpellAuraProcResult> ProcOutcome;

    /// The effect 0 value of `spellId` in the client's data; 0 when the store lacks the spell.
    int32 EffectZeroValue(uint32 spellId)
    {
        SpellEntry const* spell = sSpellStore.LookupEntry(spellId);
        return spell ? spell->CalculateSimpleValue(EFFECT_INDEX_0) : 0;
    }

    /// 81659, 81662: Evangelism: its dummy effect's proc flags take the priest's harmful spell hits and his periodic
    /// damage, and its class mask takes Smite and Penance's bolts alone. The script takes a hit of Smite, Holy Fire or
    /// Penance's bolt that dealt damage (Priest::EvangelismFromHit), so Holy Fire's hit, which the class mask lacks,
    /// procs as Smite's does. Every other event is left to the class mask and the hit kind: Holy Fire's ticks, the
    /// misses and the other spells give no stack. A body that changes nothing: the holders are being walked.
    SpellHandlerOutcome<bool> EvangelismCheckProc(CheckProcContext& ctx)
    {
        if (Priest::EvangelismFromHit(ctx.procSpell ? ctx.procSpell->ID : 0, ctx.procFlag, ctx.procEx, ctx.damage))
        {
            return SpellHandlerOutcome<bool>::Return(true);
        }
        return SpellHandlerOutcome<bool>::Continue();
    }

    /// 81659, 81662: Evangelism: for a hit that gives a stack (Priest::EvangelismFromHit), the priest casts his rank's
    /// buff (Priest::EvangelismStack) at himself, triggered by the talent: a stack added up to the buff's limit in the
    /// client's data, and its duration refreshed. While he holds it he casts the marker (87154) at himself the same
    /// way, so the marker stands as long as the stacks. CANT_TRIGGER for another event; FAILED, nothing cast, for an
    /// effect other than the dummy or a spell that is no rank of the talent; FAILED, with no marker, when the buff
    /// did not stay on him.
    ProcOutcome EvangelismProc(OnProcContext& ctx)
    {
        if (!ctx.procSpell || !Priest::EvangelismFromHit(ctx.procSpell->ID, ctx.procFlag, ctx.procEx, ctx.damage))
        {
            return ProcOutcome::Return(SPELL_AURA_PROC_CANT_TRIGGER);
        }
        uint32 const stack = Priest::EvangelismStack(ctx.spell.ID);
        if (ctx.effIndex != EFFECT_INDEX_0 || !stack)
        {
            return ProcOutcome::Return(SPELL_AURA_PROC_FAILED);
        }
        ctx.CastTriggered(ctx.caster, stack);
        if (!ctx.caster.HasAura(stack))
        {
            return ProcOutcome::Return(SPELL_AURA_PROC_FAILED);
        }
        ctx.CastTriggered(ctx.caster, EVANGELISM_MARKER);
        return ProcOutcome::Return(SPELL_AURA_PROC_OK);
    }

    /// 81660, 81661: Evangelism's buffs: when one leaves the priest and he holds no stack of either kind of
    /// Evangelism (Priest::HoldsEvangelism), the marker (87154) leaves him too. A marker that Dark Evangelism's stacks
    /// stand for stays while they do. The holder's own boosts run after it.
    SpellHandlerOutcome<void> EvangelismStackRemove(AuraRemoveContext& ctx)
    {
        if (!Priest::HoldsEvangelism(ctx.target))
        {
            ctx.RemoveAura(ctx.target, EVANGELISM_MARKER);
        }
        return SpellHandlerOutcome<void>::Continue();
    }

    /// 87151: Archangel's script effect: the priest's stacks of Evangelism (Priest::EvangelismStacks) are consumed. He
    /// casts the healing done bonus (81700) at himself, triggered by Archangel, with its effect 0 value for each stack,
    /// while he still holds the stacks, which 81700 asks of its caster in the client's data; then the mana (87152), a
    /// percent of his maximum mana, with its effect 0 value for each stack (Priest::ArchangelPoints). Then his buffs of
    /// both ranks are taken off him, and the marker with them (EvangelismStackRemove). 81700's cast puts its
    /// category's recovery, from the client's data, on the category's spells, Archangel among them: that is
    /// Archangel's cooldown. With no stack nothing is cast and nothing taken off.
    /// The Dark Evangelism arm, Mind Flay's stacks (87117, 87118) consumed into Dark Archangel (87153), is not done
    /// here: this body leaves those stacks, that spell and the marker they stand for as it finds them, and the family
    /// switches and the script library run after it.
    SpellHandlerOutcome<void> ArchangelEffect(DummyEffectContext& ctx)
    {
        if (ctx.effIndex != EFFECT_INDEX_0)
        {
            return SpellHandlerOutcome<void>::Continue();
        }
        uint32 const stacks = Priest::EvangelismStacks(ctx.caster);
        if (!stacks)
        {
            return SpellHandlerOutcome<void>::Continue();
        }
        int32 const healing = Priest::ArchangelPoints(EffectZeroValue(ARCHANGEL_HEALING), stacks);
        if (healing > 0)
        {
            int32 const basePoints[MAX_EFFECT_INDEX] = { healing, 0, 0 };
            ctx.CastTriggered(ctx.caster, ARCHANGEL_HEALING, basePoints);
        }
        int32 const mana = Priest::ArchangelPoints(EffectZeroValue(ARCHANGEL_MANA), stacks);
        if (mana > 0)
        {
            int32 const basePoints[MAX_EFFECT_INDEX] = { mana, 0, 0 };
            ctx.CastTriggered(ctx.caster, ARCHANGEL_MANA, basePoints);
        }
        for (uint32 stack : EVANGELISM_STACKS)
        {
            ctx.RemoveAura(ctx.caster, stack);
        }
        return SpellHandlerOutcome<void>::Continue();
    }
}

uint32 Priest::RegisterEvangelism(SpellHandlerRegistry& registry)
{
    uint32 rows = RegisterRanks<CheckProcHook>(registry, EVANGELISM_RANKS, &EvangelismCheckProc);
    rows += RegisterRanks<OnProcHook>(registry, EVANGELISM_RANKS, &EvangelismProc);
    rows += RegisterRanks<AuraRemoveHook>(registry, EVANGELISM_STACKS, &EvangelismStackRemove);
    rows += RegisterRanks<DummyEffectHook>(registry, ARCHANGEL, &ArchangelEffect);
    return rows;
}

bool Priest::EvangelismFromSpell(uint32 spellId)
{
    return spellId == SMITE || spellId == HOLY_FIRE || spellId == PENANCE_BOLT;
}

bool Priest::EvangelismFromHit(uint32 spellId, uint32 procFlag, uint32 procEx, uint32 damage)
{
    return EvangelismFromSpell(spellId) && (procFlag & PROC_FLAG_SUCCESSFUL_NEGATIVE_SPELL_HIT) &&
           (procEx & (PROC_EX_NORMAL_HIT | PROC_EX_CRITICAL_HIT)) && damage;
}

uint32 Priest::EvangelismStack(uint32 rankSpellId)
{
    for (std::size_t i = 0; i < sizeof(EVANGELISM_RANKS) / sizeof(EVANGELISM_RANKS[0]); ++i)
    {
        if (EVANGELISM_RANKS[i] == rankSpellId)
        {
            return EVANGELISM_STACKS[i];
        }
    }
    return 0;
}

uint32 Priest::EvangelismStacks(Unit const& priest)
{
    for (uint32 stack : EVANGELISM_STACKS)
    {
        if (SpellAuraHolder* holder = priest.GetSpellAuraHolder(stack, priest.GetObjectGuid()))
        {
            return AuraHandle(*holder).Stacks();
        }
    }
    return 0;
}

bool Priest::HoldsEvangelism(Unit const& priest)
{
    for (uint32 stack : EVANGELISM_STACKS)
    {
        if (priest.HasAura(stack))
        {
            return true;
        }
    }
    for (uint32 stack : DARK_EVANGELISM_STACKS)
    {
        if (priest.HasAura(stack))
        {
            return true;
        }
    }
    return false;
}

int32 Priest::ArchangelPoints(int32 perStack, uint32 stacks)
{
    if (perStack <= 0 || !stacks)
    {
        return 0;
    }
    int64 const points = int64(perStack) * stacks;
    return points < INT32_MAX ? int32(points) : INT32_MAX;
}
