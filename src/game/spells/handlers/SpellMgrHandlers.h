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

#ifndef MANGOSSERVER_SPELLS_HANDLERS_SPELLMGRHANDLERS_H
#define MANGOSSERVER_SPELLS_HANDLERS_SPELLMGRHANDLERS_H

#include "spells/handlers/SpellHandlerRegistry.h"
#include "Server/SharedDefines.h"

class Player;

/**
 * @file SpellMgrHandlers.h
 * @brief The `GetSpellAllowedInLocationError` site of the spell handler registry.
 *
 * The handlers are file-static functions in SpellMgrHandlers.cpp; this header holds what the registry and
 * tests see.
 */

/// What the bodies of `GetSpellAllowedInLocationError`'s switch read: the map and the player.
struct SpellAllowedInLocationContext
{
    SpellAllowedInLocationContext(uint32 const& mapIdParameter, Player const* const& playerParameter)
        : map_id(mapIdParameter), player(playerParameter) {}

    uint32 const& map_id;           ///< the function's parameter `uint32 map_id`
    Player const* const& player;    ///< the function's parameter `Player const* player`, which may be NULL
};

/// `GetSpellAllowedInLocationError`, after the battleground checks: its `switch (spellInfo->ID)`. Every
/// label answers the check's result; a miss goes on to the check's `return SPELL_CAST_OK;`.
struct SpellAllowedInLocationSite
{
    static constexpr uint32 Key = SPELL_HANDLER_SITE_SPELL_ALLOWED_IN_LOCATION;
    typedef SpellCastResult Value;
    typedef SpellAllowedInLocationContext Context;
};

/// Registers every `GetSpellAllowedInLocationError` handler on `registry` (defined in SpellMgrHandlers.cpp).
/// Returns the number of rows it registered; a row whose key was taken does not change the table, so a caller
/// that registers on an empty table and finds fewer keys than rows has a duplicate.
uint32 RegisterSpellMgrHandlers(SpellHandlerRegistry& registry);

#endif
