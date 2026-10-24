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

#include "SocialPackets.h"
#include "Opcodes.h"
#include "WorldPacket.h"

void BuildContactListPacket(WorldPacket& packet, ContactListFact const& fact)
{
    uint32 size = fact.entries.size();

    packet.Initialize(SMSG_CONTACT_LIST, (4 + 4 + size * 25)); // just can guess size
    packet << uint32(7);                                    // unk flag (0x1, 0x2, 0x4), 0x7 if it include ignore list
    packet << uint32(size);                                 // friends count

    for (ContactListFact::Entry const& entry : fact.entries)
    {
        FriendInfo const& friendInfo = entry.info;

        packet << ObjectGuid(HIGHGUID_PLAYER, entry.lowGuid); // player guid
        packet << uint32(friendInfo.Flags);                // player flag (0x1-friend?, 0x2-ignored?, 0x4-muted?)
        packet << friendInfo.Note;                         // string note
        if (friendInfo.Flags & SOCIAL_FLAG_FRIEND)         // if IsFriend()
        {
            packet << uint8(friendInfo.Status);            // online/offline/etc?
            if (friendInfo.Status)                         // if online
            {
                packet << uint32(friendInfo.Area);         // player area
                packet << uint32(friendInfo.Level);        // player level
                packet << uint32(friendInfo.Class);        // player class
            }
        }
    }
}

void BuildFriendStatusPacket(WorldPacket& packet, FriendStatusFact const& fact)
{
    FriendInfo const& fi = fact.info;

    packet.Initialize(SMSG_FRIEND_STATUS, 5);
    packet << uint8(fact.result);
    packet << ObjectGuid(HIGHGUID_PLAYER, fact.friendLowGuid);
    switch (fact.result)
    {
        case FRIEND_ADDED_OFFLINE:
        case FRIEND_ADDED_ONLINE:
            packet << fi.Note;
            break;
        default:
            break;
    }

    switch (fact.result)
    {
        case FRIEND_ADDED_ONLINE:
        case FRIEND_ONLINE:
            packet << uint8(fi.Status);
            packet << uint32(fi.Area);
            packet << uint32(fi.Level);
            packet << uint32(fi.Class);
            break;
        default:
            break;
    }
}
