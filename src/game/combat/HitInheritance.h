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

#ifndef MANGOSSERVER_COMBAT_HITINHERITANCE_H
#define MANGOSSERVER_COMBAT_HITINHERITANCE_H

#include "Platform/Define.h"

class Unit;

namespace Combat
{
    /// Which of the owner's three hit chances his pets are proportioned from.
    enum HitSource
    {
        HIT_SOURCE_MELEE,
        HIT_SOURCE_RANGED,
        HIT_SOURCE_SPELL
    };

    /// The hit chances, in percent, a unit needs to never miss a target three levels above it.
    struct HitCaps
    {
        float melee;                                        ///< also the ranged cap
        float spell;
    };

    /// A hit chance of an owner with its cap.
    struct HitKind
    {
        HitSource source;
        float cap;
    };

    /// What an owned creature takes from its owner's hit: added to its own hit chances and expertise.
    struct OwnerHitShare
    {
        OwnerHitShare() : spell(0.0f), melee(0.0f), ranged(0.0f), expertise(0) {}

        float spell;                                        ///< percent
        float melee;                                        ///< percent
        float ranged;                                       ///< percent
        int32 expertise;                                    ///< hundredths of a percent taken off dodge and parry
    };

    /// An owner's share taken once when a temporary summon is made and read from then on.
    struct OwnerHitSnapshot
    {
        OwnerHitSnapshot() : taken(false), share() {}

        bool taken;
        OwnerHitShare share;
    };

    /**
     * @brief The hit chances that cap a unit against a target three levels above it.
     *
     * Computed from MagicSpellBaseHitChance and MeleeMissChance for a unit of one level against a target three
     * levels up: each cap is the miss chance there.
     */
    HitCaps BossHitCaps();

    /**
     * @brief The hit chance an owner's pets are proportioned from, with its cap.
     *
     * @param ownerClass The owner's class.
     * @param meleeTree True for a shaman whose primary talent tree is the melee one.
     * @param caps BossHitCaps().
     * @return A hunter's ranged hit; a warrior's, paladin's, rogue's and death knight's melee hit; a shaman's
     *         melee hit when meleeTree; every other class's spell hit.
     */
    HitKind OwnerHitKind(uint8 ownerClass, bool meleeTree, HitCaps const& caps);

    /**
     * @brief A pet's hit chance of one kind from its owner's.
     *
     * @param ownerPercent The owner's hit chance of his kind, in percent.
     * @param ownerCap The cap of the owner's kind.
     * @param petCap The cap of the kind the pet rolls with.
     * @return ownerPercent x petCap / ownerCap, with ownerPercent counted up to ownerCap at most; 0 for an
     *         ownerCap of 0 or below.
     */
    float InheritedHit(float ownerPercent, float ownerCap, float petCap);

    /**
     * @brief A pet's expertise from its owner's hit chance.
     *
     * @param ownerPercent The owner's hit chance of his kind, in percent.
     * @param ownerCap The cap of the owner's kind.
     * @return The dodge and parry reduction in hundredths of a percent, as Unit::GetMeleeSpellExpertiseReduction
     *         returns it: the expertise cap at the owner's cap, in proportion below it; 0 for an ownerCap of 0
     *         or below.
     */
    int32 InheritedExpertise(float ownerPercent, float ownerCap);

    /**
     * @brief What a pet takes from one hit chance of its owner.
     *
     * @param kind OwnerHitKind of the owner.
     * @param ownerPercent The owner's hit chance of that kind.
     * @param caps BossHitCaps().
     * @return The spell, melee and ranged hit chances and the expertise the pet has from it.
     */
    OwnerHitShare InheritedShare(HitKind const& kind, float ownerPercent, HitCaps const& caps);

    /**
     * @brief The share a unit rolls with.
     *
     * @param snapshot The unit's snapshot.
     * @param live The share of its owner's hit chances now.
     * @return The snapshot's share when it was taken, else the live one.
     */
    OwnerHitShare ChosenShare(OwnerHitSnapshot const& snapshot, OwnerHitShare const& live);

    /**
     * @brief The player whose hit a unit rolls with.
     *
     * @return The player that owns a creature, found by guid; NULL for a player, a creature with no owner or
     *         with a creature as owner, a creature charmed by someone other than its owner, and an owner that is
     *         not on the map.
     */
    Unit const* RatingOwner(Unit const& unit);

    /**
     * @brief What an owned creature takes from its owner's hit.
     *
     * @return The snapshot's share when the creature took one, else the share of its player owner's hit now;
     *         no share when the unit has no player owner.
     */
    OwnerHitShare OwnerShareOf(Unit const& unit);

    /**
     * @brief Takes a creature's snapshot of its owner's share: the one read of the owner's hit a temporary
     *        summon makes.
     *
     * Does nothing for a unit that is not a creature or has no player owner.
     */
    void TakeOwnerHitSnapshot(Unit& unit);
}

#endif
