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

#ifndef MANGOS_H_LOCALEDATAREPOSITORY
#define MANGOS_H_LOCALEDATAREPOSITORY

#include "Platform/Define.h"

class QueryResult;

/// The localised texts on the world database (the `locales_` tables): `ObjectMgr`'s locale loads at
/// startup and on a reload, each run synchronously for a loader that keeps its loop.
namespace LocaleDataRepository
{
    /// Selects every creature's localised names (`locales_creature`).
    QueryResult* QueryCreatureLocales();

    /// Selects every gossip option's localised texts (`locales_gossip_menu_option`).
    QueryResult* QueryGossipMenuOptionLocales();

    /// Selects every point of interest's localised name (`locales_points_of_interest`).
    QueryResult* QueryPointOfInterestLocales();

    /// Selects every page text's localised text (`locales_page_text`).
    QueryResult* QueryPageTextLocales();

    /// Selects every NPC text's localised texts (`locales_npc_text`).
    QueryResult* QueryNpcTextLocales();

    /// Selects every game object's localised names (`locales_gameobject`).
    QueryResult* QueryGameObjectLocales();
}

#endif
