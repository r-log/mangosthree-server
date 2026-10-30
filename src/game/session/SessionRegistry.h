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

#ifndef MANGOS_H_SESSIONREGISTRY
#define MANGOS_H_SESSIONREGISTRY

#include "Common/ServerDefines.h"
#include "ObjectGuid.h"
#include "Platform/Define.h"

#include <deque>
#include <list>
#include <mutex>
#include <unordered_map>

class WorldPacket;
class WorldSession;

typedef std::unordered_map<uint32, WorldSession*> SessionMap;

/**
 * The world's client sessions: the session of each connected account, the login queue the player
 * limit holds new ones in, and the peak counts. A session is listed, updated and deleted on the world
 * thread only; Add hands one over from any thread, and the next Update lists it. The world owns the
 * registry and installs it; the code below the world reaches it through sSessionRegistry.
 */
class SessionRegistry
{
    public:
        SessionRegistry();
        /// Deletes every session still listed or still waiting to be listed.
        ~SessionRegistry();

        SessionRegistry(SessionRegistry const&) = delete;
        SessionRegistry& operator=(SessionRegistry const&) = delete;

        /// The installed registry; asserts when none is installed.
        static SessionRegistry& Installed();
        /// Installs `registry` and answers the one it replaces (NULL for none).
        static SessionRegistry* Install(SessionRegistry* registry);

        /// The session of the account, or NULL; a kicked session is listed until the next Update.
        WorldSession* Find(uint32 accountId) const;
        /// Kicks the account's session, which the next Update deletes; false while it loads a character.
        bool Remove(uint32 accountId);
        /// Hands a new session over from any thread; the next Update lists it.
        void Add(WorldSession* session);
        SessionMap const& All() const { return m_sessions; }

        uint32 ActiveAndQueuedCount() const { return m_sessions.size(); }
        uint32 ActiveCount() const { return m_sessions.size() - m_queued.size(); }
        uint32 QueuedCount() const { return m_queued.size(); }
        uint32 MaxActiveCount() const { return m_maxActiveCount; }
        uint32 MaxQueuedCount() const { return m_maxQueuedCount; }

        /// The number of players let in, or 0 when the limit is a security level.
        uint32 PlayerAmountLimit() const { return m_playerLimit >= 0 ? m_playerLimit : 0; }
        /// The lowest security level let in; SEC_PLAYER when the limit is a number of players.
        AccountTypes PlayerSecurityLimit() const { return m_playerLimit <= 0 ? AccountTypes(-m_playerLimit) : SEC_PLAYER; }
        /// Sets the limit: a number of players, or minus the lowest security level let in. Stores the
        /// security level in the realm list when it changes or `needUpdate` asks.
        void SetPlayerLimit(int32 limit, bool needUpdate = false);

        /// Sends `packet` to every player in the world whose account is at least `minSec`.
        void Broadcast(WorldPacket const& packet, AccountTypes minSec = SEC_PLAYER);
        /// Tells every client to forget what it holds of the player `guid`.
        void InvalidatePlayerData(ObjectGuid guid);
        /// Saves every player in the world, registered with the player registry or not yet.
        void SaveAllPlayers() const;
        /// Kicks every session, and empties the login queue first so no queued one is let in.
        void KickAll();
        /// Kicks every session whose account is below `sec`.
        void KickAllLess(AccountTypes sec);

        /// Lists the sessions Add handed over, updates every listed session and deletes the ones
        /// whose update ends them: the one place a listed session is deleted.
        void Update(uint32 diff);

    private:
        typedef std::list<WorldSession*> Queue;

        /// Lists a session handed over by Add: replaces the account's old one, then lets it in or
        /// queues it.
        void List(WorldSession* session);
        void UpdateMaxCounters();
        int32 QueuedPosition(WorldSession* session) const;
        void AddQueued(WorldSession* session);
        /// Takes a session out of the login queue, lets the first queued one in when the limit
        /// allows, and tells the rest their new positions. True when the session was queued.
        bool RemoveQueued(WorldSession* session);

        SessionMap m_sessions;
        Queue m_queued;
        uint32 m_maxActiveCount;
        uint32 m_maxQueuedCount;
        int32 m_playerLimit;

        std::mutex m_addQueueLock;
        std::deque<WorldSession*> m_addQueue;
};

#define sSessionRegistry SessionRegistry::Installed()

#endif
