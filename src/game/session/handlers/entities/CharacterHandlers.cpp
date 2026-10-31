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

#include "session/handlers/entities/CharacterHandlers.h"

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
#include "data/GameConfig.h"
#include "ObjectMgr.h"
#include "social/ArenaTeamRegistry.h"
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
 * @brief Starts the asynchronous character enumeration query.
 *
 * @param recv_data The received opcode packet.
 */
void CharacterHandlers::HandleCharEnum(WorldSession& session, WorldPacket & /*recv_data*/)
{
    /// get all the data necessary for loading all characters (along with their pets) on the account
    uint32 accountId = session.GetAccountId();
    proto::SessionId sessionId = session.GetSessionId();
    CharacterRepository::QueryEnum(accountId, PET_SAVE_AS_CURRENT, sGameConfig.getConfig(CONFIG_BOOL_DECLINED_NAMES_USED),
                                   [accountId, sessionId](std::vector<CharacterEnumRow> rows)
                                   {
                                       HandleCharEnumCallback(rows, accountId, sessionId);
                                   });
}

/**
 * @brief Hands the account's characters to the session that asked, if it is still the same one.
 *
 * @param rows The account's characters.
 * @param account The requesting account.
 * @param sessionId The session the request arrived on.
 */
void CharacterHandlers::HandleCharEnumCallback(std::vector<CharacterEnumRow> const& rows, uint32 account, proto::SessionId sessionId)
{
    WorldSession* session = WorldSession::FindRequesterSession(account, sessionId);
    if (!session)
    {
        return;
    }
    session->HandleCharEnum(rows);
}

/**
 * @brief Every check CMSG_CHAR_CREATE can answer from memory alone.
 *
 * Split out of the handler because it runs TWICE: once in the handler, so a request that
 * cannot succeed never reaches the database, and once in the continuation, because a tick
 * passes before the character is created and in that tick the name may have been taken, the
 * account's security may have changed and the creating-disabled mask may have been reloaded.
 * Same checks, same order, same replies, same log lines, both times.
 *
 * @param session The requesting session.
 * @param request The parsed packet; `name` is normalised in place.
 * @return CHAR_CREATE_SUCCESS when nothing objects, otherwise the code to reply with.
 */
uint8 CharacterHandlers::CharCreateChecksInMemory(WorldSession* session, CharCreateRequest& request)
{
    if (session->GetSecurity() == SEC_PLAYER)
    {
        if (uint32 mask = sGameConfig.getConfig(CONFIG_UINT32_CHARACTERS_CREATING_DISABLED))
        {
            bool disabled = false;

            Team team = TeamForRace(request.race);
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

    // The character cache answers it from memory, which lets the continuation re-run it:
    // two creates of the same name a tick apart get
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
 * Nothing is created here. The checks that need no database are made, and
 * then the account's cross-realm character count is asked for -- on the LOGIN database, which
 * has its own delay thread and its own result queue -- and the handler returns. The character
 * itself is built in HandleCharCreateCallback(), two answers later.
 *
 * @param recv_data The received opcode packet.
 */
void CharacterHandlers::HandleCharCreate(WorldSession& session, WorldPacket& recv_data)
{
    CharCreateRequest request;

    recv_data >> request.name;

    recv_data >> request.race;
    recv_data >> request.playerClass;

    // extract other data required for player creating
    recv_data >> request.gender >> request.skin >> request.face;
    recv_data >> request.hairStyle >> request.hairColor >> request.facialHair >> request.outfitId;

    uint8 res = CharCreateChecksInMemory(&session, request);
    if (res != CHAR_CREATE_SUCCESS)
    {
        SendCharCreateResult(&session, res);
        return;
    }

    QueueCharCreateAccountRead(session.GetAccountId(), session.GetSessionId(), request);
}

/**
 * @brief Stages the create's LOGIN-database read.
 *
 * One holder on the login database, one statement. A holder rather than a bare AsyncPQuery so
 * that both halves of the create read the same way.
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
void CharacterHandlers::QueueCharCreateAccountRead(uint32 accountId, proto::SessionId sessionId,
                                              CharCreateRequest request)
{
    CharacterRepository::QueueCreateAccountRead(accountId, [accountId, sessionId, request](SqlQueryHolder* h)
                                                {
                                                    CharacterHandlers::HandleCharCreateAccountCallback(std::unique_ptr<SqlQueryHolder>(h),
                                                                                                  accountId, sessionId, request);
                                                });
}

/**
 * @brief Applies the cross-realm character limit, then asks the realm.
 *
 * @param holder    The staged login-database read, answered.
 * @param accountId The requesting account.
 * @param sessionId The session the request arrived on.
 * @param request   The create request.
 */
void CharacterHandlers::HandleCharCreateAccountCallback(std::unique_ptr<SqlQueryHolder> holder, uint32 accountId,
                                                   proto::SessionId sessionId, CharCreateRequest request)
{
    WorldSession* session = WorldSession::FindRequesterSession(accountId, sessionId);
    if (!session || !holder)
    {
        return;
    }

    std::optional<uint32> accountCharCount = CharacterRepository::CreateAccountCharacterCount(holder.get());
    if (accountCharCount)
    {
        uint32 acctcharcount = *accountCharCount;

        if (acctcharcount >= sGameConfig.getConfig(CONFIG_UINT32_CHARACTERS_PER_ACCOUNT))
        {
            SendCharCreateResult(session, CHAR_CREATE_ACCOUNT_LIMIT);
            return;
        }
    }

    QueueCharCreateRealmReads(accountId, sessionId, request);
}

/**
 * @brief Stages the create's CHARACTER-database reads.
 *
 * Three statements in one holder, run back to back on the delay thread under one connection
 * lock, the last one read once.
 *
 * The middle one is staged unconditionally, though the continuation looks at its rows only when
 * `!AllowTwoSideAccounts || SkipCinematics == SAME_RACE || class == DEATH_KNIGHT`. Staging it
 * on those flags and letting the continuation re-read them could fail unsafely: a
 * `.reload config` in the tick between could flip the condition on, and the block would find an
 * empty slot and silently skip the PvP-team and heroic-slot scans, admitting a two-side
 * character or a heroic over the limit. When the condition is false it costs one `LIMIT 1`
 * read of one indexed row per character creation.
 *
 * @param accountId            The requesting account.
 * @param sessionId            The session the request arrived on.
 * @param request              The create request.
 */
void CharacterHandlers::QueueCharCreateRealmReads(uint32 accountId, proto::SessionId sessionId,
                                             CharCreateRequest request)
{
    CinematicsSkipMode skipCinematics = CinematicsSkipMode(sGameConfig.getConfig(CONFIG_UINT32_SKIP_CINEMATICS));

    CharacterRepository::QueueCreateRealmReads(accountId,
                                               skipCinematics == CINEMATICS_SKIP_SAME_RACE || request.playerClass == CLASS_DEATH_KNIGHT,
                                               [accountId, sessionId, request](SqlQueryHolder* h)
                                               {
                                                   CharacterHandlers::HandleCharCreateCallback(std::unique_ptr<SqlQueryHolder>(h),
                                                                                          accountId, sessionId, request);
                                               });
}

/**
 * @brief Creates the character, once every read it needed has answered.
 *
 * The second half of the create: the realm limit, the heroic and two-side rules,
 * the Player object, SaveToDB(), the cache entry, the realmcharacters rewrite, the starting
 * pet and the SMSG_CHAR_CREATE reply. Nothing above it wrote anything, so a continuation that
 * never arrives -- a disconnect in that tick -- leaves no half-made character behind.
 *
 * @param holder    The three staged realm reads, answered.
 * @param accountId The requesting account.
 * @param sessionId The session the request arrived on.
 * @param request   The create request.
 */
void CharacterHandlers::HandleCharCreateCallback(std::unique_ptr<SqlQueryHolder> holder, uint32 accountId,
                                            proto::SessionId sessionId, CharCreateRequest request)
{
    WorldSession* session = WorldSession::FindRequesterSession(accountId, sessionId);
    if (!session || !holder)
    {
        return;
    }

    // Everything the handler checked before it queued, checked again: two ticks have passed,
    // and the name is the one that matters.
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

        if (charcount >= sGameConfig.getConfig(CONFIG_UINT32_CHARACTERS_PER_REALM))
        {
            SendCharCreateResult(session, CHAR_CREATE_SERVER_LIMIT);
            return;
        }
    }

    // speedup check for heroic class disabled case
    uint32 heroic_free_slots = sGameConfig.getConfig(CONFIG_UINT32_HEROIC_CHARACTERS_PER_REALM);
    if (heroic_free_slots == 0 && session->GetSecurity() == SEC_PLAYER && class_ == CLASS_DEATH_KNIGHT)
    {
        SendCharCreateResult(session, CHAR_CREATE_UNIQUE_CLASS_LIMIT);
        return;
    }

    // speedup check for heroic class disabled case
    uint32 req_level_for_heroic = sGameConfig.getConfig(CONFIG_UINT32_MIN_LEVEL_FOR_HEROIC_CHARACTER_CREATING);
    if (session->GetSecurity() == SEC_PLAYER && class_ == CLASS_DEATH_KNIGHT && req_level_for_heroic > sGameConfig.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL))
    {
        SendCharCreateResult(session, CHAR_CREATE_LEVEL_REQUIREMENT);
        return;
    }

    bool AllowTwoSideAccounts = sGameConfig.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_ACCOUNTS) || session->GetSecurity() > SEC_PLAYER;
    CinematicsSkipMode skipCinematics = CinematicsSkipMode(sGameConfig.getConfig(CONFIG_UINT32_SKIP_CINEMATICS));

    bool have_same_race = false;

    // if 0 then allowed creating without any characters
    bool have_req_level_for_heroic = (req_level_for_heroic == 0);

    if (!AllowTwoSideAccounts || skipCinematics == CINEMATICS_SKIP_SAME_RACE || class_ == CLASS_DEATH_KNIGHT)
    {
        std::vector<CreateAccountCharacterRow> accountChars = CharacterRepository::CreateAccountCharacters(holder.get());
        if (!accountChars.empty())
        {
            Team team_ = TeamForRace(race_);

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
                if (acc_race == 0 || TeamForRace(acc_race) != team_)
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

    // The `characters` row exists from here on, so the cache has to know about it from here
    // on: the same values the INSERT above just wrote.
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
 * The three checks that need no database stay here, and everything else --
 * the ownership check, the log, the dump, the calendar sweep, the delete itself and the
 * SMSG_CHAR_DELETE reply -- moves into HandleCharDeleteCallback(), which runs once the one
 * holder this queues has answered.
 *
 * @param recv_data The received opcode packet.
 */
void CharacterHandlers::HandleCharDelete(WorldSession& session, WorldPacket& recv_data)
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
        SendCharDeleteResult(&session, CHAR_DELETE_FAILED_GUILD_LEADER);
        return;
    }

    // is arena team captain
    if (sArenaTeamRegistry.FindByCaptain(guid))
    {
        SendCharDeleteResult(&session, CHAR_DELETE_FAILED_ARENA_CAPTAIN);
        return;
    }

    QueueCharDeleteReads(session.GetAccountId(), session.GetSessionId(), guid);
}

/**
 * @brief Stages every read a character delete needs, in ONE holder.
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
void CharacterHandlers::QueueCharDeleteReads(uint32 accountId, proto::SessionId sessionId, ObjectGuid guid)
{
    // Which method this delete will use decides which reads it needs, and it blocks on
    // nothing: the level comes from the character cache.
    uint32 charDeleteMethod = Player::DeleteMethodFor(guid, false);

    CharacterDeleteRepository::QueueDeleteReads(guid.GetCounter(), charDeleteMethod == 0, true,
                                                [accountId, sessionId, guid, charDeleteMethod]
                                                (SqlQueryHolder* h)
                                                {
                                                    CharacterHandlers::HandleCharDeleteCallback(std::unique_ptr<SqlQueryHolder>(h),
                                                                                           accountId, sessionId, guid,
                                                                                           charDeleteMethod);
                                                });
}

/**
 * @brief Deletes the character, once every read it needs has answered.
 *
 * The ordering this preserves, in the order it happens: the three in-memory checks again (a
 * tick has passed, so the character may have logged in or become a guild leader), the
 * ownership check, the log and the optional dump, the calendar sweep, then the delete body,
 * whose reads precede its transaction because they are in this holder, and finally the reply,
 * which says "deleted" only after the transaction that deletes it has been queued.
 *
 * @param holder           The staged reads, answered.
 * @param accountId        The requesting account.
 * @param sessionId        The session the request arrived on.
 * @param guid             The character to delete.
 * @param charDeleteMethod The method the reads were staged for.
 */
void CharacterHandlers::HandleCharDeleteCallback(std::unique_ptr<SqlQueryHolder> holder, uint32 accountId,
                                            proto::SessionId sessionId, ObjectGuid guid,
                                            uint32 charDeleteMethod)
{
    WorldSession* session = WorldSession::FindRequesterSession(accountId, sessionId);
    if (!session || !holder)
    {
        return;
    }

    // The three memory checks again. A character that logged in, or whose guild or arena
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

    if (sArenaTeamRegistry.FindByCaptain(guid))
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
 * @brief Updates the at-war state for a reputation entry.
 *
 * @param recv_data The received opcode packet.
 */
void CharacterHandlers::HandleSetFactionAtWar(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_SET_FACTION_ATWAR");

    uint32 repListID;
    uint8  flag;

    recv_data >> repListID;
    recv_data >> flag;

    session.GetPlayer()->SetFactionAtWar(repListID, flag);
}

/**
 * @brief Marks a single tutorial flag as seen for the account.
 *
 * @param recv_data The received opcode packet.
 */
void CharacterHandlers::HandleTutorialFlag(WorldSession& session, WorldPacket& recv_data)
{
    uint32 iFlag;
    recv_data >> iFlag;

    uint32 wInt = (iFlag / 32);
    if (wInt >= 8)
    {
        // sLog.outError("CHEATER? Account:[%d] Guid[%u] tried to send wrong CMSG_TUTORIAL_FLAG", session.GetAccountId(),GetGUID());
        return;
    }
    uint32 rInt = (iFlag % 32);

    uint32 tutflag = session.GetTutorialInt(wInt);
    tutflag |= (1 << rInt);
    session.SetTutorialInt(wInt, tutflag);

    // DEBUG_LOG("Received Tutorial Flag Set {%u}.", iFlag);
}

/**
 * @brief Sets all tutorial flags to completed for the account.
 *
 * @param recv_data The received opcode packet.
 */
void CharacterHandlers::HandleTutorialClear(WorldSession& session, WorldPacket & /*recv_data*/)
{
    for (int i = 0; i < 8; ++i)
    {
        session.SetTutorialInt(i, 0xFFFFFFFF);
    }
}

/**
 * @brief Resets all tutorial flags for the account.
 *
 * @param recv_data The received opcode packet.
 */
void CharacterHandlers::HandleTutorialReset(WorldSession& session, WorldPacket & /*recv_data*/)
{
    for (int i = 0; i < 8; ++i)
    {
        session.SetTutorialInt(i, 0x00000000);
    }
}

/**
 * @brief Sets the watched faction shown in the reputation UI.
 *
 * @param recv_data The received opcode packet.
 */
void CharacterHandlers::HandleSetWatchedFaction(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_SET_WATCHED_FACTION");
    int32 repId;
    recv_data >> repId;
    session.GetPlayer()->SetInt32Value(PLAYER_FIELD_WATCHED_FACTION_INDEX, repId);
}

/**
 * @brief Toggles a faction's inactive state in the reputation list.
 *
 * @param recv_data The received opcode packet.
 */
void CharacterHandlers::HandleSetFactionInactive(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_SET_FACTION_INACTIVE");
    uint32 replistid;
    uint8 inactive;
    recv_data >> replistid >> inactive;

    session.GetPlayer()->GetReputationMgr().SetInactive(replistid, inactive);
}

/**
 * @brief Toggles the player's helm visibility flag.
 *
 * @param recv_data The received opcode packet.
 */
void CharacterHandlers::HandleShowingHelm(WorldSession& session, WorldPacket & /*recv_data*/)
{
    DEBUG_LOG("CMSG_SHOWING_HELM for %s", session.GetPlayer()->GetName());
    session.GetPlayer()->ToggleFlag(PLAYER_FLAGS, PLAYER_FLAGS_HIDE_HELM);
}

/**
 * @brief Toggles the player's cloak visibility flag.
 *
 * @param recv_data The received opcode packet.
 */
void CharacterHandlers::HandleShowingCloak(WorldSession& session, WorldPacket & /*recv_data*/)
{
    DEBUG_LOG("CMSG_SHOWING_CLOAK for %s", session.GetPlayer()->GetName());
    session.GetPlayer()->ToggleFlag(PLAYER_FLAGS, PLAYER_FLAGS_HIDE_CLOAK);
}
