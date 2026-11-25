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

#include "spells/classes/priest/HealCrit.h"
#include "entities/Unit.h"
#include "DBCStores.h"

#include <cstddef>

namespace
{
    uint32 const FLASH_HEAL = 2061;
    uint32 const FLASH_HEAL_INSTANT = 101062;
    uint32 const GREATER_HEAL = 2060;
    uint32 const HEAL_SPELL = 2050;
    uint32 const PENANCE_HEAL = 47750;

    uint32 const WEAKENED_SOUL = 6788;
    uint32 const GLYPH_OF_FLASH_HEAL = 55679;

    /// Renewed Hope's two ranks, the first first.
    uint32 const RENEWED_HOPE_RANKS[] = { 57470, 57472 };

    /// Grace's two auras, one per rank of the talent that puts them on.
    uint32 const GRACE_AURAS[] = { 47930, 77613 };

    /// The heals Renewed Hope and Glyph of Flash Heal raise: those the class masks of their effect 0 in the client's
    /// data cover, and Flash Heal, which Renewed Hope's text names. The crit-chance row of each asks the masks again
    /// (Priest::HealCritBonus).
    uint32 const HEAL_CRIT_SPELLS[] = { FLASH_HEAL, FLASH_HEAL_INSTANT, GREATER_HEAL, HEAL_SPELL, PENANCE_HEAL };

    /// The value of effect `eff` of `spellId` in the client's data; 0 when the store lacks the spell or the effect.
    int32 EffectValue(uint32 spellId, SpellEffectIndex eff)
    {
        SpellEntry const* spell = spellId ? sSpellStore.LookupEntry(spellId) : NULL;
        return spell ? spell->CalculateSimpleValue(eff) : 0;
    }

    /// Whether the class mask of effect 0 of `spellId` in the client's data covers `spell`'s family flags, in the
    /// family of `spellId`; false when the store lacks it or its effect 0.
    bool EffectZeroCovers(uint32 spellId, SpellEntry const& spell)
    {
        SpellEntry const* source = sSpellStore.LookupEntry(spellId);
        SpellEffectEntry const* effect = source ? source->GetSpellEffect(EFFECT_INDEX_0) : NULL;
        return effect && spell.IsFitToFamily(source->GetSpellFamilyName(), effect->EffectSpellClassMask);
    }

    /// 2061, 101062, 2060, 2050, 47750: Flash Heal and its instant cast, Greater Heal, Heal and Penance's heal: their
    /// crit chance at the heal's target rises by what Renewed Hope and Glyph of Flash Heal add for the priest who
    /// casts it (Priest::HealCritBonus).
    SpellHandlerOutcome<void> HealCritChance(CritChanceContext& ctx)
    {
        ctx.critChance += float(Priest::HealCritBonus(ctx.caster, ctx.victim, ctx.spell));
        return SpellHandlerOutcome<void>::Continue();
    }
}

uint32 Priest::RegisterHealCrit(SpellHandlerRegistry& registry)
{
    return RegisterRanks<CritChanceHook>(registry, HEAL_CRIT_SPELLS, &HealCritChance);
}

uint32 Priest::RenewedHopeRank(Unit const& priest)
{
    for (std::size_t i = sizeof(RENEWED_HOPE_RANKS) / sizeof(RENEWED_HOPE_RANKS[0]); i > 0; --i)
    {
        if (priest.HasAura(RENEWED_HOPE_RANKS[i - 1]))
        {
            return RENEWED_HOPE_RANKS[i - 1];
        }
    }
    return 0;
}

bool Priest::RenewedHopeTakes(SpellEntry const& spell)
{
    if (spell.ID == FLASH_HEAL || spell.ID == FLASH_HEAL_INSTANT)
    {
        return true;
    }
    return EffectZeroCovers(RENEWED_HOPE_RANKS[0], spell);
}

bool Priest::FlashHealGlyphTakes(SpellEntry const& spell)
{
    return EffectZeroCovers(GLYPH_OF_FLASH_HEAL, spell);
}

bool Priest::RenewedHopeTarget(Unit const& priest, Unit const& target)
{
    if (target.HasAura(WEAKENED_SOUL))
    {
        return true;
    }
    for (uint32 grace : GRACE_AURAS)
    {
        if (target.GetSpellAuraHolder(grace, priest.GetObjectGuid()))
        {
            return true;
        }
    }
    return false;
}

bool Priest::AtOrUnderPercent(uint32 health, uint32 maxHealth, int32 percent)
{
    if (!maxHealth || percent <= 0)
    {
        return false;
    }
    return uint64(health) * 100 <= uint64(maxHealth) * uint64(percent);
}

int32 Priest::HealCritBonus(Unit const& priest, Unit const& target, SpellEntry const& spell)
{
    int32 bonus = 0;
    uint32 const rank = RenewedHopeRank(priest);
    if (rank && RenewedHopeTakes(spell) && RenewedHopeTarget(priest, target))
    {
        bonus += EffectValue(rank, EFFECT_INDEX_0);
    }
    if (priest.HasAura(GLYPH_OF_FLASH_HEAL) && FlashHealGlyphTakes(spell) &&
        AtOrUnderPercent(target.GetHealth(), target.GetMaxHealth(), EffectValue(GLYPH_OF_FLASH_HEAL, EFFECT_INDEX_1)))
    {
        bonus += EffectValue(GLYPH_OF_FLASH_HEAL, EFFECT_INDEX_0);
    }
    return bonus;
}
