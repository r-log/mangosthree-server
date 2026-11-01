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
 * @file BattleGroundMgr.cpp
 * @brief Implementation of the battleground manager and queue system.
 *
 * This file contains the implementation of the BattleGroundMgr singleton class and
 * the BattleGroundQueue class, which handle:
 * - Battleground instance creation and management
 * - Player queue management and matching
 * - Team balancing for battleground invitations
 * - Average wait time calculations
 * - Bracket-based queue organization
 * - Premade group matching
 */

#include "Utilities/Util.h"
#include "Platform/Define.h"
#include <ctime>
#include <vector>
#include <map>
#include <set>
#include "SharedDefines.h"
#include "Player.h"
#include "PlayerRegistry.h"
#include "BattleGroundMgr.h"
#include "BattleGroundAV.h"
#include "BattleGroundAB.h"
#include "BattleGroundEY.h"
#include "BattleGroundWS.h"
#include "BattleGroundNA.h"
#include "BattleGroundBE.h"
#include "BattleGroundAA.h"
#include "BattleGroundRL.h"
#include "BattleGroundSA.h"
#include "BattleGroundDS.h"
#include "BattleGroundRV.h"
#include "BattleGroundIC.h"
#include "BattleGroundRB.h"
#include "MapManager.h"
#include "ObjectMgr.h"
#include "DBCStores.h"
#include "ProgressBar.h"
#include "ArenaTeam.h"
#include "WorldGameTime.h"
#include "data/GameConfig.h"
#include "GameEventMgr.h"
#include "DisableMgr.h"
#include "GameTime.h"
#include "persistence/characters/SavedVariablesRepository.h"
#include "persistence/characters/pvp/BattleGroundStatsRepository.h"
#include "persistence/world/pvp/BattleGroundDataRepository.h"
#include "data/BattleGroundHooks.h"

#include "Policies/Singleton.h"



/*********************************************************/
/***            BATTLEGROUND MANAGER                   ***/
/*********************************************************/

namespace
{
    /// The battleground's answers to a kill or a heal: the battleground the player is in is told.
    class PlayerBattleGroundHooks : public BattleGroundHooks
    {
        public:
            bool PlayerKilled(Player& victim, Player& killer) override
            {
                if (BattleGround* bg = victim.GetBattleGround())
                {
                    bg->HandleKillPlayer(&victim, &killer);
                    return true;
                }
                return false;
            }

            void CreatureKilled(Creature& victim, Player& responsible) override
            {
                if (BattleGround* bg = responsible.GetBattleGround())
                {
                    bg->HandleKillUnit(&victim, &responsible);
                }
            }

            void HealingDone(Player& healer, uint32 gain) override
            {
                if (BattleGround* bg = healer.GetBattleGround())
                {
                    bg->UpdatePlayerScore(&healer, SCORE_HEALING_DONE, gain);
                }
            }
    };
}

BattleGroundHooks& BattleGroundMgr::GetBattleGroundHooks()
{
    static PlayerBattleGroundHooks hooks;
    return hooks;
}

namespace
{
    std::atomic<BattleGroundNotices*> s_battleGroundNoticesInstalled(NULL);

    BattleGroundNotices& BaseBattleGroundNotices()
    {
        static BattleGroundNotices notices;
        return notices;
    }
}

BattleGroundNotices::~BattleGroundNotices()
{
}

void BattleGroundNotices::Status(Player& /*player*/, BattleGround* /*bg*/, uint8 /*queueSlot*/, uint8 /*statusId*/, uint32 /*time1*/, uint32 /*time2*/, ArenaType /*arenaType*/)
{
}

void BattleGroundNotices::PvpLog(Player& /*player*/, BattleGround& /*bg*/)
{
}

void BattleGroundNotices::StartTimer(Player& /*player*/, uint32 /*timeLeft*/, uint32 /*totalTime*/)
{
}

void BattleGroundNotices::SoundToAll(BattleGround& /*bg*/, uint32 /*soundId*/)
{
}

void BattleGroundNotices::Sound(Player& /*player*/, uint32 /*soundId*/)
{
}

void BattleGroundNotices::PlayerJoined(BattleGround& /*bg*/, Team /*team*/, Player& /*player*/)
{
}

void BattleGroundNotices::PlayerLeft(BattleGround& /*bg*/, Team /*team*/, ObjectGuid /*guid*/, Player* /*sender*/)
{
}

void BattleGroundNotices::WorldStateToAll(BattleGround& /*bg*/, uint32 /*state*/, uint32 /*value*/)
{
}

void BattleGroundNotices::MessageToAll(BattleGround& /*bg*/, ChatMsg /*msgtype*/, int32 /*textId*/, Player const* /*source*/, va_list* /*args*/)
{
}

void BattleGroundNotices::YellToAll(BattleGround& /*bg*/, int32 /*textId*/, Language /*language*/, Creature const& /*source*/)
{
}

void BattleGroundNotices::Message2ToAll(BattleGround& /*bg*/, ChatMsg /*msgtype*/, int32 /*textId*/, Player const* /*source*/, int32 /*arg1*/, int32 /*arg2*/)
{
}

void BattleGroundNotices::Yell2ToAll(BattleGround& /*bg*/, int32 /*textId*/, uint32 /*language*/, Creature const& /*source*/, int32 /*arg1*/, int32 /*arg2*/)
{
}

BattleGroundNotices& BattleGroundNotices::Installed()
{
    BattleGroundNotices* notices = s_battleGroundNoticesInstalled.load(std::memory_order_acquire);
    return notices ? *notices : BaseBattleGroundNotices();
}

BattleGroundNotices* BattleGroundNotices::Install(BattleGroundNotices* notices)
{
    return s_battleGroundNoticesInstalled.exchange(notices, std::memory_order_acq_rel);
}

/**
 * @brief Constructor for BattleGroundMgr.
 *
 * Initializes all battleground containers and sets testing mode to false.
 */
BattleGroundMgr::BattleGroundMgr() : m_AutoDistributionTimeChecker(0), m_highestPvPStatsId(0), m_ArenaTesting(false)
{
    for (uint8 i = BATTLEGROUND_TYPE_NONE; i < MAX_BATTLEGROUND_TYPE_ID; ++i)
    {
        m_BattleGrounds[i].clear();
    }
    m_NextRatingDiscardUpdate = sGameConfig.getConfig(CONFIG_UINT32_ARENA_RATING_DISCARD_TIMER);
    m_Testing = false;
}

/**
 * @brief Destructor for BattleGroundMgr.
 *
 * Cleans up all active and template battlegrounds.
 */
BattleGroundMgr::~BattleGroundMgr()
{
    DeleteAllBattleGrounds();
}

void BattleGroundMgr::SendWorldText(int32 textId, ...) const
{
    MANGOS_ASSERT(m_worldText);
    va_list ap;
    va_start(ap, textId);
    m_worldText(textId, &ap);
    va_end(ap);
}

/**
 * @brief Deletes all battleground instances.
 *
 * Safely removes all active battlegrounds and template battlegrounds from memory.
 * This includes template battlegrounds that are only used as templates for creating instances.
 */
void BattleGroundMgr::DeleteAllBattleGrounds()
{
    // will also delete template bgs:
    for (uint8 i = BATTLEGROUND_TYPE_NONE; i < MAX_BATTLEGROUND_TYPE_ID; ++i)
    {
        for (BattleGroundSet::iterator itr = m_BattleGrounds[i].begin(); itr != m_BattleGrounds[i].end();)
        {
            BattleGround* bg = itr->second;
            ++itr;                                          // step from invalidate iterator pos in result element remove in ~BattleGround call
            delete bg;
        }
    }
}

/**
 * @brief Updates all active battlegrounds and processes queue operations.
 *
 * Performs the main update loop for all active battleground instances, processes
 * scheduled queue updates based on the update scheduler, and removes finished
 * battlegrounds from memory. Called once per world tick.
 *
 * @param diff The time elapsed since the last update in milliseconds (unused).
 */
void BattleGroundMgr::Update(uint32 diff)
{
    // update scheduled queues
    if (!m_QueueUpdateScheduler.empty())
    {
        std::vector<uint64> scheduled;
        {
            // copy vector and clear the other
            scheduled = std::vector<uint64>(m_QueueUpdateScheduler);
            m_QueueUpdateScheduler.clear();
        }

        for (uint8 i = 0; i < scheduled.size(); ++i)
        {
            uint32 arenaRating = scheduled[i] >> 32;
            ArenaType arenaType = ArenaType(scheduled[i] >> 24 & 255);
            BattleGroundQueueTypeId bgQueueTypeId = BattleGroundQueueTypeId(scheduled[i] >> 16 & 255);
            BattleGroundTypeId bgTypeId = BattleGroundTypeId((scheduled[i] >> 8) & 255);
            BattleGroundBracketId bracket_id = BattleGroundBracketId(scheduled[i] & 255);
            m_BattleGroundQueues[bgQueueTypeId].Update(bgTypeId, bracket_id, arenaType, arenaRating > 0, arenaRating);
        }
    }

    // if rating difference counts, maybe force-update queues
    if (sGameConfig.getConfig(CONFIG_UINT32_ARENA_MAX_RATING_DIFFERENCE) && sGameConfig.getConfig(CONFIG_UINT32_ARENA_RATING_DISCARD_TIMER))
    {
        // it's time to force update
        if (m_NextRatingDiscardUpdate < diff)
        {
            // forced update for rated arenas (scan all, but skipped non rated)
            DEBUG_LOG("BattleGroundMgr: UPDATING ARENA QUEUES");
            for (uint8 qtype = BATTLEGROUND_QUEUE_2v2; qtype <= BATTLEGROUND_QUEUE_5v5; ++qtype)
            {
                for (uint8 bracket = BG_BRACKET_ID_FIRST; bracket < MAX_BATTLEGROUND_BRACKETS; ++bracket)
                {
                    m_BattleGroundQueues[qtype].Update(
                        BATTLEGROUND_AA, BattleGroundBracketId(bracket),
                        BattleGroundMgr::BGArenaType(BattleGroundQueueTypeId(qtype)), true, 0);
                }
            }

            m_NextRatingDiscardUpdate = sGameConfig.getConfig(CONFIG_UINT32_ARENA_RATING_DISCARD_TIMER);
        }
        else
        {
            m_NextRatingDiscardUpdate -= diff;
        }
    }
}

/**
 * @brief Retrieves a battleground instance by client instance ID.
 *
 * Searches for a battleground instance using the client-side instance ID that was
 * sent in the SMSG_BATTLEFIELD_LIST packet. This is used when a player joins from the UI.
 *
 * @param instanceId The client-side instance ID.
 * @param bgTypeId The battleground type to search in.
 * @return Pointer to the battleground instance, or NULL if not found.
 */
BattleGround* BattleGroundMgr::GetBattleGroundThroughClientInstance(uint32 instanceId, BattleGroundTypeId bgTypeId)
{
    // cause at HandleBattleGroundJoinOpcode the clients sends the instanceid he gets from
    // SMSG_BATTLEFIELD_LIST we need to find the battleground with this clientinstance-id
    BattleGround* bg = GetBattleGroundTemplate(bgTypeId);
    if (!bg)
    {
        return NULL;
    }

    if (bg->isArena())
    {
        return GetBattleGround(instanceId, bgTypeId);
    }

    for (BattleGroundSet::iterator itr = m_BattleGrounds[bgTypeId].begin(); itr != m_BattleGrounds[bgTypeId].end(); ++itr)
    {
        if (itr->second->GetClientInstanceID() == instanceId)
        {
            return itr->second;
        }
    }
    return NULL;
}

/**
 * @brief Retrieves a battleground instance by instance ID.
 *
 * Searches for an active battleground instance by its server instance ID. If bgTypeId
 * is BATTLEGROUND_TYPE_NONE, searches across all battleground types.
 *
 * @param InstanceID The server instance ID.
 * @param bgTypeId The battleground type to search in, or BATTLEGROUND_TYPE_NONE for all types.
 * @return Pointer to the battleground instance, or NULL if not found.
 */
BattleGround* BattleGroundMgr::GetBattleGround(uint32 InstanceID, BattleGroundTypeId bgTypeId)
{
    // search if needed
    BattleGroundSet::iterator itr;
    if (bgTypeId == BATTLEGROUND_TYPE_NONE)
    {
        for (uint8 i = BATTLEGROUND_AV; i < MAX_BATTLEGROUND_TYPE_ID; ++i)
        {
            itr = m_BattleGrounds[i].find(InstanceID);
            if (itr != m_BattleGrounds[i].end())
            {
                return itr->second;
            }
        }
        return NULL;
    }
    itr = m_BattleGrounds[bgTypeId].find(InstanceID);
    return ((itr != m_BattleGrounds[bgTypeId].end()) ? itr->second : NULL);
}

/**
 * @brief Retrieves the template battleground for a given type.
 *
 * Returns the template battleground for the specified type. The template is the lowest-ID
 * battleground in the container and is used as a reference for creating new instances.
 *
 * @param bgTypeId The battleground type.
 * @return Pointer to the template battleground, or NULL if none exists.
 */
BattleGround* BattleGroundMgr::GetBattleGroundTemplate(BattleGroundTypeId bgTypeId)
{
    // map is sorted and we can be sure that lowest instance id has only BG template
    return m_BattleGrounds[bgTypeId].empty() ? NULL : m_BattleGrounds[bgTypeId].begin()->second;
}

/**
 * @brief Creates a unique client-visible instance ID for a battleground.
 *
 * Generates a new unique client-facing instance ID for the specified battleground type and bracket.
 * Client IDs are sequential starting from 1, filling any gaps in the ID sequence. These IDs are
 * sent to clients in the battleground list packet and used when players join via the UI.
 *
 * @param bgTypeId The battleground type.
 * @param bracket_id The bracket level.
 * @return A unique client-visible instance ID.
 */
uint32 BattleGroundMgr::CreateClientVisibleInstanceId(BattleGroundTypeId bgTypeId, BattleGroundBracketId bracket_id)
{
    if (IsArenaType(bgTypeId))
    {
        return 0; // arenas don't have client-instanceids
    }

    // we create here an instanceid, which is just for
    // displaying this to the client and without any other use..
    // the client-instanceIds are unique for each battleground-type
    // the instance-id just needs to be as low as possible, beginning with 1
    // the following works, because std::set is default ordered with "<"
    // the optimalization would be to use as bitmask std::vector<uint32> - but that would only make code unreadable
    uint32 lastId = 0;
    ClientBattleGroundIdSet& ids = m_ClientBattleGroundIds[bgTypeId][bracket_id];
    for (ClientBattleGroundIdSet::const_iterator itr = ids.begin(); itr != ids.end();)
    {
        if ((++lastId) != *itr)                             // if there is a gap between the ids, we will break..
        {
            break;
        }
        lastId = *itr;
    }
    ids.insert(lastId + 1);
    return lastId + 1;
}

/**
 * @brief Creates a new battleground instance.
 *
 * Creates a new playable battleground instance by copying the template and initializing
 * it with a new instance ID, bracket ID, and game map. The new battleground is placed in
 * queue waiting for players to join.
 *
 * @param bgTypeId The type of battleground to create.
 * @param bracketEntry
 * @param arenaType
 * @param isRated
 * @return Pointer to the newly created battleground, or NULL if creation failed.
 */
BattleGround* BattleGroundMgr::CreateNewBattleGround(BattleGroundTypeId bgTypeId, PvPDifficultyEntry const* bracketEntry, ArenaType arenaType, bool isRated)
{
    // get the template BG
    BattleGround* bg_template = GetBattleGroundTemplate(bgTypeId);
    if (!bg_template)
    {
        sLog.outError("BattleGround: CreateNewBattleGround - bg template not found for %u", bgTypeId);
        return NULL;
    }

    // for arenas there is random map used
    if (bg_template->isArena())
    {
        BattleGroundTypeId arenas[] = { BATTLEGROUND_NA, BATTLEGROUND_BE, BATTLEGROUND_RL/*, BATTLEGROUND_DS, BATTLEGROUND_RV*/ };
        bgTypeId = arenas[urand(0, countof(arenas) - 1)];
        bg_template = GetBattleGroundTemplate(bgTypeId);
        if (!bg_template)
        {
            sLog.outError("BattleGround: CreateNewBattleGround - bg template not found for %u", bgTypeId);
            return NULL;
        }
    }

    BattleGround* bg = NULL;
    // create a copy of the BG template
    switch (bgTypeId)
    {
        case BATTLEGROUND_AV:
            bg = new BattleGroundAV(*(BattleGroundAV*)bg_template);
            break;
        case BATTLEGROUND_WS:
            bg = new BattleGroundWS(*(BattleGroundWS*)bg_template);
            break;
        case BATTLEGROUND_AB:
            bg = new BattleGroundAB(*(BattleGroundAB*)bg_template);
            break;
        case BATTLEGROUND_NA:
            bg = new BattleGroundNA(*(BattleGroundNA*)bg_template);
            break;
        case BATTLEGROUND_BE:
            bg = new BattleGroundBE(*(BattleGroundBE*)bg_template);
            break;
        case BATTLEGROUND_AA:
            bg = new BattleGroundAA(*(BattleGroundAA*)bg_template);
            break;
        case BATTLEGROUND_EY:
            bg = new BattleGroundEY(*(BattleGroundEY*)bg_template);
            break;
        case BATTLEGROUND_RL:
            bg = new BattleGroundRL(*(BattleGroundRL*)bg_template);
            break;
        case BATTLEGROUND_SA:
            bg = new BattleGroundSA(*(BattleGroundSA*)bg_template);
            break;
        case BATTLEGROUND_DS:
            bg = new BattleGroundDS(*(BattleGroundDS*)bg_template);
            break;
        case BATTLEGROUND_RV:
            bg = new BattleGroundRV(*(BattleGroundRV*)bg_template);
            break;
        case BATTLEGROUND_IC:
            bg = new BattleGroundIC(*(BattleGroundIC*)bg_template);
            break;
        case BATTLEGROUND_RB:
            bg = new BattleGroundRB(*(BattleGroundRB*)bg_template);
            break;
        default:
            // error, but it is handled few lines above
            return 0;
    }

    // set before Map creating for let use proper difficulty
    bg->SetBracket(bracketEntry);

    // will also set m_bgMap, instanceid
    sMapMgr.CreateBgMap(bg->GetMapId(), bg);

    bg->SetClientInstanceID(CreateClientVisibleInstanceId(bgTypeId, bracketEntry->GetBracketId()));

    // reset the new bg (set status to status_wait_queue from status_none)
    bg->Reset();

    // start the joining of the bg
    bg->SetStatus(STATUS_WAIT_JOIN);
    bg->SetArenaType(arenaType);
    bg->SetRated(isRated);
    bg->SetRandomTypeID(bgTypeId);

    return bg;
}

/**
 * @brief Creates a battleground template.
 *
 * Creates a template battleground that serves as the prototype for all instances of this type.
 * The template stores configuration like player limits, level requirements, and spawn locations.
 * New instances are created by copying this template.
 *
 * @param bgTypeId The battleground type.
 * @param IsArena Whether the template is an arena.
 * @param MinPlayersPerTeam Minimum players required per team.
 * @param MaxPlayersPerTeam Maximum players allowed per team.
 * @param LevelMin Minimum level to queue for this battleground.
 * @param LevelMax Maximum level for this battleground.
 * @param BattleGroundName The name of the battleground.
 * @param MapID The map ID for this battleground.
 * @param Team1StartLocX Alliance spawn location X coordinate.
 * @param Team1StartLocY Alliance spawn location Y coordinate.
 * @param Team1StartLocZ Alliance spawn location Z coordinate.
 * @param Team1StartLocO Alliance spawn location orientation.
 * @param Team2StartLocX Horde spawn location X coordinate.
 * @param Team2StartLocY Horde spawn location Y coordinate.
 * @param Team2StartLocZ Horde spawn location Z coordinate.
 * @param Team2StartLocO Horde spawn location orientation.
 * @param StartMaxDist Maximum distance from spawn location for initial positioning.
 * @return The instance ID of the created template battleground.
 */
uint32 BattleGroundMgr::CreateBattleGround(BattleGroundTypeId bgTypeId, bool IsArena, uint32 MinPlayersPerTeam, uint32 MaxPlayersPerTeam, uint32 LevelMin, uint32 LevelMax, char const* BattleGroundName, uint32 MapID, float Team1StartLocX, float Team1StartLocY, float Team1StartLocZ, float Team1StartLocO, float Team2StartLocX, float Team2StartLocY, float Team2StartLocZ, float Team2StartLocO, float StartMaxDist)
{
    // Create the BG
    BattleGround* bg = NULL;
    switch (bgTypeId)
    {
        case BATTLEGROUND_AV: bg = new BattleGroundAV; break;
        case BATTLEGROUND_WS: bg = new BattleGroundWS; break;
        case BATTLEGROUND_AB: bg = new BattleGroundAB; break;
        case BATTLEGROUND_NA: bg = new BattleGroundNA; break;
        case BATTLEGROUND_BE: bg = new BattleGroundBE; break;
        case BATTLEGROUND_AA: bg = new BattleGroundAA; break;
        case BATTLEGROUND_EY: bg = new BattleGroundEY; break;
        case BATTLEGROUND_RL: bg = new BattleGroundRL; break;
        case BATTLEGROUND_SA: bg = new BattleGroundSA; break;
        case BATTLEGROUND_DS: bg = new BattleGroundDS; break;
        case BATTLEGROUND_RV: bg = new BattleGroundRV; break;
        case BATTLEGROUND_IC: bg = new BattleGroundIC; break;
        case BATTLEGROUND_RB: bg = new BattleGroundRB; break;
        default:              bg = new BattleGround;   break;                           // placeholder for non implemented BG
    }

    bg->SetMapId(MapID);
    bg->SetTypeID(bgTypeId);
    bg->SetArenaorBGType(IsArena);
    bg->SetMinPlayersPerTeam(MinPlayersPerTeam);
    bg->SetMaxPlayersPerTeam(MaxPlayersPerTeam);
    bg->SetMinPlayers(MinPlayersPerTeam * 2);
    bg->SetMaxPlayers(MaxPlayersPerTeam * 2);
    bg->SetName(BattleGroundName);
    bg->SetTeamStartLoc(ALLIANCE, Team1StartLocX, Team1StartLocY, Team1StartLocZ, Team1StartLocO);
    bg->SetTeamStartLoc(HORDE,    Team2StartLocX, Team2StartLocY, Team2StartLocZ, Team2StartLocO);
    bg->SetStartMaxDist(StartMaxDist);
    bg->SetLevelRange(LevelMin, LevelMax);

    // add bg to update list
    AddBattleGround(bg->GetInstanceID(), bg->GetTypeID(), bg);

    // return some not-null value, bgTypeId is good enough for me
    return bgTypeId;
}

/**
 * @brief Primes the `pvpstats_battlegrounds` id counter once, at start-up.
 *
 * Also re-run by `.reload config` when the statistics are switched on.
 */
void BattleGroundMgr::LoadHighestPvPStatsId()
{
    uint64 highest = 0;

    // A missing or empty table answers 0 here, and GenerateNextPvPStatsId() hands out 1.
    if (std::optional<uint64> highestId = BattleGroundStatsRepository::QueryHighestId())
    {
        highest = *highestId;
    }

    m_highestPvPStatsId.store(highest);
    sLog.outString(">> Next battleground statistics id is " UI64FMTD, highest + 1);
}

/**
 * @brief Creates initial battleground templates from the database.
 *
 * Loads battleground template configurations from the database table `battleground_template`
 * and creates the template instances for each configured battleground type. These templates
 * are used as prototypes for all new battleground instances.
 */
void BattleGroundMgr::CreateInitialBattleGrounds()
{
    uint32 count = 0;

    std::vector<BattleGroundTemplateRow> rows = BattleGroundDataRepository::QueryTemplates();

    if (rows.empty())
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outErrorDb(">> Loaded 0 battlegrounds. DB table `battleground_template` is empty.");
        sLog.outString();
        return;
    }

    BarGoLink bar(rows.size());

    for (BattleGroundTemplateRow const& row : rows)
    {
        bar.step();

        uint32 bgTypeID_ = row.id;

        // can be overwrite by values from DB
        BattlemasterListEntry const* bl = sBattlemasterListStore.LookupEntry(bgTypeID_);
        if (!bl)
        {
            sLog.outError("Battleground ID %u not found in BattlemasterList.dbc. Battleground not created.", bgTypeID_);
            continue;
        }

        if (DisableMgr::IsDisabledFor(DISABLE_TYPE_BATTLEGROUND, bgTypeID_))
        {
            continue;
        }

        BattleGroundTypeId bgTypeID = BattleGroundTypeId(bgTypeID_);

        bool IsArena = (bl->InstanceType == TYPE_ARENA);
        uint32 MinPlayersPerTeam = row.minPlayersPerTeam;
        uint32 MaxPlayersPerTeam = row.maxPlayersPerTeam;

        // check values from DB
        if (MaxPlayersPerTeam == 0)
        {
            sLog.outErrorDb("Table `battleground_template` for id %u have wrong min/max players per team settings. BG not created.", bgTypeID);
            continue;
        }

        if (MinPlayersPerTeam > MaxPlayersPerTeam)
        {
            MinPlayersPerTeam = MaxPlayersPerTeam;
            sLog.outErrorDb("Table `battleground_template` for id %u has min players > max players per team settings. Min players will use same value as max players.", bgTypeID);
        }

        float AStartLoc[4];
        float HStartLoc[4];

        uint32 start1 = row.allianceStartLoc;

        WorldSafeLocsEntry const* start = sWorldSafeLocsStore.LookupEntry(start1);
        if (start)
        {
            AStartLoc[0] = start->Loc_0;
            AStartLoc[1] = start->Loc_1;
            AStartLoc[2] = start->Loc_2;
            AStartLoc[3] = row.allianceStartO;
        }
        else if (bgTypeID == BATTLEGROUND_AA || bgTypeID == BATTLEGROUND_RB)
        {
            AStartLoc[0] = 0;
            AStartLoc[1] = 0;
            AStartLoc[2] = 0;
            AStartLoc[3] = row.allianceStartO;
        }
        else
        {
            sLog.outErrorDb("Table `battleground_template` for id %u have nonexistent WorldSafeLocs.dbc id %u in field `AllianceStartLoc`. BG not created.", bgTypeID, start1);
            continue;
        }

        uint32 start2 = row.hordeStartLoc;

        start = sWorldSafeLocsStore.LookupEntry(start2);
        if (start)
        {
            HStartLoc[0] = start->Loc_0;
            HStartLoc[1] = start->Loc_1;
            HStartLoc[2] = start->Loc_2;
            HStartLoc[3] = row.hordeStartO;
        }
        else if (bgTypeID == BATTLEGROUND_AA || bgTypeID == BATTLEGROUND_RB)
        {
            HStartLoc[0] = 0;
            HStartLoc[1] = 0;
            HStartLoc[2] = 0;
            HStartLoc[3] = row.hordeStartO;
        }
        else
        {
            sLog.outErrorDb("Table `battleground_template` for id %u have nonexistent WorldSafeLocs.dbc id %u in field `HordeStartLoc`. BG not created.", bgTypeID, start2);
            continue;
        }

        float startMaxDist = row.startMaxDist;
        // sLog.outDetail("Creating battleground %s, %u-%u", bl->Name_lang[sWorld.GetDBClang()], MinLvl, MaxLvl);
        if (!CreateBattleGround(bgTypeID, IsArena, MinPlayersPerTeam, MaxPlayersPerTeam, bl->MinLevel, bl->MaxLevel, bl->Name_lang[sGameConfig.GetDefaultDbcLocale()], bl->MapID[0], AStartLoc[0], AStartLoc[1], AStartLoc[2], AStartLoc[3], HStartLoc[0], HStartLoc[1], HStartLoc[2], HStartLoc[3], startMaxDist))
        {
            continue;
        }

        ++count;
    }

    sLog.outString(">> Loaded %u battlegrounds", count);
    sLog.outString();
}

void BattleGroundMgr::InitAutomaticArenaPointDistribution()
{
    if (sGameConfig.getConfig(CONFIG_BOOL_ARENA_AUTO_DISTRIBUTE_POINTS))
    {
        DEBUG_LOG("Initializing Automatic Arena Point Distribution");
        std::optional<uint64> nextDistribution = SavedVariablesRepository::QueryNextArenaPointDistribution();
        if (!nextDistribution)
        {
            DEBUG_LOG("Battleground: Next arena point distribution time not found in SavedVariables, reseting it now.");
            m_NextAutoDistributionTime = time_t(WorldGameTime::Get() + BATTLEGROUND_ARENA_POINT_DISTRIBUTION_DAY * sGameConfig.getConfig(CONFIG_UINT32_ARENA_AUTO_DISTRIBUTE_INTERVAL_DAYS));
            SavedVariablesRepository::InsertNextArenaPointDistribution(uint64(m_NextAutoDistributionTime));
        }
        else
        {
            m_NextAutoDistributionTime = time_t(*nextDistribution);
        }
        DEBUG_LOG("Automatic Arena Point Distribution initialized.");
    }
}

/**
 * @brief Teleports a player to their assigned battleground location.
 *
 * Moves the player to the battleground map and their team's spawn location. Handles
 * retrieving the correct start location for the player's team.
 *
 * @param pl Pointer to the player to teleport.
 * @param instanceId The battleground instance ID.
 * @param bgTypeId The battleground type.
 */
void BattleGroundMgr::SendToBattleGround(Player* pl, uint32 instanceId, BattleGroundTypeId bgTypeId)
{
    BattleGround* bg = GetBattleGround(instanceId, bgTypeId);
    if (bg)
    {
        uint32 mapid = bg->GetMapId();
        float x, y, z, O;
        Team team = pl->GetBGTeam();
        if (team == 0)
        {
            team = pl->GetTeam();
        }
        bg->GetTeamStartLoc(team, x, y, z, O);

        DETAIL_LOG("BATTLEGROUND: Sending %s to map %u, X %f, Y %f, Z %f, O %f", pl->GetName(), mapid, x, y, z, O);
        pl->TeleportTo(mapid, x, y, z, O);
    }
    else
    {
        sLog.outError("player %u trying to port to nonexistent bg instance %u", pl->GetGUIDLow(), instanceId);
    }
}

