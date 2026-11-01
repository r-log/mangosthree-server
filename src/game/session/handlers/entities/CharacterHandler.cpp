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
 * @file CharacterHandler.cpp
 * @brief The session's login and the re-finding of the session a queued character request came
 * from: the character login, the session check every continuation makes, and the character
 * list's sender.
 */

#include "Database/SqlOperations.h"
#include "Common/ServerDefines.h"
#include "Platform/Define.h"
#include <functional>
#include <string>
#include <memory>
#include "Database/DatabaseEnv.h"
#include "WorldPacket.h"
#include "SharedDefines.h"
#include "WorldSession.h"
#include "Opcodes.h"
#include "Log.h"
#include "data/ServerControl.h"
#include "session/SessionRegistry.h"
#include "data/GameConfig.h"
#include "ObjectMgr.h"
#include "Pet.h"
#include "DBCStores.h"
#include "AchievementMgr.h"
#include "data/CharacterCache.h"
#include "Player.h"
#include "CinematicFlyover.h"
#include "session/packets/PlayerPacketSinks.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "CorpseManager.h"
#include "Group.h"
#include "session/PlayerDump.h"
#include "SocialMgr.h"
#include "Util.h"
#include "data/Language.h"
#include "spells/data/SpellMgr.h"
#include "Calendar.h"
#include "session/packets/social/LFGPackets.h"
#include "GameTime.h"
#include "Timer.h"
#include "MotionMaster.h"
#include "persistence/characters/CharacterDeleteRepository.h"
#include "persistence/characters/CharacterRepository.h"
#include "persistence/characters/account/AccountDataRepository.h"
#include "persistence/characters/pets/PetRepository.h"
#include "persistence/login/AccountRepository.h"
#include "session/packets/social/GuildBankPackets.h"

/**
 * @brief Re-find the session a queued character request came from (decoupling D7d, C1).
 *
 * Identities, never pointers: the account id finds the session and the session id proves it
 * is the same one -- a reconnect between the request and the answer gets a new session id, so
 * the answer is dropped rather than delivered to whoever holds the account now. Character
 * create, delete and customize have no Player at all (the character is not loaded), so unlike
 * D7b's petition continuations there is no second identity to check.
 *
 * @return NULL when the session is gone or has been replaced, in which case the continuation
 *         does nothing -- the same thing that happens today when a client disconnects with a
 *         request in flight.
 */
WorldSession* WorldSession::FindRequesterSession(uint32 accountId, proto::SessionId sessionId)
{
    WorldSession* session = sSessionRegistry.Find(accountId);
    if (!session || session->GetSessionId() != sessionId)
    {
        return NULL;
    }

    return session;
}

/**
 * @brief Re-find the session AND the character a queued request came from (D7b, C1).
 *
 * The session check above plus the guid: the player reached through the session is compared
 * against the one the registry has under that guid, so a character swap on the same session
 * (a logout and a login of a different character between the request and the answer) is
 * caught as well as a reconnect.
 *
 * @param accountId The requesting account.
 * @param sessionId The session the request arrived on.
 * @param playerGuid The character the request was made by.
 * @param session Out: the re-found session, NULL on failure.
 * @param player Out: the re-found player, NULL on failure -- except when the session is live
 *        but holds a different character, where it is that character. Both are written before
 *        the test that rejects them, so neither means anything after a false return, and no
 *        caller reads one. (The file-local helper this replaced left the session set on a
 *        session-id mismatch instead of clearing it; nothing looked, then either.)
 * @return false when either is gone or has been replaced.
 */
bool WorldSession::FindRequesterPlayer(uint32 accountId, proto::SessionId sessionId, ObjectGuid playerGuid,
                                       WorldSession*& session, Player*& player)
{
    session = FindRequesterSession(accountId, sessionId);
    if (!session)
    {
        return false;
    }

    player = session->GetPlayer();
    if (!player || player != sPlayerRegistry.Find(playerGuid))
    {
        return false;
    }

    return true;
}

class LoginQueryHolder : public SqlQueryHolder
{
    private:
        uint32 m_accountId;
        proto::SessionId m_sessionId;
        ObjectGuid m_guid;
    public:
        LoginQueryHolder(uint32 accountId, proto::SessionId sessionId, ObjectGuid guid)
            : m_accountId(accountId), m_sessionId(sessionId), m_guid(guid) { }
        ObjectGuid GetGuid() const { return m_guid; }
        uint32 GetAccountId() const { return m_accountId; }
        proto::SessionId GetSessionId() const { return m_sessionId; }
        bool Initialize();
};

/**
 * @brief Builds the set of delayed login queries required for a character load.
 *
 * @return true if all login queries were queued successfully; otherwise false.
 */
bool LoginQueryHolder::Initialize()
{
    return CharacterRepository::StageLoginReads(this, m_guid.GetCounter(), sGameConfig.getConfig(CONFIG_BOOL_DECLINED_NAMES_USED));
}

// The callbacks find their session again by account id and session id: a session can be gone,
// or replaced by a reconnect, before a query answers.
class CharacterHandler
{
    public:
        void HandlePlayerLoginCallback(SqlQueryHolder* holder, uint32 accountId, proto::SessionId sessionId)
        {
            if (!holder)
            {
                return;
            }

            WorldSession* session = WorldSession::FindRequesterSession(accountId, sessionId);
            if (!session)
            {
                delete holder;
                return;
            }
            session->HandlePlayerLogin((LoginQueryHolder*)holder);
        }
} chrHandler;

/**
 * @brief Builds and sends the character enumeration list for the session account.
 *
 * @param rows The account's characters.
 */
void WorldSession::HandleCharEnum(std::vector<CharacterEnumRow> const& rows)
{
    WorldPacket data(SMSG_CHAR_ENUM, 270);

    ByteBuffer buffer;

    data.WriteBits(0, 23);
    data.WriteBit(1);
    data.WriteBits(rows.size(), 17);

    if (!rows.empty())
    {
        for (CharacterEnumRow const& row : rows)
        {
            sLog.outDetail("Loading char guid %u from account %u.", row.guid, GetAccountId());

            if (!Player::BuildEnumData(row, &data, &buffer))
            {
                sLog.outError("Building enum data for SMSG_CHAR_ENUM has failed, aborting");
                return;
            }
        }

        data.FlushBits();
        data.append(buffer);
    }

    SendPacket(&data);
}

/**
 * @brief Starts the asynchronous player login sequence for a selected character.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandlePlayerLoginOpcode(WorldPacket& recv_data)
{
    if (PlayerLoading() || GetPlayer() != NULL)
    {
        sLog.outError("Player tryes to login again, AccountId = %d", GetAccountId());
        return;
    }

    m_playerLoading = true;

    ObjectGuid playerGuid;

    recv_data.ReadGuidMask<2, 3, 0, 6, 4, 5, 1, 7>(playerGuid);
    recv_data.ReadGuidBytes<2, 7, 0, 3, 5, 6, 1, 4>(playerGuid);

    DEBUG_LOG("WORLD: Received opcode Player Logon Message from %s", playerGuid.GetString().c_str());

    uint32 accountId = GetAccountId();
    proto::SessionId sessionId = GetSessionId();
    LoginQueryHolder* holder = new LoginQueryHolder(accountId, sessionId, playerGuid);
    if (!holder->Initialize())
    {
        delete holder;                                      // delete all unprocessed queries
        m_playerLoading = false;
        return;
    }

    // The holder is the callback's to free, so a refusal is ours: QueueLoginReads() answers false
    // without queueing once the database is shutting down, and nothing would ever run its queries
    // or its callback. Same unwind as the Initialize() failure above -- free it and let the login
    // fail rather than leak it and leave the session stuck in the loading state.
    if (!CharacterRepository::QueueLoginReads(holder, [accountId, sessionId](SqlQueryHolder* h)
                                              {
                                                  chrHandler.HandlePlayerLoginCallback(h, accountId, sessionId);
                                              }))
    {
        delete holder;                                      // delete all unprocessed queries
        m_playerLoading = false;
        return;
    }
}

/**
 * @brief Completes player login after all delayed character queries have loaded.
 *
 * @param holder The populated login query holder.
 */
void WorldSession::HandlePlayerLogin(LoginQueryHolder* holder)
{
    /* Store the player's GUID for later reference */
    ObjectGuid playerGuid = holder->GetGuid();

    /* Create a new instance of the player object */
    Player* pCurrChar = new Player(this);
    InstallPlayerPacketSinks(*pCurrChar);

    /* Initialize a motion generator */
    pCurrChar->GetMotionMaster()->Initialize();

    /* Account ID is validated in LoadFromDB (prevents cheaters logging in to characters not on their account) */
    if (!pCurrChar->LoadFromDB(playerGuid, holder))         /// Could not load character from database, cancel login
    {
        /* Disconnect the game client */
        KickPlayer();

        /* Remove references to avoid dangling pointers */
        delete pCurrChar;
        delete holder;

        /* Checked in WorldSession::Update */
        m_playerLoading = false;

        return;
    }

    /* Validation check completely, assign player to WorldSession::_player for later use */
    SetPlayer(pCurrChar);
    pCurrChar->SendDungeonDifficulty(false);

    WorldPacket data(SMSG_LOGIN_VERIFY_WORLD, 20);
    data << pCurrChar->GetMapId();
    data << pCurrChar->Where().X();
    data << pCurrChar->Where().Y();
    data << pCurrChar->Where().Z();
    data << pCurrChar->Where().Facing();
    SendPacket(&data);

    // load player specific part before send times
    LoadAccountData(AccountDataRepository::ReadHolder(holder, PLAYER_LOGIN_QUERY_LOADACCOUNTDATA), PER_CHARACTER_CACHE_MASK);
    SendAccountDataTimes(PER_CHARACTER_CACHE_MASK);

    data.Initialize(SMSG_FEATURE_SYSTEM_STATUS, 34);        // added in 2.2.0
    data << uint8(2);                                       // status
    data << uint32(1);                                      // Scrolls of Ressurection?
    data << uint32(1);
    data << uint32(2);
    data << uint32(0);
    data.WriteBit(true);
    data.WriteBit(true);
    data.WriteBit(false);
    data.WriteBit(true);
    data.WriteBit(false);
    data.WriteBit(false);                                   // enable(1)/disable(0) voice chat interface in client
    data << uint32(1);
    data << uint32(0);
    data << uint32(10);
    data << uint32(60);
    SendPacket(&data);

    // Dungeon Finder: tell the client up front when the feature is off, so
    // the Dungeon Finder panel shows the disabled state instead of an empty
    // list
    if (!sGameConfig.getConfig(CONFIG_BOOL_LFG_ENABLE))
    {
        data.Initialize(SMSG_LFG_DISABLED, 0);
        LFGPackets::BuildDisabled(data);
        SendPacket(&data);
    }

    // Send MOTD
    {
        data.Initialize(SMSG_MOTD, 50);                     // new in 2.0.1
        data << (uint32)0;

        uint32 linecount = 0;
        /* The MOTD itself */
        std::string str_motd = sServerControl.Motd();
        /* Used for tracking our position within the MOTD while iterating through it */
        std::string::size_type pos = 0, nextpos;

        /* Find the next occurance of @ in the string
         * This is how newlines are represented */
        while ((nextpos = str_motd.find('@', pos)) != std::string::npos)
        {
            /* If these are not equal, it means a '@' was found
             * These are used to represent newlines in the string
             * It is set by the code above here */
            if (nextpos != pos)
            {
                /* Send the player a system message containing the substring from pos to nextpos - pos */
                data << str_motd.substr(pos, nextpos - pos);
                ++linecount;
            }
            pos = nextpos + 1;
        }
        /* There are no more newlines in our MOTD, so we send whatever is left */
        if (pos < str_motd.length())
        {
            data << str_motd.substr(pos);
            ++linecount;
        }

        data.put(0, linecount);

        SendPacket(&data);
        DEBUG_LOG("WORLD: Sent motd (SMSG_MOTD)");
    }

    std::optional<LoginGuildRow> guildRow = CharacterRepository::LoginGuild(holder);

    if (guildRow)
    {
        /* We're in a guild, so set the player's guild data to represent that */
        pCurrChar->SetInGuild(guildRow->guildId);
        pCurrChar->SetRank(guildRow->rank);
    }
    /* Player thinks they have a guild, but it isn't in the database. Clear that information */
    else if (pCurrChar->GetGuildId())
    {
        pCurrChar->SetInGuild(0);
        pCurrChar->SetGuildLevel(0);
        pCurrChar->SetRank(0);
    }

    /* Player is in a guild
     * TODO: Can we move this code into the block above? Not sure why it's down here */
    if (pCurrChar->GetGuildId() != 0)
    {
        /* Get guild based on what we set the player's guild to above */
        Guild* guild = sGuildMgr.GetGuildById(pCurrChar->GetGuildId());

        /* More checks to see if they're in a guild? I'm sure this is redundant */
        if (guild)
        {
            pCurrChar->SetGuildLevel(guild->GetLevel());
            /* Build MOTD packet and send it to the player */
            data.Initialize(SMSG_GUILD_EVENT, (1 + 1 + guild->GetMOTD().size() + 1));
            data << uint8(GE_MOTD);
            data << uint8(1);
            data << guild->GetMOTD();
            SendPacket(&data);
            DEBUG_LOG("WORLD: Sent guild-motd (SMSG_GUILD_EVENT)");

            SendGuildBankTabsInfo(*this, *guild);
            /* Let everyone in the guild know you've just signed in */
            guild->BroadcastEvent(GE_SIGNED_ON, pCurrChar->GetObjectGuid(), pCurrChar->GetName());
        }
        /* If the player is not in a guild */
        else
        {
            // remove wrong guild data
            sLog.outError("Player %s (GUID: %u) marked as member of nonexistent guild (id: %u), removing guild membership for player.", pCurrChar->GetName(), pCurrChar->GetGUIDLow(), pCurrChar->GetGuildId());
            pCurrChar->SetInGuild(0);
            pCurrChar->SetGuildLevel(0);
        }
    }

    data.Initialize(SMSG_LEARNED_DANCE_MOVES, 4 + 4);
    data << uint64(0);
    SendPacket(&data);

    pCurrChar->SendInitialPacketsBeforeAddToMap();

    // Show cinematic at the first time that player login (TODO: activate world grids first, then cinematic)
    // move this code past the "SendInitialPacketsAfterAddToMap();" line?
    bool isFirstLogin = !pCurrChar->getCinematic();
    uint32 cinematicSequenceId = 0;
    if (isFirstLogin)
    {
        pCurrChar->setCinematic(1);

        // Class cinematic (Death Knight) takes precedence; fall back to the race intro
        if (ChrClassesEntry const* cEntry = sChrClassesStore.LookupEntry(pCurrChar->getClass()))
        {
            if (cEntry->CinematicSequenceID)
            {
                cinematicSequenceId = cEntry->CinematicSequenceID;
            }
            else if (ChrRacesEntry const* rEntry = sChrRacesStore.LookupEntry(pCurrChar->getRace()))
            {
                cinematicSequenceId = rEntry->CinematicSequenceID;
            }
        }

        if (cinematicSequenceId)
        {
            pCurrChar->SendCinematicStart(cinematicSequenceId);
        }
    }

    uint32 miscRequirement = 0;
    AreaLockStatus lockStatus = AREA_LOCKSTATUS_OK;
    if (AreaTrigger const* at = sObjectMgr.GetMapEntranceTrigger(pCurrChar->GetMapId()))
    {
        lockStatus = pCurrChar->GetAreaTriggerLockStatus(at, pCurrChar->GetDifficulty(pCurrChar->GetMap()->IsRaid()), miscRequirement);
    }
    else
    {
        // Some basic checks in case of a map without areatrigger
        MapEntry const* mapEntry = sMapStore.LookupEntry(pCurrChar->GetMapId());
        if (!mapEntry)
        {
            lockStatus = AREA_LOCKSTATUS_UNKNOWN_ERROR;
        }
        else if (pCurrChar->GetSession()->Expansion() < mapEntry->Expansion())
        {
            lockStatus = AREA_LOCKSTATUS_INSUFFICIENT_EXPANSION;
        }
    }

    bool createCinematicFlyover = isFirstLogin && cinematicSequenceId &&
        sGameConfig.getConfig(CONFIG_BOOL_CINEMATIC_FLYOVER_ENABLE);
    bool createEarlyDkCinematicFlyover = createCinematicFlyover &&
        cinematicSequenceId == 165 && pCurrChar->GetMapId() == 609;

    // Death Knight sequence 165 needs the cinematic visibility radius before
    // Map::Add performs the initial player-centered visibility pass. Other
    // flyovers keep the existing post-add arming path.
    if (lockStatus == AREA_LOCKSTATUS_OK && createEarlyDkCinematicFlyover)
    {
        pCurrChar->SetCinematicFlyover(
            std::make_unique<CinematicFlyover>(pCurrChar, cinematicSequenceId));
    }

    /* This code is run if we can not add the player to the map for some reason */
    if (lockStatus != AREA_LOCKSTATUS_OK || !pCurrChar->BoardingMap()->Add(pCurrChar))
    {
        pCurrChar->SetCinematicFlyover(nullptr);
        /* Attempt to find an areatrigger to teleport the player for us */
        AreaTrigger const* at = sObjectMgr.GetGoBackTrigger(pCurrChar->GetMapId());
        if (at)
        {
            lockStatus = pCurrChar->GetAreaTriggerLockStatus(at, pCurrChar->GetDifficulty(pCurrChar->GetMap()->IsRaid()), miscRequirement);
        }

        /* We couldn't find an areatrigger to teleport, so just move the player back to their home bind */
        if (!at || lockStatus != AREA_LOCKSTATUS_OK || !pCurrChar->TeleportTo(at->target_mapId, at->target_X, at->target_Y, at->target_Z, pCurrChar->Where().Facing()))
        {
            pCurrChar->TeleportToHomebind();
        }
    }

    sPlayerRegistry.Add(pCurrChar);
    // DEBUG_LOG("Player %s added to Map.",pCurrChar->GetName());

    pCurrChar->SendInitialPacketsAfterAddToMap();

    /* If it's the player's first login, create the cinematic flyover if enabled */
    /* Note: isFirstLogin was captured before setCinematic(1) mutated the flag */
    /* Non-DK flyovers arm after the player is fully in-world; the DK flyover */
    /* may already exist so its visibility lease affected Map::Add above. */
    /* Begin still waits for the first CMSG_NEXT_CINEMATIC_CAMERA. */
    if (createCinematicFlyover && !pCurrChar->GetCinematicFlyover())
    {
        pCurrChar->SetCinematicFlyover(
            std::make_unique<CinematicFlyover>(pCurrChar, cinematicSequenceId));
    }

    /* Mark player as online in the database */
    CharacterRepository::SetOnline(pCurrChar->GetGUIDLow());
    AccountRepository::SetActiveRealm(realmID, GetAccountId());

    /* Sync player's in-game time with server time */
    pCurrChar->SetInGameTime(GameTime::GetGameTimeMS());

    /* Send logon notification to player's group
     * This is sent after player is added to the world so that player receives it too */
    if (Group* group = pCurrChar->GetGroup())
    {
        group->SendUpdate();
    }

    /* Inform player's friends that player has come online */
    sSocialMgr.SendFriendStatus(pCurrChar, FRIEND_ONLINE, pCurrChar->GetObjectGuid(), true);

    /* Load the player's corpse if it exists, or resurrect the player if not */
    pCurrChar->LoadCorpse();

    /* If the player is dead, we need to set them as a ghost and increase movespeed */
    if (pCurrChar->m_deathState != ALIVE)
    {
        /* If player is a night elf, wisp racial should be applied */
        if (pCurrChar->getRace() == RACE_NIGHTELF)
        {
            pCurrChar->CastSpell(pCurrChar, 20584, true);   // auras SPELL_AURA_INCREASE_SPEED(+speed in wisp form), SPELL_AURA_INCREASE_SWIM_SPEED(+swim speed in wisp form), SPELL_AURA_TRANSFORM (to wisp form)
        }

        /* Apply ghost spell to player */
        pCurrChar->CastSpell(pCurrChar, 8326, true);        // auras SPELL_AURA_GHOST, SPELL_AURA_INCREASE_SPEED(why?), SPELL_AURA_INCREASE_SWIM_SPEED(why?)

        /* Allow player to walk on water */
        pCurrChar->SetWaterWalk(true);
    }

    /* If player is on a taxi, continue their flight */
    pCurrChar->ContinueTaxiFlight();

    // reset for all pets before pet loading
    if (pCurrChar->HasAtLoginFlag(AT_LOGIN_RESET_PET_TALENTS))
    {
        Pet::resetTalentsForAllPetsOf(pCurrChar);
    }

    // Load pet if any (if player not alive and in taxi flight or another then pet will remember as temporary unsummoned)
    pCurrChar->LoadPet();

    /* If we're running an FFA PvP realm and the player isn't a GM, mark them as PvP flagged */
    if (sGameConfig.IsFFAPvPRealm() && !pCurrChar->isGameMaster() && !pCurrChar->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_RESTING))
    {
        pCurrChar->SetFFAPvP(true);
    }

    if (pCurrChar->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_CONTESTED_PVP))
    {
        pCurrChar->SetContestedPvP();
    }

    /* Apply onLogon requests (such as talent resets) */
    if (pCurrChar->HasAtLoginFlag(AT_LOGIN_RESET_SPELLS))
    {
        pCurrChar->resetSpells();
        SendNotification(LANG_RESET_SPELLS);
    }

    if (pCurrChar->HasAtLoginFlag(AT_LOGIN_RESET_TALENTS))
    {
        pCurrChar->resetTalents(true, true);
        pCurrChar->SendTalentsInfoData(false);              // original talents send already in to SendInitialPacketsBeforeAddToMap, resend reset state
        pCurrChar->SendTalentsInvoluntarilyReset(false);
        SendNotification(LANG_RESET_TALENTS);
    }


    /* We've done what we need to, remove the flag */
    if (pCurrChar->HasAtLoginFlag(AT_LOGIN_FIRST))
    {
        pCurrChar->RemoveAtLoginFlag(AT_LOGIN_FIRST);
    }

    /* If the server is shutting down, show shutdown time remaining */
    if (sServerControl.IsShuttingDown())
    {
        sServerControl.ShutdownMessage(true, pCurrChar);
    }

    /* If player should have all taxi paths, give them to the player */
    if (sGameConfig.getConfig(CONFIG_BOOL_ALL_TAXI_PATHS))
    {
        pCurrChar->SetTaxiCheater(true);
    }

    /* Send GM notifications */
    if (pCurrChar->isGameMaster())
    {
        SendNotification(LANG_GM_ON);
    }

    if (!pCurrChar->isGMVisible())
    {
        SendNotification(LANG_INVISIBLE_INVISIBLE);
        SpellEntry const* invisibleAuraInfo = sSpellStore.LookupEntry(sGameConfig.getConfig(CONFIG_UINT32_GM_INVISIBLE_AURA));
        if (invisibleAuraInfo && IsSpellAppliesAura(invisibleAuraInfo))
        {
            pCurrChar->CastSpell(pCurrChar, invisibleAuraInfo, true);
        }
    }

    std::string IP_str = GetRemoteAddress();
    sLog.outChar("Account: %d (IP: %s) Login Character:[%s] (guid: %u)",
                 GetAccountId(), IP_str.c_str(), pCurrChar->GetName(), pCurrChar->GetGUIDLow());

    /* Make player stand up if they're not already stood up and not stunned */
    if (!pCurrChar->IsStandState() && !pCurrChar->Blocked(Motion::ReasonStunned))
    {
        pCurrChar->SetStandState(UNIT_STAND_STATE_STAND);
    }

    m_playerLoading = false;

    // Handle Login-Achievements (should be handled after loading)
    pCurrChar->GetAchievementMgr().UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_ON_LOGIN, 1);

    delete holder;
}

