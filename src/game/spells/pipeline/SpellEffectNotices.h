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

#ifndef MANGOS_H_SPELLEFFECTNOTICES
#define MANGOS_H_SPELLEFFECTNOTICES

#include "ObjectGuid.h"
#include "Platform/Define.h"

#include <vector>

class Player;
class Spell;
class Unit;
struct WorldLocation;

/**
 * What a spell's effects tell the clients, one call per notice: the units around the caster hear
 * its kill, dispel and steal logs, the units around a target that it is no longer targetable, and a
 * player what an effect did to it or for it. The session installs the notices that send them at
 * start-up; this base sends nothing.
 */
class SpellEffectNotices
{
    public:
        virtual ~SpellEffectNotices();

        /// `spell` kills `victim` outright; the log names the caster `casterGuid`, empty for an
        /// object's cast.
        virtual void InstaKillLog(Spell const& spell, ObjectGuid casterGuid, Unit const& victim);
        /// `spell` dispels `spellIds` from `victim`.
        virtual void DispelLog(Spell const& spell, Unit const& victim, std::vector<uint32> const& spellIds);
        /// `spell` fails to dispel `spellIds` from `victim`.
        virtual void DispelFailed(Spell const& spell, Unit const& victim, std::vector<uint32> const& spellIds);
        /// `spell` steals `spellIds` from `victim`.
        virtual void StealLog(Spell const& spell, Unit const& victim, std::vector<uint32> const& spellIds);
        /// The units around `target`, but not `target` itself, clear it as their target.
        virtual void ClearTarget(Unit const& target);
        /// `challenger` and `challenged` are asked to the duel `challenger` starts under the flag `flag`.
        virtual void DuelRequested(Player& challenger, Player& challenged, ObjectGuid flag);
        /// `owner` is told its totem `totem` stands in `slot` for `duration` milliseconds, summoned by
        /// the spell `spellId`.
        virtual void TotemCreated(Player& owner, uint8 slot, ObjectGuid totem, uint32 duration, uint32 spellId);
        /// `player` learned a taxi node, and the flight master `source` now shows it as known.
        virtual void TaxiNodeLearned(Player& player, ObjectGuid source);
        /// `player`'s home is now `home`, in the area `areaId`, bound by `binder`.
        virtual void HomeBound(Player& player, ObjectGuid binder, WorldLocation const& home, uint32 areaId);

        /// The installed notices, or the base when none are installed.
        static SpellEffectNotices& Installed();
        /// Installs `notices` and answers the ones it replaces (NULL for the base). NULL installs the
        /// base. Installed once at start-up, before the world loads.
        static SpellEffectNotices* Install(SpellEffectNotices* notices);
};

#define sSpellEffectNotices SpellEffectNotices::Installed()

#endif
