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

#include "CreatureAddonRepository.h"
#include "Database/DatabaseEnv.h"
#include "persistence/RowReader.h"

namespace CreatureAddonRepository
{
    CreatureAddonRow ReadRow(Field* fields)
    {
        CreatureAddonRow row;
        row.guidOrEntry = fields[0].GetUInt32();
        row.mount = fields[1].GetUInt32();
        row.bytes1 = fields[2].GetUInt32();
        row.sheathState = fields[3].GetUInt8();
        row.pvpState = fields[4].GetUInt8();
        row.emote = fields[5].GetUInt32();
        row.moveFlags = fields[6].GetUInt32();
        char const* auras = fields[7].GetString();
        row.auras = auras ? auras : "";
        return row;
    }

    TableRead<CreatureAddonRow> QueryCreatureAddons()
    {
        //                                            0       1        2         3              4                 5        6            7
        return ReadWorldTable<CreatureAddonRow>("SELECT `guid`, `mount`, `bytes1`, `b2_0_sheath`, `b2_1_pvp_state`, `emote`, `moveflags`, `auras` FROM `creature_addon`", ReadRow);
    }

    TableRead<CreatureAddonRow> QueryCreatureTemplateAddons()
    {
        //                                            0        1        2         3              4                 5        6            7
        return ReadWorldTable<CreatureAddonRow>("SELECT `entry`, `mount`, `bytes1`, `b2_0_sheath`, `b2_1_pvp_state`, `emote`, `moveflags`, `auras` FROM `creature_template_addon`", ReadRow);
    }
}
