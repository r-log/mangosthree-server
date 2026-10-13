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

#include "spells/handlers/SpellEffectDummyHandlers.h"
#include "spells/handlers/SpellHandlerRegistry.h"
#include "WorldHandlers/Spell.h"
#include "WorldHandlers/GridMap.h"
#include "WorldHandlers/Map.h"
#include "Object/Creature.h"
#include "Object/CreatureAI.h"
#include "Object/Pet.h"
#include "Object/SpellMgr.h"
#include "Server/DBCStores.h"
#include "Utilities/Util.h"
#include "entities/player/Player.h"
#include <algorithm>
#include <ctime>
#include <iterator>
#include <random>
#include <vector>

/// 23074: Arcanite Dragonling: with a cast item, the caster casts Arcanite Dragonling (19804) on itself through the
/// item; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericArcaniteDragonling(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.m_CastItem)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.m_caster->CastSpell(ctx.m_caster, 19804, true, ctx.m_CastItem);
    return SpellHandlerOutcome<void>::Return();
}

/// 23075: Mithril Mechanical Dragonling: with a cast item, the caster casts Mithril Mechanical Dragonling (12749) on
/// itself through the item; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericMithrilMechanicalDragonling(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.m_CastItem)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.m_caster->CastSpell(ctx.m_caster, 12749, true, ctx.m_CastItem);
    return SpellHandlerOutcome<void>::Return();
}

/// 23076: Mechanical Dragonling: with a cast item, the caster casts Mechanical Dragonling (4073) on itself through the
/// item; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericMechanicalDragonling(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.m_CastItem)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.m_caster->CastSpell(ctx.m_caster, 4073, true, ctx.m_CastItem);
    return SpellHandlerOutcome<void>::Return();
}

/// 23133: Gnomish Battle Chicken: with a cast item, the caster casts Battle Chicken (13166) on itself through the item;
/// the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericGnomishBattleChicken(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.m_CastItem)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.m_caster->CastSpell(ctx.m_caster, 13166, true, ctx.m_CastItem);
    return SpellHandlerOutcome<void>::Return();
}

/// 23138: Gate of Shazzrah: the caster casts Gate of Shazzrah (23139) on the unit target, if any; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericGateOfShazzrah(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // Effect probably include a threat change, but it is unclear if fully
    // reset or just forced upon target for teleport (SMSG_HIGHEST_THREAT_UPDATE)

    // Gate of Shazzrah
    ctx.m_caster->CastSpell(ctx.unitTarget, 23139, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 23448: Transporter Arrival: the caster casts on itself, at random, Transporter Malfunction (23444) one time in six,
/// Evil Twin (23445) four times in six or Transporter Malfunction (36902) one time in six; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericTransporterArrival(SpellEffectDummyGenericContext& ctx)
{
    int32 r = irand(0, 119);
    if (r < 20)                             // Transporter Malfunction - 1/6 polymorph
    {
        ctx.m_caster->CastSpell(ctx.m_caster, 23444, true);
    }
    else if (r < 100)                       // Evil Twin               - 4/6 evil twin
    {
        ctx.m_caster->CastSpell(ctx.m_caster, 23445, true);
    }
    else                                    // Transporter Malfunction - 1/6 miss the target
    {
        ctx.m_caster->CastSpell(ctx.m_caster, 36902, true);
    }

    return SpellHandlerOutcome<void>::Return();
}

/// 23453: Gnomish Transporter: the caster casts on itself Gadgetzan Transporter (23441) or, half the time, Gadgetzan
/// Transporter Failure (23446); the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericGnomishTransporter(SpellEffectDummyGenericContext& ctx)
{
    if (roll_chance_i(50))                  // Gadgetzan Transporter         - success
    {
        ctx.m_caster->CastSpell(ctx.m_caster, 23441, true);
    }
    else                                    // Gadgetzan Transporter Failure - failure
    {
        ctx.m_caster->CastSpell(ctx.m_caster, 23446, true);
    }

    return SpellHandlerOutcome<void>::Return();
}

/// 23645: Hourglass Sand: the caster loses Brood Affliction: Bronze (23170); the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericHourglassSand(SpellEffectDummyGenericContext& ctx)
{
    ctx.m_caster->RemoveAurasDueToSpell(23170); // Brood Affliction: Bronze
    return SpellHandlerOutcome<void>::Return();
}

/// 23725: Gift of Life: the caster casts Gift of Life (23782 and 23783) on itself; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericGiftOfLife(SpellEffectDummyGenericContext& ctx)
{
    ctx.m_caster->CastSpell(ctx.m_caster, 23782, true);
    ctx.m_caster->CastSpell(ctx.m_caster, 23783, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 24930: Hallow's End Candy: the caster casts one of the four Hallow's End Candy forms (24924 to 24927) on itself at
/// random; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericHallowsEndCandy(SpellEffectDummyGenericContext& ctx)
{
    uint32 spell_id = 0;

    switch (urand(1, 4))
    {
        case 1: spell_id = 24924; break;    // Larger and Orange
        case 2: spell_id = 24925; break;    // Skeleton
        case 3: spell_id = 24926; break;    // Pirate
        case 4: spell_id = 24927; break;    // Ghost
    }

    ctx.m_caster->CastSpell(ctx.m_caster, spell_id, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 25860: Reindeer Transformation: a mounted caster is dismounted and casts on itself the reindeer its flight or run
/// speed calls for, Flying Reindeer (44827, 44825 or 44824) or Reindeer (25859 or 25858); the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericReindeerTransformation(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.m_caster->HasAuraType(SPELL_AURA_MOUNTED))
    {
        return SpellHandlerOutcome<void>::Return();
    }

    float flyspeed = ctx.m_caster->GetSpeedRate(MOVE_FLIGHT);
    float speed = ctx.m_caster->GetSpeedRate(MOVE_RUN);

    ctx.m_caster->RemoveSpellsCausingAura(SPELL_AURA_MOUNTED);

    // 5 different spells used depending on mounted speed and if mount can fly or not
    if (flyspeed >= 4.1f)
    {
        // Flying Reindeer
        ctx.m_caster->CastSpell(ctx.m_caster, 44827, true); // 310% flying Reindeer
    }
    else if (flyspeed >= 3.8f)
    {
        // Flying Reindeer
        ctx.m_caster->CastSpell(ctx.m_caster, 44825, true); // 280% flying Reindeer
    }
    else if (flyspeed >= 1.6f)
    {
        // Flying Reindeer
        ctx.m_caster->CastSpell(ctx.m_caster, 44824, true); // 60% flying Reindeer
    }
    else if (speed >= 2.0f)
    {
        // Reindeer
        ctx.m_caster->CastSpell(ctx.m_caster, 25859, true);  // 100% ground Reindeer
    }
    else
    {
        // Reindeer
        ctx.m_caster->CastSpell(ctx.m_caster, 25858, true);  // 60% ground Reindeer
    }

    return SpellHandlerOutcome<void>::Return();
}

/// 26074: Holiday Cheer: the client shows the effect, and the server does nothing; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericHolidayCheer(SpellEffectDummyGenericContext& ctx)
{
    // implemented at client side
    return SpellHandlerOutcome<void>::Return();
}

/// 28006: Arcane Cloaking: the caster casts Naxxramas Entry Flag Effect DND (29294) on a player target; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericArcaneCloaking(SpellEffectDummyGenericContext& ctx)
{
    if (ctx.unitTarget && ctx.unitTarget->GetTypeId() == TYPEID_PLAYER)
        // Naxxramas Entry Flag Effect DND
        ctx.m_caster->CastSpell(ctx.unitTarget, 29294, true);

    return SpellHandlerOutcome<void>::Return();
}

/// 29126: Cleansing Flames: a player target casts Create Flame of Darnassus (29099) on itself; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericCleansingFlames(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.unitTarget->CastSpell(ctx.unitTarget, 29099, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 29135, 29136, 29137, 29138, 29139: Cleansing Flames: a player target casts on itself the Create Flame of the label's
/// city (29102, 29130, 29101, 29132 or 29133: Ironforge, Orgrimmar, Stormwind, Thunder Bluff or The Undercity); the
/// effect ends
static SpellHandlerOutcome<void> EffectDummyGenericCleansingFlamesIronforge(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    uint32 spellIDs[] = {29102, 29130, 29101, 29132, 29133};
    ctx.unitTarget->CastSpell(ctx.unitTarget, spellIDs[ctx.m_spellInfo->ID - 29135], true);
    return SpellHandlerOutcome<void>::Return();
}

/// 29200: Purify Helboar Meat: a player caster casts on itself Summon Purified Helboar Meat (29277) or, half the time,
/// Summon Toxic Helboar Meat (29278); the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericPurifyHelboarMeat(SpellEffectDummyGenericContext& ctx)
{
    if (ctx.m_caster->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    uint32 spell_id = roll_chance_i(50)
                      ? 29277               // Summon Purified Helboar Meat
                      : 29278;              // Summon Toxic Helboar Meat

    ctx.m_caster->CastSpell(ctx.m_caster, spell_id, true, NULL);
    return SpellHandlerOutcome<void>::Return();
}

/// 29858: Soulshatter: the caster casts Soulshatter (32835) on a hostile creature target; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericSoulshatter(SpellEffectDummyGenericContext& ctx)
{
    if (ctx.unitTarget && ctx.unitTarget->GetTypeId() == TYPEID_UNIT && ctx.unitTarget->IsHostileTo(ctx.m_caster))
    {
        ctx.m_caster->CastSpell(ctx.unitTarget, 32835, true);
    }

    return SpellHandlerOutcome<void>::Return();
}

/// 29969: Summon Blizzard: the unit target, if any, casts Circular Blizzard (29952) on itself as from the caster; the
/// effect ends
static SpellHandlerOutcome<void> EffectDummyGenericSummonBlizzard(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.unitTarget->CastSpell(ctx.unitTarget, 29952, true, NULL, NULL, ctx.m_caster->GetObjectGuid());
    return SpellHandlerOutcome<void>::Return();
}

/// 29979: Massive Magnetic Pull: the caster casts Magnetic Pull (30010) on the unit target, if any; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericMassiveMagneticPull(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.m_caster->CastSpell(ctx.unitTarget, 30010, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 30004: Flame Wreath: the caster casts Flame Wreath (29946) on a player target; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericFlameWreath(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.m_caster->CastSpell(ctx.unitTarget, 29946, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 30458: Nigh Invulnerability: with a cast item, the caster casts on itself through the item Nigh-Invulnerability
/// (30456) 86 times in 100, otherwise Complete Vulnerability (30457); the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericNighInvulnerability(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.m_CastItem)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    if (roll_chance_i(86))                  // Nigh-Invulnerability   - success
    {
        ctx.m_caster->CastSpell(ctx.m_caster, 30456, true, ctx.m_CastItem);
    }
    else                                    // Complete Vulnerability - backfire in 14% casts
    {
        ctx.m_caster->CastSpell(ctx.m_caster, 30457, true, ctx.m_CastItem);
    }

    return SpellHandlerOutcome<void>::Return();
}

/// 30507: Poultryizer: with a cast item, the caster casts Poultryized! on the unit target through the item, 30501 80
/// times in 100, otherwise 30504; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericPoultryizer(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.m_CastItem)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    if (roll_chance_i(80))                  // Poultryized! - success
    {
        ctx.m_caster->CastSpell(ctx.unitTarget, 30501, true, ctx.m_CastItem);
    }
    else                                    // Poultryized! - backfire 20%
    {
        ctx.m_caster->CastSpell(ctx.unitTarget, 30504, true, ctx.m_CastItem);
    }

    return SpellHandlerOutcome<void>::Return();
}

/// 32027: Expedition Flare: the caster casts Summon Expedition Preserver (32029) or Summon Expedition Scout (32030) on
/// itself at random; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericExpeditionFlare(SpellEffectDummyGenericContext& ctx)
{
    // 32029 = Expedition Preserver | 32030 = Expedition Scout
    ctx.m_caster->CastSpell(ctx.m_caster, (urand(0, 1) ? 32029 : 32030), true);

    return SpellHandlerOutcome<void>::Return();
}

/// 32146: Liquid Fire: a player caster gets the kill credit of a creature target, which despawns; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericLiquidFire(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_UNIT || ctx.m_caster->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ((Player*)ctx.m_caster)->KilledMonsterCredit(ctx.unitTarget->GetEntry(), ctx.unitTarget->GetObjectGuid());
    ((Creature*)ctx.unitTarget)->ForcedDespawn();
    return SpellHandlerOutcome<void>::Return();
}

/// 32300: Focus Fire: the unit target, if any, casts Fiery Blast on itself, 32302 on a map of regular difficulty and
/// 38382 otherwise; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericFocusFire(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.unitTarget->CastSpell(ctx.unitTarget, ctx.unitTarget->GetMap()->IsRegularDifficulty() ? 32302 : 38382, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 32312: Move 1; 37388: Move 2: the caster casts Move (30012) on a creature target; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericMove1(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_UNIT)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // cast generic move spell
    ctx.m_caster->CastSpell(ctx.unitTarget, 30012, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 33060: Make a Wish: a player caster casts on itself one of five spells at random (Mr Pinchy's Blessing 33053, Summon
/// Mighty Mr. Pinchy 33057, Summon Furious Mr. Pinchy 33059, Tiny Magical Crawdad 33062 or Mr. Pinchy's Gift 33064);
/// the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericMakeAWish(SpellEffectDummyGenericContext& ctx)
{
    if (ctx.m_caster->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    uint32 spell_id = 0;

    switch (urand(1, 5))
    {
        case 1: spell_id = 33053; break;    // Mr Pinchy's Blessing
        case 2: spell_id = 33057; break;    // Summon Mighty Mr. Pinchy
        case 3: spell_id = 33059; break;    // Summon Furious Mr. Pinchy
        case 4: spell_id = 33062; break;    // Tiny Magical Crawdad
        case 5: spell_id = 33064; break;    // Mr. Pinchy's Gift
    }

    ctx.m_caster->CastSpell(ctx.m_caster, spell_id, true, NULL);
    return SpellHandlerOutcome<void>::Return();
}

/// 34803: Summon Reinforcements: the caster casts 34810, 34817, 34818 and 34819, absent from the client's spell data,
/// on itself; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericSummonReinforcements(SpellEffectDummyGenericContext& ctx)
{
    ctx.m_caster->CastSpell(ctx.m_caster, 34810, true); // Summon 20083 behind of the caster
    ctx.m_caster->CastSpell(ctx.m_caster, 34817, true); // Summon 20078 right of the caster
    ctx.m_caster->CastSpell(ctx.m_caster, 34818, true); // Summon 20078 left of the caster
    ctx.m_caster->CastSpell(ctx.m_caster, 34819, true); // Summon 20078 front of the caster
    return SpellHandlerOutcome<void>::Return();
}

/// 36677: Chaos Breath: with a unit target, the caster casts on itself two of eight spells (36693 to 36700) drawn at
/// random, four on a map that is not of regular difficulty; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericChaosBreath(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    uint32 possibleSpells[] = {36693, 36694, 36695, 36696, 36697, 36698, 36699, 36700} ;
    std::vector<uint32> spellPool(possibleSpells, possibleSpells + countof(possibleSpells));

    //std::random_shuffle(spellPool.begin(), spellPool.end());
    std::mt19937 rng(std::time(nullptr));
    std::shuffle(spellPool.begin(), spellPool.end(), rng);

    for (uint8 i = 0; i < (ctx.m_caster->GetMap()->IsRegularDifficulty() ? 2 : 4); ++i)
    {
        ctx.m_caster->CastSpell(ctx.m_caster, spellPool[i], true);
    }

    return SpellHandlerOutcome<void>::Return();
}

/// 33923, 38796: Sonic Boom: the unit target, if any, casts Sonic Boom on itself, 33666 for 33923 and 38795 for 38796;
/// the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericSonicBoom(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.unitTarget->CastSpell(ctx.unitTarget, ctx.m_spellInfo->ID == 33923 ? 33666 : 38795, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 35745: Socrethar's Stone: a caster in area 3900 or 3742 casts Socrethar Portal (35743 or 35744) on itself, a caster
/// elsewhere nothing; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericSocretharsStone(SpellEffectDummyGenericContext& ctx)
{
    uint32 spell_id;
    switch (ctx.m_caster->GetTerrain()->GetAreaId(ctx.m_caster->Where().X(), ctx.m_caster->Where().Y(), ctx.m_caster->Where().Z()))
    {
        case 3900: spell_id = 35743; break; // Socrethar Portal
        case 3742: spell_id = 35744; break; // Socrethar Portal
        default: return SpellHandlerOutcome<void>::Return();
    }

    ctx.m_caster->CastSpell(ctx.m_caster, spell_id, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 37674: Chaos Blast: the caster casts Chaos Blast (37675) on the unit target, if any, with 100 base points; the
/// effect ends
static SpellHandlerOutcome<void> EffectDummyGenericChaosBlast(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    int32 basepoints0 = 100;
    ctx.m_caster->CastCustomSpell(ctx.unitTarget, 37675, &basepoints0, NULL, NULL, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 39189: Sha'tari Torch: for a player caster and a creature target without Flames (39199), the target casts Flames on
/// itself, the caster gets its kill credit and the target despawns after ten seconds; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericShatariTorch(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_UNIT || ctx.m_caster->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // Flames
    if (ctx.unitTarget->HasAura(39199))
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.unitTarget->CastSpell(ctx.unitTarget, 39199, true);
    ((Player*)ctx.m_caster)->KilledMonsterCredit(ctx.unitTarget->GetEntry(), ctx.unitTarget->GetObjectGuid());
    ((Creature*)ctx.unitTarget)->ForcedDespawn(10000);
    return SpellHandlerOutcome<void>::Return();
}

/// 39635, 39849: Throw Glaive: the unit target, if any, casts Summon Glaive (41466) on itself as from the caster; the
/// effect ends
static SpellHandlerOutcome<void> EffectDummyGenericThrowGlaive(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.unitTarget->CastSpell(ctx.unitTarget, 41466, true, NULL, NULL, ctx.m_caster->GetObjectGuid());
    return SpellHandlerOutcome<void>::Return();
}

/// 40802: Mingo's Fortune Generator: the spell creates one of twenty fortune items (32688 to 32713) at random; the
/// effect ends
static SpellHandlerOutcome<void> EffectDummyGenericMingosFortuneGenerator(SpellEffectDummyGenericContext& ctx)
{
    // selecting one from Bloodstained Fortune item
    uint32 newitemid;
    switch (urand(1, 20))
    {
        case 1:  newitemid = 32688; break;
        case 2:  newitemid = 32689; break;
        case 3:  newitemid = 32690; break;
        case 4:  newitemid = 32691; break;
        case 5:  newitemid = 32692; break;
        case 6:  newitemid = 32693; break;
        case 7:  newitemid = 32700; break;
        case 8:  newitemid = 32701; break;
        case 9:  newitemid = 32702; break;
        case 10: newitemid = 32703; break;
        case 11: newitemid = 32704; break;
        case 12: newitemid = 32705; break;
        case 13: newitemid = 32706; break;
        case 14: newitemid = 32707; break;
        case 15: newitemid = 32708; break;
        case 16: newitemid = 32709; break;
        case 17: newitemid = 32710; break;
        case 18: newitemid = 32711; break;
        case 19: newitemid = 32712; break;
        case 20: newitemid = 32713; break;
        default:
            return SpellHandlerOutcome<void>::Return();
    }

    ctx.spell->DoCreateItem(ctx.effect, newitemid);
    return SpellHandlerOutcome<void>::Return();
}

/// 40834: Agonizing Flames: the caster casts Agonizing Flames (40932) on a player target; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericAgonizingFlames(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.m_caster->CastSpell(ctx.unitTarget, 40932, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 40869: Fatal Attraction: the caster casts Fatal Attraction (41001) on a player target; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericFatalAttraction(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.m_caster->CastSpell(ctx.unitTarget, 41001, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 40962: Blade's Edge Terrace Demon Boss Summon Branch: a player target casts on itself one of the four Blade's Edge
/// Terrace Demon Boss Summon spells (40957, 40959, 40960 or 40961) at random; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericDemonBossSummonBranch(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    uint32 spell_id = 0;
    switch (urand(1, 4))
    {
        case 1: spell_id = 40957; break;    // Blade's Edge Terrace Demon Boss Summon 1
        case 2: spell_id = 40959; break;    // Blade's Edge Terrace Demon Boss Summon 2
        case 3: spell_id = 40960; break;    // Blade's Edge Terrace Demon Boss Summon 3
        case 4: spell_id = 40961; break;    // Blade's Edge Terrace Demon Boss Summon 4
    }
    ctx.unitTarget->CastSpell(ctx.unitTarget, spell_id, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 41283: Abyssal Toss: the caster summons creature 23416 at the unit target's position, if any, for thirty seconds or
/// until its corpse despawns; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericAbyssalToss(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.m_caster->SummonCreature(23416, ctx.unitTarget->Where().X(), ctx.unitTarget->Where().Y(), ctx.unitTarget->Where().Z(), 0, TEMPSPAWN_TIMED_OR_CORPSE_DESPAWN, 30000);
    return SpellHandlerOutcome<void>::Return();
}

/// 41333: Empyreal Equivalency: the unit target's health, if any, is set to the caster's health times the ratio of the
/// target's maximum health to the caster's; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericEmpyrealEquivalency(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // Equilize the health of all targets based on the corresponding health percent
    float health_diff = ctx.m_caster->GetMaxHealth() ? (float)ctx.unitTarget->GetMaxHealth() / (float)ctx.m_caster->GetMaxHealth() : 0.0f;
    ctx.unitTarget->SetHealth(uint32(ctx.m_caster->GetHealth() * health_diff));
    return SpellHandlerOutcome<void>::Return();
}

/// 42287: Salvage Wreckage: a player caster casts on itself Summon Mirefin Burrower (42289) through the cast item 66
/// times in 100, otherwise Summon Salvaged Lockbox (42288); the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericSalvageWreckage(SpellEffectDummyGenericContext& ctx)
{
    if (ctx.m_caster->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    if (roll_chance_i(66))
    {
        ctx.m_caster->CastSpell(ctx.m_caster, 42289, true, ctx.m_CastItem);
    }
    else
    {
        ctx.m_caster->CastSpell(ctx.m_caster, 42288, true);
    }

    return SpellHandlerOutcome<void>::Return();
}

/// 42485: End of Ooze Channel: a creature target casts Ooze Channel Credit (42486) on the caster, and the caster deals
/// it its whole health as damage; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericEndOfOozeChannel(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_UNIT)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.unitTarget->CastSpell(ctx.m_caster, 42486, true);

    // There is no known spell to kill the target
    ctx.m_caster->DealDamage(ctx.unitTarget, ctx.unitTarget->GetHealth(), NULL, DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NORMAL, NULL, false);
    return SpellHandlerOutcome<void>::Return();
}

/// 42489: Cast Ooze Zap When Energized: on the first effect, for a creature target and a caster with the aura the
/// second effect's value names, the caster casts the spell the effect's value names on the target, and the target's AI
/// attacks the caster; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericCastOozeZapWhenEnergized(SpellEffectDummyGenericContext& ctx)
{
    // only process first effect
    // the logic is described by spell 42488 - Caster Spell 1 Only if Aura 2 Is On Caster (not used here)
    if (ctx.effect->EffectIndex != EFFECT_INDEX_0)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_UNIT || !ctx.m_caster->HasAura(ctx.m_spellInfo->CalculateSimpleValue(EFFECT_INDEX_1)))
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.m_caster->CastSpell(ctx.unitTarget, ctx.effect->CalculateSimpleValue(), true);
    ((Creature*)ctx.unitTarget)->AI()->AttackStart(ctx.m_caster);
    return SpellHandlerOutcome<void>::Return();
}

/// 42628: Fire Bomb: the unit target, if any, casts Fire Bomb (42629) on itself; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericFireBomb(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.unitTarget->CastSpell(ctx.unitTarget, 42629, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 42631: Fire Bomb: a creature target loses Fire Bomb (42629), casts Fire Bomb (42630) on itself and despawns after
/// three seconds; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericFireBombExplode(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_UNIT)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.unitTarget->RemoveAurasDueToSpell(42629);
    ctx.unitTarget->CastSpell(ctx.unitTarget, 42630, true);

    // despawn the bomb after exploding
    ((Creature*)ctx.unitTarget)->ForcedDespawn(3000);
    return SpellHandlerOutcome<void>::Return();
}

/// 42793: Burn Body: for a player caster and a creature target with no loot recipient and out of combat, the caster
/// becomes its loot recipient, the target despawns after the spell's duration and the caster gets its kill credit; the
/// effect ends
static SpellHandlerOutcome<void> EffectDummyGenericBurnBody(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_UNIT || ctx.m_caster->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    Creature* pCreature = (Creature*)ctx.unitTarget;

    // Spell can be used in combat and may affect different target than the expected.
    // If target is not the expected we need to prevent this effect.
    if (pCreature->HasLootRecipient() || pCreature->IsInCombat())
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // set loot recipient, prevent re-use same target
    pCreature->SetLootRecipient(ctx.m_caster);

    pCreature->ForcedDespawn(ctx.m_duration);

    // EFFECT_INDEX_2 has 0 miscvalue for effect 134, doing the killcredit here instead (only one known case exist where 0)
    ((Player*)ctx.m_caster)->KilledMonster(pCreature->GetCreatureInfo(), pCreature->GetObjectGuid());
    return SpellHandlerOutcome<void>::Return();
}

/// 43014: Despawn Self: a creature caster despawns; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericDespawnSelf(SpellEffectDummyGenericContext& ctx)
{
    if (ctx.m_caster->GetTypeId() != TYPEID_UNIT)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ((Creature*)ctx.m_caster)->ForcedDespawn();
    return SpellHandlerOutcome<void>::Return();
}

/// 43036: Dismembering Corpse: for a player caster and a unit target without Gruesome, But Necessary: Vrykul On
/// Dismember (43059) on its first effect, the target casts Gruesome, But Necessary: Kill Credit (43037) on the caster
/// and 43059 on itself; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericDismemberingCorpse(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget || ctx.m_caster->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    if (ctx.unitTarget->HasAura(43059, EFFECT_INDEX_0))
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.unitTarget->CastSpell(ctx.m_caster, 43037, true);
    ctx.unitTarget->CastSpell(ctx.unitTarget, 43059, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 43069: Towers of Certain Doom: Skorn Cannonfire: the caster casts Towers of Certain Doom: Tower Caster Instakill
/// (43072) on itself; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericSkornCannonfire(SpellEffectDummyGenericContext& ctx)
{
    // Towers of Certain Doom: Tower Caster Instakill
    ctx.m_caster->CastSpell(ctx.m_caster, 43072, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 43096: Summon All Players: the caster casts Summon All Players (43097) on a player target; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericSummonAllPlayers(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.m_caster->CastSpell(ctx.unitTarget, 43097, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 43144: Hatch All Eggs: the unit target, if any, casts Summon Dragonhawk Hatchling (42493) on itself as from the
/// caster; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericHatchAllEggs(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.unitTarget->CastSpell(ctx.unitTarget, 42493, true, NULL, NULL, ctx.m_caster->GetObjectGuid());
    return SpellHandlerOutcome<void>::Return();
}

/// 43209: Place Ram Meat: the unit target, if any, loses Self Visual - Sleep Until Cancelled (DND) (6606); the effect
/// ends
static SpellHandlerOutcome<void> EffectDummyGenericPlaceRamMeat(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // Self Visual - Sleep Until Cancelled (DND)
    ctx.unitTarget->RemoveAurasDueToSpell(6606);
    return SpellHandlerOutcome<void>::Return();
}
/// 11958: Cold Snap: for a player caster, every frost mage spell on cooldown with a recovery time, but Cold Snap
/// itself, is made ready; the effect ends
static SpellHandlerOutcome<void> EffectDummyMageColdSnap(SpellEffectDummyMageContext& ctx)
{
    if (ctx.m_caster->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // immediately finishes the cooldown on Frost spells
    const SpellCooldowns& cm = ((Player*)ctx.m_caster)->GetSpellCooldownMgr().GetSpellCooldownMap();
    for (SpellCooldowns::const_iterator itr = cm.begin(); itr != cm.end();)
    {
        SpellEntry const* spellInfo = sSpellStore.LookupEntry(itr->first);

        if (spellInfo->GetSpellFamilyName() == SPELLFAMILY_MAGE &&
            (GetSpellSchoolMask(spellInfo) & SPELL_SCHOOL_MASK_FROST) &&
            spellInfo->ID != 11958 && GetSpellRecoveryTime(spellInfo) > 0)
        {
            ((Player*)ctx.m_caster)->RemoveSpellCooldown((itr++)->first, true);
        }
        else
        {
            ++itr;
        }
    }
    return SpellHandlerOutcome<void>::Return();
}

/// 31687: Summon Water Elemental, which has no dummy effect in the client's spell data: a caster with Glyph of Mana
/// Shield (70937) casts 70908 on itself, any other caster 70907, both absent from the client's spell data; the effect
/// ends
static SpellHandlerOutcome<void> EffectDummyMageSummonWaterElemental(SpellEffectDummyMageContext& ctx)
{
    if (ctx.m_caster->HasAura(70937))           // Glyph of Eternal Water (permanent limited by known spells version)
    {
        ctx.m_caster->CastSpell(ctx.m_caster, 70908, true);
    }
    else                                    // temporary version
    {
        ctx.m_caster->CastSpell(ctx.m_caster, 70907, true);
    }

    return SpellHandlerOutcome<void>::Return();
}

/// 32826: Polymorph Cast Visual: a creature target casts one of six forms on itself at random (Squirrel, Giraffe,
/// Serpent, Dragonhawk, Worgen or Sheep Form: 32813, 32816 to 32820); the effect ends
static SpellHandlerOutcome<void> EffectDummyMagePolymorphCastVisual(SpellEffectDummyMageContext& ctx)
{
    if (ctx.unitTarget && ctx.unitTarget->GetTypeId() == TYPEID_UNIT)
    {
        // Polymorph Cast Visual Rank 1
        const uint32 spell_list[6] =
        {
            32813,                          // Squirrel Form
            32816,                          // Giraffe Form
            32817,                          // Serpent Form
            32818,                          // Dragonhawk Form
            32819,                          // Worgen Form
            32820                           // Sheep Form
        };
        ctx.unitTarget->CastSpell(ctx.unitTarget, spell_list[urand(0, 5)], true);
    }
    return SpellHandlerOutcome<void>::Return();
}

/// 38194: Blink: the caster casts Blink (38203) on the unit target, if any; the effect ends
static SpellHandlerOutcome<void> EffectDummyMageBlink(SpellEffectDummyMageContext& ctx)
{
    // Blink
    if (ctx.unitTarget)
    {
        ctx.m_caster->CastSpell(ctx.unitTarget, 38203, true);
    }

    return SpellHandlerOutcome<void>::Return();
}

/// 42955: Conjure Refreshment: the damage is set to 20 and the spell creates that many of the conjured food the
/// target's level calls for; the effect ends
static SpellHandlerOutcome<void> EffectDummyMageConjureRefreshment(SpellEffectDummyMageContext& ctx)
{
    uint32 item = 0;

    uint32 level = ctx.unitTarget->getLevel();

    if (level < 44)
    {
        item = 65500;                                 // Conjured Mana Cookie (lvl 34)
    }
    else if (level < 54)
    {
        item = 65515;                            // Conjured Mana Brownie (lvl 44)
    }
    else if (level < 64)
    {
        item = 65516;                            // Conjured Mana Cupcake (lvl 54)
    }
    else if (level < 65)
    {
        item = 65517;                            // Conjured Mana Lollipop (lvl 64)
    }
    else if (level < 74)
    {
        item = 34062;                            // Conjured Mana Biscuit (lvl 65)
    }
    else if (level < 80)
    {
        item = 43518;                            // Conjured Mana Pie (lvl 74)
    }
    else if (level < 85)
    {
        item = 43523;                            // Conjured Mana Strudel (lvl 80)
    }
    else
    {
        item = 65499;                                            // Conjured Mana Cake (lvl 85)
    }

    ctx.damage = 20; // Used to set stack size.
    ctx.spell->DoCreateItem(ctx.effect,item);

    return SpellHandlerOutcome<void>::Return();
}

/// 21977: Warrior's Wrath: the caster casts Warrior's Wrath (21887) on the unit target, if any; the effect ends
static SpellHandlerOutcome<void> EffectDummyWarriorWarriorsWrath(SpellEffectDummyWarriorContext& ctx)
{
    // Warrior's Wrath
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }
    ctx.m_caster->CastSpell(ctx.unitTarget, 21887, true); // spell mod
    return SpellHandlerOutcome<void>::Return();
// Last Stand
}

/// 12975: Last Stand: the caster casts Last Stand (12976) on itself for 30% of its maximum health; the effect ends
static SpellHandlerOutcome<void> EffectDummyWarriorLastStand(SpellEffectDummyWarriorContext& ctx)
{
    int32 healthModSpellBasePoints0 = int32(ctx.m_caster->GetMaxHealth() * 0.3);
    ctx.m_caster->CastCustomSpell(ctx.m_caster, 12976, &healthModSpellBasePoints0, NULL, NULL, true, NULL);
    return SpellHandlerOutcome<void>::Return();
// Bloodthirst
}

/// 23881: Bloodthirst: the caster casts Bloodthirst (23885) on the unit target with the damage as its base points; the
/// effect ends
static SpellHandlerOutcome<void> EffectDummyWarriorBloodthirst(SpellEffectDummyWarriorContext& ctx)
{
    ctx.m_caster->CastCustomSpell(ctx.unitTarget, 23885, &ctx.damage, NULL, NULL, true, NULL);
    return SpellHandlerOutcome<void>::Return();
}

/// 30284: Change Facing: the unit target, if any, casts Chess: Face Square (30270) on the caster; the effect ends
static SpellHandlerOutcome<void> EffectDummyWarriorChangeFacing(SpellEffectDummyWarriorContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.unitTarget->CastSpell(ctx.m_caster, 30270, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 30012: Move: a unit target without the chess square-occupied aura (39400) casts Chess: Move to Square (30253) on the
/// caster, then Change Facing's body runs
static SpellHandlerOutcome<void> EffectDummyWarriorMove(SpellEffectDummyWarriorContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->HasAura(39400))
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.unitTarget->CastSpell(ctx.m_caster, 30253, true);
    return EffectDummyWarriorChangeFacing(ctx);
}

/// 37144, 37146, 37148, 37151, 37152, 37153: Move: the caster casts Move (30012) on a creature target; the effect ends
static SpellHandlerOutcome<void> EffectDummyWarriorChessMove(SpellEffectDummyWarriorContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_UNIT)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // cast generic move spell
    ctx.m_caster->CastSpell(ctx.unitTarget, 30012, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 5938: Shiv: a player caster casts each poison spell of its off-hand weapon's temporary enchantment on the unit
/// target, then Shiv (5940); the effect ends
static SpellHandlerOutcome<void> EffectDummyRogueShiv(SpellEffectDummyRogueContext& ctx)
{
    if (ctx.m_caster->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    Player* pCaster = ((Player*)ctx.m_caster);

    Item* item = pCaster->GetWeaponForAttack(OFF_ATTACK);
    if (!item)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // all poison enchantments is temporary
    uint32 enchant_id = item->GetEnchantmentId(TEMP_ENCHANTMENT_SLOT);
    if (!enchant_id)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    SpellItemEnchantmentEntry const* pEnchant = sSpellItemEnchantmentStore.LookupEntry(enchant_id);
    if (!pEnchant)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    for (int s = 0; s < 3; ++s)
    {
        if (pEnchant->Effect[s] != ITEM_ENCHANTMENT_TYPE_COMBAT_SPELL)
        {
            continue;
        }

        SpellEntry const* combatEntry = sSpellStore.LookupEntry(pEnchant->EffectArg[s]);
        if (!combatEntry || combatEntry->GetDispel() != DISPEL_POISON)
        {
            continue;
        }

        ctx.m_caster->CastSpell(ctx.unitTarget, combatEntry, true, item);
    }

    ctx.m_caster->CastSpell(ctx.unitTarget, 5940, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 14185: Preparation: for a player caster, the rogue spells of class mask 0x0000024000000860 on cooldown are made
/// ready; the effect ends
static SpellHandlerOutcome<void> EffectDummyRoguePreparation(SpellEffectDummyRogueContext& ctx)
{
    if (ctx.m_caster->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // immediately finishes the cooldown on certain Rogue abilities
    const SpellCooldowns& cm = ((Player*)ctx.m_caster)->GetSpellCooldownMgr().GetSpellCooldownMap();
    for (SpellCooldowns::const_iterator itr = cm.begin(); itr != cm.end();)
    {
        SpellEntry const *spellInfo = sSpellStore.LookupEntry(itr->first);
        SpellClassOptionsEntry const* prepClassOptions = spellInfo->GetSpellClassOptions();
        if (prepClassOptions && prepClassOptions->SpellClassSet == SPELLFAMILY_ROGUE && (prepClassOptions->SpellClassMask & UI64LIT(0x0000024000000860)))
        {
            ((Player*)ctx.m_caster)->RemoveSpellCooldown((itr++)->first,true);
        }
        else
        {
            ++itr;
        }
    }
    return SpellHandlerOutcome<void>::Return();
}

/// 31231: Cheat Death: the caster casts Cheating Death (45182) on itself; the effect ends
static SpellHandlerOutcome<void> EffectDummyRogueCheatDeath(SpellEffectDummyRogueContext& ctx)
{
    // Cheating Death
    ctx.m_caster->CastSpell(ctx.m_caster, 45182, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 51662: Hunger for Blood, absent from the client's spell data: the caster casts 63848, also absent from it, on
/// itself; the effect ends
static SpellHandlerOutcome<void> EffectDummyRogueHungerForBlood(SpellEffectDummyRogueContext& ctx)
{
    ctx.m_caster->CastSpell(ctx.m_caster, 63848, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 23989: Readiness: for a player caster, every hunter spell on cooldown with a recovery time, but Readiness itself, is
/// made ready; the effect ends
static SpellHandlerOutcome<void> EffectDummyHunterReadiness(SpellEffectDummyHunterContext& ctx)
{
    if (ctx.m_caster->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // immediately finishes the cooldown for hunter abilities
    const SpellCooldowns& cm = ((Player*)ctx.m_caster)->GetSpellCooldownMgr().GetSpellCooldownMap();
    for (SpellCooldowns::const_iterator itr = cm.begin(); itr != cm.end();)
    {
        SpellEntry const* spellInfo = sSpellStore.LookupEntry(itr->first);

        if (spellInfo->GetSpellFamilyName() == SPELLFAMILY_HUNTER && spellInfo->ID != 23989 && GetSpellRecoveryTime(spellInfo) > 0 )
        {
            ((Player*)ctx.m_caster)->RemoveSpellCooldown((itr++)->first,true);
        }
        else
        {
            ++itr;
        }
    }
    return SpellHandlerOutcome<void>::Return();
}

/// 37506: Scatter Shot: a player caster stops its auto-repeat spell and its attack and is sent the swing's cancel; the
/// effect ends
static SpellHandlerOutcome<void> EffectDummyHunterScatterShot(SpellEffectDummyHunterContext& ctx)
{
    if (ctx.m_caster->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // break Auto Shot and autohit
    ctx.m_caster->InterruptSpell(CURRENT_AUTOREPEAT_SPELL);
    ctx.m_caster->AttackStop();
    ((Player*)ctx.m_caster)->SendAttackSwingCancelAttack();
    return SpellHandlerOutcome<void>::Return();
// Last Stand
}

/// 53478: Last Stand: the unit target, if any, casts Last Stand (53479) on itself for 30% of its maximum health; the
/// effect ends
static SpellHandlerOutcome<void> EffectDummyHunterLastStand(SpellEffectDummyHunterContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }
    int32 healthModSpellBasePoints0 = int32(ctx.unitTarget->GetMaxHealth() * 0.3);
    ctx.unitTarget->CastCustomSpell(ctx.unitTarget, 53479, &healthModSpellBasePoints0, NULL, NULL, true, NULL);
    return SpellHandlerOutcome<void>::Return();
// Master's Call
}

/// 53271: Master's Call: the caster's pet casts the spell the effect's value names on the unit target; the effect ends
static SpellHandlerOutcome<void> EffectDummyHunterMastersCall(SpellEffectDummyHunterContext& ctx)
{
    Pet* pet = ctx.m_caster->GetPet();
    if (!pet || !ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    pet->CastSpell(ctx.unitTarget, ctx.effect->CalculateSimpleValue(), true);
    return SpellHandlerOutcome<void>::Return();
}

/// 19740: Blessing of Might; 20217: Blessing of Kings: the caster casts the buff its base points name on the unit
/// target, or the raid-wide variant (the next id) on itself when the target is another player in its raid; the effect
/// ends
static SpellHandlerOutcome<void> EffectDummyPaladinBlessing(SpellEffectDummyPaladinContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // Cata 4.3.4: dummy carries the buff spell id; id + 1 is the raid-wide variant
    uint32 buffId = ctx.m_currentBasePoints[ctx.effect->EffectIndex];
    if (!sSpellStore.LookupEntry(buffId))
    {
        return SpellHandlerOutcome<void>::Return();
    }

    Player* casterPlayer = ctx.m_caster->GetTypeId() == TYPEID_PLAYER ? (Player*)ctx.m_caster : NULL;
    Player* targetPlayer = ctx.unitTarget->GetCharmerOrOwnerPlayerOrPlayerItself();
    if (casterPlayer && targetPlayer && casterPlayer != targetPlayer &&
        casterPlayer->IsInSameRaidWith(targetPlayer) && sSpellStore.LookupEntry(buffId + 1))
    {
        ctx.m_caster->CastSpell(ctx.m_caster, buffId + 1, true);
    }
    else
    {
        ctx.m_caster->CastSpell(ctx.unitTarget, buffId, true);
    }
    return SpellHandlerOutcome<void>::Return();
}

/// 31789: Righteous Defense: for a player caster and an attacked friendly target in its raid (the unit target, or a
/// hostile target's victim), the second effect is aimed at up to three of the target's attackers at random; otherwise
/// the cooldown is reset and the cast fails; the effect ends
static SpellHandlerOutcome<void> EffectDummyPaladinRighteousDefense(SpellEffectDummyPaladinContext& ctx)
{
    if (ctx.m_caster->GetTypeId() != TYPEID_PLAYER)
    {
        ctx.spell->SendCastResult(SPELL_FAILED_TARGET_AFFECTING_COMBAT);
        return SpellHandlerOutcome<void>::Return();
    }

    // 31989 -> dummy effect (step 1) + dummy effect (step 2) -> 31709 (taunt like spell for each target)
    Unit* friendTarget = !ctx.unitTarget || ctx.unitTarget->IsFriendlyTo(ctx.m_caster) ? ctx.unitTarget : ctx.unitTarget->getVictim();
    if (friendTarget)
    {
        Player* player = friendTarget->GetCharmerOrOwnerPlayerOrPlayerItself();
        if (!player || !player->IsInSameRaidWith((Player*)ctx.m_caster))
        {
            friendTarget = NULL;
        }
    }

    // non-standard cast requirement check
    if (!friendTarget || friendTarget->getAttackers().empty())
    {
        ((Player*)ctx.m_caster)->RemoveSpellCooldown(ctx.m_spellInfo->ID, true);
        ctx.spell->SendCastResult(SPELL_FAILED_TARGET_AFFECTING_COMBAT);
        return SpellHandlerOutcome<void>::Return();
    }

    // Righteous Defense (step 2) (in old version 31980 dummy effect)
    // Clear targets for eff 1
    for (Spell::TargetList::iterator ihit = ctx.m_UniqueTargetInfo.begin(); ihit != ctx.m_UniqueTargetInfo.end(); ++ihit)
    {
        ihit->effectMask &= ~(1 << 1);
    }

    // not empty (checked), copy
    Unit::AttackerSet attackers = friendTarget->getAttackers();

    // selected from list 3
    for (uint32 i = 0; i < std::min(size_t(3), attackers.size()); ++i)
    {
        Unit::AttackerSet::iterator aItr = attackers.begin();
        std::advance(aItr, urand(0, attackers.size() - 1));
        ctx.spell->AddUnitTarget((*aItr), EFFECT_INDEX_1);
        attackers.erase(aItr);
    }

    // now let next effect cast spell at each target.
    return SpellHandlerOutcome<void>::Return();
}

/// 37877: Blessing of Faith: the caster casts on itself the Blessing of Lower City of the target's class (druid,
/// paladin, priest or shaman); the effect ends
static SpellHandlerOutcome<void> EffectDummyPaladinBlessingOfFaith(SpellEffectDummyPaladinContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    uint32 spell_id = 0;
    switch (ctx.unitTarget->getClass())
    {
        case CLASS_DRUID:   spell_id = 37878; break;
        case CLASS_PALADIN: spell_id = 37879; break;
        case CLASS_PRIEST:  spell_id = 37880; break;
        case CLASS_SHAMAN:  spell_id = 37881; break;
        default: return SpellHandlerOutcome<void>::Return();                    // ignore for not healing classes
    }

    ctx.m_caster->CastSpell(ctx.m_caster, spell_id, true);
    return SpellHandlerOutcome<void>::Return();
}

template <class Site>
struct SpellEffectDummyRow
{
    uint32 spellId;
    typename SpellHandler<Site>::Function function;
};

/// Registers every row of one site's table on `registry`; answers the number of rows.
template <class Site, std::size_t N>
static uint32 RegisterSpellEffectDummyRows(SpellHandlerRegistry& registry, SpellEffectDummyRow<Site> const (&rows)[N])
{
    for (SpellEffectDummyRow<Site> const& row : rows)
    {
        registry.Register<Site>(row.spellId, row.function);
    }
    return uint32(N);
}

uint32 RegisterSpellEffectDummyHandlers(SpellHandlerRegistry& registry)
{
    static SpellEffectDummyRow<SpellEffectDummyGenericSite> const generic[] =
    {
        { 23074, &EffectDummyGenericArcaniteDragonling },
        { 23075, &EffectDummyGenericMithrilMechanicalDragonling },
        { 23076, &EffectDummyGenericMechanicalDragonling },
        { 23133, &EffectDummyGenericGnomishBattleChicken },
        { 23138, &EffectDummyGenericGateOfShazzrah },
        { 23448, &EffectDummyGenericTransporterArrival },
        { 23453, &EffectDummyGenericGnomishTransporter },
        { 23645, &EffectDummyGenericHourglassSand },
        { 23725, &EffectDummyGenericGiftOfLife },
        { 24930, &EffectDummyGenericHallowsEndCandy },
        { 25860, &EffectDummyGenericReindeerTransformation },
        { 26074, &EffectDummyGenericHolidayCheer },
        { 28006, &EffectDummyGenericArcaneCloaking },
        { 29126, &EffectDummyGenericCleansingFlames },
        { 29135, &EffectDummyGenericCleansingFlamesIronforge },
        { 29136, &EffectDummyGenericCleansingFlamesIronforge },
        { 29137, &EffectDummyGenericCleansingFlamesIronforge },
        { 29138, &EffectDummyGenericCleansingFlamesIronforge },
        { 29139, &EffectDummyGenericCleansingFlamesIronforge },
        { 29200, &EffectDummyGenericPurifyHelboarMeat },
        { 29858, &EffectDummyGenericSoulshatter },
        { 29969, &EffectDummyGenericSummonBlizzard },
        { 29979, &EffectDummyGenericMassiveMagneticPull },
        { 30004, &EffectDummyGenericFlameWreath },
        { 30458, &EffectDummyGenericNighInvulnerability },
        { 30507, &EffectDummyGenericPoultryizer },
        { 32027, &EffectDummyGenericExpeditionFlare },
        { 32146, &EffectDummyGenericLiquidFire },
        { 32300, &EffectDummyGenericFocusFire },
        { 32312, &EffectDummyGenericMove1 },
        { 37388, &EffectDummyGenericMove1 },
        { 33060, &EffectDummyGenericMakeAWish },
        { 34803, &EffectDummyGenericSummonReinforcements },
        { 36677, &EffectDummyGenericChaosBreath },
        { 33923, &EffectDummyGenericSonicBoom },
        { 38796, &EffectDummyGenericSonicBoom },
        { 35745, &EffectDummyGenericSocretharsStone },
        { 37674, &EffectDummyGenericChaosBlast },
        { 39189, &EffectDummyGenericShatariTorch },
        { 39635, &EffectDummyGenericThrowGlaive },
        { 39849, &EffectDummyGenericThrowGlaive },
        { 40802, &EffectDummyGenericMingosFortuneGenerator },
        { 40834, &EffectDummyGenericAgonizingFlames },
        { 40869, &EffectDummyGenericFatalAttraction },
        { 40962, &EffectDummyGenericDemonBossSummonBranch },
        { 41283, &EffectDummyGenericAbyssalToss },
        { 41333, &EffectDummyGenericEmpyrealEquivalency },
        { 42287, &EffectDummyGenericSalvageWreckage },
        { 42485, &EffectDummyGenericEndOfOozeChannel },
        { 42489, &EffectDummyGenericCastOozeZapWhenEnergized },
        { 42628, &EffectDummyGenericFireBomb },
        { 42631, &EffectDummyGenericFireBombExplode },
        { 42793, &EffectDummyGenericBurnBody },
        { 43014, &EffectDummyGenericDespawnSelf },
        { 43036, &EffectDummyGenericDismemberingCorpse },
        { 43069, &EffectDummyGenericSkornCannonfire },
        { 43096, &EffectDummyGenericSummonAllPlayers },
        { 43144, &EffectDummyGenericHatchAllEggs },
        { 43209, &EffectDummyGenericPlaceRamMeat },
    };

    static SpellEffectDummyRow<SpellEffectDummyMageSite> const mage[] =
    {
        { 11958, &EffectDummyMageColdSnap },
        { 31687, &EffectDummyMageSummonWaterElemental },
        { 32826, &EffectDummyMagePolymorphCastVisual },
        { 38194, &EffectDummyMageBlink },
        { 42955, &EffectDummyMageConjureRefreshment },
    };

    static SpellEffectDummyRow<SpellEffectDummyWarriorSite> const warrior[] =
    {
        { 21977, &EffectDummyWarriorWarriorsWrath },
        { 12975, &EffectDummyWarriorLastStand },
        { 23881, &EffectDummyWarriorBloodthirst },
        { 30012, &EffectDummyWarriorMove },
        { 30284, &EffectDummyWarriorChangeFacing },
        { 37144, &EffectDummyWarriorChessMove },
        { 37146, &EffectDummyWarriorChessMove },
        { 37148, &EffectDummyWarriorChessMove },
        { 37151, &EffectDummyWarriorChessMove },
        { 37152, &EffectDummyWarriorChessMove },
        { 37153, &EffectDummyWarriorChessMove },
    };

    static SpellEffectDummyRow<SpellEffectDummyRogueSite> const rogue[] =
    {
        { 5938, &EffectDummyRogueShiv },
        { 14185, &EffectDummyRoguePreparation },
        { 31231, &EffectDummyRogueCheatDeath },
        { 51662, &EffectDummyRogueHungerForBlood },
    };

    static SpellEffectDummyRow<SpellEffectDummyHunterSite> const hunter[] =
    {
        { 23989, &EffectDummyHunterReadiness },
        { 37506, &EffectDummyHunterScatterShot },
        { 53478, &EffectDummyHunterLastStand },
        { 53271, &EffectDummyHunterMastersCall },
    };

    static SpellEffectDummyRow<SpellEffectDummyPaladinSite> const paladin[] =
    {
        { 19740, &EffectDummyPaladinBlessing },
        { 20217, &EffectDummyPaladinBlessing },
        { 31789, &EffectDummyPaladinRighteousDefense },
        { 37877, &EffectDummyPaladinBlessingOfFaith },
    };

    uint32 rows = RegisterSpellEffectDummyRows(registry, generic);
    rows += RegisterSpellEffectDummyRows(registry, mage);
    rows += RegisterSpellEffectDummyRows(registry, warrior);
    rows += RegisterSpellEffectDummyRows(registry, rogue);
    rows += RegisterSpellEffectDummyRows(registry, hunter);
    rows += RegisterSpellEffectDummyRows(registry, paladin);
    return rows;
}
