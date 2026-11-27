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

#ifndef MANGOSSERVER_SPELLS_CLASSES_PRIEST_SHADOWWORDDEATH_H
#define MANGOSSERVER_SPELLS_CLASSES_PRIEST_SHADOWWORDDEATH_H

#include "spells/classes/priest/Priest.h"

/**
 * @file ShadowWordDeath.h
 * @brief Shadow Word: Death, the talents and glyphs that change it, and the damage it deals back: the leaves of
 * ShadowWordDeath.cpp.
 *
 * ShadowWordDeath.cpp registers five rows. The damage-done row of Shadow Word: Death (32379) multiplies a direct hit at
 * a target below its text's percent of its health by the text's three (ShadowWordDeathMultiplier), the done multiplier
 * the hit's spell power share is under too. The damage-done rows of Mind Melt's two ranks (14910, 33371), whose first
 * effect is a dummy aura, so a row runs for every spell the priest deals, multiply a direct hit of Shadow Word: Death
 * at a target at or below the first rank's effect 2 percent by 1 plus the rank's effect 0 percent (MindMeltMultiplier).
 * The after-hit row of Shadow Word: Death, when its target lives, casts the recoil (32409) at the priest, triggered by
 * the hit, with the whole hit, the absorbed part included, less Pain and Suffering's percent as its base points
 * (ShadowWordDeathRecoil, PainAndSufferingPercent); the recoil's one tick, a second later, takes the priest's own
 * damage-taken auras through the engine's periodic path. With Glyph of Shadow Word: Death (55682) it ends the cooldown
 * of the spell and of its category and casts the glyph's lockout (95652) when the target's health before the hit was at
 * or below the glyph's effect 0 percent and the lockout is not held (GlyphResetsShadowWordDeath). When the target died
 * and he holds Glyph of Spirit Tap (63237), a target that yields experience or honor (YieldsExperienceOrHonor) has him
 * cast Spirit Tap (81301) at himself. The damage-done row of the recoil (32409) sets its periodic amount's done
 * multiplier back to 1 (RecoilDoneMultiplier): the priest's own done modifiers do not raise the hit he takes back.
 */

namespace Priest
{
    /// Shadow Word: Death's recoil, which the priest takes from his own Shadow Word: Death: an aura whose one tick,
    /// a second after the cast, is the damage its base points name.
    uint32 const SHADOW_WORD_DEATH_RECOIL = 32409;

    /// Registers the rows of Shadow Word: Death and of Mind Melt on `registry`: the damage-done row and the after-hit
    /// row of 32379, the damage-done row of each of Mind Melt's ranks and of the recoil; answers 5, the rows registered.
    uint32 RegisterShadowWordDeath(SpellHandlerRegistry& registry);

    /// The recoil's done multiplier: 1 for its periodic amount, whatever the priest's done modifiers made it; a
    /// direct amount (none is dealt) keeps `doneMultiplier`.
    float RecoilDoneMultiplier(DamageEffectType damageType, float doneMultiplier);

    /// What the hit takes of Shadow Word: Death's own text, "Deals three times as much damage to targets below 25%
    /// health": 3 for a direct hit (`damageType` SPELL_DIRECT_DAMAGE) at a target with `health` of `maxHealth` health
    /// below 25% of it, 1 for a hit at 25% and above, for a periodic amount and for a target with no maximum.
    float ShadowWordDeathMultiplier(DamageEffectType damageType, uint32 health, uint32 maxHealth);

    /// What Mind Melt (the rank whose effect 0 is `percent`) adds to the hit of `spellId`: 1 plus `percent` / 100 for
    /// a direct hit of Shadow Word: Death at a target with `health` of `maxHealth` health at or below `threshold`
    /// percent of it (Mind Melt's first rank's effect 2, 25 in the client's data), 1 for every other spell, a periodic
    /// amount, a target above the threshold and a `percent` or `threshold` of 0 or less.
    float MindMeltMultiplier(uint32 spellId, DamageEffectType damageType, uint32 health, uint32 maxHealth,
                             int32 threshold, int32 percent);

    /// The percent Pain and Suffering takes off the Shadow Word: Death damage `priest` takes from his own: the effect 1
    /// value of the rank he holds in the client's data (-20 for the first rank, -40 for the second); 0 when he holds
    /// neither or the store lacks the rank.
    int32 PainAndSufferingPercent(Unit const& priest);

    /// The base points of the recoil: the whole hit, `amount` after the absorbs and `absorbed`, the part they took,
    /// times 100 plus `percent` (Pain and Suffering's, 0 or negative) over 100, the fraction dropped; 0 for a whole hit
    /// of 0 and for a percent of -100 or below.
    int32 ShadowWordDeathRecoil(uint32 amount, uint32 absorbed, int32 percent);

    /// Whether the hit of Shadow Word: Death that left its target alive (`targetLives`) resets the cooldown through
    /// Glyph of Shadow Word: Death: the target's health before the hit, `healthBeforeHit`, was at or below `percent`
    /// (the glyph's effect 0, 25) of `maxHealth` and the priest holds no lockout (`lockoutHeld`).
    bool GlyphResetsShadowWordDeath(bool targetLives, uint64 healthBeforeHit, uint32 maxHealth, int32 percent,
                                    bool lockoutHeld);

    /// The level at or below which a kill gives a killer of level `level` nothing: the engine's gray level
    /// (MaNGOS::XP::GetGrayLevel, 0 up to level 5, then level - 5 - level / 10 up to level 39, level - 1 - level / 5
    /// up to 59 and level - 9 above), whose header needs the world and is not a script's to include.
    uint32 GrayLevelOf(uint32 level);

    /// Whether the unit of level `killerLevel` gets experience or honor for a kill of a unit of level `victimLevel`
    /// that `victimGrantsExperience` (Unit::GrantsXPAtKill): the victim's level is above the killer's gray level.
    /// Player::isHonorOrXPTarget's rule, which Unit keeps protected.
    bool YieldsExperienceOrHonor(uint32 killerLevel, uint32 victimLevel, bool victimGrantsExperience);

    /// Whether the kill of a target gives Spirit Tap: the target died (`targetDied`), the priest holds Glyph of Spirit
    /// Tap (`glyphHeld`) and the kill yields experience or honor (`yieldsReward`).
    bool SpiritTapGives(bool targetDied, bool glyphHeld, bool yieldsReward);
}

#endif
