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

/**
 * @file PlayerGlyph.cpp
 * @brief Decoupling D4k: the character's side of its glyphs (talents/GlyphMgr).
 *
 * GlyphMgr holds the glyph array and the rules over it and never sees the character. The
 * wrappers here are the part of the old GlyphMgr bodies that read or wrote the character: the
 * level, the active spec, the guid, the name and the glyph slot fields it read, the glyph slot
 * and update fields it wrote, the glyph spell's cast and the removal of its auras, the loop over
 * every slot and the login loop over the rows. Each input is read with the old expression just
 * before the one call into the manager (the glyph slot field of a loaded row is read by a
 * callback, where the old body read it); each write is a callback the manager calls where the old
 * body wrote. SetGlyph, GetGlyph and _SaveGlyphs have plain-expression inputs and stay inline in
 * Player.h.
 */

#include "Player.h"
#include "Database/DatabaseEnv.h"                           // QueryResult: the login loop fetches, advances and deletes it

namespace
{
    /// The update fields GlyphMgr wrote before decoupling D4k: SetUInt32Value(index, value).
    GlyphMgr::FieldSink UInt32Fields(Player* player)
    {
        return [player](uint16 index, uint32 value)
        {
            player->SetUInt32Value(index, value);
        };
    }
}

void Player::InitGlyphsForLevel()
{
    uint32 level = getLevel();
    GlyphMgr::GlyphSlotSink const setGlyphSlot = [this](uint8 slot, uint32 slotType)
    {
        SetGlyphSlot(slot, slotType);
    };

    m_glyphMgr.InitGlyphsForLevel(level, setGlyphSlot, UInt32Fields(this));
}

void Player::ApplyGlyph(uint8 slot, bool apply)
{
    GlyphMgr::ApplySinks sinks;
    sinks.castOnSelf = [this](uint32 spellId)
    {
        CastSpell(this, spellId, true);
    };
    sinks.removeAuras = [this](uint32 spellId)
    {
        RemoveAurasDueToSpell(spellId);
    };
    sinks.setField = UInt32Fields(this);

    m_glyphMgr.ApplyGlyph(m_talentMgr.ActiveSpec(), slot, apply, sinks);
}

void Player::ApplyGlyphs(bool apply)
{
    for (uint8 i = 0; i < MAX_GLYPH_SLOT_INDEX; ++i)
    {
        ApplyGlyph(i, apply);
    }
}

void Player::_LoadGlyphs(QueryResult* result)
{
    if (!result)
    {
        return;
    }

    // the rows are the login holder's PLAYER_LOGIN_QUERY_LOADGLYPHS result (CharacterHandler.cpp)

    do
    {
        GlyphMgr::RowInputs inputs;
        inputs.ownerGuidLow = GetGUIDLow();
        inputs.ownerName = GetName();
        inputs.glyphSlot = [this](uint8 slot)
        {
            return GetGlyphSlot(slot);
        };
        m_glyphMgr.LoadRow(result->Fetch(), inputs);
    }
    while (result->NextRow());

    delete result;
}
