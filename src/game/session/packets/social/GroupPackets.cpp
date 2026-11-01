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

#include "GroupPackets.h"
#include "Opcodes.h"
#include "Pet.h"
#include "Player.h"
#include "PlayerRegistry.h"
#include "Vehicle.h"
#include "WorldPacket.h"
#include "spells/auras/SpellAuras.h"

#include <cstring>

void BuildRaidMarkersChangedPacket(WorldPacket& packet, uint32 markerMask)
{
    packet.Initialize(SMSG_RAID_MARKERS_CHANGED, 4);
    packet << uint32(markerMask);
}

void BuildGroupSetLeaderPacket(WorldPacket& packet, std::string const& name)
{
    packet.Initialize(SMSG_GROUP_SET_LEADER, name.size() + 1);
    packet << name;
}

void BuildGroupUninvitePacket(WorldPacket& packet)
{
    packet.Initialize(SMSG_GROUP_UNINVITE, 0);
}

void BuildGroupDestroyedPacket(WorldPacket& packet)
{
    packet.Initialize(SMSG_GROUP_DESTROYED, 0);
}

void BuildEmptyGroupListPacket(WorldPacket& packet)
{
    packet.Initialize(SMSG_GROUP_LIST, 1 + 1 + 1 + 1 + 8 + 4 + 4 + 8);
    packet << uint8(0x10) << uint8(0) << uint8(0) << uint8(0);
    packet << uint64(0) << uint32(0) << uint32(0) << uint64(0);
}

void BuildGroupListPacket(WorldPacket& packet, Group const& group, Group::MemberSlot const& slot, uint8 lfgState, uint32 lfgDungeon, uint32 sequence)
{
    // guess size
    packet.Initialize(SMSG_GROUP_LIST, 1 + 1 + 1 + 1 + 8 + 4 + group.GetMembersCount() * 20);
    packet << uint8(group.GetGroupType());                  // group type (flags in 3.3)
    packet << uint8(slot.group);                            // groupid
    packet << uint8(group.GetFlags(slot));                  // group flags
    packet << uint8(slot.roles);                            // your own assigned role
    if (group.GetGroupType() & GROUPTYPE_LFD)
    {
        packet << uint8(lfgState);                          // MyLfgFlags; 2 = finished
        packet << uint32(lfgDungeon);                       // LfgSlot; concrete Entry()
        packet << uint8(0);                                 // LfgAborted; non-zero disables CanPartyLFGBackfill (sub_1406FB440)
    }
    packet << group.GetObjectGuid();                        // group guid
    packet << uint32(sequence);                             // roster version; client drops anything it has already seen
    packet << uint32(group.GetMembersCount() - 1);
    for (Group::member_citerator citr2 = group.GetMemberSlots().begin(); citr2 != group.GetMemberSlots().end(); ++citr2)
    {
        if (slot.guid == citr2->guid)
        {
            continue;
        }
        Player* member = sPlayerRegistry.Find(citr2->guid);
        uint8 onlineState = (member) ? MEMBER_STATUS_ONLINE : MEMBER_STATUS_OFFLINE;
        onlineState = onlineState | ((group.isBGGroup()) ? MEMBER_STATUS_PVP : 0);

        packet << citr2->name;
        packet << citr2->guid;
        packet << uint8(onlineState);                       // online-state
        packet << uint8(citr2->group);                      // groupid
        packet << uint8(group.GetFlags(*citr2));            // group flags
        packet << uint8(citr2->roles);                      // assigned role
    }

    packet << group.GetLeaderGuid();                        // leader guid
    if (group.GetMembersCount() - 1)
    {
        packet << uint8(group.GetLootMethod());             // loot method
        packet << group.GetLooterGuid();                    // looter guid
        packet << uint8(group.GetLootThreshold());          // loot threshold
        packet << uint8(group.GetDungeonDifficulty());      // Dungeon Difficulty
        packet << uint8(group.GetRaidDifficulty());         // Raid Difficulty
        packet << uint8(0);                                 // 3.3, dynamic difficulty?
    }
}

void BuildLootStartRollPacket(WorldPacket& packet, Roll const& roll, uint32 countDown, uint32 mapId, RollVoteMask mask)
{
    packet.Initialize(SMSG_LOOT_START_ROLL, 8 + 4 + 4 + 4 + 4 + 4 + 4 + 1);
    packet << roll.lootedTargetGUID;                        // creature guid what we're looting
    packet << uint32(mapId);                                // 3.3.3 mapid
    packet << uint32(roll.itemSlot);                        // item slot in loot
    packet << uint32(roll.itemid);                          // the itemEntryId for the item that shall be rolled for
    packet << uint32(roll.itemRandomSuffix);                // randomSuffix
    packet << uint32(roll.itemRandomPropId);                // item random property ID
    packet << uint32(roll.itemCount);                       // items in stack
    packet << uint32(countDown);                            // the countdown time to choose "need" or "greed"
    packet << uint8(mask);                                  // roll type mask, allowed choices
    packet << uint8(roll.totalPlayersRolling);
}

void BuildLootRollPacket(WorldPacket& packet, Roll const& roll, ObjectGuid target, uint32 rollNumber, uint8 rollType)
{
    packet.Initialize(SMSG_LOOT_ROLL, 8 + 4 + 8 + 4 + 4 + 4 + 1 + 1 + 1);
    packet << roll.lootedTargetGUID;                        // creature guid what we're looting
    packet << uint32(roll.itemSlot);                        // unknown, maybe amount of players, or item slot in loot
    packet << target;
    packet << uint32(roll.itemid);                          // the itemEntryId for the item that shall be rolled for
    packet << uint32(roll.itemRandomSuffix);                // randomSuffix
    packet << uint32(roll.itemRandomPropId);                // Item random property ID
    packet << uint32(rollNumber);                           // 0: "Need for: [item name]", -1: "you passed on: [item name]"      Roll number
    packet << uint8(rollType);                              // 0: "Need for: [item name]" 0: "You have selected need for [item name] 1: need roll 2: greed roll
    packet << uint8(0);                                     // auto pass on loot
}

void BuildLootRollWonPacket(WorldPacket& packet, Roll const& roll, ObjectGuid target, uint32 rollNumber, RollVote rollType)
{
    packet.Initialize(SMSG_LOOT_ROLL_WON, 8 + 4 + 4 + 4 + 4 + 8 + 1 + 1);
    packet << roll.lootedTargetGUID;                        // creature guid what we're looting
    packet << uint32(roll.itemSlot);                        // item slot in loot
    packet << uint32(roll.itemid);                          // the itemEntryId for the item that shall be rolled for
    packet << uint32(roll.itemRandomSuffix);                // randomSuffix
    packet << uint32(roll.itemRandomPropId);                // Item random property
    packet << target;                                       // guid of the player who won.
    packet << uint32(rollNumber);                           // rollnumber related to SMSG_LOOT_ROLL
    packet << uint8(rollType);                              // Rolltype related to SMSG_LOOT_ROLL
}

void BuildLootAllPassedPacket(WorldPacket& packet, Roll const& roll)
{
    packet.Initialize(SMSG_LOOT_ALL_PASSED, 8 + 4 + 4 + 4 + 4);
    packet << roll.lootedTargetGUID;                        // creature guid what we're looting
    packet << uint32(roll.itemSlot);                        // item slot in loot
    packet << uint32(roll.itemid);                          // The itemEntryId for the item that shall be rolled for
    packet << uint32(roll.itemRandomPropId);                // Item random property ID
    packet << uint32(roll.itemRandomSuffix);                // Item random suffix ID
}

void BuildMasterLootListPacket(WorldPacket& packet, std::vector<ObjectGuid> const& candidates)
{
    packet.Initialize(SMSG_LOOT_MASTER_LIST, 330);
    packet << uint8(candidates.size());
    for (std::vector<ObjectGuid>::const_iterator itr = candidates.begin(); itr != candidates.end(); ++itr)
    {
        packet << *itr;
    }
}

void BuildTargetIconSetPacket(WorldPacket& packet, ObjectGuid who, uint8 id, ObjectGuid target)
{
    packet.Initialize(MSG_RAID_TARGET_UPDATE, 1 + 8 + 1 + 8);
    packet << uint8(0);                                     // set targets
    packet << who;
    packet << uint8(id);
    packet << target;
}

void BuildTargetIconListPacket(WorldPacket& packet, Group const& group)
{
    packet.Initialize(MSG_RAID_TARGET_UPDATE, 1 + TARGET_ICON_COUNT * 9);
    packet << uint8(1);                                     // list targets

    for (int i = 0; i < TARGET_ICON_COUNT; ++i)
    {
        if (!group.GetTargetIcon(i))
        {
            continue;
        }

        packet << uint8(i);
        packet << group.GetTargetIcon(i);
    }
}

void BuildPartyMemberStatsChangedPacket(WorldPacket& packet, Player& player)
{
    uint32 mask = player.GetGroupUpdateFlag();

    if (mask & GROUP_UPDATE_FLAG_POWER_TYPE)                // if update power type, update current/max power also
    {
        mask |= (GROUP_UPDATE_FLAG_CUR_POWER | GROUP_UPDATE_FLAG_MAX_POWER);
    }

    if (mask & GROUP_UPDATE_FLAG_PET_POWER_TYPE)            // same for pets
    {
        mask |= (GROUP_UPDATE_FLAG_PET_CUR_POWER | GROUP_UPDATE_FLAG_PET_MAX_POWER);
    }

    uint32 byteCount = 0;
    for (int i = 1; i < GROUP_UPDATE_FLAGS_COUNT; ++i)
        if (mask & (1 << i))
        {
            byteCount += GroupUpdateLength[i];
        }

    packet.Initialize(SMSG_PARTY_MEMBER_STATS, 8 + 4 + byteCount);
    packet << player.GetPackGUID();
    packet << uint32(mask);

    if (mask & GROUP_UPDATE_FLAG_STATUS)
    {
        if (player.IsPvP())
        {
            packet << uint16(MEMBER_STATUS_ONLINE | MEMBER_STATUS_PVP);
        }
        else
        {
            packet << uint16(MEMBER_STATUS_ONLINE);
        }
    }

    if (mask & GROUP_UPDATE_FLAG_CUR_HP)
    {
        packet << uint32(player.GetHealth());
    }

    if (mask & GROUP_UPDATE_FLAG_MAX_HP)
    {
        packet << uint32(player.GetMaxHealth());
    }

    Powers powerType = player.GetPowerType();
    if (mask & GROUP_UPDATE_FLAG_POWER_TYPE)
    {
        packet << uint8(powerType);
    }

    if (mask & GROUP_UPDATE_FLAG_CUR_POWER)
    {
        packet << uint16(player.GetPower(powerType));
    }

    if (mask & GROUP_UPDATE_FLAG_MAX_POWER)
    {
        packet << uint16(player.GetMaxPower(powerType));
    }

    if (mask & GROUP_UPDATE_FLAG_LEVEL)
    {
        packet << uint16(player.getLevel());
    }

    if (mask & GROUP_UPDATE_FLAG_ZONE)
    {
        packet << uint16(player.GetTerrain()->GetZoneId(player.Where().X(), player.Where().Y(), player.Where().Z()));
    }

    if (mask & GROUP_UPDATE_FLAG_UNK)
    {
        packet << uint16(0);
    }

    if (mask & GROUP_UPDATE_FLAG_POSITION)
    {
        packet << uint16(player.Where().X()) << uint16(player.Where().Y()) << uint16(player.Where().Z());
    }

    if (mask & GROUP_UPDATE_FLAG_AURAS)
    {
        packet << uint8(0);                  // if true, client clears all auras that are not in auramask and whose index is lower amount sent below
        const uint64& auramask = player.GetAuraUpdateMask();
        packet << uint64(auramask);
        packet << uint32(MAX_AURAS);         // server sends here number of visible auras, but client checks
                                            // if aura is in auramask, so it seems no difference if there will be MAX_AURAS
        for (uint32 i = 0; i < MAX_AURAS; ++i)
        {
            if (auramask & (uint64(1) << i))
            {
                if (SpellAuraHolder* holder = player.GetVisibleAura(i))
                {
                    packet << uint32(holder->GetId());
                    packet << uint16(holder->GetAuraFlags());
                    if (holder->GetAuraFlags() & AFLAG_EFFECT_AMOUNT_SEND)
                    {
                        for (uint32 i = 0; i < MAX_EFFECT_INDEX; ++i)
                        {
                            if (Aura* aura = holder->GetAuraByEffectIndex(SpellEffectIndex(i)))
                            {
                                packet << int32(aura->GetModifier()->m_amount);
                            }
                            else
                            {
                                packet << int32(0);
                            }
                        }
                    }
                }
                else
                {
                    packet << uint32(0);
                    packet << uint16(0);
                }
            }
        }
    }

    Pet* pet = player.GetPet();
    if (mask & GROUP_UPDATE_FLAG_PET_GUID)
    {
        packet << (pet ? pet->GetObjectGuid() : ObjectGuid());
    }

    if (mask & GROUP_UPDATE_FLAG_PET_NAME)
    {
        if (pet)
        {
            packet << pet->GetName();
        }
        else
        {
            packet << uint8(0);
        }
    }

    if (mask & GROUP_UPDATE_FLAG_PET_MODEL_ID)
    {
        if (pet)
        {
            packet << uint16(pet->GetDisplayId());
        }
        else
        {
            packet << uint16(0);
        }
    }

    if (mask & GROUP_UPDATE_FLAG_PET_CUR_HP)
    {
        if (pet)
        {
            packet << uint32(pet->GetHealth());
        }
        else
        {
            packet << uint32(0);
        }
    }

    if (mask & GROUP_UPDATE_FLAG_PET_MAX_HP)
    {
        if (pet)
        {
            packet << uint32(pet->GetMaxHealth());
        }
        else
        {
            packet << uint32(0);
        }
    }

    if (mask & GROUP_UPDATE_FLAG_PET_POWER_TYPE)
    {
        if (pet)
        {
            packet << uint8(pet->GetPowerType());
        }
        else
        {
            packet << uint8(0);
        }
    }

    if (mask & GROUP_UPDATE_FLAG_PET_CUR_POWER)
    {
        if (pet)
        {
            packet << uint16(pet->GetPower(pet->GetPowerType()));
        }
        else
        {
            packet << uint16(0);
        }
    }

    if (mask & GROUP_UPDATE_FLAG_PET_MAX_POWER)
    {
        if (pet)
        {
            packet << uint16(pet->GetMaxPower(pet->GetPowerType()));
        }
        else
        {
            packet << uint16(0);
        }
    }

    if (mask & GROUP_UPDATE_FLAG_PET_AURAS)
    {
        if (pet)
        {
            packet << uint8(0);              // if true, client clears all auras that are not in auramask and whose index is lower amount sent below
            const uint64& auramask = pet->GetAuraUpdateMask();
            packet << uint64(auramask);
            packet << uint32(MAX_AURAS);     // server sends here number of visible auras, but client checks
                                            // if aura is in auramask, so it seems no difference if there will be MAX_AURAS
            for (uint32 i = 0; i < MAX_AURAS; ++i)
            {
                if (auramask & (uint64(1) << i))
                {
                    if (SpellAuraHolder* holder = pet->GetVisibleAura(i))
                    {
                        packet << uint32(holder->GetId());
                        packet << uint16(holder->GetAuraFlags());
                        if (holder->GetAuraFlags() & AFLAG_EFFECT_AMOUNT_SEND)
                        {
                            for (uint32 i = 0; i < MAX_EFFECT_INDEX; ++i)
                            {
                                if (Aura* aura = holder->GetAuraByEffectIndex(SpellEffectIndex(i)))
                                {
                                    packet << int32(aura->GetModifier()->m_amount);
                                }
                                else
                                {
                                    packet << int32(0);
                                }
                            }
                        }
                    }
                    else
                    {
                        packet << uint32(0);
                        packet << uint16(0);
                    }
                }
            }
        }
        else
        {
            packet << uint8(0);
            packet << uint64(0);
            packet << uint32(0);
        }
    }

    if (mask & GROUP_UPDATE_FLAG_VEHICLE_SEAT)
    {
        packet << int32(0);
    }

    if (mask & GROUP_UPDATE_FLAG_PHASE)
    {
        packet << uint32(8);
        packet << uint32(0);
        packet << uint8(0);
    }

    if (mask & GROUP_UPDATE_FLAG_VEHICLE_SEAT)
    {
        if (player.GetTransportInfo())
        {
            packet << uint32(((Unit*)player.GetTransportInfo()->GetTransport())->GetVehicleInfo()->GetVehicleEntry()->SeatID[player.GetTransportInfo()->GetTransportSeat()]);
        }
        else
        {
            packet << uint32(0);
        }
    }
}

void BuildReadyCheckConfirmPacket(WorldPacket& packet, ObjectGuid member, uint8 ready)
{
    packet.Initialize(MSG_RAID_READY_CHECK_CONFIRM, 9);
    packet << member;
    packet << uint8(ready);
}

void SessionGroupNotices::RaidMarkersChanged(Group& group)
{
    WorldPacket data;
    BuildRaidMarkersChangedPacket(data, group.GetGroupMarkerMask());
    group.BroadcastPacket(&data, true);
}

void SessionGroupNotices::LeaderChanged(Group& group, std::string const& name)
{
    WorldPacket data;
    BuildGroupSetLeaderPacket(data, name);
    group.BroadcastPacket(&data, true);
}

void SessionGroupNotices::Uninvited(Player& player)
{
    WorldPacket data;
    BuildGroupUninvitePacket(data);
    player.GetSession()->SendPacket(&data);
}

void SessionGroupNotices::Destroyed(Player& player)
{
    WorldPacket data;
    BuildGroupDestroyedPacket(data);
    player.GetSession()->SendPacket(&data);
}

void SessionGroupNotices::NoGroup(Player& player)
{
    WorldPacket data;
    BuildEmptyGroupListPacket(data);
    player.GetSession()->SendPacket(&data);
}

void SessionGroupNotices::LootStartRoll(Player& player, Roll const& roll, uint32 countDown, uint32 mapId, RollVoteMask mask)
{
    WorldPacket data;
    BuildLootStartRollPacket(data, roll, countDown, mapId, mask);
    player.GetSession()->SendPacket(&data);
}

void SessionGroupNotices::LootRoll(Group& group, Roll const& roll, ObjectGuid target, uint32 rollNumber, uint8 rollType)
{
    WorldPacket data;
    BuildLootRollPacket(data, roll, target, rollNumber, rollType);
    group.SendToRollVoters(&data, roll);
}

void SessionGroupNotices::LootRollWon(Group& group, Roll const& roll, ObjectGuid target, uint32 rollNumber, RollVote rollType)
{
    WorldPacket data;
    BuildLootRollWonPacket(data, roll, target, rollNumber, rollType);
    group.SendToRollVoters(&data, roll);
}

void SessionGroupNotices::LootAllPassed(Group& group, Roll const& roll)
{
    WorldPacket data;
    BuildLootAllPassedPacket(data, roll);
    group.SendToRollVoters(&data, roll);
}

void SessionGroupNotices::MasterLootList(std::vector<ObjectGuid> const& candidates, std::vector<Player*> const& recipients)
{
    WorldPacket data;
    BuildMasterLootListPacket(data, candidates);
    for (std::vector<Player*>::const_iterator itr = recipients.begin(); itr != recipients.end(); ++itr)
    {
        (*itr)->GetSession()->SendPacket(&data);
    }
}

void SessionGroupNotices::TargetIconSet(Group& group, ObjectGuid who, uint8 id, ObjectGuid target)
{
    WorldPacket data;
    BuildTargetIconSetPacket(data, who, id, target);
    group.BroadcastPacket(&data, true);
}

void SessionGroupNotices::GroupList(Group const& group, Player& player, Group::MemberSlot const& slot, uint8 lfgState, uint32 lfgDungeon, uint32 sequence)
{
    WorldPacket data;
    BuildGroupListPacket(data, group, slot, lfgState, lfgDungeon, sequence);
    player.GetSession()->SendPacket(&data);
}

void SessionGroupNotices::PartyMemberStatsChanged(Group& group, Player& member)
{
    WorldPacket data;
    BuildPartyMemberStatsChangedPacket(data, member);
    group.SendToMembersNotSeeing(&data, member);
}

void SessionGroupNotices::ReadyCheckOffline(Group& group, ObjectGuid member)
{
    WorldPacket data;
    BuildReadyCheckConfirmPacket(data, member, 0);
    group.BroadcastReadyCheck(&data);
}
