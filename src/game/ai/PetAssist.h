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

#ifndef MANGOS_PETASSIST_H
#define MANGOS_PETASSIST_H

#include "Platform/Define.h"

class Unit;

/**
 * @file PetAssist.h
 * @brief What a pet's stances read of its owner: the attack notice an Assist pet follows, the time it waits before it
 * follows a new target, and the unit a Defensive or Assist pet attacks for him.
 */
namespace Pets
{
    /// How long the owner must have attacked a new target before an Assist pet switches to it, in milliseconds.
    uint32 const PET_ASSIST_SWITCH_MS = 3000;

    /// Tells `owner`'s pet, when he has one with an AI, that he attacked `victim` (CreatureAI::OwnerAttacked).
    void NotifyOwnerAttacked(Unit& owner, Unit& victim);

    /// Whether an Assist pet switches to the owner's new target after the owner has attacked it for `heldMs`: at or
    /// past `switchAfterMs`.
    bool AssistSwitchDue(uint32 heldMs, uint32 switchAfterMs);

    /// The unit a Defensive or Assist pet with no victim attacks for its owner: the first of the units attacking him;
    /// NULL when none is.
    Unit* OwnerAttacker(Unit const& owner);
}

#endif
