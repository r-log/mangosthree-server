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
 * @file World.cpp
 * @brief Core world server implementation
 *
 * This file implements the World class, which is the central hub of the
 * MaNGOS game server. It manages:
 * - Server configuration and settings
 * - Game time and world updates
 * - Player sessions and limits
 * - All game systems initialization (maps, spells, quests, etc.)
 * - Server shutdown and restart procedures
 * - In-game announcements and events
 * - Various utility functions for the game world
 *
 * The World class is a singleton accessed via sWorld, and runs the main
 * server loop that processes all game logic.
 *
 * @ingroup world
 */

#include "Common/Locales.h"
#include "Utilities/Errors.h"
#include <algorithm>
#include <string>
#include <vector>
#include <set>
#include <atomic>
#include "World.h"
#include "data/GameConfig.h"
#include "Database/DatabaseEnv.h"
#include "Database/TickGuard.h"
#include "Config/Config.h"
#include "Platform/Define.h"
#include "Log.h"
#include "Opcodes.h"
#include "WorldNetwork.h"
#include "WorldSession.h"
#include "WorldPacket.h"
#include "Player.h"
#include "SkillExtraItems.h"
#include "SkillDiscovery.h"
#include "AccountMgr.h"
#include "AchievementMgr.h"
#include "ArenaTeam.h"
#include "AuctionHouseMgr.h"
#include "CharacterCache.h"
#include "InstanceDataCache.h"
#include "ObjectMgr.h"
#include "CreatureEventAIMgr.h"
#include "GuildMgr.h"
#include "spells/data/SpellMgr.h"
#include "spells/handlers/SpellHandlerRegistry.h"
#include "Chat.h"
#include "session/packets/chat/MessageChatPacket.h"
#include "session/packets/maps/WorldStatePackets.h"
#include "data/ChatPacketBuilder.h"
#include "DBCStores.h"
#include "DB2Stores.h"
#include "Mail.h"
#include "MassMailMgr.h"
#include "LootMgr.h"
#include "ItemEnchantmentMgr.h"
#include "MapManager.h"
#include "MapPhase.h"
#include "spells/auras/SpellAuras.h"
#include "Harness.h"
#include "ScriptMgr.h"
#include "CreatureAIRegistry.h"
#include "Policies/Singleton.h"
#include "BattleGround/BattleGroundMgr.h"
#include "OutdoorPvP/OutdoorPvP.h"
#include "TemporarySummon.h"
#include "terrain/FusedTerrain.hpp"
#include "terrain/GoModelStore.hpp"
#include "MoveMap.h"
#include "GameEventMgr.h"
#include "PoolManager.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "MapPersistentStateMgr.h"
#include "WaypointManager.h"
#include "GMTicketMgr.h"
#include "Util.h"
#include "AuctionHouseBot/AuctionHouseBot.h"
#include "CharacterDatabaseCleaner.h"
#include "CreatureLinkingMgr.h"
#include "Calendar.h"
#include "Weather.h"
#include "LFGMgr.h"
#include "DisableMgr.h"
#include "data/Language.h"
#include "CommandMgr.h"
#include "Version.h"
#include "UpdateTime.h"
#include "GameTime.h"
#include <cstdarg>

#include <iostream>
#include <sstream>

#include <mutex>
#include "PlayerRegistry.h"
#include "CorpseManager.h"
#include "wire/MovementCapture.h"
#include "persistence/DatabaseLifecycle.h"
#include "persistence/characters/CharacterRepository.h"
#include "persistence/characters/SavedVariablesRepository.h"
#include "persistence/characters/inventory/CurrencyRepository.h"
#include "persistence/characters/maps/CorpseRepository.h"
#include "persistence/characters/quests/QuestStatusRepository.h"
#include "persistence/login/AccountRepository.h"
#include "persistence/login/BanRepository.h"
#include "persistence/login/RealmRepository.h"
#include "persistence/world/BroadcastDataRepository.h"
#include "GameEventScheduler.h"
#include "session/ClientNotices.h"


namespace
{
    /// Stops the process when a guid generator runs out of guids.
    void StopOnGuidOverflow()
    {
        ServerControl::StopNow(ERROR_EXIT_CODE);
    }

    /// Finds a player's name by guid in the character data ObjectMgr holds.
    bool FindPlayerName(ObjectGuid guid, std::string& name)
    {
        return sObjectMgr.GetPlayerNameByGUID(guid, name);
    }
}

std::atomic<uint32> World::m_worldLoopCounter{0};

/**
 * @brief World class constructor
 *
 * Initializes all world state variables to their default values:
 * - Player limit set to 0 (unlimited)
 * - Movement allowed
 * - Shutdown mask and timer cleared
 * - Game time set to current system time
 * - Session counts zeroed
 * - All config arrays cleared
 * - DBC locale set to enUS
 * - Broadcast system disabled
 *
 * This is called once when the server starts, before any configuration
 * is loaded or systems initialized.
 */
World::World(): mail_timer(0), mail_timer_expires(0), m_social(sPlayerRegistry)
{
    m_allowMovement = true;
    m_ShutdownMask = 0;
    m_ShutdownTimer = 0;
    WorldGameTime::Set(time(NULL));
    m_startTime = WorldGameTime::Get();
    m_broadcastEnable = false;
    m_broadcastList.clear();
    m_broadcastWeight = 0;

    sGameConfig.SetDefaultDbcLocale(LOCALE_enUS);
    sGameConfig.SetAvailableDbcLocaleMask(0);

    sGameConfig.Reset();

    SessionRegistry::Install(&m_sessions);
    ResetSchedule::Install(&m_resets);
    SocialMgr::Install(&m_social);
    GroupRegistry::Install(&m_groups);
    ArenaTeamRegistry::Install(&m_arenaTeams);
    WorldBroadcast::Install(this);
    ServerControl::Install(this);
    InstallGuidOverflowHandler(&StopOnGuidOverflow);
    InstallPlayerNameResolver(&FindPlayerName);
    InstallClientNotices();
}

/// World destructor
World::~World()
{
    Wire::MovementCapture::Close();

    // it is assumed that no other thread is accessing this data when the destructor is called.  therefore, no locks are necessary

    for (auto const cliCommand : m_cliCommandQueue)
    {
        delete cliCommand;
    }

    SessionRegistry* installed = SessionRegistry::Install(NULL);
    if (installed != &m_sessions)
    {
        SessionRegistry::Install(installed);
    }

    WorldBroadcast* broadcast = WorldBroadcast::Install(NULL);
    if (broadcast != this)
    {
        WorldBroadcast::Install(broadcast);
    }

    ServerControl* control = ServerControl::Install(NULL);
    if (control != this)
    {
        ServerControl::Install(control);
    }

    SocialMgr* social = SocialMgr::Install(NULL);
    if (social != &m_social)
    {
        SocialMgr::Install(social);
    }

    GroupRegistry* groups = GroupRegistry::Install(NULL);
    if (groups != &m_groups)
    {
        GroupRegistry::Install(groups);
    }

    ArenaTeamRegistry* arenaTeams = ArenaTeamRegistry::Install(NULL);
    if (arenaTeams != &m_arenaTeams)
    {
        ArenaTeamRegistry::Install(arenaTeams);
    }

    ResetSchedule const* resets = ResetSchedule::Install(NULL);
    if (resets != &m_resets)
    {
        ResetSchedule::Install(resets);
    }

    MMAP::MMapFactory::clear();
}

/// Cleanups before world stop
void World::CleanupsBeforeStop()
{
    m_sessions.KickAll();                            // save and kick all players
    m_sessions.Update(1);                            // real players unload required a session update
    sBattleGroundMgr.DeleteAllBattleGrounds();       // unload battleground templates before different singletons destroyed
}


/// Initialize the World
void World::SetInitialWorldSettings()
{
    ///- The chat message builder of the code below the session
    InstallChatPacketBuilder(&BuildMessageChatPacket);

    ///- Initialize the random number generator
    srand((unsigned int)time(NULL));

    ///- Time server startup
    uint32 startupBegin = GameTime::GetGameTimeMS();

    ///- Initialize detour memory management
    dtAllocSetCustom(dtCustomAlloc, dtCustomFree);

    ///- Initialize config settings
    LoadConfigSettings();

    ///- Point the terrain engine at the baked tiles. Nothing else tells it where they
    ///  are, and without this every height, liquid and sight query answers "no data"
    ///  while the server otherwise starts perfectly.
    world::terrain::FusedTerrain::SetTileDir(sGameConfig.GetDataPath() + "tiles");
    world::terrain::GoModelStore::Instance().SetDirectory(sGameConfig.GetDataPath() + "gomodels");

    ///- Check the existence of the map files for all races start areas.
    if (!MapManager::ExistMapAndVMap(0, -6240.32f, 331.033f) ||                     // Dwarf/ Gnome
        !MapManager::ExistMapAndVMap(0, -8949.95f, -132.493f) ||                // Human
        !MapManager::ExistMapAndVMap(1, -618.518f, -4251.67f) ||                // Orc
        !MapManager::ExistMapAndVMap(0, 1676.35f, 1677.45f) ||                  // Scourge
        !MapManager::ExistMapAndVMap(1, 10311.3f, 832.463f) ||                  // NightElf
        !MapManager::ExistMapAndVMap(1, -2917.58f, -257.98f) ||                 // Tauren
        (getConfig(CONFIG_UINT32_EXPANSION) >= EXPANSION_TBC &&
          (!MapManager::ExistMapAndVMap(530, 10349.6f, -6357.29f) ||            // BloodElf
              !MapManager::ExistMapAndVMap(530, -3961.64f, -13931.2f))) ||          // Draenei
            (getConfig(CONFIG_UINT32_EXPANSION) >= EXPANSION_WOTLK &&
              !MapManager::ExistMapAndVMap(609, 2355.84f, -5664.77f)))              // Death Knight
    {
        sLog.outError("Baked tiles not found in '%stiles'. Run 'mep bake' against a 4.3.4 client, or correct DataDir in mangosd.conf.", sGameConfig.GetDataPath().c_str());
        Log::WaitBeforeContinueIfNeed();
        exit(1);
    }

    ///- Loading strings. Getting no records means core load has to be canceled because no error message can be output.
    sLog.outString();
    sLog.outString("Loading MaNGOS strings...");
    if (!sObjectMgr.LoadMangosStrings())
    {
        Log::WaitBeforeContinueIfNeed();
        exit(1);                                            // Error message displayed in function already
    }

    ///- Update the realm entry in the database with the realm type from the config file
    // No SQL injection as values are treated as integers

    // not send custom type REALM_FFA_PVP to realm list
    uint32 server_type = IsFFAPvPRealm() ? uint32(REALM_TYPE_PVP) : getConfig(CONFIG_UINT32_GAME_TYPE);
    uint32 realm_zone = getConfig(CONFIG_UINT32_REALM_ZONE);
    RealmRepository::SetTypeAndZone(server_type, realm_zone, realmID);

    ///- Remove the bones (they should not exist in DB though) and old corpses after a restart
    CorpseRepository::DeleteBonesAndOldCorpses(3 * DAY);

    ///- Load the DBC files
    sLog.outString("Initialize DBC data stores...");
    LoadDBCStores(sGameConfig.GetDataPath());
    DetectDBCLang();
    sObjectMgr.SetDBCLocaleIndex(GetDefaultDbcLocale());    // Get once for all the locale index of DBC language (console/broadcasts)
    LoadDB2Stores(sGameConfig.GetDataPath());

    sLog.outString("Loading SpellTemplate...");
    sObjectMgr.LoadSpellTemplate();

    sLog.outString("Loading Script Names...");
    sScriptMgr.LoadScriptNames();

    sLog.outString("Loading InstanceTemplate...");
    sObjectMgr.LoadInstanceTemplate();

    sLog.outString("Loading SkillLineAbilityMultiMap Data...");
    sSpellMgr.LoadSkillLineAbilityMap();

    sLog.outString("Loading SkillRaceClassInfoMultiMap Data...");
    sSpellMgr.LoadSkillRaceClassInfoMap();

    ///- Clean up and pack instances
    sLog.outString("Cleaning up instances...");
    sMapPersistentStateMgr.CleanupInstances();              // must be called before `creature_respawn`/`gameobject_respawn` tables

    sLog.outString("Packing instances...");
    sMapPersistentStateMgr.PackInstances();

    sLog.outString("Packing groups...");
    GroupRegistry::PackIds();                               // must be after CleanupInstances

    ///- Init highest guids before any guid using table loading to prevent using not initialized guids in some code.
    sObjectMgr.SetHighestGuids();                           // must be after packing instances
    sLog.outString();

    ///- Decoupling D7c: every offline player fact comes from here, so it is loaded before
    /// anything that asks for one. That is earlier than it looks: AuctionHouseMgr::LoadAuctions
    /// resolves every owner's name, Guild::LoadMembersFromDB repairs a broken `zone` through
    /// Player::GetZoneIdFromDB, CalendarMgr::LoadCalendarsFromDB asks for guild ids, and
    /// ObjectGuid::GetString() names a player in every log line that mentions one.
    /// The guild and arena loaders delete the orphan rows they meet as they go, and each of
    /// those deletes tells the cache.
    sLog.outString("Loading the character cache...");
    sCharacterCache.LoadFromDB();

    ///- Decoupling D7i: `instance`.`data` and `world`.`data`, which Map::CreateInstanceData
    /// used to SELECT once per map created -- on the tick, for every continent, transport
    /// deck and dungeon. Loaded here because CleanupInstances() and PackInstances() above
    /// have already deleted and renumbered what they were going to, and the first map is
    /// not created until sMapMgr.LoadTransports(), far below.
    sLog.outString("Loading instance script data...");
    sInstanceDataCache.LoadFromDB();

    sLog.outString("Loading Page Texts...");
    sObjectMgr.LoadPageTexts();

    sLog.outString("Loading Game Object Templates...");     // must be after LoadPageTexts
    sObjectMgr.LoadGameobjectInfo();

    sLog.outString("Loading GameObject models...");
    sLog.outString();

    sLog.outString("Loading Spell Chain Data...");
    sSpellMgr.LoadSpellChains();

    sLog.outString("Loading Spell Elixir types...");
    sSpellMgr.LoadSpellElixirs();

    sLog.outString("Loading Spell Learn Skills...");
    sSpellMgr.LoadSpellLearnSkills();                       // must be after LoadSpellChains

    sLog.outString("Loading Spell Learn Spells...");
    sSpellMgr.LoadSpellLearnSpells();

    sLog.outString("Loading Spell Proc Event conditions...");
    sSpellMgr.LoadSpellProcEvents();

    sLog.outString("Loading Spell Bonus Data...");
    sSpellMgr.LoadSpellBonuses();                           // must be after LoadSpellChains

    sLog.outString("Loading Spell Proc Item Enchant...");
    sSpellMgr.LoadSpellProcItemEnchant();                   // must be after LoadSpellChains

    sLog.outString("Loading Aggro Spells Definitions...");
    sSpellMgr.LoadSpellThreats();

    sLog.outString("Building the spell handler registry...");
    SpellHandlerRegistry::Game();                           // a key registered twice aborts here, not in a map tick

    sLog.outString("Loading NPC Texts...");
    sObjectMgr.LoadGossipText();

    sLog.outString("Loading Item Random Enchantments Table...");
    LoadRandomEnchantmentsTable();

    sLog.outString("Loading Disables...");                  // must be before loading quests and items
    DisableMgr::LoadDisables();

    sLog.outString("Loading Items...");                     // must be after LoadRandomEnchantmentsTable and LoadPageTexts
    sObjectMgr.LoadItemPrototypes();

    sLog.outString("Loading Item converts...");             // must be after LoadItemPrototypes
    sObjectMgr.LoadItemConverts();

    sLog.outString("Loading Item expire converts...");      // must be after LoadItemPrototypes
    sObjectMgr.LoadItemExpireConverts();

    sLog.outString("Loading Creature Model Based Info Data...");
    sObjectMgr.LoadCreatureModelInfo();

    sLog.outString("Loading Equipment templates...");
    sObjectMgr.LoadEquipmentTemplates();

    sLog.outString("Loading Creature Stats...");
    sObjectMgr.LoadCreatureClassLvlStats();

    sLog.outString("Loading Creature templates...");
    sObjectMgr.LoadCreatureTemplates();

    sLog.outString("Loading Creature template spells...");
    sObjectMgr.LoadCreatureTemplateSpells();

    sLog.outString("Loading Creature Model for race...");   // must be after creature templates
    sObjectMgr.LoadCreatureModelRace();

    sLog.outString("Loading SpellsScriptTarget...");
    sSpellMgr.LoadSpellScriptTarget();                      // must be after LoadCreatureTemplates and LoadGameobjectInfo

    sLog.outString("Loading Vehicle Accessory...");         // must be after creature templates
    sObjectMgr.LoadVehicleAccessory();

    sLog.outString("Loading ItemRequiredTarget...");
    sObjectMgr.LoadItemRequiredTarget();

    sLog.outString("Loading Reputation Reward Rates...");
    sObjectMgr.LoadReputationRewardRate();

    sLog.outString("Loading Creature Reputation OnKill Data...");
    sObjectMgr.LoadReputationOnKill();

    sLog.outString("Loading Reputation Spillover Data...");
    sObjectMgr.LoadReputationSpilloverTemplate();

    sLog.outString("Loading Points Of Interest Data...");
    sObjectMgr.LoadPointsOfInterest();

    sLog.outString("Loading Creature Data...");
    sObjectMgr.LoadCreatures();

    sLog.outString("Loading pet levelup spells...");
    sSpellMgr.LoadPetLevelupSpellMap();

    sLog.outString("Loading pet default spell additional to levelup spells...");
    sSpellMgr.LoadPetDefaultSpells();

    sLog.outString("Loading Creature Addon Data...");
    sObjectMgr.LoadCreatureAddons();                        // must be after LoadCreatureTemplates() and LoadCreatures()
    sLog.outString(">>> Creature Addon Data loaded");
    sLog.outString();

    sLog.outString("Loading Gameobject Data...");
    sObjectMgr.LoadGameObjects();

    sLog.outString("Loading Gameobject Addon Data...");
    sObjectMgr.LoadGameObjectAddon();

    sLog.outString("Loading CreatureLinking Data...");      // must be after Creatures
    sCreatureLinkingMgr.LoadFromDB();

    sLog.outString("Loading Objects Pooling Data...");
    sPoolMgr.LoadFromDB();

    sLog.outString("Loading Weather Data...");
    sWeatherMgr.LoadWeatherZoneChances();

    sLog.outString("Loading Quests...");
    sObjectMgr.LoadQuests();                                // must be loaded after DBCs, creature_template, item_template, gameobject tables

    sLog.outString("Loading Quest POI");
    sObjectMgr.LoadQuestPOI();

    sLog.outString("Loading Quests Relations...");
    sObjectMgr.LoadQuestRelations();                        // must be after quest load
    sLog.outString(">>> Quests Relations loaded");
    sLog.outString();

    sLog.outString("Checking Quest Disables...");
    DisableMgr::CheckQuestDisables();                       // must be after loading quests

    sLog.outString("Loading Game Event Data...");           // must be after sPoolMgr.LoadFromDB and quests to properly load pool events and quests for events
    sGameEventMgr.LoadFromDB();
    sLog.outString(">>> Game Event Data loaded");
    sLog.outString();

    // Load Conditions
    sLog.outString("Loading Conditions...");
    sObjectMgr.LoadConditions();

    sLog.outString("Creating map persistent states for non-instanceable maps...");     // must be after PackInstances(), LoadCreatures(), sPoolMgr.LoadFromDB(), sGameEventMgr.LoadFromDB();
    sMapPersistentStateMgr.InitWorldMaps();
    sLog.outString();

    sLog.outString("Loading Creature Respawn Data...");     // must be after LoadCreatures(), and sMapPersistentStateMgr.InitWorldMaps()
    sMapPersistentStateMgr.LoadCreatureRespawnTimes();

    sLog.outString("Loading Gameobject Respawn Data...");   // must be after LoadGameObjects(), and sMapPersistentStateMgr.InitWorldMaps()
    sMapPersistentStateMgr.LoadGameobjectRespawnTimes();

    sLog.outString("Loading UNIT_NPC_FLAG_SPELLCLICK Data...");
    sObjectMgr.LoadNPCSpellClickSpells();

    sLog.outString("Loading SpellArea Data...");            // must be after quest load
    sSpellMgr.LoadSpellAreas();

    sLog.outString("Loading AreaTrigger definitions...");
    sObjectMgr.LoadAreaTriggerTeleports();                  // must be after item template load

    sLog.outString("Loading Quest Area Triggers...");
    sObjectMgr.LoadQuestAreaTriggers();                     // must be after LoadQuests

    sLog.outString("Loading Tavern Area Triggers...");
    sObjectMgr.LoadTavernAreaTriggers();

    if (ScriptHooks::IsInstalled())
    {
        sLog.outString("Loading all script bindings...");
        sScriptMgr.LoadScriptBinding();
    }

    sLog.outString("Loading Graveyard-zone links...");
    sObjectMgr.LoadGraveyardZones();

    sLog.outString("Loading spell target destination coordinates...");
    sSpellMgr.LoadSpellTargetPositions();

    sLog.outString("Loading spell pet auras...");
    sSpellMgr.LoadSpellPetAuras();

    sLog.outString("Loading Player Create Info & Level Stats...");
    sObjectMgr.LoadPlayerInfo();
    sLog.outString(">>> Player Create Info & Level Stats loaded");
    sLog.outString();

    sLog.outString("Loading Exploration BaseXP Data...");
    sObjectMgr.LoadExplorationBaseXP();

    sLog.outString("Loading Pet Name Parts...");
    sObjectMgr.LoadPetNames();

    CharacterDatabaseCleaner::CleanDatabase();
    sLog.outString();

    sLog.outString("Loading the max pet number...");
    sObjectMgr.LoadPetNumber();

    sLog.outString("Loading pet level stats...");
    sObjectMgr.LoadPetLevelInfo();

    sLog.outString("Loading Player Corpses...");
    sCorpseManager.LoadCorpses();

    sLog.outString("Loading Player level dependent mail rewards...");
    sObjectMgr.LoadMailLevelRewards();

    sLog.outString("Loading Loot Tables...");
    LoadLootTables();
    sLog.outString(">>> Loot Tables loaded");
    sLog.outString();

    sLog.outString("Loading Skill Discovery Table...");
    LoadSkillDiscoveryTable();

    sLog.outString("Loading Skill Extra Item Table...");
    LoadSkillExtraItemTable();

    sLog.outString("Loading Skill Fishing base level requirements...");
    sObjectMgr.LoadFishingBaseSkillLevel();

    sLog.outString();
    sLog.outString("Loading Achievements...");
    sAchievementMgr.LoadAchievementReferenceList();
    sAchievementMgr.LoadAchievementCriteriaList();
    sAchievementMgr.LoadAchievementCriteriaRequirements();
    sAchievementMgr.LoadRewards();
    sAchievementMgr.LoadRewardLocales();
    sAchievementMgr.LoadCompletedAchievements();
    sAchievementMgr.ReportOrphanedCriteriaProgress();
    sLog.outString(">>> Achievements loaded");
    sLog.outString();

    sLog.outString("Loading Instance encounters data...");  // must be after Creature loading
    sObjectMgr.LoadInstanceEncounters();

    sLog.outString("Loading Gossip scripts...");
    sScriptMgr.LoadDbScripts(DBS_ON_GOSSIP);                 // must be before gossip menu options

    sObjectMgr.LoadGossipMenus();

    sLog.outString("Loading Vendors...");
    sObjectMgr.LoadVendorTemplates();                       // must be after load ItemTemplate
    sObjectMgr.LoadVendors();                               // must be after load CreatureTemplate, VendorTemplate, and ItemTemplate

    sLog.outString("Loading Trainers...");
    sObjectMgr.LoadTrainerTemplates();                      // must be after load CreatureTemplate
    sObjectMgr.LoadTrainers();                              // must be after load CreatureTemplate, TrainerTemplate

    sLog.outString("Loading Waypoint scripts...");          // before loading from creature_movement
    sScriptMgr.LoadDbScripts(DBS_ON_CREATURE_MOVEMENT);

    sLog.outString("Loading Waypoints...");
    sWaypointMgr.Load();

    ///- Loading localization data
    sLog.outString("Loading Localization strings...");
    sObjectMgr.LoadCreatureLocales();                       // must be after CreatureInfo loading
    sObjectMgr.LoadGameObjectLocales();                     // must be after GameobjectInfo loading
    sObjectMgr.LoadItemLocales();                           // must be after ItemPrototypes loading
    sObjectMgr.LoadQuestLocales();                          // must be after QuestTemplates loading
    sObjectMgr.LoadGossipTextLocales();                     // must be after LoadGossipText
    sObjectMgr.LoadPageTextLocales();                       // must be after PageText loading
    sObjectMgr.LoadGossipMenuItemsLocales();                // must be after gossip menu items loading
    sObjectMgr.LoadPointOfInterestLocales();                // must be after POI loading
    //sCommandMgr.LoadCommandHelpLocale();                  TODO: Need to figure out why this crashes
    sLog.outString(">>> Localization strings loaded");
    sLog.outString();

    // Decoupling D7h: the `command` table's security and help overrides. This used to be
    // read lazily, on the first chat or console command of the process -- i.e. on the world
    // thread, inside World::Update. Here it is start-up work like every other loader, and
    // the tick never waits for it. `.reload command` re-runs the same function.
    sLog.outString("Loading chat command overrides...");
    ChatHandler::LoadCommandTable();
    sLog.outString();

    ///- Load dynamic data tables from the database
    sLog.outString("Loading Auctions...");
    sAuctionMgr.LoadAuctionItems();
    sAuctionMgr.LoadAuctions();
    sLog.outString(">>> Auctions loaded");
    sLog.outString();

    sLog.outString("Loading Guilds...");
    sGuildMgr.LoadGuilds();

    sLog.outString("Loading ArenaTeams...");
    m_arenaTeams.Load();

    sLog.outString("Loading Groups...");
    m_groups.Load();

    sCalendarMgr.LoadCalendarsFromDB();

    sLog.outString("Loading ReservedNames...");
    sObjectMgr.LoadReservedPlayersNames();

    sLog.outString("Loading GameObjects for quests...");
    sObjectMgr.LoadGameObjectForQuests();

    sLog.outString("Loading BattleMasters...");
    sBattleGroundMgr.LoadBattleMastersEntry();

    sLog.outString("Loading BattleGround event indexes...");
    sBattleGroundMgr.LoadBattleEventIndexes();

    sLog.outString("Loading GameTeleports...");
    sObjectMgr.LoadGameTele();

    sLog.outString("Loading GM tickets...");
    sTicketMgr.LoadGMTickets();

    sLog.outString("Loading Dungeon Finder Requirements...");
    sObjectMgr.LoadDungeonFinderRequirements();

    sLog.outString("Loading Dungeon Finder Rewards...");
    sObjectMgr.LoadDungeonFinderRewards();

    sLog.outString("Loading Dungeon Finder Items...");
    sObjectMgr.LoadDungeonFinderItems();

    sLog.outString("Loading LFG Dungeon Entrances...");
    sObjectMgr.LoadLfgDungeonEntrances();

    ///- Handle outdated emails (delete/return)
    sLog.outString("Returning old mails...");
    ReturnOrDeleteOldMails(false);

    ///- Load and initialize DBScripts Engine
    sLog.outString("Loading DB-Scripts Engine...");
    sScriptMgr.LoadDbScripts(DBS_ON_QUEST_START);           // must be after load Creature/Gameobject(Template/Data) and QuestTemplate
    sScriptMgr.LoadDbScripts(DBS_ON_QUEST_END);             // must be after load Creature/Gameobject(Template/Data) and QuestTemplate
    sScriptMgr.LoadDbScripts(DBS_ON_SPELL);                 // must be after load Creature/Gameobject(Template/Data)
    sScriptMgr.LoadDbScripts(DBS_ON_GO_USE);                // must be after load Creature/Gameobject(Template/Data)
    sScriptMgr.LoadDbScripts(DBS_ON_GOT_USE);               // must be after load Creature/Gameobject(Template/Data)
    sScriptMgr.LoadDbScripts(DBS_ON_EVENT);                 // must be after load Creature/Gameobject(Template/Data)
    sScriptMgr.LoadDbScripts(DBS_ON_CREATURE_DEATH);        // must be after load Creature/Gameobject(Template/Data)
    sLog.outString(">>> DB Scripts loaded");
    sLog.outString();

    sLog.outString("Loading Scripts text locales...");      // must be after Load*Scripts calls
    sScriptMgr.LoadDbScriptStrings();

    ///- Load and initialize EventAI Scripts
    sLog.outString("Loading CreatureEventAI Texts...");
    sEventAIMgr.LoadCreatureEventAI_Texts(false);           // false, will checked in LoadCreatureEventAI_Scripts

    sLog.outString("Loading CreatureEventAI Summons...");
    sEventAIMgr.LoadCreatureEventAI_Summons(false);         // false, will checked in LoadCreatureEventAI_Scripts

    sLog.outString("Loading CreatureEventAI Scripts...");
    sEventAIMgr.LoadCreatureEventAI_Scripts();

    sLog.outString("Initializing Scripts...");
    if (ScriptHooks::IsInstalled())
    {
        switch (sScriptMgr.LoadScriptLibrary("mangosscript"))
        {
            case SCRIPT_LOAD_OK:
                sLog.outString("Scripting library loaded.");
                break;
            case SCRIPT_LOAD_ERR_NOT_FOUND:
                sLog.outError("Scripting library not found or not accessible.");
                break;
            case SCRIPT_LOAD_ERR_WRONG_API:
                sLog.outError("Scripting library has wrong list functions (outdated?).");
                break;
            case SCRIPT_LOAD_ERR_OUTDATED:
                sLog.outError("Scripting library build for old mangosd revision. You need rebuild it.");
                break;
        }
    }
    else
    {
        sLog.outError("SD3 was not included in compilation, not using it.");
    }
    sLog.outString();

    ///- Initialize game time and timers
    sLog.outString("Initialize game time and timers");
    WorldGameTime::Set(time(NULL));
    m_startTime = WorldGameTime::Get();

    tm local;
    time_t curr;
    time(&curr);
    local = *(localtime(&curr));                            // dereference and assign
    char isoDate[128];
    sprintf(isoDate, "%04d-%02d-%02d %02d:%02d:%02d",
            local.tm_year + 1900, local.tm_mon + 1, local.tm_mday, local.tm_hour, local.tm_min, local.tm_sec);

    RealmRepository::InsertUptime(realmID, uint64(m_startTime), isoDate);

    m_timers[WUPDATE_AUCTIONS].SetInterval(MINUTE * IN_MILLISECONDS);
    m_timers[WUPDATE_UPTIME].SetInterval(getConfig(CONFIG_UINT32_UPTIME_UPDATE) * MINUTE * IN_MILLISECONDS);
    // Update "uptime" table based on configuration entry in minutes.
    m_timers[WUPDATE_CORPSES].SetInterval(20 * MINUTE * IN_MILLISECONDS);
    m_timers[WUPDATE_DELETECHARS].SetInterval(DAY * IN_MILLISECONDS); // check for chars to delete every day

    // for AhBot
    m_timers[WUPDATE_AHBOT].SetInterval(20 * IN_MILLISECONDS); // every 20 sec

    // for Dungeon Finder
    m_timers[WUPDATE_LFGMGR].SetInterval(30 * IN_MILLISECONDS); // every 30 sec

    // for AutoBroadcast
    sLog.outString("Starting AutoBroadcast System");
    if (m_broadcastEnable)
    {
        LoadBroadcastStrings();
    }
    else
    {
        sLog.outString("AutoBroadcast is disabled");
    }
    sLog.outString();

    if (m_broadcastEnable)
    {
        m_broadcastTimer.SetInterval(getConfig(CONFIG_UINT32_AUTOBROADCAST_INTERVAL) * IN_MILLISECONDS);
    }

    // to set mailtimer to return mails every day between 4 and 5 am
    // mailtimer is increased when updating auctions
    // one second is 1000 -(tested on win system)
    std::tm ltm = safe_localtime(WorldGameTime::Get());
    mail_timer = uint32((((ltm.tm_hour + 20) % 24) * HOUR * IN_MILLISECONDS) / m_timers[WUPDATE_AUCTIONS].GetInterval());
    // 1440
    mail_timer_expires = uint32((DAY * IN_MILLISECONDS) / (m_timers[WUPDATE_AUCTIONS].GetInterval()));
    DEBUG_LOG("Mail timer set to: %u, mail return is called every %u minutes", mail_timer, mail_timer_expires);

    ///- Initialize static helper structures
    AIRegistry::Initialize();
    Player::InitVisibleBits();

    ///- Initialize MapManager
    sLog.outString("Starting Map System");
    sMapMgr.Initialize();
    sLog.outString();

    ///- Initialize Battlegrounds
    sLog.outString("Starting BattleGround System");
    sBattleGroundMgr.SetWorldTextSink([this](int32 textId, va_list* args)
    {
        WorldTextV(textId, args);
    });
    sBattleGroundMgr.CreateInitialBattleGrounds();
    if (getConfig(CONFIG_BOOL_BATTLEGROUND_SCORE_STATISTICS))
    {
        // Decoupling D7i: the next `pvpstats_battlegrounds` id, read once here instead of
        // once per battleground that ends, on the tick. Behind the same config test the
        // old site was behind, so a realm with the statistics off never touches the table.
        sBattleGroundMgr.LoadHighestPvPStatsId();
    }
    sBattleGroundMgr.InitAutomaticArenaPointDistribution();

    ///- Initialize Outdoor PvP
    sLog.outString("Starting Outdoor PvP System");
    sOutdoorPvPMgr.SetDefenseMessageSink([this](uint32 zoneId, int32 textId)
    {
        DefenseMessage(zoneId, textId);
    });
    sOutdoorPvPMgr.InitOutdoorPvP();

    // Not sure if this can be moved up in the sequence (with static data loading) as it uses MapManager
    sLog.outString("Loading Transports...");
    sMapMgr.LoadTransports();

    sLog.outString("Deleting expired bans...");
    BanRepository::DeleteExpiredIpBans();
    sLog.outString();

    sLog.outString("Calculate next daily quest and dungeon reset time...");
    InitDailyQuestResetTime();

    sLog.outString("Calculate next weekly quest reset time...");
    InitWeeklyQuestResetTime();

    sLog.outString("Calculate next monthly quest reset time...");
    SetMonthlyQuestResetTime();

    sLog.outString("Calculate random battleground reset time...");
    InitRandomBGResetTime();

    sLog.outString("Calculate next currency reset time...");
    InitCurrencyResetTime();

    sLog.outString("Starting Game Event system...");
    uint32 nextGameEvent = sGameEventScheduler.Initialize();
    m_timers[WUPDATE_EVENTS].SetInterval(nextGameEvent);    // depend on next event

    sLog.outString("Loading grids for active creatures or transports...");
    uint32 loadContinentsBegin = GameTime::GetGameTimeMS();
    MapManager::LivingWorldStartupStats lwStats = sMapMgr.LoadContinentActiveEntities();
    uint32 loadContinentsMs = GetMSTimeDiffToNow(loadContinentsBegin);
    sLog.outString("[LivingWorld] startup summary: maps-forced=%u, total-unique-grids=%u, total-newly-loaded=%u, total-map-transports=%u, LoadContinents=%u ms",
                   lwStats.forcedMaps, lwStats.totalUniqueGrids, lwStats.totalNewlyLoaded, lwStats.totalMapTransports, loadContinentsMs);
    sLog.outString();

    // Delete all characters which have been deleted X days before
    Player::DeleteOldCharacters();

    sLog.outString("Initialize AuctionHouseBot...");
    sAuctionBot.Initialize();
    sLog.outString();

    showFooter();

    uint32 startupDuration = GetMSTimeDiffToNow(startupBegin);
    sLog.outString("SERVER STARTUP TIME: %i minutes %i seconds", (startupDuration / 60000), ((startupDuration % 60000) / 1000));
    sLog.outString();
}

/**
 * @brief Prints the startup footer and enabled module summary.
 */
void World::showFooter()
{
    std::set<std::string> modules_;

    // SD3 is either included or disabled
    if (ScriptHooks::IsInstalled())
    {
        modules_.insert("      ScriptDev3 (SD3) : Enabled");
    }

    std::string thisClientVersion (Version::GetClientVersion());
    std::string thisClientBuilds = Version::GetAcceptedClientBuildsStr();

    std::string sModules;
    for (std::set<std::string>::const_iterator it = modules_.begin(); it != modules_.end(); ++it)
    {
        sModules = sModules + " \n" + *it;
    }

    sLog.outString("\n"
        "_______________________________________________________\n"
        "\n"
        " MaNGOS Server: World Initialization Complete\n"
        "_______________________________________________________\n"
        "\n"
        "        Server Version : %s\n"
        "      Database Version : Rel%s.%s.%s\n"
        "\n"
        "    Supporting Clients : %s\n"
        "                Builds : %s\n"
        "\n"
        "         Module Status -\n%s\n"
        "_______________________________________________________\n"
        , Version::GetFullVersion(), Version::GetWorldDBVersion(), Version::GetWorldDBStructure(), Version::GetWorldDBContent(),
            thisClientVersion.c_str(), thisClientBuilds.c_str(), sModules.c_str());
}

/**
 * @brief Detects the active DBC locale and available locale mask.
 */
void World::DetectDBCLang()
{
    // get the DBC Locale
    int uLocale = ReadDBCLocale(sGameConfig.GetDataPath());

    if (uLocale == -1)
    {
        sLog.outError("Unable to determine your DBC Locale! (corrupt or missing component.wow-<locale>.txt file)");
        Log::WaitBeforeContinueIfNeed();
        exit(1);
    }

    sGameConfig.SetDefaultDbcLocale(LocaleConstant(uLocale));
    sGameConfig.SetAvailableDbcLocaleMask(GetDBCLoadedLocaleMask() | (1 << uLocale));

    std::string availableLocales;
    for (int i = 0; i < MAX_LOCALE; ++i)
    {
        if (sGameConfig.GetAvailableDbcLocale(LocaleConstant(i)) == LocaleConstant(i))
        {
            availableLocales += localeNames[i];
            availableLocales += " ";
        }
    }

    sLog.outString("Using %s DBC Locale as default. All available DBC locales: %s", localeNames[GetDefaultDbcLocale()], availableLocales.c_str());
    sLog.outString();
}

/// Update the World !
void World::Update(uint32 diff)
{
    // Decoupling D7: for this whole body, this thread is the tick. Every synchronous
    // database acquisition made under it -- directly or through any chain below --
    // is counted, and `.server database` prints the total.
    TickGuard::Scope tickGuard;

    ///- Update the different timers
    for (int i = 0; i < WUPDATE_COUNT; ++i)
    {
        if (m_timers[i].GetCurrent() >= 0)
        {
            m_timers[i].Update(diff);
        }
        else
        {
            m_timers[i].SetCurrent(0);
        }
    }

    if (m_broadcastEnable)
    {
        if (m_broadcastTimer.GetCurrent() >= 0)
        {
            m_broadcastTimer.Update(diff);
        }
        else
        {
            m_broadcastTimer.SetCurrent(0);
        }

        if (m_broadcastTimer.Passed())
        {
            m_broadcastTimer.Reset();
            AutoBroadcast();
        }
    }

    ///- Update the game time and check for shutdown time
    _UpdateGameTime();
    GameTime::UpdateGameTimers();
    sWorldUpdateTime.UpdateWithDiff(diff);

    ///- Flush buffered log files about once per second. The file sinks are
    ///  fully buffered (setvbuf), so this bounds how long a buffered line waits
    ///  to reach disk; error paths still flush immediately at emit time. Safe as
    ///  a function-local static: World::Update runs only on the world thread.
    static uint32 logFlushTimer = 0;
    logFlushTimer += diff;
    if (logFlushTimer >= 1000)
    {
        logFlushTimer = 0;
        sLog.Flush();
    }

    ///-Update mass mailer tasks if any
    sMassMailMgr.Update();

    /// Handle daily quests and dungeon reset time
    if (WorldGameTime::Get() > m_resets.nextDailyQuestReset)
    {
        ResetDailyQuests();
    }

    /// Handle weekly quests reset time
    if (WorldGameTime::Get() > m_resets.nextWeeklyQuestReset)
    {
        ResetWeeklyQuests();
    }

    /// Handle monthly quests reset time
    if (WorldGameTime::Get() > m_resets.nextMonthlyQuestReset)
    {
        ResetMonthlyQuests();
    }

    /// Handle weekly currency cap reset time
    if (WorldGameTime::Get() > m_resets.nextCurrencyReset)
    {
        ResetCurrencyWeekCounts();
    }

    /// <ul><li> Handle auctions when the timer has passed
    if (m_timers[WUPDATE_AUCTIONS].Passed())
    {
        m_timers[WUPDATE_AUCTIONS].Reset();

        ///- Update mails (return old mails with item, or delete them)
        //(tested... works on win)
        if (++mail_timer > mail_timer_expires)
        {
            mail_timer = 0;
            ReturnOrDeleteOldMails(true);
        }

        ///- Handle expired auctions
        sAuctionMgr.Update();
    }

    /// <li> Handle AHBot operations
    if (m_timers[WUPDATE_AHBOT].Passed())
    {
        sAuctionBot.Update();
        m_timers[WUPDATE_AHBOT].Reset();
    }

    /// <li> Handle LFG (Dungeon Finder) queue and role-check processing
    if (getConfig(CONFIG_BOOL_LFG_ENABLE) && m_timers[WUPDATE_LFGMGR].Passed())
    {
        sLFGMgr.Update();
        m_timers[WUPDATE_LFGMGR].Reset();

        DEBUG_FILTER_LOG(LOG_FILTER_LFG, "WORLD: LFGMgr::Update tick");
    }

    /// <li> Handle session updates
    m_sessions.Update(diff);

    /// <li> Drop redirects whose client never opened its second world stream.
    /// Sessions withdraw their own on retry and on logout, so this only ever
    /// collects after an abrupt end; it is what keeps the registry bounded
    /// rather than merely usually empty.
    sWorldNetwork.ExpirePendingRedirects();

    /// <li> Update uptime table
    if (m_timers[WUPDATE_UPTIME].Passed())
    {
        uint32 tmpDiff = uint32(WorldGameTime::Get() - m_startTime);
        uint32 maxClientsNum = m_sessions.MaxActiveCount();

        m_timers[WUPDATE_UPTIME].Reset();
        RealmRepository::SetUptime(tmpDiff, maxClientsNum, realmID, uint64(m_startTime));
    }

    /// <li> Handle all other objects
    ///- Update objects (maps, transport, creatures,...)
    sMapMgr.Update(diff);
    ApplyAfterMapPhasePosts();
    ApplyOffMapAuraUpdates();
    // The GM harness (movement P0-C): scenario steps and samples run here, after
    // every map's tick and before anything else touches units, where console
    // commands run too -- outside the map phase the ownership guard watches.
    sHarness.Update(diff);
    sBattleGroundMgr.Update(diff);
    sOutdoorPvPMgr.Update(diff);

    ///- Delete all characters which have been deleted X days before
    if (m_timers[WUPDATE_DELETECHARS].Passed())
    {
        m_timers[WUPDATE_DELETECHARS].Reset();
        Player::DeleteOldCharacters();
    }

    // execute callbacks from sql queries that were queued recently
    UpdateResultQueue();

    ///- Erase corpses once every 20 minutes
    if (m_timers[WUPDATE_CORPSES].Passed())
    {
        m_timers[WUPDATE_CORPSES].Reset();

        sCorpseManager.RemoveOldCorpses();
    }

    ///- Process Game events when necessary
    if (m_timers[WUPDATE_EVENTS].Passed())
    {
        m_timers[WUPDATE_EVENTS].Reset();                   // to give time for Update() to be processed
        uint32 nextGameEvent = sGameEventScheduler.Update();
        m_timers[WUPDATE_EVENTS].SetInterval(nextGameEvent);
        m_timers[WUPDATE_EVENTS].Reset();
    }

    /// </ul>
    ///- Move all creatures with "delayed move" and remove and delete all objects with "delayed remove"
    sMapMgr.RemoveAllObjectsInRemoveList();

    // update the instance reset times
    sMapPersistentStateMgr.Update();

    // And last, but not least handle the issued cli commands
    ProcessCliCommands();

    // cleanup unused GridMap objects as well as VMaps
    sTerrainMgr.Update(diff);
}

namespace MaNGOS
{
    class WorldWorldTextBuilder
    {
        public:
            typedef std::vector<WorldPacket*> WorldPacketList;
            explicit WorldWorldTextBuilder(int32 textId, va_list* args = NULL) : i_textId(textId), i_args(args) {}
            void operator()(WorldPacketList& data_list, int32 loc_idx)
            {
                char const* text = sObjectMgr.GetMangosString(i_textId, loc_idx);

                if (i_args)
                {
                    // we need copy va_list before use or original va_list will corrupted
                    va_list ap;
                    va_copy(ap, *i_args);

                    char str [2048];
                    vsnprintf(str, 2048, text, ap);
                    va_end(ap);

                    do_helper(data_list, &str[0]);
                }
                else
                {
                    do_helper(data_list, (char*)text);
                }
            }
        private:
            char* lineFromMessage(char*& pos)
            {
                char* start = strtok(pos, "\n");
                pos = NULL;
                return start;
            }
            void do_helper(WorldPacketList& data_list, char* text)
            {
                char* pos = text;

                while (char* line = lineFromMessage(pos))
                {
                    WorldPacket* data = new WorldPacket();
                    BuildMessageChatPacket(*data, CHAT_MSG_SYSTEM, line);
                    data_list.push_back(data);
                }
            }

            int32 i_textId;
            va_list* i_args;
    };
}                                                           // namespace MaNGOS

/// Sends a system message, formatted with `args`, to all players
void World::WorldTextV(int32 string_id, va_list* args)
{
    MaNGOS::WorldWorldTextBuilder wt_builder(string_id, args);
    MaNGOS::LocalizedPacketListDo<MaNGOS::WorldWorldTextBuilder> wt_do(wt_builder);
    for (SessionMap::const_iterator itr = m_sessions.All().begin(); itr != m_sessions.All().end(); ++itr)
    {
        if (WorldSession* session = itr->second)
        {
            Player* player = session->GetPlayer();
            if (player && player->IsInWorld())
            {
                wt_do(player);
            }
        }
    }
}

/// Sends a server message to the specified or all players
void World::SendServerMessage(ServerMessageType type, const char* text /*=""*/, Player* player /*= NULL*/)
{
    WorldPacket data(SMSG_SERVER_MESSAGE, 50);              // guess size
    data << uint32(type);
    data << text;

    if (player)
    {
        player->GetSession()->SendPacket(&data);
    }
    else
    {
        m_sessions.Broadcast(data);
    }
}

/// Sends a zone under attack message to all players not in an instance
void World::ZoneUnderAttack(uint32 zoneId, Team team)
{
    AfterMapPhasePost const post = { AfterMapPhasePost::ZONE_UNDER_ATTACK, zoneId, 0, team, 0 };
    if (m_afterMapPhase.Post(post))
    {
        return;
    }

    WorldPacket data(SMSG_ZONE_UNDER_ATTACK, 4);
    data << uint32(zoneId);

    for (SessionMap::const_iterator itr = m_sessions.All().begin(); itr != m_sessions.All().end(); ++itr)
    {
        if (WorldSession* session = itr->second)
        {
            Player* player = session->GetPlayer();
            if (player && player->IsInWorld() && player->GetTeam() == team && !player->GetMap()->Instanceable())
            {
                itr->second->SendPacket(&data);
            }
        }
    }
}

/// Sends a world defense message to all players not in an instance
void World::DefenseMessage(uint32 zoneId, int32 textId)
{
    AfterMapPhasePost const post = { AfterMapPhasePost::DEFENSE_MESSAGE, zoneId, textId, TEAM_NONE, 0 };
    if (m_afterMapPhase.Post(post))
    {
        return;
    }

    for (SessionMap::const_iterator itr = m_sessions.All().begin(); itr != m_sessions.All().end(); ++itr)
    {
        if (WorldSession* session = itr->second)
        {
            Player* player = session->GetPlayer();
            if (player && player->IsInWorld() && !player->GetMap()->Instanceable())
            {
                char const* message = session->GetMangosString(textId);
                uint32 messageLength = strlen(message) + 1;

                WorldPacket data(SMSG_DEFENSE_MESSAGE, 4 + 4 + messageLength);
                data << uint32(zoneId);
                data << uint32(messageLength);
                data << message;
                session->SendPacket(&data);
            }
        }
    }
}

bool AfterMapPhaseQueue::Post(AfterMapPhasePost const& post)
{
    if (!MapPhase::Active())
    {
        return false;
    }

    std::lock_guard<std::mutex> guard(m_lock);
    m_posts.push_back(post);
    return true;
}

std::vector<AfterMapPhasePost> AfterMapPhaseQueue::Take()
{
    std::vector<AfterMapPhasePost> taken;
    std::lock_guard<std::mutex> guard(m_lock);
    taken.swap(m_posts);
    return taken;
}

bool World::PostAfterMapPhase(AfterMapPhasePost const& post)
{
    return m_afterMapPhase.Post(post);
}

/// Tells every client that the player `name` is the realm's first to earn `achievementId`
void World::RealmFirstAchievement(char const* name, ObjectGuid guid, uint32 achievementId)
{
    WorldPacket data;
    AchievementMgr::BuildServerFirstAchievementPacket(data, name, guid, achievementId);
    m_sessions.Broadcast(data);
}

/// Tells every client that the world state `field` is now `value`
void World::WorldStateChanged(uint32 field, uint32 value)
{
    WorldStateUpdateFact fact;
    fact.state = field;
    fact.value = value;
    WorldPacket data;
    BuildUpdateWorldStatePacket(data, fact);
    m_sessions.Broadcast(data);
}

void World::ApplyAfterMapPhasePosts()
{
    std::vector<AfterMapPhasePost> const posts = m_afterMapPhase.Take();
    for (std::vector<AfterMapPhasePost>::const_iterator itr = posts.begin(); itr != posts.end(); ++itr)
    {
        switch (itr->kind)
        {
            case AfterMapPhasePost::DEFENSE_MESSAGE:
                DefenseMessage(itr->zoneId, itr->textId);
                break;
            case AfterMapPhasePost::ZONE_UNDER_ATTACK:
                ZoneUnderAttack(itr->zoneId, itr->team);
                break;
            case AfterMapPhasePost::GRAVEYARD_LINK_TEAM:
                sObjectMgr.SetGraveYardLinkTeam(itr->graveyardId, itr->zoneId, itr->team);
                break;
        }
    }
}

void World::ApplyOffMapAuraUpdates()
{
    std::vector<OffMapAuraUpdate> const updates = OffMapAuraUpdates::Take();
    for (std::vector<OffMapAuraUpdate>::const_iterator itr = updates.begin(); itr != updates.end(); ++itr)
    {
        Map* map = sMapMgr.FindMap(itr->mapId, itr->instanceId);
        Unit* target = map ? map->GetUnit(ObjectGuid(itr->target)) : NULL;
        SpellAuraHolder* holder = target ? target->GetSpellAuraHolder(itr->spellId, ObjectGuid(itr->caster)) : NULL;
        if (!holder)
        {
            continue;
        }

        holder->UpdateHolder(itr->diff);
        if (!holder->IsDeleted() && !(holder->IsPermanent() || holder->IsPassive()) && holder->GetAuraDuration() == 0)
        {
            target->RemoveSpellAuraHolder(holder, AURA_REMOVE_BY_EXPIRE);
        }
    }
}
/// Ban an account or ban an IP address, duration_secs if it is positive used, otherwise permban
BanReturn World::Ban(BanMode mode, std::string nameOrIP, uint32 duration_secs, std::string reason, std::string const& author)
{
    BanRepository::EscapeString(nameOrIP);
    BanRepository::EscapeString(reason);
    std::string safe_author = author;
    BanRepository::EscapeString(safe_author);

    std::vector<uint32> accounts;                           // used for kicking

    ///- Update the database with ban information
    switch (mode)
    {
        case BAN_IP:
            // No SQL injection as strings are escaped
            accounts = BanRepository::QueryAccountsOnIp(nameOrIP);
            BanRepository::InsertIpBan(nameOrIP, duration_secs, safe_author, reason);
            break;
        case BAN_ACCOUNT:
            // No SQL injection as string is escaped
            accounts = BanRepository::QueryAccountIdNamed(nameOrIP);
            break;
        case BAN_CHARACTER:
            // No SQL injection as string is escaped
            accounts = CharacterRepository::QueryAccountOfCharacterNamed(nameOrIP);
            break;
        default:
            return BAN_SYNTAX_ERROR;
    }

    if (accounts.empty())
    {
        if (mode == BAN_IP)
        {
            return BAN_SUCCESS;
        }                             // ip correctly banned but nobody affected (yet)
        else
        {
            return BAN_NOTFOUND;
        }                            // Nobody to ban
    }

    ///- Disconnect all affected players (for IP it can be several)
    for (uint32 account : accounts)
    {

        if (mode != BAN_IP)
        {
            // No SQL injection as strings are escaped
            BanRepository::InsertAccountBan(account, duration_secs, safe_author, reason);
        }

        if (WorldSession* sess = m_sessions.Find(account))
            if (std::string(sess->GetPlayerName()) != author)
            {
                sess->KickPlayer();
            }
    }

    return BAN_SUCCESS;
}

/// Remove a ban from an account or IP address
bool World::RemoveBan(BanMode mode, std::string nameOrIP)
{
    if (mode == BAN_IP)
    {
        BanRepository::DeleteIpBan(nameOrIP);
    }
    else
    {
        uint32 account = 0;
        if (mode == BAN_ACCOUNT)
        {
            account = sAccountMgr.GetId(nameOrIP);
        }
        else if (mode == BAN_CHARACTER)
        {
            account = sObjectMgr.GetPlayerAccountIdByPlayerName(nameOrIP);
        }

        if (!account)
        {
            return false;
        }

        // NO SQL injection as account is uint32
        BanRepository::DeactivateAccountBans(account);
    }
    return true;
}

/// Update the game time
void World::_UpdateGameTime()
{
    ///- update the time
    time_t thisTime = WorldClock::NowUnix();
    /// The seconds step back once when a harness run ends; the step is taken
    /// as no time passed, rather than underflowing to a huge elapsed value.
    const uint32 elapsed = thisTime > WorldGameTime::Get() ? uint32(thisTime - WorldGameTime::Get()) : 0;
    WorldGameTime::Set(thisTime);

    ///- if there is a shutdown timer
    if (!ServerControl::IsStopped() && m_ShutdownTimer > 0 && elapsed > 0)
    {
        ///- ... and it is overdue, stop the world
        if (m_ShutdownTimer <= elapsed)
        {
            if (!(m_ShutdownMask & SHUTDOWN_MASK_IDLE) || m_sessions.ActiveAndQueuedCount() == 0)
            {
                ServerControl::StopNow(ServerControl::ExitCode());
            }                         // exist code already set
            else
            {
                m_ShutdownTimer = 1;
            }                        // minimum timer value to wait idle state
        }
        ///- ... else decrease it and if necessary display a shutdown countdown to the users
        else
        {
            m_ShutdownTimer -= elapsed;

            ShutdownMessage();
        }
    }
}

/// Shutdown the server
void World::Shutdown(uint32 time, uint32 options, uint8 exitcode)
{
    // ignore if server shutdown at next tick
    if (ServerControl::IsStopped())
    {
        return;
    }

    m_ShutdownMask = options;
    ServerControl::SetExitCode(exitcode);

    ///- If the shutdown time is 0, stop the world (except if shutdown is 'idle' with remaining sessions)
    if (time == 0)
    {
        if (!(options & SHUTDOWN_MASK_IDLE) || m_sessions.ActiveAndQueuedCount() == 0)
        {
                m_sessions.SaveAllPlayers();      // save all players.
                ServerControl::StopNow(exitcode);
        }
        else
        {
            m_ShutdownTimer = 1;
        }                            // So that the session count is re-evaluated at next world tick
    }
    ///- Else set the shutdown timer and warn users
    else
    {
        m_ShutdownTimer = time;
        ShutdownMessage(true);
    }
}

/// Display a shutdown message to the user(s)
void World::ShutdownMessage(bool show /*= false*/, Player* player /*= NULL*/)
{
    // not show messages for idle shutdown mode
    if (m_ShutdownMask & SHUTDOWN_MASK_IDLE)
    {
        return;
    }

    ///- Display a message every 12 hours, 1 hour, 5 minutes, 1 minute and 15 seconds
    if (show ||
        (m_ShutdownTimer < 5 * MINUTE && (m_ShutdownTimer % 15) == 0) ||            // < 5 min; every 15 sec
        (m_ShutdownTimer < 15 * MINUTE && (m_ShutdownTimer % MINUTE) == 0) ||       // < 15 min; every 1 min
        (m_ShutdownTimer < 30 * MINUTE && (m_ShutdownTimer % (5 * MINUTE)) == 0) || // < 30 min; every 5 min
        (m_ShutdownTimer < 12 * HOUR && (m_ShutdownTimer % HOUR) == 0) ||           // < 12 h; every 1 h
        (m_ShutdownTimer >= 12 * HOUR && (m_ShutdownTimer % (12 * HOUR)) == 0))     // >= 12 h; every 12 h
    {
        std::string str = secsToTimeString(m_ShutdownTimer, TimeFormat::Numeric);

        ServerMessageType msgid = (m_ShutdownMask & SHUTDOWN_MASK_RESTART) ? SERVER_MSG_RESTART_TIME : SERVER_MSG_SHUTDOWN_TIME;

        SendServerMessage(msgid, str.c_str(), player);
        DEBUG_LOG("Server is %s in %s", (m_ShutdownMask & SHUTDOWN_MASK_RESTART) ? "restart" : "shutting down", str.c_str());
    }
}

/// Cancel a planned server shutdown
void World::CancelShutdown()
{
    // nothing cancel or too later
    if (!m_ShutdownTimer || ServerControl::IsStopped())
    {
        return;
    }

    ServerMessageType msgid = (m_ShutdownMask & SHUTDOWN_MASK_RESTART) ? SERVER_MSG_RESTART_CANCELLED : SERVER_MSG_SHUTDOWN_CANCELLED;

    m_ShutdownMask = 0;
    m_ShutdownTimer = 0;
    ServerControl::SetExitCode(SHUTDOWN_EXIT_CODE);
    SendServerMessage(msgid);

    DEBUG_LOG("Server %s cancelled.", (m_ShutdownMask & SHUTDOWN_MASK_RESTART) ? "restart" : "shutdown");
}

// This handles the issued and queued CLI commands
void World::ProcessCliCommands()
{
    std::lock_guard<std::mutex> guard(m_cliCommandQueueLock);

    while (!m_cliCommandQueue.empty())
    {
        CliCommandHolder* command = m_cliCommandQueue.front();
        m_cliCommandQueue.pop_front();

        DEBUG_LOG("CLI command under processing...");
        CliCommandHolder::Print* zprint = command->m_print;
        void* callbackArg = command->m_callbackArg;
        CliHandler handler(callbackArg, zprint);
        handler.ParseCommands(command->m_command);

        if (command->m_commandFinished)
        {
            command->m_commandFinished(callbackArg, !handler.HasSentErrorMessage());
        }

        delete command;
    }
}

/**
 * @brief Initializes the asynchronous result queue state.
 */
void World::InitResultQueue()
{
}

/**
 * @brief Processes queued asynchronous database results.
 */
void World::UpdateResultQueue()
{
    // process async result queues
    DatabaseLifecycle::ProcessCharacterResults();
    DatabaseLifecycle::ProcessWorldResults();
    DatabaseLifecycle::ProcessLoginResults();
}

void World::InitWeeklyQuestResetTime()
{
    std::optional<uint64> nextReset = SavedVariablesRepository::QueryNextWeeklyQuestReset();
    if (!nextReset)
    {
        m_resets.nextWeeklyQuestReset = time_t(time(NULL));        // game time not yet init
    }
    else
    {
        m_resets.nextWeeklyQuestReset = time_t(*nextReset);
    }

    // generate time by config
    time_t curTime = time(NULL);
    tm localTm;
    localTm = safe_localtime(curTime);

    int week_day_offset = localTm.tm_wday - int(getConfig(CONFIG_UINT32_QUEST_WEEKLY_RESET_WEEK_DAY));

    // current week reset time
    localTm.tm_hour = getConfig(CONFIG_UINT32_QUEST_WEEKLY_RESET_HOUR);
    localTm.tm_min  = 0;
    localTm.tm_sec  = 0;
    time_t nextWeekResetTime = mktime(&localTm);
    nextWeekResetTime -= week_day_offset * DAY;             // move time to proper day

    // next reset time before current moment
    if (curTime >= nextWeekResetTime)
    {
        nextWeekResetTime += WEEK;
    }

    // normalize reset time
    m_resets.nextWeeklyQuestReset = m_resets.nextWeeklyQuestReset < curTime ? nextWeekResetTime - WEEK : nextWeekResetTime;

    if (!nextReset)
    {
        SavedVariablesRepository::InsertNextWeeklyQuestReset(uint64(m_resets.nextWeeklyQuestReset));
    }
}

void World::InitDailyQuestResetTime()
{
    std::optional<uint64> nextReset = SavedVariablesRepository::QueryNextDailyQuestReset();
    if (!nextReset)
    {
        m_resets.nextDailyQuestReset = time_t(time(NULL));         // game time not yet init
    }
    else
    {
        m_resets.nextDailyQuestReset = time_t(*nextReset);
    }

    // generate time by config
    time_t curTime = time(NULL);
    tm localTm;
    localTm = safe_localtime(curTime);

    localTm.tm_hour = getConfig(CONFIG_UINT32_QUEST_DAILY_RESET_HOUR);
    localTm.tm_min  = 0;
    localTm.tm_sec  = 0;

    // current day reset time
    time_t nextDayResetTime = mktime(&localTm);

    // next reset time before current moment
    if (curTime >= nextDayResetTime)
    {
        nextDayResetTime += DAY;
    }

    // normalize reset time
    m_resets.nextDailyQuestReset = m_resets.nextDailyQuestReset < curTime ? nextDayResetTime - DAY : nextDayResetTime;

    if (!nextReset)
    {
        SavedVariablesRepository::InsertNextDailyQuestReset(uint64(m_resets.nextDailyQuestReset));
    }
}

time_t NextMonthlyQuestReset(time_t now, time_t stored, bool keepEarlier)
{
    tm localTm;
    localTm = safe_localtime(now);

    int month = localTm.tm_mon;
    int year = localTm.tm_year;

    ++month;

    // month 11 is december, next is january (0)
    if (month > 11)
    {
        month = 0;
        year += 1;
    }

    // reset time for next month
    localTm.tm_year = year;
    localTm.tm_mon = month;
    localTm.tm_mday = 1;                                    // don't know if we really need config option for day/hour
    localTm.tm_hour = 0;
    localTm.tm_min  = 0;
    localTm.tm_sec  = 0;
    localTm.tm_isdst = -1;                                  // the offset in force on that date, not today's

    time_t nextMonthResetTime = mktime(&localTm);

    // a local midnight that does not exist can resolve to the hour before it, on the last day of the month
    if (safe_localtime(nextMonthResetTime).tm_mday != 1)
    {
        nextMonthResetTime += HOUR;
    }

    // the reset is always after now
    if (nextMonthResetTime <= now)
    {
        nextMonthResetTime += ((now - nextMonthResetTime) / HOUR + 1) * HOUR;
    }

    return (keepEarlier && stored < nextMonthResetTime) ? stored : nextMonthResetTime;
}

time_t MonthlyQuestResetOnLocalMidnight(time_t stored)
{
    if (stored <= 0)
    {
        return stored;
    }

    const tm storedTm = safe_localtime(stored);
    if (storedTm.tm_mday == 1 && storedTm.tm_hour == 0 && storedTm.tm_min == 0 && storedTm.tm_sec == 0)
    {
        return stored;
    }

    return NextMonthlyQuestReset(stored - 12 * HOUR, 0, false);
}

void World::SetMonthlyQuestResetTime(bool initialize)
{
    if (initialize)
    {
        std::optional<uint64> nextReset = SavedVariablesRepository::QueryNextMonthlyQuestReset();

        if (!nextReset)
        {
            m_resets.nextMonthlyQuestReset = WorldGameTime::Get();
        }
        else
        {
            const time_t stored = time_t(*nextReset);
            m_resets.nextMonthlyQuestReset = MonthlyQuestResetOnLocalMidnight(stored);
            if (m_resets.nextMonthlyQuestReset != stored)
            {
                sLog.outBasic("Monthly quest reset time %s (" UI64FMTD ") is not local midnight "
                    "on the first of a month, moved to %s (" UI64FMTD ")",
                    TimeToTimestampStr(stored).c_str(), uint64(stored),
                    TimeToTimestampStr(m_resets.nextMonthlyQuestReset).c_str(), uint64(m_resets.nextMonthlyQuestReset));
            }
        }
    }

    // computed from the world game time, the clock Update compares the reset against
    m_resets.nextMonthlyQuestReset = NextMonthlyQuestReset(WorldGameTime::Get(), m_resets.nextMonthlyQuestReset, initialize);

    // Row must exist for this to work. Currently row is added by InitDailyQuestResetTime(), called before this function
    SavedVariablesRepository::SetNextMonthlyQuestReset(uint64(m_resets.nextMonthlyQuestReset));
}

void World::InitCurrencyResetTime()
{
    std::optional<uint64> nextReset = SavedVariablesRepository::QueryNextCurrencyReset();
    if (!nextReset)
    {
        m_resets.nextCurrencyReset = time_t(time(NULL));        // game time not yet init
    }
    else
    {
        m_resets.nextCurrencyReset = time_t(*nextReset);
    }

    // re-init case or not exists
    if (!m_resets.nextCurrencyReset)
    {
        // generate time by config
        time_t curTime = time(NULL);
        tm localTm;
        localTm = safe_localtime(curTime);

        int week_day_offset = localTm.tm_wday - int(getConfig(CONFIG_UINT32_CURRENCY_RESET_TIME_WEEK_DAY));

        // current week reset time
        localTm.tm_hour = getConfig(CONFIG_UINT32_CURRENCY_RESET_TIME_HOUR);
        localTm.tm_min  = 0;
        localTm.tm_sec  = 0;
        time_t nextWeekResetTime = mktime(&localTm);
        nextWeekResetTime -= week_day_offset * DAY;

        // next reset time before current moment
        if (curTime >= nextWeekResetTime)
        {
            nextWeekResetTime += getConfig(CONFIG_UINT32_CURRENCY_RESET_INTERVAL) * DAY;
        }

        // normalize reset time
        m_resets.nextCurrencyReset = m_resets.nextCurrencyReset < curTime ? nextWeekResetTime - getConfig(CONFIG_UINT32_CURRENCY_RESET_INTERVAL) * DAY : nextWeekResetTime;
    }

    if (!nextReset)
    {
        SavedVariablesRepository::InsertNextCurrencyReset(uint64(m_resets.nextCurrencyReset));
    }
}

void World::InitRandomBGResetTime()
{
    std::optional<uint64> nextReset = SavedVariablesRepository::QueryNextRandomBGReset();
    if (!nextReset)
    {
        m_resets.nextRandomBGReset = time_t(time(NULL));         // game time not yet init
    }
    else
    {
        m_resets.nextRandomBGReset = time_t(*nextReset);
    }

    // generate time by config
    time_t curTime = time(NULL);
    tm localTm;
    localTm = safe_localtime(curTime);

    localTm.tm_hour = getConfig(CONFIG_UINT32_RANDOM_BG_RESET_HOUR);
    localTm.tm_min = 0;
    localTm.tm_sec = 0;

    // current day reset time
    time_t nextDayResetTime = mktime(&localTm);

    // next reset time before current moment
    if (curTime >= nextDayResetTime)
    {
        nextDayResetTime += DAY;
    }

    // normalize reset time
    m_resets.nextRandomBGReset = m_resets.nextRandomBGReset < curTime ? nextDayResetTime - DAY : nextDayResetTime;
    if (!nextReset)
    {
        SavedVariablesRepository::InsertNextRandomBGReset(uint64(m_resets.nextRandomBGReset));
    }
}

void World::ResetDailyQuests()
{
    DETAIL_LOG("Daily quests reset for all characters.");
    QuestStatusRepository::DeleteEveryDaily();
    for (SessionMap::const_iterator itr = m_sessions.All().begin(); itr != m_sessions.All().end(); ++itr)
        if (itr->second->GetPlayer())
        {
            itr->second->GetPlayer()->ResetDailyQuestStatus();
        }

    m_resets.nextDailyQuestReset = time_t(m_resets.nextDailyQuestReset + DAY);
    SavedVariablesRepository::SetNextDailyQuestReset(uint64(m_resets.nextDailyQuestReset));

    if (getConfig(CONFIG_BOOL_LFG_ENABLE))
    {
        sLFGMgr.ResetDailyRecords();
    }
}

void World::ResetWeeklyQuests()
{
    DETAIL_LOG("Weekly quests reset for all characters.");
    QuestStatusRepository::DeleteEveryWeekly();
    for (SessionMap::const_iterator itr = m_sessions.All().begin(); itr != m_sessions.All().end(); ++itr)
        if (itr->second->GetPlayer())
        {
            itr->second->GetPlayer()->GetQuestStatusMgr().ResetWeeklyQuestStatus();
        }

    m_resets.nextWeeklyQuestReset = time_t(m_resets.nextWeeklyQuestReset + WEEK);
    SavedVariablesRepository::SetNextWeeklyQuestReset(uint64(m_resets.nextWeeklyQuestReset));

    if (getConfig(CONFIG_BOOL_LFG_ENABLE))
    {
        sLFGMgr.ResetWeeklyRecords();
    }
}

void World::ResetMonthlyQuests()
{
    DETAIL_LOG("Monthly quests reset for all characters.");
    QuestStatusRepository::TruncateMonthly();

    for (SessionMap::const_iterator itr = m_sessions.All().begin(); itr != m_sessions.All().end(); ++itr)
        if (itr->second->GetPlayer())
        {
            itr->second->GetPlayer()->GetQuestStatusMgr().ResetMonthlyQuestStatus();
        }

    SetMonthlyQuestResetTime(false);
}

void World::ResetCurrencyWeekCounts()
{
    CurrencyRepository::ResetEveryWeekCount();

    for (SessionMap::const_iterator itr = m_sessions.All().begin(); itr != m_sessions.All().end(); ++itr)
        if (itr->second->GetPlayer())
        {
            itr->second->GetPlayer()->ResetCurrencyWeekCounts();
        }

    for (ArenaTeamRegistry::ArenaTeamMap::const_iterator titr = m_arenaTeams.All().begin(); titr != m_arenaTeams.All().end(); ++titr)
    {
        if (ArenaTeam * at = titr->second)
        {
            at->FinishWeek();                              // set played this week etc values to 0 in memory, too
            at->SaveToDB();                                // save changes
            at->NotifyStatsChanged();                      // notify the players of the changes
        }
    }

    m_resets.nextCurrencyReset = time_t(m_resets.nextCurrencyReset + DAY * sGameConfig.getConfig(CONFIG_UINT32_CURRENCY_RESET_INTERVAL));

    SavedVariablesRepository::SetNextCurrencyReset(uint64(m_resets.nextCurrencyReset));
}


/**
 * @brief Loads automatic broadcast messages and their weights from the database.
 */
void World::LoadBroadcastStrings()
{
    if (!m_broadcastEnable)
    {
        return;
    }

    std::vector<BroadcastRow> rows = BroadcastDataRepository::QueryBroadcasts();

    if (rows.empty())
    {
        m_broadcastEnable = false;
        sLog.outErrorDb("DB table `autobroadcast` is empty.");
        sLog.outString();
        return;
    }

    m_broadcastList.clear();

    BarGoLink bar(rows.size());
    m_broadcastWeight = 0;

    for (BroadcastRow const& row : rows)
    {
        bar.step();

        uint32 ratio = row.ratio;
        if (ratio == 0)
        {
            continue;
        }

        m_broadcastWeight += ratio;

        BroadcastString bs;
        bs.text = row.content ? row.content->c_str() : NULL;
        bs.freq = m_broadcastWeight;
        m_broadcastList.push_back(bs);
    }

    if (m_broadcastWeight == 0)
    {
        sLog.outString(">> Loaded 0 broadcast strings.");
        m_broadcastEnable = false;
    }
    else
    {
        sLog.outString(">> Loaded %zu broadcast strings.", m_broadcastList.size());
    }
}

/**
 * @brief Sends a weighted random automatic broadcast message.
 */
void World::AutoBroadcast()
{
    if (m_broadcastList.size() == 1)
    {
        WorldText(LANG_AUTOBROADCAST, m_broadcastList[0].text.c_str());
    }
    else
    {
        uint32 rn = urand(1, m_broadcastWeight);
        std::vector<BroadcastString>::const_iterator it;
        for (it = m_broadcastList.begin(); it != m_broadcastList.end(); ++it)
        {
            if (rn <= it->freq)
            {
                break;
            }
        }
        WorldText(LANG_AUTOBROADCAST, it->text.c_str());
    }
}

void World::StartEvent(uint16 eventId)
{
    sGameEventScheduler.StartEvent(eventId, true);
}

void World::StopEvent(uint16 eventId)
{
    sGameEventScheduler.StopEvent(eventId, true);
}

bool World::StartHarness(std::string const& what, uint32 seedBase)
{
    return sHarness.Start(what, seedBase);
}

std::string World::HarnessStatus() const
{
    return sHarness.Status();
}

uint32 World::HarnessSeedBase() const
{
    return Harness::kSeedBase;
}
