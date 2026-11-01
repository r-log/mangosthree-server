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

#include "LootStore.h"

#include "DBCStores.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "ProgressBar.h"
#include "SharedDefines.h"
#include "Util.h"
#include "data/GameConfig.h"
#include "persistence/world/economy/LootDataRepository.h"
#include "spells/data/SpellMgr.h"

static eConfigFloatValues const qualityToRate[MAX_ITEM_QUALITY] =
{
    CONFIG_FLOAT_RATE_DROP_ITEM_POOR,                       // ITEM_QUALITY_POOR
    CONFIG_FLOAT_RATE_DROP_ITEM_NORMAL,                     // ITEM_QUALITY_NORMAL
    CONFIG_FLOAT_RATE_DROP_ITEM_UNCOMMON,                   // ITEM_QUALITY_UNCOMMON
    CONFIG_FLOAT_RATE_DROP_ITEM_RARE,                       // ITEM_QUALITY_RARE
    CONFIG_FLOAT_RATE_DROP_ITEM_EPIC,                       // ITEM_QUALITY_EPIC
    CONFIG_FLOAT_RATE_DROP_ITEM_LEGENDARY,                  // ITEM_QUALITY_LEGENDARY
    CONFIG_FLOAT_RATE_DROP_ITEM_ARTIFACT,                   // ITEM_QUALITY_ARTIFACT
};

LootStore LootTemplates_Creature("creature_loot_template",     "creature entry",                 true);
LootStore LootTemplates_Disenchant("disenchant_loot_template",   "item disenchant id",             true);
LootStore LootTemplates_Fishing("fishing_loot_template",      "area id",                        true);
LootStore LootTemplates_Gameobject("gameobject_loot_template",   "gameobject Lootid",              true);
LootStore LootTemplates_Item("item_loot_template",         "item entry with ITEM_FLAG_LOOTABLE", true);
LootStore LootTemplates_Mail("mail_loot_template",         "mail template id",               false);
LootStore LootTemplates_Milling("milling_loot_template",      "item entry (herb)",              true);
LootStore LootTemplates_Pickpocketing("pickpocketing_loot_template", "creature pickpocket Lootid",     true);
LootStore LootTemplates_Prospecting("prospecting_loot_template",  "item entry (ore)",               true);
LootStore LootTemplates_Reference("reference_loot_template",    "reference id",                   false);
LootStore LootTemplates_Skinning("skinning_loot_template",     "creature skinning id",           true);
LootStore LootTemplates_Spell("spell_loot_template",        "spell id (random item creating)", false);

// Remove all data and free all memory
void LootStore::Clear()
{
    for (LootTemplateMap::const_iterator itr = m_LootTemplates.begin(); itr != m_LootTemplates.end(); ++itr)
    {
        delete itr->second;
    }
    m_LootTemplates.clear();
}

// Checks validity of the loot store
// Actual checks are done within LootTemplate::Verify() which is called for every template
void LootStore::Verify() const
{
    for (LootTemplateMap::const_iterator i = m_LootTemplates.begin(); i != m_LootTemplates.end(); ++i)
    {
        i->second->Verify(*this, i->first);
    }
}

// Loads a *_loot_template DB table into loot store
// All checks of the loaded template are called from here, no error reports at loot generation required
void LootStore::LoadLootTable()
{
    LootTemplateMap::const_iterator tab;
    uint32 count = 0;

    // Clearing store (for reloading case)
    Clear();

    sLog.outString("%s :", GetName());

    std::vector<LootTemplateRow> rows = LootDataRepository::QueryLootTemplate(GetName());

    if (!rows.empty())
    {
        BarGoLink bar(rows.size());

        for (LootTemplateRow const& row : rows)
        {
            bar.step();

            uint32 entry               = row.entry;
            uint32 item                = abs(row.item);
            uint8 type                 = row.item >= 0 ? LOOT_ITEM_TYPE_ITEM : LOOT_ITEM_TYPE_CURRENCY;
            float  chanceOrQuestChance = row.chanceOrQuestChance;
            uint8  group               = row.groupid;
            int32  mincountOrRef       = row.mincountOrRef;
            uint32 maxcount            = row.maxcount;
            uint16 conditionId         = row.conditionId;

            if (type == LOOT_ITEM_TYPE_ITEM && maxcount > std::numeric_limits<uint8>::max())
            {
                sLog.outErrorDb("Table '%s' entry %u item %u: maxcount value (%u) to large. must be less than %u - skipped", GetName(), entry, item, maxcount, uint32(std::numeric_limits<uint8>::max()));
                continue;                                   // error already printed to log/console.
            }

            if (conditionId)
            {
                const PlayerCondition* condition = sObjectMgr.GetCondition(conditionId);
                if (!condition)
                {
                    sLog.outErrorDb("Table `%s` for entry %u, item %u has condition_id %u that does not exist in `conditions`, ignoring", GetName(), entry, item, uint32(conditionId));
                    continue;
                }

                if (mincountOrRef < 0 && !PlayerCondition::CanBeUsedWithoutPlayer(conditionId))
                {
                    sLog.outErrorDb("Table '%s' entry %u mincountOrRef %i < 0 and has condition %u that requires a player and is not supported, skipped", GetName(), entry, mincountOrRef, uint32(conditionId));
                    continue;
                }
            }

            LootStoreItem storeitem = LootStoreItem(item, type, chanceOrQuestChance, group, conditionId, mincountOrRef, maxcount);

            if (!storeitem.IsValid(*this, entry))           // Validity checks
            {
                continue;
            }

            // Looking for the template of the entry
            // often entries are put together
            if (m_LootTemplates.empty() || tab->first != entry)
            {
                // Searching the template (in case template Id changed)
                tab = m_LootTemplates.find(entry);
                if (tab == m_LootTemplates.end())
                {
                    std::pair< LootTemplateMap::iterator, bool > pr = m_LootTemplates.insert(LootTemplateMap::value_type(entry, new LootTemplate));
                    tab = pr.first;
                }
            }
            // else is empty - template Id and iter are the same
            // finally iter refers to already existing or just created <entry, LootTemplate>

            // Adds current row to the template
            tab->second->AddEntry(storeitem);
            ++count;
        }

        Verify();                                           // Checks validity of the loot store

        sLog.outString();
        sLog.outString(">> Loaded %u loot definitions (%zu templates) from table %s", count, m_LootTemplates.size(), GetName());
    }
    else
    {
        sLog.outString();
        sLog.outErrorDb(">> Loaded 0 loot definitions. DB table `%s` is empty.", GetName());
    }
}

/**
 * @brief Checks whether a loot template contains any quest loot.
 *
 * @param loot_id The loot template identifier.
 * @return true if the template has quest loot; otherwise, false.
 */
bool LootStore::HaveQuestLootFor(uint32 loot_id) const
{
    LootTemplateMap::const_iterator itr = m_LootTemplates.find(loot_id);
    if (itr == m_LootTemplates.end())
    {
        return false;
    }

    // scan loot for quest items
    return itr->second->HasQuestDrop(m_LootTemplates);
}

bool LootStore::HaveConditionalLootFor(uint32 loot_id) const
{
    LootTemplateMap::const_iterator itr = m_LootTemplates.find(loot_id);
    if (itr == m_LootTemplates.end())
    {
        return false;
    }

    return itr->second->HasConditionalDrop(m_LootTemplates);
}

/**
 * @brief Gets a loot template by identifier.
 *
 * @param loot_id The loot template identifier.
 * @return The matching loot template, or null if missing.
 */
LootTemplate const* LootStore::GetLootFor(uint32 loot_id) const
{
    LootTemplateMap::const_iterator tab = m_LootTemplates.find(loot_id);

    if (tab == m_LootTemplates.end())
    {
        return NULL;
    }

    return tab->second;
}

/**
 * @brief Loads the loot table and collects all template identifiers.
 *
 * @param ids_set The destination set for loaded loot ids.
 */
void LootStore::LoadAndCollectLootIds(LootIdSet& ids_set)
{
    LoadLootTable();

    for (LootTemplateMap::const_iterator tab = m_LootTemplates.begin(); tab != m_LootTemplates.end(); ++tab)
    {
        ids_set.insert(tab->first);
    }
}

/**
 * @brief Validates referenced loot template identifiers.
 *
 * @param ref_set The set of referenced loot ids to validate.
 */
void LootStore::CheckLootRefs(LootIdSet* ref_set) const
{
    for (LootTemplateMap::const_iterator ltItr = m_LootTemplates.begin(); ltItr != m_LootTemplates.end(); ++ltItr)
    {
        ltItr->second->CheckLootRefs(ref_set);
    }
}

/**
 * @brief Reports unreferenced loot ids remaining in a set.
 *
 * @param ids_set The set of loot ids considered unused.
 */
void LootStore::ReportUnusedIds(LootIdSet const& ids_set) const
{
    // all still listed ids isn't referenced
    if (!ids_set.empty())
    {
        for (LootIdSet::const_iterator itr = ids_set.begin(); itr != ids_set.end(); ++itr)
        {
            sLog.outErrorDb("Table '%s' entry %d isn't %s and not referenced from loot, and then useless.", GetName(), *itr, GetEntryName());
        }
        sLog.outString();
    }
}

/**
 * @brief Reports usage of a loot id that does not exist in the store.
 *
 * @param id The missing loot template identifier.
 */
void LootStore::ReportNotExistedId(uint32 id) const
{
    sLog.outErrorDb("Table '%s' entry %d (%s) not exist but used as loot id in DB.", GetName(), id, GetEntryName());
}

//
// --------- LootStoreItem ---------
//

// Checks if the entry (quest, non-quest, reference) takes it's chance (at loot generation)
// RATE_DROP_ITEMS is no longer used for all types of entries
bool LootStoreItem::Roll(bool rate) const
{
    if (chance >= 100.0f)
    {
        return true;
    }

    if (mincountOrRef < 0)                                  // reference case
    {
        return roll_chance_f(chance * (rate ? sGameConfig.getConfig(CONFIG_FLOAT_RATE_DROP_ITEM_REFERENCED) : 1.0f));
    }

    if (type == LOOTITEM_TYPE_CURRENCY)
    {
        return roll_chance_f(chance * (rate ? sGameConfig.getConfig(CONFIG_FLOAT_RATE_DROP_CURRENCY) : 1.0f));
    }
    else
    {
        if (needs_quest)
        {
            return roll_chance_f(chance * (rate ? sGameConfig.getConfig(CONFIG_FLOAT_RATE_DROP_ITEM_QUEST) : 1.0f));
        }

        ItemPrototype const* pProto = ObjectMgr::GetItemPrototype(itemid);

        float qualityModifier = pProto && rate ? sGameConfig.getConfig(qualityToRate[pProto->Quality]) : 1.0f;

        return roll_chance_f(chance * qualityModifier);
    }

    return false;
}

// Checks correctness of values
bool LootStoreItem::IsValid(LootStore const& store, uint32 entry) const
{
    if (group >= 1 << 7)                                    // it stored in 7 bit field
    {
        sLog.outErrorDb("Table '%s' entry %d item %d: group (%u) must be less %u - skipped", store.GetName(), entry, itemid, group, 1 << 7);
        return false;
    }

    if (group && type == LOOT_ITEM_TYPE_CURRENCY)
    {
        sLog.outErrorDb("Table '%s' entry %d currency %d: has group %u, but currencies must not have group - skipped", store.GetName(), entry, itemid, group);
        return false;
    }

    if (mincountOrRef == 0)
    {
        sLog.outErrorDb("Table '%s' entry %d item %d: wrong mincountOrRef (%d) - skipped", store.GetName(), entry, itemid, mincountOrRef);
        return false;
    }

    if (mincountOrRef > 0)                                  // item (quest or non-quest) entry, maybe grouped
    {
        if (type == LOOT_ITEM_TYPE_ITEM)
        {
            ItemPrototype const* proto = ObjectMgr::GetItemPrototype(itemid);
            if (!proto)
            {
                sLog.outErrorDb("Table '%s' entry %d item %d: item entry not listed in `item_template` - skipped", store.GetName(), entry, itemid);
                return false;
            }
        }
        else if (type == LOOT_ITEM_TYPE_CURRENCY)
        {
            CurrencyTypesEntry const* currency = sCurrencyTypesStore.LookupEntry(itemid);
            if (!currency)
            {
                sLog.outErrorDb("Table '%s' entry %d: currency entry %u not exists - skipped", store.GetName(), entry, itemid);
                return false;
            }
        }
        else
        {
            sLog.outErrorDb("Table '%s' entry %d: has unknown item %u with type %u - skipped", store.GetName(), entry, itemid, type);
            return false;
        }

        if (chance == 0 && group == 0)                      // Zero chance is allowed for grouped entries only
        {
            sLog.outErrorDb("Table '%s' entry %d item %d: equal-chanced grouped entry, but group not defined - skipped", store.GetName(), entry, itemid);
            return false;
        }

        if (chance != 0 && chance < 0.000001f)              // loot with low chance
        {
            sLog.outErrorDb("Table '%s' entry %d item %d: low chance (%f) - skipped", store.GetName(), entry, itemid, chance);
            return false;
        }

        if (maxcount < (uint32)mincountOrRef)                       // wrong max count
        {
            sLog.outErrorDb("Table '%s' entry %d item %d: max count (%u) less that min count (%i) - skipped", store.GetName(), entry, itemid, uint32(maxcount), mincountOrRef);
            return false;
        }
    }
    else                                                    // mincountOrRef < 0
    {
        if (needs_quest)
        {
            sLog.outErrorDb("Table '%s' entry %d item %d: negative chance is given for a reference, skipped", store.GetName(), entry, itemid);
            return false;
        }
        else if (chance == 0)                               // no chance for the reference
        {
            sLog.outErrorDb("Table '%s' entry %d item %d: zero chance is given for a reference, reference will never be used, skipped", store.GetName(), entry, itemid);
            return false;
        }
    }
    return true;                                            // Referenced template existence is checked at whole store level
}

/**
 * @brief Adds a loot entry to a loot group.
 *
 * @param item The loot store item to add.
 */
void LootTemplate::LootGroup::AddEntry(LootStoreItem& item)
{
    if (item.chance != 0)
    {
        ExplicitlyChanced.push_back(item);
    }
    else
    {
        EqualChanced.push_back(item);
    }
}

/**
 * @brief Rolls one item from a loot group.
 *
 * @return Pointer to the selected loot item, or NULL if none is selected.
 */
LootStoreItem const* LootTemplate::LootGroup::Roll() const
{
    if (!ExplicitlyChanced.empty())                         // First explicitly chanced entries are checked
    {
        float Roll = rand_chance_f();

        for (uint32 i = 0; i < ExplicitlyChanced.size(); ++i) // check each explicitly chanced entry in the template and modify its chance based on quality.
        {
            if (ExplicitlyChanced[i].chance >= 100.0f)
            {
                return &ExplicitlyChanced[i];
            }

            Roll -= ExplicitlyChanced[i].chance;
            if (Roll < 0)
            {
                return &ExplicitlyChanced[i];
            }
        }
    }
    if (!EqualChanced.empty())                              // If nothing selected yet - an item is taken from equal-chanced part
    {
        return &EqualChanced[irand(0, EqualChanced.size() - 1)];
    }

    return NULL;                                            // Empty drop from the group
}

/**
 * @brief Checks whether the group contains a quest drop.
 *
 * @return true if any entry is quest-related; otherwise, false.
 */
bool LootTemplate::LootGroup::HasQuestDrop() const
{
    for (LootStoreItemList::const_iterator i = ExplicitlyChanced.begin(); i != ExplicitlyChanced.end(); ++i)
        if (i->needs_quest)
        {
            return true;
        }
    for (LootStoreItemList::const_iterator i = EqualChanced.begin(); i != EqualChanced.end(); ++i)
        if (i->needs_quest)
        {
            return true;
        }
    return false;
}

// True if group includes at least 1 conditional entry
bool LootTemplate::LootGroup::HasConditionalDrop() const
{
    for (LootStoreItemList::const_iterator i = ExplicitlyChanced.begin(); i != ExplicitlyChanced.end(); ++i)
        if (i->conditionId)
        {
            return true;
        }
    for (LootStoreItemList::const_iterator i = EqualChanced.begin(); i != EqualChanced.end(); ++i)
        if (i->conditionId)
        {
            return true;
        }
    return false;
}

/**
 * @brief Computes the raw total chance of explicitly-chanced group entries.
 *
 * @return The summed explicit chance value.
 */
float LootTemplate::LootGroup::RawTotalChance() const
{
    float result = 0;

    for (LootStoreItemList::const_iterator i = ExplicitlyChanced.begin(); i != ExplicitlyChanced.end(); ++i)
        if (!i->needs_quest)
        {
            result += i->chance;
        }

    return result;
}

/**
 * @brief Computes the effective total chance of the group.
 *
 * @return The effective total chance value.
 */
float LootTemplate::LootGroup::TotalChance() const
{
    float result = RawTotalChance();

    if (!EqualChanced.empty() && result < 100.0f)
    {
        return 100.0f;
    }

    return result;
}

/**
 * @brief Verifies loot group chance consistency.
 *
 * @param lootstore The parent loot store.
 * @param id The loot template identifier.
 * @param group_id The group identifier.
 */
void LootTemplate::LootGroup::Verify(LootStore const& lootstore, uint32 id, uint32 group_id) const
{
    float chance = RawTotalChance();
    if (chance > 101.0f)                                    // TODO: replace with 100% when DBs will be ready
    {
        sLog.outErrorDb("Table '%s' entry %u group %d has total chance > 100%% (%f)", lootstore.GetName(), id, group_id, chance);
    }

    if (chance >= 100.0f && !EqualChanced.empty())
    {
        sLog.outErrorDb("Table '%s' entry %u group %d has items with chance=0%% but group total chance >= 100%% (%f)", lootstore.GetName(), id, group_id, chance);
    }
}

/**
 * @brief Validates referenced loot templates used by the group.
 *
 * @param ref_set Optional set of referenced loot IDs to update.
 */
void LootTemplate::LootGroup::CheckLootRefs(LootIdSet* ref_set) const
{
    for (LootStoreItemList::const_iterator ieItr = ExplicitlyChanced.begin(); ieItr != ExplicitlyChanced.end(); ++ieItr)
    {
        if (ieItr->mincountOrRef < 0)
        {
            if (!LootTemplates_Reference.GetLootFor(-ieItr->mincountOrRef))
            {
                LootTemplates_Reference.ReportNotExistedId(-ieItr->mincountOrRef);
            }
            else if (ref_set)
            {
                ref_set->erase(-ieItr->mincountOrRef);
            }
        }
    }

    for (LootStoreItemList::const_iterator ieItr = EqualChanced.begin(); ieItr != EqualChanced.end(); ++ieItr)
    {
        if (ieItr->mincountOrRef < 0)
        {
            if (!LootTemplates_Reference.GetLootFor(-ieItr->mincountOrRef))
            {
                LootTemplates_Reference.ReportNotExistedId(-ieItr->mincountOrRef);
            }
            else if (ref_set)
            {
                ref_set->erase(-ieItr->mincountOrRef);
            }
        }
    }
}

/**
 * @brief Adds an item entry to the loot template.
 *
 * @param item The loot store item to add.
 */
void LootTemplate::AddEntry(LootStoreItem& item)
{
    if (item.group > 0 && item.mincountOrRef > 0)           // Group
    {
        if (item.group >= Groups.size())
        {
            Groups.resize(item.group);                       // Adds new group the the loot template if needed
        }
        Groups[item.group - 1].AddEntry(item);              // Adds new entry to the group
    }
    else                                                    // Non-grouped entries and references are stored together
    {
        Entries.push_back(item);
    }
}

/**
 * @brief Checks whether the template contains any quest drop.
 *
 * @param store The loot template map used for references.
 * @param groupId Optional specific group identifier.
 * @return true if a quest drop exists; otherwise, false.
 */
bool LootTemplate::HasQuestDrop(LootTemplateMap const& store, uint8 groupId) const
{
    if (groupId)                                            // Group reference
    {
        if (groupId > Groups.size())
        {
            return false;                                    // Error message [should be] already printed at loading stage
        }
        return Groups[groupId - 1].HasQuestDrop();
    }

    for (LootStoreItemList::const_iterator i = Entries.begin(); i != Entries.end(); ++i)
    {
        if (i->mincountOrRef < 0)                           // References
        {
            LootTemplateMap::const_iterator Referenced = store.find(-i->mincountOrRef);
            if (Referenced == store.end())
            {
                continue;                                    // Error message [should be] already printed at loading stage
            }
            if (Referenced->second->HasQuestDrop(store, i->group))
            {
                return true;
            }
        }
        else if (i->needs_quest)
        {
            return true;                                     // quest drop found
        }
    }

    // Now processing groups
    for (LootGroups::const_iterator i = Groups.begin() ; i != Groups.end() ; ++i)
        if (i->HasQuestDrop())
        {
            return true;
        }

    return false;
}

// True if template includes at least 1 conditional entry
bool LootTemplate::HasConditionalDrop(LootTemplateMap const& store, uint8 groupId) const
{
    if (groupId)                                            // Group reference
    {
        if (groupId > Groups.size())
        {
            return false;                                    // Error message [should be] already printed at loading stage
        }
        return Groups[groupId - 1].HasConditionalDrop();
    }

    for (LootStoreItemList::const_iterator i = Entries.begin(); i != Entries.end(); ++i)
    {
        if (i->conditionId)
        {
            return true;
        }

        if (i->mincountOrRef < 0)                           // References
        {
            LootTemplateMap::const_iterator Referenced = store.find(-i->mincountOrRef);
            if (Referenced == store.end())
            {
                continue;                                    // Error message [should be] already printed at loading stage
            }
            if (Referenced->second->HasConditionalDrop(store, i->group))
            {
                return true;
            }
        }
    }

    for (LootGroups::const_iterator i = Groups.begin(); i != Groups.end(); ++i)
        if (i->HasConditionalDrop())
        {
            return true;
        }

    return false;
}

/**
 * @brief Verifies consistency of the loot template.
 *
 * @param lootstore The parent loot store.
 * @param id The loot template identifier.
 */
void LootTemplate::Verify(LootStore const& lootstore, uint32 id) const
{
    // Checking group chances
    for (uint32 i = 0; i < Groups.size(); ++i)
    {
        Groups[i].Verify(lootstore, id, i + 1);
    }

    // TODO: References validity checks
}

/**
 * @brief Validates referenced loot templates used by this template.
 *
 * @param ref_set Optional set of referenced loot IDs to update.
 */
void LootTemplate::CheckLootRefs(LootIdSet* ref_set) const
{
    for (LootStoreItemList::const_iterator ieItr = Entries.begin(); ieItr != Entries.end(); ++ieItr)
    {
        if (ieItr->mincountOrRef < 0)
        {
            if (!LootTemplates_Reference.GetLootFor(-ieItr->mincountOrRef))
            {
                LootTemplates_Reference.ReportNotExistedId(-ieItr->mincountOrRef);
            }
            else if (ref_set)
            {
                ref_set->erase(-ieItr->mincountOrRef);
            }
        }
    }

    for (LootGroups::const_iterator grItr = Groups.begin(); grItr != Groups.end(); ++grItr)
    {
        grItr->CheckLootRefs(ref_set);
    }
}

/**
 * @brief Loads creature loot templates and verifies referenced loot ids.
 */
void LoadLootTemplates_Creature()
{
    LootIdSet ids_set, ids_setUsed;
    LootTemplates_Creature.LoadAndCollectLootIds(ids_set);

    // remove real entries and check existence loot
    for (uint32 i = 1; i < sObjectMgr.GetCreatureTemplates().GetMaxEntry(); ++i)
    {
        if (CreatureInfo const* cInfo = ObjectMgr::GetCreatureTemplate(i))
        {
            if (uint32 lootid = cInfo->LootId)
            {
                if (ids_set.find(lootid) == ids_set.end())
                {
                    LootTemplates_Creature.ReportNotExistedId(lootid);
                }
                else
                {
                    ids_setUsed.insert(lootid);
                }
            }
        }
    }
    for (LootIdSet::const_iterator itr = ids_setUsed.begin(); itr != ids_setUsed.end(); ++itr)
    {
        ids_set.erase(*itr);
    }

    // for alterac valley we've defined Player-loot inside creature_loot_template id=0
    // this hack is used, so that we won't need to create an extra table player_loot_template for just one case
    ids_set.erase(0);

    // output error for any still listed (not referenced from appropriate table) ids
    LootTemplates_Creature.ReportUnusedIds(ids_set);
}

/**
 * @brief Loads disenchant loot templates and verifies referenced loot ids.
 */
void LoadLootTemplates_Disenchant()
{
    LootIdSet ids_set, ids_setUsed;
    LootTemplates_Disenchant.LoadAndCollectLootIds(ids_set);

    // remove real entries and check existence loot
    for (uint32 i = 1; i < sObjectMgr.GetItemPrototypes().GetMaxEntry(); ++i)
    {
        if (ItemPrototype const* proto = ObjectMgr::GetItemPrototype(i))
        {
            if (uint32 lootid = proto->DisenchantID)
            {
                if (ids_set.find(lootid) == ids_set.end())
                {
                    LootTemplates_Disenchant.ReportNotExistedId(lootid);
                }
                else
                {
                    ids_setUsed.insert(lootid);
                }
            }
        }
    }
    for (LootIdSet::const_iterator itr = ids_setUsed.begin(); itr != ids_setUsed.end(); ++itr)
    {
        ids_set.erase(*itr);
    }
    // output error for any still listed (not referenced from appropriate table) ids
    LootTemplates_Disenchant.ReportUnusedIds(ids_set);
}

/**
 * @brief Loads fishing loot templates and verifies referenced area ids.
 */
void LoadLootTemplates_Fishing()
{
    LootIdSet ids_set;
    LootTemplates_Fishing.LoadAndCollectLootIds(ids_set);

    // remove real entries and check existence loot
    for (uint32 i = 1; i < sAreaStore.GetNumRows(); ++i)
    {
        if (AreaTableEntry const* areaEntry = sAreaStore.LookupEntry(i))
            if (ids_set.find(areaEntry->ID) != ids_set.end())
            {
                ids_set.erase(areaEntry->ID);
            }
    }

    // by default (look config options) fishing at fail provide junk loot, entry 0 use for store this loot
    ids_set.erase(0);

    // output error for any still listed (not referenced from appropriate table) ids
    LootTemplates_Fishing.ReportUnusedIds(ids_set);
}

/**
 * @brief Loads gameobject loot templates and verifies referenced loot ids.
 */
void LoadLootTemplates_Gameobject()
{
    LootIdSet ids_set, ids_setUsed;
    LootTemplates_Gameobject.LoadAndCollectLootIds(ids_set);

    // remove real entries and check existence loot
    for (GameObjectTemplateStore::const_iterator itr = sObjectMgr.GetGameObjectTemplates().begin(); itr != sObjectMgr.GetGameObjectTemplates().end(); ++itr)
    {
        if (uint32 lootid = itr->GetLootId())
        {
            if (ids_set.find(lootid) == ids_set.end())
            {
                LootTemplates_Gameobject.ReportNotExistedId(lootid);
            }
            else
            {
                ids_setUsed.insert(lootid);
            }
        }
    }
    for (LootIdSet::const_iterator itr = ids_setUsed.begin(); itr != ids_setUsed.end(); ++itr)
    {
        ids_set.erase(*itr);
    }

    // output error for any still listed (not referenced from appropriate table) ids
    LootTemplates_Gameobject.ReportUnusedIds(ids_set);
}

/**
 * @brief Loads item loot templates and verifies referenced item ids.
 */
void LoadLootTemplates_Item()
{
    LootIdSet ids_set;
    LootTemplates_Item.LoadAndCollectLootIds(ids_set);

    // remove real entries and check existence loot
    for (uint32 i = 1; i < sObjectMgr.GetItemPrototypes().GetMaxEntry(); ++i)
    {
        if (ItemPrototype const* proto = ObjectMgr::GetItemPrototype(i))
        {
            if (!(proto->Flags & ITEM_FLAG_LOOTABLE))
            {
                continue;
            }

            if (ids_set.find(proto->ItemId) != ids_set.end() || proto->MaxMoneyLoot > 0)
            {
                ids_set.erase(proto->ItemId);
            }
            // wdb have wrong data cases, so skip by default
            else if (!sLog.HasLogFilter(LOG_FILTER_DB_STRICTED_CHECK))
            {
                LootTemplates_Item.ReportNotExistedId(proto->ItemId);
            }
        }
    }

    // output error for any still listed (not referenced from appropriate table) ids
    LootTemplates_Item.ReportUnusedIds(ids_set);
}

void LoadLootTemplates_Milling()
{
    LootIdSet ids_set;
    LootTemplates_Milling.LoadAndCollectLootIds(ids_set);

    // remove real entries and check existence loot
    for (uint32 i = 1; i < sObjectMgr.GetItemPrototypes().GetMaxEntry(); ++i)
    {
        ItemPrototype const* proto = ObjectMgr::GetItemPrototype(i);
        if (!proto)
        {
            continue;
        }

        if (!(proto->Flags & ITEM_FLAG_MILLABLE))
        {
            continue;
        }

        if (ids_set.find(proto->ItemId) != ids_set.end())
        {
            ids_set.erase(proto->ItemId);
        }
        else
        {
            LootTemplates_Milling.ReportNotExistedId(proto->ItemId);
        }
    }

    // output error for any still listed (not referenced from appropriate table) ids
    LootTemplates_Milling.ReportUnusedIds(ids_set);
}

/**
 * @brief Loads pickpocketing loot templates and verifies referenced loot ids.
 */
void LoadLootTemplates_Pickpocketing()
{
    LootIdSet ids_set, ids_setUsed;
    LootTemplates_Pickpocketing.LoadAndCollectLootIds(ids_set);

    // remove real entries and check existence loot
    for (uint32 i = 1; i < sObjectMgr.GetCreatureTemplates().GetMaxEntry(); ++i)
    {
        if (CreatureInfo const* cInfo = ObjectMgr::GetCreatureTemplate(i))
        {
            if (uint32 lootid = cInfo->PickpocketLootId)
            {
                if (ids_set.find(lootid) == ids_set.end())
                {
                    LootTemplates_Pickpocketing.ReportNotExistedId(lootid);
                }
                else
                {
                    ids_setUsed.insert(lootid);
                }
            }
        }
    }
    for (LootIdSet::const_iterator itr = ids_setUsed.begin(); itr != ids_setUsed.end(); ++itr)
    {
        ids_set.erase(*itr);
    }

    // output error for any still listed (not referenced from appropriate table) ids
    LootTemplates_Pickpocketing.ReportUnusedIds(ids_set);
}

void LoadLootTemplates_Prospecting()
{
    LootIdSet ids_set;
    LootTemplates_Prospecting.LoadAndCollectLootIds(ids_set);

    // remove real entries and check existence loot
    for (uint32 i = 1; i < sObjectMgr.GetItemPrototypes().GetMaxEntry(); ++i)
    {
        ItemPrototype const* proto = ObjectMgr::GetItemPrototype(i);
        if (!proto)
        {
            continue;
        }

        if (!(proto->Flags & ITEM_FLAG_PROSPECTABLE))
        {
            continue;
        }

        if (ids_set.find(proto->ItemId) != ids_set.end())
        {
            ids_set.erase(proto->ItemId);
        }
        // else -- exist some cases that possible can be prospected but not expected have any result loot
        //    LootTemplates_Prospecting.ReportNotExistedId(proto->ItemId);
    }

    // output error for any still listed (not referenced from appropriate table) ids
    LootTemplates_Prospecting.ReportUnusedIds(ids_set);
}

/**
 * @brief Loads mail loot templates and verifies referenced mail template ids.
 */
void LoadLootTemplates_Mail()
{
    LootIdSet ids_set;
    LootTemplates_Mail.LoadAndCollectLootIds(ids_set);

    // remove real entries and check existence loot
    for (uint32 i = 1; i < sMailTemplateStore.GetNumRows(); ++i)
        if (sMailTemplateStore.LookupEntry(i))
            if (ids_set.find(i) != ids_set.end())
            {
                ids_set.erase(i);
            }

    // output error for any still listed (not referenced from appropriate table) ids
    LootTemplates_Mail.ReportUnusedIds(ids_set);
}

/**
 * @brief Loads skinning loot templates and verifies referenced loot ids.
 */
void LoadLootTemplates_Skinning()
{
    LootIdSet ids_set, ids_setUsed;
    LootTemplates_Skinning.LoadAndCollectLootIds(ids_set);

    // remove real entries and check existence loot
    for (uint32 i = 1; i < sObjectMgr.GetCreatureTemplates().GetMaxEntry(); ++i)
    {
        if (CreatureInfo const* cInfo = ObjectMgr::GetCreatureTemplate(i))
        {
            if (uint32 lootid = cInfo->SkinningLootId)
            {
                if (ids_set.find(lootid) == ids_set.end())
                {
                    LootTemplates_Skinning.ReportNotExistedId(lootid);
                }
                else
                {
                    ids_setUsed.insert(lootid);
                }
            }
        }
    }
    for (LootIdSet::const_iterator itr = ids_setUsed.begin(); itr != ids_setUsed.end(); ++itr)
    {
        ids_set.erase(*itr);
    }

    // output error for any still listed (not referenced from appropriate table) ids
    LootTemplates_Skinning.ReportUnusedIds(ids_set);
}

void LoadLootTemplates_Spell()
{
    LootIdSet ids_set;
    LootTemplates_Spell.LoadAndCollectLootIds(ids_set);

    // remove real entries and check existence loot
    for (uint32 spell_id = 1; spell_id < sSpellStore.GetNumRows(); ++spell_id)
    {
        SpellEntry const* spellInfo = sSpellStore.LookupEntry(spell_id);
        if (!spellInfo)
        {
            continue;
        }

        // possible cases
        if (!IsLootCraftingSpell(spellInfo))
        {
            continue;
        }

        if (ids_set.find(spell_id) == ids_set.end())
        {
            // not report about not trainable spells (optionally supported by DB)
            // ignore 61756 (Northrend Inscription Research (FAST QA VERSION) for example
            if (!spellInfo->HasAttribute(SPELL_ATTR_NOT_SHAPESHIFT) || spellInfo->HasAttribute(SPELL_ATTR_TRADESPELL))
            {
                LootTemplates_Spell.ReportNotExistedId(spell_id);
            }
        }
        else
        {
            ids_set.erase(spell_id);
        }
    }

    // output error for any still listed (not referenced from appropriate table) ids
    LootTemplates_Spell.ReportUnusedIds(ids_set);
}

/**
 * @brief Loads reference loot templates and validates cross-template references.
 */
void LoadLootTemplates_Reference()
{
    LootIdSet ids_set;
    LootTemplates_Reference.LoadAndCollectLootIds(ids_set);

    // check references and remove used
    LootTemplates_Creature.CheckLootRefs(&ids_set);
    LootTemplates_Fishing.CheckLootRefs(&ids_set);
    LootTemplates_Gameobject.CheckLootRefs(&ids_set);
    LootTemplates_Item.CheckLootRefs(&ids_set);
    LootTemplates_Milling.CheckLootRefs(&ids_set);
    LootTemplates_Pickpocketing.CheckLootRefs(&ids_set);
    LootTemplates_Skinning.CheckLootRefs(&ids_set);
    LootTemplates_Disenchant.CheckLootRefs(&ids_set);
    LootTemplates_Prospecting.CheckLootRefs(&ids_set);
    LootTemplates_Mail.CheckLootRefs(&ids_set);
    LootTemplates_Reference.CheckLootRefs(&ids_set);
    LootTemplates_Spell.CheckLootRefs(&ids_set);

    // output error for any still listed ids (not referenced from any loot table)
    LootTemplates_Reference.ReportUnusedIds(ids_set);
}
