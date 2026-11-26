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

#ifndef MANGOSSERVER_SPELLS_CLASSES_PRIEST_DIVINETOUCH_H
#define MANGOSSERVER_SPELLS_CLASSES_PRIEST_DIVINETOUCH_H

#include "spells/classes/priest/Priest.h"

/**
 * @file DivineTouch.h
 * @brief Divine Touch, the Holy talent (63534, 63542): which events start it and the heal it casts: the leaves of
 * DivineTouch.cpp.
 *
 * DivineTouch.cpp registers a proc row on both ranks. The talent's proc row (spell_proc_event 63534, which the engine
 * gives 63542 too) lets the priest's helpful hits of Renew's class mask through; the script answers a hit of Renew
 * (139) that landed (DivineTouchFrom) and nothing else. The priest then casts Divine Touch's heal (63544) at the unit
 * his Renew hit: his rank's percent (effect 0 of the rank, 5 or 10 in the client's data) of his Renew's tick there as
 * it will heal, his spell power and his healing-done percent included and before a crit, times retail's 4.44
 * (DivineTouchHeal): every heal in the retail capture is 0.444 of the tick at rank 2, at both hastes it shows (a tick
 * of 2422 healed 1076), and the heal takes no done bonus of its own. The heal is cast triggered by the rank's aura;
 * the engine lets its hit start procs
 * (TriggeredCastStartsProcs), so Echo of Light follows it. Its own hit never starts Divine Touch again: the proc row's
 * Renew mask cannot match the heal's class mask, and the script answers Renew alone.
 */

namespace Priest
{
    /// Registers Divine Touch's rows on `registry`: the proc row of each rank's aura (63534, 63542); answers 2, the
    /// rows registered.
    uint32 RegisterDivineTouch(SpellHandlerRegistry& registry);

    /// Whether an event of `procFlag` and `procEx` for the spell `spellId` starts Divine Touch: a hit of Renew (139),
    /// normal or critical, of a helpful spell (PROC_FLAG_SUCCESSFUL_POSITIVE_SPELL). Renew's cast end, its ticks
    /// (PROC_FLAG_ON_DO_PERIODIC or PROC_EX_PERIODIC_POSITIVE), a Renew that did not land, and the event of any other
    /// spell, Divine Touch's own heal (63544) among them, do not.
    bool DivineTouchFrom(uint32 procFlag, uint32 procEx, uint32 spellId);

    /// The heal of Divine Touch for a Renew tick of `tick` at a rank of `percent`: `tick` x `percent` / 100 x retail's
    /// 4.44, rounded to the nearest, a half up, and held to the largest int32; 0 when either is 0 or less. Haste does
    /// not change it.
    int32 DivineTouchHeal(int32 tick, int32 percent);
}

#endif
