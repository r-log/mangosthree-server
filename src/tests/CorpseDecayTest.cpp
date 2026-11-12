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

/// How long a creature corpse stays as its loot is taken: a corpse with loot keeps its delay, an
/// emptied one keeps at most 120 s, one that can still be skinned gets its delay again from the
/// looting, a skinned one goes at once, and the respawn time never moves.
#include "TestHarness.h"
#include "CorpseDecay.h"
#include "Creature.h"

namespace
{
    uint32 const NORMAL_DELAY_MS = 300 * IN_MILLISECONDS;
    uint32 const RESPAWN_IN_AN_HOUR_MS = HOUR * IN_MILLISECONDS;
    float const RETAIL_RATE = 0.0f;

    /// A normal mob's corpse with `remainingMs` left and its respawn an hour away.
    CorpseLootState NormalCorpse(uint32 remainingMs)
    {
        CorpseLootState corpse;
        corpse.remainingMs = remainingMs;
        corpse.corpseDelayMs = NORMAL_DELAY_MS;
        corpse.untilRespawnMs = RESPAWN_IN_AN_HOUR_MS;
        return corpse;
    }

    /// A creature with its update fields allocated, never in a world, tapped by a player.
    class CorpseProbe : public Creature
    {
        public:
            CorpseProbe() : Creature(CREATURE_SUBTYPE_GENERIC)
            {
                _InitValues();
                m_lootRecipientGuid = ObjectGuid(HIGHGUID_PLAYER, 1u);
            }

            /// Unit::CleanupsBeforeDelete runs the in-world teardown only for an object whose
            /// update fields exist; this one was never in a world, so they go first.
            ~CorpseProbe()
            {
                delete[] m_uint32Values;
                m_uint32Values = NULL;
            }

            /// The state a death leaves: a corpse with its full delay, due to respawn in `respawnSecs`.
            void Die(uint32 corpseSecs, uint32 respawnSecs)
            {
                m_deathState = CORPSE;
                SetCorpseDelay(corpseSecs);
                m_corpseDecayTimer = corpseSecs * IN_MILLISECONDS;
                SetRespawnDelay(respawnSecs);
                SetRespawnTime(respawnSecs);
            }

            /// The state a corpse removal leaves.
            void RemoveTheCorpse()
            {
                m_deathState = DEAD;
                m_corpseDecayTimer = 0;
            }

            void Elapse(uint32 ms) { m_corpseDecayTimer -= ms; }

            uint32 CorpseDecayMs() const { return m_corpseDecayTimer; }
    };
}

TEST(CorpseDecay_ACorpseWithLootKeepsItsFullDelayAtDeath)
{
    CorpseLootState corpse = NormalCorpse(NORMAL_DELAY_MS);
    corpse.lootable = true;
    CHECK_EQ(CorpseDecayAfterLoot(corpse, RETAIL_RATE), NORMAL_DELAY_MS);

    corpse.skinnable = true;
    CHECK_EQ(CorpseDecayAfterLoot(corpse, RETAIL_RATE), NORMAL_DELAY_MS);
}

TEST(CorpseDecay_AnEmptiedCorpseKeepsAtMost120Seconds)
{
    CHECK_EQ(CorpseDecayAfterLoot(NormalCorpse(290 * IN_MILLISECONDS), RETAIL_RATE), uint32(120 * IN_MILLISECONDS));
    CHECK_EQ(CorpseDecayAfterLoot(NormalCorpse(50 * IN_MILLISECONDS), RETAIL_RATE), uint32(50 * IN_MILLISECONDS));

    CorpseLootState rare = NormalCorpse(890 * IN_MILLISECONDS);
    rare.corpseDelayMs = 900 * IN_MILLISECONDS;
    CHECK_EQ(CorpseDecayAfterLoot(rare, RETAIL_RATE), uint32(120 * IN_MILLISECONDS));
}

TEST(CorpseDecay_ALootedCorpseThatCanBeSkinnedGetsItsFullDelayAgain)
{
    CorpseLootState corpse = NormalCorpse(50 * IN_MILLISECONDS);
    corpse.skinnable = true;
    CHECK_EQ(CorpseDecayAfterLoot(corpse, RETAIL_RATE), NORMAL_DELAY_MS);

    corpse.remainingMs = 290 * IN_MILLISECONDS;
    CHECK_EQ(CorpseDecayAfterLoot(corpse, RETAIL_RATE), NORMAL_DELAY_MS);
}

TEST(CorpseDecay_ASkinnableCorpseIsExtendedNoFurtherThanItsRespawn)
{
    CorpseLootState corpse = NormalCorpse(170 * IN_MILLISECONDS);
    corpse.corpseDelayMs = 270 * IN_MILLISECONDS;
    corpse.untilRespawnMs = 200 * IN_MILLISECONDS;
    corpse.skinnable = true;
    CHECK_EQ(CorpseDecayAfterLoot(corpse, RETAIL_RATE), uint32(200 * IN_MILLISECONDS));

    corpse.untilRespawnMs = 169 * IN_MILLISECONDS;
    CHECK_EQ(CorpseDecayAfterLoot(corpse, RETAIL_RATE), uint32(170 * IN_MILLISECONDS));
}

TEST(CorpseDecay_AGatheredCorpseGoesAtOnce)
{
    CorpseLootState corpse = NormalCorpse(250 * IN_MILLISECONDS);
    corpse.gathered = true;
    CHECK_EQ(CorpseDecayAfterLoot(corpse, RETAIL_RATE), uint32(0));

    corpse.ignoreLootedRate = true;
    CHECK_EQ(CorpseDecayAfterLoot(corpse, 0.5f), uint32(0));
}

TEST(CorpseDecay_ANonZeroRateCapsAtTheDelayTimesTheRate)
{
    CHECK_EQ(CorpseDecayAfterLoot(NormalCorpse(290 * IN_MILLISECONDS), 0.5f), uint32(150 * IN_MILLISECONDS));
    CHECK_EQ(CorpseDecayAfterLoot(NormalCorpse(50 * IN_MILLISECONDS), 0.5f), uint32(50 * IN_MILLISECONDS));
}

TEST(CorpseDecay_IgnoringTheLootedRateKeepsAnEmptiedCorpsesTime)
{
    CorpseLootState corpse = NormalCorpse(290 * IN_MILLISECONDS);
    corpse.ignoreLootedRate = true;
    CHECK_EQ(CorpseDecayAfterLoot(corpse, RETAIL_RATE), uint32(290 * IN_MILLISECONDS));
    CHECK_EQ(CorpseDecayAfterLoot(corpse, 0.5f), uint32(290 * IN_MILLISECONDS));
}

TEST(CorpseDecay_ACorpseWithNothingToLootFromTheStartKeeps120Seconds)
{
    CHECK_EQ(CorpseDecayAfterLoot(NormalCorpse(NORMAL_DELAY_MS), RETAIL_RATE), uint32(120 * IN_MILLISECONDS));
}

TEST(CorpseDecay_LootingAnEmptiedCorpseLeavesTheRespawnTimeAsSetAtDeath)
{
    CorpseProbe creature;
    creature.Die(300, 600);
    time_t const respawnAtDeath = creature.GetRespawnTime();
    creature.Elapse(10 * IN_MILLISECONDS);

    creature.AllLootRemovedFromCorpse();

    CHECK_EQ(creature.CorpseDecayMs(), uint32(120 * IN_MILLISECONDS));
    CHECK_EQ(creature.GetRespawnTime(), respawnAtDeath);
}

TEST(CorpseDecay_TakingTheSkinsRemovesTheCorpseAndLeavesTheRespawnTime)
{
    CorpseProbe creature;
    creature.Die(300, 600);
    time_t const respawnAtDeath = creature.GetRespawnTime();
    creature.lootForSkin = true;
    creature.loot.loot_type = LOOT_SKINNING;

    creature.AllLootRemovedFromCorpse();

    CHECK_EQ(creature.CorpseDecayMs(), uint32(0));
    CHECK_EQ(creature.GetRespawnTime(), respawnAtDeath);
}

TEST(CorpseDecay_ACorpseStillHoldingLootKeepsItsDelayAtDeath)
{
    CorpseProbe creature;
    creature.Die(300, 600);
    creature.SetFlag(UNIT_DYNAMIC_FLAGS, UNIT_DYNFLAG_LOOTABLE);

    creature.AllLootRemovedFromCorpse();

    CHECK_EQ(creature.CorpseDecayMs(), NORMAL_DELAY_MS);
}

TEST(CorpseDecay_ACreatureSkinnedInItsLastLifeKeepsItsCorpseAtTheNextDeath)
{
    CorpseProbe skinOnly;
    skinOnly.loot.loot_type = LOOT_SKINNING;
    skinOnly.Die(300, 600);
    skinOnly.SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_SKINNABLE);
    skinOnly.AllLootRemovedFromCorpse();
    CHECK_EQ(skinOnly.CorpseDecayMs(), NORMAL_DELAY_MS);

    CorpseProbe empty;
    empty.loot.loot_type = LOOT_SKINNING;
    empty.Die(300, 600);
    empty.AllLootRemovedFromCorpse();
    CHECK_EQ(empty.CorpseDecayMs(), uint32(120 * IN_MILLISECONDS));
}

TEST(CorpseDecay_ARemovedCorpseIsLeftAlone)
{
    CorpseProbe creature;
    creature.Die(300, 600);
    creature.SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_SKINNABLE);
    creature.RemoveTheCorpse();

    creature.AllLootRemovedFromCorpse();

    CHECK_EQ(creature.CorpseDecayMs(), uint32(0));
}
