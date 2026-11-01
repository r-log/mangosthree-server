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
 * @file ItemHandler.cpp
 * @brief Item inventory and interaction opcode handlers
 */

#include "Platform/Define.h"
#include <string>
#include "Common/TimeConstants.h"
#include <ctime>
#include "WorldPacket.h"
#include "WorldSession.h"
#include "Opcodes.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "DBCStores.h"
#include "Player.h"
#include "Item.h"
#include "UpdateData.h"


void WorldSession::SendItemDb2Reply(uint32 entry)
{
    WorldPacket data(SMSG_DB_REPLY, 44);
    ItemPrototype const* proto = sObjectMgr.GetItemPrototype(entry);
    if (!proto)
    {
        data << uint32(-1);         // entry
        data << uint32(DB2_REPLY_ITEM);
        data << uint32(time(NULL)); // hotfix date
        data << uint32(0);          // size of next block
        SendPacket(&data);
        return;
    }

    data << uint32(entry);
    data << uint32(DB2_REPLY_ITEM);
    data << uint32(sObjectMgr.GetHotfixDate(entry, DB2_REPLY_ITEM));

    ByteBuffer buff;
    buff << uint32(entry);
    buff << uint32(proto->Class);
    buff << uint32(proto->SubClass);
    buff << int32(proto->Unk0);
    buff << uint32(proto->Material);
    buff << uint32(proto->DisplayInfoID);
    buff << uint32(proto->InventoryType);
    buff << uint32(proto->Sheath);

    data << uint32(buff.size());
    data.append(buff);

    SendPacket(&data);
}

void WorldSession::SendItemSparseDb2Reply(uint32 entry)
{
    WorldPacket data(SMSG_DB_REPLY, 526);
    ItemPrototype const* proto = sObjectMgr.GetItemPrototype(entry);
    if (!proto)
    {
        data << uint32(-1);         // entry
        data << uint32(DB2_REPLY_SPARSE);
        data << uint32(time(NULL)); // hotfix date
        data << uint32(0);          // size of next block
        SendPacket(&data);
        return;
    }

    data << uint32(entry);
    data << uint32(DB2_REPLY_SPARSE);
    data << uint32(sObjectMgr.GetHotfixDate(entry, DB2_REPLY_SPARSE));

    ByteBuffer buff;
    buff << uint32(entry);
    buff << uint32(proto->Quality);
    buff << uint32(proto->Flags);
    buff << uint32(proto->Flags2);
    buff << float(proto->Unknown);
    buff << float(proto->Unknown1);
    buff << uint32(proto->BuyCount);
    buff << int32(proto->BuyPrice);
    buff << uint32(proto->SellPrice);
    buff << uint32(proto->InventoryType);
    buff << int32(proto->AllowableClass);
    buff << int32(proto->AllowableRace);
    buff << uint32(proto->ItemLevel);
    buff << uint32(proto->RequiredLevel);
    buff << uint32(proto->RequiredSkill);
    buff << uint32(proto->RequiredSkillRank);
    buff << uint32(proto->RequiredSpell);
    buff << uint32(proto->RequiredHonorRank);
    buff << uint32(proto->RequiredCityRank);
    buff << uint32(proto->RequiredReputationFaction);
    buff << uint32(proto->RequiredReputationRank);
    buff << int32(proto->MaxCount);
    buff << int32(proto->Stackable);
    buff << uint32(proto->ContainerSlots);

    for (uint32 x = 0; x < MAX_ITEM_PROTO_STATS; ++x)
    {
        buff << uint32(proto->ItemStat[x].ItemStatType);
    }

    for (uint32 x = 0; x < MAX_ITEM_PROTO_STATS; ++x)
    {
        buff << int32(proto->ItemStat[x].ItemStatValue);
    }

    for (uint32 x = 0; x < MAX_ITEM_PROTO_STATS; ++x)
    {
        buff << int32(proto->ItemStat[x].ItemStatType2);
    }

    for (uint32 x = 0; x < MAX_ITEM_PROTO_STATS; ++x)
    {
        buff << int32(proto->ItemStat[x].ItemStatValue2);
    }

    buff << uint32(proto->ScalingStatDistribution);
    buff << uint32(proto->DamageType);
    buff << uint32(proto->Delay);
    buff << float(proto->RangedModRange);

    for (uint32 x = 0; x < MAX_ITEM_PROTO_SPELLS; ++x)
    {
        buff << int32(proto->Spells[x].SpellId);
    }

    for (uint32 x = 0; x < MAX_ITEM_PROTO_SPELLS; ++x)
    {
        buff << uint32(proto->Spells[x].SpellTrigger);
    }

    for (uint32 x = 0; x < MAX_ITEM_PROTO_SPELLS; ++x)
    {
        buff << int32(proto->Spells[x].SpellCharges);
    }

    for (uint32 x = 0; x < MAX_ITEM_PROTO_SPELLS; ++x)
    {
        buff << int32(proto->Spells[x].SpellCooldown);
    }

    for (uint32 x = 0; x < MAX_ITEM_PROTO_SPELLS; ++x)
    {
        buff << uint32(proto->Spells[x].SpellCategory);
    }

    for (uint32 x = 0; x < MAX_ITEM_PROTO_SPELLS; ++x)
    {
        buff << int32(proto->Spells[x].SpellCategoryCooldown);
    }

    buff << uint32(proto->Bonding);

    // item name
    std::string name = proto->Name1;
    buff << uint16(name.length());
    if (name.length())
    {
        buff << name;
    }

    for (uint32 i = 0; i < 3; ++i) // other 3 names
    {
        buff << uint16(0);
    }

    std::string desc = proto->Description;
    buff << uint16(desc.length());
    if (desc.length())
    {
        buff << desc;
    }

    buff << uint32(proto->PageText);
    buff << uint32(proto->LanguageID);
    buff << uint32(proto->PageMaterial);
    buff << uint32(proto->StartQuest);
    buff << uint32(proto->LockID);
    buff << int32(proto->Material);
    buff << uint32(proto->Sheath);
    buff << int32(proto->RandomProperty);
    buff << int32(proto->RandomSuffix);
    buff << uint32(proto->ItemSet);

    buff << uint32(proto->Area);
    buff << uint32(proto->Map);
    buff << uint32(proto->BagFamily);
    buff << uint32(proto->TotemCategory);

    for (uint32 x = 0; x < MAX_ITEM_PROTO_SOCKETS; ++x)
    {
        buff << uint32(proto->Socket[x].Color);
    }

    for (uint32 x = 0; x < MAX_ITEM_PROTO_SOCKETS; ++x)
    {
        buff << uint32(proto->Socket[x].Content);
    }

    buff << uint32(proto->socketBonus);
    buff << uint32(proto->GemProperties);
    buff << float(proto->ArmorDamageModifier);
    buff << int32(proto->Duration);
    buff << uint32(proto->ItemLimitCategory);
    buff << uint32(proto->HolidayId);
    buff << float(proto->StatScalingFactor);
    buff << uint32(proto->Unknown400_1);
    buff << uint32(proto->Unknown400_2);

    data << uint32(buff.size());
    data.append(buff);

    SendPacket(&data);
}

void WorldSession::SendReforgeResult(bool success)
{
    WorldPacket data(SMSG_REFORGE_RESULT, 1);
    data.WriteBit(success);
    SendPacket(&data);
}

