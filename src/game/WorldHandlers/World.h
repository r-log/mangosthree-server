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

/// \addtogroup world The World
/// @{
/// \file

#ifndef MANGOS_H_WORLD
#define MANGOS_H_WORLD

#include <unordered_map>
#include <deque>
#include "LockedQueue/LockedQueue.h"
#include "Common/ServerDefines.h"
#include "Platform/Define.h"
#include "Common/Locales.h"
#include <cstdarg>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>
#include "Utilities/Util.h"
#include "Timer.h"
#include "Policies/Singleton.h"
#include "SharedDefines.h"
#include "data/GameConfig.h"
#include "WorldGameTime.h"

#include <map>
#include <set>
#include <list>
#include <mutex>
#include <atomic>
#include <optional>

class Object;
class ObjectGuid;
class WorldPacket;
class WorldSession;
class Player;
class Weather;
class SqlResultQueue;

// ServerMessages.dbc
enum ServerMessageType
{
    SERVER_MSG_SHUTDOWN_TIME          = 1,
    SERVER_MSG_RESTART_TIME           = 2,
    SERVER_MSG_CUSTOM                 = 3,
    SERVER_MSG_SHUTDOWN_CANCELLED     = 4,
    SERVER_MSG_RESTART_CANCELLED      = 5,
    SERVER_MSG_BG_SHUTDOWN_TIME       = 6,
    SERVER_MSG_BG_RESTART_TIME        = 7,
    SERVER_MSG_INSTANCE_SHUTDOWN_TIME = 8,
    SERVER_MSG_INSTANCE_RESTART_TIME  = 9,
};

enum ShutdownMask
{
    SHUTDOWN_MASK_RESTART = 1,
    SHUTDOWN_MASK_IDLE    = 2,
};

enum ShutdownExitCode
{
    SHUTDOWN_EXIT_CODE = 0,
    ERROR_EXIT_CODE    = 1,
    RESTART_EXIT_CODE  = 2,
};

/// Timers for different object refresh rates
enum WorldTimers
{
    WUPDATE_AUCTIONS    = 0,
    WUPDATE_UPTIME      = 1,
    WUPDATE_CORPSES     = 2,
    WUPDATE_EVENTS      = 3,
    WUPDATE_DELETECHARS = 4,
    WUPDATE_AHBOT       = 5,
    WUPDATE_LFGMGR      = 6,
    WUPDATE_WEATHERS    = 7,
    WUPDATE_COUNT       = 8
};

/// Can be used in SMSG_AUTH_RESPONSE packet
enum BillingPlanFlags
{
    SESSION_NONE            = 0x00,
    SESSION_UNUSED          = 0x01,
    SESSION_RECURRING_BILL  = 0x02,
    SESSION_FREE_TRIAL      = 0x04,
    SESSION_IGR             = 0x08,
    SESSION_USAGE           = 0x10,
    SESSION_TIME_MIXTURE    = 0x20,
    SESSION_RESTRICTED      = 0x40,
    SESSION_ENABLE_CAIS     = 0x80,
};

/// Storage class for commands issued for delayed execution
struct CliCommandHolder
{
    typedef void Print(void*, const char*);
    typedef void CommandFinished(void*, bool success);

    void* m_callbackArg;
    char* m_command;
    Print* m_print;
    CommandFinished* m_commandFinished;

    CliCommandHolder(void* callbackArg, const char* command, Print* zprint, CommandFinished* commandFinished)
        : m_callbackArg(callbackArg), m_print(zprint), m_commandFinished(commandFinished)
    {
        size_t len = strlen(command) + 1;
        m_command = new char[len];
        memcpy(m_command, command, len);
    }

    ~CliCommandHolder() { delete[] m_command; }
};

/// World-thread work raised inside the map phase, applied after the barrier.
struct AfterMapPhasePost
{
    enum Kind { DEFENSE_MESSAGE, ZONE_UNDER_ATTACK, GRAVEYARD_LINK_TEAM };

    Kind kind;
    uint32 zoneId;
    int32 textId;
    Team team;
    uint32 graveyardId;
};

/// The posts made by map threads during the map phase; locked because any map thread may post.
class AfterMapPhaseQueue
{
    public:
        bool Post(AfterMapPhasePost const& post);
        std::vector<AfterMapPhasePost> Take();

    private:
        std::vector<AfterMapPhasePost> m_posts;
        mutable std::mutex m_lock;
};

/// The World

typedef std::unordered_map<uint32, WorldSession*> SessionMap;

class World
{
    public:
        static std::atomic<uint32> m_worldLoopCounter;

        World();
        ~World();

        void CleanupsBeforeStop();

        WorldSession* FindSession(uint32 id) const;
        void AddSession(WorldSession* s);
        bool RemoveSession(uint32 id);
        /// Get the number of current active sessions
        void UpdateMaxSessionCounters();
        const SessionMap& GetAllSessions() const { return m_sessions; }
        uint32 GetActiveAndQueuedSessionCount() const { return m_sessions.size(); }
        uint32 GetActiveSessionCount() const { return m_sessions.size() - m_QueuedSessions.size(); }
        uint32 GetQueuedSessionCount() const { return m_QueuedSessions.size(); }
        /// Get the maximum number of parallel sessions on the server since last reboot
        uint32 GetMaxQueuedSessionCount() const { return m_maxQueuedSessionCount; }
        uint32 GetMaxActiveSessionCount() const { return m_maxActiveSessionCount; }
        Player* FindPlayerInZone(uint32 zone);

        /// Get the active session server limit (or security level limitations)
        uint32 GetPlayerAmountLimit() const { return m_playerLimit >= 0 ? m_playerLimit : 0; }
        AccountTypes GetPlayerSecurityLimit() const { return m_playerLimit <= 0 ? AccountTypes(-m_playerLimit) : SEC_PLAYER; }

        /// Set the active session server limit (or security level limitation)
        void SetPlayerLimit(int32 limit, bool needUpdate = false);

        // player Queue
        typedef std::list<WorldSession*> Queue;
        void AddQueuedSession(WorldSession*);
        bool RemoveQueuedSession(WorldSession* session);
        int32 GetQueuedSessionPos(WorldSession*);

        /// \todo Actions on m_allowMovement still to be implemented
        /// Is movement allowed?
        bool getAllowMovement() const { return m_allowMovement; }
        /// Allow/Disallow object movements
        void SetAllowMovement(bool allow) { m_allowMovement = allow; }

        /// Set a new Message of the Day
        void SetMotd(const std::string& motd) { m_motd = motd; }
        /// Get the current Message of the Day
        const char* GetMotd() const { return m_motd.c_str(); }
        void showFooter();

        LocaleConstant GetDefaultDbcLocale() const { return sGameConfig.GetDefaultDbcLocale(); }

        /// Get the path where data (dbc, maps) are stored on disk
        std::string GetDataPath() const { return m_dataPath; }

        /// When server started?
        time_t const& GetStartTime() const { return m_startTime; }
        /// Uptime (in secs)
        uint32 GetUptime() const { return uint32(WorldGameTime::Get() - m_startTime); }
        /// Next daily quests reset time
        time_t GetNextDailyQuestsResetTime() const { return m_NextDailyQuestReset; }
        time_t GetNextWeeklyQuestsResetTime() const { return m_NextWeeklyQuestReset; }

        /// Get the maximum skill level a player can reach
        uint16 GetConfigMaxSkillValue() const { return sGameConfig.GetConfigMaxSkillValue(); }

        void SetInitialWorldSettings();
        void LoadConfigSettings(bool reload = false);

        void SendWorldText(int32 string_id, ...);
        void SendWorldTextV(int32 string_id, va_list* args);
        void SendGlobalMessage(WorldPacket* packet, AccountTypes minSec = SEC_PLAYER);
        void SendServerMessage(ServerMessageType type, const char* text = "", Player* player = NULL);
        void SendZoneUnderAttackMessage(uint32 zoneId, Team team);
        void SendDefenseMessage(uint32 zoneId, int32 textId);
        bool PostAfterMapPhase(AfterMapPhasePost const& post);

        /// Are we in the middle of a shutdown?
        bool IsShutdowning() const { return m_ShutdownTimer > 0; }
        void ShutdownServ(uint32 time, uint32 options, uint8 exitcode);
        void ShutdownCancel();
        void ShutdownMsg(bool show = false, Player* player = NULL);
        static uint8 GetExitCode() { return m_ExitCode; }
        static void StopNow(uint8 exitcode) { m_stopEvent = true; m_ExitCode = exitcode; }
        static bool IsStopped() { return m_stopEvent; }

        void Update(uint32 diff);

        void UpdateSessions(uint32 diff);

        /// Get a server configuration element (see #eConfigFloatValues)
        void setConfig(eConfigFloatValues index, float value) { sGameConfig.setConfig(index, value); }
        /// Get a server configuration element (see #eConfigFloatValues)
        float getConfig(eConfigFloatValues rate) const { return sGameConfig.getConfig(rate); }

        /// Set a server configuration element (see #eConfigUInt64Values)
        void setConfig(eConfigUInt64Values index, uint64 value) { sGameConfig.setConfig(index, value); }
        /// Get a server configuration element (see #eConfigUInt64Values)
        uint64 getConfig(eConfigUInt64Values index) const { return sGameConfig.getConfig(index); }

        /// Set a server configuration element (see #eConfigInt64Values)
        void setConfig(eConfigInt64Values index, int64 value) { sGameConfig.setConfig(index, value); }
        /// Get a server configuration element (see #eConfigInt64Values)
        int64 getConfig(eConfigInt64Values index) const { return sGameConfig.getConfig(index); }

        /// Set a server configuration element (see #eConfigUInt32Values)
        void setConfig(eConfigUInt32Values index, uint32 value) { sGameConfig.setConfig(index, value); }
        /// Get a server configuration element (see #eConfigUInt32Values)
        uint32 getConfig(eConfigUInt32Values index) const { return sGameConfig.getConfig(index); }

        /// Set a server configuration element (see #eConfigInt32Values)
        void setConfig(eConfigInt32Values index, int32 value) { sGameConfig.setConfig(index, value); }
        /// Get a server configuration element (see #eConfigInt32Values)
        int32 getConfig(eConfigInt32Values index) const { return sGameConfig.getConfig(index); }

        /// Set a server configuration element (see #eConfigBoolValues)
        void setConfig(eConfigBoolValues index, bool value) { sGameConfig.setConfig(index, value); }
        /// Get a server configuration element (see #eConfigBoolValues)
        bool getConfig(eConfigBoolValues index) const { return sGameConfig.getConfig(index); }

        /// Get configuration about force-loaded maps
        bool isForceLoadMap(uint32 id) const { return m_configForceLoadMapIds.find(id) != m_configForceLoadMapIds.end(); }

        /// Are we on a "Player versus Player" server?
        bool IsPvPRealm() { return sGameConfig.IsPvPRealm(); }
        bool IsFFAPvPRealm() { return sGameConfig.IsFFAPvPRealm(); }

        void KickAll();
        void KickAllLess(AccountTypes sec);
        BanReturn BanAccount(BanMode mode, std::string nameOrIP, uint32 duration_secs, std::string reason, const std::string& author);
        bool RemoveBanAccount(BanMode mode, std::string nameOrIP);

        void ProcessCliCommands();
        void QueueCliCommand(CliCommandHolder* commandHolder) { std::lock_guard<std::mutex> guard(m_cliCommandQueueLock); m_cliCommandQueue.push_back(commandHolder); }

        void UpdateResultQueue();
        void InitResultQueue();

        void UpdateRealmCharCount(uint32 accid);

        LocaleConstant GetAvailableDbcLocale(LocaleConstant locale) const { return sGameConfig.GetAvailableDbcLocale(locale); }

        // used World DB version
        void LoadDBVersion();
        char const* GetDBVersion() { return m_DBVersion.c_str(); }

        void LoadBroadcastStrings();

        /**
        * \brief: force all client to request player data
        * \param: ObjectGuid guid : guid of the specified player
        * \returns: void
        *
        * Description: InvalidatePlayerDataToAllClient force all connected clients to clear specified player cache
        * FullName: World::InvalidatePlayerDataToAllClient
        * Access: public
        **/
        void InvalidatePlayerDataToAllClient(ObjectGuid guid);

    protected:
        void _UpdateGameTime();
        // callback for UpdateRealmCharacters
        void _UpdateRealmCharCount(std::optional<uint32> const& charCount, uint32 accountId);

        void InitCurrencyResetTime();
        void InitDailyQuestResetTime();
        void InitRandomBGResetTime();
        void InitWeeklyQuestResetTime();
        void SetMonthlyQuestResetTime(bool initialize = true);
        void ResetCurrencyWeekCounts();
        void ResetDailyQuests();
        void ResetWeeklyQuests();
        void ResetMonthlyQuests();

    private:
        void setConfig(eConfigUInt64Values index, char const* fieldname, uint64 defvalue);
        void setConfig(eConfigInt64Values index, char const* fieldname, int64 defvalue);
        void setConfig(eConfigUInt32Values index, char const* fieldname, uint32 defvalue);
        void setConfig(eConfigInt32Values index, char const* fieldname, int32 defvalue);
        void setConfig(eConfigFloatValues index, char const* fieldname, float defvalue);
        void setConfig(eConfigBoolValues index, char const* fieldname, bool defvalue);
        void setConfigPos(eConfigFloatValues index, char const* fieldname, float defvalue);
        void setConfigMin(eConfigUInt64Values index, char const* fieldname, uint64 defvalue, uint64 minvalue);
        void setConfigMin(eConfigInt64Values index, char const* fieldname, int64 defvalue, int64 minvalue);
        void setConfigMin(eConfigUInt32Values index, char const* fieldname, uint32 defvalue, uint32 minvalue);
        void setConfigMin(eConfigInt32Values index, char const* fieldname, int32 defvalue, int32 minvalue);
        void setConfigMin(eConfigFloatValues index, char const* fieldname, float defvalue, float minvalue);
        void setConfigMinMax(eConfigUInt64Values index, char const* fieldname, uint64 defvalue, uint64 minvalue, uint64 maxvalue);
        void setConfigMinMax(eConfigInt64Values index, char const* fieldname, int64 defvalue, int64 minvalue, int64 maxvalue);
        void setConfigMinMax(eConfigUInt32Values index, char const* fieldname, uint32 defvalue, uint32 minvalue, uint32 maxvalue);
        void setConfigMinMax(eConfigInt32Values index, char const* fieldname, int32 defvalue, int32 minvalue, int32 maxvalue);
        void setConfigMinMax(eConfigFloatValues index, char const* fieldname, float defvalue, float minvalue, float maxvalue);
        bool configNoReload(bool reload, eConfigUInt32Values index, char const* fieldname, uint32 defvalue);
        bool configNoReload(bool reload, eConfigInt32Values index, char const* fieldname, int32 defvalue);
        bool configNoReload(bool reload, eConfigFloatValues index, char const* fieldname, float defvalue);
        bool configNoReload(bool reload, eConfigBoolValues index, char const* fieldname, bool defvalue);

        // AutoBroadcast system
        void AutoBroadcast();
        struct BroadcastString
        {
            uint32 freq;
            std::string text;
        };
        std::vector<BroadcastString> m_broadcastList;
        uint32 m_broadcastWeight;
        bool m_broadcastEnable;
        IntervalTimer m_broadcastTimer;

        static volatile bool m_stopEvent;
        static uint8 m_ExitCode;
        uint32 m_ShutdownTimer;
        uint32 m_ShutdownMask;

        time_t m_startTime;
        IntervalTimer m_timers[WUPDATE_COUNT];
        uint32 mail_timer;
        uint32 mail_timer_expires;

        SessionMap m_sessions;
        uint32 m_maxActiveSessionCount;
        uint32 m_maxQueuedSessionCount;

        int32 m_playerLimit;
        void DetectDBCLang();
        bool m_allowMovement;
        std::string m_motd;
        std::string m_dataPath;

        // CLI command holder to be thread safe
        std::mutex m_cliCommandQueueLock;
        std::deque<CliCommandHolder *> m_cliCommandQueue;

        // scheduled reset times
        time_t m_NextCurrencyReset;
        time_t m_NextDailyQuestReset;
        time_t m_NextRandomBGReset;
        time_t m_NextWeeklyQuestReset;
        time_t m_NextMonthlyQuestReset;

        // Player Queue
        Queue m_QueuedSessions;

        // sessions that are added async
        void AddSession_(WorldSession* s);

        std::mutex m_sessionAddQueueLock;
        std::deque<WorldSession *> m_sessionAddQueue;

        void ApplyAfterMapPhasePosts();
        AfterMapPhaseQueue m_afterMapPhase;

        // used versions
        std::string m_DBVersion;
        std::string m_CreatureEventAIVersion;

        // List of Maps that should be force-loaded on startup
        std::set<uint32> m_configForceLoadMapIds;
};

/// The next monthly quest reset after now: local midnight on the first of the following month (the
/// day's first moment where that midnight does not exist); with keepEarlier, an earlier stored reset is kept.
time_t NextMonthlyQuestReset(time_t now, time_t stored, bool keepEarlier);

/// A stored monthly reset that is not local midnight on the first of a month, moved to that midnight
/// (the next one after twelve hours before it); 0 and values already on it are kept.
time_t MonthlyQuestResetOnLocalMidnight(time_t stored);

#define sWorld MaNGOS::Singleton<World>::Instance()
#endif
/// @}
