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

#ifndef MANGOS_H_CREATUREADDONREPOSITORY
#define MANGOS_H_CREATUREADDONREPOSITORY

#include "Platform/Define.h"
#include "persistence/TableRead.h"

#include <string>

class Field;

/// One `creature_addon` or `creature_template_addon` row: the spawn guid or creature entry, the
/// mount, the bytes, the sheath and PvP states, the emote, the move flags and the aura list text (a
/// NULL text reads as "").
struct CreatureAddonRow
{
    uint32 guidOrEntry = 0;
    uint32 mount = 0;
    uint32 bytes1 = 0;
    uint8 sheathState = 0;
    uint8 pvpState = 0;
    uint32 emote = 0;
    uint32 moveFlags = 0;
    std::string auras;
};

/// The creature addons on the world database.
namespace CreatureAddonRepository
{
    /// Reads one row: key, mount, bytes1, sheath state, PvP state, emote, move flags, auras.
    CreatureAddonRow ReadRow(Field* fields);

    /// Reads every spawn addon (`creature_addon`), synchronously, in the order the database returns them, with how the read ended.
    TableRead<CreatureAddonRow> QueryCreatureAddons();

    /// Reads every creature template addon (`creature_template_addon`), synchronously, in the order the database returns them, with how the read ended.
    TableRead<CreatureAddonRow> QueryCreatureTemplateAddons();
}

#endif
