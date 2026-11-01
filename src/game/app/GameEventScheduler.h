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

#ifndef MANGOS_H_GAMEEVENTSCHEDULER
#define MANGOS_H_GAMEEVENTSCHEDULER

#include "Platform/Define.h"
#include "Policies/Singleton.h"
#include "GameEventMgr.h"

/// The game event system's runtime, run by the world: it starts and stops the events GameEventMgr
/// loaded on their windows, and applies or removes what each one changes in the world.
class GameEventScheduler
{
    public:
        GameEventScheduler() : m_IsGameEventsInit(false) {}

        uint32 Initialize();                                // return the next event delay in ms
        uint32 Update(GameEventMgr::ActiveEvents const* activeAtShutdown = NULL);
        void StartEvent(uint16 event_id, bool overwrite = false, bool resume = false);
        void StopEvent(uint16 event_id, bool overwrite = false);

    private:
        void ApplyNewEvent(uint16 event_id, bool resume);
        void UnApplyEvent(uint16 event_id);
        void GameEventSpawn(int16 event_id);
        void GameEventUnspawn(int16 event_id);
        void UpdateCreatureData(int16 event_id, bool activate);
        void UpdateEventQuests(uint16 event_id, bool activate);
        void UpdateWorldStates(uint16 event_id, bool activate);
        void SendEventMails(int16 event_id);

        bool m_IsGameEventsInit;
};

#define sGameEventScheduler MaNGOS::Singleton<GameEventScheduler>::Instance()

#endif
