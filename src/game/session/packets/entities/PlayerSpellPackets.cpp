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

#include "PlayerSpellPackets.h"
#include "DBCStores.h"
#include "Pet.h"
#include "Unit.h"
#include "Opcodes.h"
#include "Player.h"
#include "WorldPacket.h"
#include "session/packets/spells/CooldownPackets.h"
#include "spells/SpellCooldownMgr.h"

#include <ctime>

void SessionPlayerSpellNotices::SpellSuperseded(Player const& player, uint32 oldId, uint32 newId)
{
    WorldPacket data(SMSG_SUPERCEDED_SPELL, 4 + 4);
    data << uint32(oldId);
    data << uint32(newId);
    player.SendDirectMessage(&data);
}

void SessionPlayerSpellNotices::SpellRemoved(Player const& player, uint32 spellId)
{
    WorldPacket data(SMSG_REMOVED_SPELL, 4);
    data << uint32(spellId);
    player.SendDirectMessage(&data);
}

void SessionPlayerSpellNotices::SpellLearned(Player const& player, uint32 spellId)
{
    WorldPacket data(SMSG_LEARNED_SPELL, 6);
    data << uint32(spellId);
    data << uint32(0);                                      // 3.3.3 unk
    player.SendDirectMessage(&data);
}

void SessionPlayerSpellNotices::ActionButtonsShown(Player const& player, std::vector<uint32> const& packed)
{
    WorldPacket data(SMSG_ACTION_BUTTONS, 1 + (packed.size() * 4));
    for (std::vector<uint32>::const_iterator itr = packed.begin(); itr != packed.end(); ++itr)
    {
        data << uint32(*itr);
    }
    data << uint8(1);                                       // talent spec amount (in packet)
    player.SendDirectMessage(&data);
}

void SessionPlayerSpellNotices::ActionButtonsLocked(Player const& player)
{
    WorldPacket data(SMSG_ACTION_BUTTONS, 1);
    // sending 2 locks actions bars, neither user can remove buttons, nor client removes buttons at spell unlearn
    // they remain locked until server sends new action buttons
    data << uint8(2);
    player.SendDirectMessage(&data);
}

void SessionPlayerSpellNotices::InitialSpellsShown(Player const& player)
{
    time_t curTime = time(NULL);
    time_t infTime = curTime + SpellCooldownMgr::infinityCooldownDelayCheck;

    /* * * * * * * * * * * * * * * * *
     * * START OF PACKET STRUCTURE * *
     * * * * * * * * * * * * * * * * */
    uint16 spellCount = 0;

    WorldPacket data(SMSG_INITIAL_SPELLS, (1 + 2 + 4 * player.GetSpellMap().size() + 2 + player.GetSpellCooldownMgr().GetSpellCooldownMap().size() * (2 + 2 + 2 + 4 + 4)));
    data << uint8(0);

    /* * * * * * * * * * * * * * * * *
     * *  END OF PACKET STRUCTURE  * *
     * * * * * * * * * * * * * * * * */
    size_t countPos = data.wpos();
    data << uint16(spellCount);                             // spell count placeholder

    /* For each spell the player knows */
    for (PlayerSpellMap::const_iterator itr = player.GetSpellMap().begin(); itr != player.GetSpellMap().end(); ++itr)
    {
        /* If the spell is marked as removed, don't send it */
        PlayerSpell const& playerSpell = itr->second;

        if (playerSpell.state == PLAYERSPELL_REMOVED)
        {
            continue;
        }

        if (!playerSpell.active || playerSpell.disabled)
        {
            continue;
        }

        /* Insert spell into vector for insertion into packet */
        data << uint32(itr->first);
        data << uint16(0);                                  // it's not slot id

        /* Increase spell counter by 1 (sent in packet) */
        spellCount += 1;
    }

    data.put<uint16>(countPos, spellCount);                 // write real count value

    /* For each spell the player has on cooldown */
    uint16 spellCooldowns = player.GetSpellCooldownMgr().GetSpellCooldownMap().size();
    data << uint16(spellCooldowns);
    for (SpellCooldowns::const_iterator itr = player.GetSpellCooldownMgr().GetSpellCooldownMap().begin(); itr != player.GetSpellCooldownMgr().GetSpellCooldownMap().end(); ++itr)
    {
        /* If the spell doesn't exist in the spellbook, just ignore it */
        SpellEntry const* sEntry = sSpellStore.LookupEntry(itr->first);
        if (!sEntry)
        {
            continue;
        }

        SpellCooldown const& spellCooldown = itr->second;

        data << uint32(itr->first);

        data << uint32(spellCooldown.itemid);                 // cast item id
        data << uint16(sEntry->GetCategory());              // spell category

        /* send infinity cooldown in special format */
        if (spellCooldown.end >= infTime)
        {
            data << uint32(1);                              // cooldown
            data << uint32(0x80000000);                     // category cooldown
            continue;
        }

        time_t cooldown = spellCooldown.end > curTime ? (spellCooldown.end - curTime) * IN_MILLISECONDS : 0;

        if (sEntry->GetCategory())                           // may be wrong, but anyway better than nothing...
        {
            data << uint32(0);                              // cooldown
            data << uint32(cooldown);                       // category cooldown
        }
        else
        {
            data << uint32(cooldown);                       // cooldown
            data << uint32(0);                              // category cooldown
        }
    }

    player.SendDirectMessage(&data);
}

void SessionPlayerSpellNotices::ProficiencyShown(Player const& player, ItemClass itemClass, uint32 itemSubclassMask)
{
    WorldPacket data(SMSG_SET_PROFICIENCY, 1 + 4);
    data << uint8(itemClass) << uint32(itemSubclassMask);
    player.SendDirectMessage(&data);
}

void SessionPlayerSpellNotices::ItemCooldownStarted(Player const& player, ObjectGuid item, uint32 spellId)
{
    WorldPacket data(SMSG_ITEM_COOLDOWN, 12);
    data << item;
    data << uint32(spellId);
    player.SendDirectMessage(&data);
}

void SessionPlayerSpellNotices::CooldownCleared(Player const& player, ObjectGuid unit, uint32 spellId)
{
    CooldownsClearedFact fact;
    fact.owner = unit;
    fact.spellIds.push_back(spellId);

    WorldPacket data;
    BuildClearCooldownsPacket(data, fact);
    player.SendDirectMessage(&data);
}

void SessionPlayerSpellNotices::PetSpellsShown(Player const& player, Pet& pet)
{
    CharmInfo& charmInfo = *pet.GetCharmInfo();

    WorldPacket data(SMSG_PET_SPELLS, 8 + 2 + 4 + 4 + 4 * MAX_UNIT_ACTION_BAR_INDEX + 1 + 1);
    data << pet.GetObjectGuid();
    data << uint16(pet.GetCreatureInfo()->Family);         // creature family (required for pet talents)
    data << uint32(0);
    data << uint8(charmInfo.GetReactState()) << uint8(charmInfo.GetCommandState()) << uint16(0);

    // action bar loop
    charmInfo.BuildActionBar(&data);

    size_t spellsCountPos = data.wpos();

    // spells count
    uint8 addlist = 0;
    data << uint8(addlist);                                 // placeholder

    if (pet.isControlled())
    {
        // spells loop
        for (PetSpellMap::const_iterator itr = pet.m_spells.begin(); itr != pet.m_spells.end(); ++itr)
        {
            if (itr->second.state == PETSPELL_REMOVED)
            {
                continue;
            }

            data << uint32(MAKE_UNIT_ACTION_BUTTON(itr->first, itr->second.active));
            ++addlist;
        }
    }

    data.put<uint8>(spellsCountPos, addlist);

    uint8 cooldownsCount = pet.m_CreatureSpellCooldowns.size() + pet.m_CreatureCategoryCooldowns.size();
    data << uint8(cooldownsCount);

    time_t curTime = time(NULL);

    for (CreatureSpellCooldowns::const_iterator itr = pet.m_CreatureSpellCooldowns.begin(); itr != pet.m_CreatureSpellCooldowns.end(); ++itr)
    {
        time_t cooldown = (itr->second > curTime) ? (itr->second - curTime) * IN_MILLISECONDS : 0;

        data << uint32(itr->first);                         // spellid
        data << uint16(0);                                  // spell category?
        data << uint32(cooldown);                           // cooldown
        data << uint32(0);                                  // category cooldown
    }

    for (CreatureSpellCooldowns::const_iterator itr = pet.m_CreatureCategoryCooldowns.begin(); itr != pet.m_CreatureCategoryCooldowns.end(); ++itr)
    {
        time_t cooldown = (itr->second > curTime) ? (itr->second - curTime) * IN_MILLISECONDS : 0;

        data << uint32(itr->first);                         // spellid
        data << uint16(0);                                  // spell category?
        data << uint32(0);                                  // cooldown
        data << uint32(cooldown);                           // category cooldown
    }

    player.SendDirectMessage(&data);
}

void SessionPlayerSpellNotices::CharmSpellsShown(Player const& player, ObjectGuid charm, bool charmIsPlayer, CharmInfo& info, uint8 charmSpellCount)
{
    WorldPacket data(SMSG_PET_SPELLS, 8 + 2 + 4 + 4 + 4 * MAX_UNIT_ACTION_BAR_INDEX + 1 + 4 * charmSpellCount + 1);
    data << charm;
    data << uint16(0);
    data << uint32(0);

    if (!charmIsPlayer)
    {
        data << uint8(info.GetReactState()) << uint8(info.GetCommandState()) << uint16(0);
    }
    else
    {
        data << uint8(0) << uint8(0) << uint16(0);
    }

    info.BuildActionBar(&data);

    data << uint8(charmSpellCount);

    if (charmSpellCount)
    {
        for (uint32 i = 0; i < CREATURE_MAX_SPELLS; ++i)
        {
            CharmSpellEntry* cspell = info.GetCharmSpell(i);
            if (cspell->GetAction())
            {
                data << uint32(cspell->packedData);
            }
        }
    }

    data << uint8(0);                                       // cooldowns count

    player.SendDirectMessage(&data);
}

void SessionPlayerSpellNotices::PetTameFailed(Player const& player, PetTameFailureReason reason)
{
    WorldPacket data(SMSG_PET_TAME_FAILURE, 1);
    data << uint8(reason);
    player.SendDirectMessage(&data);
}

void SessionPlayerSpellNotices::TalentsInvoluntarilyReset(Player const& player, bool pet)
{
    WorldPacket data(SMSG_TALENTS_INVOLUNTARILY_RESET, 1);
    data << uint8(pet ? 1 : 0);
    player.SendDirectMessage(&data);
}

void SessionPlayerSpellNotices::TalentWipeConfirm(Player const& player, ObjectGuid source, uint32 cost)
{
    WorldPacket data(MSG_TALENT_WIPE_CONFIRM, (8 + 4));
    data << ObjectGuid(source);
    data << uint32(cost);
    player.SendDirectMessage(&data);
}

void SessionPlayerSpellNotices::PetUnlearnConfirm(Player const& player, ObjectGuid pet, uint32 cost)
{
    WorldPacket data(SMSG_PET_UNLEARN_CONFIRM, (8 + 4));
    data << ObjectGuid(pet);
    data << uint32(cost);
    player.SendDirectMessage(&data);
}
