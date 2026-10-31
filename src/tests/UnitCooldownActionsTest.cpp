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

/// The five cooldown actions of a unit: the proc handlers end a spell's or a category's cooldown,
/// casts, auras and AddGameObject and RemoveGameObject start one, and ClearInCombat starts the
/// cooldown of the potion used in combat.
///
/// Unit defines the five once, public, each with the default arguments its callers leave out, and
/// Player declares none of them. A unit acts through the cooldown callbacks a player points it at;
/// a unit that is not a player has none and does nothing. A bare Creature probe (no map, no AI, no
/// auras) makes the cooldown manager Unit holds public with a using-declaration and is asked the
/// five; its manager stays as it was. A player seated on a socketless session, with the session's
/// callbacks installed, sends the one-spell clear with the bytes Player::SendClearCooldown sends,
/// and forgets its last potion once combat is over. The static_asserts pin Unit's declarations,
/// that a call compiles through a Unit and through a Player, and that Player declares none.

#include "TestHarness.h"
#include "Auth/BigNumber.h"
#include "Creature.h"
#include "DBCStores.h"
#include "Player.h"
#include "Unit.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "session/packets/PlayerPacketSinks.h"
#include "spells/SpellCooldownMgr.h"

#include <ctime>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace
{
    /// A bare Creature; the cooldown manager Unit holds is public here.
    class CooledUnit : public Creature
    {
        public:
            using Unit::m_spellCooldownMgr;

            CooledUnit() : Creature(CREATURE_SUBTYPE_GENERIC) { }
    };

    /// A player with its update fields allocated, seated on its session, in no world.
    class CooledPlayer : public Player
    {
        public:
            explicit CooledPlayer(WorldSession& session) : Player(&session)
            {
                Object::_Create(1, 0, HIGHGUID_PLAYER);
                session.SetPlayer(this);
            }

            ~CooledPlayer()
            {
                WorldSession::Of(*this)->SetPlayer(nullptr);
                delete[] m_uint32Values;
                m_uint32Values = NULL;
            }
    };

    std::string Hex(WorldPacket const& packet)
    {
        static const char* digits = "0123456789abcdef";
        std::string text;
        uint16 opcode = packet.GetOpcode();
        for (int shift = 12; shift >= 0; shift -= 4)
        {
            text += digits[(opcode >> shift) & 0x0F];
        }
        text += ":";
        text += testing::BytesToHex(packet.contents(), packet.size());
        return text;
    }

    void KeepPacket(void* context, WorldPacket const& packet)
    {
        static_cast<std::vector<std::string>*>(context)->push_back(Hex(packet));
    }

    template<class T>
    auto CallsAddCooldowns(int)
        -> decltype(std::declval<T&>().AddSpellAndCategoryCooldowns(static_cast<SpellEntry const*>(NULL), uint32(0)),
                    std::true_type());
    template<class T>
    std::false_type CallsAddCooldowns(...);

    template<class T>
    auto CallsCooldownEvent(int)
        -> decltype(std::declval<T&>().SendCooldownEvent(static_cast<SpellEntry const*>(NULL)), std::true_type());
    template<class T>
    std::false_type CallsCooldownEvent(...);

    template<class T>
    auto CallsRemoveCooldown(int) -> decltype(std::declval<T&>().RemoveSpellCooldown(uint32(0)), std::true_type());
    template<class T>
    std::false_type CallsRemoveCooldown(...);

    template<class T>
    auto CallsRemoveCategory(int) -> decltype(std::declval<T&>().RemoveSpellCategoryCooldown(uint32(0)), std::true_type());
    template<class T>
    std::false_type CallsRemoveCategory(...);

    template<class T>
    auto CallsPotionCooldown(int) -> decltype(std::declval<T&>().UpdatePotionCooldown(), std::true_type());
    template<class T>
    std::false_type CallsPotionCooldown(...);

    const uint32 kSpell = 94801;
    const uint32 kCategory = 35;
    const uint32 kPotion = 33447;
}

static_assert(std::is_same<decltype(&Unit::AddSpellAndCategoryCooldowns),
                           void (Unit::*)(SpellEntry const*, uint32, Spell*, bool)>::value,
              "Unit declares the start of a spell's and its category's cooldown");
static_assert(std::is_same<decltype(&Unit::SendCooldownEvent), void (Unit::*)(SpellEntry const*, uint32, Spell*)>::value,
              "Unit declares the cooldown event");
static_assert(std::is_same<decltype(&Unit::RemoveSpellCooldown), void (Unit::*)(uint32, bool)>::value,
              "Unit declares the end of a spell's cooldown");
static_assert(std::is_same<decltype(&Unit::RemoveSpellCategoryCooldown), void (Unit::*)(uint32, bool)>::value,
              "Unit declares the end of a category's cooldown");
static_assert(std::is_same<decltype(&Unit::UpdatePotionCooldown), void (Unit::*)(Spell*)>::value,
              "Unit declares the potion cooldown");

static_assert(std::is_same<decltype(&Player::AddSpellAndCategoryCooldowns),
                           void (Unit::*)(SpellEntry const*, uint32, Spell*, bool)>::value &&
              std::is_same<decltype(&Player::SendCooldownEvent), void (Unit::*)(SpellEntry const*, uint32, Spell*)>::value &&
              std::is_same<decltype(&Player::RemoveSpellCooldown), void (Unit::*)(uint32, bool)>::value &&
              std::is_same<decltype(&Player::RemoveSpellCategoryCooldown), void (Unit::*)(uint32, bool)>::value &&
              std::is_same<decltype(&Player::UpdatePotionCooldown), void (Unit::*)(Spell*)>::value,
              "Player declares none of the five: each member pointer is Unit's");

static_assert(decltype(CallsAddCooldowns<Unit>(0))::value && decltype(CallsCooldownEvent<Unit>(0))::value
              && decltype(CallsRemoveCooldown<Unit>(0))::value && decltype(CallsRemoveCategory<Unit>(0))::value
              && decltype(CallsPotionCooldown<Unit>(0))::value,
              "the five are public: a call through a Unit compiles, with Unit's default arguments");
static_assert(decltype(CallsAddCooldowns<Player>(0))::value && decltype(CallsCooldownEvent<Player>(0))::value
              && decltype(CallsRemoveCooldown<Player>(0))::value && decltype(CallsRemoveCategory<Player>(0))::value
              && decltype(CallsPotionCooldown<Player>(0))::value,
              "a call through a Player compiles");

TEST(UnitCooldownActions_ACreatureStartsNoCooldown)
{
    CooledUnit creature;
    Unit& unit = creature;

    unit.AddSpellAndCategoryCooldowns(NULL, 0);
    unit.AddSpellAndCategoryCooldowns(NULL, 0, NULL, true);
    unit.SendCooldownEvent(NULL);
    unit.SendCooldownEvent(NULL, 0, NULL);
    unit.UpdatePotionCooldown();
    unit.UpdatePotionCooldown(NULL);

    CHECK(creature.m_spellCooldownMgr.GetSpellCooldownMap().empty());
    CHECK(!creature.HasSpellCooldown(kSpell));
}

TEST(UnitCooldownActions_ACreatureEndsNoCooldown)
{
    CooledUnit creature;
    Unit& unit = creature;
    time_t const now = time(NULL);
    time_t const end = now + 3600;

    creature.m_spellCooldownMgr.AddSpellCooldown(kSpell, 0, end);
    creature._AddCreatureSpellCooldown(kSpell, end);
    // The category holds the stored spell, so a removal that reached the manager would end it.
    sSpellCategoryStore[kCategory].insert(kSpell);

    unit.RemoveSpellCooldown(kSpell);
    unit.RemoveSpellCooldown(kSpell, true);
    unit.RemoveSpellCategoryCooldown(kCategory);
    unit.RemoveSpellCategoryCooldown(kCategory, true);

    CHECK_EQ(creature.m_spellCooldownMgr.GetSpellCooldownMap().size(), size_t(1));
    CHECK(creature.m_spellCooldownMgr.HasSpellCooldown(kSpell, now));
    CHECK(creature.HasSpellCooldown(kSpell));

    sSpellCategoryStore.erase(kCategory);
}

TEST(UnitCooldownActions_APlayersOneSpellClearSendsTheBytesOfItsClearCooldown)
{
    std::vector<std::string> sent;
    WorldSession session(1, "cooldowns", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    session.SetSocketlessSink(&KeepPacket, &sent);
    CooledPlayer player(session);
    InstallPlayerPacketSinks(player);

    player.SendClearCooldown(kSpell, &player);
    player.GetSpellCooldownMgr().AddSpellCooldown(kSpell, 0, time(NULL) + 3600);
    player.RemoveSpellCooldown(kSpell, true);
    player.RemoveSpellCooldown(kSpell);

    REQUIRE(sent.size() == 2u);
    CHECK_STR(sent[1], sent[0]);
    CHECK(!player.HasSpellCooldown(kSpell));
}

TEST(UnitCooldownActions_APlayerForgetsItsLastPotionOnceCombatIsOver)
{
    std::vector<std::string> sent;
    WorldSession session(1, "potion", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    session.SetSocketlessSink(&KeepPacket, &sent);
    CooledPlayer player(session);
    InstallPlayerPacketSinks(player);

    player.UpdatePotionCooldown();
    CHECK_EQ(player.GetLastPotionId(), uint32(0));

    player.SetLastPotionId(kPotion);
    CHECK_EQ(player.GetLastPotionId(), kPotion);
    player.UpdatePotionCooldown();

    CHECK_EQ(player.GetLastPotionId(), uint32(0));
    CHECK(sent.empty());
}
