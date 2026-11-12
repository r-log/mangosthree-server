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

/// The instance encounter loader on a fake world database. A spell-credit row whose spell the spell
/// store holds (a server-side spell, as `spell_template` adds them) is loaded under that spell. A row
/// whose spell the store does not hold is skipped, and the error names the encounter, the credit type
/// and the spell; the rows after it still load.

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
    struct InstanceEncounterLoadMemberAccess
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

    template struct InstanceEncounterLoadMemberAccess<LogMainFile, &Log::logfile>;

    uint32 const kSeededEncounter = 990301;
    uint32 const kSeededSpell = 990302;
    uint32 const kMurozond = 1271;
    uint32 const kMurozondCredit = 110158;

    char const* const s_seededName[] = { "Seeded Encounter" };
    char const* const s_murozondName[] = { "Murozond" };

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

    /// Loads `rows` as the `instance_encounters` table.
    void LoadEncounters(FakeRows rows)
    {
        Db db;
        db.query.Answer("SELECT", std::move(rows));
        sObjectMgr.LoadInstanceEncounters();
    }

    /// The messages of the error lines the log's main file receives while the call runs: the file is
    /// swapped for a scratch file and put back.
    template <typename Call>
    std::vector<std::string> ErrorsLoggedBy(Call call)
    {
        std::filesystem::path const path = std::filesystem::temp_directory_path() /
            ("mangos_instance_encounter_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".log");
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

    /// Two encounters in the encounter store and one server-side spell in the spell store for the
    /// scope; the loaded encounters are dropped at its end.
    class SeededStores
    {
        public:
            SeededStores()
            {
                m_seeded.ID = kSeededEncounter;
                m_seeded.Name_lang = s_seededName;
                m_murozond.ID = kMurozond;
                m_murozond.Name_lang = s_murozondName;
                sDungeonEncounterStore.SetEntry(kSeededEncounter, &m_seeded);
                sDungeonEncounterStore.SetEntry(kMurozond, &m_murozond);

                m_spell.Entry().ID = kSeededSpell;
                sSpellStore.SetEntry(kSeededSpell, &m_spell.Entry());
            }

            ~SeededStores()
            {
                LoadEncounters(FakeRows{});
                sSpellStore.SetEntry(kSeededSpell, NULL);
                sDungeonEncounterStore.SetEntry(kMurozond, NULL);
                sDungeonEncounterStore.SetEntry(kSeededEncounter, NULL);
            }

        private:
            DungeonEncounterEntry m_seeded = {};
            DungeonEncounterEntry m_murozond = {};
            ServerSpell m_spell;
    };
}

TEST(InstanceEncounterLoad_ASpellCreditWhoseSpellTheStoreHoldsIsLoaded)
{
    SeededStores stores;
    std::vector<std::string> const errors = ErrorsLoggedBy([]()
    {
        LoadEncounters(FakeRows{ FakeRow{"990301", "1", "990302", "0"} });
    });

    CHECK(errors.empty());
    DungeonEncounterMapBounds credit = sObjectMgr.GetDungeonEncounterBounds(kSeededSpell);
    REQUIRE(credit.first != credit.second);
    CHECK_EQ(credit.first->second->dbcEntry->ID, kSeededEncounter);
    CHECK(credit.first->second->creditType == ENCOUNTER_CREDIT_CAST_SPELL);
    CHECK_EQ(credit.first->second->creditEntry, kSeededSpell);
}

TEST(InstanceEncounterLoad_ASpellCreditWhoseSpellNoStoreHoldsIsSkippedWithAnError)
{
    SeededStores stores;
    std::vector<std::string> const errors = ErrorsLoggedBy([]()
    {
        LoadEncounters(FakeRows{
            FakeRow{"1271", "1", "110158", "435"},
            FakeRow{"990301", "1", "990302", "0"},
        });
    });

    REQUIRE(errors.size() == 1);
    CHECK_STR(errors[0], "Table `instance_encounters` has encounter 1271 (Murozond) with credit type 1 (spell cast) "
                         "and spell 110158, which neither Spell.dbc nor `spell_template` holds; the row is skipped.");

    DungeonEncounterMapBounds skipped = sObjectMgr.GetDungeonEncounterBounds(kMurozondCredit);
    CHECK(skipped.first == skipped.second);
    DungeonEncounterMapBounds loaded = sObjectMgr.GetDungeonEncounterBounds(kSeededSpell);
    CHECK(loaded.first != loaded.second);
}
