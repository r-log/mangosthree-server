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

#ifndef MANGOS_H_DATABASELIFECYCLE
#define MANGOS_H_DATABASELIFECYCLE

#include "Platform/Define.h"

/// The three databases' delay threads and result queues, as the world loop and the server's start-up
/// and shutdown drive them, one database at a time.
namespace DatabaseLifecycle
{
    /// Each runs the callbacks of one database's answered asynchronous reads, on the calling thread.
    void ProcessCharacterResults();

    /// The world database's.
    void ProcessWorldResults();

    /// The login database's.
    void ProcessLoginResults();

    /// Each stops one database's delay thread, waits for it and runs what it still had queued.
    void HaltLoginDelayThread();

    /// The character database's.
    void HaltCharacterDelayThread();

    /// The world database's.
    void HaltWorldDelayThread();

    /// Each lets one database queue asynchronous transactions, once the world is loaded.
    void AllowCharacterAsyncTransactions();

    /// The world database's.
    void AllowWorldAsyncTransactions();

    /// The login database's.
    void AllowLoginAsyncTransactions();

    /// Each stops one database from queueing new asynchronous reads, as the shutdown begins.
    void BeginLoginShutdown();

    /// The world database's.
    void BeginWorldShutdown();

    /// The character database's.
    void BeginCharacterShutdown();

    /// Registers the world thread with the world database's client library.
    void StartWorldThread();

    /// Unregisters the world thread from the world database's client library.
    void EndWorldThread();
}

#endif
