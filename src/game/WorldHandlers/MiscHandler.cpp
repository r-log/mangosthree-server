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
 * @file MiscHandler.cpp
 * @brief Miscellaneous opcode handlers
 */

#include <zlib.h>
#include "Common/ServerDefines.h"
#include "Platform/Define.h"
#include <string>
#include <vector>
#include <ctime>
#include "Language.h"
#include "Database/DatabaseEnv.h"
#include "WorldPacket.h"
#include "Opcodes.h"
#include "Log.h"
#include "AchievementMgr.h"
#include "Player.h"
#include "data/GameConfig.h"
#include "CinematicFlyover.h"
#include "GuildMgr.h"
#include "ObjectMgr.h"
#include "Corpse.h"
#include "DBCStores.h"
#include "WorldSession.h"
#include <memory>
#include "Auth/BigNumber.h"
#include "Auth/Sha1.h"
#include "UpdateData.h"
#include "LootMgr.h"
#include "data/ScriptHooks.h"
#include "zlib.h"
#include "PlayerRegistry.h"
#include "ObjectLookup.h"
#include "Object.h"
#include "BattleGround/BattleGround.h"
#include "OutdoorPvP/OutdoorPvP.h"
#include "Guild.h"
#include "Pet.h"
#include "SocialMgr.h"
#include "DBCEnums.h"
#include "persistence/characters/social/TicketRepository.h"
#include "persistence/login/AccountRepository.h"

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
    else if (GetPlayer()->m_movementInfo.HasMovementFlag(MovementFlags(MOVEFLAG_FALLING | MOVEFLAG_FALLINGFAR)))
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

void WorldSession::HandleMoveTimeSkippedOpcode(WorldPacket& recv_data)
{
    /*  WorldSession::Update( WorldTimer::getMSTime() );*/
    DEBUG_LOG("WORLD: Received opcode CMSG_MOVE_TIME_SKIPPED");

    ObjectGuid guid;

    recv_data >> Unused<uint32>();
    recv_data.ReadGuidMask<5, 1, 3, 7, 6, 0, 4, 2>(guid);
    recv_data.ReadGuidBytes<7, 1, 2, 4, 3, 6, 0, 5>(guid);

    /*
        ObjectGuid guid;
        uint32 time_skipped;
        recv_data >> guid;
        recv_data >> time_skipped;
        DEBUG_LOG("WORLD: Received opcode CMSG_MOVE_TIME_SKIPPED");

        /// TODO
        must be need use in mangos
        We substract server Lags to move time ( AntiLags )
        for exmaple
        {
            GetPlayer()->ModifyLastMoveTime( -int32(time_skipped) );
        }
    */
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

