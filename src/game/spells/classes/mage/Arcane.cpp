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

namespace
{
    typedef SpellHandlerOutcome<SpellAuraProcResult> ProcOutcome;

    /// 29441, 29444: Magic Absorption: a mana user gets back the aura's percent of his maximum mana through 29442.
    ProcOutcome MagicAbsorptionProc(OnProcContext& ctx)
    {
        if (!Mage::ProcEffectIs(ctx, SPELL_AURA_DUMMY))
        {
            return ProcOutcome::Continue();
        }
        if (ctx.caster.GetPowerType() != POWER_MANA)
        {
            return ProcOutcome::Return(SPELL_AURA_PROC_FAILED);
        }

        int32 const basePoints[MAX_EFFECT_INDEX] = { int32(ctx.triggerAmount * ctx.caster.GetMaxPower(POWER_MANA) / 100), 0, 0 };
        return ProcOutcome::Return(Mage::CastProcSpell(ctx, ctx.caster, 29442, basePoints));
    }

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
    /// (5143) asks for on its caster.
    SpellHandlerOutcome<void> ArcaneMissilesBuffApply(AuraApplyContext& ctx)
    {
        PairArcaneMissilesAurastate(ctx);
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
    /// (79683) off him: the cast uses it up. The channel's holder on its target does nothing here.
    SpellHandlerOutcome<void> ArcaneMissilesChannelApply(AuraApplyContext& ctx)
    {
        if (ctx.caster == &ctx.target)
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

    uint32 const MAGIC_ABSORPTION[] = { 29441, 29444 };
    uint32 const ARCANE_POTENCY[] = { 31571, 31572 };
    uint32 const MANA_SHIELD[] = { 1463 };
    uint32 const ARCANE_MISSILES_BUFFS[] = { ARCANE_MISSILES_BUFF };
    uint32 const ARCANE_MISSILES_CHANNEL[] = { 5143 };
    uint32 const ARCANE_BLAST_BUFF[] = { 36032 };
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

uint32 Mage::RegisterArcane(SpellHandlerRegistry& registry)
{
    uint32 rows = RegisterRanks<OnProcHook>(registry, MAGIC_ABSORPTION, &MagicAbsorptionProc);
    rows += RegisterRanks<OnProcHook>(registry, ARCANE_POTENCY, &ArcanePotencyProc);
    rows += RegisterRanks<OnProcHook>(registry, MANA_SHIELD, &ManaShieldProc);
    rows += RegisterRanks<AuraApplyHook>(registry, ARCANE_MISSILES_BUFFS, &ArcaneMissilesBuffApply);
    rows += RegisterRanks<AuraRemoveHook>(registry, ARCANE_MISSILES_BUFFS, &ArcaneMissilesBuffRemove);
    rows += RegisterRanks<AuraApplyHook>(registry, ARCANE_MISSILES_CHANNEL, &ArcaneMissilesChannelApply);
    rows += RegisterRanks<CheckProcHook>(registry, ARCANE_BLAST_BUFF, &ArcaneBlastBuffCheckProc);
    return rows;
}
