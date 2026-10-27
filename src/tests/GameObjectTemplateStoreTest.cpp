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

/// GameObjectTemplateStore: a template is held under its entry with its four texts owned by the
/// store, a second template for an entry overwrites the first, the edit access changes the held
/// template, and the walk visits the templates in ascending entry order.

#include "TestHarness.h"
#include "data/GameObjectTemplateStore.h"

#include <string>
#include <vector>

namespace
{
    GameObjectInfo Template(uint32 entry, uint32 type)
    {
        GameObjectInfo info = GameObjectInfo();
        info.id = entry;
        info.type = type;
        info.size = 1.0f;
        info.raw.data[0] = entry + 1;
        return info;
    }
}

// The store copies the texts, so they outlive the strings it was handed, and ignores the record's own pointers.
TEST(GameObjectTemplateStore_OwnsTheTexts)
{
    GameObjectTemplateStore store;
    {
        std::string name = "a chest";
        std::string icon = "an icon";
        GameObjectInfo info = Template(42, GAMEOBJECT_TYPE_CHEST);
        info.name = NULL;
        store.Set(info, name, icon, "a caption", "");
    }
    GameObjectInfo const* info = store.Find(42);
    REQUIRE(info != NULL);
    CHECK_EQ(info->type, uint32(GAMEOBJECT_TYPE_CHEST));
    CHECK_EQ(info->raw.data[0], 43u);
    CHECK_STR(info->name, "a chest");
    CHECK_STR(info->IconName, "an icon");
    CHECK_STR(info->castBarCaption, "a caption");
    CHECK_STR(info->unk1, "");
    CHECK(store.Find(41) == NULL);
}

// A second template for an entry overwrites the first, and the edit access changes the held one.
TEST(GameObjectTemplateStore_OverwritesAndEdits)
{
    GameObjectTemplateStore store;
    store.Set(Template(7, GAMEOBJECT_TYPE_DOOR), "first", "", "", "");
    store.Set(Template(7, GAMEOBJECT_TYPE_BUTTON), "second", "", "", "");
    CHECK_EQ(store.GetCount(), 1u);
    CHECK_STR(store.Find(7)->name, "second");
    CHECK_EQ(store.Find(7)->type, uint32(GAMEOBJECT_TYPE_BUTTON));

    GameObjectInfo* edit = store.FindForEdit(7);
    REQUIRE(edit != NULL);
    edit->size = 2.0f;
    CHECK(store.Find(7)->size == 2.0f);
    CHECK(store.FindForEdit(8) == NULL);
}

// The walk visits the templates in ascending entry order, whatever order they were set in; the
// highest entry is one below the max, and clearing empties the store.
TEST(GameObjectTemplateStore_WalksInEntryOrder)
{
    GameObjectTemplateStore store;
    CHECK_EQ(store.GetMaxEntry(), 0u);
    CHECK(store.begin() == store.end());
    uint32 const entries[] = { 300, 0, 25, 7 };
    for (uint32 entry : entries)
    {
        store.Set(Template(entry, GAMEOBJECT_TYPE_GENERIC), "", "", "", "");
    }
    std::vector<uint32> walked;
    for (GameObjectTemplateStore::const_iterator itr = store.begin(); itr != store.end(); ++itr)
    {
        walked.push_back(itr->id);
        CHECK_EQ((*itr).raw.data[0], itr->id + 1);
    }
    REQUIRE(walked.size() == 4);
    CHECK_EQ(walked[0], 0u);
    CHECK_EQ(walked[1], 7u);
    CHECK_EQ(walked[2], 25u);
    CHECK_EQ(walked[3], 300u);
    CHECK_EQ(store.GetMaxEntry(), 301u);

    store.Clear();
    CHECK_EQ(store.GetCount(), 0u);
    CHECK(store.Find(300) == NULL);
}
