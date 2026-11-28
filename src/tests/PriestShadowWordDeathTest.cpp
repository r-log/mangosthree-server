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

/// Tests of priest/shadow/ShadowWordDeath.cpp and Masochism.cpp: the factor a direct hit of Shadow Word: Death takes
/// below 25% of its target's health and Mind Melt's at or below its percent, the base points of the recoil and Pain
/// and Suffering's percent, the glyph's reset rule, Spirit Tap's kill rule and the gray level it reads, the events
/// Masochism lets through, the rows the two files register, in the game's table too, the damage-taken site's three
/// rows Pain and Suffering no longer holds, and the exits of the after-hit row that cast nothing. The scripts run on
/// bare Creatures (update fields allocated; no map, template or AI); the spells whose effects a test reads are rows of
/// the test's own, put into the spell store by the test that reads them and taken out again. Scenarios 1236 and 1237
/// record Shadow Word: Death and Masochism in play.

#include "TestHarness.h"
#include "spells/classes/priest/shadow/Masochism.h"
#include "spells/classes/priest/shadow/ShadowWordDeath.h"
#include "spells/SpellScriptHooks.h"
#include "spells/handlers/SpellHandlerRegistry.h"
#include "spells/handlers/UnitSpellBonusHandlers.h"
#include "spells/data/SpellMgr.h"
#include "spells/auras/SpellAuras.h"
#include "data/ServerSpellStore.h"
#include "Creature.h"
#include "DBCStores.h"
#include "DBCStructure.h"
#include "Player.h"
#include "combat/Formulas.h"

#include <cmath>
#include <cstddef>

namespace
{
    uint32 const SHADOW_WORD_DEATH = 32379;
    uint32 const RECOIL = 32409;
    uint32 const MIND_MELT_RANK_1 = 14910;
    uint32 const MIND_MELT_RANK_2 = 33371;
    uint32 const PAIN_AND_SUFFERING_RANK_1 = 47580;
    uint32 const PAIN_AND_SUFFERING_RANK_2 = 47581;
    uint32 const PAIN_AND_SUFFERING_OLD_RANK_3 = 47582;
    uint32 const MASOCHISM_RANK_1 = 88994;
    uint32 const MASOCHISM_RANK_2 = 88995;

    uint32 const MIND_BLAST = 8092;
    uint32 const SHADOW_WORD_PAIN = 589;
    uint32 const FROSTBOLT = 116;

    /// What a periodic tick he takes and a hit he takes are, as the engine's proc flags name them.
    uint32 const TAKEN_TICK = PROC_FLAG_ON_TAKE_PERIODIC | PROC_FLAG_TAKEN_ANY_DAMAGE;
    uint32 const TAKEN_MELEE = PROC_FLAG_TAKEN_MELEE_HIT | PROC_FLAG_TAKEN_ANY_DAMAGE;
    uint32 const TAKEN_SPELL = PROC_FLAG_TAKEN_NEGATIVE_SPELL_HIT | PROC_FLAG_TAKEN_ANY_DAMAGE;

    /// Whether `a` is within a thousandth of `b`.
    bool Near(float a, float b)
    {
        return std::fabs(a - b) < 0.001f;
    }

    // A creature with its update fields allocated and no world, map, template or AI; its health and maximum health
    // set, a death state it may be put in, and the holders HoldAura files.
    class BareCreature : public Creature
    {
        public:
            BareCreature()
            {
                _InitValues();
            }

            // The holders HoldAura put in the map and the engine did not remove are the test's own, and leave the
            // map first.
            ~BareCreature()
            {
                m_auras.Holders().clear();
                delete[] m_uint32Values;
                m_uint32Values = NULL;
            }

            // Puts `holder` in the holder map under `spellId`, as Unit::AddSpellAuraHolder files it.
            void HoldAura(uint32 spellId, SpellAuraHolder& holder)
            {
                m_auras.AddHolder(spellId, &holder);
            }

            // His health and maximum health.
            void SetHealths(uint32 health, uint32 maxHealth)
            {
                SetUInt32Value(UNIT_FIELD_MAXHEALTH, maxHealth);
                SetUInt32Value(UNIT_FIELD_HEALTH, health);
            }

            // Dead, as a kill leaves a creature before the after-hit rows run.
            void Die()
            {
                m_deathState = DEAD;
            }
    };

    struct SpellRowStorage
    {
        alignas(SpellEntry) unsigned char bytes[sizeof(SpellEntry)];
    };

    struct HolderStorage
    {
        alignas(SpellAuraHolder) unsigned char bytes[sizeof(SpellAuraHolder)];
    };

    // A Spell.dbc row with only its id, which the contexts name; it is not in the spell store.
    SpellEntry const& SpellRow(SpellRowStorage& storage, uint32 spellId)
    {
        for (unsigned char& byte : storage.bytes)
        {
            byte = 0;
        }
        SpellEntry* row = reinterpret_cast<SpellEntry*>(storage.bytes);
        row->ID = spellId;
        return *row;
    }

    // A holder's storage without its constructor, every byte 0: it has no aura and only stands in the contexts.
    SpellAuraHolder& EmptyHolder(HolderStorage& storage)
    {
        for (unsigned char& byte : storage.bytes)
        {
            byte = 0;
        }
        return *reinterpret_cast<SpellAuraHolder*>(storage.bytes);
    }

    // A spell of the client's data as the scripts read it: its id and the base points of its three effects, which
    // each hold a dummy aura of that value.
    struct StoredSpell
    {
        ServerSpell row;
        SpellEffectEntry effects[MAX_EFFECT_INDEX];
        bool added;

        StoredSpell() : added(false) {}

        // Puts the spell `spellId` into the spell store, its effects added once.
        void Store(uint32 spellId, int32 points0, int32 points1, int32 points2)
        {
            int32 const points[MAX_EFFECT_INDEX] = { points0, points1, points2 };
            if (!added)
            {
                added = true;
                for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
                {
                    effects[i] = SpellEffectEntry();
                    effects[i].SpellID = spellId;
                    effects[i].EffectIndex = SpellEffectIndex(i);
                    effects[i].Effect = SPELL_EFFECT_APPLY_AURA;
                    effects[i].EffectAura = SPELL_AURA_DUMMY;
                    effects[i].EffectBasePoints = points[i];
                    AddSpellEffectEntry(&effects[i]);
                }
            }
            row.Entry().ID = spellId;
            sSpellStore.SetEntry(spellId, &row.Entry());
        }

        // Takes the spell out of the spell store again.
        void Forget(uint32 spellId)
        {
            sSpellStore.SetEntry(spellId, NULL);
        }
    };

    StoredSpell s_meltOne;
    StoredSpell s_meltTwo;
    StoredSpell s_painOne;
    StoredSpell s_painTwo;
    StoredSpell s_masochismOne;

    // Mind Melt's two ranks as the client holds them: effect 0 of 15 and 30, the first rank's effect 2 of 25.
    void StoreMindMelt()
    {
        s_meltOne.Store(MIND_MELT_RANK_1, 15, 0, 25);
        s_meltTwo.Store(MIND_MELT_RANK_2, 30, 0, 0);
    }

    void ForgetMindMelt()
    {
        s_meltOne.Forget(MIND_MELT_RANK_1);
        s_meltTwo.Forget(MIND_MELT_RANK_2);
    }

    // Pain and Suffering's two ranks: effect 1 of -20 and -40.
    void StorePainAndSuffering()
    {
        s_painOne.Store(PAIN_AND_SUFFERING_RANK_1, 1, -20, 0);
        s_painTwo.Store(PAIN_AND_SUFFERING_RANK_2, 1, -40, 0);
    }

    void ForgetPainAndSuffering()
    {
        s_painOne.Forget(PAIN_AND_SUFFERING_RANK_1);
        s_painTwo.Forget(PAIN_AND_SUFFERING_RANK_2);
    }

    // Masochism's first rank: effect 0 of 5 and effect 1 of 10, the percent of his maximum health a hit must reach.
    void StoreMasochism()
    {
        s_masochismOne.Store(MASOCHISM_RANK_1, 5, 10, 0);
    }

    void ForgetMasochism()
    {
        s_masochismOne.Forget(MASOCHISM_RANK_1);
    }

    // Files a holder under `spellId` on `unit`: the zeroed storage stands for it, as HasAura reads the key alone.
    void Hold(BareCreature& unit, uint32 spellId, HolderStorage& storage)
    {
        unit.HoldAura(spellId, EmptyHolder(storage));
    }
}

TEST(PriestShadowWordDeath_ADirectHitBelowTwentyFivePercentOfTheTargetsHealthIsDealtThreeTimes)
{
    // Below 25%: three times; at 25% and above: once.
    CHECK(Near(Priest::ShadowWordDeathMultiplier(SPELL_DIRECT_DAMAGE, 24, 100), 3.0f));
    CHECK(Near(Priest::ShadowWordDeathMultiplier(SPELL_DIRECT_DAMAGE, 25, 100), 1.0f));
    CHECK(Near(Priest::ShadowWordDeathMultiplier(SPELL_DIRECT_DAMAGE, 26, 100), 1.0f));
    CHECK(Near(Priest::ShadowWordDeathMultiplier(SPELL_DIRECT_DAMAGE, 100, 100), 1.0f));
    CHECK(Near(Priest::ShadowWordDeathMultiplier(SPELL_DIRECT_DAMAGE, 1, 100), 3.0f));
    CHECK(Near(Priest::ShadowWordDeathMultiplier(SPELL_DIRECT_DAMAGE, 0, 100), 3.0f));

    // The compare is made on whole numbers: 19372 of 77490 is 24.9994% and 19373 is 25.0006%; a maximum that would
    // overflow 32 bits when scaled is no wrap.
    CHECK(Near(Priest::ShadowWordDeathMultiplier(SPELL_DIRECT_DAMAGE, 19372, 77490), 3.0f));
    CHECK(Near(Priest::ShadowWordDeathMultiplier(SPELL_DIRECT_DAMAGE, 19373, 77490), 1.0f));
    CHECK(Near(Priest::ShadowWordDeathMultiplier(SPELL_DIRECT_DAMAGE, 19374, 77490), 1.0f));
    CHECK(Near(Priest::ShadowWordDeathMultiplier(SPELL_DIRECT_DAMAGE, 1000000000u, 4000000000u), 1.0f));
    CHECK(Near(Priest::ShadowWordDeathMultiplier(SPELL_DIRECT_DAMAGE, 999999999u, 4000000000u), 3.0f));

    // A periodic amount and a target with no maximum health are left as they are.
    CHECK(Near(Priest::ShadowWordDeathMultiplier(DOT, 10, 100), 1.0f));
    CHECK(Near(Priest::ShadowWordDeathMultiplier(SPELL_DIRECT_DAMAGE, 0, 0), 1.0f));

    // The row of 32379 multiplies the done multiplier the hit already holds, by the victim's health now.
    SpellHandlerRegistry registry;
    Priest::RegisterShadowWordDeath(registry);
    BareCreature priest;
    BareCreature victim;
    SpellRowStorage deathRow;
    SpellEntry const& death = SpellRow(deathRow, SHADOW_WORD_DEATH);
    struct Case
    {
        uint32 health;
        DamageEffectType type;
        float result;
    };
    Case const cases[] =
    {
        { 24, SPELL_DIRECT_DAMAGE, 4.5f },
        { 25, SPELL_DIRECT_DAMAGE, 1.5f },
        { 26, SPELL_DIRECT_DAMAGE, 1.5f },
        { 24, DOT, 1.5f },
    };
    for (Case const& one : cases)
    {
        victim.SetHealths(one.health, 100);
        float done = 1.5f;
        DamageDoneContext ctx(priest, victim, death, one.type, done);
        SpellHandlerOutcome<void> outcome = registry.Dispatch<DamageDoneHook>(SHADOW_WORD_DEATH, ctx);
        CHECK(!outcome.IsReturn());
        CHECK(Near(done, one.result));
    }
}

TEST(PriestShadowWordDeath_TheRecoilsPeriodicAmountTakesNoneOfThePriestsDoneModifiers)
{
    CHECK(Near(Priest::RecoilDoneMultiplier(DOT, 1.15f), 1.0f));
    CHECK(Near(Priest::RecoilDoneMultiplier(DOT, 0.5f), 1.0f));
    CHECK(Near(Priest::RecoilDoneMultiplier(SPELL_DIRECT_DAMAGE, 1.15f), 1.15f));

    // The row of 32409 sets the done multiplier the tick holds back to 1, as Shadow Power's +15% would leave it.
    SpellHandlerRegistry registry;
    Priest::RegisterShadowWordDeath(registry);
    BareCreature priest;
    SpellRowStorage recoilRow;
    SpellEntry const& recoil = SpellRow(recoilRow, RECOIL);
    float done = 1.15f;
    DamageDoneContext tick(priest, priest, recoil, DOT, done);
    CHECK(!registry.Dispatch<DamageDoneHook>(RECOIL, tick).IsReturn());
    CHECK(Near(done, 1.0f));
}

TEST(PriestShadowWordDeath_MindMeltRaisesAHitOfShadowWordDeathAtOrBelowItsPercentAndNoOtherDamage)
{
    // The first rank's 15 and the second's 30 at the threshold of 25: at 25 and below, 1.15 and 1.30; at 26, once.
    CHECK(Near(Priest::MindMeltMultiplier(SHADOW_WORD_DEATH, SPELL_DIRECT_DAMAGE, 25, 100, 25, 15), 1.15f));
    CHECK(Near(Priest::MindMeltMultiplier(SHADOW_WORD_DEATH, SPELL_DIRECT_DAMAGE, 25, 100, 25, 30), 1.30f));
    CHECK(Near(Priest::MindMeltMultiplier(SHADOW_WORD_DEATH, SPELL_DIRECT_DAMAGE, 24, 100, 25, 30), 1.30f));
    CHECK(Near(Priest::MindMeltMultiplier(SHADOW_WORD_DEATH, SPELL_DIRECT_DAMAGE, 0, 100, 25, 30), 1.30f));
    CHECK(Near(Priest::MindMeltMultiplier(SHADOW_WORD_DEATH, SPELL_DIRECT_DAMAGE, 26, 100, 25, 30), 1.0f));
    CHECK(Near(Priest::MindMeltMultiplier(SHADOW_WORD_DEATH, SPELL_DIRECT_DAMAGE, 100, 100, 25, 30), 1.0f));
    CHECK(Near(Priest::MindMeltMultiplier(SHADOW_WORD_DEATH, SPELL_DIRECT_DAMAGE, 19372, 77490, 25, 30), 1.30f));
    CHECK(Near(Priest::MindMeltMultiplier(SHADOW_WORD_DEATH, SPELL_DIRECT_DAMAGE, 19373, 77490, 25, 30), 1.0f));

    // Every other spell, a periodic amount, a target with no maximum and a talent or a threshold of 0 leave it.
    CHECK(Near(Priest::MindMeltMultiplier(MIND_BLAST, SPELL_DIRECT_DAMAGE, 10, 100, 25, 30), 1.0f));
    CHECK(Near(Priest::MindMeltMultiplier(SHADOW_WORD_PAIN, SPELL_DIRECT_DAMAGE, 10, 100, 25, 30), 1.0f));
    CHECK(Near(Priest::MindMeltMultiplier(RECOIL, DOT, 10, 100, 25, 30), 1.0f));
    CHECK(Near(Priest::MindMeltMultiplier(SHADOW_WORD_DEATH, DOT, 10, 100, 25, 30), 1.0f));
    CHECK(Near(Priest::MindMeltMultiplier(SHADOW_WORD_DEATH, SPELL_DIRECT_DAMAGE, 0, 0, 25, 30), 1.0f));
    CHECK(Near(Priest::MindMeltMultiplier(SHADOW_WORD_DEATH, SPELL_DIRECT_DAMAGE, 10, 100, 25, 0), 1.0f));
    CHECK(Near(Priest::MindMeltMultiplier(SHADOW_WORD_DEATH, SPELL_DIRECT_DAMAGE, 10, 100, 0, 30), 1.0f));

    // The rows of the two ranks, with the client's numbers in the store, each raise the done multiplier of a direct hit
    // of Shadow Word: Death; and with Shadow Word: Death's own row, a hit at 20% takes both: 3 x 1.30 = 3.9.
    StoreMindMelt();
    SpellHandlerRegistry registry;
    Priest::RegisterShadowWordDeath(registry);
    BareCreature priest;
    BareCreature victim;
    SpellRowStorage deathRow, painRow, blastRow;
    SpellEntry const& death = SpellRow(deathRow, SHADOW_WORD_DEATH);
    SpellEntry const& pain = SpellRow(painRow, SHADOW_WORD_PAIN);
    SpellEntry const& blast = SpellRow(blastRow, MIND_BLAST);

    struct Case
    {
        uint32 rank;
        SpellEntry const* spell;
        uint32 health;
        DamageEffectType type;
        float result;
    };
    Case const cases[] =
    {
        { MIND_MELT_RANK_1, &death, 25, SPELL_DIRECT_DAMAGE, 1.15f * 2.0f },
        { MIND_MELT_RANK_1, &death, 26, SPELL_DIRECT_DAMAGE, 2.0f },
        { MIND_MELT_RANK_2, &death, 25, SPELL_DIRECT_DAMAGE, 1.30f * 2.0f },
        { MIND_MELT_RANK_2, &death, 26, SPELL_DIRECT_DAMAGE, 2.0f },
        { MIND_MELT_RANK_2, &death, 25, DOT, 2.0f },
        { MIND_MELT_RANK_2, &pain, 10, SPELL_DIRECT_DAMAGE, 2.0f },
        { MIND_MELT_RANK_2, &blast, 10, SPELL_DIRECT_DAMAGE, 2.0f },
    };
    for (Case const& one : cases)
    {
        victim.SetHealths(one.health, 100);
        float done = 2.0f;
        DamageDoneContext ctx(priest, victim, *one.spell, one.type, done);
        SpellHandlerOutcome<void> outcome = registry.Dispatch<DamageDoneHook>(one.rank, ctx);
        CHECK(!outcome.IsReturn());
        CHECK(Near(done, one.result));
    }

    victim.SetHealths(20, 100);
    float both = 1.0f;
    DamageDoneContext hit(priest, victim, death, SPELL_DIRECT_DAMAGE, both);
    registry.Dispatch<DamageDoneHook>(SHADOW_WORD_DEATH, hit);
    registry.Dispatch<DamageDoneHook>(MIND_MELT_RANK_2, hit);
    CHECK(Near(both, 3.9f));

    // With the client's data missing there is no percent to add: the row leaves the hit alone.
    ForgetMindMelt();
    float none = 1.0f;
    DamageDoneContext bare(priest, victim, death, SPELL_DIRECT_DAMAGE, none);
    registry.Dispatch<DamageDoneHook>(MIND_MELT_RANK_1, bare);
    registry.Dispatch<DamageDoneHook>(MIND_MELT_RANK_2, bare);
    CHECK(Near(none, 1.0f));
    CHECK(sSpellStore.LookupEntry(MIND_MELT_RANK_1) == NULL);
    CHECK(sSpellStore.LookupEntry(MIND_MELT_RANK_2) == NULL);
}

TEST(PriestShadowWordDeath_TheRecoilIsTheWholeHitIncludingTheAbsorbedPartLessPainAndSuffering)
{
    // No talent: the whole hit; rank 1: 80%; rank 2: 60%, the fraction dropped.
    CHECK_EQ(Priest::ShadowWordDeathRecoil(1000, 0, 0), int32(1000));
    CHECK_EQ(Priest::ShadowWordDeathRecoil(1000, 0, -20), int32(800));
    CHECK_EQ(Priest::ShadowWordDeathRecoil(1000, 0, -40), int32(600));
    CHECK_EQ(Priest::ShadowWordDeathRecoil(1001, 0, -40), int32(600));
    CHECK_EQ(Priest::ShadowWordDeathRecoil(1, 0, -20), int32(0));
    CHECK_EQ(Priest::ShadowWordDeathRecoil(1, 0, 0), int32(1));

    // The absorbed part counts: a hit of 900 after the absorbs and 100 absorbed is a recoil of 1000 x 60%; a hit
    // the absorbs took whole still has its recoil. The sniff's hit of 16217 with part of it absorbed: 15892 for a
    // 2% reduction.
    CHECK_EQ(Priest::ShadowWordDeathRecoil(900, 100, -40), int32(600));
    CHECK_EQ(Priest::ShadowWordDeathRecoil(0, 1000, 0), int32(1000));
    CHECK_EQ(Priest::ShadowWordDeathRecoil(0, 1000, -40), int32(600));
    CHECK_EQ(Priest::ShadowWordDeathRecoil(16217, 0, -2), int32(15892));
    CHECK_EQ(Priest::ShadowWordDeathRecoil(12000, 4217, -2), int32(15892));

    // Nothing to deal back, and a reduction that takes it all, give 0; a whole hit past 31 bits is held to the most
    // a base value is.
    CHECK_EQ(Priest::ShadowWordDeathRecoil(0, 0, 0), int32(0));
    CHECK_EQ(Priest::ShadowWordDeathRecoil(500, 0, -100), int32(0));
    CHECK_EQ(Priest::ShadowWordDeathRecoil(500, 0, -150), int32(0));
    CHECK_EQ(Priest::ShadowWordDeathRecoil(4000000000u, 0, 0), int32(2147483647));

    // The percent is the rank he holds: -20, -40; none for none, and none when the store lacks the rank.
    StorePainAndSuffering();
    BareCreature priest;
    BareCreature other;
    HolderStorage rankOne, rankTwo, otherRankTwo;
    CHECK_EQ(Priest::PainAndSufferingPercent(priest), int32(0));
    Hold(priest, PAIN_AND_SUFFERING_RANK_1, rankOne);
    CHECK_EQ(Priest::PainAndSufferingPercent(priest), int32(-20));
    Hold(other, PAIN_AND_SUFFERING_RANK_2, otherRankTwo);
    CHECK_EQ(Priest::PainAndSufferingPercent(other), int32(-40));
    CHECK_EQ(Priest::PainAndSufferingPercent(priest), int32(-20));
    Hold(priest, PAIN_AND_SUFFERING_RANK_2, rankTwo);
    CHECK_EQ(Priest::PainAndSufferingPercent(priest), int32(-40));
    ForgetPainAndSuffering();
    CHECK_EQ(Priest::PainAndSufferingPercent(priest), int32(0));
    CHECK(sSpellStore.LookupEntry(PAIN_AND_SUFFERING_RANK_1) == NULL);
}

TEST(PriestShadowWordDeath_TheGlyphResetsTheCooldownOnlyForAHitThatLeftAnAliveTargetAtOrBelowItsPercentBeforeTheHit)
{
    // The target's health before the hit at or below 25%, alive after it, and no lockout held.
    CHECK(Priest::GlyphResetsShadowWordDeath(true, 25, 100, 25, false));
    CHECK(Priest::GlyphResetsShadowWordDeath(true, 24, 100, 25, false));
    CHECK(Priest::GlyphResetsShadowWordDeath(true, 1, 100, 25, false));
    CHECK(!Priest::GlyphResetsShadowWordDeath(true, 26, 100, 25, false));
    CHECK(!Priest::GlyphResetsShadowWordDeath(true, 100, 100, 25, false));
    CHECK(Priest::GlyphResetsShadowWordDeath(true, 19372, 77490, 25, false));
    CHECK(!Priest::GlyphResetsShadowWordDeath(true, 19373, 77490, 25, false));

    // The health before the hit is what is left and what the hit dealt: 5 left after a hit of 20 is 25 before it.
    uint64 const left = 5;
    uint64 const dealt = 20;
    CHECK(Priest::GlyphResetsShadowWordDeath(true, left + dealt, 100, 25, false));
    CHECK(!Priest::GlyphResetsShadowWordDeath(true, left + dealt + 1, 100, 25, false));
    CHECK(!Priest::GlyphResetsShadowWordDeath(true, 4000000000ull + 400000000ull, 4000000000u, 25, false));

    // A hit that killed the target, a lockout he holds, a percent of 0 and a target with no maximum reset nothing.
    CHECK(!Priest::GlyphResetsShadowWordDeath(false, 10, 100, 25, false));
    CHECK(!Priest::GlyphResetsShadowWordDeath(true, 10, 100, 25, true));
    CHECK(!Priest::GlyphResetsShadowWordDeath(true, 10, 100, 0, false));
    CHECK(!Priest::GlyphResetsShadowWordDeath(true, 0, 0, 25, false));
}

TEST(PriestShadowWordDeath_SpiritTapComesForAKillThatYieldsExperienceOrHonorOnly)
{
    // A kill, the glyph and a reward: all three, and no one without the others.
    CHECK(Priest::SpiritTapGives(true, true, true));
    CHECK(!Priest::SpiritTapGives(false, true, true));
    CHECK(!Priest::SpiritTapGives(true, false, true));
    CHECK(!Priest::SpiritTapGives(true, true, false));
    CHECK(!Priest::SpiritTapGives(false, false, false));

    // A level 85 priest's gray level is 76: a level 77 victim yields, a level 76 one and a lower one do not; a victim
    // that grants no experience (a totem, a pet, a template that forbids it) yields none.
    CHECK_EQ(Priest::GrayLevelOf(85), uint32(76));
    CHECK(Priest::YieldsExperienceOrHonor(85, 85, true));
    CHECK(Priest::YieldsExperienceOrHonor(85, 77, true));
    CHECK(!Priest::YieldsExperienceOrHonor(85, 76, true));
    CHECK(!Priest::YieldsExperienceOrHonor(85, 1, true));
    CHECK(!Priest::YieldsExperienceOrHonor(85, 85, false));

    // The gray level is the engine's for every level: Player::isHonorOrXPTarget's own table.
    for (uint32 level = 0; level <= 100; ++level)
    {
        CHECK_EQ(Priest::GrayLevelOf(level), MaNGOS::XP::GetGrayLevel(level));
    }
}

TEST(PriestShadowWordDeath_MasochismTakesTheRecoilTickOfAnySizeAndAHitOfTheTalentsPercentAndNoOtherEvent)
{
    // The recoil's tick, of any size, a tick that dealt nothing (the absorbs took it whole) among them.
    CHECK(Priest::MasochismTakes(RECOIL, TAKEN_TICK, 1, 100000, 10));
    CHECK(Priest::MasochismTakes(RECOIL, TAKEN_TICK, 1000, 100000, 10));
    CHECK(Priest::MasochismTakes(RECOIL, TAKEN_TICK, 50000, 100000, 10));
    CHECK(Priest::MasochismTakes(RECOIL, PROC_FLAG_ON_TAKE_PERIODIC, 0, 100000, 10));

    // A hit of 10% of his maximum health after the absorbs and above, of a swing or a spell; not 9.9%.
    CHECK(Priest::MasochismTakes(0, TAKEN_MELEE, 10000, 100000, 10));
    CHECK(Priest::MasochismTakes(0, TAKEN_MELEE, 11000, 100000, 10));
    CHECK(!Priest::MasochismTakes(0, TAKEN_MELEE, 9999, 100000, 10));
    CHECK(!Priest::MasochismTakes(0, TAKEN_MELEE, 9000, 100000, 10));
    CHECK(Priest::MasochismTakes(FROSTBOLT, TAKEN_SPELL, 10000, 100000, 10));
    CHECK(!Priest::MasochismTakes(FROSTBOLT, TAKEN_SPELL, 9999, 100000, 10));
    CHECK(!Priest::MasochismTakes(0, TAKEN_MELEE, 0, 100000, 10));

    // Any other periodic tick, whatever its size, and a periodic event of no spell: no. The recoil's cast, which
    // reaches the procs as a hit of no damage, is no tick: no.
    CHECK(!Priest::MasochismTakes(SHADOW_WORD_PAIN, TAKEN_TICK, 1, 100000, 10));
    CHECK(!Priest::MasochismTakes(SHADOW_WORD_PAIN, TAKEN_TICK, 50000, 100000, 10));
    CHECK(!Priest::MasochismTakes(0, TAKEN_TICK, 50000, 100000, 10));
    CHECK(!Priest::MasochismTakes(RECOIL, TAKEN_SPELL, 0, 100000, 10));

    // A percent of 0 or less and a unit with no maximum health let no hit through; the recoil still counts.
    CHECK(!Priest::MasochismTakes(0, TAKEN_MELEE, 50000, 100000, 0));
    CHECK(!Priest::MasochismTakes(0, TAKEN_MELEE, 50000, 100000, -10));
    CHECK(!Priest::MasochismTakes(0, TAKEN_MELEE, 0, 0, 10));
    CHECK(Priest::MasochismTakes(RECOIL, TAKEN_TICK, 1, 100000, 0));

    // A maximum past 32 bits times the percent is no wrap.
    CHECK(Priest::MasochismTakes(0, TAKEN_MELEE, 400000000u, 4000000000u, 10));
    CHECK(!Priest::MasochismTakes(0, TAKEN_MELEE, 399999999u, 4000000000u, 10));
}

TEST(PriestShadowWordDeath_TheFilesHoldTheirRowsInTheGameAndTheDamageTakenSiteHoldsNoPainAndSuffering)
{
    // ShadowWordDeath.cpp: the damage-done row of Shadow Word: Death, of each Mind Melt rank and of the recoil, and
    // the after-hit row of Shadow Word: Death, no default.
    SpellHandlerRegistry death;
    CHECK_EQ(Priest::RegisterShadowWordDeath(death), uint32(5));
    CHECK_EQ(death.Count(), std::size_t(5));
    CHECK_EQ(death.CountDefaults(), std::size_t(0));
    CHECK_EQ(death.CountAt(DamageDoneHook::Key), std::size_t(4));
    CHECK(death.Find<DamageDoneHook>(RECOIL) != NULL);
    CHECK_EQ(death.CountAt(AfterHitHook::Key), std::size_t(1));
    CHECK(death.Find<DamageDoneHook>(SHADOW_WORD_DEATH) != NULL);
    CHECK(death.Find<DamageDoneHook>(MIND_MELT_RANK_1) != NULL);
    CHECK(death.Find<DamageDoneHook>(MIND_MELT_RANK_2) != NULL);
    CHECK(death.Find<AfterHitHook>(SHADOW_WORD_DEATH) != NULL);
    CHECK(death.Find<DamageDoneHook>(MIND_MELT_RANK_1) != death.Find<DamageDoneHook>(MIND_MELT_RANK_2));

    // Masochism.cpp: the check-proc row of each rank, no default.
    SpellHandlerRegistry masochism;
    CHECK_EQ(Priest::RegisterMasochism(masochism), uint32(2));
    CHECK_EQ(masochism.Count(), std::size_t(2));
    CHECK_EQ(masochism.CountDefaults(), std::size_t(0));
    CHECK_EQ(masochism.CountAt(CheckProcHook::Key), std::size_t(2));
    CHECK(masochism.Find<CheckProcHook>(MASOCHISM_RANK_1) != NULL);
    CHECK(masochism.Find<CheckProcHook>(MASOCHISM_RANK_2) == masochism.Find<CheckProcHook>(MASOCHISM_RANK_1));

    // The game's table holds the same rows, and no row of the spells these files leave alone: Mind Melt's two ranks
    // hold no after-hit or check-proc row, Masochism no damage-done row, the recoil no after-hit row.
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    CHECK(game.Find<DamageDoneHook>(SHADOW_WORD_DEATH) == death.Find<DamageDoneHook>(SHADOW_WORD_DEATH));
    CHECK(game.Find<DamageDoneHook>(MIND_MELT_RANK_1) == death.Find<DamageDoneHook>(MIND_MELT_RANK_1));
    CHECK(game.Find<DamageDoneHook>(MIND_MELT_RANK_2) == death.Find<DamageDoneHook>(MIND_MELT_RANK_2));
    CHECK(game.Find<AfterHitHook>(SHADOW_WORD_DEATH) == death.Find<AfterHitHook>(SHADOW_WORD_DEATH));
    CHECK(game.Find<CheckProcHook>(MASOCHISM_RANK_1) == masochism.Find<CheckProcHook>(MASOCHISM_RANK_1));
    CHECK(game.Find<CheckProcHook>(MASOCHISM_RANK_2) == masochism.Find<CheckProcHook>(MASOCHISM_RANK_2));
    CHECK(game.Find<AfterHitHook>(MIND_MELT_RANK_1) == NULL);
    CHECK(game.Find<AfterHitHook>(MIND_MELT_RANK_2) == NULL);
    CHECK(game.Find<CheckProcHook>(MIND_MELT_RANK_1) == NULL);
    CHECK(game.Find<DamageDoneHook>(MASOCHISM_RANK_1) == NULL);
    CHECK(game.Find<DamageDoneHook>(RECOIL) == death.Find<DamageDoneHook>(RECOIL));
    CHECK(game.Find<AfterHitHook>(RECOIL) == NULL);

    // Site 61: Pain and Suffering's victim-side handler and its three rows are gone, so the damage-taken site has no
    // row for either rank or for the 3.3.5 third rank, and Shadow Word: Death's damage taken is the engine's alone;
    // the rows of the other three labels are still there.
    for (uint32 spellId : { PAIN_AND_SUFFERING_RANK_1, PAIN_AND_SUFFERING_RANK_2, PAIN_AND_SUFFERING_OLD_RANK_3 })
    {
        CHECK(game.Find<SpellDamageTakenSite>(spellId) == NULL);
    }
    for (uint32 spellId : { 45182u, 20911u, 25899u })
    {
        CHECK(game.Find<SpellDamageTakenSite>(spellId) != NULL);
    }
}

TEST(PriestShadowWordDeath_TheMasochismRowAnswersFromTheEventAndTheTalentsPercentOfTheOwnersMaximumHealth)
{
    StoreMasochism();
    SpellHandlerRegistry registry;
    Priest::RegisterMasochism(registry);
    BareCreature owner;
    BareCreature attacker;
    owner.SetHealths(100000, 100000);
    SpellRowStorage auraRow, recoilRow, painRow, frostRow;
    HolderStorage storage;
    SpellAuraHolder& holder = EmptyHolder(storage);
    SpellEntry const& aura = SpellRow(auraRow, MASOCHISM_RANK_1);
    SpellEntry const& recoil = SpellRow(recoilRow, RECOIL);
    SpellEntry const& pain = SpellRow(painRow, SHADOW_WORD_PAIN);
    SpellEntry const& frost = SpellRow(frostRow, FROSTBOLT);

    struct Event
    {
        SpellEntry const* spell;
        uint32 procFlag;
        uint32 procEx;
        uint32 damage;
        bool gives;
    };
    Event const events[] =
    {
        { &recoil, TAKEN_TICK, PROC_EX_NORMAL_HIT, 600, true },         // the recoil's tick, low
        { &recoil, TAKEN_TICK, PROC_EX_NORMAL_HIT, 30000, true },       // the recoil's tick, large
        { &recoil, PROC_FLAG_ON_TAKE_PERIODIC, PROC_EX_NORMAL_HIT, 0, true },   // its tick, absorbed whole
        { NULL, TAKEN_MELEE, PROC_EX_NORMAL_HIT, 11000, true },         // a swing of 11%
        { NULL, TAKEN_MELEE, PROC_EX_CRITICAL_HIT, 10000, true },       // a critical swing of exactly 10%
        { NULL, TAKEN_MELEE, PROC_EX_NORMAL_HIT, 9000, false },         // a swing of 9%
        { &frost, TAKEN_SPELL, PROC_EX_NORMAL_HIT, 12000, true },       // a spell hit of 12%
        { &frost, TAKEN_SPELL, PROC_EX_NORMAL_HIT, 500, false },        // a low spell hit
        { &pain, TAKEN_TICK, PROC_EX_NORMAL_HIT, 12000, false },        // another tick of 12%
        { &recoil, TAKEN_SPELL, PROC_EX_NORMAL_HIT, 0, false },         // the recoil's cast
    };
    for (uint32 rank : { MASOCHISM_RANK_1, MASOCHISM_RANK_2 })
    {
        for (Event const& event : events)
        {
            CheckProcContext ctx(owner, &attacker, aura, AuraHandle(holder), event.spell, event.procFlag,
                                 event.procEx, event.damage);
            SpellHandlerOutcome<bool> outcome = registry.Dispatch<CheckProcHook>(rank, ctx);
            CHECK(outcome.IsReturn());
            CHECK(outcome.GetValue() == event.gives);
        }
    }

    // A hit of 9% of a unit of 200000 maximum health is 18000: his percent follows his maximum.
    owner.SetHealths(200000, 200000);
    CheckProcContext big(owner, &attacker, aura, AuraHandle(holder), NULL, TAKEN_MELEE, PROC_EX_NORMAL_HIT, 19999);
    CHECK(!registry.Dispatch<CheckProcHook>(MASOCHISM_RANK_2, big).GetValue());
    CheckProcContext enough(owner, &attacker, aura, AuraHandle(holder), NULL, TAKEN_MELEE, PROC_EX_NORMAL_HIT, 20000);
    CHECK(registry.Dispatch<CheckProcHook>(MASOCHISM_RANK_2, enough).GetValue());

    // With the client's data missing there is no percent: no hit gives mana, and the recoil still does.
    ForgetMasochism();
    CheckProcContext swing(owner, &attacker, aura, AuraHandle(holder), NULL, TAKEN_MELEE, PROC_EX_NORMAL_HIT, 150000);
    CHECK(!registry.Dispatch<CheckProcHook>(MASOCHISM_RANK_1, swing).GetValue());
    CheckProcContext tick(owner, &attacker, aura, AuraHandle(holder), &recoil, TAKEN_TICK, PROC_EX_NORMAL_HIT, 1);
    CHECK(registry.Dispatch<CheckProcHook>(MASOCHISM_RANK_1, tick).GetValue());
    CHECK(sSpellStore.LookupEntry(MASOCHISM_RANK_1) == NULL);
}

TEST(PriestShadowWordDeath_TheAfterHitRowCastsNothingForAHealNoRecoilNoGlyphAndNoKill)
{
    SpellHandlerRegistry registry;
    Priest::RegisterShadowWordDeath(registry);
    BareCreature priest;
    BareCreature target;
    target.SetHealths(50, 100);
    SpellRowStorage deathRow;
    SpellEntry const& death = SpellRow(deathRow, SHADOW_WORD_DEATH);

    // A heal of the spell: no recoil, no glyph.
    AfterHitContext heal(priest, target, death, 1000, false, 0, 0, true);
    CHECK(!registry.Dispatch<AfterHitHook>(SHADOW_WORD_DEATH, heal).IsReturn());

    // A hit that dealt nothing and had nothing absorbed leaves a recoil of 0: none is cast; he holds no glyph of
    // either kind, so no cooldown is touched and no lockout cast.
    AfterHitContext nothing(priest, target, death, 0, false, 0, 0, false);
    CHECK(!registry.Dispatch<AfterHitHook>(SHADOW_WORD_DEATH, nothing).IsReturn());

    // A hit that killed a target with no Glyph of Spirit Tap on the priest: no recoil, no Spirit Tap.
    target.Die();
    AfterHitContext kill(priest, target, death, 1000, false, 0, 0, false);
    CHECK(!registry.Dispatch<AfterHitHook>(SHADOW_WORD_DEATH, kill).IsReturn());
    CHECK(priest.GetSpellAuraHolderMap().empty());
    CHECK(target.GetSpellAuraHolderMap().empty());
}
