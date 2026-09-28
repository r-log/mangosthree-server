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

#ifndef MANGOS_H_PETMGR
#define MANGOS_H_PETMGR

#include "Platform/Define.h"
#include "ManagerPacketSink.h"
#include "PlayerPetCache.h"
#include "SharedDefines.h"                                  // MAX_PET_STABLES, PetSaveMode

#include <functional>

/**
 * @brief Decoupling D4k: a character's pet-ownership state and the rules over it, held apart from
 * the object that plays the character.
 *
 * The state is the stable-slot count (saved with the character row), the number of the pet a
 * temporary unsummon put away (a mount, a taxi, a vehicle, a transport, a talent respec) so that
 * the same pet comes back afterwards, and the character's pet rows (PlayerPetCache, decoupling
 * D7e). The owner holds one by value.
 *
 * The rules are: the stable-slot count a loaded row gives (LoadStableSlotsFromField); when a
 * temporary unsummon records the pet's number (none pending, a controlled pet, not itself a
 * temporary summon), and that it is recorded BEFORE the pet is unsummoned, because the unsummon
 * and the pet's save read it; when a resummon happens (a number pending, the owner in a state
 * that allows it, no pet out), that "not yet" keeps the number, and that an attempt clears it,
 * successful or not, AFTER the load (the old order, kept and pinned; nothing in the load reads
 * the number, so the order is preserved rather than relied on); and the bytes of the
 * packet that clears the client's pet action bar.
 *
 * The object carries no owner. What it used to read from the owner is handed in at the call --
 * the owner's live pet as plain facts (LivePet), whether the owner may have a pet out now and
 * whether it has one (ResummonInputs) -- and what it used to do to the owner goes out through a
 * callback called at the exact point the old body did it: the pet's unsummon (an UnsummonSink),
 * the load of the put-away pet (a PetLoadSink) and the packet (a ManagerPacketSink, the owner's
 * session). Callbacks are parameters only, never stored. So `mangos_tests` builds one from
 * nothing.
 *
 * What stays with the owner, and why (pets/PlayerPet.cpp): finding the live pet (its lookup can
 * clear a dangling pet guid), reading the pet's facts, the unsummon itself, the new pet and its
 * load from the pet cache -- orchestration over the live pet, which this object never sees -- and
 * the two dismissals that touch none of this state (RemovePet, UnsummonPetIfAny). The pet
 * creature, its persistence to the pet tables and the stable handlers live in the pet code.
 *
 * KEPT SEMANTICS, stated rather than fixed: the stable-slot count is always MAX_PET_STABLES after
 * a load, whatever the column held (a larger value is also logged); SetStableSlots has no caller.
 */
class PetMgr
{
    public:
        /// Unsummons the owner's live pet with a save mode: `pet->Unsummon(mode, owner)`.
        typedef std::function<void(PetSaveMode mode)> UnsummonSink;
        /// Brings a put-away pet back by its number: a new pet loaded from the owner's pet rows
        /// (`LoadPetFromDB(owner, 0, petNumber, true)`), deleted again if the load fails.
        typedef std::function<void(uint32 petNumber)> PetLoadSink;

        /// The owner's live pet, read by the owner just before UnsummonTemporaryIfAny. With no pet
        /// out, `present` is false and the other fields keep their defaults.
        struct LivePet
        {
            bool   present = false;                 ///< the owner's GetPet() found a pet
            bool   controlled = false;              ///< pet->isControlled()
            bool   temporarySummoned = false;       ///< pet->isTemporarySummoned()
            uint32 petNumber = 0;                   ///< pet->GetCharmInfo()->GetPetNumber()
        };

        /// What ResummonTemporaryUnsummonedIfAny needs from the owner, read by the owner just
        /// before the call. The scalars default to false so that neither is ever indeterminate.
        struct ResummonInputs
        {
            bool        needTemporaryUnsummon = false;  ///< the owner's IsPetNeedBeTemporaryUnsummoned()
            bool        petGuidSet = false;             ///< the owner's GetPetGuid() is not empty
            PetLoadSink load;                           ///< the load, called where the old body loaded
        };

        /// Number of stable slots the character can use. Cata 4.0.1 gave
        /// every hunter MAX_PET_STABLES (5) for free and removed the
        /// CMSG_BUY_STABLE_SLOT purchase flow, so this is effectively a
        /// constant in this fork. Persisted to `characters`.stable_slots
        /// for forward-compat; clamped to MAX_PET_STABLES on load.
        uint32 GetStableSlots() const { return m_stableSlots; }
        void SetStableSlots(uint32 slots) { m_stableSlots = slots; }

        /// Called from the owner's load with the raw column value.
        /// Clamps to MAX_PET_STABLES on either side so a character row
        /// carried over from a pre-Cata default (stable_slots=0) still
        /// gets the Cata 5 free slots without a DB migration.
        void LoadStableSlotsFromField(uint32 raw);

        /// Pet number that was active before a temporary unsummon (e.g.
        /// vehicle board / transport zone-in). Zero means no pending
        /// resummon. Read by Pet.cpp during save-mode dispatch and
        /// written by CharacterHandler on character creation when a
        /// hunter starts with a deity pet.
        uint32 GetTemporaryUnsummonedPetNumber() const { return m_temporaryUnsummonedPetNumber; }
        void SetTemporaryUnsummonedPetNumber(uint32 petnumber) { m_temporaryUnsummonedPetNumber = petnumber; }

        /**
         * @brief Clears the pet action bar on the client: the pet spells packet with an empty guid.
         *
         * @param send The owner's session sink.
         */
        void RemoveActionBar(ManagerPacketSink const& send);

        /**
         * @brief Unsummons the owner's pet for now, remembering it so that it can come back.
         *
         * With no pet out, does nothing. Otherwise, if no number is pending and the pet is a
         * controlled pet that is not itself a temporary summon, records its number first; then
         * unsummons it with PET_SAVE_AS_CURRENT.
         *
         * @param pet      The owner's live pet.
         * @param unsummon Unsummons it.
         */
        void UnsummonTemporaryIfAny(LivePet const& pet, UnsummonSink const& unsummon);

        /**
         * @brief Brings back the pet a temporary unsummon put away, if the time is right.
         *
         * Does nothing with no number pending. Keeps the number, and loads nothing, while the
         * owner may not have a pet out (mounted, on a taxi, dead, out of the world) or already has
         * one. Otherwise loads the pet, then clears the number whether or not the load succeeded.
         *
         * @param inputs The owner's two facts and the load.
         */
        void ResummonTemporaryUnsummonedIfAny(ResummonInputs const& inputs);

        /// Decoupling D7e: this character's rows from the five pet tables, loaded by the
        /// login holder and kept current at every write. `Pet::LoadPetFromDB` and the stable
        /// handlers read it instead of blocking on a SELECT. Empty for a character that never
        /// went through a login holder (a character being created, the movement harness's
        /// mover) -- which reads exactly as "this character has no pet rows" did.
        PlayerPetCache& GetPetCache() { return m_petCache; }
        PlayerPetCache const& GetPetCache() const { return m_petCache; }

    private:
        uint32  m_stableSlots = MAX_PET_STABLES;
        uint32  m_temporaryUnsummonedPetNumber = 0;
        PlayerPetCache m_petCache;
};

#endif // MANGOS_H_PETMGR
