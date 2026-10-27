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

#ifndef MANGOS_H_COMMANDDATAREPOSITORY
#define MANGOS_H_COMMANDDATAREPOSITORY

#include "Platform/Define.h"

#include <string>
#include <vector>

class QueryResult;

/// One `command` row.
struct CommandRow
{
    uint32 id = 0;
    std::string commandText;
    uint16 security = 0;
    std::string helpText;
};

/// The help text locales a command help row holds (`help_text_loc1` to `help_text_loc8`).
enum
{
    COMMAND_HELP_ROW_LOCALES = 8
};

/// One `locales_command` row: the command and its help text per locale.
struct CommandHelpLocaleRow
{
    uint32 id = 0;
    std::string helpText[COMMAND_HELP_ROW_LOCALES];
};

/// The chat command table and its localised help on the world database (`command`,
/// `locales_command`): the loads at startup and on a reload, each run synchronously for a loader
/// that keeps its loop.
namespace CommandDataRepository
{
    /// Selects every chat command's security and help text (`command`).
    std::vector<CommandRow> QueryCommands();

    /// Selects every chat command's localised help text (`locales_command`).
    std::vector<CommandHelpLocaleRow> QueryCommandHelpLocales();
}

#endif
