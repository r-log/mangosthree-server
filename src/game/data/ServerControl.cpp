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

#include "data/ServerControl.h"

#include <atomic>
#include <cstddef>

namespace
{
    std::atomic<ServerControl*> s_installed(NULL);
    std::atomic<bool> s_stopped(false);
    std::atomic<uint8> s_exitCode{uint8(SHUTDOWN_EXIT_CODE)};

    ServerControl& BaseControl()
    {
        static ServerControl control;
        return control;
    }
}

ServerControl::~ServerControl()
{
}

void ServerControl::StopNow(uint8 exitCode)
{
    s_exitCode.store(exitCode, std::memory_order_release);
    s_stopped.store(true, std::memory_order_release);
}

bool ServerControl::IsStopped()
{
    return s_stopped.load(std::memory_order_acquire);
}

uint8 ServerControl::ExitCode()
{
    return s_exitCode.load(std::memory_order_acquire);
}

void ServerControl::SetExitCode(uint8 exitCode)
{
    s_exitCode.store(exitCode, std::memory_order_release);
}

bool ServerControl::IsShuttingDown() const
{
    return false;
}

void ServerControl::Shutdown(uint32 /*delay*/, uint32 /*options*/, uint8 /*exitCode*/)
{
}

void ServerControl::CancelShutdown()
{
}

void ServerControl::ShutdownMessage(bool /*show*/, Player* /*player*/)
{
}

char const* ServerControl::Motd() const
{
    return "";
}

void ServerControl::SetMotd(std::string const& /*motd*/)
{
}

bool ServerControl::MovementAllowed() const
{
    return true;
}

void ServerControl::SetMovementAllowed(bool /*allow*/)
{
}

BanReturn ServerControl::Ban(BanMode /*mode*/, std::string /*nameOrIP*/, uint32 /*durationSecs*/, std::string /*reason*/, std::string const& /*author*/)
{
    return BAN_NOTFOUND;
}

bool ServerControl::RemoveBan(BanMode /*mode*/, std::string /*nameOrIP*/)
{
    return false;
}

void ServerControl::ReloadConfig()
{
}

void ServerControl::LoadBroadcastStrings()
{
}

uint32 ServerControl::Uptime() const
{
    return 0;
}

char const* ServerControl::DbVersion() const
{
    return "";
}

ServerControl& ServerControl::Installed()
{
    ServerControl* control = s_installed.load(std::memory_order_acquire);
    return control ? *control : BaseControl();
}

ServerControl* ServerControl::Install(ServerControl* control)
{
    return s_installed.exchange(control, std::memory_order_acq_rel);
}

bool ServerControl::IsInstalled()
{
    return s_installed.load(std::memory_order_acquire) != NULL;
}
