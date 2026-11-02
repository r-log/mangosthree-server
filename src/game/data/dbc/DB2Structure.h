/*
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

#ifndef MANGOS_DB2STRUCTURE_H
#define MANGOS_DB2STRUCTURE_H

#include "Utilities/Errors.h"
#include "DBCEnums.h"
#include "Path.h"
#include "Platform/Define.h"

#include <map>
#include <set>
#include <vector>

// Structures using to access raw DB2 data and required packing to portability: the loader writes each field
// right after the one before it, so a string pointer can follow an odd number of 4-byte fields.
#if defined( __GNUC__ )
#pragma pack(1)
#else
#pragma pack(push,1)
#endif

struct ItemEntry
{
   uint32   ID;                                             // 0
   uint32   Class;                                          // 1
   uint32   SubClass;                                       // 2
   int32    SoundOverrideSubclass;                                           // 3
   int32    Material;                                       // 4
   uint32   DisplayId;                                      // 5
   uint32   InventoryType;                                  // 6
   uint32   Sheath;                                         // 7
};

typedef char const* const* DB2String;                       // char const* strings[MAX_LOCALE]

#define MAX_ITEM_SPARSE_STATS           10
#define MAX_ITEM_SPARSE_SPELLS          5
#define MAX_ITEM_SPARSE_NAMES           4
#define MAX_ITEM_SPARSE_SOCKETS         3

// Item-sparse.db2: the client's full record of each item it ships, in the order of its ItemRecSparse.
struct ItemSparseEntry
{
    uint32   ID;                                            // 0
    int32    Quality;                                       // 1
    uint32   Flags[2];                                      // 2-3
    float    PriceRandomValue;                              // 4
    float    PriceVariance;                                 // 5
    int32    BuyCount;                                      // 6
    int32    BuyPrice;                                      // 7
    int32    SellPrice;                                     // 8
    int32    InventoryType;                                 // 9
    int32    AllowableClass;                                // 10
    int32    AllowableRace;                                 // 11
    int32    ItemLevel;                                     // 12
    int32    RequiredLevel;                                 // 13
    int32    RequiredSkill;                                 // 14
    int32    RequiredSkillRank;                             // 15
    int32    RequiredSpell;                                 // 16
    int32    RequiredHonorRank;                             // 17
    int32    RequiredCityRank;                              // 18
    int32    RequiredReputationFaction;                     // 19
    int32    RequiredReputationRank;                        // 20
    int32    MaxCount;                                      // 21
    int32    Stackable;                                     // 22
    int32    ContainerSlots;                                // 23
    int32    StatModifierBonusStat[MAX_ITEM_SPARSE_STATS];  // 24-33
    int32    StatModifierBonusAmount[MAX_ITEM_SPARSE_STATS];// 34-43
    int32    StatPercentEditor[MAX_ITEM_SPARSE_STATS];      // 44-53
    int32    StatPercentageOfSocket[MAX_ITEM_SPARSE_STATS]; // 54-63
    int32    ScalingStatDistribution;                       // 64
    int32    DamageType;                                    // 65
    int32    Delay;                                         // 66
    float    RangedModRange;                                // 67
    int32    SpellID[MAX_ITEM_SPARSE_SPELLS];               // 68-72
    int32    SpellTrigger[MAX_ITEM_SPARSE_SPELLS];          // 73-77
    int32    SpellCharges[MAX_ITEM_SPARSE_SPELLS];          // 78-82
    int32    SpellCooldown[MAX_ITEM_SPARSE_SPELLS];         // 83-87
    int32    SpellCategory[MAX_ITEM_SPARSE_SPELLS];         // 88-92
    int32    SpellCategoryCooldown[MAX_ITEM_SPARSE_SPELLS]; // 93-97
    int32    Bonding;                                       // 98
    DB2String Name[MAX_ITEM_SPARSE_NAMES];                  // 99-102
    DB2String Description;                                  // 103
    int32    PageText;                                      // 104
    int32    LanguageID;                                    // 105
    int32    PageMaterial;                                  // 106
    int32    StartQuest;                                    // 107
    int32    LockID;                                        // 108
    int32    Material;                                      // 109
    int32    SheatheType;                                   // 110
    int32    RandomProperty;                                // 111
    int32    RandomSuffix;                                  // 112
    int32    ItemSet;                                       // 113
    int32    ZoneBound;                                     // 114
    int32    InstanceBound;                                 // 115
    int32    BagFamily;                                     // 116
    int32    TotemCategoryID;                               // 117
    int32    SocketColor[MAX_ITEM_SPARSE_SOCKETS];          // 118-120
    int32    SocketContent[MAX_ITEM_SPARSE_SOCKETS];        // 121-123
    int32    SocketBonus;                                   // 124
    int32    GemProperties;                                 // 125
    float    ArmorDamageModifier;                           // 126
    int32    Duration;                                      // 127
    int32    ItemLimitCategory;                             // 128
    int32    HolidayID;                                     // 129
    float    StatScalingFactor;                             // 130
    int32    CurrencySubstitutionID;                        // 131
    int32    CurrencySubstitutionCount;                     // 132
};

struct ItemCurrencyCostEntry
{
    //uint32 id;                                            // 0
    uint32 ItemID;                                          // 1
};

#define MAX_EXTENDED_COST_ITEMS         5
#define MAX_EXTENDED_COST_CURRENCIES    5

enum ItemExtendedCostFlags
{
    ITEM_EXTENDED_COST_FLAG_UNK                 = 0x01, // guild related
    ITEM_EXTENDED_COST_FLAG_SEASON_IN_INDEX_0   = 0x02, // currency requirements under these indexes require season count
    ITEM_EXTENDED_COST_FLAG_SEASON_IN_INDEX_1   = 0x04,
    ITEM_EXTENDED_COST_FLAG_SEASON_IN_INDEX_2   = 0x08,
    ITEM_EXTENDED_COST_FLAG_SEASON_IN_INDEX_3   = 0x10,
    ITEM_EXTENDED_COST_FLAG_SEASON_IN_INDEX_4   = 0x20,
};

struct ItemExtendedCostEntry
{
    uint32      ID;                                         // 0
    //                                                      // 1        unk, old reqhonorpoints
    //                                                      // 2        unk, old reqarenapoints
    uint32      ArenaBracket;                               // 3        m_arenaBracket
    uint32      ItemID[MAX_EXTENDED_COST_ITEMS];           // 5-8      m_itemID
    uint32      ItemCount[MAX_EXTENDED_COST_ITEMS];      // 9-13     m_itemCount
    uint32      RequiredPersonalArenaRating;                     // 14       m_requiredArenaRating
    //uint32                                                // 15       m_itemPurchaseGroup
    uint32      CurrencyID[MAX_EXTENDED_COST_CURRENCIES];       // 16-20
    uint32      CurrencyCount[MAX_EXTENDED_COST_CURRENCIES]; // 21-25
                                                            // 26       reputation-related
                                                            // 27       reputation-related
    uint32      Flags;                                      // 28
    //                                                      // 29
    //                                                      // 30

    bool IsSeasonCurrencyRequirement(uint32 i) const
    {
        MANGOS_ASSERT(i < MAX_EXTENDED_COST_CURRENCIES);

        // start from ITEM_EXTENDED_COST_FLAG_SEASON_IN_INDEX_0
        return Flags & 1 << (i + 1);
    }
};

#if defined( __GNUC__ )
#pragma pack()
#else
#pragma pack(pop)
#endif

#endif
