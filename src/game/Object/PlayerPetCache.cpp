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

#include "PlayerPetCache.h"
#include "Unit.h"

#include <algorithm>
#include <tuple>

// The header cannot include Unit.h -- Player.h reaches PetMgr.h reaches this, and the header
// reach gate forbids nothing of the sort from growing -- so the declension count is spelled
// twice and checked here, where including Unit.h costs nothing.
static_assert(int(PET_CACHE_DECLINED_NAME_CASES) == int(MAX_DECLINED_NAME_CASES),
              "PlayerPetCache's declension count must match Unit.h's MAX_DECLINED_NAME_CASES");

namespace
{
    /// The `(slot = PET_SAVE_AS_CURRENT OR slot > PET_SAVE_LAST_STABLE_SLOT)` clause that two
    /// of LoadPetFromDB's branches carry: the pet standing with the player, or one parked
    /// outside the stable entirely.
    inline bool IsCurrentOrUnslotted(PetCacheRow const& row)
    {
        return row.slot == uint32(PET_SAVE_AS_CURRENT)
               || row.slot > uint32(PET_SAVE_LAST_STABLE_SLOT);
    }
}

void PlayerPetCache::Clear()
{
    m_rows.clear();
    m_auras.clear();
    m_spells.clear();
    m_cooldowns.clear();
    m_declinedNames.clear();
}

void PlayerPetCache::LoadRow(PetCacheRow const& row)
{
    m_rows[row.id] = row;
}

void PlayerPetCache::LoadAura(uint32 petId, PetCacheAura const& aura)
{
    m_auras[petId].push_back(aura);
}

void PlayerPetCache::LoadSpell(uint32 petId, PetCacheSpell const& spell)
{
    m_spells[petId].push_back(spell);
}

void PlayerPetCache::LoadCooldown(uint32 petId, PetCacheCooldown const& cooldown)
{
    m_cooldowns[petId].push_back(cooldown);
}

void PlayerPetCache::LoadDeclinedName(uint32 petId, PetCacheDeclinedName const& names)
{
    m_declinedNames[petId] = names;
}

PetCacheRow const* PlayerPetCache::FindById(uint32 petId) const
{
    RowMap::const_iterator itr = m_rows.find(petId);
    return itr != m_rows.end() ? &itr->second : NULL;
}

PetCacheRow const* PlayerPetCache::FindBySlot(uint32 slot) const
{
    for (RowMap::const_iterator itr = m_rows.begin(); itr != m_rows.end(); ++itr)
    {
        if (itr->second.slot == slot)
        {
            return &itr->second;
        }
    }

    return NULL;
}

PetCacheRow const* PlayerPetCache::FindByEntryCurrentOrUnslotted(uint32 entry) const
{
    for (RowMap::const_iterator itr = m_rows.begin(); itr != m_rows.end(); ++itr)
    {
        if (itr->second.entry == entry && IsCurrentOrUnslotted(itr->second))
        {
            return &itr->second;
        }
    }

    return NULL;
}

PetCacheRow const* PlayerPetCache::FindCurrentOrUnslotted() const
{
    for (RowMap::const_iterator itr = m_rows.begin(); itr != m_rows.end(); ++itr)
    {
        if (IsCurrentOrUnslotted(itr->second))
        {
            return &itr->second;
        }
    }

    return NULL;
}

PetCacheRow const* PlayerPetCache::FindByIdInSlotRange(uint32 petId, uint32 lowSlot, uint32 highSlot) const
{
    PetCacheRow const* row = FindById(petId);
    if (!row || row->slot < lowSlot || row->slot > highSlot)
    {
        return NULL;
    }

    return row;
}

PetCacheRow const* PlayerPetCache::FindBySlotExcept(uint32 slot, uint32 exceptPetId) const
{
    for (RowMap::const_iterator itr = m_rows.begin(); itr != m_rows.end(); ++itr)
    {
        if (itr->second.slot == slot && itr->first != exceptPetId)
        {
            return &itr->second;
        }
    }

    return NULL;
}

uint32 PlayerPetCache::CountRowsUpToSlot(uint32 slot) const
{
    uint32 count = 0;
    for (RowMap::const_iterator itr = m_rows.begin(); itr != m_rows.end(); ++itr)
    {
        if (itr->second.slot <= slot)
        {
            ++count;
        }
    }

    return count;
}

std::vector<PetCacheRow const*> PlayerPetCache::RowsInSlotRange(uint32 lowSlot, uint32 highSlot,
                                                                uint32 exceptPetId) const
{
    std::vector<PetCacheRow const*> out;
    for (RowMap::const_iterator itr = m_rows.begin(); itr != m_rows.end(); ++itr)
    {
        PetCacheRow const& row = itr->second;
        if (row.slot >= lowSlot && row.slot <= highSlot && itr->first != exceptPetId)
        {
            out.push_back(&row);
        }
    }

    // The map already has them in id order, so a stable sort on the slot alone gives
    // (slot, id) -- the statement's ORDER BY plus a tie-break it did not have.
    std::stable_sort(out.begin(), out.end(),
                     [](PetCacheRow const* a, PetCacheRow const* b) { return a->slot < b->slot; });
    return out;
}

std::vector<uint32> PlayerPetCache::PetIdsExcept(uint32 exceptPetId) const
{
    std::vector<uint32> out;
    for (RowMap::const_iterator itr = m_rows.begin(); itr != m_rows.end(); ++itr)
    {
        if (itr->first != exceptPetId)
        {
            out.push_back(itr->first);
        }
    }

    return out;
}

std::vector<uint32> PlayerPetCache::DistinctSpellsOfPets(std::vector<uint32> const& petIds) const
{
    std::vector<uint32> out;
    for (size_t i = 0; i < petIds.size(); ++i)
    {
        std::map<uint32, SpellList>::const_iterator found = m_spells.find(petIds[i]);
        if (found == m_spells.end())
        {
            continue;
        }

        for (size_t s = 0; s < found->second.size(); ++s)
        {
            out.push_back(found->second[s].spell);
        }
    }

    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

PlayerPetCache::AuraList const& PlayerPetCache::Auras(uint32 petId) const
{
    static const AuraList empty;
    std::map<uint32, AuraList>::const_iterator itr = m_auras.find(petId);
    return itr != m_auras.end() ? itr->second : empty;
}

PlayerPetCache::SpellList const& PlayerPetCache::Spells(uint32 petId) const
{
    static const SpellList empty;
    std::map<uint32, SpellList>::const_iterator itr = m_spells.find(petId);
    return itr != m_spells.end() ? itr->second : empty;
}

PlayerPetCache::CooldownList const& PlayerPetCache::Cooldowns(uint32 petId) const
{
    static const CooldownList empty;
    std::map<uint32, CooldownList>::const_iterator itr = m_cooldowns.find(petId);
    return itr != m_cooldowns.end() ? itr->second : empty;
}

PetCacheDeclinedName const* PlayerPetCache::FindDeclinedName(uint32 petId) const
{
    std::map<uint32, PetCacheDeclinedName>::const_iterator itr = m_declinedNames.find(petId);
    return itr != m_declinedNames.end() ? &itr->second : NULL;
}

void PlayerPetCache::SetRow(PetCacheRow const& row)
{
    m_rows[row.id] = row;
}

void PlayerPetCache::EraseRow(uint32 petId)
{
    m_rows.erase(petId);
}

void PlayerPetCache::ErasePet(uint32 petId)
{
    m_rows.erase(petId);
    m_auras.erase(petId);
    m_spells.erase(petId);
    m_cooldowns.erase(petId);
    m_declinedNames.erase(petId);
}

void PlayerPetCache::MoveSlot(uint32 fromSlot, uint32 toSlot, uint32 exceptPetId)
{
    for (RowMap::iterator itr = m_rows.begin(); itr != m_rows.end(); ++itr)
    {
        if (itr->second.slot == fromSlot && itr->first != exceptPetId)
        {
            itr->second.slot = toSlot;
        }
    }
}

void PlayerPetCache::SetSlot(uint32 petId, uint32 slot)
{
    RowMap::iterator itr = m_rows.find(petId);
    if (itr != m_rows.end())
    {
        itr->second.slot = slot;
    }
}

void PlayerPetCache::EraseRowsAboveSlot(uint32 slot, uint32 exceptPetId)
{
    for (RowMap::iterator itr = m_rows.begin(); itr != m_rows.end();)
    {
        if (itr->second.slot > slot && itr->first != exceptPetId)
        {
            // The statement is `DELETE FROM character_pet`, so only the parent row goes; the
            // pet's aura / spell / cooldown rows are left behind exactly as the table leaves
            // them. Nothing can read them again -- every lookup starts from a `character_pet`
            // row -- and they go with the session.
            m_rows.erase(itr++);
        }
        else
        {
            ++itr;
        }
    }
}

void PlayerPetCache::SetNameRenamed(uint32 petId, std::string const& name, uint8 renamed)
{
    RowMap::iterator itr = m_rows.find(petId);
    if (itr != m_rows.end())
    {
        itr->second.name = name;
        itr->second.renamed = renamed;
    }
}

void PlayerPetCache::SetAuras(uint32 petId, AuraList auras)
{
    if (auras.empty())
    {
        m_auras.erase(petId);
        return;
    }

    // `pet_aura`'s primary key is (guid, caster_guid, item_guid, spell), and `guid` is the pet id,
    // the same for every row in this list, so sorting by the remaining three fields is what makes
    // a dismiss-and-resummon in the same session read the auras back in the order a fresh login
    // reads them.
    std::sort(auras.begin(), auras.end(),
              [](PetCacheAura const& a, PetCacheAura const& b)
              {
                  return std::tie(a.casterGuid, a.itemGuid, a.spell) <
                         std::tie(b.casterGuid, b.itemGuid, b.spell);
              });

    m_auras[petId] = std::move(auras);
}

void PlayerPetCache::SetCooldowns(uint32 petId, CooldownList cooldowns)
{
    if (cooldowns.empty())
    {
        m_cooldowns.erase(petId);
        return;
    }

    m_cooldowns[petId] = std::move(cooldowns);
}

void PlayerPetCache::SetSpell(uint32 petId, uint32 spell, uint8 active)
{
    SpellList& list = m_spells[petId];
    for (size_t i = 0; i < list.size(); ++i)
    {
        if (list[i].spell == spell)
        {
            list[i].active = active;
            return;
        }
    }

    // `pet_spell`'s primary key is (guid, spell), so an INSERT lands in spell order. Keeping
    // the list sorted is what makes a reload after a save read the rows back in the order the
    // single-pet SELECT returned them.
    list.push_back(PetCacheSpell(spell, active));
    std::sort(list.begin(), list.end(),
              [](PetCacheSpell const& a, PetCacheSpell const& b) { return a.spell < b.spell; });
}

void PlayerPetCache::EraseSpell(uint32 petId, uint32 spell)
{
    std::map<uint32, SpellList>::iterator found = m_spells.find(petId);
    if (found == m_spells.end())
    {
        return;
    }

    SpellList& list = found->second;
    for (size_t i = 0; i < list.size(); ++i)
    {
        if (list[i].spell == spell)
        {
            list.erase(list.begin() + i);
            break;
        }
    }

    if (list.empty())
    {
        m_spells.erase(found);
    }
}

void PlayerPetCache::EraseSpellEverywhere(uint32 spell)
{
    for (std::map<uint32, SpellList>::iterator itr = m_spells.begin(); itr != m_spells.end();)
    {
        SpellList& list = itr->second;
        for (size_t i = 0; i < list.size(); ++i)
        {
            if (list[i].spell == spell)
            {
                list.erase(list.begin() + i);
                break;
            }
        }

        if (list.empty())
        {
            m_spells.erase(itr++);
        }
        else
        {
            ++itr;
        }
    }
}

void PlayerPetCache::EraseSpells(std::vector<uint32> const& petIds, std::vector<uint32> const& spells)
{
    for (size_t p = 0; p < petIds.size(); ++p)
    {
        for (size_t s = 0; s < spells.size(); ++s)
        {
            EraseSpell(petIds[p], spells[s]);
        }
    }
}

void PlayerPetCache::SetDeclinedName(uint32 petId, PetCacheDeclinedName const& names)
{
    m_declinedNames[petId] = names;
}
