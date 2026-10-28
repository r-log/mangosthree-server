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
/// player, sending nothing. The CMSG_AUCTION_PLACE_BID row reads a bid of price 0 on auction 0 at
/// an empty auctioneer guid to its end and returns at the empty auction id before it reads the
/// player, sending nothing. The CMSG_CANCEL_TRADE row, given an empty packet on a session with no
/// player, finds no player to cancel the trade of and returns, reading nothing and sending nothing.
/// The CMSG_SOCKET_GEMS row reads an empty item guid to its end and, the guid naming no item,
/// returns before it reads the player, sending nothing. The CMSG_LFG_JOIN row reads a join with no
/// slot and no comment to its end and, the Dungeon Finder being off, returns before it reads the
/// player, sending nothing. The CMSG_JOIN_CHANNEL row reads a join of channel 0 with an empty name
/// and an empty password to its end and, the name being empty, returns before it reads the player,
/// sending nothing. The CMSG_CHANNEL_SET_OWNER and CMSG_CHANNEL_UNMODERATOR rows read the bytes the
/// 4.3.4 client writes for them (UserClientChatChannelSetOwner::Put and
/// UserClientChatChannelUnmoderator::Put) to their end and, the name being empty, return before they
/// read the player, sending nothing. With members on their sessions, the set-owner row refuses a
/// member who is not the owner and a player who never joined, each told so and neither changing the
/// owner or the member list, and hands the channel over for the owner; the unmoderator row removes
/// the moderator flag of the member the client names. The CMSG_CHANNEL_MUTE, CMSG_CHANNEL_UNMUTE and
/// CMSG_CHANNEL_UNBAN rows read the bytes the client writes for them (UserClientChatChannelMute::Put,
/// UserClientChatChannelUnmute::Put and UserClientChatChannelUnban::Put) to their end and, the name being
/// empty, return before they read the player, sending nothing; with members on their sessions, the mute
/// row sets and the unmute row clears the mute flag of the member the client names, the unban row lifts
/// the ban of the player the client names, and a member who is not a moderator is told so by each of
/// the three and changes nothing. The CMSG_ADD_FRIEND row reads an add-friend request of an empty name and an
/// empty note to its end and, the name being empty, returns before it reads the player, sending
/// nothing. The CMSG_GMTICKET_SYSTEMSTATUS row, given an empty packet on a session with no player,
/// answers with one SMSG_GMTICKET_SYSTEMSTATUS carrying whether the ticket system accepts tickets.
/// The CMSG_OFFER_PETITION row reads an offer of an empty petition guid to an empty player guid to
/// its end and, the guid naming no player online, returns before it reads the player, sending
/// nothing. The CMSG_VOICE_SESSION_ENABLE, CMSG_CHANNEL_VOICE_ON and CMSG_SET_ACTIVE_VOICE_CHANNEL
/// rows, on a session with no player, each read their packet to its end and send nothing, and so
/// does the CMSG_WARDEN_DATA row, of status STATUS_AUTHED. Every row bound to the combat, vendor,
/// pvp, loot, auction, trade, skill, enchant, pet, LFG, contact list, GM ticket, petition, chat
/// channel or voice chat handler class holds the status STATUS_LOGGEDIN
/// and the processing PROCESS_THREADUNSAFE, except the CMSG_ATTACKSWING, CMSG_ATTACKSTOP and
/// CMSG_SETSHEATHED rows, processed PROCESS_INPLACE, the CMSG_CANCEL_TRADE row, of status
/// STATUS_LOGGEDIN_OR_RECENTLY_LOGGEDOUT, and the CMSG_VOICE_SESSION_ENABLE and
/// CMSG_SET_ACTIVE_VOICE_CHANNEL rows, of status STATUS_AUTHED.

#include "TestHarness.h"
#include "OpcodeTable.h"
#include "WorldSession.h"
#include "WorldPacket.h"
#include "SharedDefines.h"
#include "ArenaTeam.h"
#include "GMTicketMgr.h"
#include "Auth/BigNumber.h"
#include "session/handlers/combat/CombatHandlers.h"
#include "session/handlers/connection/WardenHandlers.h"
#include "session/handlers/economy/AuctionHandlers.h"
#include "session/handlers/economy/LootHandlers.h"
#include "session/handlers/economy/TradeHandlers.h"
#include "session/handlers/economy/VendorHandlers.h"
#include "session/handlers/entities/EnchantHandlers.h"
#include "session/handlers/entities/PetHandlers.h"
#include "session/handlers/entities/SkillHandlers.h"
#include "session/handlers/pvp/PvpHandlers.h"
#include "session/handlers/social/ChannelHandlers.h"
#include "Channel.h"
#include "ChannelMgr.h"
#include "Player.h"
#include "PlayerRegistry.h"
#include "session/handlers/social/ContactHandlers.h"
#include "session/handlers/social/LfgHandlers.h"
#include "session/handlers/social/PetitionHandlers.h"
#include "session/handlers/social/TicketHandlers.h"
#include "session/handlers/social/VoiceChatHandlers.h"

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

    int CountSlotsBoundToEither(void (*first)(WorldSession&, WorldPacket&), void (*second)(WorldSession&, WorldPacket&))
    {
        int slots = 0;
        for (uint32 i = 0; i < NUM_MSG_TYPES; ++i)
        {
            if (opcodeTable[i].handler == first || opcodeTable[i].handler == second)
            {
                ++slots;
            }
        }
        return slots;
    }

    /// A player the channel code reaches: its update fields allocated, named, Alliance, flagged in world
    /// and registered, so the registry answers for it, and held by its session for the handler's reads.
    /// It enters no map; it leaves its channels the way a logout does.
    class ChannelMember : public Player
    {
        public:
            ChannelMember(WorldSession& session, uint32 low, const char* name) : Player(&session)
            {
                m_team = ALLIANCE;
                Object::_Create(low, 0, HIGHGUID_PLAYER);
                SetName(name);
                Object::AddToWorld();
                sPlayerRegistry.Add(this);
                session.SetPlayer(this);
            }

            ~ChannelMember()
            {
                CleanupChannels();
                GetSession()->SetPlayer(nullptr);
                sPlayerRegistry.Remove(this);
                Object::RemoveFromWorld();
                delete[] m_uint32Values;
                m_uint32Values = NULL;
            }
    };

    // The bytes UserClientChatChannelSetOwner::Put writes: channel length (8 bits), name length (7 bits), the name, the channel.
    WorldPacket SetOwnerRequest(const char* channel, const char* name)
    {
        WorldPacket request(CMSG_CHANNEL_SET_OWNER);
        request.WriteBits(std::strlen(channel), 8);
        request.WriteBits(std::strlen(name), 7);
        request.WriteStringData(name);
        request.WriteStringData(channel);
        return request;
    }

    // The bytes UserClientChatChannelUnmoderator::Put writes: name length (7 bits), channel length (8 bits), the channel, the name.
    WorldPacket UnmoderatorRequest(const char* channel, const char* name)
    {
        WorldPacket request(CMSG_CHANNEL_UNMODERATOR);
        request.WriteBits(std::strlen(name), 7);
        request.WriteBits(std::strlen(channel), 8);
        request.WriteStringData(channel);
        request.WriteStringData(name);
        return request;
    }

    // The bytes UserClientChatChannelMute::Put writes: channel length (8 bits), name length (7 bits), the channel, the name.
    WorldPacket MuteRequest(const char* channel, const char* name)
    {
        WorldPacket request(CMSG_CHANNEL_MUTE);
        request.WriteBits(std::strlen(channel), 8);
        request.WriteBits(std::strlen(name), 7);
        request.WriteStringData(channel);
        request.WriteStringData(name);
        return request;
    }

    // The bytes UserClientChatChannelUnmute::Put writes: channel length (8 bits), name length (7 bits), the channel, the name.
    WorldPacket UnmuteRequest(const char* channel, const char* name)
    {
        WorldPacket request(CMSG_CHANNEL_UNMUTE);
        request.WriteBits(std::strlen(channel), 8);
        request.WriteBits(std::strlen(name), 7);
        request.WriteStringData(channel);
        request.WriteStringData(name);
        return request;
    }

    // The bytes UserClientChatChannelUnban::Put writes: name length (7 bits), channel length (8 bits), the channel, the name.
    WorldPacket UnbanRequest(const char* channel, const char* name)
    {
        WorldPacket request(CMSG_CHANNEL_UNBAN);
        request.WriteBits(std::strlen(name), 7);
        request.WriteBits(std::strlen(channel), 8);
        request.WriteStringData(channel);
        request.WriteStringData(name);
        return request;
    }

    // A channel notice's type, channel name and the guid after them (empty when the notice carries none).
    struct Notice
    {
        explicit Notice(WorldPacket const& packet) : opcode(packet.GetOpcode()), type(0), oldFlags(0), newFlags(0)
        {
            WorldPacket copy(packet);
            copy >> type;
            copy >> channel;
            if (copy.rpos() + 8 <= copy.size())
            {
                copy >> guid;
            }
            if (copy.rpos() + 2 <= copy.size())
            {
                copy >> oldFlags;
                copy >> newFlags;
            }
        }

        uint16 opcode;
        uint8 type;
        std::string channel;
        ObjectGuid guid;
        uint8 oldFlags;
        uint8 newFlags;
    };
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

TEST(OpcodeDispatch_CombatRowsKeepTheirStatusAndProcessing)
{
    InitializeOpcodes();

    CHECK_EQ(opcodeTable[CMSG_ATTACKSWING].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_ATTACKSWING].packetProcessing, PROCESS_INPLACE);
    CHECK_EQ(opcodeTable[CMSG_ATTACKSTOP].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_ATTACKSTOP].packetProcessing, PROCESS_INPLACE);
    CHECK_EQ(opcodeTable[CMSG_DUEL_ACCEPTED].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_DUEL_ACCEPTED].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_DUEL_CANCELLED].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_DUEL_CANCELLED].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_SETSHEATHED].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_SETSHEATHED].packetProcessing, PROCESS_INPLACE);
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

TEST(OpcodeDispatch_VendorRowsKeepTheirStatusAndProcessing)
{
    InitializeOpcodes();

    CHECK_EQ(opcodeTable[CMSG_AUTOSTORE_BAG_ITEM].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_AUTOSTORE_BAG_ITEM].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_LIST_INVENTORY].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_LIST_INVENTORY].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_SELL_ITEM].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_SELL_ITEM].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_BUY_ITEM].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_BUY_ITEM].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_BUY_BANK_SLOT].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_BUY_BANK_SLOT].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_AUTOSTORE_BANK_ITEM].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_AUTOSTORE_BANK_ITEM].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_AUTOBANK_ITEM].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_AUTOBANK_ITEM].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_BUYBACK_ITEM].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_BUYBACK_ITEM].packetProcessing, PROCESS_THREADUNSAFE);
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

TEST(OpcodeDispatch_PvpRowsKeepTheirStatusAndProcessing)
{
    InitializeOpcodes();

    CHECK_EQ(opcodeTable[CMSG_BATTLEFIELD_LIST].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_BATTLEFIELD_LIST].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_BATTLEFIELD_STATUS].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_BATTLEFIELD_STATUS].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_BATTLEFIELD_PORT].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_BATTLEFIELD_PORT].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_BATTLEMASTER_HELLO].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_BATTLEMASTER_HELLO].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_PVP_LOG_DATA].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_PVP_LOG_DATA].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_LEAVE_BATTLEFIELD].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_LEAVE_BATTLEFIELD].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_AREA_SPIRIT_HEALER_QUERY].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_AREA_SPIRIT_HEALER_QUERY].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_AREA_SPIRIT_HEALER_QUEUE].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_AREA_SPIRIT_HEALER_QUEUE].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_BATTLEGROUND_PLAYER_POSITIONS].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_BATTLEGROUND_PLAYER_POSITIONS].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_BATTLEMASTER_JOIN].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_BATTLEMASTER_JOIN].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_ARENA_TEAM_CREATE].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_ARENA_TEAM_CREATE].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_ARENA_TEAM_QUERY].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_ARENA_TEAM_QUERY].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_ARENA_TEAM_ROSTER].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_ARENA_TEAM_ROSTER].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_ARENA_TEAM_INVITE].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_ARENA_TEAM_INVITE].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_ARENA_TEAM_ACCEPT].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_ARENA_TEAM_ACCEPT].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_ARENA_TEAM_DECLINE].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_ARENA_TEAM_DECLINE].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_ARENA_TEAM_LEAVE].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_ARENA_TEAM_LEAVE].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_ARENA_TEAM_REMOVE].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_ARENA_TEAM_REMOVE].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_ARENA_TEAM_DISBAND].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_ARENA_TEAM_DISBAND].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_ARENA_TEAM_LEADER].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_ARENA_TEAM_LEADER].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_BATTLEMASTER_JOIN_ARENA].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_BATTLEMASTER_JOIN_ARENA].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[MSG_INSPECT_ARENA_TEAMS].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[MSG_INSPECT_ARENA_TEAMS].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_REPORT_PVP_AFK].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_REPORT_PVP_AFK].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_REQUEST_RATED_BG_INFO].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_REQUEST_RATED_BG_INFO].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_REQUEST_RATED_BG_STATS].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_REQUEST_RATED_BG_STATS].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_REQUEST_PVP_REWARDS].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_REQUEST_PVP_REWARDS].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_REQUEST_PVP_OPTIONS_ENABLED].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_REQUEST_PVP_OPTIONS_ENABLED].packetProcessing, PROCESS_THREADUNSAFE);
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

TEST(OpcodeDispatch_LootRowsKeepTheirStatusAndProcessing)
{
    InitializeOpcodes();

    CHECK_EQ(opcodeTable[CMSG_AUTOSTORE_LOOT_ITEM].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_AUTOSTORE_LOOT_ITEM].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_LOOT_CURRENCY].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_LOOT_CURRENCY].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_LOOT].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_LOOT].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_LOOT_MONEY].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_LOOT_MONEY].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_LOOT_RELEASE].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_LOOT_RELEASE].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_LOOT_MASTER_GIVE].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_LOOT_MASTER_GIVE].packetProcessing, PROCESS_THREADUNSAFE);
}

TEST(OpcodeDispatch_AuctionRowReachesItsHandlerWithThePacket)
{
    InitializeOpcodes();

    std::vector<WorldPacket> sent;
    WorldSession session(1, "dispatch", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    session.SetSocketlessSink(&CapturePacket, &sent);

    WorldPacket bid(CMSG_AUCTION_PLACE_BID, 20);
    bid << uint64(0);
    bid << uint32(0);
    bid << uint64(0);

    opcodeTable[CMSG_AUCTION_PLACE_BID].handler(session, bid);

    CHECK_EQ(bid.rpos(), size_t(20));
    CHECK(sent.empty());

    session.SetSocketlessSink(nullptr, nullptr);
}

TEST(OpcodeDispatch_AuctionRowsHoldTheirHandlersThunks)
{
    InitializeOpcodes();

    CHECK(opcodeTable[MSG_AUCTION_HELLO].handler == &OpcodeThunk<&AuctionHandlers::HandleAuctionHello>);
    CHECK(opcodeTable[CMSG_AUCTION_SELL_ITEM].handler == &OpcodeThunk<&AuctionHandlers::HandleAuctionSellItem>);
    CHECK(opcodeTable[CMSG_AUCTION_REMOVE_ITEM].handler == &OpcodeThunk<&AuctionHandlers::HandleAuctionRemoveItem>);
    CHECK(opcodeTable[CMSG_AUCTION_LIST_ITEMS].handler == &OpcodeThunk<&AuctionHandlers::HandleAuctionListItems>);
    CHECK(opcodeTable[CMSG_AUCTION_LIST_OWNER_ITEMS].handler
          == &OpcodeThunk<&AuctionHandlers::HandleAuctionListOwnerItems>);
    CHECK(opcodeTable[CMSG_AUCTION_PLACE_BID].handler == &OpcodeThunk<&AuctionHandlers::HandleAuctionPlaceBid>);
    CHECK(opcodeTable[CMSG_AUCTION_LIST_BIDDER_ITEMS].handler
          == &OpcodeThunk<&AuctionHandlers::HandleAuctionListBidderItems>);
    CHECK(opcodeTable[CMSG_AUCTION_LIST_PENDING_SALES].handler
          == &OpcodeThunk<&AuctionHandlers::HandleAuctionListPendingSales>);
    CHECK(opcodeTable[CMSG_AUCTION_PLACE_BID].handler != &OpcodeThunk<&WorldSession::Handle_NULL>);
}

TEST(OpcodeDispatch_AuctionRowsKeepTheirStatusAndProcessing)
{
    InitializeOpcodes();

    CHECK_EQ(opcodeTable[MSG_AUCTION_HELLO].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[MSG_AUCTION_HELLO].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_AUCTION_SELL_ITEM].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_AUCTION_SELL_ITEM].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_AUCTION_REMOVE_ITEM].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_AUCTION_REMOVE_ITEM].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_AUCTION_LIST_ITEMS].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_AUCTION_LIST_ITEMS].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_AUCTION_LIST_OWNER_ITEMS].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_AUCTION_LIST_OWNER_ITEMS].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_AUCTION_PLACE_BID].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_AUCTION_PLACE_BID].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_AUCTION_LIST_BIDDER_ITEMS].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_AUCTION_LIST_BIDDER_ITEMS].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_AUCTION_LIST_PENDING_SALES].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_AUCTION_LIST_PENDING_SALES].packetProcessing, PROCESS_THREADUNSAFE);
}

TEST(OpcodeDispatch_TradeRowReachesItsHandlerWithThePacket)
{
    InitializeOpcodes();

    std::vector<WorldPacket> sent;
    WorldSession session(1, "dispatch", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    session.SetSocketlessSink(&CapturePacket, &sent);

    WorldPacket cancel(CMSG_CANCEL_TRADE, 0);

    opcodeTable[CMSG_CANCEL_TRADE].handler(session, cancel);

    CHECK_EQ(cancel.rpos(), size_t(0));
    CHECK(sent.empty());

    session.SetSocketlessSink(nullptr, nullptr);
}

TEST(OpcodeDispatch_TradeRowsHoldTheirHandlersThunks)
{
    InitializeOpcodes();

    CHECK(opcodeTable[CMSG_INITIATE_TRADE].handler == &OpcodeThunk<&TradeHandlers::HandleInitiateTrade>);
    CHECK(opcodeTable[CMSG_BEGIN_TRADE].handler == &OpcodeThunk<&TradeHandlers::HandleBeginTrade>);
    CHECK(opcodeTable[CMSG_BUSY_TRADE].handler == &OpcodeThunk<&TradeHandlers::HandleBusyTrade>);
    CHECK(opcodeTable[CMSG_IGNORE_TRADE].handler == &OpcodeThunk<&TradeHandlers::HandleIgnoreTrade>);
    CHECK(opcodeTable[CMSG_ACCEPT_TRADE].handler == &OpcodeThunk<&TradeHandlers::HandleAcceptTrade>);
    CHECK(opcodeTable[CMSG_UNACCEPT_TRADE].handler == &OpcodeThunk<&TradeHandlers::HandleUnacceptTrade>);
    CHECK(opcodeTable[CMSG_CANCEL_TRADE].handler == &OpcodeThunk<&TradeHandlers::HandleCancelTrade>);
    CHECK(opcodeTable[CMSG_SET_TRADE_ITEM].handler == &OpcodeThunk<&TradeHandlers::HandleSetTradeItem>);
    CHECK(opcodeTable[CMSG_CLEAR_TRADE_ITEM].handler == &OpcodeThunk<&TradeHandlers::HandleClearTradeItem>);
    CHECK(opcodeTable[CMSG_SET_TRADE_GOLD].handler == &OpcodeThunk<&TradeHandlers::HandleSetTradeGold>);
    CHECK(opcodeTable[CMSG_CANCEL_TRADE].handler != &OpcodeThunk<&WorldSession::Handle_NULL>);
}

TEST(OpcodeDispatch_TradeRowsKeepTheirStatusAndProcessing)
{
    InitializeOpcodes();

    CHECK_EQ(opcodeTable[CMSG_INITIATE_TRADE].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_INITIATE_TRADE].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_BEGIN_TRADE].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_BEGIN_TRADE].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_BUSY_TRADE].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_BUSY_TRADE].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_IGNORE_TRADE].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_IGNORE_TRADE].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_ACCEPT_TRADE].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_ACCEPT_TRADE].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_UNACCEPT_TRADE].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_UNACCEPT_TRADE].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_CANCEL_TRADE].status, STATUS_LOGGEDIN_OR_RECENTLY_LOGGEDOUT);
    CHECK_EQ(opcodeTable[CMSG_CANCEL_TRADE].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_SET_TRADE_ITEM].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_SET_TRADE_ITEM].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_CLEAR_TRADE_ITEM].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_CLEAR_TRADE_ITEM].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_SET_TRADE_GOLD].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_SET_TRADE_GOLD].packetProcessing, PROCESS_THREADUNSAFE);
}

TEST(OpcodeDispatch_SkillRowsHoldTheirHandlersThunks)
{
    InitializeOpcodes();

    CHECK(opcodeTable[CMSG_LEARN_TALENT].handler == &OpcodeThunk<&SkillHandlers::HandleLearnTalent>);
    CHECK(opcodeTable[CMSG_LEARN_TALENT_GROUP].handler == &OpcodeThunk<&SkillHandlers::HandleLearnPreviewTalents>);
    CHECK_EQ(CountSlotsBoundTo(&OpcodeThunk<&SkillHandlers::HandleLearnPreviewTalents>), 1);
    CHECK(opcodeTable[MSG_TALENT_WIPE_CONFIRM].handler == &OpcodeThunk<&SkillHandlers::HandleTalentWipeConfirm>);
    CHECK(opcodeTable[CMSG_UNLEARN_SKILL].handler == &OpcodeThunk<&SkillHandlers::HandleUnlearnSkill>);
    CHECK(opcodeTable[CMSG_LEARN_TALENT].handler != &OpcodeThunk<&WorldSession::Handle_NULL>);
}

TEST(OpcodeDispatch_SkillRowsKeepTheirStatusAndProcessing)
{
    InitializeOpcodes();

    CHECK_EQ(opcodeTable[CMSG_LEARN_TALENT].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_LEARN_TALENT].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_LEARN_TALENT_GROUP].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_LEARN_TALENT_GROUP].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[MSG_TALENT_WIPE_CONFIRM].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[MSG_TALENT_WIPE_CONFIRM].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_UNLEARN_SKILL].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_UNLEARN_SKILL].packetProcessing, PROCESS_THREADUNSAFE);
}

TEST(OpcodeDispatch_EnchantRowReachesItsHandlerWithThePacket)
{
    InitializeOpcodes();

    std::vector<WorldPacket> sent;
    WorldSession session(1, "dispatch", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    session.SetSocketlessSink(&CapturePacket, &sent);

    WorldPacket socket(CMSG_SOCKET_GEMS, 8);
    socket << uint64(0);

    opcodeTable[CMSG_SOCKET_GEMS].handler(session, socket);

    CHECK_EQ(socket.rpos(), size_t(8));
    CHECK(sent.empty());

    session.SetSocketlessSink(nullptr, nullptr);
}

TEST(OpcodeDispatch_EnchantRowsHoldTheirHandlersThunks)
{
    InitializeOpcodes();

    CHECK(opcodeTable[CMSG_WRAP_ITEM].handler == &OpcodeThunk<&EnchantHandlers::HandleWrapItem>);
    CHECK(opcodeTable[CMSG_SOCKET_GEMS].handler == &OpcodeThunk<&EnchantHandlers::HandleSocket>);
    CHECK(opcodeTable[CMSG_CANCEL_TEMP_ENCHANTMENT].handler
          == &OpcodeThunk<&EnchantHandlers::HandleCancelTempEnchantment>);
    CHECK(opcodeTable[CMSG_SOCKET_GEMS].handler != &OpcodeThunk<&WorldSession::Handle_NULL>);
}

TEST(OpcodeDispatch_EnchantRowsKeepTheirStatusAndProcessing)
{
    InitializeOpcodes();

    CHECK_EQ(opcodeTable[CMSG_WRAP_ITEM].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_WRAP_ITEM].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_SOCKET_GEMS].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_SOCKET_GEMS].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_CANCEL_TEMP_ENCHANTMENT].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_CANCEL_TEMP_ENCHANTMENT].packetProcessing, PROCESS_THREADUNSAFE);
}

TEST(OpcodeDispatch_PetCommandRowsHoldTheirHandlersThunks)
{
    InitializeOpcodes();

    CHECK(opcodeTable[CMSG_PET_ACTION].handler == &OpcodeThunk<&PetHandlers::HandlePetAction>);
    CHECK_EQ(CountSlotsBoundTo(&OpcodeThunk<&PetHandlers::HandlePetAction>), 1);
    CHECK(opcodeTable[CMSG_PET_STOP_ATTACK].handler == &OpcodeThunk<&PetHandlers::HandlePetStopAttack>);
    CHECK_EQ(CountSlotsBoundTo(&OpcodeThunk<&PetHandlers::HandlePetStopAttack>), 1);
    CHECK(opcodeTable[CMSG_PET_SET_ACTION].handler == &OpcodeThunk<&PetHandlers::HandlePetSetAction>);
    CHECK_EQ(CountSlotsBoundTo(&OpcodeThunk<&PetHandlers::HandlePetSetAction>), 1);
    CHECK(opcodeTable[CMSG_PET_SPELL_AUTOCAST].handler == &OpcodeThunk<&PetHandlers::HandlePetSpellAutocast>);
    CHECK_EQ(CountSlotsBoundTo(&OpcodeThunk<&PetHandlers::HandlePetSpellAutocast>), 1);
    CHECK(opcodeTable[CMSG_PET_CAST_SPELL].handler == &OpcodeThunk<&PetHandlers::HandlePetCastSpell>);
    CHECK_EQ(CountSlotsBoundTo(&OpcodeThunk<&PetHandlers::HandlePetCastSpell>), 1);
    CHECK(opcodeTable[CMSG_PET_ACTION].handler != &OpcodeThunk<&WorldSession::Handle_NULL>);
}

TEST(OpcodeDispatch_PetNameTalentAndCritterRowsHoldTheirHandlersThunks)
{
    InitializeOpcodes();

    CHECK(opcodeTable[CMSG_PET_NAME_QUERY].handler == &OpcodeThunk<&PetHandlers::HandlePetNameQuery>);
    CHECK_EQ(CountSlotsBoundTo(&OpcodeThunk<&PetHandlers::HandlePetNameQuery>), 1);
    CHECK(opcodeTable[CMSG_PET_RENAME].handler == &OpcodeThunk<&PetHandlers::HandlePetRename>);
    CHECK_EQ(CountSlotsBoundTo(&OpcodeThunk<&PetHandlers::HandlePetRename>), 1);
    CHECK(opcodeTable[CMSG_PET_ABANDON].handler == &OpcodeThunk<&PetHandlers::HandlePetAbandon>);
    CHECK_EQ(CountSlotsBoundTo(&OpcodeThunk<&PetHandlers::HandlePetAbandon>), 1);
    CHECK(opcodeTable[CMSG_PET_LEARN_TALENT].handler == &OpcodeThunk<&PetHandlers::HandlePetLearnTalent>);
    CHECK_EQ(CountSlotsBoundTo(&OpcodeThunk<&PetHandlers::HandlePetLearnTalent>), 1);
    CHECK(opcodeTable[CMSG_PET_LEARN_TALENT_GROUP].handler == &OpcodeThunk<&PetHandlers::HandleLearnPreviewTalentsPet>);
    CHECK_EQ(CountSlotsBoundTo(&OpcodeThunk<&PetHandlers::HandleLearnPreviewTalentsPet>), 1);
    CHECK(opcodeTable[CMSG_DISMISS_CRITTER].handler == &OpcodeThunk<&PetHandlers::HandleDismissCritter>);
    CHECK_EQ(CountSlotsBoundTo(&OpcodeThunk<&PetHandlers::HandleDismissCritter>), 1);
    CHECK(opcodeTable[CMSG_PET_RENAME].handler != &OpcodeThunk<&WorldSession::Handle_NULL>);
}

TEST(OpcodeDispatch_PetRowsKeepTheirStatusAndProcessing)
{
    InitializeOpcodes();

    CHECK_EQ(opcodeTable[CMSG_PET_ACTION].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_PET_ACTION].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_PET_STOP_ATTACK].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_PET_STOP_ATTACK].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_PET_SET_ACTION].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_PET_SET_ACTION].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_PET_SPELL_AUTOCAST].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_PET_SPELL_AUTOCAST].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_PET_CAST_SPELL].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_PET_CAST_SPELL].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_PET_NAME_QUERY].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_PET_NAME_QUERY].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_PET_RENAME].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_PET_RENAME].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_PET_ABANDON].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_PET_ABANDON].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_PET_LEARN_TALENT].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_PET_LEARN_TALENT].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_PET_LEARN_TALENT_GROUP].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_PET_LEARN_TALENT_GROUP].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_DISMISS_CRITTER].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_DISMISS_CRITTER].packetProcessing, PROCESS_THREADUNSAFE);
}

TEST(OpcodeDispatch_LfgJoinRowReachesItsHandlerWithThePacket)
{
    InitializeOpcodes();

    std::vector<WorldPacket> sent;
    WorldSession session(1, "dispatch", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    session.SetSocketlessSink(&CapturePacket, &sent);

    WorldPacket join(CMSG_LFG_JOIN, 21);
    join << uint32(0);
    join << uint32(0) << uint32(0) << uint32(0);
    join.WriteBits(0, 9);
    join.WriteBits(0, 24);
    join.FlushBits();

    opcodeTable[CMSG_LFG_JOIN].handler(session, join);

    CHECK_EQ(join.rpos(), size_t(21));
    CHECK(sent.empty());

    session.SetSocketlessSink(nullptr, nullptr);
}

TEST(OpcodeDispatch_LfgRowsHoldTheirHandlersThunks)
{
    InitializeOpcodes();

    CHECK(opcodeTable[CMSG_LFG_JOIN].handler == &OpcodeThunk<&LfgHandlers::HandleLfgJoin>);
    CHECK_EQ(CountSlotsBoundTo(&OpcodeThunk<&LfgHandlers::HandleLfgJoin>), 1);
    CHECK(opcodeTable[CMSG_LFG_LEAVE].handler == &OpcodeThunk<&LfgHandlers::HandleLfgLeave>);
    CHECK_EQ(CountSlotsBoundTo(&OpcodeThunk<&LfgHandlers::HandleLfgLeave>), 1);
    CHECK(opcodeTable[CMSG_LFG_SET_ROLES].handler == &OpcodeThunk<&LfgHandlers::HandleLfgSetRoles>);
    CHECK_EQ(CountSlotsBoundTo(&OpcodeThunk<&LfgHandlers::HandleLfgSetRoles>), 1);
    CHECK(opcodeTable[CMSG_LFG_GET_STATUS].handler == &OpcodeThunk<&LfgHandlers::HandleLfgGetStatus>);
    CHECK_EQ(CountSlotsBoundTo(&OpcodeThunk<&LfgHandlers::HandleLfgGetStatus>), 1);
    CHECK(opcodeTable[CMSG_LFG_PROPOSAL_RESULT].handler == &OpcodeThunk<&LfgHandlers::HandleLfgProposalResult>);
    CHECK_EQ(CountSlotsBoundTo(&OpcodeThunk<&LfgHandlers::HandleLfgProposalResult>), 1);
    CHECK(opcodeTable[CMSG_LFG_TELEPORT].handler == &OpcodeThunk<&LfgHandlers::HandleLfgTeleport>);
    CHECK_EQ(CountSlotsBoundTo(&OpcodeThunk<&LfgHandlers::HandleLfgTeleport>), 1);
    CHECK(opcodeTable[CMSG_SET_LFG_COMMENT].handler == &OpcodeThunk<&LfgHandlers::HandleSetLfgComment>);
    CHECK_EQ(CountSlotsBoundTo(&OpcodeThunk<&LfgHandlers::HandleSetLfgComment>), 1);
    CHECK(opcodeTable[CMSG_LFG_LOCK_INFO_REQUEST].handler == &OpcodeThunk<&LfgHandlers::HandleLfgLockInfoRequest>);
    CHECK_EQ(CountSlotsBoundTo(&OpcodeThunk<&LfgHandlers::HandleLfgLockInfoRequest>), 1);
    CHECK(opcodeTable[CMSG_LFG_SET_BOOT_VOTE].handler == &OpcodeThunk<&LfgHandlers::HandleLfgSetBootVote>);
    CHECK_EQ(CountSlotsBoundTo(&OpcodeThunk<&LfgHandlers::HandleLfgSetBootVote>), 1);
    CHECK(opcodeTable[CMSG_LFG_JOIN].handler != &OpcodeThunk<&WorldSession::Handle_NULL>);
}

TEST(OpcodeDispatch_LfgRowsKeepTheirStatusAndProcessing)
{
    InitializeOpcodes();

    CHECK_EQ(opcodeTable[CMSG_LFG_JOIN].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_LFG_JOIN].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_LFG_LEAVE].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_LFG_LEAVE].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_LFG_SET_ROLES].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_LFG_SET_ROLES].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_LFG_GET_STATUS].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_LFG_GET_STATUS].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_LFG_PROPOSAL_RESULT].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_LFG_PROPOSAL_RESULT].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_LFG_TELEPORT].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_LFG_TELEPORT].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_SET_LFG_COMMENT].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_SET_LFG_COMMENT].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_LFG_LOCK_INFO_REQUEST].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_LFG_LOCK_INFO_REQUEST].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_LFG_SET_BOOT_VOTE].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_LFG_SET_BOOT_VOTE].packetProcessing, PROCESS_THREADUNSAFE);
}

TEST(OpcodeDispatch_JoinChannelRowReturnsBeforeAnyPlayerRead)
{
    InitializeOpcodes();

    std::vector<WorldPacket> sent;
    WorldSession session(1, "dispatch", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    session.SetSocketlessSink(&CapturePacket, &sent);

    WorldPacket join(CMSG_JOIN_CHANNEL, 7);
    join << uint32(0);
    join << uint8(0);
    join << uint8(0);
    join << uint8(0);

    opcodeTable[CMSG_JOIN_CHANNEL].handler(session, join);

    CHECK_EQ(join.rpos(), size_t(7));
    CHECK(sent.empty());

    session.SetSocketlessSink(nullptr, nullptr);
}

TEST(OpcodeDispatch_ChannelSetOwnerRowReadsTheClientLayoutAndReturnsBeforeAnyPlayerRead)
{
    InitializeOpcodes();

    std::vector<WorldPacket> sent;
    WorldSession session(1, "dispatch", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    session.SetSocketlessSink(&CapturePacket, &sent);

    WorldPacket request = SetOwnerRequest("wave12", "");
    CHECK_HEX(request.contents(), request.size(), "0600776176653132");

    opcodeTable[CMSG_CHANNEL_SET_OWNER].handler(session, request);

    CHECK_EQ(request.rpos(), size_t(8));
    CHECK(sent.empty());

    session.SetSocketlessSink(nullptr, nullptr);
}

TEST(OpcodeDispatch_ChannelUnmoderatorRowReadsTheClientLayoutAndReturnsBeforeAnyPlayerRead)
{
    InitializeOpcodes();

    std::vector<WorldPacket> sent;
    WorldSession session(1, "dispatch", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    session.SetSocketlessSink(&CapturePacket, &sent);

    WorldPacket request = UnmoderatorRequest("wave12", "");
    CHECK_HEX(request.contents(), request.size(), "000c776176653132");

    opcodeTable[CMSG_CHANNEL_UNMODERATOR].handler(session, request);

    CHECK_EQ(request.rpos(), size_t(8));
    CHECK(sent.empty());

    session.SetSocketlessSink(nullptr, nullptr);
}

TEST(OpcodeDispatch_ChannelMuteRowReadsTheClientLayoutAndReturnsBeforeAnyPlayerRead)
{
    InitializeOpcodes();

    std::vector<WorldPacket> sent;
    WorldSession session(1, "dispatch", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    session.SetSocketlessSink(&CapturePacket, &sent);

    WorldPacket request = MuteRequest("wave12", "");
    CHECK_HEX(request.contents(), request.size(), "0600776176653132");

    opcodeTable[CMSG_CHANNEL_MUTE].handler(session, request);

    CHECK_EQ(request.rpos(), size_t(8));
    CHECK(sent.empty());

    session.SetSocketlessSink(nullptr, nullptr);
}

TEST(OpcodeDispatch_ChannelUnmuteRowReadsTheClientLayoutAndReturnsBeforeAnyPlayerRead)
{
    InitializeOpcodes();

    std::vector<WorldPacket> sent;
    WorldSession session(1, "dispatch", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    session.SetSocketlessSink(&CapturePacket, &sent);

    WorldPacket request = UnmuteRequest("wave12", "");
    CHECK_HEX(request.contents(), request.size(), "0600776176653132");

    opcodeTable[CMSG_CHANNEL_UNMUTE].handler(session, request);

    CHECK_EQ(request.rpos(), size_t(8));
    CHECK(sent.empty());

    session.SetSocketlessSink(nullptr, nullptr);
}

TEST(OpcodeDispatch_ChannelUnbanRowReadsTheClientLayoutAndReturnsBeforeAnyPlayerRead)
{
    InitializeOpcodes();

    std::vector<WorldPacket> sent;
    WorldSession session(1, "dispatch", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    session.SetSocketlessSink(&CapturePacket, &sent);

    WorldPacket request = UnbanRequest("wave12", "");
    CHECK_HEX(request.contents(), request.size(), "000c776176653132");

    opcodeTable[CMSG_CHANNEL_UNBAN].handler(session, request);

    CHECK_EQ(request.rpos(), size_t(8));
    CHECK(sent.empty());

    session.SetSocketlessSink(nullptr, nullptr);
}

TEST(OpcodeDispatch_ChannelSetOwnerRowRefusesANonOwnerAndANonMemberAndObeysTheOwner)
{
    InitializeOpcodes();

    std::vector<WorldPacket> toAlpha, toBravo, toCharlie;
    WorldSession alphaSession(1, "alpha", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    WorldSession bravoSession(2, "bravo", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    WorldSession charlieSession(3, "charlie", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    alphaSession.SetSocketlessSink(&CapturePacket, &toAlpha);
    bravoSession.SetSocketlessSink(&CapturePacket, &toBravo);
    charlieSession.SetSocketlessSink(&CapturePacket, &toCharlie);
    {
        ChannelMember alpha(alphaSession, 7101, "Alpha");
        ChannelMember bravo(bravoSession, 7102, "Bravo");
        ChannelMember charlie(charlieSession, 7103, "Charlie");
        Channel* channel = channelMgr(ALLIANCE)->GetJoinChannel("owner12", 0);
        channel->Join(&alpha, "");
        channel->Join(&bravo, "");
        REQUIRE(channel->GetNumPlayers() == 2);
        size_t const alphaHeard = toAlpha.size();
        size_t const bravoHeard = toBravo.size();

        // A member who is not the owner is told so; nobody else hears of it.
        WorldPacket fromBravo = SetOwnerRequest("owner12", "Alpha");
        opcodeTable[CMSG_CHANNEL_SET_OWNER].handler(bravoSession, fromBravo);
        CHECK_EQ(fromBravo.rpos(), fromBravo.size());
        REQUIRE(toBravo.size() == bravoHeard + 1);
        CHECK_EQ(Notice(toBravo.back()).opcode, uint16(SMSG_CHANNEL_NOTIFY));
        CHECK_EQ(Notice(toBravo.back()).type, uint8(CHAT_NOT_OWNER_NOTICE));
        CHECK_STR(Notice(toBravo.back()).channel, "owner12");
        CHECK_EQ(toAlpha.size(), alphaHeard);
        CHECK_EQ(channel->GetNumPlayers(), uint32(2));

        // A player who never joined is told so and is not added.
        WorldPacket fromCharlie = SetOwnerRequest("owner12", "Charlie");
        opcodeTable[CMSG_CHANNEL_SET_OWNER].handler(charlieSession, fromCharlie);
        CHECK_EQ(fromCharlie.rpos(), fromCharlie.size());
        REQUIRE(toCharlie.size() == 1);
        CHECK_EQ(Notice(toCharlie.back()).type, uint8(CHAT_NOT_MEMBER_NOTICE));
        CHECK_EQ(toAlpha.size(), alphaHeard);
        CHECK_EQ(toBravo.size(), bravoHeard + 1);
        CHECK_EQ(channel->GetNumPlayers(), uint32(2));

        // The first joiner is still the owner: its request hands the channel over, and every member hears it.
        WorldPacket fromAlpha = SetOwnerRequest("owner12", "Bravo");
        opcodeTable[CMSG_CHANNEL_SET_OWNER].handler(alphaSession, fromAlpha);
        CHECK_EQ(fromAlpha.rpos(), fromAlpha.size());
        REQUIRE(toAlpha.size() == alphaHeard + 2);
        REQUIRE(toBravo.size() == bravoHeard + 3);
        CHECK_EQ(Notice(toAlpha[alphaHeard]).type, uint8(CHAT_MODE_CHANGE_NOTICE));
        CHECK(Notice(toAlpha[alphaHeard]).guid == bravo.GetObjectGuid());
        CHECK_EQ(Notice(toAlpha.back()).type, uint8(CHAT_OWNER_CHANGED_NOTICE));
        CHECK(Notice(toAlpha.back()).guid == bravo.GetObjectGuid());
        CHECK_EQ(Notice(toBravo.back()).type, uint8(CHAT_OWNER_CHANGED_NOTICE));
        CHECK(Notice(toBravo.back()).guid == bravo.GetObjectGuid());
        CHECK_EQ(toCharlie.size(), size_t(1));
        CHECK_EQ(channel->GetNumPlayers(), uint32(2));
    }
    alphaSession.SetSocketlessSink(nullptr, nullptr);
    bravoSession.SetSocketlessSink(nullptr, nullptr);
    charlieSession.SetSocketlessSink(nullptr, nullptr);
}

TEST(OpcodeDispatch_ChannelUnmoderatorRowRemovesTheModeratorTheClientNames)
{
    InitializeOpcodes();

    std::vector<WorldPacket> toAlpha, toBravo;
    WorldSession alphaSession(1, "alpha", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    WorldSession bravoSession(2, "bravo", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    alphaSession.SetSocketlessSink(&CapturePacket, &toAlpha);
    bravoSession.SetSocketlessSink(&CapturePacket, &toBravo);
    {
        ChannelMember alpha(alphaSession, 7111, "Alpha");
        ChannelMember bravo(bravoSession, 7112, "Bravo");
        Channel* channel = channelMgr(ALLIANCE)->GetJoinChannel("unmod12", 0);
        channel->Join(&alpha, "");
        channel->Join(&bravo, "");
        channel->SetModerator(&alpha, "Bravo");
        REQUIRE(channel->GetNumPlayers() == 2);
        size_t const alphaHeard = toAlpha.size();
        size_t const bravoHeard = toBravo.size();

        WorldPacket fromAlpha = UnmoderatorRequest("unmod12", "Bravo");
        opcodeTable[CMSG_CHANNEL_UNMODERATOR].handler(alphaSession, fromAlpha);
        CHECK_EQ(fromAlpha.rpos(), fromAlpha.size());
        REQUIRE(toAlpha.size() == alphaHeard + 1);
        REQUIRE(toBravo.size() == bravoHeard + 1);
        Notice const heard(toBravo.back());
        CHECK_EQ(heard.opcode, uint16(SMSG_CHANNEL_NOTIFY));
        CHECK_EQ(heard.type, uint8(CHAT_MODE_CHANGE_NOTICE));
        CHECK_STR(heard.channel, "unmod12");
        CHECK(heard.guid == bravo.GetObjectGuid());
        CHECK_EQ(heard.oldFlags & 0x02, 0x02);
        CHECK_EQ(heard.newFlags & 0x02, 0);
        CHECK_EQ(Notice(toAlpha.back()).type, uint8(CHAT_MODE_CHANGE_NOTICE));
        CHECK_EQ(channel->GetNumPlayers(), uint32(2));
    }
    alphaSession.SetSocketlessSink(nullptr, nullptr);
    bravoSession.SetSocketlessSink(nullptr, nullptr);
}

TEST(OpcodeDispatch_ChannelMuteRowMutesTheMemberTheClientNamesForAModeratorOnly)
{
    InitializeOpcodes();

    std::vector<WorldPacket> toAlpha, toBravo;
    WorldSession alphaSession(1, "alpha", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    WorldSession bravoSession(2, "bravo", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    alphaSession.SetSocketlessSink(&CapturePacket, &toAlpha);
    bravoSession.SetSocketlessSink(&CapturePacket, &toBravo);
    {
        ChannelMember alpha(alphaSession, 7121, "Alpha");
        ChannelMember bravo(bravoSession, 7122, "Bravo");
        Channel* channel = channelMgr(ALLIANCE)->GetJoinChannel("mute12", 0);
        channel->Join(&alpha, "");
        channel->Join(&bravo, "");
        REQUIRE(channel->GetNumPlayers() == 2);
        size_t const alphaHeard = toAlpha.size();
        size_t const bravoHeard = toBravo.size();

        // A member who is not a moderator is told so; no flag changes and nobody else hears of it.
        WorldPacket fromBravo = MuteRequest("mute12", "Alpha");
        opcodeTable[CMSG_CHANNEL_MUTE].handler(bravoSession, fromBravo);
        CHECK_EQ(fromBravo.rpos(), fromBravo.size());
        REQUIRE(toBravo.size() == bravoHeard + 1);
        CHECK_EQ(Notice(toBravo.back()).opcode, uint16(SMSG_CHANNEL_NOTIFY));
        CHECK_EQ(Notice(toBravo.back()).type, uint8(CHAT_NOT_MODERATOR_NOTICE));
        CHECK_STR(Notice(toBravo.back()).channel, "mute12");
        CHECK_EQ(toAlpha.size(), alphaHeard);

        // The owner's request sets the named member's mute flag, and every member hears the mode change.
        WorldPacket fromAlpha = MuteRequest("mute12", "Bravo");
        opcodeTable[CMSG_CHANNEL_MUTE].handler(alphaSession, fromAlpha);
        CHECK_EQ(fromAlpha.rpos(), fromAlpha.size());
        REQUIRE(toAlpha.size() == alphaHeard + 1);
        REQUIRE(toBravo.size() == bravoHeard + 2);
        Notice const heard(toBravo.back());
        CHECK_EQ(heard.opcode, uint16(SMSG_CHANNEL_NOTIFY));
        CHECK_EQ(heard.type, uint8(CHAT_MODE_CHANGE_NOTICE));
        CHECK_STR(heard.channel, "mute12");
        CHECK(heard.guid == bravo.GetObjectGuid());
        CHECK_EQ(heard.oldFlags & 0x08, 0);
        CHECK_EQ(heard.newFlags & 0x08, 0x08);
        CHECK_EQ(Notice(toAlpha.back()).type, uint8(CHAT_MODE_CHANGE_NOTICE));
        CHECK_EQ(channel->GetNumPlayers(), uint32(2));
    }
    alphaSession.SetSocketlessSink(nullptr, nullptr);
    bravoSession.SetSocketlessSink(nullptr, nullptr);
}

TEST(OpcodeDispatch_ChannelUnmuteRowUnmutesTheMemberTheClientNamesForAModeratorOnly)
{
    InitializeOpcodes();

    std::vector<WorldPacket> toAlpha, toBravo;
    WorldSession alphaSession(1, "alpha", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    WorldSession bravoSession(2, "bravo", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    alphaSession.SetSocketlessSink(&CapturePacket, &toAlpha);
    bravoSession.SetSocketlessSink(&CapturePacket, &toBravo);
    {
        ChannelMember alpha(alphaSession, 7131, "Alpha");
        ChannelMember bravo(bravoSession, 7132, "Bravo");
        Channel* channel = channelMgr(ALLIANCE)->GetJoinChannel("unmute12", 0);
        channel->Join(&alpha, "");
        channel->Join(&bravo, "");
        channel->SetMute(&alpha, "Bravo");
        REQUIRE(channel->GetNumPlayers() == 2);
        size_t const alphaHeard = toAlpha.size();
        size_t const bravoHeard = toBravo.size();

        // A muted member who is not a moderator cannot unmute himself; he is told so and the flag stays.
        WorldPacket fromBravo = UnmuteRequest("unmute12", "Bravo");
        opcodeTable[CMSG_CHANNEL_UNMUTE].handler(bravoSession, fromBravo);
        CHECK_EQ(fromBravo.rpos(), fromBravo.size());
        REQUIRE(toBravo.size() == bravoHeard + 1);
        CHECK_EQ(Notice(toBravo.back()).opcode, uint16(SMSG_CHANNEL_NOTIFY));
        CHECK_EQ(Notice(toBravo.back()).type, uint8(CHAT_NOT_MODERATOR_NOTICE));
        CHECK_STR(Notice(toBravo.back()).channel, "unmute12");
        CHECK_EQ(toAlpha.size(), alphaHeard);

        // The owner's request clears the named member's mute flag, and every member hears the mode change.
        WorldPacket fromAlpha = UnmuteRequest("unmute12", "Bravo");
        opcodeTable[CMSG_CHANNEL_UNMUTE].handler(alphaSession, fromAlpha);
        CHECK_EQ(fromAlpha.rpos(), fromAlpha.size());
        REQUIRE(toAlpha.size() == alphaHeard + 1);
        REQUIRE(toBravo.size() == bravoHeard + 2);
        Notice const heard(toBravo.back());
        CHECK_EQ(heard.opcode, uint16(SMSG_CHANNEL_NOTIFY));
        CHECK_EQ(heard.type, uint8(CHAT_MODE_CHANGE_NOTICE));
        CHECK_STR(heard.channel, "unmute12");
        CHECK(heard.guid == bravo.GetObjectGuid());
        CHECK_EQ(heard.oldFlags & 0x08, 0x08);
        CHECK_EQ(heard.newFlags & 0x08, 0);
        CHECK_EQ(Notice(toAlpha.back()).type, uint8(CHAT_MODE_CHANGE_NOTICE));
        CHECK_EQ(channel->GetNumPlayers(), uint32(2));
    }
    alphaSession.SetSocketlessSink(nullptr, nullptr);
    bravoSession.SetSocketlessSink(nullptr, nullptr);
}

TEST(OpcodeDispatch_ChannelUnbanRowLiftsTheBanOfThePlayerTheClientNamesForAModeratorOnly)
{
    InitializeOpcodes();

    std::vector<WorldPacket> toAlpha, toBravo, toCharlie;
    WorldSession alphaSession(1, "alpha", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    WorldSession bravoSession(2, "bravo", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    WorldSession charlieSession(3, "charlie", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    alphaSession.SetSocketlessSink(&CapturePacket, &toAlpha);
    bravoSession.SetSocketlessSink(&CapturePacket, &toBravo);
    charlieSession.SetSocketlessSink(&CapturePacket, &toCharlie);
    {
        ChannelMember alpha(alphaSession, 7141, "Alpha");
        ChannelMember bravo(bravoSession, 7142, "Bravo");
        ChannelMember charlie(charlieSession, 7143, "Charlie");
        Channel* channel = channelMgr(ALLIANCE)->GetJoinChannel("unban12", 0);
        channel->Join(&alpha, "");
        channel->Join(&bravo, "");
        channel->Join(&charlie, "");
        channel->Ban(&alpha, "Bravo");
        REQUIRE(channel->GetNumPlayers() == 2);

        // The banned player cannot rejoin.
        channel->Join(&bravo, "");
        REQUIRE(!toBravo.empty());
        CHECK_EQ(Notice(toBravo.back()).type, uint8(CHAT_BANNED_NOTICE));
        CHECK_EQ(channel->GetNumPlayers(), uint32(2));
        size_t const alphaHeard = toAlpha.size();
        size_t const bravoHeard = toBravo.size();
        size_t const charlieHeard = toCharlie.size();

        // A member who is not a moderator is told so, and the ban stays.
        WorldPacket fromCharlie = UnbanRequest("unban12", "Bravo");
        opcodeTable[CMSG_CHANNEL_UNBAN].handler(charlieSession, fromCharlie);
        CHECK_EQ(fromCharlie.rpos(), fromCharlie.size());
        REQUIRE(toCharlie.size() == charlieHeard + 1);
        CHECK_EQ(Notice(toCharlie.back()).opcode, uint16(SMSG_CHANNEL_NOTIFY));
        CHECK_EQ(Notice(toCharlie.back()).type, uint8(CHAT_NOT_MODERATOR_NOTICE));
        CHECK_STR(Notice(toCharlie.back()).channel, "unban12");
        CHECK_EQ(toAlpha.size(), alphaHeard);
        channel->Join(&bravo, "");
        REQUIRE(toBravo.size() == bravoHeard + 1);
        CHECK_EQ(Notice(toBravo.back()).type, uint8(CHAT_BANNED_NOTICE));
        CHECK_EQ(channel->GetNumPlayers(), uint32(2));

        // The owner's request lifts the ban: every member hears it, and the player can join again.
        WorldPacket fromAlpha = UnbanRequest("unban12", "Bravo");
        opcodeTable[CMSG_CHANNEL_UNBAN].handler(alphaSession, fromAlpha);
        CHECK_EQ(fromAlpha.rpos(), fromAlpha.size());
        REQUIRE(toAlpha.size() == alphaHeard + 1);
        REQUIRE(toCharlie.size() == charlieHeard + 2);
        Notice const heard(toAlpha.back());
        CHECK_EQ(heard.opcode, uint16(SMSG_CHANNEL_NOTIFY));
        CHECK_EQ(heard.type, uint8(CHAT_PLAYER_UNBANNED_NOTICE));
        CHECK_STR(heard.channel, "unban12");
        CHECK(heard.guid == bravo.GetObjectGuid());
        CHECK_EQ(Notice(toCharlie.back()).type, uint8(CHAT_PLAYER_UNBANNED_NOTICE));
        CHECK_EQ(toBravo.size(), bravoHeard + 1);
        channel->Join(&bravo, "");
        REQUIRE(toBravo.size() > bravoHeard + 1);
        CHECK_EQ(Notice(toBravo[bravoHeard + 1]).type, uint8(CHAT_YOU_JOINED_NOTICE));
        CHECK_EQ(channel->GetNumPlayers(), uint32(3));

        // Unbanning a player who is not banned says so.
        WorldPacket again = UnbanRequest("unban12", "Bravo");
        opcodeTable[CMSG_CHANNEL_UNBAN].handler(alphaSession, again);
        CHECK_EQ(Notice(toAlpha.back()).type, uint8(CHAT_PLAYER_NOT_BANNED_NOTICE));
        CHECK_EQ(channel->GetNumPlayers(), uint32(3));
    }
    alphaSession.SetSocketlessSink(nullptr, nullptr);
    bravoSession.SetSocketlessSink(nullptr, nullptr);
    charlieSession.SetSocketlessSink(nullptr, nullptr);
}

TEST(OpcodeDispatch_ChannelRowsHoldTheirHandlersThunks)
{
    InitializeOpcodes();

    // The list and display-list handlers have identical code, and the Release linker may merge
    // them into one function, so that pair is counted together.
    CHECK(opcodeTable[CMSG_JOIN_CHANNEL].handler == &OpcodeThunk<&ChannelHandlers::HandleJoinChannel>);
    CHECK_EQ(CountSlotsBoundTo(&OpcodeThunk<&ChannelHandlers::HandleJoinChannel>), 1);
    CHECK(opcodeTable[CMSG_LEAVE_CHANNEL].handler == &OpcodeThunk<&ChannelHandlers::HandleLeaveChannel>);
    CHECK_EQ(CountSlotsBoundTo(&OpcodeThunk<&ChannelHandlers::HandleLeaveChannel>), 1);
    CHECK(opcodeTable[CMSG_CHANNEL_LIST].handler == &OpcodeThunk<&ChannelHandlers::HandleChannelList>);
    CHECK(opcodeTable[CMSG_CHANNEL_PASSWORD].handler == &OpcodeThunk<&ChannelHandlers::HandleChannelPassword>);
    CHECK_EQ(CountSlotsBoundTo(&OpcodeThunk<&ChannelHandlers::HandleChannelPassword>), 1);
    CHECK(opcodeTable[CMSG_CHANNEL_SET_OWNER].handler == &OpcodeThunk<&ChannelHandlers::HandleChannelSetOwner>);
    CHECK_EQ(CountSlotsBoundTo(&OpcodeThunk<&ChannelHandlers::HandleChannelSetOwner>), 1);
    CHECK(opcodeTable[CMSG_CHANNEL_OWNER].handler == &OpcodeThunk<&ChannelHandlers::HandleChannelOwner>);
    CHECK_EQ(CountSlotsBoundTo(&OpcodeThunk<&ChannelHandlers::HandleChannelOwner>), 1);
    CHECK(opcodeTable[CMSG_CHANNEL_MODERATOR].handler == &OpcodeThunk<&ChannelHandlers::HandleChannelModerator>);
    CHECK_EQ(CountSlotsBoundTo(&OpcodeThunk<&ChannelHandlers::HandleChannelModerator>), 1);
    CHECK(opcodeTable[CMSG_CHANNEL_UNMODERATOR].handler == &OpcodeThunk<&ChannelHandlers::HandleChannelUnmoderator>);
    CHECK_EQ(CountSlotsBoundTo(&OpcodeThunk<&ChannelHandlers::HandleChannelUnmoderator>), 1);
    CHECK(opcodeTable[CMSG_CHANNEL_MUTE].handler == &OpcodeThunk<&ChannelHandlers::HandleChannelMute>);
    CHECK_EQ(CountSlotsBoundTo(&OpcodeThunk<&ChannelHandlers::HandleChannelMute>), 1);
    CHECK(opcodeTable[CMSG_CHANNEL_UNMUTE].handler == &OpcodeThunk<&ChannelHandlers::HandleChannelUnmute>);
    CHECK_EQ(CountSlotsBoundTo(&OpcodeThunk<&ChannelHandlers::HandleChannelUnmute>), 1);
    CHECK(opcodeTable[CMSG_CHANNEL_INVITE].handler == &OpcodeThunk<&ChannelHandlers::HandleChannelInvite>);
    CHECK_EQ(CountSlotsBoundTo(&OpcodeThunk<&ChannelHandlers::HandleChannelInvite>), 1);
    CHECK(opcodeTable[CMSG_CHANNEL_KICK].handler == &OpcodeThunk<&ChannelHandlers::HandleChannelKick>);
    CHECK_EQ(CountSlotsBoundTo(&OpcodeThunk<&ChannelHandlers::HandleChannelKick>), 1);
    CHECK(opcodeTable[CMSG_CHANNEL_BAN].handler == &OpcodeThunk<&ChannelHandlers::HandleChannelBan>);
    CHECK_EQ(CountSlotsBoundTo(&OpcodeThunk<&ChannelHandlers::HandleChannelBan>), 1);
    CHECK(opcodeTable[CMSG_CHANNEL_UNBAN].handler == &OpcodeThunk<&ChannelHandlers::HandleChannelUnban>);
    CHECK_EQ(CountSlotsBoundTo(&OpcodeThunk<&ChannelHandlers::HandleChannelUnban>), 1);
    CHECK(opcodeTable[CMSG_CHANNEL_ANNOUNCEMENTS].handler == &OpcodeThunk<&ChannelHandlers::HandleChannelAnnouncements>);
    CHECK_EQ(CountSlotsBoundTo(&OpcodeThunk<&ChannelHandlers::HandleChannelAnnouncements>), 1);
    CHECK(opcodeTable[CMSG_CHANNEL_DISPLAY_LIST].handler == &OpcodeThunk<&ChannelHandlers::HandleChannelDisplayListQuery>);
    CHECK_EQ(CountSlotsBoundToEither(&OpcodeThunk<&ChannelHandlers::HandleChannelList>,
                                     &OpcodeThunk<&ChannelHandlers::HandleChannelDisplayListQuery>), 2);
    CHECK(opcodeTable[CMSG_SET_CHANNEL_WATCH].handler == &OpcodeThunk<&ChannelHandlers::HandleSetChannelWatch>);
    CHECK_EQ(CountSlotsBoundTo(&OpcodeThunk<&ChannelHandlers::HandleSetChannelWatch>), 1);
    CHECK(opcodeTable[CMSG_JOIN_CHANNEL].handler != &OpcodeThunk<&WorldSession::Handle_NULL>);
}

TEST(OpcodeDispatch_ChannelRowsKeepTheirStatusAndProcessing)
{
    InitializeOpcodes();

    CHECK_EQ(opcodeTable[CMSG_JOIN_CHANNEL].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_JOIN_CHANNEL].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_LEAVE_CHANNEL].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_LEAVE_CHANNEL].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_CHANNEL_LIST].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_CHANNEL_LIST].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_CHANNEL_PASSWORD].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_CHANNEL_PASSWORD].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_CHANNEL_SET_OWNER].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_CHANNEL_SET_OWNER].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_CHANNEL_OWNER].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_CHANNEL_OWNER].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_CHANNEL_MODERATOR].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_CHANNEL_MODERATOR].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_CHANNEL_UNMODERATOR].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_CHANNEL_UNMODERATOR].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_CHANNEL_MUTE].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_CHANNEL_MUTE].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_CHANNEL_UNMUTE].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_CHANNEL_UNMUTE].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_CHANNEL_INVITE].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_CHANNEL_INVITE].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_CHANNEL_KICK].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_CHANNEL_KICK].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_CHANNEL_BAN].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_CHANNEL_BAN].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_CHANNEL_UNBAN].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_CHANNEL_UNBAN].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_CHANNEL_ANNOUNCEMENTS].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_CHANNEL_ANNOUNCEMENTS].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_CHANNEL_DISPLAY_LIST].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_CHANNEL_DISPLAY_LIST].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_SET_CHANNEL_WATCH].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_SET_CHANNEL_WATCH].packetProcessing, PROCESS_THREADUNSAFE);
}

TEST(OpcodeDispatch_AddFriendRowReachesItsHandlerWithThePacket)
{
    InitializeOpcodes();

    std::vector<WorldPacket> sent;
    WorldSession session(1, "dispatch", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    session.SetSocketlessSink(&CapturePacket, &sent);

    WorldPacket addFriend(CMSG_ADD_FRIEND, 2);
    addFriend << std::string("");
    addFriend << std::string("");

    opcodeTable[CMSG_ADD_FRIEND].handler(session, addFriend);

    CHECK_EQ(addFriend.rpos(), size_t(2));
    CHECK(sent.empty());

    session.SetSocketlessSink(nullptr, nullptr);
}

TEST(OpcodeDispatch_ContactRowsHoldTheirHandlersThunks)
{
    InitializeOpcodes();

    CHECK(opcodeTable[CMSG_CONTACT_LIST].handler == &OpcodeThunk<&ContactHandlers::HandleContactList>);
    CHECK_EQ(CountSlotsBoundTo(&OpcodeThunk<&ContactHandlers::HandleContactList>), 1);
    CHECK(opcodeTable[CMSG_ADD_FRIEND].handler == &OpcodeThunk<&ContactHandlers::HandleAddFriend>);
    CHECK_EQ(CountSlotsBoundTo(&OpcodeThunk<&ContactHandlers::HandleAddFriend>), 1);
    CHECK(opcodeTable[CMSG_DEL_FRIEND].handler == &OpcodeThunk<&ContactHandlers::HandleDelFriend>);
    CHECK_EQ(CountSlotsBoundTo(&OpcodeThunk<&ContactHandlers::HandleDelFriend>), 1);
    CHECK(opcodeTable[CMSG_ADD_IGNORE].handler == &OpcodeThunk<&ContactHandlers::HandleAddIgnore>);
    CHECK_EQ(CountSlotsBoundTo(&OpcodeThunk<&ContactHandlers::HandleAddIgnore>), 1);
    CHECK(opcodeTable[CMSG_DEL_IGNORE].handler == &OpcodeThunk<&ContactHandlers::HandleDelIgnore>);
    CHECK_EQ(CountSlotsBoundTo(&OpcodeThunk<&ContactHandlers::HandleDelIgnore>), 1);
    CHECK(opcodeTable[CMSG_SET_CONTACT_NOTES].handler == &OpcodeThunk<&ContactHandlers::HandleSetContactNotes>);
    CHECK_EQ(CountSlotsBoundTo(&OpcodeThunk<&ContactHandlers::HandleSetContactNotes>), 1);
}

TEST(OpcodeDispatch_ContactRowsKeepTheirStatusAndProcessing)
{
    InitializeOpcodes();

    CHECK_EQ(opcodeTable[CMSG_CONTACT_LIST].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_CONTACT_LIST].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_ADD_FRIEND].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_ADD_FRIEND].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_DEL_FRIEND].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_DEL_FRIEND].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_ADD_IGNORE].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_ADD_IGNORE].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_DEL_IGNORE].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_DEL_IGNORE].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_SET_CONTACT_NOTES].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_SET_CONTACT_NOTES].packetProcessing, PROCESS_THREADUNSAFE);
}

TEST(OpcodeDispatch_GMTicketSystemStatusRowAnswersWithTheTicketSystemsState)
{
    InitializeOpcodes();

    std::vector<WorldPacket> sent;
    WorldSession session(1, "dispatch", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    session.SetSocketlessSink(&CapturePacket, &sent);

    WorldPacket status(CMSG_GMTICKET_SYSTEMSTATUS, 0);

    opcodeTable[CMSG_GMTICKET_SYSTEMSTATUS].handler(session, status);

    CHECK_EQ(status.rpos(), size_t(0));
    REQUIRE(sent.size() == 1);
    CHECK_EQ(sent[0].GetOpcode(), uint16(SMSG_GMTICKET_SYSTEMSTATUS));
    REQUIRE(sent[0].size() == 4);
    uint32 accepting = 0;
    sent[0] >> accepting;
    CHECK_EQ(accepting, sTicketMgr.WillAcceptTickets() ? uint32(1) : uint32(0));

    session.SetSocketlessSink(nullptr, nullptr);
}

TEST(OpcodeDispatch_TicketRowsHoldTheirHandlersThunks)
{
    InitializeOpcodes();

    CHECK(opcodeTable[CMSG_GMTICKET_GETTICKET].handler == &OpcodeThunk<&TicketHandlers::HandleGMTicketGetTicket>);
    CHECK_EQ(CountSlotsBoundTo(&OpcodeThunk<&TicketHandlers::HandleGMTicketGetTicket>), 1);
    CHECK(opcodeTable[CMSG_GMTICKET_UPDATETEXT].handler == &OpcodeThunk<&TicketHandlers::HandleGMTicketUpdateText>);
    CHECK_EQ(CountSlotsBoundTo(&OpcodeThunk<&TicketHandlers::HandleGMTicketUpdateText>), 1);
    CHECK(opcodeTable[CMSG_GMTICKET_DELETETICKET].handler == &OpcodeThunk<&TicketHandlers::HandleGMTicketDeleteTicket>);
    CHECK_EQ(CountSlotsBoundTo(&OpcodeThunk<&TicketHandlers::HandleGMTicketDeleteTicket>), 1);
    CHECK(opcodeTable[CMSG_GMTICKET_CREATE].handler == &OpcodeThunk<&TicketHandlers::HandleGMTicketCreate>);
    CHECK_EQ(CountSlotsBoundTo(&OpcodeThunk<&TicketHandlers::HandleGMTicketCreate>), 1);
    CHECK(opcodeTable[CMSG_GMTICKET_SYSTEMSTATUS].handler == &OpcodeThunk<&TicketHandlers::HandleGMTicketSystemStatus>);
    CHECK_EQ(CountSlotsBoundTo(&OpcodeThunk<&TicketHandlers::HandleGMTicketSystemStatus>), 1);
    CHECK(opcodeTable[CMSG_GMSURVEY_SUBMIT].handler == &OpcodeThunk<&TicketHandlers::HandleGMTicketSurveySubmit>);
    CHECK_EQ(CountSlotsBoundTo(&OpcodeThunk<&TicketHandlers::HandleGMTicketSurveySubmit>), 1);
    CHECK(opcodeTable[CMSG_GMTICKET_RESOLVE_RESPONSE].handler == &OpcodeThunk<&TicketHandlers::HandleGMResponseResolve>);
    CHECK_EQ(CountSlotsBoundTo(&OpcodeThunk<&TicketHandlers::HandleGMResponseResolve>), 1);
    CHECK(opcodeTable[CMSG_GMTICKET_GETTICKET].handler != &OpcodeThunk<&WorldSession::Handle_NULL>);
}

TEST(OpcodeDispatch_TicketRowsKeepTheirStatusAndProcessing)
{
    InitializeOpcodes();

    CHECK_EQ(opcodeTable[CMSG_GMTICKET_GETTICKET].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_GMTICKET_GETTICKET].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_GMTICKET_UPDATETEXT].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_GMTICKET_UPDATETEXT].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_GMTICKET_DELETETICKET].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_GMTICKET_DELETETICKET].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_GMTICKET_CREATE].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_GMTICKET_CREATE].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_GMTICKET_SYSTEMSTATUS].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_GMTICKET_SYSTEMSTATUS].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_GMSURVEY_SUBMIT].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_GMSURVEY_SUBMIT].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_GMTICKET_RESOLVE_RESPONSE].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_GMTICKET_RESOLVE_RESPONSE].packetProcessing, PROCESS_THREADUNSAFE);
}

TEST(OpcodeDispatch_OfferPetitionRowReturnsBeforeAnyPlayerRead)
{
    InitializeOpcodes();

    std::vector<WorldPacket> sent;
    WorldSession session(1, "dispatch", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    session.SetSocketlessSink(&CapturePacket, &sent);

    WorldPacket offer(CMSG_OFFER_PETITION, 20);
    offer << uint32(0);
    offer << uint64(0);
    offer << uint64(0);

    opcodeTable[CMSG_OFFER_PETITION].handler(session, offer);

    CHECK_EQ(offer.rpos(), size_t(20));
    CHECK(sent.empty());

    session.SetSocketlessSink(nullptr, nullptr);
}

TEST(OpcodeDispatch_PetitionRowsHoldTheirHandlersThunks)
{
    InitializeOpcodes();

    CHECK(opcodeTable[CMSG_PETITION_BUY].handler == &OpcodeThunk<&PetitionHandlers::HandlePetitionBuy>);
    CHECK_EQ(CountSlotsBoundTo(&OpcodeThunk<&PetitionHandlers::HandlePetitionBuy>), 1);
    CHECK(opcodeTable[CMSG_PETITION_SHOW_SIGNATURES].handler == &OpcodeThunk<&PetitionHandlers::HandlePetitionShowSign>);
    CHECK_EQ(CountSlotsBoundTo(&OpcodeThunk<&PetitionHandlers::HandlePetitionShowSign>), 1);
    CHECK(opcodeTable[CMSG_PETITION_QUERY].handler == &OpcodeThunk<&PetitionHandlers::HandlePetitionQuery>);
    CHECK_EQ(CountSlotsBoundTo(&OpcodeThunk<&PetitionHandlers::HandlePetitionQuery>), 1);
    CHECK(opcodeTable[MSG_PETITION_RENAME].handler == &OpcodeThunk<&PetitionHandlers::HandlePetitionRename>);
    CHECK_EQ(CountSlotsBoundTo(&OpcodeThunk<&PetitionHandlers::HandlePetitionRename>), 1);
    CHECK(opcodeTable[CMSG_PETITION_SIGN].handler == &OpcodeThunk<&PetitionHandlers::HandlePetitionSign>);
    CHECK_EQ(CountSlotsBoundTo(&OpcodeThunk<&PetitionHandlers::HandlePetitionSign>), 1);
    CHECK(opcodeTable[MSG_PETITION_DECLINE].handler == &OpcodeThunk<&PetitionHandlers::HandlePetitionDecline>);
    CHECK_EQ(CountSlotsBoundTo(&OpcodeThunk<&PetitionHandlers::HandlePetitionDecline>), 1);
    CHECK(opcodeTable[CMSG_OFFER_PETITION].handler == &OpcodeThunk<&PetitionHandlers::HandleOfferPetition>);
    CHECK_EQ(CountSlotsBoundTo(&OpcodeThunk<&PetitionHandlers::HandleOfferPetition>), 1);
    CHECK(opcodeTable[CMSG_TURN_IN_PETITION].handler == &OpcodeThunk<&PetitionHandlers::HandleTurnInPetition>);
    CHECK_EQ(CountSlotsBoundTo(&OpcodeThunk<&PetitionHandlers::HandleTurnInPetition>), 1);
    CHECK(opcodeTable[CMSG_PETITION_SHOWLIST].handler == &OpcodeThunk<&PetitionHandlers::HandlePetitionShowList>);
    CHECK_EQ(CountSlotsBoundTo(&OpcodeThunk<&PetitionHandlers::HandlePetitionShowList>), 1);
    CHECK(opcodeTable[CMSG_PETITION_SHOW_SIGNATURES].handler != &OpcodeThunk<&WorldSession::Handle_NULL>);
}

TEST(OpcodeDispatch_PetitionRowsKeepTheirStatusAndProcessing)
{
    InitializeOpcodes();

    CHECK_EQ(opcodeTable[CMSG_PETITION_BUY].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_PETITION_BUY].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_PETITION_SHOW_SIGNATURES].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_PETITION_SHOW_SIGNATURES].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_PETITION_QUERY].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_PETITION_QUERY].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[MSG_PETITION_RENAME].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[MSG_PETITION_RENAME].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_PETITION_SIGN].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_PETITION_SIGN].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[MSG_PETITION_DECLINE].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[MSG_PETITION_DECLINE].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_OFFER_PETITION].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_OFFER_PETITION].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_TURN_IN_PETITION].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_TURN_IN_PETITION].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_PETITION_SHOWLIST].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_PETITION_SHOWLIST].packetProcessing, PROCESS_THREADUNSAFE);
}

TEST(OpcodeDispatch_VoiceChatRowsReadTheirPacketsAndSendNothing)
{
    InitializeOpcodes();

    std::vector<WorldPacket> sent;
    WorldSession session(1, "dispatch", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    session.SetSocketlessSink(&CapturePacket, &sent);

    WorldPacket enable(CMSG_VOICE_SESSION_ENABLE, 2);
    enable << uint8(1);
    enable << uint8(0);
    opcodeTable[CMSG_VOICE_SESSION_ENABLE].handler(session, enable);
    CHECK_EQ(enable.rpos(), size_t(2));

    WorldPacket voiceOn(CMSG_CHANNEL_VOICE_ON, 1);
    voiceOn << uint8(0);
    opcodeTable[CMSG_CHANNEL_VOICE_ON].handler(session, voiceOn);
    CHECK_EQ(voiceOn.rpos(), size_t(1));

    WorldPacket active(CMSG_SET_ACTIVE_VOICE_CHANNEL, 5);
    active << uint32(0);
    active << uint8(0);
    opcodeTable[CMSG_SET_ACTIVE_VOICE_CHANNEL].handler(session, active);
    CHECK_EQ(active.rpos(), size_t(5));

    CHECK(sent.empty());

    session.SetSocketlessSink(nullptr, nullptr);
}

TEST(OpcodeDispatch_VoiceChatRowsHoldTheirHandlersThunks)
{
    InitializeOpcodes();

    CHECK(opcodeTable[CMSG_VOICE_SESSION_ENABLE].handler == &OpcodeThunk<&VoiceChatHandlers::HandleVoiceSessionEnable>);
    CHECK_EQ(CountSlotsBoundTo(&OpcodeThunk<&VoiceChatHandlers::HandleVoiceSessionEnable>), 1);
    CHECK(opcodeTable[CMSG_CHANNEL_VOICE_ON].handler == &OpcodeThunk<&VoiceChatHandlers::HandleChannelVoiceOn>);
    CHECK_EQ(CountSlotsBoundTo(&OpcodeThunk<&VoiceChatHandlers::HandleChannelVoiceOn>), 1);
    CHECK(opcodeTable[CMSG_SET_ACTIVE_VOICE_CHANNEL].handler ==
          &OpcodeThunk<&VoiceChatHandlers::HandleSetActiveVoiceChannel>);
    CHECK_EQ(CountSlotsBoundTo(&OpcodeThunk<&VoiceChatHandlers::HandleSetActiveVoiceChannel>), 1);
    CHECK(opcodeTable[CMSG_VOICE_SESSION_ENABLE].handler != &OpcodeThunk<&WorldSession::Handle_NULL>);
}

TEST(OpcodeDispatch_VoiceChatRowsKeepTheirStatusAndProcessing)
{
    InitializeOpcodes();

    CHECK_EQ(opcodeTable[CMSG_VOICE_SESSION_ENABLE].status, STATUS_AUTHED);
    CHECK_EQ(opcodeTable[CMSG_VOICE_SESSION_ENABLE].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_CHANNEL_VOICE_ON].status, STATUS_LOGGEDIN);
    CHECK_EQ(opcodeTable[CMSG_CHANNEL_VOICE_ON].packetProcessing, PROCESS_THREADUNSAFE);
    CHECK_EQ(opcodeTable[CMSG_SET_ACTIVE_VOICE_CHANNEL].status, STATUS_AUTHED);
    CHECK_EQ(opcodeTable[CMSG_SET_ACTIVE_VOICE_CHANNEL].packetProcessing, PROCESS_THREADUNSAFE);
}

TEST(OpcodeDispatch_WardenRowDrainsItsPacketAndSendsNothing)
{
    InitializeOpcodes();

    std::vector<WorldPacket> sent;
    WorldSession session(1, "dispatch", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    session.SetSocketlessSink(&CapturePacket, &sent);

    WorldPacket warden(CMSG_WARDEN_DATA, 9);
    warden << uint8(2);
    warden << uint32(0x01020304);
    warden << uint32(0x05060708);
    opcodeTable[CMSG_WARDEN_DATA].handler(session, warden);
    CHECK_EQ(warden.rpos(), size_t(9));
    CHECK(sent.empty());

    session.SetSocketlessSink(nullptr, nullptr);
}

TEST(OpcodeDispatch_WardenRowHoldsItsHandlersThunk)
{
    InitializeOpcodes();

    CHECK(opcodeTable[CMSG_WARDEN_DATA].handler == &OpcodeThunk<&WardenHandlers::HandleWardenData>);
    CHECK_EQ(CountSlotsBoundTo(&OpcodeThunk<&WardenHandlers::HandleWardenData>), 1);
    CHECK_EQ(opcodeTable[CMSG_WARDEN_DATA].status, STATUS_AUTHED);
    CHECK_EQ(opcodeTable[CMSG_WARDEN_DATA].packetProcessing, PROCESS_THREADUNSAFE);
}
