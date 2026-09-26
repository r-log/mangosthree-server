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

#ifndef MANGOS_H_PLAYERPETCACHE
#define MANGOS_H_PLAYERPETCACHE

#include "SharedDefines.h"

#include <map>
#include <string>
#include <vector>

/**
 * @file PlayerPetCache.h
 * @brief Decoupling D7e: one character's rows from the five pet tables, carried in memory.
 *
 * `Pet::LoadPetFromDB` used to issue five synchronous SELECTs -- `character_pet`, then
 * `pet_aura`, `pet_spell`, `pet_spell_cooldown` and `character_pet_declinedname` -- inside the
 * tick, and its summon callers need the finished `Pet` in the same tick (the spell effect adds
 * it to the map), so a two-phase summon was rejected. What replaces the SELECTs is this: every
 * row of those five tables that belongs to the character, loaded by the login holder with the
 * rest of the character and kept current at every write.
 *
 * ONE object per online `Player`, owned by `PetMgr` (which already owns the stable-slot count
 * and the temporary-unsummon number). It is plain memory touched only from the world thread, so
 * unlike `CharacterCache` -- which is global and read from the start-up loaders too -- it needs
 * no lock and hands out plain pointers.
 *
 * THE INVARIANT: every statement that writes one of the five tables for a character who is
 * ONLINE updates this object at the same place. A character who is offline has no cache to
 * update, and gets the rows from the holder at their next login; that is why character
 * creation, `Player::DeleteFromDB` and `.pdump load` need nothing.
 *
 * THE ONE ACCEPTED DIFFERENCE: a pet row edited outside the server while it is running -- a
 * manual database edit, a second realm process -- is not seen until the character re-logs.
 *
 * TIE-BREAKS. Three of the queries this replaces could match more than one row and took
 * whichever the storage engine handed back first. `character_pet` is InnoDB with `id` as its
 * primary key and `owner` as a secondary index, so a scan for one owner walks the index in
 * primary-key order: the row the engine handed back first was the one with the LOWEST `id`.
 * Every lookup here is over a `std::map` keyed by `id`, so "the first match" means the same
 * thing it meant before.
 */

/// `character_pet`, one row. Column order and names follow the table, not the SELECT list, so
/// the fields can be checked against `characterLoadDB.sql` rather than against a format string.
struct PetCacheRow
{
    uint32      id;                 ///< `character_pet`.`id` -- the pet number
    uint32      entry;
    uint32      owner;
    uint32      modelId;
    uint32      createdBySpell;
    uint8       petType;
    uint32      level;
    uint32      exp;
    uint8       reactState;
    std::string name;
    uint8       renamed;
    uint32      slot;               ///< 0 = current, 1..MAX_PET_STABLES stable, 100 = not in slot
    uint32      curHealth;
    uint32      curMana;
    uint64      saveTime;
    uint32      resetTalentsCost;
    uint64      resetTalentsTime;
    std::string abData;

    PetCacheRow()
        : id(0), entry(0), owner(0), modelId(0), createdBySpell(0), petType(0), level(0),
          exp(0), reactState(0), renamed(0), slot(0), curHealth(0), curMana(0), saveTime(0),
          resetTalentsCost(0), resetTalentsTime(0)
    {
    }
};

/// `pet_aura`, one row, minus its `guid` (which is the key this is stored under).
struct PetCacheAura
{
    uint64 casterGuid;
    uint32 itemGuid;
    uint32 spell;
    uint32 stackCount;
    uint32 remainCharges;
    int32  basePoints[3];
    uint32 periodicTime[3];
    int32  maxDuration;
    int32  remainTime;
    uint32 effIndexMask;

    PetCacheAura()
        : casterGuid(0), itemGuid(0), spell(0), stackCount(0), remainCharges(0),
          maxDuration(0), remainTime(0), effIndexMask(0)
    {
        for (int i = 0; i < 3; ++i)
        {
            basePoints[i] = 0;
            periodicTime[i] = 0;
        }
    }
};

/// `pet_spell`, one row, minus its `guid`.
struct PetCacheSpell
{
    uint32 spell;
    uint8  active;

    PetCacheSpell() : spell(0), active(0) { }
    PetCacheSpell(uint32 s, uint8 a) : spell(s), active(a) { }
};

/// `pet_spell_cooldown`, one row, minus its `guid`.
struct PetCacheCooldown
{
    uint32 spell;
    uint64 time;

    PetCacheCooldown() : spell(0), time(0) { }
    PetCacheCooldown(uint32 s, uint64 t) : spell(s), time(t) { }
};

/// The five declension forms of `character_pet_declinedname`. The count is `Unit.h`'s
/// MAX_DECLINED_NAME_CASES; this header must not reach the object layer, so the value is
/// repeated here and PlayerPetCache.cpp static_asserts the two against each other.
enum { PET_CACHE_DECLINED_NAME_CASES = 5 };

struct PetCacheDeclinedName
{
    std::string name[PET_CACHE_DECLINED_NAME_CASES];
};

class PlayerPetCache
{
    public:
        typedef std::map<uint32, PetCacheRow> RowMap;
        typedef std::vector<PetCacheAura> AuraList;
        typedef std::vector<PetCacheSpell> SpellList;
        typedef std::vector<PetCacheCooldown> CooldownList;

        // ------------------------------------------------------------------ the load
        //
        // Called once per character, from Player::LoadFromDB, with the login holder's five
        // new results. Nothing else fills the cache: a Player built by Player::Create (a new
        // character, or the movement harness's mover) has an empty one, and every lookup
        // below then answers "no such pet" -- which is what the SELECTs answered for a
        // character with no rows.

        void Clear();
        void LoadRow(PetCacheRow const& row);
        void LoadAura(uint32 petId, PetCacheAura const& aura);
        void LoadSpell(uint32 petId, PetCacheSpell const& spell);
        void LoadCooldown(uint32 petId, PetCacheCooldown const& cooldown);
        void LoadDeclinedName(uint32 petId, PetCacheDeclinedName const& names);

        // ------------------------------------------------------------------ the lookups
        //
        // One method per SELECT the conversion replaced. NULL means "no row", exactly as a
        // NULL QueryResult did.

        RowMap const& Rows() const { return m_rows; }
        size_t RowCount() const { return m_rows.size(); }

        /// `WHERE owner = X AND id = P`
        PetCacheRow const* FindById(uint32 petId) const;

        /// `WHERE owner = X AND slot = S` -- lowest id wins a tie, as the index scan did.
        PetCacheRow const* FindBySlot(uint32 slot) const;

        /// `WHERE owner = X AND entry = E AND (slot = PET_SAVE_AS_CURRENT OR slot > PET_SAVE_LAST_STABLE_SLOT)`
        PetCacheRow const* FindByEntryCurrentOrUnslotted(uint32 entry) const;

        /// `WHERE owner = X AND (slot = PET_SAVE_AS_CURRENT OR slot > PET_SAVE_LAST_STABLE_SLOT)`
        PetCacheRow const* FindCurrentOrUnslotted() const;

        /// `WHERE owner = X AND id = P AND slot >= lowSlot AND slot <= highSlot`
        PetCacheRow const* FindByIdInSlotRange(uint32 petId, uint32 lowSlot, uint32 highSlot) const;

        /// `WHERE owner = X AND slot = S AND id <> P` -- lowest id wins a tie.
        PetCacheRow const* FindBySlotExcept(uint32 slot, uint32 exceptPetId) const;

        /// `SELECT COUNT(*) ... WHERE owner = X AND slot <= S`
        uint32 CountRowsUpToSlot(uint32 slot) const;

        /// `WHERE owner = X AND slot >= lowSlot AND slot <= highSlot AND id <> P ORDER BY slot`.
        /// Ordered by (slot, id): the statement's own ORDER BY, plus the id tie-break that
        /// makes two pets parked in one slot come back in a reproducible order.
        std::vector<PetCacheRow const*> RowsInSlotRange(uint32 lowSlot, uint32 highSlot,
                                                        uint32 exceptPetId) const;

        /// Every pet id except one, in id order -- `SELECT id ... WHERE owner = X AND id <> P`.
        std::vector<uint32> PetIdsExcept(uint32 exceptPetId) const;

        /// Every spell any of those pets knows, each once, in ascending order --
        /// `SELECT DISTINCT pet_spell.spell ... AND character_pet.id <> P`.
        std::vector<uint32> DistinctSpellsOfPets(std::vector<uint32> const& petIds) const;

        AuraList const& Auras(uint32 petId) const;
        SpellList const& Spells(uint32 petId) const;
        CooldownList const& Cooldowns(uint32 petId) const;
        PetCacheDeclinedName const* FindDeclinedName(uint32 petId) const;

        // ------------------------------------------------------------------ the writes
        //
        // One method per statement, mirroring exactly what that statement does to the table --
        // including what it does NOT do. `EraseRow` drops the `character_pet` row and leaves
        // the pet's aura / spell / cooldown rows behind, because `DELETE FROM character_pet`
        // does; `ErasePet` is the five-table sweep `Pet::DeleteFromDB` issues.

        /// INSERT / REPLACE INTO `character_pet`
        void SetRow(PetCacheRow const& row);

        /// `DELETE FROM character_pet WHERE owner = X AND id = P` -- the parent row only.
        void EraseRow(uint32 petId);

        /// `Pet::DeleteFromDB`: the row and all four of its dependent tables.
        void ErasePet(uint32 petId);

        /// `UPDATE character_pet SET slot = to WHERE owner = X AND slot = from [AND id <> P]`.
        /// exceptPetId 0 means "no exception" -- a pet number is never 0.
        void MoveSlot(uint32 fromSlot, uint32 toSlot, uint32 exceptPetId);

        /// `UPDATE character_pet SET slot = S WHERE owner = X AND id = P`
        void SetSlot(uint32 petId, uint32 slot);

        /// `DELETE FROM character_pet WHERE owner = X AND slot > S AND id <> P`
        void EraseRowsAboveSlot(uint32 slot, uint32 exceptPetId);

        /// `UPDATE character_pet SET name = ?, renamed = 1 WHERE owner = X AND id = P`
        void SetNameRenamed(uint32 petId, std::string const& name, uint8 renamed);

        /// `DELETE FROM pet_aura WHERE guid = P`, then the INSERTs that follow it.
        void SetAuras(uint32 petId, AuraList auras);

        /// `DELETE FROM pet_spell_cooldown WHERE guid = P`, then the INSERTs that follow it.
        void SetCooldowns(uint32 petId, CooldownList cooldowns);

        /// `DELETE FROM pet_spell WHERE guid = P AND spell = S` then `INSERT` -- one spell.
        void SetSpell(uint32 petId, uint32 spell, uint8 active);

        /// `DELETE FROM pet_spell WHERE guid = P AND spell = S`
        void EraseSpell(uint32 petId, uint32 spell);

        /// `DELETE FROM pet_spell WHERE spell = S` -- the unknown-spell sweep in Pet::addSpell.
        /// The statement is global; this object is one character's, so it clears the spell
        /// from this character's pets and the rest is other sessions' (see the report).
        void EraseSpellEverywhere(uint32 spell);

        /// `DELETE FROM pet_spell WHERE guid IN (ids) AND spell IN (spells)`
        void EraseSpells(std::vector<uint32> const& petIds, std::vector<uint32> const& spells);

        /// `DELETE FROM character_pet_declinedname WHERE owner = X AND id = P` then `INSERT`.
        void SetDeclinedName(uint32 petId, PetCacheDeclinedName const& names);

    private:
        RowMap                                     m_rows;
        std::map<uint32, AuraList>                 m_auras;
        std::map<uint32, SpellList>                m_spells;
        std::map<uint32, CooldownList>             m_cooldowns;
        std::map<uint32, PetCacheDeclinedName>     m_declinedNames;
};

#endif // MANGOS_H_PLAYERPETCACHE
