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

#include "session/handlers/social/GroupHandlers.h"

#include "Platform/Define.h"
#include <string>
#include <cstring>
#include "Database/DatabaseEnv.h"
#include "Opcodes.h"
#include "Log.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "data/GameConfig.h"
#include "ObjectMgr.h"
#include "social/GroupRegistry.h"
#include "Pet.h"
#include "AchievementMgr.h"
#include "Player.h"
#include "spells/auras/SpellAuras.h"
#include "Group.h"
#include "LFGMgr.h"                                         // LFGRoles: PLAYER_ROLE_*
#include "SocialMgr.h"
#include "Util.h"
#include "DB2Structure.h"
#include "DB2Stores.h"
#include "Vehicle.h"
#include "Vehicle.h"
#include "PlayerRegistry.h"
#include "session/packets/social/GroupPackets.h"

/**
 * @brief Handles a request to invite a player into a party.
 *
 * @param recv_data The received opcode packet.
 */
void GroupHandlers::HandleGroupInvite(WorldSession& session, WorldPacket& recv_data)
{
    ObjectGuid guid;

    recv_data.read_skip<uint32>();      // cross-realm party related
    recv_data.read_skip<uint32>();      // roles mask?

    recv_data.ReadGuidMask<2, 7>(guid);
    uint32 realmLength = recv_data.ReadBits(9);
    recv_data.ReadGuidMask<3>(guid);
    uint32 nameLength = recv_data.ReadBits(10);
    recv_data.ReadGuidMask<5, 4, 6, 0, 1>(guid);

    recv_data.ReadGuidBytes<4, 7, 6>(guid);

    std::string membername = recv_data.ReadString(nameLength);
    std::string realmname = recv_data.ReadString(realmLength);

    // attempt add selected player

    // cheating
    if (!normalizePlayerName(membername))
    {
        session.SendPartyResult(PARTY_OP_INVITE, membername, ERR_BAD_PLAYER_NAME_S);
        return;
    }

    Player* player = sPlayerRegistry.FindByName(membername.c_str());

    // no player or cheat self-invite
    if (!player || player == session.GetPlayer())
    {
        session.SendPartyResult(PARTY_OP_INVITE, membername, ERR_BAD_PLAYER_NAME_S);
        return;
    }

    // can't group with
    if (!sGameConfig.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_GROUP) && session.GetPlayer()->GetTeam() != player->GetTeam())
    {
        session.SendPartyResult(PARTY_OP_INVITE, membername, ERR_PLAYER_WRONG_FACTION);
        return;
    }

    if (session.GetPlayer()->GetInstanceId() != 0 && player->GetInstanceId() != 0 && session.GetPlayer()->GetInstanceId() != player->GetInstanceId() && session.GetPlayer()->GetMapId() == player->GetMapId())
    {
        session.SendPartyResult(PARTY_OP_INVITE, membername, ERR_TARGET_NOT_IN_INSTANCE_S);
        return;
    }

    // just ignore us
    if (player->GetSocial()->HasIgnore(session.GetPlayer()->GetObjectGuid()))
    {
        session.SendPartyResult(PARTY_OP_INVITE, membername, ERR_IGNORING_YOU_S);
        return;
    }

    Group* group = session.GetPlayer()->GetGroup();
    if (group && group->isBGGroup())
    {
        group = session.GetPlayer()->GetOriginalGroup();
    }

    if (group && group->isRaidGroup() && !player->GetAllowLowLevelRaid() && (player->getLevel() < sGameConfig.getConfig(CONFIG_UINT32_MIN_LEVEL_FOR_RAID)))
    {
        session.SendPartyResult(PARTY_OP_INVITE, "", ERR_RAID_DISALLOWED_BY_LEVEL);
        return;
    }

    // player already invited
    if (player->GetGroupInvite())
    {
        session.SendPartyResult(PARTY_OP_INVITE, membername, ERR_ALREADY_IN_GROUP_S);
        return;
    }

    Group* group2 = player->GetGroup();
    if (group2 && group2->isBGGroup())
    {
        group2 = player->GetOriginalGroup();
    }

    // player already in another group
    if (group2)
    {
        session.SendPartyResult(PARTY_OP_INVITE, membername, ERR_ALREADY_IN_GROUP_S);

        // tell the player that they were invited but it failed as they were already in a group
        SessionOf(*player)->SendGroupInvite(player, true);

        return;
    }

    if (group)
    {
        // not have permissions for invite
        if (!group->IsLeader(session.GetPlayer()->GetObjectGuid()) && !group->IsAssistant(session.GetPlayer()->GetObjectGuid()))
        {
            session.SendPartyResult(PARTY_OP_INVITE, "", ERR_NOT_LEADER);
            return;
        }
        // not have place
        if (group->IsFull())
        {
            session.SendPartyResult(PARTY_OP_INVITE, "", ERR_GROUP_FULL);
            return;
        }
    }

    // ok, but group not exist, start a new group
    // but don't create and save the group to the DB until
    // at least one person joins
    if (!group)
    {
        group = new Group;
        // new group: if can't add then delete
        if (!group->AddLeaderInvite(session.GetPlayer()))
        {
            delete group;
            return;
        }
        if (!group->AddInvite(player))
        {
            delete group;
            return;
        }
    }
    else
    {
        // already existing group: if can't add then just leave
        if (!group->AddInvite(player))
        {
            return;
        }
    }

    SessionOf(*player)->SendGroupInvite(session.GetPlayer());
    session.SendPartyResult(PARTY_OP_INVITE, membername, ERR_PARTY_RESULT_OK);
}

/**
 * @brief Accepts a pending group invite.
 *
 * @param recv_data The received opcode packet.
 */
void GroupHandlers::HandleGroupInviteResponse(WorldSession& session, WorldPacket& recv_data)
{
    bool unk = recv_data.ReadBit();
    bool accepted = recv_data.ReadBit();
    if (unk)
    {
        recv_data.read_skip<uint32>();
    }

    Group* group = session.GetPlayer()->GetGroupInvite();
    if (!group)
    {
        return;
    }

    if (accepted)
    {
        // remove in from invites in any case
        group->RemoveInvite(session.GetPlayer());

        if (group->GetLeaderGuid() == session.GetPlayer()->GetObjectGuid())
        {
            sLog.outError("HandleGroupInviteResponseOpcode: %s tried to accept an invite to his own group",
                          session.GetPlayer()->GetGuidStr().c_str());
            return;
        }

        /** error handling **/
        /********************/

        // not have place
        if (group->IsFull())
        {
            session.SendPartyResult(PARTY_OP_INVITE, "", ERR_GROUP_FULL);
            return;
        }

        Player* leader = sPlayerRegistry.Find(group->GetLeaderGuid());

        // forming a new group, create it
        if (!group->IsCreated())
        {
            if (leader)
            {
                group->RemoveInvite(leader);
            }
            if (group->Create(group->GetLeaderGuid(), group->GetLeaderName()))
            {
                sGroupRegistry.Add(group);
            }
            else
            {
                return;
            }
        }

        // everything is fine, do it, PLAYER'S GROUP IS SET IN ADDMEMBER!!!
        if (!group->AddMember(session.GetPlayer()->GetObjectGuid(), session.GetPlayer()->GetName()))
        {
            return;
        }
    }
    else
    {
        // uninvite, group can be deleted
        session.GetPlayer()->UninviteFromGroup();

        // remember leader if online
        Player* leader = sPlayerRegistry.Find(group->GetLeaderGuid());
        if (!leader || !leader->GetSession())
        {
            return;
        }

        // report
        WorldPacket data(SMSG_GROUP_DECLINE, 10);               // guess size
        data << session.GetPlayer()->GetName();
        leader->GetSession()->SendPacket(&data);
    }
}

/**
 * @brief Uninvites a group member or invitee by guid, or starts a vote kick
 *        when the target's group is an LFD group.
 *
 * @param recv_data The received opcode packet.
 */
void GroupHandlers::HandleGroupUninviteGuid(WorldSession& session, WorldPacket& recv_data)
{
    ObjectGuid guid;
    std::string reason;
    recv_data >> guid;
    recv_data >> reason;

    // can't uninvite yourself
    if (guid == session.GetPlayer()->GetObjectGuid())
    {
        sLog.outError("GroupHandlers::HandleGroupUninviteGuid: leader %s tried to uninvite himself from the group.", session.GetPlayer()->GetGuidStr().c_str());
        return;
    }

    // The client caps the reason at 64 bytes on copy (spec section 1.1); a
    // modified client must not be able to push a longer string into the
    // group broadcast.
    trim(reason);
    if (reason.size() > 64)
    {
        reason.resize(64);
    }

    Group* grp = session.GetPlayer()->GetGroup();

    // Resolved from member slots (not sObjectMgr.GetPlayer) so it works for
    // an offline member -- needed below regardless of eligibility outcome.
    std::string targetName;
    if (grp)
    {
        for (Group::MemberSlot const& slot : grp->GetMemberSlots())
        {
            if (slot.guid == guid)
            {
                targetName = slot.name;
                break;
            }
        }
    }

    PartyResult res = session.GetPlayer()->CanUninviteFromGroup(guid);
    if (res != ERR_PARTY_RESULT_OK)
    {
        session.SendPartyResult(PARTY_OP_LEAVE, targetName, res);
        return;
    }

    if (!grp)
    {
        return;
    }

    if (grp->IsMember(guid))
    {
        if (grp->isLFGGroup() && reason.empty())
        {
            // Opens the client's "Please enter the reason to vote kick"
            // box (VOTE_KICK_REASON_NEEDED, spec section 1.1).
            session.SendPartyResult(PARTY_OP_LEAVE, targetName,
                            ERR_VOTE_KICK_REASON_NEEDED, 0, guid);
            return;
        }

        Player::RemoveFromGroup(grp, guid, session.GetPlayer()->GetObjectGuid(), reason);
        return;
    }

    if (Player* plr = grp->GetInvited(guid))
    {
        plr->UninviteFromGroup();
        return;
    }

    session.SendPartyResult(PARTY_OP_LEAVE, targetName, ERR_TARGET_NOT_IN_GROUP_S);
}

/**
 * @brief Changes the leader of the current group.
 *
 * @param recv_data The received opcode packet.
 */
void GroupHandlers::HandleGroupSetLeader(WorldSession& session, WorldPacket& recv_data)
{
    ObjectGuid guid;
    recv_data >> guid;

    Group* group = session.GetPlayer()->GetGroup();
    if (!group)
    {
        return;
    }

    Player* player = sPlayerRegistry.Find(guid);

    /** error handling **/
    if (!player || !group->IsLeader(session.GetPlayer()->GetObjectGuid()) || player->GetGroup() != group)
    {
        return;
    }
    /********************/

    // everything is fine, do it
    group->ChangeLeader(guid);
}

/**
 * @brief Handles a request to leave or disband the current group.
 *
 * @param recv_data The received opcode packet.
 */
void GroupHandlers::HandleGroupDisband(WorldSession& session, WorldPacket& /*recv_data*/)
{
    if (!session.GetPlayer()->GetGroup())
    {
        return;
    }

    if (session.GetPlayer()->InBattleGround())
    {
        session.SendPartyResult(PARTY_OP_INVITE, "", ERR_INVITE_RESTRICTED);
        return;
    }

    /** error handling **/
    /********************/

    // everything is fine, do it
    session.SendPartyResult(PARTY_OP_LEAVE, session.GetPlayer()->GetName(), ERR_PARTY_RESULT_OK);

    session.GetPlayer()->RemoveFromGroup();
}

/**
 * @brief Updates the group's loot rules.
 *
 * @param recv_data The received opcode packet.
 */
void GroupHandlers::HandleLootMethod(WorldSession& session, WorldPacket& recv_data)
{
    uint32 lootMethod;
    ObjectGuid lootMaster;
    uint32 lootThreshold;
    recv_data >> lootMethod >> lootMaster >> lootThreshold;

    Group* group = session.GetPlayer()->GetGroup();
    if (!group)
    {
        return;
    }

    /** error handling **/
    if (!group->IsLeader(session.GetPlayer()->GetObjectGuid()))
    {
        return;
    }

    // A dungeon-finder group keeps the loot rules it was formed with.
    if (group->isLFGGroup())
    {
        return;
    }
    /********************/

    // everything is fine, do it
    group->SetLootMethod((LootMethod)lootMethod);
    group->SetLooterGuid(lootMaster);
    group->SetLootThreshold((ItemQualities)lootThreshold);
    group->SendUpdate();
}

/**
 * @brief Handles a player's loot roll choice.
 *
 * @param recv_data The received opcode packet.
 */
void GroupHandlers::HandleLootRoll(WorldSession& session, WorldPacket& recv_data)
{
    ObjectGuid lootedTarget;
    uint32 itemSlot;
    uint8  rollType;
    recv_data >> lootedTarget;                              // guid of the item rolled
    recv_data >> itemSlot;
    recv_data >> rollType;

    // DEBUG_LOG("WORLD RECIEVE CMSG_LOOT_ROLL, From:%u, Numberofplayers:%u, rollType:%u", (uint32)Guid, NumberOfPlayers, rollType);

    Group* group = session.GetPlayer()->GetGroup();
    if (!group)
    {
        return;
    }

    if (rollType >= MAX_ROLL_FROM_CLIENT)
    {
        return;
    }

    // everything is fine, do it, if false then some cheating problem found
    if (!group->CountRollVote(session.GetPlayer(), lootedTarget, itemSlot, RollVote(rollType)))
    {
        return;
    }

    switch (rollType)
    {
        case ROLL_NEED:
            session.GetPlayer()->GetAchievementMgr().UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_ROLL_NEED, 1);
            break;
        case ROLL_GREED:
        case ROLL_DISENCHANT:
            session.GetPlayer()->GetAchievementMgr().UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_ROLL_GREED, 1);
            break;
    }
}

/**
 * @brief Broadcasts a minimap ping to the player's group.
 *
 * @param recv_data The received opcode packet.
 */
void GroupHandlers::HandleMinimapPing(WorldSession& session, WorldPacket& recv_data)
{
    float x, y;
    recv_data >> x;
    recv_data >> y;

    if (!session.GetPlayer()->GetGroup())
    {
        return;
    }

    // DEBUG_LOG("Received opcode MSG_MINIMAP_PING X: %f, Y: %f", x, y);

    /** error handling **/
    /********************/

    // everything is fine, do it
    WorldPacket data(MSG_MINIMAP_PING, (8 + 4 + 4));
    data << session.GetPlayer()->GetObjectGuid();
    data << float(x);
    data << float(y);
    session.GetPlayer()->GetGroup()->BroadcastPacket(&data, true, -1, session.GetPlayer()->GetObjectGuid());
}

/**
 * @brief Rolls a random value and broadcasts it to the party if applicable.
 *
 * @param recv_data The received opcode packet.
 */
void GroupHandlers::HandleRandomRoll(WorldSession& session, WorldPacket& recv_data)
{
    uint32 minimum, maximum, roll;
    recv_data >> minimum;
    recv_data >> maximum;

    /** error handling **/
    if (minimum > maximum || maximum > 10000)               // < 32768 for urand call
    {
        return;
    }
    /********************/

    // everything is fine, do it
    roll = urand(minimum, maximum);

    // DEBUG_LOG("ROLL: MIN: %u, MAX: %u, ROLL: %u", minimum, maximum, roll);

    WorldPacket data(MSG_RANDOM_ROLL, 4 + 4 + 4 + 8);
    data << uint32(minimum);
    data << uint32(maximum);
    data << uint32(roll);
    data << session.GetPlayer()->GetObjectGuid();
    if (session.GetPlayer()->GetGroup())
    {
        session.GetPlayer()->GetGroup()->BroadcastPacket(&data, false);
    }
    else
    {
        session.SendPacket(&data);
    }
}

/**
 * @brief Handles raid target icon queries and updates.
 *
 * @param recv_data The received opcode packet.
 */
void GroupHandlers::HandleRaidTargetUpdate(WorldSession& session, WorldPacket& recv_data)
{
    uint8  x;
    recv_data >> x;

    Group* group = session.GetPlayer()->GetGroup();
    if (!group)
    {
        return;
    }

    /** error handling **/
    /********************/

    // everything is fine, do it
    if (x == 0xFF)                                          // target icon request
    {
        WorldPacket data;
        BuildTargetIconListPacket(data, *group);
        session.SendPacket(&data);
    }
    else                                                    // target icon update
    {
        if (group->isRaidGroup() &&
                !group->IsLeader(session.GetPlayer()->GetObjectGuid()) &&
                !group->IsAssistant(session.GetPlayer()->GetObjectGuid()))
            return;

        ObjectGuid guid;
        recv_data >> guid;
        group->SetTargetIcon(x, session.GetPlayer()->GetObjectGuid(), guid);
    }
}

/**
 * @brief Converts the current group between party and raid mode.
 *
 * @param recv_data The received opcode packet; carries the target mode.
 */
void GroupHandlers::HandleGroupRaidConvert(WorldSession& session, WorldPacket& recv_data)
{
    // Lua ConvertToRaid() sends 1, ConvertToParty() sends 0 on the same opcode
    uint8 toRaid;
    recv_data >> toRaid;

    Group* group = session.GetPlayer()->GetGroup();
    if (!group)
    {
        return;
    }

    // A dungeon-finder group cannot be converted; the client hides the option
    // behind HasLFGRestrictions, but that is presentation, not enforcement.
    if (session.GetPlayer()->InBattleGround() || group->isLFGGroup())
    {
        return;
    }

    /** error handling **/
    if (!group->IsLeader(session.GetPlayer()->GetObjectGuid()) || group->GetMembersCount() < 2)
    {
        return;
    }

    // a party cannot hold more than MAX_GROUP_SIZE players
    if (!toRaid && group->GetMembersCount() > MAX_GROUP_SIZE)
    {
        session.SendPartyResult(PARTY_OP_INVITE, "", ERR_GROUP_FULL);
        return;
    }
    /********************/

    // everything is fine, do it (is it 0 (PARTY_OP_INVITE) correct code)
    session.SendPartyResult(PARTY_OP_INVITE, "", ERR_PARTY_RESULT_OK);

    if (toRaid)
    {
        group->ConvertToRaid();
    }
    else
    {
        group->ConvertToParty();
    }
}

/**
 * Builds SMSG_ROLE_CHANGED_INFORM.
 *
 * Bit and byte order are transcribed from the client's own reader; the two GUIDs
 * interleave, so this cannot use the single-GUID WriteGuidMask helpers.
 */
static void BuildRoleChangedInform(WorldPacket& data, ObjectGuid from, ObjectGuid changed,
                                   uint32 oldRole, uint32 newRole)
{
    uint64 rawFrom = from.GetRawValue();
    uint64 rawChanged = changed.GetRawValue();
    uint8 const* f = (uint8 const*)&rawFrom;
    uint8 const* c = (uint8 const*)&rawChanged;

    data.WriteBit(f[1]); data.WriteBit(c[0]); data.WriteBit(c[2]); data.WriteBit(c[4]);
    data.WriteBit(c[7]); data.WriteBit(c[3]); data.WriteBit(f[7]); data.WriteBit(c[5]);
    data.WriteBit(f[5]); data.WriteBit(f[4]); data.WriteBit(f[3]); data.WriteBit(c[6]);
    data.WriteBit(f[2]); data.WriteBit(f[6]); data.WriteBit(c[1]); data.WriteBit(f[0]);

    data.FlushBits();

    #define ROLE_SEQ(b) if (b) { data << uint8((b) ^ 1); }
    ROLE_SEQ(f[7]); ROLE_SEQ(c[3]); ROLE_SEQ(f[6]); ROLE_SEQ(c[4]); ROLE_SEQ(c[0]);
    data << uint32(newRole);
    ROLE_SEQ(c[6]); ROLE_SEQ(c[2]); ROLE_SEQ(f[0]); ROLE_SEQ(f[4]); ROLE_SEQ(c[1]);
    ROLE_SEQ(f[3]); ROLE_SEQ(f[5]); ROLE_SEQ(f[2]); ROLE_SEQ(c[5]); ROLE_SEQ(c[7]);
    ROLE_SEQ(f[1]);
    data << uint32(oldRole);
    #undef ROLE_SEQ
}

/**
 * @brief Records the role a player picked in the role-poll popup.
 *
 * @param recv_data The received opcode packet; uint32 role then a packed GUID.
 */
void GroupHandlers::HandleSetRole(WorldSession& session, WorldPacket& recv_data)
{
    uint32 role;
    recv_data >> role;

    ObjectGuid changedUnit;
    recv_data.ReadGuidMask<2, 6, 3, 7, 5, 1, 0, 4>(changedUnit);
    recv_data.ReadGuidBytes<6, 4, 1, 3, 0, 5, 2, 7>(changedUnit);

    // never trust the client with the spare bits
    role &= (PLAYER_ROLE_LEADER | PLAYER_ROLE_TANK | PLAYER_ROLE_HEALER | PLAYER_ROLE_DAMAGE);

    Group* group = session.GetPlayer()->GetGroup();
    if (!group)
    {
        return;
    }

    // only the leader/assistants may re-role someone else
    if (changedUnit != session.GetPlayer()->GetObjectGuid() &&
        !group->IsLeader(session.GetPlayer()->GetObjectGuid()) &&
        !group->IsAssistant(session.GetPlayer()->GetObjectGuid()))
    {
        return;
    }

    uint8 oldRole = group->GetLfgRoles(changedUnit);
    if (oldRole == uint8(role))
    {
        return;
    }

    WorldPacket data(SMSG_ROLE_CHANGED_INFORM, 8 + 8 + 4 + 4);
    BuildRoleChangedInform(data, session.GetPlayer()->GetObjectGuid(), changedUnit, oldRole, role);
    group->BroadcastPacket(&data, false);

    // roles also ride back to every client inside SMSG_GROUP_LIST
    group->SetLfgRoles(changedUnit, uint8(role));
}

/**
 * @brief Starts a role check; asks every member to re-pick their role.
 *
 * @param recv_data The received opcode packet; an empty body starts the poll.
 */
void GroupHandlers::HandleRolePollBegin(WorldSession& session, WorldPacket& recv_data)
{
    Group* group = session.GetPlayer()->GetGroup();
    if (!group)
    {
        return;
    }

    if (!recv_data.empty())
    {
        return;
    }

    ObjectGuid guid = session.GetPlayer()->GetObjectGuid();
    if (!group->IsLeader(guid) && !group->IsAssistant(guid))
    {
        return;
    }

    WorldPacket data(SMSG_ROLE_POLL_BEGIN, 8);
    data.WriteGuidMask<1, 5, 7, 3, 2, 4, 0, 6>(guid);
    data.WriteGuidBytes<4, 7, 0, 5, 1, 6, 2, 3>(guid);

    group->BroadcastPacket(&data, true);
}

/**
 * @brief Toggles the raid's "everyone is assistant" flag.
 *
 * @param recv_data The received opcode packet.
 */
void GroupHandlers::HandleSetEveryoneIsAssistant(WorldSession& session, WorldPacket& recv_data)
{
    bool apply = recv_data.ReadBit();

    Group* group = session.GetPlayer()->GetGroup();
    if (!group)
    {
        return;
    }

    if (!group->IsLeader(session.GetPlayer()->GetObjectGuid()))
    {
        return;
    }

    group->SetEveryoneIsAssistant(apply);
}

/**
 * @brief Takes down a raid world marker; the client sends an out-of-range slot
 *        to mean "remove them all".
 *
 * @param recv_data The received opcode packet.
 */
void GroupHandlers::HandleClearRaidMarker(WorldSession& session, WorldPacket& recv_data)
{
    uint8 marker;
    recv_data >> marker;

    Group* group = session.GetPlayer()->GetGroup();
    if (!group)
    {
        return;
    }

    // same gate as placing: restricted to leader/assistants in a raid, open to
    // any member of a party
    if (group->isRaidGroup() &&
        !group->IsLeader(session.GetPlayer()->GetObjectGuid()) &&
        !group->IsAssistant(session.GetPlayer()->GetObjectGuid()))
    {
        return;
    }

    group->ClearRaidMarker(marker);
}

/**
 * @brief Moves a raid member into another subgroup.
 *
 * @param recv_data The received opcode packet.
 */
void GroupHandlers::HandleGroupChangeSubGroup(WorldSession& session, WorldPacket& recv_data)
{
    std::string name;
    uint8 groupNr;
    recv_data >> name;

    recv_data >> groupNr;

    if (groupNr >= MAX_RAID_SUBGROUPS)
    {
        return;
    }

    // we will get correct pointer for group here, so we don't have to check if group is BG raid
    Group* group = session.GetPlayer()->GetGroup();
    if (!group)
    {
        return;
    }

    /** error handling **/
    if (!group->IsLeader(session.GetPlayer()->GetObjectGuid()) &&
            !group->IsAssistant(session.GetPlayer()->GetObjectGuid()))
        return;

    if (!group->HasFreeSlotSubGroup(groupNr))
    {
        return;
    }
    /********************/

    // everything is fine, do it
    if (Player* player = sPlayerRegistry.FindByName(name.c_str()))
    {
        group->ChangeMembersGroup(player, groupNr);
    }
    else
    {
        if (ObjectGuid guid = sObjectMgr.GetPlayerGuidByName(name.c_str()))
        {
            group->ChangeMembersGroup(guid, groupNr);
        }
    }
}

/**
 * @brief Sets or clears the assistant leader flag for a raid member.
 *
 * @param recv_data The received opcode packet.
 */
void GroupHandlers::HandleGroupAssistantLeader(WorldSession& session, WorldPacket& recv_data)
{
    ObjectGuid guid;
    uint8 flag;
    recv_data >> guid;
    recv_data >> flag;

    Group* group = session.GetPlayer()->GetGroup();
    if (!group)
    {
        return;
    }

    /** error handling **/
    if (!group->IsLeader(session.GetPlayer()->GetObjectGuid()))
    {
        return;
    }
    /********************/

    // everything is fine, do it
    group->SetAssistant(guid, (flag == 0 ? false : true));
}

/**
 * @brief Updates main tank or main assist raid assignments.
 *
 * @param recv_data The received opcode packet.
 */
void GroupHandlers::HandlePartyAssignment(WorldSession& session, WorldPacket& recv_data)
{
    uint8 role;
    uint8 apply;
    ObjectGuid guid;
    recv_data >> role >> apply;                             // role 0 = Main Tank, 1 = Main Assistant
    recv_data >> guid;

    DEBUG_LOG("MSG_PARTY_ASSIGNMENT");

    Group* group = session.GetPlayer()->GetGroup();
    if (!group)
    {
        return;
    }

    /** error handling **/
    if (!group->IsLeader(session.GetPlayer()->GetObjectGuid()))
    {
        return;
    }
    /********************/

    // everything is fine, do it
    if (apply)
    {
        switch (role)
        {
            case 0: group->SetMainTank(guid); break;
            case 1: group->SetMainAssistant(guid); break;
            default: break;
        }
    }
    else
    {
        if (group->GetMainTankGuid() == guid)
        {
            group->SetMainTank(ObjectGuid());
        }
        if (group->GetMainAssistantGuid() == guid)
        {
            group->SetMainAssistant(ObjectGuid());
        }
    }
}

/**
 * @brief Starts or answers a raid ready check.
 *
 * @param recv_data The received opcode packet.
 */
void GroupHandlers::HandleRaidReadyCheck(WorldSession& session, WorldPacket& recv_data)
{
    if (recv_data.empty())                                  // request
    {
        Group* group = session.GetPlayer()->GetGroup();
        if (!group)
        {
            return;
        }

        /** error handling **/
        if (!group->IsLeader(session.GetPlayer()->GetObjectGuid()) &&
                !group->IsAssistant(session.GetPlayer()->GetObjectGuid()))
            return;
        /********************/

        // everything is fine, do it
        WorldPacket data(MSG_RAID_READY_CHECK, 8);
        data << ObjectGuid(session.GetPlayer()->GetObjectGuid());
        group->BroadcastPacket(&data, false, -1);

        group->OfflineReadyCheck();
    }
    else                                                    // answer
    {
        uint8 state;
        recv_data >> state;

        Group* group = session.GetPlayer()->GetGroup();
        if (!group)
        {
            return;
        }

        // everything is fine, do it
        WorldPacket data(MSG_RAID_READY_CHECK_CONFIRM, 9);
        data << session.GetPlayer()->GetObjectGuid();
        data << uint8(state);
        group->BroadcastReadyCheck(&data);
    }
}

/**
 * @brief Handles the completion of a raid ready check.
 *
 * @param recv_data The received opcode packet.
 */
void GroupHandlers::HandleRaidReadyCheckFinished(WorldSession& /*session*/, WorldPacket& /*recv_data*/)
{
    // Group* group = session.GetPlayer()->GetGroup();
    // if (!group)
    //    return;

    // if (!group->IsLeader(session.GetPlayer()->GetGUID()) && !group->IsAssistant(session.GetPlayer()->GetGUID()))
    //    return;

    // Is any reaction need?
}

/*this procedure handles clients CMSG_REQUEST_PARTY_MEMBER_STATS request*/
void GroupHandlers::HandleRequestPartyMemberStats(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_REQUEST_PARTY_MEMBER_STATS");
    ObjectGuid guid;
    recv_data >> guid;

    Player* player = sPlayerRegistry.Find(guid, false);
    if (!player)
    {
        WorldPacket data(SMSG_PARTY_MEMBER_STATS_FULL, 3 + 4 + 2);
        data << uint8(0);                                   // only for SMSG_PARTY_MEMBER_STATS_FULL, probably arena/bg related
        data << guid.WriteAsPacked();
        data << uint32(GROUP_UPDATE_FLAG_STATUS);
        data << uint16(MEMBER_STATUS_OFFLINE);
        session.SendPacket(&data);
        return;
    }

    Pet* pet = player->GetPet();

    WorldPacket data(SMSG_PARTY_MEMBER_STATS_FULL, 4 + 2 + 2 + 2 + 1 + 2 * 6 + 8 + 1 + 8);
    data << uint8(0);                                       // only for SMSG_PARTY_MEMBER_STATS_FULL, probably arena/bg related
    data << player->GetPackGUID();

    uint32 mask1 = GROUP_UPDATE_FLAG_STATUS | GROUP_UPDATE_FLAG_CUR_HP | GROUP_UPDATE_FLAG_MAX_HP |
        GROUP_UPDATE_FLAG_POWER_TYPE | GROUP_UPDATE_FLAG_CUR_POWER | GROUP_UPDATE_FLAG_MAX_POWER |
        GROUP_UPDATE_FLAG_LEVEL | GROUP_UPDATE_FLAG_ZONE | GROUP_UPDATE_FLAG_POSITION |
        GROUP_UPDATE_FLAG_AURAS | GROUP_UPDATE_FLAG_PET_NAME | GROUP_UPDATE_FLAG_PET_AURAS |
        GROUP_UPDATE_FLAG_PHASE;

    if (pet)
    {
        mask1 = 0x7FEFFEFF; // full mask & ~(GROUP_UPDATE_FLAG_VEHICLE_SEAT | GROUP_UPDATE_FLAG_UNK)
    }

    Powers powerType = player->GetPowerType();
    data << uint32(mask1);                                  // group update mask
    data << uint16(MEMBER_STATUS_ONLINE);                   // member's online status
    data << uint32(player->GetHealth());                    // GROUP_UPDATE_FLAG_CUR_HP
    data << uint32(player->GetMaxHealth());                 // GROUP_UPDATE_FLAG_MAX_HP
    data << uint8(powerType);                               // GROUP_UPDATE_FLAG_POWER_TYPE
    data << uint16(player->GetPower(powerType));            // GROUP_UPDATE_FLAG_CUR_POWER
    data << uint16(player->GetMaxPower(powerType));         // GROUP_UPDATE_FLAG_MAX_POWER
    data << uint16(player->getLevel());                     // GROUP_UPDATE_FLAG_LEVEL

    // verify player coordinates and zoneid to send to teammates
    uint16 iZoneId = 0;
    uint16 iCoordX = 0;
    uint16 iCoordY = 0;
    uint16 iCoordZ = 0;

    if (player->IsInWorld())
    {
        iZoneId = player->GetTerrain()->GetZoneId(player->Where().X(), player->Where().Y(), player->Where().Z());
        iCoordX = player->Where().X();
        iCoordY = player->Where().Y();
        iCoordZ = player->Where().Z();
    }
    else if (player->IsBeingTeleported())               // Player is in teleportation
    {
        WorldLocation& loc = player->GetTeleportDest(); // So take teleportation destination
        iZoneId = sTerrainMgr.GetZoneId(loc.mapid, loc.coord_x, loc.coord_y, loc.coord_z);
        iCoordX = loc.coord_x;
        iCoordY = loc.coord_y;
        iCoordZ = loc.coord_z;
    }
    else
    {
        // unknown player status.
    }

    data << uint16(iZoneId);                              // GROUP_UPDATE_FLAG_ZONE
    data << uint16(iCoordX);                              // GROUP_UPDATE_FLAG_POSITION
    data << uint16(iCoordY);                              // GROUP_UPDATE_FLAG_POSITION
    data << uint16(iCoordZ);                              // GROUP_UPDATE_FLAG_POSITION

    data << uint8(1);                                     // if true, client clears all auras that are not in auramask and whose index is lower amount sent below
    uint64 auramask = 0;
    size_t maskPos = data.wpos();
    data << uint64(auramask);                             // placeholder, server sends 0xFFFFFFFFFFFFFFFF here, but with 1 above it seems no difference
    data << uint32(MAX_AURAS);                            // server sends here number of visible auras, but client checks
                                                          // if aura is in auramask, so it seems no difference if there will be MAX_AURAS
    for (uint8 i = 0; i < MAX_AURAS; ++i)
    {
        if (SpellAuraHolder* holder = player->GetVisibleAura(i))
        {
            auramask |= (uint64(1) << i);
            data << uint32(holder->GetId());
            data << uint16(holder->GetAuraFlags());
            if (holder->GetAuraFlags() & AFLAG_EFFECT_AMOUNT_SEND)
            {
                for (uint32 i = 0; i < MAX_EFFECT_INDEX; ++i)
                {
                    if (Aura* aura = holder->GetAuraByEffectIndex(SpellEffectIndex(i)))
                    {
                        data << int32(aura->GetModifier()->m_amount);
                    }
                    else
                    {
                        data << int32(0);
                    }
                }
            }
        }
    }
    data.put<uint64>(maskPos, auramask);                    // GROUP_UPDATE_FLAG_AURAS

    if (pet)
    {
        Powers petpowertype = pet->GetPowerType();
        data << pet->GetObjectGuid();                       // GROUP_UPDATE_FLAG_PET_GUID
        data << pet->GetName();                             // GROUP_UPDATE_FLAG_PET_NAME
        data << uint16(pet->GetDisplayId());                // GROUP_UPDATE_FLAG_PET_MODEL_ID
        data << uint32(pet->GetHealth());                   // GROUP_UPDATE_FLAG_PET_CUR_HP
        data << uint32(pet->GetMaxHealth());                // GROUP_UPDATE_FLAG_PET_MAX_HP
        data << uint8(petpowertype);                        // GROUP_UPDATE_FLAG_PET_POWER_TYPE
        data << uint16(pet->GetPower(petpowertype));        // GROUP_UPDATE_FLAG_PET_CUR_POWER
        data << uint16(pet->GetMaxPower(petpowertype));     // GROUP_UPDATE_FLAG_PET_MAX_POWER

        data << uint8(1);                                   // if true, client clears all auras that are not in auramask and whose index is lower amount sent below
        uint64 petauramask = 0;
        size_t petMaskPos = data.wpos();
        data << uint64(petauramask);                        // placeholder, server sends 0xFFFFFFFFFFFFFFFF here, but with 1 above it seems no difference
        data << uint32(MAX_AURAS);                          // server sends here number of visible auras, but client checks
                                                            // if aura is in auramask, so it seems no difference if there will be MAX_AURAS
        for (uint8 i = 0; i < MAX_AURAS; ++i)
        {
            if (SpellAuraHolder* holder = pet->GetVisibleAura(i))
            {
                petauramask |= (uint64(1) << i);
                data << uint32(holder->GetId());
                data << uint16(holder->GetAuraFlags());
                if (holder->GetAuraFlags() & AFLAG_EFFECT_AMOUNT_SEND)
                {
                    for (uint32 i = 0; i < MAX_EFFECT_INDEX; ++i)
                    {
                        if (Aura* aura = holder->GetAuraByEffectIndex(SpellEffectIndex(i)))
                        {
                            data << int32(aura->GetModifier()->m_amount);
                        }
                        else
                        {
                            data << int32(0);
                        }
                    }
                }
            }
        }
        data.put<uint64>(petMaskPos, petauramask);          // GROUP_UPDATE_FLAG_PET_AURAS
    }
    else
    {
        data << uint8(0);                                   // GROUP_UPDATE_FLAG_PET_NAME
        data << uint8(1);                                   // GROUP_UPDATE_FLAG_PET_AURAS
        data << uint64(0);                                  // GROUP_UPDATE_FLAG_PET_AURAS
        data << uint32(0);                                  // GROUP_UPDATE_FLAG_PET_AURAS
    }

    if (player->GetTransportInfo())                         // GROUP_UPDATE_FLAG_VEHICLE_SEAT
    {
        data << uint32(((Unit*)player->GetTransportInfo()->GetTransport())->GetVehicleInfo()->GetVehicleEntry()->SeatID[player->GetTransportInfo()->GetTransportSeat()]);
    }

    data << uint32(8);                                      // GROUP_UPDATE_FLAG_PHASE
    data << uint32(0);                                      // GROUP_UPDATE_FLAG_PHASE
    data << uint8(0);                                       // GROUP_UPDATE_FLAG_PHASE

    session.SendPacket(&data);
}

/**
 * @brief Sends the saved raid instance information to the client.
 *
 * @param recv_data The received opcode packet.
 */
void GroupHandlers::HandleRequestRaidInfo(WorldSession& session, WorldPacket& /*recv_data*/)
{
    // every time the player checks the character screen
    session.GetPlayer()->SendRaidInfo();
}

/**
 * @brief Handles the client's opt-out-of-loot setting.
 *
 * @param recv_data The received opcode packet.
 */
void GroupHandlers::HandleOptOutOfLoot(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_OPT_OUT_OF_LOOT");

    uint32 unkn;
    recv_data >> unkn;

    // ignore if player not loaded
    if (!session.GetPlayer())                                       // needed because STATUS_AUTHED
    {
        if (unkn != 0)
        {
            sLog.outError("CMSG_OPT_OUT_OF_LOOT value<>0 for not-loaded character!");
        }
        return;
    }

    // Set player's opt-out-of-loot preference
    session.GetPlayer()->SetOptOutOfLoot(unkn != 0);
}

void GroupHandlers::HandleSetAllowLowLevelRaid(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_LOW_LEVEL_RAID or CMSG_LOW_LEVEL_RAID_USER: %4X", recv_data.GetOpcode());

    uint8 allow;
    recv_data >> allow;

    session.GetPlayer()->SetAllowLowLevelRaid(allow);
}

void GroupHandlers::HandleGroupRequestJoinUpdates(WorldSession& session, WorldPacket& recv_data)
{
    Group* group = session.GetPlayer()->GetGroup();
    if (!group)
    {
        return;
    }

    WorldPacket data(SMSG_REAL_GROUP_UPDATE, 1 + 4 + 8);
    data << uint8(group->GetGroupType());
    data << uint32(group->GetMembersCount() - 1);
    data << ObjectGuid(group->GetLeaderGuid());
    session.SendPacket(&data);
}
