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

#ifndef __UPDATEDATA_H
#define __UPDATEDATA_H

#include "ObjectGuid.h"

#include <memory>
#include <vector>

enum ObjectUpdateType
{
    UPDATETYPE_VALUES               = 0,
    UPDATETYPE_CREATE_OBJECT        = 1,
    UPDATETYPE_CREATE_OBJECT2       = 2,
    UPDATETYPE_OUT_OF_RANGE_OBJECTS = 3,
};

enum ObjectUpdateFlags
{
    UPDATEFLAG_NONE                 = 0x0000,
    UPDATEFLAG_SELF                 = 0x0001,
    UPDATEFLAG_TRANSPORT            = 0x0002,
    UPDATEFLAG_HAS_ATTACKING_TARGET = 0x0004,
    UPDATEFLAG_UNK                  = 0x0008,
    UPDATEFLAG_LOW                  = 0x0010,
    UPDATEFLAG_LIVING               = 0x0020,
    UPDATEFLAG_HAS_POSITION         = 0x0040,
    UPDATEFLAG_VEHICLE              = 0x0080,
    UPDATEFLAG_POSITION             = 0x0100,
    UPDATEFLAG_ROTATION             = 0x0200,
    UPDATEFLAG_UNK1                 = 0x0400,
    UPDATEFLAG_ANIM_KITS            = 0x0800,
    UPDATEFLAG_TRANSPORT_ARR        = 0x1000,
    UPDATEFLAG_ENABLE_PORTALS       = 0x2000,
    UPDATEFLAG_UNK2                 = 0x4000,
};

struct MovementUpdate;

/// The values part of an update block as one viewer sees it: the update mask and, in index order,
/// the 32-bit value of each field the mask names. Nothing is shown for a block built with no
/// viewer.
struct UpdateValues
{
    UpdateValues() : shown(false), blockCount(0) {}

    bool shown;
    uint8 blockCount;                                       ///< the mask's 32-bit blocks
    std::vector<uint8> mask;                                ///< the mask's bytes
    std::vector<uint32> values;
};

/// One block of an object update: a values block, or a create block that also carries the object's
/// type and its movement part.
struct UpdateBlock
{
    UpdateBlock() : type(UPDATETYPE_VALUES), objectTypeId(0) {}

    uint8 type;                                             ///< ObjectUpdateType
    ObjectGuid guid;
    uint8 objectTypeId;                                     ///< a create block's TypeID
    std::shared_ptr<MovementUpdate const> movement;         ///< a create block's movement part
    UpdateValues values;
};

class UpdateData
{
    public:
        UpdateData(uint32 mapId);

        void AddOutOfRangeGUID(GuidSet& guids);
        void AddOutOfRangeGUID(ObjectGuid const& guid);
        /// Adds a block; the packet carries the blocks in the order they were added.
        void AddBlock(UpdateBlock const& block) { m_blocks.push_back(block); }
        std::vector<UpdateBlock> const& GetBlocks() const { return m_blocks; }
        bool HasData() { return !m_blocks.empty() || !m_outOfRangeGUIDs.empty(); }
        void Clear();

        GuidSet const& GetOutOfRangeGUIDs() const { return m_outOfRangeGUIDs; }

        void SetMapId(uint32 mapId) { m_map = mapId; }
        uint32 GetMapId() const { return m_map; }
        uint32 GetBlockCount() const { return uint32(m_blocks.size()); }

    protected:

        /**
         * @brief The map this update belongs to.
         *
         * uint32, because that is what a map id is everywhere else in the tree:
         * Object::GetMapId(), GridMap, BattleGround and MapPersistentState all
         * return uint32, and every caller here passes one straight through.
         *
         * The narrowing is the one on the wire, in the update packet's builder
         * (BuildUpdateObjectPacket), and it is deliberate, visible and checked.
         * See the note there: the field is 16 bits, and a map id above 65535
         * does not fit it.
         */
        uint32 m_map;
        GuidSet m_outOfRangeGUIDs;
        std::vector<UpdateBlock> m_blocks;
};
#endif
