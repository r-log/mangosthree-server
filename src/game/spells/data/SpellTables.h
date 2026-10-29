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

#ifndef MANGOSSERVER_SPELLS_DATA_SPELLTABLES_H
#define MANGOSSERVER_SPELLS_DATA_SPELLTABLES_H

#include "Platform/Define.h"

/**
 * @file SpellTables.h
 * @brief What the spell pipeline looks up by spell id: one named lookup per table, each table sorted by spell id in
 *        SpellTables.cpp.
 */

/// The most targets a spell's effect hits when the spell fixes it: `lo` when `lo` == `hi`, else a draw in [lo, hi].
struct SpellMaxTargetsRange
{
    uint32 lo;
    uint32 hi;
};

/// The most-targets range a generic spell fixes; NULL when it fixes none.
SpellMaxTargetsRange const* FindSpellMaxTargets(uint32 spellId);

/// Whether a generic spell's effect reaches as far as the instance visibility distance, whatever its own radius.
bool HasInstanceVisibilityRadius(uint32 spellId);

/// Whether a druid spell's effect, which has no radius of its own, takes the contact distance.
bool HasContactRadius(uint32 spellId);

/// Whether a modifier aura gets exactly one charge when it applies.
bool IsOneChargeModifierAura(uint32 spellId);

/// Whether a max-health percent aura sets its target's health to the new maximum when it changes.
bool IsHealthToMaxAura(uint32 spellId);

/// Whether an aura holder starts at the spell's maximum stack.
bool IsMaxStackAtCreateAura(uint32 spellId);

/// Whether a triggered overload spell ignores the cast time it carries from the spell that triggered it.
bool IsOverloadIgnoringCastTime(uint32 spellId);

/// Whether a spell may target a player on a taxi flight.
bool MayTargetTaxiFlyer(uint32 spellId);

#endif
