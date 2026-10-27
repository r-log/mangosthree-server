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

/// The script hooks the core calls, with no script library: the base answers what a server with no
/// script library answers, an installed object is what sScriptHooks answers until the hooks it
/// replaced are put back, the creature AI selector asks the installed hooks first and keeps the AI a
/// script made, and the script manager starts, frees and reports the installed library and finds none
/// when none is installed.

#include "TestHarness.h"
#include "data/ScriptHooks.h"
#include "Creature.h"
#include "CreatureAISelector.h"
#include "NullCreatureAI.h"
#include "QuestDef.h"
#include "ScriptMgr.h"

#include <string>

namespace
{
    class TestCreature : public Creature
    {
        public:
            TestCreature() : Creature(CREATURE_SUBTYPE_GENERIC)
            {
                _InitValues();
            }

            /// This creature was never in a world: its update fields go first.
            ~TestCreature()
            {
                delete[] m_uint32Values;
                m_uint32Values = NULL;
            }
    };

    class RecordingHooks : public ScriptHooks
    {
        public:
            Creature* askedForAI = NULL;
            CreatureAI* answer = NULL;
            uint32 calls = 0;

            CreatureAI* GetCreatureAI(Creature* pCreature) override
            {
                ++calls;
                askedForAI = pCreature;
                return answer;
            }
    };

    class LibraryHooks : public ScriptHooks
    {
        public:
            std::string calls;

            void InitScriptLibrary() override
            {
                calls += "init ";
            }

            void FreeScriptLibrary() override
            {
                calls += "free ";
            }

            char const* GetScriptLibraryVersion() override
            {
                return "test library";
            }
    };

    /// Installs hooks for one test and puts back the hooks they replaced.
    class InstalledHooks
    {
        public:
            explicit InstalledHooks(ScriptHooks* hooks) : m_previous(ScriptHooks::Install(hooks))
            {
            }

            ~InstalledHooks()
            {
                ScriptHooks::Install(m_previous);
            }

        private:
            ScriptHooks* m_previous;
    };
}

TEST(ScriptHooks_TheBaseAnswersWhatAServerWithNoScriptLibraryAnswers)
{
    InstalledHooks base(NULL);
    CHECK(sScriptHooks.GetCreatureAI(NULL) == NULL);
    CHECK(sScriptHooks.GetGameObjectAI(NULL) == NULL);
    CHECK(sScriptHooks.CreateInstanceData(NULL) == NULL);
    CHECK(!sScriptHooks.OnGossipHello((Player*)NULL, (Creature*)NULL));
    CHECK(!sScriptHooks.OnQuestAccept((Player*)NULL, (GameObject*)NULL, NULL));
    CHECK(!sScriptHooks.OnGameObjectUse((Unit*)NULL, (GameObject*)NULL));
    CHECK(!sScriptHooks.OnEffectDummy(NULL, 1, EFFECT_INDEX_0, (Item*)NULL, ObjectGuid()));
    CHECK(!sScriptHooks.OnAuraDummy(NULL, true));
    CHECK_EQ(sScriptHooks.GetDialogStatus((Player*)NULL, (Creature*)NULL), uint32(DIALOG_STATUS_UNDEFINED));
    CHECK_EQ(sScriptHooks.GetDialogStatus((Player*)NULL, (GameObject*)NULL), uint32(DIALOG_STATUS_UNDEFINED));
}

TEST(ScriptHooks_AnInstalledObjectAnswersUntilTheReplacedHooksArePutBack)
{
    RecordingHooks hooks;
    ScriptHooks* before = &sScriptHooks;
    {
        InstalledHooks installed(&hooks);
        CHECK(&sScriptHooks == &hooks);
        CHECK(sScriptHooks.GetCreatureAI(NULL) == NULL);
        CHECK_EQ(hooks.calls, 1u);
    }
    CHECK(&sScriptHooks == before);
}

TEST(ScriptHooks_TheAISelectorKeepsTheAIAScriptMade)
{
    TestCreature creature;
    NullCreatureAI scripted(&creature);
    RecordingHooks hooks;
    hooks.answer = &scripted;
    InstalledHooks installed(&hooks);

    CreatureAI* ai = FactorySelector::selectAI(&creature);
    CHECK_EQ(hooks.calls, 1u);
    CHECK(hooks.askedForAI == &creature);
    CHECK(ai == &scripted);
}

TEST(ScriptHooks_TheScriptManagerStartsFreesAndReportsTheInstalledLibrary)
{
    LibraryHooks library;
    InstalledHooks installed(&library);
    CHECK(ScriptHooks::IsInstalled());

    CHECK(sScriptMgr.LoadScriptLibrary("mangosscript") == SCRIPT_LOAD_OK);
    CHECK_STR(library.calls, "free init ");
    CHECK(sScriptMgr.LoadScriptLibrary("another") == SCRIPT_LOAD_ERR_NOT_FOUND);
    CHECK_STR(library.calls, "free init ");

    sScriptMgr.UnloadScriptLibrary();
    CHECK_STR(library.calls, "free init free ");
    CHECK_STR(std::string(sScriptHooks.GetScriptLibraryVersion()), "test library");
}

TEST(ScriptHooks_WithNoLibraryInstalledTheScriptManagerFindsNone)
{
    InstalledHooks base(NULL);
    CHECK(!ScriptHooks::IsInstalled());
    CHECK(sScriptMgr.LoadScriptLibrary("mangosscript") == SCRIPT_LOAD_ERR_NOT_FOUND);
    CHECK(sScriptHooks.GetScriptLibraryVersion() == NULL);
    sScriptMgr.UnloadScriptLibrary();
}
