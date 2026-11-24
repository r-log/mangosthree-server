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

/// Creature threat and the threat-reduction vectors.
///
/// The static_asserts below hold SelectHostileTarget, TauntApply and TauntFadeOut on Creature and off Unit, and a
/// bare Creature (its AI null) selects no hostile target. The cases after them hold a hostile reference's threat
/// reduction: an ask holds off the threat the reference has earned, and the threat earned under it, never below zero;
/// withdrawing it shows all of it again; asks add up and are withdrawn one at a time; a percent change and the taunt
/// temporary under an ask move the threat earned; the unit's every list is asked and a list made later is not; and a
/// total-threat aura with a negative amount withdraws its ask when it ends although its caster is gone by then.

#include "TestHarness.h"
#include "Auth/BigNumber.h"
#include "Creature.h"
#include "DBCStores.h"
#include "Player.h"
#include "PlayerRegistry.h"
#include "Unit.h"
#include "WorldSession.h"
#include "combat/HostileRefManager.h"
#include "combat/ThreatManager.h"
#include "data/ServerSpellStore.h"
#include "spells/auras/SpellAuras.h"
#include <type_traits>

namespace
{
    // Each trait is true only when &T::<name> names a member of T itself or of a base of
    // T. After the move, Creature declares all three and Unit declares none, so every pair
    // below holds; before the move, Unit declared all three and Creature inherited them, so
    // the three !Has...<Unit> asserts all fail.
    template <typename T, typename = void>
    struct HasSelectHostileTarget : std::false_type {};
    template <typename T>
    struct HasSelectHostileTarget<T, std::void_t<decltype(&T::SelectHostileTarget)>> : std::true_type {};

    template <typename T, typename = void>
    struct HasTauntApply : std::false_type {};
    template <typename T>
    struct HasTauntApply<T, std::void_t<decltype(&T::TauntApply)>> : std::true_type {};

    template <typename T, typename = void>
    struct HasTauntFadeOut : std::false_type {};
    template <typename T>
    struct HasTauntFadeOut<T, std::void_t<decltype(&T::TauntFadeOut)>> : std::true_type {};

    static_assert(!HasSelectHostileTarget<Unit>::value, "SelectHostileTarget must not be declared on Unit");
    static_assert(HasSelectHostileTarget<Creature>::value, "SelectHostileTarget must be declared on Creature");

    static_assert(!HasTauntApply<Unit>::value, "TauntApply must not be declared on Unit");
    static_assert(HasTauntApply<Creature>::value, "TauntApply must be declared on Creature");

    static_assert(!HasTauntFadeOut<Unit>::value, "TauntFadeOut must not be declared on Unit");
    static_assert(HasTauntFadeOut<Creature>::value, "TauntFadeOut must be declared on Creature");

    // The trait itself has to be right or the asserts above are vacuous. These two pin it
    // against a member that did not move and one that never existed.
    template <typename T, typename = void>
    struct HasFixateTarget : std::false_type {};
    template <typename T>
    struct HasFixateTarget<T, std::void_t<decltype(&T::FixateTarget)>> : std::true_type {};

    template <typename T, typename = void>
    struct HasNoSuchMethod : std::false_type {};
    template <typename T>
    struct HasNoSuchMethod<T, std::void_t<decltype(&T::ThisMemberDoesNotExist)>> : std::true_type {};

    static_assert(HasFixateTarget<Unit>::value, "FixateTarget stays on Unit: its callers hold a Unit*");
    static_assert(HasFixateTarget<Creature>::value, "Creature still inherits FixateTarget from Unit");
    static_assert(!HasNoSuchMethod<Unit>::value, "the detection trait must report a missing member as absent");
    static_assert(!HasNoSuchMethod<Creature>::value, "the detection trait must report a missing member as absent");
}

TEST(CreatureThreat_BareCreatureSelectsNoHostileTarget)
{
    // Supplementary runtime check: a bare Creature (no map, no AI, empty threat list) is
    // alive (Unit::Unit sets m_deathState = ALIVE) and so reaches the AI test, where i_AI
    // is NULL (Creature::Creature) and the method returns false without touching a map.
    Creature creature(CREATURE_SUBTYPE_GENERIC);
    CHECK(!creature.SelectHostileTarget());
}

/// Threat held off a hostile reference (HostileReference::AddThreatReduction) and shown again, on bare creatures
/// (update fields allocated; no map, template or AI): one creature is the unit held, each other one lists it with
/// ThreatManager::addThreat. The aura case holds a bare player (update fields allocated, a session with no socket, no
/// map) and its caster is another one, found by its guid while it is flagged in world and registered. The unit held
/// is declared before the creatures that list it, so they let go of it first.
namespace
{
    class ThreatTestCreature : public Creature
    {
        public:
            ThreatTestCreature() : Creature(CREATURE_SUBTYPE_GENERIC)
            {
                _InitValues();
            }

            ~ThreatTestCreature()
            {
                delete[] m_uint32Values;
                m_uint32Values = NULL;
            }
    };

    /// A player of guid `guidLow`; once it arrives it is flagged in world and registered, so a guid lookup finds it,
    /// until it leaves.
    class ThreatTestPlayer : public Player
    {
        public:
            ThreatTestPlayer(WorldSession& session, uint32 guidLow) : Player(&session), m_arrived(false)
            {
                Object::_Create(guidLow, 0, HIGHGUID_PLAYER);
            }

            ~ThreatTestPlayer()
            {
                Leave();
                delete[] m_uint32Values;
                m_uint32Values = NULL;
            }

            void Arrive()
            {
                Object::AddToWorld();
                sPlayerRegistry.Add(this);
                m_arrived = true;
            }

            void Leave()
            {
                if (m_arrived)
                {
                    sPlayerRegistry.Remove(this);
                    Object::RemoveFromWorld();
                    m_arrived = false;
                }
            }

        private:
            bool m_arrived;
    };

    const float kAsk = 90000000.0f;     // Mirror Image's, Fade's and Shadowmeld's aura 103 amount, as held off
    const uint32 kReductionSpell = 199124;      // a test spell id the client's data does not hold
    const int32 kReductionPoints = -9000000;    // Stasis Strike's aura 103 amount (88178, effect 1)

    /// A spell of one effect, aura 103 (SPELL_AURA_MOD_TOTAL_THREAT) at kReductionPoints on an enemy, put in the spell
    /// store once for the rest of the run; no other test reads it.
    SpellEntry const* StoreReductionSpell()
    {
        static ServerSpell spell;
        static bool stored = false;
        if (!stored)
        {
            stored = true;
            spell.Entry().ID = kReductionSpell;
            spell.effect.SpellID = kReductionSpell;
            spell.effect.EffectIndex = EFFECT_INDEX_0;
            spell.effect.Effect = SPELL_EFFECT_APPLY_AURA;
            spell.effect.EffectAura = SPELL_AURA_MOD_TOTAL_THREAT;
            spell.effect.EffectBasePoints = kReductionPoints;
            spell.effect.ImplicitTarget_0 = TARGET_CHAIN_DAMAGE;
            sSpellStore.SetEntry(kReductionSpell, &spell.Entry());
            AddSpellEffectEntry(&spell.effect);
        }
        return &spell.Entry();
    }

    float Shown(ThreatTestCreature& lister, Unit& held)
    {
        return lister.GetThreatManager().getThreat(&held);
    }
}

TEST(CreatureThreat_AnAskHoldsTheThreatEarnedUnderItAndShowsItWhole)
{
    ThreatTestCreature mage;
    ThreatTestCreature camel;
    camel.GetThreatManager().addThreat(&mage, 5000.0f);
    REQUIRE(Shown(camel, mage) == 5000.0f);

    mage.GetHostileRefManager().AddThreatReduction(kAsk);
    CHECK_EQ(Shown(camel, mage), 0.0f);
    CHECK(!camel.GetThreatManager().isThreatListEmpty());

    camel.GetThreatManager().addThreat(&mage, 300.0f);
    CHECK_EQ(Shown(camel, mage), 0.0f);

    mage.GetHostileRefManager().RemoveThreatReduction(kAsk);
    CHECK_EQ(Shown(camel, mage), 5300.0f);
}

TEST(CreatureThreat_TwoAsksAreWithdrawnOneAtATime)
{
    ThreatTestCreature mage;
    ThreatTestCreature camel;
    camel.GetThreatManager().addThreat(&mage, 5000.0f);

    mage.GetHostileRefManager().AddThreatReduction(kAsk);
    mage.GetHostileRefManager().AddThreatReduction(kAsk);
    CHECK_EQ(Shown(camel, mage), 0.0f);

    mage.GetHostileRefManager().RemoveThreatReduction(kAsk);
    CHECK_EQ(Shown(camel, mage), 0.0f);

    mage.GetHostileRefManager().RemoveThreatReduction(kAsk);
    CHECK_EQ(Shown(camel, mage), 5000.0f);
}

TEST(CreatureThreat_AnAskOnNoThreatChangesNothing)
{
    ThreatTestCreature mage;
    ThreatTestCreature camel;
    camel.GetThreatManager().addThreat(&mage, 0.0f);
    REQUIRE(!camel.GetThreatManager().isThreatListEmpty());

    mage.GetHostileRefManager().AddThreatReduction(kAsk);
    CHECK_EQ(Shown(camel, mage), 0.0f);

    mage.GetHostileRefManager().RemoveThreatReduction(kAsk);
    CHECK_EQ(Shown(camel, mage), 0.0f);
}

TEST(CreatureThreat_AnAskSmallerThanTheThreatHoldsOffItsAmount)
{
    ThreatTestCreature mage;
    ThreatTestCreature camel;
    camel.GetThreatManager().addThreat(&mage, 5000.0f);

    mage.GetHostileRefManager().AddThreatReduction(1000.0f);
    CHECK_EQ(Shown(camel, mage), 4000.0f);

    camel.GetThreatManager().addThreat(&mage, 300.0f);
    CHECK_EQ(Shown(camel, mage), 4300.0f);

    mage.GetHostileRefManager().RemoveThreatReduction(1000.0f);
    CHECK_EQ(Shown(camel, mage), 5300.0f);
}

TEST(CreatureThreat_APercentUnderAnAskTakesItFromTheThreatEarned)
{
    ThreatTestCreature mage;
    ThreatTestCreature camel;
    camel.GetThreatManager().addThreat(&mage, 5000.0f);

    mage.GetHostileRefManager().AddThreatReduction(kAsk);
    mage.GetHostileRefManager().addThreatPercent(-10);
    CHECK_EQ(Shown(camel, mage), 0.0f);

    mage.GetHostileRefManager().RemoveThreatReduction(kAsk);
    CHECK_EQ(Shown(camel, mage), 4500.0f);
}

TEST(CreatureThreat_TheTauntTemporaryUnderAnAskMovesTheThreatEarned)
{
    ThreatTestCreature mage;
    ThreatTestCreature camel;
    camel.GetThreatManager().addThreat(&mage, 5000.0f);
    HostileReference* ref = mage.GetHostileRefManager().getFirst();
    REQUIRE(ref);

    mage.GetHostileRefManager().AddThreatReduction(1000.0f);
    REQUIRE(Shown(camel, mage) == 4000.0f);

    ref->setTempThreat(10000.0f);
    CHECK_EQ(Shown(camel, mage), 10000.0f);

    ref->resetTempThreat();
    CHECK_EQ(Shown(camel, mage), 4000.0f);

    mage.GetHostileRefManager().RemoveThreatReduction(1000.0f);
    CHECK_EQ(Shown(camel, mage), 5000.0f);
}

TEST(CreatureThreat_TheManagerAsksEveryListAndALaterListIsUntouched)
{
    ThreatTestCreature mage;
    ThreatTestCreature first;
    ThreatTestCreature second;
    ThreatTestCreature third;
    ThreatTestCreature later;
    first.GetThreatManager().addThreat(&mage, 5000.0f);
    second.GetThreatManager().addThreat(&mage, 700.0f);
    third.GetThreatManager().addThreat(&mage, 2500.0f);

    mage.GetHostileRefManager().AddThreatReduction(1000.0f);
    CHECK_EQ(Shown(first, mage), 4000.0f);
    CHECK_EQ(Shown(second, mage), 0.0f);
    CHECK_EQ(Shown(third, mage), 1500.0f);

    later.GetThreatManager().addThreat(&mage, 600.0f);
    CHECK_EQ(Shown(later, mage), 600.0f);

    mage.GetHostileRefManager().RemoveThreatReduction(1000.0f);
    CHECK_EQ(Shown(first, mage), 5000.0f);
    CHECK_EQ(Shown(second, mage), 700.0f);
    CHECK_EQ(Shown(third, mage), 2500.0f);
    CHECK_EQ(Shown(later, mage), 600.0f);
}

TEST(CreatureThreat_AReductionWhoseCasterIsGoneWhenItEndsIsWithdrawn)
{
    SpellEntry const* spell = StoreReductionSpell();
    WorldSession heldSession(1, "threat", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    WorldSession casterSession(2, "threat", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    ThreatTestPlayer held(heldSession, 1);
    ThreatTestPlayer caster(casterSession, 2);
    ThreatTestCreature camel;
    camel.GetThreatManager().addThreat(&held, 5000.0f);
    REQUIRE(Shown(camel, held) == 5000.0f);

    // The aura on the held player, cast by the other one, who is there when it is applied.
    caster.Arrive();
    SpellAuraHolder* holder = new SpellAuraHolder(spell, &held, NULL, NULL, NULL);
    holder->SetCasterGuid(caster.GetObjectGuid());
    int32 basePoints = kReductionPoints;
    Aura* aura = CreateAura(spell, EFFECT_INDEX_0, &basePoints, holder, &held);
    holder->AddAura(aura, EFFECT_INDEX_0);
    CHECK(aura->GetCaster() == &caster);

    aura->ApplyModifier(true, true);
    CHECK_EQ(Shown(camel, held), 0.0f);

    // The caster is gone when the aura ends: the ask is withdrawn all the same.
    caster.Leave();
    CHECK(aura->GetCaster() == NULL);
    aura->ApplyModifier(false, true);
    CHECK_EQ(Shown(camel, held), 5000.0f);

    delete holder;
}
