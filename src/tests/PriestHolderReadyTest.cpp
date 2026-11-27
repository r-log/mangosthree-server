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

/// Tests of the aura holder's ready state (spells/auras/SpellAuras.cpp): a holder starts CREATED whatever the memory
/// it is made in held, so that only the adder that puts it on a unit marks it READY and the proc loop passes over it
/// until then. The holder is made through its constructor over a buffer filled with the words a freed holder, a
/// holder being removed and other memory leave, so the test does not depend on the allocator; its target is a bare
/// Creature (update fields allocated; no world, map, template or AI) and its spell a Spell.dbc row with only its id,
/// put into the spell store and taken out of it again. The adders that mark a holder READY need a unit in a map, which
/// a test cannot make, so scenario 1233 (priest-holder-ready) is their proof: the GM aura command's holder, an area
/// aura's holder on a member, a copy and the aura of a spell's own hit.

#include "TestHarness.h"
#include "spells/auras/SpellAuras.h"
#include "data/ServerSpellStore.h"
#include "Creature.h"
#include "DBCStores.h"
#include "DBCStructure.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

namespace
{
    uint32 const RENEW = 139;

    /// What a holder's state word read in the wave 4 box's log, in memory the engine had not set it in.
    uint32 const STRAY_WORD = 1684956521;

    // A creature with its update fields allocated and no world, map, template or AI.
    class BareCreature : public Creature
    {
        public:
            BareCreature()
            {
                _InitValues();
            }

            ~BareCreature()
            {
                delete[] m_uint32Values;
                m_uint32Values = NULL;
            }

            // The guid of `counter`.
            void Name(uint32 counter)
            {
                SetGuidValue(OBJECT_FIELD_GUID, ObjectGuid(HIGHGUID_UNIT, 50249, counter));
            }
    };

    struct HolderStorage
    {
        alignas(SpellAuraHolder) unsigned char bytes[sizeof(SpellAuraHolder)];
    };

    // Every whole 32-bit word of `storage` set to `word`.
    void FillWords(HolderStorage& storage, uint32_t word)
    {
        for (size_t offset = 0; offset + sizeof(word) <= sizeof(storage.bytes); offset += sizeof(word))
        {
            std::memcpy(storage.bytes + offset, &word, sizeof(word));
        }
    }

    ServerSpell s_row;

    // Puts Renew, with its id alone, into the spell store.
    void StoreRow()
    {
        s_row.Entry().ID = RENEW;
        sSpellStore.SetEntry(RENEW, &s_row.Entry());
    }

    // Takes it out of the spell store again.
    void ForgetRow()
    {
        sSpellStore.SetEntry(RENEW, NULL);
    }
}

TEST(PriestHolderReady_AHolderStartsCreatedWhateverTheMemoryHeld)
{
    StoreRow();
    {
        BareCreature target;
        target.Name(1);

        uint32_t const words[] = { 0, uint32_t(SPELLAURAHOLDER_STATE_READY), uint32_t(SPELLAURAHOLDER_STATE_REMOVING),
                                   uint32_t(STRAY_WORD) };
        for (uint32_t word : words)
        {
            HolderStorage storage;
            FillWords(storage, word);
            if (word == uint32_t(SPELLAURAHOLDER_STATE_READY))
            {
                // the buffer is what a freed holder leaves: every word of it, the state's among them, reads READY
                CHECK_EQ(int(reinterpret_cast<SpellAuraHolder const*>(storage.bytes)->GetState()),
                         int(SPELLAURAHOLDER_STATE_READY));
            }

            SpellAuraHolder* holder = new (storage.bytes) SpellAuraHolder(sSpellStore.LookupEntry(RENEW), &target,
                                                                           NULL, NULL, NULL);
            CHECK_EQ(int(holder->GetState()), int(SPELLAURAHOLDER_STATE_CREATED));

            // the adder's mark, once the unit holds it
            holder->SetState(SPELLAURAHOLDER_STATE_READY);
            CHECK_EQ(int(holder->GetState()), int(SPELLAURAHOLDER_STATE_READY));
            holder->~SpellAuraHolder();
        }
    }
    ForgetRow();
}
