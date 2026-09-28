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

/// Decoupling D4k: a character's honor -- the daily kill rollover and a kill's honor -- with no
/// character and no victim.
///
/// Before this PR HonorMgr held a pointer to its owner and read the owner's arena state, BG team,
/// inactive aura, team, level and honor-gain aura, the victim through a Unit pointer (its type,
/// team, level, class, race, title, NO_PVP_CREDIT aura, racial-leader flag), the world's realm type
/// and honor rate, the clock and the random draw, and wrote the owner's kill fields, achievements,
/// session and honor currency. Now the facts are values (HonorInputs: OwnerFacts, VictimFacts, the
/// realm), the clock, today's kills and the draw are read callbacks, and the writes are callbacks
/// (RewardSinks), so every case builds one from nothing. A Wire records, in call order, every read
/// callback's call and every write callback's arguments.
///
/// The kill fields are modelled as the object holds them: PLAYER_FIELD_KILLS is one 32-bit word,
/// today's kills in the low half (offset 0) and yesterday's in the high half (offset 1), written the
/// way Object::SetUInt16Value / SetUInt32Value / ApplyModUInt32Value write it (ObjectValues.cpp:
/// the half shifted by offset * 16; the whole word; the whole word as an int32 plus the change,
/// clamped at 0) and read the way GetUInt16Value reads it on a little-endian target.
///
/// Every expected honor value is the old arithmetic (HonorMgr.cpp at 9fb598e4e) done by hand in
/// float32, derived in the row's comment; an independent float32 model (each operation rounded to
/// float32) agreed with each. The packet is recorded as it goes on the wire: opcode, then the bytes
/// (uint32 honor, the victim's guid as 8 raw bytes, uint32 rank; little-endian).

#include "TestHarness.h"
#include "HonorMgr.h"
#include "Object/UpdateFields.h"
#include "Common/TimeConstants.h"
#include "ObjectGuid.h"
#include "Opcodes.h"
#include "WorldPacket.h"

#include <string>
#include <vector>

namespace
{
    // The opcode, the criteria types, the currency and the fields the rows expect, pinned to their
    // values: a row that compares against a constant from the same header would not see that
    // header change.
    static_assert(SMSG_PVP_CREDIT == 0x6015, "SMSG_PVP_CREDIT is 0x6015 in 4.3.4 15595");
    static_assert(ACHIEVEMENT_CRITERIA_TYPE_EARN_HONORABLE_KILL == 113, "the honorable-kill criteria type is 113");
    static_assert(ACHIEVEMENT_CRITERIA_TYPE_HK_CLASS == 52 && ACHIEVEMENT_CRITERIA_TYPE_HK_RACE == 53, "the class and race kill criteria");
    static_assert(CURRENCY_HONOR_POINTS == 392, "honor points are currency 392");
    static_assert(DAY == 86400, "a day is 86400 seconds");
    static_assert(PLAYER_FIELD_LIFETIME_HONORABLE_KILLS == PLAYER_FIELD_KILLS + 1, "the two kill fields are neighbours");

    /// Midnight UTC of day 20717 (2026-09-21): the rollover's "today".
    const time_t kToday = time_t(20717) * DAY;
    const time_t kYesterday = kToday - DAY;

    /// The victim's guid: every byte distinct, so the packet shows where each one lands.
    const uint64 kVictimGuid = 0x0102030405060708ULL;
    const uint8 kRogue = 4;
    const uint8 kOrc = 2;

    std::string FieldName(uint16 index)
    {
        if (index == PLAYER_FIELD_KILLS)
        {
            return "KILLS";
        }
        if (index == PLAYER_FIELD_LIFETIME_HONORABLE_KILLS)
        {
            return "LIFETIME";
        }
        return "field" + std::to_string(index);
    }

    /// The packet as it goes on the wire: the opcode and the bytes in hex.
    std::string PacketText(WorldPacket const& packet)
    {
        static const char* digits = "0123456789abcdef";
        std::string text;
        uint16 opcode = packet.GetOpcode();
        for (int shift = 12; shift >= 0; shift -= 4)
        {
            text += digits[(opcode >> shift) & 0x0F];
        }
        text += ":";
        text += testing::BytesToHex(packet.contents(), packet.size());
        return text;
    }

    /// The recorded event of SMSG_PVP_CREDIT carrying (honor, guid, rank): three little-endian values.
    std::string Credit(uint32 honor, uint64 guid, uint32 rank)
    {
        uint8 bytes[16];
        for (int i = 0; i < 4; ++i)
        {
            bytes[i] = uint8(honor >> (8 * i));
            bytes[12 + i] = uint8(rank >> (8 * i));
        }
        for (int i = 0; i < 8; ++i)
        {
            bytes[4 + i] = uint8(guid >> (8 * i));
        }
        return "packet 6015:" + testing::BytesToHex(bytes, 16);
    }

    /// What the callbacks received, in call order, and the two kill fields as the object holds
    /// them. The clock answers `clockReads` in order (the last one again if it runs out: the
    /// events show how many were taken); the draw answers `draw`.
    struct Wire
    {
        std::vector<time_t> clockReads;
        size_t clockNext = 0;
        uint32 draw = 10;
        uint32 kills = 0;                   ///< PLAYER_FIELD_KILLS: yesterday << 16 | today
        uint32 lifetime = 0;                ///< PLAYER_FIELD_LIFETIME_HONORABLE_KILLS
        std::vector<std::string> events;

        uint32& Field(uint16 index)
        {
            return index == PLAYER_FIELD_LIFETIME_HONORABLE_KILLS ? lifetime : kills;
        }

        HonorMgr::Clock Clock()
        {
            return [this]()
            {
                events.push_back("clock");
                time_t t = 0;
                if (!clockReads.empty())
                {
                    t = clockReads[clockNext < clockReads.size() ? clockNext : clockReads.size() - 1];
                }
                ++clockNext;
                return t;
            };
        }

        HonorMgr::KillFields Fields()
        {
            HonorMgr::KillFields fields;
            fields.getUInt16Value = [this](uint16 index, uint8 offset)
            {
                events.push_back("get16 " + FieldName(index) + " " + std::to_string(offset));
                return uint16(Field(index) >> (offset * 16));
            };
            fields.setUInt16Value = [this](uint16 index, uint8 offset, uint16 value)
            {
                events.push_back("set16 " + FieldName(index) + " " + std::to_string(offset) + " " + std::to_string(value));
                uint32& word = Field(index);
                word &= ~uint32(uint32(0xFFFF) << (offset * 16));
                word |= uint32(uint32(value) << (offset * 16));
            };
            fields.setUInt32Value = [this](uint16 index, uint32 value)
            {
                events.push_back("set32 " + FieldName(index) + " " + std::to_string(value));
                Field(index) = value;
            };
            fields.applyModUInt32Value = [this](uint16 index, int32 val, bool apply)
            {
                events.push_back("mod " + FieldName(index) + " " + std::to_string(val) + " " + (apply ? "1" : "0"));
                int32 cur = int32(Field(index));
                cur += (apply ? val : -val);
                if (cur < 0)
                {
                    cur = 0;
                }
                Field(index) = uint32(cur);
            };
            return fields;
        }

        HonorMgr::RewardSinks Sinks()
        {
            HonorMgr::RewardSinks sinks;
            sinks.fields = Fields();
            sinks.updateAchievement = [this](AchievementCriteriaTypes type, uint32 miscValue1)
            {
                events.push_back("ach " + std::to_string(uint32(type)) + " " + std::to_string(miscValue1));
            };
            sinks.send = [this](WorldPacket const* packet)
            {
                events.push_back("packet " + PacketText(*packet));
            };
            sinks.modifyCurrencyCount = [this](uint32 currencyId, int32 count)
            {
                events.push_back("currency " + std::to_string(currencyId) + " " + std::to_string(count));
            };
            return sinks;
        }

        /// A rewarded character kill at level 85 on 85, Alliance on Horde, in the open world, rate 1,
        /// no honor-gain aura, victim title 0.
        HonorMgr::HonorInputs Kill()
        {
            HonorMgr::HonorInputs in;
            in.clock = Clock();
            in.owner.inArena = false;
            in.owner.bgTeam = ALLIANCE;
            in.owner.inactive = false;
            in.owner.team = ALLIANCE;
            in.owner.level = 85;
            in.owner.grayLevel = 76;                        // GetGrayLevel(85) = 85 - 9
            in.owner.honorGainModifier = 0;
            in.victim.present = true;
            in.victim.isOwner = false;
            in.victim.isPlayer = true;
            in.victim.noPvpCredit = false;
            in.victim.guid = ObjectGuid(kVictimGuid);
            in.victim.bgTeam = HORDE;
            in.victim.team = HORDE;
            in.victim.level = 85;
            in.victim.classId = kRogue;
            in.victim.race = kOrc;
            in.victim.chosenTitle = 0;
            in.victim.racialLeader = false;
            in.ffaRealm = false;
            in.honorRate = 1.0f;
            in.draw = [this]()
            {
                events.push_back("draw");
                return draw;
            };
            return in;
        }
    };

    std::string Join(std::vector<std::string> const& list)
    {
        std::string text;
        for (size_t i = 0; i < list.size(); ++i)
        {
            text += (i ? "; " : "") + list[i];
        }
        return text;
    }

    void CheckEvents(std::vector<std::string> const& got, std::vector<std::string> const& want, int line)
    {
        if (got != want)
        {
            testing::ReportFailure(__FILE__, line, "events differ: got [" + Join(got) + "] want [" + Join(want) + "]");
        }
    }

    /// The events of a rewarded character kill with no rollover due: the clock's two reads, the two
    /// kill counts, the three criteria (the default victim is a rogue orc), the draw, the packet,
    /// the currency.
    std::vector<std::string> KillEvents(uint32 honor, uint64 guid, uint32 rank)
    {
        return { "clock", "clock", "mod KILLS 1 1", "mod LIFETIME 1 1", "ach 113 0", "ach 52 4", "ach 53 2", "draw",
                 Credit(honor, guid, rank), "currency 392 " + std::to_string(honor) };
    }

    /// Runs Reward on a manager last updated during kToday, with the clock on kToday: no rollover.
    bool Run(Wire& w, HonorMgr::HonorInputs const& in, uint32 groupsize = 1, float honor = -1)
    {
        HonorMgr mgr(kToday + 100);
        w.clockReads = { kToday + 200, kToday + 200 };
        return mgr.Reward(groupsize, honor, in, w.Sinks());
    }

    /// Runs Reward on a manager last updated one second before kToday, with the clock on kToday:
    /// the rollover is due.
    bool RunWithRollover(Wire& w, HonorMgr::HonorInputs const& in, uint32 groupsize = 1, float honor = -1)
    {
        HonorMgr mgr(kToday - 1);
        w.clockReads = { kToday + 5, kToday + 5 };
        return mgr.Reward(groupsize, honor, in, w.Sinks());
    }
}

// ---------------------------------------------------------------------------------------------
// The kill rollover.
//
// Old body (HonorMgr.cpp:37-64): now = first read; today = (second read / DAY) * DAY; if the last
// update is before today, read today's kills and either move them to yesterday's half (the last
// update was on or after yesterday's midnight) or clear the whole word (older); then last = now.

TEST(HonorMgr_RolloverTable)
{
    struct Row
    {
        char const* what;
        time_t last;
        time_t read1;
        time_t read2;
        uint32 killsBefore;                 ///< yesterday << 16 | today
        std::vector<std::string> events;
        uint32 killsAfter;
    };
    const Row rows[] =
    {
        // Same day: no field is read or written.
        { "same day", kToday + 100, kToday + 200, kToday + 200, 0x00050003, { "clock", "clock" }, 0x00050003 },
        // last == today's midnight is not before today (the '<'): no rollover.
        { "last at today's midnight", kToday, kToday + 1, kToday + 1, 0x00050003, { "clock", "clock" }, 0x00050003 },
        // One second before today's midnight is yesterday: today's 3 kills move to yesterday, today is 0.
        { "last one second before midnight", kToday - 1, kToday + 1, kToday + 1, 0x00050003,
          { "clock", "clock", "get16 KILLS 0", "set16 KILLS 0 0", "set16 KILLS 1 3" }, 0x00030000 },
        // last == yesterday's midnight is on or after yesterday (the '>='): the move, not the clear.
        { "last at yesterday's midnight", kYesterday, kToday + 1, kToday + 1, 0x00050003,
          { "clock", "clock", "get16 KILLS 0", "set16 KILLS 0 0", "set16 KILLS 1 3" }, 0x00030000 },
        // One second earlier is older than yesterday: the whole word is cleared.
        { "last one second before yesterday", kYesterday - 1, kToday + 1, kToday + 1, 0x00050003,
          { "clock", "clock", "get16 KILLS 0", "set32 KILLS 0" }, 0 },
        // Never updated (0): cleared.
        { "never updated", 0, kToday + 1, kToday + 1, 0x00050003,
          { "clock", "clock", "get16 KILLS 0", "set32 KILLS 0" }, 0 },
        // Today's half at its maximum moves whole; yesterday's old value is replaced.
        { "full today half", kToday - 1, kToday + 1, kToday + 1, 0x1234FFFF,
          { "clock", "clock", "get16 KILLS 0", "set16 KILLS 0 0", "set16 KILLS 1 65535" }, 0xFFFF0000 },
        // The day is the SECOND read's: the first read is still yesterday, the second is today.
        { "midnight between the reads", kToday - 50, kToday - 1, kToday, 0x00050003,
          { "clock", "clock", "get16 KILLS 0", "set16 KILLS 0 0", "set16 KILLS 1 3" }, 0x00030000 },
        // A first read of today and a second of yesterday (a clock set back between the reads):
        // the day is still the second read's, so no rollover.
        { "reads swapped", kToday - 50, kToday + 1, kToday - 1, 0x00050003, { "clock", "clock" }, 0x00050003 },
    };

    for (Row const& row : rows)
    {
        Wire w;
        w.kills = row.killsBefore;
        w.clockReads = { row.read1, row.read2 };
        HonorMgr mgr(row.last);
        mgr.UpdateKills(w.Clock(), w.Fields());
        CheckEvents(w.events, row.events, __LINE__);
        if (w.kills != row.killsAfter)
        {
            testing::ReportFailure(__FILE__, __LINE__, std::string(row.what) + ": kills " + std::to_string(w.kills)
                                   + " want " + std::to_string(row.killsAfter));
        }
    }
}

// last = the FIRST read. Probe: a later call rolls over iff last is before the probe's midnight.
TEST(HonorMgr_RolloverStoresTheFirstRead)
{
    // First read kToday - 1 (yesterday), second read kToday: it rolls over now and stores
    // kToday - 1, so a call later today rolls over AGAIN (KEPT: a midnight between the two reads).
    // The kill made in between moves to yesterday; the 3 moved the first time are lost.
    {
        Wire w;
        w.kills = 0x00050003;
        w.clockReads = { kToday - 1, kToday };
        HonorMgr mgr(kToday - 50);
        mgr.UpdateKills(w.Clock(), w.Fields());
        CHECK_EQ(w.kills, uint32(0x00030000));
        w.kills += 1;                                       // a kill today
        w.events.clear();
        w.clockReads = { kToday + 60, kToday + 60 };
        w.clockNext = 0;
        mgr.UpdateKills(w.Clock(), w.Fields());
        CheckEvents(w.events, { "clock", "clock", "get16 KILLS 0", "set16 KILLS 0 0", "set16 KILLS 1 1" }, __LINE__);
        CHECK_EQ(w.kills, uint32(0x00010000));
    }
    // Both reads today: last = today, so a later call today does nothing.
    {
        Wire w;
        w.kills = 0x00050003;
        w.clockReads = { kToday + 5, kToday + 5 };
        HonorMgr mgr(kToday - 50);
        mgr.UpdateKills(w.Clock(), w.Fields());
        w.events.clear();
        w.clockReads = { kToday + 60, kToday + 60 };
        w.clockNext = 0;
        mgr.UpdateKills(w.Clock(), w.Fields());
        CheckEvents(w.events, { "clock", "clock" }, __LINE__);
    }
    // SetLastKillUpdate replaces the timestamp (the login's logout time).
    {
        Wire w;
        w.kills = 0x00050003;
        w.clockReads = { kToday + 5, kToday + 5 };
        HonorMgr mgr(kToday + 1);
        mgr.SetLastKillUpdate(kYesterday - 1);
        mgr.UpdateKills(w.Clock(), w.Fields());
        CheckEvents(w.events, { "clock", "clock", "get16 KILLS 0", "set32 KILLS 0" }, __LINE__);
    }
    // The constructor's value is the first comparison's; a default-constructed one is 0 (never).
    {
        Wire w;
        w.kills = 0x00050003;
        w.clockReads = { kToday + 5, kToday + 5 };
        HonorMgr mgr;
        mgr.UpdateKills(w.Clock(), w.Fields());
        CheckEvents(w.events, { "clock", "clock", "get16 KILLS 0", "set32 KILLS 0" }, __LINE__);
    }
}

// ---------------------------------------------------------------------------------------------
// Reward's early-outs.

// In an arena: no clock, no field, no draw, no packet, no currency -- only the verdict. False for
// no victim, the owner itself, a non-character victim, the same BG team; true (the on-kill proc)
// otherwise. Neither the inactive aura nor the team is the arena's check.
TEST(HonorMgr_ArenaEarlyOuts)
{
    struct Row
    {
        char const* what;
        bool present;
        bool isOwner;
        bool isPlayer;
        Team victimBGTeam;
        bool want;
    };
    const Row rows[] =
    {
        { "no victim", false, false, true, HORDE, false },
        { "the owner", true, true, true, HORDE, false },
        { "a creature", true, false, false, HORDE, false },
        { "same BG team", true, false, true, ALLIANCE, false },
        { "other BG team", true, false, true, HORDE, true },
    };
    for (Row const& row : rows)
    {
        Wire w;
        HonorMgr::HonorInputs in = w.Kill();
        in.owner.inArena = true;
        in.owner.bgTeam = ALLIANCE;
        in.victim.present = row.present;
        in.victim.isOwner = row.isOwner;
        in.victim.isPlayer = row.isPlayer;
        in.victim.bgTeam = row.victimBGTeam;
        in.victim.team = ALLIANCE;
        in.owner.inactive = true;
        bool got = RunWithRollover(w, in);
        if (got != row.want)
        {
            testing::ReportFailure(__FILE__, __LINE__, std::string(row.what) + ": wrong verdict");
        }
        CheckEvents(w.events, {}, __LINE__);
    }

    // The arena compares the BG teams, not the teams: an owner whose team (Horde) differs from its
    // BG team (Alliance), on a Horde-BG victim, gets true.
    Wire w;
    HonorMgr::HonorInputs in = w.Kill();
    in.owner.inArena = true;
    in.owner.team = HORDE;
    in.owner.bgTeam = ALLIANCE;
    in.victim.team = HORDE;
    in.victim.bgTeam = HORDE;
    CHECK(RunWithRollover(w, in));
    CheckEvents(w.events, {}, __LINE__);
}

// The inactive aura: false, before the rollover (no clock read), whatever else holds.
TEST(HonorMgr_InactiveEarlyOut)
{
    Wire w;
    HonorMgr::HonorInputs in = w.Kill();
    in.owner.inactive = true;
    CHECK(!RunWithRollover(w, in));
    CheckEvents(w.events, {}, __LINE__);

    Wire given;
    HonorMgr::HonorInputs gin = given.Kill();
    gin.owner.inactive = true;
    gin.victim.present = false;
    CHECK(!RunWithRollover(given, gin, 1, 50.0f));
    CheckEvents(given.events, {}, __LINE__);
}

// With the honor to be computed (honor <= 0): false AFTER the rollover for no victim, the owner
// itself, a NO_PVP_CREDIT victim, a same-team character outside a free-for-all realm, a grey
// character, a creature that is not a racial leader. Each row runs with a rollover due, so the
// rollover's events show it ran first.
TEST(HonorMgr_VictimEarlyOuts)
{
    struct Row
    {
        char const* what;
        float honor;
        bool present;
        bool isOwner;
        bool noPvpCredit;
        bool isPlayer;
        Team victimTeam;
        bool ffa;
        uint32 victimLevel;
        bool racialLeader;
        bool want;
    };
    const Row rows[] =
    {
        { "no victim", -1.0f, false, false, false, true, HORDE, false, 85, false, false },
        { "no victim, honor exactly 0", 0.0f, false, false, false, true, HORDE, false, 85, false, false },
        { "the owner", -1.0f, true, true, false, true, HORDE, false, 85, false, false },
        { "NO_PVP_CREDIT", -1.0f, true, false, true, true, HORDE, false, 85, false, false },
        { "same team", -1.0f, true, false, false, true, ALLIANCE, false, 85, false, false },
        { "same team in a free-for-all realm", -1.0f, true, false, false, true, ALLIANCE, true, 85, false, true },
        { "grey (76 at 85)", -1.0f, true, false, false, true, HORDE, false, 76, false, false },
        { "creature, not a leader", -1.0f, true, false, false, false, HORDE, false, 85, false, false },
        { "creature, a leader", -1.0f, true, false, false, false, HORDE, false, 85, true, true },
    };
    const std::vector<std::string> head = { "clock", "clock", "get16 KILLS 0", "set16 KILLS 0 0", "set16 KILLS 1 2" };
    for (Row const& row : rows)
    {
        Wire w;
        HonorMgr::HonorInputs in = w.Kill();
        in.victim.present = row.present;
        in.victim.isOwner = row.isOwner;
        in.victim.noPvpCredit = row.noPvpCredit;
        in.victim.isPlayer = row.isPlayer;
        in.victim.team = row.victimTeam;
        in.ffaRealm = row.ffa;
        in.victim.level = row.victimLevel;
        in.victim.racialLeader = row.racialLeader;
        w.kills = 0x00000002;                               // the rollover moves 2 to yesterday
        bool got = RunWithRollover(w, in, 1, row.honor);
        if (got != row.want)
        {
            testing::ReportFailure(__FILE__, __LINE__, std::string(row.what) + ": wrong verdict");
        }
        if (!row.want)
        {
            CheckEvents(w.events, head, __LINE__);
        }
        else
        {
            REQUIRE(w.events.size() > head.size());
            CheckEvents(std::vector<std::string>(w.events.begin(), w.events.begin() + head.size()), head, __LINE__);
        }
    }

    // The same-team check compares the teams, not the BG teams: an owner whose BG team (Horde)
    // differs from its team (Alliance), on a Horde victim outside a free-for-all realm, is rewarded.
    Wire w;
    HonorMgr::HonorInputs in = w.Kill();
    in.owner.team = ALLIANCE;
    in.owner.bgTeam = HORDE;
    in.victim.team = HORDE;
    in.victim.bgTeam = HORDE;
    in.ffaRealm = false;
    CHECK(Run(w, in));
    CheckEvents(w.events, KillEvents(40, 0, 0), __LINE__);
}

// Every combination of the facts the early-outs read, against the old body's decision tree as
// transcribed here from HonorMgr.cpp:72-199 at 9fb598e4e: the verdict, whether the rollover ran,
// whether a character kill was counted, whether the draw was taken, whether the packet and the
// currency change went out. 12 facts and the honor's sign (-1 or exactly 0 when not given): 8192
// combinations.
TEST(HonorMgr_EarlyOutsExhaustive)
{
    enum { ARENA, PRESENT, OWNER, PLAYER, SAME_BG, INACTIVE, GIVEN, NO_CREDIT, SAME_TEAM, FFA, GREY, LEADER, COUNT };
    int combos = 0;
    int mismatches = 0;
    for (uint32 bits = 0; bits < (1u << (COUNT + 1)); ++bits)
    {
        auto has = [bits](int fact) { return ((bits >> fact) & 1) != 0; };
        const bool zeroNotNegative = ((bits >> COUNT) & 1) != 0;

        // The old decision tree.
        bool verdict;
        bool rollover = false;
        bool counted = false;
        bool drawn = false;
        bool sent = false;
        if (has(ARENA))
        {
            verdict = !(!has(PRESENT) || has(OWNER) || !has(PLAYER)) && !has(SAME_BG);
        }
        else if (has(INACTIVE))
        {
            verdict = false;
        }
        else
        {
            rollover = true;
            verdict = true;
            if (!has(GIVEN))
            {
                if (!has(PRESENT) || has(OWNER) || has(NO_CREDIT))
                {
                    verdict = false;
                }
                else if (has(PLAYER))
                {
                    if ((has(SAME_TEAM) && !has(FFA)) || has(GREY))
                    {
                        verdict = false;
                    }
                    else
                    {
                        counted = true;
                    }
                }
                else if (!has(LEADER))
                {
                    verdict = false;
                }
            }
            if (verdict)
            {
                drawn = has(PRESENT);
                sent = true;
            }
        }

        Wire w;
        HonorMgr::HonorInputs in = w.Kill();
        in.owner.inArena = has(ARENA);
        in.victim.present = has(PRESENT);
        in.victim.isOwner = has(OWNER);
        in.victim.isPlayer = has(PLAYER);
        in.victim.bgTeam = has(SAME_BG) ? ALLIANCE : HORDE;
        in.owner.inactive = has(INACTIVE);
        in.victim.noPvpCredit = has(NO_CREDIT);
        in.victim.team = has(SAME_TEAM) ? ALLIANCE : HORDE;
        in.ffaRealm = has(FFA);
        in.victim.level = has(GREY) ? 76 : 85;
        in.victim.racialLeader = has(LEADER);
        const float honor = has(GIVEN) ? 50.0f : (zeroNotNegative ? 0.0f : -1.0f);
        const bool got = RunWithRollover(w, in, 1, honor);

        auto count = [&w](std::string const& prefix)
        {
            int n = 0;
            for (std::string const& e : w.events)
            {
                n += e.compare(0, prefix.size(), prefix) == 0 ? 1 : 0;
            }
            return n;
        };
        ++combos;
        if (got != verdict || count("clock") != (rollover ? 2 : 0) || count("set16") != (rollover ? 2 : 0)
            || count("mod") != (counted ? 2 : 0) || count("ach") != (counted ? 3 : 0) || count("draw") != (drawn ? 1 : 0)
            || count("packet") != (sent ? 1 : 0) || count("currency") != (sent ? 1 : 0))
        {
            ++mismatches;
            if (mismatches <= 5)
            {
                testing::ReportFailure(__FILE__, __LINE__, "combination " + std::to_string(bits) + ": [" + Join(w.events) + "]");
            }
        }
    }
    CHECK_EQ(combos, 8192);
    CHECK_EQ(mismatches, 0);
}

// ---------------------------------------------------------------------------------------------
// The honor of a character kill.
//
// honor = ((1 * diff * (190 + 1 * 10)) / 6) * (k / 70), diff = (v - grey) / (k - grey), or 1 when
// k == grey; then * rate * ((modifier + 100) / 100); / groupsize when above 1; * (draw / 10).
// All in float32; the packet carries uint32(honor), the currency int32(honor).

// The grey level is the owner's MaNGOS::XP::GetGrayLevel(level) (Formulas.h, read by the owner;
// its header needs the character class, so the rows hold the values, derived by hand from its four
// bands): 0 up to 5, then level - 5 - level/10 up to 39, level - 1 - level/5 up to 59, level - 9
// above. A victim at or below it gives nothing; one above it gives the level-scaled honor. Draw 10
// (factor 1), title 0 (the guid cleared, rank 0).
TEST(HonorMgr_GreyLevelTable)
{
    struct Row
    {
        uint32 k;
        uint32 grey;                        ///< derived by hand from GetGrayLevel's four bands
        uint32 v;
        bool want;
        uint32 honor;                       ///< uint32 of the float32 result
    };
    const Row rows[] =
    {
        // k 5: grey 0. diff (1-0)/(5-0) = 0.2; 200 * 0.2 / 6 = 6.6666665; * 5/70 = 0.47619048 -> 0.
        { 5, 0, 1, true, 0 },
        // k 6: grey 6-5-0 = 1. v 1 is grey; v 2: diff 0.2; 6.6666665 * 6/70 = 0.5714286 -> 0.
        { 6, 1, 1, false, 0 },
        { 6, 1, 2, true, 0 },
        // k 39: grey 39-5-3 = 31. v 31 grey; v 32: diff 1/8; 200/8/6 = 4.1666665; * 39/70 = 2.3214285 -> 2.
        { 39, 31, 31, false, 0 },
        { 39, 31, 32, true, 2 },
        // k 40: grey 40-1-8 = 31. v 32: diff 1/9; 200/9/6 = 3.7037036; * 40/70 = 2.1164024 -> 2.
        { 40, 31, 31, false, 0 },
        { 40, 31, 32, true, 2 },
        // k 59: grey 59-1-11 = 47. v 48: diff 1/12; 200/12/6 = 2.7777777; * 59/70 = 2.34127 -> 2.
        { 59, 47, 47, false, 0 },
        { 59, 47, 48, true, 2 },
        // k 60: grey 60-9 = 51. v 52: diff 1/9; 3.7037036 * 60/70 = 3.1746035 -> 3.
        { 60, 51, 51, false, 0 },
        { 60, 51, 52, true, 3 },
        // k 85: grey 76. v 76 grey; v 77: diff 1/9; 3.7037036 * 85/70 = 4.497355 -> 4; v 80: diff
        // 4/9; 14.814815 * 85/70 = 17.98942 -> 17; v 85: diff 1; 33.333332 * 85/70 = 40.47619 -> 40;
        // v 88: diff 12/9 = 1.3333334; 44.444447 * 85/70 = 53.968258 -> 53.
        { 85, 76, 76, false, 0 },
        { 85, 76, 77, true, 4 },
        { 85, 76, 80, true, 17 },
        { 85, 76, 85, true, 40 },
        { 85, 76, 88, true, 53 },
    };
    for (Row const& row : rows)
    {
        Wire w;
        HonorMgr::HonorInputs in = w.Kill();
        in.owner.level = row.k;
        in.owner.grayLevel = row.grey;
        in.victim.level = row.v;
        bool got = Run(w, in);
        CHECK_EQ(got, row.want);
        if (row.want)
        {
            CheckEvents(w.events, KillEvents(row.honor, 0, 0), __LINE__);
        }
        else
        {
            CheckEvents(w.events, { "clock", "clock" }, __LINE__);
        }
    }
}

// k == grey takes diff 1 instead of a division by zero. The grey level is an input, so the row
// can hold it where the formula never does (it is 0 up to level 5, and level 0 has no victim
// above it): k 10, grey 10, v 11 -> 200/6 = 33.333332 * 10/70 = 4.7619047 -> 4. One below, grey 9:
// diff (11-9)/(10-9) = 2 -> 66.666664 * 10/70 = 9.523809 -> 9.
TEST(HonorMgr_LevelEqualsGrey)
{
    Wire w;
    HonorMgr::HonorInputs in = w.Kill();
    in.owner.level = 10;
    in.owner.grayLevel = 10;
    in.victim.level = 11;
    CHECK(Run(w, in));
    CheckEvents(w.events, KillEvents(4, 0, 0), __LINE__);

    Wire w9;
    HonorMgr::HonorInputs in9 = w9.Kill();
    in9.owner.level = 10;
    in9.owner.grayLevel = 9;
    in9.victim.level = 11;
    CHECK(Run(w9, in9));
    CheckEvents(w9.events, KillEvents(9, 0, 0), __LINE__);
}

// The victim's chosen title: 0 and 29+ clear the guid (no "HK: <rank>" line) and leave rank 0;
// 1..14 give rank title + 4 (5..18); 15..28 give rank title - 14 + 4 (5..18). 85 on 85: 40.
TEST(HonorMgr_TitleRank)
{
    struct Row
    {
        uint32 title;
        uint64 guid;
        uint32 rank;
    };
    const Row rows[] =
    {
        { 0, 0, 0 },
        { 1, kVictimGuid, 5 },
        { 7, kVictimGuid, 11 },
        { 14, kVictimGuid, 18 },
        { 15, kVictimGuid, 5 },
        { 21, kVictimGuid, 11 },
        { 28, kVictimGuid, 18 },
        { 29, 0, 0 },
        { 38, 0, 0 },
        { 39, 0, 0 },
        { 0xFFFFFFFF, 0, 0 },
    };
    for (Row const& row : rows)
    {
        Wire w;
        HonorMgr::HonorInputs in = w.Kill();
        in.victim.chosenTitle = row.title;
        CHECK(Run(w, in));
        CheckEvents(w.events, KillEvents(40, row.guid, row.rank), __LINE__);
    }
}

// The factors after the kill's honor (40.47619 at 85 on 85): the rate, the honor-gain modifier,
// the group split (only above 1), the draw (each of 8..12). Title 1: the guid kept, rank 5.
TEST(HonorMgr_Factors)
{
    struct Row
    {
        char const* what;
        float rate;
        int32 modifier;
        uint32 groupsize;
        uint32 draw;
        uint32 honor;
    };
    const Row rows[] =
    {
        // 40.47619 * 0.8 = 32.380951 -> 32; * 0.9 = 36.42857 -> 36; * 1.0 = 40; * 1.1 = 44.52381 -> 44;
        // * 1.2 = 48.57143 -> 48.
        { "draw 8", 1.0f, 0, 1, 8, 32 },
        { "draw 9", 1.0f, 0, 1, 9, 36 },
        { "draw 10", 1.0f, 0, 1, 10, 40 },
        { "draw 11", 1.0f, 0, 1, 11, 44 },
        { "draw 12", 1.0f, 0, 1, 12, 48 },
        // * 2.5 = 101.19048 -> 101.
        { "rate 2.5", 2.5f, 0, 1, 10, 101 },
        // * (50 + 100) / 100 = 1.5: 60.714283 -> 60.
        { "modifier 50", 1.0f, 50, 1, 10, 60 },
        // groupsize 0 and 1 divide by nothing; 2: 20.238094 -> 20; 3: 13.492063 -> 13.
        { "group 0", 1.0f, 0, 0, 10, 40 },
        { "group 1", 1.0f, 0, 1, 10, 40 },
        { "group 2", 1.0f, 0, 2, 10, 20 },
        { "group 3", 1.0f, 0, 3, 10, 13 },
        // All four: 40.47619 * 3 = 121.42857; * 1.25 = 151.78572; / 2 = 75.89286; * 1.2 = 91.07143 -> 91.
        { "all four", 3.0f, 25, 2, 12, 91 },
    };
    for (Row const& row : rows)
    {
        Wire w;
        HonorMgr::HonorInputs in = w.Kill();
        in.victim.chosenTitle = 1;
        in.honorRate = row.rate;
        in.owner.honorGainModifier = row.modifier;
        w.draw = row.draw;
        CHECK(Run(w, in, row.groupsize));
        CheckEvents(w.events, KillEvents(row.honor, kVictimGuid, 5), __LINE__);
    }

    // The ORDER of the factors, in float32: two rows whose integer result changes when two factors
    // are applied in another order. Level 12 (grey 6, the owner's GetGrayLevel(12) = 12 - 5 - 1).
    struct OrderRow
    {
        char const* what;
        uint32 v;
        float rate;
        int32 modifier;
        uint32 groupsize;
        uint32 draw;
        uint32 honor;
    };
    const OrderRow orderRows[] =
    {
        // v 13: diff 7/6 = 1.16666663; * 200 = 233.333328; / 6 = 38.8888893; * 12/70 (0.171428576) =
        // 6.66666698; * 1 (rate); * 1.5 = 10; / 3 = 3.33333325; * 0.9 (0.899999976) = 2.99999976 -> 2.
        // The draw before the split gives 10 * 0.9 = 9, / 3 = 3 -> 3.
        { "split before draw", 13, 1.0f, 50, 3, 9, 2 },
        // v 9: diff 3/6 = 0.5; * 200 = 100; / 6 = 16.666666; * 12/70 = 2.85714293; * 3 (rate) =
        // 8.5714283; * 1.05 (1.04999995) = 8.99999905; * 1 (draw 10) -> 8. The modifier before the
        // rate gives 2.85714293 * 1.04999995 = 3, * 3 = 9 -> 9.
        { "rate before modifier", 9, 3.0f, 5, 1, 10, 8 },
    };
    for (OrderRow const& row : orderRows)
    {
        Wire w;
        HonorMgr::HonorInputs in = w.Kill();
        in.victim.chosenTitle = 1;
        in.owner.level = 12;
        in.owner.grayLevel = 6;
        in.victim.level = row.v;
        in.honorRate = row.rate;
        in.owner.honorGainModifier = row.modifier;
        w.draw = row.draw;
        CHECK(Run(w, in, row.groupsize));
        CheckEvents(w.events, KillEvents(row.honor, kVictimGuid, 5), __LINE__);
    }
}

// ---------------------------------------------------------------------------------------------
// The other two rewards.

// A given honor value (the quest reward, the battleground's, the outdoor PvP zones', `.honor
// add`): no victim, so no factor and no draw -- the value as it is, an empty guid, rank 0 -- and
// no kill counted. With a victim (no caller passes one today) the factors and the draw apply, but
// the guid stays empty, the rank 0, and none of the victim's facts is consulted.
TEST(HonorMgr_GivenHonor)
{
    {
        Wire w;
        HonorMgr::HonorInputs in = w.Kill();
        in.victim.present = false;
        in.honorRate = 2.0f;
        in.owner.honorGainModifier = 10;
        CHECK(Run(w, in, 0, 100.0f));
        CheckEvents(w.events, { "clock", "clock", Credit(100, 0, 0), "currency 392 100" }, __LINE__);
    }
    {
        // 100 * 2 = 200; * 1.1 = 220; / 4 = 55; * 0.9 = 49.5 -> 49. The victim is the owner, on the
        // same team, titled: none of it consulted.
        Wire w;
        HonorMgr::HonorInputs in = w.Kill();
        in.victim.isOwner = true;
        in.victim.team = ALLIANCE;
        in.victim.chosenTitle = 3;
        in.honorRate = 2.0f;
        in.owner.honorGainModifier = 10;
        w.draw = 9;
        CHECK(Run(w, in, 4, 100.0f));
        CheckEvents(w.events, { "clock", "clock", "draw", Credit(49, 0, 0), "currency 392 49" }, __LINE__);
    }
    {
        // The value is truncated, not rounded: 12.9 -> 12 in the packet and in the currency.
        Wire w;
        HonorMgr::HonorInputs in = w.Kill();
        in.victim.present = false;
        CHECK(Run(w, in, 1, 12.9f));
        CheckEvents(w.events, { "clock", "clock", Credit(12, 0, 0), "currency 392 12" }, __LINE__);
    }
}

// A racial leader: honor 100, rank 19 ("HK: Leader"), the creature's guid kept, then the factors
// and the draw; no kill counted and no criteria. 100 * 1.5 (rate) = 150; / 2 = 75; * 1.1 = 82.5 -> 82.
TEST(HonorMgr_RacialLeader)
{
    Wire w;
    HonorMgr::HonorInputs in = w.Kill();
    in.victim.isPlayer = false;
    in.victim.racialLeader = true;
    in.victim.team = ALLIANCE;                              // a creature's team is not checked
    in.victim.level = 1;                                    // nor its level
    in.honorRate = 1.5f;
    w.draw = 11;
    CHECK(Run(w, in, 2));
    CheckEvents(w.events, { "clock", "clock", "draw", Credit(82, kVictimGuid, 19), "currency 392 82" }, __LINE__);
}

// ---------------------------------------------------------------------------------------------
// The packet and the order of the effects.

// SMSG_PVP_CREDIT, byte by byte, written out rather than built by Credit(): 85 on 85 (40), a
// rank-5 victim: 28000000 (40), the guid's eight bytes low first, 05000000.
TEST(HonorMgr_CreditBytes)
{
    Wire w;
    HonorMgr::HonorInputs in = w.Kill();
    in.victim.chosenTitle = 1;
    CHECK(Run(w, in));
    REQUIRE(w.events.size() == 10);
    CHECK_STR(w.events[8], "packet 6015:28000000080706050403020105000000");
    CHECK_STR(w.events[9], "currency 392 40");
}

// A character kill with a rollover due: the rollover, the two kill counts (today's half, emptied
// by the rollover, takes the kill), the three criteria with the victim's class and race, the draw,
// the packet, the currency -- in that order, each once.
TEST(HonorMgr_KillSinkOrder)
{
    Wire w;
    w.kills = 0x00000007;                                   // 7 kills on the previous day
    w.lifetime = 1000;
    HonorMgr::HonorInputs in = w.Kill();
    in.victim.classId = 9;
    in.victim.race = 10;
    in.victim.chosenTitle = 20;                             // rank 20 - 14 + 4 = 10
    w.draw = 12;
    CHECK(RunWithRollover(w, in));
    CheckEvents(w.events,
        { "clock", "clock", "get16 KILLS 0", "set16 KILLS 0 0", "set16 KILLS 1 7",
          "mod KILLS 1 1", "mod LIFETIME 1 1", "ach 113 0", "ach 52 9", "ach 53 10", "draw",
          Credit(48, kVictimGuid, 10), "currency 392 48" }, __LINE__);
    CHECK_EQ(w.kills, uint32(0x00070001));                  // yesterday 7, today 1
    CHECK_EQ(w.lifetime, uint32(1001));
}

// A kill adds 1 to the whole word as an int32 clamped at 0 (KEPT): today's half at 65535 carries
// into yesterday's; a word that reads negative as an int32 (yesterday's half 32768 or more) is
// cleared.
TEST(HonorMgr_KillCountPacking)
{
    struct Row
    {
        uint32 before;
        uint32 after;
    };
    const Row rows[] =
    {
        { 0x00000000, 0x00000001 },
        { 0x00050003, 0x00050004 },
        { 0x0005FFFF, 0x00060000 },
        { 0x80000000, 0x00000000 },
        { 0xFFFFFFFF, 0x00000000 },
    };
    for (Row const& row : rows)
    {
        Wire w;
        w.kills = row.before;
        HonorMgr::HonorInputs in = w.Kill();
        CHECK(Run(w, in));
        CHECK_EQ(w.kills, row.after);
    }
}
