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

/// The opcode table dispatches through one free-function handler type. Every row holds the
/// address of OpcodeThunk bound to its handler; a call through a row reaches that handler on the
/// session it is given, with the packet it is given. A socketless session, built as the GM
/// harness builds one, answers a CMSG_PING handed to the CMSG_PING row with one SMSG_PONG through
/// its sink, carrying the ping's sequence; the row consumes the whole packet. The slots no row
/// names hold the thunk of Handle_NULL, every slot bound to one handler holds one address, and
/// the row of an overloaded handler name holds the overload that takes the packet. A row bound to
/// a handler class's static entry point holds the thunk of that static and reaches it the same
/// way: the CMSG_ATTACKSWING row reads a swing at an empty guid to its end and, the guid naming no
/// unit, returns before it reads the player, sending nothing. The CMSG_SELL_ITEM row reads a sale
/// of an empty item guid to its end and returns at that guid before it reads the player, sending
/// nothing. The CMSG_ARENA_TEAM_INVITE row reads an invite to team 0 of an empty name to its end
/// and, the name finding no player, answers with one SMSG_ARENA_TEAM_COMMAND_RESULT through the
/// session before it reads the player. The CMSG_BATTLEFIELD_LIST row reads a list request for type
/// 0 to its end and, no battlemaster list entry naming that type, returns before it reads the
/// player, sending nothing.

#include "TestHarness.h"
#include "OpcodeTable.h"
#include "WorldSession.h"
#include "WorldPacket.h"
#include "SharedDefines.h"
#include "ArenaTeam.h"
#include "Auth/BigNumber.h"
#include "session/handlers/combat/CombatHandlers.h"
#include "session/handlers/economy/LootHandlers.h"
#include "session/handlers/economy/VendorHandlers.h"
#include "session/handlers/pvp/PvpHandlers.h"

#include <cstring>
#include <type_traits>
#include <vector>

static_assert(std::is_same<decltype(OpcodeHandler::handler), void (*)(WorldSession&, WorldPacket&)>::value,
              "the opcode table stores a free function taking the session and the packet");

namespace
{
    void CapturePacket(void* context, WorldPacket const& packet)
    {
        static_cast<std::vector<WorldPacket>*>(context)->push_back(packet);
    }

    int CountSlotsBoundTo(void (*handler)(WorldSession&, WorldPacket&))
    {
        int slots = 0;
        for (uint32 i = 0; i < NUM_MSG_TYPES; ++i)
        {
            if (opcodeTable[i].handler == handler)
            {
                ++slots;
            }
        }
        return slots;
    }
}

TEST(OpcodeDispatch_PingRowAnswersWithThePongOfItsSequence)
{
    InitializeOpcodes();

    std::vector<WorldPacket> sent;
    WorldSession session(1, "dispatch", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    session.SetSocketlessSink(&CapturePacket, &sent);

    WorldPacket ping(CMSG_PING, 8);
    ping << uint32(0);
    ping << uint32(7);

    opcodeTable[CMSG_PING].handler(session, ping);

    CHECK_EQ(ping.rpos(), size_t(8));
    REQUIRE(sent.size() == 1);
    CHECK_EQ(sent[0].GetOpcode(), uint16(SMSG_PONG));
    REQUIRE(sent[0].size() == 4);
    uint32 sequence = 0;
    sent[0] >> sequence;
    CHECK_EQ(sequence, uint32(7));

    session.SetSocketlessSink(nullptr, nullptr);
}

TEST(OpcodeDispatch_PingRowHoldsThePingHandlersThunk)
{
    InitializeOpcodes();

    CHECK(opcodeTable[CMSG_PING].handler == &OpcodeThunk<&WorldSession::HandlePingOpcode>);
    CHECK(opcodeTable[CMSG_PING].handler != &OpcodeThunk<&WorldSession::Handle_NULL>);
}

TEST(OpcodeDispatch_SlotsNoRowNamesHoldTheNullHandler)
{
    InitializeOpcodes();

    int unknown = 0;
    int unboundUnknown = 0;
    for (uint32 i = 0; i < NUM_MSG_TYPES; ++i)
    {
        if (std::strcmp(opcodeTable[i].name, "UNKNOWN") == 0)
        {
            ++unknown;
            if (opcodeTable[i].handler != &OpcodeThunk<&WorldSession::Handle_NULL>)
            {
                ++unboundUnknown;
            }
        }
    }
    CHECK(unknown > 0);
    CHECK_EQ(unboundUnknown, 0);
}

TEST(OpcodeDispatch_RowsOfOneHandlerShareOneAddress)
{
    InitializeOpcodes();

    CHECK_EQ(CountSlotsBoundTo(&OpcodeThunk<&WorldSession::HandleMovementOpcodes>), 27);
}

TEST(OpcodeDispatch_OverloadedHandlerNameBindsThePacketOverload)
{
    InitializeOpcodes();

    CHECK(opcodeTable[MSG_MOVE_WORLDPORT_ACK].handler
          == &OpcodeThunk<static_cast<void (WorldSession::*)(WorldPacket&)>(&WorldSession::HandleMoveWorldportAckOpcode)>);
}

TEST(OpcodeDispatch_FreeFunctionRowReachesItsHandlerWithThePacket)
{
    InitializeOpcodes();

    std::vector<WorldPacket> sent;
    WorldSession session(1, "dispatch", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    session.SetSocketlessSink(&CapturePacket, &sent);

    WorldPacket swing(CMSG_ATTACKSWING, 8);
    swing << uint64(0);

    opcodeTable[CMSG_ATTACKSWING].handler(session, swing);

    CHECK_EQ(swing.rpos(), size_t(8));
    CHECK(sent.empty());

    session.SetSocketlessSink(nullptr, nullptr);
}

TEST(OpcodeDispatch_FreeFunctionRowsHoldTheirHandlersThunks)
{
    InitializeOpcodes();

    CHECK(opcodeTable[CMSG_ATTACKSWING].handler == &OpcodeThunk<&CombatHandlers::HandleAttackSwing>);
    CHECK(opcodeTable[CMSG_ATTACKSTOP].handler == &OpcodeThunk<&CombatHandlers::HandleAttackStop>);
    CHECK(opcodeTable[CMSG_SETSHEATHED].handler == &OpcodeThunk<&CombatHandlers::HandleSetSheathed>);
    CHECK(opcodeTable[CMSG_DUEL_ACCEPTED].handler == &OpcodeThunk<&CombatHandlers::HandleDuelAccepted>);
    CHECK(opcodeTable[CMSG_DUEL_CANCELLED].handler == &OpcodeThunk<&CombatHandlers::HandleDuelCancelled>);
    CHECK(opcodeTable[CMSG_ATTACKSWING].handler != &OpcodeThunk<&WorldSession::Handle_NULL>);
}

TEST(OpcodeDispatch_VendorRowReachesItsHandlerWithThePacket)
{
    InitializeOpcodes();

    std::vector<WorldPacket> sent;
    WorldSession session(1, "dispatch", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    session.SetSocketlessSink(&CapturePacket, &sent);

    WorldPacket sell(CMSG_SELL_ITEM, 20);
    sell << uint64(0);
    sell << uint64(0);
    sell << uint32(0);

    opcodeTable[CMSG_SELL_ITEM].handler(session, sell);

    CHECK_EQ(sell.rpos(), size_t(20));
    CHECK(sent.empty());

    session.SetSocketlessSink(nullptr, nullptr);
}

TEST(OpcodeDispatch_VendorRowsHoldTheirHandlersThunks)
{
    InitializeOpcodes();

    CHECK(opcodeTable[CMSG_SELL_ITEM].handler == &OpcodeThunk<&VendorHandlers::HandleSellItemOpcode>);
    CHECK(opcodeTable[CMSG_BUYBACK_ITEM].handler == &OpcodeThunk<&VendorHandlers::HandleBuybackItem>);
    CHECK(opcodeTable[CMSG_BUY_ITEM].handler == &OpcodeThunk<&VendorHandlers::HandleBuyItemOpcode>);
    CHECK(opcodeTable[CMSG_LIST_INVENTORY].handler == &OpcodeThunk<&VendorHandlers::HandleListInventoryOpcode>);
    CHECK(opcodeTable[CMSG_AUTOSTORE_BAG_ITEM].handler == &OpcodeThunk<&VendorHandlers::HandleAutoStoreBagItemOpcode>);
    CHECK(opcodeTable[CMSG_BUY_BANK_SLOT].handler == &OpcodeThunk<&VendorHandlers::HandleBuyBankSlotOpcode>);
    CHECK(opcodeTable[CMSG_AUTOBANK_ITEM].handler == &OpcodeThunk<&VendorHandlers::HandleAutoBankItemOpcode>);
    CHECK(opcodeTable[CMSG_AUTOSTORE_BANK_ITEM].handler == &OpcodeThunk<&VendorHandlers::HandleAutoStoreBankItemOpcode>);
    CHECK(opcodeTable[CMSG_SELL_ITEM].handler != &OpcodeThunk<&WorldSession::Handle_NULL>);
}

TEST(OpcodeDispatch_GuildAutoDeclineRowIsLoggedInAndThreadUnsafe)
{
    InitializeOpcodes();

    CHECK_EQ(opcodeTable[CMSG_GUILD_AUTO_DECLINE].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_GUILD_AUTO_DECLINE].packetProcessing, PROCESS_THREADUNSAFE);
}

TEST(OpcodeDispatch_SetSelectionRowIsLoggedInAndThreadUnsafe)
{
    InitializeOpcodes();

    CHECK_EQ(opcodeTable[CMSG_SET_SELECTION].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_SET_SELECTION].packetProcessing, PROCESS_THREADUNSAFE);
}

TEST(OpcodeDispatch_ZoneUpdateRowIsLoggedInAndThreadUnsafe)
{
    InitializeOpcodes();

    CHECK_EQ(opcodeTable[CMSG_ZONEUPDATE].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_ZONEUPDATE].packetProcessing, PROCESS_THREADUNSAFE);
}

TEST(OpcodeDispatch_ArenaInviteRowReachesItsHandlerAndItsSender)
{
    InitializeOpcodes();

    std::vector<WorldPacket> sent;
    WorldSession session(1, "dispatch", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    session.SetSocketlessSink(&CapturePacket, &sent);

    WorldPacket invite(CMSG_ARENA_TEAM_INVITE, 5);
    invite << uint32(0);
    invite << uint8(0);

    opcodeTable[CMSG_ARENA_TEAM_INVITE].handler(session, invite);

    CHECK_EQ(invite.rpos(), size_t(5));
    REQUIRE(sent.size() == 1);
    CHECK_EQ(sent[0].GetOpcode(), uint16(SMSG_ARENA_TEAM_COMMAND_RESULT));
    REQUIRE(sent[0].size() == 10);
    uint16 lengths = 1;
    uint32 action = 1;
    uint32 error = 0;
    sent[0] >> lengths >> action >> error;
    CHECK_EQ(lengths, uint16(0));
    CHECK_EQ(action, uint32(ERR_ARENA_TEAM_CREATE_S));
    CHECK_EQ(error, uint32(ERR_ARENA_TEAM_PLAYER_NOT_FOUND_S));

    session.SetSocketlessSink(nullptr, nullptr);
}

TEST(OpcodeDispatch_ArenaTeamRowsHoldTheirHandlersThunks)
{
    InitializeOpcodes();

    CHECK(opcodeTable[MSG_INSPECT_ARENA_TEAMS].handler == &OpcodeThunk<&PvpHandlers::HandleInspectArenaTeams>);
    CHECK(opcodeTable[CMSG_ARENA_TEAM_QUERY].handler == &OpcodeThunk<&PvpHandlers::HandleArenaTeamQuery>);
    CHECK(opcodeTable[CMSG_ARENA_TEAM_ROSTER].handler == &OpcodeThunk<&PvpHandlers::HandleArenaTeamRoster>);
    CHECK(opcodeTable[CMSG_ARENA_TEAM_CREATE].handler == &OpcodeThunk<&PvpHandlers::HandleArenaTeamCreate>);
    CHECK(opcodeTable[CMSG_ARENA_TEAM_INVITE].handler == &OpcodeThunk<&PvpHandlers::HandleArenaTeamInvite>);
    CHECK(opcodeTable[CMSG_ARENA_TEAM_ACCEPT].handler == &OpcodeThunk<&PvpHandlers::HandleArenaTeamAccept>);
    CHECK(opcodeTable[CMSG_ARENA_TEAM_DECLINE].handler == &OpcodeThunk<&PvpHandlers::HandleArenaTeamDecline>);
    CHECK(opcodeTable[CMSG_ARENA_TEAM_LEAVE].handler == &OpcodeThunk<&PvpHandlers::HandleArenaTeamLeave>);
    CHECK(opcodeTable[CMSG_ARENA_TEAM_DISBAND].handler == &OpcodeThunk<&PvpHandlers::HandleArenaTeamDisband>);
    CHECK(opcodeTable[CMSG_ARENA_TEAM_REMOVE].handler == &OpcodeThunk<&PvpHandlers::HandleArenaTeamRemove>);
    CHECK(opcodeTable[CMSG_ARENA_TEAM_LEADER].handler == &OpcodeThunk<&PvpHandlers::HandleArenaTeamLeader>);
    CHECK(opcodeTable[CMSG_ARENA_TEAM_INVITE].handler != &OpcodeThunk<&WorldSession::Handle_NULL>);
}

TEST(OpcodeDispatch_BattlefieldListRowReachesItsHandlerWithThePacket)
{
    InitializeOpcodes();

    std::vector<WorldPacket> sent;
    WorldSession session(1, "dispatch", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    session.SetSocketlessSink(&CapturePacket, &sent);

    WorldPacket list(CMSG_BATTLEFIELD_LIST, 4);
    list << uint32(0);

    opcodeTable[CMSG_BATTLEFIELD_LIST].handler(session, list);

    CHECK_EQ(list.rpos(), size_t(4));
    CHECK(sent.empty());

    session.SetSocketlessSink(nullptr, nullptr);
}

TEST(OpcodeDispatch_BattleGroundRowsHoldTheirHandlersThunks)
{
    InitializeOpcodes();

    CHECK(opcodeTable[CMSG_BATTLEMASTER_HELLO].handler == &OpcodeThunk<&PvpHandlers::HandleBattlemasterHello>);
    CHECK(opcodeTable[CMSG_BATTLEMASTER_JOIN].handler == &OpcodeThunk<&PvpHandlers::HandleBattlemasterJoin>);
    CHECK(opcodeTable[CMSG_BATTLEGROUND_PLAYER_POSITIONS].handler
          == &OpcodeThunk<&PvpHandlers::HandleBattleGroundPlayerPositions>);
    CHECK(opcodeTable[CMSG_PVP_LOG_DATA].handler == &OpcodeThunk<&PvpHandlers::HandlePVPLogData>);
    CHECK(opcodeTable[CMSG_BATTLEFIELD_LIST].handler == &OpcodeThunk<&PvpHandlers::HandleBattlefieldList>);
    CHECK(opcodeTable[CMSG_BATTLEFIELD_PORT].handler == &OpcodeThunk<&PvpHandlers::HandleBattleFieldPort>);
    CHECK(opcodeTable[CMSG_LEAVE_BATTLEFIELD].handler == &OpcodeThunk<&PvpHandlers::HandleLeaveBattlefield>);
    CHECK(opcodeTable[CMSG_BATTLEFIELD_STATUS].handler == &OpcodeThunk<&PvpHandlers::HandleBattlefieldStatus>);
    CHECK(opcodeTable[CMSG_AREA_SPIRIT_HEALER_QUERY].handler == &OpcodeThunk<&PvpHandlers::HandleAreaSpiritHealerQuery>);
    CHECK(opcodeTable[CMSG_AREA_SPIRIT_HEALER_QUEUE].handler == &OpcodeThunk<&PvpHandlers::HandleAreaSpiritHealerQueue>);
    CHECK(opcodeTable[CMSG_BATTLEMASTER_JOIN_ARENA].handler == &OpcodeThunk<&PvpHandlers::HandleBattlemasterJoinArena>);
    CHECK(opcodeTable[CMSG_REPORT_PVP_AFK].handler == &OpcodeThunk<&PvpHandlers::HandleReportPvPAFK>);
    CHECK(opcodeTable[CMSG_REQUEST_RATED_BG_STATS].handler == &OpcodeThunk<&PvpHandlers::HandleRequestRatedBGStats>);
    CHECK(opcodeTable[CMSG_REQUEST_PVP_OPTIONS_ENABLED].handler
          == &OpcodeThunk<&PvpHandlers::HandleRequestPvPOptionsEnabled>);
    CHECK(opcodeTable[CMSG_REQUEST_PVP_REWARDS].handler == &OpcodeThunk<&PvpHandlers::HandleRequestPvPRewards>);
    CHECK(opcodeTable[CMSG_REQUEST_RATED_BG_INFO].handler == &OpcodeThunk<&PvpHandlers::HandleRequestRatedBgInfo>);
    CHECK(opcodeTable[CMSG_BATTLEFIELD_LIST].handler != &OpcodeThunk<&WorldSession::Handle_NULL>);
}

TEST(OpcodeDispatch_LootRowsHoldTheirHandlersThunks)
{
    InitializeOpcodes();

    CHECK(opcodeTable[CMSG_AUTOSTORE_LOOT_ITEM].handler == &OpcodeThunk<&LootHandlers::HandleAutostoreLootItem>);
    CHECK(opcodeTable[CMSG_LOOT_CURRENCY].handler == &OpcodeThunk<&LootHandlers::HandleAutostoreLootItem>);
    CHECK_EQ(CountSlotsBoundTo(&OpcodeThunk<&LootHandlers::HandleAutostoreLootItem>), 2);
    CHECK(opcodeTable[CMSG_LOOT].handler == &OpcodeThunk<&LootHandlers::HandleLoot>);
    CHECK(opcodeTable[CMSG_LOOT_MONEY].handler == &OpcodeThunk<&LootHandlers::HandleLootMoney>);
    CHECK(opcodeTable[CMSG_LOOT_RELEASE].handler == &OpcodeThunk<&LootHandlers::HandleLootRelease>);
    CHECK(opcodeTable[CMSG_LOOT_MASTER_GIVE].handler == &OpcodeThunk<&LootHandlers::HandleLootMasterGive>);
    CHECK(opcodeTable[CMSG_LOOT].handler != &OpcodeThunk<&WorldSession::Handle_NULL>);
}
