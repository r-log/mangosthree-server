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

/// The bytes of SMSG_UPDATE_OBJECT from the blocks the domain collects: the map id, the block
/// count, the out-of-range guids, then each block's type, guid, mask and values.

#include "TestHarness.h"
#include "ObjectGuid.h"
#include "UpdateData.h"
#include "WorldPacket.h"
#include "session/packets/entities/UpdateObjectPacket.h"

namespace
{
    /// A values block for `guid` with one mask block whose bit `bit` is set, holding `value`.
    UpdateBlock ValuesBlock(ObjectGuid guid, uint32 bit, uint32 value)
    {
        UpdateBlock block;
        block.type = UPDATETYPE_VALUES;
        block.guid = guid;
        block.values.shown = true;
        block.values.blockCount = 1;
        block.values.mask.assign(4, 0);
        block.values.mask[bit >> 3] = uint8(1 << (bit & 7));
        block.values.values.push_back(value);
        return block;
    }
}

TEST(UpdateObjectPacket_AValuesBlockBytes)
{
    UpdateData data(1);
    data.AddBlock(ValuesBlock(ObjectGuid(uint64(5)), 2, 0x11223344));
    WorldPacket packet;
    REQUIRE(BuildUpdateObjectPacket(data, &packet));

    // map 1, one block; type 0, the packed guid (mask 01, byte 05); one mask block with bit 2; the value.
    CHECK_HEX(packet.contents(), packet.size(), "0100" "01000000" "00" "0105" "01" "04000000" "44332211");
}

TEST(UpdateObjectPacket_TheOutOfRangeGuidsComeFirstAndCountAsABlock)
{
    UpdateData data(0);
    data.AddOutOfRangeGUID(ObjectGuid(uint64(7)));
    data.AddBlock(ValuesBlock(ObjectGuid(uint64(5)), 0, 1));
    WorldPacket packet;
    REQUIRE(BuildUpdateObjectPacket(data, &packet));

    // map 0, two blocks; the out-of-range list (type 3, one guid); then the values block.
    CHECK_HEX(packet.contents(), packet.size(), "0000" "02000000" "03" "01000000" "0107" "00" "0105" "01" "01000000" "01000000");
}

TEST(UpdateObjectPacket_ABlockBuiltWithNoViewerShowsNoValues)
{
    UpdateData data(0);
    UpdateBlock block;
    block.guid = ObjectGuid(uint64(5));
    data.AddBlock(block);
    WorldPacket packet;
    REQUIRE(BuildUpdateObjectPacket(data, &packet));

    CHECK_HEX(packet.contents(), packet.size(), "0000" "01000000" "00" "0105");
}
