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

#ifndef MANGOS_H_SPELLCOOLDOWNMGR
#define MANGOS_H_SPELLCOOLDOWNMGR

#include "Platform/Define.h"
#include "Common/TimeConstants.h"
#include "ManagerPacketSink.h"
#include <ctime>
#include <functional>
#include <map>

class Field;
class ObjectGuid;
struct ItemPrototype;
struct SpellEntry;

/**
 * @brief Structure to hold spell cooldown information
 */
struct SpellCooldown
{
    time_t end;    ///< End time of the cooldown
    uint16 itemid; ///< Item ID associated with the cooldown
};

typedef std::map<uint32, SpellCooldown> SpellCooldowns;

/**
 * @brief Decoupling D4k: a character's active spell cooldowns and the rules over them, held apart
 * from the object that plays the character.
 *
 * The state is the map from spell id to the cooldown's end (wall-clock seconds) and the item that
 * started it. The owner holds one by value, fills it row by row at login (LoadRow), and saves it
 * with the rest of the character (SaveToDB).
 *
 * The object carries no owner. What it used to read from the owner is handed in at the call --
 * the clock (`now`, the owner's `time(NULL)`), the owner's guid, and for a cast the facts in
 * CastInputs -- and what it used to write to the owner goes out through a callback called at the
 * exact point the old body wrote: the cooldown packets it builds (a PacketSink, the owner's
 * session), the owner's one-spell clear (a ClearSink) and the owner's cooldown spell mods (the
 * in/out CooldownMod). Callbacks are parameters only, never stored. So `mangos_tests` builds one
 * from nothing, with fixed clocks, and reads every packet's bytes.
 *
 * It reads three global stores itself, as before: the spell store (a loaded row's spell must
 * exist; the arena reset reads each spell's recovery times), the spell category sets (the
 * category cooldowns) and, through the spell entry, the spell cooldown and category rows. The
 * item prototype is a lookup the owner passes in (CastInputs::itemPrototype).
 *
 * What stays with the owner, and why: the potion bookkeeping (UpdatePotionCooldown reads and
 * clears the owner's last-potion id, its combat state and the cast), and the one-spell clear
 * packet itself (the owner sends it for its pet's spells as well).
 *
 * KEPT SEMANTICS, stated rather than fixed (backlog): the item id is held as uint16, so an item id
 * above 65535 is truncated here, in the save and in the login's spell list; an end is in whole
 * seconds (a recovery's milliseconds are divided down, so under a second ends at once); a
 * one-spell remove with `update` sends its clear even when the spell had no cooldown.
 */
class SpellCooldownMgr
{
    public:
        static uint32 const infinityCooldownDelay = MONTH; // used for set "infinity cooldowns" for spells and check
        static uint32 const infinityCooldownDelayCheck = MONTH / 2;

        /// Hands a built cooldown packet to the owner's session.
        typedef ManagerPacketSink PacketSink;
        /// Sends the owner's one-spell clear for `spellId`: `SendClearCooldown(spellId, owner)`.
        typedef std::function<void(uint32 spellId)> ClearSink;
        /// The item prototype store's lookup: `ObjectMgr::GetItemPrototype`.
        typedef std::function<ItemPrototype const*(uint32 itemId)> ItemPrototypeLookup;
        /// Applies the owner's cooldown spell mods to `cooldown` in place:
        /// `ApplySpellMod(spellId, SPELLMOD_COOLDOWN, cooldown, spell)`, the cast passed through.
        typedef std::function<void(uint32 spellId, int32& cooldown)> CooldownMod;

        /// What a cast's cooldown needs from the owner, read by the owner just before the call.
        /// The scalars default to false and 0 so that no field is ever indeterminate; every builder
        /// (the owner's ReadCastInputs, the test's Wire) sets every field anyway.
        struct CastInputs
        {
            ItemPrototypeLookup itemPrototype;      ///< the item prototype store
            bool autoRepeatRanged = false;          ///< the spell is an auto-repeat ranged spell (IsAutoRepeatRangedSpell)
            uint32 rangedAttackTime = 0;            ///< the owner's ranged attack time, GetAttackTime(RANGED_ATTACK)
            CooldownMod applyCooldownMod;           ///< the owner's cooldown spell mods
        };

        SpellCooldowns const& GetSpellCooldownMap() const { return m_cooldowns; }

        bool HasSpellCooldown(uint32 spell_id, time_t now) const
        {
            SpellCooldowns::const_iterator itr = m_cooldowns.find(spell_id);
            return itr != m_cooldowns.end() && itr->second.end > now;
        }

        time_t GetSpellCooldownDelay(uint32 spell_id, time_t now) const
        {
            SpellCooldowns::const_iterator itr = m_cooldowns.find(spell_id);
            time_t t = now;
            return itr != m_cooldowns.end() && itr->second.end > t ? itr->second.end - t : 0;
        }

        void AddSpellAndCategoryCooldowns(SpellEntry const* spellInfo, uint32 itemId, time_t now, CastInputs const& inputs, bool infinityCooldown = false);
        void AddSpellCooldown(uint32 spell_id, uint32 itemid, time_t end_time);
        void SendCooldownEvent(SpellEntry const* spellInfo, uint32 itemId, time_t now, CastInputs const& inputs, ObjectGuid ownerGuid, PacketSink const& send);
        void RemoveSpellCooldown(uint32 spell_id, bool update, ClearSink const& sendClear);
        void RemoveSpellCategoryCooldown(uint32 cat, bool update, ClearSink const& sendClear);
        void RemoveArenaSpellCooldowns(ClearSink const& sendClear);
        void RemoveAllSpellCooldown(ObjectGuid ownerGuid, PacketSink const& send);
        /// One row of the login holder's cooldown result: spell, item, end time. `now` is the
        /// owner's clock, read once before the first row; `ownerGuidLow` is for the log lines.
        void LoadRow(Field* fields, time_t now, uint32 ownerGuidLow);
        void SaveToDB(uint32 ownerGuidLow, time_t now);

    private:
        SpellCooldowns m_cooldowns;
};

#endif
