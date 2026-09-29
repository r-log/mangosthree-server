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

#ifndef MANGOS_HARNESS_SPELL_RECORDER_H
#define MANGOS_HARNESS_SPELL_RECORDER_H

#include "Recorder.h"

#include <string>

class Player;
class Unit;

namespace Harness
{
    /// The units the spell recorder watches besides the player (who is always `self`): the unit a
    /// cast is aimed at and, for a scenario where something other than the player casts, that
    /// caster. An empty guid watches nothing in that role.
    struct SpellWatch
    {
        ObjectGuid target;
        ObjectGuid caster;
        /// The player's own pet (decoupling D11 PR 2, scenario 938), a guid of the map's pet store.
        ObjectGuid pet;
        /// Also read, for every watched creature, the stored keys of Creature's own cooldown model
        /// -- the spell map and the category map (Creature.h:841-842). Off unless a scenario asks,
        /// so a record made without it keeps every line it had.
        bool creatureCooldowns = false;
    };

    /**
     * The spell family's recorder (decoupling D11, the Unit reopening note's section 3(a)): the
     * Recorder core (Recorder.h -- the windows, the sink, the pkt, snap, call and state lines, the
     * digest) reading what a cast does to each watched unit, by its role -- never its guid.
     *
     * ITS STATE DELTA at each window's close, per role (`self`, `target`, `caster`):
     *  - `<role>.health` and `<role>.power` (the unit's power type, current and maximum);
     *  - `<role>.combat` (the in-combat flag) and `<role>.victim` (the role of its victim);
     *  - `<role>.spell.<slot>`: the four current-spell slots (melee, generic, autorepeat,
     *    channeled), each as the spell and its state, or "none";
     *  - `<role>.aura.<spell>#<k>`: every aura holder (the k-th holder of that spell, in the
     *    container's order) as its effect mask, stack, charges, caster role, visible slot and the
     *    remaining duration against the maximum -- the stepped world's milliseconds, never a wall
     *    clock (-1 for a permanent aura);
     *  - `<role>.slot.<nn>`: the visible aura slots, each as the spell its holder carries;
     *  - for a player, the id set `<role>.cooldowns`: the stored keys of the cooldown manager's map
     *    (its GetSpellCooldownMap), never HasSpellCooldown -- whose answer reads the
     *    wall clock the stepped world does not move.
     *  - for a creature, when the watch asks for it (SpellWatch::creatureCooldowns), the id sets
     *    `<role>.creatureSpellCooldowns` and `<role>.creatureCategoryCooldowns`: the stored keys
     *    of the two maps the pet and charm cast paths write (Creature::AddCreatureSpellCooldown),
     *    never their values, which are wall-clock times.
     * A unit that is not there reads `<role>=gone`. The roles are `self`, `target`, `caster`
     * and, since D11's PR 2, `pet`.
     *
     * ITS SNAP LINE after every digested packet (Ruling 19's rule): per role, the health, the
     * power, the combat flag, the generic and channeled slots' spells and the holder count -- so a
     * statement moved across a packet in the cast (the power taken before or after SPELL_GO, the
     * damage before or after its log) changes the digest.
     */
    class SpellRecorder : public Recorder
    {
    public:
        /// Installs the sink and opens the setup window `spawn`, whose close prints every watched
        /// unit's whole state as the delta from nothing.
        void Begin(char const* scenario, Player* player, SpellWatch const& watch);

    private:
        State Take() const override;
        std::string Mini() const override;
        /// The watched unit in `role`, on the player's map, or NULL.
        Unit* UnitIn(ObjectGuid guid) const;
        void TakeUnit(char const* role, Unit* unit, State& s) const;
        std::string MiniUnit(char const* role, Unit* unit) const;

        SpellWatch                  m_watch;
    };
}

#endif
