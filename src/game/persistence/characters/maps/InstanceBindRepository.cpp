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

#include "InstanceBindRepository.h"
#include "Database/DatabaseEnv.h"

namespace InstanceBindRepository
{
    void InsertBind(uint32 guid, uint32 instanceId, bool permanent)
    {
        CharacterDatabase.PExecute("INSERT INTO `character_instance` (`guid`, `instance`, `permanent`) VALUES ('%u', '%u', '%u')",
                                   guid, instanceId, permanent);
    }

    void UpdateBind(uint32 newInstanceId, bool permanent, uint32 guid, uint32 oldInstanceId)
    {
        CharacterDatabase.PExecute("UPDATE `character_instance` SET `instance` = '%u', `permanent` = '%u' WHERE `guid` = '%u' AND `instance` = '%u'",
                                   newInstanceId, permanent, guid, oldInstanceId);
    }

    void DeleteBind(uint32 guid, uint32 instanceId)
    {
        CharacterDatabase.PExecute("DELETE FROM `character_instance` WHERE `guid` = '%u' AND `instance` = '%u'", guid, instanceId);
    }

    void CopyBindsToGroup(uint32 guid)
    {
        CharacterDatabase.PExecute("INSERT INTO `group_instance` SELECT `guid`, `instance`, `permanent` FROM `character_instance` WHERE `guid` = '%u'", guid);
    }

    void DeleteTemporaryBinds(uint32 guid)
    {
        CharacterDatabase.PExecute("DELETE FROM `character_instance` WHERE `guid` = '%u' AND `permanent` = 0", guid);
    }
}
