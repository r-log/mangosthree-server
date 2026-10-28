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

#include "TestHarness.h"

#include <string>
#include "data/GameConfig.h"
#include "session/handlers/social/ChatLinks.h"

/**
 * @file
 * @brief The chat shift-link check at the severities that read no game data: 1 accepts only the
 * |c, |H, |h, |r and || commands, 2 also wants them in the order |c |H |h |h |r. Any severity
 * refuses a message over 255 bytes.
 */

namespace
{
    /// Sets the severity for one test and puts the old one back.
    class Severity
    {
        public:
            explicit Severity(uint32 value) : m_old(sGameConfig.getConfig(CONFIG_UINT32_CHAT_STRICT_LINK_CHECKING_SEVERITY))
            {
                sGameConfig.setConfig(CONFIG_UINT32_CHAT_STRICT_LINK_CHECKING_SEVERITY, value);
            }
            ~Severity() { sGameConfig.setConfig(CONFIG_UINT32_CHAT_STRICT_LINK_CHECKING_SEVERITY, m_old); }
        private:
            uint32 m_old;
    };

    const char* const kItemLink = "|cff9d9d9d|Hitem:812:0:0:0:0:0:0:0:70|h[Glowing Brightwood Staff]|h|r";
}

TEST(ChatLinks_plain_text_is_valid)
{
    Severity severity(1);
    CHECK(ChatLinks::IsValid("hello there"));
    CHECK(ChatLinks::IsValid("an escaped || pipe"));
}

TEST(ChatLinks_a_message_over_255_bytes_is_refused)
{
    Severity severity(1);
    CHECK(ChatLinks::IsValid(std::string(255, 'a').c_str()));
    CHECK(!ChatLinks::IsValid(std::string(256, 'a').c_str()));
}

TEST(ChatLinks_severity_one_refuses_an_unknown_command_only)
{
    Severity severity(1);
    CHECK(ChatLinks::IsValid(kItemLink));
    CHECK(ChatLinks::IsValid("|r|h|H|c out of order"));
    CHECK(!ChatLinks::IsValid("a |x command"));
}

TEST(ChatLinks_severity_two_also_wants_the_commands_in_order)
{
    Severity severity(2);
    CHECK(ChatLinks::IsValid(kItemLink));
    CHECK(ChatLinks::IsValid(std::string(std::string(kItemLink) + " and " + kItemLink).c_str()));
    CHECK(!ChatLinks::IsValid("|r|h|H|c out of order"));
    CHECK(!ChatLinks::IsValid("a |x command"));
}
