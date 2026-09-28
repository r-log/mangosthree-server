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

#ifndef MANGOS_H_CURRENCYMGR
#define MANGOS_H_CURRENCYMGR

#include "Platform/Define.h"
#include "DBCEnums.h"                                       // AchievementCriteriaTypes
#include "ManagerPacketSink.h"

#include <functional>
#include <unordered_map>

class Field;
class ObjectGuid;
struct CurrencyTypesEntry;

/**
 * Per-currency flags persisted in character_currencies.flags. Cata client
 * only uses two: SHOW_IN_BACKPACK toggles whether the currency appears in
 * the on-screen tracker, UNUSED is reserved.
 */
enum PlayerCurrencyFlag
{
    PLAYERCURRENCY_FLAG_NONE                = 0x0,
    PLAYERCURRENCY_FLAG_UNK1                = 0x1,  // unused?
    PLAYERCURRENCY_FLAG_UNK2                = 0x2,  // unused?
    PLAYERCURRENCY_FLAG_SHOW_IN_BACKPACK    = 0x4,
    PLAYERCURRENCY_FLAG_UNUSED              = 0x8,

    PLAYERCURRENCY_MASK_USED_BY_CLIENT =
        PLAYERCURRENCY_FLAG_SHOW_IN_BACKPACK |
        PLAYERCURRENCY_FLAG_UNUSED,
};

/**
 * Per-currency-row dirty state. Drives whether Save emits INSERT, UPDATE,
 * DELETE, or nothing.
 */
enum PlayerCurrencyState
{
    PLAYERCURRENCY_UNCHANGED        = 0,
    PLAYERCURRENCY_CHANGED          = 1,
    PLAYERCURRENCY_NEW              = 2,
    PLAYERCURRENCY_REMOVED          = 3
};

/**
 * Per-currency state held in the map keyed by currency-id.
 * currencyEntry caches the DBC row to avoid repeated lookups; it must be
 * populated whenever a new entry is added to the map.
 */
struct PlayerCurrency
{
    PlayerCurrencyState state;
    uint32 totalCount;
    uint32 weekCount;
    uint32 seasonCount;
    uint8 flags;
    CurrencyTypesEntry const* currencyEntry;
};

typedef std::unordered_map<uint32, PlayerCurrency> PlayerCurrenciesMap;

/**
 * @brief Decoupling D4k: a character's Cataclysm currencies (honor, conquest, justice, valor and the
 * per-meta categories) and the rules over them, held apart from the object that plays the
 * character.
 *
 * The state is the map from currency id to its counts (total, this week, this season), its client
 * flags, its save state and its DBC row. The owner holds one by value, fills it row by row at
 * login (LoadRow) and saves it with the rest of the character (Save). The rules are the counts,
 * the caps (a week cap from the DBC, conquest points' from the server config; a total cap from the
 * DBC), ModifyCount's arithmetic -- the gain multiplier, the clamps and the state transitions, a
 * meta currency forwarded to conquest points -- the flags, the weekly reset, and the bytes of the
 * four currency packets.
 *
 * The object carries no owner. What it used to read from the owner is handed in at the call
 * (CurrencyInputs): the owner's currency-gain aura multiplier and whether the owner may be told
 * about a change (in the world and not loading) as read callbacks, called where the old body read
 * them -- the multiplier takes the currency id, and the check is read again after the owner's own
 * effects have run -- and the conquest week cap from the server config as a value; the owner's
 * guid for the load's log line and for the save. What it used to do to the owner goes out through
 * callbacks called at the exact point the old body did it (ModifySinks): the achievement update,
 * the packets (a ManagerPacketSink, the owner's session) and the owner's two currency quest checks.
 * Callbacks are parameters only, never stored. So `mangos_tests` builds one from nothing.
 *
 * It reads one global store itself, as before: the currency types store (a new id's row, and a
 * loaded row's).
 *
 * What stays with the owner: buying currency at a vendor (it calls ModifyCount through the owner),
 * the loot notification, and the two convenience checks HasCurrencyCount / HasCurrencySeasonCount
 * over the count getters.
 *
 * KEPT SEMANTICS, stated rather than fixed (backlog): an unknown id in a loaded row is deleted
 * from every character's rows (the DELETE names only the id); SetFlags and ResetWeekCounts turn a
 * NEW entry CHANGED, so its first save is an UPDATE of a row that is not there; the total cap's
 * excess is taken off the week count even when the week is not being modified (the week count can
 * go below zero and is stored wrapped); the week cap's excess is taken off the total, so with the
 * week count already over a lowered conquest cap a change costs the whole excess; a meta
 * currency's gain has the gain multiplier applied twice (once for the meta, once more when it is
 * forwarded to conquest points); a gain accumulates the new TOTAL into the achievement
 * criterion; the REMOVED state is never set.
 */
class CurrencyMgr
{
    public:
        /// Hands a built currency packet to the owner's session.
        typedef ManagerPacketSink PacketSink;
        /// The owner's currency-gain multiplier for `currencyId`:
        /// `GetTotalAuraMultiplierByMiscValue(SPELL_AURA_MOD_CURRENCY_GAIN, currencyId)`.
        typedef std::function<float(uint32 currencyId)> GainMultiplier;
        /// Whether the owner may be told about a change now: `IsInWorld() && !GetSession()->PlayerLoading()`.
        typedef std::function<bool()> NotifyCheck;
        /// The owner's `UpdateAchievementCriteria(type, miscValue1, miscValue2)`.
        typedef std::function<void(AchievementCriteriaTypes type, uint32 miscValue1, uint32 miscValue2)> AchievementUpdate;
        /// One of the owner's currency quest checks: `CurrencyAddedQuestCheck(currencyId)` or
        /// `CurrencyRemovedQuestCheck(currencyId)`.
        typedef std::function<void(uint32 currencyId)> QuestCheck;

        /// What the currency rules read from the owner. The two reads are callbacks, called where
        /// the old body read them; the cap is a value the owner reads just before the call. The
        /// scalar defaults to 0 so that no field is ever indeterminate; every builder (the owner's
        /// ReadCurrencyInputs, the test's Wire) sets every field anyway.
        struct CurrencyInputs
        {
            GainMultiplier gainMultiplier;          ///< the owner's currency-gain aura multiplier, by currency id
            NotifyCheck canNotify;                  ///< the owner is in the world and not loading
            uint32 conquestWeekCap = 0;             ///< CONFIG_UINT32_CURRENCY_CONQUEST_POINTS_DEFAULT_WEEK_CAP
        };

        /// What a count change does to the owner, each called at the old statement.
        struct ModifySinks
        {
            AchievementUpdate updateAchievement;    ///< a gain's achievement update
            PacketSink send;                        ///< SMSG_SET_CURRENCY and a new currency's week limit
            QuestCheck addedQuestCheck;             ///< after a gain
            QuestCheck removedQuestCheck;           ///< after a loss
        };

        // Totals
        uint32 GetCount(uint32 id) const;
        uint32 GetSeasonCount(uint32 id) const;
        uint32 GetWeekCount(uint32 id) const;

        // Caps. Take a CurrencyTypesEntry rather than an id so callers
        // already holding the DBC entry don't re-look it up.
        uint32 GetWeekCap(CurrencyTypesEntry const* currency, uint32 conquestWeekCap) const;
        uint32 GetTotalCap(CurrencyTypesEntry const* currency) const;

        // Mutations
        void ModifyCount(uint32 id, int32 count, bool modifyWeek, bool modifySeason, bool ignoreMultipliers, CurrencyInputs const& inputs, ModifySinks const& sinks);
        void SetCount(uint32 id, uint32 count, CurrencyInputs const& inputs, ModifySinks const& sinks);
        void SetFlags(uint32 currencyId, uint8 flags);
        void ResetWeekCounts(PacketSink const& send);

        // Client notifications
        void SendAll(uint32 conquestWeekCap, PacketSink const& send) const;
        void SendWeekCap(uint32 id, CurrencyInputs const& inputs, PacketSink const& send) const;
        void SendWeekCap(CurrencyTypesEntry const* currency, CurrencyInputs const& inputs, PacketSink const& send) const;

        // DB lifecycle
        /// One row of the login holder's currency result: id, total, week, season, flags. The
        /// owner keeps the loop. `ownerGuid` is for the log line of an unknown id.
        void LoadRow(Field* fields, uint32 conquestWeekCap, ObjectGuid ownerGuid);
        void Save(uint32 ownerGuidLow);

    private:
        PlayerCurrenciesMap m_currencies;
};

#endif // MANGOS_H_CURRENCYMGR
