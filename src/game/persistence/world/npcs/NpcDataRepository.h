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

#include <string>
#include <vector>

class QueryResult;

/// One `gossip_menu` row.
struct GossipMenuRow
{
    uint32 entry = 0;
    uint32 textId = 0;
    uint32 scriptId = 0;
    uint16 conditionId = 0;
};

/// One `gossip_menu_option` row.
struct GossipMenuOptionRow
{
    uint32 menuId = 0;
    uint32 id = 0;
    uint8 optionIcon = 0;
    std::string optionText;
    uint32 optionId = 0;
    uint32 npcOptionNpcflag = 0;
    int32 actionMenuId = 0;
    uint32 actionPoiId = 0;
    uint32 actionScriptId = 0;
    uint8 boxCoded = 0;
    uint32 boxMoney = 0;
    std::string boxText;
    uint16 conditionId = 0;
};

/// One row of a trainer spell table (`npc_trainer` or `npc_trainer_template`).
struct TrainerSpellRow
{
    uint32 entry = 0;
    uint32 spell = 0;
    uint32 spellcost = 0;
    uint32 reqskill = 0;
    uint32 reqskillvalue = 0;
    uint32 reqlevel = 0;
};

/// One row of a vendor item table (`npc_vendor` or `npc_vendor_template`).
struct VendorItemRow
{
    uint32 entry = 0;
    int32 item = 0;
    uint32 maxcount = 0;
    uint32 incrtime = 0;
    uint32 extendedCost = 0;
    uint16 conditionId = 0;
};

/// One `npc_spellclick_spells` row.
struct SpellClickSpellRow
{
    uint32 npcEntry = 0;
    uint32 spellId = 0;
    uint32 questStart = 0;
    bool questStartActive = false;
    uint32 questEnd = 0;
    uint8 castFlags = 0;
    uint16 conditionId = 0;
};

/// One `points_of_interest` row.
struct PointOfInterestRow
{
    uint32 entry = 0;
    float x = 0.0f;
    float y = 0.0f;
    uint32 icon = 0;
    uint32 flags = 0;
    uint32 data = 0;
    std::string iconName;
};

/// The NPC data on the world database: `ObjectMgr`'s gossip, text, trainer, vendor, spell click and
/// point of interest loads at startup and on a reload, each run synchronously; a GM's vendor item
/// writes are queued and written to the SQL log.
namespace NpcDataRepository
{
    /// Selects every gossip menu (`gossip_menu`).
    std::vector<GossipMenuRow> QueryGossipMenus();

    /// Selects every gossip menu option, ordered by menu and id (`gossip_menu_option`).
    std::vector<GossipMenuOptionRow> QueryGossipMenuOptions();

    /// Selects every NPC text (`npc_text`).
    QueryResult* QueryNpcTexts();

    /// Selects every row of a trainer spell table.
    std::vector<TrainerSpellRow> QueryTrainerSpells(char const* table);

    /// Selects every row of a vendor item table.
    std::vector<VendorItemRow> QueryVendorItems(char const* table);

    /// Inserts a vendor's item, a currency's entry negated by the caller.
    void InsertVendorItem(uint32 entry, uint32 item, uint32 maxcount, uint32 incrtime, uint32 extendedcost);

    /// Deletes a vendor's item, a currency's entry negated by the caller.
    void DeleteVendorItem(uint32 entry, uint32 item);

    /// Selects every spell an NPC casts when clicked (`npc_spellclick_spells`).
    std::vector<SpellClickSpellRow> QuerySpellClickSpells();

    /// Selects every gossip point of interest (`points_of_interest`).
    std::vector<PointOfInterestRow> QueryPointsOfInterest();

    /// Selects every item a vendor sells, for the auction house bot's filter.
    QueryResult* QueryVendorItemEntries();
}

#endif
