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

#include "spells/handlers/AuraShapeshiftHandlers.h"
#include "spells/handlers/SpellHandlerRegistry.h"
#include "Object/Creature.h"
#include "Log.h"

/// 16739: Orb of Deception: a display by the target's native model
static SpellHandlerOutcome<void> AuraTransformOrbOfDeception(AuraTransformContext& ctx)
{
    uint32 orb_model = ctx.target->GetNativeDisplayId();
    switch (orb_model)
    {
            // Troll Female
        case 1479: ctx.target->SetDisplayId(10134); break;
            // Troll Male
        case 1478: ctx.target->SetDisplayId(10135); break;
            // Tauren Male
        case 59:   ctx.target->SetDisplayId(10136); break;
            // Human Male
        case 49:   ctx.target->SetDisplayId(10137); break;
            // Human Female
        case 50:   ctx.target->SetDisplayId(10138); break;
            // Orc Male
        case 51:   ctx.target->SetDisplayId(10139); break;
            // Orc Female
        case 52:   ctx.target->SetDisplayId(10140); break;
            // Dwarf Male
        case 53:   ctx.target->SetDisplayId(10141); break;
            // Dwarf Female
        case 54:   ctx.target->SetDisplayId(10142); break;
            // NightElf Male
        case 55:   ctx.target->SetDisplayId(10143); break;
            // NightElf Female
        case 56:   ctx.target->SetDisplayId(10144); break;
            // Undead Female
        case 58:   ctx.target->SetDisplayId(10145); break;
            // Undead Male
        case 57:   ctx.target->SetDisplayId(10146); break;
            // Tauren Female
        case 60:   ctx.target->SetDisplayId(10147); break;
            // Gnome Male
        case 1563: ctx.target->SetDisplayId(10148); break;
            // Gnome Female
        case 1564: ctx.target->SetDisplayId(10149); break;
            // BloodElf Female
        case 15475: ctx.target->SetDisplayId(17830); break;
            // BloodElf Male
        case 15476: ctx.target->SetDisplayId(17829); break;
            // Dranei Female
        case 16126: ctx.target->SetDisplayId(17828); break;
            // Dranei Male
        case 16125: ctx.target->SetDisplayId(17827); break;
        default: break;
    }
    return SpellHandlerOutcome<void>::Continue();
}

/// 42365: Murloc costume
static SpellHandlerOutcome<void> AuraTransformMurlocCostume(AuraTransformContext& ctx)
{
    ctx.target->SetDisplayId(21723);
    return SpellHandlerOutcome<void>::Continue();
    // case 44186:                          // Gossip NPC Appearance - All, Brewfest
    // break;
    // case 48305:                          // Gossip NPC Appearance - All, Spirit of Competition
    // break;
}

/// 50517: Dread Corsair; 51926: Corsair Costume: a pirate display by the target's race and gender
static SpellHandlerOutcome<void> AuraTransformCorsairCostume(AuraTransformContext& ctx)
{
    // expected for players
    uint32 race = ctx.target->getRace();

    switch (race)
    {
        case RACE_HUMAN:
            ctx.target->SetDisplayId(ctx.target->getGender() == GENDER_MALE ? 25037 : 25048);
            break;
        case RACE_ORC:
            ctx.target->SetDisplayId(ctx.target->getGender() == GENDER_MALE ? 25039 : 25050);
            break;
        case RACE_DWARF:
            ctx.target->SetDisplayId(ctx.target->getGender() == GENDER_MALE ? 25034 : 25045);
            break;
        case RACE_NIGHTELF:
            ctx.target->SetDisplayId(ctx.target->getGender() == GENDER_MALE ? 25038 : 25049);
            break;
        case RACE_UNDEAD:
            ctx.target->SetDisplayId(ctx.target->getGender() == GENDER_MALE ? 25042 : 25053);
            break;
        case RACE_TAUREN:
            ctx.target->SetDisplayId(ctx.target->getGender() == GENDER_MALE ? 25040 : 25051);
            break;
        case RACE_GNOME:
            ctx.target->SetDisplayId(ctx.target->getGender() == GENDER_MALE ? 25035 : 25046);
            break;
        case RACE_TROLL:
            ctx.target->SetDisplayId(ctx.target->getGender() == GENDER_MALE ? 25041 : 25052);
            break;
        case RACE_GOBLIN:                   // not really player race (3.x), but model exist
            ctx.target->SetDisplayId(ctx.target->getGender() == GENDER_MALE ? 25036 : 25047);
            break;
        case RACE_BLOODELF:
            ctx.target->SetDisplayId(ctx.target->getGender() == GENDER_MALE ? 25032 : 25043);
            break;
        case RACE_DRAENEI:
            ctx.target->SetDisplayId(ctx.target->getGender() == GENDER_MALE ? 25033 : 25044);
            break;
    }

    return SpellHandlerOutcome<void>::Continue();
// case 50531:                              // Gossip NPC Appearance - All, Pirate Day
// break;
// case 51010:                              // Dire Brew
// break;
// case 53806:                              // Pygmy Oil
// break;
// case 62847:                              // NPC Appearance - Valiant 02
// break;
// case 62852:                              // NPC Appearance - Champion 01
// break;
// case 63965:                              // NPC Appearance - Champion 02
// break;
// case 63966:                              // NPC Appearance - Valiant 03
// break;
}

/// 65386: Honor the Dead; 65495: a Day of the Dead display by the target's gender
static SpellHandlerOutcome<void> AuraTransformHonorTheDead(AuraTransformContext& ctx)
{
    switch (ctx.target->getGender())
    {
        case GENDER_MALE:
            ctx.target->SetDisplayId(29203);    // Chapman
            break;
        case GENDER_FEMALE:
        case GENDER_NONE:
            ctx.target->SetDisplayId(29204);    // Catrina
            break;
    }
    return SpellHandlerOutcome<void>::Continue();
// case 65511:                              // Gossip NPC Appearance - Brewfest
// break;
// case 65522:                              // Gossip NPC Appearance - Winter Veil
// break;
// case 65523:                              // Gossip NPC Appearance - Default
// break;
// case 65524:                              // Gossip NPC Appearance - Lunar Festival
// break;
// case 65525:                              // Gossip NPC Appearance - Hallow's End
// break;
// case 65526:                              // Gossip NPC Appearance - Midsummer
// break;
// case 65527:                              // Gossip NPC Appearance - Spirit of Competition
// break;
}

/// 65528: Gossip NPC Appearance - Pirates' Day: a pirate display by the target's race, either gender
static SpellHandlerOutcome<void> AuraTransformPiratesDay(AuraTransformContext& ctx)
{
    // expecting npc's using this spell to have models with race info.
    // random gender, regardless of current gender
    switch (ctx.target->getRace())
    {
        case RACE_HUMAN:
            ctx.target->SetDisplayId(roll_chance_i(50) ? 25037 : 25048);
            break;
        case RACE_ORC:
            ctx.target->SetDisplayId(roll_chance_i(50) ? 25039 : 25050);
            break;
        case RACE_DWARF:
            ctx.target->SetDisplayId(roll_chance_i(50) ? 25034 : 25045);
            break;
        case RACE_NIGHTELF:
            ctx.target->SetDisplayId(roll_chance_i(50) ? 25038 : 25049);
            break;
        case RACE_UNDEAD:
            ctx.target->SetDisplayId(roll_chance_i(50) ? 25042 : 25053);
            break;
        case RACE_TAUREN:
            ctx.target->SetDisplayId(roll_chance_i(50) ? 25040 : 25051);
            break;
        case RACE_GNOME:
            ctx.target->SetDisplayId(roll_chance_i(50) ? 25035 : 25046);
            break;
        case RACE_TROLL:
            ctx.target->SetDisplayId(roll_chance_i(50) ? 25041 : 25052);
            break;
        case RACE_GOBLIN:
            ctx.target->SetDisplayId(roll_chance_i(50) ? 25036 : 25047);
            break;
        case RACE_BLOODELF:
            ctx.target->SetDisplayId(roll_chance_i(50) ? 25032 : 25043);
            break;
        case RACE_DRAENEI:
            ctx.target->SetDisplayId(roll_chance_i(50) ? 25033 : 25044);
            break;
    }

    return SpellHandlerOutcome<void>::Continue();
}

/// 65529: Gossip NPC Appearance - Day of the Dead (DotD): either Day of the Dead display
static SpellHandlerOutcome<void> AuraTransformDayOfTheDead(AuraTransformContext& ctx)
{
    // random, regardless of current gender
    ctx.target->SetDisplayId(roll_chance_i(50) ? 29203 : 29204);
    return SpellHandlerOutcome<void>::Continue();
    // case 66236:                          // Incinerate Flesh
    // break;
    // case 69999:                          // [DND] Swap IDs
    // break;
    // case 70764:                          // Citizen Costume (note: many spells w/same name)
    // break;
    // case 71309:                          // [DND] Spawn Portal
    // break;
}

/// 71450: Crown Parcel Service Uniform: a display by the target's gender
static SpellHandlerOutcome<void> AuraTransformCrownParcelUniform(AuraTransformContext& ctx)
{
    ctx.target->SetDisplayId(ctx.target->getGender() == GENDER_MALE ? 31002 : 31003);
    return SpellHandlerOutcome<void>::Continue();
    // case 75531:                          // Gnomeregan Pride
    // break;
    // case 75532:                          // Darkspear Pride
    // break;
}

/// Any other id with no creature entry: logs that the spell needs a custom model
static SpellHandlerOutcome<void> AuraTransformDefault(AuraTransformContext& ctx)
{
    sLog.outError("Aura::HandleAuraTransform, spell %u does not have creature entry defined, need custom defined model.", ctx.spellId);
    return SpellHandlerOutcome<void>::Continue();
}

template <class Site>
struct AuraShapeshiftRow
{
    uint32 spellId;
    typename SpellHandler<Site>::Function function;
};

/// Registers every row of one site's table on `registry`; answers the number of rows.
template <class Site, std::size_t N>
static uint32 RegisterAuraShapeshiftRows(SpellHandlerRegistry& registry, AuraShapeshiftRow<Site> const (&rows)[N])
{
    for (AuraShapeshiftRow<Site> const& row : rows)
    {
        registry.Register<Site>(row.spellId, row.function);
    }
    return uint32(N);
}

uint32 RegisterAuraShapeshiftHandlers(SpellHandlerRegistry& registry)
{
    static AuraShapeshiftRow<AuraTransformSite> const transform[] =
    {
        { 16739, &AuraTransformOrbOfDeception },
        { 42365, &AuraTransformMurlocCostume },
        { 50517, &AuraTransformCorsairCostume },
        { 51926, &AuraTransformCorsairCostume },
        { 65386, &AuraTransformHonorTheDead },
        { 65495, &AuraTransformHonorTheDead },
        { 65528, &AuraTransformPiratesDay },
        { 65529, &AuraTransformDayOfTheDead },
        { 71450, &AuraTransformCrownParcelUniform },
    };

    uint32 rows = RegisterAuraShapeshiftRows(registry, transform);
    registry.RegisterDefault<AuraTransformSite>(&AuraTransformDefault);
    ++rows;
    return rows;
}
