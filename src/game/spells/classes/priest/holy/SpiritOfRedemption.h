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

#ifndef MANGOSSERVER_SPELLS_CLASSES_PRIEST_SPIRITOFREDEMPTION_H
#define MANGOSSERVER_SPELLS_CLASSES_PRIEST_SPIRITOFREDEMPTION_H

#include "spells/classes/priest/Priest.h"

/**
 * @file SpiritOfRedemption.h
 * @brief Spirit of Redemption's form (27827), as the retail 4.3.4 sniff (build 15595, 2012-05-28) shows it: the
 * immunity the form carries and the stun it ends in: the leaves of SpiritOfRedemption.cpp.
 *
 * Unit::DealDamage casts the form, 27827 for 15 seconds, at a priest who holds the talent (20711) at his killing
 * blow (combat/UnitDamage.cpp). The form's shapeshift effect casts 27792 and then 27795 (Aura::HandleShapeshiftBoosts).
 * 27792 roots him, makes him unattackable and takes the mana cost off his Holy spells; 27795 pacifies him and holds
 * aura 176, whose removal kills him (Aura::HandleSpiritOfRedemption): that is the form's end. The sniff shows 27827
 * cast at the death, then 27795 and 62371 (packets 51739 and 51740), and 99755 at the end (packet 52943), 15.0
 * seconds after the cast; 27792 is never in it. SpiritOfRedemption.cpp adds the two spells the engine lacked and keeps
 * 27792 until a live test shows the sniff's set alone roots the priest and makes him unattackable:
 *  - an aura-apply row on 27827: once the form is on him and its boosts have cast 27792 and 27795, he casts 62371, the
 *    immunity to the damage of every school, at himself. 62371 is a passive, so Unit::RemoveAllAurasOnDeath would not
 *    take it off his corpse;
 *  - an aura-remove row on 27792: the form ends by taking its boosts off in the order they were cast, 27792 first and
 *    then 27795, whose aura 176 kills him inside its own removal, so this is the last moment he is alive: he takes
 *    62371 off, as while it stands the stun misses him as immune (Unit::SpellHitResult), then casts 99755, the end's
 *    stun, at himself (SpiritOfRedemptionStuns). The form's own removal row would run after the holder's auras,
 *    27795's among them, and so after his death;
 *  - an aura-remove row on 27827: 62371 leaves him with the form, whatever takes the form off, when the root's row
 *    has not taken it already.
 * 27792, 27795, 62371 and 99755 share a name and the spell icon 1 but are no chain of ranks, and the no-stack rules
 * never set a passive beside a spell that is not one (SpellMgr.cpp, IsNoStackSpellDueToSpell) or beside another
 * passive that is no rank of it (Unit::RemoveNoStackAurasDueToAuraHolder), so no pair needs an exemption: 99755 is the
 * one that is no passive, and it meets only the auras of the priest that the death takes off a moment later.
 * 25100 (Untransform Hero, a script effect) is cosmetic and is not cast.
 */

namespace Priest
{
    /// Registers Spirit of Redemption's rows on `registry`: the aura-apply row of the form (27827) and the aura-remove
    /// rows of its root (27792) and of the form; answers 3, the rows registered.
    uint32 RegisterSpiritOfRedemption(SpellHandlerRegistry& registry);

    /// Whether the end of the form stuns the priest: when he is alive (`alive` and `health` above 0) and in the world
    /// (`inWorld`), and no longer holds the form (`holdsForm` false). Not for one who has died already, whose stun
    /// would find no one to hold it, nor for one who stands off the map, nor while the form is still on him.
    bool SpiritOfRedemptionStuns(bool alive, uint32 health, bool inWorld, bool holdsForm);
}

#endif
