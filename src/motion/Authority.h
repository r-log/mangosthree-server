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

#ifndef MANGOS_MOTION_AUTHORITY_H
#define MANGOS_MOTION_AUTHORITY_H

#include "Platform/Define.h"

#include <vector>

/**
 * Which units one client may move, and the one it has selected (design v2 §7,
 * F1). Membership changes only through the server's control transitions --
 * login, possession, a vehicle's control seat, and their releases -- and the
 * client's CMSG_SET_ACTIVE_MOVER / CMSG_MOVE_NOT_ACTIVE_MOVER select and
 * deselect a member. A movement packet is honoured for the selected unit only;
 * an ack for any member, because a change sent to the player is answered as
 * the player while the vehicle is selected. Every refusal is counted and the
 * packet dropped; thresholds are a later rung of the ladder (§10.1). The
 * session owns one; it runs in the map phase like the packets it judges, so
 * its counters are plain.
 */
namespace Motion
{
    struct AuthorityCounters
    {
        uint32 added, removed, selected, deselected, badSelect, badDeselect, notActive, notMember, unresolved;
        AuthorityCounters() : added(0), removed(0), selected(0), deselected(0), badSelect(0), badDeselect(0),
                              notActive(0), notMember(0), unresolved(0) {}
    };

    class Authority
    {
    public:
        Authority();

        /// Membership, idempotent; selects guid: the server pre-selects what it hands over.
        void Add(uint64 guid);
        /// Drops membership; clears the selection when guid was selected. False for a non-member.
        bool Remove(uint64 guid);
        bool IsMember(uint64 guid) const;
        /// 0 when nothing is selected.
        uint64 Selected() const { return m_selected; }
        /// The client's CMSG_SET_ACTIVE_MOVER: a member is selected, a stranger counts badSelect.
        bool Select(uint64 guid);
        /// The client's CMSG_MOVE_NOT_ACTIVE_MOVER: the selected guid is cleared, anything else counts badDeselect.
        bool Deselect(uint64 guid);
        /// A movement packet: true for the selected unit, else counts notActive.
        bool MovesAs(uint64 guid);
        /// An ack: true for any member, else counts notMember.
        bool MayAck(uint64 guid);
        /// The selected member could not be found in the map (the session counts it).
        void Unresolved() { ++m_counters.unresolved; }
        /// Every member gone (logout); each counts as removed.
        void Clear();

        std::vector<uint64> const& Members() const { return m_members; }
        AuthorityCounters const& Counters() const { return m_counters; }

    private:
        std::vector<uint64> m_members;   ///< insertion order; two or three entries at most
        uint64              m_selected;
        AuthorityCounters   m_counters;
    };
}

#endif
