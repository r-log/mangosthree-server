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

#ifndef MANGOSSERVER_SPELLS_CLASSES_PRIEST_SHADOWFORM_H
#define MANGOSSERVER_SPELLS_CLASSES_PRIEST_SHADOWFORM_H

#include "spells/classes/priest/Priest.h"

/**
 * @file Shadowform.h
 * @brief Shadowform's look: the leaf of Shadowform.cpp.
 *
 * The 4.3.4 client shows a priest in Shadowform with a second aura beside the form: 107903, or 107904 for a priest who
 * holds Glyph of Shadow (107906, "Alters the appearance of your Shadowform"). Shadowform.cpp registers the two rows
 * that put it there: the aura-apply row of Shadowform (15473) casts the look at the priest, and its aura-remove row
 * takes both looks off him when the form ends, however it ends.
 */

namespace Priest
{
    /// Registers Shadowform's aura-apply and aura-remove rows on `registry`; answers 2, the rows registered.
    uint32 RegisterShadowform(SpellHandlerRegistry& registry);

    /// The look a priest's form shows: 107904 when he holds Glyph of Shadow (`glyphed`), 107903 when he does not.
    uint32 ShadowformLook(bool glyphed);
}

#endif
