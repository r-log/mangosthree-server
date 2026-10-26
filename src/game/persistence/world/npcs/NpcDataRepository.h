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

#ifndef MANGOS_H_NPCDATAREPOSITORY
#define MANGOS_H_NPCDATAREPOSITORY

#include "Platform/Define.h"

class QueryResult;

/// The NPC data on the world database: `ObjectMgr`'s gossip, text, trainer, vendor, spell click and
/// point of interest loads at startup and on a reload, each run synchronously for a loader that
/// keeps its loop; a GM's vendor item writes are queued and written to the SQL log.
namespace NpcDataRepository
{
    /// Selects every gossip menu (`gossip_menu`).
    QueryResult* QueryGossipMenus();

    /// Selects every gossip menu option, ordered by menu and id (`gossip_menu_option`).
    QueryResult* QueryGossipMenuOptions();

    /// Selects every NPC text (`npc_text`).
    QueryResult* QueryNpcTexts();

    /// Selects every row of a trainer spell table.
    QueryResult* QueryTrainerSpells(char const* table);

    /// Selects every row of a vendor item table.
    QueryResult* QueryVendorItems(char const* table);

    /// Inserts a vendor's item, a currency's entry negated by the caller.
    void InsertVendorItem(uint32 entry, uint32 item, uint32 maxcount, uint32 incrtime, uint32 extendedcost);

    /// Deletes a vendor's item, a currency's entry negated by the caller.
    void DeleteVendorItem(uint32 entry, uint32 item);

    /// Selects every spell an NPC casts when clicked (`npc_spellclick_spells`).
    QueryResult* QuerySpellClickSpells();

    /// Selects every gossip point of interest (`points_of_interest`).
    QueryResult* QueryPointsOfInterest();
}

#endif
