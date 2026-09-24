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

/// Decoupling D5d (server #136): Unit's aura storage is an AuraContainer.
///
/// An `Aura` cannot be constructed in this binary -- its constructor reaches `sSpellStore`,
/// whose identity assert is fatal without a loaded DBC set -- and neither can a
/// `SpellAuraHolder`. That is exactly why the container is testable: it stores the two
/// pointer types and never dereferences, copies through, or deletes what it stores, so
/// pointers to sentinel bytes drive every operation it has. Nothing here ever reads a byte
/// of `slots`; the addresses are the whole point.
///
/// What is pinned is the behaviour `Unit` relies on and could not otherwise prove without a
/// live world: insertion order inside one spell id, the erasure-safe update walk and its
/// rule, and the deferred-deletion lists' order and emptiness.

#include "TestHarness.h"
#include "spells/AuraContainer.h"

#include <vector>

alignas(16) static unsigned char slots[8][16];

static SpellAuraHolder* Holder(int i)
{
    return reinterpret_cast<SpellAuraHolder*>(slots[i]);
}

static Aura* AuraAt(int i)
{
    return reinterpret_cast<Aura*>(slots[i]);
}

TEST(AuraContainer_BoundsKeepInsertionOrder)
{
    AuraContainer c;
    c.AddHolder(100, Holder(0));
    c.AddHolder(200, Holder(1));
    c.AddHolder(100, Holder(2));

    AuraContainer::HolderBounds bounds = c.Bounds(100);
    std::vector<SpellAuraHolder*> found;
    for (AuraContainer::HolderMap::iterator it = bounds.first; it != bounds.second; ++it)
    {
        found.push_back(it->second);
    }

    CHECK_EQ(found.size(), size_t(2));
    CHECK(found[0] == Holder(0));
    CHECK(found[1] == Holder(2));

    // the other id is untouched, and a missing id is an empty range
    CHECK(c.Bounds(200).first->second == Holder(1));
    CHECK(c.Bounds(300).first == c.Bounds(300).second);
    CHECK_EQ(c.Holders().size(), size_t(3));
}

TEST(AuraContainer_UpdateWalkVisitsEveryHolderOnce)
{
    AuraContainer c;
    c.AddHolder(10, Holder(0));
    c.AddHolder(20, Holder(1));
    c.AddHolder(20, Holder(2));
    c.AddHolder(30, Holder(3));

    std::vector<SpellAuraHolder*> seen;
    for (SpellAuraHolder* h = c.BeginUpdate(); h; h = c.NextUpdate())
    {
        seen.push_back(h);
    }

    CHECK_EQ(seen.size(), size_t(4));
    CHECK(seen[0] == Holder(0));
    CHECK(seen[1] == Holder(1));
    CHECK(seen[2] == Holder(2));
    CHECK(seen[3] == Holder(3));

    // the walk leaves the iterator at end(), which is what Unit::RemoveSpellAuraHolder's
    // rule relies on outside an update
    CHECK(c.NextUpdate() == NULL);

    // an empty container walks zero times
    AuraContainer empty;
    CHECK(empty.BeginUpdate() == NULL);
}

TEST(AuraContainer_EraseUnderUpdateIteratorResumesAtTheNext)
{
    // Unit::Update's hazard: a holder's UpdateHolder() removes the holder the walk is
    // standing on. NextUpdate() has already returned Holder(0) and moved on to Holder(1),
    // so the entry at risk is Holder(1) -- erasing it must leave the walk on Holder(2),
    // neither skipping it nor repeating Holder(1).
    AuraContainer c;
    c.AddHolder(10, Holder(0));
    c.AddHolder(20, Holder(1));
    c.AddHolder(30, Holder(2));
    c.AddHolder(40, Holder(3));

    SpellAuraHolder* first = c.BeginUpdate();
    CHECK(first == Holder(0));

    AuraContainer::HolderBounds bounds = c.Bounds(20);
    CHECK(bounds.first != bounds.second);
    c.EraseHolder(bounds.first);
    CHECK_EQ(c.Holders().size(), size_t(3));

    std::vector<SpellAuraHolder*> rest;
    for (SpellAuraHolder* h = c.NextUpdate(); h; h = c.NextUpdate())
    {
        rest.push_back(h);
    }

    CHECK_EQ(rest.size(), size_t(2));
    CHECK(rest[0] == Holder(2));
    CHECK(rest[1] == Holder(3));
}

TEST(AuraContainer_EraseElsewhereLeavesTheWalkAlone)
{
    AuraContainer c;
    c.AddHolder(10, Holder(0));
    c.AddHolder(20, Holder(1));
    c.AddHolder(30, Holder(2));
    c.AddHolder(40, Holder(3));

    CHECK(c.BeginUpdate() == Holder(0));

    // a LATER element: the iterator is on Holder(1), so erasing Holder(3) must not move it
    AuraContainer::HolderBounds later = c.Bounds(40);
    c.EraseHolder(later.first);

    std::vector<SpellAuraHolder*> rest;
    for (SpellAuraHolder* h = c.NextUpdate(); h; h = c.NextUpdate())
    {
        rest.push_back(h);
    }

    CHECK_EQ(rest.size(), size_t(2));
    CHECK(rest[0] == Holder(1));
    CHECK(rest[1] == Holder(2));

    // and an EARLIER element, one the walk has already handed out, is equally harmless
    AuraContainer d;
    d.AddHolder(10, Holder(0));
    d.AddHolder(20, Holder(1));
    d.AddHolder(30, Holder(2));

    CHECK(d.BeginUpdate() == Holder(0));
    AuraContainer::HolderBounds earlier = d.Bounds(10);
    d.EraseHolder(earlier.first);

    CHECK(d.NextUpdate() == Holder(1));
    CHECK(d.NextUpdate() == Holder(2));
    CHECK(d.NextUpdate() == NULL);
}

TEST(AuraContainer_TakeDeferredReturnsBothListsInOrderAndEmpties)
{
    AuraContainer c;
    CHECK(c.DeferredEmpty());

    c.DeferDelete(AuraAt(0));
    CHECK(!c.DeferredEmpty());
    c.DeferDelete(Holder(1));
    c.DeferDelete(AuraAt(2));
    c.DeferDelete(Holder(3));
    c.DeferDelete(Holder(4));

    AuraContainer::AuraList auras;
    AuraContainer::HolderList holders;
    c.TakeDeferred(auras, holders);

    CHECK_EQ(auras.size(), size_t(2));
    AuraContainer::AuraList::const_iterator a = auras.begin();
    CHECK(*a++ == AuraAt(0));
    CHECK(*a++ == AuraAt(2));

    CHECK_EQ(holders.size(), size_t(3));
    AuraContainer::HolderList::const_iterator h = holders.begin();
    CHECK(*h++ == Holder(1));
    CHECK(*h++ == Holder(3));
    CHECK(*h++ == Holder(4));

    // Unit::~Unit asserts on this, and Unit::CleanupDeletedAuras deletes what it took: the
    // container must be empty the moment it hands the lists over, or something is deleted
    // twice.
    CHECK(c.DeferredEmpty());

    AuraContainer::AuraList again;
    AuraContainer::HolderList againHolders;
    c.TakeDeferred(again, againHolders);
    CHECK(again.empty());
    CHECK(againHolders.empty());
}

TEST(AuraContainer_ByTypeHoldsWhatWasAddedAndDropsWhatWasRemoved)
{
    AuraContainer c;
    CHECK(c.ByType(SPELL_AURA_MOD_STUN).empty());

    c.ByType(SPELL_AURA_MOD_STUN).push_back(AuraAt(0));
    c.ByType(SPELL_AURA_MOD_STUN).push_back(AuraAt(1));
    c.ByType(SPELL_AURA_MOD_ROOT).push_back(AuraAt(2));

    AuraContainer const& ro = c;
    CHECK_EQ(ro.ByType(SPELL_AURA_MOD_STUN).size(), size_t(2));
    CHECK(ro.ByType(SPELL_AURA_MOD_STUN).front() == AuraAt(0));
    CHECK(ro.ByType(SPELL_AURA_MOD_STUN).back() == AuraAt(1));
    CHECK_EQ(ro.ByType(SPELL_AURA_MOD_ROOT).size(), size_t(1));

    // Unit::RemoveAura's spelling
    c.ByType(SPELL_AURA_MOD_STUN).remove(AuraAt(0));
    CHECK_EQ(ro.ByType(SPELL_AURA_MOD_STUN).size(), size_t(1));
    CHECK(ro.ByType(SPELL_AURA_MOD_STUN).front() == AuraAt(1));

    // the other types are untouched, including the last slot of the array
    CHECK_EQ(ro.ByType(SPELL_AURA_MOD_ROOT).size(), size_t(1));
    CHECK(ro.ByType(AuraType(TOTAL_AURAS - 1)).empty());
    c.ByType(AuraType(TOTAL_AURAS - 1)).push_back(AuraAt(3));
    CHECK_EQ(ro.ByType(AuraType(TOTAL_AURAS - 1)).size(), size_t(1));

    // per-type lists and the holder map are separate stores
    CHECK(c.Holders().empty());
    CHECK(c.DeferredEmpty());
}
