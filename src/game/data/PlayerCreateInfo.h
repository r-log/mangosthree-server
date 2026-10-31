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

#ifndef MANGOS_H_PLAYERCREATEINFO
#define MANGOS_H_PLAYERCREATEINFO

#include <functional>
#include <list>

#include "Platform/Define.h"
#include "SharedDefines.h"

/**
 * @brief Action button type enumeration
 */
enum ActionButtonType
{
    ACTION_BUTTON_SPELL = 0x00,                                   ///< Spell button
    ACTION_BUTTON_C = 0x01,                                       ///< Click button
    ACTION_BUTTON_EQSET = 0x20,
    ACTION_BUTTON_EXPANDABLE = 0x30,
    ACTION_BUTTON_MACRO = 0x40,                                   ///< Macro button
    ACTION_BUTTON_CMACRO = ACTION_BUTTON_C | ACTION_BUTTON_MACRO, ///< Click macro button
    ACTION_BUTTON_ITEM = 0x80                                     ///< Item button
};

#define ACTION_BUTTON_ACTION(X) (uint32(X) & 0x00FFFFFF)
#define ACTION_BUTTON_TYPE(X)   ((uint32(X) & 0xFF000000) >> 24)
#define MAX_ACTION_BUTTON_ACTION_VALUE (0x00FFFFFF+1)

// some action button indexes used in code or clarify structure
enum ActionButtonIndex
{
    ACTION_BUTTON_SHAMAN_TOTEMS_BAR = 132,
};

#define MAX_ACTION_BUTTONS 144

/// The player an action button is checked for: its name, for the log, and whether it knows a spell.
struct ActionButtonOwner
{
    char const* name;
    std::function<bool(uint32 spellId)> knowsSpell;
};

/// Whether an action button's data is valid: the button and the action in range and the spell or
/// item it names in the client's or the world's data. For an owner, a spell must also be one it
/// knows, not passive, and a totem spell on the totem bar; with none, the button is a
/// `playercreateinfo_action` row. `msg` logs why a button is not valid.
bool IsActionButtonDataValid(uint8 button, uint32 action, uint8 type, ActionButtonOwner const* owner, bool msg = true);

struct PlayerCreateInfoItem
{
    PlayerCreateInfoItem(uint32 id, uint32 amount) : item_id(id), item_amount(amount) {}

    uint32 item_id;     // Item ID
    uint32 item_amount; // Item amount
};

typedef std::list<PlayerCreateInfoItem> PlayerCreateInfoItems;

// Structure to hold player level info
struct PlayerLevelInfo
{
    PlayerLevelInfo()
    {
        for (int i = 0; i < MAX_STATS; ++i)
        {
            stats[i] = 0;
        }
    }

    uint8 stats[MAX_STATS]; // Stats array
};

typedef std::list<uint32> PlayerCreateInfoSpells;

// Structure to hold player create info action
struct PlayerCreateInfoAction
{
    PlayerCreateInfoAction() : button(0), type(0), action(0) {}
    PlayerCreateInfoAction(uint8 _button, uint32 _action, uint8 _type) : button(_button), type(_type), action(_action) {}

    uint8 button;  // Button index
    uint8 type;    // Action type
    uint32 action; // Action ID
};

typedef std::list<PlayerCreateInfoAction> PlayerCreateInfoActions;

// Structure to hold player info
struct PlayerInfo
{
    // existence checked by displayId != 0             // existence checked by displayId != 0
    PlayerInfo() : mapId(0), areaId(0), positionX(0.f), positionY(0.f), positionZ(0.f), orientation(0.f), displayId_m(0), displayId_f(0), levelInfo(NULL)
    {
    }

    uint32 mapId;
    uint32 areaId;
    float positionX;
    float positionY;
    float positionZ;
    float orientation;
    uint16 phaseMap;
    uint16 displayId_m;
    uint16 displayId_f;
    PlayerCreateInfoItems item;
    PlayerCreateInfoSpells spell;
    PlayerCreateInfoActions action;


    PlayerLevelInfo* levelInfo; // Level info array [level-1] 0..MaxPlayerLevel-1
};

#endif
