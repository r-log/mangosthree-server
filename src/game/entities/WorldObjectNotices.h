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

#ifndef MANGOS_H_WORLDOBJECTNOTICES
#define MANGOS_H_WORLDOBJECTNOTICES

#include "ObjectGuid.h"
#include "Platform/Define.h"
#include "SharedDefines.h"

class Creature;
class GameObject;
class Player;
class Unit;
class WorldObject;

/**
 * What a world object shows the clients around it, one call per notice: its animations, sounds and
 * music, a unit's emote, power, mount and vehicle, a creature's reaction and a building's damage. The
 * object keeps when it is shown. The session installs the notices that send them at start-up; this
 * base sends nothing.
 */
class WorldObjectNotices
{
    public:
        virtual ~WorldObjectNotices();

        /// The units around `sender`, `sender` among them, see the object `guid` play its despawn animation.
        virtual void ObjectDespawnAnim(WorldObject const& sender, ObjectGuid guid);
        /// The units around `sender`, `sender` among them, see the game object `guid` play its animation
        /// `animId`.
        virtual void GameObjectCustomAnim(WorldObject const& sender, ObjectGuid guid, uint32 animId);
        /// `target`, or the units around `source` with `source` among them when there is none, hear the
        /// sound `soundId` from `source`.
        virtual void DistanceSoundPlayed(WorldObject const& source, uint32 soundId, Player const* target);
        /// `target`, or the units around `source` with `source` among them when there is none, hear the
        /// sound `soundId`.
        virtual void SoundPlayed(WorldObject const& source, uint32 soundId, Player const* target);
        /// `target`, or the units around `source` with `source` among them when there is none, hear the
        /// music `soundId`.
        virtual void MusicPlayed(WorldObject const& source, uint32 soundId, Player const* target);
        /// The units around `unit`, `unit` among them, see it play the emote `emoteId`.
        virtual void Emoted(Unit const& unit, uint32 emoteId);
        /// The units around `unit`, `unit` among them, are told its power `power` is now `value`.
        virtual void PowerUpdated(Unit const& unit, Powers power, uint32 value);
        /// The units around `caster`, `caster` among them, are told it gave `victim` `amount` of the power
        /// `power` with the spell `spellId`.
        virtual void Energized(Unit const& caster, Unit const& victim, uint32 spellId, uint32 amount, Powers power);
        /// The units around `unit`, `unit` among them, see it dismount.
        virtual void Dismounted(Unit const& unit);
        /// The units around `unit`, `unit` among them, see it play the animation kit `id` once.
        virtual void OneShotAnimKitPlayed(Unit const& unit, uint32 id);
        /// The units around `unit`, `unit` among them, are told its vehicle is now `entry` (0 for none).
        virtual void VehicleRecIdChanged(Unit const& unit, uint32 entry);
        /// The units around `creature`, `creature` among them, see its reaction `reaction`.
        virtual void AiReacted(Creature const& creature, AiReaction reaction);
        /// The units around `building`, but not `building` itself, are told `caster` (and the one that
        /// controls it) dealt it `damage` with the spell `spell`.
        virtual void BuildingDamaged(GameObject const& building, Unit& caster, uint32 damage, uint32 spell);

        /// The installed notices, or the base when none are installed.
        static WorldObjectNotices& Installed();
        /// Installs `notices` and answers the ones it replaces (NULL for the base). NULL installs the
        /// base. Installed once at start-up, before the world loads.
        static WorldObjectNotices* Install(WorldObjectNotices* notices);
};

#define sWorldObjectNotices WorldObjectNotices::Installed()

#endif
