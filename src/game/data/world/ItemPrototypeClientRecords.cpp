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

#include "ItemPrototypeClientRecords.h"
#include "ItemPrototype.h"
#include "DB2Structure.h"

static_assert(int(MAX_ITEM_PROTO_STATS) == int(MAX_ITEM_SPARSE_STATS), "a prototype holds every stat of the client's record");
static_assert(int(MAX_ITEM_PROTO_SPELLS) == int(MAX_ITEM_SPARSE_SPELLS), "a prototype holds every spell of the client's record");
static_assert(int(MAX_ITEM_PROTO_SOCKETS) == int(MAX_ITEM_SPARSE_SOCKETS), "a prototype holds every socket of the client's record");

namespace
{
    const int32 kUnlimited = 0x7FFFFFFF;

    int32 CountOrUnlimited(int32 value)
    {
        return value == kUnlimited ? -1 : value;
    }

    uint32 IdOrNone(int32 value)
    {
        return value < 0 ? 0 : uint32(value);
    }
}

void TakeItemRecord(ItemPrototype& proto, ItemEntry const& record)
{
    proto.Class = record.Class;
    proto.SubClass = record.SubClass;
    proto.Unk0 = record.SoundOverrideSubclass;
    proto.Material = record.Material;
    proto.DisplayInfoID = record.DisplayId;
    proto.InventoryType = record.InventoryType;
    proto.Sheath = record.Sheath;
}

void TakeItemSparseRecord(ItemPrototype& proto, ItemSparseEntry const& record)
{
    proto.Quality = uint32(record.Quality);
    proto.Flags = record.Flags[0];
    proto.Flags2 = record.Flags[1];
    proto.Unknown = record.PriceRandomValue;
    proto.Unknown1 = record.PriceVariance;
    proto.BuyCount = uint32(record.BuyCount);
    proto.BuyPrice = uint32(record.BuyPrice);
    proto.SellPrice = uint32(record.SellPrice);
    proto.AllowableClass = uint32(record.AllowableClass);
    proto.AllowableRace = uint32(record.AllowableRace);
    proto.ItemLevel = uint32(record.ItemLevel);
    proto.RequiredLevel = uint32(record.RequiredLevel);
    proto.RequiredSkill = uint32(record.RequiredSkill);
    proto.RequiredSkillRank = uint32(record.RequiredSkillRank);
    proto.RequiredSpell = uint32(record.RequiredSpell);
    proto.RequiredHonorRank = uint32(record.RequiredHonorRank);
    proto.RequiredCityRank = uint32(record.RequiredCityRank);
    proto.RequiredReputationFaction = uint32(record.RequiredReputationFaction);
    proto.RequiredReputationRank = uint32(record.RequiredReputationRank);
    proto.MaxCount = CountOrUnlimited(record.MaxCount);
    proto.Stackable = CountOrUnlimited(record.Stackable);
    proto.ContainerSlots = uint32(record.ContainerSlots);
    for (uint32 k = 0; k < MAX_ITEM_PROTO_STATS; ++k)
    {
        proto.ItemStat[k].ItemStatType = IdOrNone(record.StatModifierBonusStat[k]);
        proto.ItemStat[k].ItemStatValue = record.StatModifierBonusAmount[k];
        proto.ItemStat[k].ItemStatType2 = uint32(record.StatPercentEditor[k]);
        proto.ItemStat[k].ItemStatValue2 = record.StatPercentageOfSocket[k];
    }
    proto.ScalingStatDistribution = uint32(record.ScalingStatDistribution);
    proto.DamageType = uint32(record.DamageType);
    proto.Delay = uint32(record.Delay);
    proto.RangedModRange = record.RangedModRange;
    for (uint32 k = 0; k < MAX_ITEM_PROTO_SPELLS; ++k)
    {
        proto.Spells[k].SpellId = IdOrNone(record.SpellID[k]);
        proto.Spells[k].SpellTrigger = uint32(record.SpellTrigger[k]);
        proto.Spells[k].SpellCharges = record.SpellCharges[k];
        proto.Spells[k].SpellCooldown = record.SpellCooldown[k];
        proto.Spells[k].SpellCategory = uint32(record.SpellCategory[k]);
        proto.Spells[k].SpellCategoryCooldown = record.SpellCategoryCooldown[k];
    }
    proto.Bonding = uint32(record.Bonding);
    proto.PageText = uint32(record.PageText);
    proto.LanguageID = uint32(record.LanguageID);
    proto.PageMaterial = uint32(record.PageMaterial);
    proto.StartQuest = uint32(record.StartQuest);
    proto.LockID = uint32(record.LockID);
    proto.RandomProperty = uint32(record.RandomProperty);
    proto.RandomSuffix = uint32(record.RandomSuffix);
    proto.ItemSet = uint32(record.ItemSet);
    proto.Area = uint32(record.ZoneBound);
    proto.Map = uint32(record.InstanceBound);
    proto.BagFamily = uint32(record.BagFamily);
    proto.TotemCategory = uint32(record.TotemCategoryID);
    for (uint32 k = 0; k < MAX_ITEM_PROTO_SOCKETS; ++k)
    {
        proto.Socket[k].Color = uint32(record.SocketColor[k]);
        proto.Socket[k].Content = uint32(record.SocketContent[k]);
    }
    proto.socketBonus = uint32(record.SocketBonus);
    proto.GemProperties = uint32(record.GemProperties);
    proto.ArmorDamageModifier = record.ArmorDamageModifier;
    proto.Duration = uint32(record.Duration);
    proto.ItemLimitCategory = uint32(record.ItemLimitCategory);
    proto.HolidayId = uint32(record.HolidayID);
    proto.StatScalingFactor = record.StatScalingFactor;
    proto.Unknown400_1 = uint32(record.CurrencySubstitutionID);
    proto.Unknown400_2 = uint32(record.CurrencySubstitutionCount);
}

std::set<uint32> ItemSparseOverrides(HotfixData const& hotfixes)
{
    std::set<uint32> entries;
    for (HotfixInfo const& hotfix : hotfixes)
    {
        if (hotfix.Type == HOTFIX_TABLE_ITEM_SPARSE)
        {
            entries.insert(hotfix.Entry);
        }
    }
    return entries;
}