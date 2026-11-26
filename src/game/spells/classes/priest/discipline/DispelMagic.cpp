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

#include "spells/classes/priest/discipline/DispelMagic.h"
#include "entities/Unit.h"
#include "DBCStores.h"

#include <cstdint>

namespace
{
    uint32 const DISPEL_MAGIC_SPELL[] = { 527 };
    uint32 const DISPEL_MAGIC_FRIENDLY = 97690;
    uint32 const DISPEL_MAGIC_HOSTILE = 97691;
    uint32 const ABSOLUTION = 33167;
    uint32 const GLYPH_OF_DISPEL_MAGIC = 55677;
    uint32 const GLYPH_OF_DISPEL_MAGIC_HEAL = 56131;

    /// Whether `target` is `priest` himself or a unit friendly to him.
    bool SelfOrFriendly(Unit const& priest, Unit const& target)
    {
        return &target == &priest || priest.IsFriendlyTo(&target);
    }

    /// 527: Dispel Magic: the priest may cast it at himself and at any unit not friendly to him, and at another
    /// friendly unit only while he holds Absolution (33167), which the Discipline and Holy trees teach; a cast at such
    /// a unit without it is refused, SPELL_FAILED_BAD_TARGETS (Priest::DispelMagicRefused). A pure check of the cast's
    /// unit target; nothing for a cast with none.
    SpellHandlerOutcome<SpellCastResult> DispelMagicCheck(CheckCastContext& ctx)
    {
        if (ctx.target && Priest::DispelMagicRefused(ctx.target == &ctx.caster, ctx.caster.IsFriendlyTo(ctx.target),
                                                     ctx.caster.HasAura(ABSOLUTION)))
        {
            return SpellHandlerOutcome<SpellCastResult>::Return(SPELL_FAILED_BAD_TARGETS);
        }
        return SpellHandlerOutcome<SpellCastResult>::Continue();
    }

    /// 527: Dispel Magic's effect 0, a dummy: the priest casts at the unit it lands on, triggered, the dispel the
    /// client's data names for that unit (Priest::DispelMagicSpell), whose dispel log names it: 97690 at himself or a
    /// friendly unit, 97691 at any other. With Glyph of Dispel Magic (55677) on him, a 97690 that took at least one
    /// effect off the unit (DummyEffectContext::CastTriggeredDispel, Priest::GlyphOfDispelMagicHeals) is followed by
    /// 56131 at that unit, triggered, for the glyph's effect 0 value in the client's data (3) in percent of its maximum
    /// health (Priest::GlyphOfDispelMagicHeal); a dispel that took nothing off heals nothing. Return(): the effect is
    /// the script's.
    SpellHandlerOutcome<void> DispelMagicEffect(DummyEffectContext& ctx)
    {
        if (ctx.effIndex != EFFECT_INDEX_0 || !ctx.target)
        {
            return SpellHandlerOutcome<void>::Continue();
        }
        Unit& target = *ctx.target;
        uint32 const dispel = Priest::DispelMagicSpell(SelfOrFriendly(ctx.caster, target));
        uint32 const taken = ctx.CastTriggeredDispel(target, dispel);
        if (Priest::GlyphOfDispelMagicHeals(dispel, taken, ctx.caster.HasAura(GLYPH_OF_DISPEL_MAGIC)) &&
            target.IsAlive())
        {
            SpellEntry const* glyph = sSpellStore.LookupEntry(GLYPH_OF_DISPEL_MAGIC);
            int32 const heal = glyph ? Priest::GlyphOfDispelMagicHeal(target.GetMaxHealth(),
                                                                      glyph->CalculateSimpleValue(EFFECT_INDEX_0))
                                     : 0;
            if (heal > 0)
            {
                int32 const points[MAX_EFFECT_INDEX] = { heal, 0, 0 };
                ctx.CastTriggered(target, GLYPH_OF_DISPEL_MAGIC_HEAL, points);
            }
        }
        return SpellHandlerOutcome<void>::Return();
    }
}

uint32 Priest::RegisterDispelMagic(SpellHandlerRegistry& registry)
{
    uint32 rows = RegisterRanks<CheckCastHook>(registry, DISPEL_MAGIC_SPELL, &DispelMagicCheck);
    rows += RegisterRanks<DummyEffectHook>(registry, DISPEL_MAGIC_SPELL, &DispelMagicEffect);
    return rows;
}

bool Priest::DispelMagicRefused(bool self, bool friendly, bool absolution)
{
    return !self && friendly && !absolution;
}

uint32 Priest::DispelMagicSpell(bool friendly)
{
    return friendly ? DISPEL_MAGIC_FRIENDLY : DISPEL_MAGIC_HOSTILE;
}

bool Priest::GlyphOfDispelMagicHeals(uint32 dispelSpellId, uint32 taken, bool glyph)
{
    return glyph && taken > 0 && dispelSpellId == DISPEL_MAGIC_FRIENDLY;
}

int32 Priest::GlyphOfDispelMagicHeal(uint32 maxHealth, int32 percent)
{
    if (percent <= 0)
    {
        return 0;
    }
    uint64 const heal = uint64(maxHealth) * uint64(percent) / 100;
    return heal < uint64(INT32_MAX) ? int32(heal) : INT32_MAX;
}
