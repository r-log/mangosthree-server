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
#include "session/SessionRegistry.h"
#include "data/WorldBroadcast.h"
#include "data/ServerControl.h"
#include "data/ResetSchedule.h"
#include "SocialMgr.h"

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
class World : public WorldBroadcast, public ServerControl
{
    public:
        static std::atomic<uint32> m_worldLoopCounter;

        World();
        ~World();

        void CleanupsBeforeStop();

        /// \todo Actions on m_allowMovement still to be implemented
        /// Is movement allowed?
        bool MovementAllowed() const override { return m_allowMovement; }
        /// Allow/Disallow object movements
        void SetMovementAllowed(bool allow) override { m_allowMovement = allow; }

        /// Set a new Message of the Day
        void SetMotd(std::string const& motd) override { m_motd = motd; }
        /// Get the current Message of the Day
        char const* Motd() const override { return m_motd.c_str(); }
        void showFooter();

        LocaleConstant GetDefaultDbcLocale() const { return sGameConfig.GetDefaultDbcLocale(); }

        /// Get the path where data (dbc, maps) are stored on disk

        /// When server started?
        time_t const& GetStartTime() const { return m_startTime; }
        /// Uptime (in secs)
        uint32 Uptime() const override { return uint32(WorldGameTime::Get() - m_startTime); }

        /// Get the maximum skill level a player can reach
        uint16 GetConfigMaxSkillValue() const { return sGameConfig.GetConfigMaxSkillValue(); }

        void SetInitialWorldSettings();
        void LoadConfigSettings(bool reload = false);
        void ReloadConfig() override { LoadConfigSettings(true); }

        void WorldTextV(int32 stringId, va_list* args) override;
        void SendServerMessage(ServerMessageType type, const char* text = "", Player* player = NULL);
        void ZoneUnderAttack(uint32 zoneId, Team team) override;
        void DefenseMessage(uint32 zoneId, int32 textId) override;
        bool PostAfterMapPhase(AfterMapPhasePost const& post) override;
        void RealmFirstAchievement(char const* name, ObjectGuid guid, uint32 achievementId) override;
        void WorldStateChanged(uint32 field, uint32 value) override;

        /// Are we in the middle of a shutdown?
        bool IsShuttingDown() const override { return m_ShutdownTimer > 0; }
        void Shutdown(uint32 delay, uint32 options, uint8 exitCode) override;
        void CancelShutdown() override;
        void ShutdownMessage(bool show = false, Player* player = NULL) override;

        void Update(uint32 diff);

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

        /// Are we on a "Player versus Player" server?
        bool IsPvPRealm() { return sGameConfig.IsPvPRealm(); }
        bool IsFFAPvPRealm() { return sGameConfig.IsFFAPvPRealm(); }

        BanReturn Ban(BanMode mode, std::string nameOrIP, uint32 durationSecs, std::string reason, std::string const& author) override;
        bool RemoveBan(BanMode mode, std::string nameOrIP) override;

        void ProcessCliCommands();
        void QueueCliCommand(CliCommandHolder* commandHolder) { std::lock_guard<std::mutex> guard(m_cliCommandQueueLock); m_cliCommandQueue.push_back(commandHolder); }

        void UpdateResultQueue();
        void InitResultQueue();


        LocaleConstant GetAvailableDbcLocale(LocaleConstant locale) const { return sGameConfig.GetAvailableDbcLocale(locale); }

        // used World DB version
        void LoadDBVersion();
        char const* DbVersion() const override { return m_DBVersion.c_str(); }

        void LoadBroadcastStrings() override;

    protected:
        void _UpdateGameTime();

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

        uint32 m_ShutdownTimer;
        uint32 m_ShutdownMask;

        time_t m_startTime;
        IntervalTimer m_timers[WUPDATE_COUNT];
        uint32 mail_timer;
        uint32 mail_timer_expires;

        SessionRegistry m_sessions;

        void DetectDBCLang();
        bool m_allowMovement;
        std::string m_motd;

        // CLI command holder to be thread safe
        std::mutex m_cliCommandQueueLock;
        std::deque<CliCommandHolder *> m_cliCommandQueue;

        // scheduled reset times
        ResetSchedule m_resets;

        SocialMgr m_social;


        void ApplyAfterMapPhasePosts();
        AfterMapPhaseQueue m_afterMapPhase;
        /// Runs the aura holder updates the map phase left for the world thread (OffMapAuraUpdates), each
        /// with the time its update was given, and removes a holder that expired in it.
        void ApplyOffMapAuraUpdates();

        // used versions
        std::string m_DBVersion;
        std::string m_CreatureEventAIVersion;

        // List of Maps that should be force-loaded on startup
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
