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

#ifndef MANGOS_H_GAMECONFIG
#define MANGOS_H_GAMECONFIG

#include "Platform/Define.h"
#include "Common/Locales.h"

#define MAX_VISIBILITY_DISTANCE     333.0f                  // max distance for visible object show, limited in 333 yards
#define DEFAULT_VISIBILITY_DISTANCE 90.0f                   // default visible distance, 90 yards on continents
#define DEFAULT_VISIBILITY_INSTANCE 120.0f                  // default visible distance in instances, 120 yards
#define DEFAULT_VISIBILITY_BGARENAS 180.0f                  // default visible distance in BG/Arenas, 180 yards

/// Configuration elements

// just placeholder for int64 values
enum eConfigInt64Values
{
    CONFIG_INT64_VALUE_COUNT = 1    // change this value after new constances added
};

enum eConfigUInt64Values
{
    CONFIG_UINT64_START_PLAYER_MONEY = 0,
    CONFIG_UINT64_VALUE_COUNT
};

enum eConfigUInt32Values
{
    CONFIG_UINT32_COMPRESSION = 0,
    CONFIG_UINT32_INTERVAL_SAVE,
    CONFIG_UINT32_INTERVAL_GRIDCLEAN,
    CONFIG_UINT32_INTERVAL_MAPUPDATE,
    CONFIG_UINT32_INTERVAL_CHANGEWEATHER,
    CONFIG_UINT32_PORT_WORLD,
    CONFIG_UINT32_GAME_TYPE,
    CONFIG_UINT32_REALM_ZONE,
    CONFIG_UINT32_STRICT_PLAYER_NAMES,
    CONFIG_UINT32_STRICT_CHARTER_NAMES,
    CONFIG_UINT32_STRICT_PET_NAMES,
    CONFIG_UINT32_MIN_PLAYER_NAME,
    CONFIG_UINT32_MIN_CHARTER_NAME,
    CONFIG_UINT32_MIN_PET_NAME,
    CONFIG_UINT32_CHARACTERS_CREATING_DISABLED,
    CONFIG_UINT32_CHARACTERS_PER_ACCOUNT,
    CONFIG_UINT32_CHARACTERS_PER_REALM,
    CONFIG_UINT32_HEROIC_CHARACTERS_PER_REALM,
    CONFIG_UINT32_MIN_LEVEL_FOR_HEROIC_CHARACTER_CREATING,
    CONFIG_UINT32_SKIP_CINEMATICS,
    CONFIG_UINT32_MAX_PLAYER_LEVEL,
    CONFIG_UINT32_START_PLAYER_LEVEL,
    CONFIG_UINT32_START_HEROIC_PLAYER_LEVEL,
    CONFIG_UINT32_START_PLAYER_MONEY,
    CONFIG_UINT32_MAX_HONOR_POINTS,
    CONFIG_UINT32_START_HONOR_POINTS,
    CONFIG_UINT32_MAX_ARENA_POINTS,
    CONFIG_UINT32_START_ARENA_POINTS,
    CONFIG_UINT32_CURRENCY_START_HONOR_POINTS,
    CONFIG_UINT32_CURRENCY_START_CONQUEST_POINTS,
    CONFIG_UINT32_CURRENCY_CONQUEST_POINTS_DEFAULT_WEEK_CAP,
    CONFIG_UINT32_CURRENCY_ARENA_CONQUEST_POINTS_REWARD,
    CONFIG_UINT32_CURRENCY_RESET_TIME_HOUR,
    CONFIG_UINT32_CURRENCY_RESET_INTERVAL,
    CONFIG_UINT32_CURRENCY_RESET_TIME_WEEK_DAY,
    CONFIG_UINT32_INSTANCE_RESET_TIME_HOUR,
    CONFIG_UINT32_INSTANCE_UNLOAD_DELAY,
    CONFIG_UINT32_MAX_SPELL_CASTS_IN_CHAIN,
    CONFIG_UINT32_BIRTHDAY_TIME,
    CONFIG_UINT32_RABBIT_DAY,
    CONFIG_UINT32_MAX_PRIMARY_TRADE_SKILL,
    CONFIG_UINT32_MIN_PETITION_SIGNS,
    CONFIG_UINT32_GM_LOGIN_STATE,
    CONFIG_UINT32_GM_VISIBLE_STATE,
    CONFIG_UINT32_GM_ACCEPT_TICKETS,
    CONFIG_UINT32_GM_TICKET_LIST_SIZE,
    CONFIG_UINT32_GM_CHAT,
    CONFIG_UINT32_GM_WISPERING_TO,
    CONFIG_UINT32_GM_LEVEL_IN_GM_LIST,
    CONFIG_UINT32_GM_LEVEL_IN_WHO_LIST,
    CONFIG_UINT32_START_GM_LEVEL,
    CONFIG_UINT32_GM_INVISIBLE_AURA,
    CONFIG_UINT32_GM_MAX_SPEED_FACTOR,
    CONFIG_UINT32_GROUP_VISIBILITY,
    CONFIG_UINT32_MAIL_DELIVERY_DELAY,
    CONFIG_UINT32_MASS_MAILER_SEND_PER_TICK,
    CONFIG_UINT32_UPTIME_UPDATE,
    CONFIG_UINT32_AUCTION_DEPOSIT_MIN,
    CONFIG_UINT32_SKILL_CHANCE_ORANGE,
    CONFIG_UINT32_SKILL_CHANCE_YELLOW,
    CONFIG_UINT32_SKILL_CHANCE_GREEN,
    CONFIG_UINT32_SKILL_CHANCE_GREY,
    CONFIG_UINT32_SKILL_CHANCE_MINING_STEPS,
    CONFIG_UINT32_SKILL_CHANCE_SKINNING_STEPS,
    CONFIG_UINT32_SKILL_GAIN_CRAFTING,
    CONFIG_UINT32_SKILL_GAIN_GATHERING,
    CONFIG_UINT32_MAX_OVERSPEED_PINGS,
    CONFIG_UINT32_EXPANSION,
    CONFIG_UINT32_CHATFLOOD_MESSAGE_COUNT,
    CONFIG_UINT32_CHATFLOOD_MESSAGE_DELAY,
    CONFIG_UINT32_CHATFLOOD_MUTE_TIME,
    CONFIG_UINT32_CREATURE_FAMILY_ASSISTANCE_DELAY,
    CONFIG_UINT32_CREATURE_FAMILY_FLEE_DELAY,
    CONFIG_UINT32_WORLD_BOSS_LEVEL_DIFF,
    CONFIG_UINT32_QUEST_DAILY_RESET_HOUR,
    CONFIG_UINT32_QUEST_WEEKLY_RESET_WEEK_DAY,
    CONFIG_UINT32_QUEST_WEEKLY_RESET_HOUR,

    CONFIG_UINT32_LIVINGWORLD_ANCHOR_MASK,
    CONFIG_UINT32_LIVINGWORLD_CELL_ENVELOPE_DOWNGRADE_DELAY,

    CONFIG_UINT32_CHAT_STRICT_LINK_CHECKING_SEVERITY,
    CONFIG_UINT32_CHAT_STRICT_LINK_CHECKING_KICK,
    CONFIG_UINT32_CORPSE_DECAY_NORMAL,
    CONFIG_UINT32_CORPSE_DECAY_RARE,
    CONFIG_UINT32_CORPSE_DECAY_ELITE,
    CONFIG_UINT32_CORPSE_DECAY_RAREELITE,
    CONFIG_UINT32_CORPSE_DECAY_WORLDBOSS,
    CONFIG_UINT32_INSTANT_LOGOUT,
    CONFIG_UINT32_BATTLEGROUND_INVITATION_TYPE,
    CONFIG_UINT32_BATTLEGROUND_PREMATURE_FINISH_TIMER,
    CONFIG_UINT32_BATTLEGROUND_PREMADE_GROUP_WAIT_FOR_MATCH,
    CONFIG_UINT32_BATTLEGROUND_QUEUE_ANNOUNCER_JOIN,
    CONFIG_UINT32_ARENA_MAX_RATING_DIFFERENCE,
    CONFIG_UINT32_ARENA_RATING_DISCARD_TIMER,
    CONFIG_UINT32_ARENA_AUTO_DISTRIBUTE_INTERVAL_DAYS,
    CONFIG_UINT32_ARENA_SEASON_ID,
    CONFIG_UINT32_ARENA_SEASON_PREVIOUS_ID,
    CONFIG_UINT32_CLIENTCACHE_VERSION,
    CONFIG_UNIT32_GUILD_PETITION_COST,
    CONFIG_UINT32_GUILD_EVENT_LOG_COUNT,
    CONFIG_UINT32_GUILD_BANK_EVENT_LOG_COUNT,
    CONGIG_UINT32_GUILD_UNDELETABLE_LEVEL,
    CONFIG_UINT32_TIMERBAR_FATIGUE_GMLEVEL,
    CONFIG_UINT32_TIMERBAR_FATIGUE_MAX,
    CONFIG_UINT32_TIMERBAR_BREATH_GMLEVEL,
    CONFIG_UINT32_TIMERBAR_BREATH_MAX,
    CONFIG_UINT32_TIMERBAR_FIRE_GMLEVEL,
    CONFIG_UINT32_TIMERBAR_FIRE_MAX,
    CONFIG_UINT32_MIN_LEVEL_STAT_SAVE,
    CONFIG_UINT32_CHARDELETE_KEEP_DAYS,
    CONFIG_UINT32_CHARDELETE_METHOD,
    CONFIG_UINT32_CHARDELETE_MIN_LEVEL,
    CONFIG_UINT32_NUMTHREADS,
    CONFIG_UINT32_GUID_RESERVE_SIZE_CREATURE,
    CONFIG_UINT32_GUID_RESERVE_SIZE_GAMEOBJECT,
    CONFIG_UINT32_MIN_LEVEL_FOR_RAID,
    CONFIG_UINT32_CREATURE_RESPAWN_AGGRO_DELAY,
    CONFIG_UINT32_RANDOM_BG_RESET_HOUR,
    CONFIG_UINT32_MAX_WHOLIST_RETURNS,
    CONFIG_UINT32_LOG_WHISPERS,

    CONFIG_UINT32_AUTOBROADCAST_INTERVAL,

    CONFIG_UINT32_CINEMATIC_FLYOVER_UPDATE_INTERVAL_MS,
    CONFIG_UINT32_CINEMATIC_FLYOVER_TIMEOUT_SEC,
    CONFIG_UINT32_CINEMATIC_FLYOVER_BODY_ENTRY,

    CONFIG_UINT32_LFG_DESERTER_MIN_REMAINING,
    CONFIG_UINT32_LFG_CALL_TO_ARMS_MIN_QUEUED,
    // Movement kernel (design v2 §6.2): the pending-change machine's timeout policy
    CONFIG_UINT32_MOVEMENT_HARNESS_BARE_MAP,
    CONFIG_UINT32_MOVEMENT_FOLLOW_HORIZON_MS,
    CONFIG_UINT32_VALUE_COUNT
};

/// Configuration elements
enum eConfigInt32Values
{
    CONFIG_INT32_DEATH_SICKNESS_LEVEL = 0,
    CONFIG_INT32_ARENA_STARTRATING,
    CONFIG_INT32_ARENA_STARTPERSONALRATING,
    CONFIG_INT32_QUEST_LOW_LEVEL_HIDE_DIFF,
    CONFIG_INT32_QUEST_HIGH_LEVEL_HIDE_DIFF,
    CONFIG_INT32_VALUE_COUNT
};

/// Server config
enum eConfigFloatValues
{
    CONFIG_FLOAT_RATE_HEALTH = 0,
    CONFIG_FLOAT_RATE_POWER_MANA,
    CONFIG_FLOAT_RATE_POWER_RAGE_INCOME,
    CONFIG_FLOAT_RATE_POWER_RAGE_LOSS,
    CONFIG_FLOAT_RATE_POWER_RUNICPOWER_INCOME,
    CONFIG_FLOAT_RATE_POWER_RUNICPOWER_LOSS,
    CONFIG_FLOAT_RATE_POWER_FOCUS,
    CONFIG_FLOAT_RATE_POWER_ENERGY,
    CONFIG_FLOAT_RATE_SKILL_DISCOVERY,
    CONFIG_FLOAT_RATE_DROP_ITEM_POOR,
    CONFIG_FLOAT_RATE_DROP_ITEM_NORMAL,
    CONFIG_FLOAT_RATE_DROP_ITEM_UNCOMMON,
    CONFIG_FLOAT_RATE_DROP_ITEM_RARE,
    CONFIG_FLOAT_RATE_DROP_ITEM_EPIC,
    CONFIG_FLOAT_RATE_DROP_ITEM_LEGENDARY,
    CONFIG_FLOAT_RATE_DROP_ITEM_ARTIFACT,
    CONFIG_FLOAT_RATE_DROP_ITEM_REFERENCED,
    CONFIG_FLOAT_RATE_DROP_ITEM_QUEST,
    CONFIG_FLOAT_RATE_DROP_MONEY,
    CONFIG_FLOAT_RATE_DROP_CURRENCY,
    CONFIG_FLOAT_RATE_DROP_CURRENCY_AMOUNT,
    CONFIG_FLOAT_RATE_PET_XP_KILL,
    CONFIG_FLOAT_RATE_XP_KILL,
    CONFIG_FLOAT_RATE_XP_QUEST,
    CONFIG_FLOAT_RATE_XP_EXPLORE,
    CONFIG_FLOAT_RATE_REPUTATION_GAIN,
    CONFIG_FLOAT_RATE_REPUTATION_LOWLEVEL_KILL,
    CONFIG_FLOAT_RATE_REPUTATION_LOWLEVEL_QUEST,
    CONFIG_FLOAT_RATE_CREATURE_NORMAL_HP,
    CONFIG_FLOAT_RATE_CREATURE_ELITE_ELITE_HP,
    CONFIG_FLOAT_RATE_CREATURE_ELITE_RAREELITE_HP,
    CONFIG_FLOAT_RATE_CREATURE_ELITE_WORLDBOSS_HP,
    CONFIG_FLOAT_RATE_CREATURE_ELITE_RARE_HP,
    CONFIG_FLOAT_RATE_CREATURE_NORMAL_DAMAGE,
    CONFIG_FLOAT_RATE_CREATURE_ELITE_ELITE_DAMAGE,
    CONFIG_FLOAT_RATE_CREATURE_ELITE_RAREELITE_DAMAGE,
    CONFIG_FLOAT_RATE_CREATURE_ELITE_WORLDBOSS_DAMAGE,
    CONFIG_FLOAT_RATE_CREATURE_ELITE_RARE_DAMAGE,
    CONFIG_FLOAT_RATE_CREATURE_NORMAL_SPELLDAMAGE,
    CONFIG_FLOAT_RATE_CREATURE_ELITE_ELITE_SPELLDAMAGE,
    CONFIG_FLOAT_RATE_CREATURE_ELITE_RAREELITE_SPELLDAMAGE,
    CONFIG_FLOAT_RATE_CREATURE_ELITE_WORLDBOSS_SPELLDAMAGE,
    CONFIG_FLOAT_RATE_CREATURE_ELITE_RARE_SPELLDAMAGE,
    CONFIG_FLOAT_RATE_CREATURE_AGGRO,
    CONFIG_FLOAT_RATE_REST_INGAME,
    CONFIG_FLOAT_RATE_REST_OFFLINE_IN_TAVERN_OR_CITY,
    CONFIG_FLOAT_RATE_REST_OFFLINE_IN_WILDERNESS,
    CONFIG_FLOAT_RATE_DAMAGE_FALL,
    CONFIG_FLOAT_RATE_AUCTION_TIME,
    CONFIG_FLOAT_RATE_AUCTION_DEPOSIT,
    CONFIG_FLOAT_RATE_AUCTION_CUT,
    CONFIG_FLOAT_RATE_HONOR,
    CONFIG_FLOAT_RATE_MINING_AMOUNT,
    CONFIG_FLOAT_RATE_MINING_NEXT,
    CONFIG_FLOAT_RATE_TALENT,
    CONFIG_FLOAT_RATE_CORPSE_DECAY_LOOTED,
    CONFIG_FLOAT_RATE_INSTANCE_RESET_TIME,
    CONFIG_FLOAT_RATE_TARGET_POS_RECALCULATION_RANGE,
    CONFIG_FLOAT_RATE_DURABILITY_LOSS_DAMAGE,
    CONFIG_FLOAT_RATE_DURABILITY_LOSS_PARRY,
    CONFIG_FLOAT_RATE_DURABILITY_LOSS_ABSORB,
    CONFIG_FLOAT_RATE_DURABILITY_LOSS_BLOCK,
    CONFIG_FLOAT_SIGHT_GUARDER,
    CONFIG_FLOAT_SIGHT_MONSTER,
    CONFIG_FLOAT_LISTEN_RANGE_SAY,
    CONFIG_FLOAT_LISTEN_RANGE_YELL,
    CONFIG_FLOAT_LISTEN_RANGE_TEXTEMOTE,
    CONFIG_FLOAT_CREATURE_FAMILY_FLEE_ASSISTANCE_RADIUS,
    CONFIG_FLOAT_CREATURE_FAMILY_ASSISTANCE_RADIUS,
    CONFIG_FLOAT_GROUP_XP_DISTANCE,
    CONFIG_FLOAT_THREAT_RADIUS,
    CONFIG_FLOAT_GHOST_RUN_SPEED_WORLD,
    CONFIG_FLOAT_GHOST_RUN_SPEED_BG,

    CONFIG_FLOAT_CINEMATIC_FLYOVER_VISIBILITY_DISTANCE,
    CONFIG_FLOAT_MOVEMENT_CONFUSE_RADIUS,
    CONFIG_FLOAT_MOVEMENT_TAXI_SPEED,
    CONFIG_FLOAT_VALUE_COUNT
};

/// Configuration elements
enum eConfigBoolValues
{
    CONFIG_BOOL_GRID_UNLOAD = 0,
    CONFIG_BOOL_SAVE_RESPAWN_TIME_IMMEDIATELY,
    CONFIG_BOOL_OFFHAND_CHECK_AT_TALENTS_RESET,
    CONFIG_BOOL_ALLOW_TWO_SIDE_ACCOUNTS,
    CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_CHAT,
    CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_CHANNEL,
    CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_GROUP,
    CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_GUILD,
    CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_AUCTION,
    CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_MAIL,
    CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_CALENDAR,
    CONFIG_BOOL_ALLOW_TWO_SIDE_WHO_LIST,
    CONFIG_BOOL_ALLOW_TWO_SIDE_ADD_FRIEND,
    CONFIG_BOOL_INSTANCE_IGNORE_LEVEL,
    CONFIG_BOOL_INSTANCE_IGNORE_RAID,
    CONFIG_BOOL_CAST_UNSTUCK,
    CONFIG_BOOL_GM_LOG_TRADE,
    CONFIG_BOOL_GM_LOWER_SECURITY,
    CONFIG_BOOL_GM_ALLOW_ACHIEVEMENT_GAINS,
    CONFIG_BOOL_GM_TICKET_OFFLINE_CLOSING,
    CONFIG_BOOL_SKILL_PROSPECTING,
    CONFIG_BOOL_WEATHER,
    CONFIG_BOOL_EVENT_ANNOUNCE,
    CONFIG_BOOL_QUEST_IGNORE_RAID,
    CONFIG_BOOL_LIVINGWORLD_CELL_ENVELOPE_LOAD,
    CONFIG_BOOL_DETECT_POS_COLLISION,
    CONFIG_BOOL_RESTRICTED_LFG_CHANNEL,
    CONFIG_BOOL_SILENTLY_GM_JOIN_TO_CHANNEL,
    CONFIG_BOOL_TALENTS_INSPECTING,
    CONFIG_BOOL_CHAT_FAKE_MESSAGE_PREVENTING,
    CONFIG_BOOL_CHAT_STRICT_LINK_CHECKING_SEVERITY,
    CONFIG_BOOL_CHAT_STRICT_LINK_CHECKING_KICK,
    CONFIG_BOOL_ADDON_CHANNEL,
    CONFIG_BOOL_CORPSE_EMPTY_LOOT_SHOW,
    CONFIG_BOOL_DEATH_CORPSE_RECLAIM_DELAY_PVP,
    CONFIG_BOOL_DEATH_CORPSE_RECLAIM_DELAY_PVE,
    CONFIG_BOOL_DEATH_BONES_WORLD,
    CONFIG_BOOL_DEATH_BONES_BG_OR_ARENA,
    CONFIG_BOOL_ALL_TAXI_PATHS,
    CONFIG_BOOL_DECLINED_NAMES_USED,
    CONFIG_BOOL_SKILL_MILLING,
    CONFIG_BOOL_SKILL_FAIL_LOOT_FISHING,
    CONFIG_BOOL_SKILL_FAIL_GAIN_FISHING,
    CONFIG_BOOL_SKILL_FAIL_POSSIBLE_FISHINGPOOL,
    CONFIG_BOOL_BATTLEGROUND_CAST_DESERTER,
    CONFIG_BOOL_BATTLEGROUND_QUEUE_ANNOUNCER_START,
    CONFIG_BOOL_BATTLEGROUND_SCORE_STATISTICS,
    CONFIG_BOOL_ARENA_AUTO_DISTRIBUTE_POINTS,
    CONFIG_BOOL_ARENA_QUEUE_ANNOUNCER_JOIN,
    CONFIG_BOOL_ARENA_QUEUE_ANNOUNCER_EXIT,
    CONFIG_BOOL_OUTDOORPVP_SI_ENABLED,
    CONFIG_BOOL_OUTDOORPVP_EP_ENABLED,
    CONFIG_BOOL_OUTDOORPVP_HP_ENABLED,
    CONFIG_BOOL_OUTDOORPVP_ZM_ENABLED,
    CONFIG_BOOL_OUTDOORPVP_TF_ENABLED,
    CONFIG_BOOL_OUTDOORPVP_NA_ENABLED,
    CONFIG_BOOL_OUTDOORPVP_GH_ENABLED,
    CONFIG_BOOL_KICK_PLAYER_ON_BAD_PACKET,
    CONFIG_BOOL_STATS_SAVE_ONLY_ON_LOGOUT,
    CONFIG_BOOL_CLEAN_CHARACTER_DB,
    CONFIG_BOOL_VMAP_INDOOR_CHECK,
    CONFIG_BOOL_PET_UNSUMMON_AT_MOUNT,
    CONFIG_BOOL_MMAP_ENABLED,
    CONFIG_BOOL_PLAYER_COMMANDS,
    CONFIG_BOOL_GUILD_LEVELING_ENABLED,
    CONFIG_BOOL_ENABLE_QUEST_TRACKER,

    // Recommended Or New Flag
    CONFIG_BOOL_REALM_RECOMMENDED_OR_NEW_ENABLED,
    CONFIG_BOOL_REALM_RECOMMENDED_OR_NEW,

    // Cinematic flyover
    CONFIG_BOOL_CINEMATIC_FLYOVER_ENABLE,
    CONFIG_BOOL_CINEMATIC_FLYOVER_DEBUG,

    // LFG (Dungeon Finder)
    CONFIG_BOOL_LFG_ENABLE,
    CONFIG_BOOL_LFG_DESERTER_ON_VOTE_KICK,
    CONFIG_BOOL_LFG_CALL_TO_ARMS,

    // Movement arbiter
    CONFIG_BOOL_MOVEMENT_DECISION_RING,
    CONFIG_BOOL_MOVEMENT_WIRE_PARITY,
    CONFIG_BOOL_MOVEMENT_SMOOTH_GROUND_PATHS,
    CONFIG_BOOL_VALUE_COUNT
};

/// Type of server, this is values from second column of Cfg_Configs.dbc (1.12.1 have another numeration)
enum RealmType
{
    REALM_TYPE_NORMAL   = 0,
    REALM_TYPE_PVP      = 1,
    REALM_TYPE_NORMAL2  = 4,
    REALM_TYPE_RP       = 6,
    REALM_TYPE_RPPVP    = 8,
    REALM_TYPE_FFA_PVP  = 16                                // custom, free for all pvp mode like arena PvP in all zones except rest activated places and sanctuaries
    // replaced by REALM_PVP in realm list
};

/// This is values from first column of Cfg_Categories.dbc (1.12.1 have another numeration)
enum RealmZone
{
    REALM_ZONE_UNKNOWN       = 0,                           // any language
    REALM_ZONE_DEVELOPMENT   = 1,                           // any language
    REALM_ZONE_UNITED_STATES = 2,                           // extended-Latin
    REALM_ZONE_OCEANIC       = 3,                           // extended-Latin
    REALM_ZONE_LATIN_AMERICA = 4,                           // extended-Latin
    REALM_ZONE_TOURNAMENT_5  = 5,                           // basic-Latin at create, any at login
    REALM_ZONE_KOREA         = 6,                           // East-Asian
    REALM_ZONE_TOURNAMENT_7  = 7,                           // basic-Latin at create, any at login
    REALM_ZONE_ENGLISH       = 8,                           // extended-Latin
    REALM_ZONE_GERMAN        = 9,                           // extended-Latin
    REALM_ZONE_FRENCH        = 10,                          // extended-Latin
    REALM_ZONE_SPANISH       = 11,                          // extended-Latin
    REALM_ZONE_RUSSIAN       = 12,                          // Cyrillic
    REALM_ZONE_TOURNAMENT_13 = 13,                          // basic-Latin at create, any at login
    REALM_ZONE_TAIWAN        = 14,                          // East-Asian
    REALM_ZONE_TOURNAMENT_15 = 15,                          // basic-Latin at create, any at login
    REALM_ZONE_CHINA         = 16,                          // East-Asian
    REALM_ZONE_CN1           = 17,                          // basic-Latin at create, any at login
    REALM_ZONE_CN2           = 18,                          // basic-Latin at create, any at login
    REALM_ZONE_CN3           = 19,                          // basic-Latin at create, any at login
    REALM_ZONE_CN4           = 20,                          // basic-Latin at create, any at login
    REALM_ZONE_CN5           = 21,                          // basic-Latin at create, any at login
    REALM_ZONE_CN6           = 22,                          // basic-Latin at create, any at login
    REALM_ZONE_CN7           = 23,                          // basic-Latin at create, any at login
    REALM_ZONE_CN8           = 24,                          // basic-Latin at create, any at login
    REALM_ZONE_TOURNAMENT_25 = 25,                          // basic-Latin at create, any at login
    REALM_ZONE_TEST_SERVER   = 26,                          // any language
    REALM_ZONE_TOURNAMENT_27 = 27,                          // basic-Latin at create, any at login
    REALM_ZONE_QA_SERVER     = 28,                          // any language
    REALM_ZONE_CN9           = 29,                          // basic-Latin at create, any at login
    REALM_ZONE_TEST_SERVER_2 = 30,                          // any language
    // in 3.x
    REALM_ZONE_CN10          = 31,                          // basic-Latin at create, any at login
    REALM_ZONE_CTC           = 32,
    REALM_ZONE_CNC           = 33,
    REALM_ZONE_CN1_4         = 34,                          // basic-Latin at create, any at login
    REALM_ZONE_CN2_6_9       = 35,                          // basic-Latin at create, any at login
    REALM_ZONE_CN3_7         = 36,                          // basic-Latin at create, any at login
    REALM_ZONE_CN5_8         = 37                           // basic-Latin at create, any at login
};

/// The server configuration values read from mangosd.conf. World loads them; everything else reads them.
class GameConfig
{
    public:
        float getConfig(eConfigFloatValues rate) const { return m_configFloatValues[rate]; }
        uint64 getConfig(eConfigUInt64Values index) const { return m_configUint64Values[index]; }
        int64 getConfig(eConfigInt64Values index) const { return m_configInt64Values[index]; }
        uint32 getConfig(eConfigUInt32Values index) const { return m_configUint32Values[index]; }
        int32 getConfig(eConfigInt32Values index) const { return m_configInt32Values[index]; }
        bool getConfig(eConfigBoolValues index) const { return m_configBoolValues[index]; }

        /// Are we on a "Player versus Player" server?
        bool IsPvPRealm() const { return (getConfig(CONFIG_UINT32_GAME_TYPE) == REALM_TYPE_PVP || getConfig(CONFIG_UINT32_GAME_TYPE) == REALM_TYPE_RPPVP || getConfig(CONFIG_UINT32_GAME_TYPE) == REALM_TYPE_FFA_PVP); }
        bool IsFFAPvPRealm() const { return getConfig(CONFIG_UINT32_GAME_TYPE) == REALM_TYPE_FFA_PVP; }

        /// Get the maximum skill level a player can reach
        uint16 GetConfigMaxSkillValue() const
        {
            uint32 lvl = getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL);
            return lvl > 60 ? 300 + ((lvl - 60) * 75) / 10 : lvl * 5;
        }

        /// The locale DBC strings are read in
        LocaleConstant GetDefaultDbcLocale() const { return m_defaultDbcLocale; }
        /// The given locale if its DBC strings are loaded, else the default one
        LocaleConstant GetAvailableDbcLocale(LocaleConstant locale) const { if (m_availableDbcLocaleMask & (1 << locale)) { return locale; } else { return m_defaultDbcLocale; } }

        // for max speed access
        static float GetMaxVisibleDistanceOnContinents()    { return m_MaxVisibleDistanceOnContinents; }
        static float GetMaxVisibleDistanceInInstances()     { return m_MaxVisibleDistanceInInstances;  }
        static float GetMaxVisibleDistanceInBGArenas()      { return m_MaxVisibleDistanceInBGArenas;   }

        static float GetMaxVisibleDistanceInFlight()        { return m_MaxVisibleDistanceInFlight;    }
        static float GetVisibleUnitGreyDistance()           { return m_VisibleUnitGreyDistance;       }
        static float GetVisibleObjectGreyDistance()         { return m_VisibleObjectGreyDistance;     }

        static float GetRelocationLowerLimitSq()            { return m_relocation_lower_limit_sq; }
        static uint32 GetRelocationAINotifyDelay()          { return m_relocation_ai_notify_delay; }

        static bool   GetVisibilityObserverSweepEnabled()   { return m_visibility_observer_sweep_enabled; }
        static uint32 GetVisibilityObserverSweepInterval()  { return m_visibility_observer_sweep_interval; }

    private:
        friend class World;

        void setConfig(eConfigFloatValues index, float value) { m_configFloatValues[index] = value; }
        void setConfig(eConfigUInt64Values index, uint64 value) { m_configUint64Values[index] = value; }
        void setConfig(eConfigInt64Values index, int64 value) { m_configInt64Values[index] = value; }
        void setConfig(eConfigUInt32Values index, uint32 value) { m_configUint32Values[index] = value; }
        void setConfig(eConfigInt32Values index, int32 value) { m_configInt32Values[index] = value; }
        void setConfig(eConfigBoolValues index, bool value) { m_configBoolValues[index] = value; }

        void SetDefaultDbcLocale(LocaleConstant locale) { m_defaultDbcLocale = locale; }
        void SetAvailableDbcLocaleMask(uint32 mask) { m_availableDbcLocaleMask = mask; }

        /// Zeroes the uint32, int32, float and bool values.
        void Reset()
        {
            for (int i = 0; i < CONFIG_UINT32_VALUE_COUNT; ++i)
            {
                m_configUint32Values[i] = 0;
            }

            for (int i = 0; i < CONFIG_INT32_VALUE_COUNT; ++i)
            {
                m_configInt32Values[i] = 0;
            }

            for (int i = 0; i < CONFIG_FLOAT_VALUE_COUNT; ++i)
            {
                m_configFloatValues[i] = 0.0f;
            }

            for (int i = 0; i < CONFIG_BOOL_VALUE_COUNT; ++i)
            {
                m_configBoolValues[i] = false;
            }
        }

        uint64 m_configUint64Values[CONFIG_UINT64_VALUE_COUNT];
        int64 m_configInt64Values[CONFIG_INT64_VALUE_COUNT];
        uint32 m_configUint32Values[CONFIG_UINT32_VALUE_COUNT];
        int32 m_configInt32Values[CONFIG_INT32_VALUE_COUNT];
        float m_configFloatValues[CONFIG_FLOAT_VALUE_COUNT];
        bool m_configBoolValues[CONFIG_BOOL_VALUE_COUNT];
        LocaleConstant m_defaultDbcLocale;                  // from config for one from loaded DBC locales
        uint32 m_availableDbcLocaleMask;                    // by loaded DBC

        // for max speed access
        static float m_MaxVisibleDistanceOnContinents;
        static float m_MaxVisibleDistanceInInstances;
        static float m_MaxVisibleDistanceInBGArenas;

        static float m_MaxVisibleDistanceInFlight;
        static float m_VisibleUnitGreyDistance;
        static float m_VisibleObjectGreyDistance;

        static float  m_relocation_lower_limit_sq;
        static uint32 m_relocation_ai_notify_delay;

        static bool   m_visibility_observer_sweep_enabled;
        static uint32 m_visibility_observer_sweep_interval;
};

extern GameConfig sGameConfig;

extern uint32 realmID;

#endif
