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

/// An item prototype taking the client's records: the Item.db2 fields, the Item-sparse.db2 fields with the three
/// mappings, the server-only fields left as the database row set them, and the items an override in
/// `hotfix_data` keeps on their row.

#include "TestHarness.h"
#include "data/world/ItemPrototypeClientRecords.h"
#include "data/world/ItemPrototype.h"
#include "data/dbc/DB2Structure.h"

namespace
{
    char kRowName[] = "Row Name";

    /// A prototype as the database row filled it: client fields that the records replace, server-only fields
    /// they must not touch.
    ItemPrototype RowPrototype()
    {
        ItemPrototype proto = ItemPrototype();
        proto.ItemId = 64488;
        proto.Name1 = kRowName;
        proto.Quality = 1;
        proto.Flags2 = 40960;
        proto.BuyPrice = 12;
        proto.SellPrice = 3;
        proto.ItemLevel = 0;
        proto.MaxCount = 1;
        proto.Stackable = 5;
        proto.socketBonus = uint32(-1);
        proto.MaxDurability = 100;
        proto.RequiredDisenchantSkill = 275;
        proto.DisenchantID = 7;
        proto.FoodType = 3;
        proto.Spells[0].SpellPPMRate = 2.5f;
        proto.MinMoneyLoot = 10;
        proto.MaxMoneyLoot = 20;
        proto.ExtraFlags = 1;
        return proto;
    }

    ItemSparseEntry SparseRecord()
    {
        ItemSparseEntry record = ItemSparseEntry();
        record.ID = 64488;
        record.Quality = 3;
        record.Flags[0] = 0x80000;
        record.Flags[1] = 8192;
        record.PriceRandomValue = 1.0219f;
        record.BuyPrice = 652633;
        record.SellPrice = 163158;
        record.ItemLevel = 1;
        record.MaxCount = 0x7FFFFFFF;
        record.Stackable = 0x7FFFFFFF;
        record.StatModifierBonusStat[0] = 7;
        record.StatModifierBonusStat[1] = -1;
        record.StatModifierBonusAmount[0] = 508;
        record.SpellID[0] = 483;
        record.SpellID[1] = -1;
        record.SocketBonus = 0;
        record.ZoneBound = 12;
        record.InstanceBound = 34;
        record.CurrencySubstitutionID = 5;
        record.CurrencySubstitutionCount = 2;
        return record;
    }
}

TEST(ItemPrototypeClientRecords_TheSparseRecordReplacesTheRowsClientFields)
{
    ItemPrototype proto = RowPrototype();
    TakeItemSparseRecord(proto, SparseRecord());
    CHECK_EQ(proto.Quality, 3u);
    CHECK_EQ(proto.Flags, 0x80000u);
    CHECK_EQ(proto.Flags2, 8192u);
    CHECK(proto.Unknown == 1.0219f);
    CHECK_EQ(proto.BuyPrice, 652633u);
    CHECK_EQ(proto.SellPrice, 163158u);
    CHECK_EQ(proto.ItemLevel, 1u);
    CHECK_EQ(proto.ItemStat[0].ItemStatType, 7u);
    CHECK_EQ(proto.ItemStat[0].ItemStatValue, 508);
    CHECK_EQ(proto.Spells[0].SpellId, 483u);
    CHECK_EQ(proto.socketBonus, 0u);
    CHECK_EQ(proto.Area, 12u);
    CHECK_EQ(proto.Map, 34u);
    CHECK_EQ(proto.Unknown400_1, 5u);
    CHECK_EQ(proto.Unknown400_2, 2u);
}

TEST(ItemPrototypeClientRecords_AnEmptyIdIsNoneAndAnUnlimitedCountIsMinusOne)
{
    ItemPrototype proto = RowPrototype();
    TakeItemSparseRecord(proto, SparseRecord());
    CHECK_EQ(proto.ItemStat[1].ItemStatType, 0u);
    CHECK_EQ(proto.Spells[1].SpellId, 0u);
    CHECK_EQ(proto.MaxCount, -1);
    CHECK_EQ(proto.Stackable, -1);
}

TEST(ItemPrototypeClientRecords_TheServerOnlyFieldsAndTheNameStayAsTheRowSetThem)
{
    ItemPrototype proto = RowPrototype();
    TakeItemSparseRecord(proto, SparseRecord());
    CHECK(proto.Name1 == kRowName);
    CHECK_EQ(proto.MaxDurability, 100u);
    CHECK_EQ(proto.RequiredDisenchantSkill, 275);
    CHECK_EQ(proto.DisenchantID, 7u);
    CHECK_EQ(proto.FoodType, 3u);
    CHECK(proto.Spells[0].SpellPPMRate == 2.5f);
    CHECK_EQ(proto.MinMoneyLoot, 10u);
    CHECK_EQ(proto.MaxMoneyLoot, 20u);
    CHECK_EQ(proto.ExtraFlags, 1u);
}

TEST(ItemPrototypeClientRecords_TheItemRecordGivesItsSevenFields)
{
    ItemPrototype proto = RowPrototype();
    ItemEntry record = ItemEntry();
    record.ID = 64488;
    record.Class = 4;
    record.SubClass = 1;
    record.SoundOverrideSubclass = -1;
    record.Material = 5;
    record.DisplayId = 123;
    record.InventoryType = 7;
    record.Sheath = 2;
    TakeItemRecord(proto, record);
    CHECK_EQ(proto.Class, 4u);
    CHECK_EQ(proto.SubClass, 1u);
    CHECK_EQ(proto.Unk0, -1);
    CHECK_EQ(proto.Material, 5);
    CHECK_EQ(proto.DisplayInfoID, 123u);
    CHECK_EQ(proto.InventoryType, 7u);
    CHECK_EQ(proto.Sheath, 2u);
    CHECK_EQ(proto.BuyPrice, 12u);
}

TEST(ItemPrototypeClientRecords_OnlySparseRowsInTheHotfixListAreOverrides)
{
    HotfixData hotfixes;
    HotfixInfo item;
    item.Type = HOTFIX_TABLE_ITEM;
    item.Entry = 6948;
    item.Timestamp = 1;
    HotfixInfo sparse;
    sparse.Type = HOTFIX_TABLE_ITEM_SPARSE;
    sparse.Entry = 72351;
    sparse.Timestamp = 2;
    hotfixes.push_back(item);
    hotfixes.push_back(sparse);
    std::set<uint32> const overrides = ItemSparseOverrides(hotfixes);
    CHECK_EQ(overrides.size(), size_t(1));
    CHECK(overrides.count(72351) == 1);
    CHECK(overrides.count(6948) == 0);
}