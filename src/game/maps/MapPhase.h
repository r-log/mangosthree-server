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

#ifndef MANGOS_MAPPHASE_H
#define MANGOS_MAPPHASE_H

#include "Platform/Define.h"

#include <vector>

class Map;

/**
 * Who may touch a unit's movement kernel right now (design v2 §10.4, F21): the
 * map phase flag is the world's (Begin/End around sMapMgr.Update), but which
 * maps a thread owns is set by the map updates themselves, through Scope: a
 * thread owns the map of every Scope open on it. A vessel's deck is updated by
 * TransportMap::Update, nested inside the world map's own tick on that same
 * thread, so the deck's update owns the deck and the world map it interrupted,
 * and on return the world map's own Scope is the innermost again. A unit on a
 * deck is also owned by the thread updating the map its vessel sails
 * (OwnsUnitOn): a world map and the decks of the vessels sailing it are
 * updated on one thread. Outside any map's Update (the session phase, the
 * console) the world thread owns everything, because the phases never
 * overlap. The guard witnesses that barrier: a violation is counted
 * process-wide (and asserted under MANGOS_DEBUG), never silently allowed.
 */
namespace MapPhase
{
    void Begin();                  ///< World::Update: the maps are about to run
    void End();                    ///< every map is done (after MapUpdater::wait)

    /// RAII: the thread owns `map` for the scope's lifetime, together with the
    /// maps of the scopes already open on it. The constructor links the scope to
    /// the thread's innermost open scope and makes it the innermost; the
    /// destructor makes the linked scope the innermost again -- so a transport
    /// deck's nested Map::Update owns the deck and the world map it interrupted,
    /// and hands the thread back to the world map alone when its own Update
    /// returns.
    class Scope
    {
    public:
        explicit Scope(Map const* map);
        ~Scope();

    private:
        friend bool Owns(Map const* map);

        Map const* m_map;
        Scope const* m_outer;

        Scope(Scope const&) = delete;
        Scope& operator=(Scope const&) = delete;
    };

    bool Active();
    bool Owns(Map const* map);     ///< !Active(), or `map` is the map of a Scope open on this thread
    bool UpdatesAMap();            ///< a Scope is open on this thread
    /// Whether this thread owns a unit on `map`: Owns(map), or `sailed` is not NULL and Owns(sailed).
    /// `sailed` is the map the vessel of `map` sails when `map` is a deck, and NULL otherwise.
    bool OwnsUnitOn(Map const* map, Map const* sailed);
    uint32 Violations();           ///< process-wide, since start
    /// Counts one violation; the first ten are logged, naming what was touched (`subject`) and whose (`unit`).
    void Violation(char const* subject, char const* unit);
}

/// One aura holder's update that waits for the end of the map phase: the holder of the spell
/// `spellId` that `caster` cast on the unit `target` of the map `mapId`, instance `instanceId`, and
/// the time `diff` its update was given. The guids are raw values.
struct OffMapAuraUpdate
{
    uint32 mapId;
    uint32 instanceId;
    uint64 target;
    uint32 spellId;
    uint64 caster;
    uint32 diff;
};

/**
 * The aura holder updates that wait for the end of the map phase. A holder's update reads and
 * writes its caster; when the caster is a player on a map this thread does not update, the
 * player's own map may be updating that player on another thread. The target's map posts the holder's
 * update here instead, and the world thread runs it once every map is done
 * (World::ApplyOffMapAuraUpdates), with the same time.
 */
namespace OffMapAuraUpdates
{
    /// Whether an aura update whose caster stands on `casterMap` waits: this thread owns no unit
    /// there (`casterSailed`: the map the caster's vessel sails, NULL when the caster rides none).
    bool Waits(Map const* casterMap, Map const* casterSailed);
    /// Queues `update`; any thread may post.
    void Post(OffMapAuraUpdate const& update);
    /// The queued updates in the order they were posted, leaving the queue empty.
    std::vector<OffMapAuraUpdate> Take();
}

#endif
