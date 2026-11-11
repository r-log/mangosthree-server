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
#include "Group.h"
#include "Player.h"
#include "WorldSession.h"
#include "spells/handlers/SpellEffectDummyHandlers.h"
#include "spells/handlers/SpellHandlerRegistry.h"

#include <set>

/**
 * @file
 * @brief The raid buffs whose dummy effect casts a single-target or a raid-wide aura: the table of the seven
 * buffs, the dummy-effect site of each class family that reaches the shared handler, and the rule that picks the
 * raid-wide aura for a caster in a group.
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

    /// A raid buff's ids and level as Spell.dbc, SpellEffect.dbc and SpellLevels.dbc of build 15595 hold them.
    RaidBuffSpells const EXPECTED[] =
    {
        { 21562, 79104, 79105, 14 },    // Power Word: Fortitude
        { 27683, 79106, 79107, 52 },    // Shadow Protection
        { 1126,  79060, 79061, 30 },    // Mark of the Wild
        { 1459,  79057, 79058, 58 },    // Arcane Brilliance
        { 61316, 79039, 79038, 80 },    // Dalaran Brilliance
        { 19740, 79101, 79102, 56 },    // Blessing of Might
        { 20217, 79062, 79063, 22 },    // Blessing of Kings
    };

    /// The number of the four raid buff sites holding `spellId`.
    uint32 RaidBuffSitesHolding(SpellHandlerRegistry const& registry, uint32 spellId)
    {
        return uint32(registry.Find<SpellEffectDummyPriestSite>(spellId) != NULL) +
               uint32(registry.Find<SpellEffectDummyDruidSite>(spellId) != NULL) +
               uint32(registry.Find<SpellEffectDummyMageSite>(spellId) != NULL) +
               uint32(registry.Find<SpellEffectDummyPaladinSite>(spellId) != NULL);
    }
}

TEST(RaidBuff_TheTableHoldsEachBuffsSingleAndRaidAuraAndLevel)
{
    std::array<RaidBuffSpells, 7> const& table = RaidBuffTable();
    CHECK_EQ(table.size(), std::size_t(7));

    std::set<uint32> casts;
    for (std::size_t i = 0; i < table.size(); ++i)
    {
        RaidBuffSpells const& expected = EXPECTED[i];
        CHECK_EQ(table[i].castSpellId, expected.castSpellId);
        CHECK_EQ(table[i].singleSpellId, expected.singleSpellId);
        CHECK_EQ(table[i].raidSpellId, expected.raidSpellId);
        CHECK_EQ(table[i].level, expected.level);

        RaidBuffSpells const* found = FindRaidBuff(expected.castSpellId);
        CHECK(found == &table[i]);
        casts.insert(table[i].castSpellId);
    }
    CHECK_EQ(casts.size(), std::size_t(7));

    // Dalaran Brilliance's raid-wide aura is 79038; 79040, its id + 1, is Trampoline Bounce, a knock-back.
    RaidBuffSpells const* dalaran = FindRaidBuff(61316);
    CHECK(dalaran != NULL);
    CHECK_EQ(dalaran->singleSpellId, uint32(79039));
    CHECK_EQ(dalaran->raidSpellId, uint32(79038));
    for (RaidBuffSpells const& buff : table)
    {
        CHECK(buff.castSpellId != 79040);
        CHECK(buff.singleSpellId != 79040);
        CHECK(buff.raidSpellId != 79040);
    }

    // An aura id, the knock-back and the paladin's other dummy labels are no raid buff.
    for (uint32 spellId : { 0u, 79104u, 79105u, 79040u, 31789u, 37877u, 47540u })
    {
        CHECK(FindRaidBuff(spellId) == NULL);
    }
}

TEST(RaidBuff_EachClassFamilysDummySiteReachesTheRaidBuffHandler)
{
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();

    // Each buff is a key at its own family's site and at no other raid buff site or the generic site.
    typedef SpellHandler<SpellEffectDummyPriestSite>::Function PriestBody;
    PriestBody fortitude = game.Find<SpellEffectDummyPriestSite>(21562);
    CHECK(fortitude != NULL);
    CHECK(game.Find<SpellEffectDummyPriestSite>(27683) == fortitude);
    CHECK(game.Find<SpellEffectDummyDruidSite>(1126) != NULL);
    CHECK(game.Find<SpellEffectDummyMageSite>(1459) != NULL);
    CHECK(game.Find<SpellEffectDummyMageSite>(61316) == game.Find<SpellEffectDummyMageSite>(1459));
    CHECK(game.Find<SpellEffectDummyPaladinSite>(19740) != NULL);
    CHECK(game.Find<SpellEffectDummyPaladinSite>(20217) == game.Find<SpellEffectDummyPaladinSite>(19740));
    for (RaidBuffSpells const& buff : RaidBuffTable())
    {
        CHECK_EQ(RaidBuffSitesHolding(game, buff.castSpellId), uint32(1));
        CHECK(game.Find<SpellEffectDummyGenericSite>(buff.castSpellId) == NULL);
    }
    CHECK_EQ(game.CountAt(SpellEffectDummyPriestSite::Key), std::size_t(2));
    CHECK_EQ(game.CountAt(SpellEffectDummyDruidSite::Key), std::size_t(1));
    CHECK(game.FindDefault<SpellEffectDummyPriestSite>() == NULL);
    CHECK(game.FindDefault<SpellEffectDummyDruidSite>() == NULL);

    // With no unit target each site's handler ends the effect with nothing changed; an id with no row (Penance at
    // the priest site, Starfall at the druid site) is a miss, so the family's case goes on.
    alignas(16) static unsigned char objects[1][16];
    Spell* spell = reinterpret_cast<Spell*>(objects[0]);
    Unit* caster = NULL;
    Unit* target = NULL;

    SpellEffectDummyPriestContext priest(spell, caster, target);
    CHECK(game.Dispatch<SpellEffectDummyPriestSite>(21562, priest).IsReturn());
    CHECK(game.Dispatch<SpellEffectDummyPriestSite>(27683, priest).IsReturn());
    CHECK(game.Dispatch<SpellEffectDummyPriestSite>(47540, priest).IsMiss());

    SpellEffectDummyDruidContext druid(spell, caster, target);
    CHECK(game.Dispatch<SpellEffectDummyDruidSite>(1126, druid).IsReturn());
    CHECK(game.Dispatch<SpellEffectDummyDruidSite>(48505, druid).IsMiss());

    int32 damage = 79057;
    SpellEffectEntry const* effect = NULL;
    SpellEffectDummyMageContext mage(spell, caster, target, damage, effect);
    CHECK(game.Dispatch<SpellEffectDummyMageSite>(1459, mage).IsReturn());
    CHECK(game.Dispatch<SpellEffectDummyMageSite>(61316, mage).IsReturn());

    SpellEntry const* spellInfo = NULL;
    Spell::TargetList targets;
    SpellEffectDummyPaladinContext paladin(spell, caster, target, spellInfo, targets);
    CHECK(game.Dispatch<SpellEffectDummyPaladinSite>(19740, paladin).IsReturn());
    CHECK(game.Dispatch<SpellEffectDummyPaladinSite>(20217, paladin).IsReturn());

    CHECK(caster == NULL);
    CHECK(target == NULL);
    CHECK_EQ(damage, int32(79057));
    CHECK(targets.empty());
}

TEST(RaidBuff_AGroupedCasterCastsTheRaidWideAuraOnItselfOrAGroupMember)
{
    WorldSession casterSession(1, "caster", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    WorldSession mateSession(2, "mate", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    WorldSession strangerSession(3, "stranger", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    Group party;
    Group otherParty;
    {
        SessionPlayer caster(casterSession, 1);
        SessionPlayer mate(mateSession, 2);
        SessionPlayer stranger(strangerSession, 3);

        // Out of a group: a self-cast and a cast on another player both cast the single-target aura.
        CHECK(!CastsRaidWideBuff(&caster, &caster));
        CHECK(!CastsRaidWideBuff(&caster, &mate));

        // In a group: a self-cast casts the raid-wide aura, as does a cast on a member of the group; a cast on a
        // player outside it, grouped or not, casts the single-target aura.
        caster.SetGroup(&party, 0);
        mate.SetGroup(&party, 0);
        CHECK(CastsRaidWideBuff(&caster, &caster));
        CHECK(CastsRaidWideBuff(&caster, &mate));
        CHECK(CastsRaidWideBuff(&mate, &caster));
        CHECK(!CastsRaidWideBuff(&caster, &stranger));
        CHECK(!CastsRaidWideBuff(&stranger, &caster));
        stranger.SetGroup(&otherParty, 0);
        CHECK(!CastsRaidWideBuff(&caster, &stranger));
        CHECK(CastsRaidWideBuff(&stranger, &stranger));

        // A caster that is no player, or a target that belongs to no player, casts the single-target aura.
        CHECK(!CastsRaidWideBuff(NULL, &caster));
        CHECK(!CastsRaidWideBuff(&caster, NULL));

        stranger.SetGroup(NULL);
        mate.SetGroup(NULL);
        caster.SetGroup(NULL);
        CHECK(!CastsRaidWideBuff(&caster, &caster));
    }
}
