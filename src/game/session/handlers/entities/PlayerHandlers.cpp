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

#include "session/handlers/entities/PlayerHandlers.h"

#include "Platform/Define.h"
#include <memory>
#include "data/Language.h"
#include "WorldPacket.h"
#include "Opcodes.h"
#include "Log.h"
#include "AchievementMgr.h"
#include "Player.h"
#include "PlayerRegistry.h"
#include "data/GameConfig.h"
#include "CinematicFlyover.h"
#include "CUFProfile.h"
#include "GuildMgr.h"
#include "Guild.h"
#include "ObjectMgr.h"
#include "ObjectLookup.h"
#include "DBCStores.h"
#include "session/packets/chat/SystemMessage.h"
#include "WorldSession.h"

/**
 * @brief Sets the player's selected object guid.
 *
 * @param recv_data The received opcode packet.
 */
void PlayerHandlers::HandleSetSelection(WorldSession& session, WorldPacket& recv_data)
{
    ObjectGuid guid;
    recv_data >> guid;

    session.GetPlayer()->SetSelectionGuid(guid);

    // update reputation list if need
    Unit* unit = ObjectLookup::GetUnit(*session.GetPlayer(), guid);   // can select group members at diff maps
    if (!unit)
    {
        return;
    }

    if (FactionTemplateEntry const* factionTemplateEntry = sFactionTemplateStore.LookupEntry(unit->getFaction()))
    {
        session.GetPlayer()->SetFactionVisible(factionTemplateEntry);
    }
}

/**
 * @brief Changes the player's stand state animation.
 *
 * @param recv_data The received opcode packet.
 */
void PlayerHandlers::HandleStandStateChange(WorldSession& session, WorldPacket& recv_data)
{
    // DEBUG_LOG("WORLD: Received opcode CMSG_STANDSTATECHANGE"); -- too many spam in log at lags/debug stop
    uint32 animstate;
    recv_data >> animstate;

    session.GetPlayer()->SetStandState(animstate);
}

/**
 * @brief Updates one action button assignment.
 *
 * @param recv_data The received opcode packet.
 */
void PlayerHandlers::HandleSetActionButton(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_SET_ACTION_BUTTON");
    uint8 button;
    uint32 packetData;
    recv_data >> button >> packetData;

    uint32 action = ACTION_BUTTON_ACTION(packetData);
    uint8  type   = ACTION_BUTTON_TYPE(packetData);

    DETAIL_LOG("BUTTON: %u ACTION: %u TYPE: %u", button, action, type);
    if (!packetData)
    {
        DETAIL_LOG("MISC: Remove action from button %u", button);
        session.GetPlayer()->removeActionButton(session.GetPlayer()->GetTalentMgr().ActiveSpec(), button);
    }
    else
    {
        switch (type)
        {
            case ACTION_BUTTON_MACRO:
            case ACTION_BUTTON_CMACRO:
                DETAIL_LOG("MISC: Added Macro %u into button %u", action, button);
                break;
            case ACTION_BUTTON_EQSET:
                DETAIL_LOG("MISC: Added EquipmentSet %u into button %u", action, button);
                break;
            case ACTION_BUTTON_SPELL:
                DETAIL_LOG("MISC: Added Spell %u into button %u", action, button);
                break;
            case ACTION_BUTTON_ITEM:
                DETAIL_LOG("MISC: Added Item %u into button %u", action, button);
                break;
            case ACTION_BUTTON_EXPANDABLE:
                DETAIL_LOG("MISC: Added Expandable action %u into button %u", action, button);
                break;
            default:
                sLog.outError("MISC: Unknown action button type %u for action %u into button %u", type, action, button);
                return;
        }
        session.GetPlayer()->addActionButton(session.GetPlayer()->GetTalentMgr().ActiveSpec(), button, action, type);
    }
}

/**
 * @brief Updates the player's action-bar toggle byte.
 *
 * @param recv_data The received opcode packet.
 */
void PlayerHandlers::HandleSetActionBarToggles(WorldSession& session, WorldPacket& recv_data)
{
    uint8 ActionBar;

    recv_data >> ActionBar;

    if (!session.GetPlayer())                                       // ignore until not logged (check needed because STATUS_AUTHED)
    {
        if (ActionBar != 0)
        {
            sLog.outError("PlayerHandlers::HandleSetActionBarToggles in not logged state with value: %u, ignored", uint32(ActionBar));
        }
        return;
    }

    session.GetPlayer()->SetByteValue(PLAYER_FIELD_BYTES, 2, ActionBar);
}

void PlayerHandlers::HandleSetTitle(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_SET_TITLE");

    int32 title;
    recv_data >> title;

    // -1 at none
    if (title > 0 && title < MAX_TITLE_INDEX)
    {
        if (!session.GetPlayer()->HasTitle(title))
        {
            return;
        }
    }
    else
    {
        title = 0;
    }

    session.GetPlayer()->SetUInt32Value(PLAYER_CHOSEN_TITLE, title);
}

/**
 * @brief Cancels the player's mount aura when allowed.
 *
 * @param recv_data The received opcode packet.
 */
void PlayerHandlers::HandleCancelMountAura(WorldSession& session, WorldPacket& /*recv_data*/)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_CANCEL_MOUNT_AURA");

    // The flight first: a taxi passenger carries no UNIT_FLAG_MOUNT, so the mounted test below
    // would answer "not mounted" to one.
    if (session.GetPlayer()->IsTaxiFlying())                            // not blizz like; no any messages on blizz
    {
        SendSystemMessage(session, LANG_YOU_IN_FLIGHT);
        return;
    }

    // If player is not mounted, so go out :)
    if (!session.GetPlayer()->IsMounted())                              // not blizz like; no any messages on blizz
    {
        SendSystemMessage(session, LANG_CHAR_NON_MOUNTED);
        return;
    }

    session.GetPlayer()->Unmount(session.GetPlayer()->HasAuraType(SPELL_AURA_MOUNTED));
    session.GetPlayer()->RemoveSpellsCausingAura(SPELL_AURA_MOUNTED);
}

/**
 * @brief Enables or disables farsight camera mode.
 *
 * @param recv_data The received opcode packet.
 */
void PlayerHandlers::HandleFarSight(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_FAR_SIGHT");
    // recv_data.hexlike();

    uint8 op;
    recv_data >> op;

    WorldObject* obj = session.GetPlayer()->GetMap()->GetWorldObject(session.GetPlayer()->GetFarSightGuid());
    if (!obj)
    {
        return;
    }

    switch (op)
    {
        case 0:
            DEBUG_LOG("Removed FarSight from %s", session.GetPlayer()->GetGuidStr().c_str());
            session.GetPlayer()->GetCamera().ResetView(false);
            break;
        case 1:
            DEBUG_LOG("Added FarSight %s to %s", session.GetPlayer()->GetFarSightGuid().GetString().c_str(), session.GetPlayer()->GetGuidStr().c_str());
            session.GetPlayer()->GetCamera().SetView(obj, false);
            break;
    }
}

/**
 * @brief Sends the player's total and current-level played time.
 *
 * @param recv_data The received opcode packet.
 */
void PlayerHandlers::HandlePlayedTime(WorldSession& session, WorldPacket& recv_data)
{
    uint8 unk1;
    recv_data >> unk1;                                      // 0 or 1 expected

    WorldPacket data(SMSG_PLAYED_TIME, 4 + 4 + 1);
    data << uint32(session.GetPlayer()->GetTotalPlayedTime());
    data << uint32(session.GetPlayer()->GetLevelPlayedTime());
    data << uint8(unk1);                                    // 0 - will not show in chat frame
    session.SendPacket(&data);
}

/**
 * @brief Stores raid-frame profiles the client edited.
 *
 * @param recv_data The received opcode packet.
 */
void PlayerHandlers::HandleSaveCUFProfiles(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_SAVE_CUF_PROFILES");

    Player* player = session.GetPlayer();
    if (!player)
    {
        recv_data.rfinish();
        return;
    }

    uint8 count = uint8(recv_data.ReadBits(20));
    if (count > MAX_CUF_PROFILES)
    {
        sLog.outError("PlayerHandlers::HandleSaveCUFProfiles: %s sent %u profiles, max is %u",
                      player->GetName(), count, MAX_CUF_PROFILES);
        recv_data.rfinish();
        return;
    }

    std::unique_ptr<CUFProfile> profiles[MAX_CUF_PROFILES];
    uint8 nameLengths[MAX_CUF_PROFILES];

    // the read order differs from the write order -- both are the client's
    for (uint8 i = 0; i < count; ++i)
    {
        profiles[i].reset(new CUFProfile());
        profiles[i]->BoolOptions.set(CUF_AUTO_ACTIVATE_SPEC_2, recv_data.ReadBit());
        profiles[i]->BoolOptions.set(CUF_AUTO_ACTIVATE_10_PLAYERS, recv_data.ReadBit());
        profiles[i]->BoolOptions.set(CUF_UNK_157, recv_data.ReadBit());
        profiles[i]->BoolOptions.set(CUF_DISPLAY_HEAL_PREDICTION, recv_data.ReadBit());
        profiles[i]->BoolOptions.set(CUF_AUTO_ACTIVATE_SPEC_1, recv_data.ReadBit());
        profiles[i]->BoolOptions.set(CUF_AUTO_ACTIVATE_PVP, recv_data.ReadBit());
        profiles[i]->BoolOptions.set(CUF_DISPLAY_POWER_BAR, recv_data.ReadBit());
        profiles[i]->BoolOptions.set(CUF_AUTO_ACTIVATE_15_PLAYERS, recv_data.ReadBit());
        profiles[i]->BoolOptions.set(CUF_AUTO_ACTIVATE_40_PLAYERS, recv_data.ReadBit());
        profiles[i]->BoolOptions.set(CUF_DISPLAY_PETS, recv_data.ReadBit());
        profiles[i]->BoolOptions.set(CUF_AUTO_ACTIVATE_5_PLAYERS, recv_data.ReadBit());
        profiles[i]->BoolOptions.set(CUF_DISPLAY_ONLY_DISPELLABLE_DEBUFFS, recv_data.ReadBit());
        profiles[i]->BoolOptions.set(CUF_AUTO_ACTIVATE_2_PLAYERS, recv_data.ReadBit());
        profiles[i]->BoolOptions.set(CUF_UNK_156, recv_data.ReadBit());
        profiles[i]->BoolOptions.set(CUF_DISPLAY_NON_BOSS_DEBUFFS, recv_data.ReadBit());
        profiles[i]->BoolOptions.set(CUF_DISPLAY_MAIN_TANK_AND_ASSIST, recv_data.ReadBit());
        profiles[i]->BoolOptions.set(CUF_DISPLAY_AGGRO_HIGHLIGHT, recv_data.ReadBit());
        profiles[i]->BoolOptions.set(CUF_AUTO_ACTIVATE_3_PLAYERS, recv_data.ReadBit());
        profiles[i]->BoolOptions.set(CUF_DISPLAY_BORDER, recv_data.ReadBit());
        profiles[i]->BoolOptions.set(CUF_USE_CLASS_COLORS, recv_data.ReadBit());
        profiles[i]->BoolOptions.set(CUF_UNK_145, recv_data.ReadBit());
        nameLengths[i] = uint8(recv_data.ReadBits(8));
        profiles[i]->BoolOptions.set(CUF_AUTO_ACTIVATE_PVE, recv_data.ReadBit());
        profiles[i]->BoolOptions.set(CUF_DISPLAY_HORIZONTAL_GROUPS, recv_data.ReadBit());
        profiles[i]->BoolOptions.set(CUF_AUTO_ACTIVATE_25_PLAYERS, recv_data.ReadBit());
        profiles[i]->BoolOptions.set(CUF_KEEP_GROUPS_TOGETHER, recv_data.ReadBit());
    }

    for (uint8 i = 0; i < count; ++i)
    {
        recv_data >> profiles[i]->TopPoint;
        profiles[i]->ProfileName = recv_data.ReadString(nameLengths[i]);
        recv_data >> profiles[i]->BottomOffset;
        recv_data >> profiles[i]->FrameHeight;
        recv_data >> profiles[i]->FrameWidth;
        recv_data >> profiles[i]->TopOffset;
        recv_data >> profiles[i]->HealthText;
        recv_data >> profiles[i]->BottomPoint;
        recv_data >> profiles[i]->SortBy;
        recv_data >> profiles[i]->LeftOffset;
        recv_data >> profiles[i]->LeftPoint;

        player->SaveCUFProfile(i, std::move(profiles[i]));
    }

    // slots the client no longer uses
    for (uint8 i = count; i < MAX_CUF_PROFILES; ++i)
    {
        player->SaveCUFProfile(i, std::unique_ptr<CUFProfile>());
    }
}

/**
 * @brief Acknowledges cinematic completion.
 *
 * @param recv_data The received opcode packet.
 */
void PlayerHandlers::HandleCompleteCinematic(WorldSession& session, WorldPacket & /*recv_data*/)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_COMPLETE_CINEMATIC");

    // Stop cinematic flyover if present; DK may hold an early
    // visibility lease before the flyover becomes active.
    Player* player = session.GetPlayer();
    if (!player)
    {
        return;
    }

    if (CinematicFlyover* flyover = player->GetCinematicFlyover())
    {
        flyover->Stop();
    }

    // The flyover has stopped, so DK intro-deferred state can now be applied (the
    // player is in-world at the intro spawn): area-exploration discovery/XP and
    // the hostile-area PvP flag. Both are no-ops for races.
    player->CheckAreaExploreAndOutdoor();
    player->ApplyDeferredIntroPvP();
}

/**
 * @brief Advances the client's cinematic camera.
 *
 * @param recv_data The received opcode packet.
 */
void PlayerHandlers::HandleNextCinematicCamera(WorldSession& session, WorldPacket & /*recv_data*/)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_NEXT_CINEMATIC_CAMERA");

    // The client sends this when it enters the cinematic. Begin the flyover now
    // (summon body + bind camera) so farsight binds in sync with the client's
    // cinematic rather than during the login control window. Begin() is guarded.
    Player* player = session.GetPlayer();
    if (!player)
    {
        return;
    }

    if (CinematicFlyover* flyover = player->GetCinematicFlyover())
    {
        flyover->Begin();
    }
}

/**
 * @brief Begins inspecting another nearby friendly player.
 *
 * @param recv_data The received opcode packet.
 */
void PlayerHandlers::HandleInspect(WorldSession& session, WorldPacket& recv_data)
{
    ObjectGuid guid;
    recv_data >> guid;
    DEBUG_LOG("Inspected guid is %s", guid.GetString().c_str());

    Player* plr = sPlayerRegistry.Find(guid);
    if (!plr)                                               // wrong player
    {
        return;
    }

    if (!InReach(*session.GetPlayer(), *plr, INSPECT_DISTANCE, false))
    {
        return;
    }

    if (session.GetPlayer()->IsHostileTo(plr))
    {
        return;
    }

    WorldPacket data(SMSG_INSPECT_RESULTS, 50);
    data << plr->GetObjectGuid();

    if (sGameConfig.getConfig(CONFIG_BOOL_TALENTS_INSPECTING) || session.GetPlayer()->isGameMaster())
    {
        plr->BuildPlayerTalentsInfoData(&data);
    }
    else
    {
        data << uint32(0);                                  // unspentTalentPoints
        data << uint8(0);                                   // talentGroupCount
        data << uint8(0);                                   // talentGroupIndex
    }

    plr->BuildEnchantmentsInfoData(&data);
    if (Guild* guild = sGuildMgr.GetGuildById(plr->GetGuildId()))
    {
        data << guild->GetObjectGuid();
        data << uint32(guild->GetLevel());
        data << uint64(0/*guild->GetXP()*/);
        data << uint32(guild->GetMemberSize());             // number of members
    }

    session.SendPacket(&data);
}

/**
 * @brief Sends honor statistics for an inspected player.
 *
 * @param recv_data The received opcode packet.
 */
void PlayerHandlers::HandleInspectHonorStats(WorldSession& session, WorldPacket& recv_data)
{
    ObjectGuid guid;
    recv_data.ReadGuidMask<1, 5, 7, 3, 2, 4, 0, 6>(guid);
    recv_data.ReadGuidBytes<4, 7, 0, 5, 1, 6, 2, 3>(guid);

    Player* player = sPlayerRegistry.Find(guid);
    if (!player)
    {
        sLog.outError("PlayerHandlers::HandleInspectHonorStats: player not found");
        return;
    }

    if (!InReach(*session.GetPlayer(), *player, INSPECT_DISTANCE, false))
    {
        return;
    }

    if (session.GetPlayer()->IsHostileTo(player))
    {
        return;
    }

    WorldPacket data(SMSG_INSPECT_HONOR_STATS, 18);
    data.WriteGuidMask<4, 3, 6, 2, 5, 0, 7, 1>(player->GetObjectGuid());
    data << uint8(0);                                                   // rank
    data << uint16(player->GetUInt16Value(PLAYER_FIELD_KILLS, 1));      // yesterday kills
    data << uint16(player->GetUInt16Value(PLAYER_FIELD_KILLS, 0));      // today kills
    data.WriteGuidBytes<2, 0, 6, 3, 4, 1, 5>(player->GetObjectGuid());
    data << uint32(player->GetUInt32Value(PLAYER_FIELD_LIFETIME_HONORABLE_KILLS));
    data.WriteGuidBytes<7>(player->GetObjectGuid());

    session.SendPacket(&data);
}

void PlayerHandlers::HandleQueryInspectAchievements(WorldSession& session, WorldPacket& recv_data)
{
    ObjectGuid guid;

    recv_data >> guid.ReadAsPacked();

    Player* player = sPlayerRegistry.Find(guid);
    if (!player)
    {
        return;
    }

    if (!InReach(*session.GetPlayer(), *player, INSPECT_DISTANCE, false))
    {
        return;
    }

    if (session.GetPlayer()->IsHostileTo(player))
    {
        return;
    }

    player->GetAchievementMgr().SendRespondInspectAchievements(session.GetPlayer());
}
