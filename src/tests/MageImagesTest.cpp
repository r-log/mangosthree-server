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

/// Tests of the images of the mage's Mirror Image (spells/classes/mage/Images.cpp): their table rows, which images are
/// a mage's own, the summon the glyph and the primary tree choose, the glyph slots' read, the spell power an image
/// takes, the spell it casts next, the units it may strike, the casts it interrupts and the unit it casts at, Mirror
/// Image's and Clone Me!'s rows at the dummy-effect hook and Initialize Images' at the target hook with the images'
/// creature AI, Clone Me!'s weapons on a player's image, and what a script reads of a player's specialization
/// (Spells::ActiveSpecialization) and of a unit's crowd control (Spells::HeldByControlDamageBreaks). The units are
/// bare creatures (update fields allocated; no map, template, aura or AI) and a player on a session with no socket.

#include "TestHarness.h"
#include "spells/classes/mage/Mage.h"
#include "spells/handlers/SpellHandlerRegistry.h"
#include "spells/SpellScriptQueries.h"
#include "ai/CreatureAI.h"
#include "Creature.h"
#include "Player.h"
#include "WorldSession.h"
#include "Auth/BigNumber.h"
#include "DBCStructure.h"

#include <list>
#include <set>

namespace
{
    // A creature with its update fields allocated and no world, map, template or AI.
    class ImageTestCreature : public Creature
    {
        public:
            ImageTestCreature() : Creature(CREATURE_SUBTYPE_GENERIC)
            {
                _InitValues();
            }

            ~ImageTestCreature()
            {
                delete[] m_uint32Values;
                m_uint32Values = NULL;
            }

            // Gives the creature a guid of its entry and `low`.
            void TakeGuid(uint32 low)
            {
                Object::_Create(low, GetEntry(), HIGHGUID_UNIT);
            }
    };

    // A player with its update fields allocated, on a session with no socket, in no world.
    class ImageTestPlayer : public Player
    {
        public:
            explicit ImageTestPlayer(WorldSession& session) : Player(&session)
            {
                Object::_Create(1, 0, HIGHGUID_PLAYER);
            }

            ~ImageTestPlayer()
            {
                delete[] m_uint32Values;
                m_uint32Values = NULL;
            }
    };

    struct SpellRowStorage
    {
        alignas(SpellEntry) unsigned char bytes[sizeof(SpellEntry)];
    };

    // A Spell.dbc row with only its id, which the context names; it is not in the spell store.
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
}

TEST(MageImages_TheRowsNameTheCreaturesTheirSummonsAndTheirSpells)
{
    // The images without the glyph cast Frostbolt and Fire Blast; the Arcane images Arcane Blast; the Fire images
    // Fireball.
    Mage::ImageRow const* plain = Mage::ImageRowFor(31216);
    REQUIRE(plain != NULL);
    CHECK_EQ(plain->creature, uint32(31216));
    CHECK_EQ(plain->summonSpell, uint32(58832));
    CHECK_EQ(plain->spell, uint32(59638));
    CHECK_EQ(plain->fireBlast, uint32(59637));
    Mage::ImageRow const* arcane = Mage::ImageRowFor(47243);
    REQUIRE(arcane != NULL);
    CHECK_EQ(arcane->summonSpell, uint32(88091));
    CHECK_EQ(arcane->spell, uint32(88084));
    CHECK_EQ(arcane->fireBlast, uint32(0));
    Mage::ImageRow const* fire = Mage::ImageRowFor(47244);
    REQUIRE(fire != NULL);
    CHECK_EQ(fire->summonSpell, uint32(88092));
    CHECK_EQ(fire->spell, uint32(88082));
    CHECK_EQ(fire->fireBlast, uint32(0));

    // The Flaming Orb, the Camel, the mage's Water Elemental and no creature are no image.
    for (uint32 entry : { 44214u, 50249u, 510u, 0u })
    {
        CHECK(Mage::ImageRowFor(entry) == NULL);
    }
}

TEST(MageImages_AnImageIsHisWhenItIsOfTheTableAndHeOwnsIt)
{
    ObjectGuid const mage(HIGHGUID_PLAYER, uint32(1));
    ObjectGuid const other(HIGHGUID_PLAYER, uint32(2));

    // Each image of the table, his; another mage's or no one's is not.
    for (uint32 entry : { 31216u, 47243u, 47244u })
    {
        CHECK(Mage::IsOwnImage(entry, mage, mage));
        CHECK(!Mage::IsOwnImage(entry, other, mage));
        CHECK(!Mage::IsOwnImage(entry, ObjectGuid(), mage));
    }

    // His Water Elemental and no creature are no image; a caster of no guid owns none.
    CHECK(!Mage::IsOwnImage(510, mage, mage));
    CHECK(!Mage::IsOwnImage(0, mage, mage));
    CHECK(!Mage::IsOwnImage(31216, ObjectGuid(), ObjectGuid()));
}

TEST(MageImages_TheGlyphAndThePrimaryTreeChooseTheSummon)
{
    // Without the glyph every mage summons the images without it, whatever his tree.
    for (uint32 tree : { 799u, 851u, 823u, 0u })
    {
        CHECK_EQ(Mage::ImageSummonSpell(false, tree), uint32(58832));
    }

    // With it, an Arcane mage the Arcane images, a Fire mage the Fire images; a Frost mage and a mage with no tree
    // chosen the images without it.
    CHECK_EQ(Mage::ImageSummonSpell(true, 799), uint32(88091));
    CHECK_EQ(Mage::ImageSummonSpell(true, 851), uint32(88092));
    CHECK_EQ(Mage::ImageSummonSpell(true, 823), uint32(58832));
    CHECK_EQ(Mage::ImageSummonSpell(true, 0), uint32(58832));
}

TEST(MageImages_AGlyphIsHeldInAnyOfTheSlots)
{
    uint32 glyphs[MAX_GLYPH_SLOT_INDEX] = {};

    // Empty slots hold no glyph, and asking for 0 never finds an empty slot.
    CHECK(!Mage::HoldsGlyph(glyphs, MAX_GLYPH_SLOT_INDEX, 699));
    CHECK(!Mage::HoldsGlyph(glyphs, MAX_GLYPH_SLOT_INDEX, 0));

    // In the first slot, in the last, among others.
    glyphs[0] = 699;
    CHECK(Mage::HoldsGlyph(glyphs, MAX_GLYPH_SLOT_INDEX, 699));
    glyphs[0] = 0;
    glyphs[MAX_GLYPH_SLOT_INDEX - 1] = 699;
    CHECK(Mage::HoldsGlyph(glyphs, MAX_GLYPH_SLOT_INDEX, 699));
    glyphs[3] = 312;
    CHECK(Mage::HoldsGlyph(glyphs, MAX_GLYPH_SLOT_INDEX, 312));

    // A slot past the count is not read, and no slots hold nothing.
    CHECK(!Mage::HoldsGlyph(glyphs, MAX_GLYPH_SLOT_INDEX - 1, 699));
    CHECK(!Mage::HoldsGlyph(glyphs, 0, 312));
    CHECK(!Mage::HoldsGlyph(NULL, MAX_GLYPH_SLOT_INDEX, 699));
}

TEST(MageImages_AnImageTakesThreeQuartersOfTheMagesSpellPower)
{
    CHECK_EQ(Mage::ImageBonusDamage(0), int32(0));
    CHECK_EQ(Mage::ImageBonusDamage(3000), int32(2250));
    CHECK_EQ(Mage::ImageBonusDamage(3001), int32(2250));
    CHECK_EQ(Mage::ImageBonusDamage(10197), int32(7647));
    CHECK_EQ(Mage::ImageBonusDamage(4), int32(3));

    // A spell power below 0 gives none.
    CHECK_EQ(Mage::ImageBonusDamage(-500), int32(0));
}

TEST(MageImages_TheImagesWithoutTheGlyphCastFireBlastWhenItsCooldownIsOver)
{
    Mage::ImageRow const* plain = Mage::ImageRowFor(31216);
    REQUIRE(plain != NULL);

    // Its cooldown over, or never started: Fire Blast; while it runs: Frostbolt.
    CHECK_EQ(Mage::ImageSpellToCast(*plain, 0), uint32(59637));
    CHECK_EQ(Mage::ImageSpellToCast(*plain, -50), uint32(59637));
    CHECK_EQ(Mage::ImageSpellToCast(*plain, 1), uint32(59638));
    CHECK_EQ(Mage::ImageSpellToCast(*plain, 6000), uint32(59638));

    // The glyph's images have no Fire Blast: always their one spell.
    for (uint32 entry : { 47243u, 47244u })
    {
        Mage::ImageRow const* row = Mage::ImageRowFor(entry);
        REQUIRE(row != NULL);
        CHECK_EQ(Mage::ImageSpellToCast(*row, 0), row->spell);
        CHECK_EQ(Mage::ImageSpellToCast(*row, 6000), row->spell);
    }
}

TEST(MageImages_AnImageStrikesAnAttackableUnitNotFriendlyAndNotUnderABreakableControl)
{
    CHECK(Mage::ImageMayStrike(true, false, false));

    // Not attackable (dead, or not selectable for attack), friendly, or held by an aura any damage removes.
    CHECK(!Mage::ImageMayStrike(false, false, false));
    CHECK(!Mage::ImageMayStrike(true, true, false));
    CHECK(!Mage::ImageMayStrike(true, false, true));
    CHECK(!Mage::ImageMayStrike(false, true, true));
}

TEST(MageImages_AnImageInterruptsACastAtAUnitABreakableControlNowHolds)
{
    // A cast at a unit that a crowd control any damage breaks now holds (a Polymorph put on it mid-cast) is
    // interrupted.
    CHECK(Mage::ImageStopsCast(true, true));

    // A cast at a free unit goes on; a cast at no unit, or at one gone from the image's map, is left alone.
    CHECK(!Mage::ImageStopsCast(true, false));
    CHECK(!Mage::ImageStopsCast(false, true));
    CHECK(!Mage::ImageStopsCast(false, false));

    // The rule of the start check: at a unit it may attack and that is not friendly, the image interrupts a cast
    // exactly when it would not start one.
    for (bool held : { false, true })
    {
        CHECK(Mage::ImageStopsCast(true, held) == !Mage::ImageMayStrike(true, false, held));
    }
}

TEST(MageImages_AUnitWithNoAuraIsHeldByNoControl)
{
    // The query reads the unit's confuse and stun auras; SpellTablesTest holds the rule it applies to each one.
    ImageTestCreature camel;
    camel.SetEntry(50249);
    CHECK(!Spells::HeldByControlDamageBreaks(camel));
}

TEST(MageImages_TheTargetIsTheVictimThenTheSelectionThenTheAttacker)
{
    ImageTestCreature victim;
    ImageTestCreature selection;
    ImageTestCreature attacker;
    std::set<Unit const*> refused;
    auto const mayStrike = [&refused](Unit& unit) { return refused.count(&unit) == 0; };

    // The three accepted: the victim; then the selection, then the attacker, as each one before is refused.
    CHECK(Mage::ImageTarget(&victim, &selection, &attacker, mayStrike) == &victim);
    refused.insert(&victim);
    CHECK(Mage::ImageTarget(&victim, &selection, &attacker, mayStrike) == &selection);
    refused.insert(&selection);
    CHECK(Mage::ImageTarget(&victim, &selection, &attacker, mayStrike) == &attacker);
    refused.insert(&attacker);
    CHECK(Mage::ImageTarget(&victim, &selection, &attacker, mayStrike) == NULL);

    // A missing unit is skipped: no victim gives the selection, and none of the three gives none.
    refused.clear();
    CHECK(Mage::ImageTarget(NULL, &selection, &attacker, mayStrike) == &selection);
    CHECK(Mage::ImageTarget(NULL, NULL, &attacker, mayStrike) == &attacker);
    CHECK(Mage::ImageTarget(NULL, NULL, NULL, mayStrike) == NULL);
}

TEST(MageImages_AScriptReadsThePlayersPrimaryTreeAndGlyphs)
{
    WorldSession session(1, "images", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    ImageTestPlayer mage(session);
    uint8 const spec = mage.GetTalentMgr().ActiveSpec();

    // No tree chosen and no glyph: all 0.
    Spells::SpecializationView none = Spells::ActiveSpecialization(mage);
    CHECK_EQ(none.primaryTree, uint32(0));
    for (uint32 slot = 0; slot < MAX_GLYPH_SLOT_INDEX; ++slot)
    {
        CHECK_EQ(none.glyphs[slot], uint32(0));
    }

    // The Arcane tree and Glyph of Mirror Image in the fourth slot.
    mage.GetTalentMgr().SetPrimaryTree(spec, 799);
    mage.SetGlyph(3, 699);
    Spells::SpecializationView arcane = Spells::ActiveSpecialization(mage);
    CHECK_EQ(arcane.primaryTree, uint32(799));
    CHECK_EQ(arcane.glyphs[3], uint32(699));
    CHECK(Mage::HoldsGlyph(arcane.glyphs, MAX_GLYPH_SLOT_INDEX, 699));
    CHECK_EQ(Mage::ImageSummonSpell(true, arcane.primaryTree), uint32(88091));

    // A creature has no specialization.
    ImageTestCreature creature;
    Spells::SpecializationView creatures = Spells::ActiveSpecialization(creature);
    CHECK_EQ(creatures.primaryTree, uint32(0));
    CHECK(!Mage::HoldsGlyph(creatures.glyphs, MAX_GLYPH_SLOT_INDEX, 699));
}

TEST(MageImages_TheRowsSitOnTheDummyEffectAndTargetHooksAndTheImagesHaveTheirAI)
{
    // Mirror Image and Clone Me! at the dummy-effect hook and Initialize Images at the target hook: three rows, three
    // keys, each the game's body too.
    SpellHandlerRegistry registry;
    CHECK_EQ(Mage::RegisterImages(registry), uint32(3));
    CHECK_EQ(registry.Count(), std::size_t(3));
    CHECK_EQ(registry.CountDefaults(), std::size_t(0));
    CHECK(registry.Find<DummyEffectHook>(55342) != NULL);
    CHECK(registry.Find<DummyEffectHook>(45204) != NULL);
    CHECK(registry.Find<SelectTargetsHook>(58836) != NULL);
    CHECK(registry.Find<DummyEffectHook>(55342) != registry.Find<DummyEffectHook>(45204));
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    CHECK(game.Find<DummyEffectHook>(55342) == registry.Find<DummyEffectHook>(55342));
    CHECK(game.Find<DummyEffectHook>(45204) == registry.Find<DummyEffectHook>(45204));
    CHECK(game.Find<SelectTargetsHook>(58836) == registry.Find<SelectTargetsHook>(58836));
    for (uint32 spellId : { 58832u, 88091u, 88092u, 58836u, 45206u })
    {
        CHECK(game.Find<DummyEffectHook>(spellId) == NULL);
    }
    for (uint32 spellId : { 55342u, 45204u, 58832u, 88091u, 88092u })
    {
        CHECK(game.Find<SelectTargetsHook>(spellId) == NULL);
    }

    // The images' AI is chosen by its permit for the three image creatures alone, and registering again keeps the one
    // factory.
    SelectableAI const* factory = CreatureAIRepository::Instance().GetRegistryItem("MirrorImageAI");
    REQUIRE(factory != NULL);
    for (uint32 entry : { 31216u, 47243u, 47244u })
    {
        ImageTestCreature image;
        image.SetEntry(entry);
        CHECK_EQ(factory->Permit(&image), int(PERMIT_BASE_SPECIAL));
    }
    for (uint32 entry : { 44214u, 50249u })
    {
        ImageTestCreature other;
        other.SetEntry(entry);
        CHECK_EQ(factory->Permit(&other), int(PERMIT_BASE_NO));
    }
    Mage::RegisterImages(registry);
    CHECK(CreatureAIRepository::Instance().GetRegistryItem("MirrorImageAI") == factory);

    // An image with no owner on its map does nothing: it neither starts nor casts.
    ImageTestCreature image;
    image.SetEntry(31216);
    CreatureAI* ai = factory->Create(&image);
    REQUIRE(ai != NULL);
    ai->UpdateAI(100);
    ai->UpdateAI(30000);
    CHECK(image.IsAlive());
    CHECK(!image.IsNonMeleeSpellCasted(false));
    delete ai;
}

TEST(MageImages_MirrorImagesScriptEndsItsEffectsAndCloneMeEndsOnlyAtTheMagesImage)
{
    SpellHandlerRegistry registry;
    Mage::RegisterImages(registry);
    ImageTestCreature caster;
    ImageTestCreature image;
    caster.SetEntry(50249);
    caster.TakeGuid(7);
    image.SetEntry(31216);
    SpellRowStorage row;
    int32 value = 0;
    int32 word = 0;

    // Mirror Image's effects other than its dummy one end there and cast nothing.
    DummyEffectContext threat(caster, &caster, NULL, NULL, SpellRow(row, 55342), EFFECT_INDEX_0, value, ObjectGuid(),
                              NULL, word);
    CHECK(registry.Dispatch<DummyEffectHook>(55342, threat).IsReturn());
    CHECK(!caster.IsNonMeleeSpellCasted(false));

    // Clone Me!'s auras and an effect at a unit that is no image go on to their handler.
    DummyEffectContext look(caster, &image, NULL, NULL, SpellRow(row, 45204), EFFECT_INDEX_0, value, ObjectGuid(),
                            &image, word);
    CHECK(registry.Dispatch<DummyEffectHook>(45204, look).IsContinue());
    ImageTestCreature camel;
    camel.SetEntry(50249);
    DummyEffectContext notImage(caster, &camel, NULL, NULL, SpellRow(row, 45204), EFFECT_INDEX_2, value, ObjectGuid(),
                                &camel, word);
    CHECK(registry.Dispatch<DummyEffectHook>(45204, notImage).IsContinue());

    // An image of another owner goes on too; the caster's own image ends the effect. A caster that is no player has no
    // visible weapons: the image's virtual item slots stay as they were.
    image.SetOwnerGuid(ObjectGuid(HIGHGUID_PLAYER, uint32(4711)));
    DummyEffectContext other(caster, &image, NULL, NULL, SpellRow(row, 45204), EFFECT_INDEX_2, value, ObjectGuid(),
                             &image, word);
    CHECK(registry.Dispatch<DummyEffectHook>(45204, other).IsContinue());
    image.SetUInt32Value(UNIT_VIRTUAL_ITEM_SLOT_ID, 28782);
    image.SetOwnerGuid(caster.GetObjectGuid());
    DummyEffectContext own(caster, &image, NULL, NULL, SpellRow(row, 45204), EFFECT_INDEX_2, value, ObjectGuid(),
                           &image, word);
    CHECK(registry.Dispatch<DummyEffectHook>(45204, own).IsReturn());
    CHECK_EQ(image.GetUInt32Value(UNIT_VIRTUAL_ITEM_SLOT_ID), uint32(28782));
}

TEST(MageImages_CloneMeGivesTheMagesImageHisWeaponsAndNothingComesBackAtHim)
{
    SpellHandlerRegistry registry;
    Mage::RegisterImages(registry);
    WorldSession session(1, "images", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    ImageTestPlayer mage(session);
    mage.SetUInt32Value(PLAYER_VISIBLE_ITEM_16_ENTRYID, 71086);
    mage.SetUInt32Value(PLAYER_VISIBLE_ITEM_17_ENTRYID, 71154);
    mage.SetUInt32Value(PLAYER_VISIBLE_ITEM_18_ENTRYID, 71153);
    ImageTestCreature image;
    image.SetEntry(31216);
    image.SetOwnerGuid(mage.GetObjectGuid());
    SpellRowStorage row;
    int32 value = 0;
    int32 word = 0;

    // His image takes his main hand, off hand and ranged items in its three virtual item slots, and the effect ends
    // there: the image casts no Copy Off-hand Weapon (45206) at him, so he holds none of its aura (45205).
    DummyEffectContext own(mage, &image, NULL, NULL, SpellRow(row, 45204), EFFECT_INDEX_2, value, ObjectGuid(),
                           &image, word);
    CHECK(registry.Dispatch<DummyEffectHook>(45204, own).IsReturn());
    CHECK_EQ(image.GetUInt32Value(UNIT_VIRTUAL_ITEM_SLOT_ID + 0), uint32(71086));
    CHECK_EQ(image.GetUInt32Value(UNIT_VIRTUAL_ITEM_SLOT_ID + 1), uint32(71154));
    CHECK_EQ(image.GetUInt32Value(UNIT_VIRTUAL_ITEM_SLOT_ID + 2), uint32(71153));
    CHECK(!image.IsNonMeleeSpellCasted(false));
    CHECK(!mage.HasAura(45205));
    CHECK(!mage.HasAura(45206));

    // Another mage's image takes nothing of his and goes on to the effect's handler.
    ImageTestCreature other;
    other.SetEntry(31216);
    other.SetOwnerGuid(ObjectGuid(HIGHGUID_PLAYER, uint32(4711)));
    DummyEffectContext others(mage, &other, NULL, NULL, SpellRow(row, 45204), EFFECT_INDEX_2, value, ObjectGuid(),
                              &other, word);
    CHECK(registry.Dispatch<DummyEffectHook>(45204, others).IsContinue());
    for (uint32 slot = 0; slot < 3; ++slot)
    {
        CHECK_EQ(other.GetUInt32Value(UNIT_VIRTUAL_ITEM_SLOT_ID + slot), uint32(0));
    }
}

TEST(MageImages_InitializeImagesKeepsTheCastersOwnImagesAlone)
{
    SpellHandlerRegistry registry;
    Mage::RegisterImages(registry);
    ImageTestCreature caster;
    caster.SetEntry(50249);
    caster.TakeGuid(7);
    ImageTestCreature plain;
    plain.SetEntry(31216);
    plain.SetOwnerGuid(caster.GetObjectGuid());
    ImageTestCreature fire;
    fire.SetEntry(47244);
    fire.SetOwnerGuid(caster.GetObjectGuid());
    ImageTestCreature others;
    others.SetEntry(31216);
    others.SetOwnerGuid(ObjectGuid(HIGHGUID_PLAYER, uint32(4711)));
    ImageTestCreature wild;
    wild.SetEntry(47243);
    ImageTestCreature elemental;
    elemental.SetEntry(510);
    elemental.SetOwnerGuid(caster.GetObjectGuid());
    SpellRowStorage row;
    uint32 maxTargets = 0;

    // The area's units: his two images stay, in their order; another mage's image, an image of no owner and his
    // creature that is no image go. The cut after the hook is left as it was.
    std::list<Unit*> area = { &plain, &others, &wild, &elemental, &fire };
    SelectTargetsContext renamed(caster, SpellRow(row, 58836), EFFECT_INDEX_0, TARGET_AREAEFFECT_INSTANT, area,
                                 maxTargets);
    CHECK(registry.Dispatch<SelectTargetsHook>(58836, renamed).IsContinue());
    REQUIRE(area.size() == std::size_t(2));
    CHECK(area.front() == &plain);
    CHECK(area.back() == &fire);
    CHECK_EQ(maxTargets, uint32(0));

    // Its other effect, the break of targeting at the caster, keeps its list.
    std::list<Unit*> self = { &caster };
    SelectTargetsContext deselect(caster, SpellRow(row, 58836), EFFECT_INDEX_1, TARGET_NONE, self, maxTargets);
    CHECK(registry.Dispatch<SelectTargetsHook>(58836, deselect).IsContinue());
    REQUIRE(self.size() == std::size_t(1));
    CHECK(self.front() == &caster);
}
