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

/**
 * @file UpdateData.cpp
 * @brief Object update packet builder and compressor
 *
 * This file implements UpdateData, which accumulates object creation,
 * destruction, and value updates for efficient network transmission.
 *
 * Features:
 * - Accumulates object update blocks for batch transmission
 * - Tracks out-of-range objects (visibility removal)
 * - zlib compression for large packets (configurable threshold: 100 bytes)
 * - Packed GUID encoding for bandwidth efficiency
 *
 * Packet structure:
 * - Block count
 * - Transport flag
 * - Out-of-range GUID list (optional)
 * - Update data blocks
 *
 * @see UpdateData for the data accumulator
 * @see Object::BuildCreateUpdateBlockForPlayer for object creation
 * @see Object::BuildValuesUpdateBlockForPlayer for value updates
 */

#include "Utilities/Errors.h"
#include "Platform/Define.h"
#include "UpdateData.h"
#include "ByteBuffer.h"
#include "ObjectGuid.h"

/**
 * @brief Construct empty UpdateData
 *
 * Initializes an empty update data accumulator with zero blocks.
 * Data buffers are allocated as needed during block accumulation.
 */
UpdateData::UpdateData(uint32 map) : m_map(map), m_blockCount(0)
{
}

/**
 * @brief Add multiple out-of-range GUIDs
 * @param guids Set of GUIDs to add
 *
 * Adds a set of object GUIDs that the player can no longer see.
 * These will be sent as out-of-range objects in the update packet,
 * causing the client to remove them from the scene.
 */
void UpdateData::AddOutOfRangeGUID(GuidSet& guids)
{
    m_outOfRangeGUIDs.insert(guids.begin(), guids.end());
}

/**
 * @brief Add single out-of-range GUID
 * @param guid Object GUID to add
 *
 * Adds a single object GUID that the player can no longer see.
 * @see AddOutOfRangeGUID(GuidSet&)
 */
void UpdateData::AddOutOfRangeGUID(ObjectGuid const& guid)
{
    m_outOfRangeGUIDs.insert(guid);
}

/**
 * @brief Clear all accumulated data
 *
 * Resets the UpdateData to empty state:
 * - Clears update data buffer
 * - Clears out-of-range GUID set
 * - Resets block counter to 0
 *
 * Called after the packet is built and sent.
 */
void UpdateData::Clear()
{
    m_data.clear();
    m_outOfRangeGUIDs.clear();
    m_blockCount = 0;
    m_map = 0;
}
