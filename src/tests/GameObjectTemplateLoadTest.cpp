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

/// The game object template loader on a fake world database. A trap or a goober naming a spell the
/// spell store does not hold is reported, one line per template; one naming a server-side spell (a
/// `spell_template` row, which the spell store holds) is not.

#include "TestHarness.h"
#include "FakeDatabase.h"
#include "Database/DatabaseEnv.h"
#include "DBCStores.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "data/ServerSpellStore.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

namespace
{
    /// Hands out a pointer to a private member: an explicit instantiation may name it, and the
    /// friend the instantiation defines returns it.
    template<class Tag, typename Tag::type Member>
    struct GameObjectTemplateLoadMemberAccess
    {
        friend typename Tag::type MemberOf(Tag)
        {
            return Member;
        }
    };

    struct LogMainFile
    {
        typedef FILE* Log::* type;
        friend type MemberOf(LogMainFile);
    };

    template struct GameObjectTemplateLoadMemberAccess<LogMainFile, &Log::logfile>;

    uint32 const kServerSpell = 990501;
    uint32 const kUnknownSpell = 990599;
    uint32 const kTrapSpellField = 3;
    uint32 const kGooberSpellField = 10;
    size_t const kFirstDataColumn = 16;
    size_t const kColumns = 51;

    /// The fakes on the global world database.
    struct Db
    {
        FakeConnection query;
        FakeConnection async;
        SqlResultQueue results;
        AttachedFakes attached;

        Db() : query(WorldDatabase), async(WorldDatabase), attached(WorldDatabase, &query, &async, &results)
        {
        }
    };

    /// A `gameobject_template` row of `type` whose data field `field` holds `spell`, every other
    /// number 0 and its size 1.
    FakeRow Template(uint32 entry, uint32 type, uint32 field, uint32 spell)
    {
        FakeRow row(kColumns, "0");
        row[0] = std::to_string(entry);
        row[1] = std::to_string(type);
        row[3] = row[4] = row[5] = row[6] = "";
        row[9] = "1";
        row[kFirstDataColumn + field] = std::to_string(spell);
        return row;
    }

    /// Loads `rows` as the `gameobject_template` table.
    void LoadTemplates(FakeRows rows)
    {
        Db db;
        db.query.Answer("SELECT `entry`", std::move(rows));
        sObjectMgr.LoadGameobjectInfo();
    }

    /// The messages of the error lines the log's main file receives while the call runs: the file is
    /// swapped for a scratch file and put back.
    template <typename Call>
    std::vector<std::string> ErrorsLoggedBy(Call call)
    {
        std::filesystem::path const path = std::filesystem::temp_directory_path() /
            ("mangos_gameobject_template_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".log");
        FILE* scratch = std::fopen(path.string().c_str(), "w+");
        if (!scratch)
        {
            return std::vector<std::string>(1, "the scratch log could not be opened");
        }
        FILE* const saved = sLog.*MemberOf(LogMainFile());
        sLog.*MemberOf(LogMainFile()) = scratch;
        call();
        sLog.*MemberOf(LogMainFile()) = saved;

        std::string text;
        std::fseek(scratch, 0, SEEK_SET);
        char buffer[512];
        for (std::size_t n; (n = std::fread(buffer, 1, sizeof(buffer), scratch)) > 0;)
        {
            text.append(buffer, n);
        }
        std::fclose(scratch);
        std::filesystem::remove(path);

        std::string const marker = "ERROR:";
        std::vector<std::string> errors;
        for (std::size_t at = text.find(marker); at != std::string::npos; at = text.find(marker, at))
        {
            at += marker.size();
            std::size_t const end = text.find('\n', at);
            errors.push_back(text.substr(at, end == std::string::npos ? std::string::npos : end - at));
        }
        return errors;
    }

    /// A server-side spell in the spell store for the scope.
    class SeededServerSpell
    {
        public:
            SeededServerSpell()
            {
                m_spell.Entry().ID = kServerSpell;
                sSpellStore.SetEntry(kServerSpell, &m_spell.Entry());
            }

            ~SeededServerSpell()
            {
                sSpellStore.SetEntry(kServerSpell, NULL);
            }

        private:
            ServerSpell m_spell;
    };
}

TEST(GameObjectTemplateLoad_ATrapOrGooberWithAnUnknownSpellIsReported)
{
    SeededServerSpell spell;
    std::vector<std::string> const errors = ErrorsLoggedBy([]()
    {
        LoadTemplates(FakeRows{
            Template(990601, GAMEOBJECT_TYPE_TRAP, kTrapSpellField, kUnknownSpell),
            Template(990602, GAMEOBJECT_TYPE_GOOBER, kGooberSpellField, kUnknownSpell),
        });
    });

    REQUIRE(errors.size() == 2);
    CHECK_STR(errors[0], "Gameobject (Entry: 990601 GoType: 6) have data3=990599 but Spell (Entry 990599) not exist.");
    CHECK_STR(errors[1], "Gameobject (Entry: 990602 GoType: 10) have data10=990599 but Spell (Entry 990599) not exist.");
}

TEST(GameObjectTemplateLoad_ATrapOrGooberWithASpellTemplateSpellIsNotReported)
{
    SeededServerSpell spell;
    std::vector<std::string> const errors = ErrorsLoggedBy([]()
    {
        LoadTemplates(FakeRows{
            Template(990603, GAMEOBJECT_TYPE_TRAP, kTrapSpellField, kServerSpell),
            Template(990604, GAMEOBJECT_TYPE_GOOBER, kGooberSpellField, kServerSpell),
            Template(990605, GAMEOBJECT_TYPE_TRAP, kTrapSpellField, 0),
        });
    });

    CHECK(errors.empty());
    REQUIRE(sObjectMgr.GetGameObjectInfo(990603) != NULL);
    CHECK_EQ(sObjectMgr.GetGameObjectInfo(990603)->trap.spellId, kServerSpell);
    REQUIRE(sObjectMgr.GetGameObjectInfo(990604) != NULL);
    CHECK_EQ(sObjectMgr.GetGameObjectInfo(990604)->goober.spellId, kServerSpell);
}
