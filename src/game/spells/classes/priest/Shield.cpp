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

#include "spells/classes/priest/Shield.h"
#include "spells/data/SpellMgr.h"
#include "entities/Unit.h"

#include <cstddef>

namespace
{
    uint32 const POWER_WORD_SHIELD = 17;
    uint32 const SHIELD_DISCIPLINE = 77484;
    uint32 const RAPTURE_ENERGIZE = 47755;

    /// The id the shield's scripts hold at the aura-amount and aura-remove hooks.
    uint32 const SHIELD_IDS[] = { POWER_WORD_SHIELD };

    /// Improved Power Word: Shield's ranks, Twin Disciplines' and Rapture's, the highest first: a priest knows one rank
    /// of each.
    uint32 const IMPROVED_POWER_WORD_SHIELD[] = { 14768, 14748 };
    uint32 const TWIN_DISCIPLINES[] = { 47588, 47587, 47586 };
    uint32 const RAPTURE[] = { 47537, 47536, 47535 };

    /// The first of `ranks` that `unit` holds; 0 when he holds none.
    template <std::size_t N>
    uint32 HeldRank(Unit const& unit, uint32 const (&ranks)[N])
    {
        for (uint32 rank : ranks)
        {
            if (unit.GetSpellAuraHolder(rank))
            {
                return rank;
            }
        }
        return 0;
    }

    /// The effect `eff` amount of `unit`'s holder of `spellId`; 0 when he holds none.
    int32 HeldAmount(Unit const& unit, uint32 spellId, SpellEffectIndex eff)
    {
        SpellAuraHolder* holder = spellId ? unit.GetSpellAuraHolder(spellId) : NULL;
        return holder ? AuraHandle(*holder).Amount(eff) : 0;
    }

    /// 17: Power Word: Shield: when its absorb is made, the amount is the whole shield: the base the engine reckoned,
    /// plus the spell power part (the caster's spell power of the shield's school times the coefficient the absorb
    /// kind takes), raised by the effect 0 percent of the Improved Power Word: Shield rank the caster holds, by the
    /// effect 1 percent of his Twin Disciplines rank (its healing done) and by his Shield Discipline mastery
    /// (Priest::ShieldAmount, Priest::AbsorbMastery), as the client's own tooltip reckons it. The absorb aura adds no
    /// spell power part at its apply for a spell with this row (Aura::HandleSchoolAbsorb), so the holder's slot update,
    /// sent before the apply, carries the whole amount, and its boosts, Glyph of Power Word: Shield's heal among them,
    /// read it. A shield made while its target loads, or with no caster, keeps the amount the engine reckoned.
    SpellHandlerOutcome<void> ShieldAuraAmount(AuraAmountContext& ctx)
    {
        if (!ctx.caster || ctx.target.IsLoading() || ctx.effIndex != EFFECT_INDEX_0)
        {
            return SpellHandlerOutcome<void>::Continue();
        }
        int32 const spellPower = ctx.caster->SpellBaseDamageBonusDone(GetSpellSchoolMask(&ctx.spell));
        int32 const part = ctx.caster->SpellBonusWithCoeffs(&ctx.spell, 0, spellPower, 0, SPELL_DIRECT_DAMAGE, true,
                                                            SPELL_BONUS_KIND_ABSORB);
        int32 const improvedPercent = HeldAmount(*ctx.caster, HeldRank(*ctx.caster, IMPROVED_POWER_WORD_SHIELD),
                                                 EFFECT_INDEX_0);
        int32 const twinPercent = HeldAmount(*ctx.caster, HeldRank(*ctx.caster, TWIN_DISCIPLINES), EFFECT_INDEX_1);
        ctx.amount = Priest::ShieldAmount(ctx.amount + part, improvedPercent, twinPercent,
                                          Priest::AbsorbMastery(*ctx.caster));
        return SpellHandlerOutcome<void>::Continue();
    }

    /// 17: Power Word: Shield: when damage used up its last point or it was dispelled (Priest::RaptureFires), a caster
    /// who knows Rapture casts at himself the energize (47755) for the effect 0 percent of his rank of his maximum mana
    /// (Priest::RaptureMana), triggered by his rank's effect 0 aura. The lockout is the energize's own recovery time
    /// (12 s in the client's data), which its cast keeps in his cooldown store, saved with him: inside it the store
    /// refuses the cast, and a cast triggered by an aura is refused as SPELL_FAILED_DONT_REPORT, the one result the
    /// client is told. The shield may be on any unit: the mana and the lockout are the caster's. Any other removal, a
    /// caster who cannot be found or knows no Rapture, and a rank that holds no percent cast nothing. The removal's
    /// boosts still run.
    SpellHandlerOutcome<void> ShieldAuraRemove(AuraRemoveContext& ctx)
    {
        if (!ctx.caster || !Priest::RaptureFires(ctx.mode))
        {
            return SpellHandlerOutcome<void>::Continue();
        }
        uint32 const rank = HeldRank(*ctx.caster, RAPTURE);
        Aura* talent = rank ? ctx.caster->GetAura(rank, EFFECT_INDEX_0) : NULL;
        int32 const mana = Priest::RaptureMana(ctx.caster->GetMaxPower(POWER_MANA),
                                               HeldAmount(*ctx.caster, rank, EFFECT_INDEX_0));
        if (!talent || mana <= 0)
        {
            return SpellHandlerOutcome<void>::Continue();
        }
        int32 const basePoints[MAX_EFFECT_INDEX] = { mana, 0, 0 };
        ctx.caster->CastCustomSpell(ctx.caster, RAPTURE_ENERGIZE, &basePoints[EFFECT_INDEX_0], NULL, NULL, true, NULL,
                                    talent);
        return SpellHandlerOutcome<void>::Continue();
    }
}

uint32 Priest::RegisterShield(SpellHandlerRegistry& registry)
{
    uint32 rows = RegisterRanks<AuraAmountHook>(registry, SHIELD_IDS, &ShieldAuraAmount);
    rows += RegisterRanks<AuraRemoveHook>(registry, SHIELD_IDS, &ShieldAuraRemove);
    return rows;
}

int32 Priest::AbsorbMastery(Unit const& caster)
{
    SpellAuraHolder* holder = caster.GetSpellAuraHolder(SHIELD_DISCIPLINE);
    return holder ? AuraHandle(*holder).Amount(EFFECT_INDEX_0) : 0;
}

int32 Priest::ShieldAmount(int32 amount, int32 improvedPercent, int32 twinPercent, int32 masteryPercent)
{
    if (amount <= 0)
    {
        return amount;
    }
    int64 const improved = 100 + (improvedPercent > 0 ? improvedPercent : 0);
    int64 const twin = 100 + (twinPercent > 0 ? twinPercent : 0);
    int64 const mastery = 100 + (masteryPercent > 0 ? masteryPercent : 0);
    return int32(int64(amount) * improved * twin * mastery / 1000000);
}

bool Priest::RaptureFires(AuraRemoveMode mode)
{
    return mode == AURA_REMOVE_BY_SHIELD_BREAK || mode == AURA_REMOVE_BY_DISPEL;
}

int32 Priest::RaptureMana(uint32 maxMana, int32 percent)
{
    if (percent <= 0)
    {
        return 0;
    }
    return int32(uint64(maxMana) * uint64(percent) / 100);
}
