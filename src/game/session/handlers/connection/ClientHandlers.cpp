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

#include "session/handlers/connection/ClientHandlers.h"

#include <zlib.h>
#include "Platform/Define.h"
#include <string>
#include <vector>
#include <ctime>
#include "WorldPacket.h"
#include "Opcodes.h"
#include "Log.h"
#include "Player.h"
#include "WorldSession.h"

/**
 * @brief Consumes an account-data update packet.
 *
 * @param recv_data The received opcode packet.
 */
void ClientHandlers::HandleUpdateAccountData(WorldSession& session, WorldPacket& recv_data)
{
    DETAIL_LOG("WORLD: Received opcode CMSG_UPDATE_ACCOUNT_DATA");

    uint32 type, timestamp, decompressedSize;
    recv_data >> type >> timestamp >> decompressedSize;

    DEBUG_LOG("UAD: type %u, time %u, decompressedSize %u", type, timestamp, decompressedSize);

    if (type > NUM_ACCOUNT_DATA_TYPES)
    {
        return;
    }

    if (decompressedSize == 0)                              // erase
    {
        session.SetAccountData(AccountDataType(type), 0, "");

        WorldPacket data(SMSG_UPDATE_ACCOUNT_DATA_COMPLETE, 4 + 4);
        data << uint32(type);
        data << uint32(0);
        session.SendPacket(&data);

        return;
    }

    if (decompressedSize > 0xFFFF)
    {
        recv_data.rpos(recv_data.wpos());                   // unnneded warning spam in this case
        sLog.outError("UAD: Account data packet too big, size %u", decompressedSize);
        return;
    }

    ByteBuffer dest;
    dest.resize(decompressedSize);

    uLongf realSize = decompressedSize;
    if (uncompress(const_cast<uint8*>(dest.contents()), &realSize, const_cast<uint8*>(recv_data.contents() + recv_data.rpos()), recv_data.size() - recv_data.rpos()) != Z_OK)
    {
        recv_data.rpos(recv_data.wpos());                   // unneded warning spam in this case
        sLog.outError("UAD: Failed to decompress account data");
        return;
    }

    recv_data.rpos(recv_data.wpos());                       // uncompress read (recv_data.size() - recv_data.rpos())

    std::string adata;
    dest >> adata;

    session.SetAccountData(AccountDataType(type), timestamp, adata);

    WorldPacket data(SMSG_UPDATE_ACCOUNT_DATA_COMPLETE, 4 + 4);
    data << uint32(type);
    data << uint32(0);
    session.SendPacket(&data);
}

/**
 * @brief Acknowledges an account-data request packet.
 *
 * @param recv_data The received opcode packet.
 */
void ClientHandlers::HandleRequestAccountData(WorldSession& session, WorldPacket& recv_data)
{
    DETAIL_LOG("WORLD: Received opcode CMSG_REQUEST_ACCOUNT_DATA");

    uint32 type;
    recv_data >> type;

    DEBUG_LOG("RAD: type %u", type);

    if (type > NUM_ACCOUNT_DATA_TYPES)
    {
        return;
    }

    AccountData* adata = session.GetAccountData(AccountDataType(type));

    uint32 size = adata->Data.size();

    uLongf destSize = compressBound(size);

    ByteBuffer dest;
    dest.resize(destSize);

    if (size && compress(const_cast<uint8*>(dest.contents()), &destSize, (uint8*)adata->Data.c_str(), size) != Z_OK)
    {
        DEBUG_LOG("RAD: Failed to compress account data");
        return;
    }

    dest.resize(destSize);

    WorldPacket data(SMSG_UPDATE_ACCOUNT_DATA, 8 + 4 + 4 + 4 + destSize);
    data << (session.GetPlayer() ? session.GetPlayer()->GetObjectGuid() : ObjectGuid());// player guid
    data << uint32(type);                                   // type (0-7)
    data << uint32(adata->Time);                            // unix time
    data << uint32(size);                                   // decompressed length
    data.append(dest);                                      // compressed data
    session.SendPacket(&data);
}

void ClientHandlers::HandleReadyForAccountDataTimes(WorldSession& session, WorldPacket& /*recv_data*/)
{
    // empty opcode
    DEBUG_LOG("WORLD: Received opcode CMSG_READY_FOR_ACCOUNT_DATA_TIMES");

    session.SendAccountDataTimes(GLOBAL_CACHE_MASK);
}

void ClientHandlers::HandleRealmSplit(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_REALM_SPLIT");

    uint32 unk;
    std::string split_date = "01/01/01";
    recv_data >> unk;

    WorldPacket data(SMSG_REALM_SPLIT, 4 + 4 + split_date.size() + 1);
    data << unk;
    data << uint32(0x00000000);                             // realm split state
    // split states:
    // 0x0 realm normal
    // 0x1 realm split
    // 0x2 realm split pending
    data << split_date;
    session.SendPacket(&data);
    // DEBUG_LOG("response sent %u", unk);
}

void ClientHandlers::HandleUITimeRequest(WorldSession& session, WorldPacket& /*recv_data*/)
{
    // empty opcode
    DEBUG_LOG("WORLD: Received opcode SMSG_UI_TIME");

    WorldPacket data(SMSG_UI_TIME, 4);
    data << uint32(time(NULL));
    session.SendPacket(&data);
}

void ClientHandlers::HandleRequestHotfix(WorldSession& session, WorldPacket& recv_data)
{
    uint32 type = 0, count = 0;
    recv_data >> type;

    count = recv_data.ReadBits(23);

    std::vector<ObjectGuid> guids;
    guids.resize(count);

    for (uint32 i = 0; i < count; ++i)
    {
        recv_data.ReadGuidMask<0, 4, 7, 2, 5, 3, 6, 1>(guids[i]);
    }

    uint32 entry = 0;
    for (uint32 i = 0; i < count; ++i)
    {
        recv_data.ReadGuidBytes<5, 6, 7, 0, 1, 3, 4>(guids[i]);
        recv_data >> entry;
        recv_data.ReadGuidBytes<2>(guids[i]);

        switch (type)
        {
            case DB2_REPLY_ITEM:
                session.SendItemDb2Reply(entry);
                break;
            case DB2_REPLY_SPARSE:
                session.SendItemSparseDb2Reply(entry);
                break;
            default:
                sLog.outError("CMSG_REQUEST_HOTFIX: Received unknown hotfix type: %u", type);
                recv_data.rfinish();
                break;
        }
    }
}

/**
 * @brief Records client taxi benchmark mode changes.
 *
 * @param recv_data The received opcode packet.
 */
void ClientHandlers::HandleSetTaxiBenchmark(WorldSession& /*session*/, WorldPacket& recv_data)
{
    uint8 mode;
    recv_data >> mode;

    DEBUG_LOG("Client used \"/timetest %d\" command", mode);
}
