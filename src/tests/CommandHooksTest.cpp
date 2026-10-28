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

#include "Auth/BigNumber.h"
#include "Chat.h"
#include "data/CommandHooks.h"
#include "Player.h"
#include "WorldSession.h"

/**
 * @file
 * @brief What the core asks of the GM command system: the base answers with nothing installed, the
 * installed hooks answer until replaced, and the command system's own hooks answer from the
 * command table and the player's access level.
 */

namespace
{
    /// A player with its update fields allocated, on a session with no socket, in no world.
    class SessionPlayer : public Player
    {
        public:
            explicit SessionPlayer(WorldSession& session) : Player(&session)
            {
                Object::_Create(1, 0, HIGHGUID_PLAYER);
            }

            ~SessionPlayer()
            {
                delete[] m_uint32Values;
                m_uint32Values = NULL;
            }
    };

    /// Counts the questions and answers yes to the first, no to the second.
    class CountingHooks : public CommandHooks
    {
        public:
            int asked = 0;

            bool MayUseCommand(Player& /*player*/, char const* /*command*/) override
            {
                ++asked;
                return true;
            }

            bool ParseChatCommand(Player& /*player*/, char const* /*text*/) override
            {
                ++asked;
                return false;
            }
    };

    /// Installs `hooks` for one test and puts the old ones back.
    class InstalledHooks
    {
        public:
            explicit InstalledHooks(CommandHooks* hooks) : m_previous(CommandHooks::Install(hooks)) {}
            ~InstalledHooks() { CommandHooks::Install(m_previous); }
        private:
            CommandHooks* m_previous;
    };
}

TEST(CommandHooks_with_none_installed_no_command_may_be_used_and_a_command_line_is_dropped)
{
    InstalledHooks none(NULL);
    WorldSession session(1, "hooks", nullptr, nullptr, SEC_ADMINISTRATOR, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    SessionPlayer player(session);

    CHECK(!CommandHooks::IsInstalled());
    CHECK(!sCommandHooks.MayUseCommand(player, "auction"));
    CHECK(sCommandHooks.ParseChatCommand(player, ".gm on"));
    CHECK(sCommandHooks.ParseChatCommand(player, "!help"));
    CHECK(!sCommandHooks.ParseChatCommand(player, "hello"));
}

TEST(CommandHooks_the_installed_hooks_answer_until_replaced)
{
    CountingHooks hooks;
    WorldSession session(1, "hooks", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    SessionPlayer player(session);
    {
        InstalledHooks installed(&hooks);
        CHECK(CommandHooks::IsInstalled());
        CHECK(sCommandHooks.MayUseCommand(player, "auction"));
        CHECK(!sCommandHooks.ParseChatCommand(player, ".gm on"));
        CHECK_EQ(hooks.asked, 2);
    }
    CHECK(&sCommandHooks != &hooks);
}

TEST(CommandHooks_the_command_table_lets_an_administrator_use_the_auction_command_and_not_a_player)
{
    WorldSession playerSession(1, "player", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    WorldSession adminSession(2, "admin", nullptr, nullptr, SEC_ADMINISTRATOR, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    SessionPlayer player(playerSession);
    SessionPlayer admin(adminSession);
    CommandHooks& hooks = ChatHandler::GetCommandHooks();

    CHECK(!hooks.MayUseCommand(player, "auction"));
    CHECK(hooks.MayUseCommand(admin, "auction"));
}
