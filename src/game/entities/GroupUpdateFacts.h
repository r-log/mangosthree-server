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

#ifndef MANGOS_H_GROUPUPDATEFACTS
#define MANGOS_H_GROUPUPDATEFACTS

#include "Platform/Define.h"
#include "Utilities/Errors.h"

#include <functional>

class Pet;

// What a player's group is told about the player: one fact per kind of change, each reported
// through a callback the session installs on the player, which marks the change for the group's
// next update of that player.

/// A group-visible stat of the player changed; `flag` is its bit in the group update mask.
struct GroupStatFact
{
    uint32 flag = 0;
};

/// One of the player's own aura slots changed; `flag` is the auras' bit in the group update mask.
struct GroupAuraFact
{
    uint32 flag = 0;
    uint8 slot = 0;
};

/// One of the aura slots of `pet`, the player's pet, changed; `flag` is the pet auras' bit in the
/// player's group update mask, and the slot is marked on `pet`.
struct PetGroupAuraFact
{
    uint32 flag = 0;
    uint8 slot = 0;
    Pet* pet = NULL;
};

typedef std::function<void(GroupStatFact const&)> GroupStatSink;
typedef std::function<void(GroupAuraFact const&)> GroupAuraSink;
typedef std::function<void(PetGroupAuraFact const&)> PetGroupAuraSink;

/// What the session installs on a player for its group: the three facts' callbacks.
struct GroupCallbacks
{
    GroupStatSink stat;
    GroupAuraSink aura;
    PetGroupAuraSink petAura;
};

/// The callbacks `callbacks` points to; NULL (a unit that is not a player, or a player whose
/// destructor has ended its use) fails the assertion.
inline GroupCallbacks const& InstalledGroupCallbacks(GroupCallbacks const* callbacks)
{
    MANGOS_ASSERT(callbacks);
    return *callbacks;
}

/// Reports `fact` through `sink`; an empty `sink` (a player no session installed its callbacks
/// on) fails the assertion.
template <class Fact>
void ReportGroupFact(std::function<void(Fact const&)> const& sink, Fact const& fact)
{
    MANGOS_ASSERT(sink);
    sink(fact);
}

#endif
