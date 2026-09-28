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

#ifndef MANGOS_H_GLYPHMGR
#define MANGOS_H_GLYPHMGR

#include "Platform/Define.h"
#include "SharedDefines.h"                                  // MAX_GLYPH_SLOT_INDEX, MAX_TALENT_SPEC_COUNT

#include <functional>

class Field;

/**
 * @brief Lifecycle state of a glyph slot's dirty flag.
 *
 * Used by GlyphMgr::Save to decide INSERT vs UPDATE vs DELETE on the
 * character_glyphs row.
 */
enum GlyphUpdateState
{
    GLYPH_UNCHANGED = 0,    ///< Slot matches the persisted row, no DB work needed.
    GLYPH_CHANGED   = 1,    ///< Slot was modified, emit UPDATE.
    GLYPH_NEW       = 2,    ///< Slot has no persisted row yet, emit INSERT.
    GLYPH_DELETED   = 3     ///< Slot was cleared, emit DELETE.
};

/**
 * @brief Per-slot glyph state with dirty-tracking state machine.
 *
 * SetId encodes the transitions between GlyphUpdateState values and decides
 * what DB operation Save should emit for this slot.
 */
struct Glyph
{
    uint32 id;                  ///< DBC glyph property id, 0 when slot is empty.
    GlyphUpdateState uState;    ///< Current dirty state for DB persistence.

    /**
     * @brief Default-constructs an empty, unchanged glyph slot.
     */
    Glyph() : id(0), uState(GLYPH_UNCHANGED) {}

    /**
     * @brief Returns the slot's current glyph id.
     *
     * @return The glyph property id, or 0 if the slot is empty.
     */
    uint32 GetId() const { return id; }

    /**
     * @brief Sets the slot's glyph id and updates the dirty state.
     *
     * Encodes the transitions between glyph dirty states so that Save emits
     * the correct INSERT / UPDATE / DELETE for this row.
     *
     * @param newId The new glyph property id, or 0 to clear the slot.
     */
    void SetId(uint32 newId)
    {
        if (newId == id)
        {
            return;
        }

        if (id == 0 && uState == GLYPH_UNCHANGED)           // not yet in db and not yet saved
        {
            uState = GLYPH_NEW;
        }
        else if (newId == 0)
        {
            if (uState == GLYPH_NEW)                        // delete before add new -> no change
            {
                uState = GLYPH_UNCHANGED;
            }
            else                                            // delete existing data
            {
                uState = GLYPH_DELETED;
            }
        }
        else if (uState != GLYPH_NEW)                       // if not new data, change current data
        {
            uState = GLYPH_CHANGED;
        }

        id = newId;
    }
};

/**
 * @brief Decoupling D4k: a character's glyphs, per talent spec and glyph slot, and the rules over
 * them, held apart from the object that plays the character.
 *
 * The state is the glyph array: for each spec and slot, the glyph property id and its save state
 * (the Glyph state machine above). The owner holds one by value, fills it row by row at login
 * (LoadRow), and saves it with the rest of the character (Save).
 *
 * The object carries no owner. What it used to read from the owner is handed in at the call --
 * the level at InitGlyphsForLevel, the active spec at ApplyGlyph, the guid and the spec count at
 * Save, and for a loaded row the guid, the name and a read of the owner's glyph slot fields
 * (RowInputs) -- and what it used to write to the owner goes out through a callback called at the
 * exact point the old body wrote: the glyph slot fields (a GlyphSlotSink), the glyph and
 * glyphs-enabled update fields (a FieldSink), and the glyph's spell cast on the owner and the
 * removal of its auras (SpellSinks, bundled with the field sink in ApplySinks). Callbacks are
 * parameters only, never stored. So `mangos_tests` builds one from nothing.
 *
 * It reads two global stores itself, as before: the glyph slot store (the slot types and their
 * order; a loaded row's slot type must exist) and the glyph property store (a glyph's spell and
 * slot type). It builds no packet: the client sees glyphs through the update fields above and
 * through the talent info packet the owner builds.
 *
 * What stays with the owner, and why: the loop that applies or removes every slot (ApplyGlyphs:
 * each slot reads the active spec afresh, after the previous slot's cast), the glyph slot fields
 * themselves (SetGlyphSlot/GetGlyphSlot are update fields), and the login loop over the rows.
 *
 * KEPT SEMANTICS, stated rather than fixed (backlog): a row whose glyph is unknown is deleted for
 * every character (that DELETE names the glyph only); a loaded row's spec and slot are not checked
 * against the array's bounds; a loaded glyph keeps the UNCHANGED state, so the next save writes
 * nothing for it.
 */
class GlyphMgr
{
    public:
        /// Writes the owner's glyph slot type field: `SetGlyphSlot(slot, slotType)`.
        typedef std::function<void(uint8 slot, uint32 slotType)> GlyphSlotSink;
        /// Writes one of the owner's update fields: `SetUInt32Value(index, value)`.
        typedef std::function<void(uint16 index, uint32 value)> FieldSink;
        /// Does something to the owner with a spell id: the glyph's cast, or the removal of its auras.
        typedef std::function<void(uint32 spellId)> SpellSink;
        /// Reads the owner's glyph slot type field: `GetGlyphSlot(slot)`.
        typedef std::function<uint32(uint8 slot)> GlyphSlotRead;

        /// What ApplyGlyph writes to the owner, each called where the old body wrote it.
        struct ApplySinks
        {
            SpellSink castOnSelf;           ///< `CastSpell(owner, spellId, true)`
            SpellSink removeAuras;          ///< `RemoveAurasDueToSpell(spellId)`
            FieldSink setField;             ///< `SetUInt32Value(index, value)`
        };

        /// What a loaded row needs from the owner, read by the owner for each row. The scalars
        /// default to 0 and NULL so that none is ever indeterminate; every builder (the owner's
        /// _LoadGlyphs, the test's Inputs) sets every field anyway.
        struct RowInputs
        {
            uint32 ownerGuidLow = 0;        ///< the owner's GetGUIDLow(): the per-character DELETEs
            char const* ownerName = NULL;   ///< the owner's GetName(): the log lines
            GlyphSlotRead glyphSlot;        ///< the owner's GetGlyphSlot(slot), called where the old body read it
        };

        /**
         * @brief Refreshes glyph slot types and unlock mask for the owner's level.
         *
         * Writes the slot type of every glyph slot, in the glyph slot store's order, and sets
         * PLAYER_GLYPHS_ENABLED with the bitmask of slots unlocked at this level. Called on level
         * change, on character creation, at login and by the GM `.reset` commands.
         *
         * @param level        The owner's level, getLevel().
         * @param setGlyphSlot Writes the owner's slot type field of one slot.
         * @param setField     Writes PLAYER_GLYPHS_ENABLED.
         */
        void InitGlyphsForLevel(uint32 level, GlyphSlotSink const& setGlyphSlot, FieldSink const& setField);

        /**
         * @brief Apply or remove the spell from a single glyph slot on the owner.
         *
         * @param activeSpec The owner's active spec, TalentMgr::ActiveSpec().
         * @param slot       The glyph slot index.
         * @param apply      True to cast and write the slot's spell, false to remove it.
         * @param sinks      The cast, the aura removal and the PLAYER_FIELD_GLYPHS_1 + slot field.
         */
        void ApplyGlyph(uint8 activeSpec, uint8 slot, bool apply, ApplySinks const& sinks);

        /**
         * @brief Load one row of the login holder's glyph result (spec, slot, glyph).
         *
         * An invalid row is logged and deleted, and leaves the array as it was.
         *
         * @param fields The row.
         * @param inputs The owner's guid, name and glyph slot read.
         */
        void LoadRow(Field* fields, RowInputs const& inputs);

        /**
         * @brief Persist dirty glyph slots to character_glyphs.
         *
         * Emits INSERT / UPDATE / DELETE for each dirty slot of the first specsCount specs, then
         * clears their dirty flags.
         *
         * @param ownerGuidLow The owner's GetGUIDLow().
         * @param specsCount   The owner's TalentMgr::SpecsCount().
         */
        void Save(uint32 ownerGuidLow, uint8 specsCount);

        /**
         * @brief Returns the glyph id stored in a given spec / slot.
         *
         * @param spec The talent spec index.
         * @param slot The glyph slot index.
         * @return The glyph property id in that slot, or 0 if empty.
         */
        uint32 GetGlyph(uint8 spec, uint8 slot) const { return m_glyphs[spec][slot].GetId(); }

        /**
         * @brief Writes a glyph id into a given spec / slot.
         *
         * @param spec The talent spec index.
         * @param slot The glyph slot index.
         * @param id   The new glyph property id, or 0 to clear the slot.
         */
        void   SetGlyph(uint8 spec, uint8 slot, uint32 id) { m_glyphs[spec][slot].SetId(id); }

    private:
        Glyph   m_glyphs[MAX_TALENT_SPEC_COUNT][MAX_GLYPH_SLOT_INDEX];  ///< Per-spec, per-slot glyph state owned by this manager.
};

#endif // MANGOS_H_GLYPHMGR
