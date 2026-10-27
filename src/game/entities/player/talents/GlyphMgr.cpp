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

#include "GlyphMgr.h"
#include "DBCStores.h"
#include "Log.h"
#include "Object/UpdateFields.h"
#include "persistence/characters/talents/GlyphRepository.h"

void GlyphMgr::InitGlyphsForLevel(uint32 level, GlyphSlotSink const& setGlyphSlot, FieldSink const& setField)
{
    uint32 slot = 0;
    for (uint32 i = 0; i < sGlyphSlotStore.GetNumRows() && slot < MAX_GLYPH_SLOT_INDEX; ++i)
        if (GlyphSlotEntry const* gs = sGlyphSlotStore.LookupEntry(i))
        {
            setGlyphSlot(slot++, gs->ID);
        }

    uint32 value = 0;

    // 0x3F = 0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 for 80 level
    if (level >= 25)
    {
        value |= 0x01 | 0x02 | 0x40;
    }
    if (level >= 50)
    {
        value |= 0x04 | 0x08 | 0x80;
    }
    if (level >= 75)
    {
        value |= 0x10 | 0x20 | 0x100;
    }

    setField(PLAYER_GLYPHS_ENABLED, value);
}

void GlyphMgr::ApplyGlyph(uint8 activeSpec, uint8 slot, bool apply, ApplySinks const& sinks)
{
    if (uint32 glyph = GetGlyph(activeSpec, slot))
    {
        if (GlyphPropertiesEntry const* gp = sGlyphPropertiesStore.LookupEntry(glyph))
        {
            if (apply)
            {
                sinks.castOnSelf(gp->SpellID);
                sinks.setField(PLAYER_FIELD_GLYPHS_1 + slot, glyph);
            }
            else
            {
                sinks.removeAuras(gp->SpellID);
                sinks.setField(PLAYER_FIELD_GLYPHS_1 + slot, 0);
            }
        }
    }
}

void GlyphMgr::LoadRow(GlyphRow const& row, RowInputs const& inputs)
{
    uint8 spec = row.spec;
    uint8 slot = row.slot;
    uint32 glyph = row.glyph;

    GlyphPropertiesEntry const* gp = sGlyphPropertiesStore.LookupEntry(glyph);
    if (!gp)
    {
        sLog.outError("Player %s has not existing glyph entry %u on index %u, spec %u", inputs.ownerName, glyph, slot, spec);
        GlyphRepository::DeleteInvalidRow(inputs.ownerGuidLow, spec, slot);
        return;
    }

    GlyphSlotEntry const* gs = sGlyphSlotStore.LookupEntry(inputs.glyphSlot(slot));
    if (!gs)
    {
        sLog.outError("Player %s has not existing glyph slot entry %u on index %u, spec %u", inputs.ownerName, inputs.glyphSlot(slot), slot, spec);
        GlyphRepository::DeleteInvalidRow(inputs.ownerGuidLow, spec, slot);
        return;
    }

    if (gp->GlyphSlotFlags != gs->Type)
    {
        sLog.outError("Player %s has glyph with typeflags %u in slot with typeflags %u, removing.", inputs.ownerName, gp->GlyphSlotFlags, gs->Type);
        GlyphRepository::DeleteInvalidRow(inputs.ownerGuidLow, spec, slot);
        return;
    }

    m_glyphs[spec][slot].id = glyph;
}

void GlyphMgr::Save(uint32 ownerGuidLow, uint8 specsCount)
{
    for (uint8 spec = 0; spec < specsCount; ++spec)
    {
        for (uint8 slot = 0; slot < MAX_GLYPH_SLOT_INDEX; ++slot)
        {
            switch (m_glyphs[spec][slot].uState)
            {
                case GLYPH_NEW:
                    GlyphRepository::Insert(ownerGuidLow, spec, slot, m_glyphs[spec][slot].GetId());
                    break;
                case GLYPH_CHANGED:
                    GlyphRepository::Update(ownerGuidLow, spec, slot, m_glyphs[spec][slot].GetId());
                    break;
                case GLYPH_DELETED:
                    GlyphRepository::Delete(ownerGuidLow, spec, slot);
                    break;
                case GLYPH_UNCHANGED:
                    break;
            }
            m_glyphs[spec][slot].uState = GLYPH_UNCHANGED;
        }
    }
}
