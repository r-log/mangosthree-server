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

#ifndef MANGOS_H_RUNEMGR
#define MANGOS_H_RUNEMGR

#include "Platform/Define.h"
#include "Common/TimeConstants.h"
#include "ManagerPacketSink.h"
#include "SharedDefines.h"

#include <functional>

class Aura;
class WorldPacket;

#define MAX_RUNES               6

enum RuneCooldowns
{
    RUNE_BASE_COOLDOWN          = 10000,
    RUNE_MISS_COOLDOWN          = 1500     // cooldown applied on runes when the spell misses
};

struct RuneInfo
{
    uint8  BaseRune;
    uint8  CurrentRune;
    uint16 BaseCooldown;
    uint16 Cooldown;                                        // msec
    Aura const* ConvertAura;
};

struct Runes
{
    RuneInfo runes[MAX_RUNES];
    uint8 runeState;                                        // mask of available runes
    uint32 lastUsedRuneMask;

    void SetRuneState(uint8 index, bool set = true)
    {
        if (set)
        {
            runeState |= (1 << index);                      // usable
        }
        else
        {
            runeState &= ~(1 << index);                     // on cooldown
        }
    }
};

/**
 * @brief Decoupling D4k: a death knight's six runes and the rules over them, held apart from the
 * object that plays the character.
 *
 * The state is the six slots (base type, current type, base cooldown, cooldown, the aura that
 * converted the slot), the mask of usable runes and the mask of the rune types the last cast
 * used. It is runtime-only: Init() rebuilds it at character creation, at login and on the GM
 * commands `.reset level` and `.reset stats`, and nothing saves it. The owner holds one by value
 * and asks for it only for a death knight (its callers check the class first).
 *
 * The object carries no owner. What it used to read from the owner is handed in at the call --
 * the class at Init(), the regeneration auras' multiplier and the melee haste rating at
 * UpdateRuneRegen(), the facts about a slot's convert aura at RestoreBaseRune() -- and what it
 * used to write to the owner goes out through a callback called at the exact point the old body
 * wrote it: the rune packets (built here, handed to a PacketSink), the regeneration fields (a
 * RegenSink) and the removal of a convert aura that no slot holds any more (an AuraDrop). So
 * `mangos_tests` builds one from nothing and reads every packet's bytes.
 *
 * THE CONVERT AURA IS AN OPAQUE IDENTITY. A slot stores the `Aura const*` that converted it; this
 * object stores it, compares it and hands it back, and never dereferences it (Aura is only
 * forward-declared here, and CheckHeaderReach keeps SpellAuras.h out of reach of both files).
 *
 * What stays with the owner, and why: the regeneration auras' multiplier, the haste rating and
 * the convert aura's facts (they read the owner's auras and ratings), the loop that updates all
 * four regeneration rates (each needs its own multiplier), and AddRuneByAuraEffect (the
 * Death Eater cast of the T11 bonus, then two calls here).
 *
 * KEPT SEMANTICS, stated rather than fixed (backlog): the constructor leaves the state
 * uninitialised, as it always did -- Init() fills it for a death knight only, and it never
 * touches the last-used mask; RemoveRunesByAuraEffect(NULL) restores every slot that has no
 * convert aura, sending a packet for each.
 */
class RuneMgr
{
    public:
        /// Hands a built rune packet to the owner's session: `GetSession()->SendPacket(packet)`.
        typedef ManagerPacketSink PacketSink;
        /// Writes one rune type's regeneration rate: `SetFloatValue(PLAYER_RUNE_REGEN_1 + runeType, value)`.
        typedef std::function<void(uint32 runeType, float value)> RegenSink;
        /// Takes a convert aura that no slot holds any more off its target.
        typedef std::function<void(Aura const* aura)> AuraDrop;

        /// What the owner knows about a slot's convert aura, read just before RestoreBaseRune.
        /// Each fact is read only for a non-NULL aura; for NULL the owner passes false. The facts
        /// default to false so that none is ever indeterminate; every builder (the owner's
        /// RestoreBaseRune, the test's Facts) sets all three anyway.
        struct ConvertAuraFacts
        {
            bool nonPassive = false;            ///< the aura's spell is not passive
            bool bloodOfTheNorthHeld = false;   ///< the aura is Blood of the North (54637) and the owner has aura 54637
            bool convertsRunes = false;         ///< the aura's modifier is SPELL_AURA_CONVERT_RUNE
        };

        /// The empty body keeps the state default-initialised exactly as before (the owner-bound
        /// constructor did not touch it); Init() remains the only initialiser. `= default` would let
        /// the owner's `m_runeMgr()` zero it instead: unobservable today, but not the old code (backlog).
        RuneMgr() {}

        uint8 GetRunesState() const { return m_data.runeState; }
        RuneType GetBaseRune(uint8 index) const { return RuneType(m_data.runes[index].BaseRune); }
        RuneType GetCurrentRune(uint8 index) const { return RuneType(m_data.runes[index].CurrentRune); }
        uint16 GetRuneCooldown(uint8 index) const { return m_data.runes[index].Cooldown; }
        uint16 GetBaseRuneCooldown(uint8 index) const { return m_data.runes[index].BaseCooldown; }
        Aura const* GetRuneConvertAura(uint8 index) const { return m_data.runes[index].ConvertAura; }
        uint8 GetRuneCooldownFraction(uint8 index) const;
        /// `auraMod` is the product of the regeneration auras' multipliers, `hasteRating` the
        /// melee haste rating bonus in percent; both are the owner's, read by the owner.
        void UpdateRuneRegen(RuneType rune, float auraMod, float hasteRating, RegenSink const& setRegen) const;
        bool IsBaseRuneSlotsOnCooldown(RuneType runeType) const;
        void ClearLastUsedRuneMask() { m_data.lastUsedRuneMask = 0; }
        bool IsLastUsedRune(uint8 index) const { return (m_data.lastUsedRuneMask & (1 << index)) != 0; }
        void SetLastUsedRune(RuneType type) { m_data.lastUsedRuneMask |= 1 << uint32(type); }
        void SetBaseRune(uint8 index, RuneType baseRune) { m_data.runes[index].BaseRune = baseRune; }
        void SetCurrentRune(uint8 index, RuneType currentRune) { m_data.runes[index].CurrentRune = currentRune; }
        void SetRuneCooldown(uint8 index, uint16 cooldown) { m_data.runes[index].Cooldown = cooldown; m_data.SetRuneState(index, (cooldown == 0) ? true : false); }
        void SetBaseRuneCooldown(uint8 index, uint16 cooldown) { m_data.runes[index].BaseCooldown = cooldown; }
        void SetRuneConvertAura(uint8 index, Aura const* aura) { m_data.runes[index].ConvertAura = aura; }
        void RemoveRunesByAuraEffect(Aura const* aura, PacketSink const& send);
        void RestoreBaseRune(uint8 index, ConvertAuraFacts const& facts, PacketSink const& send, AuraDrop const& dropAura);
        void ConvertRune(uint8 index, RuneType newType, PacketSink const& send);
        bool ActivateRunes(RuneType type, uint32 count);
        void ResyncRunes(PacketSink const& send) const;
        void AddRunePower(uint8 index, PacketSink const& send) const;
        /// `classId` is the owner's class; anything but a death knight leaves the state untouched.
        void Init(uint8 classId, RegenSink const& setRegen);

    private:
        Runes m_data;
};

#endif
