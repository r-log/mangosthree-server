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
#include "spells/pipeline/Spell.h"
#include "WorldHandlers/CellImpl.h"
#include "WorldHandlers/GridMap.h"
#include "WorldHandlers/GridNotifiers.h"
#include "WorldHandlers/GridNotifiersImpl.h"
#include "WorldHandlers/Map.h"
#include "spells/auras/SpellAuras.h"
#include "BattleGround/BattleGround.h"
#include "MotionGenerators/MotionMaster.h"
#include "Object/Creature.h"
#include "Object/CreatureAI.h"
#include "Object/GameObject.h"
#include "Object/Pet.h"
#include "spells/data/SpellMgr.h"
#include "Server/DBCStores.h"
#include "Server/WorldSession.h"
#include "Utilities/Util.h"
#include "entities/player/Player.h"
#include "Opcodes.h"
#include "WorldPacket.h"
#include <algorithm>
#include <ctime>
#include <iterator>
#include <list>
#include <random>
#include <vector>

/// 3360: Curse of the Eye: the caster casts on the unit target, if any, Curse of the Eye (10651) for a male target,
/// otherwise Curse of the Eye (10653); the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericCurseOfTheEye(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    uint32 spell_id = (ctx.unitTarget->getGender() == GENDER_MALE) ? 10651 : 10653;

    ctx.m_caster->CastSpell(ctx.unitTarget, spell_id, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 7671: Transformation (the label says human to worgen): the unit target, if any, casts Transform Visual (24085) on
/// itself; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericTransformation(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // Transform Visual
    ctx.unitTarget->CastSpell(ctx.unitTarget, 24085, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 8063: Deviate Fish: a player caster casts on itself one of Sleepy (8064), Invigorate (8065), Shrink (8066), Party
/// Time! (8067) and Healthy Spirit (8068) at random; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericDeviateFish(SpellEffectDummyGenericContext& ctx)
{
    if (ctx.m_caster->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    uint32 spell_id = 0;
    switch (urand(1, 5))
    {
        case 1: spell_id = 8064; break;     // Sleepy
        case 2: spell_id = 8065; break;     // Invigorate
        case 3: spell_id = 8066; break;     // Shrink
        case 4: spell_id = 8067; break;     // Party Time!
        case 5: spell_id = 8068; break;     // Healthy Spirit
    }
    ctx.m_caster->CastSpell(ctx.m_caster, spell_id, true, NULL);
    return SpellHandlerOutcome<void>::Return();
}

/// 8213: Cooked Deviate Fish (the label says Savory Deviate Delight): a player caster casts on itself, at even odds,
/// Flip Out (8219 for a male caster, 8220 otherwise) or Yaaarrrr (8221 for a male caster, 8222 otherwise); the effect
/// ends
static SpellHandlerOutcome<void> EffectDummyGenericCookedDeviateFish(SpellEffectDummyGenericContext& ctx)
{
    if (ctx.m_caster->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    uint32 spell_id = 0;
    switch (urand(1, 2))
    {
        // Flip Out - ninja
        case 1: spell_id = (ctx.m_caster->getGender() == GENDER_MALE ? 8219 : 8220); break;
        // Yaaarrrr - pirate
        case 2: spell_id = (ctx.m_caster->getGender() == GENDER_MALE ? 8221 : 8222); break;
    }

    ctx.m_caster->CastSpell(ctx.m_caster, spell_id, true, NULL);
    return SpellHandlerOutcome<void>::Return();
}

/// 9976: Polly Eats the E.C.A.C.: a creature target casts Summon Polly Jr. (9998) on itself and despawns after a tenth
/// of a second; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericPollyEatsTheECAC(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_UNIT)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // Summon Polly Jr.
    ctx.unitTarget->CastSpell(ctx.unitTarget, 9998, true);

    ((Creature*)ctx.unitTarget)->ForcedDespawn(100);
    return SpellHandlerOutcome<void>::Return();
}

/// 10254: Stone Dwarf Awaken Visual
static SpellHandlerOutcome<void> EffectDummyGeneric10254(SpellEffectDummyGenericContext& ctx)
{
    if (ctx.m_caster->GetTypeId() != TYPEID_UNIT)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // see spell 10255 (aura dummy)
    ctx.m_caster->GetMotionMaster()->Uninhibit(Motion::Inhibition::Rooted, Motion::ControlClaim(10255, 0, ctx.m_caster->GetObjectGuid().GetCounter()));
    ctx.m_caster->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
    return SpellHandlerOutcome<void>::Return();
}

/// 13120: Net-o-Matic: the caster casts on the unit target, if any, one of the three Net-o-Matic spells by a roll of a
/// hundred: 16566 (a self root) under 2, 13119 (a root and a charge) under 4, otherwise 13099; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericNetOMatic(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    uint32 spell_id = 0;

    uint32 roll = urand(0, 99);

    if (roll < 2)                           // 2% for 30 sec self root (off-like chance unknown)
    {
        spell_id = 16566;
    }
    else if (roll < 4)                      // 2% for 20 sec root, charge to target (off-like chance unknown)
    {
        spell_id = 13119;
    }
    else                                    // normal root
    {
        spell_id = 13099;
    }

    ctx.m_caster->CastSpell(ctx.unitTarget, spell_id, true, NULL);
    return SpellHandlerOutcome<void>::Return();
}

/// 13489: Burning Spirit (the label has no name): the unit target, if any, casts Burning Spirit (14744) on itself; the
/// effect ends
static SpellHandlerOutcome<void> EffectDummyGenericBurningSpirit(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.unitTarget->CastSpell(ctx.unitTarget, 14744, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 13567: Dummy Trigger: with a triggering aura and a unit target, the caster casts on the target the spell the
/// triggering aura chooses, Persistent Shield (26470) with the effect's damage as its first value for Persistent Shield
/// (26467), and logs an error for any other aura; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericDummyTrigger(SpellEffectDummyGenericContext& ctx)
{
    // can be used for different aura triggering, so select by aura
    if (!ctx.m_triggeredByAuraSpell || !ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    switch (ctx.m_triggeredByAuraSpell->ID)
    {
        case 26467:                         // Persistent Shield
            ctx.m_caster->CastCustomSpell(ctx.unitTarget, 26470, &ctx.damage, NULL, NULL, true);
            break;
        default:
            sLog.outError("EffectDummyGenericDummyTrigger: Non-handled case for spell 13567 for triggered aura %u", ctx.m_triggeredByAuraSpell->ID);
            break;
    }
    return SpellHandlerOutcome<void>::Return();
}

/// 14537: Six Demon Bag: the caster casts through the cast item, by a roll of a hundred, Fireball (15662) under 25,
/// Frostbolt (11538) under 50, Chain Lightning (21179) under 70 or Polymorph (14621) under 77 on the unit target, if
/// any, Polymorph (14621) on itself under 80, Enveloping Winds (25189) on the target under 95, otherwise Summon
/// Felhound Minion (14642) on itself; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericSixDemonBag(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    Unit* newTarget = ctx.unitTarget;
    uint32 spell_id = 0;
    uint32 roll = urand(0, 99);
    if (roll < 25)                          // Fireball (25% chance)
    {
        spell_id = 15662;
    }
    else if (roll < 50)                     // Frostbolt (25% chance)
    {
        spell_id = 11538;
    }
    else if (roll < 70)                     // Chain Lighting (20% chance)
    {
        spell_id = 21179;
    }
    else if (roll < 77)                     // Polymorph (10% chance, 7% to target)
    {
        spell_id = 14621;
    }
    else if (roll < 80)                     // Polymorph (10% chance, 3% to self, backfire)
    {
        spell_id = 14621;
        newTarget = ctx.m_caster;
    }
    else if (roll < 95)                     // Enveloping Winds (15% chance)
    {
        spell_id = 25189;
    }
    else                                    // Summon Felhund minion (5% chance)
    {
        spell_id = 14642;
        newTarget = ctx.m_caster;
    }

    ctx.m_caster->CastSpell(newTarget, spell_id, true, ctx.m_CastItem);
    return SpellHandlerOutcome<void>::Return();
}

/// 15998: Capture Worg Pup; 29435: Capture Female Kaliri Hatchling: a creature target despawns; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericCaptureWorgPupKaliriHatchling(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_UNIT)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    Creature* creatureTarget = (Creature*)ctx.unitTarget;

    creatureTarget->ForcedDespawn();
    return SpellHandlerOutcome<void>::Return();
}

/// 16589: Noggenfogger Elixir: a player caster casts on itself one of the three Noggenfogger Elixir spells (16595,
/// 16593 or 16591) at random; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericNoggenfoggerElixir(SpellEffectDummyGenericContext& ctx)
{
    if (ctx.m_caster->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    uint32 spell_id = 0;
    switch (urand(1, 3))
    {
        case 1: spell_id = 16595; break;
        case 2: spell_id = 16593; break;
        default: spell_id = 16591; break;
    }

    ctx.m_caster->CastSpell(ctx.m_caster, spell_id, true, NULL);
    return SpellHandlerOutcome<void>::Return();
}

/// 17009: Voodoo: the caster casts on a player target one of Hex (16707, 16708 or 16709), Grow (16711), Special Brew
/// (16712), Ghostly (16713) and Launch (16716) at random, as from the original caster and triggered by this spell; the
/// effect ends
static SpellHandlerOutcome<void> EffectDummyGenericVoodoo(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    uint32 spell_id = 0;
    switch (urand(0, 6))
    {
        case 0: spell_id = 16707; break;    // Hex
        case 1: spell_id = 16708; break;    // Hex
        case 2: spell_id = 16709; break;    // Hex
        case 3: spell_id = 16711; break;    // Grow
        case 4: spell_id = 16712; break;    // Special Brew
        case 5: spell_id = 16713; break;    // Ghostly
        case 6: spell_id = 16716; break;    // Launch
    }

    ctx.m_caster->CastSpell(ctx.unitTarget, spell_id, true, NULL, NULL, ctx.m_originalCasterGUID, ctx.m_spellInfo);
    return SpellHandlerOutcome<void>::Return();
}

/// 17251: Spirit Healer Res: with a unit target and a player as the affective caster, that player's session is sent
/// SMSG_SPIRIT_HEALER_CONFIRM naming the target; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericSpiritHealerRes(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    Unit* caster = ctx.spell->GetAffectiveCaster();

    if (ClientCallbacks const* client = caster ? caster->GetClientCallbacks() : NULL)
    {
        SpiritHealerConfirmFact fact;
        fact.healer = ctx.unitTarget->GetObjectGuid();
        ReportClientFact(client->spiritHealerConfirm, fact);
    }
    return SpellHandlerOutcome<void>::Return();
}

/// 17271: Test Fetid Skull: with an item target or a player caster, the caster casts on itself Create Resonating Skull
/// (17269) or Create Bone Dust (17270) at even odds; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericTestFetidSkull(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.itemTarget && ctx.m_caster->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    uint32 spell_id = urand(0, 1)
                      ? 17269               // Create Resonating Skull
                      : 17270;              // Create Bone Dust

    ctx.m_caster->CastSpell(ctx.m_caster, spell_id, true, NULL);
    return SpellHandlerOutcome<void>::Return();
}

/// 17770: Wolfshead Helm Energy: the caster casts Wolfshead Helm Energy (29940) on itself; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericWolfsheadHelmEnergy(SpellEffectDummyGenericContext& ctx)
{
    ctx.m_caster->CastSpell(ctx.m_caster, 29940, true, NULL);
    return SpellHandlerOutcome<void>::Return();
}

/// 17950: Shadow Portal: the caster casts on the unit target, if any, one of the six Shadow Portal spells (17863,
/// 17939, 17943, 17944, 17946 or 17948) at random; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericShadowPortal(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // Shadow Portal
    const uint32 spell_list[6] = {17863, 17939, 17943, 17944, 17946, 17948};

    ctx.m_caster->CastSpell(ctx.unitTarget, spell_list[urand(0, 5)], true);
    return SpellHandlerOutcome<void>::Return();
}

/// 19395: Gordunni Trap: a player target casts on itself 19394 or 11756 at even odds, two ids the client does not hold;
/// the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericGordunniTrap(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.unitTarget->CastSpell(ctx.unitTarget, urand(0, 1) ? 19394 : 11756, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 19411, 20474: Lava Bomb: with a unit target, a game object of entry 177704 is created in the target's map at the
/// target's position and facing and in the caster's phase, with half a minute's respawn time, its template's trap level
/// and this spell's id, and added to the map; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericLavaBomb(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // Hack alert!
    // This dummy are expected to cast spell 20494 to summon GO entry 177704
    // Spell does not exist client side, so we have to make a hack, creating the GO (SPELL_EFFECT_SUMMON_OBJECT_WILD)
    // Spell should appear in both SMSG_SPELL_START/GO and SMSG_SPELLLOGEXECUTE

    // For later, creating custom spell
    // _START: packguid: target, cast flags: 0xB, TARGET_FLAG_SELF
    // _GO: packGuid: target, cast flags: 0x4309, TARGET_FLAG_DEST_LOCATION
    // LOG: spell: 20494, effect, pguid: goguid

    GameObject* pGameObj = new GameObject;

    Map* map = ctx.unitTarget->GetMap();

    if (!pGameObj->Create(map->GenerateLocalLowGuid(HIGHGUID_GAMEOBJECT), 177704,
                          map, ctx.m_caster->GetPhaseMask(),
                          ctx.unitTarget->Where().X(), ctx.unitTarget->Where().Y(), ctx.unitTarget->Where().Z(),
                          ctx.unitTarget->Where().Facing()))
    {
        delete pGameObj;
        return SpellHandlerOutcome<void>::Return();
    }

    DEBUG_LOG("Gameobject, create custom in SpellEffectDummyHandlers.cpp EffectDummyGenericLavaBomb");

    // Expect created without owner, but with level from _template
    pGameObj->SetRespawnTime(MINUTE / 2);
    pGameObj->SetUInt32Value(GAMEOBJECT_LEVEL, pGameObj->GetGOInfo()->trap.level);
    pGameObj->SetSpellId(ctx.m_spellInfo->ID);

    map->Add(pGameObj);

    return SpellHandlerOutcome<void>::Return();
}

/// 19869: Dragon Orb: a player target without Mind Exhaustion (23958) casts Possess (19832) on itself; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericDragonOrb(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_PLAYER || ctx.unitTarget->HasAura(23958))
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.unitTarget->CastSpell(ctx.unitTarget, 19832, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 20037: Explode Orb Effect: the unit target, if any, casts Explosion (20038) on itself; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericExplodeOrbEffect(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.unitTarget->CastSpell(ctx.unitTarget, 20038, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 20577: Cannibalize: with a unit target, the caster casts Cannibalize (20578) on itself, not triggered; the effect
/// ends
static SpellHandlerOutcome<void> EffectDummyGenericCannibalize(SpellEffectDummyGenericContext& ctx)
{
    if (ctx.unitTarget)
    {
        ctx.m_caster->CastSpell(ctx.m_caster, 20578, false, NULL);
    }

    return SpellHandlerOutcome<void>::Return();
}

/// 21147: Arcane Vacuum: the threat the unit target, if any, holds on the caster's list, when it is not zero, is cut by
/// a hundred percent, and the caster casts Summon Player (21150) on the target; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericArcaneVacuum(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // Spell used by Azuregos to teleport all the players to him
    // This also resets the target threat
    if (ctx.m_caster->GetThreatManager().getThreat(ctx.unitTarget))
    {
        ctx.m_caster->GetThreatManager().modifyThreatPercent(ctx.unitTarget, -100);
    }

    // cast summon player
    ctx.m_caster->CastSpell(ctx.unitTarget, 21150, true);

    return SpellHandlerOutcome<void>::Return();
}

/// 23019: Crystal Prison Dummy DND: for a living creature target that is no pet, a game object of entry 179644 is
/// created in the target's map at the target's position and facing and in the caster's phase, with the time to the
/// target's respawn as its respawn time, the caster as its owner, the caster's level and this spell's id; the target
/// despawns and the object is added to the map; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericCrystalPrisonDummyDND(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget || !ctx.unitTarget->IsAlive() || ctx.unitTarget->GetTypeId() != TYPEID_UNIT || ctx.unitTarget->IsPet())
    {
        return SpellHandlerOutcome<void>::Return();
    }

    Creature* creatureTarget = (Creature*)ctx.unitTarget;
    if (creatureTarget->IsPet())
    {
        return SpellHandlerOutcome<void>::Return();
    }

    GameObject* pGameObj = new GameObject;

    Map* map = creatureTarget->GetMap();

    // create before death for get proper coordinates
    if (!pGameObj->Create(map->GenerateLocalLowGuid(HIGHGUID_GAMEOBJECT), 179644, map, ctx.m_caster->GetPhaseMask(),
                          creatureTarget->Where().X(), creatureTarget->Where().Y(), creatureTarget->Where().Z(),
                          creatureTarget->Where().Facing()))
    {
        delete pGameObj;
        return SpellHandlerOutcome<void>::Return();
    }

    pGameObj->SetRespawnTime(creatureTarget->GetRespawnTime() - time(NULL));
    pGameObj->SetOwnerGuid(ctx.m_caster->GetObjectGuid());
    pGameObj->SetUInt32Value(GAMEOBJECT_LEVEL, ctx.m_caster->getLevel());
    pGameObj->SetSpellId(ctx.m_spellInfo->ID);

    creatureTarget->ForcedDespawn();

    DEBUG_LOG("AddObject at SpellEffectDummyHandlers.cpp EffectDummyGenericCrystalPrisonDummyDND");
    map->Add(pGameObj);

    return SpellHandlerOutcome<void>::Return();
}

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
    ctx.unitTarget->AI()->AttackStart(ctx.m_caster);
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

/// 43498: Siphon Soul: when the unit target is the last of the spell's targets, the caster casts Siphon Soul (43501) on
/// one of the targets that are players on its map, at random; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericSiphonSoul(SpellEffectDummyGenericContext& ctx)
{
    // This spell should cast the next spell only for one (player)target, however it should hit multiple targets, hence this kind of implementation
    if (!ctx.unitTarget || ctx.m_UniqueTargetInfo.rbegin()->targetGUID != ctx.unitTarget->GetObjectGuid())
    {
        return SpellHandlerOutcome<void>::Return();
    }

    std::vector<Unit*> possibleTargets;
    possibleTargets.reserve(ctx.m_UniqueTargetInfo.size());
    for (Spell::TargetList::const_iterator itr = ctx.m_UniqueTargetInfo.begin(); itr != ctx.m_UniqueTargetInfo.end(); ++itr)
    {
        // Skip Non-Players
        if (!itr->targetGUID.IsPlayer())
        {
            continue;
        }

        if (Unit* target = ctx.m_caster->GetMap()->GetPlayer(itr->targetGUID))
        {
            possibleTargets.push_back(target);
        }
    }

    // Cast Siphon Soul channeling spell
    if (!possibleTargets.empty())
    {
        ctx.m_caster->CastSpell(possibleTargets[urand(0, possibleTargets.size() - 1)], 43501, false);
    }

    return SpellHandlerOutcome<void>::Return();
}

/// 43572: Send Them Packing: On /Raise Emote Dummy to Player: the caster casts Send Them Packing: On Successful Dummy
/// Spell Kill Credit (42721) on the unit target, if any; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericSendThemPacking(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // m_caster (creature) should start walking back to it's "home" here, no clear way how to do that

    // Send Them Packing: On Successful Dummy Spell Kill Credit
    ctx.m_caster->CastSpell(ctx.unitTarget, 42721, true);
    return SpellHandlerOutcome<void>::Return();
// Demon Broiled Surprise
}

/// 43723: Demon Broiled Surprise: a player caster casts Demon Broiled Surprise (43753) on the unit target through the
/// cast item, as from the original caster and triggered by this spell; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericDemonBroiledSurprise(SpellEffectDummyGenericContext& ctx)
{
    if (ctx.m_caster->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ((Player*)ctx.m_caster)->CastSpell(ctx.unitTarget, 43753, true, ctx.m_CastItem, NULL, ctx.m_originalCasterGUID, ctx.m_spellInfo);
    return SpellHandlerOutcome<void>::Return();
}

/// 43882: Scourging Crystal Controller Dummy: a creature target loses Scourge Mur'gul Camp: Force Shield Arcane Purple
/// x3 (43874); the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericScourgingCrystalControllerDummy(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_UNIT)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // see spell dummy 50133
    ctx.unitTarget->RemoveAurasDueToSpell(43874);
    return SpellHandlerOutcome<void>::Return();
}

/// 44454: Tasty Reef Fish: the caster casts The Way to His Heart...: Character Script Effect Reverse Cast (44455) on a
/// creature target through the cast item; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericTastyReefFish(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_UNIT)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.m_caster->CastSpell(ctx.unitTarget, 44455, true, ctx.m_CastItem);
    return SpellHandlerOutcome<void>::Return();
}

/// 44869: Spectral Blast: a player target without Spectral Exhaustion (44867) or Spectral Realm (46021) casts on itself
/// Spectral Blast (44866 and 46648) as from the caster and Spectral Realm (44811); the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericSpectralBlast(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // If target has spectral exhaustion or spectral realm aura return
    if (ctx.unitTarget->HasAura(44867) || ctx.unitTarget->HasAura(46021))
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // Cast the spectral realm effect spell, visual spell and spectral blast rift summoning
    ctx.unitTarget->CastSpell(ctx.unitTarget, 44866, true, NULL, NULL, ctx.m_caster->GetObjectGuid());
    ctx.unitTarget->CastSpell(ctx.unitTarget, 46648, true, NULL, NULL, ctx.m_caster->GetObjectGuid());
    ctx.unitTarget->CastSpell(ctx.unitTarget, 44811, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 44875: Complete Raptor Capture: a creature target despawns, and the caster casts Raptor Capture Credit (42337) on
/// itself; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericCompleteRaptorCapture(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_UNIT)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    Creature* creatureTarget = (Creature*)ctx.unitTarget;

    creatureTarget->ForcedDespawn();

    // cast spell Raptor Capture Credit
    ctx.m_caster->CastSpell(ctx.m_caster, 42337, true, NULL);
    return SpellHandlerOutcome<void>::Return();
}

/// 44997: Converting Sentry: a creature target despawns, and the caster casts Converted Sentry Credit (45009) on
/// itself; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericConvertingSentry(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_UNIT)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    Creature* creatureTarget = (Creature*)ctx.unitTarget;

    creatureTarget->ForcedDespawn();

    // Converted Sentry Credit
    ctx.m_caster->CastSpell(ctx.m_caster, 45009, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 45030: Impale Emissary: the caster casts Emissary of Hate Credit (45088) on itself; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericImpaleEmissary(SpellEffectDummyGenericContext& ctx)
{
    // Emissary of Hate Credit
    ctx.m_caster->CastSpell(ctx.m_caster, 45088, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 45449: Arcane Prisoner Rescue: the caster casts Summon Arcane Prisoner (45446 or 45448, at random) on itself; with
/// no unit target the effect ends, otherwise the target casts Arcane Prisoner Kill Credit (45456) on the caster and the
/// effect goes on to the script library
static SpellHandlerOutcome<void> EffectDummyGenericArcanePrisonerRescue(SpellEffectDummyGenericContext& ctx)
{
    uint32 spellId = 0;
    switch (urand(0, 1))
    {
        case 0: spellId = 45446; break;    // Summon Arcane Prisoner - Male
        case 1: spellId = 45448; break;    // Summon Arcane Prisoner - Female
    }
    // Spawn
    ctx.m_caster->CastSpell(ctx.m_caster, spellId, true);

    if (!ctx.unitTarget) return SpellHandlerOutcome<void>::Return();
    // Arcane Prisoner Kill Credit
    ctx.unitTarget->CastSpell(ctx.m_caster, 45456, true);

    return SpellHandlerOutcome<void>::Continue();
}

/// 45583: Throw Gnomish Grenade: for a player caster and a unit target, the caster gets the target's kill credit, and
/// the nearest game object 187675 and every game object 187676 within the spell's range of the target that is not
/// spawned is spawned for thirty seconds; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericThrowGnomishGrenade(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget || ctx.m_caster->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ((Player*)ctx.m_caster)->KilledMonsterCredit(ctx.unitTarget->GetEntry(), ctx.unitTarget->GetObjectGuid());

    // look for gameobject within max spell range of unitTarget, and respawn if found

    // big fire
    GameObject* pGo = NULL;

    float fMaxDist = GetSpellMaxRange(sSpellRangeStore.LookupEntry(ctx.m_spellInfo->RangeIndex));

    MaNGOS::NearestGameObjectEntryInPosRangeCheck go_check_big(*ctx.unitTarget, 187675, ctx.unitTarget->Where().X(), ctx.unitTarget->Where().Y(), ctx.unitTarget->Where().Z(), fMaxDist);
    MaNGOS::GameObjectSearcher<MaNGOS::NearestGameObjectEntryInPosRangeCheck> checker1(pGo, go_check_big);

    Cell::VisitGridObjects(ctx.unitTarget, checker1, fMaxDist);

    if (pGo && !pGo->isSpawned())
    {
        pGo->SetRespawnTime(MINUTE / 2);
        pGo->Refresh();
    }

    // small fire
    std::list<GameObject*> lList;

    MaNGOS::GameObjectEntryInPosRangeCheck go_check_small(*ctx.unitTarget, 187676, ctx.unitTarget->Where().X(), ctx.unitTarget->Where().Y(), ctx.unitTarget->Where().Z(), fMaxDist);
    MaNGOS::GameObjectListSearcher<MaNGOS::GameObjectEntryInPosRangeCheck> checker2(lList, go_check_small);

    Cell::VisitGridObjects(ctx.unitTarget, checker2, fMaxDist);

    for (std::list<GameObject*>::iterator iter = lList.begin(); iter != lList.end(); ++iter)
    {
        if (!(*iter)->isSpawned())
        {
            (*iter)->SetRespawnTime(MINUTE / 2);
            (*iter)->Refresh();
        }
    }

    return SpellHandlerOutcome<void>::Return();
}

/// 45685: Hah... : Magnataur On Death 2: the caster loses Bigger! (45673), Shrunk (45672), Yellow (45677), Ghost
/// (45681) and Polymorph: Crafty Wobblesprocket (45683); the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericHahMagnataurOnDeath2(SpellEffectDummyGenericContext& ctx)
{
    ctx.m_caster->RemoveAurasDueToSpell(45673);
    ctx.m_caster->RemoveAurasDueToSpell(45672);
    ctx.m_caster->RemoveAurasDueToSpell(45677);
    ctx.m_caster->RemoveAurasDueToSpell(45681);
    ctx.m_caster->RemoveAurasDueToSpell(45683);
    return SpellHandlerOutcome<void>::Return();
}

/// 45958: Signal Alliance: the caster casts on itself the spell the effect's value names; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericSignalAlliance(SpellEffectDummyGenericContext& ctx)
{
    ctx.m_caster->CastSpell(ctx.m_caster, ctx.effect->CalculateSimpleValue(), true);
    return SpellHandlerOutcome<void>::Return();
}

/// 45976: Open Portal; 46177: Open All Portals: with no unit target the effect ends, otherwise the target casts Open
/// Portal (45977) on itself and the effect goes on to the script library
static SpellHandlerOutcome<void> EffectDummyGenericOpenPortal(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // portal visual
    ctx.unitTarget->CastSpell(ctx.unitTarget, 45977, true);

    // break in case additional procressing in scripting library required
    return SpellHandlerOutcome<void>::Continue();
}

/// 45980: Re-Cursive Transmatter Injection: for a player caster and a unit target, the caster casts Summon Fizzcrank
/// Survivor (46022) on the target and gets the kill credit of the creature that spell's first effect names, and a
/// creature target despawns; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericReCursiveTransmatterInjection(SpellEffectDummyGenericContext& ctx)
{
    if (ctx.m_caster->GetTypeId() == TYPEID_PLAYER && ctx.unitTarget)
    {
        if (const SpellEntry* pSpell = sSpellStore.LookupEntry(46022))
        {
            ctx.m_caster->CastSpell(ctx.unitTarget, pSpell, true);
            SpellEffectEntry const* killSpellEffect = pSpell->GetSpellEffect(EFFECT_INDEX_0);
            ((Player*)ctx.m_caster)->KilledMonsterCredit(killSpellEffect ? killSpellEffect->EffectMiscValue_0 : 0);
        }

        if (ctx.unitTarget->GetTypeId() == TYPEID_UNIT)
        {
            ((Creature*)ctx.unitTarget)->ForcedDespawn();
        }
    }

    return SpellHandlerOutcome<void>::Return();
}

/// 45989: Summon Void Sentinel Summoner Visual: the unit target, if any, casts Summon Void Sentinel (45988) on itself;
/// the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericSummonVoidSentinelSummonerVisual(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // summon void sentinel
    ctx.unitTarget->CastSpell(ctx.unitTarget, 45988, true);

    return SpellHandlerOutcome<void>::Return();
}

/// 45990: Collect Oil: a creature target casts Summon Oil Collector (45991) on itself and despawns after the spell's
/// duration; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericCollectOil(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_UNIT)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    if (const SpellEntry* pSpell = sSpellStore.LookupEntry(45991))
    {
        ctx.unitTarget->CastSpell(ctx.unitTarget, pSpell, true);
        ((Creature*)ctx.unitTarget)->ForcedDespawn(ctx.m_duration);
    }

    return SpellHandlerOutcome<void>::Return();
}

/// 46167: Planning for the Future: Create Snowfall Glade Pup Cover; 50918: Gluttonous Lurkers: Create Basilisk Crystals
/// Cover; 50926: Gluttonous Lurkers: Create Zul'Drak Rat Cover; 51026: Create Drakkari Medallion Cover; 51592: Pickup
/// Primordial Hatchling; 51961: Captured Chicken Cover; 55364: Create Ghoul Drool Cover; 61832: Rifle the Bodies:
/// Create Magehunter Personal Effects Cover; 63125: Search Maloric; 74904: Pickup Sen'jin Frog: for a creature target
/// and a player caster, the target casts on the caster the spell the label names (46773, 50919, 50927, 50737, 51593,
/// 51037, 55363, 47096, 63126 or 74905) and despawns a millisecond after that spell's cast time; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericCreateCover(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_UNIT || ctx.m_caster->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    uint32 spellId = 0;

    switch (ctx.m_spellInfo->ID)
    {
        case 46167: spellId = 46773; break;
        case 50918: spellId = 50919; break;
        case 50926: spellId = 50927; break;
        case 51026: spellId = 50737; break;
        case 51592: spellId = 51593; break;
        case 51961: spellId = 51037; break;
        case 55364: spellId = 55363; break;
        case 61832: spellId = 47096; break;
        case 63125: spellId = 63126; break;
        case 74904: spellId = 74905; break;
    }

    if (const SpellEntry* pSpell = sSpellStore.LookupEntry(spellId))
    {
        ctx.unitTarget->CastSpell(ctx.m_caster, spellId, true);

        Creature* creatureTarget = (Creature*)ctx.unitTarget;

        if (const SpellCastTimesEntry* pCastTime = sSpellCastTimesStore.LookupEntry(pSpell->CastingTimeIndex))
        {
            creatureTarget->ForcedDespawn(pCastTime->CastTime + 1);
        }
    }
    return SpellHandlerOutcome<void>::Return();
}

/// 46171: Scuttle Wrecked Flying Machine: for a player caster and a unit target, the caster gets the target's kill
/// credit, and the nearest game object 187675 within the spell's range of the target, if it is not spawned, is spawned
/// for thirty seconds; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericScuttleWreckedFlyingMachine(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget || ctx.m_caster->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ((Player*)ctx.m_caster)->KilledMonsterCredit(ctx.unitTarget->GetEntry(), ctx.unitTarget->GetObjectGuid());

    // look for gameobject within max spell range of unitTarget, and respawn if found
    GameObject* pGo = NULL;

    float fMaxDist = GetSpellMaxRange(sSpellRangeStore.LookupEntry(ctx.m_spellInfo->RangeIndex));

    MaNGOS::NearestGameObjectEntryInPosRangeCheck go_check(*ctx.unitTarget, 187675, ctx.unitTarget->Where().X(), ctx.unitTarget->Where().Y(), ctx.unitTarget->Where().Z(), fMaxDist);
    MaNGOS::GameObjectSearcher<MaNGOS::NearestGameObjectEntryInPosRangeCheck> checker(pGo, go_check);

    Cell::VisitGridObjects(ctx.unitTarget, checker, fMaxDist);

    if (pGo && !pGo->isSpawned())
    {
        pGo->SetRespawnTime(MINUTE / 2);
        pGo->Refresh();
    }

    return SpellHandlerOutcome<void>::Return();
}

/// 46292: Cataclysm Breath: the caster casts on a player target one of eight spells at random (Corrosive Poison 46293,
/// Fevered Fatigue 46294, Hex 46295, Necrotic Poison 46296, Piercing Shadow 46297, Shrink 46298, Wavering Will 46299 or
/// Withered Touch 46300); the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericCataclysmBreath(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    uint32 spellId = 0;

    switch (urand(0, 7))
    {
        case 0: spellId = 46293; break;     // Corrosive Poison
        case 1: spellId = 46294; break;     // Fevered Fatigue
        case 2: spellId = 46295; break;     // Hex
        case 3: spellId = 46296; break;     // Necrotic Poison
        case 4: spellId = 46297; break;     // Piercing Shadow
        case 5: spellId = 46298; break;     // Shrink
        case 6: spellId = 46299; break;     // Wavering Will
        case 7: spellId = 46300; break;     // Withered Touch
    }

    ctx.m_caster->CastSpell(ctx.unitTarget, spellId, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 46372: Ice Spear Target Picker: the caster casts Summon Ice Spear Bunny (46359) on a player target; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericIceSpearTargetPicker(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.m_caster->CastSpell(ctx.unitTarget, 46359, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 46485: The Greatmother's Soulcatcher: the caster casts Player Force Cast (46486) on a creature target and, read as a
/// player, gets the kill credit of the creature named by the spell that 46486 triggers, and the target despawns; the
/// effect ends
static SpellHandlerOutcome<void> EffectDummyGenericTheGreatmothersSoulcatcher(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_UNIT)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    if (const SpellEntry* pSpell = sSpellStore.LookupEntry(46486))
    {
        ctx.m_caster->CastSpell(ctx.unitTarget, pSpell, true);

        if (SpellEffectEntry const* pSpellEffect = pSpell->GetSpellEffect(EFFECT_INDEX_0))
            if (const SpellEntry *pSpellCredit = sSpellStore.LookupEntry(pSpellEffect->EffectTriggerSpell))
                if (SpellEffectEntry const* pSpellCreditEffect = pSpellCredit->GetSpellEffect(EFFECT_INDEX_0))
                {
                    ((Player*)ctx.m_caster)->KilledMonsterCredit(pSpellCreditEffect->EffectMiscValue_0);
                }

        ((Creature*)ctx.unitTarget)->ForcedDespawn();
    }

    return SpellHandlerOutcome<void>::Return();
}

/// 46606: Tillinghast's Plague Canister Dummy: a creature target casts Green Eggs and Whelps: Summon Plagued
/// Proto-Whelp (43160) on the caster, dies and has its health set to zero; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericTillinghastsPlagueCanisterDummy(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_UNIT)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.unitTarget->CastSpell(ctx.m_caster, 43160, true);
    ctx.unitTarget->SetDeathState(JUST_DIED);
    ctx.unitTarget->SetHealth(0);
    return SpellHandlerOutcome<void>::Return();
}

/// 46671, 46672: Cleansing Flames: a player target casts on itself Create Flame of the Exodar (46690) for 46671 or
/// Create Flame of Silvermoon (46689) for 46672; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericCleansingFlamesExodar(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.unitTarget->CastSpell(ctx.unitTarget, ctx.m_spellInfo->ID == 46671 ? 46690 : 46689, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 46797: Quest - Borean Tundra - Set Explosives Cart: for a player caster and a unit target, the caster gets the
/// target's kill credit and the target casts Quest - Borean Tundra - Summon Explosives Cart (46798) on itself; the
/// effect ends
static SpellHandlerOutcome<void> EffectDummyGenericSetExplosivesCart(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget || ctx.m_caster->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ((Player*)ctx.m_caster)->KilledMonsterCredit(ctx.unitTarget->GetEntry(), ctx.unitTarget->GetObjectGuid());

    // Quest - Borean Tundra - Summon Explosives Cart
    ctx.unitTarget->CastSpell(ctx.unitTarget, 46798, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 47110: Summon Drakuru's Image: a caster in area 4255, 4209, 4270, 4216 or 4196 casts on itself that area's Reagent
/// Check (47381, 47386, 47389, 47408 or 50441) through the cast item, as from itself and triggered by this spell; the
/// effect ends
static SpellHandlerOutcome<void> EffectDummyGenericSummonDrakurusImage(SpellEffectDummyGenericContext& ctx)
{
    uint32 spellId = 0;

    // Spell 47117,47149,47316,47405,50439 exist, are these used to check area/meet requirement
    // and to cast correct spell in correct area?

    switch (ctx.m_caster->GetTerrain()->GetAreaId(ctx.m_caster->Where().X(), ctx.m_caster->Where().Y(), ctx.m_caster->Where().Z()))
    {
        case 4255: spellId = 47381; break;  // Reagent Check (Frozen Mojo)
        case 4209: spellId = 47386; break;  // Reagent Check (Zim'Bo's Mojo)
        case 4270: spellId = 47389; break;  // Reagent Check (Desperate Mojo)
        case 4216: spellId = 47408; break;  // Reagent Check (Sacred Mojo)
        case 4196: spellId = 50441; break;  // Reagent Check (Survival Mojo)
    }

    // The additional castspell arguments are needed here to remove reagents for triggered spells
    if (spellId)
    {
        ctx.m_caster->CastSpell(ctx.m_caster, spellId, true, ctx.m_CastItem, NULL, ctx.m_caster->GetObjectGuid(), ctx.m_spellInfo);
    }

    return SpellHandlerOutcome<void>::Return();
}

/// 47170: Impale Leviroth: a creature target goes to eight percent health, casts Cosmetic - Underwater Blood (no sound)
/// (47172) on itself, and its AI attacks the caster; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericImpaleLeviroth(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_UNIT)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.unitTarget->SetHealthPercent(8.0f);

    // Cosmetic - Underwater Blood (no sound)
    ctx.unitTarget->CastSpell(ctx.unitTarget, 47172, true);

    ctx.unitTarget->AI()->AttackStart(ctx.m_caster);
    return SpellHandlerOutcome<void>::Return();
}

/// 47176: Infect Ice Troll: the unit target, if any, casts Plague Effect Self (47178) on itself; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericInfectIceTroll(SpellEffectDummyGenericContext& ctx)
{
    // Spell has wrong areaGroupid, so it can not be casted where expected.
    // TODO: research if spells casted by NPC, having TARGET_SCRIPT, can have disabled area check
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // Plague Effect Self
    ctx.unitTarget->CastSpell(ctx.unitTarget, 47178, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 47305: Potent Explosive Charge: a creature target at eighty percent health or less casts Quest - Jormungar Explosion
/// Spell Spawner (47311) and Potent Explosive Charge (47306) on itself; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericPotentExplosiveCharge(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_UNIT)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // only if below 80% hp
    if (ctx.unitTarget->GetHealthPercent() > 80.0f)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // Issues with explosion animation (remove insta kill spell resolves the issue)

    // Quest - Jormungar Explosion Spell Spawner
    ctx.unitTarget->CastSpell(ctx.unitTarget, 47311, true);

    // Potent Explosive Charge
    ctx.unitTarget->CastSpell(ctx.unitTarget, 47306, true);

    return SpellHandlerOutcome<void>::Return();
}

/// 47381, 47386, 47389, 47408, 50441: Reagent Check: a player caster casts on itself the Envision Drakuru of the
/// label's mojo (47118, 47150, 47317, 47406 or 50440); the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericReagentCheck(SpellEffectDummyGenericContext& ctx)
{
    if (ctx.m_caster->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    switch (ctx.m_spellInfo->ID)
    {
        case 47381:
            // Envision Drakuru
            ctx.m_caster->CastSpell(ctx.m_caster, 47118, true);
            break;
        case 47386:
            ctx.m_caster->CastSpell(ctx.m_caster, 47150, true);
            break;
        case 47389:
            ctx.m_caster->CastSpell(ctx.m_caster, 47317, true);
            break;
        case 47408:
            ctx.m_caster->CastSpell(ctx.m_caster, 47406, true);
            break;
        case 50441:
            ctx.m_caster->CastSpell(ctx.m_caster, 50440, true);
            break;
    }

    return SpellHandlerOutcome<void>::Return();
}

/// 48046: Use Camera: with a unit target, the caster casts Iron Dwarf Snapshot Credit (48047) on itself through the
/// cast item, as from the target; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericUseCamera(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // No despawn expected, nor any change in dynamic flags/other flags.
    // Need internal way to track if credit has been given for this object.

    // Iron Dwarf Snapshot Credit
    ctx.m_caster->CastSpell(ctx.m_caster, 48047, true, ctx.m_CastItem, NULL, ctx.unitTarget->GetObjectGuid());
    return SpellHandlerOutcome<void>::Return();
}

/// 48790: Neltharion's Flame: the unit target, if any, casts Neltharion's Flame Fire Bunny: Periodic Fire Aura (48786)
/// on itself; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericNeltharionsFlame(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // Neltharion's Flame Fire Bunny: Periodic Fire Aura
    ctx.unitTarget->CastSpell(ctx.unitTarget, 48786, false);
    return SpellHandlerOutcome<void>::Return();
}

/// 49357: Brewfest Mount Transformation: a mounted player caster is dismounted and casts on itself, at a run speed rate
/// of two or more, Swift Brewfest Ram (43900) for the Alliance or Great Brewfest Kodo (49379) for the Horde, and below
/// it Brewfest Ram (43899) or Brewfest Riding Kodo (49378); the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericBrewfestMountTransformation(SpellEffectDummyGenericContext& ctx)
{
    if (ctx.m_caster->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    if (!ctx.m_caster->HasAuraType(SPELL_AURA_MOUNTED))
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.m_caster->RemoveSpellsCausingAura(SPELL_AURA_MOUNTED);

    // Ram for Alliance, Kodo for Horde
    if (((Player*)ctx.m_caster)->GetTeam() == ALLIANCE)
    {
        if (ctx.m_caster->GetSpeedRate(MOVE_RUN) >= 2.0f)
            // 100% Ram
            ctx.m_caster->CastSpell(ctx.m_caster, 43900, true);
        else
            // 60% Ram
            ctx.m_caster->CastSpell(ctx.m_caster, 43899, true);
    }
    else
    {
        if (ctx.m_caster->GetSpeedRate(MOVE_RUN) >= 2.0f)
            // 100% Kodo
            ctx.m_caster->CastSpell(ctx.m_caster, 49379, true);
        else
            // 60% Kodo
            ctx.m_caster->CastSpell(ctx.m_caster, 49378, true);
    }
    return SpellHandlerOutcome<void>::Return();
}

/// 49634: Sergeant's Flare: the caster casts Towers of Certain Doom: Tower Bunny Smoke Flare Effect (56511) on a
/// creature target, then on itself the first of the four Towers of Certain Doom kill credits (43077, 43067, 43087,
/// 43086) whose first effect names the target's entry; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericSergeantsFlare(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_UNIT)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // Towers of Certain Doom: Tower Bunny Smoke Flare Effect
    // TODO: MaNGOS::DynamicObjectUpdater::VisitHelper prevent aura to be applied to dummy creature (see HandleAuraDummy for effect of aura)
    ctx.m_caster->CastSpell(ctx.unitTarget, 56511, true);

    static uint32 const spellCredit[4] =
    {
        43077,                              // E Kill Credit
        43067,                              // NW Kill Credit
        43087,                              // SE Kill Credit
        43086,                              // SW Kill Credit
    };

    // for sizeof(spellCredit)
    for (int i = 0; i < 4; ++i)
    {
        const SpellEntry* pSpell = sSpellStore.LookupEntry(spellCredit[i]);

        if (pSpell->GetEffectMiscValue(EFFECT_INDEX_0) == ctx.unitTarget->GetEntry())
        {
            ctx.m_caster->CastSpell(ctx.m_caster, spellCredit[i], true);
            break;
        }
    }

    return SpellHandlerOutcome<void>::Return();
}

/// 49859: Rune of Command: a creature target casts Captive Stone Giant Kill Credit (43564) on the caster and despawns;
/// the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericRuneOfCommand(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_UNIT)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // Captive Stone Giant Kill Credit
    ctx.unitTarget->CastSpell(ctx.m_caster, 43564, true);
    // Is it supposed to despawn?
    ((Creature*)ctx.unitTarget)->ForcedDespawn();
    return SpellHandlerOutcome<void>::Return();
}

/// 50133: Scourging Crystal Controller: for a creature target with Scourge Mur'gul Camp: Force Shield Arcane Purple x3
/// (43874) and without Scourging Crystal Controller (43878), the caster casts 43878 on it through the cast item; the
/// effect ends
static SpellHandlerOutcome<void> EffectDummyGenericScourgingCrystalController(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_UNIT)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // Scourge Mur'gul Camp: Force Shield Arcane Purple x3
    if (ctx.unitTarget->HasAura(43874))
    {
        // someone else is already channeling target
        if (ctx.unitTarget->HasAura(43878))
        {
            return SpellHandlerOutcome<void>::Return();
        }

        // Scourging Crystal Controller
        ctx.m_caster->CastSpell(ctx.unitTarget, 43878, true, ctx.m_CastItem);
    }

    return SpellHandlerOutcome<void>::Return();
}

/// 50243: Teach Language: a player caster casts on itself, 34 times in 100, 1000001 (50242) for the Alliance or
/// 01001000 (50246) for the Horde; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericTeachLanguage(SpellEffectDummyGenericContext& ctx)
{
    if (ctx.m_caster->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // spell has a 1/3 chance to trigger one of the below
    if (roll_chance_i(66))
    {
        return SpellHandlerOutcome<void>::Return();
    }

    if (((Player*)ctx.m_caster)->GetTeam() == ALLIANCE)
    {
        // 1000001 - gnomish binary
        ctx.m_caster->CastSpell(ctx.m_caster, 50242, true);
    }
    else
    {
        // 01001000 - goblin binary
        ctx.m_caster->CastSpell(ctx.m_caster, 50246, true);
    }

    return SpellHandlerOutcome<void>::Return();
}

/// 50440: Envision Drakuru: the unit target, if any, casts Script Cast Summon Image of Drakuru 05 (50439) on itself;
/// the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericEnvisionDrakuru(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // Script Cast Summon Image of Drakuru 05
    ctx.unitTarget->CastSpell(ctx.unitTarget, 50439, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 50546: The Focus on the Beach: Ley Line Focus Control Ring Effect; 50547: Atop the Woodlands: Ley Line Focus Control
/// Amulet Effect; 50548: The End of the Line: Ley Line Focus Control Talisman Effect: with an original caster, a
/// creature target casts on it the label's Ley Line Focus Bunny Beam (47390, 47472 or 47635); the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericLeyLineFocusControl(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.m_originalCaster || !ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_UNIT)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    switch (ctx.m_spellInfo->ID)
    {
        case 50546: ctx.unitTarget->CastSpell(ctx.m_originalCaster, 47390, true); break;
        case 50547: ctx.unitTarget->CastSpell(ctx.m_originalCaster, 47472, true); break;
        case 50548: ctx.unitTarget->CastSpell(ctx.m_originalCaster, 47635, true); break;
    }

    return SpellHandlerOutcome<void>::Return();
}

/// 51276: Incinerate Corpse: a creature target casts Incineration (51278) on itself and Defeated Argent Footman KC
/// (51279) on the caster, and dies; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericIncinerateCorpse(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_UNIT)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.unitTarget->CastSpell(ctx.unitTarget, 51278, true);
    ctx.unitTarget->CastSpell(ctx.m_caster, 51279, true);

    ctx.unitTarget->SetDeathState(JUST_DIED);
    return SpellHandlerOutcome<void>::Return();
}

/// 51330: Shoot RJR: with a unit target, the caster casts on it through the cast item, 75 times in 100, Miss Apple
/// (51332) or Miss Apple, Hit Bird (51366) at even odds, otherwise Hit Apple (51331); the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericShootRJR(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // guessed chances
    if (roll_chance_i(75))
    {
        ctx.m_caster->CastSpell(ctx.unitTarget, roll_chance_i(50) ? 51332 : 51366, true, ctx.m_CastItem);
    }
    else
    {
        ctx.m_caster->CastSpell(ctx.unitTarget, 51331, true, ctx.m_CastItem);
    }

    return SpellHandlerOutcome<void>::Return();
}

/// 51333: Dig For Treasure: with a unit target, the caster casts Summon Drakkari Spectre (51370) on it through the cast
/// item 75 times in 100, otherwise Summon Ancient Drakkari Chest (51345) on itself; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericDigForTreasure(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    if (roll_chance_i(75))
    {
        ctx.m_caster->CastSpell(ctx.unitTarget, 51370, true, ctx.m_CastItem);
    }
    else
    {
        ctx.m_caster->CastSpell(ctx.m_caster, 51345, true);
    }

    return SpellHandlerOutcome<void>::Return();
}

/// 51336: Magic Pull: the caster casts Magic Pull (50770) on the unit target, if any; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericMagicPull(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.m_caster->CastSpell(ctx.unitTarget, 50770, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 51420: Digging for Treasure Ping: for a creature target and a caster with a protector pet, the pet faces the target
/// and casts Digging for Treasure (51405) on it, and the target despawns after a millisecond; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericDiggingForTreasurePing(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_UNIT)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // only spell related protector pets exist currently
    Pet* pPet = ctx.m_caster->GetProtectorPet();
    if (!pPet)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    pPet->SetFacingToObject(ctx.unitTarget);

    // Digging for Treasure
    pPet->CastSpell(ctx.unitTarget, 51405, true);

    ((Creature*)ctx.unitTarget)->ForcedDespawn(1);
    return SpellHandlerOutcome<void>::Return();
}

/// 51582: Rocket Boots Engaged (the label adds Rocket Boots Xtreme and Rocket Boots Xtreme Lite): a player caster in a
/// battleground reports its dropped flag to it, and casts Rocket Boots Engaged (30452) on itself; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericRocketBootsEngaged(SpellEffectDummyGenericContext& ctx)
{
    if (ctx.m_caster->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    if (BattleGround* bg = ((Player*)ctx.m_caster)->GetBattleGround())
    {
        bg->EventPlayerDroppedFlag((Player*)ctx.m_caster);
    }

    ctx.m_caster->CastSpell(ctx.m_caster, 30452, true, NULL);
    return SpellHandlerOutcome<void>::Return();
}

/// 51840: Despawn Fruit Tosser: a creature target casts on the caster Summon Adventurous Dwarf (52070) one time in
/// five, otherwise one of Bananas Fall to Ground (51836), Orange Falls to Ground (51837) and Papaya Falls to Ground
/// (51839) at random, and despawns after five seconds; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericDespawnFruitTosser(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_UNIT)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    if (roll_chance_i(20))
    {
        // summon NPC, or...
        ctx.unitTarget->CastSpell(ctx.m_caster, 52070, true);
    }
    else
    {
        // ...drop banana, orange or papaya
        switch (urand(0, 2))
        {
            case 0: ctx.unitTarget->CastSpell(ctx.m_caster, 51836, true); break;
            case 1: ctx.unitTarget->CastSpell(ctx.m_caster, 51837, true); break;
            case 2: ctx.unitTarget->CastSpell(ctx.m_caster, 51839, true); break;
        }
    }

    ((Creature*)ctx.unitTarget)->ForcedDespawn(5000);
    return SpellHandlerOutcome<void>::Return();
}

/// 51964: Tormentor's Incense: a creature target with an AI attacks the caster; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericTormentorsIncense(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_UNIT)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // This might not be the best way, and effect may need some adjustment. Removal of any aura from surrounding dummy creatures?
    if (ctx.unitTarget->AI())
    {
        ctx.unitTarget->AI()->AttackStart(ctx.m_caster);
    }

    return SpellHandlerOutcome<void>::Return();
}

/// 51858: Siphon of Acherus: with an original caster and a creature target, the player the original caster is, is owned
/// by or is charmed by gets the target's kill credit; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericSiphonOfAcherus(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.m_originalCaster || !ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_UNIT)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    if (Player* pPlayer = ctx.m_originalCaster->GetCharmerOrOwnerPlayerOrPlayerItself())
    {
        pPlayer->KilledMonsterCredit(ctx.unitTarget->GetEntry(), ctx.unitTarget->GetObjectGuid());
    }

    return SpellHandlerOutcome<void>::Return();
}

/// 51866: Kick Nass
static SpellHandlerOutcome<void> EffectDummyGeneric51866(SpellEffectDummyGenericContext& ctx)
{
    // It is possible that Nass Heartbeat (spell id 61438) is involved in this
    // If so, unclear how it should work and using the below instead (even though it could be a bit hack-ish)

    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_UNIT)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // Only own guardian pet
    if (ctx.m_caster != ctx.unitTarget->GetOwner())
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // This means we already set state (see below) and need to wait.
    if (ctx.unitTarget->Blocked(Motion::ReasonRooted))
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // Expecting pTargetDummy to be summoned by AI at death of target creatures.

    Creature* pTargetDummy = NULL;
    float fRange = GetSpellMaxRange(sSpellRangeStore.LookupEntry(ctx.m_spellInfo->RangeIndex));

    MaNGOS::NearestCreatureEntryWithLiveStateInObjectRangeCheck u_check(*ctx.m_caster, 28523, true, false, fRange * 2);
    MaNGOS::CreatureLastSearcher<MaNGOS::NearestCreatureEntryWithLiveStateInObjectRangeCheck> searcher(pTargetDummy, u_check);

    Cell::VisitGridObjects(ctx.m_caster, searcher, fRange * 2);

    if (pTargetDummy)
    {
        if (ctx.unitTarget->GetMotionMaster()->Latches().follow || ctx.unitTarget->GetMotionMaster()->Latches().followLeg)
        {
            ctx.unitTarget->GetMotionMaster()->MovementExpired();
        }

        ctx.unitTarget->MonsterMoveWithSpeed(pTargetDummy->Where().X(), pTargetDummy->Where().Y(), pTargetDummy->Where().Z(), 24.f);

        // Add state to temporarily prevent follow
        ctx.unitTarget->GetMotionMaster()->Inhibit(Motion::Inhibition::Rooted, Motion::ControlClaim(51866, 0, ctx.m_caster->GetObjectGuid().GetCounter()));

        // Collect Hair Sample
        ctx.unitTarget->CastSpell(pTargetDummy, 51870, true);
    }

    return SpellHandlerOutcome<void>::Return();
}

/// 51872: Hair Sample Collected
static SpellHandlerOutcome<void> EffectDummyGeneric51872(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_UNIT)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // clear state to allow follow again: m_caster here is the guardian pet
    // rooted by Kick Nass (spell 51866; that effect's unitTarget), so the
    // release must use its owner's counter to match the claim it was given.
    // Without an owner there is no way to reconstruct that identity; guessing
    // one (e.g. the pet's own counter) would silently leak the root instead, so
    // skip the release and say so.
    if (Unit* owner = ctx.m_caster->GetOwner())
    {
        ctx.m_caster->GetMotionMaster()->Uninhibit(Motion::Inhibition::Rooted,
            Motion::ControlClaim(51866, 0, owner->GetObjectGuid().GetCounter()));
    }
    else
    {
        sLog.outError("SpellEffectDummy: spell 51872 (Hair Sample Collected) found no owner for %s to release its Kick Nass root", ctx.m_caster->GetGuidStr().c_str());
    }

    // Nass Kill Credit
    ctx.m_caster->CastSpell(ctx.m_caster, 51871, true);

    // Despawn dummy creature
    ((Creature*)ctx.unitTarget)->ForcedDespawn();

    return SpellHandlerOutcome<void>::Return();
}

/// 52308: Take Sputum Sample: on the first effect, a caster with the aura the second effect's value names casts on
/// itself the spell the first effect's value names; the second and third effects do nothing; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericTakeSputumSample(SpellEffectDummyGenericContext& ctx)
{
    switch(ctx.effect->EffectIndex)
    {
        case EFFECT_INDEX_0:
        {
            uint32 spellID = ctx.m_spellInfo->CalculateSimpleValue(EFFECT_INDEX_0);
            uint32 reqAuraID = ctx.m_spellInfo->CalculateSimpleValue(EFFECT_INDEX_1);

            if (ctx.m_caster->HasAura(reqAuraID, EFFECT_INDEX_0))
            {
                ctx.m_caster->CastSpell(ctx.m_caster, spellID, true, NULL);
            }
            return SpellHandlerOutcome<void>::Return();
        }
        case EFFECT_INDEX_1:                // additional data for dummy[0]
        case EFFECT_INDEX_2:
            return SpellHandlerOutcome<void>::Return();
    }
    return SpellHandlerOutcome<void>::Return();
}

/// 52369, 52371: Detonate Explosives: the unit target, if any, casts Cosmetic - Explosion (46419) on itself, and every
/// game object 182071 within the spell's range of the target that is not spawned is spawned for half a minute; the
/// effect ends
static SpellHandlerOutcome<void> EffectDummyGenericDetonateExplosives(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // Cosmetic - Explosion
    ctx.unitTarget->CastSpell(ctx.unitTarget, 46419, true);

    // look for gameobjects within max spell range of unitTarget, and respawn if found
    std::list<GameObject*> lList;

    float fMaxDist = GetSpellMaxRange(sSpellRangeStore.LookupEntry(ctx.m_spellInfo->RangeIndex));

    MaNGOS::GameObjectEntryInPosRangeCheck go_check(*ctx.unitTarget, 182071, ctx.unitTarget->Where().X(), ctx.unitTarget->Where().Y(), ctx.unitTarget->Where().Z(), fMaxDist);
    MaNGOS::GameObjectListSearcher<MaNGOS::GameObjectEntryInPosRangeCheck> checker(lList, go_check);

    Cell::VisitGridObjects(ctx.unitTarget, checker, fMaxDist);

    for (std::list<GameObject*>::iterator iter = lList.begin(); iter != lList.end(); ++iter)
    {
        if (!(*iter)->isSpawned())
        {
            (*iter)->SetRespawnTime(MINUTE / 2);
            (*iter)->Refresh();
        }
    }

    return SpellHandlerOutcome<void>::Return();
}

/// 52759: Ancestral Awakening: the caster casts on the unit target, if any, Ancestral Awakening (52752) with the
/// effect's damage as its first value; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericAncestralAwakening(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.m_caster->CastCustomSpell(ctx.unitTarget, 52752, &ctx.damage, NULL, NULL, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 52845: Brewfest Mount Transformation (Faction Swap): a mounted player caster loses its mount auras and casts on
/// itself, for the Horde, Swift Brewfest Ram (43900) at a run speed rate of two or more and Brewfest Ram (43899) below
/// it, otherwise Great Brewfest Kodo (49379) or Brewfest Riding Kodo (49378) by the same rate; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericBrewfestMountFactionSwap(SpellEffectDummyGenericContext& ctx)
{
    if (ctx.m_caster->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    if (!ctx.m_caster->HasAuraType(SPELL_AURA_MOUNTED))
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.m_caster->RemoveSpellsCausingAura(SPELL_AURA_MOUNTED);

    // Ram for Horde, Kodo for Alliance
    if (((Player*)ctx.m_caster)->GetTeam() == HORDE)
    {
        if (ctx.m_caster->GetSpeedRate(MOVE_RUN) >= 2.0f)
            // Swift Brewfest Ram, 100% Ram
            ctx.m_caster->CastSpell(ctx.m_caster, 43900, true);
        else
            // Brewfest Ram, 60% Ram
            ctx.m_caster->CastSpell(ctx.m_caster, 43899, true);
    }
    else
    {
        if (ctx.m_caster->GetSpeedRate(MOVE_RUN) >= 2.0f)
            // Great Brewfest Kodo, 100% Kodo
            ctx.m_caster->CastSpell(ctx.m_caster, 49379, true);
        else
            // Brewfest Riding Kodo, 60% Kodo
            ctx.m_caster->CastSpell(ctx.m_caster, 49378, true);
    }
    return SpellHandlerOutcome<void>::Return();
}

/// 53341: Rune of Cinderglacier; 53343: Rune of Razorice: the caster casts Runeforging Credit (54586) on itself; the
/// effect ends
static SpellHandlerOutcome<void> EffectDummyGenericRuneOfCinderglacierRazorice(SpellEffectDummyGenericContext& ctx)
{
    // Runeforging Credit
    ctx.m_caster->CastSpell(ctx.m_caster, 54586, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 53475: Set Oracle Faction Friendly; 53487: Set Wolvar Faction Honored; 54015: Set Oracle Faction Honored: on the
/// first effect, a player caster whose base reputation with the faction the first effect's base points name is below
/// the second effect's base points has that reputation set to them; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericSetOracleWolvarFaction(SpellEffectDummyGenericContext& ctx)
{
    if (ctx.m_caster->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    if (ctx.effect->EffectIndex == EFFECT_INDEX_0)
    {
        Player* pPlayer = (Player*)ctx.m_caster;

        uint32 faction_id = ctx.m_currentBasePoints[ctx.effect->EffectIndex];
        int32  rep_change = ctx.m_currentBasePoints[EFFECT_INDEX_1];

        FactionEntry const* factionEntry = sFactionStore.LookupEntry(faction_id);

        if (!factionEntry)
        {
            return SpellHandlerOutcome<void>::Return();
        }

        // Set rep to baserep + basepoints (expecting spillover for oposite faction -> become hated)
        // Not when player already has equal or higher rep with this faction
        if (pPlayer->GetReputationMgr().GetBaseReputation(factionEntry) < rep_change)
        {
            pPlayer->SetReputation(factionEntry, rep_change);
        }

        // EFFECT_INDEX_2 most likely update at war state, we already handle this in SetReputation
    }

    return SpellHandlerOutcome<void>::Return();
}

/// 53808: Pygmy Oil: a caster holding Pygmy Oil (53805) loses it and casts Pygmy Oil (53806) on itself with a chance of
/// five percent per stack; otherwise a caster without Pygmy Oil (53806) casts Pygmy Oil (53805) on itself; the effect
/// ends
static SpellHandlerOutcome<void> EffectDummyGenericPygmyOil(SpellEffectDummyGenericContext& ctx)
{
    const uint32 spellShrink = 53805;
    const uint32 spellTransf = 53806;

    if (SpellAuraHolder* holder = ctx.m_caster->GetSpellAuraHolder(spellShrink))
    {
        // chance to become pygmified (5, 10, 15 etc)
        if (roll_chance_i(holder->GetStackAmount() * 5))
        {
            ctx.m_caster->RemoveAurasDueToSpell(spellShrink);
            ctx.m_caster->CastSpell(ctx.m_caster, spellTransf, true);
            return SpellHandlerOutcome<void>::Return();
        }
    }

    if (ctx.m_caster->HasAura(spellTransf))
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.m_caster->CastSpell(ctx.m_caster, spellShrink, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 54577: Throw U.D.E.D.: a creature target casts Permanent Feign Death (29266) on itself, despawns after two seconds,
/// and casts Mammoth Explosion Spell Spawner (54581) on itself through the cast item; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericThrowUDED(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_UNIT)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // Sometimes issues with explosion animation. Unclear why
    // but possibly caused by the order of spells.

    // Permanent Feign Death
    ctx.unitTarget->CastSpell(ctx.unitTarget, 29266, true);

    // need to despawn later
    ((Creature*)ctx.unitTarget)->ForcedDespawn(2000);

    // Mammoth Explosion Spell Spawner
    ctx.unitTarget->CastSpell(ctx.unitTarget, 54581, true, ctx.m_CastItem);
    return SpellHandlerOutcome<void>::Return();
}

/// 54850: Emerge: the caster casts Emerge Summon (54851) on itself; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericEmerge(SpellEffectDummyGenericContext& ctx)
{
    // Cast Emerge summon
    ctx.m_caster->CastSpell(ctx.m_caster, 54851, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 54092: Monster Slayer's Kit: the caster casts on the unit target, if any, one of Monster Slayer's Axe (51853),
/// Monster Slayer's Symbol (54063), Monster Slayer's Dagger (54071) and Monster Slayer's Holy Water (54086) at random;
/// the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericMonsterSlayersKit(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    uint32 spellIds[] = {51853, 54063, 54071, 54086};
    ctx.m_caster->CastSpell(ctx.unitTarget, spellIds[urand(0, 3)], true);
    return SpellHandlerOutcome<void>::Return();
}

/// 55004: Nitro Boosts: with a cast item, the caster casts on itself through the item Nitro Boosts (54861) ninety-five
/// times in a hundred, otherwise Knocked Up (46014); the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericNitroBoosts(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.m_CastItem)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    if (roll_chance_i(95))                  // Nitro Boosts - success
    {
        ctx.m_caster->CastSpell(ctx.m_caster, 54861, true, ctx.m_CastItem);
    }
    else                                    // Knocked Up   - backfire 5%
    {
        ctx.m_caster->CastSpell(ctx.m_caster, 46014, true, ctx.m_CastItem);
    }

    return SpellHandlerOutcome<void>::Return();
}

/// 55818: Hurl Boulder: the caster casts Summon Earthen (55528) on itself three to five times; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericHurlBoulder(SpellEffectDummyGenericContext& ctx)
{
    // unclear how many summon min/max random, best guess below
    uint32 random = urand(3, 5);

    for (uint32 i = 0; i < random; ++i)
    {
        ctx.m_caster->CastSpell(ctx.m_caster, 55528, true);
    }

    return SpellHandlerOutcome<void>::Return();
}

/// 56430: Arcane Bomb: the unit target, if any, casts Arcane Bomb (56431) and Arcane Overload (56432) on itself as from
/// the caster; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericArcaneBomb(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.unitTarget->CastSpell(ctx.unitTarget, 56431, true, NULL, NULL, ctx.m_caster->GetObjectGuid());
    ctx.unitTarget->CastSpell(ctx.unitTarget, 56432, true, NULL, NULL, ctx.m_caster->GetObjectGuid());
    return SpellHandlerOutcome<void>::Return();
}

/// 57578: Lava Strike: the caster casts on the unit target, if any, the spell the effect's value names; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericLavaStrike(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.m_caster->CastSpell(ctx.unitTarget, ctx.effect->CalculateSimpleValue(), true);
    return SpellHandlerOutcome<void>::Return();
}

/// 57908: Stain Cloth: the spell finishes, the caster casts Create Cloth (57915) on itself through the cast item, not
/// triggered, and the cast item is cleared; the effect goes on to the script library
static SpellHandlerOutcome<void> EffectDummyGenericStainCloth(SpellEffectDummyGenericContext& ctx)
{
    // nothing do more
    ctx.spell->finish();

    ctx.m_caster->CastSpell(ctx.m_caster, 57915, false, ctx.m_CastItem);

    // cast item deleted
    ctx.spell->ClearCastItem();
    return SpellHandlerOutcome<void>::Continue();
}

/// 58418: Portal to Orgrimmar; 58420: Portal to Stormwind: the script library implements them, and the server does
/// nothing here; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericPortalToOrgrimmarStormwind(SpellEffectDummyGenericContext& /*ctx*/)
{
    return SpellHandlerOutcome<void>::Return();                                 // implemented in EffectScript[0]
}

/// 58601: Remove Flight Auras: the caster loses the spells that give it the fly and the mounted flight speed auras; the
/// effect ends
static SpellHandlerOutcome<void> EffectDummyGenericRemoveFlightAuras(SpellEffectDummyGenericContext& ctx)
{
    ctx.m_caster->RemoveSpellsCausingAura(SPELL_AURA_FLY);
    ctx.m_caster->RemoveSpellsCausingAura(SPELL_AURA_MOD_FLIGHT_SPEED_MOUNTED);
    return SpellHandlerOutcome<void>::Return();
}

/// 59640: Underbelly Elixir: a player caster casts on itself one of the three Underbelly Elixir spells (59645, 59831 or
/// 59843) at random; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericUnderbellyElixir(SpellEffectDummyGenericContext& ctx)
{
    if (ctx.m_caster->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    uint32 spell_id = 0;
    switch (urand(1, 3))
    {
        case 1: spell_id = 59645; break;
        case 2: spell_id = 59831; break;
        case 3: spell_id = 59843; break;
    }

    ctx.m_caster->CastSpell(ctx.m_caster, spell_id, true, NULL);
    return SpellHandlerOutcome<void>::Return();
}

/// 60932: Disengage, the creature version: the caster casts Disengage (60934) on the unit target, if any; the effect
/// ends
static SpellHandlerOutcome<void> EffectDummyGenericDisengageCreature(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.m_caster->CastSpell(ctx.unitTarget, 60934, true, NULL);
    return SpellHandlerOutcome<void>::Return();
}

/// 62105: To'kini's Blowgun: a creature target casts Sleeping Sleep (62248) on itself, takes the caster as its loot
/// recipient, joins faction 190 (ambient, neutral) and is not attackable out of combat; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericTokinisBlowgun(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_UNIT)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // Sleeping Sleep
    ctx.unitTarget->CastSpell(ctx.unitTarget, 62248, true);

    // Although not really correct, it's needed to have access to m_caster later,
    // to properly process spell 62110 (cast from gossip).
    // Can possibly be replaced with a similar function that doesn't set any dynamic flags.
    ((Creature*)ctx.unitTarget)->SetLootRecipient(ctx.m_caster);

    ctx.unitTarget->setFaction(190);            // Ambient (neutral)
    ctx.unitTarget->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_OOC_NOT_ATTACKABLE);
    return SpellHandlerOutcome<void>::Return();
}

/// 62278: Lightning Orb Charger: the unit target, if any, casts Lightning Charge (62466) on the caster and Lightning
/// Charge (62279) on itself; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericLightningOrbCharger(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.unitTarget->CastSpell(ctx.m_caster, 62466, true);
    ctx.unitTarget->CastSpell(ctx.unitTarget, 62279, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 62652: Tidal Wave: the caster casts on a player target Tidal Wave (62653) on a regular-difficulty map, otherwise
/// Tidal Wave (62935); the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericTidalWave(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.m_caster->CastSpell(ctx.unitTarget, ctx.m_caster->GetMap()->IsRegularDifficulty() ? 62653 : 62935, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 62797: Storm Cloud: the caster casts on a player target Storm Cloud (65123) on a regular-difficulty map, otherwise
/// Storm Cloud (65133); the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericStormCloud(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.m_caster->CastSpell(ctx.unitTarget, ctx.m_caster->GetMap()->IsRegularDifficulty() ? 65123 : 65133, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 62907: Freya's Ward: the caster casts on the unit target, if any, the spell the effect's value names, five times;
/// the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericFreyasWard(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    for (uint8 i = 0; i < 5; ++i)
    {
        ctx.m_caster->CastSpell(ctx.unitTarget, ctx.effect->CalculateSimpleValue(), true);
    }
    return SpellHandlerOutcome<void>::Return();
}

/// 63030: Boil Ominously: the caster casts Summon Guardian (63031) on a player target; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericBoilOminously(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.m_caster->CastSpell(ctx.unitTarget, 63031, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 63499: Dispel Magic: the unit target, if any, loses the spell the effect's value names; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericDispelMagic(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.unitTarget->RemoveAurasDueToSpell(ctx.effect->CalculateSimpleValue());
    return SpellHandlerOutcome<void>::Return();
}

/// 63545: Icicle: the caster casts on a player target the spell the effect's value names; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericIcicle(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.m_caster->CastSpell(ctx.unitTarget, ctx.effect->CalculateSimpleValue(), true);
    return SpellHandlerOutcome<void>::Return();
}

/// 63744: Sara's Anger; 63745: Sara's Blessing; 63747: Sara's Fervor: the caster casts on the unit target, if any, the
/// spell the effect's value names; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericSarasAngerBlessingFervor(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.m_caster->CastSpell(ctx.unitTarget, ctx.effect->CalculateSimpleValue(), true);
    return SpellHandlerOutcome<void>::Return();
}

/// 63984: Hate to Zero: the threat the unit target, if any, holds on the caster's list, when it is not zero, is cut by
/// a hundred percent; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericHateToZero(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    if (ctx.m_caster->GetThreatManager().getThreat(ctx.unitTarget))
    {
        ctx.m_caster->GetThreatManager().modifyThreatPercent(ctx.unitTarget, -100);
    }
    return SpellHandlerOutcome<void>::Return();
}

/// 64172: Titanic Storm: a unit target with the aura the effect's value names takes direct damage from the caster equal
/// to its health; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericTitanicStorm(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget || !ctx.unitTarget->HasAura(ctx.effect->CalculateSimpleValue()))
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // There is no known spell to kill the target
    ctx.m_caster->DealDamage(ctx.unitTarget, ctx.unitTarget->GetHealth(), NULL, DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NORMAL, NULL, false);
    return SpellHandlerOutcome<void>::Return();
}

/// 64385: Spinning: the caster turns to a random facing; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericSpinning(SpellEffectDummyGenericContext& ctx)
{
    ctx.m_caster->SetFacingTo(frand(0, M_PI_F * 2));
    return SpellHandlerOutcome<void>::Return();
}

/// 64402: Rocket Strike: the unit target, if any, casts Rocket Strike (63681) on itself; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericRocketStrike(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.unitTarget->CastSpell(ctx.unitTarget, 63681, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 64489: Feral Rush: the caster casts Feral Rush (64496) on a player target; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericFeralRush(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.m_caster->CastSpell(ctx.unitTarget, 64496, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 64543: Melt Ice: the caster casts on the unit target, if any, the spell the effect's value names, and Melt Ice
/// (64540) on itself; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericMeltIce(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.m_caster->CastSpell(ctx.unitTarget, ctx.effect->CalculateSimpleValue(), true);
    ctx.m_caster->CastSpell(ctx.m_caster, 64540, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 64555: Insane Periodic: on a player target that has neither Sanity (63050) nor the spell the effect's value names,
/// the caster casts Insane (64464) and that spell; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericInsanePeriodic(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_PLAYER || ctx.unitTarget->HasAura(63050) || ctx.unitTarget->HasAura(ctx.effect->CalculateSimpleValue()))
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.m_caster->CastSpell(ctx.unitTarget, 64464, true);
    ctx.m_caster->CastSpell(ctx.unitTarget, ctx.effect->CalculateSimpleValue(), true);
    return SpellHandlerOutcome<void>::Return();
}

/// 64673: Feral Rush (h), the label's name for an id the client does not hold: the caster casts Feral Rush (64674) on a
/// player target; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericFeralRushHeroic(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.m_caster->CastSpell(ctx.unitTarget, 64674, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 64981: Summon Random Vanquished Tentacle: the caster casts on itself one of Summon Vanquished Crusher Tentacle
/// (64982), Summon Vanquished Constrictor Tentacle (64983) and Summon Vanquished Corruptor Tentacle (64984) at random;
/// the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericSummonRandomVanquishedTentacle(SpellEffectDummyGenericContext& ctx)
{
    uint32 spell_id = 0;

    switch (urand(0, 2))
    {
        case 0: spell_id = 64982; break;    // Summon Vanquished Crusher Tentacle
        case 1: spell_id = 64983; break;    // Summon Vanquished Constrictor Tentacle
        case 2: spell_id = 64984; break;    // Summon Vanquished Corruptor Tentacle
    }

    ctx.m_caster->CastSpell(ctx.m_caster, spell_id, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 65206: Destabilization Matrix: the caster casts on the unit target, if any, the spell the effect's value names; the
/// effect ends
static SpellHandlerOutcome<void> EffectDummyGenericDestabilizationMatrix(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.m_caster->CastSpell(ctx.unitTarget, ctx.effect->CalculateSimpleValue(), true);
    return SpellHandlerOutcome<void>::Return();
}

/// 65346: Proximity Mine: with a player target, the caster casts on itself Explosion (66351) on a regular-difficulty
/// map, otherwise Explosion (63009), loses Proximity Mine (65345) and is not selectable; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericProximityMine(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.m_caster->CastSpell(ctx.m_caster, ctx.m_caster->GetMap()->IsRegularDifficulty() ? 66351 : 63009, true);
    ctx.m_caster->RemoveAurasDueToSpell(65345);
    ctx.m_caster->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
    return SpellHandlerOutcome<void>::Return();
}

/// 65869: Disengage: the unit target, if any, casts Disengage (65870) on itself; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericDisengage(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.unitTarget->CastSpell(ctx.unitTarget, 65870, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 66312: Light Ball Passive: for a creature caster and a player target, the target casts Powering Up (67590) on itself
/// when it has Light Essence (65686) in the map's difficulty, otherwise the caster casts Unleashed Light (65795) on
/// itself, and the caster despawns; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericLightBallPassive(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget || ctx.m_caster->GetTypeId() != TYPEID_UNIT)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    if (ctx.unitTarget->GetTypeId() == TYPEID_PLAYER)
    {
        if (ctx.unitTarget->HasAuraOfDifficulty(65686))
        {
            ctx.unitTarget->CastSpell(ctx.unitTarget, 67590, true);
        }
        else
        {
            ctx.m_caster->CastSpell(ctx.m_caster, 65795, true);
        }

        ((Creature*)ctx.m_caster)->ForcedDespawn();
    }
    return SpellHandlerOutcome<void>::Return();
}

/// 66314: Dark Ball Passive: for a creature caster and a player target, the target casts Powering Up (67590) on itself
/// when it has Dark Essence (65684) in the map's difficulty, otherwise the caster casts Unleashed Dark (65808) on
/// itself, and the caster despawns; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericDarkBallPassive(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget || ctx.m_caster->GetTypeId() != TYPEID_UNIT)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    if (ctx.unitTarget->GetTypeId() == TYPEID_PLAYER)
    {
        if (ctx.unitTarget->HasAuraOfDifficulty(65684))
        {
            ctx.unitTarget->CastSpell(ctx.unitTarget, 67590, true);
        }
        else
        {
            ctx.m_caster->CastSpell(ctx.m_caster, 65808, true);
        }

        ((Creature*)ctx.m_caster)->ForcedDespawn();
    }
    return SpellHandlerOutcome<void>::Return();
}

/// 66390: Read Last Rites: for a player caster and a creature target, the target casts Summon Tualiq Proxy (66411) and
/// Summon Tualiq Spirit (66412) on itself, the caster gets the target's kill credit, and the target despawns after a
/// second; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericReadLastRites(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_UNIT || ctx.m_caster->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // Summon Tualiq Proxy
    // Not known what purpose this has
    ctx.unitTarget->CastSpell(ctx.unitTarget, 66411, true);

    // Summon Tualiq Spirit
    // Offtopic note: the summoned has aura from spell 37119 and 66419. One of them should
    // most likely make summoned "rise", hover up/sideways in the air (MOVEFLAG_LEVITATING + MOVEFLAG_HOVER)
    ctx.unitTarget->CastSpell(ctx.unitTarget, 66412, true);

    ((Player*)ctx.m_caster)->KilledMonsterCredit(ctx.unitTarget->GetEntry(), ctx.unitTarget->GetObjectGuid());

    // Must have a delay for proper spell animation
    ((Creature*)ctx.unitTarget)->ForcedDespawn(1000);
    return SpellHandlerOutcome<void>::Return();
}

/// 67019: Flask of the North: a player caster casts on itself the Flask of the North of its class: 67018 for warriors
/// and death knights, 67017 for rogues and hunters, 67016 for priests, mages and warlocks, 67016 or 67017 at even odds
/// for shamans, and 67016 or 67018 at even odds for paladins, druids and any other class; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericFlaskOfTheNorth(SpellEffectDummyGenericContext& ctx)
{
    if (ctx.m_caster->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    uint32 spell_id = 0;
    switch (ctx.m_caster->getClass())
    {
        case CLASS_WARRIOR:
        case CLASS_DEATH_KNIGHT:
            spell_id = 67018;               // STR for Warriors, Death Knights
            break;
        case CLASS_ROGUE:
        case CLASS_HUNTER:
            spell_id = 67017;               // AP for Rogues, Hunters
            break;
        case CLASS_PRIEST:
        case CLASS_MAGE:
        case CLASS_WARLOCK:
            spell_id = 67016;               // SPD for Priests, Mages, Warlocks
            break;
        case CLASS_SHAMAN:
            // random (SPD, AP)
            spell_id = roll_chance_i(50) ? 67016 : 67017;
            break;
        case CLASS_PALADIN:
        case CLASS_DRUID:
        default:
            // random (SPD, STR)
            spell_id = roll_chance_i(50) ? 67016 : 67018;
            break;
    }
    ctx.m_caster->CastSpell(ctx.m_caster, spell_id, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 69922: Temper Quel'Delar: the unit target, if any, casts Return Tempered Quel'Delar (69956) on the caster; the
/// effect ends
static SpellHandlerOutcome<void> EffectDummyGenericTemperQuelDelar(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // Return Tempered Quel'Delar
    ctx.unitTarget->CastSpell(ctx.m_caster, 69956, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 71307: Vile Gas: the caster casts on a player target the spell the effect's value names; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericVileGas(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.m_caster->CastSpell(ctx.unitTarget, ctx.effect->CalculateSimpleValue(), true);
    return SpellHandlerOutcome<void>::Return();
}

/// 71445, 71471: Twilight Bloodbolt: the caster casts Twilight Bloodbolt (71818) on a player target; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericTwilightBloodbolt(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.m_caster->CastSpell(ctx.unitTarget, 71818, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 71718: Conjure Flame; 72040: Conjure Empowered Flame: the caster casts on the unit target, if any, the spell the
/// effect's value names; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericConjureFlame(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.m_caster->CastSpell(ctx.unitTarget, ctx.effect->CalculateSimpleValue(), true);
    return SpellHandlerOutcome<void>::Return();
}

/// 71837: Vampiric Bite: the caster casts Vampiric Bite (71726) on a player target; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericVampiricBite(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.m_caster->CastSpell(ctx.unitTarget, 71726, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 71861: Swarming Shadows: the caster casts Swarming Shadows (71264) on a player target; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericSwarmingShadows(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.m_caster->CastSpell(ctx.unitTarget, 71264, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 72202: Blood Link: the unit target, if any, casts Blood Link (72195) on itself; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericBloodLink(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.unitTarget->CastSpell(ctx.unitTarget, 72195, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 72254: Mark of the Fallen Champion: the caster casts on a player target that does not have the spell the effect's
/// value names that spell; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericMarkOfTheFallenChampion(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_PLAYER || ctx.unitTarget->HasAura(ctx.effect->CalculateSimpleValue()))
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.m_caster->CastSpell(ctx.unitTarget, ctx.effect->CalculateSimpleValue(), true);
    return SpellHandlerOutcome<void>::Return();
}

/// 72261: Delirious Slash: the caster casts on the unit target, if any, Delirious Slash (71623) when the target is in
/// its melee reach, otherwise Delirious Slash (72264); the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericDeliriousSlash(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.m_caster->CastSpell(ctx.unitTarget, InMeleeReach(*ctx.m_caster, *ctx.unitTarget) ? 71623 : 72264, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 72285: Vile Gas Trigger: the caster casts on a player target the spell the effect's value names; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericVileGasTrigger(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.m_caster->CastSpell(ctx.unitTarget, ctx.effect->CalculateSimpleValue(), true);
    return SpellHandlerOutcome<void>::Return();
}

/// 74452: Conflagration: the caster casts Flame Beacon (74453) on a player target, and Conflagration (74454) on it as
/// from itself and triggered by this spell; the effect ends
static SpellHandlerOutcome<void> EffectDummyGenericConflagration(SpellEffectDummyGenericContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.m_caster->CastSpell(ctx.unitTarget, 74453, true);
    ctx.m_caster->CastSpell(ctx.unitTarget, 74454, true, NULL, NULL, ctx.m_caster->GetObjectGuid(), ctx.m_spellInfo);
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
            ctx.m_caster->RemoveSpellCooldown((itr++)->first, true);
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
            ctx.m_caster->RemoveSpellCooldown((itr++)->first,true);
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
            ctx.m_caster->RemoveSpellCooldown((itr++)->first,true);
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
    ctx.m_caster->SendAttackSwingCancelAttack();
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
        ctx.m_caster->RemoveSpellCooldown(ctx.m_spellInfo->ID, true);
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
        { 3360, &EffectDummyGenericCurseOfTheEye },
        { 7671, &EffectDummyGenericTransformation },
        { 8063, &EffectDummyGenericDeviateFish },
        { 8213, &EffectDummyGenericCookedDeviateFish },
        { 9976, &EffectDummyGenericPollyEatsTheECAC },
        { 10254, &EffectDummyGeneric10254 },
        { 13120, &EffectDummyGenericNetOMatic },
        { 13489, &EffectDummyGenericBurningSpirit },
        { 13567, &EffectDummyGenericDummyTrigger },
        { 14537, &EffectDummyGenericSixDemonBag },
        { 15998, &EffectDummyGenericCaptureWorgPupKaliriHatchling },
        { 29435, &EffectDummyGenericCaptureWorgPupKaliriHatchling },
        { 16589, &EffectDummyGenericNoggenfoggerElixir },
        { 17009, &EffectDummyGenericVoodoo },
        { 17251, &EffectDummyGenericSpiritHealerRes },
        { 17271, &EffectDummyGenericTestFetidSkull },
        { 17770, &EffectDummyGenericWolfsheadHelmEnergy },
        { 17950, &EffectDummyGenericShadowPortal },
        { 19395, &EffectDummyGenericGordunniTrap },
        { 19411, &EffectDummyGenericLavaBomb },
        { 20474, &EffectDummyGenericLavaBomb },
        { 19869, &EffectDummyGenericDragonOrb },
        { 20037, &EffectDummyGenericExplodeOrbEffect },
        { 20577, &EffectDummyGenericCannibalize },
        { 21147, &EffectDummyGenericArcaneVacuum },
        { 23019, &EffectDummyGenericCrystalPrisonDummyDND },
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
        { 43498, &EffectDummyGenericSiphonSoul },
        { 43572, &EffectDummyGenericSendThemPacking },
        { 43723, &EffectDummyGenericDemonBroiledSurprise },
        { 43882, &EffectDummyGenericScourgingCrystalControllerDummy },
        { 44454, &EffectDummyGenericTastyReefFish },
        { 44869, &EffectDummyGenericSpectralBlast },
        { 44875, &EffectDummyGenericCompleteRaptorCapture },
        { 44997, &EffectDummyGenericConvertingSentry },
        { 45030, &EffectDummyGenericImpaleEmissary },
        { 45449, &EffectDummyGenericArcanePrisonerRescue },
        { 45583, &EffectDummyGenericThrowGnomishGrenade },
        { 45685, &EffectDummyGenericHahMagnataurOnDeath2 },
        { 45958, &EffectDummyGenericSignalAlliance },
        { 45976, &EffectDummyGenericOpenPortal },
        { 46177, &EffectDummyGenericOpenPortal },
        { 45980, &EffectDummyGenericReCursiveTransmatterInjection },
        { 45989, &EffectDummyGenericSummonVoidSentinelSummonerVisual },
        { 45990, &EffectDummyGenericCollectOil },
        { 46167, &EffectDummyGenericCreateCover },
        { 50918, &EffectDummyGenericCreateCover },
        { 50926, &EffectDummyGenericCreateCover },
        { 51026, &EffectDummyGenericCreateCover },
        { 51592, &EffectDummyGenericCreateCover },
        { 51961, &EffectDummyGenericCreateCover },
        { 55364, &EffectDummyGenericCreateCover },
        { 61832, &EffectDummyGenericCreateCover },
        { 63125, &EffectDummyGenericCreateCover },
        { 74904, &EffectDummyGenericCreateCover },
        { 46171, &EffectDummyGenericScuttleWreckedFlyingMachine },
        { 46292, &EffectDummyGenericCataclysmBreath },
        { 46372, &EffectDummyGenericIceSpearTargetPicker },
        { 46485, &EffectDummyGenericTheGreatmothersSoulcatcher },
        { 46606, &EffectDummyGenericTillinghastsPlagueCanisterDummy },
        { 46671, &EffectDummyGenericCleansingFlamesExodar },
        { 46672, &EffectDummyGenericCleansingFlamesExodar },
        { 46797, &EffectDummyGenericSetExplosivesCart },
        { 47110, &EffectDummyGenericSummonDrakurusImage },
        { 47170, &EffectDummyGenericImpaleLeviroth },
        { 47176, &EffectDummyGenericInfectIceTroll },
        { 47305, &EffectDummyGenericPotentExplosiveCharge },
        { 47381, &EffectDummyGenericReagentCheck },
        { 47386, &EffectDummyGenericReagentCheck },
        { 47389, &EffectDummyGenericReagentCheck },
        { 47408, &EffectDummyGenericReagentCheck },
        { 50441, &EffectDummyGenericReagentCheck },
        { 48046, &EffectDummyGenericUseCamera },
        { 48790, &EffectDummyGenericNeltharionsFlame },
        { 49357, &EffectDummyGenericBrewfestMountTransformation },
        { 49634, &EffectDummyGenericSergeantsFlare },
        { 49859, &EffectDummyGenericRuneOfCommand },
        { 50133, &EffectDummyGenericScourgingCrystalController },
        { 50243, &EffectDummyGenericTeachLanguage },
        { 50440, &EffectDummyGenericEnvisionDrakuru },
        { 50546, &EffectDummyGenericLeyLineFocusControl },
        { 50547, &EffectDummyGenericLeyLineFocusControl },
        { 50548, &EffectDummyGenericLeyLineFocusControl },
        { 51276, &EffectDummyGenericIncinerateCorpse },
        { 51330, &EffectDummyGenericShootRJR },
        { 51333, &EffectDummyGenericDigForTreasure },
        { 51336, &EffectDummyGenericMagicPull },
        { 51420, &EffectDummyGenericDiggingForTreasurePing },
        { 51582, &EffectDummyGenericRocketBootsEngaged },
        { 51840, &EffectDummyGenericDespawnFruitTosser },
        { 51964, &EffectDummyGenericTormentorsIncense },
        { 51858, &EffectDummyGenericSiphonOfAcherus },
        { 51866, &EffectDummyGeneric51866 },
        { 51872, &EffectDummyGeneric51872 },
        { 52308, &EffectDummyGenericTakeSputumSample },
        { 52369, &EffectDummyGenericDetonateExplosives },
        { 52371, &EffectDummyGenericDetonateExplosives },
        { 52759, &EffectDummyGenericAncestralAwakening },
        { 52845, &EffectDummyGenericBrewfestMountFactionSwap },
        { 53341, &EffectDummyGenericRuneOfCinderglacierRazorice },
        { 53343, &EffectDummyGenericRuneOfCinderglacierRazorice },
        { 53475, &EffectDummyGenericSetOracleWolvarFaction },
        { 53487, &EffectDummyGenericSetOracleWolvarFaction },
        { 54015, &EffectDummyGenericSetOracleWolvarFaction },
        { 53808, &EffectDummyGenericPygmyOil },
        { 54577, &EffectDummyGenericThrowUDED },
        { 54850, &EffectDummyGenericEmerge },
        { 54092, &EffectDummyGenericMonsterSlayersKit },
        { 55004, &EffectDummyGenericNitroBoosts },
        { 55818, &EffectDummyGenericHurlBoulder },
        { 56430, &EffectDummyGenericArcaneBomb },
        { 57578, &EffectDummyGenericLavaStrike },
        { 57908, &EffectDummyGenericStainCloth },
        { 58418, &EffectDummyGenericPortalToOrgrimmarStormwind },
        { 58420, &EffectDummyGenericPortalToOrgrimmarStormwind },
        { 58601, &EffectDummyGenericRemoveFlightAuras },
        { 59640, &EffectDummyGenericUnderbellyElixir },
        { 60932, &EffectDummyGenericDisengageCreature },
        { 62105, &EffectDummyGenericTokinisBlowgun },
        { 62278, &EffectDummyGenericLightningOrbCharger },
        { 62652, &EffectDummyGenericTidalWave },
        { 62797, &EffectDummyGenericStormCloud },
        { 62907, &EffectDummyGenericFreyasWard },
        { 63030, &EffectDummyGenericBoilOminously },
        { 63499, &EffectDummyGenericDispelMagic },
        { 63545, &EffectDummyGenericIcicle },
        { 63744, &EffectDummyGenericSarasAngerBlessingFervor },
        { 63745, &EffectDummyGenericSarasAngerBlessingFervor },
        { 63747, &EffectDummyGenericSarasAngerBlessingFervor },
        { 63984, &EffectDummyGenericHateToZero },
        { 64172, &EffectDummyGenericTitanicStorm },
        { 64385, &EffectDummyGenericSpinning },
        { 64402, &EffectDummyGenericRocketStrike },
        { 64489, &EffectDummyGenericFeralRush },
        { 64543, &EffectDummyGenericMeltIce },
        { 64555, &EffectDummyGenericInsanePeriodic },
        { 64673, &EffectDummyGenericFeralRushHeroic },
        { 64981, &EffectDummyGenericSummonRandomVanquishedTentacle },
        { 65206, &EffectDummyGenericDestabilizationMatrix },
        { 65346, &EffectDummyGenericProximityMine },
        { 65869, &EffectDummyGenericDisengage },
        { 66312, &EffectDummyGenericLightBallPassive },
        { 66314, &EffectDummyGenericDarkBallPassive },
        { 66390, &EffectDummyGenericReadLastRites },
        { 67019, &EffectDummyGenericFlaskOfTheNorth },
        { 69922, &EffectDummyGenericTemperQuelDelar },
        { 71307, &EffectDummyGenericVileGas },
        { 71445, &EffectDummyGenericTwilightBloodbolt },
        { 71471, &EffectDummyGenericTwilightBloodbolt },
        { 71718, &EffectDummyGenericConjureFlame },
        { 72040, &EffectDummyGenericConjureFlame },
        { 71837, &EffectDummyGenericVampiricBite },
        { 71861, &EffectDummyGenericSwarmingShadows },
        { 72202, &EffectDummyGenericBloodLink },
        { 72254, &EffectDummyGenericMarkOfTheFallenChampion },
        { 72261, &EffectDummyGenericDeliriousSlash },
        { 72285, &EffectDummyGenericVileGasTrigger },
        { 74452, &EffectDummyGenericConflagration },
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
