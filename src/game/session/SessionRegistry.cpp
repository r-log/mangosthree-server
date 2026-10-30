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

#include "session/SessionRegistry.h"

#include "Utilities/Errors.h"
#include "WorldSession.h"
#include "WorldPacket.h"
#include "Opcodes.h"
#include "Player.h"
#include "Log.h"
#include "data/GameConfig.h"
#include "persistence/login/RealmRepository.h"

#include <algorithm>
#include <atomic>

namespace
{
    std::atomic<SessionRegistry*> s_installed(NULL);
}

SessionRegistry::SessionRegistry() : m_maxActiveCount(0), m_maxQueuedCount(0), m_playerLimit(0)
{
}

SessionRegistry::~SessionRegistry()
{
    // it is assumed that no other thread is accessing this data when the destructor is called.  therefore, no locks are necessary
    for (auto const session : m_sessions)
    {
        delete session.second;
    }

    for (auto const session : m_addQueue)
    {
        delete session;
    }
}

SessionRegistry& SessionRegistry::Installed()
{
    SessionRegistry* registry = s_installed.load(std::memory_order_acquire);
    MANGOS_ASSERT(registry);
    return *registry;
}

SessionRegistry* SessionRegistry::Install(SessionRegistry* registry)
{
    return s_installed.exchange(registry, std::memory_order_acq_rel);
}

WorldSession* SessionRegistry::Find(uint32 accountId) const
{
    SessionMap::const_iterator itr = m_sessions.find(accountId);

    if (itr != m_sessions.end())
    {
        return itr->second;
    }                                 // also can return NULL for kicked session
    else
    {
        return NULL;
    }
}

bool SessionRegistry::Remove(uint32 accountId)
{
    ///- Find the session, kick the user, but we can't delete session at this moment to prevent iterator invalidation
    SessionMap::const_iterator itr = m_sessions.find(accountId);

    if (itr != m_sessions.end() && itr->second)
    {
        if (itr->second->PlayerLoading())
        {
            return false;
        }
        itr->second->KickPlayer();
    }

    return true;
}

void SessionRegistry::Add(WorldSession* session)
{
    std::lock_guard<std::mutex> guard(m_addQueueLock);

    m_addQueue.push_back(session);
}

void SessionRegistry::List(WorldSession* s)
{
    MANGOS_ASSERT(s);

    // NOTE - Still there is race condition in WorldSession* being used in the Sockets

    ///- kick already loaded player with same account (if any) and remove session
    ///- if player is in loading and want to load again, return
    if (!Remove(s->GetAccountId()))
    {
        s->KickPlayer();
        delete s;                                           // session not added yet in session list, so not listed in queue
        return;
    }

    // decrease session counts only at not reconnection case
    bool decrease_session = true;

    // if session already exist, prepare to it deleting at next world update
    // NOTE - KickPlayer() should be called on "old" in Remove()
    {
        SessionMap::const_iterator old = m_sessions.find(s->GetAccountId());

        if (old != m_sessions.end())
        {
            // prevent decrease sessions count if session queued
            if (RemoveQueued(old->second))
            {
                decrease_session = false;
            }
            // not remove replaced session form queue if listed
            delete old->second;
        }
    }

    m_sessions[s->GetAccountId()] = s;

    uint32 Sessions = ActiveAndQueuedCount();
    uint32 pLimit = PlayerAmountLimit();
    uint32 QueueSize = QueuedCount();                       // number of players in the queue

    // so we don't count the user trying to
    // login as a session and queue the socket that we are using
    if (decrease_session)
    {
        --Sessions;
    }

    if (pLimit > 0 && Sessions >= pLimit && s->GetSecurity() == SEC_PLAYER)
    {
        AddQueued(s);
        UpdateMaxCounters();
        DETAIL_LOG("PlayerQueue: Account id %u is in Queue Position (%u).", s->GetAccountId(), ++QueueSize);
        return;
    }

    WorldPacket packet(SMSG_AUTH_RESPONSE, 17);

    packet.WriteBit(false);                                 // has queue
    packet.WriteBit(true);                                  // has account info

    packet << uint32(0);                                    // Unknown - 4.3.2
    packet << uint8(s->Expansion());                        // 0 - normal, 1 - TBC, 2 - WotLK, 3 - CT. must be set in database manually for each account
    packet << uint32(0);                                    // BillingTimeRemaining
    packet << uint8(s->Expansion());                        // 0 - normal, 1 - TBC, 2 - WotLK, 3 - CT. Must be set in database manually for each account.
    packet << uint32(0);                                    // BillingTimeRested
    packet << uint8(0);                                     // BillingPlanFlags
    packet << uint8(AUTH_OK);

    s->SendPacket(&packet);

    s->SendAddonsInfo();

    WorldPacket pkt(SMSG_CLIENTCACHE_VERSION, 4);
    pkt << uint32(sGameConfig.getConfig(CONFIG_UINT32_CLIENTCACHE_VERSION));
    s->SendPacket(&pkt);

    s->SendTutorialsData();

    UpdateMaxCounters();

    // Updates the population
    if (pLimit > 0)
    {
        float popu = float(ActiveCount());                  // updated number of users on the server
        popu /= pLimit;
        popu *= 2;

        RealmRepository::SetPopulation(popu, realmID);

        DETAIL_LOG("Server Population (%f).", popu);
    }
}

int32 SessionRegistry::QueuedPosition(WorldSession* sess) const
{
    uint32 position = 1;

    for (Queue::const_iterator iter = m_queued.begin(); iter != m_queued.end(); ++iter, ++position)
        if ((*iter) == sess)
        {
            return position;
        }

    return 0;
}

void SessionRegistry::AddQueued(WorldSession* sess)
{
    sess->SetInQueue(true);
    m_queued.push_back(sess);

    // The 1st SMSG_AUTH_RESPONSE needs to contain other info too.
    WorldPacket packet (SMSG_AUTH_RESPONSE, 21);

    packet.WriteBit(true);                                  // has queue
    packet.WriteBit(false);                                 // unk queue-related
    packet.WriteBit(true);                                  // has account data

    packet << uint32(0);                                    // Unknown - 4.3.2
    packet << uint8(sess->Expansion());                     // 0 - normal, 1 - TBC, 2 - WotLK, 3 - CT. must be set in database manually for each account
    packet << uint32(0);                                    // BillingTimeRemaining
    packet << uint8(sess->Expansion());                     // 0 - normal, 1 - TBC, 2 - WotLK, 3 - CT. Must be set in database manually for each account.
    packet << uint32(0);                                    // BillingTimeRested
    packet << uint8(0);                                     // BillingPlanFlags
    packet << uint8(AUTH_WAIT_QUEUE);
    packet << uint32(QueuedPosition(sess));                 // position in queue

    sess->SendPacket(&packet);
}

bool SessionRegistry::RemoveQueued(WorldSession* sess)
{
    // sessions count including queued to remove (if removed_session set)
    uint32 sessions = ActiveCount();

    uint32 position = 1;
    Queue::iterator iter = m_queued.begin();

    // search to remove and count skipped positions
    bool found = false;

    for (; iter != m_queued.end(); ++iter, ++position)
    {
        if (*iter == sess)
        {
            sess->SetInQueue(false);
            iter = m_queued.erase(iter);
            found = true;                                   // removing queued session
            break;
        }
    }

    // iter point to next socked after removed or end()
    // position store position of removed socket and then new position next socket after removed

    // if session not queued then we need decrease sessions count
    if (!found && sessions)
    {
        --sessions;
    }

    // accept first in queue
    if ((!m_playerLimit || (int32)sessions < m_playerLimit) && !m_queued.empty())
    {
        WorldSession* pop_sess = m_queued.front();
        pop_sess->SetInQueue(false);
        pop_sess->SendAuthWaitQue(0);
        pop_sess->SendAddonsInfo();

        WorldPacket pkt(SMSG_CLIENTCACHE_VERSION, 4);
        pkt << uint32(sGameConfig.getConfig(CONFIG_UINT32_CLIENTCACHE_VERSION));
        pop_sess->SendPacket(&pkt);

        pop_sess->SendAccountDataTimes(GLOBAL_CACHE_MASK);
        pop_sess->SendTutorialsData();

        m_queued.pop_front();

        // update iter to point first queued socket or end() if queue is empty now
        iter = m_queued.begin();
        position = 1;
    }

    // update position from iter to end()
    // iter point to first not updated socket, position store new position
    for (; iter != m_queued.end(); ++iter, ++position)
    {
        (*iter)->SendAuthWaitQue(position);
    }

    return found;
}

void SessionRegistry::SetPlayerLimit(int32 limit, bool needUpdate)
{
    if (limit < -SEC_ADMINISTRATOR)
    {
        limit = -SEC_ADMINISTRATOR;
    }

    // lock update need
    bool db_update_need = needUpdate || (limit < 0) != (m_playerLimit < 0) || (limit < 0 && m_playerLimit < 0 && limit != m_playerLimit);

    m_playerLimit = limit;

    if (db_update_need)
        RealmRepository::SetAllowedSecurityLevel(uint32(PlayerSecurityLimit()), realmID);
}

void SessionRegistry::UpdateMaxCounters()
{
    m_maxActiveCount = std::max(m_maxActiveCount, uint32(m_sessions.size() - m_queued.size()));
    m_maxQueuedCount = std::max(m_maxQueuedCount, uint32(m_queued.size()));
}

void SessionRegistry::Broadcast(WorldPacket const& packet, AccountTypes minSec)
{
    for (SessionMap::const_iterator itr = m_sessions.begin(); itr != m_sessions.end(); ++itr)
    {
        if (WorldSession* session = itr->second)
        {
            if (session->GetSecurity() < minSec)
            {
                continue;
            }
            Player* player = session->GetPlayer();
            if (player && player->IsInWorld())
            {
                session->SendPacket(&packet);
            }
        }
    }
}

void SessionRegistry::InvalidatePlayerData(ObjectGuid guid)
{
    WorldPacket data(SMSG_INVALIDATE_PLAYER, 8);
    data << guid;
    Broadcast(data);
}

void SessionRegistry::SaveAllPlayers() const
{
    // Driven from the session list rather than the player registry: a session can exist
    // without its player being registered there yet, and those still need saving.
    for (const auto& iter : m_sessions)
    {
        if (Player* player = iter.second->GetPlayer())
        {
            if (player->IsInWorld())
            {
                player->SaveToDB();
            }
        }
    }
}

void SessionRegistry::KickAll()
{
    m_queued.clear();                                       // prevent send queue update packet and login queued sessions

    // session not removed at kick and will removed in next update tick
    for (SessionMap::const_iterator itr = m_sessions.begin(); itr != m_sessions.end(); ++itr)
    {
        itr->second->KickPlayer();
    }
}

void SessionRegistry::KickAllLess(AccountTypes sec)
{
    // session not removed at kick and will removed in next update tick
    for (SessionMap::const_iterator itr = m_sessions.begin(); itr != m_sessions.end(); ++itr)
        if (WorldSession* session = itr->second)
            if (session->GetSecurity() < sec)
            {
                session->KickPlayer();
            }
}

void SessionRegistry::Update(uint32 /*diff*/)
{
    ///- Add new sessions
    {
        std::lock_guard<std::mutex> guard(m_addQueueLock);

        std::for_each(m_addQueue.begin(), m_addQueue.end(), [&](WorldSession *session) { List(session); });

        m_addQueue.clear();
    }

    ///- Then send an update signal to remaining ones
    for (SessionMap::iterator itr = m_sessions.begin(), next; itr != m_sessions.end(); itr = next)
    {
        next = itr;
        ++next;
        ///- and remove not active sessions from the list
        WorldSession* pSession = itr->second;
        WorldSessionFilter updater(pSession);

        if (!pSession->Update(updater))
        {
            RemoveQueued(pSession);
            m_sessions.erase(itr);
            delete pSession;
        }
    }
}
