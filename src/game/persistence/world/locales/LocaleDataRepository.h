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

#include <string>
#include <vector>

enum
{
    /// The locales a locale row holds a text for: every locale after the default one (`loc1` to
    /// `loc11`).
    LOCALE_ROW_LOCALES = 11,
    /// The texts of an NPC text, each in two forms (`Text<n>_0` and `Text<n>_1`).
    NPC_TEXT_LOCALE_ROW_TEXTS = 8
};

/// One `locales_creature` row: a creature's name and subname in each locale.
struct CreatureLocaleRow
{
    uint32 entry = 0;
    std::string name[LOCALE_ROW_LOCALES];
    std::string subname[LOCALE_ROW_LOCALES];
};

/// One `locales_gossip_menu_option` row: a gossip option's text and box text in each locale.
struct GossipMenuOptionLocaleRow
{
    uint16 menuId = 0;
    uint16 id = 0;
    std::string optionText[LOCALE_ROW_LOCALES];
    std::string boxText[LOCALE_ROW_LOCALES];
};

/// One `locales_points_of_interest` row: a point of interest's icon name in each locale.
struct PointOfInterestLocaleRow
{
    uint32 entry = 0;
    std::string iconName[LOCALE_ROW_LOCALES];
};

/// One `locales_page_text` row: a page's text in each locale.
struct PageTextLocaleRow
{
    uint32 entry = 0;
    std::string text[LOCALE_ROW_LOCALES];
};

/// One `locales_npc_text` row: each of an NPC text's texts in both forms, in each locale.
struct NpcTextLocaleRow
{
    uint32 entry = 0;
    std::string text0[LOCALE_ROW_LOCALES][NPC_TEXT_LOCALE_ROW_TEXTS];
    std::string text1[LOCALE_ROW_LOCALES][NPC_TEXT_LOCALE_ROW_TEXTS];
};

/// One `locales_gameobject` row: a game object's name and cast bar caption in each locale.
struct GameObjectLocaleRow
{
    uint32 entry = 0;
    std::string name[LOCALE_ROW_LOCALES];
    std::string castBarCaption[LOCALE_ROW_LOCALES];
};

/// The localised texts on the world database (the `locales_` tables): `ObjectMgr`'s locale loads at
/// startup and on a reload, each run synchronously and read into rows.
namespace LocaleDataRepository
{
    /// Selects every creature's localised names (`locales_creature`).
    std::vector<CreatureLocaleRow> QueryCreatureLocales();

    /// Selects every gossip option's localised texts (`locales_gossip_menu_option`).
    std::vector<GossipMenuOptionLocaleRow> QueryGossipMenuOptionLocales();

    /// Selects every point of interest's localised name (`locales_points_of_interest`).
    std::vector<PointOfInterestLocaleRow> QueryPointOfInterestLocales();

    /// Selects every page text's localised text (`locales_page_text`).
    std::vector<PageTextLocaleRow> QueryPageTextLocales();

    /// Selects every NPC text's localised texts (`locales_npc_text`).
    std::vector<NpcTextLocaleRow> QueryNpcTextLocales();

    /// Selects every game object's localised names (`locales_gameobject`).
    std::vector<GameObjectLocaleRow> QueryGameObjectLocales();
}

#endif
