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

/// Decoupling D4k: a character's reputations -- ranks, base reputation and default flags from the
/// race and class masks, set and modify with the clamp, the rank counters, the spillover (template,
/// team parent, sisters), the visible / at-war / inactive transitions, the five packets, the per-row
/// load and the save -- with no character.
///
/// Before this PR ReputationMgr held a pointer to its owner and read the owner's race and class
/// masks, name, session (the loading flag), the object manager's spillover templates, and called
/// the owner's quest check, achievement manager and SendDirectMessage. Now the masks and the name
/// are owner facts set once (SetOwnerFacts), the rest are call-scoped read callbacks and sinks, so
/// every case builds one from nothing. A Wire records, in call order, every read of the loading
/// flag ("loading?"), every template and team-list read ("tpl <id>", "team <id>"), every packet
/// (opcode and bytes), every quest check -- with what it sees of the manager at that moment:
/// "changed <id> rep=<GetReputation> vis=<visible> hon=<honored> rev=<revered> exa=<exalted>" --
/// and every achievement update ("ach <type> <id>").
///
/// The faction store is seeded the way TalentMgrTest seeds its stores: DBCStorage::SetEntry. No
/// other test in this binary uses the faction store. A seeded store's GetNumRows() is its entry
/// COUNT (DBCStorage::SetEntry switches it to its map), and Initialize walks 1 .. GetNumRows() - 1,
/// so the ids are 1..12 with id 0 seeded as a NULL row: 13 entries, and the walk sees all twelve.
/// Each faction's reputation list id is its id + 20, so a packet (list id) and a statement
/// (faction id) that swapped the two show.
///
/// The factions (race mask 0x4 and class mask 0x1 unless a case says otherwise):
///   id  list  rows (raceMask/classMask: base, flags)         parent  mod0  mod1  cap0
///    1   21   any: 0, VISIBLE                                  -
///    2   22   0x8/any: -10000, AT_WAR; 0x4/0x2: 5000, PEACE_FORCED;
///             0xC/0x1: 21000, VISIBLE; any: 42000, HIDDEN      -
///    3   23   any: 0, HIDDEN                                   -
///    4   24   any: -42000, AT_WAR|INVISIBLE_FORCED (0x0A)      -
///    5   25   any: 0, none                                     -      (the sisters' parent)
///    6   26   any: 0, VISIBLE                                  5     0.5   0.5   REVERED
///    7   27   any: 0, none                                     5     0.25  0.5   FRIENDLY
///    8   28   any: 0, VISIBLE|TEAM_REPUTATION (0x81)           -      (a team-reputation parent)
///    9   29   any: 0, VISIBLE                                  8     1.0   0.25  EXALTED
///   10   30   any: 0, VISIBLE                                  -      (the spillover template's source)
///   11    -   no reputation index                              -
///   12   32   any: 0, VISIBLE|PEACE_FORCED (0x11)              -
/// Team lists: 5 -> {6, 7}, 8 -> {9}. Spillover template of 10: faction {1, 2, 0, 3, 0}, rate
/// {0.5, 2.0, 9.0, 1.0, 0}, rank {HONORED, NEUTRAL, EXALTED, EXALTED, HATED}.
///
/// Ranks, by hand from PointsInRank {36000, 3000, 3000, 3000, 6000, 12000, 21000, 1000} counted
/// down from Reputation_Cap + 1 = 43000: exalted >= 42000, revered >= 21000, honored >= 9000,
/// friendly >= 3000, neutral >= 0, unfriendly >= -3000, hostile >= -6000, hated >= -42000, and
/// below that the loop ends at hated too.
///
/// Opcodes (Opcodes.h): SMSG_INITIALIZE_FACTIONS 0x4634, SMSG_SET_FACTION_VISIBLE 0x2525,
/// SMSG_SET_FACTION_STANDING 0x0126, SMSG_SET_FORCED_REACTIONS 0x4615, SMSG_SET_FACTION_ATWAR
/// 0x4216; PacketText prints the opcode as four hex digits, then the bytes. Numbers are
/// little-endian: 9000 = 0x2328 -> 28230000, 42999 = 0xa7f7 -> f7a70000, -42000 = 0xffff5bf0 ->
/// f05bffff, -21000 = 0xffffadf8 -> f8adffff, 1000 -> e8030000, 500 -> f4010000, 250 -> fa000000,
/// 125 -> 7d000000, 100 -> 64000000.
///
/// The save runs through the GLOBAL CharacterDatabase with D7a's fakes attached and asynchronous
/// writes on, as in SocialMgrTest: the prepared statements arrive with their parameters quoted.

#include "TestHarness.h"
#include "FakeDatabase.h"
#include "Database/DatabaseEnv.h"
#include "Database/TickGuard.h"
#include "DBCStores.h"
#include "Opcodes.h"
#include "WorldPacket.h"
#include "ReputationMgr.h"

#include <cstdio>
#include <list>
#include <map>
#include <string>
#include <vector>

namespace
{
    static_assert(SMSG_INITIALIZE_FACTIONS == 0x4634, "the list opcode, by hand above");
    static_assert(SMSG_SET_FACTION_VISIBLE == 0x2525, "the visible opcode, by hand above");
    static_assert(SMSG_SET_FACTION_STANDING == 0x0126, "the standing opcode, by hand above");
    static_assert(SMSG_SET_FORCED_REACTIONS == 0x4615, "the forced reactions opcode, by hand above");
    static_assert(SMSG_SET_FACTION_ATWAR == 0x4216, "the at-war opcode, by hand above");
    static_assert(ACHIEVEMENT_CRITERIA_TYPE_KNOWN_FACTIONS == 89 && ACHIEVEMENT_CRITERIA_TYPE_GAIN_REPUTATION == 46
                  && ACHIEVEMENT_CRITERIA_TYPE_GAIN_EXALTED_REPUTATION == 47 && ACHIEVEMENT_CRITERIA_TYPE_GAIN_REVERED_REPUTATION == 87
                  && ACHIEVEMENT_CRITERIA_TYPE_GAIN_HONORED_REPUTATION == 88, "the five criteria types, in the events below");

    const uint32 kRace = 0x4;                               // race 3: 1 << (3 - 1)
    const uint32 kClass = 0x1;                              // class 1: 1 << (1 - 1)
    const uint32 kOwner = 42;

    FactionEntry s_factions[13];

    /// A faction whose first row fits every race and class.
    void Plain(FactionEntry& f, uint32 id, int32 listId, int32 base, uint32 flags)
    {
        f = FactionEntry();
        f.ID = id;
        f.ReputationIndex = listId;
        f.ReputationBase[0] = base;
        f.ReputationFlags[0] = flags;
    }

    void SeedStores()
    {
        static bool seeded = false;
        if (seeded)
        {
            return;
        }
        seeded = true;

        Plain(s_factions[1], 1, 21, 0, FACTION_FLAG_VISIBLE);

        FactionEntry& masks = s_factions[2];
        masks = FactionEntry();
        masks.ID = 2;
        masks.ReputationIndex = 22;
        masks.ReputationRaceMask[0] = 0x8;  masks.ReputationClassMask[0] = 0;   masks.ReputationBase[0] = -10000; masks.ReputationFlags[0] = FACTION_FLAG_AT_WAR;
        masks.ReputationRaceMask[1] = 0x4;  masks.ReputationClassMask[1] = 0x2; masks.ReputationBase[1] = 5000;   masks.ReputationFlags[1] = FACTION_FLAG_PEACE_FORCED;
        masks.ReputationRaceMask[2] = 0xC;  masks.ReputationClassMask[2] = 0x1; masks.ReputationBase[2] = 21000;  masks.ReputationFlags[2] = FACTION_FLAG_VISIBLE;
        masks.ReputationRaceMask[3] = 0;    masks.ReputationClassMask[3] = 0;   masks.ReputationBase[3] = 42000;  masks.ReputationFlags[3] = FACTION_FLAG_HIDDEN;

        Plain(s_factions[3], 3, 23, 0, FACTION_FLAG_HIDDEN);
        Plain(s_factions[4], 4, 24, -42000, FACTION_FLAG_AT_WAR | FACTION_FLAG_INVISIBLE_FORCED);
        Plain(s_factions[5], 5, 25, 0, 0);
        Plain(s_factions[6], 6, 26, 0, FACTION_FLAG_VISIBLE);
        s_factions[6].ParentFactionID = 5;
        s_factions[6].ParentFactionMod_0 = 0.5f;
        s_factions[6].ParentFactionMod_1 = 0.5f;
        s_factions[6].ParentFactionCap_0 = REP_REVERED;
        Plain(s_factions[7], 7, 27, 0, 0);
        s_factions[7].ParentFactionID = 5;
        s_factions[7].ParentFactionMod_0 = 0.25f;
        s_factions[7].ParentFactionMod_1 = 0.5f;
        s_factions[7].ParentFactionCap_0 = REP_FRIENDLY;
        Plain(s_factions[8], 8, 28, 0, FACTION_FLAG_VISIBLE | FACTION_FLAG_TEAM_REPUTATION);
        Plain(s_factions[9], 9, 29, 0, FACTION_FLAG_VISIBLE);
        s_factions[9].ParentFactionID = 8;
        s_factions[9].ParentFactionMod_0 = 1.0f;
        s_factions[9].ParentFactionMod_1 = 0.25f;
        s_factions[9].ParentFactionCap_0 = REP_EXALTED;
        Plain(s_factions[10], 10, 30, 0, FACTION_FLAG_VISIBLE);
        Plain(s_factions[11], 11, -1, 0, FACTION_FLAG_VISIBLE);
        Plain(s_factions[12], 12, 32, 0, FACTION_FLAG_VISIBLE | FACTION_FLAG_PEACE_FORCED);

        sFactionStore.SetEntry(0, NULL);
        for (uint32 id = 1; id <= 12; ++id)
        {
            sFactionStore.SetEntry(id, &s_factions[id]);
        }
    }

    FactionEntry const* F(uint32 id)
    {
        return sFactionStore.LookupEntry(id);
    }

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

    typedef std::vector<std::string> Events;

    /// The callbacks, recording in call order. `mgr` is the manager under test, read by the quest
    /// check so the event shows what the owner's check would see at that moment.
    struct Wire
    {
        Events events;
        bool loading = false;
        std::map<uint32, RepSpilloverTemplate> templates;
        std::map<uint32, std::list<uint32> > teams;
        ReputationMgr const* mgr = NULL;

        Wire()
        {
            RepSpilloverTemplate t = RepSpilloverTemplate();
            t.faction[0] = 1;  t.faction_rate[0] = 0.5f;  t.faction_rank[0] = REP_HONORED;
            t.faction[1] = 2;  t.faction_rate[1] = 2.0f;  t.faction_rank[1] = REP_NEUTRAL;
            t.faction[2] = 0;  t.faction_rate[2] = 9.0f;  t.faction_rank[2] = REP_EXALTED;
            t.faction[3] = 3;  t.faction_rate[3] = 1.0f;  t.faction_rank[3] = REP_EXALTED;
            t.faction[4] = 0;  t.faction_rate[4] = 0.0f;  t.faction_rank[4] = REP_HATED;
            templates[10] = t;
            teams[5] = { 6, 7 };
            teams[8] = { 9 };
        }

        ReputationMgr::FlagNotify Notify()
        {
            ReputationMgr::FlagNotify notify;
            notify.playerLoading = [this]()
            {
                events.push_back("loading?");
                return loading;
            };
            notify.send = [this](WorldPacket const* packet)
            {
                events.push_back(PacketText(*packet));
            };
            return notify;
        }

        ReputationMgr::ChangeInputs Inputs()
        {
            ReputationMgr::ChangeInputs inputs;
            inputs.spilloverTemplate = [this](uint32 factionId) -> RepSpilloverTemplate const*
            {
                events.push_back("tpl " + std::to_string(factionId));
                std::map<uint32, RepSpilloverTemplate>::const_iterator itr = templates.find(factionId);
                return itr != templates.end() ? &itr->second : NULL;
            };
            inputs.teamList = [this](uint32 factionId) -> std::list<uint32> const*
            {
                events.push_back("team " + std::to_string(factionId));
                std::map<uint32, std::list<uint32> >::const_iterator itr = teams.find(factionId);
                return itr != teams.end() ? &itr->second : NULL;
            };
            return inputs;
        }

        ReputationMgr::ChangeSinks Sinks()
        {
            ReputationMgr::ChangeSinks sinks;
            sinks.notify = Notify();
            sinks.reputationChanged = [this](FactionEntry const* factionEntry)
            {
                events.push_back("changed " + std::to_string(factionEntry->ID)
                    + " rep=" + std::to_string(mgr->GetReputation(factionEntry))
                    + " vis=" + std::to_string(mgr->GetVisibleFactionCount())
                    + " hon=" + std::to_string(mgr->GetHonoredFactionCount())
                    + " rev=" + std::to_string(mgr->GetReveredFactionCount())
                    + " exa=" + std::to_string(mgr->GetExaltedFactionCount()));
            };
            sinks.updateAchievement = [this](AchievementCriteriaTypes type, uint32 miscValue1)
            {
                events.push_back("ach " + std::to_string(uint32(type)) + " " + std::to_string(miscValue1));
            };
            return sinks;
        }

        void Modify(ReputationMgr& m, uint32 id, int32 standing)
        {
            m.ModifyReputation(F(id), standing, Inputs(), Sinks());
        }

        void Set(ReputationMgr& m, uint32 id, int32 standing)
        {
            m.SetReputation(F(id), standing, Inputs(), Sinks());
        }

        Events Take()
        {
            Events out;
            out.swap(events);
            return out;
        }
    };

    /// The five achievement updates for one faction, in the old order.
    Events Ach(uint32 id)
    {
        std::string s = " " + std::to_string(id);
        return { "ach 89" + s, "ach 46" + s, "ach 47" + s, "ach 87" + s, "ach 88" + s };
    }

    Events Cat(std::initializer_list<Events> parts)
    {
        Events out;
        for (Events const& p : parts)
        {
            out.insert(out.end(), p.begin(), p.end());
        }
        return out;
    }

    void CheckEvents(Events const& got, Events const& want, int line)
    {
        if (got != want)
        {
            std::string text = "events differ (case line " + std::to_string(line) + "):\n  got  [";
            for (size_t i = 0; i < got.size(); ++i)
            {
                text += (i ? " | " : "") + got[i];
            }
            text += "]\n  want [";
            for (size_t i = 0; i < want.size(); ++i)
            {
                text += (i ? " | " : "") + want[i];
            }
            testing::ReportFailure(__FILE__, line, text + "]");
        }
    }

    /// A manager for the default owner (race mask 0x4, class mask 0x1), initialized, with the login
    /// list sent (every needSend cleared) into a sink that is dropped.
    void Build(ReputationMgr& mgr, Wire& wire)
    {
        SeedStores();
        wire.mgr = &mgr;
        mgr.SetOwnerFacts(kRace, kClass, "Kael");
        mgr.Initialize();
        mgr.SendInitialReputations([](WorldPacket const*) {});
    }

    int32 StandingOf(ReputationMgr const& mgr, RepListID listId)
    {
        FactionState const* state = mgr.GetState(listId);
        return state ? state->Standing : int32(0x7FFFFFFF);
    }

    uint32 FlagsOf(ReputationMgr const& mgr, RepListID listId)
    {
        FactionState const* state = mgr.GetState(listId);
        return state ? state->Flags : 0xFFFFFFFFu;
    }

    /// The rank, from the boundary list above (independent of ReputationToRank).
    ReputationRank HandRank(int32 standing)
    {
        if (standing >= 42000) return REP_EXALTED;
        if (standing >= 21000) return REP_REVERED;
        if (standing >= 9000) return REP_HONORED;
        if (standing >= 3000) return REP_FRIENDLY;
        if (standing >= 0) return REP_NEUTRAL;
        if (standing >= -3000) return REP_UNFRIENDLY;
        if (standing >= -6000) return REP_HOSTILE;
        return REP_HATED;
    }

    FakeRow RepRow(std::string const& faction, std::string const& standing, std::string const& flags)
    {
        FakeRow row;
        row.push_back(faction);
        row.push_back(standing);
        row.push_back(flags);
        return row;
    }

    /// Loads one row through the per-row load.
    void LoadOne(ReputationMgr& mgr, Wire& wire, FakeRow const& row)
    {
        FakeQueryResult result(FakeRows{ row });
        if (!result.NextRow())
        {
            testing::ReportFailure(__FILE__, __LINE__, "the fake row did not load");
            return;
        }
        mgr.LoadRow(result.Fetch(), wire.Notify());
    }

    /// The fakes on the global character database, asynchronous writes on.
    struct Db
    {
        FakeConnection query;
        FakeConnection async;
        SqlResultQueue results;
        AttachedFakes attached;
        size_t seen = 0;

        Db() : query(CharacterDatabase), async(CharacterDatabase),
            attached(CharacterDatabase, &query, &async, &results, /*asyncWrites*/ true)
        {
            TickGuard::ResetViolations();
        }

        std::vector<std::string> Take()
        {
            CharacterDatabase.ExecuteQueuedForTest();
            std::vector<std::string> out(async.executed.begin() + seen, async.executed.end());
            seen = async.executed.size();
            return out;
        }
    };

    std::string DeleteSql(uint32 faction)
    {
        return "DELETE FROM `character_reputation` WHERE `guid` = '42' AND `faction`='" + std::to_string(faction) + "'";
    }

    std::string InsertSql(uint32 faction, int32 standing, uint32 flags)
    {
        return "INSERT INTO `character_reputation` (`guid`,`faction`,`standing`,`flags`) VALUES ('42', '"
            + std::to_string(faction) + "', '" + std::to_string(standing) + "', '" + std::to_string(flags) + "')";
    }
}

// The rank from a standing at every boundary, and one step either side of it. Two ranks meet at
// each boundary, so a comparison off by one (> for >=) moves exactly one of each pair.
TEST(ReputationMgr_RankFromStandingAtEveryBoundary)
{
    struct Row { int32 standing; ReputationRank rank; };
    const Row rows[] = {
        { 42999, REP_EXALTED }, { 42000, REP_EXALTED }, { 41999, REP_REVERED },
        { 21000, REP_REVERED }, { 20999, REP_HONORED },
        { 9000, REP_HONORED }, { 8999, REP_FRIENDLY },
        { 3000, REP_FRIENDLY }, { 2999, REP_NEUTRAL },
        { 0, REP_NEUTRAL }, { -1, REP_UNFRIENDLY },
        { -3000, REP_UNFRIENDLY }, { -3001, REP_HOSTILE },
        { -6000, REP_HOSTILE }, { -6001, REP_HATED },
        { -42000, REP_HATED }, { -42001, REP_HATED }, { 50000, REP_EXALTED },
    };
    for (Row const& r : rows)
    {
        if (ReputationMgr::ReputationToRank(r.standing) != r.rank)
        {
            testing::ReportFailure(__FILE__, __LINE__, "standing " + std::to_string(r.standing) + ": rank "
                + std::to_string(int(ReputationMgr::ReputationToRank(r.standing))) + " want " + std::to_string(int(r.rank)));
        }
        CHECK_EQ(int(HandRank(r.standing)), int(r.rank));
    }
    CHECK_EQ(ReputationMgr::Reputation_Cap, 42999);
    CHECK_EQ(ReputationMgr::Reputation_Bottom, -42000);
}

// Base reputation and default flags come from the first race/class row that fits the owner facts.
// Faction 2 picks row 0 for race mask 0x8 (-10000, AT_WAR), row 1 for 0x4/0x2 (5000,
// PEACE_FORCED), row 2 for 0x4/0x1 (21000, VISIBLE) and row 3 for anything else (42000, HIDDEN).
// A NULL faction has base 0; a faction without a list entry has reputation 0 whatever its base.
TEST(ReputationMgr_BaseReputationAndDefaultFlagsFromTheMasks)
{
    SeedStores();
    struct Row { uint32 race; uint32 cls; int32 base; uint32 flags; ReputationRank baseRank; };
    const Row rows[] = {
        { 0x8, 0x1, -10000, FACTION_FLAG_AT_WAR, REP_HATED },
        { 0x8, 0x400, -10000, FACTION_FLAG_AT_WAR, REP_HATED },
        { 0x4, 0x2, 5000, FACTION_FLAG_PEACE_FORCED, REP_FRIENDLY },
        { 0x4, 0x1, 21000, FACTION_FLAG_VISIBLE, REP_REVERED },
        { 0x4, 0x4, 42000, FACTION_FLAG_HIDDEN, REP_EXALTED },
        { 0x1, 0x1, 42000, FACTION_FLAG_HIDDEN, REP_EXALTED },
        { 0x0, 0x1, 42000, FACTION_FLAG_HIDDEN, REP_EXALTED },
    };
    for (Row const& r : rows)
    {
        ReputationMgr mgr;
        mgr.SetOwnerFacts(r.race, r.cls, "Kael");
        CHECK_EQ(mgr.GetBaseReputation(F(2)), r.base);
        CHECK_EQ(int(mgr.GetBaseRank(F(2))), int(r.baseRank));
        CHECK_EQ(mgr.GetReputation(F(2)), 0);               // no list yet
        mgr.Initialize();
        CHECK_EQ(FlagsOf(mgr, 22), r.flags);
        CHECK_EQ(mgr.GetReputation(F(2)), r.base);          // standing 0 over the base
        CHECK_EQ(mgr.GetReputation(uint32(2)), r.base);
        CHECK_EQ(int(mgr.GetRank(F(2))), int(r.baseRank));
    }

    ReputationMgr mgr;
    mgr.SetOwnerFacts(kRace, kClass, "Kael");
    CHECK_EQ(mgr.GetBaseReputation(NULL), 0);
    CHECK_EQ(mgr.GetReputation((FactionEntry const*)NULL), 0);
    mgr.Initialize();
    CHECK_EQ(mgr.GetReputation(F(11)), 0);                  // no reputation index: no state
    CHECK_EQ(mgr.GetReputation(uint32(99)), 0);             // unknown id: logged with the name, 0
    CHECK_EQ(mgr.GetReputation(F(4)), -42000);
    CHECK_EQ(int(mgr.GetRank(F(4))), int(REP_HATED));
    CHECK(mgr.GetState(F(11)) == NULL);
    CHECK(mgr.GetState(F(1)) == mgr.GetState(RepListID(21)));
}

// Initialize: one state per faction with a list index (list id = id + 20; 11 has none), standing
// 0, the default flags, needSend and needSave; the visible count is the defaults with VISIBLE (21,
// 22, 26, 28, 29, 30, 32 = 7); the rank counters count the BASE ranks (22 is revered at 21000:
// honored 1, revered 1; 24 is hated). Initializing again resets, not adds.
TEST(ReputationMgr_InitializeBuildsTheListAndTheCounters)
{
    SeedStores();
    ReputationMgr mgr;
    mgr.SetOwnerFacts(kRace, kClass, "Kael");
    for (int pass = 0; pass < 2; ++pass)
    {
        mgr.Initialize();
        struct Want { RepListID list; uint32 id; uint32 flags; };
        const Want want[] = {
            { 21, 1, 0x01 }, { 22, 2, 0x01 }, { 23, 3, 0x04 }, { 24, 4, 0x0A }, { 25, 5, 0x00 }, { 26, 6, 0x01 },
            { 27, 7, 0x00 }, { 28, 8, 0x81 }, { 29, 9, 0x01 }, { 30, 10, 0x01 }, { 32, 12, 0x11 },
        };
        CHECK_EQ(mgr.GetStateList().size(), size_t(11));
        for (Want const& w : want)
        {
            FactionState const* state = mgr.GetState(w.list);
            REQUIRE(state != NULL);
            CHECK_EQ(state->ID, w.id);
            CHECK_EQ(state->ReputationListID, w.list);
            CHECK_EQ(state->Flags, w.flags);
            CHECK_EQ(state->Standing, 0);
            CHECK(state->needSend);
            CHECK(state->needSave);
        }
        CHECK_EQ(uint32(mgr.GetVisibleFactionCount()), 7u);
        CHECK_EQ(uint32(mgr.GetHonoredFactionCount()), 1u);
        CHECK_EQ(uint32(mgr.GetReveredFactionCount()), 1u);
        CHECK_EQ(uint32(mgr.GetExaltedFactionCount()), 0u);
    }
}

// Set and modify on a faction with no spillover. Each change reads the template then the team
// list (none), writes the standing, runs the visible check, the at-war check, the counters, then
// the quest check (which already sees the new standing and counters), the five achievement
// updates in the old order, and last the standing packet with the rank-increase flag.
TEST(ReputationMgr_SetAndModifyClampCountersAndSinkOrder)
{
    ReputationMgr mgr;
    Wire wire;
    Build(mgr, wire);

    // +9000 from 0: honored (rank up).
    wire.Modify(mgr, 1, 9000);
    CheckEvents(wire.Take(), Cat({ { "tpl 1", "team 1", "changed 1 rep=9000 vis=7 hon=2 rev=1 exa=0" }, Ach(1),
        { "0126:" "00000000" "01" "01000000" "15000000" "28230000" } }), __LINE__);
    CHECK_EQ(StandingOf(mgr, 21), 9000);

    // Absolute 50000: clamped to the cap 42999, exalted (rank up; revered and exalted counted).
    wire.Set(mgr, 1, 50000);
    CheckEvents(wire.Take(), Cat({ { "tpl 1", "team 1", "changed 1 rep=42999 vis=7 hon=2 rev=2 exa=1" }, Ach(1),
        { "0126:" "00000000" "01" "01000000" "15000000" "f7a70000" } }), __LINE__);

    // -100000: 42999 - 100000 = -57001, clamped to the bottom -42000: hated, so war is declared
    // (the at-war packet comes before the quest check), and the counters drop back.
    wire.Modify(mgr, 1, -100000);
    CheckEvents(wire.Take(), Cat({ { "tpl 1", "team 1", "loading?", "4216:" "15000000" "03",
        "changed 1 rep=-42000 vis=7 hon=1 rev=1 exa=0" }, Ach(1),
        { "0126:" "00000000" "00" "01000000" "15000000" "f05bffff" } }), __LINE__);
    CHECK_EQ(FlagsOf(mgr, 21), 0x03u);

    // A base of 21000 (faction 2): +100 is 21100 total, 100 stored; the rank stays revered.
    wire.Modify(mgr, 2, 100);
    CheckEvents(wire.Take(), Cat({ { "tpl 2", "team 2", "changed 2 rep=21100 vis=7 hon=1 rev=1 exa=0" }, Ach(2),
        { "0126:" "00000000" "00" "01000000" "16000000" "64000000" } }), __LINE__);
    CHECK_EQ(StandingOf(mgr, 22), 100);
    CHECK_EQ(mgr.GetReputation(F(2)), 21100);

    // Absolute 0 on that base: stored -21000, neutral; revered and honored drop.
    wire.Set(mgr, 2, 0);
    CheckEvents(wire.Take(), Cat({ { "tpl 2", "team 2", "changed 2 rep=0 vis=7 hon=0 rev=0 exa=0" }, Ach(2),
        { "0126:" "00000000" "00" "01000000" "16000000" "f8adffff" } }), __LINE__);
    CHECK_EQ(StandingOf(mgr, 22), -21000);

    // A faction without a list index changes nothing and sends nothing (the lookups still run).
    wire.Modify(mgr, 11, 500);
    CheckEvents(wire.Take(), { "tpl 11", "team 11" }, __LINE__);
}

// The spillover template (faction 10): faction 1 (neutral <= honored) gets 1000 * 0.5 = 500;
// faction 2 (revered > neutral) gets nothing; the empty slot (rate 9.0) is skipped; faction 3
// (hidden, neutral <= exalted) gets 1000 * 1.0 and is not made visible; then 10 itself. The team
// list is not read. One standing packet: 10 first, then every other changed faction in list order.
TEST(ReputationMgr_SpilloverTemplate)
{
    ReputationMgr mgr;
    Wire wire;
    Build(mgr, wire);

    wire.Modify(mgr, 10, 1000);
    CheckEvents(wire.Take(), Cat({ { "tpl 10", "changed 1 rep=500 vis=7 hon=1 rev=1 exa=0" }, Ach(1),
        { "changed 3 rep=1000 vis=7 hon=1 rev=1 exa=0" }, Ach(3),
        { "changed 10 rep=1000 vis=7 hon=1 rev=1 exa=0" }, Ach(10),
        { "0126:" "00000000" "00" "03000000" "1e000000" "e8030000" "15000000" "f4010000" "17000000" "e8030000" } }), __LINE__);
    CHECK_EQ(StandingOf(mgr, 22), 0);
    CHECK_EQ(FlagsOf(mgr, 23), uint32(FACTION_FLAG_HIDDEN));

    // Absolute: the rate applies to the absolute value (1000 * 0.5 = 500 again, as a SET), and a
    // rank increase anywhere lights the packet's flag: 3 goes to 3000 * 1.0 = friendly.
    wire.Set(mgr, 10, 3000);
    CheckEvents(wire.Take(), Cat({ { "tpl 10", "changed 1 rep=1500 vis=7 hon=1 rev=1 exa=0" }, Ach(1),
        { "changed 3 rep=3000 vis=7 hon=1 rev=1 exa=0" }, Ach(3),
        { "changed 10 rep=3000 vis=7 hon=1 rev=1 exa=0" }, Ach(10),
        { "0126:" "00000000" "01" "03000000" "1e000000" "b80b0000" "15000000" "dc050000" "17000000" "b80b0000" } }), __LINE__);

    // The fifth slot counts too (review M-1, X13): a template whose last slot names faction 5 at
    // rate 0.5 up to exalted. Fresh list: 1 gets 500, 2 is skipped (revered > neutral), 3 gets
    // 1000, then slot 4: 5 (neutral <= exalted) gets int32(1000 * 0.5) = 500 and becomes visible
    // (flags 0 by default: the visible packet for list id 25 = 0x19, the count 7 -> 8); then 10.
    // The standing packet: 30 first, then 21, 23, 25 in list order, count 4.
    {
        ReputationMgr fresh;
        Wire full;
        Build(fresh, full);
        full.templates[10].faction[4] = 5;
        full.templates[10].faction_rate[4] = 0.5f;
        full.templates[10].faction_rank[4] = REP_EXALTED;
        full.Modify(fresh, 10, 1000);
        CheckEvents(full.Take(), Cat({ { "tpl 10", "changed 1 rep=500 vis=7 hon=1 rev=1 exa=0" }, Ach(1),
            { "changed 3 rep=1000 vis=7 hon=1 rev=1 exa=0" }, Ach(3),
            { "loading?", "2525:" "19000000", "changed 5 rep=500 vis=8 hon=1 rev=1 exa=0" }, Ach(5),
            { "changed 10 rep=1000 vis=8 hon=1 rev=1 exa=0" }, Ach(10),
            { "0126:" "00000000" "00" "04000000" "1e000000" "e8030000" "15000000" "f4010000" "17000000" "e8030000"
              "19000000" "f4010000" } }), __LINE__);
    }
}

// The team: a child's change spills to its sisters through the parent's team list, each at its
// own incoming rate (mod0) of the child's outgoing share (mod1), unless the sister is above its
// cap; a team-reputation parent takes the share itself and the sisters get nothing; a change to
// the parent spills to every child at full outgoing; an incremental share of 0 is skipped, an
// absolute one is set.
TEST(ReputationMgr_SpilloverTeamParentAndSisters)
{
    {
        ReputationMgr mgr;
        Wire wire;
        Build(mgr, wire);
        // 6: +1000, out 500; 7 (neutral <= friendly) gets int32(500 * 0.25) = 125 and becomes
        // visible (the visible packet at once); 6 itself last.
        wire.Modify(mgr, 6, 1000);
        CheckEvents(wire.Take(), Cat({ { "tpl 6", "team 6", "team 5", "loading?", "2525:" "1b000000",
            "changed 7 rep=125 vis=8 hon=1 rev=1 exa=0" }, Ach(7),
            { "changed 6 rep=1000 vis=8 hon=1 rev=1 exa=0" }, Ach(6),
            { "0126:" "00000000" "00" "02000000" "1a000000" "e8030000" "1b000000" "7d000000" } }), __LINE__);

        // 7 absolute 9000 (honored): out 4500 to 6 (neutral <= revered): int32(4500 * 0.5) = 2250,
        // set absolutely.
        wire.Set(mgr, 7, 9000);
        CheckEvents(wire.Take(), Cat({ { "tpl 7", "team 7", "team 5", "changed 6 rep=2250 vis=8 hon=1 rev=1 exa=0" }, Ach(6),
            { "changed 7 rep=9000 vis=8 hon=2 rev=1 exa=0" }, Ach(7),
            { "0126:" "00000000" "01" "02000000" "1b000000" "28230000" "1a000000" "ca080000" } }), __LINE__);

        // 7 is now honored, above its cap (friendly): 6's change no longer reaches it.
        wire.Modify(mgr, 6, 1000);
        CheckEvents(wire.Take(), Cat({ { "tpl 6", "team 6", "team 5", "changed 6 rep=3250 vis=8 hon=2 rev=1 exa=0" }, Ach(6),
            { "0126:" "00000000" "01" "01000000" "1a000000" "b20c0000" } }), __LINE__);
    }
    {
        // The real Alliance/Horde shape (review M-1, X4): the parent 5 VISIBLE (flags 0x01) but not
        // TEAM_REPUTATION. The sister path still runs -- the parent's own standing is untouched --
        // exactly as in the first block, with the visible count one higher from the start (8 at
        // Initialize, 9 once 7 turns visible). The seeded flags are put back before any check.
        uint32 savedFlags = s_factions[5].ReputationFlags[0];
        s_factions[5].ReputationFlags[0] = FACTION_FLAG_VISIBLE;
        ReputationMgr mgr;
        Wire wire;
        Build(mgr, wire);
        wire.Modify(mgr, 6, 1000);
        s_factions[5].ReputationFlags[0] = savedFlags;
        CheckEvents(wire.Take(), Cat({ { "tpl 6", "team 6", "team 5", "loading?", "2525:" "1b000000",
            "changed 7 rep=125 vis=9 hon=1 rev=1 exa=0" }, Ach(7),
            { "changed 6 rep=1000 vis=9 hon=1 rev=1 exa=0" }, Ach(6),
            { "0126:" "00000000" "00" "02000000" "1a000000" "e8030000" "1b000000" "7d000000" } }), __LINE__);
        CHECK_EQ(FlagsOf(mgr, 25), 0x01u);
        CHECK_EQ(StandingOf(mgr, 25), 0);
    }
    {
        // A team-reputation parent: 9's share int32(1000 * 0.25) = 250 goes to 8, no sister list.
        ReputationMgr mgr;
        Wire wire;
        Build(mgr, wire);
        wire.Modify(mgr, 9, 1000);
        CheckEvents(wire.Take(), Cat({ { "tpl 9", "team 9", "changed 8 rep=250 vis=7 hon=1 rev=1 exa=0" }, Ach(8),
            { "changed 9 rep=1000 vis=7 hon=1 rev=1 exa=0" }, Ach(9),
            { "0126:" "00000000" "00" "02000000" "1d000000" "e8030000" "1c000000" "fa000000" } }), __LINE__);
    }
    {
        // The parent itself: its own team list {6, 7} at full outgoing (1000): 6 gets 500, 7 gets
        // 250 (and becomes visible), then 5 (made visible too).
        ReputationMgr mgr;
        Wire wire;
        Build(mgr, wire);
        wire.Modify(mgr, 5, 1000);
        CheckEvents(wire.Take(), Cat({ { "tpl 5", "team 5", "changed 6 rep=500 vis=7 hon=1 rev=1 exa=0" }, Ach(6),
            { "loading?", "2525:" "1b000000", "changed 7 rep=250 vis=8 hon=1 rev=1 exa=0" }, Ach(7),
            { "loading?", "2525:" "19000000", "changed 5 rep=1000 vis=9 hon=1 rev=1 exa=0" }, Ach(5),
            { "0126:" "00000000" "00" "03000000" "19000000" "e8030000" "1a000000" "f4010000" "1b000000" "fa000000" } }), __LINE__);
    }
    {
        // A zero share: 6 +1 gives 7 int32(0.5 * 0.25) = 0 -- skipped when incremental, set (to
        // 0, which still makes 7 visible) when absolute.
        ReputationMgr mgr;
        Wire wire;
        Build(mgr, wire);
        wire.Modify(mgr, 6, 1);
        CheckEvents(wire.Take(), Cat({ { "tpl 6", "team 6", "team 5", "changed 6 rep=1 vis=7 hon=1 rev=1 exa=0" }, Ach(6),
            { "0126:" "00000000" "00" "01000000" "1a000000" "01000000" } }), __LINE__);
        wire.Set(mgr, 6, 1);
        CheckEvents(wire.Take(), Cat({ { "tpl 6", "team 6", "team 5", "loading?", "2525:" "1b000000",
            "changed 7 rep=0 vis=8 hon=1 rev=1 exa=0" }, Ach(7),
            { "changed 6 rep=1 vis=8 hon=1 rev=1 exa=0" }, Ach(6),
            { "0126:" "00000000" "00" "02000000" "1a000000" "01000000" "1b000000" "00000000" } }), __LINE__);
    }
}

// Visible, at war, inactive: each transition's rule, flag, counter and packet; the loading flag is
// read only when a packet would go, and a loading character gets the change without the packet.
TEST(ReputationMgr_VisibleAtWarInactiveTransitions)
{
    ReputationMgr mgr;
    Wire wire;
    Build(mgr, wire);

    // Not visible (25's default flags are none): it cannot go inactive.
    mgr.SetInactive(25, true);
    CHECK_EQ(FlagsOf(mgr, 25), 0x00u);
    CHECK(!mgr.GetState(RepListID(25))->needSend);

    mgr.SetVisible(F(5), wire.Notify());
    CheckEvents(wire.Take(), { "loading?", "2525:" "19000000" }, __LINE__);
    CHECK_EQ(FlagsOf(mgr, 25), 0x01u);
    CHECK_EQ(uint32(mgr.GetVisibleFactionCount()), 8u);
    CHECK(mgr.GetState(RepListID(25))->needSend);
    CHECK(mgr.GetState(RepListID(25))->needSave);

    mgr.SetVisible(F(5), wire.Notify());                    // already visible
    mgr.SetVisible(F(3), wire.Notify());                    // hidden
    mgr.SetVisible(F(4), wire.Notify());                    // invisible forced
    mgr.SetVisible(F(11), wire.Notify());                   // no list index
    FactionTemplateEntry none = FactionTemplateEntry();
    mgr.SetVisible(&none, wire.Notify());                   // a template of faction 0
    CheckEvents(wire.Take(), {}, __LINE__);
    CHECK_EQ(uint32(mgr.GetVisibleFactionCount()), 8u);

    wire.loading = true;                                    // by template, while loading
    FactionTemplateEntry seven = FactionTemplateEntry();
    seven.Faction = 7;
    mgr.SetVisible(&seven, wire.Notify());
    CheckEvents(wire.Take(), { "loading?" }, __LINE__);
    CHECK_EQ(FlagsOf(mgr, 27), 0x01u);
    CHECK_EQ(uint32(mgr.GetVisibleFactionCount()), 9u);
    wire.loading = false;

    mgr.SetAtWar(25, true, wire.Notify());
    CheckEvents(wire.Take(), { "loading?", "4216:" "19000000" "03" }, __LINE__);
    mgr.SetAtWar(25, true, wire.Notify());                  // already at war
    CheckEvents(wire.Take(), {}, __LINE__);
    mgr.SetAtWar(25, false, wire.Notify());                 // neutral: may call it off
    CheckEvents(wire.Take(), { "loading?", "4216:" "19000000" "01" }, __LINE__);
    CHECK_EQ(FlagsOf(mgr, 25), 0x01u);

    // Peace forced, above hated: refused, and the real flags are sent back.
    mgr.SetAtWar(32, true, wire.Notify());
    CheckEvents(wire.Take(), { "loading?", "4216:" "20000000" "11" }, __LINE__);
    CHECK_EQ(FlagsOf(mgr, 32), 0x11u);
    CHECK(!mgr.GetState(RepListID(32))->needSend);          // a refusal marks nothing (the login list cleared it)

    mgr.SetAtWar(24, false, wire.Notify());                 // invisible forced: silent
    mgr.SetAtWar(23, true, wire.Notify());                  // hidden: silent
    mgr.SetAtWar(99, true, wire.Notify());                  // no such list id
    CheckEvents(wire.Take(), {}, __LINE__);
    CHECK_EQ(FlagsOf(mgr, 24), 0x0Au);

    // Hostile standing: war cannot be called off (the base is included in the rank).
    wire.Set(mgr, 1, -5000);
    wire.Take();
    CHECK_EQ(FlagsOf(mgr, 21), 0x03u);
    mgr.SetAtWar(21, false, wire.Notify());
    CheckEvents(wire.Take(), { "loading?", "4216:" "15000000" "03" }, __LINE__);
    CHECK_EQ(FlagsOf(mgr, 21), 0x03u);

    // A loading character: the flag changes, the packet does not go.
    wire.loading = true;
    mgr.SetAtWar(26, true, wire.Notify());
    CheckEvents(wire.Take(), { "loading?" }, __LINE__);
    CHECK_EQ(FlagsOf(mgr, 26), 0x03u);
    wire.loading = false;

    // Inactive: never a packet; only a visible, not hidden, not invisible faction goes inactive;
    // any faction may go active.
    mgr.SendInitialReputations([](WorldPacket const*) {});
    mgr.SetInactive(25, true);
    CHECK_EQ(FlagsOf(mgr, 25), 0x21u);
    CHECK(mgr.GetState(RepListID(25))->needSend);
    CHECK(mgr.GetState(RepListID(25))->needSave);
    mgr.SetInactive(23, true);                              // hidden
    mgr.SetInactive(24, true);                              // invisible forced
    mgr.SetInactive(99, true);                              // no such list id
    CHECK_EQ(FlagsOf(mgr, 23), 0x04u);
    CHECK_EQ(FlagsOf(mgr, 24), 0x0Au);
    mgr.SetInactive(25, false);
    CHECK_EQ(FlagsOf(mgr, 25), 0x01u);
    CheckEvents(wire.Take(), {}, __LINE__);
}

// The login list: a uint32 0x100, then 256 slots of (flags byte, standing uint32) by list id,
// absent ids as zeros; every needSend is cleared. The standing packet after it carries only the
// changed faction.
TEST(ReputationMgr_SendInitialReputationsBytes)
{
    SeedStores();
    ReputationMgr mgr;
    Wire wire;
    wire.mgr = &mgr;
    mgr.SetOwnerFacts(kRace, kClass, "Kael");
    mgr.Initialize();
    mgr.SetReputation(F(8), 250, wire.Inputs(), wire.Sinks());      // 28 at 250, and its team list {9} at 250 * 1.0
    wire.Take();
    mgr.SetVisible(F(5), wire.Notify());                              // 25: flags 01
    wire.Take();

    std::map<uint32, std::string> slots;
    slots[21] = "01" "00000000";
    slots[22] = "01" "00000000";
    slots[23] = "04" "00000000";
    slots[24] = "0a" "00000000";
    slots[25] = "01" "00000000";
    slots[26] = "01" "00000000";
    slots[27] = "00" "00000000";
    slots[28] = "81" "fa000000";
    slots[29] = "01" "fa000000";
    slots[30] = "01" "00000000";
    slots[32] = "11" "00000000";
    std::string want = "4634:" "00010000";
    for (uint32 a = 0; a < 256; ++a)
    {
        std::map<uint32, std::string>::const_iterator itr = slots.find(a);
        want += itr != slots.end() ? itr->second : std::string("00" "00000000");
    }

    Events sent;
    mgr.SendInitialReputations([&sent](WorldPacket const* packet) { sent.push_back(PacketText(*packet)); });
    REQUIRE(sent.size() == 1u);
    CHECK_EQ(sent[0].size(), size_t(5 + 2 * (4 + 256 * 5)));
    CHECK_STR(sent[0], want);
    for (FactionStateList::const_iterator itr = mgr.GetStateList().begin(); itr != mgr.GetStateList().end(); ++itr)
    {
        CHECK(!itr->second.needSend);
    }

    wire.Modify(mgr, 1, 10);
    Events events = wire.Take();
    CHECK_STR(events.back(), "0126:" "00000000" "00" "01000000" "15000000" "0a000000");
}

// The forced reactions: a count, then (faction id, rank) pairs in faction id order; removing one
// drops it; the lookup by faction template.
TEST(ReputationMgr_ForcedReactionsBytes)
{
    ReputationMgr mgr;
    Events sent;
    ManagerPacketSink sink = [&sent](WorldPacket const* packet) { sent.push_back(PacketText(*packet)); };

    mgr.SendForceReactions(sink);
    mgr.ApplyForceReaction(5, REP_HOSTILE, true);
    mgr.ApplyForceReaction(1, REP_EXALTED, true);
    mgr.SendForceReactions(sink);
    FactionTemplateEntry five = FactionTemplateEntry();
    five.Faction = 5;
    REQUIRE(mgr.GetForcedRankIfAny(&five) != NULL);
    CHECK_EQ(int(*mgr.GetForcedRankIfAny(&five)), int(REP_HOSTILE));
    mgr.ApplyForceReaction(5, REP_HOSTILE, false);
    CHECK(mgr.GetForcedRankIfAny(&five) == NULL);
    mgr.SendForceReactions(sink);
    CheckEvents(sent, {
        "4615:" "00000000",
        "4615:" "02000000" "01000000" "07000000" "05000000" "01000000",
        "4615:" "01000000" "01000000" "07000000" }, __LINE__);
}

// The per-row load over the initialized list: the standing, the counters from the base rank to the
// loaded rank, the flags through the same transitions (hidden/invisible and peace-forced rules
// apply), a forced reaction or a hostile rank declares war, and a row whose resulting flags equal
// the saved ones is marked neither to send nor to save. Unknown or index-less factions do nothing.
TEST(ReputationMgr_LoadRowValidAndInvalidRows)
{
    ReputationMgr mgr;
    Wire wire;
    SeedStores();
    wire.mgr = &mgr;
    mgr.SetOwnerFacts(kRace, kClass, "Kael");
    mgr.Initialize();
    mgr.ApplyForceReaction(6, REP_HOSTILE, true);           // before the load, as an aura would be

    // 1: 5000 (friendly), flags VISIBLE|INACTIVE (33): inactive set, nothing sent, clean.
    LoadOne(mgr, wire, RepRow("1", "5000", "33"));
    CheckEvents(wire.Take(), {}, __LINE__);
    CHECK_EQ(StandingOf(mgr, 21), 5000);
    CHECK_EQ(FlagsOf(mgr, 21), 0x21u);
    CHECK(!mgr.GetState(RepListID(21))->needSend);
    CHECK(!mgr.GetState(RepListID(21))->needSave);

    // 5 while loading: -10000 (hated), flags VISIBLE|AT_WAR (3): made visible (counted), war set,
    // the loading flag read twice and nothing sent; the hostile check finds war already set.
    wire.loading = true;
    LoadOne(mgr, wire, RepRow("5", "-10000", "3"));
    CheckEvents(wire.Take(), { "loading?", "loading?" }, __LINE__);
    CHECK_EQ(StandingOf(mgr, 25), -10000);
    CHECK_EQ(FlagsOf(mgr, 25), 0x03u);
    CHECK_EQ(uint32(mgr.GetVisibleFactionCount()), 8u);
    CHECK(!mgr.GetState(RepListID(25))->needSend);
    wire.loading = false;

    // 6 with a forced HOSTILE reaction: saved VISIBLE (1), neutral; the force declares war (sent),
    // so the flags (3) differ from the saved ones and the row stays marked.
    LoadOne(mgr, wire, RepRow("6", "0", "1"));
    CheckEvents(wire.Take(), { "loading?", "4216:" "1a000000" "03" }, __LINE__);
    CHECK_EQ(FlagsOf(mgr, 26), 0x03u);
    CHECK(mgr.GetState(RepListID(26))->needSend);
    CHECK(mgr.GetState(RepListID(26))->needSave);

    // 12 peace forced, saved at war at 0: war refused (the flags sent back), not at war.
    LoadOne(mgr, wire, RepRow("12", "0", "19"));
    CheckEvents(wire.Take(), { "loading?", "4216:" "20000000" "11" }, __LINE__);
    CHECK_EQ(FlagsOf(mgr, 32), 0x11u);

    // 2 on a base of 21000: 21000 more is 42000, exalted; the counters move from the base rank
    // (revered) to exalted.
    LoadOne(mgr, wire, RepRow("2", "21000", "1"));
    CHECK_EQ(mgr.GetReputation(F(2)), 42000);
    CHECK_EQ(uint32(mgr.GetHonoredFactionCount()), 1u);
    CHECK_EQ(uint32(mgr.GetReveredFactionCount()), 1u);
    CHECK_EQ(uint32(mgr.GetExaltedFactionCount()), 1u);

    // 1 again at -4000 (hostile) saved not at war, visible: war is declared by the rank (the
    // inactive bit from its first row is still there: 0x21 | AT_WAR = 0x23).
    LoadOne(mgr, wire, RepRow("1", "-4000", "1"));
    CheckEvents(wire.Take(), { "loading?", "4216:" "15000000" "23" }, __LINE__);

    // Unknown faction and a faction without a list index: nothing at all.
    size_t before = mgr.GetStateList().size();
    LoadOne(mgr, wire, RepRow("99", "100", "1"));
    LoadOne(mgr, wire, RepRow("11", "100", "1"));
    CheckEvents(wire.Take(), {}, __LINE__);
    CHECK_EQ(mgr.GetStateList().size(), before);

    // At war by team at load (review M-1, X7): race mask 0x8 fits faction 2's row 0 (base -10000,
    // default flags AT_WAR only, not visible). A saved row (standing 20000, flags 0) is 10000 in
    // all: honored (the counter moves from the base rank, hated). Saved not at war, but the faction
    // is not visible, so the war is NOT called off; honored is not hostile, so none is declared.
    // Nothing is sent, and the flags (2) differ from the saved ones (0): the row stays marked.
    ReputationMgr enemy;
    Wire enemyWire;
    enemyWire.mgr = &enemy;
    enemy.SetOwnerFacts(0x8, kClass, "Kael");
    enemy.Initialize();
    CHECK_EQ(FlagsOf(enemy, 22), 0x02u);
    CHECK_EQ(uint32(enemy.GetHonoredFactionCount()), 0u);
    LoadOne(enemy, enemyWire, RepRow("2", "20000", "0"));
    CheckEvents(enemyWire.Take(), {}, __LINE__);
    CHECK_EQ(FlagsOf(enemy, 22), 0x02u);
    CHECK_EQ(StandingOf(enemy, 22), 20000);
    CHECK_EQ(enemy.GetReputation(F(2)), 10000);
    CHECK_EQ(uint32(enemy.GetHonoredFactionCount()), 1u);
    CHECK(enemy.GetState(RepListID(22))->needSend);
    CHECK(enemy.GetState(RepListID(22))->needSave);
}

// The save: every faction marked to save, in list order, a DELETE then an INSERT of (owner guid,
// faction ID -- not the list id --, standing relative to the base, flags), queued (no blocking
// call); then nothing is marked; a later change saves just that faction.
TEST(ReputationMgr_SaveStatementStream)
{
    Db db;
    ReputationMgr mgr;
    Wire wire;
    Build(mgr, wire);

    {
        TickGuard::Scope scope;
        mgr.SaveToDB(kOwner);
        CHECK_EQ(db.async.executed.size(), size_t(0));
        CHECK_EQ(TickGuard::Violations(), 0u);
    }
    std::vector<std::string> want;
    const uint32 ids[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12 };
    const uint32 flags[] = { 1, 1, 4, 10, 0, 1, 0, 129, 1, 1, 17 };
    for (size_t i = 0; i < 11; ++i)
    {
        want.push_back(DeleteSql(ids[i]));
        want.push_back(InsertSql(ids[i], 0, flags[i]));
    }
    CheckEvents(db.Take(), want, __LINE__);

    mgr.SaveToDB(kOwner);
    CheckEvents(db.Take(), {}, __LINE__);

    wire.Modify(mgr, 2, -500);                              // 21000 - 500 = 20500 total, -500 stored
    wire.Take();
    mgr.SaveToDB(kOwner);
    CheckEvents(db.Take(), { DeleteSql(2), InsertSql(2, -500, 1) }, __LINE__);
}

// EXHAUSTIVE, the owner facts: for every race (0..22) and class (1..11), the OLD expression --
// the faction's first row fitting (1 << (race - 1), or 0 for race 0) and (1 << (class - 1)), as
// Unit::getRaceMask/getClassMask give them -- against the manager's base reputation, default
// flags and base rank for every seeded faction, with the owner facts set from those masks. The
// fit is written out here, not taken from FactionEntry::GetIndexFitTo. The CONTROL sets the facts
// with race and class swapped and must disagree somewhere.
TEST(ReputationMgr_OwnerFactsExhaustiveAgainstTheOldReads)
{
    SeedStores();
    auto oldIndex = [](FactionEntry const* f, uint32 race, uint32 cls) -> int
    {
        uint32 raceMask = race ? 1u << (race - 1) : 0u;
        uint32 classMask = 1u << (cls - 1);
        for (int i = 0; i < 4; ++i)
        {
            bool raceFits = f->ReputationRaceMask[i] == 0 || (f->ReputationRaceMask[i] & raceMask) != 0;
            bool classFits = f->ReputationClassMask[i] == 0 || (f->ReputationClassMask[i] & classMask) != 0;
            if (raceFits && classFits)
            {
                return i;
            }
        }
        return -1;
    };
    size_t combos = 0, mismatches = 0, controlMismatches = 0;
    for (uint32 race = 0; race <= 22; ++race)
    {
        for (uint32 cls = 1; cls <= 11; ++cls)
        {
            uint32 raceMask = race ? 1u << (race - 1) : 0u;
            uint32 classMask = 1u << (cls - 1);
            ReputationMgr mgr;
            mgr.SetOwnerFacts(raceMask, classMask, "Kael");
            mgr.Initialize();
            ReputationMgr control;
            control.SetOwnerFacts(classMask, raceMask, "Kael");
            control.Initialize();
            for (uint32 id = 1; id <= 12; ++id)
            {
                FactionEntry const* f = F(id);
                int idx = oldIndex(f, race, cls);
                int32 base = idx >= 0 ? f->ReputationBase[idx] : 0;
                uint32 flags = idx >= 0 ? f->ReputationFlags[idx] : 0;
                ++combos;
                bool same = mgr.GetBaseReputation(f) == base && mgr.GetBaseRank(f) == HandRank(base)
                    && (f->ReputationIndex < 0 || FlagsOf(mgr, RepListID(f->ReputationIndex)) == flags);
                mismatches += same ? 0 : 1;
                bool controlSame = control.GetBaseReputation(f) == base
                    && (f->ReputationIndex < 0 || FlagsOf(control, RepListID(f->ReputationIndex)) == flags);
                controlMismatches += controlSame ? 0 : 1;
            }
        }
    }
    CHECK_EQ(combos, size_t(23 * 11 * 12));
    CHECK_EQ(mismatches, size_t(0));
    CHECK(controlMismatches > 0);
    std::printf("  owner facts: %u combinations, %u mismatches, control %u mismatches\n",
        unsigned(combos), unsigned(mismatches), unsigned(controlMismatches));
}

// EXHAUSTIVE, the spillover conditions. (a) The template: the OLD condition was the owner's
// GetReputationRank(target) <= the row's rank, and the owner's GetReputationRank is
// GetReputationMgr().GetRank(sFactionStore.LookupEntry(target)); the manager now asks itself.
// For every template rank (hated..exalted) and every target standing at a boundary (both sides),
// the target spills exactly when HandRank(base + standing) <= rank. (b) The sister cap: a sister
// spills unless HandRank(its reputation) > its cap, for every cap and the same standings. The
// CONTROLS (< for <=, >= for >) must disagree somewhere.
TEST(ReputationMgr_SpilloverConditionsExhaustive)
{
    SeedStores();
    const int32 standings[] = { -42000, -6001, -6000, -3001, -3000, -1, 0, 2999, 3000, 8999, 9000,
                                20999, 21000, 41999, 42000, 42999 };
    size_t combos = 0, mismatches = 0, controlMismatches = 0;
    for (uint32 rank = REP_HATED; rank <= REP_EXALTED; ++rank)
    {
        for (int32 standing : standings)
        {
            // (a) template: source 10 -> target 1 at `rank`, rate 1.0.
            {
                ReputationMgr mgr;
                Wire wire;
                Build(mgr, wire);
                RepSpilloverTemplate t = RepSpilloverTemplate();
                t.faction[0] = 1;
                t.faction_rate[0] = 1.0f;
                t.faction_rank[0] = rank;
                wire.templates[10] = t;
                wire.Set(mgr, 1, standing);
                int32 before = mgr.GetReputation(F(1));
                wire.Take();
                wire.Modify(mgr, 10, 2);
                bool spilled = mgr.GetReputation(F(1)) != before
                    || (before >= ReputationMgr::Reputation_Cap - 1);   // at the cap a change may not move it
                bool changedEvent = false;
                for (std::string const& e : wire.Take())
                {
                    changedEvent = changedEvent || e.compare(0, 10, "changed 1 ") == 0;
                }
                bool oldSpills = HandRank(before) <= ReputationRank(rank);
                bool controlSpills = HandRank(before) < ReputationRank(rank);
                ++combos;
                mismatches += (changedEvent == oldSpills && (!changedEvent || spilled)) ? 0 : 1;
                controlMismatches += changedEvent == controlSpills ? 0 : 1;
            }
            // (b) sister cap: 6 -> 7 through parent 5, 7's cap set to `rank`.
            {
                ReputationMgr mgr;
                Wire wire;
                Build(mgr, wire);
                uint32 savedCap = s_factions[7].ParentFactionCap_0;
                s_factions[7].ParentFactionCap_0 = rank;
                wire.Set(mgr, 7, standing);
                int32 before = mgr.GetReputation(F(7));
                wire.Take();
                wire.Modify(mgr, 6, 1000);                  // share int32(500 * 0.25) = 125
                bool changedEvent = false;
                for (std::string const& e : wire.Take())
                {
                    changedEvent = changedEvent || e.compare(0, 10, "changed 7 ") == 0;
                }
                s_factions[7].ParentFactionCap_0 = savedCap;
                bool oldSpills = !(HandRank(before) > ReputationRank(rank));
                bool controlSpills = !(HandRank(before) >= ReputationRank(rank));
                ++combos;
                mismatches += changedEvent == oldSpills ? 0 : 1;
                controlMismatches += changedEvent == controlSpills ? 0 : 1;
            }
        }
    }
    CHECK_EQ(combos, size_t(8 * 16 * 2));
    CHECK_EQ(mismatches, size_t(0));
    CHECK(controlMismatches > 0);
    std::printf("  spillover: %u combinations, %u mismatches, control %u mismatches\n",
        unsigned(combos), unsigned(mismatches), unsigned(controlMismatches));
}
