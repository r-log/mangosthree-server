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

#ifndef MANGOS_H_OBJECTMGR
#define MANGOS_H_OBJECTMGR

#include <unordered_map>
#include <functional>
#include <utility>
#include "Utilities/Errors.h"
#include "Platform/Define.h"
#include "Common/Locales.h"
#include "Utilities/PackedValues.h"
#include <vector>
#include <set>
#include <list>
#include <ctime>
#include "Log.h"
#include "SharedDefines.h"
#include "DBCEnums.h"
#include "data/CreatureRecords.h"
#include "data/GameObjectRecords.h"
#include "QuestDef.h"
#include "ItemPrototype.h"
#include "data/GossipRecords.h"
#include "Database/DatabaseEnv.h"
#include "ObjectGuid.h"
#include "Policies/Singleton.h"
#include "data/AreaTriggerStore.h"
#include "data/CellObjectGuidStore.h"
#include "data/CellObjectGuids.h"
#include "data/CreatureAddonStore.h"
#include "data/CreatureEquipmentStore.h"
#include "data/CreatureModelInfoStore.h"
#include "data/CreatureModelRaceStore.h"
#include "data/CreatureSpawnStore.h"
#include "data/DungeonFinderStore.h"
#include "data/ExplorationBaseXpStore.h"
#include "data/FishingBaseSkillStore.h"
#include "data/GameObjectSpawnStore.h"
#include "data/GameObjectTemplateStore.h"
#include "data/GameTeleStore.h"
#include "data/GossipMenuStore.h"
#include "data/GraveyardStore.h"
#include "data/InstanceTemplateStore.h"
#include "data/ItemConvertStore.h"
#include "data/ItemRequiredTargetStore.h"
#include "data/KeyedRecordStore.h"
#include "data/LfgDungeonEntranceStore.h"
#include "data/MailLevelRewardStore.h"
#include "data/NpcTextStore.h"
#include "data/PlayerCreateInfo.h"
#include "data/PageTextStore.h"
#include "data/PetLevelInfoStore.h"
#include "data/PetNameStore.h"
#include "data/PlayerCreateInfoStore.h"
#include "data/PointOfInterestStore.h"
#include "data/QuestRelationStore.h"
#include "data/QuestStore.h"
#include "data/QuestPOIStore.h"
#include "data/ReputationRecords.h"
#include "data/ReservedNameStore.h"
#include "data/ServerSpellStore.h"
#include "data/SpellClickStore.h"
#include "data/TextPool.h"
#include "data/VehicleAccessoryStore.h"

#include <string>
#include <map>
#include <limits>

struct GameEventCreatureData;
class Group;
class ArenaTeam;
class Item;
class Player;
class Creature;
class GameObject;
class Unit;
class WorldObject;
class Map;
struct DeclinedName;
enum class TableReadStatus;
struct SpellTemplateRow;
struct ItemTemplateRow;

// mangos string ranges
#define MIN_MANGOS_STRING_ID           1                    // 'mangos_string'
#define MAX_MANGOS_STRING_ID           2000000000
#define MIN_DB_SCRIPT_STRING_ID        MAX_MANGOS_STRING_ID // 'db_script_string'
#define MAX_DB_SCRIPT_STRING_ID        2001000000
#define MIN_CREATURE_AI_TEXT_STRING_ID (-1)                 // 'creature_ai_texts'
#define MAX_CREATURE_AI_TEXT_STRING_ID (-1000000)
// Anything below MAX_CREATURE_AI_TEXT_STRING_ID is handled by the external script lib

static_assert(MAX_DB_SCRIPT_STRING_ID < INT32_MAX, "Must scope with int32 range");

/// Logs an error about a text entry in the log of the entry's range: the script library's, the event
/// AI's, the core's or the database scripts'.
void LogTextEntryError(int32 entry, char const* format, ...) ATTR_PRINTF(2, 3);

struct MangosStringLocale
{
    MangosStringLocale() : SoundId(0), Type(0), LanguageId(LANG_UNIVERSAL), Emote(0) { }

    std::vector<std::string> Content;                       // 0 -> default, i -> i-1 locale index
    uint32 SoundId;
    uint8  Type;
    Language LanguageId;
    uint32 Emote;                       // 0 -> default, i -> i-1 locale index
};

typedef std::unordered_map<uint32, CreatureLocale> CreatureLocaleMap;
typedef std::unordered_map<uint32, GameObjectLocale> GameObjectLocaleMap;
typedef std::unordered_map<uint32, ItemLocale> ItemLocaleMap;
typedef std::unordered_map<uint32, QuestLocale> QuestLocaleMap;
typedef std::unordered_map<uint32, NpcTextLocale> NpcTextLocaleMap;
typedef std::unordered_map<uint32, PageTextLocale> PageTextLocaleMap;
typedef std::unordered_map<int32, MangosStringLocale> MangosStringLocaleMap;
typedef std::unordered_map<uint32, GossipMenuItemsLocale> GossipMenuItemsLocaleMap;
typedef std::unordered_map<uint32, PointOfInterestLocale> PointOfInterestLocaleMap;

struct DungeonEncounter
{
    DungeonEncounter(DungeonEncounterEntry const* _dbcEntry, EncounterCreditType _creditType, uint32 _creditEntry, uint32 _lastEncounterDungeon)
        : dbcEntry(_dbcEntry), creditType(_creditType), creditEntry(_creditEntry), lastEncounterDungeon(_lastEncounterDungeon) { }
    DungeonEncounterEntry const* dbcEntry;
    EncounterCreditType creditType;
    uint32 creditEntry;
    uint32 lastEncounterDungeon;
};

typedef std::multimap<uint32, DungeonEncounter const*> DungeonEncounterMap;
typedef std::pair<DungeonEncounterMap::const_iterator, DungeonEncounterMap::const_iterator> DungeonEncounterMapBounds;

enum ConditionType
{
    //                                                      // value1       value2  for the Condition enumed
    CONDITION_NOT                   = -3,                   // cond-id-1    0          returns !cond-id-1
    CONDITION_OR                    = -2,                   // cond-id-1    cond-id-2  returns cond-id-1 OR cond-id-2
    CONDITION_AND                   = -1,                   // cond-id-1    cond-id-2  returns cond-id-1 AND cond-id-2
    CONDITION_NONE                  = 0,                    // 0            0
    CONDITION_AURA                  = 1,                    // spell_id     effindex
    CONDITION_ITEM                  = 2,                    // item_id      count   check present req. amount items in inventory
    CONDITION_ITEM_EQUIPPED         = 3,                    // item_id      0
    CONDITION_AREAID                = 4,                    // area_id      0, 1 (0: in (sub)area, 1: not in (sub)area)
    CONDITION_REPUTATION_RANK_MIN   = 5,                    // faction_id   min_rank
    CONDITION_TEAM                  = 6,                    // player_team  0,      (469 - Alliance 67 - Horde)
    CONDITION_SKILL                 = 7,                    // skill_id     skill_value
    CONDITION_QUESTREWARDED         = 8,                    // quest_id     0
    CONDITION_QUESTTAKEN            = 9,                    // quest_id     0,1,2   for condition true while quest active (0 any state, 1 if quest incomplete, 2 if quest completed).
    CONDITION_AD_COMMISSION_AURA    = 10,                   // 0            0,      for condition true while one from AD commission aura active
    CONDITION_NO_AURA               = 11,                   // spell_id     effindex
    CONDITION_ACTIVE_GAME_EVENT     = 12,                   // event_id     0
    CONDITION_AREA_FLAG             = 13,                   // area_flag    area_flag_not
    CONDITION_RACE_CLASS            = 14,                   // race_mask    class_mask
    CONDITION_LEVEL                 = 15,                   // player_level 0, 1 or 2 (0: equal to, 1: equal or higher than, 2: equal or less than)
    CONDITION_NOITEM                = 16,                   // item_id      count   check not present req. amount items in inventory
    CONDITION_SPELL                 = 17,                   // spell_id     0, 1 (0: has spell, 1: hasn't spell)
    CONDITION_INSTANCE_SCRIPT       = 18,                   // map_id       instance_condition_id (instance script specific enum)
    CONDITION_QUESTAVAILABLE        = 19,                   // quest_id     0       for case when loot/gossip possible only if player can start quest
    CONDITION_ACHIEVEMENT           = 20,                   // ach_id       0, 1 (0: has achievement, 1: hasn't achievement) for player
    CONDITION_ACHIEVEMENT_REALM     = 21,                   // ach_id       0, 1 (0: has achievement, 1: hasn't achievement) for server
    CONDITION_QUEST_NONE            = 22,                   // quest_id     0 (quest did not take and not rewarded)
    CONDITION_ITEM_WITH_BANK        = 23,                   // item_id      count   check present req. amount items in inventory or bank
    CONDITION_NOITEM_WITH_BANK      = 24,                   // item_id      count   check not present req. amount items in inventory or bank
    CONDITION_NOT_ACTIVE_GAME_EVENT = 25,                   // event_id     0
    CONDITION_ACTIVE_HOLIDAY        = 26,                   // holiday_id   0       preferred use instead CONDITION_ACTIVE_GAME_EVENT when possible
    CONDITION_NOT_ACTIVE_HOLIDAY    = 27,                   // holiday_id   0       preferred use instead CONDITION_NOT_ACTIVE_GAME_EVENT when possible
    CONDITION_LEARNABLE_ABILITY     = 28,                   // spell_id     0 or item_id
    // True when player can learn ability (using min skill value from SkillLineAbility).
    // Item_id can be defined in addition, to check if player has one (1) item in inventory or bank.
    // When player has spell or has item (when defined), condition return false.
    CONDITION_SKILL_BELOW           = 29,                   // skill_id     skill_value
    // True if player has skill skill_id and skill less than (and not equal) skill_value (for skill_value > 1)
    // If skill_value == 1, then true if player has not skill skill_id
    CONDITION_REPUTATION_RANK_MAX   = 30,                   // faction_id   max_rank
    CONDITION_COMPLETED_ENCOUNTER   = 31,                   // encounter_id encounter_id2       encounter_id[2] = DungeonEncounter(dbc).id (if value2 provided it will return value1 OR value2)
    CONDITION_SOURCE_AURA           = 32,                   // spell_id     effindex (returns true if the source of the condition check has aura of spell_id, effIndex)
    CONDITION_LAST_WAYPOINT         = 33,                   // waypointId   0 = exact, 1: wp <= waypointId, 2: wp > waypointId  Use to check what waypoint was last reached
    CONDITION_XP_USER               = 34,                   // 0, 1 (0: XP off, 1: XP on) for player    0
    CONDITION_GENDER                = 35,                   // 0=male, 1=female, 2=none (see enum Gender)
    CONDITION_DEAD_OR_AWAY          = 36,                   // value1: 0=player dead, 1=player is dead (with group dead), 2=player in instance are dead, 3=creature is dead
                                                            // value2: if != 0 only consider players in range of this value
    CONDITION_CREATURE_IN_RANGE     = 37,                   // value1: creature entry; value2: range; returns only alive creatures
    CONDITION_GAMEOBJECT_IN_RANGE   = 38,                   // value1: gameobject entry; value2: range
};

enum ConditionSource                                        // From where was the condition called?
{
    CONDITION_FROM_LOOT             = 0,                    // Used to check a *_loot_template entry
    CONDITION_FROM_REFERING_LOOT    = 1,                    // Used to check a entry refering to a reference_loot_template entry
    CONDITION_FROM_GOSSIP_MENU      = 2,                    // Used to check a gossip menu menu-text
    CONDITION_FROM_GOSSIP_OPTION    = 3,                    // Used to check a gossip menu option-item
    CONDITION_FROM_EVENTAI          = 4,                    // Used to check EventAI Event "On Receive Emote"
    CONDITION_FROM_HARDCODED        = 5,                    // Used to check a hardcoded event - not actually a condition
    CONDITION_FROM_VENDOR           = 6,                    // Used to check a condition from a vendor
    CONDITION_FROM_SPELL_AREA       = 7,                    // Used to check a condition from spell_area table
    CONDITION_FROM_SPELLCLICK       = 8,                    // Used to check a condition from npc_spellclick_spells table
    CONDITION_FROM_DBSCRIPTS        = 9,                    // Used to check a condition from DB Scripts Engine
};

class PlayerCondition
{
    public:
        // Default constructor, required for SQL Storage (Will give errors if used elsewise)
        PlayerCondition() : m_entry(0), m_condition(CONDITION_AND), m_value1(0), m_value2(0) {}

        PlayerCondition(uint16 _entry, int16 _condition, uint32 _value1, uint32 _value2)
            : m_entry(_entry), m_condition(ConditionType(_condition)), m_value1(_value1), m_value2(_value2) {}

        // Checks correctness of values
        bool IsValid() const
        {
            return IsValid(m_entry, m_condition, m_value1, m_value2);
        }
        static bool IsValid(uint16 entry, ConditionType condition, uint32 value1, uint32 value2);

        static bool CanBeUsedWithoutPlayer(uint16 entry);

        /// The condition's entry in the conditions table.
        uint16 GetEntry() const { return m_entry; }

    private:
        uint16 m_entry;                                     // entry of the condition

    public:
        ConditionType m_condition;                          // additional condition type
        uint32 m_value1;                                    // data for the condition - see ConditionType definition
        uint32 m_value2;
};

typedef std::unordered_map<uint32, VendorItemData> CacheVendorItemMap;
typedef std::unordered_map<uint32, TrainerSpellData> CacheTrainerSpellMap;

enum SkillRangeType
{
    SKILL_RANGE_LANGUAGE,                                   // 300..300
    SKILL_RANGE_LEVEL,                                      // 1..max skill for level
    SKILL_RANGE_MONO,                                       // 1..1, grey monolite bar
    SKILL_RANGE_RANK,                                       // 1..skill for known rank
    SKILL_RANGE_NONE,                                       // 0..0 always
};

/**
 * Determines the skill progression range type for the specified skill line.
 */
SkillRangeType GetSkillRangeType(SkillLineEntry const* pSkill, bool racial);

struct HotfixInfo
{
    uint32 Type;
    uint32 Timestamp;
    uint32 Entry;
};

typedef std::vector<HotfixInfo> HotfixData;

#define MAX_PLAYER_NAME          12                         // max allowed by client name length
#define MAX_INTERNAL_PLAYER_NAME 15                         // max server internal player name length ( > MAX_PLAYER_NAME for support declined names )
#define MAX_PET_NAME             12                         // max allowed by client name length
#define MAX_CHARTER_NAME         24                         // max allowed by client name length

/**
 * Normalizes a player name to the server's canonical capitalization and format rules.
 */
bool normalizePlayerName(std::string& name);

struct  LanguageDesc
{
    Language lang_id;
    uint32   spell_id;
    uint32   skill_id;
};

extern LanguageDesc lang_description[LANGUAGES_COUNT];
 LanguageDesc const* GetLanguageDescByID(uint32 lang);

class PlayerDumpReader;

template<typename T>
class IdGenerator
{
    public:                                                 // constructors
        explicit IdGenerator(char const* _name) : m_name(_name), m_nextGuid(1) {}

    public:                                                 // modifiers
        void Set(T val)
        {
            m_nextGuid = val;
        }
        T Generate();

    public:                                                 // accessors
        T GetNextAfterMaxUsed() const
        {
            return m_nextGuid;
        }

    private:                                                // fields
        char const* m_name;
        T m_nextGuid;
};

/**
 * A problem the vendor item check finds, for a GM's command to report: a `mangos_string` entry or a
 * text, and the values it names (valueCount of them).
 */
struct VendorItemProblem
{
    int32 entry;
    char const* text;
    uint32 valueCount;
    uint32 values[3];

    static VendorItemProblem Entry(int32 entry)
    {
        return Make(entry, NULL, 0, 0, 0, 0);
    }

    static VendorItemProblem Entry(int32 entry, uint32 a)
    {
        return Make(entry, NULL, 1, a, 0, 0);
    }

    static VendorItemProblem Entry(int32 entry, uint32 a, uint32 b)
    {
        return Make(entry, NULL, 2, a, b, 0);
    }

    static VendorItemProblem Entry(int32 entry, uint32 a, uint32 b, uint32 c)
    {
        return Make(entry, NULL, 3, a, b, c);
    }

    static VendorItemProblem Text(char const* text)
    {
        return Make(0, text, 0, 0, 0, 0);
    }

    static VendorItemProblem Text(char const* text, uint32 a)
    {
        return Make(0, text, 1, a, 0, 0);
    }

    private:
        static VendorItemProblem Make(int32 entry, char const* text, uint32 count, uint32 a, uint32 b, uint32 c)
        {
            VendorItemProblem problem;
            problem.entry = entry;
            problem.text = text;
            problem.valueCount = count;
            problem.values[0] = a;
            problem.values[1] = b;
            problem.values[2] = c;
            return problem;
        }
};

class ObjectMgr
{
        friend class PlayerDumpReader;

    public:
        ObjectMgr();
        ~ObjectMgr();

        typedef std::unordered_map<uint32, Item*> ItemMap;

        typedef QuestStore::QuestMap QuestMap;

        typedef AreaTriggerStore::AreaTriggerMap AreaTriggerMap;

        typedef std::unordered_map<uint32, RepRewardRate > RepRewardRateMap;
        typedef std::unordered_map<uint32, ReputationOnKillEntry> RepOnKillMap;
        typedef std::unordered_map<uint32, RepSpilloverTemplate> RepSpilloverTemplateMap;

        typedef CellObjectGuidStore::ActiveCreatureGuidsOnMap ActiveCreatureGuidsOnMap;

        void LoadGameobjectInfo();
        void AddGameobjectInfo(GameObjectInfo* goinfo);

        CreatureModelInfo const* GetCreatureModelRandomGender(uint32 display_id) const;
        uint32 GetCreatureModelAlternativeModel(uint32 modelId) const;

        PetLevelInfo const* GetPetLevelInfo(uint32 creature_id, uint32 level) const;

        void GetPlayerClassLevelInfo(uint32 class_, uint32 level, uint32& baseHP, uint32& baseMana) const;

        PlayerInfo const* GetPlayerInfo(uint32 race, uint32 class_) const
        {
            if (race   >= MAX_RACES)
            {
                return NULL;
            }
            if (class_ >= MAX_CLASSES)
            {
                return NULL;
            }
            PlayerInfo const* info = &m_playerInfos.Get(race, class_);
            if (info->displayId_m == 0 || info->displayId_f == 0)
            {
                return NULL;
            }
            return info;
        }
        void GetPlayerLevelInfo(uint32 race, uint32 class_, uint32 level, PlayerLevelInfo* info) const;

        ObjectGuid GetPlayerGuidByName(std::string name) const;
        bool GetPlayerNameByGUID(ObjectGuid guid, std::string& name) const;
        Team GetPlayerTeamByGUID(ObjectGuid guid) const;
        uint32 GetPlayerAccountIdByGUID(ObjectGuid guid) const;
        uint32 GetPlayerAccountIdByPlayerName(const std::string& name) const;

        uint32 GetNearestTaxiNode(float x, float y, float z, uint32 mapid, Team team);
        void GetTaxiPath(uint32 source, uint32 destination, uint32& path, uint32& cost);
        uint32 GetTaxiMountDisplayId(uint32 id, Team team, bool allowed_alt_team = false);

        Quest const* GetQuestTemplate(uint32 quest_id) const
        {
            return m_quests.Find(quest_id);
        }
        QuestMap const& GetQuestTemplates() const
        {
            return m_quests.All();
        }

        uint32 GetQuestForAreaTrigger(uint32 Trigger_ID) const
        {
            return m_areaTriggers.QuestFor(Trigger_ID);
        }
        bool IsTavernAreaTrigger(uint32 Trigger_ID) const
        {
            return m_areaTriggers.IsTavern(Trigger_ID);
        }

        bool IsGameObjectForQuests(uint32 entry) const
        {
            return m_questRelations.IsGameObjectForQuests(entry);
        }

        GossipText const* GetGossipText(uint32 Text_ID) const;

        WorldSafeLocsEntry const* GetClosestGraveYard(float x, float y, float z, uint32 MapId, Team team);
        bool AddGraveYardLink(uint32 id, uint32 zone, Team team, bool inDB = true);
        void SetGraveYardLinkTeam(uint32 id, uint32 zoneId, Team team);
        void LoadGraveyardZones();
        GraveYardData const* FindGraveYardData(uint32 id, uint32 zone) const;

        AreaTrigger const* GetAreaTrigger(uint32 trigger) const
        {
            return m_areaTriggers.FindTeleport(trigger);
        }

        AreaTrigger const* GetGoBackTrigger(uint32 Map) const;
        AreaTrigger const* GetMapEntranceTrigger(uint32 Map) const;

        RepRewardRate const* GetRepRewardRate(uint32 factionId) const
        {
            RepRewardRateMap::const_iterator itr = m_RepRewardRateMap.find(factionId);
            if (itr != m_RepRewardRateMap.end())
            {
                return &itr->second;
            }

            return NULL;
        }

        ReputationOnKillEntry const* GetReputationOnKillEntry(uint32 id) const
        {
            RepOnKillMap::const_iterator itr = mRepOnKill.find(id);
            if (itr != mRepOnKill.end())
            {
                return &itr->second;
            }
            return NULL;
        }

        RepSpilloverTemplate const* GetRepSpilloverTemplate(uint32 factionId) const
        {
            RepSpilloverTemplateMap::const_iterator itr = m_RepSpilloverTemplateMap.find(factionId);
            if (itr != m_RepSpilloverTemplateMap.end())
            {
                return &itr->second;
            }

            return NULL;
        }

        /// The condition `id`, or NULL when there is none or the load erased it.
        PlayerCondition const* GetCondition(uint32 id) const
        {
            return m_conditions.Find(id);
        }

        PointOfInterest const* GetPointOfInterest(uint32 id) const
        {
            return m_pointsOfInterest.Find(id);
        }

        PageText const* GetPageText(uint32 id) const
        {
            return m_pageTexts.Find(id);
        }

        /// The passengers boarded onto `vehicleEntry`, in the order they were loaded.
        VehicleAccessoryStore::Bounds GetVehicleAccessories(uint32 vehicleEntry) const
        {
            return m_vehicleAccessories.GetBounds(vehicleEntry);
        }

        QuestPOIVector const* GetQuestPOIVector(uint32 questId) const
        {
            return m_questPoi.Find(questId);
        }

        DungeonFinderRequirements const* GetDungeonFinderRequirements(uint32 mapId, uint32 difficulty) const
        {
            return m_dungeonFinder.FindRequirements(mapId, difficulty);
        }

        DungeonFinderRewards const* GetDungeonFinderRewards(uint32 level) const
        {
            return m_dungeonFinder.FindRewards(level);
        }

        DungeonFinderItemsMap const& GetDungeonFinderItemsMap() const { return m_dungeonFinder.Items(); }

        typedef ::LfgDungeonEntrance LfgDungeonEntrance;

        /// Entrance override for one LFGDungeons.dbc id; NULL = fall back to
        /// GetMapEntranceTrigger (multi-wing maps only carry overrides).
        /// The item prototypes.
        KeyedRecordStore<ItemPrototype> const& GetItemPrototypes() const
        {
            return m_itemPrototypes;
        }

        /// The creature templates.
        KeyedRecordStore<CreatureInfo> const& GetCreatureTemplates() const
        {
            return m_creatureTemplates;
        }

        /// The game object templates, walked in ascending entry order.
        GameObjectTemplateStore const& GetGameObjectTemplates() const
        {
            return m_gameObjectTemplates;
        }

        LfgDungeonEntrance const* GetLfgDungeonEntrance(uint32 dungeonId) const
        {
            return m_lfgDungeonEntrances.Find(dungeonId);
        }

        // Static wrappers for various accessors
        static GameObjectInfo const* GetGameObjectInfo(uint32 id);                  ///< Lookup in the game object template store
        static GameObjectDataAddon const* GetGameObjectAddon(uint32 lowguid);       ///< Lookup in the game object addon store
        static CreatureInfo const* GetCreatureTemplate(uint32 id);                  ///< Lookup in the creature template store
        static CreatureModelInfo const* GetCreatureModelInfo(uint32 modelid);       ///< Lookup in the creature model info store
        static CreatureTemplateSpells const* GetCreatureTemplateSpells(uint32 entry); ///< Lookup in the creature template spells store
        static EquipmentInfo const* GetEquipmentInfo(uint32 entry);                 ///< Lookup in the creature equipment store
        static CreatureDataAddon const* GetCreatureAddon(uint32 lowguid);           ///< Lookup in the creature addon store
        static CreatureDataAddon const* GetCreatureTemplateAddon(uint32 entry);     ///< Lookup in the creature template addon store
        static ItemPrototype const* GetItemPrototype(uint32 id);                    ///< Lookup in the item prototype store
        static InstanceTemplate const* GetInstanceTemplate(uint32 map);             ///< Lookup in the instance template store

        // Loading functions
        void LoadQuests();
        void LoadQuestRelations()
        {
            LoadGameobjectQuestRelations();
            LoadGameobjectInvolvedRelations();
            LoadCreatureQuestRelations();
            LoadCreatureInvolvedRelations();
        }
        void LoadGameobjectQuestRelations();
        void LoadGameobjectInvolvedRelations();
        void LoadCreatureQuestRelations();
        void LoadCreatureInvolvedRelations();

        bool LoadMangosStrings(DatabaseType& db, char const* table, int32 min_value, int32 max_value, bool extra_content);
        bool LoadMangosStrings()
        {
            return LoadMangosStrings(WorldDatabase, "mangos_string", MIN_MANGOS_STRING_ID, MAX_MANGOS_STRING_ID, false);
        }
        void LoadCreatureLocales();
        void LoadCreatureTemplates();
        void LoadCreatures();
        void LoadCreatureAddons();
        void LoadCreatureClassLvlStats();
        void LoadCreatureModelInfo();
        void LoadCreatureModelRace();
        void LoadEquipmentTemplates();
        void LoadGameObjectLocales();
        void LoadGameObjects();
        void LoadGameObjectAddon();
        void LoadItemPrototypes();
        /// Fills the item prototypes from `item_template` rows without the load's checks; the tests' item fixture fills them so.
        void FillItemPrototypes(std::vector<ItemTemplateRow> const& rows);
        void LoadItemConverts();
        void LoadItemExpireConverts();
        void LoadItemRequiredTarget();
        void LoadItemLocales();
        void LoadQuestLocales();
        void LoadGossipTextLocales();
        void LoadPageTextLocales();
        void LoadGossipMenuItemsLocales();
        void LoadPointOfInterestLocales();
        void LoadInstanceEncounters();
        void LoadInstanceTemplate();
        void LoadConditions();
        /// Reloads the conditions; when the table cannot be read, keeps the conditions already loaded. Answers how the read ended.
        TableReadStatus ReloadConditions();
        void LoadMailLevelRewards();

        void LoadGossipText();

        void LoadAreaTriggerTeleports();
        void LoadQuestAreaTriggers();
        void LoadTavernAreaTriggers();
        void LoadGameObjectForQuests();

        void LoadPageTexts();
        /// Reloads the page texts; when the table cannot be read, keeps the pages already loaded. Answers how the read ended.
        TableReadStatus ReloadPageTexts();

        void LoadPlayerInfo();
        void LoadPetLevelInfo();
        void LoadExplorationBaseXP();
        void LoadPetNames();
        void LoadPetNumber();
        void LoadFishingBaseSkillLevel();

        void LoadReputationRewardRate();
        void LoadReputationOnKill();
        void LoadReputationSpilloverTemplate();

        void LoadPointsOfInterest();
        void LoadQuestPOI();

        void LoadDungeonFinderRequirements();
        void LoadDungeonFinderRewards();
        void LoadDungeonFinderItems();
        void LoadLfgDungeonEntrances();

        void LoadNPCSpellClickSpells();
        void LoadSpellTemplate();
        /// Fills a server-side spell's zeroed records from its `spell_template` row; its effect is loaded
        /// unless the row has none or its aura needs base points, which the table does not hold.
        static void FillServerSpell(SpellTemplateRow const& row, ServerSpell& spell);
        void LoadCreatureTemplateSpells();

        void LoadGameTele();

        void LoadGossipMenus();

        void LoadVendorTemplates();
        void LoadVendors()
        {
            LoadVendors("npc_vendor", false);
        }
        void LoadTrainerTemplates();
        void LoadTrainers()
        {
            LoadTrainers("npc_trainer", false);
        }

        void LoadVehicleAccessory();

        std::string GeneratePetName(uint32 entry);
        uint32 GetBaseXP(uint32 level) const
        {
            return m_explorationBaseXp.Get(level);
        }
        uint32 GetXPForLevel(uint32 level) const;
        uint32 GetXPForPetLevel(uint32 level) const
        {
            return GetXPForLevel(level) / 20;
        }

        int32 GetFishingBaseSkillLevel(uint32 entry) const
        {
            return m_fishingBaseSkill.Get(entry);
        }


        void SetHighestGuids();

        // used for set initial guid counter for map local guids
        uint32 GetFirstTemporaryCreatureLowGuid() const
        {
            return m_FirstTemporaryCreatureGuid;
        }
        uint32 GetFirstTemporaryGameObjectLowGuid() const
        {
            return m_FirstTemporaryGameObjectGuid;
        }

        // used in .npc add/.gobject add commands for adding static spawns
        uint32 GenerateStaticCreatureLowGuid()
        {
            if (m_StaticCreatureGuids.GetNextAfterMaxUsed() >= m_FirstTemporaryCreatureGuid)
            {
                return 0;
            }
            return m_StaticCreatureGuids.Generate();
        }
        uint32 GenerateStaticGameObjectLowGuid()
        {
            if (m_StaticGameObjectGuids.GetNextAfterMaxUsed() >= m_FirstTemporaryGameObjectGuid)
            {
                return 0;
            }
            return m_StaticGameObjectGuids.Generate();
        }

        uint32 GeneratePlayerLowGuid()
        {
            return m_CharGuids.Generate();
        }
        uint32 GenerateItemLowGuid()
        {
            return m_ItemGuids.Generate();
        }
        uint32 GenerateCorpseLowGuid()
        {
            return m_CorpseGuids.Generate();
        }
        uint32 GenerateInstanceLowGuid() { return m_InstanceGuids.Generate(); }
        uint32 GenerateGroupLowGuid()    { return m_GroupGuids.Generate();    }

        uint32 GenerateArenaTeamId() { return m_ArenaTeamIds.Generate(); }
        uint32 GenerateAuctionID()
        {
            return m_AuctionIds.Generate();
        }
        uint64 GenerateEquipmentSetGuid() { return m_EquipmentSetIds.Generate(); }
        uint32 GenerateGuildId()
        {
            return m_GuildIds.Generate();
        }
        // uint32 GenerateItemTextID() { return m_ItemGuids.Generate(); }
        uint32 GenerateMailID()
        {
            return m_MailIds.Generate();
        }
        uint32 GeneratePetNumber()
        {
            return m_PetNumbers.Generate();
        }

        MailLevelReward const* GetMailLevelReward(uint32 level, uint32 raceMask)
        {
            return m_mailLevelRewards.Find(level, raceMask);
        }

        CreatureDataPair const* GetCreatureDataPair(uint32 guid) const
        {
            return m_creatureSpawns.Find(guid);
        }

        CreatureData const* GetCreatureData(uint32 guid) const
        {
            CreatureDataPair const* dataPair = GetCreatureDataPair(guid);
            return dataPair ? &dataPair->second : NULL;
        }

        CreatureData& NewOrExistCreatureData(uint32 guid)
        {
            return m_creatureSpawns.FindOrAdd(guid);
        }
        void DeleteCreatureData(uint32 guid);

        template<typename Worker>
        void DoCreatureData(Worker& worker) const
        {
            for (CreatureDataMap::const_iterator itr = m_creatureSpawns.All().begin(); itr != m_creatureSpawns.All().end(); ++itr)
                if (worker(*itr))
                {
                    break;
                }
        }

        ActiveCreatureGuidsOnMap const* GetActiveCreatureGuids() const
        {
            return &m_cellObjectGuids.ActiveCreatures();
        }

        CreatureLocale const* GetCreatureLocale(uint32 entry) const
        {
            CreatureLocaleMap::const_iterator itr = mCreatureLocaleMap.find(entry);
            if (itr == mCreatureLocaleMap.end())
            {
                return NULL;
            }
            return &itr->second;
        }

        void GetCreatureLocaleStrings(uint32 entry, int32 loc_idx, char const** namePtr, char const** subnamePtr = NULL) const;

        GameObjectLocale const* GetGameObjectLocale(uint32 entry) const
        {
            GameObjectLocaleMap::const_iterator itr = mGameObjectLocaleMap.find(entry);
            if (itr == mGameObjectLocaleMap.end())
            {
                return NULL;
            }
            return &itr->second;
        }

        ItemLocale const* GetItemLocale(uint32 entry) const
        {
            ItemLocaleMap::const_iterator itr = mItemLocaleMap.find(entry);
            if (itr == mItemLocaleMap.end())
            {
                return NULL;
            }
            return &itr->second;
        }

        void GetItemLocaleStrings(uint32 entry, int32 loc_idx, std::string* namePtr, std::string* descriptionPtr = NULL) const;

        QuestLocale const* GetQuestLocale(uint32 entry) const
        {
            QuestLocaleMap::const_iterator itr = mQuestLocaleMap.find(entry);
            if (itr == mQuestLocaleMap.end())
            {
                return NULL;
            }
            return &itr->second;
        }

        void GetQuestLocaleStrings(uint32 entry, int32 loc_idx, std::string* titlePtr) const;

        NpcTextLocale const* GetNpcTextLocale(uint32 entry) const
        {
            NpcTextLocaleMap::const_iterator itr = mNpcTextLocaleMap.find(entry);
            if (itr == mNpcTextLocaleMap.end())
            {
                return NULL;
            }
            return &itr->second;
        }

        typedef std::string NpcTextArray[MAX_GOSSIP_TEXT_OPTIONS];
        void GetNpcTextLocaleStringsAll(uint32 entry, int32 loc_idx, NpcTextArray *text0_Ptr, NpcTextArray* text1_Ptr) const;
        void GetNpcTextLocaleStrings0(uint32 entry, int32 loc_idx, std::string* text0_0_Ptr, std::string* text1_0_Ptr) const;

        PageTextLocale const* GetPageTextLocale(uint32 entry) const
        {
            PageTextLocaleMap::const_iterator itr = mPageTextLocaleMap.find(entry);
            if (itr == mPageTextLocaleMap.end())
            {
                return NULL;
            }
            return &itr->second;
        }

        GossipMenuItemsLocale const* GetGossipMenuItemsLocale(uint32 entry) const
        {
            GossipMenuItemsLocaleMap::const_iterator itr = mGossipMenuItemsLocaleMap.find(entry);
            if (itr == mGossipMenuItemsLocaleMap.end())
            {
                return NULL;
            }
            return &itr->second;
        }

        PointOfInterestLocale const* GetPointOfInterestLocale(uint32 poi_id) const
        {
            PointOfInterestLocaleMap::const_iterator itr = mPointOfInterestLocaleMap.find(poi_id);
            if (itr == mPointOfInterestLocaleMap.end())
            {
                return NULL;
            }
            return &itr->second;
        }

        GameObjectDataPair const* GetGODataPair(uint32 guid) const
        {
            return m_gameObjectSpawns.Find(guid);
        }

        GameObjectData const* GetGOData(uint32 guid) const
        {
            GameObjectDataPair const* dataPair = GetGODataPair(guid);
            return dataPair ? &dataPair->second : NULL;
        }

        GameObjectData& NewGOData(uint32 guid)
        {
            return m_gameObjectSpawns.FindOrAdd(guid);
        }
        void DeleteGOData(uint32 guid);

        template<typename Worker>
        void DoGOData(Worker& worker) const
        {
            for (GameObjectDataMap::const_iterator itr = m_gameObjectSpawns.All().begin(); itr != m_gameObjectSpawns.All().end(); ++itr)
                if (worker(*itr))                           // arg = GameObjectDataPair
                {
                    break;
                }
        }

        MangosStringLocale const* GetMangosStringLocale(int32 entry) const
        {
            MangosStringLocaleMap::const_iterator itr = mMangosStringLocaleMap.find(entry);
            if (itr == mMangosStringLocaleMap.end())
            {
                return NULL;
            }
            return &itr->second;
        }
        uint32 GetLoadedStringsCount(int32 minEntry) const
        {
            std::map<int32, uint32>::const_iterator itr = m_loadedStringCount.find(minEntry);
            if (itr != m_loadedStringCount.end())
            {
                return itr->second;
            }
            return 0;
        }

        const char* GetMangosString(int32 entry, int locale_idx) const;
        const char* GetMangosStringForDBCLocale(int32 entry) const { return GetMangosString(entry, DBCLocaleIndex); }
        int32 GetDBCLocaleIndex() const { return DBCLocaleIndex; }
        void SetDBCLocaleIndex(uint32 lang) { DBCLocaleIndex = GetIndexForLocale(LocaleConstant(lang)); }

        // global grid objects state (static DB spawns, global spawn mods from gameevent system)
        CellObjectGuids const& GetCellObjectGuids(uint32 mapid, uint8 spawnMode, uint32 cell_id)
        {
            return m_cellObjectGuids.FindOrAddCell(mapid, spawnMode, cell_id);
        }

        // The spawns filed under one cell of a map and spawn mode, or NULL when the cell has none;
        // unlike GetCellObjectGuids() it never adds an empty cell, so a scan of many cells changes nothing.
        CellObjectGuids const* GetCellObjectGuidsReadOnly(uint32 mapid, uint8 spawnMode, uint32 cell_id) const
        {
            return m_cellObjectGuids.FindCell(mapid, spawnMode, cell_id);
        }

        // modifiers for global grid objects state (static DB spawns, global spawn mods from gameevent system)
        // Don't must be used for modify instance specific spawn state modifications
        void AddCreatureToGrid(uint32 guid, CreatureData const* data);
        void RemoveCreatureFromGrid(uint32 guid, CreatureData const* data);
        void AddGameobjectToGrid(uint32 guid, GameObjectData const* data);
        void RemoveGameobjectFromGrid(uint32 guid, GameObjectData const* data);
        void AddCorpseCellData(uint32 mapid, uint32 cellid, uint32 player_guid, uint32 instance);
        void DeleteCorpseCellData(uint32 mapid, uint32 cellid, uint32 player_guid);

        // reserved names
        void LoadReservedPlayersNames();
        bool IsReservedName(const std::string& name) const;

        // name with valid structure and symbols
        static uint8 CheckPlayerName(const std::string& name, bool create = false);
        static PetNameInvalidReason CheckPetName(const std::string& name);
        static bool IsValidCharterName(const std::string& name);

        static bool CheckDeclinedNames(const std::wstring& mainpart, DeclinedName const& names);

        int GetIndexForLocale(LocaleConstant loc);
        LocaleConstant GetLocaleForIndex(int i);

        // Check if a player meets condition conditionId

        void GetConditions(uint32 conditionId, std::vector<PlayerCondition const*>& out) const;

        GameTele const* GetGameTele(uint32 id) const
        {
            return m_gameTeles.Find(id);
        }

        GameTele const* GetGameTele(const std::string& name) const;
        GameTeleMap const& GetGameTeleMap() const
        {
            return m_gameTeles.All();
        }
        bool AddGameTele(GameTele& data);
        bool DeleteGameTele(const std::string& name);

        TrainerSpellData const* GetNpcTrainerSpells(uint32 entry) const
        {
            CacheTrainerSpellMap::const_iterator iter = m_mCacheTrainerSpellMap.find(entry);
            if (iter == m_mCacheTrainerSpellMap.end())
            {
                return NULL;
            }

            return &iter->second;
        }

        TrainerSpellData const* GetNpcTrainerTemplateSpells(uint32 entry) const
        {
            CacheTrainerSpellMap::const_iterator iter = m_mCacheTrainerTemplateSpellMap.find(entry);
            if (iter == m_mCacheTrainerTemplateSpellMap.end())
            {
                return NULL;
            }

            return &iter->second;
        }

        VendorItemData const* GetNpcVendorItemList(uint32 entry) const
        {
            CacheVendorItemMap::const_iterator  iter = m_mCacheVendorItemMap.find(entry);
            if (iter == m_mCacheVendorItemMap.end())
            {
                return NULL;
            }

            return &iter->second;
        }

        VendorItemData const* GetNpcVendorTemplateItemList(uint32 entry) const
        {
            CacheVendorItemMap::const_iterator  iter = m_mCacheVendorTemplateItemMap.find(entry);
            if (iter == m_mCacheVendorTemplateItemMap.end())
            {
                return NULL;
            }

            return &iter->second;
        }

        void AddVendorItem(uint32 entry, uint32 item, uint32 maxcount, uint32 incrtime, uint32 extendedcost);
        void AddVendorItem(uint32 entry, uint32 item, uint8 type, uint32 maxcount, uint32 incrtime, uint32 ExtendedCost);
        bool RemoveVendorItem(uint32 entry, uint32 item, uint8 type);
        bool IsVendorItemValid(bool isTemplate, char const* tableName, uint32 vendor_entry, uint32 item, uint8 type, uint32 maxcount, uint32 ptime, uint32 ExtendedCost, uint16 conditionId, std::vector<VendorItemProblem>* problems = NULL, std::set<uint32>* skip_vendors = NULL) const;
        bool IsVendorItemValid(bool isTemplate, char const* tableName, uint32 vendor_entry, uint32 item, uint32 maxcount, uint32 ptime, uint32 ExtendedCost, uint16 conditionId, std::vector<VendorItemProblem>* problems = NULL, std::set<uint32>* skip_vendors = NULL) const;

        static inline void GetLocaleString(const StringVector& data, int loc_idx, std::string& value)
        {
            if (data.size() > size_t(loc_idx) && !data[loc_idx].empty())
            {
                value = data[loc_idx];
            }
        }

        int GetOrNewIndexForLocale(LocaleConstant loc);

        SpellClickInfoMapBounds GetSpellClickInfoMapBounds(uint32 creature_id) const
        {
            return m_spellClicks.ForCreature(creature_id);
        }

        uint32 GetItemConvert(uint32 itemEntry, uint32 raceMask) const
        {
            uint32 targetEntry = m_itemConverts.RaceConvert(itemEntry);
            if (!targetEntry)
            {
                return 0;
            }

            ItemPrototype const* proto = GetItemPrototype(targetEntry);
            return (proto && proto->AllowableRace & raceMask) ? targetEntry : 0;
        }

        uint32 GetItemExpireConvert(uint32 itemEntry) const
        {
            return m_itemConverts.ExpireConvert(itemEntry);
        }

        ItemRequiredTargetMapBounds GetItemRequiredTargetMapBounds(uint32 uiItemEntry) const
        {
            return m_itemRequiredTargets.ForItem(uiItemEntry);
        }

        DungeonEncounterMapBounds GetDungeonEncounterBounds(uint32 creditEntry) const
        {
            return m_DungeonEncounters.equal_range(creditEntry);
        }

        GossipMenusMapBounds GetGossipMenusMapBounds(uint32 uiMenuId) const
        {
            return m_gossipMenus.Menus(uiMenuId);
        }

        GossipMenuItemsMapBounds GetGossipMenuItemsMapBounds(uint32 uiMenuId) const
        {
            return m_gossipMenus.MenuItems(uiMenuId);
        }

        ExclusiveQuestGroupsMapBounds GetExclusiveQuestGroupsMapBounds(int32 groupId) const
        {
            return m_quests.ExclusiveGroup(groupId);
        }

        QuestRelationsMapBounds GetCreatureQuestRelationsMapBounds(uint32 entry) const
        {
            return m_questRelations.CreatureStarts(entry);
        }

        QuestRelationsMapBounds GetCreatureQuestInvolvedRelationsMapBounds(uint32 entry) const
        {
            return m_questRelations.CreatureEnds(entry);
        }

        QuestRelationsMapBounds GetGOQuestRelationsMapBounds(uint32 entry) const
        {
            return m_questRelations.GameObjectStarts(entry);
        }

        QuestRelationsMapBounds GetGOQuestInvolvedRelationsMapBounds(uint32 entry) const
        {
            return m_questRelations.GameObjectEnds(entry);
        }

        QuestRelationsMap& GetCreatureQuestRelationsMap()
        {
            return m_questRelations.CreatureStartsForEdit();
        }
        uint32 GetModelForRace(uint32 sourceModelId, uint32 racemask);
        /**
        * \brief: Data returned is used to compute health, mana, armor, damage of creatures. May be NULL.
        * \param uint32 level               creature level
        * \param uint32 unitClass           creature class, related to CLASSMASK_ALL_CREATURES
        * \param uint32 expansion           creature expansion (we could have creature exp = 0 for vanilla, exp = 1 for tbc, exp = 2 for wotlk)
        * \return: CreatureClassLvlStats const* or NULL
        *
        * Description: GetCreatureClassLvlStats give fast access to creature stats data.
        * FullName: ObjectMgr::GetCreatureClassLvlStats
        * Access: public
        * Qualifier: const
        **/
        CreatureClassLvlStats const* GetCreatureClassLvlStats(uint32 level, uint32 unitClass, int32 expansion) const;

        void LoadHotfixData();
        HotfixData const& GetHotfixData() const { return m_hotfixData; }
        uint32 GetHotfixDate(uint32 entry, uint32 type) const
        {
            uint32 ret = 0;
            for (HotfixData::const_iterator itr = m_hotfixData.begin(); itr != m_hotfixData.end(); ++itr)
                if (itr->Entry == entry && itr->Type == type)
                    if (itr->Timestamp > ret)
                    {
                        ret = itr->Timestamp;
                    }

            return ret ? ret : uint32(time(NULL));
        }

    protected:

        // first free id for selected id type
        IdGenerator<uint32> m_ArenaTeamIds;
        IdGenerator<uint32> m_AuctionIds;
        IdGenerator<uint64> m_EquipmentSetIds;
        IdGenerator<uint32> m_GuildIds;
        IdGenerator<uint32> m_MailIds;
        IdGenerator<uint32> m_PetNumbers;

        // initial free low guid for selected guid type for map local guids
        uint32 m_FirstTemporaryCreatureGuid;
        uint32 m_FirstTemporaryGameObjectGuid;

        // guids from reserved range for use in .npc add/.gobject add commands for adding new static spawns (saved in DB) from client.
        ObjectGuidGenerator<HIGHGUID_UNIT>        m_StaticCreatureGuids;
        ObjectGuidGenerator<HIGHGUID_GAMEOBJECT>  m_StaticGameObjectGuids;

        // first free low guid for selected guid type
        ObjectGuidGenerator<HIGHGUID_PLAYER>     m_CharGuids;
        ObjectGuidGenerator<HIGHGUID_ITEM>       m_ItemGuids;
        ObjectGuidGenerator<HIGHGUID_CORPSE>     m_CorpseGuids;
        ObjectGuidGenerator<HIGHGUID_INSTANCE>   m_InstanceGuids;
        ObjectGuidGenerator<HIGHGUID_GROUP>      m_GroupGuids;

        QuestStore          m_quests;
        QuestRelationStore  m_questRelations;

        typedef CreatureModelRaceStore::CreatureModelRaceMap CreatureModelRaceMap;
        typedef CreatureModelRaceStore::CreatureModelRaceMapBounds CreatureModelRaceMapBounds;

        NpcTextStore        m_npcTexts;
        AreaTriggerStore    m_areaTriggers;

        RepRewardRateMap    m_RepRewardRateMap;
        RepOnKillMap        mRepOnKill;
        RepSpilloverTemplateMap m_RepSpilloverTemplateMap;

        GossipMenuStore     m_gossipMenus;
        PointOfInterestStore m_pointsOfInterest;

        PageTextStore m_pageTexts;

        InstanceTemplateStore m_instanceTemplates;

        CreatureEquipmentStore m_creatureEquipment;

        VehicleAccessoryStore m_vehicleAccessories;

        KeyedRecordStore<PlayerCondition> m_conditions;

        CreatureAddonStore m_creatureAddons;
        CreatureAddonStore m_creatureTemplateAddons;

        CreatureModelInfoStore m_creatureModels;

        KeyedRecordStore<CreatureTemplateSpells> m_creatureTemplateSpells;

        KeyedRecordStore<ItemPrototype> m_itemPrototypes;
        TextPool m_itemPrototypeTexts;

        KeyedRecordStore<CreatureInfo> m_creatureTemplates;
        TextPool m_creatureTemplateTexts;

        GameObjectTemplateStore m_gameObjectTemplates;

        KeyedRecordStore<GameObjectDataAddon> m_gameObjectAddons;

        ServerSpellStore m_serverSpells;

        QuestPOIStore m_questPoi;

        DungeonFinderStore m_dungeonFinder;
        LfgDungeonEntranceStore m_lfgDungeonEntrances;

        ReservedNameStore   m_reservedNames;

        GraveyardStore      m_graveyards;

        GameTeleStore       m_gameTeles;

        SpellClickStore     m_spellClicks;

        ItemConvertStore        m_itemConverts;
        ItemRequiredTargetStore m_itemRequiredTargets;

        typedef             std::vector<LocaleConstant> LocalForIndex;
        LocalForIndex        m_LocalForIndex;

        int DBCLocaleIndex;

    private:
        void LoadQuestRelationsHelper(QuestRelationsMap& map, QuestActor actor, QuestRole role);
        void LoadVendors(char const* tableName, bool isTemplates);
        void LoadTrainers(char const* tableName, bool isTemplates);

        void LoadGossipMenu(std::set<uint32>& gossipScriptSet);
        void LoadGossipMenuItems(std::set<uint32>& gossipScriptSet);

        MailLevelRewardStore m_mailLevelRewards;

        PetLevelInfoStore m_petLevels;

        void BuildPlayerLevelInfo(uint8 race, uint8 class_, uint8 level, PlayerLevelInfo* plinfo) const;
        PlayerCreateInfoStore m_playerInfos;

        ExplorationBaseXpStore m_explorationBaseXp;

        FishingBaseSkillStore m_fishingBaseSkill;

        PetNameStore m_petNames;

        // Array to store creature stats, Max creature level + 1 (for data alignement with in game level)
        CreatureClassLvlStats m_creatureClassLvlStats[DEFAULT_MAX_CREATURE_LEVEL + 1][MAX_CREATURE_CLASS][MAX_EXPANSION + 1];

        CellObjectGuidStore m_cellObjectGuids;
        CreatureSpawnStore m_creatureSpawns;
        CreatureLocaleMap mCreatureLocaleMap;
        GameObjectSpawnStore m_gameObjectSpawns;
        GameObjectLocaleMap mGameObjectLocaleMap;
        ItemLocaleMap mItemLocaleMap;
        QuestLocaleMap mQuestLocaleMap;
        NpcTextLocaleMap mNpcTextLocaleMap;
        PageTextLocaleMap mPageTextLocaleMap;
        MangosStringLocaleMap mMangosStringLocaleMap;
        std::map < int32 /*minEntryOfBracket*/, uint32 /*count*/ > m_loadedStringCount;
        GossipMenuItemsLocaleMap mGossipMenuItemsLocaleMap;
        PointOfInterestLocaleMap mPointOfInterestLocaleMap;
        DungeonEncounterMap m_DungeonEncounters;

        CreatureModelRaceStore  m_creatureModelRaces;

        CacheVendorItemMap m_mCacheVendorTemplateItemMap;
        CacheVendorItemMap m_mCacheVendorItemMap;
        CacheTrainerSpellMap m_mCacheTrainerTemplateSpellMap;
        CacheTrainerSpellMap m_mCacheTrainerSpellMap;

        HotfixData m_hotfixData;
};

#define sObjectMgr MaNGOS::Singleton<ObjectMgr>::Instance()

/// The display a creature of the template shows: the game event's model, then the spawn's override,
/// then one of the template's models (a random one where it has several and the model data allows
/// it), and creature 1's first model when the template has none.
uint32 ChooseCreatureDisplayId(CreatureInfo const* cinfo, CreatureData const* data = NULL, GameEventCreatureData const* eventData = NULL);

// scripting access functions
 bool LoadMangosStrings(DatabaseType& db, char const* table, int32 start_value = MAX_CREATURE_AI_TEXT_STRING_ID, int32 end_value = std::numeric_limits<int32>::min(), bool extra_content = false);
 CreatureInfo const* GetCreatureTemplateStore(uint32 entry);
 Quest const* GetQuestTemplateStore(uint32 entry);
 MangosStringLocale const* GetMangosStringData(int32 entry);

#endif
