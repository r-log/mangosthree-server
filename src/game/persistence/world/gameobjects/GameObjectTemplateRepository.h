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

#ifndef MANGOS_H_GAMEOBJECTTEMPLATEREPOSITORY
#define MANGOS_H_GAMEOBJECTTEMPLATEREPOSITORY

#include "Platform/Define.h"
#include "persistence/TableRead.h"

#include <string>

class Field;

/// The quest item and data columns of a `gameobject_template` row.
enum
{
    GAMEOBJECT_TEMPLATE_QUEST_ITEM_COLUMNS = 6,
    GAMEOBJECT_TEMPLATE_DATA_COLUMNS = 32
};

/// One `gameobject_template` row: the entry, type, display, the four texts, faction, flags, size,
/// the quest items, the type's data, `unk2` and the gold range.
struct GameObjectTemplateRow
{
    uint32 entry = 0;
    uint32 type = 0;
    uint32 displayId = 0;
    std::string name;
    std::string iconName;
    std::string castBarCaption;
    std::string unk1;
    uint32 faction = 0;
    uint32 flags = 0;
    float size = 0.0f;
    uint32 questItems[GAMEOBJECT_TEMPLATE_QUEST_ITEM_COLUMNS] = {};
    uint32 data[GAMEOBJECT_TEMPLATE_DATA_COLUMNS] = {};
    uint32 unk2 = 0;
    uint32 minGold = 0;
    uint32 maxGold = 0;
};

/// The game object templates on the world database.
namespace GameObjectTemplateRepository
{
    /// Reads one row in the column order of the statement; a NULL text reads as "".
    GameObjectTemplateRow ReadRow(Field* fields);

    /// Reads every row, synchronously, in the order the database returns them, with how the read ended.
    TableRead<GameObjectTemplateRow> QueryGameObjectTemplates();
}

#endif
