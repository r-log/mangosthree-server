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

/// The packets a player's own client is told, built from the player's twenty-three facts: the bytes of each
/// builder, the callbacks the session installs on a player, which build the packet at each report
/// and send it to the session the player holds at that moment, the security level read from that
/// session at each call, and the assertion on a callback no session installed.

#include "TestHarness.h"
#include "ObjectGuid.h"
#include "Player.h"
#include "WorldPacket.h"
#include "session/packets/PlayerPacketSinks.h"
#include "session/packets/chat/MessageChatPacket.h"
#include "WorldSession.h"
#include "Auth/BigNumber.h"

#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace
{
    // A creature guid, entry 3000 and counter 42: raw 0xF1300BB80000002A, bytes (low first)
    // 2a 00 00 00 b8 0b 30 f1, so its packed form skips three zero bytes and keeps the high byte.
    const ObjectGuid kCreature(HIGHGUID_UNIT, uint32(3000), uint32(42));
    // A pet guid, entry 416 and counter 1: raw 0xF14001A000000001.
    const ObjectGuid kPet(HIGHGUID_PET, uint32(416), uint32(1));
    // A player guid, counter 15728640: raw 0x0000000000F00000.
    const ObjectGuid kPlayer(HIGHGUID_PLAYER, uint32(15728640));

    std::string Hex(WorldPacket const& packet)
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

    template <class Fact>
    std::string Built(void (*build)(WorldPacket&, Fact const&), Fact const& fact)
    {
        WorldPacket packet;
        build(packet, fact);
        return Hex(packet);
    }

    AutoRepeatCancelledFact AutoRepeat(ObjectGuid target)
    {
        AutoRepeatCancelledFact fact;
        fact.target = target;
        return fact;
    }

    CurrentPetFact PetIs(ObjectGuid pet)
    {
        CurrentPetFact fact;
        fact.pet = pet;
        return fact;
    }

    StandStateFact Stand(uint8 state)
    {
        StandStateFact fact;
        fact.state = state;
        return fact;
    }

    PvpCreditFact Credit(uint32 honor, ObjectGuid victim, uint32 victimRank)
    {
        PvpCreditFact fact;
        fact.honor = honor;
        fact.victim = victim;
        fact.victimRank = victimRank;
        return fact;
    }

    RuneConvertedFact Converted(uint8 index, uint8 type)
    {
        RuneConvertedFact fact;
        fact.index = index;
        fact.type = type;
        return fact;
    }

    /// Slots blood, blood, unholy, death, frost, frost; three quarters, all, none, half, all and all
    /// of their cooldowns passed.
    RunesResyncedFact Resynced()
    {
        RunesResyncedFact fact;
        const uint8 types[MAX_RUNES] = { 0, 0, 1, 3, 2, 2 };
        const uint8 fractions[MAX_RUNES] = { 0xbf, 0xff, 0x00, 0x7f, 0xff, 0xff };
        for (int i = 0; i < MAX_RUNES; ++i)
        {
            fact.types[i] = types[i];
            fact.cooldownFractions[i] = fractions[i];
        }
        return fact;
    }

    RunePowerAddedFact PowerAdded(uint8 index)
    {
        RunePowerAddedFact fact;
        fact.index = index;
        return fact;
    }

    CurrencySetFact CurrencyIs(uint32 id, bool weekShown, bool seasonShown, bool messageHidden, uint32 season,
                               uint32 total, uint32 week)
    {
        CurrencySetFact fact;
        fact.id = id;
        fact.weekShown = weekShown;
        fact.seasonShown = seasonShown;
        fact.messageHidden = messageHidden;
        fact.season = season;
        fact.total = total;
        fact.week = week;
        return fact;
    }

    CurrencyWeekCapFact WeekCapIs(uint32 id, uint32 cap)
    {
        CurrencyWeekCapFact fact;
        fact.id = id;
        fact.cap = cap;
        return fact;
    }

    CurrencyListFact::Entry ListEntry(uint32 id, uint8 flags, bool hasWeekCap, bool hasSeason, bool weekShown,
                                      uint32 total, uint32 weekCap, uint32 season, uint32 week)
    {
        CurrencyListFact::Entry entry;
        entry.id = id;
        entry.flags = flags;
        entry.hasWeekCap = hasWeekCap;
        entry.hasSeason = hasSeason;
        entry.weekShown = weekShown;
        entry.total = total;
        entry.weekCap = weekCap;
        entry.season = season;
        entry.week = week;
        return entry;
    }

    ForcedReactionsFact TwoForcedReactions()
    {
        ForcedReactionsFact fact;
        fact.reactions.push_back(std::make_pair(uint32(1), uint32(7)));
        fact.reactions.push_back(std::make_pair(uint32(5), uint32(1)));
        return fact;
    }

    FactionStandingFact Standing(bool anyRankIncreased, uint32 listId, uint32 standing)
    {
        FactionStandingFact fact;
        fact.anyRankIncreased = anyRankIncreased;
        fact.standings.push_back(std::make_pair(listId, standing));
        return fact;
    }

    /// Two slots: flags 0x11 at standing 9000, then an absent one.
    InitialReputationsFact TwoSlots()
    {
        InitialReputationsFact fact;
        InitialReputationsFact::Slot slot;
        slot.flags = 0x11;
        slot.standing = 9000;
        fact.slots.push_back(slot);
        fact.slots.push_back(InitialReputationsFact::Slot());
        return fact;
    }

    FactionVisibleFact Visible(uint32 listId)
    {
        FactionVisibleFact fact;
        fact.listId = listId;
        return fact;
    }

    FactionAtWarFact AtWar(uint32 listId, uint8 flags)
    {
        FactionAtWarFact fact;
        fact.listId = listId;
        fact.flags = flags;
        return fact;
    }

    /// A friend with the note "hi", DND in area 1519, level 85, class 8.
    FriendInfo DndFriend()
    {
        FriendInfo info(SOCIAL_FLAG_FRIEND, "hi");
        info.Status = FRIEND_STATUS_DND;
        info.Area = 1519;
        info.Level = 85;
        info.Class = 8;
        return info;
    }

    /// One entry: the friend above, low guid 7.
    ContactListFact OneContact()
    {
        ContactListFact fact;
        ContactListFact::Entry entry;
        entry.lowGuid = 7;
        entry.info = DndFriend();
        fact.entries.push_back(entry);
        return fact;
    }

    /// `result` about the friend above, low guid 7.
    FriendStatusFact StatusOf(FriendsResult result)
    {
        FriendStatusFact fact;
        fact.result = result;
        fact.friendLowGuid = 7;
        fact.info = DndFriend();
        return fact;
    }

    /// Conquest points (390) with a week cap and a week count, then honor points (392) with a
    /// season count.
    CurrencyListFact TwoCurrencies()
    {
        CurrencyListFact fact;
        fact.entries.push_back(ListEntry(390, 0x4, true, false, true, 100, 1350, 0, 20));
        fact.entries.push_back(ListEntry(392, 0x0, false, true, false, 5, 0, 9, 0));
        return fact;
    }

    /// One bonus talent, 150 copper and 450 XP for quest 26389, the next quest's details to follow.
    QuestCompleteFact QuestDone()
    {
        QuestCompleteFact fact;
        fact.bonusTalents = 1;
        fact.money = 150;
        fact.xp = 450;
        fact.questId = 26389;
        fact.useQuestReward = true;
        return fact;
    }

    /// Stands in for the session: records "<opcode>:<hex>" per packet it is sent, and answers its
    /// account security level.
    struct FakeSession
    {
        std::vector<std::string> sent;
        AccountTypes security = SEC_PLAYER;

        void SendPacket(WorldPacket const* packet)
        {
            sent.push_back(Hex(*packet));
        }

        AccountTypes GetSecurity() const
        {
            return security;
        }
    };

    /// Stands in for the player: GetSession() answers whichever session it holds now.
    struct FakeOwner
    {
        FakeSession* session = NULL;

        FakeSession* GetSession() const
        {
            return session;
        }
    };

    /// Reports one of each fact through `callbacks`, in the order of the golden list below.
    void ReportEach(Player::ClientCallbacks const& callbacks)
    {
        ReportClientFact(callbacks.swingOutOfReach, SwingOutOfReachFact());
        ReportClientFact(callbacks.swingBadFacing, SwingBadFacingFact());
        ReportClientFact(callbacks.combatCancelled, CombatCancelledFact());
        ReportClientFact(callbacks.autoRepeatCancelled, AutoRepeat(kCreature));
        ReportClientFact(callbacks.currentPet, PetIs(kPet));
        ReportClientFact(callbacks.petActionBarCleared, PetActionBarClearedFact());
        ReportClientFact(callbacks.standState, Stand(8));
        ReportClientFact(callbacks.pvpCredit, Credit(40, kPlayer, 5));
        ReportClientFact(callbacks.runeConverted, Converted(1, 3));
        ReportClientFact(callbacks.runesResynced, Resynced());
        ReportClientFact(callbacks.runePowerAdded, PowerAdded(3));
        ReportClientFact(callbacks.currencySet, CurrencyIs(392, true, true, false, 300, 1500, 50));
        ReportClientFact(callbacks.currencyWeekCap, WeekCapIs(396, 1000));
        ReportClientFact(callbacks.currencyWeekReset, CurrencyWeekResetFact());
        ReportClientFact(callbacks.currencyList, TwoCurrencies());
        ReportClientFact(callbacks.forcedReactions, TwoForcedReactions());
        ReportClientFact(callbacks.factionStanding, Standing(true, 21, 10));
        ReportClientFact(callbacks.initialReputations, TwoSlots());
        ReportClientFact(callbacks.factionVisible, Visible(25));
        ReportClientFact(callbacks.factionAtWar, AtWar(25, 0x03));
        ReportClientFact(callbacks.contactList, OneContact());
        ReportClientFact(callbacks.friendStatus, StatusOf(FRIEND_ONLINE));
        ReportClientFact(callbacks.questComplete, QuestDone());
    }

    const char* const kEach[] =
    {
        "0b36:",
        "6c07:",
        "4f04:",
        "6436:f12ab80b30f1",
        "2d26:0100000001000000a00140f1",
        "4114:0000000000000000",
        "6f04:08",
        "6015:280000000000f0000000000005000000",
        "4f14:0103",
        "6224:0600000000bf00ff0100037f02ff02ff",
        "6915:08000000",
        "59b0:c02c010000dc0500008801000032000000",
        "70a7:e80300008c010000",
        "3ca1:",
        "15a5:000005480864000000460500008601000014000000050000000900000088010000",
        "4615:0200000001000000070000000500000001000000",
        "0126:000000000101000000150000000a000000",
        "4634:0001000011282300000000000000",
        "2525:19000000",
        "4216:1900000003",
        "6017:070000000100000007000000000000000100000068690004ef0500005500000008000000",
        "0717:02070000000000000004ef0500005500000008000000",
        "55a4:010000000000000096000000c2010000156700000000000040",
    };

    /// The empty-callback case ends the process on its assertion, so it runs only when
    /// MANGOS_TESTS_ABORT_CASE names it (CheckAbortCase.cmake); any other run returns at once.
    bool AbortCaseSelected(char const* name)
    {
        char const* selected = std::getenv("MANGOS_TESTS_ABORT_CASE");
        if (!selected || std::strcmp(selected, name) != 0)
        {
            return false;
        }
#ifdef _MSC_VER
        _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
        return true;
    }
}

// SMSG_ATTACKSWING_NOTINRANGE (0x0B36), SMSG_ATTACKSWING_BADFACING (0x6C07) and SMSG_CANCEL_COMBAT
// (0x4F04): no payload.
TEST(PlayerClientPackets_EmptyNoticesBytes)
{
    CHECK_STR(Built(&BuildAttackSwingNotInRangePacket, SwingOutOfReachFact()), "0b36:");
    CHECK_STR(Built(&BuildAttackSwingBadFacingPacket, SwingBadFacingFact()), "6c07:");
    CHECK_STR(Built(&BuildCancelCombatPacket, CombatCancelledFact()), "4f04:");
}

// SMSG_CANCEL_AUTO_REPEAT (0x6436): the target's packed guid, a mask byte with one bit per non-zero
// byte of the raw guid, then those bytes, low first.
TEST(PlayerClientPackets_CancelAutoRepeatBytes)
{
    REQUIRE(kCreature.GetRawValue() == UI64LIT(0xF1300BB80000002A));
    REQUIRE(kPlayer.GetRawValue() == UI64LIT(0x0000000000F00000));

    // Bytes 0, 4, 5, 6 and 7 are non-zero: mask 11110001 = f1, then 2a b8 0b 30 f1.
    CHECK_STR(Built(&BuildCancelAutoRepeatPacket, AutoRepeat(kCreature)), "6436:f12ab80b30f1");
    // Byte 2 only: mask 00000100 = 04, then f0.
    CHECK_STR(Built(&BuildCancelAutoRepeatPacket, AutoRepeat(kPlayer)), "6436:04f0");
    // The empty guid: mask 00, no byte.
    CHECK_STR(Built(&BuildCancelAutoRepeatPacket, AutoRepeat(ObjectGuid())), "6436:00");
}

// The packed guid the builder writes is the one an object caches for its own guid (Object::_Create
// sets it with PackedGuid::Set from the guid it stores).
TEST(PlayerClientPackets_CancelAutoRepeatWritesTheCachedPackedGuid)
{
    const ObjectGuid guids[] = { kCreature, kPet, kPlayer, ObjectGuid() };
    for (ObjectGuid const& guid : guids)
    {
        PackedGuid cached;
        cached.Set(guid);
        ByteBuffer expected;
        expected << cached;

        WorldPacket packet;
        BuildCancelAutoRepeatPacket(packet, AutoRepeat(guid));
        REQUIRE(packet.size() == expected.size());
        CHECK(std::memcmp(packet.contents(), expected.contents(), expected.size()) == 0);
    }
}

// SMSG_PET_GUIDS (0x2D26): uint32 1, then the pet's raw guid as a uint64.
TEST(PlayerClientPackets_PetGuidsBytes)
{
    REQUIRE(kPet.GetRawValue() == UI64LIT(0xF14001A000000001));

    // 01000000; the guid, low byte first: 01 00 00 00 a0 01 40 f1.
    CHECK_STR(Built(&BuildPetGuidsPacket, PetIs(kPet)), "2d26:0100000001000000a00140f1");
    CHECK_STR(Built(&BuildPetGuidsPacket, PetIs(kCreature)), "2d26:010000002a000000b80b30f1");
}

// SMSG_PET_SPELLS (0x4114) with an empty guid: eight zero bytes (the guid as a uint64), which
// clears the client's pet action bar.
TEST(PlayerClientPackets_PetActionBarClearedBytes)
{
    CHECK_STR(Built(&BuildPetActionBarClearedPacket, PetActionBarClearedFact()), "4114:0000000000000000");
}

// SMSG_STANDSTATE_UPDATE (0x6F04): the state, one byte.
TEST(PlayerClientPackets_StandStateUpdateBytes)
{
    CHECK_STR(Built(&BuildStandStateUpdatePacket, Stand(0)), "6f04:00");
    CHECK_STR(Built(&BuildStandStateUpdatePacket, Stand(1)), "6f04:01");
    CHECK_STR(Built(&BuildStandStateUpdatePacket, Stand(8)), "6f04:08");
    CHECK_STR(Built(&BuildStandStateUpdatePacket, Stand(255)), "6f04:ff");
}

// SMSG_PVP_CREDIT (0x6015): the honor as a uint32, the victim's raw guid as a uint64, then the
// victim's rank as a uint32.
TEST(PlayerClientPackets_PvpCreditBytes)
{
    // 28000000; the guid, low byte first: 00 00 f0 00 00 00 00 00; 05000000.
    CHECK_STR(Built(&BuildPvpCreditPacket, Credit(40, kPlayer, 5)), "6015:280000000000f0000000000005000000");
    // No victim named, rank 0.
    CHECK_STR(Built(&BuildPvpCreditPacket, Credit(0, ObjectGuid(), 0)), "6015:00000000000000000000000000000000");
    CHECK_STR(Built(&BuildPvpCreditPacket, Credit(0xFFFFFFFF, kCreature, 19)), "6015:ffffffff2a000000b80b30f113000000");
}

// SMSG_CONVERT_RUNE (0x4F14): the slot, then the new rune type, one byte each.
TEST(PlayerClientPackets_ConvertRuneBytes)
{
    CHECK_STR(Built(&BuildConvertRunePacket, Converted(1, 3)), "4f14:0103");
    CHECK_STR(Built(&BuildConvertRunePacket, Converted(5, 0)), "4f14:0500");
}

// SMSG_RESYNC_RUNES (0x6224): uint32 MAX_RUNES, then the rune type and the cooldown fraction of
// each slot, one byte each.
TEST(PlayerClientPackets_ResyncRunesBytes)
{
    CHECK_STR(Built(&BuildResyncRunesPacket, Resynced()), "6224:0600000000bf00ff0100037f02ff02ff");
    CHECK_STR(Built(&BuildResyncRunesPacket, RunesResyncedFact()), "6224:06000000000000000000000000000000");
}

// SMSG_ADD_RUNE_POWER (0x6915): the uint32 mask 1 << slot.
TEST(PlayerClientPackets_AddRunePowerBytes)
{
    CHECK_STR(Built(&BuildAddRunePowerPacket, PowerAdded(0)), "6915:01000000");
    CHECK_STR(Built(&BuildAddRunePowerPacket, PowerAdded(3)), "6915:08000000");
    CHECK_STR(Built(&BuildAddRunePowerPacket, PowerAdded(5)), "6915:20000000");
}

// SMSG_SET_CURRENCY (0x59B0): three bits, most significant first (the week count sent, the season
// count sent, no message), padded to a byte; then the season count if sent, the total, the id and
// the week count if sent, each a uint32.
TEST(PlayerClientPackets_SetCurrencyBytes)
{
    // bits 110 -> c0; season 300, total 1500, id 392, week 50.
    CHECK_STR(Built(&BuildSetCurrencyPacket, CurrencyIs(392, true, true, false, 300, 1500, 50)),
              "59b0:c02c010000dc0500008801000032000000");
    // bits 000 -> 00; total 7, id 395.
    CHECK_STR(Built(&BuildSetCurrencyPacket, CurrencyIs(395, false, false, false, 0, 7, 0)), "59b0:00070000008b010000");
    // bits 001 -> 20; total 0, id 483.
    CHECK_STR(Built(&BuildSetCurrencyPacket, CurrencyIs(483, false, false, true, 0, 0, 0)), "59b0:2000000000e3010000");
}

// SMSG_SET_CURRENCY_WEEK_LIMIT (0x70A7): the limit, then the id, each a uint32.
TEST(PlayerClientPackets_SetCurrencyWeekLimitBytes)
{
    CHECK_STR(Built(&BuildSetCurrencyWeekLimitPacket, WeekCapIs(396, 1000)), "70a7:e80300008c010000");
}

// SMSG_WEEKLY_RESET_CURRENCIES (0x3CA1): no payload.
TEST(PlayerClientPackets_WeeklyResetCurrenciesBytes)
{
    CHECK_STR(Built(&BuildWeeklyResetCurrenciesPacket, CurrencyWeekResetFact()), "3ca1:");
}

// SMSG_SEND_CURRENCIES (0x15A5): the count in 23 bits, then per currency the week count bit, the
// four flag bits, the week cap bit and the season bit, padded to a byte; then per currency the
// total, the week cap if sent, the season count if sent, the id and the week count if sent.
TEST(PlayerClientPackets_SendCurrenciesBytes)
{
    // 2 in 23 bits, 1 0100 1 0, 0 0000 0 1, padded: 00 00 05 48 08; then 100, 1350, 390, 20; 5, 9, 392.
    CHECK_STR(Built(&BuildSendCurrenciesPacket, TwoCurrencies()),
              "15a5:000005480864000000460500008601000014000000050000000900000088010000");
    // 1 in 23 bits, 0 1111 0 0, padded: 00 00 02 f0; then 1, 361.
    CurrencyListFact one;
    one.entries.push_back(ListEntry(361, 0xF, false, false, false, 1, 0, 0, 0));
    CHECK_STR(Built(&BuildSendCurrenciesPacket, one), "15a5:000002f00100000069010000");
}

// SMSG_SET_FORCED_REACTIONS (0x4615): the count, then each faction id and rank, each a uint32.
TEST(PlayerClientPackets_SetForcedReactionsBytes)
{
    CHECK_STR(Built(&BuildSetForcedReactionsPacket, ForcedReactionsFact()), "4615:00000000");
    CHECK_STR(Built(&BuildSetForcedReactionsPacket, TwoForcedReactions()), "4615:0200000001000000070000000500000001000000");
}

// SMSG_SET_FACTION_STANDING (0x0126): the refer-a-friend bonus as a float (0), the visual effect
// flag as a byte, the count, then each reputation list id and standing, each a uint32.
TEST(PlayerClientPackets_SetFactionStandingBytes)
{
    CHECK_STR(Built(&BuildSetFactionStandingPacket, Standing(true, 21, 10)), "0126:000000000101000000150000000a000000");
    // Two standings, no rank increased; -42000 = 0xffff5bf0.
    FactionStandingFact two = Standing(false, 21, 10);
    two.standings.push_back(std::make_pair(uint32(3), uint32(-42000)));
    CHECK_STR(Built(&BuildSetFactionStandingPacket, two), "0126:000000000002000000150000000a00000003000000f05bffff");
}

// SMSG_INITIALIZE_FACTIONS (0x4634): uint32 0x100, then each slot's flags as a byte and its
// standing as a uint32.
TEST(PlayerClientPackets_InitializeFactionsBytes)
{
    CHECK_STR(Built(&BuildInitializeFactionsPacket, TwoSlots()), "4634:0001000011282300000000000000");
}

// SMSG_SET_FACTION_VISIBLE (0x2525): the reputation list id as a uint32.
TEST(PlayerClientPackets_SetFactionVisibleBytes)
{
    CHECK_STR(Built(&BuildSetFactionVisiblePacket, Visible(25)), "2525:19000000");
}

// SMSG_SET_FACTION_ATWAR (0x4216): the reputation list id as a uint32, then the flags' low byte.
TEST(PlayerClientPackets_SetFactionAtWarBytes)
{
    CHECK_STR(Built(&BuildSetFactionAtWarPacket, AtWar(25, 0x03)), "4216:1900000003");
}

// SMSG_CONTACT_LIST (0x6017): 7, the count, then per entry the guid as a uint64, the flags, the note
// and its 0 byte and, for a friend, the status byte and, when online, the area, level and class.
TEST(PlayerClientPackets_ContactListBytes)
{
    // 07000000 01000000; guid 7, flags 1, "hi", DND 04, area 1519 ef050000, level 85, class 8.
    CHECK_STR(Built(&BuildContactListPacket, OneContact()),
              "6017:070000000100000007000000000000000100000068690004ef0500005500000008000000");
    CHECK_STR(Built(&BuildContactListPacket, ContactListFact()), "6017:0700000000000000");
}

// SMSG_FRIEND_STATUS (0x0717): the result byte and the guid as a uint64, then the note for the two
// ADDED results, then the status, area, level and class for ADDED_ONLINE and ONLINE.
TEST(PlayerClientPackets_FriendStatusBytes)
{
    CHECK_STR(Built(&BuildFriendStatusPacket, StatusOf(FRIEND_ONLINE)), "0717:02070000000000000004ef0500005500000008000000");
    CHECK_STR(Built(&BuildFriendStatusPacket, StatusOf(FRIEND_ADDED_ONLINE)),
              "0717:06070000000000000068690004ef0500005500000008000000");
    CHECK_STR(Built(&BuildFriendStatusPacket, StatusOf(FRIEND_ADDED_OFFLINE)), "0717:070700000000000000686900");
    CHECK_STR(Built(&BuildFriendStatusPacket, StatusOf(FRIEND_REMOVED)), "0717:050700000000000000");
}

// The callbacks the session installs turn each fact into the packet above and send it to the
// owner's session, each fact to its own packet.
TEST(PlayerClientPackets_InstalledCallbacksSendTheGoldenPackets)
{
    FakeSession session;
    FakeOwner owner;
    owner.session = &session;

    Player::ClientCallbacks callbacks = ClientCallbacksToSession<Player::ClientCallbacks>(&owner);
    ReportEach(callbacks);

    REQUIRE(session.sent.size() == 23u);
    for (size_t i = 0; i < 23; ++i)
    {
        CHECK_STR(session.sent[i], kEach[i]);
    }
}

// The session is read at each send, never kept: callbacks made before the owner's session changed
// send to the new one.
TEST(PlayerClientPackets_InstalledCallbacksReadTheSessionAtEachSend)
{
    FakeSession first;
    FakeSession second;
    FakeOwner owner;
    owner.session = &first;

    Player::ClientCallbacks callbacks = ClientCallbacksToSession<Player::ClientCallbacks>(&owner);
    ReportEach(callbacks);
    owner.session = &second;
    ReportEach(callbacks);

    REQUIRE(first.sent.size() == 23u);
    REQUIRE(second.sent.size() == 23u);
    for (size_t i = 0; i < 23; ++i)
    {
        CHECK_STR(first.sent[i], kEach[i]);
        CHECK_STR(second.sent[i], kEach[i]);
    }
}

// The security level is the session's, read at each call: a change of the session's level, or of
// the session, between two calls changes the answer.
TEST(PlayerClientPackets_SecurityLevelReadsTheSessionAtEachCall)
{
    FakeSession first;
    FakeSession second;
    second.security = SEC_ADMINISTRATOR;
    FakeOwner owner;
    owner.session = &first;

    Player::ClientCallbacks callbacks = ClientCallbacksToSession<Player::ClientCallbacks>(&owner);
    REQUIRE(callbacks.securityLevel);

    CHECK(callbacks.securityLevel() == uint32(SEC_PLAYER));
    first.security = SEC_GAMEMASTER;
    CHECK(callbacks.securityLevel() == uint32(SEC_GAMEMASTER));
    first.security = SEC_CONSOLE;
    CHECK(callbacks.securityLevel() == uint32(SEC_CONSOLE));
    owner.session = &second;
    CHECK(callbacks.securityLevel() == uint32(SEC_ADMINISTRATOR));
    CHECK(first.sent.empty());
    CHECK(second.sent.empty());
}

// A reported fact reaches the callback once, as given.
TEST(PlayerClientPackets_ReportCallsTheCallbackOnce)
{
    std::vector<uint32> seen;
    StandStateSink sink = [&seen](StandStateFact const& fact)
    {
        seen.push_back(fact.state);
    };

    ReportClientFact(sink, Stand(3));
    ReportClientFact(sink, Stand(0));

    REQUIRE(seen.size() == 2u);
    CHECK(seen[0] == 3u);
    CHECK(seen[1] == 0u);
}

TEST(PlayerClientPackets_EmptyCallbackAsserts)
{
    if (!AbortCaseSelected("PlayerClientPackets_EmptyCallbackAsserts"))
    {
        return;
    }
    ReportClientFact(CombatCancelledSink(), CombatCancelledFact());
    testing::ReportFailure(__FILE__, __LINE__, "the empty client callback passed the assertion");
}

namespace
{
    /// The packet SendSystemMessage gives one line: from player 1, untagged, in the universal language.
    WorldPacket SystemLine(char const* text)
    {
        WorldPacket data;
        BuildMessageChatPacket(data, CHAT_MSG_SYSTEM, text, LANG_UNIVERSAL, CHAT_TAG_NONE, ObjectGuid(HIGHGUID_PLAYER, uint32(1)));
        return data;
    }

    void KeepSystemPacket(void* context, WorldPacket const& packet)
    {
        static_cast<std::vector<std::string>*>(context)->push_back(Hex(packet));
    }

    /// A player with its update fields allocated, seated on its session, in no world.
    class SeatedPlayer : public Player
    {
        public:
            explicit SeatedPlayer(WorldSession& session) : Player(&session)
            {
                Object::_Create(1, 0, HIGHGUID_PLAYER);
                session.SetPlayer(this);
            }

            ~SeatedPlayer()
            {
                GetSession()->SetPlayer(nullptr);
                delete[] m_uint32Values;
                m_uint32Values = NULL;
            }
    };
}

// The installed system message callback sends the text to the player's real session, one
// SMSG_MESSAGECHAT of type CHAT_MSG_SYSTEM per line, the player as the sender.
TEST(PlayerClientPackets_SystemMessageGoesToTheSessionOneLineAPacket)
{
    std::vector<std::string> sent;
    WorldSession session(1, "system", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    session.SetSocketlessSink(&KeepSystemPacket, &sent);
    SeatedPlayer player(session);
    InstallPlayerPacketSinks(player);

    player.ReportSystemText("one\ntwo");

    REQUIRE(sent.size() == 2u);
    CHECK_STR(sent[0], Hex(SystemLine("one")));
    CHECK_STR(sent[1], Hex(SystemLine("two")));
}
