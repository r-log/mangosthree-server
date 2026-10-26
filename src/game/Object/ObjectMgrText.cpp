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
 * @file ObjectMgrText.cpp
 * @brief ObjectMgr's page text and gossip text loaders.
 */

#include <sstream>
#include <set>
#include <type_traits>
#include "ObjectMgr.h"
#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "ProgressBar.h"
#include "SQLStorages.h"
#include "DBCStores.h"
#include "Language.h"
#include "persistence/world/npcs/NpcDataRepository.h"

static_assert(int(NPC_TEXT_ROW_OPTIONS) == int(MAX_GOSSIP_TEXT_OPTIONS), "an NPC text row holds every option");
static_assert(int(NPC_TEXT_ROW_EMOTES) == int(std::extent<decltype(GossipTextOption::Emotes)>::value), "an NPC text option row holds every emote");

/**
 * @brief Loads page text records and validates page chains.
 */
void ObjectMgr::LoadPageTexts()
{
    sPageTextStore.Load();
    sLog.outString(">> Loaded %u page texts", sPageTextStore.GetRecordCount());
    sLog.outString();

    for (uint32 i = 1; i < sPageTextStore.GetMaxEntry(); ++i)
    {
        // check data correctness
        PageText const* page = sPageTextStore.LookupEntry<PageText>(i);
        if (!page)
        {
            continue;
        }

        if (page->Next_Page && !sPageTextStore.LookupEntry<PageText>(page->Next_Page))
        {
            sLog.outErrorDb("Page text (Id: %u) has not existing next page (Id:%u)", i, page->Next_Page);
            continue;
        }

        // detect circular reference
        std::set<uint32> checkedPages;
        for (PageText const* pageItr = page; pageItr; pageItr = sPageTextStore.LookupEntry<PageText>(pageItr->Next_Page))
        {
            if (!pageItr->Next_Page)
            {
                break;
            }
            checkedPages.insert(pageItr->Page_ID);
            if (checkedPages.find(pageItr->Next_Page) != checkedPages.end())
            {
                std::ostringstream ss;
                ss << "The text page(s) ";
                for (std::set<uint32>::iterator itr = checkedPages.begin(); itr != checkedPages.end(); ++itr)
                {
                    ss << *itr << " ";
                }
                ss << "create(s) a circular reference, which can cause the server to freeze. Changing Next_Page of page "
                   << pageItr->Page_ID << " to 0";
                sLog.outErrorDb("%s", ss.str().c_str());
                const_cast<PageText*>(pageItr)->Next_Page = 0;
                break;
            }
        }
    }
}

/**
 * @brief Loads npc gossip text records from the database.
 */
void ObjectMgr::LoadGossipText()
{
    std::vector<NpcTextRow> rows = NpcDataRepository::QueryNpcTexts();

    int count = 0;
    if (rows.empty())
    {
        BarGoLink bar(1);
        bar.step();

        sLog.outString(">> Loaded %u npc texts", count);
        sLog.outString();
        return;
    }

    BarGoLink bar(rows.size());

    for (NpcTextRow const& row : rows)
    {
        ++count;

        bar.step();

        uint32 Text_ID    = row.id;
        if (!Text_ID)
        {
            sLog.outErrorDb("Table `npc_text` has record wit reserved id 0, ignore.");
            continue;
        }

        GossipText& gText = mGossipText[Text_ID];

        for (int i = 0; i < MAX_GOSSIP_TEXT_OPTIONS; ++i)
        {
            gText.Options[i].Text_0           = row.options[i].text0;
            gText.Options[i].Text_1           = row.options[i].text1;

            gText.Options[i].Language         = row.options[i].language;
            gText.Options[i].Probability      = row.options[i].probability;

            for (int j = 0; j < 3; ++j)
            {
                gText.Options[i].Emotes[j]._Delay  = row.options[i].emoteDelay[j];
                gText.Options[i].Emotes[j]._Emote  = row.options[i].emote[j];
            }
        }
    }

    sLog.outString(">> Loaded %u npc texts", count);
    sLog.outString();
}
