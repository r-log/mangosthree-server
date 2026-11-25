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

/// Tests of the priest's class folder, spells/classes/priest: the list of its mechanic files, which
/// src/game/CMakeLists.txt writes at configure time, against the folder's files; Priest::RegisterFiles, run on two
/// files the test makes in a mechanic file's shape, on spell ids no client data holds; and the rows of every listed
/// file, each registered once, each a line of goldens/spell_handler_keys.txt and a key of the game's table, none two
/// files'. These tests name no file and no count of the folder: a mechanic file a PR adds is in them as it lands, and
/// its own test file (PriestScriptsTest is the folder's; a PR adds Priest<Mechanic>Test.cpp, which src/tests'
/// CMakeLists.txt finds by its name) tests its bodies.

#include "TestHarness.h"
#include "SpellHandlerGolden.h"
#include "spells/classes/priest/Priest.h"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace
{
    // The spell ids the test's two files register at the proc hook; no client data holds them. Both files register
    // the shared one.
    uint32 const kFirstFileSpell = 94301;
    uint32 const kSecondFileSpell = 94302;
    uint32 const kSharedSpell = 94303;

    SpellHandlerOutcome<SpellAuraProcResult> FirstFileProc(OnProcContext& /*ctx*/)
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_OK);
    }

    SpellHandlerOutcome<SpellAuraProcResult> SecondFileProc(OnProcContext& /*ctx*/)
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }

    // A mechanic file's registration: two rows by RegisterScript.
    uint32 RegisterFirstFile(SpellHandlerRegistry& registry)
    {
        RegisterScript<OnProcHook>(registry, kFirstFileSpell, &FirstFileProc);
        RegisterScript<OnProcHook>(registry, kSharedSpell, &FirstFileProc);
        return 2;
    }

    // A mechanic file's registration: two rows by RegisterRanks.
    uint32 RegisterSecondFile(SpellHandlerRegistry& registry)
    {
        uint32 const ids[] = { kSecondFileSpell, kSharedSpell };
        return RegisterRanks<OnProcHook>(registry, ids, &SecondFileProc);
    }
}

TEST(PriestScripts_RegisterFilesRunsEachFileInTheListsOrder)
{
    // Each file's rows reach the table and what the files answer is summed: four rows, three keys, so a caller that
    // finds fewer keys than rows sees the key both files registered. That key keeps the body of the file listed
    // first, so the list's order decides it.
    std::vector<Priest::ScriptFile> const files =
    {
        { "First", &RegisterFirstFile },
        { "Second", &RegisterSecondFile },
    };
    SpellHandlerRegistry registry;
    CHECK_EQ(Priest::RegisterFiles(registry, files), uint32(4));
    CHECK_EQ(registry.Count(), std::size_t(3));
    CHECK_EQ(registry.CountAt(OnProcHook::Key), std::size_t(3));
    CHECK(registry.Find<OnProcHook>(kFirstFileSpell) == &FirstFileProc);
    CHECK(registry.Find<OnProcHook>(kSecondFileSpell) == &SecondFileProc);
    CHECK(registry.Find<OnProcHook>(kSharedSpell) == &FirstFileProc);

    std::vector<Priest::ScriptFile> const reversed = { files[1], files[0] };
    SpellHandlerRegistry other;
    CHECK_EQ(Priest::RegisterFiles(other, reversed), uint32(4));
    CHECK_EQ(other.Count(), std::size_t(3));
    CHECK(other.Find<OnProcHook>(kSharedSpell) == &SecondFileProc);

    // No file registers nothing.
    SpellHandlerRegistry none;
    CHECK_EQ(Priest::RegisterFiles(none, std::vector<Priest::ScriptFile>()), uint32(0));
    CHECK_EQ(none.Count(), std::size_t(0));

    // The test's ids are no key of the game's table.
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    for (uint32 spellId : { kFirstFileSpell, kSecondFileSpell, kSharedSpell })
    {
        CHECK(game.Find<OnProcHook>(spellId) == NULL);
    }
}

TEST(PriestScripts_TheListIsEveryMechanicFileOfTheFolderByFileName)
{
    // Every .cpp of spells/classes/priest but Priest.cpp, by file name. A file the list lacks (CMake not run since
    // the file was added, or a name that is no C++ name) would never be registered.
    std::filesystem::path const folder = std::filesystem::path(MANGOS_TEST_DATA_DIR) / ".." / ".." / "game" /
                                         "spells" / "classes" / "priest";
    REQUIRE(std::filesystem::is_directory(folder));
    std::vector<std::string> onDisk;
    for (std::filesystem::directory_entry const& entry : std::filesystem::directory_iterator(folder))
    {
        std::filesystem::path const& file = entry.path();
        if (entry.is_regular_file() && file.extension().string() == ".cpp" && file.filename().string() != "Priest.cpp")
        {
            onDisk.push_back(file.stem().string());
        }
    }
    std::sort(onDisk.begin(), onDisk.end());

    std::vector<std::string> listed;
    for (Priest::ScriptFile const& file : Priest::ScriptFiles())
    {
        listed.push_back(file.name);
        CHECK(file.registerRows != NULL);
    }
    CHECK_EQ(listed.size(), onDisk.size());
    for (std::size_t i = 0; i < std::min(listed.size(), onDisk.size()); ++i)
    {
        CHECK_STR(listed[i], onDisk[i]);
    }
}

TEST(PriestScripts_EachFileRegistersItsRowsOnceAndTheGameHoldsThem)
{
    // Each listed file on a table of its own: it answers its keys and its defaults, every key it registers is a line
    // of the golden and a key of the game's table, and no key is two files'. RegisterPriestScripts registers them all,
    // the sums of theirs, and so do the class scripts the game's table runs.
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    std::vector<std::pair<uint32, uint32> > const all = game.Keys();
    uint32 answered = 0;
    std::size_t keys = 0;
    std::size_t defaults = 0;
    std::set<std::pair<uint32, uint32> > seen;
    for (Priest::ScriptFile const& file : Priest::ScriptFiles())
    {
        SpellHandlerRegistry own;
        uint32 const rows = file.registerRows(own);
        CHECK_EQ(std::size_t(rows), own.Count() + own.CountDefaults());
        for (std::pair<uint32, uint32> const& key : own.Keys())
        {
            CHECK(seen.insert(key).second);
            CHECK(SpellHandlerGolden::Holds(key));
            CHECK(std::binary_search(all.begin(), all.end(), key));
        }
        answered += rows;
        keys += own.Count();
        defaults += own.CountDefaults();
    }

    SpellHandlerRegistry registry;
    CHECK_EQ(RegisterPriestScripts(registry), answered);
    CHECK_EQ(registry.Count(), keys);
    CHECK_EQ(registry.CountDefaults(), defaults);
    std::vector<std::pair<uint32, uint32> > const mine = registry.Keys();
    std::vector<std::pair<uint32, uint32> > const theirs = SpellHandlerGolden::ClassScripts().Keys();
    CHECK(std::includes(theirs.begin(), theirs.end(), mine.begin(), mine.end()));
}
