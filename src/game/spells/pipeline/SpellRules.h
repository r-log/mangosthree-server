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

#ifndef MANGOS_H_SPELLRULES
#define MANGOS_H_SPELLRULES

#include "spells/data/SpellMgr.h"

class Player;
class Spell;
class Unit;

/// The spell rules that read a caster, a player or a cast in progress, over the spell tables.

/// A cast's cast time: the record's, with the caster's level, scaling, mods and haste.
uint32 GetSpellCastTime(SpellEntry const* spellInfo, Spell const* spell);

/// A spell's duration after the caster's combo points and mods.
int32 CalculateSpellDuration(SpellEntry const* spellInfo, Unit const* caster);

/// Whether the spell may be cast here: its area group, continent, raid instance, spell-area rows,
/// arena and battleground rules and its own location handler.
SpellCastResult GetSpellAllowedInLocationError(SpellEntry const* spellInfo, uint32 map_id, uint32 zone_id, uint32 area_id, Player const* player,
                                               SpellAreaConditionCheck const& meetsCondition);

/// Whether the player meets a spell-area row in the zone and area.
bool IsFitToRequirements(SpellArea const& spellArea, Player const* player, uint32 newZone, uint32 newArea, SpellAreaConditionCheck const& meetsCondition);

/// Casts the row's spell on the player when the row fits and autocasts it, or removes it when it no
/// longer fits and `onlyApply` is false.
void ApplyOrRemoveSpellIfCan(SpellArea const& spellArea, Player* player, uint32 newZone, uint32 newArea, bool onlyApply, SpellAreaConditionCheck const& meetsCondition);

#endif
