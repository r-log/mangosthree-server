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

#include "TestHarness.h"

#include "ObjectGuid.h"
#include "data/DbScriptHooks.h"

/**
 * @file
 * @brief What the domain asks of the database script engine: a scheduled step knows the script it
 * belongs to, the base runs no step and takes no event, and the installed hooks are handed the
 * step and the event.
 */

namespace
{
    /// Records the step it is handed and asks to keep the script's following steps.
    class RecordingHooks : public DbScriptHooks
    {
        public:
            int runs = 0;
            DbScriptStep const* seen = NULL;

            bool RunStep(Map* /*map*/, DbScriptStep const& step) override
            {
                ++runs;
                seen = &step;
                return false;
            }

            uint32 eventId = 0;
            bool eventStarts = false;

            bool StartEvent(Map* /*map*/, uint32 id, Object* /*source*/, Object* /*target*/, bool isStart, Unit* /*forwardToPvp*/) override
            {
                eventId = id;
                eventStarts = isStart;
                return true;
            }
    };

    /// Installs `hooks` for one test and puts the old ones back.
    class InstalledHooks
    {
        public:
            explicit InstalledHooks(DbScriptHooks* hooks) : m_previous(DbScriptHooks::Install(hooks)) {}
            ~InstalledHooks() { DbScriptHooks::Install(m_previous); }
        private:
            DbScriptHooks* m_previous;
    };

    ScriptInfo StepOfScript(uint32 id)
    {
        ScriptInfo info = ScriptInfo();
        info.id = id;
        return info;
    }
}

// A step matches its own script and source, an empty guid matches any object, and a step of
// another script, type or object does not match.
TEST(DbScriptHooks_StepMatchesItsScriptAndObjects)
{
    ScriptInfo info = StepOfScript(42);
    ObjectGuid source(HIGHGUID_UNIT, uint32(100), uint32(7));
    ObjectGuid other(HIGHGUID_UNIT, uint32(100), uint32(8));
    DbScriptStep step(DBS_ON_EVENT, source, ObjectGuid(), ObjectGuid(), &info);

    CHECK(step.GetType() == DBS_ON_EVENT);
    CHECK(step.GetId() == 42u);
    CHECK(step.GetScript() == &info);
    CHECK(step.IsSameScript(DBS_ON_EVENT, 42, source, ObjectGuid(), ObjectGuid()));
    CHECK(step.IsSameScript(DBS_ON_EVENT, 42, ObjectGuid(), ObjectGuid(), ObjectGuid()));
    CHECK(!step.IsSameScript(DBS_ON_EVENT, 42, other, ObjectGuid(), ObjectGuid()));
    CHECK(!step.IsSameScript(DBS_ON_EVENT, 43, source, ObjectGuid(), ObjectGuid()));
    CHECK(!step.IsSameScript(DBS_ON_SPELL, 42, source, ObjectGuid(), ObjectGuid()));
}

// With no hooks installed, a step runs nothing and ends its script.
TEST(DbScriptHooks_BaseEndsTheScript)
{
    InstalledHooks none(NULL);
    ScriptInfo info = StepOfScript(1);
    DbScriptStep step(DBS_ON_EVENT, ObjectGuid(), ObjectGuid(), ObjectGuid(), &info);

    CHECK(!DbScriptHooks::IsInstalled());
    CHECK(sDbScriptHooks.RunStep(NULL, step));
    CHECK(!StartEvents_Event(NULL, 9, NULL, NULL));
}

// The installed hooks are handed the step and answer for it, until the old ones are put back.
TEST(DbScriptHooks_InstalledHooksRunTheStep)
{
    RecordingHooks hooks;
    ScriptInfo info = StepOfScript(5);
    DbScriptStep step(DBS_ON_GO_USE, ObjectGuid(), ObjectGuid(), ObjectGuid(), &info);
    {
        InstalledHooks installed(&hooks);
        CHECK(DbScriptHooks::IsInstalled());
        CHECK(!sDbScriptHooks.RunStep(NULL, step));
        CHECK(hooks.runs == 1);
        CHECK(hooks.seen == &step);
    }
    CHECK(&sDbScriptHooks != &hooks);
}

// StartEvents_Event hands the event to the installed hooks, starting unless told otherwise.
TEST(DbScriptHooks_EventsGoToTheInstalledHooks)
{
    RecordingHooks hooks;
    InstalledHooks installed(&hooks);

    CHECK(StartEvents_Event(NULL, 77, NULL, NULL));
    CHECK(hooks.eventId == 77u);
    CHECK(hooks.eventStarts);

    CHECK(StartEvents_Event(NULL, 78, NULL, NULL, false));
    CHECK(hooks.eventId == 78u);
    CHECK(!hooks.eventStarts);
}
