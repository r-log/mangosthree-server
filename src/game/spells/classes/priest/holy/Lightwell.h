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

#ifndef MANGOSSERVER_SPELLS_CLASSES_PRIEST_LIGHTWELL_H
#define MANGOSSERVER_SPELLS_CLASSES_PRIEST_LIGHTWELL_H

#include "spells/classes/priest/Priest.h"

/**
 * @file Lightwell.h
 * @brief Lightwell (724) and Glyph of Lightwell (55673): the Lightwell's creature, what a click on it gives, how many
 * clicks it holds and which hits cancel the heal: the leaves of Lightwell.cpp.
 *
 * Lightwell (724) has one effect that does anything in the 4.3.4 client's data: the summon of creature 31897, one
 * Lightwell (the effect's value is 1 and no scaling row raises it), for 3 minutes; its effect 1 holds 30, the percent
 * of his maximum health a hit must reach to cancel the heal. The world holds the creature's click row: a click casts
 * Lightwell Renew's script spell (60123) at the player who clicked, cast by the Lightwell. Lightwell.cpp registers two
 * rows and the Lightwell's creature AI:
 *  - LightwellAI, chosen by its permit for creature 31897 alone (IsLightwellCreature). On its first update a
 *    Lightwell that 724 made takes its summoner, the priest, as its owner and its creator, and casts Lightwell Charges
 *    (59907) at itself with the priest as the original caster: the aura's effects all target the caster, so the
 *    priest's own cast would put it on him, while the original caster is the aura's caster, whose spell modifiers
 *    raise its proc charges, 10 in the client's data, by the 5 of Glyph of Lightwell (55673). The Lightwell stands for
 *    724's duration, which the summon's own timer ends, or until its charges are gone (LightwellStands);
 *  - a dummy-effect row on 60123: the click. A Lightwell that stands with its priest on its map answers the player who
 *    clicked, when he is alive and friendly to the priest (LightwellServes), by casting Lightwell Renew (7001) at him
 *    with the priest as the original caster, and spends one of its charges. The priest is then the aura's caster: the
 *    amount reads his spell power (the bonus row's 0.308 a tick), the ticks roll his crit and are his heals to the
 *    procs of his Grace. The Lightwell's AI sends it away at its next update when the last charge is gone;
 *  - a check-proc row on 7001: the aura's own proc charge, 1, on the taken-damage flags of its data, removes it on a hit
 *    its holder takes, so the row lets through a hit of at least 724's effect 1 percent of his maximum health
 *    (LightwellHitCancels) and refuses a smaller one.
 * Nothing makes the Lightwell join its priest's combat: an owned creature is put in combat only by what it fights
 * itself, by its owner's pet or guardian links, or by the assist branch of a hit it casts at a unit in combat, which
 * 60123's flags (no initial aggro) stay out of, and the Lightwell is none of a pet, a guardian or an attacker; its AI
 * attacks nothing and evades nothing. A click on it is therefore never refused for its combat state.
 */

namespace Priest
{
    /// Registers the Lightwell's rows on `registry`, its dummy-effect row on 60123 and its check-proc row on 7001, and
    /// the Lightwell's creature AI, whose factory is added to the creature AI registry once; answers 2, the rows
    /// registered.
    uint32 RegisterLightwell(SpellHandlerRegistry& registry);

    /// Whether `creatureEntry` is the Lightwell's creature (31897).
    bool IsLightwellCreature(uint32 creatureEntry);

    /// Whether a Lightwell stands on: while it holds a charge of Lightwell Charges (`charges` above 0).
    bool LightwellStands(uint32 charges);

    /// Whether a click is answered: the Lightwell's priest is on its map (`priest`), the player who clicked is alive and
    /// friendly to him (`friendly`), and a charge is left (`charges` above 0).
    bool LightwellServes(bool priest, bool friendly, uint32 charges);

    /// Whether a hit of `damage` takes Lightwell Renew off the unit it heals, whose maximum health is `maxHealth`:
    /// when it is at least `percent` percent of it, where `percent` is 724's effect 1 value. Never when `percent` or
    /// `maxHealth` is 0.
    bool LightwellHitCancels(uint32 damage, uint32 maxHealth, int32 percent);
}

#endif
