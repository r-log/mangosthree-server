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

#include "RuneMgr.h"
#include "SharedDefines.h"
#include "Opcodes.h"
#include "WorldPacket.h"

// Every body below is the one this file held before decoupling D4k, statement for statement and
// in the same order. What changed is where the owner comes in: the class through `classId`, the
// regeneration auras' multiplier through `auraMod` and the melee haste rating through
// `hasteRating` (the owner computes both with the old expressions, in the old order), the
// convert aura's facts through `facts`; the owner's regeneration fields through `setRegen`, its
// session through `send` and the convert aura's removal through `dropAura`, each called where the
// old body made that write.

static RuneType runeSlotTypes[MAX_RUNES] =
{
    /*0*/ RUNE_BLOOD,
    /*1*/ RUNE_BLOOD,
    /*2*/ RUNE_UNHOLY,
    /*3*/ RUNE_UNHOLY,
    /*4*/ RUNE_FROST,
    /*5*/ RUNE_FROST
};

void RuneMgr::UpdateRuneRegen(RuneType rune, float auraMod, float hasteRating, RegenSink const& setRegen) const
{
    if (rune >= RUNE_DEATH)
    {
        return;
    }

    RuneType actualRune = rune;
    float cooldown = RUNE_BASE_COOLDOWN;
    for (uint8 i = 0; i < MAX_RUNES; i += 2)
    {
        if (GetBaseRune(i) != rune)
        {
            continue;
        }

        uint32 cd = GetRuneCooldown(i);
        uint32 secondRuneCd = GetRuneCooldown(i + 1);
        if (!cd && !secondRuneCd)
        {
            actualRune = GetCurrentRune(i);
        }
        else if (secondRuneCd && (cd > secondRuneCd || !cd))
        {
            cooldown = GetBaseRuneCooldown(i + 1);
            actualRune = GetCurrentRune(i + 1);
        }
        else
        {
            cooldown = GetBaseRuneCooldown(i);
            actualRune = GetCurrentRune(i);
        }

        break;
    }

    float hastePct = (100.0f - hasteRating) / 100.0f;
    if (hastePct < 0)
    {
        hastePct = 1.0f;
    }

    cooldown *= hastePct / auraMod;

    float value = float(1 * IN_MILLISECONDS) / cooldown;
    setRegen(uint8(actualRune), value);
}

uint8 RuneMgr::GetRuneCooldownFraction(uint8 index) const
{
    uint16 baseCd = GetBaseRuneCooldown(index);
    if (!baseCd || !GetRuneCooldown(index))
    {
        return 255;
    }
    else if (baseCd == GetRuneCooldown(index))
    {
        return 0;
    }

    return uint8(float(baseCd - GetRuneCooldown(index)) / baseCd * 255);
}

void RuneMgr::RemoveRunesByAuraEffect(Aura const* aura, PacketSink const& send)
{
    for (uint8 i = 0; i < MAX_RUNES; ++i)
    {
        if (m_data.runes[i].ConvertAura == aura)
        {
            ConvertRune(i, GetBaseRune(i), send);
            SetRuneConvertAura(i, NULL);
        }
    }
}

void RuneMgr::RestoreBaseRune(uint8 index, ConvertAuraFacts const& facts, PacketSink const& send, AuraDrop const& dropAura)
{
    Aura const* aura = m_data.runes[index].ConvertAura;
    // If rune was converted by a non-pasive aura that still active we should keep it converted
    if (aura && facts.nonPassive)
    {
        return;
    }

    // Blood of the North
    if (aura && facts.bloodOfTheNorthHeld)
    {
        return;
    }

    ConvertRune(index, GetBaseRune(index), send);
    SetRuneConvertAura(index, NULL);
    // Don't drop passive talents providing rune convertion
    if (!aura || !facts.convertsRunes)
    {
        return;
    }

    for (uint8 i = 0; i < MAX_RUNES; ++i)
        if (aura == m_data.runes[i].ConvertAura)
        {
            return;
        }

    dropAura(aura);
}

void RuneMgr::ConvertRune(uint8 index, RuneType newType, PacketSink const& send)
{
    SetCurrentRune(index, newType);

    WorldPacket data(SMSG_CONVERT_RUNE, 2);
    data << uint8(index);
    data << uint8(newType);
    send(&data);
}

bool RuneMgr::ActivateRunes(RuneType type, uint32 count)
{
    bool modify = false;
    for (uint32 j = 0; count > 0 && j < MAX_RUNES; ++j)
    {
        if (GetRuneCooldown(j) && GetCurrentRune(j) == type)
        {
            SetRuneCooldown(j, 0);
            --count;
            modify = true;
        }
    }

    return modify;
}

void RuneMgr::ResyncRunes(PacketSink const& send) const
{
    WorldPacket data(SMSG_RESYNC_RUNES, 4 + MAX_RUNES * 2);
    data << uint32(MAX_RUNES);
    for (uint32 i = 0; i < MAX_RUNES; ++i)
    {
        data << uint8(GetCurrentRune(i));                   // rune type
        data << uint8(GetRuneCooldownFraction(i));
    }
    send(&data);
}

void RuneMgr::AddRunePower(uint8 index, PacketSink const& send) const
{
    WorldPacket data(SMSG_ADD_RUNE_POWER, 4);
    data << uint32(1 << index);                             // mask (0x00-0x3F probably)
    send(&data);
}

void RuneMgr::Init(uint8 classId, RegenSink const& setRegen)
{
    if (classId != CLASS_DEATH_KNIGHT)
    {
        return;
    }

    m_data.runeState = 0;

    for (uint32 i = 0; i < MAX_RUNES; ++i)
    {
        SetBaseRune(i, runeSlotTypes[i]);                   // init base types
        SetCurrentRune(i, runeSlotTypes[i]);                // init current types
        SetBaseRuneCooldown(i, 0);                          // reset base cooldowns
        SetRuneCooldown(i, 0);                              // reset cooldowns
        SetRuneConvertAura(i, NULL);                        // clear convert-aura ptr:
                                                            // uninitialised it is garbage
                                                            // and RestoreBaseRune derefs it
        m_data.SetRuneState(i);
    }

    for (uint32 i = 0; i < NUM_RUNE_TYPES; ++i)
    {
        setRegen(i, 0.1f);
    }
}

bool RuneMgr::IsBaseRuneSlotsOnCooldown(RuneType runeType) const
{
    for (uint32 i = 0; i < MAX_RUNES; ++i)
        if (GetBaseRune(i) == runeType && GetRuneCooldown(i) == 0)
        {
            return false;
        }

    return true;
}
