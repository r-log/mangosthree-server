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

#include "CliService.h"

#include "Common/ServerDefines.h"
#include "Console/ConsoleUI.h"
#include "Log.h"
#include "Util.h"
#include "World.h"

#include <chrono>
#include <cstdio>
#include <string>
#include <thread>

#ifdef _WIN32
#  include <io.h>
#  include <windows.h>
#else
#  include <sys/select.h>
#  include <unistd.h>
#endif

namespace
{
    /**
     * @brief Draw the prompt.
     *
     * Routed through the console writer rather than printf: stdout has a single
     * owner, so the prompt cannot overtake queued log lines or tear against a
     * progress-bar redraw -- which is exactly what happens when a command like
     * .reload finishes and its output is still draining.
     */
    void Prompt(void* /*callbackArg*/ = nullptr, bool /*success*/ = true)
    {
        if (MaNGOS::Console::ConsoleUI::Instance().Active())
        {
            return;                             // the console draws its own input row
        }

        sLog.ConsoleEmitRaw("mangos>");
    }

    /**
     * @brief True when stdin is a terminal, i.e. somebody can type at it.
     *
     * End of input means two unrelated things either side of this, and nothing
     * else distinguishes them. See the EOF branch of RunLineBased().
     */
    bool StdinIsTerminal()
    {
#ifdef _WIN32
        return _isatty(_fileno(stdin)) != 0;
#else
        return isatty(STDIN_FILENO) != 0;
#endif
    }

#ifndef _WIN32
    /// True when a line is waiting on stdin. Never blocks.
    bool LinePending()
    {
        timeval tv;
        tv.tv_sec  = 0;
        tv.tv_usec = 0;

        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);

        select(STDIN_FILENO + 1, &fds, nullptr, nullptr, &tv);
        return FD_ISSET(STDIN_FILENO, &fds) != 0;
    }
#endif
}

CliService::CliService(bool beep)
    : m_beep(beep),
      m_stop(false)
{
}

CliService::~CliService()
{
    RequestStop();
    Join();
}

void CliService::Start()
{
    m_thread = std::thread([this] { Run(); });
}

void CliService::RequestStop()
{
    m_stop.store(true, std::memory_order_release);

#ifdef _WIN32
    // On Windows the reader is parked inside fgets() and cannot be polled out of
    // it, so synthesise a Return keypress to make that call return. POSIX does
    // not need this: the loop below polls with select() and never blocks.
    INPUT_RECORD rec{};
    rec.EventType                        = KEY_EVENT;
    rec.Event.KeyEvent.bKeyDown          = TRUE;
    rec.Event.KeyEvent.dwControlKeyState = 0;
    rec.Event.KeyEvent.uChar.AsciiChar   = '\r';
    rec.Event.KeyEvent.wVirtualKeyCode   = VK_RETURN;
    rec.Event.KeyEvent.wRepeatCount      = 1;
    rec.Event.KeyEvent.wVirtualScanCode  = 0x1c;

    DWORD written = 0;
    WriteConsoleInput(GetStdHandle(STD_INPUT_HANDLE), &rec, 1, &written);
#endif
}

void CliService::Join()
{
    if (m_thread.joinable())
    {
        m_thread.join();
    }
}

void CliService::Run()
{
    // Let start-up finish printing before the prompt lands in the middle of it.
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // The bell has to bypass the log pipeline entirely. With the full-screen UI
    // up, ConsoleLogWriter::Emit() folds a raw record into the UI's scrollback
    // as text, so a BEL routed through sLog is displayed rather than sounded --
    // which is why gating this on !ConsoleUI::Active() silenced it outright.
    if (m_beep)
    {
#ifdef _WIN32
        // The system "default" sound, i.e. what the console bell used to raise.
        MessageBeep(MB_OK);
#else
        // Straight to the terminal: non-printing, so it cannot disturb a frame.
        std::fputs("\a", stdout);
        std::fflush(stdout);
#endif
    }

    Prompt();

    if (MaNGOS::Console::ConsoleUI::Instance().Active())
    {
        RunManaged();
    }
    else
    {
        RunLineBased();
    }
}

void CliService::RunManaged()
{
    std::string line;

    while (!m_stop.load(std::memory_order_acquire) && !World::IsStopped())
    {
        // Already UTF-8: the console decodes keystrokes to code points itself, so
        // the consoleToUtf8() step of the line-based path would double-convert.
        if (!MaNGOS::Console::ConsoleUI::Instance().PollInput(line))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(15));
            continue;
        }

        sWorld.QueueCliCommand(
            new CliCommandHolder(nullptr, line.c_str(),
                                 &utf8print, &Prompt));
    }
}

void CliService::RunLineBased()
{
    char buffer[256];

    while (!m_stop.load(std::memory_order_acquire) && !World::IsStopped())
    {
#ifndef _WIN32
        // Poll rather than block, so shutdown does not have to wait for the
        // operator to press Return. The sleep also caps the console at roughly
        // ten commands a second.
        while (!LinePending())
        {
            if (m_stop.load(std::memory_order_acquire) || World::IsStopped())
            {
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
#endif

        char* line = fgets(buffer, sizeof(buffer), stdin);

        if (line == nullptr)
        {
            if (!feof(stdin))
            {
                continue;               // a failed or interrupted read, not EOF
            }

            // End of input. At a terminal that is the operator pressing Ctrl-D,
            // which has always meant "shut the server down". Anywhere else it
            // means only that no more commands are coming, and the world must
            // outlive its console.
            //
            // A service manager hands the daemon /dev/null, which reports EOF on
            // the very first read: stopping the world here ended it about a
            // second after start-up finished. The exit was clean -- status 0, a
            // full shutdown sequence -- so Restart=on-failure never fired and
            // the journal held nothing that read as an error. It presented as
            // the world port being closed, which looks like a firewall fault
            // from outside. Windows already avoided this by refusing to start a
            // console in service mode (Master.cpp); nothing covered POSIX.
            if (StdinIsTerminal())
            {
                World::StopNow(SHUTDOWN_EXIT_CODE);
            }
            else
            {
                sLog.outString("Console: end of input on a stdin that is not a "
                               "terminal. Console stopped; world still running.");
            }
            return;
        }

        // Trim the line ending in place.
        for (char* p = line; *p; ++p)
        {
            if (*p == '\r' || *p == '\n')
            {
                *p = '\0';
                break;
            }
        }

        if (!*line)
        {
            Prompt();
            continue;
        }

        std::string command;
        if (!consoleToUtf8(line, command))
        {
            Prompt();
            continue;
        }

        // Queued, never executed here: game state belongs to the world thread.
        sWorld.QueueCliCommand(
            new CliCommandHolder(nullptr, command.c_str(),
                                 &utf8print, &Prompt));
    }
}
