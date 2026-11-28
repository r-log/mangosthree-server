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

#ifndef MANGOSSERVER_SPELLS_CLASSES_PRIEST_SILENCE_H
#define MANGOSSERVER_SPELLS_CLASSES_PRIEST_SILENCE_H

#include "spells/classes/priest/Priest.h"

/**
 * @file Silence.h
 * @brief Silence: the interrupt a silenced non-player takes: the leaves of Silence.cpp.
 *
 * Silence (15487) silences its target for 5 s, and its text adds "Non-player victim spellcasting is also interrupted
 * for $32747d". Silence.cpp registers one row, the aura-apply row of 15487: once the holder stands on a target that is
 * no player (SilenceInterrupts), the priest casts the interrupt (32747, an interrupt-cast effect and 3 s) at it,
 * triggered, as his own cast; the effect ends the target's cast in progress. Its school lock is the player's
 * (Player::ProhibitSpellSchool; a creature's is empty), so on a creature the lock waits on the engine, and
 * Silence's own aura already stops the cast. A player is silenced alone, as before.
 */

namespace Priest
{
    /// Registers Silence's aura-apply row on `registry`; answers 1, the rows registered.
    uint32 RegisterSilence(SpellHandlerRegistry& registry);

    /// Whether a silenced unit of type `targetType` (Object::GetTypeId) is also interrupted: every type but a player.
    bool SilenceInterrupts(uint8 targetType);
}

#endif
