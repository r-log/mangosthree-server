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

#ifndef MANGOS_H_CORPSEDECAY
#define MANGOS_H_CORPSEDECAY

#include "Platform/Define.h"

/// A creature corpse when its loot changes: its timers and what is left on it.
struct CorpseLootState
{
    uint32 remainingMs = 0;         ///< time the corpse has left
    uint32 corpseDelayMs = 0;       ///< the corpse's full delay, Corpse.Decay.* for its rank
    uint32 untilRespawnMs = 0;      ///< time until the creature is due to respawn
    bool ignoreLootedRate = false;  ///< an emptied corpse keeps its time
    bool lootable = false;          ///< loot still waits on the corpse
    bool skinnable = false;         ///< it can still be skinned, herbed, mined or salvaged
    bool gathered = false;          ///< its skinning, herb, ore or salvage loot has been taken
};

/// The corpse's new countdown in milliseconds. A corpse with loot keeps its time. One that can
/// still be gathered from gets its full delay again, never past the respawn. A gathered one goes
/// at once. An emptied one keeps at most 120 s, or at most its full delay times `lootedRate` when
/// that is not 0.
uint32 CorpseDecayAfterLoot(CorpseLootState const& corpse, float lootedRate);

#endif
