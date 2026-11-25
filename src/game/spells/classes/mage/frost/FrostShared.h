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

#ifndef MANGOSSERVER_SPELLS_CLASSES_MAGE_FROST_FROSTSHARED_H
#define MANGOSSERVER_SPELLS_CLASSES_MAGE_FROST_FROSTSHARED_H

#include "spells/classes/mage/Mage.h"

/**
 * @file FrostShared.h
 * @brief What two or more of the Frost folder's mechanic files share: Shatter's ranks (Shatter.cpp, Frostburn.cpp),
 * Frostbolt (Frostbolt.cpp, Frostburn.cpp), Ice Lance (Frostburn.cpp, FingersOfFrost.cpp), Frostfire Bolt and Brain
 * Freeze's buff (FingersOfFrost.cpp, BrainFreeze.cpp), Ice Barrier (IceBarrier.cpp, ReactiveBarrier.cpp), and three
 * helpers: EffectValue, which Frostburn.cpp defines and IceBarrier.cpp, ImprovedFreeze.cpp and PiercingChill.cpp call
 * too; CastTimeGoingOff, which Shatter.cpp defines and Frostburn.cpp calls too; and PiercingChillTalent, which
 * PiercingChill.cpp defines and Frostbolt.cpp calls too.
 *
 * They stand at namespace scope, as they stood in the anonymous namespace of the one file they came from, so the
 * files that share them name them as before; only this folder's files include this header.
 */

uint32 const SHATTER_RANK_ONE = 11170;
uint32 const SHATTER_RANK_TWO = 12982;
uint32 const FROSTBOLT = 116;
uint32 const ICE_LANCE_SPELL = 30455;
uint32 const FROSTFIRE_BOLT = 44614;
uint32 const BRAIN_FREEZE_BUFF = 57761;
uint32 const ICE_BARRIER = 11426;

/// The value of effect `index` of spell `spellId` in the spell store; 0 when the store lacks the spell.
int32 EffectValue(uint32 spellId, SpellEffectIndex index);

/// The cast time in ms that the caster's cast of `spell` in one of his current spell slots started with, when that
/// cast has not been launched yet: the cast whose hit the crit-chance and damage-done hooks reckon as it goes off.
/// 0 when it is not above 0, and when no slot holds such a cast, as for a triggered cast, which stands in none.
uint32 CastTimeGoingOff(Unit const& caster, SpellEntry const& spell);

/// The rank of Piercing Chill `mage` knows: 83157, else 83156; 0 when he knows neither.
uint32 PiercingChillTalent(Unit const& mage);

#endif
