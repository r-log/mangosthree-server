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

#include "LootMgr.h"
#include "Log.h"
#include "Group.h"
#include "ObjectMgr.h"
#include "Item.h"
#include "Player.h"
#include "data/GameConfig.h"
#include "Util.h"
#include "SharedDefines.h"
#include "DBCStores.h"
#include "PlayerRegistry.h"
#include "PlayerConditions.h"

static void ProcessLootTemplate(LootTemplate const& lootTemplate, Loot& loot, LootStore const& store, bool rate, uint8 groupId);
static bool HasQuestDropForPlayer(LootTemplate const& lootTemplate, LootTemplateMap const& store, Player const* player, uint8 groupId = 0);
static bool HasConditionalDropForPlayer(LootTemplate const& lootTemplate, LootTemplateMap const& store, Player const* player, WorldObject const* lootTarget, uint8 groupId = 0);


/**
 * @brief Checks whether a loot template contains quest loot visible to a player.
 *
 * @param store The loot store.
 * @param loot_id The loot template identifier.
 * @param player The player to evaluate.
 * @return true if the player can receive quest loot from the template; otherwise, false.
 */
bool HaveQuestLootForPlayer(LootStore const& store, uint32 loot_id, Player* player)
{
    LootTemplateMap::const_iterator tab = store.GetTemplates().find(loot_id);
    if (tab != store.GetTemplates().end())
        if (HasQuestDropForPlayer(*tab->second, store.GetTemplates(), player))
        {
            return true;
        }

    return false;
}

bool HaveConditionalLootForPlayer(LootStore const& store, uint32 loot_id, Player* player, WorldObject const* lootTarget)
{
    if (!player)
    {
        return false;
    }

    LootTemplateMap::const_iterator itr = store.GetTemplates().find(loot_id);
    if (itr == store.GetTemplates().end())
    {
        return false;
    }

    return HasConditionalDropForPlayer(*itr->second, store.GetTemplates(), player, lootTarget);
}

//
// --------- LootItem ---------
//

// Constructor, copies most fields from LootStoreItem and generates random count
LootItem::LootItem(LootStoreItem const& li)
{
    itemid      = li.itemid;
    type        = li.type;
    conditionId = li.conditionId;
    currency    = type == LOOT_ITEM_TYPE_CURRENCY;
    count       = urand(li.mincountOrRef, li.maxcount);     // constructor called for mincountOrRef > 0 only

    is_looted = 0;
    is_blocked = 0;
    is_underthreshold = 0;
    is_counted = 0;

    if (currency)
    {
        freeforall = false;
        needs_quest = false;
        randomSuffix = 0;
        randomPropertyId = 0;
        count = uint32(count * sGameConfig.getConfig(CONFIG_FLOAT_RATE_DROP_CURRENCY_AMOUNT));
    }
    else
    {
        ItemPrototype const* proto = ObjectMgr::GetItemPrototype(itemid);
        freeforall  = proto && (proto->Flags & ITEM_FLAG_PARTY_LOOT);
        needs_quest = li.needs_quest;
        randomSuffix = GenerateEnchSuffixFactor(itemid);
        randomPropertyId = Item::GenerateItemRandomPropertyId(itemid);
    }
}

/**
 * @brief Creates a loot item from explicit item values.
 *
 * @param itemid_ The item entry.
 * @param count_ The stack count.
 * @param randomPropertyId_ The random property identifier.
 */
LootItem::LootItem(uint32 itemid_, uint8 type_, uint32 count_, uint32 randomSuffix_, int32 randomPropertyId_)
{
    itemid      = itemid_;
    type        = type_;
    conditionId = 0;
    count       = count_;
    randomSuffix = randomSuffix_;
    randomPropertyId = randomPropertyId_;
    is_looted = 0;
    is_blocked = 0;
    is_underthreshold = 0;
    is_counted = 0;
    currency = type == LOOT_ITEM_TYPE_CURRENCY;
    needs_quest = false;

    if (currency)
    {
        freeforall = false;
    }
    else
    {
        ItemPrototype const* proto = ObjectMgr::GetItemPrototype(itemid);
        freeforall  = proto && (proto->Flags & ITEM_FLAG_PARTY_LOOT);
    }
}

// Basic checks for player/item compatibility - if false no chance to see the item in the loot
bool LootItem::AllowedForPlayer(Player const* player, WorldObject const* lootTarget) const
{
    // DB conditions check
    if (conditionId && !IsPlayerMeetToCondition(conditionId, player, player->GetMap(), lootTarget, CONDITION_FROM_LOOT))
    {
        return false;
    }

    if (type == LOOT_ITEM_TYPE_ITEM)
    {
        ItemPrototype const* pProto = ObjectMgr::GetItemPrototype(itemid);
        if (!pProto)
        {
            return false;
        }

        // not show loot for not own team
        if ((pProto->Flags2 & ITEM_FLAG2_HORDE_ONLY) && player->GetTeam() != HORDE)
        {
            return false;
        }

        if ((pProto->Flags2 & ITEM_FLAG2_ALLIANCE_ONLY) && player->GetTeam() != ALLIANCE)
        {
            return false;
        }

        if (needs_quest)
        {
            // Checking quests for quest-only drop (check only quests requirements in this case)
            if (!player->HasQuestForItem(itemid))
            {
                return false;
            }
        }
        else
        {
            // Not quest only drop (check quest starting items for already accepted non-repeatable quests)
            if (pProto->StartQuest && player->GetQuestStatusMgr().GetQuestStatus(pProto->StartQuest) != QUEST_STATUS_NONE && !player->HasQuestForItem(itemid))
            {
                return false;
            }
        }
    }
    else if (type == LOOT_ITEM_TYPE_CURRENCY)
    {
        CurrencyTypesEntry const * currency = sCurrencyTypesStore.LookupEntry(itemid);
        if (!itemid)
        {
            return false;
        }

        if (!player->isGameMaster())
        {
            if (currency->CategoryID == CURRENCY_CATEGORY_META)
            {
                return false;
            }

            if (currency->CategoryID == CURRENCY_CATEGORY_ARCHAEOLOGY && !player->HasSkill(SKILL_ARCHAEOLOGY))
            {
                return false;
            }
        }
    }

    return true;
}

/**
 * @brief Gets the slot visibility type for shared-loot rules.
 *
 * @param permission The viewer's loot permission.
 * @param viewer The player viewing the loot.
 * @param lootTarget The looted world object.
 * @param condition_ok true if loot conditions were already validated.
 * @return The slot type visible to the viewer.
 */
LootSlotType LootItem::GetSlotTypeForSharedLoot(PermissionTypes permission, Player const* viewer, WorldObject const* lootTarget, bool condition_ok /*= false*/) const
{
    // ignore currencies, looted items, FFA (each player get own copy) and not allowed items
    if (currency || is_looted || freeforall || (conditionId && !condition_ok) || !AllowedForPlayer(viewer, lootTarget))
    {
        return MAX_LOOT_SLOT_TYPE;
    }

    switch (permission)
    {
        case ALL_PERMISSION:
            return LOOT_SLOT_NORMAL;
        case GROUP_PERMISSION:
            return (is_blocked || is_underthreshold) ? LOOT_SLOT_NORMAL : LOOT_SLOT_VIEW;
        case MASTER_PERMISSION:
            return !is_underthreshold ? LOOT_SLOT_MASTER : LOOT_SLOT_NORMAL;
        case OWNER_PERMISSION:
            return LOOT_SLOT_OWNER;
        default:
            return MAX_LOOT_SLOT_TYPE;
    }
}

//
// --------- Loot ---------
//

// Inserts the item into the loot (called by LootTemplate processors)
/**
 * @brief Adds a generated loot store item to the loot container.
 *
 * @param item The loot store item being inserted.
 */
void Loot::AddItem(LootStoreItem const& item)
{
    if (item.needs_quest)                                   // Quest drop
    {
        if (m_questItems.size() < MAX_NR_QUEST_ITEMS)
        {
            m_questItems.push_back(LootItem(item));
        }
    }
    else if (items.size() < MAX_NR_LOOT_ITEMS)              // Non-quest drop
    {
        items.push_back(LootItem(item));

        // non-conditional one-player only items are counted here,
        // currencies are counter in FillCurrencyLoot,
        // free for all items are counted in FillFFALoot(),
        // non-ffa conditionals are counted in FillNonQuestNonFFANonCurrencyConditionalLoot()
        if (!item.conditionId && item.type == LOOT_ITEM_TYPE_ITEM)
        {
            ItemPrototype const* proto = ObjectMgr::GetItemPrototype(item.itemid);
            if (!proto || !(proto->Flags & ITEM_FLAG_PARTY_LOOT))
            {
                ++unlootedCount;
            }
        }
    }
}

// Calls processor of corresponding LootTemplate (which handles everything including references)
/**
 * @brief Fills a loot container from a loot template.
 *
 * @param loot_id The loot template identifier.
 * @param store The loot store to resolve the template from.
 * @param loot_owner The player receiving initial loot rights.
 * @param personal true for personal loot handling.
 * @param noEmptyError true to suppress missing-template errors.
 * @return true if loot processing succeeded; otherwise, false.
 */
bool Loot::FillLoot(uint32 loot_id, LootStore const& store, Player* loot_owner, bool personal, bool noEmptyError)
{
    // Must be provided
    if (!loot_owner)
    {
        return false;
    }

    LootTemplate const* tab = store.GetLootFor(loot_id);

    if (!tab)
    {
        if (!noEmptyError)
        {
            sLog.outErrorDb("Table '%s' loot id #%u used but it doesn't have records.", store.GetName(), loot_id);
        }
        return false;
    }

    items.reserve(MAX_NR_LOOT_ITEMS);
    m_questItems.reserve(MAX_NR_QUEST_ITEMS);

    ProcessLootTemplate(*tab, *this, store, store.IsRatesAllowed(), 0);     // Processing is done there, callback via Loot::AddItem()

    // Setting access rights for group loot case
    Group* pGroup = loot_owner->GetGroup();
    if (!personal && pGroup)
    {
        for (GroupReference* itr = pGroup->GetFirstMember(); itr != NULL; itr = itr->next())
            if (Player* pl = itr->getSource())
            {
                FillNotNormalLootFor(pl);
            }
    }
    // ... for personal loot
    else
    {
        FillNotNormalLootFor(loot_owner);
    }

    return true;
}

/**
 * @brief Initializes non-standard loot views for a player.
 *
 * @param pl The player whose personal loot mappings are prepared.
 */
void Loot::FillNotNormalLootFor(Player* pl)
{
    uint32 plguid = pl->GetGUIDLow();

    QuestItemMap::const_iterator qmapitr = m_playerQuestItems.find(plguid);
    if (qmapitr == m_playerQuestItems.end())
    {
        FillQuestLoot(pl);
    }

    qmapitr = m_playerCurrencies.find(plguid);
    if (qmapitr == m_playerCurrencies.end())
    {
        FillCurrencyLoot(pl);
    }


    qmapitr = m_playerFFAItems.find(plguid);
    if (qmapitr == m_playerFFAItems.end())
    {
        FillFFALoot(pl);
    }

    qmapitr = m_playerNonQuestNonFFANonCurrencyConditionalItems.find(plguid);
    if (qmapitr == m_playerNonQuestNonFFANonCurrencyConditionalItems.end())
    {
        FillNonQuestNonFFANonCurrencyConditionalLoot(pl);
    }
}

QuestItemList* Loot::FillCurrencyLoot(Player* player)
{
    QuestItemList* ql = new QuestItemList();

    for (uint8 i = 0; i < items.size(); ++i)
    {
        LootItem& item = items[i];
        if (!item.is_looted && item.currency && item.AllowedForPlayer(player, m_lootTarget))
        {
            ql->push_back(QuestItem(i));
            ++unlootedCount;
        }
    }
    if (ql->empty())
    {
        delete ql;
        return NULL;
    }

    m_playerCurrencies[player->GetGUIDLow()] = ql;
    return ql;
}

/**
 * @brief Builds the free-for-all loot list for a player.
 *
 * @param player The player receiving the free-for-all view.
 * @return Pointer to the generated quest-item list, or NULL if empty.
 */
QuestItemList* Loot::FillFFALoot(Player* player)
{
    QuestItemList* ql = new QuestItemList();

    for (uint8 i = 0; i < items.size(); ++i)
    {
        LootItem& item = items[i];
        if (!item.is_looted && item.freeforall && item.AllowedForPlayer(player, m_lootTarget))
        {
            ql->push_back(QuestItem(i));
            ++unlootedCount;
        }
    }
    if (ql->empty())
    {
        delete ql;
        return NULL;
    }

    m_playerFFAItems[player->GetGUIDLow()] = ql;
    return ql;
}

/**
 * @brief Builds the quest loot list for a player.
 *
 * @param player The player receiving the quest loot view.
 * @return Pointer to the generated quest-item list, or NULL if empty.
 */
QuestItemList* Loot::FillQuestLoot(Player* player)
{
    if (items.size() == MAX_NR_LOOT_ITEMS)
    {
        return NULL;
    }
    QuestItemList* ql = new QuestItemList();

    for (uint8 i = 0; i < m_questItems.size(); ++i)
    {
        LootItem& item = m_questItems[i];
        if (!item.is_looted && item.AllowedForPlayer(player, m_lootTarget))
        {
            ql->push_back(QuestItem(i));

            // questitems get blocked when they first apper in a
            // player's quest vector
            //
            // increase once if one looter only, looter-times if free for all
            if (item.freeforall || !item.is_blocked)
            {
                ++unlootedCount;
            }

            item.is_blocked = true;

            if (items.size() + ql->size() == MAX_NR_LOOT_ITEMS)
            {
                break;
            }
        }
    }
    if (ql->empty())
    {
        delete ql;
        return NULL;
    }

    m_playerQuestItems[player->GetGUIDLow()] = ql;
    return ql;
}

/**
 * @brief Builds non-quest conditional loot entries for a player.
 *
 * @param player The player receiving the conditional loot view.
 * @return Pointer to the generated item list, or NULL if empty.
 */
QuestItemList* Loot::FillNonQuestNonFFANonCurrencyConditionalLoot(Player* player)
{
    QuestItemList* ql = new QuestItemList();

    for (uint8 i = 0; i < items.size(); ++i)
    {
        LootItem& item = items[i];
        if (!item.is_looted && !item.freeforall && !item.currency && item.conditionId && item.AllowedForPlayer(player, m_lootTarget))
        {
            ql->push_back(QuestItem(i));
            if (!item.is_counted)
            {
                ++unlootedCount;
                item.is_counted = true;
            }
        }
    }
    if (ql->empty())
    {
        delete ql;
        return NULL;
    }

    m_playerNonQuestNonFFANonCurrencyConditionalItems[player->GetGUIDLow()] = ql;
    return ql;
}

//===================================================

/**
 * @brief Notifies current looters that a loot item was removed.
 *
 * @param lootIndex The removed loot slot index.
 */
void Loot::NotifyItemRemoved(uint8 lootIndex)
{
    // notify all players that are looting this that the item was removed
    // convert the index to the slot the player sees
    GuidSet::iterator i_next;
    for (GuidSet::iterator i = m_playersLooting.begin(); i != m_playersLooting.end(); i = i_next)
    {
        i_next = i;
        ++i_next;
        if (Player* pl = sPlayerRegistry.Find(*i))
        {
            pl->SendNotifyLootItemRemoved(lootIndex);
        }
        else
        {
            m_playersLooting.erase(i);
        }
    }
}

/**
 * @brief Notifies current looters that loot money was removed.
 */
void Loot::NotifyMoneyRemoved()
{
    // notify all players that are looting this that the money was removed
    GuidSet::iterator i_next;
    for (GuidSet::iterator i = m_playersLooting.begin(); i != m_playersLooting.end(); i = i_next)
    {
        i_next = i;
        ++i_next;
        if (Player* pl = sPlayerRegistry.Find(*i))
        {
            pl->SendNotifyLootMoneyRemoved();
        }
        else
        {
            m_playersLooting.erase(i);
        }
    }
}

/**
 * @brief Notifies current looters that a quest loot item was removed.
 *
 * @param questIndex The removed quest loot index.
 */
void Loot::NotifyQuestItemRemoved(uint8 questIndex)
{
    // when a free for all questitem is looted
    // all players will get notified of it being removed
    // (other questitems can be looted by each group member)
    // bit inefficient but isnt called often

    GuidSet::iterator i_next;
    for (GuidSet::iterator i = m_playersLooting.begin(); i != m_playersLooting.end(); i = i_next)
    {
        i_next = i;
        ++i_next;
        if (Player* pl = sPlayerRegistry.Find(*i))
        {
            QuestItemMap::const_iterator pq = m_playerQuestItems.find(pl->GetGUIDLow());
            if (pq != m_playerQuestItems.end() && pq->second)
            {
                // find where/if the player has the given item in it's vector
                QuestItemList& pql = *pq->second;

                uint8 j;
                for (j = 0; j < pql.size(); ++j)
                    if (pql[j].index == questIndex)
                    {
                        break;
                    }

                if (j < pql.size())
                {
                    pl->SendNotifyLootItemRemoved(items.size() + j);
                }
            }
        }
        else
        {
            m_playersLooting.erase(i);
        }
    }
}

/**
 * @brief Generates random money loot within a configured range.
 *
 * @param minAmount The minimum money amount.
 * @param maxAmount The maximum money amount.
 */
void Loot::generateMoneyLoot(uint32 minAmount, uint32 maxAmount)
{
    if (maxAmount > 0)
    {
        if (maxAmount <= minAmount)
        {
            gold = uint32(maxAmount * sGameConfig.getConfig(CONFIG_FLOAT_RATE_DROP_MONEY));
        }
        else if ((maxAmount - minAmount) < 32700)
        {
            gold = uint32(urand(minAmount, maxAmount) * sGameConfig.getConfig(CONFIG_FLOAT_RATE_DROP_MONEY));
        }
        else
        {
            gold = uint32(urand(minAmount >> 8, maxAmount >> 8) * sGameConfig.getConfig(CONFIG_FLOAT_RATE_DROP_MONEY)) << 8;
        }
    }
}

/**
 * @brief Retrieves the loot item visible in a slot for a player.
 *
 * @param lootSlot The player-visible loot slot.
 * @param player The player looting.
 * @param qitem Receives quest loot metadata when applicable.
 * @param ffaitem Receives free-for-all loot metadata when applicable.
 * @param conditem Receives conditional loot metadata when applicable.
 * @return Pointer to the loot item, or NULL if unavailable.
 */
LootItem* Loot::LootItemInSlot(uint32 lootSlot, Player* player, QuestItem** qitem, QuestItem** ffaitem, QuestItem** conditem, QuestItem** currency)
{
    LootItem* item = NULL;
    bool is_looted = true;
    if (lootSlot >= items.size())
    {
        uint32 questSlot = lootSlot - items.size();
        QuestItemMap::const_iterator itr = m_playerQuestItems.find(player->GetGUIDLow());
        if (itr != m_playerQuestItems.end() && questSlot < itr->second->size())
        {
            QuestItem* qitem2 = &itr->second->at(questSlot);
            if (qitem)
            {
                *qitem = qitem2;
            }
            item = &m_questItems[qitem2->index];
            is_looted = qitem2->is_looted;
        }
    }
    else
    {
        item = &items[lootSlot];
        is_looted = item->is_looted;
        if (item->currency)
        {
            QuestItemMap::const_iterator itr = m_playerCurrencies.find(player->GetGUIDLow());
            if (itr != m_playerCurrencies.end())
            {
                for (QuestItemList::const_iterator iter = itr->second->begin(); iter != itr->second->end(); ++iter)
                {
                    if (iter->index == lootSlot)
                    {
                        QuestItem* currency2 = (QuestItem*) & (*iter);
                        if (currency)
                        {
                            *currency = currency2;
                        }
                        is_looted = currency2->is_looted;
                        break;
                    }
                }
            }
        }
        else if (item->freeforall)
        {
            QuestItemMap::const_iterator itr = m_playerFFAItems.find(player->GetGUIDLow());
            if (itr != m_playerFFAItems.end())
            {
                for (QuestItemList::const_iterator iter = itr->second->begin(); iter != itr->second->end(); ++iter)
                    if (iter->index == lootSlot)
                    {
                        QuestItem* ffaitem2 = (QuestItem*) & (*iter);
                        if (ffaitem)
                        {
                            *ffaitem = ffaitem2;
                        }
                        is_looted = ffaitem2->is_looted;
                        break;
                    }
            }
        }
        else if (item->conditionId)
        {
            QuestItemMap::const_iterator itr = m_playerNonQuestNonFFANonCurrencyConditionalItems.find(player->GetGUIDLow());
            if (itr != m_playerNonQuestNonFFANonCurrencyConditionalItems.end())
            {
                for (QuestItemList::const_iterator iter = itr->second->begin(); iter != itr->second->end(); ++iter)
                {
                    if (iter->index == lootSlot)
                    {
                        QuestItem* conditem2 = (QuestItem*) & (*iter);
                        if (conditem)
                        {
                            *conditem = conditem2;
                        }
                        is_looted = conditem2->is_looted;
                        break;
                    }
                }
            }
        }
    }

    if (is_looted)
    {
        return NULL;
    }

    return item;
}

/**
 * @brief Gets the maximum visible loot slot count for a player.
 *
 * @param player The player viewing the loot.
 * @return The number of visible loot slots.
 */
uint32 Loot::GetMaxSlotInLootFor(Player* player) const
{
    QuestItemMap::const_iterator itr = m_playerQuestItems.find(player->GetGUIDLow());
    return items.size() + (itr != m_playerQuestItems.end() ?  itr->second->size() : 0);
}

/**
 * @brief Serializes a loot item entry into a byte buffer.
 *
 * @param b The output byte buffer.
 * @param li The loot item view.
 * @return ByteBuffer& The populated byte buffer.
 */
ByteBuffer& operator<<(ByteBuffer& b, LootItem const& li)
{
    if (li.type == LOOT_ITEM_TYPE_ITEM)
    {
        b << uint32(li.itemid);
        b << uint32(li.count);                                  // nr of items of this type
        b << uint32(ObjectMgr::GetItemPrototype(li.itemid)->DisplayInfoID);
        b << uint32(li.randomSuffix);
        b << uint32(li.randomPropertyId);
    }
    else if (li.type == LOOT_ITEM_TYPE_CURRENCY)
    {
        b << uint32(li.itemid);
        b << uint32(li.count);
    }
    // b << uint8(0);                                       // slot type - will send after this function call
    return b;
}

/**
 * @brief Serializes a loot view for a specific viewer into a byte buffer.
 *
 * @param b The output byte buffer.
 * @param lv The loot view to serialize.
 * @return ByteBuffer& The populated byte buffer.
 */
ByteBuffer& operator<<(ByteBuffer& b, LootView const& lv)
{
    if (lv.permission == NONE_PERMISSION)
    {
        b << uint32(0);                                     // gold
        b << uint8(0);                                      // item count
        b << uint8(0);                                      // currency count
        return b;
    }

    Loot& l = lv.loot;

    uint8 itemsShown = 0;
    uint8 currenciesShown = 0;

    // gold
    b << uint32(l.gold);

    size_t count_pos = b.wpos();                            // pos of item count byte
    b << uint8(0);                                          // item count placeholder
    size_t currency_count_pos = b.wpos();                   // pos of currency count byte
    b << uint8(0);                                          // currency count placeholder

    for (uint8 i = 0; i < l.items.size(); ++i)
    {
        LootSlotType slot_type = l.items[i].GetSlotTypeForSharedLoot(lv.permission, lv.viewer, l.GetLootTarget());
        if (slot_type >= MAX_LOOT_SLOT_TYPE)
        {
            continue;
        }

        b << uint8(i) << l.items[i];
        b << uint8(slot_type);                              // 0 - get 1 - look only 2 - master selection
        ++itemsShown;
    }

    QuestItemMap const& lootPlayerNonQuestNonFFAConditionalItems = l.GetPlayerNonQuestNonFFANonCurrencyConditionalItems();
    QuestItemMap::const_iterator nn_itr = lootPlayerNonQuestNonFFAConditionalItems.find(lv.viewer->GetGUIDLow());
    if (nn_itr != lootPlayerNonQuestNonFFAConditionalItems.end())
    {
        QuestItemList* conditional_list =  nn_itr->second;
        for (QuestItemList::const_iterator ci = conditional_list->begin() ; ci != conditional_list->end(); ++ci)
        {
            LootItem& item = l.items[ci->index];

            LootSlotType slot_type = item.GetSlotTypeForSharedLoot(lv.permission, lv.viewer, l.GetLootTarget(), !ci->is_looted);
            if (slot_type >= MAX_LOOT_SLOT_TYPE)
            {
                continue;
            }

            b << uint8(ci->index) << item;
            b << uint8(slot_type);                          // allow loot
            ++itemsShown;
        }
    }

    // in next cases used same slot type for all items
    LootSlotType slot_type = lv.permission == OWNER_PERMISSION ? LOOT_SLOT_OWNER : LOOT_SLOT_NORMAL;

    QuestItemMap const& lootPlayerQuestItems = l.GetPlayerQuestItems();
    QuestItemMap::const_iterator q_itr = lootPlayerQuestItems.find(lv.viewer->GetGUIDLow());
    if (q_itr != lootPlayerQuestItems.end())
    {
        QuestItemList* q_list = q_itr->second;
        for (QuestItemList::const_iterator qi = q_list->begin() ; qi != q_list->end(); ++qi)
        {
            LootItem& item = l.m_questItems[qi->index];
            if (!qi->is_looted && !item.is_looted)
            {
                b << uint8(l.items.size() + (qi - q_list->begin()));
                b << item;
                b << uint8(slot_type);                      // allow loot
                ++itemsShown;
            }
        }
    }

    QuestItemMap const& lootPlayerFFAItems = l.GetPlayerFFAItems();
    QuestItemMap::const_iterator ffa_itr = lootPlayerFFAItems.find(lv.viewer->GetGUIDLow());
    if (ffa_itr != lootPlayerFFAItems.end())
    {
        QuestItemList* ffa_list = ffa_itr->second;
        for (QuestItemList::const_iterator fi = ffa_list->begin() ; fi != ffa_list->end(); ++fi)
        {
            LootItem& item = l.items[fi->index];
            if (!fi->is_looted && !item.is_looted)
            {
                b << uint8(fi->index) << item;
                b << uint8(slot_type);                      // allow loot
                ++itemsShown;
            }
        }
    }

    QuestItemMap const& lootPlayerCurrencies = l.GetPlayerCurrencies();
    QuestItemMap::const_iterator currency_itr = lootPlayerCurrencies.find(lv.viewer->GetGUIDLow());
    if (currency_itr != lootPlayerCurrencies.end())
    {
        QuestItemList* currency_list = currency_itr->second;
        for (QuestItemList::const_iterator ci = currency_list->begin() ; ci != currency_list->end(); ++ci)
        {
            LootItem& item = l.items[ci->index];
            if (!ci->is_looted && !item.is_looted)
            {
                b << uint8(ci->index) << item;
                ++currenciesShown;
            }
        }
    }

    // update number of items and currencies shown
    b.put<uint8>(count_pos, itemsShown);
    b.put<uint8>(currency_count_pos, currenciesShown);

    return b;
}

/**
 * @brief Checks whether the group contains a quest drop relevant to a player.
 *
 * @param player The player to test against.
 * @return true if a relevant quest drop exists; otherwise, false.
 */
static bool HasQuestDropForPlayer(LootTemplate::LootGroup const& group, Player const* player)
{
    for (LootStoreItemList::const_iterator i = group.GetExplicitlyChanced().begin(); i != group.GetExplicitlyChanced().end(); ++i)
        if (player->HasQuestForItem(i->itemid))
        {
            return true;
        }
    for (LootStoreItemList::const_iterator i = group.GetEqualChanced().begin(); i != group.GetEqualChanced().end(); ++i)
        if (player->HasQuestForItem(i->itemid))
        {
            return true;
        }
    return false;
}

// True if group includes at least 1 conditional entry available to the player
static bool HasConditionalDropForPlayer(LootTemplate::LootGroup const& group, Player const* player, WorldObject const* lootTarget)
{
    for (LootStoreItemList::const_iterator i = group.GetExplicitlyChanced().begin(); i != group.GetExplicitlyChanced().end(); ++i)
        if (i->conditionId && IsPlayerMeetToCondition(i->conditionId, player, player->GetMap(), lootTarget, CONDITION_FROM_LOOT))
        {
            return true;
        }
    for (LootStoreItemList::const_iterator i = group.GetEqualChanced().begin(); i != group.GetEqualChanced().end(); ++i)
        if (i->conditionId && IsPlayerMeetToCondition(i->conditionId, player, player->GetMap(), lootTarget, CONDITION_FROM_LOOT))
        {
            return true;
        }
    return false;
}

/**
 * @brief Processes the group and adds the rolled item to loot.
 *
 * @param loot The loot container being filled.
 */
static void ProcessLootGroup(LootTemplate::LootGroup const& group, Loot& loot)
{
    LootStoreItem const* item = group.Roll();
    if (item != NULL)
    {
        loot.AddItem(*item);
    }
}

/**
 * @brief Processes the template and appends rolled items to loot.
 *
 * @param loot The loot container being filled.
 * @param store The loot store owning the template.
 * @param rate true to apply loot rate modifiers.
 * @param groupId Optional specific group identifier for reference processing.
 */
static void ProcessLootTemplate(LootTemplate const& lootTemplate, Loot& loot, LootStore const& store, bool rate, uint8 groupId)
{
    if (groupId)                                            // Group reference uses own processing of the group
    {
        if (groupId > lootTemplate.GetGroups().size())
        {
            return;                                          // Error message already printed at loading stage
        }

        ProcessLootGroup(lootTemplate.GetGroups()[groupId - 1], loot);
        return;
    }

    // Rolling non-grouped items
    for (LootStoreItemList::const_iterator i = lootTemplate.GetEntries().begin() ; i != lootTemplate.GetEntries().end() ; ++i)
    {
        if (!i->Roll(rate))
        {
            continue;                                        // Bad luck for the entry
        }

        if (i->mincountOrRef < 0 && i->type == LOOT_ITEM_TYPE_ITEM)    // References processing
        {
            LootTemplate const* Referenced = LootTemplates_Reference.GetLootFor(-i->mincountOrRef);

            if (!Referenced)
            {
                continue;                                    // Error message already printed at loading stage
            }

            // Check condition
            if (i->conditionId && !IsPlayerMeetToCondition(i->conditionId, NULL, NULL, loot.GetLootTarget(), CONDITION_FROM_REFERING_LOOT))
            {
                continue;
            }

            for (uint32 loop = 0; loop < i->maxcount; ++loop) // Ref multiplicator
            {
                ProcessLootTemplate(*Referenced, loot, store, rate, i->group);
            }
        }
        else                                                // Plain entries (not a reference, not grouped)
        {
            loot.AddItem(*i);                                // Chance is already checked, just add
        }
    }

    // Now processing groups
    for (LootTemplate::LootGroups::const_iterator i = lootTemplate.GetGroups().begin() ; i != lootTemplate.GetGroups().end() ; ++i)
    {
        ProcessLootGroup(*i, loot);
    }
}

/**
 * @brief Checks whether the template contains a quest drop relevant to a player.
 *
 * @param store The loot template map used for references.
 * @param player The player to test against.
 * @param groupId Optional specific group identifier.
 * @return true if a relevant quest drop exists; otherwise, false.
 */
static bool HasQuestDropForPlayer(LootTemplate const& lootTemplate, LootTemplateMap const& store, Player const* player, uint8 groupId)
{
    if (groupId)                                            // Group reference
    {
        if (groupId > lootTemplate.GetGroups().size())
        {
            return false;                                    // Error message already printed at loading stage
        }
        return HasQuestDropForPlayer(lootTemplate.GetGroups()[groupId - 1], player);
    }

    // Checking non-grouped entries
    for (LootStoreItemList::const_iterator i = lootTemplate.GetEntries().begin() ; i != lootTemplate.GetEntries().end() ; ++i)
    {
        if (i->mincountOrRef < 0)                           // References processing
        {
            LootTemplateMap::const_iterator Referenced = store.find(-i->mincountOrRef);
            if (Referenced == store.end())
            {
                continue;                                    // Error message already printed at loading stage
            }
            if (HasQuestDropForPlayer(*Referenced->second, store, player, i->group))
            {
                return true;
            }
        }
        else if (player->HasQuestForItem(i->itemid))
        {
            return true;                                     // active quest drop found
        }
    }

    // Now checking groups
    for (LootTemplate::LootGroups::const_iterator i = lootTemplate.GetGroups().begin(); i != lootTemplate.GetGroups().end(); ++i)
        if (HasQuestDropForPlayer(*i, player))
        {
            return true;
        }

    return false;
}

// True if template includes at least 1 conditional entry available to the player
static bool HasConditionalDropForPlayer(LootTemplate const& lootTemplate, LootTemplateMap const& store, Player const* player, WorldObject const* lootTarget, uint8 groupId)
{
    if (!player)
    {
        return false;
    }

    if (groupId)                                            // Group reference
    {
        if (groupId > lootTemplate.GetGroups().size())
        {
            return false;                                    // Error message already printed at loading stage
        }
        return HasConditionalDropForPlayer(lootTemplate.GetGroups()[groupId - 1], player, lootTarget);
    }

    for (LootStoreItemList::const_iterator i = lootTemplate.GetEntries().begin(); i != lootTemplate.GetEntries().end(); ++i)
    {
        if (i->mincountOrRef < 0)                           // References
        {
            if (i->conditionId)
            {
                if (IsPlayerMeetToCondition(i->conditionId, player, player->GetMap(), lootTarget, CONDITION_FROM_REFERING_LOOT))
                {
                    return true;
                }

                continue;
            }

            LootTemplateMap::const_iterator Referenced = store.find(-i->mincountOrRef);
            if (Referenced == store.end())
            {
                continue;                                    // Error message already printed at loading stage
            }
            if (HasConditionalDropForPlayer(*Referenced->second, store, player, lootTarget, i->group))
            {
                return true;
            }
        }
        else if (i->conditionId && IsPlayerMeetToCondition(i->conditionId, player, player->GetMap(), lootTarget, CONDITION_FROM_LOOT))
        {
            return true;
        }
    }

    for (LootTemplate::LootGroups::const_iterator i = lootTemplate.GetGroups().begin(); i != lootTemplate.GetGroups().end(); ++i)
        if (HasConditionalDropForPlayer(*i, player, lootTarget))
        {
            return true;
        }

    return false;
}
