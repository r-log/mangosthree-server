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

#ifndef MANGOS_H_DBSCRIPTHOOKS
#define MANGOS_H_DBSCRIPTHOOKS

#include "Platform/Define.h"
#include "ObjectGuid.h"
#include "data/DbScriptStore.h"

class Map;

/// One step of a database script that a map has scheduled: the script's type, the step, and the
/// objects the script was started for (the owner is the source item's owner).
class DbScriptStep
{
    public:
        DbScriptStep(DBScriptType _type, ObjectGuid _sourceGuid, ObjectGuid _targetGuid, ObjectGuid _ownerGuid, ScriptInfo const* _script) :
            m_type(_type), m_sourceGuid(_sourceGuid), m_targetGuid(_targetGuid), m_ownerGuid(_ownerGuid), m_script(_script)
        {}

        DBScriptType GetType() const
        {
            return m_type;
        }
        uint32 GetId() const
        {
            return m_script->id;
        }
        ObjectGuid GetSourceGuid() const
        {
            return m_sourceGuid;
        }
        ObjectGuid GetTargetGuid() const
        {
            return m_targetGuid;
        }
        ObjectGuid GetOwnerGuid() const
        {
            return m_ownerGuid;
        }

        bool IsSameScript(DBScriptType type, uint32 id, ObjectGuid sourceGuid, ObjectGuid targetGuid, ObjectGuid ownerGuid) const
        {
            return type == m_type && id == GetId() &&
                   (sourceGuid == m_sourceGuid || !sourceGuid) &&
                   (targetGuid == m_targetGuid || !targetGuid) &&
                   (ownerGuid == m_ownerGuid || !ownerGuid);
        }
        ScriptInfo const* GetScript() const
        {
            return m_script;
        }

    private:
        DBScriptType m_type;                                // which type has the script was started
        ObjectGuid m_sourceGuid;
        ObjectGuid m_targetGuid;
        ObjectGuid m_ownerGuid;                             // owner of source if source is item
        ScriptInfo const* m_script;                         // pointer to static script data
};

/**
 * What the domain asks of the database script engine, which sits above it (`ScriptMgr`,
 * `ScriptAction`): the run of a step a map has scheduled. mangosd installs the engine's hooks
 * (`ScriptMgr::GetDbScriptHooks`) at start-up; the core calls the installed ones through
 * sDbScriptHooks. This base answers for a core with no engine: a step runs nothing and ends its
 * script.
 */
class DbScriptHooks
{
    public:
        virtual ~DbScriptHooks();

        /// Runs `step` on `map`. True when the script's following steps are to be dropped.
        virtual bool RunStep(Map* map, DbScriptStep const& step);

        /// The installed hooks, or the base when none are installed.
        static DbScriptHooks& Installed();
        /// Installs `hooks` and answers the hooks it replaces (NULL for the base). NULL installs
        /// the base. Installed once at start-up, before the world loads.
        static DbScriptHooks* Install(DbScriptHooks* hooks);
        /// Whether hooks are installed.
        static bool IsInstalled();
};

#define sDbScriptHooks DbScriptHooks::Installed()

#endif
