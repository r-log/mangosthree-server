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

#ifndef MANGOS_H_CHARACTERCACHE_H
#define MANGOS_H_CHARACTERCACHE_H

#include "ObjectGuid.h"
#include "SharedDefines.h"
#include "Policies/Singleton.h"

#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>

/// Arena team slots, one per bracket. Kept as its own constant so this header does not
/// have to pull ArenaTeam.h in; CharacterCache.cpp static_asserts the two agree.
#define CHARACTER_CACHE_ARENA_SLOTS 3

/**
 * @brief Everything the server asks about a character that is not logged in.
 *
 * One row of `characters` plus that character's `guild_member` and
 * `arena_team_member` rows, which together are the whole of what the ten offline
 * lookups used to go to MySQL for.
 *
 * Entries are IMMUTABLE once published. A writer builds a copy, changes the one
 * field and publishes the copy; a reader that already holds the old one keeps a
 * valid object to read until it lets go. That is what makes a value read while
 * another thread writes it either the old one or the new one, never a half-written
 * string.
 */
struct CharacterCacheEntry
{
    ObjectGuid  guid;
    uint32      accountId = 0;
    std::string name;
    uint8       race = 0;
    uint8       playerClass = 0;
    uint8       level = 0;
    uint32      zoneId = 0;
    uint32      guildId = 0;                                ///< 0 = none
    uint32      guildRank = 0;                              ///< meaningless when guildId is 0
    uint32      arenaTeamId[CHARACTER_CACHE_ARENA_SLOTS] = { 0, 0, 0 };  ///< by slot, 0 = none
};

/// What a lookup hands back: a reference to an entry that cannot change under it.
typedef std::shared_ptr<CharacterCacheEntry const> CharacterCacheRef;

/**
 * @brief The in-memory index of every character on the realm.
 *
 * Loaded once at startup and kept current at every setter that changes one of the
 * cached fields, so `Player::GetLevelFromDB` and its nine siblings answer from memory
 * instead of blocking the tick on MySQL (decoupling D7c).
 *
 * @note The one thing this cache does not see is a `characters`, `guild_member` or
 * `arena_team_member` row edited OUTSIDE the server while it runs: that edit is
 * invisible until the next restart. Every other table the server loads at startup is
 * already treated that way.
 *
 * Threading: the world thread and the map-update workers both read and write it (a
 * level-up on a worker, a lookup from a handler on another). The two indexes have to
 * move together, so they are guarded by one `std::shared_mutex` -- readers shared,
 * writers exclusive. That is the same primitive and the same discipline as
 * MaNGOS::ConcurrentRegistry (src/shared/Utilities/ConcurrentRegistry.h), which is the
 * tree's own reader/writer container; it is not reused here because it is a single-key
 * index of NON-owning pointers, and this needs two indexes mutated as one, over entries
 * it owns.
 */
class CharacterCache : public MaNGOS::Singleton<CharacterCache>
{
        friend class MaNGOS::Singleton<CharacterCache>;

    public:

        /// The three startup reads: `characters`, `guild_member`, `arena_team_member`.
        /// Called from World::SetInitialWorldSettings, before the tick exists.
        void LoadFromDB();

        // ---- read side -------------------------------------------------------------

        CharacterCacheRef GetByGuid(ObjectGuid guid) const;

        /// By name, the way `characters`.`name` compares: that column is utf8_general_ci,
        /// so the SELECT this replaces matched without regard to case AND treated several
        /// accented letters as their base letter. CharacterCache.cpp's generated fold table
        /// is what reproduces it; do not simplify it to a lower-case.
        CharacterCacheRef GetByName(std::string const& name) const;

        /// The team the cached race belongs to, or TEAM_NONE when the character is unknown.
        Team GetTeam(ObjectGuid guid) const;

        /// The owning account, or 0 when the character is unknown.
        uint32 GetAccountId(ObjectGuid guid) const;

        size_t Size() const;

        // ---- write side, one method per event --------------------------------------

        /// A character was created (its row now exists).
        void Add(CharacterCacheEntry const& entry);

        /// A character was deleted for good (`DELETE FROM characters`).
        void Remove(ObjectGuid guid);

        /// Rename, customize-with-rename, and the name-clearing half of a soft delete.
        void UpdateName(ObjectGuid guid, std::string const& name);

        /// The soft delete unlinks the row from its account; the restore relinks it.
        void UpdateAccount(ObjectGuid guid, uint32 accountId);

        void UpdateLevel(ObjectGuid guid, uint8 level);
        void UpdateZone(ObjectGuid guid, uint32 zoneId);

        /// Joined (guildId != 0, with the rank) or left (guildId 0).
        void UpdateGuild(ObjectGuid guid, uint32 guildId, uint32 rank = 0);
        void UpdateGuildRank(ObjectGuid guid, uint32 rank);

        /// Every member of this guild left it at once (a broken guild dropped at load).
        void ClearGuild(uint32 guildId);

        void UpdateArenaTeam(ObjectGuid guid, uint8 slot, uint32 arenaTeamId);

        /// Every member of this arena team left it at once (an orphan team at load).
        void ClearArenaTeam(uint32 arenaTeamId);

        // ---- the zone repair -------------------------------------------------------

        /// The position of a character whose stored `zone` is 0, which is the only case
        /// Player::GetZoneIdFromDB has ever needed a position for. False when the
        /// character is unknown or its zone was stored properly.
        bool GetPositionForZonelessCharacter(ObjectGuid guid, uint32& mapId,
                                             float& x, float& y, float& z) const;

        /// Drop a repaired character from the zoneless set (its zone is known now).
        void ForgetZonelessPosition(ObjectGuid guid);

        /// Test-only: empty both indexes, as if nothing had ever been loaded.
        void Clear();

    private:

        CharacterCache() = default;
        ~CharacterCache() = default;

        struct Position
        {
            uint32 mapId = 0;
            float  x = 0.0f;
            float  y = 0.0f;
            float  z = 0.0f;
        };

        typedef std::unordered_map<ObjectGuid, std::shared_ptr<CharacterCacheEntry> > EntryMap;
        typedef std::unordered_map<std::string, ObjectGuid> NameMap;
        typedef std::unordered_map<ObjectGuid, Position> PositionMap;

        /// The lower-cased form the name index is keyed on. Empty in, empty out.
        static std::string FoldName(std::string const& name);

        /// The copy-on-write step every setter shares: find, copy, hand the copy to
        /// `change`, publish it. Does nothing when the character is not cached.
        template <typename F>
        void Mutate(ObjectGuid guid, F&& change);

        /// Re-key the name index for an entry whose name is about to become `newName`.
        /// Caller holds the exclusive lock.
        void ReindexNameLocked(std::string const& oldName, std::string const& newName,
                               ObjectGuid guid);

        mutable std::shared_mutex m_lock;
        EntryMap                  m_entries;
        NameMap                   m_byName;
        PositionMap               m_zoneless;
};

#define sCharacterCache MaNGOS::Singleton<CharacterCache>::Instance()

#endif
