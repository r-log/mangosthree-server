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

#include "Common/Locales.h"
#include "Utilities/Errors.h"
#include <cstring>
#include <sstream>
#include <string>
#include <vector>
#include "Utilities/PackedValues.h"
#include "ObjectMgr.h"
#include "data/MapGeometry.h"
#include "DBCStores.h"
#include "Database/DatabaseEnv.h"
#include "Database/SqlOperations.h"
#include "Policies/Singleton.h"

#include "Log.h"
#include "ObjectGuid.h"
#include "spells/data/SpellMgr.h"
#include "data/GameConfig.h"
#include "CharacterCache.h"
#include "ProgressBar.h"
#include "Language.h"
#include "PoolManager.h"
#include "GameEventMgr.h"
#include "Util.h"
#include "WaypointManager.h"
#include "DB2Structure.h"
#include "DB2Stores.h"
#include "DisableMgr.h"

#include <cstdarg>
#include <limits>
#include <set>
#include "persistence/characters/CharacterRepository.h"
#include "persistence/characters/CharacterSaveRepository.h"
#include "persistence/characters/economy/AuctionRepository.h"
#include "persistence/characters/inventory/ItemRepository.h"
#include "persistence/characters/mail/MailRepository.h"
#include "persistence/characters/maps/CorpseRepository.h"
#include "persistence/characters/maps/InstanceRepository.h"
#include "persistence/characters/pets/PetRepository.h"
#include "persistence/characters/social/ArenaTeamRepository.h"
#include "persistence/characters/social/GroupRepository.h"
#include "persistence/characters/social/GuildRepository.h"
#include "persistence/world/spawns/SpawnRepository.h"
#include "persistence/world/creatures/CreatureDataRepository.h"
#include "persistence/world/items/ItemDataRepository.h"
#include "persistence/world/maps/MapDataRepository.h"
#include "persistence/world/maps/InstanceTemplateRepository.h"
#include "persistence/world/ConditionRepository.h"
#include "persistence/world/npcs/NpcDataRepository.h"
#include "persistence/world/players/PlayerDataRepository.h"
#include "persistence/world/quests/QuestDataRepository.h"
#include "persistence/world/spells/SpellTemplateRepository.h"
#include "persistence/world/locales/LocaleDataRepository.h"

static_assert(int(STRING_ROW_LOCALES) == int(MAX_LOCALE) - 1, "a string row holds every locale but the default");


// Temporary startup accumulator for LivingWorld observability (written during LoadActiveEntities(NULL) only)

/**
 * @brief Normalizes a player name to the server's canonical casing.
 *
 * @param name The player name to normalize.
 * @return true if the name was normalized successfully; otherwise false.
 */
bool normalizePlayerName(std::string& name)
{
    if (name.empty())
    {
        return false;
    }

    wchar_t wstr_buf[MAX_INTERNAL_PLAYER_NAME + 1];
    size_t wstr_len = MAX_INTERNAL_PLAYER_NAME;

    if (!Utf8toWStr(name, &wstr_buf[0], wstr_len))
    {
        return false;
    }

    wstr_buf[0] = wcharToUpper(wstr_buf[0]);
    for (size_t i = 1; i < wstr_len; ++i)
    {
        wstr_buf[i] = wcharToLower(wstr_buf[i]);
    }

    if (!WStrToUtf8(wstr_buf, wstr_len, name))
    {
        return false;
    }

    return true;
}

LanguageDesc lang_description[LANGUAGES_COUNT] =
{
    { LANG_ADDON,           0, 0                       },
    { LANG_UNIVERSAL,       0, 0                       },
    { LANG_ORCISH,        669, SKILL_LANG_ORCISH       },
    { LANG_DARNASSIAN,    671, SKILL_LANG_DARNASSIAN   },
    { LANG_TAURAHE,       670, SKILL_LANG_TAURAHE      },
    { LANG_DWARVISH,      672, SKILL_LANG_DWARVEN      },
    { LANG_COMMON,        668, SKILL_LANG_COMMON       },
    { LANG_DEMONIC,       815, SKILL_LANG_DEMON_TONGUE },
    { LANG_TITAN,         816, SKILL_LANG_TITAN        },
    { LANG_THALASSIAN,    813, SKILL_LANG_THALASSIAN   },
    { LANG_DRACONIC,      814, SKILL_LANG_DRACONIC     },
    { LANG_KALIMAG,       817, SKILL_LANG_OLD_TONGUE   },
    { LANG_GNOMISH,      7340, SKILL_LANG_GNOMISH      },
    { LANG_TROLL,        7341, SKILL_LANG_TROLL        },
    { LANG_GUTTERSPEAK, 17737, SKILL_LANG_GUTTERSPEAK  },
    { LANG_DRAENEI,     29932, SKILL_LANG_DRAENEI      },
    { LANG_ZOMBIE,          0, 0                       },
    { LANG_GNOMISH_BINARY,  0, 0                       },
    { LANG_GOBLIN_BINARY,   0, 0                       },
    { LANG_WORGEN,      69270, SKILL_LANG_WORGEN       },
    { LANG_GOBLIN,      69269, SKILL_LANG_GOBLIN       }
};

/**
 * @brief Looks up language metadata by language id.
 *
 * @param lang The language id.
 * @return LanguageDesc const* The matching language descriptor, or null if not found.
 */
LanguageDesc const* GetLanguageDescByID(uint32 lang)
{
    for (int i = 0; i < LANGUAGES_COUNT; ++i)
    {
        if (uint32(lang_description[i].lang_id) == lang)
        {
            return &lang_description[i];
        }
    }

    return NULL;
}

template<typename T>
/**
 * @brief Generates the next identifier from the typed guid generator.
 *
 * @return T The next generated identifier value.
 */
T IdGenerator<T>::Generate()
{
    if (m_nextGuid >= std::numeric_limits<T>::max() - 1)
    {
        sLog.outError("%s guid overflow!! Can't continue, shutting down server. ", m_name);
        GuidOverflow();
    }
    return m_nextGuid++;
}

template uint32 IdGenerator<uint32>::Generate();
template uint64 IdGenerator<uint64>::Generate();

/**
 * @brief Initializes the global object manager.
 */
ObjectMgr::ObjectMgr() :
    m_ArenaTeamIds("Arena team ids"),
    m_AuctionIds("Auction ids"),
    m_EquipmentSetIds("Equipment set ids"),
    m_GuildIds("Guild ids"),
    m_MailIds("Mail ids"),
    m_PetNumbers("Pet numbers"),
    m_FirstTemporaryCreatureGuid(1),
    m_FirstTemporaryGameObjectGuid(1),
    DBCLocaleIndex(LOCALE_enUS)
{
}

/**
 * @brief Releases dynamically allocated object manager resources.
 */
ObjectMgr::~ObjectMgr()
{
    for (QuestMap::iterator i = mQuestTemplates.begin(); i != mQuestTemplates.end(); ++i)
    {
        delete i->second;
    }

    for (PetLevelInfoMap::iterator i = petInfo.begin(); i != petInfo.end(); ++i)
    {
        delete[] i->second;
    }

    for (int race = 0; race < MAX_RACES; ++race)
    {
        for (int class_ = 0; class_ < MAX_CLASSES; ++class_)
        {
            delete[] playerInfo[race][class_].levelInfo;
        }
    }

    for (CacheVendorItemMap::iterator itr = m_mCacheVendorTemplateItemMap.begin(); itr != m_mCacheVendorTemplateItemMap.end(); ++itr)
    {
        itr->second.Clear();
    }

    for (CacheVendorItemMap::iterator itr = m_mCacheVendorItemMap.begin(); itr != m_mCacheVendorItemMap.end(); ++itr)
    {
        itr->second.Clear();
    }

    for (CacheTrainerSpellMap::iterator itr = m_mCacheTrainerSpellMap.begin(); itr != m_mCacheTrainerSpellMap.end(); ++itr)
    {
        itr->second.Clear();
    }
}


/**
 * The five lookups below answer from CharacterCache, which holds the `characters` rows in memory
 * and is kept current while the characters are online.
 */

// name must be checked to correctness (if received) before call this function
ObjectGuid ObjectMgr::GetPlayerGuidByName(std::string name) const
{
    // The `characters`.`name` column is case-insensitive, and so is the cache's index.
    if (CharacterCacheRef entry = sCharacterCache.GetByName(name))
    {
        return entry->guid;
    }

    return ObjectGuid();
}

/**
 * @brief Resolves a player name from a player GUID.
 *
 * @param guid The player GUID.
 * @param name Receives the resolved player name.
 * @return true if the player name was found; otherwise, false.
 */
bool ObjectMgr::GetPlayerNameByGUID(ObjectGuid guid, std::string& name) const
{
    if (CharacterCacheRef entry = sCharacterCache.GetByGuid(guid))
    {
        name = entry->name;
        return true;
    }

    return false;
}

/**
 * @brief Resolves a player's team from a player GUID.
 *
 * @param guid The player GUID.
 * @return The player's team, or TEAM_NONE if unavailable.
 */
Team ObjectMgr::GetPlayerTeamByGUID(ObjectGuid guid) const
{
    return sCharacterCache.GetTeam(guid);
}

/**
 * @brief Resolves an account id from a player GUID.
 *
 * @param guid The player GUID.
 * @return The account id, or 0 if unavailable.
 */
uint32 ObjectMgr::GetPlayerAccountIdByGUID(ObjectGuid guid) const
{
    if (!guid.IsPlayer())
    {
        return 0;
    }

    return sCharacterCache.GetAccountId(guid);
}

/**
 * @brief Resolves an account id from a player name.
 *
 * @param name The player name.
 * @return The account id, or 0 if unavailable.
 */
uint32 ObjectMgr::GetPlayerAccountIdByPlayerName(const std::string& name) const
{
    if (CharacterCacheRef entry = sCharacterCache.GetByName(name))
    {
        return entry->accountId;
    }

    return 0;
}

/* ********************************************************************************************* */
/* *                                Static Wrappers                                              */
/* ********************************************************************************************* */

/**
 * @brief Gets static gameobject template data by entry id.
 *
 * @param id The gameobject entry id.
 * @return The gameobject template, or null if missing.
 */
GameObjectInfo const* ObjectMgr::GetGameObjectInfo(uint32 id) { return sObjectMgr.m_gameObjectTemplates.Find(id); }

/**
 * @brief Gets a game object spawn's addon.
 *
 * @param lowguid The game object spawn guid.
 * @return The addon, or null if missing.
 */
GameObjectDataAddon const* ObjectMgr::GetGameObjectAddon(uint32 lowguid) { return sObjectMgr.m_gameObjectAddons.Find(lowguid); }

/**
 * @brief Gets static creature template data by entry id.
 *
 * @param id The creature entry id.
 * @return The creature template, or null if missing.
 */
CreatureInfo const* ObjectMgr::GetCreatureTemplate(uint32 id) { return sObjectMgr.m_creatureTemplates.Find(id); }

/**
 * @brief Gets creature model metadata by display id.
 *
 * @param modelid The creature model id.
 * @return The creature model info, or null if missing.
 */
CreatureModelInfo const* ObjectMgr::GetCreatureModelInfo(uint32 modelid) { return sObjectMgr.m_creatureModels.Find(modelid); }

/**
 * @brief Gets a creature entry's template spells.
 *
 * @param entry The creature entry.
 * @return The template spells, or null if missing.
 */
CreatureTemplateSpells const* ObjectMgr::GetCreatureTemplateSpells(uint32 entry) { return sObjectMgr.m_creatureTemplateSpells.Find(entry); }

/**
 * @brief Gets equipment template data by entry id.
 *
 * @param entry The equipment template entry id.
 * @return The equipment template, or null if missing.
 */
EquipmentInfo const* ObjectMgr::GetEquipmentInfo(uint32 entry) { return sObjectMgr.m_creatureEquipment.Find(entry); }

/**
 * @brief Gets creature spawn addon data by low GUID.
 *
 * @param lowguid The creature spawn low GUID.
 * @return The addon data, or null if missing.
 */
CreatureDataAddon const* ObjectMgr::GetCreatureAddon(uint32 lowguid) { return sObjectMgr.m_creatureAddons.Find(lowguid); }

/**
 * @brief Gets creature template addon data by entry id.
 *
 * @param entry The creature entry id.
 * @return The template addon data, or null if missing.
 */
CreatureDataAddon const* ObjectMgr::GetCreatureTemplateAddon(uint32 entry) { return sObjectMgr.m_creatureTemplateAddons.Find(entry); }

/**
 * @brief Gets item prototype data by entry id.
 *
 * @param id The item entry id.
 * @return The item prototype, or null if missing.
 */
ItemPrototype const* ObjectMgr::GetItemPrototype(uint32 id) { return sObjectMgr.m_itemPrototypes.Find(id); }


/**
 * @brief Loads instance templates and validates map and ghost entrance data.
 */
void ObjectMgr::LoadInstanceTemplate()
{
    TableRead<InstanceTemplateRow> read = InstanceTemplateRepository::QueryInstanceTemplates();
    if (read.Unreadable())
    {
        sLog.outError("Error loading `instance_template` table: %s\n", TableReadReason(read.status));
        Log::WaitBeforeContinueIfNeed();
        exit(1);
    }

    uint32 loaded = 0;
    if (read.status == TableReadStatus::Empty)
    {
        sLog.outError("instance_template table is empty!\n");
    }
    else
    {
        m_instanceTemplates.Clear();
        BarGoLink bar(read.rows.size());
        for (InstanceTemplateRow const& row : read.rows)
        {
            bar.step();
            InstanceTemplate instance;
            instance.map = row.map;
            instance.parent = row.parent;
            instance.levelMin = row.levelMin;
            instance.levelMax = row.levelMax;
            m_instanceTemplates.Set(instance);
        }
        loaded = uint32(read.rows.size());
    }

    for (uint32 i = 0; i < m_instanceTemplates.GetMaxEntry(); ++i)
    {
        InstanceTemplate const* temp = m_instanceTemplates.Find(i);
        if (!temp)
        {
            continue;
        }

        MapEntry const* mapEntry = sMapStore.LookupEntry(temp->map);
        if (!mapEntry)
        {
            sLog.outErrorDb("ObjectMgr::LoadInstanceTemplate: bad mapid %d for template!", temp->map);
            m_instanceTemplates.Erase(i);
            continue;
        }

        if (!mapEntry->Instanceable())
        {
            sLog.outErrorDb("ObjectMgr::LoadInstanceTemplate: non-instanceable mapid %d for template!", temp->map);
            m_instanceTemplates.Erase(i);
            continue;
        }

        if (temp->parent > 0)
        {
            // check existence
            MapEntry const* parentEntry = sMapStore.LookupEntry(temp->parent);
            if (!parentEntry)
            {
                sLog.outErrorDb("ObjectMgr::LoadInstanceTemplate: bad parent map id %u for instance template %d template!",
                                temp->parent, temp->map);
                m_instanceTemplates.FindForEdit(i)->parent = 0;
                continue;
            }

            if (parentEntry->IsContinent())
            {
                sLog.outErrorDb("ObjectMgr::LoadInstanceTemplate: parent point to continent map id %u for instance template %d template, ignored, need be set only for non-continent parents!",
                                parentEntry->ID, temp->map);
                m_instanceTemplates.FindForEdit(i)->parent = 0;
                continue;
            }
        }
    }

    sLog.outString(">> Loaded %u Instance Template definitions", loaded);
    sLog.outString();
}

/**
 * @brief Fills the conditions from a read that answered rows or an empty table, then removes invalid entries.
 */
static void FillConditions(KeyedRecordStore<PlayerCondition>& conditions, TableRead<ConditionRow> const& read)
{
    // the rows of the last read that answered any, which the log line counts
    static uint32 loaded = 0;
    if (read.status == TableReadStatus::Empty)
    {
        sLog.outError("conditions table is empty!\n");
    }
    else
    {
        conditions.Clear();
        BarGoLink bar(read.rows.size());
        for (ConditionRow const& row : read.rows)
        {
            bar.step();
            conditions.Set(row.entry, PlayerCondition(uint16(row.entry), int16(row.type), row.value1, row.value2));
        }
        loaded = uint32(read.rows.size());
    }

    for (uint32 i = 0; i < conditions.GetMaxEntry(); ++i)
    {
        const PlayerCondition* condition = conditions.Find(i);
        if (!condition)
        {
            continue;
        }

        if (!condition->IsValid())
        {
            sLog.outErrorDb("ObjectMgr::LoadConditions: invalid condition_entry %u, skip", i);
            conditions.Erase(i);
            continue;
        }
    }

    sLog.outString(">> Loaded %u Condition definitions", loaded);
    sLog.outString();
}

/**
 * @brief Loads player condition definitions and removes invalid entries; stops the server when the table cannot be read.
 */
void ObjectMgr::LoadConditions()
{
    TableRead<ConditionRow> read = ConditionRepository::QueryConditions();
    if (read.Unreadable())
    {
        sLog.outError("Error loading `conditions` table: %s\n", TableReadReason(read.status));
        Log::WaitBeforeContinueIfNeed();
        exit(1);
    }

    FillConditions(m_conditions, read);
}

/**
 * @brief Reloads player condition definitions and removes invalid entries; keeps the conditions already loaded when the table cannot be read.
 */
TableReadStatus ObjectMgr::ReloadConditions()
{
    TableRead<ConditionRow> read = ConditionRepository::QueryConditions();
    if (read.Unreadable())
    {
        sLog.outError("Error loading `conditions` table: %s; the %u conditions already loaded are kept\n", TableReadReason(read.status), m_conditions.GetCount());
        return read.status;
    }

    FillConditions(m_conditions, read);
    return read.status;
}

/**
 * @brief Gets a loaded gossip text record by id.
 *
 * @param Text_ID The gossip text identifier.
 * @return The gossip text record, or null if missing.
 */
GossipText const* ObjectMgr::GetGossipText(uint32 Text_ID) const
{
    GossipTextMap::const_iterator itr = mGossipText.find(Text_ID);
    if (itr != mGossipText.end())
    {
        return &itr->second;
    }
    return NULL;
}

/**
 * @brief Loads area trigger to quest objective relationships.
 */
void ObjectMgr::LoadQuestAreaTriggers()
{
    mQuestAreaTriggerMap.clear();                           // need for reload case

    std::vector<QuestRelationRow> rows = QuestDataRepository::QueryActorQuestRelations(QA_AREATRIGGER);

    uint32 count = 0;

    if (rows.empty())
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString(">> Loaded %u quest trigger points", count);
        sLog.outString();
        return;
    }

    BarGoLink bar(rows.size());

    for (QuestRelationRow const& row : rows)
    {
        ++count;
        bar.step();

        uint32 trigger_ID = row.entry;
        uint32 quest_ID   = row.quest;

        AreaTriggerEntry const* atEntry = sAreaTriggerStore.LookupEntry(trigger_ID);
        if (!atEntry)
        {
            sLog.outErrorDb("Table `quest_relations` has area trigger (ID: %u) not listed in `AreaTrigger.dbc`.", trigger_ID);
            continue;
        }

        Quest const* quest = GetQuestTemplate(quest_ID);
        if (!quest)
        {
            sLog.outErrorDb("Table `quest_relations` has record (id: %u) for not existing quest %u", trigger_ID, quest_ID);
            continue;
        }

        if (!quest->HasSpecialFlag(QUEST_SPECIAL_FLAG_EXPLORATION_OR_EVENT))
        {
            sLog.outErrorDb("Table `quest_relations` has record (id: %u) for not quest %u, but quest not have flag QUEST_SPECIAL_FLAG_EXPLORATION_OR_EVENT. Trigger or quest flags must be fixed, quest modified to require objective.", trigger_ID, quest_ID);

            // this will prevent quest completing without objective
            const_cast<Quest*>(quest)->SetSpecialFlag(QUEST_SPECIAL_FLAG_EXPLORATION_OR_EVENT);

            // continue; - quest modified to required objective and trigger can be allowed.
        }

        mQuestAreaTriggerMap[trigger_ID] = quest_ID;
    }

    sLog.outString(">> Loaded %u quest trigger points", count);
    sLog.outString();
}

/**
 * @brief Loads area triggers that mark tavern rest zones.
 */
void ObjectMgr::LoadTavernAreaTriggers()
{
    mTavernAreaTriggerSet.clear();                          // need for reload case

    std::vector<TavernAreaTriggerRow> rows = MapDataRepository::QueryTavernAreaTriggers();

    uint32 count = 0;

    if (rows.empty())
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString(">> Loaded %u tavern triggers", count);
        sLog.outString();
        return;
    }

    BarGoLink bar(rows.size());

    for (TavernAreaTriggerRow const& row : rows)
    {
        ++count;
        bar.step();

        uint32 Trigger_ID      = row.id;

        AreaTriggerEntry const* atEntry = sAreaTriggerStore.LookupEntry(Trigger_ID);
        if (!atEntry)
        {
            sLog.outErrorDb("Table `areatrigger_tavern` has area trigger (ID:%u) not listed in `AreaTrigger.dbc`.", Trigger_ID);
            continue;
        }

        mTavernAreaTriggerSet.insert(Trigger_ID);
    }

    sLog.outString(">> Loaded %u tavern triggers", count);
    sLog.outString();
}

/**
 * @brief Initializes high-water marks for generated GUID and id sequences.
 */
void ObjectMgr::SetHighestGuids()
{
    if (std::optional<uint32> highest = CharacterRepository::QueryHighestGuid())
    {
        m_CharGuids.Set(*highest + 1);
    }

    if (std::optional<uint32> highest = SpawnRepository::QueryHighestCreatureGuid())
    {
        m_FirstTemporaryCreatureGuid = *highest + 1;
    }

    if (std::optional<uint32> highest = ItemRepository::QueryHighestGuid())
    {
        m_ItemGuids.Set(*highest + 1);
    }

    if (std::optional<uint32> highest = InstanceRepository::QueryHighestId())
    {
        m_InstanceGuids.Set(*highest + 1);
    }

    // Cleanup other tables from nonexistent guids (>=m_hiItemGuid)
    ItemRepository::DeleteRowsOfMissingItems(m_ItemGuids.GetNextAfterMaxUsed());

    if (std::optional<uint32> highest = SpawnRepository::QueryHighestGameObjectGuid())
    {
        m_FirstTemporaryGameObjectGuid = *highest + 1;
    }

    if (std::optional<uint32> highest = AuctionRepository::QueryHighestId())
    {
        m_AuctionIds.Set(*highest + 1);
    }

    if (std::optional<uint32> highest = MailRepository::QueryHighestId())
    {
        m_MailIds.Set(*highest + 1);
    }

    if (std::optional<uint32> highest = CorpseRepository::QueryHighestGuid())
    {
        m_CorpseGuids.Set(*highest + 1);
    }

    if (std::optional<uint32> highest = ArenaTeamRepository::QueryHighestId())
    {
        m_ArenaTeamIds.Set(*highest + 1);
    }

    if (std::optional<uint64> highest = CharacterSaveRepository::QueryHighestEquipmentSetId())
    {
        m_EquipmentSetIds.Set(*highest + 1);
    }

    if (std::optional<uint32> highest = GuildRepository::QueryHighestId())
    {
        m_GuildIds.Set(*highest + 1);
    }

    if (std::optional<uint32> highest = GroupRepository::QueryHighestId())
    {
        m_GroupGuids.Set(*highest + 1);
    }

    // setup reserved ranges for static guids spawn
    m_StaticCreatureGuids.Set(m_FirstTemporaryCreatureGuid);
    m_FirstTemporaryCreatureGuid += sGameConfig.getConfig(CONFIG_UINT32_GUID_RESERVE_SIZE_CREATURE);

    m_StaticGameObjectGuids.Set(m_FirstTemporaryGameObjectGuid);
    m_FirstTemporaryGameObjectGuid += sGameConfig.getConfig(CONFIG_UINT32_GUID_RESERVE_SIZE_GAMEOBJECT);
}


/**
 * @brief Loads exploration base experience values by level.
 */
void ObjectMgr::LoadExplorationBaseXP()
{
    uint32 count = 0;
    std::vector<ExplorationBaseXpRow> rows = PlayerDataRepository::QueryExplorationXp();

    if (rows.empty())
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString(">> Loaded %u BaseXP definitions", count);
        sLog.outString();
        return;
    }

    BarGoLink bar(rows.size());

    for (ExplorationBaseXpRow const& row : rows)
    {
        bar.step();

        uint32 level  = row.level;
        uint32 basexp = row.basexp;
        m_explorationBaseXp.Set(level, basexp);
        ++count;
    }

    sLog.outString(">> Loaded %u BaseXP definitions", count);
    sLog.outString();
}

/**
 * @brief Gets the XP required for the next player level.
 *
 * @param level The current player level index.
 * @return The XP requirement, or 0 if out of range.
 */
uint32 ObjectMgr::GetXPForLevel(uint32 level) const
{
    if (level < mPlayerXPperLevel.size())
    {
        return mPlayerXPperLevel[level];
    }
    return 0;
}

/**
 * @brief Loads pet name fragments used for random pet naming.
 */
void ObjectMgr::LoadPetNames()
{
    uint32 count = 0;
    std::vector<PetNamePartRow> rows = CreatureDataRepository::QueryPetNameParts();

    if (rows.empty())
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString(">> Loaded %u pet name parts", count);
        sLog.outString();
        return;
    }

    BarGoLink bar(rows.size());

    for (PetNamePartRow const& row : rows)
    {
        bar.step();

        std::string word = row.word.c_str();
        uint32 entry     = row.entry;
        bool   half      = row.half;
        if (half)
        {
            PetHalfName1[entry].push_back(word);
        }
        else
        {
            PetHalfName0[entry].push_back(word);
        }
        ++count;
    }

    sLog.outString(">> Loaded %u pet name parts", count);
    sLog.outString();
}

/**
 * @brief Initializes the next generated pet number from existing pets.
 */
void ObjectMgr::LoadPetNumber()
{
    if (std::optional<uint32> highest = PetRepository::QueryHighestId())
    {
        m_PetNumbers.Set(*highest + 1);
    }

    BarGoLink bar(1);
    bar.step();

    sLog.outString(">> Loaded the max pet number: %d", m_PetNumbers.GetNextAfterMaxUsed() - 1);
    sLog.outString();
}

/**
 * @brief Generates a random or fallback pet name for a creature entry.
 *
 * @param entry The creature entry id.
 * @return The generated pet name.
 */
std::string ObjectMgr::GeneratePetName(uint32 entry)
{
    std::vector<std::string>& list0 = PetHalfName0[entry];
    std::vector<std::string>& list1 = PetHalfName1[entry];

    if (list0.empty() || list1.empty())
    {
        CreatureInfo const* cinfo = GetCreatureTemplate(entry);
        char const* petname = GetPetName(cinfo->Family, sGameConfig.GetDefaultDbcLocale());
        if (!petname)
        {
            petname = cinfo->Name;
        }
        return std::string(petname);
    }

    return *(list0.begin() + urand(0, list0.size() - 1)) + *(list1.begin() + urand(0, list1.size() - 1));
}

/**
 * @brief Loads point-of-interest definitions used by NPC map markers.
 */
void ObjectMgr::LoadPointsOfInterest()
{
    m_pointsOfInterest.Clear();                             // need for reload case

    uint32 count = 0;

    std::vector<PointOfInterestRow> rows = NpcDataRepository::QueryPointsOfInterest();

    if (rows.empty())
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outErrorDb(">> Loaded 0 Points of Interest definitions. DB table `points_of_interest` is empty.");
        sLog.outString();
        return;
    }

    BarGoLink bar(rows.size());

    for (PointOfInterestRow const& row : rows)
    {
        bar.step();

        uint32 point_id = row.entry;

        PointOfInterest POI;
        POI.x                    = row.x;
        POI.y                    = row.y;
        POI.icon                 = row.icon;
        POI.flags                = row.flags;
        POI.data                 = row.data;
        POI.icon_name            = row.iconName;

        if (!MaNGOS::IsValidMapCoord(POI.x, POI.y))
        {
            sLog.outErrorDb("Table `points_of_interest` (Entry: %u) have invalid coordinates (X: %f Y: %f), ignored.", point_id, POI.x, POI.y);
            continue;
        }

        m_pointsOfInterest.Set(point_id, POI);

        ++count;
    }

    sLog.outString(">> Loaded %u Points of Interest definitions", count);
}

void ObjectMgr::LoadQuestPOI()
{
    m_questPoi.Clear();                                // need for reload case

    uint32 count = 0;

    std::vector<QuestPoiRow> rows = QuestDataRepository::QueryQuestPois();

    if (rows.empty())
    {
        BarGoLink bar(1);

        bar.step();

        sLog.outString();
        sLog.outErrorDb(">> Loaded 0 quest POI definitions. DB table `quest_poi` is empty.");
        return;
    }

    BarGoLink bar(rows.size());

    for (QuestPoiRow const& row : rows)
    {
        bar.step();

        uint32 questId          = row.questId;
        uint32 poiId            = row.poiId;
        int32  objIndex         = row.objIndex;
        uint32 mapId            = row.mapId;
        uint32 mapAreaId        = row.mapAreaId;
        uint32 floorId          = row.floorId;
        uint32 unk3             = row.unk3;
        uint32 unk4             = row.unk4;

        QuestPOI POI(poiId, objIndex, mapId, mapAreaId, floorId, unk3, unk4);

        m_questPoi.Add(questId, POI);

        ++count;
    }

    std::vector<QuestPoiPointRow> points = QuestDataRepository::QueryQuestPoiPoints();

    for (QuestPoiPointRow const& pointRow : points)
    {
        uint32 questId      = pointRow.questId;
        uint32 poiId        = pointRow.poiId;
        int32  x            = pointRow.x;
        int32  y            = pointRow.y;

        QuestPOIVector& vect = m_questPoi.Edit(questId);

        for (QuestPOIVector::iterator itr = vect.begin(); itr != vect.end(); ++itr)
        {
            if (itr->PoiId != poiId)
            {
                continue;
            }

            QuestPOIPoint point(x, y);
            itr->points.push_back(point);
            break;
        }
    }

    sLog.outString();
    sLog.outString(">> Loaded %u quest POI definitions", count);
}


void ObjectMgr::GetConditions(uint32 conditionId, std::vector<PlayerCondition const*>& out) const
{
    const PlayerCondition* condition = sObjectMgr.GetCondition(conditionId);
    if (!condition)
    {
        return;
    }

    if (condition->m_condition == CONDITION_OR || condition->m_condition == CONDITION_AND)
    {
        GetConditions(condition->m_value1, out);
        GetConditions(condition->m_value2, out);
        return;
    }
    else if (condition->m_condition == CONDITION_NOT)
    {
        GetConditions(condition->m_value1, out);
        return;
    }

    out.push_back(condition);
}

/**
 * @brief Fills a server-side spell's zeroed records from its `spell_template` row.
 *
 * The columns land in the fields they name: the four attribute columns in `Attributes` to
 * `AttributesExC`, `duration_index` in `DurationIndex`, `proc_chance` and `proc_flags` in the aura
 * options' `ProcChance` and `ProcTypeMask`, and the `effect0` columns in effect 0. Every other field
 * stays 0. The table holds no base points, so an aura that needs them leaves the spell without its
 * effect, and so does a row with no effect.
 *
 * @param row The row.
 * @param spell The spell, as the store added it.
 */
void ObjectMgr::FillServerSpell(SpellTemplateRow const& row, ServerSpell& spell)
{
    SpellEntry& entry = spell.Entry();
    entry.ID = row.id;
    entry.Attributes = row.attributes;
    entry.AttributesEx = row.attributesEx;
    entry.AttributesExB = row.attributesEx2;
    entry.AttributesExC = row.attributesEx3;
    entry.DurationIndex = row.durationIndex;

    spell.auraOptions.ProcChance = row.procChance;
    spell.auraOptions.ProcTypeMask = row.procFlags;

    SpellEffectEntry& effect = spell.effect;
    effect.Effect = row.effect;
    effect.ImplicitTarget_0 = row.effectImplicitTargetA;
    effect.ImplicitTarget_1 = row.effectImplicitTargetB;
    effect.EffectRadiusIndex_0 = row.effectRadiusIndex;
    effect.EffectAura = row.effectApplyAuraName;
    effect.EffectMiscValue_0 = int32(row.effectMiscValue);
    effect.EffectMiscValue_1 = int32(row.effectMiscValueB);
    effect.EffectTriggerSpell = row.effectTriggerSpell;
    effect.SpellID = row.id;
    effect.EffectIndex = EFFECT_INDEX_0;

    switch (row.effectApplyAuraName)
    {
        case SPELL_AURA_NONE:
        case SPELL_AURA_DUMMY:
        case SPELL_AURA_PROC_TRIGGER_SPELL:
        case SPELL_AURA_TRANSFORM:
            spell.hasEffect = row.effect != 0;
            break;
        default:
            spell.hasEffect = false;
            break;
    }
}

/**
 * @brief Loads the server-side spells (`spell_template`) into the spell stores.
 *
 * Each spell goes into the spell store at its id, its effect into the effect map and its aura
 * options past the last aura options row. A spell `Spell.dbc` already holds is refused and the DBC
 * spell kept, and so is an id past the spell store's last row.
 */
void ObjectMgr::LoadSpellTemplate()
{
    TableRead<SpellTemplateRow> read = SpellTemplateRepository::QuerySpellTemplates();
    if (read.Unreadable())
    {
        sLog.outError("Error loading `spell_template` table: %s\n", TableReadReason(read.status));
        Log::WaitBeforeContinueIfNeed();
        exit(1);
    }

    if (read.status == TableReadStatus::Empty)
    {
        sLog.outError("spell_template table is empty!\n");
    }
    else
    {
        BarGoLink bar(read.rows.size());
        for (SpellTemplateRow const& row : read.rows)
        {
            bar.step();

            if (row.id >= sSpellStore.GetNumRows())
            {
                sLog.outErrorDb("Loading Spell Template for spell %u, index out of bounds (max = %u)", row.id, sSpellStore.GetNumRows());
                continue;
            }

            if (sSpellStore.LookupEntry(row.id))
            {
                sLog.outErrorDb("Table `spell_template` has spell %u, which Spell.dbc holds; the Spell.dbc spell is kept.", row.id);
                continue;
            }

            ServerSpell& spell = m_serverSpells.Add(row.id, row.comments);
            FillServerSpell(row, spell);
            if (!spell.hasEffect && row.effect)
            {
                sLog.outErrorDb("Table `spell_template` has spell %u with aura %u, which needs base points the table does not hold; the spell is loaded without its effect.", row.id, row.effectApplyAuraName);
            }

            spell.Entry().AuraOptionsID = sSpellAuraOptionsStore.AppendEntry(&spell.auraOptions);
            sSpellStore.InsertEntry(&spell.Entry(), row.id);
            if (spell.hasEffect)
            {
                AddSpellEffectEntry(&spell.effect);
            }
        }
    }

    sLog.outString(">> Loaded %u spell definitions", uint32(read.rows.size()));
    sLog.outString();
}

/**
 * @brief Removes stored creature spawn data and its grid mapping.
 *
 * @param guid The creature spawn GUID.
 */
void ObjectMgr::DeleteCreatureData(uint32 guid)
{
    // remove mapid*cellid -> guid_set map
    CreatureData const* data = GetCreatureData(guid);
    if (data)
    {
        RemoveCreatureFromGrid(guid, data);
    }

    mCreatureDataMap.erase(guid);
}

/**
 * @brief Removes stored gameobject spawn data and its grid mapping.
 *
 * @param guid The gameobject spawn GUID.
 */
void ObjectMgr::DeleteGOData(uint32 guid)
{
    // remove mapid*cellid -> guid_set map
    GameObjectData const* data = GetGOData(guid);
    if (data)
    {
        RemoveGameobjectFromGrid(guid, data);
    }

    mGameObjectDataMap.erase(guid);
}

/**
 * @brief Adds corpse cell lookup data for a player corpse.
 *
 * @param mapid The map id.
 * @param cellid The cell id.
 * @param player_guid The owning player GUID low part.
 * @param instance The instance id.
 */
void ObjectMgr::AddCorpseCellData(uint32 mapid, uint32 cellid, uint32 player_guid, uint32 instance)
{
    // corpses are always added to spawn mode 0 and they are spawned by their instance id
    CellObjectGuids& cell_guids = mMapObjectGuids[MAKE_PAIR64(mapid, 0)][cellid];
    cell_guids.corpses[player_guid] = instance;
}

/**
 * @brief Removes corpse cell lookup data for a player corpse.
 *
 * @param mapid The map id.
 * @param cellid The cell id.
 * @param player_guid The owning player GUID low part.
 */
void ObjectMgr::DeleteCorpseCellData(uint32 mapid, uint32 cellid, uint32 player_guid)
{
    // corpses are always added to spawn mode 0 and they are spawned by their instance id
    CellObjectGuids& cell_guids = mMapObjectGuids[MAKE_PAIR64(mapid, 0)][cellid];
    cell_guids.corpses.erase(player_guid);
}


/**
 * @brief Gets the internal locale index for a locale constant.
 *
 * @param loc The locale constant.
 * @return The locale index, or -1 for the default locale.
 */
int ObjectMgr::GetIndexForLocale(LocaleConstant loc)
{
    if (loc == LOCALE_enUS)
    {
        return -1;
    }

    for (size_t i = 0; i < m_LocalForIndex.size(); ++i)
        if (m_LocalForIndex[i] == loc)
        {
            return i;
        }

    return -1;
}

/**
 * @brief Gets the locale constant mapped to an internal locale index.
 *
 * @param i The locale index.
 * @return The mapped locale constant, or the default locale if out of range.
 */
LocaleConstant ObjectMgr::GetLocaleForIndex(int i)
{
    if (i < 0 || i >= (int32)m_LocalForIndex.size())
    {
        return LOCALE_enUS;
    }

    return m_LocalForIndex[i];
}

/**
 * @brief Gets or creates the internal locale index for a locale constant.
 *
 * @param loc The locale constant.
 * @return The locale index, or -1 for the default locale.
 */
int ObjectMgr::GetOrNewIndexForLocale(LocaleConstant loc)
{
    if (loc == LOCALE_enUS)
    {
        return -1;
    }

    for (size_t i = 0; i < m_LocalForIndex.size(); ++i)
        if (m_LocalForIndex[i] == loc)
        {
            return i;
        }

    m_LocalForIndex.push_back(loc);
    return m_LocalForIndex.size() - 1;
}


/**
 * @brief Logs a formatted script text lookup error for a string entry.
 *
 * @param entry The string entry id.
 * @param text The printf-style error text.
 */
void LogTextEntryError(int32 entry, char const* text, ...)
{
    MANGOS_ASSERT(text);

    char buf[256];
    va_list ap;
    va_start(ap, text);
    vsnprintf(buf, 256, text, ap);
    va_end(ap);

    if (entry <= MAX_CREATURE_AI_TEXT_STRING_ID)            // script library error
    {
        sLog.outErrorScriptLib("%s", buf);
    }
    else if (entry <= MIN_CREATURE_AI_TEXT_STRING_ID)       // eventAI error
    {
        sLog.outErrorEventAI("%s", buf);
    }
    else if (entry < MIN_DB_SCRIPT_STRING_ID)               // mangos string error
    {
        sLog.outError("%s", buf);
    }
    else // if (entry > MIN_DB_SCRIPT_STRING_ID)            // DB script text error
    {
        sLog.outErrorDb("DB-SCRIPTS: %s", buf);
    }
}

/**
 * @brief Loads localized string templates from a database table.
 *
 * @param db The database to query.
 * @param table The source table name.
 * @param min_value The inclusive lower id bound.
 * @param max_value The exclusive upper id bound.
 * @param extra_content true to also load sound/chat metadata.
 * @return true if the load succeeded; otherwise, false.
 */
bool ObjectMgr::LoadMangosStrings(DatabaseType& db, char const* table, int32 min_value, int32 max_value, bool extra_content)
{
    int32 start_value = min_value;
    int32 end_value   = max_value;
    // some string can have negative indexes range
    if (start_value < 0)
    {
        if (end_value >= start_value)
        {
            sLog.outErrorDb("Table '%s' attempt loaded with invalid range (%d - %d), strings not loaded.", table, min_value, max_value);
            return false;
        }

        // real range (max+1,min+1) exaple: (-10,-1000) -> -999...-10+1
        std::swap(start_value, end_value);
        ++start_value;
        ++end_value;
    }
    else
    {
        if (start_value >= end_value)
        {
            sLog.outErrorDb("Table '%s' attempt loaded with invalid range (%d - %d), strings not loaded.", table, min_value, max_value);
            return false;
        }
    }

    // cleanup affected map part for reloading case
    for (MangosStringLocaleMap::iterator itr = mMangosStringLocaleMap.begin(); itr != mMangosStringLocaleMap.end();)
    {
        if (itr->first >= start_value && itr->first < end_value)
        {
            mMangosStringLocaleMap.erase(itr++);
        }
        else
        {
            ++itr;
        }
    }

    sLog.outString("Loading texts from %s%s", table, extra_content ? ", with additional data" : "");

    std::vector<StringRow> rows = LocaleDataRepository::QueryStrings(db, table, extra_content);

    if (rows.empty())
    {
        BarGoLink bar(1);

        bar.step();

        sLog.outString();
        if (min_value == MIN_MANGOS_STRING_ID)              // error only in case internal strings
        {
            sLog.outErrorDb(">> Loaded 0 mangos strings. DB table `%s` is empty. Can not continue.", table);
        }
        else
        {
            sLog.outString(">> Loaded 0 string templates. DB table `%s` is empty.", table);
        }
        return false;
    }

    uint32 count = 0;

    BarGoLink bar(rows.size());

    for (StringRow const& row : rows)
    {
        bar.step();

        int32 entry = row.entry;

        if (entry == 0)
        {
            LogTextEntryError(start_value, "Table `%s` contain reserved entry 0, ignored.", table);
            continue;
        }
        else if (entry < start_value || entry >= end_value)
        {
            LogTextEntryError(start_value, "Table `%s` contain entry %i out of allowed range (%d - %d), ignored.", table, entry, min_value, max_value);
            continue;
        }

        MangosStringLocale& data = mMangosStringLocaleMap[entry];

        if (!data.Content.empty())
        {
            LogTextEntryError(entry, "Table `%s` contain data for already loaded entry  %i (from another table?), ignored.", table, entry);
            continue;
        }

        data.Content.resize(1);
        ++count;

        // 0 -> default, idx in to idx+1
        data.Content[0] = row.contentDefault;

        for (int i = 1; i < MAX_LOCALE; ++i)
        {
            std::string str = row.content[i - 1];
            if (!str.empty())
            {
                int idx = GetOrNewIndexForLocale(LocaleConstant(i));
                if (idx >= 0)
                {
                    // 0 -> default, idx in to idx+1
                    if ((int32)data.Content.size() <= idx + 1)
                    {
                        data.Content.resize(idx + 2);
                    }

                    data.Content[idx + 1] = str;
                }
            }
        }

        // Load additional string content if necessary
        if (extra_content)
        {
            data.SoundId     = row.sound;
            data.Type        = row.type;
            data.LanguageId  = Language(row.language);
            data.Emote       = row.emote;

            if (data.SoundId && !sSoundEntriesStore.LookupEntry(data.SoundId))
            {
                LogTextEntryError(entry, "Entry %i in table `%s` has soundId %u but sound does not exist.", entry, table, data.SoundId);
                data.SoundId = 0;
            }

            if (!GetLanguageDescByID(data.LanguageId))
            {
                LogTextEntryError(entry, "Entry %i in table `%s` using Language %u but Language does not exist.", entry, table, uint32(data.LanguageId));
                data.LanguageId = LANG_UNIVERSAL;
            }

            if (data.Type > CHAT_TYPE_ZONE_YELL)
            {
                LogTextEntryError(entry, "Entry %i in table `%s` has Type %u but this Chat Type does not exist.", entry, table, data.Type);
                data.Type = CHAT_TYPE_SAY;
            }

            if (data.Emote && !sEmotesStore.LookupEntry(data.Emote))
            {
                LogTextEntryError(entry, "Entry %i in table `%s` has Emote %u but emote does not exist.", entry, table, data.Emote);
                data.Emote = EMOTE_ONESHOT_NONE;
            }
        }
    }


    if (min_value == MIN_MANGOS_STRING_ID)
    {
        sLog.outString(">> Loaded %u MaNGOS strings from table %s", count, table);
    }
    else
    {
        sLog.outString(">> Loaded %u %s templates from %s", count, extra_content ? "text" : "string", table);
    }
    sLog.outString();

    m_loadedStringCount[min_value] = count;

    return true;
}

/**
 * @brief Gets a localized MaNGOS string entry.
 *
 * @param entry The string entry id.
 * @param locale_idx The internal locale index.
 * @return The localized text, or a fallback error string.
 */
const char* ObjectMgr::GetMangosString(int32 entry, int locale_idx) const
{
    // locale_idx==-1 -> default, locale_idx >= 0 in to idx+1
    // Content[0] always exist if exist MangosStringLocale
    if (MangosStringLocale const* msl = GetMangosStringLocale(entry))
    {
        if ((int32)msl->Content.size() > locale_idx + 1 && !msl->Content[locale_idx + 1].empty())
        {
            return msl->Content[locale_idx + 1].c_str();
        }
        else
        {
            return msl->Content[0].c_str();
        }
    }

    LogTextEntryError(entry, "Entry %i not found but requested", entry);

    return "<error>";
}


bool ObjectMgr::CheckDeclinedNames(const std::wstring& mainpart, DeclinedName const& names)
{
    for (int i = 0; i < MAX_DECLINED_NAME_CASES; ++i)
    {
        std::wstring wname;
        if (!Utf8toWStr(names.name[i], wname))
        {
            return false;
        }

        if (mainpart != GetMainPartOfName(wname, i + 1))
        {
            return false;
        }
    }
    return true;
}

// Verification of condition values validity
bool PlayerCondition::IsValid(uint16 entry, ConditionType condition, uint32 value1, uint32 value2)
{
    switch (condition)
    {
        case CONDITION_NOT:
        {
            if (value1 >= entry)
            {
                sLog.outErrorDb("CONDITION_NOT (entry %u, type %d) has invalid value1 %u, must be lower than entry, skipped", entry, condition, value1);
                return false;
            }
            const PlayerCondition* condition1 = sObjectMgr.GetCondition(value1);
            if (!condition1)
            {
                sLog.outErrorDb("CONDITION_NOT (entry %u, type %d) has value1 %u without proper condition, skipped", entry, condition, value1);
                return false;
            }
            break;
        }
        case CONDITION_OR:
        case CONDITION_AND:
        {
            if (value1 >= entry)
            {
                sLog.outErrorDb("CONDITION _AND or _OR (entry %u, type %d) has invalid value1 %u, must be lower than entry, skipped", entry, condition, value1);
                return false;
            }
            if (value2 >= entry)
            {
                sLog.outErrorDb("CONDITION _AND or _OR (entry %u, type %d) has invalid value2 %u, must be lower than entry, skipped", entry, condition, value2);
                return false;
            }
            const PlayerCondition* condition1 = sObjectMgr.GetCondition(value1);
            if (!condition1)
            {
                sLog.outErrorDb("CONDITION _AND or _OR (entry %u, type %d) has value1 %u without proper condition, skipped", entry, condition, value1);
                return false;
            }
            const PlayerCondition* condition2 = sObjectMgr.GetCondition(value2);
            if (!condition2)
            {
                sLog.outErrorDb("CONDITION _AND or _OR (entry %u, type %d) has value2 %u without proper condition, skipped", entry, condition, value2);
                return false;
            }
            break;
        }
        case CONDITION_AURA:
        case CONDITION_SOURCE_AURA:
        {
            if (!sSpellStore.LookupEntry(value1))
            {
                sLog.outErrorDb("Aura condition (entry %u, type %u) requires to have non existing spell (Id: %d), skipped", entry, condition, value1);
                return false;
            }
            if (value2 >= MAX_EFFECT_INDEX)
            {
                sLog.outErrorDb("Aura condition (entry %u, type %u) requires to have non existing effect index (%u) (must be 0..%u), skipped", entry, condition, value2, MAX_EFFECT_INDEX - 1);
                return false;
            }
            break;
        }
        case CONDITION_ITEM:
        case CONDITION_NOITEM:
        case CONDITION_ITEM_WITH_BANK:
        case CONDITION_NOITEM_WITH_BANK:
        {
            ItemPrototype const* proto = ObjectMgr::GetItemPrototype(value1);
            if (!proto)
            {
                sLog.outErrorDb("Item condition (entry %u, type %u) requires to have non existing item (%u), skipped", entry, condition, value1);
                return false;
            }

            if (value2 < 1)
            {
                sLog.outErrorDb("Item condition (entry %u, type %u) useless with count < 1, skipped", entry, condition);
                return false;
            }
            break;
        }
        case CONDITION_ITEM_EQUIPPED:
        {
            ItemPrototype const* proto = ObjectMgr::GetItemPrototype(value1);
            if (!proto)
            {
                sLog.outErrorDb("ItemEquipped condition (entry %u, type %u) requires to have non existing item (%u) equipped, skipped", entry, condition, value1);
                return false;
            }
            break;
        }
        case CONDITION_AREAID:
        {
            AreaTableEntry const* areaEntry = GetAreaEntryByAreaID(value1);
            if (!areaEntry)
            {
                sLog.outErrorDb("Zone condition (entry %u, type %u) requires to be in non existing area (%u), skipped", entry, condition, value1);
                return false;
            }

            if (value2 > 1)
            {
                sLog.outErrorDb("Zone condition (entry %u, type %u) has invalid argument %u (must be 0..1), skipped", entry, condition, value2);
                return false;
            }
            break;
        }
        case CONDITION_REPUTATION_RANK_MIN:
        case CONDITION_REPUTATION_RANK_MAX:
        {
            FactionEntry const* factionEntry = sFactionStore.LookupEntry(value1);
            if (!factionEntry)
            {
                sLog.outErrorDb("Reputation condition (entry %u, type %u) requires to have reputation non existing faction (%u), skipped", entry, condition, value1);
                return false;
            }

            if (value2 >= MAX_REPUTATION_RANK)
            {
                sLog.outErrorDb("Reputation condition (entry %u, type %u) has invalid rank requirement (value2 = %u) - must be between %u and %u, skipped", entry, condition, value2, MIN_REPUTATION_RANK, MAX_REPUTATION_RANK - 1);
                return false;
            }
            break;
        }
        case CONDITION_TEAM:
        {
            if (value1 != ALLIANCE && value1 != HORDE)
            {
                sLog.outErrorDb("Team condition (entry %u, type %u) specifies unknown team (%u), skipped", entry, condition, value1);
                return false;
            }
            break;
        }
        case CONDITION_SKILL:
        case CONDITION_SKILL_BELOW:
        {
            SkillLineEntry const* pSkill = sSkillLineStore.LookupEntry(value1);
            if (!pSkill)
            {
                sLog.outErrorDb("Skill condition (entry %u, type %u) specifies non-existing skill (%u), skipped", entry, condition, value1);
                return false;
            }
            if (value2 < 1 || value2 > sGameConfig.GetConfigMaxSkillValue())
            {
                sLog.outErrorDb("Skill condition (entry %u, type %u) specifies invalid skill value (%u), skipped", entry, condition, value2);
                return false;
            }
            break;
        }
        case CONDITION_QUESTREWARDED:
        case CONDITION_QUESTTAKEN:
        case CONDITION_QUESTAVAILABLE:
        case CONDITION_QUEST_NONE:
        {
            Quest const* Quest = sObjectMgr.GetQuestTemplate(value1);
            if (!Quest)
            {
                sLog.outErrorDb("Quest condition (entry %u, type %u) specifies non-existing quest (%u), skipped", entry, condition, value1);
                return false;
            }

            if (value2 && condition != CONDITION_QUESTTAKEN)
            {
                sLog.outErrorDb("Quest condition (entry %u, type %u) has useless data in value2 (%u)!", entry, condition, value2);
            }
            break;
        }
        case CONDITION_AD_COMMISSION_AURA:
        {
            if (value1)
            {
                sLog.outErrorDb("Quest condition (entry %u, type %u) has useless data in value1 (%u)!", entry, condition, value1);
            }
            if (value2)
            {
                sLog.outErrorDb("Quest condition (entry %u, type %u) has useless data in value2 (%u)!", entry, condition, value2);
            }
            break;
        }
        case CONDITION_NO_AURA:
        {
            if (!sSpellStore.LookupEntry(value1))
            {
                sLog.outErrorDb("Aura condition (entry %u, type %u) requires to have non existing spell (Id: %d), skipped", entry, condition, value1);
                return false;
            }
            if (value2 > MAX_EFFECT_INDEX)
            {
                sLog.outErrorDb("Aura condition (entry %u, type %u) requires to have non existing effect index (%u) (must be 0..%u), skipped", entry, condition, value2, MAX_EFFECT_INDEX - 1);
                return false;
            }
            break;
        }
        case CONDITION_ACTIVE_GAME_EVENT:
        case CONDITION_NOT_ACTIVE_GAME_EVENT:
        {
            if (!sGameEventMgr.IsValidEvent(value1))
            {
                sLog.outErrorDb("(Not)Active event condition (entry %u, type %u) requires existing event id (%u), skipped", entry, condition, value1);
                return false;
            }
            break;
        }
        case CONDITION_AREA_FLAG:
        {
            if (!value1 && !value2)
            {
                sLog.outErrorDb("Area flag condition (entry %u, type %u) has both values like 0, skipped", entry, condition);
                return false;
            }
            break;
        }
        case CONDITION_RACE_CLASS:
        {
            if (!value1 && !value2)
            {
                sLog.outErrorDb("Race_class condition (entry %u, type %u) has both values like 0, skipped", entry, condition);
                return false;
            }

            if (value1 && !(value1 & RACEMASK_ALL_PLAYABLE))
            {
                sLog.outErrorDb("Race_class condition (entry %u, type %u) has invalid player class %u, skipped", entry, condition, value1);
                return false;
            }

            if (value2 && !(value2 & CLASSMASK_ALL_PLAYABLE))
            {
                sLog.outErrorDb("Race_class condition (entry %u, type %u) has invalid race mask %u, skipped", entry, condition, value2);
                return false;
            }
            break;
        }
        case CONDITION_LEVEL:
        {
            if (!value1 || value1 > sGameConfig.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL))
            {
                sLog.outErrorDb("Level condition (entry %u, type %u)has invalid level %u, skipped", entry, condition, value1);
                return false;
            }

            if (value2 > 2)
            {
                sLog.outErrorDb("Level condition (entry %u, type %u) has invalid argument %u (must be 0..2), skipped", entry, condition, value2);
                return false;
            }

            break;
        }
        case CONDITION_SPELL:
        {
            if (!sSpellStore.LookupEntry(value1))
            {
                sLog.outErrorDb("Spell condition (entry %u, type %u) requires to have non existing spell (Id: %d), skipped", entry, condition, value1);
                return false;
            }

            if (value2 > 1)
            {
                sLog.outErrorDb("Spell condition (entry %u, type %u) has invalid argument %u (must be 0..1), skipped", entry, condition, value2);
                return false;
            }

            break;
        }
        case CONDITION_INSTANCE_SCRIPT:
            break;
        case CONDITION_ACHIEVEMENT:
        case CONDITION_ACHIEVEMENT_REALM:
        {
            if (!sAchievementStore.LookupEntry(value1))
            {
                sLog.outErrorDb("Achievement condition (entry %u, type %u) requires to have non existing achievement (Id: %d), skipped", entry, condition, value1);
                return false;
            }

            if (value2 > 1)
            {
                sLog.outErrorDb("Achievement condition (entry %u, type %u) has invalid argument %u (must be 0..1), skipped", entry, condition, value2);
                return false;
            }

            break;
        }
        case CONDITION_ACTIVE_HOLIDAY:
        case CONDITION_NOT_ACTIVE_HOLIDAY:
        {
            if (!sHolidaysStore.LookupEntry(value1))
            {
                sLog.outErrorDb("(Not)Active holiday condition (entry %u, type %u) requires existing holiday id (%u), skipped", entry, condition, value1);
                return false;
            }
            break;
        }
        case CONDITION_LEARNABLE_ABILITY:
        {
            SkillLineAbilityMapBounds bounds = sSpellMgr.GetSkillLineAbilityMapBounds(value1);

            if (bounds.first == bounds.second)
            {
                sLog.outErrorDb("Learnable ability condition (entry %u, type %u) has spell id %u defined, but this spell is not listed in SkillLineAbility and can not be used, skipping.", entry, condition, value1);
                return false;
            }

            if (value2)
            {
                ItemPrototype const* proto = ObjectMgr::GetItemPrototype(value2);
                if (!proto)
                {
                    sLog.outErrorDb("Learnable ability condition (entry %u, type %u) has item entry %u defined but item does not exist, skipping.", entry, condition, value2);
                    return false;
                }
            }

            break;
        }
        case CONDITION_COMPLETED_ENCOUNTER:
        {
            DungeonEncounterEntry const* dbcEntry1 = sDungeonEncounterStore.LookupEntry(value1);
            DungeonEncounterEntry const* dbcEntry2 = sDungeonEncounterStore.LookupEntry(value2);
            if (!dbcEntry1)
            {
                sLog.outErrorDb("Completed Encounter condition (entry %u, type %u) has an unknown DungeonEncounter entry %u defined (in value1), skipping.", entry, condition, value1);
                return false;
            }
            if (value2 && !dbcEntry2)
            {
                sLog.outErrorDb("Completed Encounter condition (entry %u, type %u) has an unknown DungeonEncounter entry %u defined (in value2), skipping.", entry, condition, value2);
                return false;
            }
            if (dbcEntry2 && dbcEntry1->MapID != dbcEntry2->MapID)
            {
                sLog.outErrorDb("Completed Encounter condition (entry %u, type %u) has different mapIds for both encounters, skipping.", entry, condition);
                return false;
            }
            break;
        }
        case CONDITION_LAST_WAYPOINT:
        {
            if (value2 > 2)
            {
                sLog.outErrorDb("Last Waypoint condition (entry %u, type %u) has an invalid value in value2. (Has %u, supported 0, 1, or 2), skipping.", entry, condition, value2);
                return false;
            }
            break;
        }
        case CONDITION_XP_USER:
        {
            if (value1 > 1)
            {
                sLog.outErrorDb("XP user condition (entry %u, type %u) has invalid argument %u (must be 0..1), skipped", entry, condition, value1);
                return false;
            }

            if (value2)
            {
                sLog.outErrorDb("XP user condition (entry %u, type %u) has useless data in value2 (%u)!", entry, condition, value2);
            }

            break;
        }
        case CONDITION_GENDER:
        {
            if (value1 >= MAX_GENDER)
            {
                sLog.outErrorDb("Gender condition (entry %u, type %u) has an invalid value in value1. (Has %u, must be smaller than %u), skipping.", entry, condition, value1, MAX_GENDER);
                return false;
            }
            break;
        }
        case CONDITION_DEAD_OR_AWAY:
        {
            if (value1 >= 4)
            {
                sLog.outErrorDb("Dead condition (entry %u, type %u) has an invalid value in value1. (Has %u, must be smaller than 4), skipping.", entry, condition, value1);
                return false;
            }
            break;
        }
        case CONDITION_CREATURE_IN_RANGE:
        {
            if (!ObjectMgr::GetCreatureTemplate(value1))
            {
                sLog.outErrorDb("Creature in range condition (entry %u, type %u) has an invalid value in value1. (Creature %u does not exist in the database), skipping.", entry, condition, value1);
                return false;
            }
            if (value2 <= 0)
            {
                sLog.outErrorDb("Creature in range condition (entry %u, type %u) has an invalid value in value2. (Range %u must be greater than 0), skipping.", entry, condition, value2);
                return false;
            }
            break;
        }
        case CONDITION_GAMEOBJECT_IN_RANGE:
        {
            if (!ObjectMgr::GetGameObjectInfo(value1))
            {
                sLog.outErrorDb("Game object in range condition (entry %u, type %u) has an invalid value in value1 (gameobject). (Game object %u does not exist in the database), skipping.", entry, condition, value1);
                return false;
            }
            if (value2 <= 0)
            {
                sLog.outErrorDb("Game object in range condition (entry %u, type %u) has an invalid value in value2 (range). (Range %u must be greater than 0), skipping.", entry, condition, value2);
                return false;
            }
            break;
        }
        case CONDITION_NONE:
            break;
        default:
            sLog.outErrorDb("Condition entry %u has bad type of %d, skipped ", entry, condition);
            return false;
    }
    return true;
}

// Check if a condition can be used without providing a player param
bool PlayerCondition::CanBeUsedWithoutPlayer(uint16 entry)
{
    PlayerCondition const* condition = sObjectMgr.GetCondition(entry);
    if (!condition)
    {
        return false;
    }

    switch (condition->m_condition)
    {
        case CONDITION_NOT:
            return CanBeUsedWithoutPlayer(condition->m_value1);
        case CONDITION_AND:
        case CONDITION_OR:
            return CanBeUsedWithoutPlayer(condition->m_value1) && CanBeUsedWithoutPlayer(condition->m_value2);
        case CONDITION_NONE:
        case CONDITION_ACTIVE_GAME_EVENT:
        case CONDITION_ACHIEVEMENT_REALM:
        case CONDITION_NOT_ACTIVE_GAME_EVENT:
        case CONDITION_ACTIVE_HOLIDAY:
        case CONDITION_NOT_ACTIVE_HOLIDAY:
        case CONDITION_AREAID:
        case CONDITION_AREA_FLAG:
        case CONDITION_INSTANCE_SCRIPT:
        case CONDITION_COMPLETED_ENCOUNTER:
        case CONDITION_SOURCE_AURA:
        case CONDITION_LAST_WAYPOINT:
            return true;
        default:
            return false;
    }
}

/**
 * @brief Determines the training range type used by a skill line.
 *
 * @param pSkill The skill line entry.
 * @param racial True if the skill is racial.
 * @return SkillRangeType The applicable skill range type.
 */
SkillRangeType GetSkillRangeType(SkillLineEntry const* pSkill, bool racial)
{
    switch (pSkill->CategoryID)
    {
        case SKILL_CATEGORY_LANGUAGES:
            return SKILL_RANGE_LANGUAGE;
        case SKILL_CATEGORY_WEAPON:
            return SKILL_RANGE_LEVEL;
        case SKILL_CATEGORY_ARMOR:
        case SKILL_CATEGORY_CLASS:
            return SKILL_RANGE_MONO;
        case SKILL_CATEGORY_SECONDARY:
        case SKILL_CATEGORY_PROFESSION:
            // not set skills for professions and racial abilities
            if (IsProfessionSkill(pSkill->ID))
            {
                return SKILL_RANGE_RANK;
            }
            else if (racial)
            {
                return SKILL_RANGE_NONE;
            }
            else
            {
                return SKILL_RANGE_MONO;
            }
        default:
        case SKILL_CATEGORY_ATTRIBUTES:                     // not found in dbc
        case SKILL_CATEGORY_GENERIC:                        // only GENERIC(DND)
            return SKILL_RANGE_NONE;
    }
}

void ObjectMgr::LoadMailLevelRewards()
{
    m_mailLevelRewards.Clear();                             // for reload case

    uint32 count = 0;
    std::vector<LevelRewardMailRow> rows = PlayerDataRepository::QueryLevelRewardMails();

    if (rows.empty())
    {
        BarGoLink bar(1);

        bar.step();

        sLog.outString();
        sLog.outErrorDb(">> Loaded `mail_level_reward`, table is empty!");
        return;
    }

    BarGoLink bar(rows.size());

    for (LevelRewardMailRow const& row : rows)
    {
        bar.step();

        uint8 level           = row.level;
        uint32 raceMask       = row.raceMask;
        uint32 mailTemplateId = row.mailTemplateId;
        uint32 senderEntry    = row.senderEntry;

        if (level > MAX_LEVEL)
        {
            sLog.outErrorDb("Table `mail_level_reward` have data for level %u that more supported by client (%u), ignoring.", level, MAX_LEVEL);
            continue;
        }

        if (!(raceMask & RACEMASK_ALL_PLAYABLE))
        {
            sLog.outErrorDb("Table `mail_level_reward` have raceMask (%u) for level %u that not include any player races, ignoring.", raceMask, level);
            continue;
        }

        if (!sMailTemplateStore.LookupEntry(mailTemplateId))
        {
            sLog.outErrorDb("Table `mail_level_reward` have invalid mailTemplateId (%u) for level %u that invalid not include any player races, ignoring.", mailTemplateId, level);
            continue;
        }

        if (!GetCreatureTemplate(senderEntry))
        {
            sLog.outErrorDb("Table `mail_level_reward` have nonexistent sender creature entry (%u) for level %u that invalid not include any player races, ignoring.", senderEntry, level);
            continue;
        }

        m_mailLevelRewards.Add(level, MailLevelReward(raceMask, mailTemplateId, senderEntry));

        ++count;
    }

    sLog.outString();
    sLog.outString(">> Loaded %u level dependent mail rewards,", count);
}

void ObjectMgr::AddVendorItem(uint32 entry, uint32 item, uint8 type, uint32 maxcount, uint32 incrtime, uint32 extendedcost)
{
    VendorItemData& vList = m_mCacheVendorItemMap[entry];
    vList.AddItem(item, type, maxcount, incrtime, extendedcost, 0);

    NpcDataRepository::InsertVendorItem(entry, type == VENDOR_ITEM_TYPE_CURRENCY ? -int32(item) : item, maxcount, incrtime, extendedcost);
}

void ObjectMgr::AddVendorItem(uint32 entry, uint32 item, uint32 maxcount, uint32 incrtime, uint32 extendedcost)
{
    VendorItemData& vList = m_mCacheVendorItemMap[entry];
    uint8 type = VENDOR_ITEM_TYPE_CURRENCY ? -int32(item) : item;
    vList.AddItem(item, type, maxcount, incrtime, extendedcost, 0);

    NpcDataRepository::InsertVendorItem(entry, type == VENDOR_ITEM_TYPE_CURRENCY ? -int32(item) : item, maxcount, incrtime, extendedcost);
}

/**
 * @brief Removes a vendor item from a creature vendor list and database.
 *
 * @param entry The vendor creature entry.
 * @param item The item entry.
 * @return true if the item was removed; otherwise, false.
 */
bool ObjectMgr::RemoveVendorItem(uint32 entry, uint32 item, uint8 type)
{
    CacheVendorItemMap::iterator  iter = m_mCacheVendorItemMap.find(entry);
    if (iter == m_mCacheVendorItemMap.end())
    {
        return false;
    }

    if (!iter->second.RemoveItem(item, type))
    {
        return false;
    }

    NpcDataRepository::DeleteVendorItem(entry, type == VENDOR_ITEM_TYPE_CURRENCY ? -int32(item) : item);
    return true;
}

/**
 * @brief Validates a vendor item definition for a vendor or vendor template.
 *
 * @param isTemplate true when validating a vendor template.
 * @param tableName The source table name.
 * @param vendor_entry The vendor or template entry id.
 * @param item_id The item entry id.
 * @param maxcount The limited stock count.
 * @param incrtime The stock replenishment interval.
 * @param conditionId The optional condition id.
 * @param problems Where a GM's command collects the problems found, to report them; NULL logs them instead.
 * @param skip_vendors Optional set used to suppress repeated vendor errors.
 * @return true if the vendor item definition is valid; otherwise, false.
 */
bool ObjectMgr::IsVendorItemValid(bool isTemplate, char const* tableName, uint32 vendor_entry, uint32 item_id, uint32 maxcount, uint32 incrtime, uint32 ExtendedCost, uint16 conditionId, std::vector<VendorItemProblem>* problems, std::set<uint32>* skip_vendors) const
{
    char const* idStr = isTemplate ? "vendor template" : "vendor";
    CreatureInfo const* cInfo = NULL;

    if (!isTemplate)
    {
        cInfo = GetCreatureTemplate(vendor_entry);
        if (!cInfo)
        {
            if (problems)
            {
                problems->push_back(VendorItemProblem::Entry(LANG_COMMAND_VENDORSELECTION));
            }
            else
            {
                sLog.outErrorDb("Table `%s` has data for nonexistent creature (Entry: %u), ignoring", tableName, vendor_entry);
            }
            return false;
        }

        if (!(cInfo->NpcFlags & UNIT_NPC_FLAG_VENDOR))
        {
            if (!skip_vendors || skip_vendors->count(vendor_entry) == 0)
            {
                if (problems)
                {
                    problems->push_back(VendorItemProblem::Entry(LANG_COMMAND_VENDORSELECTION));
                }
                else
                {
                    sLog.outErrorDb("Table `%s` has data for creature (Entry: %u) without vendor flag, ignoring", tableName, vendor_entry);
                }

                if (skip_vendors)
                {
                    skip_vendors->insert(vendor_entry);
                }
            }
            return false;
        }
    }

    if (!GetItemPrototype(item_id))
    {
        if (problems)
        {
            problems->push_back(VendorItemProblem::Entry(LANG_ITEM_NOT_FOUND, item_id));
        }
        else
            sLog.outErrorDb("Table `%s` for %s %u contain nonexistent item (%u), ignoring",
                            tableName, idStr, vendor_entry, item_id);
        return false;
    }

    if (ExtendedCost && !sItemExtendedCostStore.LookupEntry(ExtendedCost))
    {
        if (problems)
        {
            problems->push_back(VendorItemProblem::Entry(LANG_EXTENDED_COST_NOT_EXIST, ExtendedCost));
        }
        else
            sLog.outErrorDb("Table `%s` contain item (Entry: %u) with wrong ExtendedCost (%u) for %s %u, ignoring",
                            tableName, item_id, ExtendedCost, idStr, vendor_entry);
        return false;
    }

    if (maxcount > 0 && incrtime == 0)
    {
        if (problems)
        {
            problems->push_back(VendorItemProblem::Text("MaxCount!=0 (%u) but IncrTime==0", maxcount));
        }
        else
            sLog.outErrorDb("Table `%s` has `maxcount` (%u) for item %u of %s %u but `incrtime`=0, ignoring",
                            tableName, maxcount, item_id, idStr, vendor_entry);
        return false;
    }
    else if (maxcount == 0 && incrtime > 0)
    {
        if (problems)
        {
            problems->push_back(VendorItemProblem::Text("MaxCount==0 but IncrTime<>=0"));
        }
        else
            sLog.outErrorDb("Table `%s` has `maxcount`=0 for item %u of %s %u but `incrtime`<>0, ignoring",
                            tableName, item_id, idStr, vendor_entry);
        return false;
    }

    if (conditionId && !sObjectMgr.GetCondition(conditionId))
    {
        sLog.outErrorDb("Table `%s` has `condition_id`=%u for item %u of %s %u but this condition is not valid, ignoring", tableName, conditionId, item_id, idStr, vendor_entry);
        return false;
    }

    VendorItemData const* vItems = isTemplate ? GetNpcVendorTemplateItemList(vendor_entry) : GetNpcVendorItemList(vendor_entry);
    VendorItemData const* tItems = isTemplate ? NULL : GetNpcVendorTemplateItemList(vendor_entry);

    if (!vItems && !tItems)
    {
        return true;                                        // later checks for non-empty lists
    }

    if (vItems && vItems->FindItem(item_id))
    {
        if (problems)
        {
            problems->push_back(VendorItemProblem::Entry(LANG_ITEM_ALREADY_IN_LIST, item_id));
        }
        else
        {
            sLog.outErrorDb("Table `%s` has duplicate items %u for %s %u, ignoring",
            tableName, item_id, idStr, vendor_entry);
        }
        return false;
    }

    if (!isTemplate)
    {
        if (tItems && tItems->GetItem(item_id))
        {
            if (problems)
            {
                problems->push_back(VendorItemProblem::Entry(LANG_ITEM_ALREADY_IN_LIST, item_id));
            }
            else
            {
                if (!cInfo->VendorTemplateId)
                    sLog.outErrorDb("Table `%s` has duplicate items %u for %s %u, ignoring",
                    tableName, item_id, idStr, vendor_entry);
                else
                    sLog.outErrorDb("Table `%s` has duplicate items %u for %s %u (or possible in vendor template %u), ignoring",
                    tableName, item_id, idStr, vendor_entry, cInfo->VendorTemplateId);
            }
            return false;
        }
    }

    uint32 countItems = vItems ? vItems->GetItemCount() : 0;
    countItems += tItems ? tItems->GetItemCount() : 0;

    if (countItems >= MAX_VENDOR_ITEMS)
    {
        if (problems)
        {
            problems->push_back(VendorItemProblem::Entry(LANG_COMMAND_ADDVENDORITEMITEMS));
        }
        else
            sLog.outErrorDb("Table `%s` has too many items (%u >= %i) for %s %u, ignoring",
            tableName, countItems, MAX_VENDOR_ITEMS, idStr, vendor_entry);
        return false;
    }

    return true;
}

/*
   This is the Pre Dev21 version of the function
   Delete this if the above works?
*/
bool ObjectMgr::IsVendorItemValid(bool isTemplate, char const* tableName, uint32 vendor_entry, uint32 item_id, uint8 type, uint32 maxcount, uint32 incrtime, uint32 ExtendedCost, uint16 conditionId, std::vector<VendorItemProblem>* problems, std::set<uint32>* skip_vendors) const
{
    char const* idStr = isTemplate ? "vendor template" : "vendor";
    char const* nameStr = type == VENDOR_ITEM_TYPE_CURRENCY ? "Currency" : "Item";
    CreatureInfo const* cInfo = NULL;

    if (!isTemplate)
    {
        cInfo = GetCreatureTemplate(vendor_entry);
        if (!cInfo)
        {
            if (problems)
            {
                problems->push_back(VendorItemProblem::Entry(LANG_COMMAND_VENDORSELECTION));
            }
            else
                sLog.outErrorDb("Table `%s` has data for nonexistent creature (Entry: %u), ignoring", tableName, vendor_entry);
            return false;
        }

        if (!(cInfo->NpcFlags & UNIT_NPC_FLAG_VENDOR))
        {
            if (!skip_vendors || skip_vendors->count(vendor_entry) == 0)
            {
                if (problems)
                {
                    problems->push_back(VendorItemProblem::Entry(LANG_COMMAND_VENDORSELECTION));
                }
                else
                    sLog.outErrorDb("Table `%s` has data for creature (Entry: %u) without vendor flag, ignoring", tableName, vendor_entry);

                if (skip_vendors)
                {
                    skip_vendors->insert(vendor_entry);
                }
            }
            return false;
        }
    }

    CurrencyTypesEntry const * currencyEntry;
    if (type == VENDOR_ITEM_TYPE_ITEM)
    {
        if (!GetItemPrototype(item_id))
        {
            if (problems)
            {
                problems->push_back(VendorItemProblem::Entry(LANG_ITEM_NOT_FOUND, item_id));
            }
            else
                sLog.outErrorDb("Table `%s` for %s %u contains nonexistent item (%u), ignoring",
                                tableName, idStr, vendor_entry, item_id);
            return false;
        }
    }
    else if (type == VENDOR_ITEM_TYPE_CURRENCY)
    {
        currencyEntry = sCurrencyTypesStore.LookupEntry(item_id);
        if (!currencyEntry)
        {
            if (problems)
            {
                problems->push_back(VendorItemProblem::Entry(LANG_CURRENCY_NOT_FOUND, item_id));
            }
            else
                sLog.outErrorDb("Table `%s` for %s %u contains nonexistent currency (%u), ignoring",
                                tableName, idStr, vendor_entry, item_id);
            return false;
        }
        else
        {
            if (currencyEntry->CategoryID == CURRENCY_CATEGORY_META)
            {
                if (problems)
                {
                    problems->push_back(VendorItemProblem::Entry(LANG_VENDOR_META_CURRENCY_NOT_ALLOWED, item_id));
                }
                else
                    sLog.outErrorDb("Table `%s` for %s %u contains not allowed meta currency (%u), ignoring",
                                    tableName, idStr, vendor_entry, item_id);
                return false;
            }
        }
    }
    else
    {
        if (problems)
        {
            problems->push_back(VendorItemProblem::Entry(LANG_VENDOR_WRONG_ITEM_TYPE, item_id, type));
        }
        else
            sLog.outErrorDb("Table `%s` for %s %u contains nonexistent vendor item type %u (entry %u), ignoring",
                            tableName, idStr, vendor_entry, type, item_id);
    }

    if (ExtendedCost && !sItemExtendedCostStore.LookupEntry(ExtendedCost))
    {
        if (problems)
        {
            problems->push_back(VendorItemProblem::Entry(LANG_EXTENDED_COST_NOT_EXIST, ExtendedCost));
        }
        else
            sLog.outErrorDb("Table `%s` contains %s (Entry: %u) with wrong ExtendedCost (%u) for %s %u, ignoring",
                                    tableName, nameStr, item_id, ExtendedCost, idStr, vendor_entry);
        return false;
    }

    if (type == VENDOR_ITEM_TYPE_ITEM)
    {
        if (maxcount > 0 && incrtime == 0)
        {
            if (problems)
            {
                problems->push_back(VendorItemProblem::Text("MaxCount!=0 (%u) but IncrTime==0", maxcount));
            }
            else
                sLog.outErrorDb("Table `%s` has `maxcount` (%u) for %s %u of %s %u but `incrtime`=0, ignoring",
                                tableName, maxcount, nameStr, item_id, idStr, vendor_entry);
            return false;
        }
        else if (maxcount == 0 && incrtime > 0)
        {
            if (problems)
            {
                problems->push_back(VendorItemProblem::Text("MaxCount==0 but IncrTime<>=0"));
            }
            else
                sLog.outErrorDb("Table `%s` has `maxcount`=0 for %s %u of %s %u but `incrtime`<>0, ignoring",
                                tableName, nameStr, item_id, idStr, vendor_entry);
            return false;
        }
    }
    else if (type == VENDOR_ITEM_TYPE_CURRENCY)
    {
        if (maxcount < uint32(currencyEntry->GetPrecision()))
        {
            if (problems)
            {
                problems->push_back(VendorItemProblem::Entry(LANG_VENDOR_WRONG_CURRENCY_MAXCOUNT, item_id, uint32(currencyEntry->GetPrecision())));
            }
            else
                sLog.outErrorDb("Table `%s` contains %s (Entry: %u) with too low maxcount. Maxcount for currencies is buycount, so it can't be 0 or less than that's currency precision (%u), ignoring",
                                        tableName, nameStr, item_id, uint32(currencyEntry->GetPrecision()));
            return false;
        }
    }

    if (conditionId && !sObjectMgr.GetCondition(conditionId))
    {
        sLog.outErrorDb("Table `%s` has `condition_id`=%u for item %u of %s %u but this condition is not valid, ignoring", tableName, conditionId, item_id, idStr, vendor_entry);
        return false;
    }

    VendorItemData const* vItems = isTemplate ? GetNpcVendorTemplateItemList(vendor_entry) : GetNpcVendorItemList(vendor_entry);
    VendorItemData const* tItems = isTemplate ? NULL : GetNpcVendorTemplateItemList(vendor_entry);

    if (!vItems && !tItems)
        return true;                                        // later checks for non-empty lists

    if (vItems && vItems->FindItemCostPair(item_id, type, ExtendedCost))
    {
        if (problems)
        {
            problems->push_back(VendorItemProblem::Entry(LANG_ITEM_ALREADY_IN_LIST, item_id, type == VENDOR_ITEM_TYPE_CURRENCY, ExtendedCost));
        }
        else
            sLog.outErrorDb("Table `%s` has duplicate %s %u (with extended cost %u) for %s %u, ignoring",
                            tableName, nameStr, item_id, ExtendedCost, idStr, vendor_entry);
        return false;
    }

    if (!isTemplate)
    {
        if (tItems && tItems->FindItemCostPair(item_id, type, ExtendedCost))
        {
            if (problems)
            {
                problems->push_back(VendorItemProblem::Entry(LANG_ITEM_ALREADY_IN_LIST, item_id, type == VENDOR_ITEM_TYPE_CURRENCY, ExtendedCost));
            }
            else
            {
                if (!cInfo->VendorTemplateId)
                    sLog.outErrorDb("Table `%s` has duplicate %s %u (with extended cost %u) for %s %u, ignoring",
                                    tableName, nameStr, item_id, ExtendedCost, idStr, vendor_entry);
                else
                    sLog.outErrorDb("Table `%s` has duplicate %s %u (with extended cost %u) for %s %u (or possible in vendor template %u), ignoring",
                                    tableName, nameStr, item_id, ExtendedCost, idStr, vendor_entry, cInfo->VendorTemplateId);
            }
            return false;
        }
    }

    uint32 countItems = vItems ? vItems->GetItemCount() : 0;
    countItems += tItems ? tItems->GetItemCount() : 0;

    if (countItems >= MAX_VENDOR_ITEMS)
    {
        if (problems)
        {
            problems->push_back(VendorItemProblem::Entry(LANG_COMMAND_ADDVENDORITEMITEMS));
        }
        else
            sLog.outErrorDb("Table `%s` has too many entries (%u >= %i) for %s %u, ignoring",
                            tableName, countItems, MAX_VENDOR_ITEMS, idStr, vendor_entry);
        return false;
    }

    return true;
}

// Functions for scripting access
bool LoadMangosStrings(DatabaseType& db, char const* table, int32 start_value, int32 end_value, bool extra_content)
{
    // MAX_DB_SCRIPT_STRING_ID is max allowed negative value for scripts (scrpts can use only more deep negative values
    // start/end reversed for negative values
    if (start_value > MAX_DB_SCRIPT_STRING_ID || end_value >= start_value)
    {
        sLog.outErrorDb("Table '%s' attempt loaded with reserved by mangos range (%d - %d), strings not loaded.", table, start_value, end_value + 1);
        return false;
    }

    return sObjectMgr.LoadMangosStrings(db, table, start_value, end_value, extra_content);
}


/**
 * @brief Retrieves a creature template from the global creature store.
 *
 * @param entry The creature template entry.
 * @return CreatureInfo const* The matching creature template, or null if missing.
 */
CreatureInfo const* GetCreatureTemplateStore(uint32 entry)
{
    return ObjectMgr::GetCreatureTemplate(entry);
}

/**
 * @brief Retrieves a quest template from the object manager.
 *
 * @param entry The quest template entry.
 * @return Quest const* The matching quest template, or null if missing.
 */
Quest const* GetQuestTemplateStore(uint32 entry)
{
    return sObjectMgr.GetQuestTemplate(entry);
}

/**
 * @brief Retrieves localized MaNGOS string data by entry id.
 *
 * @param entry The localized string entry.
 * @return MangosStringLocale const* The matching localized string data, or null if missing.
 */
MangosStringLocale const* GetMangosStringData(int32 entry)
{
    return sObjectMgr.GetMangosStringLocale(entry);
}

void ObjectMgr::LoadHotfixData()
{
    uint32 count = 0;
    m_hotfixData.clear();

    std::vector<HotfixRow> rows = ItemDataRepository::QueryHotfixes();
    if (rows.empty())
    {
        BarGoLink bar(1);

        bar.step();

        sLog.outString(">> Loaded %u hotfix info entries. DB table `hotfix_data` is empty.", count);
        return;
    }

    for (HotfixRow const& row : rows)
    {
        HotfixInfo info;
        info.Entry      = row.entry;
        info.Type       = row.type;
        info.Timestamp  = row.timestamp;
        m_hotfixData.push_back(info);
        ++count;
    }

    sLog.outString();
    sLog.outString(">> Loaded %u hotfix info entries.", count);
}
