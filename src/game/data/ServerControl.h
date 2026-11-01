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

#ifndef MANGOS_H_SERVERCONTROL
#define MANGOS_H_SERVERCONTROL

#include "Platform/Define.h"
#include "SharedDefines.h"

#include <string>

class Player;

enum ShutdownMask
{
    SHUTDOWN_MASK_RESTART = 1,
    SHUTDOWN_MASK_IDLE    = 2,
};

enum ShutdownExitCode
{
    SHUTDOWN_EXIT_CODE = 0,
    ERROR_EXIT_CODE    = 1,
    RESTART_EXIT_CODE  = 2,
};

/**
 * The operator's handle on the server: the timed shutdown, the message of the day, whether objects
 * may move, the bans, the configuration and broadcast reloads, the uptime and the world database's
 * version, a game event's start and stop, and the GM harness's runs; and the process's stop request,
 * which any thread may raise. The world installs itself at
 * construction; the code below it calls the installed one through sServerControl. This base, installed
 * when nothing else is, runs no shutdown, bans nobody and reloads nothing.
 */
class ServerControl
{
    public:
        virtual ~ServerControl();

        /// Asks the process to stop at the end of the current tick, exiting with `exitCode`.
        static void StopNow(uint8 exitCode);
        /// Whether the process was asked to stop.
        static bool IsStopped();
        /// The code the process exits with: the stop's, or the one a pending shutdown will use.
        static uint8 ExitCode();
        /// Sets the code a pending shutdown will exit with, without stopping.
        static void SetExitCode(uint8 exitCode);

        /// Whether a timed shutdown is counting down.
        virtual bool IsShuttingDown() const;
        /// Shuts the server down in `delay` seconds with `options` (ShutdownMask) and `exitCode`.
        virtual void Shutdown(uint32 delay, uint32 options, uint8 exitCode);
        /// Cancels a timed shutdown that is counting down.
        virtual void CancelShutdown();
        /// Tells `player`, or every player when NULL, the time left before the shutdown; `show` sends it
        /// even when the time left is not one the countdown announces.
        virtual void ShutdownMessage(bool show = false, Player* player = NULL);

        virtual char const* Motd() const;
        virtual void SetMotd(std::string const& motd);

        /// Whether objects may move.
        virtual bool MovementAllowed() const;
        virtual void SetMovementAllowed(bool allow);

        /// Bans the account, the character's account or the IP address `nameOrIP` for `durationSecs`
        /// (for good when it is 0).
        virtual BanReturn Ban(BanMode mode, std::string nameOrIP, uint32 durationSecs, std::string reason, std::string const& author);
        /// Lifts the ban on `nameOrIP`; false when there is none to lift.
        virtual bool RemoveBan(BanMode mode, std::string nameOrIP);

        /// Reads the configuration file again.
        virtual void ReloadConfig();
        /// Loads the automatic broadcast messages again.
        virtual void LoadBroadcastStrings();

        /// Seconds since the server started.
        virtual uint32 Uptime() const;
        /// The world database's version.
        virtual char const* DbVersion() const;

        /// Starts game event `eventId` now.
        virtual void StartEvent(uint16 eventId);
        /// Stops game event `eventId` now.
        virtual void StopEvent(uint16 eventId);

        /// Starts the harness scenarios `what` names with `seedBase`; false when they cannot start.
        virtual bool StartHarness(std::string const& what, uint32 seedBase);
        /// The harness's status line.
        virtual std::string HarnessStatus() const;
        /// The seed base the harness uses when the operator gives none.
        virtual uint32 HarnessSeedBase() const;

        /// The installed control, or the base when none is installed.
        static ServerControl& Installed();
        /// Installs `control` and answers the one it replaces (NULL for the base). NULL installs the base.
        static ServerControl* Install(ServerControl* control);
        /// Whether a control is installed.
        static bool IsInstalled();
};

#define sServerControl ServerControl::Installed()

#endif
