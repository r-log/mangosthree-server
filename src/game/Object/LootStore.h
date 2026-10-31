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

#ifndef MANGOS_H_LOOTSTORE
#define MANGOS_H_LOOTSTORE

#include <cmath>
#include <set>
#include <unordered_map>
#include <vector>
#include "Platform/Define.h"

class LootStore;
class LootTemplate;

/// The loot tables: the *_loot_template stores, their templates, groups and entries, the rolls of an
/// entry and of a group, and their loaders and load checks. LootMgr.h holds what is looted.

enum LootItemType
{
    LOOT_ITEM_TYPE_ITEM         = 0,
    LOOT_ITEM_TYPE_CURRENCY     = 1,
    LOOTITEM_TYPE_CONDITIONNAL  = 3,
    LOOTITEM_TYPE_CURRENCY      = 4,
};

struct LootStoreItem
{
    uint32  itemid;                                         // id of the item
    uint8   type;                                           // 0 = item, 1 = currency
    float   chance;                                         // always positive, chance to drop for both quest and non-quest items, chance to be used for refs
    int32   mincountOrRef;                                  // mincount for drop items (positive) or minus referenced TemplateleId (negative)
    uint8   maxcount;                                       // max drop count for the item (mincountOrRef positive) or Ref multiplicator (mincountOrRef negative)
    uint8   group       : 7;
    bool    needs_quest : 1;                                // quest drop (negative ChanceOrQuestChance in DB)
    uint16  conditionId : 16;                               // additional loot condition Id

    // Constructor, converting ChanceOrQuestChance -> (chance, needs_quest)
    // displayid is filled in IsValid() which must be called after
    LootStoreItem(uint32 _itemid, uint8 _type, float _chanceOrQuestChance, int8 _group, uint16 _conditionId, int32 _mincountOrRef, uint32 _maxcount)
        : itemid(_itemid), type(_type), chance(fabs(_chanceOrQuestChance)), mincountOrRef(_mincountOrRef),
          maxcount(_maxcount), group(_group), needs_quest(_chanceOrQuestChance < 0), conditionId(_conditionId)
    {}

    bool Roll(bool rate) const;                             // Checks if the entry takes it's chance (at loot generation)
    bool IsValid(LootStore const& store, uint32 entry) const;
    // Checks correctness of values
};

typedef std::vector<LootStoreItem> LootStoreItemList;
typedef std::unordered_map<uint32, LootTemplate*> LootTemplateMap;

typedef std::set<uint32> LootIdSet;

class LootStore
{
    public:
        explicit LootStore(char const* name, char const* entryName, bool ratesAllowed)
            : m_name(name), m_entryName(entryName), m_ratesAllowed(ratesAllowed) {}
        virtual ~LootStore() { Clear(); }

        void Verify() const;

        void LoadAndCollectLootIds(LootIdSet& ids_set);
        void CheckLootRefs(LootIdSet* ref_set = NULL) const;// check existence reference and remove it from ref_set
        void ReportUnusedIds(LootIdSet const& ids_set) const;
        void ReportNotExistedId(uint32 id) const;

        bool HaveLootFor(uint32 loot_id) const { return m_LootTemplates.find(loot_id) != m_LootTemplates.end(); }
        bool HaveQuestLootFor(uint32 loot_id) const;
        bool HaveConditionalLootFor(uint32 loot_id) const;

        LootTemplate const* GetLootFor(uint32 loot_id) const;

        char const* GetName() const { return m_name; }
        char const* GetEntryName() const { return m_entryName; }
        bool IsRatesAllowed() const { return m_ratesAllowed; }
        LootTemplateMap const& GetTemplates() const { return m_LootTemplates; }
    protected:
        void LoadLootTable();
        void Clear();
    private:
        LootTemplateMap m_LootTemplates;
        char const* m_name;
        char const* m_entryName;
        bool m_ratesAllowed;
};

class LootTemplate
{
    public:
        class LootGroup;                                   // A set of loot definitions for items (refs are not allowed inside)
        typedef std::vector<LootGroup> LootGroups;

        // Adds an entry to the group (at loading stage)
        void AddEntry(LootStoreItem& item);

        // True if template includes at least 1 quest drop entry
        bool HasQuestDrop(LootTemplateMap const& store, uint8 GroupId = 0) const;
        bool HasConditionalDrop(LootTemplateMap const& store, uint8 groupId = 0) const;

        // Checks integrity of the template
        void Verify(LootStore const& store, uint32 Id) const;
        void CheckLootRefs(LootIdSet* ref_set) const;

        LootStoreItemList const& GetEntries() const { return Entries; }
        LootGroups const& GetGroups() const { return Groups; }
    private:
        LootStoreItemList Entries;                          // not grouped only
        LootGroups        Groups;                           // groups have own (optimised) processing, grouped entries go there
};

class LootTemplate::LootGroup                               // A set of loot definitions for items (refs are not allowed)
{
    public:
        void AddEntry(LootStoreItem& item);                 // Adds an entry to the group (at loading stage)
        bool HasQuestDrop() const;                          // True if group includes at least 1 quest drop entry
        bool HasConditionalDrop() const;                    // True if group includes at least 1 conditional entry
        float RawTotalChance() const;                       // Overall chance for the group (without equal chanced items)
        float TotalChance() const;                          // Overall chance for the group

        void Verify(LootStore const& lootstore, uint32 id, uint32 group_id) const;
        void CollectLootIds(LootIdSet& set) const;
        void CheckLootRefs(LootIdSet* ref_set) const;
        LootStoreItem const* Roll() const;                  // Rolls an item from the group, returns NULL if all miss their chances

        LootStoreItemList const& GetExplicitlyChanced() const { return ExplicitlyChanced; }
        LootStoreItemList const& GetEqualChanced() const { return EqualChanced; }
    private:
        LootStoreItemList ExplicitlyChanced;                // Entries with chances defined in DB
        LootStoreItemList EqualChanced;                     // Zero chances - every entry takes the same chance
};

extern LootStore LootTemplates_Creature;
extern LootStore LootTemplates_Fishing;
extern LootStore LootTemplates_Gameobject;
extern LootStore LootTemplates_Item;
extern LootStore LootTemplates_Mail;
extern LootStore LootTemplates_Milling;
extern LootStore LootTemplates_Pickpocketing;
extern LootStore LootTemplates_Skinning;
extern LootStore LootTemplates_Disenchant;
extern LootStore LootTemplates_Prospecting;
extern LootStore LootTemplates_Reference;
extern LootStore LootTemplates_Spell;

/**
 * Loads creature loot templates.
 */
void LoadLootTemplates_Creature();

/**
 * Loads fishing loot templates.
 */
void LoadLootTemplates_Fishing();

/**
 * Loads gameobject loot templates.
 */
void LoadLootTemplates_Gameobject();

/**
 * Loads item loot templates.
 */
void LoadLootTemplates_Item();

/**
 * Loads mail loot templates.
 */
void LoadLootTemplates_Mail();

/**
 * Loads milling loot templates.
 */
void LoadLootTemplates_Milling();

/**
 * Loads pickpocketing loot templates.
 */
void LoadLootTemplates_Pickpocketing();

/**
 * Loads skinning loot templates.
 */
void LoadLootTemplates_Skinning();

/**
 * Loads disenchant loot templates.
 */
void LoadLootTemplates_Disenchant();
void LoadLootTemplates_Prospecting();

void LoadLootTemplates_Spell();

/**
 * Loads reference loot templates used by other loot tables.
 */
void LoadLootTemplates_Reference();

inline void LoadLootTables()
{
    LoadLootTemplates_Creature();
    LoadLootTemplates_Fishing();
    LoadLootTemplates_Gameobject();
    LoadLootTemplates_Item();
    LoadLootTemplates_Mail();
    LoadLootTemplates_Milling();
    LoadLootTemplates_Pickpocketing();
    LoadLootTemplates_Skinning();
    LoadLootTemplates_Disenchant();
    LoadLootTemplates_Prospecting();
    LoadLootTemplates_Spell();

    LoadLootTemplates_Reference();
}

#endif
