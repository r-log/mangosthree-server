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

#ifndef MANGOS_H_SPELLNOTICES
#define MANGOS_H_SPELLNOTICES

#include "ObjectGuid.h"
#include "Platform/Define.h"
#include "SharedDefines.h"

#include <vector>

class Player;
class Spell;
struct SpellEntry;

/// The heal a spell start predicts: its amount and its type (0 for a direct heal, DOT for a
/// periodic one).
struct SpellHealPrediction
{
    uint32 amount = 0;
    uint8 type = 0;
};

/// The targets a spell go reports: the hits (the units, then the objects, which always hit), then
/// the missed units, each with its condition and, for a reflect, the reflect's result.
struct SpellGoTargets
{
    struct Miss
    {
        ObjectGuid guid;
        uint8 condition = 0;
        uint8 reflectResult = 0;
    };

    std::vector<ObjectGuid> hits;
    std::vector<Miss> misses;
};

/**
 * What a cast tells the clients that see it, one call per notice; the units around the caster hear
 * the cast's notices, a player its own cast result and resurrection offer. The session installs the
 * notices that send them at start-up; this base sends nothing.
 */
class SpellNotices
{
    public:
        virtual ~SpellNotices();

        /// `caster` is told its cast `castCount` of `spellInfo` failed with `result`, for its pet's
        /// cast when `isPetCastResult` is true.
        virtual void CastResult(Player& caster, SpellEntry const* spellInfo, uint8 castCount, SpellCastResult result, bool isPetCastResult);
        /// `spell` starts, with the cast flags `castFlags` and, under CAST_FLAG_HEAL_PREDICTION, the
        /// heal `prediction`.
        virtual void SpellStart(Spell const& spell, uint32 castFlags, SpellHealPrediction const& prediction);
        /// `spell` goes, with the cast flags `castFlags` and its classified `targets`
        /// (`Spell::ClassifyGoTargets`).
        virtual void SpellGo(Spell const& spell, uint32 castFlags, SpellGoTargets const& targets);
        /// The effect log of `spell`.
        virtual void LogExecute(Spell const& spell);
        /// `spell` is interrupted with `result`.
        virtual void Interrupted(Spell const& spell, uint8 result);
        /// `spell`'s channel has `time` milliseconds left.
        virtual void ChannelUpdate(Spell const& spell, uint32 time);
        /// `spell`'s channel starts, for `duration` milliseconds.
        virtual void ChannelStart(Spell const& spell, uint32 duration);
        /// `target` is asked whether `spell`'s caster may resurrect it.
        virtual void ResurrectRequest(Spell const& spell, Player& target);
        /// `spell`'s cast is pushed back by `delay` milliseconds.
        virtual void CastDelayed(Spell const& spell, int32 delay);

        /// The installed notices, or the base when none are installed.
        static SpellNotices& Installed();
        /// Installs `notices` and answers the ones it replaces (NULL for the base). NULL installs the
        /// base. Installed once at start-up, before the world loads.
        static SpellNotices* Install(SpellNotices* notices);
};

#define sSpellNotices SpellNotices::Installed()

#endif
