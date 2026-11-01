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

#ifndef MANGOS_LOOTMGR_H
#define MANGOS_LOOTMGR_H

#include <unordered_map>
#include <cmath>
#include <set>
#include "ItemEnchantmentMgr.h"
#include "ByteBuffer.h"
#include "ObjectGuid.h"
#include "Utilities/LinkedReference/RefManager.h"
#include "LootStore.h"

#include <map>
#include <vector>

class Player;
class WorldObject;

#define MAX_NR_LOOT_ITEMS 16
// note: the client can not show more than 16 items total
#define MAX_NR_QUEST_ITEMS 32
// unrelated to the number of quest items shown, just for reserve

enum PermissionTypes
{
    ALL_PERMISSION    = 0,
    GROUP_PERMISSION  = 1,
    MASTER_PERMISSION = 2,
    OWNER_PERMISSION  = 3,                                  // for single player only loots
    NONE_PERMISSION   = 4
};

enum LootType
{
    LOOT_CORPSE                 = 1,
    LOOT_PICKPOCKETING          = 2,
    LOOT_FISHING                = 3,
    LOOT_DISENCHANTING          = 4,
    // ignored always by client
    LOOT_SKINNING               = 6,                        // unsupported by client, sending LOOT_PICKPOCKETING instead
    LOOT_PROSPECTING            = 7,                        // unsupported by client, sending LOOT_PICKPOCKETING instead
    LOOT_MILLING                = 8,

    LOOT_FISHINGHOLE            = 20,                       // unsupported by client, sending LOOT_FISHING instead
    LOOT_FISHING_FAIL           = 21,                       // unsupported by client, sending LOOT_FISHING instead
    LOOT_INSIGNIA               = 22,                        // unsupported by client, sending LOOT_CORPSE instead
    LOOT_MAIL                   = 23,
    LOOT_SPELL                  = 24,
};

enum LootSlotType
{
    LOOT_SLOT_NORMAL  = 0,                                  // can be looted
    LOOT_SLOT_VIEW    = 1,                                  // can be only view (ignore any loot attempts)
    LOOT_SLOT_MASTER  = 2,                                  // can be looted only master (error message)
    LOOT_SLOT_REQS    = 3,                                  // can't be looted (error message about missing reqs)
    LOOT_SLOT_OWNER   = 4,                                  // ignore binding confirmation and etc, for single player looting
    MAX_LOOT_SLOT_TYPE                                      // custom, use for mark skipped from show items
};

struct LootItem
{
    uint32  itemid;
    uint8   type;                                           // 0 = item, 1 = currency
    uint32  randomSuffix;
    int32   randomPropertyId;
    uint32  count;
    uint16  conditionId       : 16;                         // allow compiler pack structure
    bool    currency          : 1;
    bool    is_looted         : 1;
    bool    is_blocked        : 1;
    bool    freeforall        : 1;                          // free for all
    bool    is_underthreshold : 1;
    bool    is_counted        : 1;
    bool    needs_quest       : 1;                          // quest drop

    // Constructor, copies most fields from LootStoreItem, generates random count and random suffixes/properties
    // Should be called for non-reference LootStoreItem entries only (mincountOrRef > 0)
    explicit LootItem(LootStoreItem const& li);

    LootItem(uint32 itemid_, uint8 type_, uint32 count_, uint32 randomSuffix_ = 0, int32 randomPropertyId_ = 0);

    // Basic checks for player/item compatibility - if false no chance to see the item in the loot
    bool AllowedForPlayer(Player const* player, WorldObject const* lootTarget) const;
    LootSlotType GetSlotTypeForSharedLoot(PermissionTypes permission, Player const* viewer, WorldObject const* lootTarget, bool condition_ok = false) const;
};

typedef std::vector<LootItem> LootItemList;

struct QuestItem
{
    uint8   index;                                          // position in quest_items;
    bool    is_looted;

    QuestItem()
        : index(0), is_looted(false) {}

    QuestItem(uint8 _index, bool _islooted = false)
        : index(_index), is_looted(_islooted) {}
};

struct Loot;

typedef std::vector<QuestItem> QuestItemList;
typedef std::map<uint32, QuestItemList*> QuestItemMap;
//=====================================================

class LootValidatorRef :  public Reference<Loot, LootValidatorRef>
{
    public:
        LootValidatorRef() {}
        void targetObjectDestroyLink() override {}
        void sourceObjectDestroyLink() override {}
};

//=====================================================

class LootValidatorRefManager : public RefManager<Loot, LootValidatorRef>
{
    public:
        typedef LinkedListHead::Iterator< LootValidatorRef > iterator;

        LootValidatorRef* getFirst() { return (LootValidatorRef*)RefManager<Loot, LootValidatorRef>::getFirst(); }
        LootValidatorRef* getLast() { return (LootValidatorRef*)RefManager<Loot, LootValidatorRef>::getLast(); }

        iterator begin() { return iterator(getFirst()); }
        iterator end() { return iterator(NULL); }
        iterator rbegin() { return iterator(getLast()); }
        iterator rend() { return iterator(NULL); }
};

//=====================================================
struct LootView;

/**
 * Serializes a loot item entry into a byte buffer.
 */
ByteBuffer& operator<<(ByteBuffer& b, LootItem const& li);

/**
 * Serializes a loot view into a byte buffer.
 */
ByteBuffer& operator<<(ByteBuffer& b, LootView const& lv);

struct Loot
{
        friend ByteBuffer& operator<<(ByteBuffer& b, LootView const& lv);

        QuestItemMap const& GetPlayerCurrencies() const { return m_playerCurrencies; }
        QuestItemMap const& GetPlayerQuestItems() const { return m_playerQuestItems; }
        QuestItemMap const& GetPlayerFFAItems() const { return m_playerFFAItems; }
        QuestItemMap const& GetPlayerNonQuestNonFFANonCurrencyConditionalItems() const { return m_playerNonQuestNonFFANonCurrencyConditionalItems; }

        LootItemList items;
        uint32 gold;
        uint8 unlootedCount;
        LootType loot_type;                                 // required for achievement system

        Loot(WorldObject const* lootTarget, uint32 _gold = 0) : gold(_gold), unlootedCount(0), loot_type(LOOT_CORPSE), m_lootTarget(lootTarget) {}
        ~Loot() { clear(); }

        // if loot becomes invalid this reference is used to inform the listener
        void addLootValidatorRef(LootValidatorRef* pLootValidatorRef)
        {
            m_LootValidatorRefManager.insertFirst(pLootValidatorRef);
        }

        // void clear();
        void clear()
        {
            for (QuestItemMap::const_iterator itr = m_playerCurrencies.begin(); itr != m_playerCurrencies.end(); ++itr)
            {
                delete itr->second;
            }
            m_playerCurrencies.clear();

            for (QuestItemMap::const_iterator itr = m_playerQuestItems.begin(); itr != m_playerQuestItems.end(); ++itr)
            {
                delete itr->second;
            }
            m_playerQuestItems.clear();

            for (QuestItemMap::const_iterator itr = m_playerFFAItems.begin(); itr != m_playerFFAItems.end(); ++itr)
            {
                delete itr->second;
            }
            m_playerFFAItems.clear();

            for (QuestItemMap::const_iterator itr = m_playerNonQuestNonFFANonCurrencyConditionalItems.begin(); itr != m_playerNonQuestNonFFANonCurrencyConditionalItems.end(); ++itr)
            {
                delete itr->second;
            }
            m_playerNonQuestNonFFANonCurrencyConditionalItems.clear();

            m_playersLooting.clear();
            items.clear();
            m_questItems.clear();
            gold = 0;
            unlootedCount = 0;
            m_LootValidatorRefManager.clearReferences();
        }

        bool empty() const { return items.empty() && gold == 0; }
        bool isLooted() const { return gold == 0 && unlootedCount == 0; }

        void NotifyItemRemoved(uint8 lootIndex);
        void NotifyQuestItemRemoved(uint8 questIndex);
        void NotifyMoneyRemoved();
        void AddLooter(ObjectGuid guid) { m_playersLooting.insert(guid); }
        void RemoveLooter(ObjectGuid guid) { m_playersLooting.erase(guid); }

        void generateMoneyLoot(uint32 minAmount, uint32 maxAmount);
        bool FillLoot(uint32 loot_id, LootStore const& store, Player* loot_owner, bool personal, bool noEmptyError = false);

        // Inserts the item into the loot (called by LootTemplate processors)
        void AddItem(LootStoreItem const& item);

        LootItem* LootItemInSlot(uint32 lootslot, Player* player, QuestItem** qitem = NULL, QuestItem** ffaitem = NULL, QuestItem** conditem = NULL, QuestItem** currency = NULL);
        uint32 GetMaxSlotInLootFor(Player* player) const;

        WorldObject const* GetLootTarget() const { return m_lootTarget; }

    private:
        void FillNotNormalLootFor(Player* player);
        QuestItemList* FillCurrencyLoot(Player* player);
        QuestItemList* FillFFALoot(Player* player);
        QuestItemList* FillQuestLoot(Player* player);
        QuestItemList* FillNonQuestNonFFANonCurrencyConditionalLoot(Player* player);

        LootItemList m_questItems;

        GuidSet m_playersLooting;

        QuestItemMap m_playerCurrencies;
        QuestItemMap m_playerQuestItems;
        QuestItemMap m_playerFFAItems;
        QuestItemMap m_playerNonQuestNonFFANonCurrencyConditionalItems;

        // All rolls are registered here. They need to know, when the loot is not valid anymore
        LootValidatorRefManager m_LootValidatorRefManager;

        // What is looted
        WorldObject const* m_lootTarget;
};

/// Whether the loot `loot_id` of `store` holds an entry for an active quest of the player.
bool HaveQuestLootForPlayer(LootStore const& store, uint32 loot_id, Player* player);
/// Whether the loot `loot_id` of `store` holds a conditional entry the player meets.
bool HaveConditionalLootForPlayer(LootStore const& store, uint32 loot_id, Player* player, WorldObject const* lootTarget);

struct LootView
{
    Loot& loot;
    Player const* viewer;
    PermissionTypes permission;
    LootView(Loot& _loot, Player const* _viewer, PermissionTypes _permission = ALL_PERMISSION)
        : loot(_loot), viewer(_viewer), permission(_permission) {}
};

#endif
