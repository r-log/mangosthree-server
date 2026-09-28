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

/// Decoupling D4k: a character's friend and ignore list, its statements and packets, and the
/// online-visibility verdict, with no character.
///
/// Before this PR PlayerSocial found its owner through the object manager to send the contact list,
/// and the verdict lived inside SocialMgr::GetFriendInfo and ::BroadcastToFriendListers, over two
/// live characters. Now the list takes the per-entry fill and the packet sink as parameters, and
/// the verdict is a free function over plain facts (SocialList.h), so every case builds what it
/// needs from nothing.
///
/// The statements run through the GLOBAL CharacterDatabase with D7a's fakes attached and
/// asynchronous writes on: they are queued, and what the delay thread would have sent to MySQL is
/// the exact SQL asserted after ExecuteQueuedForTest(). The note is a bound statement, which
/// arrives through SqlPlainPreparedStatement with its parameters quoted (the fake connection's
/// escape copies the text as it is).
///
/// Bytes, by hand: a guid is `ObjectGuid(HIGHGUID_PLAYER, low)`, HIGHGUID_PLAYER = 0, so the raw
/// value is the low guid, written as a little-endian uint64 (low 7 -> 0700000000000000). A string
/// is its characters and a 0 byte ("hi" -> 686900). SMSG_CONTACT_LIST = 0x6017 and
/// SMSG_FRIEND_STATUS = 0x0717 (Opcodes.h); PacketText prints the opcode as four hex digits.
/// The owner is low guid 42 everywhere, and every listed guid differs from it and from the flag
/// values, so a swapped pair of arguments shows.

#include "TestHarness.h"
#include "FakeDatabase.h"
#include "Database/DatabaseEnv.h"
#include "Database/TickGuard.h"
#include "Opcodes.h"
#include "WorldPacket.h"
#include "SocialList.h"

#include <map>
#include <string>
#include <vector>

namespace
{
    const uint32 kOwner = 42;

    static_assert(SMSG_CONTACT_LIST == 0x6017, "the contact list opcode, by hand above");
    static_assert(SMSG_FRIEND_STATUS == 0x0717, "the friend status opcode, by hand above");
    static_assert(HIGHGUID_PLAYER == 0, "a character guid's raw value is its low guid");
    static_assert(SOCIALMGR_FRIEND_LIMIT == 50 && SOCIALMGR_IGNORE_LIMIT == 50, "the two limits");

    ObjectGuid Guid(uint32 low)
    {
        return ObjectGuid(HIGHGUID_PLAYER, low);
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

    /// Records every packet the sink receives.
    struct Sink
    {
        std::vector<std::string> packets;

        ManagerPacketSink Get()
        {
            return [this](WorldPacket const* packet)
            {
                packets.push_back(PacketText(*packet));
            };
        }
    };

    /// A fill from a table: what the requester may see of each listed character. Records the guids
    /// in call order. An unknown guid is filled as nothing (the entry keeps what it has).
    struct Fill
    {
        std::map<uint32, FriendInfo> seen;
        std::vector<uint32> calls;

        FriendInfoFill Get()
        {
            return [this](uint32 friendLowGuid, FriendInfo& info)
            {
                calls.push_back(friendLowGuid);
                std::map<uint32, FriendInfo>::const_iterator itr = seen.find(friendLowGuid);
                if (itr == seen.end())
                {
                    return;
                }
                info.Status = itr->second.Status;
                info.Area = itr->second.Area;
                info.Level = itr->second.Level;
                info.Class = itr->second.Class;
            };
        }

        void Online(uint32 low, FriendStatus status, uint32 area, uint32 level, uint32 cls)
        {
            FriendInfo info;
            info.Status = status;
            info.Area = area;
            info.Level = level;
            info.Class = cls;
            seen[low] = info;
        }
    };

    FakeRow SocialRow(uint32 friendLow, uint32 flags, std::string const& note)
    {
        FakeRow row;
        row.push_back(std::to_string(friendLow));
        row.push_back(std::to_string(flags));
        row.push_back(note);
        return row;
    }

    /// Loads one row into `social` through the per-row load, with the caller's counters.
    void LoadOne(PlayerSocial& social, uint32 friendLow, uint32 flags, std::string const& note,
                 uint32& friendCounter, uint32& ignoreCounter)
    {
        FakeQueryResult result(FakeRows{ SocialRow(friendLow, flags, note) });
        if (!result.NextRow())
        {
            testing::ReportFailure(__FILE__, __LINE__, "the fake row did not load");
            return;
        }
        social.LoadRow(result.Fetch(), friendCounter, ignoreCounter);
    }

    std::string FlagsSql(char op, uint32 flag, uint32 friendLow)
    {
        return std::string("UPDATE `character_social` SET `flags` = (`flags` ") + (op == '|' ? "| " : "& ~")
            + std::to_string(flag) + ") WHERE `guid` = '42' AND `friend` = '" + std::to_string(friendLow) + "'";
    }

    std::string InsertSql(uint32 friendLow, uint32 flag)
    {
        return "INSERT INTO `character_social` (`guid`, `friend`, `flags`) VALUES ('42', '"
            + std::to_string(friendLow) + "', '" + std::to_string(flag) + "')";
    }

    std::string DeleteSql(uint32 friendLow)
    {
        return "DELETE FROM `character_social` WHERE `guid` = '42' AND `friend` = '" + std::to_string(friendLow) + "'";
    }

    void CheckLines(std::vector<std::string> const& got, std::vector<std::string> const& want, int line)
    {
        if (got != want)
        {
            std::string text = "lines differ (case line " + std::to_string(line) + "): got [";
            for (size_t i = 0; i < got.size(); ++i)
            {
                text += (i ? ", " : "") + got[i];
            }
            text += "] want [";
            for (size_t i = 0; i < want.size(); ++i)
            {
                text += (i ? ", " : "") + want[i];
            }
            testing::ReportFailure(__FILE__, line, text + "]");
        }
    }

    /// The fakes on the global character database, asynchronous writes on, and a way to read what
    /// was executed since the last read.
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
}

// The flag merge. A friend added then ignored is ONE entry with both flags: the second add ORs the
// bit in (UPDATE ... | 2), it does not insert again. Removing one flag keeps the other (UPDATE ...
// & ~1); removing the last deletes the row and the entry. Removing an unlisted guid does nothing;
// removing a flag the entry does not have still writes the AND (KEPT); adding a flag the entry
// already has still writes the OR (KEPT). Every statement is queued, none blocks.
TEST(SocialMgr_AddRemoveFlagMergeStatements)
{
    Db db;
    PlayerSocial social;
    social.SetPlayerGuid(Guid(kOwner));

    {
        TickGuard::Scope scope;
        CHECK(social.AddToSocialList(Guid(7), false));
        CHECK(social.AddToSocialList(Guid(7), true));
        CHECK(social.AddToSocialList(Guid(9), true));
        CHECK_EQ(db.async.executed.size(), size_t(0));      // queued, not run on the tick
        CHECK_EQ(TickGuard::Violations(), 0u);
    }
    CheckLines(db.Take(), { InsertSql(7, 1), FlagsSql('|', 2, 7), InsertSql(9, 2) }, __LINE__);
    CHECK(social.HasFriend(Guid(7)));
    CHECK(social.HasIgnore(Guid(7)));
    CHECK(!social.HasFriend(Guid(9)));
    CHECK(social.HasIgnore(Guid(9)));
    CHECK(!social.HasFriend(Guid(5)));
    CHECK(!social.HasIgnore(Guid(5)));
    CHECK_EQ(social.GetNumberOfSocialsWithFlag(SOCIAL_FLAG_FRIEND), 1u);
    CHECK_EQ(social.GetNumberOfSocialsWithFlag(SOCIAL_FLAG_IGNORED), 2u);

    // One flag off, the other stays.
    social.RemoveFromSocialList(Guid(7), false);
    CheckLines(db.Take(), { FlagsSql('&', 1, 7) }, __LINE__);
    CHECK(!social.HasFriend(Guid(7)));
    CHECK(social.HasIgnore(Guid(7)));

    // The last flag off: the row and the entry go.
    social.RemoveFromSocialList(Guid(7), true);
    CheckLines(db.Take(), { DeleteSql(7) }, __LINE__);
    CHECK(!social.HasIgnore(Guid(7)));
    CHECK_EQ(social.GetNumberOfSocialsWithFlag(SOCIAL_FLAG_IGNORED), 1u);

    // Unlisted: nothing at all.
    social.RemoveFromSocialList(Guid(5), false);
    social.RemoveFromSocialList(Guid(7), true);
    CheckLines(db.Take(), {}, __LINE__);

    // KEPT: a flag the entry does not have is still ANDed out, and one it has is still ORed in.
    social.RemoveFromSocialList(Guid(9), false);
    CHECK(social.AddToSocialList(Guid(9), true));
    CheckLines(db.Take(), { FlagsSql('&', 1, 9), FlagsSql('|', 2, 9) }, __LINE__);
    CHECK(social.HasIgnore(Guid(9)));
    CHECK(!social.HasFriend(Guid(9)));

    // The literal text of each kind once, so the helpers themselves are pinned.
    REQUIRE(db.async.executed.size() == size_t(7));
    CHECK_STR(db.async.executed[0], "INSERT INTO `character_social` (`guid`, `friend`, `flags`) VALUES ('42', '7', '1')");
    CHECK_STR(db.async.executed[1], "UPDATE `character_social` SET `flags` = (`flags` | 2) WHERE `guid` = '42' AND `friend` = '7'");
    CHECK_STR(db.async.executed[3], "UPDATE `character_social` SET `flags` = (`flags` & ~1) WHERE `guid` = '42' AND `friend` = '7'");
    CHECK_STR(db.async.executed[4], "DELETE FROM `character_social` WHERE `guid` = '42' AND `friend` = '7'");
    CHECK_EQ(db.query.executed.size(), size_t(0));
}

// The two limits are counted apart, by flag, and checked BEFORE the entry is looked up: at 50
// friends a 51st friend is refused, and so is the friend flag on an entry already listed as a
// friend; the ignore list still takes an ignore, merged into a listed friend. A refused add writes
// nothing and changes nothing. Freeing one friend slot lets a friend in again.
TEST(SocialMgr_LimitsCountedPerFlag)
{
    Db db;
    PlayerSocial social;
    social.SetPlayerGuid(Guid(kOwner));

    for (uint32 low = 100; low < 150; ++low)
    {
        CHECK(social.AddToSocialList(Guid(low), false));
    }
    CHECK_EQ(db.Take().size(), size_t(50));
    CHECK_EQ(social.GetNumberOfSocialsWithFlag(SOCIAL_FLAG_FRIEND), 50u);

    CHECK(!social.AddToSocialList(Guid(150), false));       // the 51st friend
    CHECK(!social.AddToSocialList(Guid(100), false));       // a listed friend, at the limit
    CheckLines(db.Take(), {}, __LINE__);
    CHECK(!social.HasFriend(Guid(150)));
    CHECK_EQ(social.GetNumberOfSocialsWithFlag(SOCIAL_FLAG_FRIEND), 50u);

    CHECK(social.AddToSocialList(Guid(100), true));         // the ignore list is empty
    CheckLines(db.Take(), { FlagsSql('|', 2, 100) }, __LINE__);
    for (uint32 low = 200; low < 249; ++low)
    {
        CHECK(social.AddToSocialList(Guid(low), true));
    }
    CHECK_EQ(db.Take().size(), size_t(49));
    CHECK_EQ(social.GetNumberOfSocialsWithFlag(SOCIAL_FLAG_IGNORED), 50u);
    CHECK(!social.AddToSocialList(Guid(249), true));        // the 51st ignore
    CheckLines(db.Take(), {}, __LINE__);
    CHECK(!social.HasIgnore(Guid(249)));

    social.RemoveFromSocialList(Guid(101), false);          // 49 friends
    CHECK(social.AddToSocialList(Guid(150), false));
    CheckLines(db.Take(), { DeleteSql(101), InsertSql(150, 1) }, __LINE__);
    CHECK_EQ(social.GetNumberOfSocialsWithFlag(SOCIAL_FLAG_FRIEND), 50u);
    CHECK_EQ(social.GetNumberOfSocialsWithFlag(SOCIAL_FLAG_IGNORED), 50u);
    CHECK_EQ(social.GetNumberOfSocialsWithFlag(SOCIAL_FLAG_MUTED), 0u);
}

// A note is set only on a listed character (friend or ignore alike), cut to 48 characters, and
// written through the bound statement; the stored note is the cut text, shown by the list packet.
TEST(SocialMgr_SetFriendNote)
{
    Db db;
    PlayerSocial social;
    social.SetPlayerGuid(Guid(kOwner));

    social.SetFriendNote(Guid(7), "nobody");
    CheckLines(db.Take(), {}, __LINE__);
    CHECK(!social.HasFriend(Guid(7)) && !social.HasIgnore(Guid(7)));

    CHECK(social.AddToSocialList(Guid(7), false));
    {
        TickGuard::Scope scope;
        social.SetFriendNote(Guid(7), "hi");
        social.SetFriendNote(Guid(7), std::string(60, 'a'));
        CHECK_EQ(TickGuard::Violations(), 0u);
    }
    CheckLines(db.Take(), {
        InsertSql(7, 1),
        "UPDATE `character_social` SET `note` = 'hi' WHERE `guid` = '42' AND `friend` = '7'",
        "UPDATE `character_social` SET `note` = '" + std::string(48, 'a') + "' WHERE `guid` = '42' AND `friend` = '7'",
    }, __LINE__);
    CHECK_EQ(db.query.executed.size(), size_t(0));

    // The stored note: guid 7, flags 1, 48 'a' and the 0, then the friend's status byte (offline).
    Sink sink;
    Fill fill;
    social.SendSocialList(fill.Get(), sink.Get());
    REQUIRE(sink.packets.size() == size_t(1));
    std::string aaa;
    for (int i = 0; i < 48; ++i)
    {
        aaa += "61";
    }
    CHECK_STR(sink.packets[0], "6017:0700000001000000" "0700000000000000" "01000000" + aaa + "00" "00");
}

// The contact list, entry by entry in the list's (guid) order, each filled first:
//   header: uint32 7, uint32 4 entries                        07000000 04000000
//   7  friend, note "hi", DND in area 1519, level 85, class 8 0700000000000000 01000000 686900 04 ef050000 55000000 08000000
//   9  friend, no note, offline (status 0: no area/level/class) 0900000000000000 01000000 00 00
//   11 ignored, note "x": no status at all, although the fill says online
//                                                             0b00000000000000 02000000 7800
//   13 both flags, AFK in area 12, level 10, class 1          0d00000000000000 03000000 00 02 0c000000 0a000000 01000000
// The fill writes into the STORED entry (KEPT): a second send with a fill that knows nobody sends
// the same bytes. An empty list is the header with 0 entries. One packet per call.
TEST(SocialMgr_ContactListBytes)
{
    Db db;
    PlayerSocial social;
    social.SetPlayerGuid(Guid(kOwner));
    uint32 friends = 0;
    uint32 ignores = 0;

    Sink sink;
    Fill fill;
    social.SendSocialList(fill.Get(), sink.Get());
    REQUIRE(sink.packets.size() == size_t(1));
    CHECK_STR(sink.packets[0], "6017:0700000000000000");
    CHECK(fill.calls.empty());

    LoadOne(social, 13, 3, "", friends, ignores);
    LoadOne(social, 11, 2, "x", friends, ignores);
    LoadOne(social, 9, 1, "", friends, ignores);
    LoadOne(social, 7, 1, "hi", friends, ignores);
    fill.Online(7, FRIEND_STATUS_DND, 1519, 85, 8);
    fill.Online(11, FRIEND_STATUS_ONLINE, 99, 98, 97);
    fill.Online(13, FRIEND_STATUS_AFK, 12, 10, 1);

    const std::string want = std::string("6017:") + "07000000" "04000000"
        "0700000000000000" "01000000" "686900" "04" "ef050000" "55000000" "08000000"
        "0900000000000000" "01000000" "00" "00"
        "0b00000000000000" "02000000" "7800"
        "0d00000000000000" "03000000" "00" "02" "0c000000" "0a000000" "01000000";

    sink.packets.clear();
    social.SendSocialList(fill.Get(), sink.Get());
    REQUIRE(sink.packets.size() == size_t(1));
    CHECK_STR(sink.packets[0], want);
    CHECK(fill.calls == std::vector<uint32>({ 7, 9, 11, 13 }));

    Fill nobody;
    social.SendSocialList(nobody.Get(), sink.Get());
    REQUIRE(sink.packets.size() == size_t(2));
    CHECK_STR(sink.packets[1], want);
    CHECK(nobody.calls == std::vector<uint32>({ 7, 9, 11, 13 }));

    CheckLines(db.Take(), {}, __LINE__);                    // the list writes nothing
}

// The per-row load. Each row's own flag meets its own limit; a row with both flags meets both
// checks but counts as an ignore only (KEPT); a row with neither flag counts as a friend and meets
// no limit. A skipped row is not listed and moves no counter. The load writes nothing.
TEST(SocialMgr_LoadRowLimits)
{
    Db db;
    PlayerSocial social;
    social.SetPlayerGuid(Guid(kOwner));
    uint32 friends = 0;
    uint32 ignores = 0;

    LoadOne(social, 7, SOCIAL_FLAG_FRIEND, "a", friends, ignores);
    CHECK_EQ(friends, 1u);
    CHECK_EQ(ignores, 0u);
    LoadOne(social, 11, SOCIAL_FLAG_IGNORED, "", friends, ignores);
    CHECK_EQ(friends, 1u);
    CHECK_EQ(ignores, 1u);
    LoadOne(social, 13, SOCIAL_FLAG_FRIEND | SOCIAL_FLAG_IGNORED, "", friends, ignores);
    CHECK_EQ(friends, 1u);                                  // both flags: an ignore only
    CHECK_EQ(ignores, 2u);
    LoadOne(social, 15, SOCIAL_FLAG_MUTED, "", friends, ignores);
    CHECK_EQ(friends, 2u);                                  // neither flag: a friend
    CHECK_EQ(ignores, 2u);
    CHECK(social.HasFriend(Guid(7)) && !social.HasIgnore(Guid(7)));
    CHECK(social.HasIgnore(Guid(11)) && !social.HasFriend(Guid(11)));
    CHECK(social.HasIgnore(Guid(13)) && social.HasFriend(Guid(13)));
    CHECK(!social.HasIgnore(Guid(15)) && !social.HasFriend(Guid(15)));

    friends = SOCIALMGR_FRIEND_LIMIT;                       // the friend limit reached
    LoadOne(social, 17, SOCIAL_FLAG_FRIEND, "", friends, ignores);
    CHECK(!social.HasFriend(Guid(17)));
    CHECK_EQ(friends, 50u);
    LoadOne(social, 19, SOCIAL_FLAG_IGNORED, "", friends, ignores);
    CHECK(social.HasIgnore(Guid(19)));                      // the other limit is not reached
    CHECK_EQ(ignores, 3u);
    LoadOne(social, 21, SOCIAL_FLAG_FRIEND | SOCIAL_FLAG_IGNORED, "", friends, ignores);
    CHECK(!social.HasIgnore(Guid(21)));                     // both flags: the friend check skips it
    CHECK_EQ(ignores, 3u);
    LoadOne(social, 23, SOCIAL_FLAG_MUTED, "", friends, ignores);
    CHECK_EQ(friends, 51u);                                 // neither flag meets no limit

    friends = 0;
    ignores = SOCIALMGR_IGNORE_LIMIT;                       // the ignore limit reached
    LoadOne(social, 25, SOCIAL_FLAG_IGNORED, "", friends, ignores);
    LoadOne(social, 27, SOCIAL_FLAG_FRIEND | SOCIAL_FLAG_IGNORED, "", friends, ignores);
    CHECK(!social.HasIgnore(Guid(25)));
    CHECK(!social.HasIgnore(Guid(27)));                     // both flags: the ignore check skips it
    CHECK_EQ(ignores, 50u);
    LoadOne(social, 29, SOCIAL_FLAG_FRIEND, "", friends, ignores);
    CHECK(social.HasFriend(Guid(29)));
    CHECK_EQ(friends, 1u);

    CHECK_EQ(social.GetNumberOfSocialsWithFlag(SOCIAL_FLAG_FRIEND), 3u);    // 7, 13, 29
    CHECK_EQ(social.GetNumberOfSocialsWithFlag(SOCIAL_FLAG_IGNORED), 3u);   // 11, 13, 19
    CHECK_EQ(social.GetNumberOfSocialsWithFlag(SOCIAL_FLAG_MUTED), 2u);     // 15, 23

    // The loaded note and flags, through the list packet: 7 "a" (flags 1, offline), 11 (2), 13 (3,
    // offline), 15 (4: no status), 19 (2), 23 (4), 29 (1, offline).
    Sink sink;
    Fill fill;
    social.SendSocialList(fill.Get(), sink.Get());
    REQUIRE(sink.packets.size() == size_t(1));
    CHECK_STR(sink.packets[0], std::string("6017:") + "07000000" "07000000"
        "0700000000000000" "01000000" "6100" "00"
        "0b00000000000000" "02000000" "00"
        "0d00000000000000" "03000000" "00" "00"
        "0f00000000000000" "04000000" "00"
        "1300000000000000" "02000000" "00"
        "1700000000000000" "04000000" "00"
        "1d00000000000000" "01000000" "00" "00");

    CheckLines(db.Take(), {}, __LINE__);
    CHECK_EQ(db.query.executed.size(), size_t(0));
}

// The online-visibility verdict, one input flipped per row from a base that is visible (a player
// of the same team, both SEC_PLAYER, the GM-in-who level SEC_PLAYER, present and globally visible).
TEST(SocialMgr_VisibilityVerdictTable)
{
    struct Row
    {
        bool present;
        AccountTypes viewerSecurity;
        Team viewerTeam;
        AccountTypes seenSecurity;
        Team seenTeam;
        bool twoSide;
        AccountTypes gmLevel;
        bool visible;
        bool want;
        const char* why;
    };
    const Row rows[] =
    {
        { true,  SEC_PLAYER,     ALLIANCE, SEC_PLAYER,        ALLIANCE, false, SEC_PLAYER,     true,  true,  "base: same team, both players" },
        { false, SEC_PLAYER,     ALLIANCE, SEC_PLAYER,        ALLIANCE, false, SEC_PLAYER,     true,  false, "not present (offline)" },
        { true,  SEC_PLAYER,     ALLIANCE, SEC_PLAYER,        ALLIANCE, false, SEC_PLAYER,     false, false, "not globally visible" },
        { true,  SEC_PLAYER,     ALLIANCE, SEC_PLAYER,        HORDE,    false, SEC_PLAYER,     true,  false, "other team, no two-side config" },
        { true,  SEC_PLAYER,     ALLIANCE, SEC_PLAYER,        HORDE,    true,  SEC_PLAYER,     true,  true,  "other team, two-side config" },
        { true,  SEC_PLAYER,     HORDE,    SEC_PLAYER,        ALLIANCE, false, SEC_PLAYER,     true,  false, "other team the other way round" },
        { true,  SEC_PLAYER,     ALLIANCE, SEC_GAMEMASTER,    ALLIANCE, false, SEC_PLAYER,     true,  false, "seen security above the GM-in-who level" },
        { true,  SEC_PLAYER,     ALLIANCE, SEC_MODERATOR,     ALLIANCE, false, SEC_MODERATOR,  true,  true,  "seen security equal to the GM-in-who level" },
        { true,  SEC_PLAYER,     ALLIANCE, SEC_GAMEMASTER,    ALLIANCE, false, SEC_MODERATOR,  true,  false, "seen security one above the GM-in-who level" },
        { true,  SEC_PLAYER,     ALLIANCE, SEC_PLAYER,        ALLIANCE, false, SEC_ADMINISTRATOR, true, true, "seen player under a high GM-in-who level" },
        { true,  SEC_PLAYER,     ALLIANCE, SEC_GAMEMASTER,    HORDE,    true,  SEC_PLAYER,     true,  false, "two-side config does not lift the security rule" },
        { true,  SEC_MODERATOR,  ALLIANCE, SEC_ADMINISTRATOR, HORDE,    false, SEC_PLAYER,     true,  true,  "viewer a moderator sees all" },
        { true,  SEC_GAMEMASTER, HORDE,    SEC_GAMEMASTER,    ALLIANCE, false, SEC_PLAYER,     true,  true,  "viewer a game master sees all" },
        { true,  SEC_GAMEMASTER, ALLIANCE, SEC_PLAYER,        ALLIANCE, false, SEC_PLAYER,     false, false, "a game master still needs global visibility" },
        { false, SEC_GAMEMASTER, ALLIANCE, SEC_PLAYER,        ALLIANCE, false, SEC_PLAYER,     true,  false, "a game master still needs presence" },
    };
    for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); ++i)
    {
        FriendVisibility facts;
        facts.present = rows[i].present;
        facts.viewerSecurity = rows[i].viewerSecurity;
        facts.viewerTeam = rows[i].viewerTeam;
        facts.seenSecurity = rows[i].seenSecurity;
        facts.seenTeam = rows[i].seenTeam;
        facts.allowTwoSideWhoList = rows[i].twoSide;
        facts.gmLevelInWhoList = rows[i].gmLevel;
        facts.seenVisibleGlobally = rows[i].visible;
        if (IsFriendVisibleOnline(facts) != rows[i].want)
        {
            testing::ReportFailure(__FILE__, __LINE__, std::string("verdict row ") + std::to_string(i) + ": " + rows[i].why);
        }
    }

    // The defaults: nothing read is not visible.
    CHECK(!IsFriendVisibleOnline(FriendVisibility()));
}

// SMSG_FRIEND_STATUS for every result: the result byte and the guid; the note ("hi" 686900) after
// that for ADDED_ONLINE (6) and ADDED_OFFLINE (7) only; status, area, level, class (DND 04, area
// 1519 ef050000, level 85 55000000, class 8 08000000) for ADDED_ONLINE (6) and ONLINE (2) only.
// The fill is called once per packet, with the related guid (7), after the header.
TEST(SocialMgr_FriendStatusBytesPerResult)
{
    const std::string guid = "0700000000000000";
    const std::string note = "686900";
    const std::string online = "04" "ef050000" "55000000" "08000000";
    for (uint32 r = FRIEND_DB_ERROR; r <= FRIEND_UNKNOWN; ++r)
    {
        std::vector<std::string> events;
        FriendInfoFill fill = [&events](uint32 friendLowGuid, FriendInfo& info)
        {
            events.push_back("fill " + std::to_string(friendLowGuid));
            info.Note = "hi";
            info.Status = FRIEND_STATUS_DND;
            info.Area = 1519;
            info.Level = 85;
            info.Class = 8;
        };
        WorldPacket data;
        BuildFriendStatusPacket(FriendsResult(r), 7, fill, data);

        static const char* digits = "0123456789abcdef";
        std::string want = std::string("0717:") + digits[r >> 4] + digits[r & 0x0F] + guid;
        if (r == FRIEND_ADDED_ONLINE || r == FRIEND_ADDED_OFFLINE)
        {
            want += note;
        }
        if (r == FRIEND_ADDED_ONLINE || r == FRIEND_ONLINE)
        {
            want += online;
        }
        if (PacketText(data) != want)
        {
            testing::ReportFailure(__FILE__, __LINE__, "result " + std::to_string(r) + ": got " + PacketText(data) + " want " + want);
        }
        CHECK(events == std::vector<std::string>({ "fill 7" }));
    }

    // The three shapes spelled out once, so the loop's rule is pinned by literal bytes.
    FriendInfoFill offline = [](uint32, FriendInfo& info)
    {
        info.Note = "x";
    };
    WorldPacket a;
    BuildFriendStatusPacket(FRIEND_ADDED_ONLINE, 9, offline, a);
    CHECK_STR(PacketText(a), "0717:06" "0900000000000000" "7800" "00" "00000000" "00000000" "00000000");
    WorldPacket b;
    BuildFriendStatusPacket(FRIEND_REMOVED, 9, offline, b);
    CHECK_STR(PacketText(b), "0717:05" "0900000000000000");
    WorldPacket c;
    MakeFriendStatusPacket(FRIEND_IGNORE_ADDED, 11, &c);
    CHECK_STR(PacketText(c), "0717:0f" "0b00000000000000");
}
