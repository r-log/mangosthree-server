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

#include "spells/classes/priest/discipline/InnerFocus.h"
#include "spells/data/SpellMgr.h"
#include "entities/Unit.h"
#include "DBCStores.h"
#include "Utilities/Util.h"

#include <cstddef>
#include <cstdint>

namespace
{
    uint32 const INNER_FOCUS = 89485;

    /// The spells Inner Focus's mods take, by the class masks of its two effects in the client's data: Flash Heal and
    /// its instant cast, Greater Heal, Prayer of Healing and Binding Heal. The after-cast row of each asks the masks
    /// again (Priest::InnerFocusTakes).
    uint32 const INNER_FOCUS_TAKERS[] = { 2061, 101062, 2060, 596, 32546 };

    uint32 const INNER_FOCUS_SPELL[] = { INNER_FOCUS };

    uint32 const HEAL_SPELL = 2050;
    uint32 const GREATER_HEAL = 2060;
    uint32 const FLASH_HEAL = 2061;
    uint32 const FLASH_HEAL_INSTANT = 101062;

    /// Strength of Soul's two ranks, and the immunity each gives when Inner Focus is applied, in the same order.
    uint32 const STRENGTH_OF_SOUL_RANKS[] = { 89488, 89489 };
    uint32 const STRENGTH_OF_SOUL_IMMUNITIES[] = { 96266, 96267 };

    /// The script effect both ranks trigger at the unit a heal of the priest heals.
    uint32 const STRENGTH_OF_SOUL_EFFECT[] = { 89490 };

    uint32 const WEAKENED_SOUL = 6788;

    /// Train of Thought's two ranks; both ranks' tooltips read their seconds from the first.
    uint32 const TRAIN_OF_THOUGHT_RANKS[] = { 92295, 92297 };

    uint32 const SMITE = 585;
    uint32 const PENANCE = 47540;

    typedef SpellHandlerOutcome<SpellAuraProcResult> ProcOutcome;

    /// `seconds` as milliseconds, held to INT32_MAX; 0 for 0 seconds or less.
    int32 SecondsToMs(int32 seconds)
    {
        if (seconds <= 0)
        {
            return 0;
        }
        int64 const ms = int64(seconds) * 1000;
        return ms < INT32_MAX ? int32(ms) : INT32_MAX;
    }

    /// The value of effect `eff` of `spellId` in the client's data; 0 when the store lacks the spell or the effect.
    int32 EffectValue(uint32 spellId, SpellEffectIndex eff)
    {
        SpellEntry const* spell = spellId ? sSpellStore.LookupEntry(spellId) : NULL;
        return spell ? spell->CalculateSimpleValue(eff) : 0;
    }

    /// 2061, 101062, 2060, 596, 32546: the spells Inner Focus's mods (-100% mana cost, +25% critical effect chance)
    /// take. A cast that finished has taken its cost and dealt its heals with the mods, so a caster who holds Inner
    /// Focus loses it when the cast's spell is one its mods take (Priest::InnerFocusTakes). Inner Focus is disabled
    /// while active in the client's data, so the engine holds its cooldown while the aura stands and starts it, with
    /// its cooldown event, when the aura goes. A cast that fails or is interrupted does not finish, and leaves Inner
    /// Focus on him. A Greater Heal whose Train of Thought shortens Inner Focus's cooldown has already taken Inner
    /// Focus off at its hit (TrainOfThoughtProc), and finds none here.
    SpellHandlerOutcome<void> InnerFocusTaken(AfterCastContext& ctx)
    {
        if (ctx.caster.HasAura(INNER_FOCUS) && Priest::InnerFocusTakes(ctx.spell))
        {
            ctx.caster.RemoveAurasDueToSpell(INNER_FOCUS);
        }
        return SpellHandlerOutcome<void>::Continue();
    }

    /// 89485: Inner Focus: when its holder is added to the priest, not while he loads, he casts the immunity of his
    /// rank of Strength of Soul at himself, triggered (Priest::StrengthOfSoulImmunity): immune to interrupts, silences
    /// and dispels for the immunity's duration in the client's data (3 s for 96266, 5 s for 96267). Nothing without a
    /// rank, and nothing at the call before the stacking checks.
    SpellHandlerOutcome<void> InnerFocusApplied(AuraApplyContext& ctx)
    {
        if (!ctx.real || ctx.target.IsLoading())
        {
            return SpellHandlerOutcome<void>::Continue();
        }
        if (uint32 const immunity = Priest::StrengthOfSoulImmunity(Priest::StrengthOfSoulRank(ctx.target)))
        {
            ctx.CastTriggered(ctx.target, immunity);
        }
        return SpellHandlerOutcome<void>::Continue();
    }

    /// 89488, 89489: Strength of Soul: its trigger effect's proc flags take every helpful spell of the priest that
    /// heals, and the client's data gives it no class mask; its effect (89490) follows only a Heal, a Greater Heal or a
    /// Flash Heal (Priest::StrengthOfSoulFrom). Every other spell is dropped from the proc; those three are left to the
    /// engine's checks. A body that changes nothing: the holders are being walked.
    SpellHandlerOutcome<bool> StrengthOfSoulCheckProc(CheckProcContext& ctx)
    {
        if (!ctx.procSpell || !Priest::StrengthOfSoulFrom(ctx.procSpell->ID))
        {
            return SpellHandlerOutcome<bool>::Return(false);
        }
        return SpellHandlerOutcome<bool>::Continue();
    }

    /// 89490: Strength of Soul's script effect, which the talent's ranks trigger at the unit the priest's heal heals:
    /// that unit's Weakened Soul (6788) loses the seconds of the rank the priest holds (Priest::StrengthOfSoulRank),
    /// its effect 0 value in the client's data (Priest::WeakenedSoulCut). What is left becomes its whole duration too
    /// (AuraHandle::Shorten), and its clients are told both; a Weakened Soul left with no time runs out at the unit's
    /// next update. 89490's own value is not read. Nothing for a unit with no Weakened Soul, nor for a priest with no
    /// rank. The switches after it run.
    SpellHandlerOutcome<void> StrengthOfSoulEffect(DummyEffectContext& ctx)
    {
        if (ctx.effIndex != EFFECT_INDEX_0 || !ctx.target)
        {
            return SpellHandlerOutcome<void>::Continue();
        }
        int32 const cut = Priest::WeakenedSoulCut(EffectValue(Priest::StrengthOfSoulRank(ctx.caster), EFFECT_INDEX_0));
        SpellAuraHolder* soul = ctx.target->GetSpellAuraHolder(WEAKENED_SOUL);
        if (cut > 0 && soul)
        {
            AuraHandle(*soul).Shorten(cut);
        }
        return SpellHandlerOutcome<void>::Continue();
    }

    /// 92295, 92297: Train of Thought: its dummy effect's proc flags take the priest's helpful spells and his harmful
    /// spells' hits. A hit of Greater Heal or of Smite shortens the cooldown Priest::TrainOfThoughtCooldown names,
    /// Inner Focus's or Penance's, by Priest::TrainOfThoughtCut, read from 92295's effects as both ranks' tooltips read
    /// them, at the rank's chance (Priest::TrainOfThoughtChance); a chance of 100 draws no roll. The cut goes to the
    /// priest's store and to his client (OnProcContext::ShortenCooldown): exactly, -5000 or -500 ms, to the client,
    /// rounded up to whole seconds in the store. A Greater Heal that Inner Focus empowers has paid its cost and rolled
    /// its crit before its hit procs, so Inner Focus leaves the priest here, before the cut, which starts its cooldown,
    /// held while the aura stood, and the cut then shortens that cooldown: Inner Focus gone, its cooldown event, the
    /// cut, then the heal's log. CANT_TRIGGER for any other spell; FAILED for an effect other than the dummy, or a roll
    /// that failed.
    ProcOutcome TrainOfThoughtProc(OnProcContext& ctx)
    {
        uint32 const cooldown = ctx.procSpell ? Priest::TrainOfThoughtCooldown(ctx.procSpell->ID) : 0;
        if (!cooldown)
        {
            return ProcOutcome::Return(SPELL_AURA_PROC_CANT_TRIGGER);
        }
        if (ctx.effIndex != EFFECT_INDEX_0)
        {
            return ProcOutcome::Return(SPELL_AURA_PROC_FAILED);
        }
        int32 const chance = Priest::TrainOfThoughtChance(ctx.spell.ID, ctx.spell.CalculateSimpleValue(EFFECT_INDEX_2),
                                                          int32(ctx.spell.GetProcChance()));
        if (chance <= 0 || (chance < 100 && !roll_chance_i(chance)))
        {
            return ProcOutcome::Return(SPELL_AURA_PROC_FAILED);
        }
        int32 const cut = Priest::TrainOfThoughtCut(cooldown, EffectValue(TRAIN_OF_THOUGHT_RANKS[0], EFFECT_INDEX_0),
                                                    EffectValue(TRAIN_OF_THOUGHT_RANKS[0], EFFECT_INDEX_1));
        if (cut > 0)
        {
            if (cooldown == INNER_FOCUS && ctx.caster.HasAura(INNER_FOCUS) && Priest::InnerFocusTakes(*ctx.procSpell))
            {
                ctx.caster.RemoveAurasDueToSpell(INNER_FOCUS);
            }
            ctx.ShortenCooldown(cooldown, uint32(cut));
        }
        return ProcOutcome::Return(SPELL_AURA_PROC_OK);
    }
}

uint32 Priest::RegisterInnerFocus(SpellHandlerRegistry& registry)
{
    uint32 rows = RegisterRanks<AfterCastHook>(registry, INNER_FOCUS_TAKERS, &InnerFocusTaken);
    rows += RegisterRanks<AuraApplyHook>(registry, INNER_FOCUS_SPELL, &InnerFocusApplied);
    rows += RegisterRanks<CheckProcHook>(registry, STRENGTH_OF_SOUL_RANKS, &StrengthOfSoulCheckProc);
    rows += RegisterRanks<DummyEffectHook>(registry, STRENGTH_OF_SOUL_EFFECT, &StrengthOfSoulEffect);
    rows += RegisterRanks<OnProcHook>(registry, TRAIN_OF_THOUGHT_RANKS, &TrainOfThoughtProc);
    return rows;
}

bool Priest::InnerFocusTakes(SpellEntry const& spell)
{
    SpellEntry const* focus = sSpellStore.LookupEntry(INNER_FOCUS);
    if (!focus)
    {
        return false;
    }
    for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        SpellEffectEntry const* effect = focus->GetSpellEffect(SpellEffectIndex(i));
        if (effect && spell.IsFitToFamily(focus->GetSpellFamilyName(), effect->EffectSpellClassMask))
        {
            return true;
        }
    }
    return false;
}

uint32 Priest::StrengthOfSoulImmunity(uint32 rankSpellId)
{
    for (std::size_t i = 0; i < sizeof(STRENGTH_OF_SOUL_RANKS) / sizeof(STRENGTH_OF_SOUL_RANKS[0]); ++i)
    {
        if (STRENGTH_OF_SOUL_RANKS[i] == rankSpellId)
        {
            return STRENGTH_OF_SOUL_IMMUNITIES[i];
        }
    }
    return 0;
}

uint32 Priest::StrengthOfSoulRank(Unit const& priest)
{
    if (priest.HasAura(STRENGTH_OF_SOUL_RANKS[1]))
    {
        return STRENGTH_OF_SOUL_RANKS[1];
    }
    return priest.HasAura(STRENGTH_OF_SOUL_RANKS[0]) ? STRENGTH_OF_SOUL_RANKS[0] : 0;
}

bool Priest::StrengthOfSoulFrom(uint32 spellId)
{
    return spellId == HEAL_SPELL || spellId == GREATER_HEAL || spellId == FLASH_HEAL ||
           spellId == FLASH_HEAL_INSTANT;
}

int32 Priest::WeakenedSoulCut(int32 seconds)
{
    return SecondsToMs(seconds);
}

uint32 Priest::TrainOfThoughtCooldown(uint32 spellId)
{
    if (spellId == GREATER_HEAL)
    {
        return INNER_FOCUS;
    }
    return spellId == SMITE ? PENANCE : 0;
}

int32 Priest::TrainOfThoughtCut(uint32 cooldownSpellId, int32 seconds, int32 penanceMs)
{
    if (cooldownSpellId == INNER_FOCUS)
    {
        return SecondsToMs(seconds);
    }
    if (cooldownSpellId == PENANCE)
    {
        return penanceMs > 0 ? penanceMs : 0;
    }
    return 0;
}

int32 Priest::TrainOfThoughtChance(uint32 rankSpellId, int32 effectTwoValue, int32 procChance)
{
    if (rankSpellId == TRAIN_OF_THOUGHT_RANKS[0])
    {
        return effectTwoValue;
    }
    return rankSpellId == TRAIN_OF_THOUGHT_RANKS[1] ? procChance : 0;
}
