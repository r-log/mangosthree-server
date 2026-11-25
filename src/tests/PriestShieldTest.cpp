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

/// Tests of spells/classes/priest/Shield.cpp: Power Word: Shield's amount with Improved Power Word: Shield, Twin
/// Disciplines and the mastery, Rapture's removal modes and mana, the two rows the file registers, in the game's
/// table too, and the exits of its two scripts that change nothing; and Glyph of Power Word: Shield's heal, which
/// Spells::HealTakesNoDoneBonus keeps from the healing-done bonus. The scripts run on a bare Creature (update fields
/// allocated; no map, template or AI), whose holder map holds the test's empty holders where a script asks whether it
/// holds an aura; an empty holder has no aura, so its effect 0 amount is 0. No case reaches a cast, and only the glyph
/// heal's test reads the spell store: it puts its own rows there and takes its spell rows out again, so the rows other
/// tests leave there change nothing here. Scenario 1202 records the amount as the clients are told it, the heal, the
/// mana and the lockout, the energize's recovery time in his cooldown store, in play.

#include "TestHarness.h"
#include "spells/classes/priest/Shield.h"
#include "spells/SpellScriptHooks.h"
#include "spells/handlers/SpellHandlerRegistry.h"
#include "spells/auras/SpellAuras.h"
#include "spells/data/SpellTables.h"
#include "data/ServerSpellStore.h"
#include "Creature.h"
#include "DBCStores.h"
#include "DBCStructure.h"

#include <cstddef>
#include <vector>

namespace
{
    uint32 const POWER_WORD_SHIELD = 17;

    // A creature with its update fields allocated and no world, map, template or AI.
    class BareCreature : public Creature
    {
        public:
            BareCreature()
            {
                _InitValues();
            }

            // Unit::CleanupsBeforeDelete tears down an in-world unit only while the update fields exist. The holders
            // HoldAura put in the map are the test's own storage and leave the map first, and the lists ListAura
            // filled are emptied.
            ~BareCreature()
            {
                m_auras.Holders().clear();
                for (AuraType type : m_listedTypes)
                {
                    m_auras.ByType(type).clear();
                }
                delete[] m_uint32Values;
                m_uint32Values = NULL;
            }

            // Puts `holder` in the holder map under `spellId`, so Unit::HasAura(spellId) finds it.
            void HoldAura(uint32 spellId, SpellAuraHolder& holder)
            {
                m_auras.AddHolder(spellId, &holder);
            }

            // Puts `aura` in the list of `type`, so the unit's reads of that aura type read its amount.
            void ListAura(AuraType type, Aura* aura)
            {
                m_auras.ByType(type).push_back(aura);
                m_listedTypes.push_back(type);
            }

        private:
            std::vector<AuraType> m_listedTypes;
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

    // A holder's storage without its constructor, every byte 0: it has no aura and only stands in the holder map.
    SpellAuraHolder& EmptyHolder(HolderStorage& storage)
    {
        for (unsigned char& byte : storage.bytes)
        {
            byte = 0;
        }
        return *reinterpret_cast<SpellAuraHolder*>(storage.bytes);
    }
}

TEST(PriestShield_TheAmountIsRaisedByImprovedPowerWordShieldTwinDisciplinesAndTheMasteryEachAFactor)
{
    // Improved Power Word: Shield's two ranks (14748, 14768: 10 and 20), Twin Disciplines' three (47586, 47587, 47588:
    // 2, 4 and 6) and the mastery's 8 points at 2.5 a point (20).
    CHECK_EQ(Priest::ShieldAmount(10000, 0, 0, 0), int32(10000));
    CHECK_EQ(Priest::ShieldAmount(10000, 10, 0, 0), int32(11000));
    CHECK_EQ(Priest::ShieldAmount(10000, 20, 0, 0), int32(12000));
    CHECK_EQ(Priest::ShieldAmount(10000, 0, 6, 0), int32(10600));
    CHECK_EQ(Priest::ShieldAmount(10000, 0, 0, 20), int32(12000));
    CHECK_EQ(Priest::ShieldAmount(10000, 20, 0, 20), int32(14400));
    CHECK_EQ(Priest::ShieldAmount(10000, 10, 0, 20), int32(13200));
    CHECK_EQ(Priest::ShieldAmount(10000, 20, 6, 20), int32(15264));
    CHECK_EQ(Priest::ShieldAmount(10000, 20, 2, 20), int32(14688));

    // Truncated once, after every factor: 1707 x 1.2 x 1.2 is 2458.08, truncated after each it would be 2457; with
    // Twin Disciplines 3, 1707 x 1.2 x 1.06 x 1.2 is 2605.56.
    CHECK_EQ(Priest::ShieldAmount(1707, 20, 0, 20), int32(2458));
    CHECK_EQ(Priest::ShieldAmount(1707, 20, 6, 20), int32(2605));
    CHECK_EQ(Priest::ShieldAmount(1, 20, 6, 20), int32(1));

    // A percent of 0 or less raises nothing; an amount of 0 or less stays as it is.
    CHECK_EQ(Priest::ShieldAmount(10000, -20, -6, -20), int32(10000));
    CHECK_EQ(Priest::ShieldAmount(0, 20, 6, 20), int32(0));
    CHECK_EQ(Priest::ShieldAmount(-5, 20, 6, 20), int32(-5));

    // A large shield does not overflow on the way.
    CHECK_EQ(Priest::ShieldAmount(1000000, 20, 6, 50), int32(1908000));
}

namespace
{
    uint32 const GLYPH_HEAL = 56160;
    uint32 const kOtherHeal = 94470;            // a heal no table names
    uint32 const kHealingDoneAura = 94471;      // stands for Twin Disciplines 3/3's healing done
    uint32 const kMagicCategories = 94472;

    ServerSpell s_glyphHeal;
    ServerSpell s_otherHeal;
    ServerSpell s_healingDone;
    SpellCategoriesEntry s_magicCategories;

    // Puts the two heals, each of the magic damage class (as 56160's row is in the client's data, so it can crit), and
    // the healing-done aura's spell, one effect whose apply and removal change nothing on a unit, into the stores; the
    // categories and the effect once.
    void StoreHealRows()
    {
        static bool stored = false;
        if (!stored)
        {
            s_magicCategories = SpellCategoriesEntry();
            s_magicCategories.DefenseType = SPELL_DAMAGE_CLASS_MAGIC;
            sSpellCategoriesStore.SetEntry(kMagicCategories, &s_magicCategories);
            s_healingDone.effect = SpellEffectEntry();
            s_healingDone.effect.SpellID = kHealingDoneAura;
            s_healingDone.effect.EffectIndex = EFFECT_INDEX_0;
            s_healingDone.effect.Effect = SPELL_EFFECT_APPLY_AURA;
            s_healingDone.effect.EffectAura = SPELL_AURA_MOD_DAMAGE_FROM_CASTER;
            s_healingDone.effect.ImplicitTarget_0 = TARGET_SELF;
            AddSpellEffectEntry(&s_healingDone.effect);
            stored = true;
        }
        s_glyphHeal.Entry().ID = GLYPH_HEAL;
        s_glyphHeal.Entry().CategoriesID = kMagicCategories;
        s_otherHeal.Entry().ID = kOtherHeal;
        s_otherHeal.Entry().CategoriesID = kMagicCategories;
        s_healingDone.Entry().ID = kHealingDoneAura;
        sSpellStore.SetEntry(GLYPH_HEAL, &s_glyphHeal.Entry());
        sSpellStore.SetEntry(kOtherHeal, &s_otherHeal.Entry());
        sSpellStore.SetEntry(kHealingDoneAura, &s_healingDone.Entry());
    }

    // Takes the three spell rows out of the spell store again.
    void ForgetHealRows()
    {
        for (uint32 spellId : { GLYPH_HEAL, kOtherHeal, kHealingDoneAura })
        {
            sSpellStore.SetEntry(spellId, NULL);
        }
    }
}

TEST(PriestShield_TheGlyphHealIsAFifthOfTheShieldWithNoHealingDoneBonusAndTwiceThatOnACrit)
{
    // The table names Glyph of Power Word: Shield's heal alone.
    CHECK(Spells::HealTakesNoDoneBonus(GLYPH_HEAL));
    for (uint32 spellId : { 0u, 17u, 2061u, 2060u, 47750u, 33110u, 139u, 47753u })
    {
        CHECK(!Spells::HealTakesNoDoneBonus(spellId));
    }

    // A shield of 10600, Twin Disciplines 3/3's 6% counted in it: the holder's boosts cast the glyph's 20% of it.
    int32 const shield = 10600;
    int32 const glyphPercent = 20;
    uint32 const cast = uint32(shield * glyphPercent / 100);
    CHECK_EQ(cast, uint32(2120));

    // The priest holds Twin Disciplines 3/3's 6% healing done. The glyph's heal takes none of it and heals 2120, a crit
    // twice that; a magic heal no table names takes it, 2120 x 1.06.
    StoreHealRows();
    {
        BareCreature priest;
        SpellAuraHolder* twin = new SpellAuraHolder(&s_healingDone.Entry(), &priest, NULL, NULL, NULL);
        int32 basePoints = 6;
        twin->AddAura(CreateAura(&s_healingDone.Entry(), EFFECT_INDEX_0, &basePoints, twin, &priest), EFFECT_INDEX_0);
        priest.ListAura(SPELL_AURA_MOD_HEALING_DONE_PERCENT, twin->GetAuraByEffectIndex(EFFECT_INDEX_0));
        CHECK_EQ(s_glyphHeal.Entry().GetDmgClass(), uint32(SPELL_DAMAGE_CLASS_MAGIC));

        uint32 const healed = priest.SpellHealingBonusDone(&priest, &s_glyphHeal.Entry(), int32(cast), HEAL);
        CHECK_EQ(healed, uint32(2120));
        CHECK_EQ(priest.SpellCriticalHealingBonus(&s_glyphHeal.Entry(), healed, &priest), uint32(4240));
        CHECK_EQ(priest.SpellHealingBonusDone(&priest, &s_otherHeal.Entry(), int32(cast), HEAL), uint32(2247));
        delete twin;
    }
    ForgetHealRows();
    CHECK(sSpellStore.LookupEntry(GLYPH_HEAL) == NULL);
}

TEST(PriestShield_RaptureComesFromAShieldUsedUpOrDispelledAlone)
{
    CHECK(Priest::RaptureFires(AURA_REMOVE_BY_SHIELD_BREAK));
    CHECK(Priest::RaptureFires(AURA_REMOVE_BY_DISPEL));
    AuraRemoveMode const silent[] =
    {
        AURA_REMOVE_BY_DEFAULT, AURA_REMOVE_BY_STACK, AURA_REMOVE_BY_CANCEL, AURA_REMOVE_BY_DEATH,
        AURA_REMOVE_BY_DELETE, AURA_REMOVE_BY_EXPIRE, AURA_REMOVE_BY_TRACKING, AURA_REMOVE_BY_TARGET_LIMIT,
        AURA_REMOVE_BY_DAMAGE,
    };
    for (AuraRemoveMode mode : silent)
    {
        CHECK(!Priest::RaptureFires(mode));
    }
}

TEST(PriestShield_RaptureGivesTheRanksPercentOfMaximumMana)
{
    // The three ranks' effect 0 percents (47535, 47536, 47537: 2, 5 and 7). The retail 4.3.4 sniff's recorder got 8456
    // of about 120,800, 7%.
    CHECK_EQ(Priest::RaptureMana(120800, 7), int32(8456));
    CHECK_EQ(Priest::RaptureMana(120800, 5), int32(6040));
    CHECK_EQ(Priest::RaptureMana(120800, 2), int32(2416));
    CHECK_EQ(Priest::RaptureMana(23745, 7), int32(1662));
    CHECK_EQ(Priest::RaptureMana(99, 1), int32(0));
    CHECK_EQ(Priest::RaptureMana(0, 7), int32(0));
    CHECK_EQ(Priest::RaptureMana(120800, 0), int32(0));
    CHECK_EQ(Priest::RaptureMana(120800, -7), int32(0));
    CHECK_EQ(Priest::RaptureMana(4294967295u, 7), int32(300647710));
}

TEST(PriestShield_PowerWordShieldHoldsTheAuraAmountAndRemoveScriptsInTheGame)
{
    // Two rows on 17, one at each hook, no default.
    SpellHandlerRegistry registry;
    CHECK_EQ(Priest::RegisterShield(registry), uint32(2));
    CHECK_EQ(registry.Count(), std::size_t(2));
    CHECK_EQ(registry.CountDefaults(), std::size_t(0));
    CHECK_EQ(registry.CountAt(AuraAmountHook::Key), std::size_t(1));
    CHECK_EQ(registry.CountAt(AuraRemoveHook::Key), std::size_t(1));
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    CHECK(registry.Find<AuraAmountHook>(POWER_WORD_SHIELD) != NULL);
    CHECK(registry.Find<AuraRemoveHook>(POWER_WORD_SHIELD) != NULL);
    CHECK(game.Find<AuraAmountHook>(POWER_WORD_SHIELD) == registry.Find<AuraAmountHook>(POWER_WORD_SHIELD));
    CHECK(game.Find<AuraRemoveHook>(POWER_WORD_SHIELD) == registry.Find<AuraRemoveHook>(POWER_WORD_SHIELD));

    // The talents and spells the scripts read hold no row of their own at either hook: Improved Power Word: Shield's
    // ranks, Twin Disciplines', Shield Discipline, Rapture's ranks and its energize, Borrowed Time's ranks and its
    // haste.
    for (uint32 spellId : { 14748u, 14768u, 47586u, 47587u, 47588u, 77484u, 47535u, 47536u, 47537u, 47755u, 52795u,
                            52797u, 59888u })
    {
        CHECK(game.Find<AuraAmountHook>(spellId) == NULL);
        CHECK(game.Find<AuraRemoveHook>(spellId) == NULL);
    }
}

TEST(PriestShield_TheMasteryIsTheShieldDisciplineHoldersAmountAndNothingWithoutIt)
{
    // A unit without Shield Discipline (77484) adds nothing; a holder of it with no aura holds no amount.
    BareCreature caster;
    CHECK_EQ(Priest::AbsorbMastery(caster), int32(0));
    HolderStorage storage;
    caster.HoldAura(77484, EmptyHolder(storage));
    CHECK(caster.HasAura(77484));
    CHECK_EQ(Priest::AbsorbMastery(caster), int32(0));
}

TEST(PriestShield_TheScriptsChangeNothingAndLetTheBoostsRunOutsideTheirCase)
{
    SpellHandlerRegistry registry;
    Priest::RegisterShield(registry);
    BareCreature priest;
    SpellRowStorage row;
    HolderStorage storage;
    SpellAuraHolder& holder = EmptyHolder(storage);
    SpellEntry const& shield = SpellRow(row, POWER_WORD_SHIELD);

    // The amount: a shield with no caster, and an effect other than the absorb, keep the amount the engine reckoned;
    // a caster with no spell power (a creature, with no spell power aura), no talent and no mastery leaves it too.
    int32 amount = 8136;
    AuraAmountContext noCaster(NULL, priest, shield, EFFECT_INDEX_0, amount);
    CHECK(registry.Dispatch<AuraAmountHook>(POWER_WORD_SHIELD, noCaster).IsContinue());
    CHECK_EQ(amount, int32(8136));
    AuraAmountContext otherEffect(&priest, priest, shield, EFFECT_INDEX_1, amount);
    CHECK(registry.Dispatch<AuraAmountHook>(POWER_WORD_SHIELD, otherEffect).IsContinue());
    CHECK_EQ(amount, int32(8136));
    AuraAmountContext plain(&priest, priest, shield, EFFECT_INDEX_0, amount);
    CHECK(registry.Dispatch<AuraAmountHook>(POWER_WORD_SHIELD, plain).IsContinue());
    CHECK_EQ(amount, int32(8136));

    // The removal: every mode but a break and a dispel, a caster who cannot be found and a caster who knows no Rapture
    // cast nothing.
    for (AuraRemoveMode mode : { AURA_REMOVE_BY_EXPIRE, AURA_REMOVE_BY_CANCEL, AURA_REMOVE_BY_DEFAULT,
                                 AURA_REMOVE_BY_DEATH, AURA_REMOVE_BY_STACK })
    {
        AuraRemoveContext other(priest, &priest, shield, AuraHandle(holder), mode);
        CHECK(registry.Dispatch<AuraRemoveHook>(POWER_WORD_SHIELD, other).IsContinue());
    }
    AuraRemoveContext noRemover(priest, NULL, shield, AuraHandle(holder), AURA_REMOVE_BY_SHIELD_BREAK);
    CHECK(registry.Dispatch<AuraRemoveHook>(POWER_WORD_SHIELD, noRemover).IsContinue());
    AuraRemoveContext noTalent(priest, &priest, shield, AuraHandle(holder), AURA_REMOVE_BY_SHIELD_BREAK);
    CHECK(registry.Dispatch<AuraRemoveHook>(POWER_WORD_SHIELD, noTalent).IsContinue());

    // A caster whose Rapture rank's holder holds no aura gives nothing, broken or dispelled.
    HolderStorage rankStorage;
    priest.HoldAura(47537, EmptyHolder(rankStorage));
    for (AuraRemoveMode mode : { AURA_REMOVE_BY_SHIELD_BREAK, AURA_REMOVE_BY_DISPEL })
    {
        AuraRemoveContext emptyRank(priest, &priest, shield, AuraHandle(holder), mode);
        CHECK(registry.Dispatch<AuraRemoveHook>(POWER_WORD_SHIELD, emptyRank).IsContinue());
    }

    // Nothing was cast on him: his holder map holds the test's holder alone, and no aura reached him.
    CHECK_EQ(priest.GetSpellAuraHolderMap().size(), std::size_t(1));
    CHECK(!priest.HasAura(47755));
    CHECK(!priest.HasAura(63853));
}
