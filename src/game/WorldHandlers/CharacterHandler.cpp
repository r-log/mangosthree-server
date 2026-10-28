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
 * @brief Character creation, deletion, and management handlers
 *
 * Character creation includes validation of name, race, class,
 * appearance customization, and starting location setup.
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
#include "World.h"
#include "ObjectMgr.h"
#include "Pet.h"
#include "DBCStores.h"
#include "AchievementMgr.h"
#include "CharacterCache.h"
#include "Player.h"
#include "CinematicFlyover.h"
#include "session/packets/PlayerPacketSinks.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "CorpseManager.h"
#include "Group.h"
#include "PlayerDump.h"
#include "SocialMgr.h"
#include "Util.h"
#include "Language.h"
#include "spells/data/SpellMgr.h"
#include "Calendar.h"
#include "LFGPackets.h"
#include "GameTime.h"
#include "Timer.h"
#include "MotionMaster.h"
#include "persistence/characters/CharacterDeleteRepository.h"
#include "persistence/characters/CharacterRepository.h"
#include "persistence/characters/account/AccountDataRepository.h"
#include "persistence/characters/pets/PetRepository.h"
#include "persistence/login/AccountRepository.h"

// config option SkipCinematics supported values
enum CinematicsSkipMode
{
    CINEMATICS_SKIP_NONE      = 0,
    CINEMATICS_SKIP_SAME_RACE = 1,
    CINEMATICS_SKIP_ALL       = 2,
};

namespace
{
    /// The one-byte SMSG_CHAR_CREATE answer, from wherever the decision was made.
    void SendCharCreateResult(WorldSession* session, uint8 result)
    {
        WorldPacket data(SMSG_CHAR_CREATE, 1);
        data << uint8(result);
        session->SendPacket(&data);
    }

    /// The one-byte SMSG_CHAR_DELETE answer.
    void SendCharDeleteResult(WorldSession* session, uint8 result)
    {
        WorldPacket data(SMSG_CHAR_DELETE, 1);
        data << uint8(result);
        session->SendPacket(&data);
    }
}

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
    WorldSession* session = sWorld.FindSession(accountId);
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
    return CharacterRepository::StageLoginReads(this, m_guid.GetCounter(), sWorld.getConfig(CONFIG_BOOL_DECLINED_NAMES_USED));
}

// The callbacks find their session again by account id and session id: a session can be gone,
// or replaced by a reconnect, before a query answers.
class CharacterHandler
{
    public:
        void HandleCharEnumCallback(std::vector<CharacterEnumRow> const& rows, uint32 account, proto::SessionId sessionId)
        {
            WorldSession* session = WorldSession::FindRequesterSession(account, sessionId);
            if (!session)
            {
                return;
            }
            session->HandleCharEnum(rows);
        }
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
 * @brief Starts the asynchronous character enumeration query.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleCharEnumOpcode(WorldPacket & /*recv_data*/)
{
    /// get all the data necessary for loading all characters (along with their pets) on the account
    uint32 accountId = GetAccountId();
    proto::SessionId sessionId = GetSessionId();
    CharacterRepository::QueryEnum(accountId, PET_SAVE_AS_CURRENT, sWorld.getConfig(CONFIG_BOOL_DECLINED_NAMES_USED),
                                   [accountId, sessionId](std::vector<CharacterEnumRow> rows)
                                   {
                                       chrHandler.HandleCharEnumCallback(rows, accountId, sessionId);
                                   });
}

/**
 * @brief Every check CMSG_CHAR_CREATE can answer from memory alone (decoupling D7d).
 *
 * Split out of the handler because it runs TWICE: once in the handler, so a request that
 * cannot succeed never reaches the database, and once in the continuation, because a tick
 * passes before the character is created and in that tick the name may have been taken, the
 * account's security may have changed and the creating-disabled mask may have been reloaded
 * (C3). Same checks, same order, same replies, same log lines, both times.
 *
 * @param session The requesting session.
 * @param request The parsed packet; `name` is normalised in place, as the old handler did.
 * @return CHAR_CREATE_SUCCESS when nothing objects, otherwise the code to reply with.
 */
uint8 WorldSession::CharCreateChecksInMemory(WorldSession* session, CharCreateRequest& request)
{
    if (session->GetSecurity() == SEC_PLAYER)
    {
        if (uint32 mask = sWorld.getConfig(CONFIG_UINT32_CHARACTERS_CREATING_DISABLED))
        {
            bool disabled = false;

            Team team = Player::TeamForRace(request.race);
            switch (team)
            {
                case ALLIANCE: disabled = mask & (1 << 0); break;
                case HORDE:    disabled = mask & (1 << 1); break;
                default: break;
            }

            if (disabled)
            {
                return CHAR_CREATE_DISABLED;
            }
        }
    }

    ChrClassesEntry const* classEntry = sChrClassesStore.LookupEntry(request.playerClass);
    ChrRacesEntry const* raceEntry = sChrRacesStore.LookupEntry(request.race);

    if (!classEntry || !raceEntry)
    {
        sLog.outError("Class: %u or Race %u not found in DBC (Wrong DBC files?) or Cheater?", request.playerClass, request.race);
        return CHAR_CREATE_FAILED;
    }

    // prevent character creating Expansion race without Expansion account
    if (raceEntry->Race_related > session->Expansion())
    {
        sLog.outError("Expansion %u account:[%d] tried to Create character with expansion %u race (%u)", session->Expansion(), session->GetAccountId(), raceEntry->Race_related, request.race);
        return CHAR_CREATE_EXPANSION;
    }

    // prevent character creating Expansion class without Expansion account
    if (classEntry->Required_expansion > session->Expansion())
    {
        sLog.outError("Expansion %u account:[%d] tried to Create character with expansion %u class (%u)", session->Expansion(), session->GetAccountId(), classEntry->Required_expansion, request.playerClass);
        return CHAR_CREATE_EXPANSION_CLASS;
    }

    // prevent character creating with invalid name
    if (!normalizePlayerName(request.name))
    {
        sLog.outError("Account:[%d] but tried to Create character with empty [name]", session->GetAccountId());
        return CHAR_NAME_NO_NAME;
    }

    // check name limitations
    uint8 res = ObjectMgr::CheckPlayerName(request.name, true);
    if (res != CHAR_NAME_SUCCESS)
    {
        return res;
    }

    if (session->GetSecurity() == SEC_PLAYER && sObjectMgr.IsReservedName(request.name))
    {
        return CHAR_NAME_RESERVED;
    }

    // Answered by the character cache since D7c, so it is a memory check now -- and being one
    // is what lets the continuation re-run it: two creates of the same name a tick apart get
    // one character and one CHAR_CREATE_NAME_IN_USE, because the first create's cache entry is
    // published before the second continuation runs.
    if (sObjectMgr.GetPlayerGuidByName(request.name))
    {
        return CHAR_CREATE_NAME_IN_USE;
    }

    return CHAR_CREATE_SUCCESS;
}

/**
 * @brief Handles character creation requests from the client.
 *
 * Decoupling D7d: nothing is created here. The checks that need no database are made, and
 * then the account's cross-realm character count is asked for -- on the LOGIN database, which
 * has its own delay thread and its own result queue -- and the handler returns. The character
 * itself is built in HandleCharCreateCallback(), two answers later.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleCharCreateOpcode(WorldPacket& recv_data)
{
    CharCreateRequest request;

    recv_data >> request.name;

    recv_data >> request.race;
    recv_data >> request.playerClass;

    // extract other data required for player creating
    recv_data >> request.gender >> request.skin >> request.face;
    recv_data >> request.hairStyle >> request.hairColor >> request.facialHair >> request.outfitId;

    uint8 res = CharCreateChecksInMemory(this, request);
    if (res != CHAR_CREATE_SUCCESS)
    {
        SendCharCreateResult(this, res);
        return;
    }

    QueueCharCreateAccountRead(GetAccountId(), GetSessionId(), request);
}

/**
 * @brief Stages the create's LOGIN-database read (decoupling D7d).
 *
 * One holder on the login database, one statement. A holder rather than a bare AsyncPQuery so
 * that both halves of the create read the same way -- and so a later PR that needs a second
 * login-side statement adds a slot instead of a shape.
 *
 * The two reads are CHAINED rather than queued together: they live on two different databases,
 * so they would be answered by two different delay threads into two different result queues,
 * and joining them would need shared state with a counter and a decision about which tick the
 * pair completes in. Chaining costs one extra tick and keeps the check order the handler had:
 * the cross-realm limit is answered, and only then is the realm asked anything.
 *
 * @param accountId The requesting account.
 * @param sessionId The session the request arrived on.
 * @param request   The create request, already checked against memory.
 */
void WorldSession::QueueCharCreateAccountRead(uint32 accountId, proto::SessionId sessionId,
                                              CharCreateRequest request)
{
    CharacterRepository::QueueCreateAccountRead(accountId, [accountId, sessionId, request](SqlQueryHolder* h)
                                                {
                                                    WorldSession::HandleCharCreateAccountCallback(std::unique_ptr<SqlQueryHolder>(h),
                                                                                                  accountId, sessionId, request);
                                                });
}

/**
 * @brief Applies the cross-realm character limit, then asks the realm (decoupling D7d).
 *
 * @param holder    The staged login-database read, answered.
 * @param accountId The requesting account.
 * @param sessionId The session the request arrived on.
 * @param request   The create request.
 */
void WorldSession::HandleCharCreateAccountCallback(std::unique_ptr<SqlQueryHolder> holder, uint32 accountId,
                                                   proto::SessionId sessionId, CharCreateRequest request)
{
    WorldSession* session = FindRequesterSession(accountId, sessionId);
    if (!session || !holder)
    {
        return;
    }

    std::optional<uint32> accountCharCount = CharacterRepository::CreateAccountCharacterCount(holder.get());
    if (accountCharCount)
    {
        uint32 acctcharcount = *accountCharCount;

        if (acctcharcount >= sWorld.getConfig(CONFIG_UINT32_CHARACTERS_PER_ACCOUNT))
        {
            SendCharCreateResult(session, CHAR_CREATE_ACCOUNT_LIMIT);
            return;
        }
    }

    QueueCharCreateRealmReads(accountId, sessionId, request);
}

/**
 * @brief Stages the create's CHARACTER-database reads (decoupling D7d, C4).
 *
 * Three statements in one holder, run back to back on the delay thread under one connection
 * lock. The last is the statement the old handler issued TWICE (once to test, once to fetch,
 * leaking both results) collapsed into one.
 *
 * The middle one is staged UNCONDITIONALLY, where the old handler issued it only when
 * `!AllowTwoSideAccounts || SkipCinematics == SAME_RACE || class == DEATH_KNIGHT`. Staging it
 * on those flags and then letting the continuation re-read them is the one way this chain
 * could fail unsafely: a `.reload config` in the tick between could flip the condition on, and
 * the block would find an empty slot and silently skip the PvP-team and heroic-slot scans --
 * admitting a two-side character or a heroic over the limit. The block still decides whether
 * to LOOK at the rows, so behaviour is unchanged; what it costs when the old condition was
 * false is one extra `LIMIT 1` read of one indexed row per character creation.
 *
 * @param accountId            The requesting account.
 * @param sessionId            The session the request arrived on.
 * @param request              The create request.
 */
void WorldSession::QueueCharCreateRealmReads(uint32 accountId, proto::SessionId sessionId,
                                             CharCreateRequest request)
{
    CinematicsSkipMode skipCinematics = CinematicsSkipMode(sWorld.getConfig(CONFIG_UINT32_SKIP_CINEMATICS));

    CharacterRepository::QueueCreateRealmReads(accountId,
                                               skipCinematics == CINEMATICS_SKIP_SAME_RACE || request.playerClass == CLASS_DEATH_KNIGHT,
                                               [accountId, sessionId, request](SqlQueryHolder* h)
                                               {
                                                   WorldSession::HandleCharCreateCallback(std::unique_ptr<SqlQueryHolder>(h),
                                                                                          accountId, sessionId, request);
                                               });
}

/**
 * @brief Creates the character, once every read it needed has answered (decoupling D7d, C3).
 *
 * The whole second half of the old handler: the realm limit, the heroic and two-side rules,
 * the Player object, SaveToDB(), the cache entry, the realmcharacters rewrite, the starting
 * pet and the SMSG_CHAR_CREATE reply. Nothing above it wrote anything, so a continuation that
 * never arrives -- a disconnect in that tick -- leaves no half-made character behind.
 *
 * @param holder    The three staged realm reads, answered.
 * @param accountId The requesting account.
 * @param sessionId The session the request arrived on.
 * @param request   The create request.
 */
void WorldSession::HandleCharCreateCallback(std::unique_ptr<SqlQueryHolder> holder, uint32 accountId,
                                            proto::SessionId sessionId, CharCreateRequest request)
{
    WorldSession* session = FindRequesterSession(accountId, sessionId);
    if (!session || !holder)
    {
        return;
    }

    // C3: everything the handler checked before it queued, checked again -- two ticks have
    // passed, and the name is the one that matters.
    uint8 res = CharCreateChecksInMemory(session, request);
    if (res != CHAR_CREATE_SUCCESS)
    {
        SendCharCreateResult(session, res);
        return;
    }

    uint8 race_ = request.race;
    uint8 class_ = request.playerClass;

    std::optional<uint8> realmCharCount = CharacterRepository::CreateRealmCharacterCount(holder.get());
    uint8 charcount = 0;
    if (realmCharCount)
    {
        charcount = *realmCharCount;

        if (charcount >= sWorld.getConfig(CONFIG_UINT32_CHARACTERS_PER_REALM))
        {
            SendCharCreateResult(session, CHAR_CREATE_SERVER_LIMIT);
            return;
        }
    }

    // speedup check for heroic class disabled case
    uint32 heroic_free_slots = sWorld.getConfig(CONFIG_UINT32_HEROIC_CHARACTERS_PER_REALM);
    if (heroic_free_slots == 0 && session->GetSecurity() == SEC_PLAYER && class_ == CLASS_DEATH_KNIGHT)
    {
        SendCharCreateResult(session, CHAR_CREATE_UNIQUE_CLASS_LIMIT);
        return;
    }

    // speedup check for heroic class disabled case
    uint32 req_level_for_heroic = sWorld.getConfig(CONFIG_UINT32_MIN_LEVEL_FOR_HEROIC_CHARACTER_CREATING);
    if (session->GetSecurity() == SEC_PLAYER && class_ == CLASS_DEATH_KNIGHT && req_level_for_heroic > sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL))
    {
        SendCharCreateResult(session, CHAR_CREATE_LEVEL_REQUIREMENT);
        return;
    }

    bool AllowTwoSideAccounts = sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_ACCOUNTS) || session->GetSecurity() > SEC_PLAYER;
    CinematicsSkipMode skipCinematics = CinematicsSkipMode(sWorld.getConfig(CONFIG_UINT32_SKIP_CINEMATICS));

    bool have_same_race = false;

    // if 0 then allowed creating without any characters
    bool have_req_level_for_heroic = (req_level_for_heroic == 0);

    if (!AllowTwoSideAccounts || skipCinematics == CINEMATICS_SKIP_SAME_RACE || class_ == CLASS_DEATH_KNIGHT)
    {
        std::vector<CreateAccountCharacterRow> accountChars = CharacterRepository::CreateAccountCharacters(holder.get());
        if (!accountChars.empty())
        {
            Team team_ = Player::TeamForRace(race_);

            size_t next = 0;
            CreateAccountCharacterRow const* accountChar = &accountChars[next];
            uint8 acc_race  = accountChar->race;

            if (session->GetSecurity() == SEC_PLAYER && class_ == CLASS_DEATH_KNIGHT)
            {
                uint8 acc_class = accountChar->classId;
                if (acc_class == CLASS_DEATH_KNIGHT)
                {
                    if (heroic_free_slots > 0)
                    {
                        --heroic_free_slots;
                    }

                    if (heroic_free_slots == 0)
                    {
                        SendCharCreateResult(session, CHAR_CREATE_UNIQUE_CLASS_LIMIT);
                        return;
                    }
                }

                if (!have_req_level_for_heroic)
                {
                    uint32 acc_level = accountChar->level;
                    if (acc_level >= req_level_for_heroic)
                    {
                        have_req_level_for_heroic = true;
                    }
                }
            }

            // need to check team only for first character
            // TODO: what to if account already has characters of both races?
            if (!AllowTwoSideAccounts)
            {
                if (acc_race == 0 || Player::TeamForRace(acc_race) != team_)
                {
                    SendCharCreateResult(session, CHAR_CREATE_PVP_TEAMS_VIOLATION);
                    return;
                }
            }

            // search same race for cinematic or same class if need
            // TODO: check if cinematic already shown? (already logged in?; cinematic field)
            while ((skipCinematics == CINEMATICS_SKIP_SAME_RACE && !have_same_race) || class_ == CLASS_DEATH_KNIGHT)
            {
                if (++next >= accountChars.size())
                {
                    break;
                }

                accountChar = &accountChars[next];
                acc_race = accountChar->race;

                if (!have_same_race)
                {
                    have_same_race = race_ == acc_race;
                }

                if (session->GetSecurity() == SEC_PLAYER && class_ == CLASS_DEATH_KNIGHT)
                {
                    uint8 acc_class = accountChar->classId;
                    if (acc_class == CLASS_DEATH_KNIGHT)
                    {
                        if (heroic_free_slots > 0)
                        {
                            --heroic_free_slots;
                        }

                        if (heroic_free_slots == 0)
                        {
                            SendCharCreateResult(session, CHAR_CREATE_UNIQUE_CLASS_LIMIT);
                            return;
                        }
                    }

                    if (!have_req_level_for_heroic)
                    {
                        uint32 acc_level = accountChar->level;
                        if (acc_level >= req_level_for_heroic)
                        {
                            have_req_level_for_heroic = true;
                        }
                    }
                }
            }
        }
    }

    if (session->GetSecurity() == SEC_PLAYER && class_ == CLASS_DEATH_KNIGHT && !have_req_level_for_heroic)
    {
        SendCharCreateResult(session, CHAR_CREATE_LEVEL_REQUIREMENT);
        return;
    }

    Player* pNewChar = new Player(session);
    InstallPlayerPacketSinks(*pNewChar);
    // Sets the createdTime of the character which is UNIX timestamp
    uint32 createdDate = GetUnixTimeStamp(); // Unix Timestamp in seconds
    pNewChar->SetCreatedDate(createdDate); // TODO get currentTimeStamp for createdTime

    if (!pNewChar->Create(sObjectMgr.GeneratePlayerLowGuid(), request.name, race_, class_, request.gender,
                          request.skin, request.face, request.hairStyle, request.hairColor,
                          request.facialHair, request.outfitId))
    {
        // Player not create (race/class problem?)
        delete pNewChar;

        SendCharCreateResult(session, CHAR_CREATE_ERROR);

        return;
    }

    if ((have_same_race && skipCinematics == CINEMATICS_SKIP_SAME_RACE) || skipCinematics == CINEMATICS_SKIP_ALL)
    {
        pNewChar->setCinematic(1);                           // not show intro
    }

    pNewChar->SetAtLoginFlag(AT_LOGIN_FIRST);               // First login

    // Player created, save it now
    pNewChar->SaveToDB();

    // Decoupling D7c: the `characters` row exists from here on, so the cache has to know
    // about it from here on -- the same values the INSERT above just wrote.
    {
        CharacterCacheEntry cached;
        cached.guid        = pNewChar->GetObjectGuid();
        cached.accountId   = accountId;
        cached.name        = pNewChar->GetName();
        cached.race        = pNewChar->getRace();
        cached.playerClass = pNewChar->getClass();
        cached.level       = uint8(pNewChar->getLevel());
        cached.zoneId      = pNewChar->GetCachedZoneId();
        sCharacterCache.Add(cached);
    }

    charcount += 1;

    AccountRepository::SetRealmCharacterCount(accountId, realmID, charcount);
    uint32 pet_id = 1;
    {
        std::optional<uint32> highestPetId = CharacterRepository::CreateHighestPetId(holder.get());
        if (highestPetId)
        {
            pet_id = *highestPetId;
            pet_id += 1;
        }
        //else
            //pet_id = 1;
    }

    if (class_ == CLASS_WARLOCK)
    {
        PetRepository::ReplaceStarterPet(STARTER_PET_IMP, pet_id, pNewChar->GetGUIDLow());
        pNewChar->GetPetMgr().SetTemporaryUnsummonedPetNumber(pet_id);
    }
    if (class_ == CLASS_HUNTER)
    {
        switch(race_)
        {
        case RACE_HUMAN:
            PetRepository::ReplaceStarterPet(STARTER_PET_WOLF, pet_id, pNewChar->GetGUIDLow());
            break;
        case RACE_DWARF:
            PetRepository::ReplaceStarterPet(STARTER_PET_BEAR, pet_id, pNewChar->GetGUIDLow());
            break;
        case RACE_ORC:
            PetRepository::ReplaceStarterPet(STARTER_PET_BOAR, pet_id, pNewChar->GetGUIDLow());
            break;
        case RACE_NIGHTELF:
            PetRepository::ReplaceStarterPet(STARTER_PET_CAT, pet_id, pNewChar->GetGUIDLow());
            break;
        case RACE_UNDEAD:
            PetRepository::ReplaceStarterPet(STARTER_PET_SPIDER, pet_id, pNewChar->GetGUIDLow());
            break;
        case RACE_TAUREN:
            PetRepository::ReplaceStarterPet(STARTER_PET_TALLSTRIDER, pet_id, pNewChar->GetGUIDLow());
            break;
        case RACE_TROLL:
            PetRepository::ReplaceStarterPet(STARTER_PET_RAPTOR, pet_id, pNewChar->GetGUIDLow());
            break;
        case RACE_GOBLIN:
            PetRepository::ReplaceStarterPet(STARTER_PET_CRAB, pet_id, pNewChar->GetGUIDLow());
            break;
        case RACE_BLOODELF:
            PetRepository::ReplaceStarterPet(STARTER_PET_DRAGONHAWK, pet_id, pNewChar->GetGUIDLow());
            break;
        case RACE_DRAENEI:
            PetRepository::ReplaceStarterPet(STARTER_PET_MOTH, pet_id, pNewChar->GetGUIDLow());
            break;
        case RACE_WORGEN:
            PetRepository::ReplaceStarterPet(STARTER_PET_DOG, pet_id, pNewChar->GetGUIDLow());
            break;
        }
        pNewChar->GetPetMgr().SetTemporaryUnsummonedPetNumber(pet_id);
    }

    pNewChar->CleanupsBeforeDelete();
    SendCharCreateResult(session, CHAR_CREATE_SUCCESS);

    std::string IP_str = session->GetRemoteAddress();
    BASIC_LOG("Account: %d (IP: %s) Create Character:[%s] (guid: %u)", accountId, IP_str.c_str(), request.name.c_str(), pNewChar->GetGUIDLow());
    sLog.outChar("Account: %d (IP: %s) Create Character:[%s] (guid: %u)", accountId, IP_str.c_str(), request.name.c_str(), pNewChar->GetGUIDLow());

    delete pNewChar;                                        // created only to call SaveToDB()
}

/**
 * @brief Deletes a character owned by the current account.
 *
 * Decoupling D7d: the three checks that need no database stay here, and everything else --
 * the ownership check, the log, the dump, the calendar sweep, the delete itself and the
 * SMSG_CHAR_DELETE reply -- moves into HandleCharDeleteCallback(), which runs once the one
 * holder this queues has answered.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleCharDeleteOpcode(WorldPacket& recv_data)
{
    ObjectGuid guid;
    recv_data >> guid;

    // can't delete loaded character
    if (sObjectMgr.GetPlayer(guid))
    {
        return;
    }

    // is guild leader
    if (sGuildMgr.GetGuildByLeader(guid))
    {
        SendCharDeleteResult(this, CHAR_DELETE_FAILED_GUILD_LEADER);
        return;
    }

    // is arena team captain
    if (sObjectMgr.GetArenaTeamByCaptain(guid))
    {
        SendCharDeleteResult(this, CHAR_DELETE_FAILED_ARENA_CAPTAIN);
        return;
    }

    QueueCharDeleteReads(GetAccountId(), GetSessionId(), guid);
}

/**
 * @brief Stages every read a character delete needs, in ONE holder (decoupling D7d, C4).
 *
 * The handler's own ownership read (`account`, `name`) shares the holder with the six the
 * delete body needs, so the whole delete costs one round trip. It is kept as a real read
 * rather than a character-cache lookup on purpose: it is the check that stops one account
 * deleting another's character, and the cache is documented as possibly disagreeing with a
 * row edited outside the server.
 *
 * @param accountId The requesting account.
 * @param sessionId The session the request arrived on.
 * @param guid      The character to delete.
 */
void WorldSession::QueueCharDeleteReads(uint32 accountId, proto::SessionId sessionId, ObjectGuid guid)
{
    // Which method this delete will use decides which reads it needs, and it blocks on
    // nothing: the level comes from the character cache since D7c.
    uint32 charDeleteMethod = Player::DeleteMethodFor(guid, false);

    CharacterDeleteRepository::QueueDeleteReads(guid.GetCounter(), charDeleteMethod == 0, true,
                                                [accountId, sessionId, guid, charDeleteMethod]
                                                (SqlQueryHolder* h)
                                                {
                                                    WorldSession::HandleCharDeleteCallback(std::unique_ptr<SqlQueryHolder>(h),
                                                                                           accountId, sessionId, guid,
                                                                                           charDeleteMethod);
                                                });
}

/**
 * @brief Deletes the character, once every read it needs has answered (decoupling D7d).
 *
 * The ordering this preserves, in the order it happens: the three in-memory checks again (a
 * tick has passed, so the character may have logged in or become a guild leader), the
 * ownership check, the log and the optional dump, the calendar sweep, then the delete body --
 * whose reads precede its transaction exactly as they did when it was synchronous, because
 * they are in this holder -- and finally the reply, which now says "deleted" only after the
 * transaction that deletes it has been queued.
 *
 * @param holder           The staged reads, answered.
 * @param accountId        The requesting account.
 * @param sessionId        The session the request arrived on.
 * @param guid             The character to delete.
 * @param charDeleteMethod The method the reads were staged for.
 */
void WorldSession::HandleCharDeleteCallback(std::unique_ptr<SqlQueryHolder> holder, uint32 accountId,
                                            proto::SessionId sessionId, ObjectGuid guid,
                                            uint32 charDeleteMethod)
{
    WorldSession* session = FindRequesterSession(accountId, sessionId);
    if (!session || !holder)
    {
        return;
    }

    // C3: the three memory checks again. A character that logged in, or whose guild or arena
    // team made it a leader in the meantime, must not be deleted by an answer in flight.
    //
    // PlayerLoading() is the half GetPlayer() cannot see: CMSG_PLAYER_LOGIN sets the flag and
    // queues its own holder, so a client that pipelines delete-then-login on one guid in one
    // tick has a login in flight with no Player in the registry yet. Without this the delete
    // would go through under it and the login would seat a character on rows that are being
    // deleted. The session simply keeps the character; the client can delete it after logout.
    if (sObjectMgr.GetPlayer(guid) || session->PlayerLoading())
    {
        return;
    }

    if (sGuildMgr.GetGuildByLeader(guid))
    {
        SendCharDeleteResult(session, CHAR_DELETE_FAILED_GUILD_LEADER);
        return;
    }

    if (sObjectMgr.GetArenaTeamByCaptain(guid))
    {
        SendCharDeleteResult(session, CHAR_DELETE_FAILED_ARENA_CAPTAIN);
        return;
    }

    uint32 lowguid = guid.GetCounter();

    uint32 rowAccountId = 0;
    std::string name;

    if (std::optional<DeleteOwnerRow> owner = CharacterDeleteRepository::DeleteOwner(holder.get()))
    {
        rowAccountId = owner->account;
        name = owner->name;
    }

    // prevent deleting other players' characters using cheating tools
    if (rowAccountId != accountId)
    {
        return;
    }

    std::string IP_str = session->GetRemoteAddress();
    BASIC_LOG("Account: %d (IP: %s) Delete Character:[%s] (guid: %u)", accountId, IP_str.c_str(), name.c_str(), lowguid);
    sLog.outChar("Account: %d (IP: %s) Delete Character:[%s] (guid: %u)", accountId, IP_str.c_str(), name.c_str(), lowguid);

    if (sLog.IsOutCharDump())                               // optimize GetPlayerDump call
    {
        std::string dump = PlayerDumpWriter().GetDump(lowguid);
        sLog.outCharDump(dump.c_str(), accountId, lowguid, name.c_str());
    }

    sCalendarMgr.RemovePlayerCalendar(guid);

    Player::DeleteFromDBFromHolder(std::move(holder), guid, accountId, true, charDeleteMethod);

    SendCharDeleteResult(session, CHAR_DELETE_SUCCESS);
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
    if (!sWorld.getConfig(CONFIG_BOOL_LFG_ENABLE))
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
        std::string str_motd = sWorld.GetMotd();
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

            guild->DisplayGuildBankTabsInfo(this);
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
        sWorld.getConfig(CONFIG_BOOL_CINEMATIC_FLYOVER_ENABLE);
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
    if (sWorld.IsFFAPvPRealm() && !pCurrChar->isGameMaster() && !pCurrChar->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_RESTING))
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
    if (sWorld.IsShutdowning())
    {
        sWorld.ShutdownMsg(true, pCurrChar);
    }

    /* If player should have all taxi paths, give them to the player */
    if (sWorld.getConfig(CONFIG_BOOL_ALL_TAXI_PATHS))
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
        SpellEntry const* invisibleAuraInfo = sSpellStore.LookupEntry(sWorld.getConfig(CONFIG_UINT32_GM_INVISIBLE_AURA));
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

/**
 * @brief Updates the at-war state for a reputation entry.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleSetFactionAtWarOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_SET_FACTION_ATWAR");

    uint32 repListID;
    uint8  flag;

    recv_data >> repListID;
    recv_data >> flag;

    GetPlayer()->SetFactionAtWar(repListID, flag);
}

/**
 * @brief Marks a single tutorial flag as seen for the account.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleTutorialFlagOpcode(WorldPacket& recv_data)
{
    uint32 iFlag;
    recv_data >> iFlag;

    uint32 wInt = (iFlag / 32);
    if (wInt >= 8)
    {
        // sLog.outError("CHEATER? Account:[%d] Guid[%u] tried to send wrong CMSG_TUTORIAL_FLAG", GetAccountId(),GetGUID());
        return;
    }
    uint32 rInt = (iFlag % 32);

    uint32 tutflag = GetTutorialInt(wInt);
    tutflag |= (1 << rInt);
    SetTutorialInt(wInt, tutflag);

    // DEBUG_LOG("Received Tutorial Flag Set {%u}.", iFlag);
}

/**
 * @brief Sets all tutorial flags to completed for the account.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleTutorialClearOpcode(WorldPacket & /*recv_data*/)
{
    for (int i = 0; i < 8; ++i)
    {
        SetTutorialInt(i, 0xFFFFFFFF);
    }
}

/**
 * @brief Resets all tutorial flags for the account.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleTutorialResetOpcode(WorldPacket & /*recv_data*/)
{
    for (int i = 0; i < 8; ++i)
    {
        SetTutorialInt(i, 0x00000000);
    }
}

/**
 * @brief Sets the watched faction shown in the reputation UI.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleSetWatchedFactionOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_SET_WATCHED_FACTION");
    int32 repId;
    recv_data >> repId;
    GetPlayer()->SetInt32Value(PLAYER_FIELD_WATCHED_FACTION_INDEX, repId);
}

/**
 * @brief Toggles a faction's inactive state in the reputation list.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleSetFactionInactiveOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_SET_FACTION_INACTIVE");
    uint32 replistid;
    uint8 inactive;
    recv_data >> replistid >> inactive;

    _player->GetReputationMgr().SetInactive(replistid, inactive);
}

/**
 * @brief Toggles the player's helm visibility flag.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleShowingHelmOpcode(WorldPacket & /*recv_data*/)
{
    DEBUG_LOG("CMSG_SHOWING_HELM for %s", _player->GetName());
    _player->ToggleFlag(PLAYER_FLAGS, PLAYER_FLAGS_HIDE_HELM);
}

/**
 * @brief Toggles the player's cloak visibility flag.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleShowingCloakOpcode(WorldPacket & /*recv_data*/)
{
    DEBUG_LOG("CMSG_SHOWING_CLOAK for %s", _player->GetName());
    _player->ToggleFlag(PLAYER_FLAGS, PLAYER_FLAGS_HIDE_CLOAK);
}

