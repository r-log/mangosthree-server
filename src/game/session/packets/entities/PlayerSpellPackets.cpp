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
