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

#include "spells/classes/priest/shadow/ShadowWordDeath.h"
#include "entities/Unit.h"
#include "DBCStores.h"

#include <cstddef>

namespace
{
    /// Shadow Word: Death.
    uint32 const SHADOW_WORD_DEATH = 32379;

    /// Shadow Word: Death's 4.3.4 text, "Deals three times as much damage to targets below 25% health.": the
    /// percent of a target's health a hit must be below and the number of times the hit is dealt there. Neither is a
    /// field of the client's data, where the spell's effect holds the hit's own damage alone.
    uint32 const SHADOW_WORD_DEATH_BELOW_PERCENT = 25;
    float const SHADOW_WORD_DEATH_TIMES = 3.0f;

    /// Mind Melt's two ranks: effect 0, a dummy aura, holds the percent a hit of Shadow Word: Death is raised by; the
    /// first rank's effect 2 holds the percent of a target's health the hit must be at or below (25), the text of
    /// both ranks reading it there.
    uint32 const MIND_MELT_RANK_1 = 14910;
    uint32 const MIND_MELT_RANK_2 = 33371;
    uint32 const MIND_MELT_ROW_1[] = { MIND_MELT_RANK_1 };
    uint32 const MIND_MELT_ROW_2[] = { MIND_MELT_RANK_2 };

    /// Pain and Suffering's two ranks: effect 1 holds the percent taken off the priest's own Shadow Word: Death.
    uint32 const PAIN_AND_SUFFERING_RANKS[] = { 47580, 47581 };

    /// Glyph of Shadow Word: Death, whose effect 0 holds the percent of the target's health the hit starts at or
    /// below to reset the cooldown (25), and the lockout it casts, which holds for 6 s.
    uint32 const GLYPH_OF_SHADOW_WORD_DEATH = 55682;
    uint32 const GLYPH_LOCKOUT = 95652;

    /// Glyph of Spirit Tap and the mana regeneration it casts: 3% of his mana every 3 s for 12 s.
    uint32 const GLYPH_OF_SPIRIT_TAP = 63237;
    uint32 const SPIRIT_TAP = 81301;

    /// The ids each row is registered under.
    uint32 const SHADOW_WORD_DEATH_ROW[] = { SHADOW_WORD_DEATH };

    /// The effect `index` of `spellId` in the client's data; 0 when the store lacks the spell.
    int32 DataValue(uint32 spellId, SpellEffectIndex index)
    {
        SpellEntry const* spell = sSpellStore.LookupEntry(spellId);
        return spell ? spell->CalculateSimpleValue(index) : 0;
    }

    /// 32379: Shadow Word: Death: a direct hit at a target below the text's percent deals the text's three times
    /// (Priest::ShadowWordDeathMultiplier). The done multiplier is applied to the hit's base and its spell power share
    /// alike (Unit::SpellDamageBonusDone), so the whole hit is tripled.
    SpellHandlerOutcome<void> ShadowWordDeathDamageDone(DamageDoneContext& ctx)
    {
        ctx.doneTotalMod *= Priest::ShadowWordDeathMultiplier(ctx.damageType, ctx.victim.GetHealth(),
                                                              ctx.victim.GetMaxHealth());
        return SpellHandlerOutcome<void>::Continue();
    }

    /// Mind Melt's row of the rank `Rank`: its first effect is a dummy aura, so the row runs for every spell its
    /// holder deals; a direct hit of Shadow Word: Death at a target at or below the first rank's effect 2 percent
    /// takes 1 + the rank's effect 0 / 100 as a second factor (Priest::MindMeltMultiplier).
    template <uint32 Rank>
    SpellHandlerOutcome<void> MindMeltDamageDone(DamageDoneContext& ctx)
    {
        ctx.doneTotalMod *= Priest::MindMeltMultiplier(ctx.spell.ID, ctx.damageType, ctx.victim.GetHealth(),
                                                       ctx.victim.GetMaxHealth(),
                                                       DataValue(MIND_MELT_RANK_1, EFFECT_INDEX_2),
                                                       DataValue(Rank, EFFECT_INDEX_0));
        return SpellHandlerOutcome<void>::Continue();
    }

    /// 32379: Shadow Word: Death: after a hit that left the target alive, the recoil is cast at the priest, triggered
    /// by the hit, with the whole hit less Pain and Suffering's percent as its base points, and Glyph of Shadow Word:
    /// Death, which he holds, ends the cooldown of the spell and its category, with his client told, and casts its
    /// lockout, when the target was at or below the glyph's percent before the hit and he holds no lockout. After a
    /// hit that killed the target no recoil is cast; Glyph of Spirit Tap, which he holds, has him cast Spirit Tap at
    /// himself when the kill yields experience or honor.
    SpellHandlerOutcome<void> ShadowWordDeathAfterHit(AfterHitContext& ctx)
    {
        if (ctx.heal)
        {
            return SpellHandlerOutcome<void>::Continue();
        }

        Unit& priest = ctx.caster;
        bool const targetLives = ctx.target.IsAlive();
        if (targetLives)
        {
            int32 const recoil = Priest::ShadowWordDeathRecoil(ctx.amount, ctx.absorbed,
                                                               Priest::PainAndSufferingPercent(priest));
            if (recoil > 0)
            {
                int32 const basePoints[MAX_EFFECT_INDEX] = { recoil, 0, 0 };
                ctx.CastTriggered(priest, Priest::SHADOW_WORD_DEATH_RECOIL, basePoints);
            }
        }

        // the target's health before the hit: what it has now and the damage the hit dealt it
        uint64 const healthBeforeHit = uint64(ctx.target.GetHealth()) + ctx.amount;
        if (priest.HasAura(GLYPH_OF_SHADOW_WORD_DEATH) &&
            Priest::GlyphResetsShadowWordDeath(targetLives, healthBeforeHit, ctx.target.GetMaxHealth(),
                                               DataValue(GLYPH_OF_SHADOW_WORD_DEATH, EFFECT_INDEX_0),
                                               priest.HasAura(GLYPH_LOCKOUT)))
        {
            priest.RemoveSpellCooldown(ctx.spell.ID, true);
            priest.RemoveSpellCategoryCooldown(ctx.spell.GetCategory(), true);
            ctx.CastTriggered(priest, GLYPH_LOCKOUT);
        }

        // only a player has a reward to give; a unit that is not one has no Glyph of Spirit Tap
        bool const yieldsReward = priest.GetTypeId() == TYPEID_PLAYER &&
                                  Priest::YieldsExperienceOrHonor(priest.getLevel(), ctx.target.getLevel(),
                                                                  ctx.target.GrantsXPAtKill());
        if (Priest::SpiritTapGives(!targetLives, priest.HasAura(GLYPH_OF_SPIRIT_TAP), yieldsReward))
        {
            ctx.CastTriggered(priest, SPIRIT_TAP);
        }
        return SpellHandlerOutcome<void>::Continue();
    }
}

uint32 Priest::RegisterShadowWordDeath(SpellHandlerRegistry& registry)
{
    uint32 rows = RegisterRanks<DamageDoneHook>(registry, SHADOW_WORD_DEATH_ROW, &ShadowWordDeathDamageDone);
    rows += RegisterRanks<AfterHitHook>(registry, SHADOW_WORD_DEATH_ROW, &ShadowWordDeathAfterHit);
    rows += RegisterRanks<DamageDoneHook>(registry, MIND_MELT_ROW_1, &MindMeltDamageDone<MIND_MELT_RANK_1>);
    rows += RegisterRanks<DamageDoneHook>(registry, MIND_MELT_ROW_2, &MindMeltDamageDone<MIND_MELT_RANK_2>);
    return rows;
}

float Priest::ShadowWordDeathMultiplier(DamageEffectType damageType, uint32 health, uint32 maxHealth)
{
    if (damageType != SPELL_DIRECT_DAMAGE || !maxHealth ||
        uint64(health) * 100 >= uint64(maxHealth) * SHADOW_WORD_DEATH_BELOW_PERCENT)
    {
        return 1.0f;
    }
    return SHADOW_WORD_DEATH_TIMES;
}

float Priest::MindMeltMultiplier(uint32 spellId, DamageEffectType damageType, uint32 health, uint32 maxHealth,
                                 int32 threshold, int32 percent)
{
    if (spellId != SHADOW_WORD_DEATH || damageType != SPELL_DIRECT_DAMAGE || !maxHealth || threshold <= 0 ||
        percent <= 0 || uint64(health) * 100 > uint64(maxHealth) * uint32(threshold))
    {
        return 1.0f;
    }
    return 1.0f + float(percent) / 100.0f;
}

int32 Priest::PainAndSufferingPercent(Unit const& priest)
{
    for (std::size_t i = sizeof(PAIN_AND_SUFFERING_RANKS) / sizeof(PAIN_AND_SUFFERING_RANKS[0]); i-- > 0;)
    {
        if (priest.HasAura(PAIN_AND_SUFFERING_RANKS[i]))
        {
            return DataValue(PAIN_AND_SUFFERING_RANKS[i], EFFECT_INDEX_1);
        }
    }
    return 0;
}

int32 Priest::ShadowWordDeathRecoil(uint32 amount, uint32 absorbed, int32 percent)
{
    if (percent <= -100)
    {
        return 0;
    }
    uint64 const whole = uint64(amount) + absorbed;
    uint64 const recoil = whole * uint32(100 + percent) / 100;
    return recoil > uint64(INT32_MAX) ? INT32_MAX : int32(recoil);
}

bool Priest::GlyphResetsShadowWordDeath(bool targetLives, uint64 healthBeforeHit, uint32 maxHealth, int32 percent,
                                        bool lockoutHeld)
{
    return targetLives && !lockoutHeld && maxHealth && percent > 0 &&
           healthBeforeHit * 100 <= uint64(maxHealth) * uint32(percent);
}

uint32 Priest::GrayLevelOf(uint32 level)
{
    if (level <= 5)
    {
        return 0;
    }
    if (level <= 39)
    {
        return level - 5 - level / 10;
    }
    if (level <= 59)
    {
        return level - 1 - level / 5;
    }
    return level - 9;
}

bool Priest::YieldsExperienceOrHonor(uint32 killerLevel, uint32 victimLevel, bool victimGrantsExperience)
{
    return victimLevel > GrayLevelOf(killerLevel) && victimGrantsExperience;
}

bool Priest::SpiritTapGives(bool targetDied, bool glyphHeld, bool yieldsReward)
{
    return targetDied && glyphHeld && yieldsReward;
}
