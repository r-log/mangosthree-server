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

#ifndef MANGOS_HARNESS_OWNERSHIP_H
#define MANGOS_HARNESS_OWNERSHIP_H

#include "Platform/Define.h"

/**
 * What a scenario still owns of a harness player by the time the runner's teardown
 * reaches it. Pure: pointers are compared here, never followed.
 *
 * A harness player is two allocations the scenario made itself -- the `Player` and the
 * `WorldSession` under him -- and exactly one of them can be taken away behind the
 * scenario's back. `Map::Remove(player, true)` ends in `Map::DeleteFromWorld`, which
 * unregisters the player and deletes him (Map.cpp:479-483) and never looks at a session;
 * nothing anywhere frees the session but the scenario's own teardown. So before the
 * teardown dereferences the pointer it kept, it has to ask whether that pointer is still
 * a player.
 *
 * THE WITNESS is the player registry, and what makes it a sound one is the shape of
 * `Map::DeleteFromWorld`: its whole body is `sPlayerRegistry.Remove(pl); delete pl;`, in
 * that order, and a harness run is single-threaded on the world thread with no other
 * session online (`Runner::Start` refuses otherwise), so nothing observes the gap between
 * the two statements. While the object lives the registry holds it; the moment it does
 * not, the registry does not either. The lookup must be the `inWorld = false` one: the
 * default hides a player who is registered but out of the world, and reading that as
 * "destroyed" would delete a session out from under a player who is still alive.
 *
 * IDENTITY, not presence, because a guid is not a lifetime. The harness hands its players
 * guids out of one small reserved block and restarts at the bottom of it for every
 * scenario, so a later player can be standing on an earlier one's guid: a lookup that
 * merely found something there would hand the teardown somebody else's player to
 * unregister, remove from the map and delete, which is a worse bug than the leak it was
 * trying to avoid.
 */
namespace Harness
{
    enum class Ownership : uint8
    {
        Held,        ///< the registry still holds the very object the record kept: tear it down whole
        Destroyed,   ///< nothing is registered on that guid; something else has already ended him
        Replaced     ///< another player object answers that guid; ours is gone, and that one is not ours to touch
    };

    /**
     * @param owned      the pointer the scenario's record kept.
     * @param registered whatever the registry answers for that record's guid right now, NULL
     *                   when nothing does.
     *
     * Both parameters are `void const*` on purpose: this is asked precisely where following
     * `owned` may be a use-after-free, so the signature takes the one pointer type a caller
     * cannot accidentally dereference.
     */
    inline Ownership ClassifyOwnership(void const* owned, void const* registered)
    {
        if (!registered)
        {
            return Ownership::Destroyed;
        }
        return registered == owned ? Ownership::Held : Ownership::Replaced;
    }
}

#endif
