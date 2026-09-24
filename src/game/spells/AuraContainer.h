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

#ifndef MANGOSSERVER_SPELLS_AURACONTAINER_H
#define MANGOSSERVER_SPELLS_AURACONTAINER_H

#include "Platform/Define.h"
#include "SpellAuraDefines.h"

#include <list>
#include <map>
#include <utility>

class Aura;
class SpellAuraHolder;

/**
 * @brief The aura storage a \ref Unit owns.
 *
 * Decoupling D5d (server #136), design rule F3: state before orchestration. These five
 * members used to sit on \ref Unit directly -- the holder multimap, the iterator
 * \ref Unit::Update walks it with, the two deferred-deletion lists and the per-type aura
 * array. They are one object now, and every operation on them is the same operation
 * through this class; nothing about iteration order, erasure order, deferred-deletion
 * order or the update-iterator rule changed when they moved.
 *
 * The container **never dereferences and never deletes** what it stores (design rule F6:
 * no deleting drain). `Aura` and `SpellAuraHolder` are incomplete here on purpose: the
 * holders and auras are owned, applied and deleted by \ref Unit exactly as before, so a
 * test can drive the whole class with pointers to bytes it never touches -- which is what
 * `src/tests/AuraContainerTest.cpp` does, an `Aura` being unconstructible in the test
 * binary.
 *
 * The one rule that is not plain storage is the **update-iterator rule**. \ref Unit::Update
 * walks the holder map with a member iterator rather than a local one, so that a holder
 * removed from inside another holder's update -- which happens, through proc chains and
 * triggered spells -- can push that iterator forward before the entry under it dies.
 * \ref EraseHolder is the only erase that applies the rule, and \ref BeginUpdate /
 * \ref NextUpdate are the walk it protects.
 */
class AuraContainer
{
    public:
        typedef std::multimap < uint32 /*spellId*/, SpellAuraHolder* > HolderMap;
        typedef std::pair<HolderMap::iterator, HolderMap::iterator> HolderBounds;
        typedef std::pair<HolderMap::const_iterator, HolderMap::const_iterator> HolderConstBounds;
        typedef std::list<Aura*> AuraList;
        typedef std::list<SpellAuraHolder*> HolderList;

        AuraContainer()
        {
            m_updateIterator = m_holders.end();
        }

        // ---- holders -------------------------------------------------------------

        /**
         * @brief Stores a holder under a spell id. Equal ids keep insertion order.
         */
        void AddHolder(uint32 spellId, SpellAuraHolder* holder)
        {
            m_holders.insert(HolderMap::value_type(spellId, holder));
        }

        /**
         * @brief Erases one holder entry, applying the update-iterator rule.
         *
         * If the update iterator points at the entry being erased it is shifted forward
         * first, so the walk in \ref BeginUpdate / \ref NextUpdate resumes at the entry
         * after the erased one instead of holding a dangling iterator.
         *
         * @param it The entry to erase; must be a valid entry of this container.
         * @return The iterator following the erased entry.
         */
        HolderMap::iterator EraseHolder(HolderMap::iterator it)
        {
            if (it == m_updateIterator)
            {
                ++m_updateIterator;
            }
            return m_holders.erase(it);
        }

        HolderBounds Bounds(uint32 spellId)
        {
            return m_holders.equal_range(spellId);
        }

        HolderConstBounds Bounds(uint32 spellId) const
        {
            return m_holders.equal_range(spellId);
        }

        /// The map itself, for the walkers that restart at begin() and for find().
        HolderMap&       Holders()       { return m_holders; }
        HolderMap const& Holders() const { return m_holders; }

        // ---- the Unit::Update walk, erasure-safe ---------------------------------

        /**
         * @brief Starts the update walk at the first holder.
         * @return The first holder, or NULL when there is none.
         */
        SpellAuraHolder* BeginUpdate()
        {
            m_updateIterator = m_holders.begin();
            return NextUpdate();
        }

        /**
         * @brief Takes the holder under the update iterator and shifts the iterator past it.
         *
         * The shift happens before the caller does anything with the holder, so a removal
         * triggered from inside that work cannot leave the iterator on a dead entry.
         *
         * @return The next holder, or NULL when the walk is over.
         */
        SpellAuraHolder* NextUpdate()
        {
            if (m_updateIterator == m_holders.end())
            {
                return NULL;
            }
            SpellAuraHolder* holder = m_updateIterator->second;
            ++m_updateIterator;
            return holder;
        }

        // ---- per-type lists -------------------------------------------------------

        AuraList&       ByType(AuraType type)       { return m_modAuras[type]; }
        AuraList const& ByType(AuraType type) const { return m_modAuras[type]; }

        // ---- deferred deletion (the container never deletes) ----------------------

        /// Queues an aura that was removed while in use; the owner deletes it later.
        void DeferDelete(Aura* aura)                { m_deletedAuras.push_back(aura); }
        /// Queues a holder that was removed while in use; the owner deletes it later.
        void DeferDelete(SpellAuraHolder* holder)   { m_deletedHolders.push_back(holder); }

        bool DeferredEmpty() const
        {
            return m_deletedAuras.empty() && m_deletedHolders.empty();
        }

        /**
         * @brief Hands both deferred lists to the caller and empties the container.
         *
         * Call with two empty lists: whatever they held is dropped on the floor, not
         * deleted and not kept. The container is empty when this returns, which is what
         * makes the deletion the caller's alone -- the container never deletes (F6).
         */
        void TakeDeferred(AuraList& auras, HolderList& holders)
        {
            auras.swap(m_deletedAuras);
            holders.swap(m_deletedHolders);
            m_deletedAuras.clear();
            m_deletedHolders.clear();
        }

    private:
        HolderMap m_holders;
        HolderMap::iterator m_updateIterator;   // != end() during the Unit::Update walk, points at the next element
        AuraList m_deletedAuras;                // auras removed while in ApplyModifier and waiting deleted
        HolderList m_deletedHolders;
        AuraList m_modAuras[TOTAL_AURAS];
};

#endif
