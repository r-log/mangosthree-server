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

#ifndef MANGOS_H_GAMEEVENTDATAREPOSITORY
#define MANGOS_H_GAMEEVENTDATAREPOSITORY

#include "Platform/Define.h"

#include <optional>
#include <string>
#include <vector>

/// One `game_event` row.
struct GameEventRow
{
    uint16 entry = 0;
    uint64 startTime = 0;
    uint64 endTime = 0;
    uint32 occurence = 0;
    uint32 length = 0;
    uint32 holiday = 0;
    std::string description;
};

/// One creature spawn's game event (`game_event_creature`).
struct GameEventCreatureRow
{
    uint32 guid = 0;
    int16 event = 0;
};

/// One game object spawn's game event (`game_event_gameobject`).
struct GameEventGameObjectRow
{
    uint32 guid = 0;
    int16 event = 0;
};

/// One creature spawn's changes while a game event runs (`game_event_creature_data`).
struct GameEventCreatureDataRow
{
    uint32 guid = 0;
    uint16 event = 0;
    uint32 modelid = 0;
    uint32 equipmentId = 0;
    uint32 entryId = 0;
    uint32 spellStart = 0;
    uint32 spellEnd = 0;
};

/// One `game_event_quest` row.
struct GameEventQuestRow
{
    uint32 quest = 0;
    uint16 event = 0;
};

/// One `game_event_mail` row.
struct GameEventMailRow
{
    uint16 event = 0;
    uint32 raceMask = 0;
    uint32 quest = 0;
    uint32 mailTemplateId = 0;
    uint32 senderEntry = 0;
};

/// The game event data on the world database: `GameEventMgr`'s loads at startup, each run
/// synchronously and read into rows.
namespace GameEventDataRepository
{
    /// Selects the highest game event entry.
    std::optional<uint16> QueryHighestEventEntry();

    /// Selects every game event with its start and end times (`game_event`).
    std::vector<GameEventRow> QueryEvents();

    /// Selects every creature spawn of a game event.
    std::vector<GameEventCreatureRow> QueryEventCreatures();

    /// Selects every game object spawn of a game event.
    std::vector<GameEventGameObjectRow> QueryEventGameObjects();

    /// Selects every creature's changed model, equipment and spells during a game event.
    std::vector<GameEventCreatureDataRow> QueryEventCreatureData();

    /// Selects every quest of a game event (`game_event_quest`).
    std::vector<GameEventQuestRow> QueryEventQuests();

    /// Selects every mail of a game event (`game_event_mail`).
    std::vector<GameEventMailRow> QueryEventMails();
}

#endif
