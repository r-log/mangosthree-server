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

#ifndef MANGOS_H_TALENTMGR
#define MANGOS_H_TALENTMGR

#include "SharedDefines.h"

#include <ctime>
#include <functional>
#include <unordered_map>

/**
 * @file TalentMgr.h
 * @brief Decoupling D4c: one character's talents and specs, held apart from the object that plays it.
 *
 * The state is the per-spec talent maps, the primary tree of each spec, the active spec and the
 * spec count, the free and used talent points, and the cost and time of the last paid reset.
 * The object carries no owner: the character's guid and class mask, the game time, the talent
 * rate and the quest-reward talent bonus are handed in at the call, so `mangos_tests` can build
 * one from nothing and drive it with fake rows.
 *
 * What stays with the owning object, and why: learning and unlearning the talent spells (the
 * spell book, auras, packets), switching specs (it unlearns and learns spells, re-applies glyphs,
 * unsummons the pet and resets power), changing the spec count (action buttons, packets),
 * turning the points into a reset when they no longer fit (it may unlearn), the paid reset
 * (money, achievements) and the talent packets. The quest-reward talent bonus stays there too:
 * the quest load and the quest reward write it.
 *
 * THE MAPS ARE HANDED OUT BY REFERENCE. `Talents(spec)` is the map itself, never a copy. The
 * spec switch walks the active map while the spell removal it triggers erases entries from that
 * same map through `UnlearnRank`, restarts its walk from `begin()`, and afterwards writes saved
 * states back into the entries `LearnRank` created; all of that only works on the one map.
 *
 * THE LOAD IS PER ROW. `LoadRow` does ONE `character_talent` row: the validations, the DELETE of
 * a row that fails one, and the inactive-spec insertion. An active-spec row is learned by the
 * owner, through `onActiveSpell`, at the exact point of the row where the old loop learned it:
 * that learn reaches `LearnRank` and `SpendPoints` here and may recompute the free points, so a
 * later row must see the earlier rows' effects exactly as before.
 *
 * KEPT SEMANTICS, stated rather than fixed (backlog): three of the six load DELETEs have no
 * `guid` predicate (an unknown talent, an unknown talent tab, a spec above the maximum) and so
 * delete the row for every character; the spec check `spec > MAX_TALENT_SPEC_COUNT` is one too
 * lenient and is masked by the spec-count check after it; the rank check reads `SpellRank[rank]`
 * for a rank equal to `MAX_TALENT_RANK`, one past the array.
 */

struct TalentEntry;
struct SpellEntry;
class Field;

/**
 * @brief Save state of one row of the spell book or the talent maps.
 *
 * Shared with the spell book, which stays with the owning object; it is defined here because
 * the talent maps are its first user that may not include the owner's header.
 */
enum PlayerSpellState
{
    PLAYERSPELL_UNCHANGED = 0, ///< Spell unchanged
    PLAYERSPELL_CHANGED = 1,   ///< Spell changed
    PLAYERSPELL_NEW = 2,       ///< New spell
    PLAYERSPELL_REMOVED = 3    ///< Spell removed
};

/// One learned talent of one spec: the talent, the rank index learned, and its save state.
struct PlayerTalent
{
    TalentEntry const* talentEntry;
    uint32 currentRank;
    PlayerSpellState state;
};

/// Talent id -> the learned talent, one map per spec.
typedef std::unordered_map<uint32, PlayerTalent> PlayerTalentMap;

class TalentMgr
{
    public:
        /// Learns one active-spec talent spell; the owner's `addSpell(spellId, true, false, false, false)`.
        typedef std::function<void(uint32 spellId)> ActiveSpellCallback;

        TalentMgr();

        /*** state ***/

        /// The spec's talent map itself: iterator- and reference-stable, never a copy.
        PlayerTalentMap& Talents(uint8 spec) { return m_talents[spec]; }
        PlayerTalentMap const& Talents(uint8 spec) const { return m_talents[spec]; }

        uint8 ActiveSpec() const { return m_activeSpec; }
        void SetActiveSpec(uint8 spec) { m_activeSpec = spec; }
        uint8 SpecsCount() const { return m_specsCount; }
        void SetSpecsCount(uint8 count) { m_specsCount = count; }

        uint32 PrimaryTree(uint8 spec) const { return m_primaryTree[spec]; }
        void SetPrimaryTree(uint8 spec, uint32 tree) { m_primaryTree[spec] = tree; }

        uint32 FreePoints() const { return m_freePoints; }
        void SetFreePoints(uint32 points) { m_freePoints = points; }
        uint32 UsedPoints() const { return m_usedPoints; }

        /// The cost and time of the last paid reset (`characters` columns, loaded and saved by the owner).
        uint32 ResetCost() const { return m_resetCost; }
        void SetResetCost(uint32 cost) { m_resetCost = cost; }
        time_t ResetTime() const { return m_resetTime; }
        void SetResetTime(time_t resetTime) { m_resetTime = resetTime; }

        /*** queries ***/

        /// The active spec's talent, NULL when it is not held or is marked removed.
        PlayerTalent const* GetKnownTalentById(int32 talentId) const;
        /// The spell of the active spec's learned rank of the talent, NULL when not held.
        SpellEntry const* GetKnownTalentRankById(int32 talentId) const;
        /// What the next paid reset costs, in copper: 1, 5, 10 gold, then +5 gold per reset up to
        /// 50 gold, falling by 5 gold per month since the last one down to 10 gold.
        uint32 ResetTalentsCost(time_t now) const;
        /// The talent points a character of `level` and `classId` has, before any is spent:
        /// `NumTalentsAtLevel.dbc` times `rate`; a death knight's are the quest-reward bonus plus
        /// the levels past 55, capped at the other classes' count.
        static uint32 CalculateTalentsPoints(uint32 level, uint8 classId, float rate, uint32 questRewardTalents);

        /*** the spell book's talent upkeep ***/

        /// A talent spell was learned: the rank goes into the active spec's map. A new entry is
        /// NEW when `inWorld`, else UNCHANGED (loading); a held one takes the rank and becomes
        /// CHANGED unless it is NEW, when it was removed or its rank differs.
        void LearnRank(uint32 talentId, uint32 rank, bool inWorld);
        /// A talent spell was removed: the active spec's entry is marked REMOVED, or erased when
        /// it is NEW. False when the active spec does not hold the talent (the owner logs it).
        bool UnlearnRank(uint32 talentId);
        /// The used-point count of the active spec's learned ranks.
        void SpendPoints(uint32 cost) { m_usedPoints += cost; }
        /// Frees `cost` used points, never below zero.
        void RefundPoints(uint32 cost);

        /*** rows ***/

        /// One `character_talent` row: `talent_id, current_rank, spec`. `guidLow` is the
        /// character's, for the log lines and the scoped DELETEs; `classMask` is the character's
        /// class mask; `specsCount` and `activeSpec` are read by the owner at the start of the row.
        void LoadRow(Field* fields, uint32 guidLow, uint32 classMask, uint8 specsCount, uint8 activeSpec,
                     ActiveSpellCallback const& onActiveSpell);
        /// DELETE for every REMOVED or CHANGED entry, INSERT for every NEW or CHANGED one, spec by
        /// spec; REMOVED entries are erased and the rest become UNCHANGED. Asynchronous, as before.
        void SaveTalents(uint32 guidLow);

    private:
        PlayerTalentMap m_talents[MAX_TALENT_SPEC_COUNT];
        uint32 m_primaryTree[MAX_TALENT_SPEC_COUNT];
        uint8 m_activeSpec;
        uint8 m_specsCount;
        uint32 m_freePoints;
        uint32 m_usedPoints;
        uint32 m_resetCost;
        time_t m_resetTime;
};

#endif
