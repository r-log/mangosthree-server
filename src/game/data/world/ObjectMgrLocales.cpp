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

/**
 * @file ObjectMgrLocales.cpp
 * @brief ObjectMgr's localized string table loaders (creature, gossip menu item, point of interest, page text, gossip
 * text and game object locales).
 */

#include "Common/Locales.h"
#include <string>
#include "Utilities/PackedValues.h"
#include "ObjectMgr.h"
#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "ProgressBar.h"
#include "DBCStores.h"
#include "persistence/world/locales/LocaleDataRepository.h"

static_assert(int(LOCALE_ROW_LOCALES) == int(MAX_LOCALE) - 1, "a locale row holds a text for every locale but the default one");
static_assert(int(NPC_TEXT_LOCALE_ROW_TEXTS) == int(MAX_GOSSIP_TEXT_OPTIONS), "an NPC text locale row holds every text option");

/**
 * @brief Loads localized creature names and subnames from the database.
 */
void ObjectMgr::LoadCreatureLocales()
{
    m_creatureLocales.Clear();                              // need for reload case

    std::vector<CreatureLocaleRow> rows = LocaleDataRepository::QueryCreatureLocales();
    if (rows.empty())
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString(">> Loaded 0 creature locale strings. DB table `locales_creature` is empty.");
        return;
    }

    BarGoLink bar(rows.size());

    for (CreatureLocaleRow const& row : rows)
    {
        bar.step();

        uint32 entry = row.entry;

        if (!GetCreatureTemplate(entry))
        {
            ERROR_DB_STRICT_LOG("Table `locales_creature` has data for not existed creature entry %u, skipped.", entry);
            continue;
        }

        CreatureLocale& data = m_creatureLocales.FindOrAdd(entry);

        for (int i = 1; i < MAX_LOCALE; ++i)
        {
            std::string str = row.name[i - 1];
            if (!str.empty())
            {
                int idx = GetOrNewIndexForLocale(LocaleConstant(i));
                if (idx >= 0)
                {
                    if ((int32)data.Name.size() <= idx)
                    {
                        data.Name.resize(idx + 1);
                    }

                    data.Name[idx] = str;
                }
            }
            str = row.subname[i - 1];
            if (!str.empty())
            {
                int idx = GetOrNewIndexForLocale(LocaleConstant(i));
                if (idx >= 0)
                {
                    if ((int32)data.SubName.size() <= idx)
                    {
                        data.SubName.resize(idx + 1);
                    }

                    data.SubName[idx] = str;
                }
            }
        }
    }

    sLog.outString(">> Loaded %u creature locale strings", m_creatureLocales.GetCount());
    sLog.outString();
}

/**
 * @brief Loads localized gossip menu option and confirmation box text.
 */
void ObjectMgr::LoadGossipMenuItemsLocales()
{
    m_gossipMenuItemsLocales.Clear();                       // need for reload case

    std::vector<GossipMenuOptionLocaleRow> rows = LocaleDataRepository::QueryGossipMenuOptionLocales();

    if (rows.empty())
    {
        BarGoLink bar(1);

        bar.step();

        sLog.outString();
        sLog.outString(">> Loaded 0 gossip_menu_option locale strings. DB table `locales_gossip_menu_option` is empty.");
        return;
    }

    BarGoLink bar(rows.size());

    for (GossipMenuOptionLocaleRow const& row : rows)
    {
        bar.step();

        uint16 menuId   = row.menuId;
        uint16 id       = row.id;

        GossipMenuItemsMapBounds bounds = GetGossipMenuItemsMapBounds(menuId);

        bool found = false;
        if (bounds.first != bounds.second)
        {
            for (GossipMenuItemsMap::const_iterator itr = bounds.first; itr != bounds.second; ++itr)
            {
                if (itr->second.id == id)
                {
                    found = true;
                    break;
                }
            }
        }

        if (!found)
        {
            ERROR_DB_STRICT_LOG("Table `locales_gossip_menu_option` has data for nonexistent gossip menu %u item %u, skipped.", menuId, id);
            continue;
        }

        GossipMenuItemsLocale& data = m_gossipMenuItemsLocales.FindOrAdd(MAKE_PAIR32(menuId, id));

        for (int i = 1; i < MAX_LOCALE; ++i)
        {
            std::string str = row.optionText[i - 1];
            if (!str.empty())
            {
                int idx = GetOrNewIndexForLocale(LocaleConstant(i));
                if (idx >= 0)
                {
                    if ((int32)data.OptionText.size() <= idx)
                    {
                        data.OptionText.resize(idx + 1);
                    }

                    data.OptionText[idx] = str;
                }
            }
            str = row.boxText[i - 1];
            if (!str.empty())
            {
                int idx = GetOrNewIndexForLocale(LocaleConstant(i));
                if (idx >= 0)
                {
                    if ((int32)data.BoxText.size() <= idx)
                    {
                        data.BoxText.resize(idx + 1);
                    }

                    data.BoxText[idx] = str;
                }
            }
        }
    }

    sLog.outString(">> Loaded %u gossip_menu_option locale strings", m_gossipMenuItemsLocales.GetCount());
    sLog.outString();
}

/**
 * @brief Loads localized point-of-interest icon names.
 */
void ObjectMgr::LoadPointOfInterestLocales()
{
    m_pointOfInterestLocales.Clear();                       // need for reload case

    std::vector<PointOfInterestLocaleRow> rows = LocaleDataRepository::QueryPointOfInterestLocales();

    if (rows.empty())
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString(">> Loaded 0 points_of_interest locale strings. DB table `locales_points_of_interest` is empty.");
        return;
    }

    BarGoLink bar(rows.size());

    for (PointOfInterestLocaleRow const& row : rows)
    {
        bar.step();

        uint32 entry = row.entry;

        if (!GetPointOfInterest(entry))
        {
            ERROR_DB_STRICT_LOG("Table `locales_points_of_interest` has data for nonexistent POI entry %u, skipped.", entry);
            continue;
        }

        PointOfInterestLocale& data = m_pointOfInterestLocales.FindOrAdd(entry);

        for (int i = 1; i < MAX_LOCALE; ++i)
        {
            std::string str = row.iconName[i - 1];
            if (str.empty())
            {
                continue;
            }

            int idx = GetOrNewIndexForLocale(LocaleConstant(i));
            if (idx >= 0)
            {
                if ((int32)data.IconName.size() <= idx)
                {
                    data.IconName.resize(idx + 1);
                }

                data.IconName[idx] = str;
            }
        }
    }

    sLog.outString(">> Loaded %u points_of_interest locale strings", m_pointOfInterestLocales.GetCount());
    sLog.outString();
}

/**
 * @brief Loads localized page text content.
 */
void ObjectMgr::LoadPageTextLocales()
{
    m_pageTextLocales.Clear();                              // need for reload case

    std::vector<PageTextLocaleRow> rows = LocaleDataRepository::QueryPageTextLocales();
    if (rows.empty())
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString(">> Loaded 0 PageText locale strings. DB table `locales_page_text` is empty.");
        return;
    }

    BarGoLink bar(rows.size());

    for (PageTextLocaleRow const& row : rows)
    {
        bar.step();

        uint32 entry = row.entry;

        if (!GetPageText(entry))
        {
            ERROR_DB_STRICT_LOG("Table `locales_page_text` has data for nonexistent page text entry %u, skipped.", entry);
            continue;
        }

        PageTextLocale& data = m_pageTextLocales.FindOrAdd(entry);

        for (int i = 1; i < MAX_LOCALE; ++i)
        {
            std::string str = row.text[i - 1];
            if (str.empty())
            {
                continue;
            }

            int idx = GetOrNewIndexForLocale(LocaleConstant(i));
            if (idx >= 0)
            {
                if ((int32)data.Text.size() <= idx)
                {
                    data.Text.resize(idx + 1);
                }

                data.Text[idx] = str;
            }
        }
    }

    sLog.outString(">> Loaded %u PageText locale strings", m_pageTextLocales.GetCount());
    sLog.outString();
}

/**
 * @brief Loads localized npc gossip text variants.
 */
void ObjectMgr::LoadGossipTextLocales()
{
    m_npcTextLocales.Clear();                               // need for reload case

    std::vector<NpcTextLocaleRow> rows = LocaleDataRepository::QueryNpcTextLocales();
    if (rows.empty())
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString(">> Loaded 0 Quest locale strings. DB table `locales_npc_text` is empty.");
        return;
    }

    BarGoLink bar(rows.size());

    for (NpcTextLocaleRow const& row : rows)
    {
        bar.step();

        uint32 entry = row.entry;

        if (!GetGossipText(entry))
        {
            ERROR_DB_STRICT_LOG("Table `locales_npc_text` has data for nonexistent gossip text entry %u, skipped.", entry);
            continue;
        }

        NpcTextLocale& data = m_npcTextLocales.FindOrAdd(entry);

        for (int i = 1; i < MAX_LOCALE; ++i)
        {
            for (int j = 0; j < 8; ++j)
            {
                std::string str0 = row.text0[i - 1][j];
                if (!str0.empty())
                {
                    int idx = GetOrNewIndexForLocale(LocaleConstant(i));
                    if (idx >= 0)
                    {
                        if ((int32)data.Text_0[j].size() <= idx)
                        {
                            data.Text_0[j].resize(idx + 1);
                        }

                        data.Text_0[j][idx] = str0;
                    }
                }
                std::string str1 = row.text1[i - 1][j];
                if (!str1.empty())
                {
                    int idx = GetOrNewIndexForLocale(LocaleConstant(i));
                    if (idx >= 0)
                    {
                        if ((int32)data.Text_1[j].size() <= idx)
                        {
                            data.Text_1[j].resize(idx + 1);
                        }

                        data.Text_1[j][idx] = str1;
                    }
                }
            }
        }
    }

    sLog.outString(">> Loaded %u NpcText locale strings", m_npcTextLocales.GetCount());
    sLog.outString();
}

/**
 * @brief Loads localized gameobject names.
 */
void ObjectMgr::LoadGameObjectLocales()
{
    m_gameObjectLocales.Clear();                            // need for reload case

    std::vector<GameObjectLocaleRow> rows = LocaleDataRepository::QueryGameObjectLocales();
    if (rows.empty())
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString(">> Loaded 0 gameobject locale strings. DB table `locales_gameobject` is empty.");
        return;
    }

    BarGoLink bar(rows.size());

    for (GameObjectLocaleRow const& row : rows)
    {
        bar.step();

        uint32 entry = row.entry;

        if (!GetGameObjectInfo(entry))
        {
            ERROR_DB_STRICT_LOG("Table `locales_gameobject` has data for nonexistent gameobject entry %u, skipped.", entry);
            continue;
        }

        GameObjectLocale& data = m_gameObjectLocales.FindOrAdd(entry);

        for (int i = 1; i < MAX_LOCALE; ++i)
        {
            std::string str = row.name[i - 1];
            if (!str.empty())
            {
                int idx = GetOrNewIndexForLocale(LocaleConstant(i));
                if (idx >= 0)
                {
                    if ((int32)data.Name.size() <= idx)
                    {
                        data.Name.resize(idx + 1);
                    }

                    data.Name[idx] = str;
                }
            }
        }

        for (int i = 1; i < MAX_LOCALE; ++i)
        {
            std::string str = row.castBarCaption[i - 1];
            if (!str.empty())
            {
                int idx = GetOrNewIndexForLocale(LocaleConstant(i));
                if (idx >= 0)
                {
                    if ((int32)data.CastBarCaption.size() <= idx)
                    {
                        data.CastBarCaption.resize(idx + 1);
                    }

                    data.CastBarCaption[idx] = str;
                }
            }
        }
    }

    sLog.outString(">> Loaded %u gameobject locale strings", m_gameObjectLocales.GetCount());
    sLog.outString();
}
