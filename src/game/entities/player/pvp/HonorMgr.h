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

#ifndef MANGOS_H_HONORMGR
#define MANGOS_H_HONORMGR

#include "Platform/Define.h"
#include "SharedDefines.h"                                  // Team
#include "DBCEnums.h"                                       // AchievementCriteriaTypes
#include "ObjectGuid.h"
#include "ManagerPacketSink.h"

#include <ctime>
#include <functional>

/**
 * @brief Decoupling D4k: a character's honor-kill bookkeeping and the rules that turn a kill into
 * honor, held apart from the object that plays the character.
 *
 * The state is the time the daily kill rollover last ran. The rules are the rollover itself
 * (today's kills move to yesterday's half of PLAYER_FIELD_KILLS once a UTC day has passed, or both
 * halves are cleared after a longer gap) and Reward's: the arena and inactive early-outs, the
 * victim checks (none, the owner itself, NO_PVP_CREDIT, the same team outside a free-for-all
 * realm, a grey victim, a creature that is not a racial leader), the honor of a kill from the two
 * levels, the victim's title rank, the rate, the honor-gain aura, the group split and the 80-120%
 * draw, and the bytes of SMSG_PVP_CREDIT.
 *
 * The object carries no owner. What it used to read from the owner and the victim is handed in at
 * the call as plain values (HonorInputs: OwnerFacts, VictimFacts, the realm's type and honor rate),
 * read by the owner just before the call; three reads stay callbacks, called where the old body
 * read them: the clock (the rollover reads it twice), today's kills (read inside the rollover), and
 * the draw (`urand(8, 12)`, which must consume a random number only where the old body did). What
 * it used to do to the owner goes out through callbacks called at the exact point the old body did
 * it (RewardSinks): the kill-field writes, the three achievement updates, the packet (a
 * ManagerPacketSink, the owner's session) and the honor currency change, last. Callbacks are
 * parameters only, never stored. So `mangos_tests` builds one from nothing.
 *
 * What stays with the owner: the victim's type and the casts that read its facts, the grey level
 * of the owner's level (MaNGOS::XP::GetGrayLevel, whose header reaches the world), the victim
 * eligibility filter shared with XP (isHonorOrXPTarget), and the honor balance itself (the honor
 * currency, CurrencyMgr).
 *
 * KEPT SEMANTICS, stated rather than fixed (backlog): the rollover's day is the UTC day, and it
 * reads the clock twice, so a midnight falling between the two reads rolls over twice; a kill adds
 * 1 to the whole PLAYER_FIELD_KILLS word as an int32 clamped at 0, so today's count at 65535 carries
 * into yesterday's half and a word whose yesterday half is 32768 or more is cleared; every early-out
 * after the arena and inactive checks returns false with the rollover already done.
 */
class HonorMgr
{
    public:
        /// Hands the built SMSG_PVP_CREDIT to the owner's session: `GetSession()->SendPacket(packet)`.
        typedef ManagerPacketSink PacketSink;
        /// The clock: `time(NULL)`.
        typedef std::function<time_t()> Clock;
        /// The honor draw: `urand(8, 12)`.
        typedef std::function<uint32()> Draw;
        /// Reads a half of one of the owner's update fields: `GetUInt16Value(index, offset)`.
        typedef std::function<uint16(uint16 index, uint8 offset)> FieldHalfRead;
        /// Writes a half of one of the owner's update fields: `SetUInt16Value(index, offset, value)`.
        typedef std::function<void(uint16 index, uint8 offset, uint16 value)> FieldHalfSink;
        /// Writes one of the owner's update fields: `SetUInt32Value(index, value)`.
        typedef std::function<void(uint16 index, uint32 value)> FieldSink;
        /// Adds to or takes from one of the owner's update fields: `ApplyModUInt32Value(index, val, apply)`.
        typedef std::function<void(uint16 index, int32 val, bool apply)> FieldModSink;
        /// The owner's `UpdateAchievementCriteria(type, miscValue1)`.
        typedef std::function<void(AchievementCriteriaTypes type, uint32 miscValue1)> AchievementUpdate;
        /// The owner's `ModifyCurrencyCount(currencyId, count)`, with its default flags.
        typedef std::function<void(uint32 currencyId, int32 count)> CurrencyChange;

        /// What the kill rollover and a kill do to the owner's kill fields, each called where the
        /// old body read or wrote the field. The rollover uses the first three; only Reward calls
        /// applyModUInt32Value, so the owner's UpdateHonorKills leaves it unset.
        struct KillFields
        {
            FieldHalfRead getUInt16Value;           ///< today's kills, read inside the rollover
            FieldHalfSink setUInt16Value;           ///< the rollover's two half writes
            FieldSink setUInt32Value;               ///< the rollover's clear after a longer gap
            FieldModSink applyModUInt32Value;       ///< a kill: today's kills and the lifetime kills
        };

        /// What Reward knows about the owner, read by the owner just before the call. The scalars
        /// default to false and 0 so that none is ever indeterminate; every builder (the owner's
        /// ReadHonorInputs, the test's rows) sets every field anyway.
        struct OwnerFacts
        {
            bool inArena = false;                   ///< InArena()
            Team bgTeam = TEAM_NONE;                ///< GetBGTeam()
            bool inactive = false;                  ///< GetDummyAura(SPELL_AURA_PLAYER_INACTIVE) is not NULL
            Team team = TEAM_NONE;                  ///< GetTeam()
            uint32 level = 0;                       ///< getLevel()
            uint32 grayLevel = 0;                   ///< MaNGOS::XP::GetGrayLevel(getLevel())
            int32 honorGainModifier = 0;            ///< GetMaxPositiveAuraModifier(SPELL_AURA_MOD_HONOR_GAIN)
        };

        /// What Reward knows about the victim, read by the owner just before the call. A fact is
        /// read only under the old body's type guards, by the victim's type (none of them without a
        /// victim, the character facts only for a character, the racial leader only for a
        /// creature), and stays at its default otherwise; they are read on more paths than before
        /// (the arena, inactive, owner-victim and given-honor paths), all pure reads.
        struct VictimFacts
        {
            bool present = false;                   ///< the victim is not NULL
            bool isOwner = false;                   ///< the victim is the owner itself
            bool isPlayer = false;                  ///< GetTypeId() == TYPEID_PLAYER
            bool noPvpCredit = false;               ///< HasAuraType(SPELL_AURA_NO_PVP_CREDIT)
            ObjectGuid guid;                        ///< GetObjectGuid()
            Team bgTeam = TEAM_NONE;                ///< a character victim's GetBGTeam()
            Team team = TEAM_NONE;                  ///< a character victim's GetTeam()
            uint32 level = 0;                       ///< a character victim's getLevel()
            uint8 classId = 0;                      ///< a character victim's getClass()
            uint8 race = 0;                         ///< a character victim's getRace()
            uint32 chosenTitle = 0;                 ///< a character victim's GetUInt32Value(PLAYER_CHOSEN_TITLE)
            bool racialLeader = false;              ///< a creature victim's IsRacialLeader()
        };

        /// What Reward reads. The two callbacks are called where the old body read them; the rest
        /// are values the owner reads just before the call.
        struct HonorInputs
        {
            Clock clock;                            ///< the rollover's clock, read twice
            OwnerFacts owner;
            VictimFacts victim;
            bool ffaRealm = false;                  ///< sWorld.IsFFAPvPRealm()
            float honorRate = 0.0f;                 ///< sWorld.getConfig(CONFIG_FLOAT_RATE_HONOR)
            Draw draw;                              ///< urand(8, 12), called only where the old body drew
        };

        /// What Reward does to the owner, each called at the old statement (and, in `fields`, the
        /// rollover's read of today's kills, kept with the writes it goes with).
        struct RewardSinks
        {
            KillFields fields;                      ///< the rollover and the kill counts
            AchievementUpdate updateAchievement;    ///< the three honorable-kill criteria
            PacketSink send;                        ///< SMSG_PVP_CREDIT
            CurrencyChange modifyCurrencyCount;     ///< the honor points, last
        };

        /**
         * @brief Constructs the manager with the time the rollover compares against.
         *
         * The owner passes `time(NULL)` in its constructor's initializer list, the point where the
         * old owner-bound constructor read the clock.
         *
         * @param lastKillUpdate The initial last-update timestamp.
         */
        explicit HonorMgr(time_t lastKillUpdate = 0) : m_lastUpdateTime(lastKillUpdate) {}

        /**
         * @brief Rolls today's kill counter over to yesterday once a calendar day has passed.
         *
         * Called from Reward before crediting a new kill, from SaveToDB to
         * keep the persisted counts honest, and indirectly on character load.
         *
         * @param clock  The clock, read twice as the old body did (`now`, then the day).
         * @param fields The owner's kill fields: today's kills read, the rollover's writes.
         */
        void UpdateKills(Clock const& clock, KillFields const& fields);

        /**
         * @brief Compute honor points for a kill and credit them to the owner.
         *
         * Sends an SMSG_PVP_CREDIT packet and adds honor to CURRENCY_HONOR_POINTS
         * when the kill is eligible. In arenas no honor flows but on-kill
         * spell procs are still allowed to fire.
         *
         * @param groupsize Number of group members to split the honor across; 1 means solo.
         * @param honor     Explicit honor value to award; pass a non-positive value to have it computed.
         * @param in        The owner's, the victim's and the realm's facts, the clock and the draw.
         * @param sinks     The kill fields, the achievement updates, the packet and the currency change.
         * @return True if honor was awarded or arena on-kill procs should fire; false otherwise.
         */
        bool Reward(uint32 groupsize, float honor, HonorInputs const& in, RewardSinks const& sinks);

        /**
         * @brief Reset the timestamp that the daily rollover compares against.
         *
         * Called from the owner's LoadFromDB with the saved logout time so a long
         * absence triggers the correct yesterday-shift on first save.
         *
         * @param t The new last-update timestamp.
         */
        void SetLastKillUpdate(time_t t) { m_lastUpdateTime = t; }

    private:
        time_t  m_lastUpdateTime;   ///< Timestamp the daily kill rollover compares against.
};

#endif // MANGOS_H_HONORMGR
