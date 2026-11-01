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

#ifndef MANGOS_H_PLAYERSPELLNOTICES
#define MANGOS_H_PLAYERSPELLNOTICES

#include "ItemPrototype.h"
#include "ObjectGuid.h"
#include "Platform/Define.h"
#include "SharedDefines.h"

#include <vector>

class CharmInfo;
class Pet;
class Player;

/**
 * What a player's spells tell its own client, one call per notice: its spell book, its action bars,
 * its proficiencies and its cooldowns, its pet's and its charm's spells, a failed taming and its
 * talent resets. The player keeps when each is told. The session installs the
 * notices that send them at start-up; this base sends nothing.
 */
class PlayerSpellNotices
{
    public:
        virtual ~PlayerSpellNotices();

        /// `player` is told the spell `newId` takes the place of `oldId` in its spell book and action bars.
        virtual void SpellSuperseded(Player const& player, uint32 oldId, uint32 newId);
        /// `player` is told the spell `spellId` left its spell book.
        virtual void SpellRemoved(Player const& player, uint32 spellId);
        /// `player` is told it learned the spell `spellId`.
        virtual void SpellLearned(Player const& player, uint32 spellId);
        /// `player` is told its action buttons, one packed value per button (0 for an empty one).
        virtual void ActionButtonsShown(Player const& player, std::vector<uint32> const& packed);
        /// `player`'s action bars are locked until its buttons are sent again.
        virtual void ActionButtonsLocked(Player const& player);
        /// `player` is told every active spell it knows and every cooldown it has.
        virtual void InitialSpellsShown(Player const& player);
        /// `player` is told the item subclasses `itemSubclassMask` of `itemClass` it is proficient in.
        virtual void ProficiencyShown(Player const& player, ItemClass itemClass, uint32 itemSubclassMask);
        /// `player` is told the item `item`'s spell `spellId` is cooling down.
        virtual void ItemCooldownStarted(Player const& player, ObjectGuid item, uint32 spellId);
        /// `player` is told the cooldown of the spell `spellId` of the unit `unit` is cleared.
        virtual void CooldownCleared(Player const& player, ObjectGuid unit, uint32 spellId);
        /// `player` is told the spell bar, spells and cooldowns of its pet `pet`.
        virtual void PetSpellsShown(Player const& player, Pet& pet);
        /// `player` is told the action bar of the unit `charm` it charms (a player when `charmIsPlayer`),
        /// from `info`, with its first `charmSpellCount` charm spells.
        virtual void CharmSpellsShown(Player const& player, ObjectGuid charm, bool charmIsPlayer, CharmInfo& info, uint8 charmSpellCount);
        /// `player` is told its taming failed for `reason`.
        virtual void PetTameFailed(Player const& player, PetTameFailureReason reason);
        /// `player` is told its talents, or its pet's when `pet` is true, were reset without its asking.
        virtual void TalentsInvoluntarilyReset(Player const& player, bool pet);
        /// `player` is asked to confirm resetting its talents for `cost` at `source`.
        virtual void TalentWipeConfirm(Player const& player, ObjectGuid source, uint32 cost);
        /// `player` is asked to confirm resetting its pet `pet`'s talents for `cost`.
        virtual void PetUnlearnConfirm(Player const& player, ObjectGuid pet, uint32 cost);

        /// The installed notices, or the base when none are installed.
        static PlayerSpellNotices& Installed();
        /// Installs `notices` and answers the ones it replaces (NULL for the base). NULL installs the
        /// base. Installed once at start-up, before the world loads.
        static PlayerSpellNotices* Install(PlayerSpellNotices* notices);
};

#define sPlayerSpellNotices PlayerSpellNotices::Installed()

#endif
