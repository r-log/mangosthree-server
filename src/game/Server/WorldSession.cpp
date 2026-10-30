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

/**
 * @file WorldSession.cpp
 * @brief World session implementation
 *
 * This file implements WorldSession which manages a player's connection
 * to the world server. It handles:
 *
 * - Packet processing and opcode dispatch
 * - Player authentication and login
 * - Character management
 * - Movement and action handling
 * - Chat and social interactions
 *
 * Which of the two drains runs a packet's handler: see \ref PacketProcessing in OpcodeTable.h.
 *
 * @see WorldSession for the session class
 * @see proto::ClientConnection for the network connection
 * @see OpcodeTable.cpp for opcode registration
 */

#include <zlib.h>
#include "IClientLink.h"
#include "Common/ServerDefines.h"
#include "Platform/Define.h"
#include "Common/Locales.h"
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <set>
#include <memory>
#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "OpcodeTable.h"
#include "WorldPacket.h"
#include "movement/WireParity.h"
#include "wire/MovementCapture.h"
#include "wire/MovementFamilies.h"
#include "wire/MovementSequences.h"
#include "WorldSession.h"
#include "Player.h"
#include "ObjectMgr.h"
#include "DBCStores.h"
#include "ObjectLookup.h"
#include "GameTime.h"
#include "Group.h"
#include "LFGMgr.h"
#include "CinematicFlyover.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "data/GameConfig.h"
#include "BattleGround/BattleGroundMgr.h"
#include "MapManager.h"
#include "SocialMgr.h"
#include "Auth/AuthCrypt.h"
#include "SessionLinks.h"
#include "WorldNetwork.h"
#include "zlib.h"
#include "MotionMaster.h"
#include "State.h"
#include "persistence/characters/CharacterRepository.h"
#include "persistence/characters/account/AccountDataRepository.h"
#include "persistence/login/AccountRepository.h"

#include <chrono>
#include <mutex>
#include <utility>
#include <cstdarg>

namespace
{
    /// Redirects to send before giving up on a session. A client that has
    /// refused this many is not going to accept the next one either -- its
    /// modulus is not ours -- and leaving it sitting at the loading screen tells
    /// its owner nothing.
    const uint32 MAX_SECOND_STREAM_ATTEMPTS = 3;

    WorldSession::AckTotalsCounters s_ackTotals;
}

WorldSession::AckTotalsCounters const& WorldSession::AckTotals()
{
    return s_ackTotals;
}

Unit* WorldSession::MemberUnit(ObjectGuid guid)
{
    if (!_player)
    {
        return NULL;
    }
    // The player resolves to itself whether or not it is in the world: a far
    // transfer keeps its membership, and its in-flight packets (processed
    // PROCESS_THREADSAFE even while !IsInWorld()) take this path instead of
    // counting as a wrong guid or an unresolved member. Only another member's
    // lookup needs the map.
    if (guid == _player->GetObjectGuid())
    {
        return _player;
    }
    if (!_player->IsInWorld())
    {
        return NULL;
    }
    return ObjectLookup::GetUnit(*_player, guid);
}

Unit* WorldSession::SelectedMover()
{
    const uint64 selected = m_movers.Selected();
    if (!selected)
    {
        return NULL;
    }
    return MemberUnit(ObjectGuid(selected));
}

void WorldSession::GrantMover(Unit* unit, uint32 now)
{
    unit->MotionState().SetMode(Motion::Mode::ClientDriven, now);
    unit->SetMoverSession(this);
    if (unit == _player)
    {
        m_movers.SetBase(unit->GetObjectGuid().GetRawValue());
    }
    m_movers.Add(unit->GetObjectGuid().GetRawValue());
    // The authority is what the client root reads (Unit::IsClientMover), and it has just
    // changed: Unit::TakePossessOf raises the Possessed inhibition before it grants, so a body
    // stunned before the take was projected as the server-driven unit it then still was and
    // would come out of the take unrooted without this.
    unit->GetMotionMaster()->RefreshClientRoot();
}

void WorldSession::RevokeMover(Unit* unit, uint32 now)
{
    m_movers.Remove(unit->GetObjectGuid().GetRawValue());
    // The mode flip is guarded like the session clear: a unit already handed to
    // another session (or none) is not this session's mode to touch.
    if (unit->MoverSession() == this)
    {
        unit->SetMoverSession(NULL);
        unit->MotionState().SetMode(Motion::Mode::ServerDriven, now);
    }
    // The mirror of the grant's: a body released while still stunned reverts to server-driven,
    // and a stunned server-driven unit is stopped, not rooted. Unconditional, because a revoke
    // that the guard above refused changed nothing for the projection to see.
    unit->GetMotionMaster()->RefreshClientRoot();
}

void WorldSession::RevokeAllMovers(uint32 now)
{
    // The player first and directly: it may be out of the world (a logout during a
    // transfer), when MemberUnit resolves nothing.
    if (_player)
    {
        RevokeMover(_player, now);
    }
    std::vector<uint64> const members = m_movers.Members();
    for (size_t i = 0; i < members.size(); ++i)
    {
        if (Unit* unit = MemberUnit(ObjectGuid(members[i])))
        {
            RevokeMover(unit, now);
        }
    }
    m_movers.Clear();
}

/**
 * @brief Tallies one movement ack outcome, on this session and process-wide.
 *
 * @param field The AckCounters member the kernel's outcome names.
 * @param total The same outcome's AckTotalsCounters member.
 */
void WorldSession::CountAck(uint32 AckCounters::*field, std::atomic<uint32> AckTotalsCounters::*total)
{
    ++(m_ackCounters.*field);
    ++(s_ackTotals.*total);
}

/**
 * @brief Helper for Map session filtering
 * @param session World session
 * @param opHandle Opcode handler
 * @return True if packet can be processed in Map::Update
 *
 * Refuses a thread-unsafe packet, a session with no player and a player not in the world.
 */
static bool MapSessionFilterHelper(WorldSession* session, OpcodeHandler const& opHandle)
{
    // we do not process thread-unsafe packets
    if (opHandle.packetProcessing == PROCESS_THREADUNSAFE)
    {
        return false;
    }

    // we do not process not loggined player packets
    Player* plr = session->GetPlayer();
    if (!plr)
    {
        return false;
    }

    // in Map::Update() we do not process packets where player is not in world!
    return plr->IsInWorld();
}


/**
 * @brief Process packet in Map context
 * @param packet Packet to process
 * @return True if packet should be processed
 *
 * Filters packets for processing in Map::Update context.
 * Takes an in-place packet, and a thread-safe packet while the session's player is in the world.
 */
bool MapSessionFilter::Process(WorldPacket* packet)
{
    OpcodeHandler const& opHandle = opcodeTable[packet->GetOpcode()];
    if (opHandle.packetProcessing == PROCESS_INPLACE)
    {
        return true;
    }

    // let's check if our opcode can be really processed in Map::Update()
    return MapSessionFilterHelper(m_pSession, opHandle);
}

/**
 * @brief Process packet in World context
 * @param packet Packet to process
 * @return True if packet should be processed
 *
 * Filters packets for processing in World::UpdateSessions context.
 * Takes an in-place or thread-unsafe packet, and a thread-safe packet while the session has no player in the world.
 */
bool WorldSessionFilter::Process(WorldPacket* packet)
{
    OpcodeHandler const& opHandle = opcodeTable[packet->GetOpcode()];
    // an in-place packet runs at whichever drain reaches it first
    if (opHandle.packetProcessing == PROCESS_INPLACE)
    {
        return true;
    }

    // let's check if our opcode can't be processed in Map::Update()
    return !MapSessionFilterHelper(m_pSession, opHandle);
}

/// WorldSession constructor
WorldSession::WorldSession(uint32 id, const std::string& accountName,
                           std::shared_ptr<proto::SessionLinks> links,
                           std::shared_ptr<SessionMailbox> mailbox,
                           AccountTypes sec, uint8 expansion, time_t mute_time,
                           LocaleConstant locale, const BigNumber& sessionKey) :
    m_muteTime(mute_time), _player(NULL), m_Socket(std::move(links)),
    m_mailbox(mailbox ? std::move(mailbox) : std::make_shared<SessionMailbox>()),
    m_sessionKey(sessionKey), m_secondStreamAttempts(0),
    m_sessionId(proto::INVALID_SESSION_ID),
    _security(sec), _accountId(id), m_accountName(accountName),
    m_expansion(expansion), _logoutTime(0),
    m_inQueue(false), m_playerLoading(false), m_playerLogout(false), m_playerRecentlyLogout(false), m_playerSave(false),
    m_sessionDbcLocale(sGameConfig.GetAvailableDbcLocale(locale)), m_sessionDbLocaleIndex(sObjectMgr.GetIndexForLocale(locale)),
    m_latency(), m_badPackets(0), m_tutorialState(TUTORIALDATA_UNCHANGED)
{
    if (m_Socket)
    {
        m_Address = m_Socket->GetRemoteAddress();
    }
}

/// WorldSession destructor
WorldSession::~WorldSession()
{
    ///- unload player if not unloaded
    if (_player)
    {
        LogoutPlayer(true);
    }


    // A ticket left in the registry would keep the next client behind the same
    // address waiting for a redirect that is never coming back.
    sWorldNetwork.CancelSecondStream(m_sessionId);

    /// - If the connection is still up, close it. Dropping the shared_ptr is
    /// all the bookkeeping there is now: the transport owns the socket, and
    /// the link stays safe to call even after the peer is gone.
    if (m_Socket)
    {
        m_Socket->Close();
        m_Socket.reset();
    }

    ///- empty incoming packet queue
    m_mailbox->Close();
}

/**
 * @brief Logs an invalid client packet size for the current opcode.
 *
 * @param packet The offending packet.
 * @param size The expected packet size.
 */
void WorldSession::SizeError(WorldPacket const& packet, uint32 size) const
{
    sLog.outError("Client (account %u) send packet %s (%u) with size %zu but expected %u (attempt crash server?), skipped",
                  GetAccountId(), LookupOpcodeName(packet.GetOpcode()), packet.GetOpcode(), packet.size(), size);
}

/// Get the player name
char const* WorldSession::GetPlayerName() const
{
    return GetPlayer() ? GetPlayer()->GetName() : "<none>";
}

/// Send a packet to the client
void WorldSession::SendPacket(WorldPacket const* packet)
{
    if (!m_Socket)
    {
        // No socket: a real session whose link was reset, or the harness's own session. Only
        // the harness sets a sink (SetSocketlessSink), so for every real session this is the
        // plain return it always was.
        if (m_socketlessSink)
        {
            m_socketlessSink(m_socketlessSinkContext, *packet);
        }
        return;
    }

    if (opcodeTable[packet->GetOpcode()].status == STATUS_UNHANDLED)
    {
        sLog.outError("SESSION: tried to send an unhandled opcode 0x%.4X", packet->GetOpcode());
        return;
    }

    const_cast<WorldPacket*>(packet)->FlushBits();

#ifdef MANGOS_DEBUG

    // Code for network use statistic
    static uint64 sendPacketCount = 0;
    static uint64 sendPacketBytes = 0;

    static time_t firstTime = time(NULL);
    static time_t lastTime = firstTime;                     // next 60 secs start time

    static uint64 sendLastPacketCount = 0;
    static uint64 sendLastPacketBytes = 0;

    time_t cur_time = time(NULL);

    if ((cur_time - lastTime) < 60)
    {
        sendPacketCount += 1;
        sendPacketBytes += packet->size();

        sendLastPacketCount += 1;
        sendLastPacketBytes += packet->size();
    }
    else
    {
        uint64 minTime = uint64(cur_time - lastTime);
        uint64 fullTime = uint64(lastTime - firstTime);
        DETAIL_LOG("Send all time packets count: " UI64FMTD " bytes: " UI64FMTD " avr.count/sec: %f avr.bytes/sec: %f time: %u", sendPacketCount, sendPacketBytes, float(sendPacketCount) / fullTime, float(sendPacketBytes) / fullTime, uint32(fullTime));
        DETAIL_LOG("Send last min packets count: " UI64FMTD " bytes: " UI64FMTD " avr.count/sec: %f avr.bytes/sec: %f", sendLastPacketCount, sendLastPacketBytes, float(sendLastPacketCount) / minTime, float(sendLastPacketBytes) / minTime);

        lastTime = cur_time;
        sendLastPacketCount = 1;
        sendLastPacketBytes = packet->wpos();               // wpos is real written size
    }

#endif                                                  // !MANGOS_DEBUG

    // The packet dump is ClientConnection's: it traces both directions, and also what never
    // reaches this function, the auth handshake.

    // SendPacket is void and safe to call on a dead link: there is no failure to react to here.

    // The other direction of the same capture: what this server puts on the wire
    // for the opcodes the wire layer knows -- a registry layout or a hand-written
    // family -- so a replay can judge the legacy writers by the same
    // codecs the client reads with.
    if (Wire::MovementCapture::IsOpen() && Wire::IsKnown(packet->GetOpcode()))
    {
        Wire::MovementCapture::Record('S', packet->GetOpcode(), packet->contents(), packet->size());
    }

    if (WireParity::Enabled())
    {
        WireParity::Outbound(packet->GetOpcode(), *packet);
    }

    m_Socket->SendPacket(*packet);
}

/// Add an incoming packet to the queue
void WorldSession::QueuePacket(WorldPacket* new_packet)
{
    m_mailbox->Enqueue(std::unique_ptr<WorldPacket>(new_packet));
}

/// Logging helper for unexpected opcodes
void WorldSession::LogUnexpectedOpcode(WorldPacket* packet, const char* reason)
{
    sLog.outError("SESSION: received unexpected opcode %s (0x%.4X) %s",
                  LookupOpcodeName(packet->GetOpcode()),
                  packet->GetOpcode(),
                  reason);
}

/// Logging helper for unexpected opcodes
void WorldSession::LogUnprocessedTail(WorldPacket* packet)
{
    sLog.outError("SESSION: opcode %s (0x%.4X) have unprocessed tail data (read stop at %zu from %zu)",
                  LookupOpcodeName(packet->GetOpcode()),
                  packet->GetOpcode(),
                  packet->rpos(), packet->wpos());
}

/// Update the WorldSession (triggered by World update)
bool WorldSession::Update(PacketFilter& updater)
{
    ///- Retrieve packets from the receive queue and call the appropriate handlers
    /// not process packets if socket already closed
    WorldPacket* packet = NULL;
    while (m_Socket && !m_Socket->IsClosed() && m_mailbox->Next(packet, updater))
    {
        /*#if 1
        sLog.outError( "MOEP: %s (0x%.4X)",
                        LookupOpcodeName(packet->GetOpcode()),
                        packet->GetOpcode());
        #endif*/

        // Entering the world is the first thing that needs the client's second
        // stream, and it is also what opens it. So the login does not run when
        // it arrives -- it is held, the redirect goes out, and it runs once the
        // client is back on both connections. Everything it replies with, from
        // SMSG_LOGIN_VERIFY_WORLD onward, then has somewhere to go.
        // Still queued means not entitled to enter the world yet, so the login
        // falls through to the gate below that says so. Redirecting first would
        // hand a queued client a second stream and hold a redirect slot for it.
        if (packet->GetOpcode() == CMSG_PLAYER_LOGIN && !m_inQueue && !HasSecondStream())
        {
            BeginSecondStream(std::unique_ptr<WorldPacket>(packet));
            continue;
        }

        OpcodeHandler const& opHandle = opcodeTable[packet->GetOpcode()];
        try
        {
            switch (opHandle.status)
            {
                case STATUS_LOGGEDIN:
                    if (!_player)
                    {
                        // skip STATUS_LOGGEDIN opcode unexpected errors if player logout sometime ago - this can be network lag delayed packets
                        if (!m_playerRecentlyLogout)
                        {
                            LogUnexpectedOpcode(packet, "the player has not logged in yet");
                        }
                    }
                    else if (_player->IsInWorld())
                    {
                        ExecuteOpcode(opHandle, packet);
                    }

                    // lag can cause STATUS_LOGGEDIN opcodes to arrive after the player started a transfer
                    break;
                case STATUS_LOGGEDIN_OR_RECENTLY_LOGGEDOUT:
                    if (!_player && !m_playerRecentlyLogout)
                    {
                        LogUnexpectedOpcode(packet, "the player has not logged in yet and not recently logout");
                    }
                    else
                        // not expected _player or must checked in packet hanlder
                    {
                        ExecuteOpcode(opHandle, packet);
                    }
                    break;
                case STATUS_TRANSFER:
                    if (!_player)
                    {
                        LogUnexpectedOpcode(packet, "the player has not logged in yet");
                    }
                    else if (_player->IsInWorld())
                    {
                        LogUnexpectedOpcode(packet, "the player is still in world");
                    }
                    else
                    {
                        ExecuteOpcode(opHandle, packet);
                    }
                    break;
                case STATUS_AUTHED:
                    // Prevent skipping the queue -- but a queued client's own heartbeats
                    // are STATUS_AUTHED too, and dropping them times it out.
                    if (m_inQueue &&
                        packet->GetOpcode() != CMSG_PING &&
                        packet->GetOpcode() != CMSG_KEEP_ALIVE)
                    {
                        LogUnexpectedOpcode(packet, "the player not pass queue yet");
                        break;
                    }

                    // A heartbeat is not activity, so it must not close the
                    // recently-logged-out window. Same opcodes as the gate above by
                    // coincidence, not by rule.
                    if (packet->GetOpcode() != CMSG_SET_ACTIVE_VOICE_CHANNEL &&
                        packet->GetOpcode() != CMSG_PING &&
                        packet->GetOpcode() != CMSG_KEEP_ALIVE)
                    {
                        m_playerRecentlyLogout = false;
                    }

                    ExecuteOpcode(opHandle, packet);
                    break;
                case STATUS_NEVER:
                    sLog.outError("SESSION: received not allowed opcode %s (0x%.4X)",
                                  LookupOpcodeName(packet->GetOpcode()),
                                  packet->GetOpcode());
                    break;
                case STATUS_UNHANDLED:
                    DEBUG_LOG("SESSION: received not handled opcode %s (0x%.4X)",
                              LookupOpcodeName(packet->GetOpcode()),
                              packet->GetOpcode());
                    break;
                default:
                    sLog.outError("SESSION: received wrong-status-req opcode %s (0x%.4X)",
                                  LookupOpcodeName(packet->GetOpcode()),
                                  packet->GetOpcode());
                    break;
            }
        }
        catch (ByteBufferException&)
        {
            sLog.outError("WorldSession::Update ByteBufferException occured while parsing a packet (opcode: %u) from client %s, accountid=%i.",
                          packet->GetOpcode(), GetRemoteAddress().c_str(), GetAccountId());
            ++m_badPackets;
            if (sLog.HasLogLevelOrHigher(LOG_LVL_DEBUG))
            {
                DEBUG_LOG("Dumping error causing packet:");
                packet->hexlike();
            }

            if (sGameConfig.getConfig(CONFIG_BOOL_KICK_PLAYER_ON_BAD_PACKET))
            {
                DETAIL_LOG("Disconnecting session [account id %u / address %s] for badly formatted packet.",
                           GetAccountId(), GetRemoteAddress().c_str());

                KickPlayer();
            }
        }

        delete packet;
    }

    UpdateSecondStream();

    ///- Cleanup socket pointer if need
    if (m_Socket && m_Socket->IsClosed())
    {
        m_Socket.reset();
    }

    // check if we are safe to proceed with logout
    // logout procedure should happen only in World::UpdateSessions() method!!!
    if (updater.ProcessLogout())
    {
        ///- If necessary, log the player out
        time_t currTime = time(NULL);
        if (!m_Socket || (ShouldLogOut(currTime) && !m_playerLoading))
        {
            LogoutPlayer(true);
        }
        if (!m_Socket)
        {
            return false;                                    // Will remove this session from the world session map
        }
    }

    return true;
}

/// %Log the player out
void WorldSession::LogoutPlayer(bool Save)
{
    // finish pending transfers before starting the logout
    while (_player && _player->IsBeingTeleportedFar())
    {
        HandleMoveWorldportAckOpcode();
    }

    m_playerLogout = true;
    m_playerSave = Save;

    if (_player)
    {
        // Stop cinematic flyover if present; DK may hold an early
        // visibility lease before the flyover becomes active.
        if (CinematicFlyover* flyover = _player->GetCinematicFlyover())
        {
            flyover->Stop();
        }

        sLog.outChar("Account: %d (IP: %s) Logout Character:[%s] (guid: %u)", GetAccountId(), GetRemoteAddress().c_str(), _player->GetName() , _player->GetGUIDLow());

        if (ObjectGuid lootGuid = GetPlayer()->GetLootGuid())
        {
            DoLootRelease(lootGuid);
        }

        ///- If the player just died before logging out, make him appear as a ghost
        // FIXME: logout must be delayed in case lost connection with client in time of combat
        if (_player->GetDeathTimer())
        {
            _player->GetHostileRefManager().deleteReferences();
            _player->BuildPlayerRepop();
            _player->RepopAtGraveyard();
        }
        else if (!_player->getAttackers().empty())
        {
            // Build the set of player (or player-pet) attackers first; the
            // kill-on-logout cascade below only runs when real players are
            // involved (PvP-style honor death). PvE-only attackers — training
            // dummies, neutral mobs that briefly grabbed aggro, anything whose
            // owner isn't a player — get the cheap CombatStop and then continue
            // into the normal save-and-logout path. Without this guard, just
            // attacking a PACIFIED training dummy and typing /logout would
            // KillPlayer, save the character to DB with 0 HP, and force a
            // graveyard revive on the next login.
            std::set<Player*> aset;
            for (Unit::AttackerSet::const_iterator itr = _player->getAttackers().begin(); itr != _player->getAttackers().end(); ++itr)
            {
                Unit* owner = (*itr)->GetOwner();           // including player controlled case
                if (owner)
                {
                    if (Player* ownerPlayer = owner->ToPlayer())
                    {
                        aset.insert(ownerPlayer);
                    }
                }
                else if ((*itr)->GetTypeId() == TYPEID_PLAYER)
                {
                    aset.insert((Player*)(*itr));
                }
            }

            _player->CombatStop();
            _player->GetHostileRefManager().setOnlineOfflineState(false);

            if (!aset.empty())
            {
                _player->RemoveAllAurasOnDeath();
                _player->SetPvPDeath(true);
                _player->KillPlayer();
                _player->BuildPlayerRepop();
                _player->RepopAtGraveyard();

                // give honor to all attackers from set like group case
                for (std::set<Player*>::const_iterator itr = aset.begin(); itr != aset.end(); ++itr)
                {
                    (*itr)->RewardHonor(_player, aset.size());
                }

                // give bg rewards and update counters like kill by first from attackers
                // this can't be called for all attackers.
                if (BattleGround* bg = _player->GetBattleGround())
                {
                    bg->HandleKillPlayer(_player, *aset.begin());
                }
            }
        }
        else if (_player->HasAuraType(SPELL_AURA_SPIRIT_OF_REDEMPTION))
        {
            // this will kill character by SPELL_AURA_SPIRIT_OF_REDEMPTION
            _player->RemoveSpellsCausingAura(SPELL_AURA_MOD_SHAPESHIFT);
            //_player->SetDeathPvP(*); set at SPELL_AURA_SPIRIT_OF_REDEMPTION apply time
            _player->KillPlayer();
            _player->BuildPlayerRepop();
            _player->RepopAtGraveyard();
        }
        // drop a flag if player is carrying it
        if (BattleGround* bg = _player->GetBattleGround())
        {
            bg->EventPlayerLoggedOut(_player);
        }

        ///- Teleport to home if the player is in an invalid instance
        if (!_player->m_InstanceValid && !_player->isGameMaster())
        {
            _player->TeleportToHomebind();
            // this is a bad place to call for far teleport because we need player to be in world for successful logout
            // maybe we should implement delayed far teleport logout?
        }

        // FG: finish pending transfers after starting the logout
        // this should fix players beeing able to logout and login back with full hp at death position
        while (_player->IsBeingTeleportedFar())
        {
            HandleMoveWorldportAckOpcode();
        }

        for (int i = 0; i < PLAYER_MAX_BATTLEGROUND_QUEUES; ++i)
        {
            if (BattleGroundQueueTypeId bgQueueTypeId = _player->GetBattleGroundQueueTypeId(i))
            {
                _player->RemoveBattleGroundQueueId(bgQueueTypeId);
                sBattleGroundMgr.m_BattleGroundQueues[ bgQueueTypeId ].RemovePlayer(_player->GetObjectGuid(), true);
            }
        }

        ///- Reset the online field in the account table
        // no point resetting online in character table here as Player::SaveToDB() will set it to 1 since player has not been removed from world at this stage
        AccountRepository::SetActiveRealm(0, GetAccountId());

        ///- If the player is in a guild, update the guild roster and broadcast a logout message to other guild members
        if (Guild* guild = sGuildMgr.GetGuildById(_player->GetGuildId()))
        {
            if (MemberSlot* slot = guild->GetMemberSlot(_player->GetObjectGuid()))
            {
                slot->SetMemberStats(_player);
                slot->UpdateLogoutTime();
            }

            guild->BroadcastEvent(GE_SIGNED_OFF, _player->GetObjectGuid(), _player->GetName());
        }

        ///- Remove pet
        _player->RemovePet(PET_SAVE_AS_CURRENT);

        ///- empty buyback items and save the player in the database
        // some save parts only correctly work in case player present in map/player_lists (pets, etc)
        if (Save)
        {
            _player->SaveToDB();
        }

        ///- Leave all channels before player delete...
        _player->CleanupChannels();

        ///- If the player is in a group (or invited), remove him. If the group if then only 1 person, disband the group.
        bool const retainLfgGroup = sLFGMgr.OnPlayerLogout(_player);
        _player->UninviteFromGroup();

        // remove player from the group if he is:
        // a) in group; b) not in raid group; c) logging out normally (not being kicked or disconnected)
        // LFD groups keep their offline members (retail behavior).
        if (!retainLfgGroup && _player->GetGroup() &&
            !_player->GetGroup()->isRaidGroup() && m_Socket)
        {
            _player->RemoveFromGroup();
        }

        ///- Send update to group
        if (_player->GetGroup())
        {
            // a world marker belongs to the raid, not to whoever placed it, so
            // hand its dynamic object to someone still online rather than
            // letting it die with this player
            _player->GetGroup()->ReanchorMarkersFrom(_player->GetObjectGuid());
            _player->GetGroup()->SendUpdate();
        }

        ///- Broadcast a logout message to the player's friends
        sSocialMgr.SendFriendStatus(_player, FRIEND_OFFLINE, _player->GetObjectGuid(), true);
        sSocialMgr.RemovePlayerSocial(_player->GetGUIDLow());

        ///- Every unit this client moved is the server's again; no packet,
        ///  the session is ending.
        RevokeAllMovers(GameTime::GetGameTimeMS());

        ///- Remove the player from the world
        // the player may not be in the world when logging out
        // e.g if he got disconnected during a transfer to another map
        // calls to GetMap in this case may cause crashes
        if (_player->IsInWorld())
        {
            Map* _map = _player->GetMap();
            _map->Remove(_player, true);
        }
        else
        {
            _player->CleanupsBeforeDelete();
            Map::DeleteFromWorld(_player);
        }

        SetPlayer(NULL);                                    // deleted in Remove/DeleteFromWorld call
        // The second stream belongs to the character, not the account: nothing to
        // recover until the next login opens one.
        m_secondStreamWanted = false;
        m_secondStreamRecovering = false;

        ///- Send the 'logout complete' packet to the client
        WorldPacket data(SMSG_LOGOUT_COMPLETE, 0);
        SendPacket(&data);

        ///- Since each account can only have one online character at any given time, ensure all characters for active account are marked as offline
        CharacterRepository::SetAccountOffline(GetAccountId());

        DEBUG_LOG("SESSION: Sent SMSG_LOGOUT_COMPLETE Message");
    }

    m_playerLogout = false;
    m_playerSave = false;
    m_playerRecentlyLogout = true;
    LogoutRequest(0);
}

/// Kick a player out of the World
void WorldSession::KickPlayer()
{
    if (m_Socket)
    {
        m_Socket->Close();
    }
}

/**
 * @brief Starts the logout flow and validates whether logout is allowed.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleLogoutRequestOpcode(WorldPacket & /*recv_data*/)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_LOGOUT_REQUEST, security %u", GetSecurity());

    if (ObjectGuid lootGuid = GetPlayer()->GetLootGuid())
    {
        DoLootRelease(lootGuid);
    }

    bool instantLogout = (GetPlayer()->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_RESTING) || GetPlayer()->IsTaxiFlying() || GetSecurity() >= (AccountTypes)sGameConfig.getConfig(CONFIG_UINT32_INSTANT_LOGOUT));

    bool canLogoutInCombat = GetPlayer()->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_RESTING);

    uint8 reason = 0;
    if (GetPlayer()->IsInCombat() && !canLogoutInCombat)
    {
        reason = 1;
    }
    else if (GetPlayer()->ClientStatus().HasMovementFlag(MovementFlags(MOVEFLAG_FALLING | MOVEFLAG_FALLINGFAR)))
    {
        reason = 3;                                         // is jumping or falling
    }
    else if (GetPlayer()->duel || GetPlayer()->HasAura(9454)) // is dueling or frozen by GM via freeze command
    {
        reason = 2;                                         // FIXME - Need the correct value
    }

    WorldPacket data(SMSG_LOGOUT_RESPONSE, 1 + 4);
    data << uint32(reason);
    data << uint8(instantLogout);
    SendPacket(&data);

    if (reason)
    {
        LogoutRequest(time(0));
        return;
    }

    // instant logout in taverns/cities or on taxi or for admins, gm's, mod's if its enabled in mangosd.conf
    if (instantLogout)
    {
        LogoutPlayer(true);
        return;
    }

    // not set flags if player can't free move to prevent lost state at logout cancel
    if (GetPlayer()->CanFreeMove())
    {
        float height = GetPlayer()->GetMap()->GetHeight(GetPlayer()->GetPhaseMask(), GetPlayer()->Where().X(), GetPlayer()->Where().Y(), GetPlayer()->Where().Z());
        if ((GetPlayer()->Where().Z() < height + 0.1f) && !(GetPlayer()->IsInWater()))
        {
            GetPlayer()->SetStandState(UNIT_STAND_STATE_SIT);
        }

        GetPlayer()->SetRoot(true);
        GetPlayer()->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_STUNNED);
    }

    LogoutRequest(time(NULL));
}

/**
 * @brief Cancels a pending logout request.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleLogoutCancelOpcode(WorldPacket & /*recv_data*/)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_LOGOUT_CANCEL Message");

    LogoutRequest(0);

    WorldPacket data(SMSG_LOGOUT_CANCEL_ACK, 0);
    SendPacket(&data);

    // not remove flags if can't free move - its not set in Logout request code.
    if (GetPlayer()->CanFreeMove())
    {
        //!we can move again
        GetPlayer()->SetRoot(false);

        //! Stand Up
        GetPlayer()->SetStandState(UNIT_STAND_STATE_STAND);

        //! DISABLE_ROTATE
        GetPlayer()->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_STUNNED);
    }

    DEBUG_LOG("WORLD: sent SMSG_LOGOUT_CANCEL_ACK Message");
}

/// Ping and keep-alive are ordinary opcodes handled on the world thread, so latency, the
/// overspeed policy and an account's security level stay on the game side of the seam.
void WorldSession::HandlePingOpcode(WorldPacket& recvPacket)
{
    // 4.3.4 sends the latency FIRST and the sequence second.
    //
    // On the wire, one client sitting still (world-packets.log, two sessions):
    //
    //     CMSG_PING  00 00 00 00 | 01 00 00 00
    //     CMSG_PING  00 00 00 00 | 02 00 00 00
    //     CMSG_PING  00 00 00 00 | 03 00 00 00
    //
    // The second dword counts; the first never moves. The counter is the
    // sequence the client waits to see echoed, and the other field is the
    // latency it reports. Read the other way round, the sequence lands in the
    // latency and the latency is echoed as the sequence.
    //
    // Nothing in the packet distinguishes the two fields by shape: both are
    // uint32, and a wrong guess is only visible as a number that never rises.
    uint32 latency = 0;
    uint32 ping = 0;
    recvPacket >> latency;
    recvPacket >> ping;

    uint32 fastPingRun =
        m_pingTracker.Record(SessionPingTracker::Clock::now());
    uint32 maximum =
        sGameConfig.getConfig(CONFIG_UINT32_MAX_OVERSPEED_PINGS);
    if (m_pingTracker.ShouldKick(maximum, GetSecurity() == SEC_PLAYER))
    {
        sLog.outError(
            "WorldSession::HandlePingOpcode: account %u kicked for "
            "overspeeded pings (%u in a row), address = %s",
            GetAccountId(), fastPingRun, GetRemoteAddress().c_str());
        KickPlayer();
        return;
    }

    // Against the wire it came in on: the client measures the two streams
    // independently and shows both.
    const proto::LinkSlot stream =
        recvPacket.HasStream() ? recvPacket.GetStream() : proto::LinkSlot::Zero;

    SetLatency(stream, latency);

    // Echo the sequence, not the latency: the client matches a pong to the
    // ping it answers by this value alone, and drops one it cannot place.
    WorldPacket response(SMSG_PONG, 4);

    // Back down the wire it came up. The client pings both streams, counts
    // each one separately, and only matches a pong against the counter of the
    // connection it arrives on -- so answering everything on stream 0 left the
    // World readout of GetNetStats() at zero however good the link was.
    response.SetStream(stream);

    response << ping;
    SendPacket(&response);
}

void WorldSession::HandleKeepAliveOpcode(WorldPacket& recvPacket)
{
    DEBUG_LOG("CMSG_KEEP_ALIVE ,size: %zu ", recvPacket.size());
}

void WorldSession::HandleTimeSyncResp(WorldPacket& recv_data)
{
    uint32 counter, clientTicks;
    recv_data >> counter >> clientTicks;
    // The session clock learns the client's tick from the pair; a reply to a
    // counter never sent, or too old to trust, is counted and changes nothing.
    Motion::SampleResult const result = m_timeBase.Responded(counter, clientTicks, GameTime::GetGameTimeMS());
    DEBUG_LOG("WORLD: CMSG_TIME_SYNC_RESP counter %u, client ticks %u: %s, delta %u, rtt %u",
              counter, clientTicks, Motion::SampleResultName(result), m_timeBase.Delta(), m_timeBase.LastRtt());
}

void WorldSession::SendAreaTriggerMessage(const char* Text, ...)
{
    va_list ap;
    char szStr [1024];
    szStr[0] = '\0';

    va_start(ap, Text);
    vsnprintf(szStr, 1024, Text, ap);
    va_end(ap);

    uint32 length = strlen(szStr) + 1;
    WorldPacket data(SMSG_AREA_TRIGGER_MESSAGE, 4 + length);
    data << length;
    data << szStr;
    SendPacket(&data);
}

/**
 * @brief Sends a formatted notification message to the client.
 *
 * @param format The printf-style message format.
 */
void WorldSession::SendNotification(const char* format, ...)
{
    if (format)
    {
        va_list ap;
        char szStr [1024];
        szStr[0] = '\0';
        va_start(ap, format);
        vsnprintf(szStr, 1024, format, ap);
        va_end(ap);

        WorldPacket data(SMSG_NOTIFICATION, (strlen(szStr) + 1));
        data.WriteBits(strlen(szStr), 13);
        data.FlushBits();
        data.append(szStr, strlen(szStr));
        SendPacket(&data);
    }
}

/**
 * @brief Sends a localized formatted notification message to the client.
 *
 * @param string_id The localization string identifier.
 */
void WorldSession::SendNotification(int32 string_id, ...)
{
    char const* format = GetMangosString(string_id);
    if (format)
    {
        va_list ap;
        char szStr [1024];
        szStr[0] = '\0';
        va_start(ap, string_id);
        vsnprintf(szStr, 1024, format, ap);
        va_end(ap);

        WorldPacket data(SMSG_NOTIFICATION, (strlen(szStr) + 1));
        data.WriteBits(strlen(szStr), 13);
        data.FlushBits();
        data.append(szStr, strlen(szStr));
        SendPacket(&data);
    }
}

void WorldSession::SendSetPhaseShift(uint32 phaseMask, uint32 mapId)
{
    ObjectGuid guid = _player->GetObjectGuid();

    uint32 phaseFlags = 0;

    for (uint32 i = 0; i < sPhaseStore.GetNumRows(); i++)
    {
        if (PhaseEntry const* phase = sPhaseStore.LookupEntry(i))
        {
            if (phase->PhaseShift == phaseMask)
            {
                phaseFlags = phase->Flags;
                break;
            }
        }
    }

    WorldPacket data(SMSG_SET_PHASE_SHIFT, 30);
    data.WriteGuidMask<2, 3, 1, 6, 4, 5, 0, 7>(guid);
    data.WriteGuidBytes<7, 4>(guid);

    data << uint32(0);                  // number of WorldMapArea.dbc entries to control world map shift * 2

    data.WriteGuidBytes<1>(guid);
    data << uint32(phaseMask ? phaseFlags : 8);
    data.WriteGuidBytes<2, 6>(guid);

    data << uint32(0);                  // number of inactive terrain swaps * 2

    data << uint32(phaseMask ? 2 : 0);  // WRONG: number of Phase.dbc ids * 2
    if (phaseMask)
    {
        data << uint16(phaseMask);
    }

    data.WriteGuidBytes<3, 0>(guid);

    data << uint32(mapId ? 2 : 0);      // number of terrains swaps * 2
    if (mapId)
    {
        data << uint16(mapId);
    }

    data.WriteGuidBytes<5>(guid);
    SendPacket(&data);
}

/*
void WorldSession::SendSetPhaseShift(std::set<uint32> const& phaseIds, std::set<uint32> const& terrainswaps)
{
    if (PlayerLoading())
    {
        return;
    }

    ObjectGuid guid = _player->GetObjectGuid();

    WorldPacket data(SMSG_SET_PHASE_SHIFT, 1 + 8 + 4 + 4 + 4 + 4 + 2 * phaseIds.size() + 4 + terrainswaps.size() * 2);
    data.WriteGuidMask<2, 3, 1, 6, 4, 5, 0, 7>(guid);
    data.WriteGuidBytes<7, 4>(guid);

    data << uint32(0);
    //for (uint8 i = 0; i < worldMapAreaCount; ++i)
    //    data << uint16(0);                    // WorldMapArea.dbc id (controls map display)

    data.WriteGuidBytes<1>(guid);
    data << uint32(phaseIds.size() ? 0 : 8);  // flags (not phasemask)
    data.WriteGuidBytes<2, 6>(guid);

    data << uint32(0);                          // Inactive terrain swaps
    //for (uint8 i = 0; i < inactiveSwapsCount; ++i)
    //    data << uint16(0);

    data << uint32(phaseIds.size() * 2);        // Phase.dbc ids
    for (std::set<uint32>::const_iterator itr = phaseIds.begin(); itr != phaseIds.end(); ++itr)
    {
        data << uint16(*itr);
    }

    data.WriteGuidBytes<3, 0>(guid);

    data << uint32(terrainswaps.size() * 2);    // Active terrain swaps
    for (std::set<uint32>::const_iterator itr = terrainswaps.begin(); itr != terrainswaps.end(); ++itr)
    {
        data << uint16(*itr);
    }

    data.WriteGuidBytes<5>(guid);
    SendPacket(&data);
}
*/

/**
 * @brief Resolves a localized MaNGOS string for this session locale.
 *
 * @param entry The localization entry id.
 * @return const char* The localized string text.
 */
const char* WorldSession::GetMangosString(int32 entry) const
{
    return sObjectMgr.GetMangosString(entry, GetSessionDbLocaleIndex());
}

/**
 * @brief Logs receipt of an unimplemented opcode handler.
 *
 * @param recvPacket The received opcode packet.
 */
void WorldSession::Handle_NULL(WorldPacket& recvPacket)
{
    DEBUG_LOG("SESSION: received unimplemented opcode %s (0x%.4X)",
              LookupOpcodeName(recvPacket.GetOpcode()),
              recvPacket.GetOpcode());
}

/**
 * @brief Logs receipt of an opcode that should be handled earlier in socket processing.
 *
 * @param recvPacket The received opcode packet.
 */
void WorldSession::Handle_EarlyProccess(WorldPacket& recvPacket)
{
    sLog.outError("SESSION: received opcode %s (0x%.4X) that must be processed by proto::ClientConnection",
                  LookupOpcodeName(recvPacket.GetOpcode()),
                  recvPacket.GetOpcode());
}

/**
 * @brief Logs receipt of an opcode reserved for server-side use.
 *
 * @param recvPacket The received opcode packet.
 */
void WorldSession::Handle_ServerSide(WorldPacket& recvPacket)
{
    sLog.outError("SESSION: received server-side opcode %s (0x%.4X)",
                  LookupOpcodeName(recvPacket.GetOpcode()),
                  recvPacket.GetOpcode());
}

/**
 * @brief Logs receipt of a deprecated client opcode.
 *
 * @param recvPacket The received opcode packet.
 */
void WorldSession::Handle_Deprecated(WorldPacket& recvPacket)
{
    sLog.outError("SESSION: received deprecated opcode %s (0x%.4X)",
                  LookupOpcodeName(recvPacket.GetOpcode()),
                  recvPacket.GetOpcode());
}

/**
 * @brief Sends the authentication response or queue position to the client.
 *
 * @param position The queue position, or zero when login may proceed immediately.
 */
void WorldSession::SendAuthWaitQue(uint32 position)
{
    if (position == 0)
    {
        WorldPacket packet( SMSG_AUTH_RESPONSE, 2 );
        packet.WriteBit(false);
        packet.WriteBit(false);
        packet << uint8(AUTH_OK);
        SendPacket(&packet);
    }
    else
    {
        WorldPacket packet(SMSG_AUTH_RESPONSE, 1 + 4 + 1);
        packet.WriteBit(true);      // has queue
        packet.WriteBit(false);     // unk queue-related
        packet.WriteBit(false);     // has account info
        packet << uint8(AUTH_WAIT_QUEUE);
        packet << uint32(position);
        SendPacket(&packet);
    }
}

void WorldSession::LoadGlobalAccountData()
{
    LoadAccountData(
        AccountDataRepository::QueryGlobal(GetAccountId()),
        GLOBAL_CACHE_MASK
    );
}

void WorldSession::LoadAccountData(std::vector<AccountDataRow> const& rows, uint32 mask)
{
    for (uint32 i = 0; i < NUM_ACCOUNT_DATA_TYPES; ++i)
        if (mask & (1 << i))
        {
            m_accountData[i] = AccountData();
        }

    for (AccountDataRow const& row : rows)
    {
        uint32 type = row.type;
        if (type >= NUM_ACCOUNT_DATA_TYPES)
        {
            sLog.outError("Table `%s` have invalid account data type (%u), ignore.",
                          mask == GLOBAL_CACHE_MASK ? "account_data" : "character_account_data", type);
            continue;
        }

        if ((mask & (1 << type)) == 0)
        {
            sLog.outError("Table `%s` have non appropriate for table  account data type (%u), ignore.",
                          mask == GLOBAL_CACHE_MASK ? "account_data" : "character_account_data", type);
            continue;
        }

        m_accountData[type].Time = time_t(row.time);
        m_accountData[type].Data = row.data;
    }
}

void WorldSession::SetAccountData(AccountDataType type, time_t time_, const std::string& data)
{
    if ((1 << type) & GLOBAL_CACHE_MASK)
    {
        AccountDataRepository::ReplaceGlobal(GetAccountId(), uint32(type), uint64(time_), data);
    }
    else
    {
        // _player can be NULL and packet received after logout but m_GUID still store correct guid
        if (!m_GUIDLow)
        {
            return;
        }

        AccountDataRepository::ReplaceCharacter(m_GUIDLow, uint32(type), uint64(time_), data);
    }

    m_accountData[type].Time = time_;
    m_accountData[type].Data = data;
}

void WorldSession::SendAccountDataTimes(uint32 mask)
{
    WorldPacket data(SMSG_ACCOUNT_DATA_TIMES, 4 + 1 + 4 + 8 * 4); // changed in WotLK
    data << uint32(time(NULL));                             // unix time of something
    data << uint8(1);
    data << uint32(mask);                                   // type mask
    for (uint32 i = 0; i < NUM_ACCOUNT_DATA_TYPES; ++i)
    {
        if (mask & (1 << i))
        {
            data << uint32(GetAccountData(AccountDataType(i))->Time);// also unix time
        }
    }
    SendPacket(&data);
}

/**
 * @brief Sends the character's raid-frame (CompactUnitFrame) profiles.
 *
 * The 4.3.4 client will not fire COMPACT_UNIT_FRAME_PROFILES_LOADED until this
 * arrives, and Blizzard_CompactUnitFrameProfiles.lua activates no profile until
 * it does -- which leaves CompactRaidFrameManager's container disabled and the
 * raid frames hidden. An empty list is valid: the client then builds its own
 * default profile.
 */
void WorldSession::SendLoadCUFProfiles()
{
    Player* player = GetPlayer();
    if (!player)
    {
        return;
    }

    std::vector<CUFProfile const*> profiles;
    for (uint8 i = 0; i < MAX_CUF_PROFILES; ++i)
    {
        if (CUFProfile const* profile = player->GetCUFProfile(i))
        {
            profiles.push_back(profile);
        }
    }

    WorldPacket data(SMSG_LOAD_CUF_PROFILES, 3 + profiles.size() * 32);

    data.WriteBits(profiles.size(), 20);

    // bit order below is the client's, not a sensible one -- do not "tidy" it
    for (CUFProfile const* profile : profiles)
    {
        data.WriteBit(profile->BoolOptions[CUF_UNK_157]);
        data.WriteBit(profile->BoolOptions[CUF_AUTO_ACTIVATE_10_PLAYERS]);
        data.WriteBit(profile->BoolOptions[CUF_AUTO_ACTIVATE_5_PLAYERS]);
        data.WriteBit(profile->BoolOptions[CUF_AUTO_ACTIVATE_25_PLAYERS]);
        data.WriteBit(profile->BoolOptions[CUF_DISPLAY_HEAL_PREDICTION]);
        data.WriteBit(profile->BoolOptions[CUF_AUTO_ACTIVATE_PVE]);
        data.WriteBit(profile->BoolOptions[CUF_DISPLAY_HORIZONTAL_GROUPS]);
        data.WriteBit(profile->BoolOptions[CUF_AUTO_ACTIVATE_40_PLAYERS]);
        data.WriteBit(profile->BoolOptions[CUF_AUTO_ACTIVATE_3_PLAYERS]);
        data.WriteBit(profile->BoolOptions[CUF_DISPLAY_AGGRO_HIGHLIGHT]);
        data.WriteBit(profile->BoolOptions[CUF_DISPLAY_BORDER]);
        data.WriteBit(profile->BoolOptions[CUF_AUTO_ACTIVATE_2_PLAYERS]);
        data.WriteBit(profile->BoolOptions[CUF_DISPLAY_NON_BOSS_DEBUFFS]);
        data.WriteBit(profile->BoolOptions[CUF_DISPLAY_MAIN_TANK_AND_ASSIST]);
        data.WriteBit(profile->BoolOptions[CUF_UNK_156]);
        data.WriteBit(profile->BoolOptions[CUF_AUTO_ACTIVATE_SPEC_2]);
        data.WriteBit(profile->BoolOptions[CUF_USE_CLASS_COLORS]);
        data.WriteBit(profile->BoolOptions[CUF_DISPLAY_POWER_BAR]);
        data.WriteBit(profile->BoolOptions[CUF_AUTO_ACTIVATE_SPEC_1]);
        data.WriteBits(profile->ProfileName.size(), 8);
        data.WriteBit(profile->BoolOptions[CUF_DISPLAY_ONLY_DISPELLABLE_DEBUFFS]);
        data.WriteBit(profile->BoolOptions[CUF_KEEP_GROUPS_TOGETHER]);
        data.WriteBit(profile->BoolOptions[CUF_UNK_145]);
        data.WriteBit(profile->BoolOptions[CUF_AUTO_ACTIVATE_15_PLAYERS]);
        data.WriteBit(profile->BoolOptions[CUF_DISPLAY_PETS]);
        data.WriteBit(profile->BoolOptions[CUF_AUTO_ACTIVATE_PVP]);
    }

    data.FlushBits();

    for (CUFProfile const* profile : profiles)
    {
        data << uint16(profile->LeftOffset);
        data << uint16(profile->FrameHeight);
        data << uint16(profile->BottomOffset);
        data << uint8(profile->BottomPoint);
        data << uint16(profile->TopOffset);
        data << uint8(profile->TopPoint);
        data << uint8(profile->HealthText);
        data << uint8(profile->SortBy);
        data << uint16(profile->FrameWidth);
        data << uint8(profile->LeftPoint);
        data.append(profile->ProfileName.c_str(), profile->ProfileName.size());
    }

    SendPacket(&data);
}

/**
 * @brief Loads tutorial flag state for the current account.
 */
void WorldSession::LoadTutorialsData()
{
    for (int aX = 0 ; aX < 8 ; ++aX)
    {
        m_Tutorials[ aX ] = 0;
    }

    std::optional<TutorialRow> tutorials = AccountDataRepository::QueryTutorials(GetAccountId());

    if (!tutorials)
    {
        m_tutorialState = TUTORIALDATA_NEW;
        return;
    }

    for (int iI = 0; iI < 8; ++iI)
    {
        m_Tutorials[iI] = tutorials->flags[iI];
    }

    m_tutorialState = TUTORIALDATA_UNCHANGED;
}

/**
 * @brief Sends the current tutorial flags to the client.
 */
void WorldSession::SendTutorialsData()
{
    WorldPacket data(SMSG_TUTORIAL_FLAGS, 4 * 8);
    for (uint32 i = 0; i < 8; ++i)
    {
        data << m_Tutorials[i];
    }
    SendPacket(&data);
}

/**
 * @brief Persists tutorial flag state changes for the current account.
 */
void WorldSession::SaveTutorialsData()
{
    switch (m_tutorialState)
    {
        case TUTORIALDATA_CHANGED:
            AccountDataRepository::UpdateTutorials(GetAccountId(), m_Tutorials);
            break;

        case TUTORIALDATA_NEW:
            AccountDataRepository::InsertTutorials(GetAccountId(), m_Tutorials);
            break;
        case TUTORIALDATA_UNCHANGED:
            break;
    }

    m_tutorialState = TUTORIALDATA_UNCHANGED;
}

// Send chat information about aborted transfer (mostly used by Player::SendTransferAbortedByLockstatus())
void WorldSession::SendTransferAborted(uint32 mapid, uint8 reason, uint8 arg)
{
    WorldPacket data(SMSG_TRANSFER_ABORTED, 4 + 2);
    data << uint32(mapid);
    data << uint8(reason);                                  // transfer abort reason
    switch (reason)
    {
        case TRANSFER_ABORT_INSUF_EXPAN_LVL:
        case TRANSFER_ABORT_DIFFICULTY:
        case TRANSFER_ABORT_UNIQUE_MESSAGE:
            data << uint8(arg);
            break;
    }
    SendPacket(&data);
}

void WorldSession::ReadAddonsInfo(ByteBuffer &data)
{
    if (data.rpos() + 4 > data.size())
    {
        return;
    }
    uint32 size;
    data >> size;

    if (!size)
    {
        return;
    }

    if (size > 0xFFFFF)
    {
        sLog.outError("WorldSession::ReadAddonsInfo addon info too big, size %u", size);
        return;
    }

    uLongf uSize = size;

    uint32 pos = data.rpos();

    ByteBuffer addonInfo;
    addonInfo.resize(size);

    if (uncompress(const_cast<uint8*>(addonInfo.contents()), &uSize, const_cast<uint8*>(data.contents() + pos), data.size() - pos) == Z_OK)
    {
        uint32 addonsCount;
        addonInfo >> addonsCount;                         // addons count
        DEBUG_LOG("Addon count: %u", addonsCount);

        for (uint32 i = 0; i < addonsCount; ++i)
        {
            std::string addonName;
            uint8 enabled;
            uint32 crc, unk1;

            // check next addon data format correctness
            if (addonInfo.rpos() + 1 > addonInfo.size())
            {
                return;
            }

            addonInfo >> addonName;

            addonInfo >> enabled >> crc >> unk1;

            DEBUG_LOG("ADDON: Name: %s, Enabled: 0x%x, CRC: 0x%x, Unknown2: 0x%x", addonName.c_str(), enabled, crc, unk1);

            m_addonsList.push_back(AddonInfo(addonName, enabled, crc));
        }

        uint32 unk2;
        addonInfo >> unk2;

        if (addonInfo.rpos() != addonInfo.size())
        {
            DEBUG_LOG("packet under read!");
        }
    }
    else
    {
        sLog.outError("Addon packet uncompress error!");
    }
}

void WorldSession::SendAddonsInfo()
{
    unsigned char tdata[256] =
    {
        0xC3, 0x5B, 0x50, 0x84, 0xB9, 0x3E, 0x32, 0x42, 0x8C, 0xD0, 0xC7, 0x48, 0xFA, 0x0E, 0x5D, 0x54,
        0x5A, 0xA3, 0x0E, 0x14, 0xBA, 0x9E, 0x0D, 0xB9, 0x5D, 0x8B, 0xEE, 0xB6, 0x84, 0x93, 0x45, 0x75,
        0xFF, 0x31, 0xFE, 0x2F, 0x64, 0x3F, 0x3D, 0x6D, 0x07, 0xD9, 0x44, 0x9B, 0x40, 0x85, 0x59, 0x34,
        0x4E, 0x10, 0xE1, 0xE7, 0x43, 0x69, 0xEF, 0x7C, 0x16, 0xFC, 0xB4, 0xED, 0x1B, 0x95, 0x28, 0xA8,
        0x23, 0x76, 0x51, 0x31, 0x57, 0x30, 0x2B, 0x79, 0x08, 0x50, 0x10, 0x1C, 0x4A, 0x1A, 0x2C, 0xC8,
        0x8B, 0x8F, 0x05, 0x2D, 0x22, 0x3D, 0xDB, 0x5A, 0x24, 0x7A, 0x0F, 0x13, 0x50, 0x37, 0x8F, 0x5A,
        0xCC, 0x9E, 0x04, 0x44, 0x0E, 0x87, 0x01, 0xD4, 0xA3, 0x15, 0x94, 0x16, 0x34, 0xC6, 0xC2, 0xC3,
        0xFB, 0x49, 0xFE, 0xE1, 0xF9, 0xDA, 0x8C, 0x50, 0x3C, 0xBE, 0x2C, 0xBB, 0x57, 0xED, 0x46, 0xB9,
        0xAD, 0x8B, 0xC6, 0xDF, 0x0E, 0xD6, 0x0F, 0xBE, 0x80, 0xB3, 0x8B, 0x1E, 0x77, 0xCF, 0xAD, 0x22,
        0xCF, 0xB7, 0x4B, 0xCF, 0xFB, 0xF0, 0x6B, 0x11, 0x45, 0x2D, 0x7A, 0x81, 0x18, 0xF2, 0x92, 0x7E,
        0x98, 0x56, 0x5D, 0x5E, 0x69, 0x72, 0x0A, 0x0D, 0x03, 0x0A, 0x85, 0xA2, 0x85, 0x9C, 0xCB, 0xFB,
        0x56, 0x6E, 0x8F, 0x44, 0xBB, 0x8F, 0x02, 0x22, 0x68, 0x63, 0x97, 0xBC, 0x85, 0xBA, 0xA8, 0xF7,
        0xB5, 0x40, 0x68, 0x3C, 0x77, 0x86, 0x6F, 0x4B, 0xD7, 0x88, 0xCA, 0x8A, 0xD7, 0xCE, 0x36, 0xF0,
        0x45, 0x6E, 0xD5, 0x64, 0x79, 0x0F, 0x17, 0xFC, 0x64, 0xDD, 0x10, 0x6F, 0xF3, 0xF5, 0xE0, 0xA6,
        0xC3, 0xFB, 0x1B, 0x8C, 0x29, 0xEF, 0x8E, 0xE5, 0x34, 0xCB, 0xD1, 0x2A, 0xCE, 0x79, 0xC3, 0x9A,
        0x0D, 0x36, 0xEA, 0x01, 0xE0, 0xAA, 0x91, 0x20, 0x54, 0xF0, 0x72, 0xD8, 0x1E, 0xC7, 0x89, 0xD2
    };

    WorldPacket data(SMSG_ADDON_INFO, 4);

    for (AddonsList::iterator itr = m_addonsList.begin(); itr != m_addonsList.end(); ++itr)
    {
        uint8 state = 2;                                    // 2 is sent here
        data << uint8(state);

        uint8 unk1 = 1;                                     // 1 is sent here
        data << uint8(unk1);
        if (unk1)
        {
            uint8 unk2 = (itr->CRC != 0x4c1c776d);          // If addon is Standard addon CRC
            data << uint8(unk2);                            // if 1, than add addon public signature
            if (unk2)                                       // if CRC is wrong, add public key (client need it)
            {
                data.append(tdata, sizeof(tdata));
            }

            data << uint32(0);
        }

        uint8 unk3 = 0;                                     // 0 is sent here
        data << uint8(unk3);                                // use <Addon>\<Addon>.url file or not
        if (unk3)
        {
            // String, 256 (null terminated?)
            data << uint8(0);
        }
    }

    m_addonsList.clear();

    uint32 count = 0;
    data << uint32(count);                                  // BannedAddons count
    /*for(uint32 i = 0; i < count; ++i)
    {
        uint32
        string (16 bytes)
        string (16 bytes)
        uint32
        uint32
        uint32
    }*/

    SendPacket(&data);
}

bool WorldSession::HasSecondStream() const
{
    return m_Socket && m_Socket->IsSlotLive(proto::LinkSlot::One);
}

void WorldSession::BeginSecondStream(std::unique_ptr<WorldPacket> deferredLogin)
{
    m_deferredLogin = std::move(deferredLogin);
    m_secondStreamAttempts = 0;
    m_secondStreamDeadline = std::chrono::steady_clock::time_point();

    // Nothing more to do here: UpdateSecondStream drives the request, its
    // retries and its deadline from one place, and it runs on the next tick.
}

void WorldSession::UpdateSecondStream()
{
    if (!m_Socket)
    {
        return;
    }

    const bool live = HasSecondStream();

    if (!m_deferredLogin)
    {
        // Before login nothing is expected on stream 1. After it, losing the
        // stream is a fault to recover from: the player is still in the world on
        // stream 0, and everything routed to slot 1 is queueing behind a socket
        // that is gone. Ask the client to open it again, on the same budget a
        // login gets; a client that will not is kicked with a line in the log,
        // not left with half its traffic silently dropped.
        // Only a session with a player in the world has anything to recover: a
        // login that failed, or a character that logged out to the selection
        // screen, has no stream-1 traffic and must not be kicked for the lack of
        // a socket it does not need.
        if (!m_secondStreamWanted || !GetPlayer() || m_Socket->IsClosed())
        {
            return;
        }
        if (live)
        {
            m_secondStreamRecovering = false;
            return;
        }
        if (!m_secondStreamRecovering)
        {
            sLog.outError("WorldSession: account %u lost its second world stream; "
                          "asking the client to open it again", GetAccountId());
            m_secondStreamRecovering = true;
            m_secondStreamAttempts = 0;
            m_secondStreamDeadline = std::chrono::steady_clock::time_point();
        }
    }
    else if (live)
    {
        // The client came back. Put the login back in the queue rather than running
        // it here: this method is reached from Map::Update as well as from
        // World::UpdateSessions, and CMSG_PLAYER_LOGIN is thread-unsafe. Requeued,
        // it is dispatched in the context the opcode table asks for, and the
        // interception above lets it through now that the second stream is live.
        sWorldNetwork.CancelSecondStream(GetSessionId());
        QueuePacket(m_deferredLogin.release());
        m_secondStreamWanted = true;
        return;
    }

    const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    if (m_secondStreamDeadline != std::chrono::steady_clock::time_point() &&
        now < m_secondStreamDeadline)
    {
        return;
    }

    if (m_secondStreamAttempts >= MAX_SECOND_STREAM_ATTEMPTS)
    {
        sLog.outError("WorldSession: account %u %s its second world stream "
                      "after %u attempts; its client is not running our redirect key",
                      GetAccountId(),
                      m_secondStreamRecovering ? "did not reopen" : "never opened",
                      m_secondStreamAttempts);
        m_deferredLogin.reset();
        KickPlayer();
        return;
    }

    proto::RedirectTicket ticket;
    ticket.clientAddress = GetRemoteAddress();
    ticket.session       = GetSessionId();
    ticket.sessionKey    = m_sessionKey;
    ticket.accountName   = m_accountName;
    ticket.links         = m_Socket;
    ticket.generation    = ++m_secondStreamGeneration;

    // Announce the new generation before anything else: from this point a socket
    // that claimed the previous ticket is refused promotion, whatever it does
    // while the replacement is being withdrawn and reissued below.
    m_Socket->ExpectSlotOne(ticket.generation);

    // A retry has to withdraw the pending ticket first: the registry allows one
    // in flight per client address, and the client refuses a second redirect
    // while one is still staged.
    sWorldNetwork.CancelSecondStream(GetSessionId());

    // The same setting bounds the registry's ticket and this wait, so the two
    // never disagree about how long a redirect is worth waiting for.
    m_secondStreamDeadline = now + sWorldNetwork.GetRedirectTimeout();

    switch (sWorldNetwork.RequestSecondStream(ticket))
    {
        case RedirectRequest::Sent:
            ++m_secondStreamAttempts;
            break;
        case RedirectRequest::Busy:
            // Another client behind the same address is mid-redirect. That is
            // queueing rather than failing; burning the budget on it would kick
            // whoever happens to log in second from a shared address.
            break;
        case RedirectRequest::Failed:
            // No redirect could be produced: a fault of this server, not of the
            // client, and a permanent one. It counts, so it ends in a kick and a
            // log line instead of a player parked at the loading screen forever.
            sLog.outError("WorldSession: could not issue a redirect for account %u "
                          "(attempt %u); check <DataDir>/keys/server.secret",
                          GetAccountId(), m_secondStreamAttempts + 1);
            ++m_secondStreamAttempts;
            break;
    }
}

void WorldSession::HandleConnectToFailed(WorldPacket& /*recvPacket*/)
{
    // The client tells us it could not act on the redirect. Retrying at once is
    // right when the cause was transient and harmless when it was not -- the
    // attempt counter still bounds it.
    sLog.outError("WorldSession: account %u refused the second-stream redirect",
                  GetAccountId());

    m_secondStreamDeadline = std::chrono::steady_clock::time_point();
}

/**
 * @brief Executes a validated opcode handler with delayed-teleport protection.
 *
 * @param opHandle The opcode handler metadata.
 * @param packet The packet to process.
 */
void WorldSession::ExecuteOpcode(OpcodeHandler const& opHandle, WorldPacket* packet)
{
    // Passive capture of every packet the wire layer can decode -- a registry
    // layout or a hand-written family -- at the one place every handler is
    // reached from; the handler-local hook in HandleMovementOpcodes covers what
    // is still unknown to both.
    if (Wire::MovementCapture::IsOpen() && Wire::IsKnown(packet->GetOpcode()))
    {
        Wire::MovementCapture::Record('C', packet->GetOpcode(), packet->contents(), packet->size());
    }

    // need prevent do internal far teleports in handlers because some handlers do lot steps
    // or call code that can do far teleports in some conditions unexpectedly for generic way work code
    if (_player)
    {
        _player->SetCanDelayTeleport(true);
    }

    opHandle.handler(*this, *packet);

    if (_player)
    {
        // can be not set in fact for login opcode, but this not create porblems.
        _player->SetCanDelayTeleport(false);

        // we should execute delayed teleports only for alive(!) players
        // because we don't want player's ghost teleported from graveyard
        if (_player->IsHasDelayedTeleport())
        {
            _player->TeleportTo(_player->m_teleport_dest, _player->m_teleport_options);
        }
    }

    if (packet->rpos() < packet->wpos() && sLog.HasLogLevelOrHigher(LOG_LVL_DEBUG))
    {
        LogUnprocessedTail(packet);
    }
}
