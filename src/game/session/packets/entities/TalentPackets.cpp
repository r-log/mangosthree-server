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

#include "TalentPackets.h"
#include "DBCStores.h"
#include "Item.h"
#include "Pet.h"
#include "Player.h"
#include "WorldPacket.h"

void WriteTalentsInfo(WorldPacket& packet, Player const& player)
{
    TalentMgr const& talentMgr = player.GetTalentMgr();
    GlyphMgr const& glyphMgr = player.GetGlyphMgr();

    packet << uint32(talentMgr.FreePoints());              // unspentTalentPoints
    packet << uint8(talentMgr.SpecsCount());               // talent group count (0, 1 or 2)
    packet << uint8(talentMgr.ActiveSpec());               // talent group index (0 or 1)

    if (talentMgr.SpecsCount())
    {
        uint32 const specsCount = talentMgr.SpecsCount() > MAX_TALENT_SPEC_COUNT ? MAX_TALENT_SPEC_COUNT : talentMgr.SpecsCount();

        // loop through all specs (only 1 for now)
        for (uint32 specIdx = 0; specIdx < specsCount; ++specIdx)
        {
            packet << uint32(talentMgr.PrimaryTree(specIdx));
            uint8 talentIdCount = 0;
            size_t pos = packet.wpos();
            packet << uint8(talentIdCount);                  // [PH], talentIdCount

            // find class talent tabs (all players have 3 talent tabs)
            uint32 const* talentTabIds = GetTalentTabPages(player.getClass());

            for (uint32 i = 0; i < MAX_TALENT_TABS; ++i)
            {
                uint32 talentTabId = talentTabIds[i];
                for (PlayerTalentMap::const_iterator iter = talentMgr.Talents(specIdx).begin(); iter != talentMgr.Talents(specIdx).end(); ++iter)
                {
                    PlayerTalent const& talent = (*iter).second;

                    if (talent.state == PLAYERSPELL_REMOVED)
                    {
                        continue;
                    }

                    // skip another tab talents
                    if (talent.talentEntry->TabID != talentTabId)
                    {
                        continue;
                    }

                    packet << uint32(talent.talentEntry->ID);  // Talent.dbc
                    packet << uint8(talent.currentRank);     // talentMaxRank (0-4)

                    ++talentIdCount;
                }
            }

            packet.put<uint8>(pos, talentIdCount);           // put real count

            packet << uint8(MAX_GLYPH_SLOT_INDEX);           // glyphs count

            // GlyphProperties.dbc
            for (uint8 i = 0; i < MAX_GLYPH_SLOT_INDEX; ++i)
            {
                packet << uint16(glyphMgr.GetGlyph(specIdx, i));
            }
        }
    }
}

void WritePetTalentsInfo(WorldPacket& packet, Player const& player)
{
    uint32 unspentTalentPoints = 0;
    size_t pointsPos = packet.wpos();
    packet << uint32(unspentTalentPoints);                   // [PH], unspentTalentPoints

    uint8 talentIdCount = 0;
    size_t countPos = packet.wpos();
    packet << uint8(talentIdCount);                          // [PH], talentIdCount

    Pet* pet = player.GetPet();
    if (!pet)
    {
        return;
    }

    unspentTalentPoints = pet->GetFreeTalentPoints();

    packet.put<uint32>(pointsPos, unspentTalentPoints);      // put real points

    CreatureInfo const* ci = pet->GetCreatureInfo();
    if (!ci)
    {
        return;
    }

    CreatureFamilyEntry const* pet_family = sCreatureFamilyStore.LookupEntry(ci->Family);
    if (!pet_family || pet_family->PetTalentType < 0)
    {
        return;
    }

    for (uint32 talentTabId = 1; talentTabId < sTalentTabStore.GetNumRows(); ++talentTabId)
    {
        TalentTabEntry const* talentTabInfo = sTalentTabStore.LookupEntry(talentTabId);
        if (!talentTabInfo)
        {
            continue;
        }

        if (!((1 << pet_family->PetTalentType) & talentTabInfo->PetTalentMask))
        {
            continue;
        }

        for (uint32 talentId = 0; talentId < sTalentStore.GetNumRows(); ++talentId)
        {
            TalentEntry const* talentInfo = sTalentStore.LookupEntry(talentId);
            if (!talentInfo)
            {
                continue;
            }

            // skip another tab talents
            if (talentInfo->TabID != talentTabId)
            {
                continue;
            }

            // find max talent rank
            int32 curtalent_maxrank = -1;
            for (int32 k = 4; k > -1; --k)
            {
                if (talentInfo->SpellRank[k] && pet->HasSpell(talentInfo->SpellRank[k]))
                {
                    curtalent_maxrank = k;
                    break;
                }
            }

            // not learned talent
            if (curtalent_maxrank < 0)
            {
                continue;
            }

            packet << uint32(talentInfo->ID);          // Talent.dbc
            packet << uint8(curtalent_maxrank);              // talentMaxRank (0-4)

            ++talentIdCount;
        }

        packet.put<uint8>(countPos, talentIdCount);          // put real count

        break;
    }
}

void WriteEnchantmentsInfo(WorldPacket& packet, Player const& player)
{
    uint32 slotUsedMask = 0;
    size_t slotUsedMaskPos = packet.wpos();
    packet << uint32(slotUsedMask);                          // slotUsedMask < 0x80000

    for (uint32 i = 0; i < EQUIPMENT_SLOT_END; ++i)
    {
        Item* item = player.GetInventoryMgr().GetItemByPos(INVENTORY_SLOT_BAG_0, i);

        if (!item)
        {
            continue;
        }

        slotUsedMask |= (1 << i);

        packet << uint32(item->GetEntry());                  // item entry

        uint16 enchantmentMask = 0;
        size_t enchantmentMaskPos = packet.wpos();
        packet << uint16(enchantmentMask);                   // enchantmentMask < 0x1000

        for (uint32 j = 0; j < MAX_ENCHANTMENT_SLOT; ++j)
        {
            uint32 enchId = item->GetEnchantmentId(EnchantmentSlot(j));

            if (!enchId)
            {
                continue;
            }

            enchantmentMask |= (1 << j);

            packet << uint16(enchId);                        // enchantmentId?
        }

        packet.put<uint16>(enchantmentMaskPos, enchantmentMask);

        packet << uint16(item->GetItemRandomPropertyId());
        packet << item->GetGuidValue(ITEM_FIELD_CREATOR).WriteAsPacked();
        packet << uint32(item->GetItemSuffixFactor());
    }

    packet.put<uint32>(slotUsedMaskPos, slotUsedMask);
}
