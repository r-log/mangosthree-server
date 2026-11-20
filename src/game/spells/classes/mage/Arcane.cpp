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
#include "spells/data/SpellMgr.h"
#include "spells/data/SpellTables.h"
#include "spells/SpellScriptQueries.h"
#include "entities/Unit.h"
#include "DBCStores.h"
#include "Log.h"

#include <algorithm>
#include <iterator>

namespace
{
    typedef SpellHandlerOutcome<SpellAuraProcResult> ProcOutcome;

    /// 31571, 31572: Arcane Potency: casts the buff of the aura's rank (GetArcanePotencyProcSpell) on the owner.
    ProcOutcome ArcanePotencyProc(OnProcContext& ctx)
    {
        if (!Mage::ProcEffectIs(ctx, SPELL_AURA_DUMMY))
        {
            return ProcOutcome::Continue();
        }
        if (!ctx.procSpell)
        {
            return ProcOutcome::Return(SPELL_AURA_PROC_FAILED);
        }

        uint32 const buff = GetArcanePotencyProcSpell(ctx.spell.ID);
        if (!buff)
        {
            sLog.outError("Arcane Potency: spell %u has no buff of its rank", ctx.spell.ID);
            return ProcOutcome::Return(SPELL_AURA_PROC_FAILED);
        }
        return ProcOutcome::Return(Mage::CastProcSpell(ctx, ctx.caster, buff));
    }

    /// 1463: Mana Shield: its proc, which a chance-of-success modifier such as the Incanter's Regalia set's (37424)
    /// gives it, casts Arcane Surge (37436) on a player owner.
    ProcOutcome ManaShieldProc(OnProcContext& ctx)
    {
        if (!Mage::ProcEffectIs(ctx, SPELL_AURA_MANA_SHIELD))
        {
            return ProcOutcome::Continue();
        }
        if (ctx.caster.GetTypeId() != TYPEID_PLAYER)
        {
            return ProcOutcome::Return(SPELL_AURA_PROC_FAILED);
        }
        return ProcOutcome::Return(Mage::CastProcSpell(ctx, ctx.caster, 37436));
    }

    uint32 const INCANTERS_ABSORPTION[] = { 44394, 44395 };
    uint32 const INCANTERS_ABSORPTION_KNOCKBACK = 86261;

    /// 1463: Mana Shield: when damage used up its last point and the mage knows a rank of Incanter's Absorption, he
    /// casts 86261, which knocks back the enemies within 6 yards of him (Mage::IncantersAbsorptionKnocksBack). A shield
    /// that is cancelled, runs out or is dispelled casts nothing. The removal's boosts run.
    SpellHandlerOutcome<void> ManaShieldAuraRemove(AuraRemoveContext& ctx)
    {
        bool knowsTalent = false;
        for (uint32 rank : INCANTERS_ABSORPTION)
        {
            knowsTalent = knowsTalent || ctx.target.HasAura(rank);
        }
        if (Mage::IncantersAbsorptionKnocksBack(ctx.mode, knowsTalent))
        {
            ctx.CastTriggered(ctx.target, INCANTERS_ABSORPTION_KNOCKBACK);
        }
        return SpellHandlerOutcome<void>::Continue();
    }

    uint32 const IMPROVED_MANA_GEM[] = { 31584, 31585 };
    uint32 const IMPROVED_MANA_GEM_BUFF = 83098;
    uint32 const MANA_GEMS[] = { 5513, 5514, 8007, 8008, 22044, 33312, 36799 };

    /// 5405: Replenish Mana, the Mana Gem's spell: once its cast from a Mana Gem (Mage::IsManaGem) is over, a mage who
    /// knows a rank of Improved Mana Gem casts 83098 on himself, both of its effects (spell damage and healing done) at
    /// the rank's percent of his maximum mana (Mage::ImprovedManaGemAmount). A cast from any other item or from no item,
    /// and a caster without the talent, cast nothing.
    SpellHandlerOutcome<void> ReplenishManaAfterCast(AfterCastContext& ctx)
    {
        if (!Mage::IsManaGem(ctx.castItemEntry))
        {
            return SpellHandlerOutcome<void>::Continue();
        }
        int32 percent = 0;
        for (uint32 rank : IMPROVED_MANA_GEM)
        {
            if (SpellAuraHolder* holder = ctx.caster.GetSpellAuraHolder(rank))
            {
                percent = AuraHandle(*holder).Amount(EFFECT_INDEX_0);
            }
        }
        int32 const amount = Mage::ImprovedManaGemAmount(ctx.caster.GetMaxPower(POWER_MANA), percent);
        if (amount > 0)
        {
            int32 const basePoints[MAX_EFFECT_INDEX] = { amount, amount, 0 };
            ctx.CastTriggered(ctx.caster, IMPROVED_MANA_GEM_BUFF, basePoints);
        }
        return SpellHandlerOutcome<void>::Continue();
    }

    uint32 const ARCANE_MISSILES_BUFF = 79683;
    uint32 const ARCANE_MISSILES_AURASTATE = 79808;

    /// Casts Arcane Missiles Aurastate (79808) on the unit the context's aura is on, or takes it off him, as
    /// Mage::ArcaneMissilesAurastateStep answers for what he holds of it and of Arcane Missiles! (79683).
    template <class Context>
    void PairArcaneMissilesAurastate(Context& ctx)
    {
        switch (Mage::ArcaneMissilesAurastateStep(ctx.target.HasAura(ARCANE_MISSILES_BUFF),
                                                  ctx.target.HasAura(ARCANE_MISSILES_AURASTATE)))
        {
            case Mage::AURASTATE_APPLY:
                ctx.CastTriggered(ctx.target, ARCANE_MISSILES_AURASTATE);
                break;
            case Mage::AURASTATE_REMOVE:
                ctx.RemoveAura(ctx.target, ARCANE_MISSILES_AURASTATE);
                break;
            case Mage::AURASTATE_KEEP:
                break;
        }
    }

    /// 79683: Arcane Missiles!: at its apply the mage gets Arcane Missiles Aurastate (79808), which Arcane Missiles
    /// (5143) asks for on its caster. The call before the holder is added does nothing.
    SpellHandlerOutcome<void> ArcaneMissilesBuffApply(AuraApplyContext& ctx)
    {
        if (ctx.real)
        {
            PairArcaneMissilesAurastate(ctx);
        }
        return SpellHandlerOutcome<void>::Continue();
    }

    /// 79683: Arcane Missiles!: at its removal (used up, run out, cancelled or at death) Arcane Missiles
    /// Aurastate (79808) is taken off the mage.
    SpellHandlerOutcome<void> ArcaneMissilesBuffRemove(AuraRemoveContext& ctx)
    {
        PairArcaneMissilesAurastate(ctx);
        return SpellHandlerOutcome<void>::Continue();
    }

    /// 5143: Arcane Missiles: the channel's holder on the mage himself, applied at the cast, takes Arcane Missiles!
    /// (79683) off him: the cast uses it up. The channel's holder on its target, and the call before the holder is
    /// added, do nothing here.
    SpellHandlerOutcome<void> ArcaneMissilesChannelApply(AuraApplyContext& ctx)
    {
        if (ctx.real && ctx.caster == &ctx.target)
        {
            ctx.RemoveAura(ctx.target, ARCANE_MISSILES_BUFF);
        }
        return SpellHandlerOutcome<void>::Continue();
    }

    uint32 const ARCANE_BLAST = 30451;
    uint32 const ARCANE_EXPLOSION = 1449;

    /// 36032: Arcane Blast's buff: the engine's proc handling, which spends the buff's one charge and so removes it, is
    /// reached only at the end of the mage's own cast of an Arcane damage spell other than Arcane Blast and Arcane
    /// Explosion (Mage::ArcaneBlastBuffEnds). Every other event is dropped: the end of an Arcane Blast or Arcane
    /// Explosion cast, where the engine lets a modifier aura with charges spend one, a triggered spell's and any hit.
    /// A cast is his own when it stands in one of his current spell slots, where no triggered cast goes.
    SpellHandlerOutcome<bool> ArcaneBlastBuffCheckProc(CheckProcContext& ctx)
    {
        SpellEntry const* const spell = ctx.procSpell;
        if (spell && Mage::ArcaneBlastBuffEnds(spell->ID, spell->SchoolMask, Mage::DealsOrTriggersDamage(*spell),
                                               (ctx.procEx & PROC_EX_CAST_END) != 0,
                                               ctx.owner.FindCurrentSpellBySpellId(spell->ID) != NULL))
        {
            return SpellHandlerOutcome<bool>::Continue();
        }
        return SpellHandlerOutcome<bool>::Return(false);
    }

    /// 57529, 57531: Arcane Potency's buffs: an event of a spell that neither deals nor triggers damage
    /// (Mage::DealsOrTriggersDamage) is dropped and spends no charge, such as the cast of Arcane Blast's buff (36032),
    /// which Arcane Blast triggers and whose class mask the buffs' crit modifier names. Every other event goes on to
    /// the engine's proc handling, which spends a charge at the end of a cast of a spell the modifier names.
    SpellHandlerOutcome<bool> ArcanePotencyBuffCheckProc(CheckProcContext& ctx)
    {
        SpellEntry const* const spell = ctx.procSpell;
        if (spell && !Mage::DealsOrTriggersDamage(*spell))
        {
            return SpellHandlerOutcome<bool>::Return(false);
        }
        return SpellHandlerOutcome<bool>::Continue();
    }

    uint32 const TORMENT_THE_WEAK[] = { 29447, 55339, 55340 };
    uint32 const SLOW = 31589;

    /// 29447, 55339, 55340: Torment the Weak, run for the rank the caster holds: his mage spells of the Arcane school
    /// deal the rank's percent more damage to a target that is snared or slowed (Mage::TormentTheWeakMultiplier,
    /// Mage::IsSnaredOrSlowed).
    SpellHandlerOutcome<void> TormentTheWeakDamageDone(DamageDoneContext& ctx)
    {
        int32 percent = 0;
        for (uint32 rank : TORMENT_THE_WEAK)
        {
            if (SpellAuraHolder* holder = ctx.caster.GetSpellAuraHolder(rank))
            {
                percent = AuraHandle(*holder).Amount(EFFECT_INDEX_0);
            }
        }
        uint32 const family = ctx.spell.GetSpellFamilyName();
        bool const slowed = Mage::IsSnaredOrSlowed(ctx.victim);
        ctx.doneTotalMod *= Mage::TormentTheWeakMultiplier(percent, family, ctx.spell.SchoolMask, slowed);
        return SpellHandlerOutcome<void>::Continue();
    }

    /// 86181, 86209: Nether Vortex: after the talent's own proc chance, a hit of the mage's Arcane Blast that deals
    /// damage casts Slow (31589) at its target, triggered, so even beyond Slow's range, while none of his Slow is on
    /// any unit (Mage::NetherVortexSlows, Mage::HasSlowOut). Every other event casts nothing.
    ProcOutcome NetherVortexProc(OnProcContext& ctx)
    {
        if (!Mage::ProcEffectIs(ctx, SPELL_AURA_DUMMY))
        {
            return ProcOutcome::Continue();
        }
        uint32 const procSpellId = ctx.procSpell ? ctx.procSpell->ID : 0;
        if (!ctx.victim || ctx.victim == &ctx.caster ||
            !Mage::NetherVortexSlows(procSpellId, ctx.procEx, ctx.damage, Mage::HasSlowOut(ctx.caster)))
        {
            return ProcOutcome::Return(SPELL_AURA_PROC_CANT_TRIGGER);
        }
        return ProcOutcome::Return(Mage::CastProcSpell(ctx, *ctx.victim, SLOW));
    }

    /// 54646: Focus Magic: its proc-trigger effect (1, Focus Magic's buff 54648) casts nothing. Cast by the unit Focus
    /// Magic is on, the buff, whose one effect is at its caster, would land on that unit; the mage's own cast of it at
    /// himself comes from the crit-chance effect's proc (FocusMagicBuffCaster), so effect 0 goes on to it.
    ProcOutcome FocusMagicProc(OnProcContext& ctx)
    {
        if (!Mage::ProcEffectIs(ctx, SPELL_AURA_PROC_TRIGGER_SPELL))
        {
            return ProcOutcome::Continue();
        }
        return ProcOutcome::Return(SPELL_AURA_PROC_CANT_TRIGGER);
    }

    uint32 const COUNTERSPELL = 2139;

    /// 84722, 84723: Invocation: answers true for a hit of the mage's Counterspell that interrupted a spell its target
    /// was casting (Mage::InvocationGrants), so the engine's proc casts the buff (87098) at the rank's value though the
    /// hit dealt no damage; every other event is dropped.
    SpellHandlerOutcome<bool> InvocationCheckProc(CheckProcContext& ctx)
    {
        uint32 const procSpellId = ctx.procSpell ? ctx.procSpell->ID : 0;
        return SpellHandlerOutcome<bool>::Return(Mage::InvocationGrants(procSpellId, ctx.procEx));
    }

    uint32 const IMPROVED_POLYMORPH_RANK_ONE = 11210;
    uint32 const IMPROVED_POLYMORPH_RANK_TWO = 12592;
    uint32 const IMPROVED_POLYMORPH_STUN_RANK_ONE = 83046;
    uint32 const IMPROVED_POLYMORPH_STUN_RANK_TWO = 83047;
    uint32 const IMPROVED_POLYMORPH_MARKER = 87515;

    /// The aura types of a damage over time effect: periodic damage, a periodic health leech, and periodic damage of a
    /// percent of the target's maximum health.
    AuraType const DAMAGE_OVER_TIME_AURA_TYPES[] =
    {
        SPELL_AURA_PERIODIC_DAMAGE, SPELL_AURA_PERIODIC_LEECH, SPELL_AURA_PERIODIC_DAMAGE_PERCENT
    };

    /// 118, 28271, 28272, 61025, 61305, 61721, 61780: Polymorph, each form a mage casts: when damage broke it and its
    /// caster knows Improved Polymorph, the target casts the rank's stun and Improved Polymorph Marker (87515) at
    /// itself for the Polymorph's caster, unless it holds the marker (Mage::ImprovedPolymorphStun). The removal's
    /// boosts run.
    SpellHandlerOutcome<void> PolymorphRemove(AuraRemoveContext& ctx)
    {
        bool const knowsRankOne = ctx.caster && ctx.caster->HasAura(IMPROVED_POLYMORPH_RANK_ONE);
        bool const knowsRankTwo = ctx.caster && ctx.caster->HasAura(IMPROVED_POLYMORPH_RANK_TWO);
        uint32 const stun = Mage::ImprovedPolymorphStun(ctx.mode, knowsRankOne, knowsRankTwo,
                                                        ctx.target.HasAura(IMPROVED_POLYMORPH_MARKER));
        if (stun)
        {
            ctx.CastTriggered(ctx.target, stun);
            ctx.CastTriggered(ctx.target, IMPROVED_POLYMORPH_MARKER);
        }
        return SpellHandlerOutcome<void>::Continue();
    }

    uint32 const GLYPH_OF_POLYMORPH = 56375;

    /// 118, 28271, 28272, 61025, 61305, 61721, 61780: Polymorph, each form a mage casts: as it lands, a caster who
    /// holds Glyph of Polymorph (56375) takes every damage over time aura off the target, whoever cast it
    /// (Mage::GlyphOfPolymorphRemoves). Nothing at the call before the add, and nothing for a caster without the
    /// glyph. The holder's boosts run.
    SpellHandlerOutcome<void> PolymorphApply(AuraApplyContext& ctx)
    {
        if (!ctx.real)
        {
            return SpellHandlerOutcome<void>::Continue();
        }
        bool const holdsGlyph = ctx.caster && ctx.caster->HasAura(GLYPH_OF_POLYMORPH);
        for (AuraType type : DAMAGE_OVER_TIME_AURA_TYPES)
        {
            if (Mage::GlyphOfPolymorphRemoves(type, holdsGlyph))
            {
                ctx.target.RemoveSpellsCausingAura(type);
            }
        }
        return SpellHandlerOutcome<void>::Continue();
    }

    /// The aura types whose negative amount slows the unit they are on, for Torment the Weak: its movement, its
    /// attacks or its casts.
    AuraType const SLOW_AURA_TYPES[] =
    {
        SPELL_AURA_MOD_DECREASE_SPEED,
        SPELL_AURA_MOD_ATTACKSPEED, SPELL_AURA_MOD_ATTACKSPEED_2, SPELL_AURA_MOD_MELEE_HASTE,
        SPELL_AURA_MOD_RANGED_HASTE, SPELL_AURA_MOD_MELEE_RANGED_HASTE, SPELL_AURA_HASTE_RANGED,
        SPELL_AURA_MOD_MELEE_ATTACK_SPEED, SPELL_AURA_MOD_RANGED_ATTACK_SPEED,
        SPELL_AURA_MOD_CASTING_SPEED_NOT_STACK, SPELL_AURA_HASTE_SPELLS,
        SPELL_AURA_HASTE_ALL, SPELL_AURA_SLOW_ALL
    };

    uint32 const ARCANE_POTENCY[] = { 31571, 31572 };
    uint32 const MANA_SHIELD[] = { 1463 };
    uint32 const REPLENISH_MANA[] = { 5405 };
    uint32 const ARCANE_MISSILES_BUFFS[] = { ARCANE_MISSILES_BUFF };
    uint32 const ARCANE_MISSILES_CHANNEL[] = { 5143 };
    uint32 const ARCANE_BLAST_BUFF[] = { 36032 };
    uint32 const ARCANE_POTENCY_BUFFS[] = { 57529, 57531 };
    uint32 const NETHER_VORTEX[] = { 86181, 86209 };
    uint32 const FOCUS_MAGIC[] = { 54646 };
    uint32 const INVOCATION[] = { 84722, 84723 };
    uint32 const POLYMORPH[] = { 118, 28271, 28272, 61025, 61305, 61721, 61780 };
}

Mage::AurastateStep Mage::ArcaneMissilesAurastateStep(bool holdsBuff, bool holdsAurastate)
{
    if (holdsBuff == holdsAurastate)
    {
        return AURASTATE_KEEP;
    }
    return holdsBuff ? AURASTATE_APPLY : AURASTATE_REMOVE;
}

bool Mage::ArcaneBlastBuffEnds(uint32 spellId, uint32 schoolMask, bool dealsDamage, bool castEnd, bool ownCast)
{
    return castEnd && ownCast && dealsDamage && (schoolMask & SPELL_SCHOOL_MASK_ARCANE) != 0 &&
           spellId != ARCANE_BLAST && spellId != ARCANE_EXPLOSION;
}

bool Mage::DealsOrTriggersDamage(SpellEntry const& spell)
{
    if (Spells::DealsDamage(spell))
    {
        return true;
    }
    for (uint32 i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        SpellEffectEntry const* const effect = spell.GetSpellEffect(SpellEffectIndex(i));
        if (!effect || !effect->EffectTriggerSpell)
        {
            continue;
        }
        SpellEntry const* const triggered = sSpellStore.LookupEntry(effect->EffectTriggerSpell);
        if (triggered && Spells::DealsDamage(*triggered))
        {
            return true;
        }
    }
    return false;
}

bool Mage::CountsAsSlow(uint32 auraType, int32 amount)
{
    if (amount >= 0)
    {
        return false;
    }
    for (AuraType type : SLOW_AURA_TYPES)
    {
        if (auraType == uint32(type))
        {
            return true;
        }
    }
    return false;
}

bool Mage::IsSnaredOrSlowed(Unit const& unit)
{
    for (AuraType type : SLOW_AURA_TYPES)
    {
        if (CountsAsSlow(type, unit.GetMaxNegativeAuraModifier(type)))
        {
            return true;
        }
    }
    return false;
}

float Mage::TormentTheWeakMultiplier(int32 percent, uint32 spellFamily, uint32 schoolMask, bool targetSlowed)
{
    if (percent <= 0 || !targetSlowed || spellFamily != SPELLFAMILY_MAGE ||
        (schoolMask & SPELL_SCHOOL_MASK_ARCANE) == 0)
    {
        return 1.0f;
    }
    return (100.0f + percent) / 100.0f;
}

bool Mage::HasSlowOut(Unit const& mage)
{
    Unit::TrackedAuraTargetMap const& tracked = mage.GetTrackedAuraTargets(TRACK_AURA_TYPE_SINGLE_TARGET);
    for (Unit::TrackedAuraTargetMap::const_iterator i = tracked.begin(); i != tracked.end(); ++i)
    {
        if (i->first && i->first->ID == SLOW)
        {
            return true;
        }
    }
    return false;
}

bool Mage::NetherVortexSlows(uint32 procSpellId, uint32 procEx, uint32 damage, bool slowOut)
{
    return procSpellId == ARCANE_BLAST && (procEx & (PROC_EX_NORMAL_HIT | PROC_EX_CRITICAL_HIT)) != 0 && damage > 0 &&
           !slowOut;
}

bool Mage::InvocationGrants(uint32 procSpellId, uint32 procEx)
{
    return procSpellId == COUNTERSPELL && SpellMgr::InterruptedACast(procEx);
}

uint32 Mage::ImprovedPolymorphStun(AuraRemoveMode mode, bool knowsRankOne, bool knowsRankTwo, bool targetMarked)
{
    if (mode != AURA_REMOVE_BY_DAMAGE || targetMarked)
    {
        return 0;
    }
    if (knowsRankTwo)
    {
        return IMPROVED_POLYMORPH_STUN_RANK_TWO;
    }
    return knowsRankOne ? IMPROVED_POLYMORPH_STUN_RANK_ONE : 0;
}

bool Mage::GlyphOfPolymorphRemoves(uint32 auraType, bool casterHoldsGlyph)
{
    if (!casterHoldsGlyph)
    {
        return false;
    }
    for (AuraType type : DAMAGE_OVER_TIME_AURA_TYPES)
    {
        if (auraType == uint32(type))
        {
            return true;
        }
    }
    return false;
}

bool Mage::IncantersAbsorptionKnocksBack(AuraRemoveMode mode, bool knowsTalent)
{
    return mode == AURA_REMOVE_BY_SHIELD_BREAK && knowsTalent;
}

int32 Mage::ImprovedManaGemAmount(uint32 maxMana, int32 percent)
{
    if (percent <= 0)
    {
        return 0;
    }
    return int32(uint64(maxMana) * uint64(percent) / 100);
}

bool Mage::IsManaGem(uint32 itemEntry)
{
    return std::find(std::begin(MANA_GEMS), std::end(MANA_GEMS), itemEntry) != std::end(MANA_GEMS);
}

uint32 Mage::RegisterArcane(SpellHandlerRegistry& registry)
{
    uint32 rows = RegisterRanks<OnProcHook>(registry, ARCANE_POTENCY, &ArcanePotencyProc);
    rows += RegisterRanks<OnProcHook>(registry, MANA_SHIELD, &ManaShieldProc);
    rows += RegisterRanks<AuraRemoveHook>(registry, MANA_SHIELD, &ManaShieldAuraRemove);
    rows += RegisterRanks<AfterCastHook>(registry, REPLENISH_MANA, &ReplenishManaAfterCast);
    rows += RegisterRanks<AuraApplyHook>(registry, ARCANE_MISSILES_BUFFS, &ArcaneMissilesBuffApply);
    rows += RegisterRanks<AuraRemoveHook>(registry, ARCANE_MISSILES_BUFFS, &ArcaneMissilesBuffRemove);
    rows += RegisterRanks<AuraApplyHook>(registry, ARCANE_MISSILES_CHANNEL, &ArcaneMissilesChannelApply);
    rows += RegisterRanks<CheckProcHook>(registry, ARCANE_BLAST_BUFF, &ArcaneBlastBuffCheckProc);
    rows += RegisterRanks<CheckProcHook>(registry, ARCANE_POTENCY_BUFFS, &ArcanePotencyBuffCheckProc);
    rows += RegisterRanks<DamageDoneHook>(registry, TORMENT_THE_WEAK, &TormentTheWeakDamageDone);
    rows += RegisterRanks<OnProcHook>(registry, NETHER_VORTEX, &NetherVortexProc);
    rows += RegisterRanks<OnProcHook>(registry, FOCUS_MAGIC, &FocusMagicProc);
    rows += RegisterRanks<CheckProcHook>(registry, INVOCATION, &InvocationCheckProc);
    rows += RegisterRanks<AuraRemoveHook>(registry, POLYMORPH, &PolymorphRemove);
    rows += RegisterRanks<AuraApplyHook>(registry, POLYMORPH, &PolymorphApply);
    return rows;
}
