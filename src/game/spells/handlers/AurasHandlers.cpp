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


#include "spells/handlers/AurasHandlers.h"
#include "spells/handlers/SpellHandlerRegistry.h"
#include "WorldHandlers/SpellAuras.h"
#include "WorldHandlers/CellImpl.h"
#include "WorldHandlers/GridNotifiers.h"
#include "WorldHandlers/GridNotifiersImpl.h"
#include "WorldHandlers/Map.h"
#include "Object/Creature.h"
#include "Object/CreatureAI.h"
#include "Object/LootMgr.h"
#include "Utilities/Util.h"
#include "entities/player/Player.h"
#include "Geometry/Placement.h"

/// 812: Periodic Mana Burn
static SpellHandlerOutcome<void> AuraTriggerSpellGeneric812(AuraTriggerSpellContext& ctx)
{
    ctx.trigger_spell_id = 25779;           // Mana Burn

    if (ctx.aura->GetTarget()->GetTypeId() != TYPEID_UNIT)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.triggerTarget = ((Creature*)ctx.aura->GetTarget())->SelectAttackingTarget(ATTACKING_TARGET_TOPAGGRO, 0, ctx.trigger_spell_id, SELECT_FLAG_POWER_MANA);
    if (!ctx.triggerTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    return SpellHandlerOutcome<void>::Continue();
//                    // Polymorphic Ray
//                    case 6965: break;
}

/// 9712: Thaumaturgy Channel
static SpellHandlerOutcome<void> AuraTriggerSpellGeneric9712(AuraTriggerSpellContext& ctx)
{
    if (Unit* caster = ctx.aura->GetCaster())
    {
        caster->CastSpell(caster, 21029, true);
    }
    return SpellHandlerOutcome<void>::Return();
//                    // Egan's Blaster
//                    case 17368: break;
//                    // Haunted
//                    case 18347: break;
//                    // Ranshalla Waiting
//                    case 18953: break;

}

/// 19695: Inferno
static SpellHandlerOutcome<void> AuraTriggerSpellGeneric19695(AuraTriggerSpellContext& ctx)
{
    int32 damageForTick[8] = { 500, 500, 1000, 1000, 2000, 2000, 3000, 5000 };
    ctx.triggerTarget->CastCustomSpell(ctx.triggerTarget, 19698, &damageForTick[ctx.aura->GetAuraTicks() - 1], NULL, NULL, true, NULL);
    return SpellHandlerOutcome<void>::Return();
//                    // Frostwolf Muzzle DND
//                    case 21794: break;
//                    // Alterac Ram Collar DND
//                    case 21866: break;
//                    // Celebras Waiting
//                    case 21916: break;
}

/// 23170: Brood Affliction: Bronze
static SpellHandlerOutcome<void> AuraTriggerSpellGeneric23170(AuraTriggerSpellContext& ctx)
{
    ctx.target->CastSpell(ctx.target, 23171, true, NULL, ctx.aura);
    return SpellHandlerOutcome<void>::Return();
}

/// one body for 3 labels: 23184: Mark of Frost, 25041: Mark of Nature, 37125: Mark of Death
static SpellHandlerOutcome<void> AuraTriggerSpellGenericMarkDebuff(AuraTriggerSpellContext& ctx)
{
    std::list<Player*> targets;

    // spells existed in 1.x.x; 23183 - mark of frost; 25042 - mark of nature; both had radius of 100.0 yards in 1.x.x DBC
    // spells are used by Azuregos and the Emerald dragons in order to put a stun debuff on the players which resurrect during the encounter
    // in order to implement the missing spells we need to make a grid search for hostile players and check their auras; if they are marked apply debuff
    // spell 37127 used for the Mark of Death, is used server side, so it needs to be implemented here

    uint32 markSpellId = 0;
    uint32 debuffSpellId = 0;

    switch (ctx.auraId)
    {
        case 23184:
            markSpellId = 23182;
            debuffSpellId = 23186;
            break;
        case 25041:
            markSpellId = 25040;
            debuffSpellId = 25043;
            break;
        case 37125:
            markSpellId = 37128;
            debuffSpellId = 37131;
            break;
    }

    MaNGOS::AnyPlayerInObjectRangeWithAuraCheck u_check(ctx.aura->GetTarget(), 100.0f, markSpellId);
    MaNGOS::PlayerListSearcher<MaNGOS::AnyPlayerInObjectRangeWithAuraCheck > checker(targets, u_check);
    Cell::VisitWorldObjects(ctx.aura->GetTarget(), checker, 100.0f);

    for (std::list<Player*>::iterator itr = targets.begin(); itr != targets.end(); ++itr)
    {
        (*itr)->CastSpell((*itr), debuffSpellId, true, NULL, NULL, ctx.casterGUID);
    }

    return SpellHandlerOutcome<void>::Return();
}

/// 23493: Restoration
static SpellHandlerOutcome<void> AuraTriggerSpellGeneric23493(AuraTriggerSpellContext& ctx)
{
    uint32 heal = ctx.triggerTarget->GetMaxHealth() / 10;
    uint32 absorb = 0;
    ctx.triggerTarget->CalculateHealAbsorb(heal, &absorb);
    ctx.triggerTarget->DealHeal(ctx.triggerTarget, heal - absorb, ctx.auraSpellInfo, false, absorb);

    if (int32 mana = ctx.triggerTarget->GetMaxPower(POWER_MANA))
    {
        mana /= 10;
        ctx.triggerTarget->EnergizeBySpell(ctx.triggerTarget, 23493, mana, POWER_MANA);
    }
    return SpellHandlerOutcome<void>::Return();
//                    // Stoneclaw Totem Passive TEST
//                    case 23792: break;
//                    // Axe Flurry
//                    case 24018: break;
}

/// 24210: Mark of Arlokk
static SpellHandlerOutcome<void> AuraTriggerSpellGeneric24210(AuraTriggerSpellContext& ctx)
{
    // Replacement for (classic) spell 24211 (doesn't exist anymore)
    std::list<Creature*> lList;

    // Search for all Zulian Prowler in range
    MaNGOS::AllCreaturesOfEntryInRangeCheck check(ctx.triggerTarget, 15101, 15.0f);
    MaNGOS::CreatureListSearcher<MaNGOS::AllCreaturesOfEntryInRangeCheck> searcher(lList, check);
    Cell::VisitGridObjects(ctx.triggerTarget, searcher, 15.0f);

    for (std::list<Creature*>::const_iterator itr = lList.begin(); itr != lList.end(); ++itr)
        if ((*itr)->IsAlive())
        {
            (*itr)->AddThreat(ctx.triggerTarget, float(5000));
        }

    return SpellHandlerOutcome<void>::Return();
//                    // Restoration
//                    case 24379: break;
//                    // Happy Pet
//                    case 24716: break;
}

/// 24780: Dream Fog
static SpellHandlerOutcome<void> AuraTriggerSpellGeneric24780(AuraTriggerSpellContext& ctx)
{
    // Note: In 1.12 triggered spell 24781 still exists, need to script dummy effect for this spell then
    // Select an unfriendly enemy in 100y range and attack it
    if (ctx.target->GetTypeId() != TYPEID_UNIT)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ThreatList const& tList = ctx.target->GetThreatManager().getThreatList();
    for (ThreatList::const_iterator itr = tList.begin(); itr != tList.end(); ++itr)
    {
        Unit* pUnit = ctx.target->GetMap()->GetUnit((*itr)->getUnitGuid());

        if (pUnit && ctx.target->GetThreatManager().getThreat(pUnit))
        {
            ctx.target->GetThreatManager().modifyThreatPercent(pUnit, -100);
        }
    }

    if (Unit* pEnemy = ctx.target->SelectRandomUnfriendlyTarget(ctx.target->getVictim(), 100.0f))
    {
        ((Creature*)ctx.target)->AI()->AttackStart(pEnemy);
    }

    return SpellHandlerOutcome<void>::Return();
//                    // Cannon Prep
//                    case 24832: break;
}

/// 24834: Shadow Bolt Whirl
static SpellHandlerOutcome<void> AuraTriggerSpellGeneric24834(AuraTriggerSpellContext& ctx)
{
    uint32 spellForTick[8] = { 24820, 24821, 24822, 24823, 24835, 24836, 24837, 24838 };
    uint32 tick = (ctx.aura->GetAuraTicks() + 7/*-1*/) % 8;

    // casted in left/right (but triggered spell have wide forward cone)
    float forward = ctx.target->Where().Facing();
    if (tick <= 3)
    {
        ctx.target->Place().Face(forward + 0.75f * M_PI_F - tick * M_PI_F / 8);        // Left
    }
    else
    {
        ctx.target->Place().Face(forward - 0.75f * M_PI_F + (8 - tick) * M_PI_F / 8);  // Right
    }

    ctx.triggerTarget->CastSpell(ctx.triggerTarget, spellForTick[tick], true, NULL, ctx.aura, ctx.casterGUID);
    ctx.target->Place().Face(forward);
    return SpellHandlerOutcome<void>::Return();
//                    // Stink Trap
//                    case 24918: break;
//                    // Agro Drones
//                    case 25152: break;
}

/// 25371: Consume
static SpellHandlerOutcome<void> AuraTriggerSpellGeneric25371(AuraTriggerSpellContext& ctx)
{
    int32 bpDamage = ctx.triggerTarget->GetMaxHealth() * 10 / 100;
    ctx.triggerTarget->CastCustomSpell(ctx.triggerTarget, 25373, &bpDamage, NULL, NULL, true, NULL, ctx.aura, ctx.casterGUID);
    return SpellHandlerOutcome<void>::Return();
//                    // Pain Spike
//                    case 25572: break;
}

/// one body for 2 labels: 26009: Rotate 360, 26136: Rotate -360
static SpellHandlerOutcome<void> AuraTriggerSpellGenericRotate360(AuraTriggerSpellContext& ctx)
{
    float newAngle = ctx.target->Where().Facing();

    if (ctx.auraId == 26009)
    {
        newAngle += M_PI_F / 40;
    }
    else
    {
        newAngle -= M_PI_F / 40;
    }

    newAngle = Geometry::Placement::NormalizeOrientation(newAngle);

    ctx.target->SetFacingTo(newAngle);

    ctx.target->CastSpell(ctx.target, 26029, true);
    return SpellHandlerOutcome<void>::Return();
//                    // Consume
//                    case 26196: break;
//                    // Berserk
//                    case 26615: break;
//                    // Defile
//                    case 27177: break;
//                    // Teleport: IF/UC
//                    case 27601: break;
//                    // Five Fat Finger Exploding Heart Technique
//                    case 27673: break;
//                    // Nitrous Boost
//                    case 27746: break;
//                    // Steam Tank Passive
//                    case 27747: break;
}

/// 27808: Frost Blast
static SpellHandlerOutcome<void> AuraTriggerSpellGeneric27808(AuraTriggerSpellContext& ctx)
{
    int32 bpDamage = ctx.triggerTarget->GetMaxHealth() * 26 / 100;
    ctx.triggerTarget->CastCustomSpell(ctx.triggerTarget, 29879, &bpDamage, NULL, NULL, true, NULL, ctx.aura, ctx.casterGUID);
    return SpellHandlerOutcome<void>::Return();
// Detonate Mana
}

/// 27819: Detonate Mana
static SpellHandlerOutcome<void> AuraTriggerSpellGeneric27819(AuraTriggerSpellContext& ctx)
{
    // 33% Mana Burn on normal mode, 50% on heroic mode
    int32 bpDamage = (int32)ctx.triggerTarget->GetPower(POWER_MANA) / (ctx.triggerTarget->GetMap()->GetDifficulty() ? 2 : 3);
    ctx.triggerTarget->ModifyPower(POWER_MANA, -bpDamage);
    ctx.triggerTarget->CastCustomSpell(ctx.triggerTarget, 27820, &bpDamage, NULL, NULL, true, NULL, ctx.aura, ctx.triggerTarget->GetObjectGuid());
    return SpellHandlerOutcome<void>::Return();
//                    // Controller Timer
//                    case 28095: break;
// Stalagg Chain and Feugen Chain
}

/// one body for 2 labels: 28096: Stalagg Chain, 28111: Feugen Chain
static SpellHandlerOutcome<void> AuraTriggerSpellGenericTeslaChain(AuraTriggerSpellContext& ctx)
{
    // X-Chain is casted by Tesla to X, so: caster == Tesla, target = X
    Unit* pCaster = ctx.aura->GetCaster();
    if (pCaster && pCaster->GetTypeId() == TYPEID_UNIT && !InReach(*pCaster, *ctx.target, 60.0f))
    {
        pCaster->InterruptNonMeleeSpells(true);
        ((Creature*)pCaster)->SetInCombatWithZone();
        // Stalagg Tesla Passive or Feugen Tesla Passive
        pCaster->CastSpell(pCaster, ctx.auraId == 28096 ? 28097 : 28109, true, NULL, NULL, ctx.target->GetObjectGuid());
    }
    return SpellHandlerOutcome<void>::Return();
// Stalagg Tesla Passive and Feugen Tesla Passive
}

/// one body for 2 labels: 28097: Stalagg Tesla Passive, 28109: Feugen Tesla Passive
static SpellHandlerOutcome<void> AuraTriggerSpellGenericTeslaPassive(AuraTriggerSpellContext& ctx)
{
    // X-Tesla-Passive is casted by Tesla on Tesla with original caster X, so: caster = X, target = Tesla
    Unit* pCaster = ctx.aura->GetCaster();
    if (pCaster && pCaster->GetTypeId() == TYPEID_UNIT)
    {
        if (pCaster->getVictim() && !InReach(*pCaster, *ctx.target, 60.0f))
        {
            if (Unit* pTarget = ((Creature*)pCaster)->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0))
            {
                ctx.target->CastSpell(pTarget, 28099, false);// Shock
            }
        }
        else
        {
            // "Evade"
            ctx.target->RemoveAurasDueToSpell(ctx.auraId);
            ctx.target->DeleteThreatList();
            ctx.target->CombatStop(true);
            // Recast chain (Stalagg Chain or Feugen Chain
            ctx.target->CastSpell(pCaster, ctx.auraId == 28097 ? 28096 : 28111, false);
        }
    }
    return SpellHandlerOutcome<void>::Return();
//                    // Mark of Didier
//                    case 28114: break;
//                    // Communique Timer, camp
//                    case 28346: break;
//                    // Icebolt
//                    case 28522: break;
//                    // Silithyst
//                    case 29519: break;
}

/// 29528: Inoculate Nestlewood Owlkin
static SpellHandlerOutcome<void> AuraTriggerSpellGeneric29528(AuraTriggerSpellContext& ctx)
{
    // prevent error reports in case ignored player target
    if (ctx.triggerTarget->GetTypeId() != TYPEID_UNIT)
    {
        return SpellHandlerOutcome<void>::Return();
    }
    return SpellHandlerOutcome<void>::Continue();
//                    // Overload
//                    case 29768: break;
//                    // Return Fire
//                    case 29788: break;
//                    // Return Fire
//                    case 29793: break;
//                    // Return Fire
//                    case 29794: break;
//                    // Guardian of Icecrown Passive
//                    case 29897: break;
}

/// 29917: Feed Captured Animal
static SpellHandlerOutcome<void> AuraTriggerSpellGeneric29917(AuraTriggerSpellContext& ctx)
{
    ctx.trigger_spell_id = 29916;
    return SpellHandlerOutcome<void>::Continue();
//                    // Flame Wreath
//                    case 29946: break;
//                    // Flame Wreath
//                    case 29947: break;
//                    // Mind Exhaustion Passive
//                    case 30025: break;
//                    // Nether Beam - Serenity
//                    case 30401: break;
}

/// 30427: Extract Gas
static SpellHandlerOutcome<void> AuraTriggerSpellGeneric30427(AuraTriggerSpellContext& ctx)
{
    Unit* caster = ctx.aura->GetCaster();
    if (!caster)
    {
        return SpellHandlerOutcome<void>::Return();
    }
    // move loot to player inventory and despawn target
    if (caster->GetTypeId() == TYPEID_PLAYER &&
            ctx.triggerTarget->GetTypeId() == TYPEID_UNIT &&
            ((Creature*)ctx.triggerTarget)->GetCreatureInfo()->CreatureType == CREATURE_TYPE_GAS_CLOUD)
    {
        Player* player = (Player*)caster;
        Creature* creature = (Creature*)ctx.triggerTarget;
        // missing Lootid has been reported on startup - just return
        if (!creature->GetCreatureInfo()->SkinningLootId)
        {
            return SpellHandlerOutcome<void>::Return();
        }

        player->AutoStoreLoot(creature, creature->GetCreatureInfo()->SkinningLootId, LootTemplates_Skinning, true);

        creature->ForcedDespawn();
    }
    return SpellHandlerOutcome<void>::Return();
}

/// 30576: absent from the client's spell data
static SpellHandlerOutcome<void> AuraTriggerSpellGeneric30576(AuraTriggerSpellContext& ctx)
{
    ctx.trigger_spell_id = 30571;
    return SpellHandlerOutcome<void>::Continue();
//                    // Burning Maul
//                    case 30598: break;
//                    // Regeneration
//                    case 30799:
//                    case 30800:
//                    case 30801:
//                        break;
//                    // Despawn Self - Smoke cloud
//                    case 31269: break;
//                    // Time Rift Periodic
//                    case 31320: break;
//                    // Corrupt Medivh
//                    case 31326: break;
}

/// 31347: Doom
static SpellHandlerOutcome<void> AuraTriggerSpellGeneric31347(AuraTriggerSpellContext& ctx)
{
    ctx.target->CastSpell(ctx.target, 31350, true);
    ctx.target->DealDamage(ctx.target, ctx.target->GetHealth(), NULL, DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NORMAL, NULL, false);
    return SpellHandlerOutcome<void>::Return();
}

/// 31373: Spellcloth
static SpellHandlerOutcome<void> AuraTriggerSpellGeneric31373(AuraTriggerSpellContext& ctx)
{
    // Summon Elemental after create item
    ctx.triggerTarget->SummonCreature(17870, 0.0f, 0.0f, 0.0f, ctx.triggerTarget->Where().Facing(), TEMPSPAWN_DEAD_DESPAWN, 0);
    return SpellHandlerOutcome<void>::Return();
//                    // Bloodmyst Tesla
//                    case 31611: break;
}

/// 31944: Doomfire
static SpellHandlerOutcome<void> AuraTriggerSpellGeneric31944(AuraTriggerSpellContext& ctx)
{
    int32 damage = ctx.aura->GetModifier()->m_amount * ((ctx.aura->GetAuraDuration() + ctx.aura->GetModifier()->periodictime) / ctx.aura->GetAuraMaxDuration());
    ctx.triggerTarget->CastCustomSpell(ctx.triggerTarget, 31969, &damage, NULL, NULL, true, NULL, ctx.aura, ctx.casterGUID);
    return SpellHandlerOutcome<void>::Return();
//                    // Teleport Test
//                    case 32236: break;
//                    // Earthquake
//                    case 32686: break;
//                    // Possess
//                    case 33401: break;
//                    // Draw Shadows
//                    case 33563: break;
//                    // Murmur's Touch
//                    case 33711: break;
}

/// 34229: Flame Quills
static SpellHandlerOutcome<void> AuraTriggerSpellGeneric34229(AuraTriggerSpellContext& ctx)
{
    // cast 24 spells 34269-34289, 34314-34316
    for (uint32 spell_id = 34269; spell_id != 34290; ++spell_id)
    {
        ctx.triggerTarget->CastSpell(ctx.triggerTarget, spell_id, true, NULL, ctx.aura, ctx.casterGUID);
    }
    for (uint32 spell_id = 34314; spell_id != 34317; ++spell_id)
    {
        ctx.triggerTarget->CastSpell(ctx.triggerTarget, spell_id, true, NULL, ctx.aura, ctx.casterGUID);
    }
    return SpellHandlerOutcome<void>::Return();
//                    // Gravity Lapse
//                    case 34480: break;
//                    // Tornado
//                    case 34683: break;
//                    // Frostbite Rotate
//                    case 34748: break;
//                    // Arcane Flurry
//                    case 34821: break;
//                    // Interrupt Shutdown
//                    case 35016: break;
//                    // Interrupt Shutdown
//                    case 35176: break;
//                    // Inferno
//                    case 35268: break;
//                    // Salaadin's Tesla
//                    case 35515: break;
//                    // Ethereal Channel (Red)
//                    case 35518: break;
//                    // Nether Vapor
//                    case 35879: break;
//                    // Dark Portal Storm
//                    case 36018: break;
//                    // Burning Maul
//                    case 36056: break;
//                    // Living Grove Defender Lifespan
//                    case 36061: break;
//                    // Professor Dabiri Talks
//                    case 36064: break;
//                    // Kael Gaining Power
//                    case 36091: break;
//                    // They Must Burn Bomb Aura
//                    case 36344: break;
//                    // They Must Burn Bomb Aura (self)
//                    case 36350: break;
//                    // Stolen Ravenous Ravager Egg
//                    case 36401: break;
//                    // Activated Cannon
//                    case 36410: break;
//                    // Stolen Ravenous Ravager Egg
//                    case 36418: break;
//                    // Enchanted Weapons
//                    case 36510: break;
//                    // Cursed Scarab Periodic
//                    case 36556: break;
//                    // Cursed Scarab Despawn Periodic
//                    case 36561: break;
}

/// 36573: Vision Guide
static SpellHandlerOutcome<void> AuraTriggerSpellGeneric36573(AuraTriggerSpellContext& ctx)
{
    if (ctx.aura->GetAuraTicks() == 10 && ctx.target->GetTypeId() == TYPEID_PLAYER)
    {
        ((Player*)ctx.target)->AreaExploredOrEventHappens(10525);
        ctx.target->RemoveAurasDueToSpell(36573);
    }

    return SpellHandlerOutcome<void>::Return();
//                    // Cannon Charging (platform)
//                    case 36785: break;
//                    // Cannon Charging (self)
//                    case 36860: break;
}

/// 37027: Remote Toy
static SpellHandlerOutcome<void> AuraTriggerSpellGeneric37027(AuraTriggerSpellContext& ctx)
{
    ctx.trigger_spell_id = 37029;
    return SpellHandlerOutcome<void>::Continue();
//                    // Mark of Death
//                    case 37125: break;
//                    // Arcane Flurry
//                    case 37268: break;
}

/// one body for 2 labels: 37429: Spout, 37430: Spout
static SpellHandlerOutcome<void> AuraTriggerSpellGenericSpout(AuraTriggerSpellContext& ctx)
{
    float newAngle = ctx.target->Where().Facing();

    if (ctx.auraId == 37429)
    {
        newAngle += 2 * M_PI_F / 100;
    }
    else
    {
        newAngle -= 2 * M_PI_F / 100;
    }

    newAngle = Geometry::Placement::NormalizeOrientation(newAngle);

    ctx.target->SetFacingTo(newAngle);

    ctx.target->CastSpell(ctx.target, 37433, true);
    return SpellHandlerOutcome<void>::Return();
//                    // Karazhan - Chess NPC AI, Snapshot timer
//                    case 37440: break;
//                    // Karazhan - Chess NPC AI, action timer
//                    case 37504: break;
//                    // Karazhan - Chess: Is Square OCCUPIED aura (DND)
//                    case 39400: break;
//                    // Banish
//                    case 37546: break;
//                    // Shriveling Gaze
//                    case 37589: break;
//                    // Fake Aggro Radius (2 yd)
//                    case 37815: break;
//                    // Corrupt Medivh
//                    case 37853: break;
}

/// 38495: Eye of Grillok
static SpellHandlerOutcome<void> AuraTriggerSpellGeneric38495(AuraTriggerSpellContext& ctx)
{
    ctx.target->CastSpell(ctx.target, 38530, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 38554: Absorb Eye of Grillok
static SpellHandlerOutcome<void> AuraTriggerSpellGeneric38554(AuraTriggerSpellContext& ctx)
{
    if (ctx.target->GetTypeId() != TYPEID_UNIT)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    if (Unit* caster = ctx.aura->GetCaster())
    {
        caster->CastSpell(caster, 38495, true, NULL, ctx.aura);
    }
    else
    {
        return SpellHandlerOutcome<void>::Return();
    }

    Creature* creatureTarget = (Creature*)ctx.target;

    creatureTarget->ForcedDespawn();
    return SpellHandlerOutcome<void>::Return();
//                    // Magic Sucker Device timer
//                    case 38672: break;
//                    // Tomb Guarding Charging
//                    case 38751: break;
//                    // Murmur's Touch
//                    case 38794: break;
}

/// 39105: Activate Nether-wraith Beacon
static SpellHandlerOutcome<void> AuraTriggerSpellGeneric39105(AuraTriggerSpellContext& ctx)
{
    float fX, fY, fZ;
    ClosePointNear(*ctx.triggerTarget, fX, fY, fZ, ctx.triggerTarget->Where().Extent(), 20.0f);
    ctx.triggerTarget->SummonCreature(22408, fX, fY, fZ, ctx.triggerTarget->Where().Facing(), TEMPSPAWN_DEAD_DESPAWN, 0);
    return SpellHandlerOutcome<void>::Return();
//                    // Drain World Tree Visual
//                    case 39140: break;
//                    // Quest - Dustin's Undead Dragon Visual aura
//                    case 39259: break;
//                    // Hellfire - The Exorcism, Jules releases darkness, aura
//                    case 39306: break;
//                    // Inferno
//                    case 39346: break;
//                    // Enchanted Weapons
//                    case 39489: break;
//                    // Shadow Bolt Whirl
//                    case 39630: break;
//                    // Shadow Bolt Whirl
//                    case 39634: break;
//                    // Shadow Inferno
//                    case 39645: break;
}

/// 39857: Tear of Azzinoth Summon Channel
static SpellHandlerOutcome<void> AuraTriggerSpellGeneric39857(AuraTriggerSpellContext& ctx)
{
    ctx.trigger_spell_id = 39856;
    return SpellHandlerOutcome<void>::Continue();
//                    // Soulgrinder Ritual Visual (Smashed)
//                    case 39974: break;
//                    // Simon Game Pre-game timer
//                    case 40041: break;
//                    // Knockdown Fel Cannon: The Aggro Check Aura
//                    case 40113: break;
//                    // Spirit Lance
//                    case 40157: break;
}

/// 40398: Demon Transform 2
static SpellHandlerOutcome<void> AuraTriggerSpellGeneric40398(AuraTriggerSpellContext& ctx)
{
    switch (ctx.aura->GetAuraTicks())
    {
        case 1:
            if (ctx.target->HasAura(40506))
            {
                ctx.target->RemoveAurasDueToSpell(40506);
            }
            else
            {
                ctx.trigger_spell_id = 40506;
            }
            break;
        case 2:
            ctx.trigger_spell_id = 40510;
            break;
    }
    return SpellHandlerOutcome<void>::Continue();
}

/// 40511: Demon Transform 1
static SpellHandlerOutcome<void> AuraTriggerSpellGeneric40511(AuraTriggerSpellContext& ctx)
{
    ctx.trigger_spell_id = 40398;
    return SpellHandlerOutcome<void>::Continue();
//                    // Ancient Flames
//                    case 40657: break;
//                    // Ethereal Ring Cannon: Cannon Aura
//                    case 40734: break;
//                    // Cage Trap
//                    case 40760: break;
//                    // Random Periodic
//                    case 40867: break;
//                    // Prismatic Shield
//                    case 40879: break;
//                    // Aura of Desire
//                    case 41350: break;
//                    // Dementia
//                    case 41404: break;
//                    // Chaos Form
//                    case 41629: break;
//                    // Alert Drums
//                    case 42177: break;
}

/// one body for 2 labels: 42581: Spout, 42582: Spout
static SpellHandlerOutcome<void> AuraTriggerSpellGenericSpoutByBasePoints(AuraTriggerSpellContext& ctx)
{
    float newAngle = ctx.target->Where().Facing();

    if (ctx.auraId == 42581)
    {
        newAngle += 2 * M_PI_F / 100;
    }
    else
    {
        newAngle -= 2 * M_PI_F / 100;
    }

    newAngle = Geometry::Placement::NormalizeOrientation(newAngle);

    ctx.target->SetFacingTo(newAngle);

    ctx.target->CastSpell(ctx.target, ctx.auraSpellInfo->CalculateSimpleValue(ctx.aura->GetEffIndex()), true);
    return SpellHandlerOutcome<void>::Return();
//                    // Return to the Spirit Realm
//                    case 44035: break;
//                    // Curse of Boundless Agony
//                    case 45050: break;
//                    // Earthquake
//                    case 46240: break;
}

/// 46736: Personalized Weather
static SpellHandlerOutcome<void> AuraTriggerSpellGeneric46736(AuraTriggerSpellContext& ctx)
{
    ctx.trigger_spell_id = 46737;
    return SpellHandlerOutcome<void>::Continue();
//                    // Stay Submerged
//                    case 46981: break;
//                    // Dragonblight Ram
//                    case 47015: break;
//                    // Party G.R.E.N.A.D.E.
//                    case 51510: break;
//                    // Horseman Abilities
//                    case 52347: break;
//                    // GPS (Greater drake Positioning System)
//                    case 53389: break;
//                    // WotLK Prologue Frozen Shade Summon Aura
//                    case 53459: break;
//                    // WotLK Prologue Frozen Shade Speech
//                    case 53460: break;
//                    // WotLK Prologue Dual-plagued Brain Summon Aura
//                    case 54295: break;
//                    // WotLK Prologue Dual-plagued Brain Speech
//                    case 54299: break;
//                    // Rotate 360 (Fast)
//                    case 55861: break;
//                    // Shadow Sickle
//                    case 56702: break;
//                    // Portal Periodic
//                    case 58008: break;
//                    // Destroy Door Seal
//                    case 58040: break;
//                    // Draw Magic
//                    case 58185: break;
}

/// 58886: Food
static SpellHandlerOutcome<void> AuraTriggerSpellGeneric58886(AuraTriggerSpellContext& ctx)
{
    if (ctx.aura->GetAuraTicks() != 1)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    uint32 randomBuff[5] = {57288, 57139, 57111, 57286, 57291};

    ctx.trigger_spell_id = urand(0, 1) ? 58891 : randomBuff[urand(0, 4)];

    return SpellHandlerOutcome<void>::Continue();
//                    // Shadow Sickle
//                    case 59103: break;
//                    // Time Bomb
//                    case 59376: break;
//                    // Whirlwind Visual
//                    case 59551: break;
//                    // Hearstrike
//                    case 59783: break;
//                    // Z Check
//                    case 61678: break;
//                    // isDead Check
//                    case 61976: break;
//                    // Start the Engine
//                    case 62432: break;
//                    // Enchanted Broom
//                    case 62571: break;
//                    // Mulgore Hatchling
//                    case 62586: break;
}

/// 62679: Durotar Scorpion
static SpellHandlerOutcome<void> AuraTriggerSpellGeneric62679(AuraTriggerSpellContext& ctx)
{
    ctx.trigger_spell_id = ctx.auraSpellInfo->CalculateSimpleValue(ctx.aura->GetEffIndex());
    return SpellHandlerOutcome<void>::Continue();
//                    // Fighting Fish
//                    case 62833: break;
//                    // Shield Level 1
//                    case 63130: break;
//                    // Shield Level 2
//                    case 63131: break;
//                    // Shield Level 3
//                    case 63132: break;
//                    // Food
}

/// 64345: absent from the client's spell data
static SpellHandlerOutcome<void> AuraTriggerSpellGeneric64345(AuraTriggerSpellContext& ctx)
{
    ctx.target->RemoveSpellsCausingAura(SPELL_AURA_PHASE);
    return SpellHandlerOutcome<void>::Return();
//                    case 64445: break;
//                    // Food
//                    case 65418: break;
//                    // Food
//                    case 65419: break;
//                    // Food
//                    case 65420: break;
//                    // Food
//                    case 65421: break;
//                    // Food
//                    case 65422: break;
//                    // Rolling Throw
//                    case 67546: break;
}

/// 69012: Explosive Barrage
static SpellHandlerOutcome<void> AuraTriggerSpellGeneric69012(AuraTriggerSpellContext& ctx)
{
    // Summon an Exploding Orb for each player in combat with the caster
    ThreatList const& threatList = ctx.target->GetThreatManager().getThreatList();
    for (ThreatList::const_iterator itr = threatList.begin(); itr != threatList.end(); ++itr)
    {
        if (Unit* expectedTarget = ctx.target->GetMap()->GetUnit((*itr)->getUnitGuid()))
        {
            if (expectedTarget->GetTypeId() == TYPEID_PLAYER)
            {
                ctx.target->CastSpell(expectedTarget, 69015, true);
            }
        }
    }
    return SpellHandlerOutcome<void>::Return();
//                    // Gunship Cannon Fire
//                    case 70017: break;
//                    // Ice Tomb
//                    case 70157: break;
//                    // Mana Barrier                       // HANDLED IN SD2!
//                    case 70842: break;
//                    // Summon Timer: Suppresser
//                    case 70912: break;
//                    // Aura of Darkness
//                    case 71110: break;
//                    // Aura of Darkness
//                    case 71111: break;
//                    // Ball of Flames Visual
//                    case 71706: break;
//                    // Summon Broken Frostmourne
//                    case 74081: break;
}

/// every other id: nothing
static SpellHandlerOutcome<void> AuraTriggerSpellGenericDefault(AuraTriggerSpellContext& /*ctx*/)
{
    return SpellHandlerOutcome<void>::Continue();
}

template <class Site>
struct AurasRow
{
    uint32 spellId;
    typename SpellHandler<Site>::Function function;
};

/// Registers every row of one site's table on `registry`; answers the number of rows.
template <class Site, std::size_t N>
static uint32 RegisterAurasRows(SpellHandlerRegistry& registry, AurasRow<Site> const (&rows)[N])
{
    for (AurasRow<Site> const& row : rows)
    {
        registry.Register<Site>(row.spellId, row.function);
    }
    return uint32(N);
}

uint32 RegisterAurasHandlers(SpellHandlerRegistry& registry)
{
    static AurasRow<AuraTriggerSpellGenericSite> const triggerSpellGeneric[] =
    {
        { 812, &AuraTriggerSpellGeneric812 },
        { 9712, &AuraTriggerSpellGeneric9712 },
        { 19695, &AuraTriggerSpellGeneric19695 },
        { 23170, &AuraTriggerSpellGeneric23170 },
        { 23184, &AuraTriggerSpellGenericMarkDebuff },
        { 25041, &AuraTriggerSpellGenericMarkDebuff },
        { 37125, &AuraTriggerSpellGenericMarkDebuff },
        { 23493, &AuraTriggerSpellGeneric23493 },
        { 24210, &AuraTriggerSpellGeneric24210 },
        { 24780, &AuraTriggerSpellGeneric24780 },
        { 24834, &AuraTriggerSpellGeneric24834 },
        { 25371, &AuraTriggerSpellGeneric25371 },
        { 26009, &AuraTriggerSpellGenericRotate360 },
        { 26136, &AuraTriggerSpellGenericRotate360 },
        { 27808, &AuraTriggerSpellGeneric27808 },
        { 27819, &AuraTriggerSpellGeneric27819 },
        { 28096, &AuraTriggerSpellGenericTeslaChain },
        { 28111, &AuraTriggerSpellGenericTeslaChain },
        { 28097, &AuraTriggerSpellGenericTeslaPassive },
        { 28109, &AuraTriggerSpellGenericTeslaPassive },
        { 29528, &AuraTriggerSpellGeneric29528 },
        { 29917, &AuraTriggerSpellGeneric29917 },
        { 30427, &AuraTriggerSpellGeneric30427 },
        { 30576, &AuraTriggerSpellGeneric30576 },
        { 31347, &AuraTriggerSpellGeneric31347 },
        { 31373, &AuraTriggerSpellGeneric31373 },
        { 31944, &AuraTriggerSpellGeneric31944 },
        { 34229, &AuraTriggerSpellGeneric34229 },
        { 36573, &AuraTriggerSpellGeneric36573 },
        { 37027, &AuraTriggerSpellGeneric37027 },
        { 37429, &AuraTriggerSpellGenericSpout },
        { 37430, &AuraTriggerSpellGenericSpout },
        { 38495, &AuraTriggerSpellGeneric38495 },
        { 38554, &AuraTriggerSpellGeneric38554 },
        { 39105, &AuraTriggerSpellGeneric39105 },
        { 39857, &AuraTriggerSpellGeneric39857 },
        { 40398, &AuraTriggerSpellGeneric40398 },
        { 40511, &AuraTriggerSpellGeneric40511 },
        { 42581, &AuraTriggerSpellGenericSpoutByBasePoints },
        { 42582, &AuraTriggerSpellGenericSpoutByBasePoints },
        { 46736, &AuraTriggerSpellGeneric46736 },
        { 58886, &AuraTriggerSpellGeneric58886 },
        { 62679, &AuraTriggerSpellGeneric62679 },
        { 64345, &AuraTriggerSpellGeneric64345 },
        { 69012, &AuraTriggerSpellGeneric69012 },
    };

    uint32 rows = RegisterAurasRows(registry, triggerSpellGeneric);
    registry.RegisterDefault<AuraTriggerSpellGenericSite>(&AuraTriggerSpellGenericDefault);
    return rows + 1;
}
