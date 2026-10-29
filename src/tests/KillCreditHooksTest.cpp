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
#include "BattleGround/BattleGroundMgr.h"
#include "Creature.h"
#include "Group.h"
#include "OutdoorPvP/OutdoorPvPMgr.h"
#include "Player.h"
#include "WorldSession.h"
#include "data/BattleGroundHooks.h"
#include "data/GroupHooks.h"
#include "data/OutdoorPvPHooks.h"

#include <string>
#include <vector>

/**
 * @file
 * @brief What a kill or a heal tells battlegrounds, outdoor PvP zones and groups: with no hooks
 * installed no player is in a battleground and nothing is told; the installed hooks are told, with
 * the objects of the call, until replaced; and the battleground and outdoor PvP managers' own hooks
 * tell nobody about a player in no battleground or a zone with no script.
 */

namespace
{
    /// A player with its update fields allocated, on a session with no socket, in no world.
    class SessionPlayer : public Player
    {
        public:
            explicit SessionPlayer(WorldSession& session, uint32 guidLow) : Player(&session)
            {
                Object::_Create(guidLow, 0, HIGHGUID_PLAYER);
            }

            ~SessionPlayer()
            {
                delete[] m_uint32Values;
                m_uint32Values = NULL;
            }
    };

    /// Records each call and answers that the victim is in a battleground.
    class RecordingBattleGroundHooks : public BattleGroundHooks
    {
        public:
            std::vector<std::string> calls;
            std::vector<void const*> objects;

            bool PlayerKilled(Player& victim, Player& killer) override
            {
                calls.push_back("player killed");
                objects.push_back(&victim);
                objects.push_back(&killer);
                return true;
            }

            void CreatureKilled(Creature& victim, Player& responsible) override
            {
                calls.push_back("creature killed");
                objects.push_back(&victim);
                objects.push_back(&responsible);
            }

            void HealingDone(Player& healer, uint32 gain) override
            {
                calls.push_back("healing done " + std::to_string(gain));
                objects.push_back(&healer);
            }
    };

    /// Records each call with its zone.
    class RecordingOutdoorPvPHooks : public OutdoorPvPHooks
    {
        public:
            std::vector<std::string> calls;
            std::vector<void const*> objects;

            void PlayerKilled(uint32 zone, Player& killer, Player& victim) override
            {
                calls.push_back("player killed in " + std::to_string(zone));
                objects.push_back(&killer);
                objects.push_back(&victim);
            }

            void CreatureDied(uint32 zone, Creature& victim) override
            {
                calls.push_back("creature died in " + std::to_string(zone));
                objects.push_back(&victim);
            }
    };

    /// Records each reward with its group, victim and tap.
    class RecordingGroupHooks : public GroupHooks
    {
        public:
            std::vector<void const*> objects;

            void RewardAtKill(Group& group, Unit& victim, Player* tap) override
            {
                objects.push_back(&group);
                objects.push_back(&victim);
                objects.push_back(tap);
            }
    };

    /// Installs `hooks` for one test and puts the old ones back.
    template<class Hooks>
    class InstalledHooks
    {
        public:
            explicit InstalledHooks(Hooks* hooks) : m_previous(Hooks::Install(hooks)) {}
            ~InstalledHooks() { Hooks::Install(m_previous); }
        private:
            Hooks* m_previous;
    };
}

TEST(KillCreditHooks_with_none_installed_no_player_is_in_a_battleground_and_nothing_is_told)
{
    InstalledHooks<BattleGroundHooks> battleGrounds(NULL);
    InstalledHooks<OutdoorPvPHooks> outdoorPvP(NULL);
    InstalledHooks<GroupHooks> groups(NULL);
    WorldSession victimSession(1, "victim", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    WorldSession killerSession(2, "killer", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    {
        SessionPlayer victim(victimSession, 1);
        SessionPlayer killer(killerSession, 2);
        Creature creature(CREATURE_SUBTYPE_GENERIC);
        Group group;

        CHECK(!BattleGroundHooks::IsInstalled());
        CHECK(!OutdoorPvPHooks::IsInstalled());
        CHECK(!GroupHooks::IsInstalled());
        CHECK(!sBattleGroundHooks.PlayerKilled(victim, killer));
        sBattleGroundHooks.CreatureKilled(creature, killer);
        sBattleGroundHooks.HealingDone(killer, 250);
        sOutdoorPvPHooks.PlayerKilled(3483, killer, victim);
        sOutdoorPvPHooks.CreatureDied(3483, creature);
        sGroupHooks.RewardAtKill(group, creature, &killer);
        sGroupHooks.RewardAtKill(group, creature, NULL);
    }
}

TEST(KillCreditHooks_the_installed_hooks_are_told_with_the_objects_of_the_call_until_replaced)
{
    RecordingBattleGroundHooks battleGroundHooks;
    RecordingOutdoorPvPHooks outdoorPvPHooks;
    RecordingGroupHooks groupHooks;
    WorldSession victimSession(1, "victim", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    WorldSession killerSession(2, "killer", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    {
        SessionPlayer victim(victimSession, 1);
        SessionPlayer killer(killerSession, 2);
        Creature creature(CREATURE_SUBTYPE_GENERIC);
        Group group;
        {
            InstalledHooks<BattleGroundHooks> battleGrounds(&battleGroundHooks);
            InstalledHooks<OutdoorPvPHooks> outdoorPvP(&outdoorPvPHooks);
            InstalledHooks<GroupHooks> groups(&groupHooks);

            CHECK(BattleGroundHooks::IsInstalled());
            CHECK(sBattleGroundHooks.PlayerKilled(victim, killer));
            sBattleGroundHooks.CreatureKilled(creature, killer);
            sBattleGroundHooks.HealingDone(killer, 250);
            sOutdoorPvPHooks.PlayerKilled(3483, killer, victim);
            sOutdoorPvPHooks.CreatureDied(3518, creature);
            sGroupHooks.RewardAtKill(group, creature, &killer);
            sGroupHooks.RewardAtKill(group, victim, NULL);
        }

        CHECK(battleGroundHooks.calls == std::vector<std::string>({"player killed", "creature killed", "healing done 250"}));
        CHECK(battleGroundHooks.objects == std::vector<void const*>({&victim, &killer, &creature, &killer, &killer}));
        CHECK(outdoorPvPHooks.calls == std::vector<std::string>({"player killed in 3483", "creature died in 3518"}));
        CHECK(outdoorPvPHooks.objects == std::vector<void const*>({&killer, &victim, &creature}));
        CHECK(groupHooks.objects == std::vector<void const*>({&group, &creature, &killer, &group, &victim, NULL}));

        CHECK(&sBattleGroundHooks != &battleGroundHooks);
        CHECK(&sOutdoorPvPHooks != &outdoorPvPHooks);
        CHECK(&sGroupHooks != &groupHooks);
    }
}

TEST(KillCreditHooks_the_managers_hooks_tell_nobody_about_a_player_in_no_battleground_or_a_zone_with_no_script)
{
    BattleGroundHooks& battleGrounds = BattleGroundMgr::GetBattleGroundHooks();
    OutdoorPvPHooks& outdoorPvP = OutdoorPvPMgr::GetOutdoorPvPHooks();
    WorldSession victimSession(1, "victim", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    WorldSession killerSession(2, "killer", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    {
        SessionPlayer victim(victimSession, 1);
        SessionPlayer killer(killerSession, 2);
        Creature creature(CREATURE_SUBTYPE_GENERIC);

        CHECK(&battleGrounds == &BattleGroundMgr::GetBattleGroundHooks());
        CHECK(!battleGrounds.PlayerKilled(victim, killer));
        battleGrounds.CreatureKilled(creature, killer);
        battleGrounds.HealingDone(killer, 250);
        outdoorPvP.PlayerKilled(0, killer, victim);
        outdoorPvP.CreatureDied(0, creature);
    }
}
