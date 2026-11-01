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
#include "spells/auras/SpellAuras.h"
#include "spells/data/SpellMgr.h"
#include "spells/pipeline/Spell.h"
#include "data/dbc/DBCStores.h"
#include "Maps/CellImpl.h"
#include "Maps/GridNotifiers.h"
#include "Maps/GridNotifiersImpl.h"
#include "Maps/Map.h"
#include "Object/Creature.h"
#include "ai/CreatureAI.h"
#include "economy/LootMgr.h"
#include "Object/Pet.h"
#include "Utilities/Util.h"
#include "entities/player/Player.h"
#include "Geometry/Placement.h"

/// 812: Periodic Mana Burn
static SpellHandlerOutcome<void> AuraTriggerSpellGeneric812(AuraTriggerSpellContext& ctx)
{
    ctx.trigger_spell_id = 25779;           // Mana Burn

    Creature* targetCreature = ctx.aura->GetTarget()->ToCreature();
    if (!targetCreature)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.triggerTarget = targetCreature->SelectAttackingTarget(ATTACKING_TARGET_TOPAGGRO, 0, ctx.trigger_spell_id, SELECT_FLAG_POWER_MANA);
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
        ctx.target->AI()->AttackStart(pEnemy);
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
    Creature* casterCreature = pCaster ? pCaster->ToCreature() : NULL;
    if (casterCreature && !InReach(*pCaster, *ctx.target, 60.0f))
    {
        pCaster->InterruptNonMeleeSpells(true);
        casterCreature->SetInCombatWithZone();
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
    if (Creature* casterCreature = pCaster ? pCaster->ToCreature() : NULL)
    {
        if (pCaster->getVictim() && !InReach(*pCaster, *ctx.target, 60.0f))
        {
            if (Unit* pTarget = casterCreature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0))
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
    Player* player = caster->ToPlayer();
    Creature* creature = player ? ctx.triggerTarget->ToCreature() : NULL;
    if (creature && creature->GetCreatureType() == CREATURE_TYPE_GAS_CLOUD)
    {
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
    Player* targetPlayer = ctx.target->ToPlayer();
    if (ctx.aura->GetAuraTicks() == 10 && targetPlayer)
    {
        targetPlayer->AreaExploredOrEventHappens(10525);
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
    Creature* creatureTarget = ctx.target->ToCreature();
    if (!creatureTarget)
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

/// 66: Invisibility
static SpellHandlerOutcome<void> AuraTriggerSpellMage66(AuraTriggerSpellContext& /*ctx*/)
{
    // Here need periodic trigger reducing threat spell (or do it manually)
    return SpellHandlerOutcome<void>::Return();
}

/// every other id: nothing
static SpellHandlerOutcome<void> AuraTriggerSpellMageDefault(AuraTriggerSpellContext& /*ctx*/)
{
    return SpellHandlerOutcome<void>::Continue();
}

/// one body for 3 labels: 53302: Sniper Training, 53303: Sniper Training, 53304: Sniper Training
static SpellHandlerOutcome<void> AuraTriggerSpellHunterSniperTraining(AuraTriggerSpellContext& ctx)
{
    Player* triggerTargetPlayer = ctx.triggerTarget->ToPlayer();
    if (!triggerTargetPlayer)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // Reset reapply counter at move
    if (triggerTargetPlayer->isMoving())
    {
        ctx.aura->GetModifier()->m_amount = 6;
        return SpellHandlerOutcome<void>::Return();
    }

    // We are standing at the moment
    if (ctx.aura->GetModifier()->m_amount > 0)
    {
        --ctx.aura->GetModifier()->m_amount;
        return SpellHandlerOutcome<void>::Return();
    }

    // select rank of buff
    switch (ctx.auraId)
    {
        case 53302: ctx.trigger_spell_id = 64418; break;
        case 53303: ctx.trigger_spell_id = 64419; break;
        case 53304: ctx.trigger_spell_id = 64420; break;
    }

    // If aura is active - no need to continue
    if (ctx.triggerTarget->HasAura(ctx.trigger_spell_id))
    {
        return SpellHandlerOutcome<void>::Return();
    }

    return SpellHandlerOutcome<void>::Continue();
}

/// every other id: nothing
static SpellHandlerOutcome<void> AuraTriggerSpellHunterDefault(AuraTriggerSpellContext& /*ctx*/)
{
    return SpellHandlerOutcome<void>::Continue();
}

/// 768: Cat Form
static SpellHandlerOutcome<void> AuraTriggerSpellDruid768(AuraTriggerSpellContext& /*ctx*/)
{
    // trigger_spell_id not set and unknown effect triggered in this case, ignoring for while
    return SpellHandlerOutcome<void>::Return();
}

/// one body for 4 labels: 22842: Frenzied Regeneration; 22895, 22896, 26999: absent from the client's spell data
static SpellHandlerOutcome<void> AuraTriggerSpellDruidFrenziedRegeneration(AuraTriggerSpellContext& ctx)
{
    int32 LifePerRage = ctx.aura->GetModifier()->m_amount;

    int32 lRage = ctx.target->GetPower(POWER_RAGE);
    if (lRage > 100)                    // rage stored as rage*10
    {
        lRage = 100;
    }
    ctx.target->ModifyPower(POWER_RAGE, -lRage);
    int32 FRTriggerBasePoints = int32(lRage * LifePerRage / 10);
    ctx.target->CastCustomSpell(ctx.target, 22845, &FRTriggerBasePoints, NULL, NULL, true, NULL, ctx.aura);
    return SpellHandlerOutcome<void>::Return();
}

/// every other id: nothing
static SpellHandlerOutcome<void> AuraTriggerSpellDruidDefault(AuraTriggerSpellContext& /*ctx*/)
{
    return SpellHandlerOutcome<void>::Continue();
}

/// 28820: Lightning Shield
static SpellHandlerOutcome<void> AuraTriggerSpellShaman28820(AuraTriggerSpellContext& ctx)
{
    // Need remove self if Lightning Shield not active
    Unit::SpellAuraHolderMap const& auras = ctx.triggerTarget->GetSpellAuraHolderMap();
    for (Unit::SpellAuraHolderMap::const_iterator itr = auras.begin(); itr != auras.end(); ++itr)
    {
        SpellEntry const* spell = itr->second->GetSpellProto();
        if (spell->IsFitToFamily(SPELLFAMILY_SHAMAN, UI64LIT(0x0000000000000400)))
        {
            return SpellHandlerOutcome<void>::Return();
        }
    }
    ctx.triggerTarget->RemoveAurasDueToSpell(28820);
    return SpellHandlerOutcome<void>::Return();
}

/// 38443: Totemic Mastery
static SpellHandlerOutcome<void> AuraTriggerSpellShaman38443(AuraTriggerSpellContext& ctx)
{
    if (ctx.triggerTarget->IsAllTotemSlotsUsed())
    {
        ctx.triggerTarget->CastSpell(ctx.triggerTarget, 38437, true, NULL, ctx.aura);
    }
    else
    {
        ctx.triggerTarget->RemoveAurasDueToSpell(38437);
    }
    return SpellHandlerOutcome<void>::Return();
}

/// every other id: nothing
static SpellHandlerOutcome<void> AuraTriggerSpellShamanDefault(AuraTriggerSpellContext& /*ctx*/)
{
    return SpellHandlerOutcome<void>::Continue();
}

/// 9347: Mortal Strike
static SpellHandlerOutcome<void> AuraTriggerSpellCustom9347(AuraTriggerSpellContext& ctx)
{
    Creature* targetCreature = ctx.target->ToCreature();
    if (!targetCreature)
    {
        return SpellHandlerOutcome<void>::Return();
    }
    // expected selection current fight target
    ctx.triggerTarget = targetCreature->SelectAttackingTarget(ATTACKING_TARGET_TOPAGGRO, 0, ctx.triggeredSpellInfo);
    if (!ctx.triggerTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    return SpellHandlerOutcome<void>::Continue();
}

/// 1010: absent from the client's spell data
static SpellHandlerOutcome<void> AuraTriggerSpellCustom1010(AuraTriggerSpellContext& ctx)
{
    // TODO: spell casted by result in correct way mostly
    // BUT:
    // 1) target show casting at each triggered cast: target don't must show casting animation for any triggered spell
    //      but must show affect apply like item casting
    // 2) maybe aura must be replace by new with accumulative stat mods instead stacking

    // prevent cast by triggered auras
    if (ctx.casterGUID == ctx.triggerTarget->GetObjectGuid())
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // stop triggering after each affected stats lost > 90
    int32 intelectLoss = 0;
    int32 spiritLoss = 0;

    Unit::AuraList const& mModStat = ctx.triggerTarget->GetAurasByType(SPELL_AURA_MOD_STAT);
    for (Unit::AuraList::const_iterator i = mModStat.begin(); i != mModStat.end(); ++i)
    {
        if ((*i)->GetId() == 1010)
        {
            switch ((*i)->GetModifier()->m_miscvalue)
            {
                case STAT_INTELLECT: intelectLoss += (*i)->GetModifier()->m_amount; break;
                case STAT_SPIRIT:    spiritLoss   += (*i)->GetModifier()->m_amount; break;
                default: break;
            }
        }
    }

    if (intelectLoss <= -90 && spiritLoss <= -90)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    return SpellHandlerOutcome<void>::Continue();
}

/// 16191: Mana Tide
static SpellHandlerOutcome<void> AuraTriggerSpellCustom16191(AuraTriggerSpellContext& ctx)
{
    ctx.triggerTarget->CastCustomSpell(ctx.triggerTarget, ctx.trigger_spell_id, &ctx.aura->GetModifier()->m_amount, NULL, NULL, true, NULL, ctx.aura);
    return SpellHandlerOutcome<void>::Return();
}

/// 33525: Ground Slam
static SpellHandlerOutcome<void> AuraTriggerSpellCustom33525(AuraTriggerSpellContext& ctx)
{
    ctx.triggerTarget->CastSpell(ctx.triggerTarget, ctx.trigger_spell_id, true, NULL, ctx.aura, ctx.casterGUID);
    return SpellHandlerOutcome<void>::Return();
}

/// 38736: Rod of Purification
static SpellHandlerOutcome<void> AuraTriggerSpellCustom38736(AuraTriggerSpellContext& ctx)
{
    if (Unit* caster = ctx.aura->GetCaster())
    {
        caster->CastSpell(ctx.triggerTarget, ctx.trigger_spell_id, true, NULL, ctx.aura);
    }
    return SpellHandlerOutcome<void>::Return();
}

/// 43149: Claw Rage
static SpellHandlerOutcome<void> AuraTriggerSpellCustom43149(AuraTriggerSpellContext& ctx)
{
    // Need to provide explicit target for trigger spell target combination
    ctx.target->CastSpell(ctx.target->getVictim(), ctx.trigger_spell_id, true, NULL, ctx.aura);
    return SpellHandlerOutcome<void>::Return();
}

/// one body for 2 labels: 44883: Encapsulate, 56505: Surge of Power
static SpellHandlerOutcome<void> AuraTriggerSpellCustomSelfCast(AuraTriggerSpellContext& ctx)
{
    // Self cast spell, hence overwrite caster (only channeled spell where the triggered spell deals dmg to SELF)
    ctx.triggerCaster = ctx.triggerTarget;
    return SpellHandlerOutcome<void>::Continue();
}

/// 53563: Beacon of Light
static SpellHandlerOutcome<void> AuraTriggerSpellCustom53563(AuraTriggerSpellContext& ctx)
{
    // original caster must be target (beacon)
    ctx.target->CastSpell(ctx.target, ctx.trigger_spell_id, true, NULL, ctx.aura, ctx.target->GetObjectGuid());
    return SpellHandlerOutcome<void>::Return();
}

/// one body for 2 labels: 56654: Rapid Recuperation, 58882: Rapid Recuperation
static SpellHandlerOutcome<void> AuraTriggerSpellCustomRapidRecuperation(AuraTriggerSpellContext& ctx)
{
    int32 mana = ctx.target->GetMaxPower(POWER_MANA) * ctx.aura->GetModifier()->m_amount / 100;
    ctx.triggerTarget->CastCustomSpell(ctx.triggerTarget, ctx.trigger_spell_id, &mana, NULL, NULL, true, NULL, ctx.aura);
    return SpellHandlerOutcome<void>::Return();
}

/// one body for 7 labels: 43093: Grievous Throw, 31956 and 38801: Grievous Wound, 35321, 38363 and 39215:
/// Gushing Wound, 48920: Grievous Bite
static SpellHandlerOutcome<void> AuraPeriodicDamageTickRemoveAtFullHealth(AuraPeriodicDamageTickContext& ctx)
{
    if (ctx.target->GetHealth() == ctx.target->GetMaxHealth())
    {
        ctx.target->RemoveAurasDueToSpell(ctx.aura->GetId());
        return SpellHandlerOutcome<void>::Return();
    }
    return SpellHandlerOutcome<void>::Continue();
}

/// 38772: Grievous Wound
static SpellHandlerOutcome<void> AuraPeriodicDamageTick38772(AuraPeriodicDamageTickContext& ctx)
{
    uint32 percent =
        ctx.aura->GetEffIndex() < EFFECT_INDEX_2 && ctx.spellProto->GetSpellEffectIdByIndex(ctx.aura->GetEffIndex()) == SPELL_EFFECT_DUMMY ?
        ctx.pCaster->CalculateSpellDamage(ctx.target, ctx.spellProto, SpellEffectIndex(ctx.aura->GetEffIndex() + 1)) :
        100;
    if (ctx.target->GetHealth() * 100 >= ctx.target->GetMaxHealth() * percent)
    {
        ctx.target->RemoveAurasDueToSpell(ctx.aura->GetId());
        return SpellHandlerOutcome<void>::Return();
    }
    return SpellHandlerOutcome<void>::Continue();
}

/// every other id: nothing
static SpellHandlerOutcome<void> AuraPeriodicDamageTickDefault(AuraPeriodicDamageTickContext& /*ctx*/)
{
    return SpellHandlerOutcome<void>::Continue();
}

/// one body for 2 labels: 21056: Mark of Kazzak, 31447: Mark of Kaz'rogal
static SpellHandlerOutcome<void> AuraPeriodicManaLeechTickMarkOfKazzak(AuraPeriodicManaLeechTickContext& ctx)
{
    uint32 triggerSpell = 0;
    switch (ctx.aura->GetId())
    {
        case 21056: triggerSpell = 21058; break;
        case 31447: triggerSpell = 31463; break;
        case 32960: triggerSpell = 32961; break;
    }
    if (ctx.target->GetTypeId() == TYPEID_PLAYER && ctx.target->GetPower(ctx.power) == 0)
    {
        ctx.target->CastSpell(ctx.target, triggerSpell, true, NULL, ctx.aura);
        ctx.target->RemoveAurasDueToSpell(ctx.aura->GetId());
    }
    return SpellHandlerOutcome<void>::Continue();
}

/// 32960: Mark of Kazzak
static SpellHandlerOutcome<void> AuraPeriodicManaLeechTick32960(AuraPeriodicManaLeechTickContext& ctx)
{
    if (ctx.target->GetTypeId() == TYPEID_PLAYER && ctx.target->GetPowerType() == POWER_MANA)
    {
        // Drain 5% of target's mana
        ctx.pdamage = ctx.target->GetMaxPower(POWER_MANA) * 5 / 100;
        ctx.drain_amount = ctx.target->GetPower(POWER_MANA) > ctx.pdamage ? ctx.pdamage : ctx.target->GetPower(POWER_MANA);
        ctx.target->ModifyPower(POWER_MANA, -ctx.drain_amount);

        SpellPeriodicAuraLogInfo info(ctx.aura, ctx.drain_amount, 0, 0, 0, 0.0f);
        ctx.target->SendPeriodicAuraLog(&info);
    }
    // no break here
    return AuraPeriodicManaLeechTickMarkOfKazzak(ctx);
}

/// 7054: Forsaken Ability
static SpellHandlerOutcome<void> AuraPeriodicDummyTickGeneric7054(AuraPeriodicDummyTickContext& /*ctx*/)
{
    // Forsaken Skills
    // Possibly need cast one of them (but
    // 7038 Forsaken Skill: Swords
    // 7039 Forsaken Skill: Axes
    // 7040 Forsaken Skill: Daggers
    // 7041 Forsaken Skill: Maces
    // 7042 Forsaken Skill: Staves
    // 7043 Forsaken Skill: Bows
    // 7044 Forsaken Skill: Guns
    // 7045 Forsaken Skill: 2H Axes
    // 7046 Forsaken Skill: 2H Maces
    // 7047 Forsaken Skill: 2H Swords
    // 7048 Forsaken Skill: Defense
    // 7049 Forsaken Skill: Fire
    // 7050 Forsaken Skill: Frost
    // 7051 Forsaken Skill: Holy
    // 7053 Forsaken Skill: Shadow
    return SpellHandlerOutcome<void>::Return();
}

/// 7057: Haunting Spirits
static SpellHandlerOutcome<void> AuraPeriodicDummyTickGeneric7057(AuraPeriodicDummyTickContext& ctx)
{
    if (roll_chance_i(33))
    {
        ctx.target->CastSpell(ctx.target, ctx.aura->GetModifier()->m_amount, true, NULL, ctx.aura);
    }
    return SpellHandlerOutcome<void>::Return();
//              // Panda
//              case 19230: break;
}

/// 30019: Control Piece
static SpellHandlerOutcome<void> AuraPeriodicDummyTickGeneric30019(AuraPeriodicDummyTickContext& ctx)
{
    if (ctx.target->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    Unit* chessPiece = ctx.target->GetCharm();
    if (!chessPiece)
    {
        ctx.target->CastSpell(ctx.target, 30529, true);
        ctx.target->RemoveAurasDueToSpell(30019);
        ctx.target->RemoveAurasDueToSpell(30532);
    }
    return SpellHandlerOutcome<void>::Return();
//              // Gossip NPC Periodic - Talk
//              case 33208: break;
//              // Gossip NPC Periodic - Despawn
//              case 33209: break;
//              // Steal Weapon
//              case 36207: break;
//              // Simon Game START timer, (DND)
//              case 39993: break;
//              // Knockdown Fel Cannon: break; The Aggro Burst
//              case 40119: break;
//              // Old Mount Spell
//              case 40154: break;
//              // Magnetic Pull
//              case 40581: break;
//              // Ethereal Ring: break; The Bolt Burst
//              case 40801: break;
//              // Crystal Prison
//              case 40846: break;
//              // Copy Weapon
//              case 41054: break;
//              // Dementia
//              case 41404: break;
//              // Ethereal Ring Visual, Lightning Aura
//              case 41477: break;
//              // Ethereal Ring Visual, Lightning Aura (Fork)
//              case 41525: break;
//              // Ethereal Ring Visual, Lightning Jumper Aura
//              case 41567: break;
//              // No Man's Land
//              case 41955: break;
//              // Headless Horseman - Fire
//              case 42074: break;
//              // Headless Horseman - Visual - Large Fire
//              case 42075: break;
//              // Headless Horseman - Start Fire, Periodic Aura
//              case 42140: break;
//              // Ram Speed Boost
//              case 42152: break;
//              // Headless Horseman - Fires Out Victory Aura
//              case 42235: break;
//              // Pumpkin Life Cycle
//              case 42280: break;
//              // Brewfest Request Chick Chuck Mug Aura
//              case 42537: break;
//              // Squashling
//              case 42596: break;
//              // Headless Horseman Climax, Head: Periodic
//              case 42603: break;
}

/// 42621: Fire Bomb
static SpellHandlerOutcome<void> AuraPeriodicDummyTickGeneric42621(AuraPeriodicDummyTickContext& ctx)
{
    // Cast the summon spells (42622 to 42627) with increasing chance
    uint32 rand = urand(0, 99);
    for (uint32 i = 1; i <= 6; ++i)
    {
        if (rand < i * (i + 1) / 2 * 5)
        {
            ctx.target->CastSpell(ctx.target, ctx.spell->ID + i, true);
            break;
        }
    }
    return SpellHandlerOutcome<void>::Return();
//              // Headless Horseman - Conflagrate, Periodic Aura
//              case 42637: break;
//              // Headless Horseman - Create Pumpkin Treats Aura
//              case 42774: break;
//              // Headless Horseman Climax - Summoning Rhyme Aura
//              case 42879: break;
}

/// 42919: Tricky Treat
static SpellHandlerOutcome<void> AuraPeriodicDummyTickGeneric42919(AuraPeriodicDummyTickContext& ctx)
{
    ctx.target->CastSpell(ctx.target, 42966, true);
    return SpellHandlerOutcome<void>::Return();
//              // Giddyup!
//              case 42924: break;
//              // Ram - Trot
//              case 42992: break;
//              // Ram - Canter
//              case 42993: break;
//              // Ram - Gallop
//              case 42994: break;
//              // Ram Level - Neutral
//              case 43310: break;
//              // Headless Horseman - Maniacal Laugh, Maniacal, Delayed 17
//              case 43884: break;
//              // Wretched!
//              case 43963: break;
//              // Headless Horseman - Maniacal Laugh, Maniacal, other, Delayed 17
//              case 44000: break;
//              // Energy Feedback
//              case 44328: break;
//              // Romantic Picnic
//              case 45102: break;
//              // Romantic Picnic
//              case 45123: break;
//              // Looking for Love
//              case 45124: break;
//              // Kite - Lightning Strike Kite Aura
//              case 45197: break;
//              // Rocket Chicken
//              case 45202: break;
//              // Copy Offhand Weapon
//              case 45205: break;
//              // Upper Deck - Kite - Lightning Periodic Aura
//              case 45207: break;
//              // Kite -Sky  Lightning Strike Kite Aura
//              case 45251: break;
//              // Ribbon Pole Dancer Check Aura
//              case 45390: break;
//              // Holiday - Midsummer, Ribbon Pole Periodic Visual
//              case 45406: break;
//              // Parachute
//              case 45472: break;
//              // Alliance Flag, Extra Damage Debuff
//              case 45898: break;
//              // Horde Flag, Extra Damage Debuff
//              case 45899: break;
//              // Ahune - Summoning Rhyme Aura
//              case 45926: break;
//              // Ahune - Slippery Floor
//              case 45945: break;
//              // Ahune's Shield
//              case 45954: break;
//              // Nether Vapor Lightning
//              case 45960: break;
//              // Darkness
//              case 45996: break;
}

/// 46041: Summon Blood Elves Periodic
static SpellHandlerOutcome<void> AuraPeriodicDummyTickGeneric46041(AuraPeriodicDummyTickContext& ctx)
{
    ctx.target->CastSpell(ctx.target, 46037, true, NULL, ctx.aura);
    ctx.target->CastSpell(ctx.target, roll_chance_i(50) ? 46038 : 46039, true, NULL, ctx.aura);
    ctx.target->CastSpell(ctx.target, 46040, true, NULL, ctx.aura);
    return SpellHandlerOutcome<void>::Return();
//              // Transform Visual Missile Periodic
//              case 46205: break;
//              // Find Opening Beam End
//              case 46333: break;
}

/// 46371: Ice Spear Control Aura
static SpellHandlerOutcome<void> AuraPeriodicDummyTickGeneric46371(AuraPeriodicDummyTickContext& ctx)
{
    ctx.target->CastSpell(ctx.target, 46372, true, NULL, ctx.aura);
    return SpellHandlerOutcome<void>::Return();
//              // Hailstone Chill
//              case 46458: break;
//              // Hailstone Chill, Internal
//              case 46465: break;
//              // Chill, Internal Shifter
//              case 46549: break;
//              // Summon Ice Spear Knockback Delayer
//              case 46878: break;
//              // Burninate Effect
//              case 47214: break;
//              // Fizzcrank Practice Parachute
//              case 47228: break;
//              // Send Mug Control Aura
//              case 47369: break;
//              // Direbrew's Disarm (precast)
//              case 47407: break;
//              // Mole Machine Port Schedule
//              case 47489: break;
//              case 47941: break; // Crystal Spike
//              case 48200: break; // Healer Aura
}

/// one body for 2 labels: 48630: Summon Gauntlet Mobs Periodic, 59275: Summon Gauntlet Mobs Periodic
static SpellHandlerOutcome<void> AuraPeriodicDummyTickGenericSummonGauntletMobs(AuraPeriodicDummyTickContext& ctx)
{
    bool chance = roll_chance_i(50);

    ctx.target->CastSpell(ctx.target, chance ? 48631 : 48632, true, NULL, ctx.aura);

    if (ctx.aura->GetAuraTicks() % 2)                 // which doctor at odd tick
    {
        ctx.target->CastSpell(ctx.target, chance ? 48636 : 48635, true, NULL, ctx.aura);
    }
    else                                    // or harponeer, at even tick
    {
        ctx.target->CastSpell(ctx.target, chance ? 48634 : 48633, true, NULL, ctx.aura);
    }

    return SpellHandlerOutcome<void>::Return();
//              case 49313: break; // Proximity Mine Area Aura
//              // Mole Machine Portal Schedule
//              case 49466: break;
}

/// one body for 2 labels: 49555: Corpse Explode, 59807: Corpse Explode
static SpellHandlerOutcome<void> AuraPeriodicDummyTickGenericCorpseExplode(AuraPeriodicDummyTickContext& ctx)
{
    Creature* targetCreature = ctx.target->ToCreature();
    if (ctx.aura->GetAuraTicks() == 3 && targetCreature)
    {
        targetCreature->ForcedDespawn();
    }
    if (ctx.aura->GetAuraTicks() != 2)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    if (Unit* pCaster = ctx.aura->GetCaster())
    {
        pCaster->CastSpell(ctx.target, ctx.spell->ID == 49555 ? 49618 : 59809, true);
    }

    return SpellHandlerOutcome<void>::Return();
//              case 49592: break; // Temporal Rift
//              case 49957: break; // Cutting Laser
//              case 50085: break; // Slow Fall
//              // Listening to Music
//              case 50493: break;
//              // Love Rocket Barrage
//              case 50530: break;
}

/// one body for 2 labels: 50789: Summon Iron Dwarf, 59860: Summon Iron Dwarf
static SpellHandlerOutcome<void> AuraPeriodicDummyTickGenericSummonIronDwarf(AuraPeriodicDummyTickContext& ctx)
{
    ctx.target->CastSpell(ctx.target, roll_chance_i(50) ? 50790 : 50791, true, NULL, ctx.aura);
    return SpellHandlerOutcome<void>::Return();
}

/// one body for 2 labels: 50792: Summon Iron Trogg, 59859: Summon Iron Trogg
static SpellHandlerOutcome<void> AuraPeriodicDummyTickGenericSummonIronTrogg(AuraPeriodicDummyTickContext& ctx)
{
    ctx.target->CastSpell(ctx.target, roll_chance_i(50) ? 50793 : 50794, true, NULL, ctx.aura);
    return SpellHandlerOutcome<void>::Return();
}

/// one body for 2 labels: 50801: Summon Malformed Ooze, 59858: Summon Malformed Ooze
static SpellHandlerOutcome<void> AuraPeriodicDummyTickGenericSummonMalformedOoze(AuraPeriodicDummyTickContext& ctx)
{
    ctx.target->CastSpell(ctx.target, roll_chance_i(50) ? 50802 : 50803, true, NULL, ctx.aura);
    return SpellHandlerOutcome<void>::Return();
}

/// 50824: Summon Earthen Dwarf
static SpellHandlerOutcome<void> AuraPeriodicDummyTickGeneric50824(AuraPeriodicDummyTickContext& ctx)
{
    ctx.target->CastSpell(ctx.target, roll_chance_i(50) ? 50825 : 50826, true, NULL, ctx.aura);
    return SpellHandlerOutcome<void>::Return();
}

/// 52441: Cool Down
static SpellHandlerOutcome<void> AuraPeriodicDummyTickGeneric52441(AuraPeriodicDummyTickContext& ctx)
{
    ctx.target->CastSpell(ctx.target, 52443, true);
    return SpellHandlerOutcome<void>::Return();
}

/// one body for 3 labels: 53035: Summon Anub'ar Champion Periodic, 53036: Summon Anub'ar Necromancer Periodic,
/// 53037: Summon Anub'ar Crypt Fiend Periodic
static SpellHandlerOutcome<void> AuraPeriodicDummyTickGenericSummonAnubar(AuraPeriodicDummyTickContext& ctx)
{
    uint32 summonSpells[3][3] =
    {
        {53090, 53014, 53064},              // Summon Anub'ar Champion
        {53092, 53015, 53066},              // Summon Anub'ar Necromancer
        {53091, 53016, 53065}               // Summon Anub'ar Crypt Fiend
    };

    // Cast different spell depending on trigger position
    // This will summon a different npc entry on each location - each of those has individual movement patern
    if (ctx.target->Where().Z() < 750.0f)
    {
        ctx.target->CastSpell(ctx.target, summonSpells[ctx.spell->ID - 53035][0], true, NULL, ctx.aura);
    }
    else if (ctx.target->Where().X() > 500.0f)
    {
        ctx.target->CastSpell(ctx.target, summonSpells[ctx.spell->ID - 53035][1], true, NULL, ctx.aura);
    }
    else
    {
        ctx.target->CastSpell(ctx.target, summonSpells[ctx.spell->ID - 53035][2], true, NULL, ctx.aura);
    }

    return SpellHandlerOutcome<void>::Return();
}

/// 53520: Carrion Beetles
static SpellHandlerOutcome<void> AuraPeriodicDummyTickGeneric53520(AuraPeriodicDummyTickContext& ctx)
{
    ctx.target->CastSpell(ctx.target, 53521, true, NULL, ctx.aura);
    ctx.target->CastSpell(ctx.target, 53521, true, NULL, ctx.aura);
    return SpellHandlerOutcome<void>::Return();
}

/// 55592: Clean
static SpellHandlerOutcome<void> AuraPeriodicDummyTickGeneric55592(AuraPeriodicDummyTickContext& ctx)
{
    switch (urand(0, 2))
    {
        case 0: ctx.target->CastSpell(ctx.target, 55731, true); break;
        case 1: ctx.target->CastSpell(ctx.target, 55738, true); break;
        case 2: ctx.target->CastSpell(ctx.target, 55739, true); break;
    }
    return SpellHandlerOutcome<void>::Return();
}

/// 61968: Flash Freeze
static SpellHandlerOutcome<void> AuraPeriodicDummyTickGeneric61968(AuraPeriodicDummyTickContext& ctx)
{
    if (ctx.aura->GetAuraTicks() == 1 && !ctx.target->HasAura(62464))
    {
        ctx.target->CastSpell(ctx.target, 61970, true, NULL, ctx.aura);
    }
    return SpellHandlerOutcome<void>::Return();
}

/// 62018: Collapse
static SpellHandlerOutcome<void> AuraPeriodicDummyTickGeneric62018(AuraPeriodicDummyTickContext& ctx)
{
    // lose 1% of health every second
    ctx.target->DealDamage(ctx.target, ctx.target->GetMaxHealth() * .01, NULL, DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NONE, NULL, false);
    return SpellHandlerOutcome<void>::Return();
}

/// 62019: Rune of Summoning
static SpellHandlerOutcome<void> AuraPeriodicDummyTickGeneric62019(AuraPeriodicDummyTickContext& ctx)
{
    ctx.target->CastSpell(ctx.target, 62020, true, NULL, ctx.aura);
    return SpellHandlerOutcome<void>::Return();
}

/// 62038: Biting Cold
static SpellHandlerOutcome<void> AuraPeriodicDummyTickGeneric62038(AuraPeriodicDummyTickContext& ctx)
{
    Player* targetPlayer = ctx.target->ToPlayer();
    if (!targetPlayer)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // if player is moving remove one aura stack
    if (targetPlayer->isMoving())
    {
        ctx.target->RemoveAuraHolderFromStack(62039);
    }
    // otherwise add one aura stack each 3 seconds
    else if (ctx.aura->GetAuraTicks() % 3 && !ctx.target->HasAura(62821))
    {
        ctx.target->CastSpell(ctx.target, 62039, true, NULL, ctx.aura);
    }
    return SpellHandlerOutcome<void>::Return();
}

/// 62039: Biting Cold
static SpellHandlerOutcome<void> AuraPeriodicDummyTickGeneric62039(AuraPeriodicDummyTickContext& ctx)
{
    ctx.target->CastSpell(ctx.target, 62188, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 62566: absent from the client's spell data
static SpellHandlerOutcome<void> AuraPeriodicDummyTickGeneric62566(AuraPeriodicDummyTickContext& ctx)
{
    ctx.target->CastSpell(ctx.target, 62582, true);
    ctx.target->CastSpell(ctx.target, 62591, true);
    ctx.target->CastSpell(ctx.target, 62592, true);
    ctx.target->CastSpell(ctx.target, 62593, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 62717: Slag Pot
static SpellHandlerOutcome<void> AuraPeriodicDummyTickGeneric62717(AuraPeriodicDummyTickContext& ctx)
{
    ctx.target->CastSpell(ctx.target, ctx.target->GetMap()->IsRegularDifficulty() ? 65722 : 65723, true, NULL, ctx.aura);

    // cast Slag Imbued if the target survives up to the last tick
    if (ctx.aura->GetAuraTicks() == 10)
    {
        ctx.target->CastSpell(ctx.target, 63536, true, NULL, ctx.aura);
    }
    return SpellHandlerOutcome<void>::Return();
}

/// 63050: Sanity
static SpellHandlerOutcome<void> AuraPeriodicDummyTickGeneric63050(AuraPeriodicDummyTickContext& ctx)
{
    if (ctx.aura->GetHolder()->GetStackAmount() <= 25 && !ctx.target->HasAura(63752))
    {
        ctx.target->CastSpell(ctx.target, 63752, true);
    }
    else if (ctx.aura->GetHolder()->GetStackAmount() > 25 && ctx.target->HasAura(63752))
    {
        ctx.target->RemoveAurasDueToSpell(63752);
    }
    return SpellHandlerOutcome<void>::Return();
}

/// 63382: Rapid Burst
static SpellHandlerOutcome<void> AuraPeriodicDummyTickGeneric63382(AuraPeriodicDummyTickContext& ctx)
{
    if (ctx.aura->GetAuraTicks() % 2)
    {
        ctx.target->CastSpell(ctx.target, ctx.target->GetMap()->IsRegularDifficulty() ? 64019 : 64532, true);
    }
    else
    {
        ctx.target->CastSpell(ctx.target, ctx.target->GetMap()->IsRegularDifficulty() ? 63387 : 64531, true);
    }
    return SpellHandlerOutcome<void>::Return();
}

/// 64101: Defend
static SpellHandlerOutcome<void> AuraPeriodicDummyTickGeneric64101(AuraPeriodicDummyTickContext& ctx)
{
    ctx.target->CastSpell(ctx.target, 62719, true);
    ctx.target->CastSpell(ctx.target, 64192, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 64217: Overcharged
static SpellHandlerOutcome<void> AuraPeriodicDummyTickGeneric64217(AuraPeriodicDummyTickContext& ctx)
{
    if (ctx.aura->GetHolder()->GetStackAmount() >= 10)
    {
        ctx.target->CastSpell(ctx.target, 64219, true);
        ctx.target->DealDamage(ctx.target, ctx.target->GetHealth(), NULL, DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NORMAL, NULL, false);
    }
    return SpellHandlerOutcome<void>::Return();
}

/// 64412: Phase Punch
static SpellHandlerOutcome<void> AuraPeriodicDummyTickGeneric64412(AuraPeriodicDummyTickContext& ctx)
{
    if (SpellAuraHolder* phaseAura = ctx.target->GetSpellAuraHolder(64412))
    {
        uint32 uiAuraId = 0;
        switch (phaseAura->GetStackAmount())
        {
            case 1: uiAuraId = 64435; break;
            case 2: uiAuraId = 64434; break;
            case 3: uiAuraId = 64428; break;
            case 4: uiAuraId = 64421; break;
            case 5: uiAuraId = 64417; break;
        }

        if (uiAuraId && !ctx.target->HasAura(uiAuraId))
        {
            ctx.target->CastSpell(ctx.target, uiAuraId, true, NULL, ctx.aura);

            // remove original aura if phased
            if (uiAuraId == 64417)
            {
                ctx.target->RemoveAurasDueToSpell(64412);
                ctx.target->CastSpell(ctx.target, 62169, true, NULL, ctx.aura);
            }
        }
    }
    return SpellHandlerOutcome<void>::Return();
}

/// 65272: Shatter Chest
static SpellHandlerOutcome<void> AuraPeriodicDummyTickGeneric65272(AuraPeriodicDummyTickContext& ctx)
{
    ctx.target->CastSpell(ctx.target, 62501, true, NULL, ctx.aura);
    return SpellHandlerOutcome<void>::Return();
}

/// one body for 4 labels: 66118: Leeching Swarm, 67630: Leeching Swarm, 68646: Leeching Swarm, 68647: Leeching Swarm
static SpellHandlerOutcome<void> AuraPeriodicDummyTickGenericLeechingSwarm(AuraPeriodicDummyTickContext& ctx)
{
    Unit* caster = ctx.aura->GetCaster();
    if (!caster)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    int32 lifeLeeched = int32(ctx.target->GetHealth() * ctx.aura->GetModifier()->m_amount * 0.01f);

    if (lifeLeeched < 250)
    {
        lifeLeeched = 250;
    }

    // Leeching swarm damage
    caster->CastCustomSpell(ctx.target, 66240, &lifeLeeched, NULL, NULL, true, NULL, ctx.aura);

    // Leeching swarm heal
    ctx.target->CastCustomSpell(caster, 66125, &lifeLeeched, NULL, NULL, true, NULL, ctx.aura);

    return SpellHandlerOutcome<void>::Return();
}

/// 66798: Death's Respite
static SpellHandlerOutcome<void> AuraPeriodicDummyTickGeneric66798(AuraPeriodicDummyTickContext& ctx)
{
    Unit* caster = ctx.aura->GetCaster();
    if (!caster)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    caster->CastSpell(ctx.target, 66797, true, NULL, ctx.aura);
    ctx.target->RemoveAurasDueToSpell(ctx.aura->GetId());
    return SpellHandlerOutcome<void>::Return();
}

/// one body for 2 labels: 68875: Wailing Souls, 68876: Wailing Souls
static SpellHandlerOutcome<void> AuraPeriodicDummyTickGenericWailingSouls(AuraPeriodicDummyTickContext& ctx)
{
    // Sweep around
    float newAngle = ctx.target->Where().Facing();
    if (ctx.spell->ID == 68875)
    {
        newAngle += 0.09f;
    }
    else
    {
        newAngle -= 0.09f;
    }

    newAngle = Geometry::Placement::NormalizeOrientation(newAngle);

    ctx.target->SetFacingTo(newAngle);

    // Should actually be SMSG_SPELL_START, too
    ctx.target->CastSpell(ctx.target, 68873, true);
    return SpellHandlerOutcome<void>::Return();
// Exist more after, need add later
}

/// every other id: nothing
static SpellHandlerOutcome<void> AuraPeriodicDummyTickGenericDefault(AuraPeriodicDummyTickContext& /*ctx*/)
{
    return SpellHandlerOutcome<void>::Continue();
}

/// 55342: Mirror Image
static SpellHandlerOutcome<void> AuraPeriodicDummyTickMage55342(AuraPeriodicDummyTickContext& ctx)
{
    if (ctx.aura->GetAuraTicks() != 1)
    {
        return SpellHandlerOutcome<void>::Return();
    }
    if (Unit* pCaster = ctx.aura->GetCaster())
    {
        pCaster->CastSpell(pCaster, ctx.aura->GetSpellProto()->CalculateSimpleValue(ctx.aura->GetEffIndex()), true, NULL, ctx.aura);
    }
    return SpellHandlerOutcome<void>::Return();
}

/// every other id: nothing
static SpellHandlerOutcome<void> AuraPeriodicDummyTickMageDefault(AuraPeriodicDummyTickContext& /*ctx*/)
{
    return SpellHandlerOutcome<void>::Continue();
}

/// 22842: Frenzied Regeneration
static SpellHandlerOutcome<void> AuraPeriodicDummyTickDruid22842(AuraPeriodicDummyTickContext& ctx)
{
    // Frenzied Regeneration
    // Converts up to 10 rage per second into health for $d.  Each point of rage is converted into ${$m2/10}.1% of max health.
    // Should be manauser
    if (ctx.target->GetPowerType() != POWER_RAGE)
    {
        return SpellHandlerOutcome<void>::Return();
    }
    uint32 rage = ctx.target->GetPower(POWER_RAGE);
    // Nothing todo
    if (rage == 0)
    {
        return SpellHandlerOutcome<void>::Return();
    }
    int32 mod = (rage < 100) ? rage : 100;
    int32 points = ctx.target->CalculateSpellDamage(ctx.target, ctx.spell, EFFECT_INDEX_1);
    int32 regen = ctx.target->GetMaxHealth() * (mod * points / 10) / 1000;
    ctx.target->CastCustomSpell(ctx.target, 22845, &regen, NULL, NULL, true, NULL, ctx.aura);
    ctx.target->SetPower(POWER_RAGE, rage - mod);
    return SpellHandlerOutcome<void>::Return();
// Force of Nature
}

/// 33831: Force of Nature
static SpellHandlerOutcome<void> AuraPeriodicDummyTickDruid33831(AuraPeriodicDummyTickContext& /*ctx*/)
{
    return SpellHandlerOutcome<void>::Return();
}

/// every other id: nothing
static SpellHandlerOutcome<void> AuraPeriodicDummyTickDruidDefault(AuraPeriodicDummyTickContext& /*ctx*/)
{
    return SpellHandlerOutcome<void>::Continue();
}

/// 51690: Killing Spree
static SpellHandlerOutcome<void> AuraPeriodicDummyTickRogue51690(AuraPeriodicDummyTickContext& ctx)
{
    if (ctx.target->Blocked(Motion::ReasonStunned) || ctx.target->isFeared())
    {
        return SpellHandlerOutcome<void>::Return();
    }

    Spell::UnitList targets;
    {
        // eff_radius ==0
        float radius = GetSpellMaxRange(sSpellRangeStore.LookupEntry(ctx.spell->RangeIndex));

        MaNGOS::AnyUnfriendlyVisibleUnitInObjectRangeCheck u_check(ctx.target, ctx.target, radius);
        MaNGOS::UnitListSearcher<MaNGOS::AnyUnfriendlyVisibleUnitInObjectRangeCheck> checker(targets, u_check);
        Cell::VisitAllObjects(ctx.target, checker, radius);
    }

    if (targets.empty())
    {
        return SpellHandlerOutcome<void>::Return();
    }

    Spell::UnitList::const_iterator itr = targets.begin();
    std::advance(itr, urand(0, targets.size() - 1));
    Unit* victim = *itr;

    ctx.target->CastSpell(victim, 57840, true);
    ctx.target->CastSpell(victim, 57841, true);
    return SpellHandlerOutcome<void>::Return();
}

/// every other id: nothing
static SpellHandlerOutcome<void> AuraPeriodicDummyTickRogueDefault(AuraPeriodicDummyTickContext& /*ctx*/)
{
    return SpellHandlerOutcome<void>::Continue();
}

/// one body for 2 labels: 53511: Feeding Frenzy, 53512: Feeding Frenzy
static SpellHandlerOutcome<void> AuraPeriodicDummyTickHunterFeedingFrenzy(AuraPeriodicDummyTickContext& ctx)
{
    // Harpooner's Mark
    // case 40084:
    //    return;
    // Feeding Frenzy Rank 1 & 2
    Unit* victim = ctx.target->getVictim();
    if (victim && victim->GetHealth() * 100 < victim->GetMaxHealth() * 35)
    {
        ctx.target->CastSpell(ctx.target, ctx.spell->ID == 53511 ? 60096 : 60097, true, NULL, ctx.aura);
    }
    return SpellHandlerOutcome<void>::Return();
}

/// every other id: nothing
static SpellHandlerOutcome<void> AuraPeriodicDummyTickHunterDefault(AuraPeriodicDummyTickContext& /*ctx*/)
{
    return SpellHandlerOutcome<void>::Continue();
}

/// 29865: Deathbloom
static SpellHandlerOutcome<void> AuraBoostGeneric29865(AuraBoostContext& ctx)
{
    if (!ctx.apply && ctx.holder->GetRemoveMode() == AURA_REMOVE_BY_EXPIRE)
    {
        ctx.cast_at_remove = true;
        ctx.spellId1 = 55594;
    }
    else
    {
        return SpellHandlerOutcome<void>::Return();
    }
    return SpellHandlerOutcome<void>::Continue();
}

/// 55053: Deathbloom
static SpellHandlerOutcome<void> AuraBoostGeneric55053(AuraBoostContext& ctx)
{
    if (!ctx.apply && ctx.holder->GetRemoveMode() == AURA_REMOVE_BY_EXPIRE)
    {
        ctx.cast_at_remove = true;
        ctx.spellId1 = 55601;
    }
    else
    {
        return SpellHandlerOutcome<void>::Return();
    }
    return SpellHandlerOutcome<void>::Continue();
}

/// 50720: Vigilance
static SpellHandlerOutcome<void> AuraBoostGeneric50720(AuraBoostContext& ctx)
{
    ctx.spellId1 = 68066;                       // Damage Reduction
    return SpellHandlerOutcome<void>::Continue();
}

/// 57350: Illusionary Barrier
static SpellHandlerOutcome<void> AuraBoostGeneric57350(AuraBoostContext& ctx)
{
    if (!ctx.apply && ctx.holder->GetTarget()->GetPowerType() == POWER_MANA)
    {
        ctx.cast_at_remove = true;
        ctx.spellId1 = 60242;                   // Darkmoon Card: Illusion
    }
    else
    {
        return SpellHandlerOutcome<void>::Return();
    }
    return SpellHandlerOutcome<void>::Continue();
}

/// 58914: absent from the client's spell data
static SpellHandlerOutcome<void> AuraBoostGeneric58914(AuraBoostContext& ctx)
{
    // Removal is needed here because the dummy aura handler is applied / removed at stacks change
    if (!ctx.apply)
        if (Unit* caster = ctx.holder->GetCaster())
        {
            caster->RemoveAurasDueToSpell(34027);
        }
    return SpellHandlerOutcome<void>::Return();
}

/// 62692: Aura of Despair
static SpellHandlerOutcome<void> AuraBoostGeneric62692(AuraBoostContext& ctx)
{
    ctx.spellId1 = 64848;
    return SpellHandlerOutcome<void>::Continue();
}

/// 71905: Soul Fragment
static SpellHandlerOutcome<void> AuraBoostGeneric71905(AuraBoostContext& ctx)
{
    if (!ctx.apply)
    {
        ctx.spellId1 = 72521;                   // Shadowmourne Visual Low
        ctx.spellId2 = 72523;                   // Shadowmourne Visual High
    }
    else
    {
        return SpellHandlerOutcome<void>::Return();
    }
    return SpellHandlerOutcome<void>::Continue();
}

/// every other id: nothing
static SpellHandlerOutcome<void> AuraBoostGenericDefault(AuraBoostContext& /*ctx*/)
{
    return SpellHandlerOutcome<void>::Return();
}

/// 11129: Combustion
static SpellHandlerOutcome<void> AuraBoostMage11129(AuraBoostContext& ctx)
{
    if (!ctx.apply)
    {
        ctx.spellId1 = 28682;
    }
    else
    {
        return SpellHandlerOutcome<void>::Return();
    }
    return SpellHandlerOutcome<void>::Continue();
}

/// 28682: absent from the client's spell data
static SpellHandlerOutcome<void> AuraBoostMage28682(AuraBoostContext& ctx)
{
    if (!ctx.apply)
    {
        ctx.spellId1 = 11129;
    }
    else
    {
        return SpellHandlerOutcome<void>::Return();
    }
    return SpellHandlerOutcome<void>::Continue();
}

/// one body for 3 labels: 44401: absent from the client's spell data, 48108: Hot Streak, 57761: Brain Freeze
static SpellHandlerOutcome<void> AuraBoostMagePushingTheLimit(AuraBoostContext& ctx)
{
    // consumed aura (at proc charges 0)
    if (!ctx.apply && ctx.holder->GetRemoveMode() == AURA_REMOVE_BY_DEFAULT)
    {
        Unit* caster = ctx.holder->GetCaster();
        // Item - Mage T10 2P Bonus
        if (!caster || !caster->HasAura(70752))
        {
            return SpellHandlerOutcome<void>::Return();
        }

        ctx.cast_at_remove = true;
        ctx.spellId1 = 70753;                   // Pushing the Limit
    }
    else
    {
        return SpellHandlerOutcome<void>::Return();
    }
    return SpellHandlerOutcome<void>::Continue();
}

/// 74396: absent from the client's spell data
static SpellHandlerOutcome<void> AuraBoostMage74396(AuraBoostContext& ctx)
{
    if (!ctx.apply)
    {
        ctx.spellId1 = 44544;
    }
    else
    {
        return SpellHandlerOutcome<void>::Return();
    }
    return SpellHandlerOutcome<void>::Continue();
}

/// every other id: nothing
static SpellHandlerOutcome<void> AuraBoostMageDefault(AuraBoostContext& /*ctx*/)
{
    return SpellHandlerOutcome<void>::Return();
}

/// 552: absent from the client's spell data
static SpellHandlerOutcome<void> AuraBoostPriest552(AuraBoostContext& ctx)
{
        // Abolish Disease (remove 1 more poison effect with Body and Soul)
    if (ctx.apply)
    {
        int chance = 0;
        Unit::AuraList const& dummyAuras = ctx.holder->GetTarget()->GetAurasByType(SPELL_AURA_DUMMY);
        for (Unit::AuraList::const_iterator itr = dummyAuras.begin(); itr != dummyAuras.end(); ++itr)
        {
            SpellEntry const* dummyEntry = (*itr)->GetSpellProto();
            // Body and Soul (talent ranks)
            if (dummyEntry->GetSpellFamilyName() == SPELLFAMILY_PRIEST && dummyEntry->SpellIconID == 2218 &&
                dummyEntry->SpellVisualID[0]==0)
            {
                chance = (*itr)->GetSpellProto()->CalculateSimpleValue(EFFECT_INDEX_1);
                break;
            }
        }

        if (roll_chance_i(chance))
        {
            ctx.spellId1 = 64134;               // Body and Soul (periodic dispel effect)
        }
    }
    else
    {
        ctx.spellId1 = 64134;                   // Body and Soul (periodic dispel effect)
    }
    return SpellHandlerOutcome<void>::Continue();
// Dispersion mana reg and immunity
}

/// 47585: Dispersion
static SpellHandlerOutcome<void> AuraBoostPriest47585(AuraBoostContext& ctx)
{
    ctx.spellId1 = 60069;                       // Dispersion
    ctx.spellId2 = 63230;                       // Dispersion
    return SpellHandlerOutcome<void>::Continue();
}

/// every other id: nothing
static SpellHandlerOutcome<void> AuraBoostPriestDefault(AuraBoostContext& /*ctx*/)
{
    return SpellHandlerOutcome<void>::Return();
}

/// 34074: absent from the client's spell data
static SpellHandlerOutcome<void> AuraBoostHunter34074(AuraBoostContext& ctx)
{
    if (!ctx.apply || ctx.holder->GetTarget()->HasAura(60144)) // Viper Attack Speed
    {
        ctx.spellId1 = 61609;                   // Vicious Viper
    }
    else
    {
        return SpellHandlerOutcome<void>::Return();
    }
    return SpellHandlerOutcome<void>::Continue();
}

/// one body for 2 labels: 19574: Bestial Wrath, 34471: The Beast Within
static SpellHandlerOutcome<void> AuraBoostHunterBestialWrathImmunity(AuraBoostContext& ctx)
{
    ctx.spellId1 = 24395;
    ctx.spellId2 = 24396;
    ctx.spellId3 = 24397;
    ctx.spellId4 = 26592;
    return SpellHandlerOutcome<void>::Continue();
}

/// 34027: absent from the client's spell data
static SpellHandlerOutcome<void> AuraBoostHunter34027(AuraBoostContext& ctx)
{
    if (ctx.apply)
    {
        if (ctx.holder->GetTarget()->HasAura(35029))       // Focused Fire, rank 1
        {
            ctx.spellId1 = 60110;               // Kill Command, Focused Fire rank 1 bonus
        }
        else if (ctx.holder->GetTarget()->HasAura(35030))  // Focused Fire, rank 2
        {
            ctx.spellId1 = 60113;               // Kill Command, Focused Fire rank 2 bonus
        }
        else
        {
            return SpellHandlerOutcome<void>::Return();
        }
    }
    else
    {
        ctx.spellId1 = 34026;                   // Kill Command, owner casting aura
        ctx.spellId2 = 60110;                   // Kill Command, Focused Fire rank 1 bonus
        ctx.spellId3 = 60113;                   // Kill Command, Focused Fire rank 2 bonus
        if (Unit* pet = ctx.holder->GetTarget()->GetPet())
        {
            pet->RemoveAurasDueToSpell(58914); // Kill Command, pet aura
        }
    }
    return SpellHandlerOutcome<void>::Continue();
}

/// 35029: Improved Kill Command
static SpellHandlerOutcome<void> AuraBoostHunter35029(AuraBoostContext& ctx)
{
    if (ctx.apply && !ctx.holder->GetTarget()->HasAura(34027)) // Kill Command, owner casting aura
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.spellId1 = 60110;                       // Kill Command, Focused Fire rank 1 bonus
    return SpellHandlerOutcome<void>::Continue();
}

/// 35030: Improved Kill Command
static SpellHandlerOutcome<void> AuraBoostHunter35030(AuraBoostContext& ctx)
{
    if (ctx.apply && !ctx.holder->GetTarget()->HasAura(34027)) // Kill Command, owner casting aura
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.spellId1 = 60113;                       // Kill Command, Focused Fire rank 2 bonus
    return SpellHandlerOutcome<void>::Continue();
}

/// every other id: as the switch had it
static SpellHandlerOutcome<void> AuraBoostHunterDefault(AuraBoostContext& ctx)
{
    // Freezing Trap Effect
    if (ctx.holder->GetSpellProto()->IsFitToFamilyMask(UI64LIT(0x0000000000000008)))
    {
        if (!ctx.apply)
        {
            Unit* caster = ctx.holder->GetCaster();
            // Glyph of Freezing Trap
            if (caster && caster->HasAura(56845))
            {
                ctx.cast_at_remove = true;
                ctx.spellId1 = 61394;
            }
            else
            {
                return SpellHandlerOutcome<void>::Return();
            }
        }
        else
        {
            return SpellHandlerOutcome<void>::Return();
        }
    }
    // Aspect of the Dragonhawk dodge
    else if (ctx.holder->GetSpellProto()->IsFitToFamilyMask(UI64LIT(0x0000000000000000), 0x00001000))
    {
        ctx.spellId1 = 61848;

        // triggered spell have same category as main spell and cooldown
        if (ctx.apply && ctx.holder->GetTarget()->GetTypeId() == TYPEID_PLAYER)
        {
            ctx.holder->GetTarget()->RemoveSpellCooldown(61848);
        }
    }
    else
    {
        return SpellHandlerOutcome<void>::Return();
    }
    return SpellHandlerOutcome<void>::Continue();
}

/// 49039: Lichborne
static SpellHandlerOutcome<void> AuraBoostDeathKnight49039(AuraBoostContext& ctx)
{
    ctx.spellId1 = 50397;
    return SpellHandlerOutcome<void>::Continue();
}

/// one body for 3 labels: 48263: Blood Presence, 48265: Unholy Presence, 48266: Frost Presence
static SpellHandlerOutcome<void> AuraBoostDeathKnightPresence(AuraBoostContext& ctx)
{
    // else part one per 3 pair
    if (ctx.holder->GetId() == 48263 || ctx.holder->GetId() == 48265) // Frost Presence or Unholy Presence
    {
        // Improved Blood Presence
        int32 heal_pct = 0;
        if (ctx.apply)
        {
            Unit::AuraList const& bloodAuras = ctx.holder->GetTarget()->GetAurasByType(SPELL_AURA_DUMMY);
            for (Unit::AuraList::const_iterator itr = bloodAuras.begin(); itr != bloodAuras.end(); ++itr)
            {
                // skip same icon
                if ((*itr)->GetSpellProto()->GetSpellFamilyName() == SPELLFAMILY_DEATHKNIGHT &&
                    (*itr)->GetSpellProto()->SpellIconID == 2636)
                {
                    heal_pct = (*itr)->GetModifier()->m_amount;
                    break;
                }
            }
        }

        if (heal_pct)
        {
            ctx.holder->GetTarget()->CastCustomSpell(ctx.holder->GetTarget(), 63611, &heal_pct, NULL, NULL, true, NULL, NULL, ctx.holder->GetCasterGuid());
        }
        else
        {
            ctx.holder->GetTarget()->RemoveAurasDueToSpell(63611);
        }
    }
    else
        ctx.spellId1 = 63611;                   // Improved Blood Presence, trigger for heal

    if (ctx.holder->GetId() == 48263 || ctx.holder->GetId() == 48266) // Frost Presence or Blood Presence
    {
        // Improved Unholy Presence
        int32 power_pct = 0;
        if (ctx.apply)
        {
            Unit::AuraList const& unholyAuras = ctx.holder->GetTarget()->GetAurasByType(SPELL_AURA_DUMMY);
            for (Unit::AuraList::const_iterator itr = unholyAuras.begin(); itr != unholyAuras.end(); ++itr)
            {
                // skip same icon
                if ((*itr)->GetSpellProto()->GetSpellFamilyName() == SPELLFAMILY_DEATHKNIGHT &&
                    (*itr)->GetSpellProto()->SpellIconID == 2633)
                {
                    power_pct = (*itr)->GetModifier()->m_amount;
                    break;
                }
            }
        }
        if (power_pct || !ctx.apply)
            ctx.spellId2 = 49772;               // Unholy Presence, speed part, spell1 used for Improvement presence fit to own presence
    }
    else
    {
        ctx.spellId1 = 49772;                   // Unholy Presence move speed
    }

    if (ctx.holder->GetId() == 48265 || ctx.holder->GetId() == 48266)   // Unholy Presence or Blood Presence
    {
        // Improved Frost Presence
        int32 stamina_pct = 0;
        if (ctx.apply)
        {
            Unit::AuraList const& frostAuras = ctx.holder->GetTarget()->GetAurasByType(SPELL_AURA_DUMMY);
            for (Unit::AuraList::const_iterator itr = frostAuras.begin(); itr != frostAuras.end(); ++itr)
            {
                // skip same icon
                if ((*itr)->GetSpellProto()->GetSpellFamilyName() == SPELLFAMILY_DEATHKNIGHT &&
                    (*itr)->GetSpellProto()->SpellIconID == 2632)
                {
                    stamina_pct = (*itr)->GetModifier()->m_amount;
                    break;
                }
            }
        }

        if (stamina_pct)
        {
            ctx.holder->GetTarget()->CastCustomSpell(ctx.holder->GetTarget(), 61261, &stamina_pct, NULL, NULL, true, NULL, NULL, ctx.holder->GetCasterGuid());
        }
        else
        {
            ctx.holder->GetTarget()->RemoveAurasDueToSpell(61261);
        }
    }
    else
    {
        ctx.spellId1 = 61261;                   // Frost Presence, stamina
    }

    if (ctx.holder->GetId() == 48265)                   // Unholy Presence
    {
        // Improved Unholy Presence, special case for own presence
        int32 power_pct = 0;
        if (ctx.apply)
        {
            Unit::AuraList const& unholyAuras = ctx.holder->GetTarget()->GetAurasByType(SPELL_AURA_DUMMY);
            for (Unit::AuraList::const_iterator itr = unholyAuras.begin(); itr != unholyAuras.end(); ++itr)
            {
                // skip same icon
                if ((*itr)->GetSpellProto()->GetSpellFamilyName() == SPELLFAMILY_DEATHKNIGHT &&
                    (*itr)->GetSpellProto()->SpellIconID == 2633)
                {
                    power_pct = (*itr)->GetModifier()->m_amount;
                    break;
                }
            }
        }

        if (power_pct)
        {
            int32 bp = 5;
            ctx.holder->GetTarget()->CastCustomSpell(ctx.holder->GetTarget(), 63622, &bp, &bp, &bp, true, NULL, NULL, ctx.holder->GetCasterGuid());
            ctx.holder->GetTarget()->CastCustomSpell(ctx.holder->GetTarget(), 65095, &bp, NULL, NULL, true, NULL, NULL, ctx.holder->GetCasterGuid());
        }
        else
        {
            ctx.holder->GetTarget()->RemoveAurasDueToSpell(63622);
            ctx.holder->GetTarget()->RemoveAurasDueToSpell(65095);
        }
    }
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

    static AurasRow<AuraTriggerSpellMageSite> const triggerSpellMage[] =
    {
        { 66, &AuraTriggerSpellMage66 },
    };

    static AurasRow<AuraTriggerSpellHunterSite> const triggerSpellHunter[] =
    {
        { 53302, &AuraTriggerSpellHunterSniperTraining },
        { 53303, &AuraTriggerSpellHunterSniperTraining },
        { 53304, &AuraTriggerSpellHunterSniperTraining },
    };

    static AurasRow<AuraTriggerSpellDruidSite> const triggerSpellDruid[] =
    {
        { 768, &AuraTriggerSpellDruid768 },
        { 22842, &AuraTriggerSpellDruidFrenziedRegeneration },
        { 22895, &AuraTriggerSpellDruidFrenziedRegeneration },
        { 22896, &AuraTriggerSpellDruidFrenziedRegeneration },
        { 26999, &AuraTriggerSpellDruidFrenziedRegeneration },
    };

    static AurasRow<AuraTriggerSpellShamanSite> const triggerSpellShaman[] =
    {
        { 28820, &AuraTriggerSpellShaman28820 },
        { 38443, &AuraTriggerSpellShaman38443 },
    };

    static AurasRow<AuraTriggerSpellCustomSite> const triggerSpellCustom[] =
    {
        { 9347, &AuraTriggerSpellCustom9347 },
        { 1010, &AuraTriggerSpellCustom1010 },
        { 16191, &AuraTriggerSpellCustom16191 },
        { 33525, &AuraTriggerSpellCustom33525 },
        { 38736, &AuraTriggerSpellCustom38736 },
        { 43149, &AuraTriggerSpellCustom43149 },
        { 44883, &AuraTriggerSpellCustomSelfCast },
        { 56505, &AuraTriggerSpellCustomSelfCast },
        { 53563, &AuraTriggerSpellCustom53563 },
        { 56654, &AuraTriggerSpellCustomRapidRecuperation },
        { 58882, &AuraTriggerSpellCustomRapidRecuperation },
    };

    static AurasRow<AuraPeriodicDamageTickSite> const periodicDamageTick[] =
    {
        { 43093, &AuraPeriodicDamageTickRemoveAtFullHealth },
        { 31956, &AuraPeriodicDamageTickRemoveAtFullHealth },
        { 38801, &AuraPeriodicDamageTickRemoveAtFullHealth },
        { 35321, &AuraPeriodicDamageTickRemoveAtFullHealth },
        { 38363, &AuraPeriodicDamageTickRemoveAtFullHealth },
        { 39215, &AuraPeriodicDamageTickRemoveAtFullHealth },
        { 48920, &AuraPeriodicDamageTickRemoveAtFullHealth },
        { 38772, &AuraPeriodicDamageTick38772 },
    };

    static AurasRow<AuraPeriodicManaLeechTickSite> const periodicManaLeechTick[] =
    {
        { 32960, &AuraPeriodicManaLeechTick32960 },
        { 21056, &AuraPeriodicManaLeechTickMarkOfKazzak },
        { 31447, &AuraPeriodicManaLeechTickMarkOfKazzak },
    };

    static AurasRow<AuraPeriodicDummyTickGenericSite> const periodicDummyTickGeneric[] =
    {
        { 7054, &AuraPeriodicDummyTickGeneric7054 },
        { 7057, &AuraPeriodicDummyTickGeneric7057 },
        { 30019, &AuraPeriodicDummyTickGeneric30019 },
        { 42621, &AuraPeriodicDummyTickGeneric42621 },
        { 42919, &AuraPeriodicDummyTickGeneric42919 },
        { 46041, &AuraPeriodicDummyTickGeneric46041 },
        { 46371, &AuraPeriodicDummyTickGeneric46371 },
        { 48630, &AuraPeriodicDummyTickGenericSummonGauntletMobs },
        { 59275, &AuraPeriodicDummyTickGenericSummonGauntletMobs },
        { 49555, &AuraPeriodicDummyTickGenericCorpseExplode },
        { 59807, &AuraPeriodicDummyTickGenericCorpseExplode },
        { 50789, &AuraPeriodicDummyTickGenericSummonIronDwarf },
        { 59860, &AuraPeriodicDummyTickGenericSummonIronDwarf },
        { 50792, &AuraPeriodicDummyTickGenericSummonIronTrogg },
        { 59859, &AuraPeriodicDummyTickGenericSummonIronTrogg },
        { 50801, &AuraPeriodicDummyTickGenericSummonMalformedOoze },
        { 59858, &AuraPeriodicDummyTickGenericSummonMalformedOoze },
        { 50824, &AuraPeriodicDummyTickGeneric50824 },
        { 52441, &AuraPeriodicDummyTickGeneric52441 },
        { 53035, &AuraPeriodicDummyTickGenericSummonAnubar },
        { 53036, &AuraPeriodicDummyTickGenericSummonAnubar },
        { 53037, &AuraPeriodicDummyTickGenericSummonAnubar },
        { 53520, &AuraPeriodicDummyTickGeneric53520 },
        { 55592, &AuraPeriodicDummyTickGeneric55592 },
        { 61968, &AuraPeriodicDummyTickGeneric61968 },
        { 62018, &AuraPeriodicDummyTickGeneric62018 },
        { 62019, &AuraPeriodicDummyTickGeneric62019 },
        { 62038, &AuraPeriodicDummyTickGeneric62038 },
        { 62039, &AuraPeriodicDummyTickGeneric62039 },
        { 62566, &AuraPeriodicDummyTickGeneric62566 },
        { 62717, &AuraPeriodicDummyTickGeneric62717 },
        { 63050, &AuraPeriodicDummyTickGeneric63050 },
        { 63382, &AuraPeriodicDummyTickGeneric63382 },
        { 64101, &AuraPeriodicDummyTickGeneric64101 },
        { 64217, &AuraPeriodicDummyTickGeneric64217 },
        { 64412, &AuraPeriodicDummyTickGeneric64412 },
        { 65272, &AuraPeriodicDummyTickGeneric65272 },
        { 66118, &AuraPeriodicDummyTickGenericLeechingSwarm },
        { 67630, &AuraPeriodicDummyTickGenericLeechingSwarm },
        { 68646, &AuraPeriodicDummyTickGenericLeechingSwarm },
        { 68647, &AuraPeriodicDummyTickGenericLeechingSwarm },
        { 66798, &AuraPeriodicDummyTickGeneric66798 },
        { 68875, &AuraPeriodicDummyTickGenericWailingSouls },
        { 68876, &AuraPeriodicDummyTickGenericWailingSouls },
    };

    static AurasRow<AuraPeriodicDummyTickMageSite> const periodicDummyTickMage[] =
    {
        { 55342, &AuraPeriodicDummyTickMage55342 },
    };

    static AurasRow<AuraPeriodicDummyTickDruidSite> const periodicDummyTickDruid[] =
    {
        { 22842, &AuraPeriodicDummyTickDruid22842 },
        { 33831, &AuraPeriodicDummyTickDruid33831 },
    };

    static AurasRow<AuraPeriodicDummyTickRogueSite> const periodicDummyTickRogue[] =
    {
        { 51690, &AuraPeriodicDummyTickRogue51690 },
    };

    static AurasRow<AuraPeriodicDummyTickHunterSite> const periodicDummyTickHunter[] =
    {
        { 53511, &AuraPeriodicDummyTickHunterFeedingFrenzy },
        { 53512, &AuraPeriodicDummyTickHunterFeedingFrenzy },
    };

    static AurasRow<AuraBoostGenericSite> const boostGeneric[] =
    {
        { 29865, &AuraBoostGeneric29865 },
        { 55053, &AuraBoostGeneric55053 },
        { 50720, &AuraBoostGeneric50720 },
        { 57350, &AuraBoostGeneric57350 },
        { 58914, &AuraBoostGeneric58914 },
        { 62692, &AuraBoostGeneric62692 },
        { 71905, &AuraBoostGeneric71905 },
    };

    static AurasRow<AuraBoostMageSite> const boostMage[] =
    {
        { 11129, &AuraBoostMage11129 },
        { 28682, &AuraBoostMage28682 },
        { 44401, &AuraBoostMagePushingTheLimit },
        { 48108, &AuraBoostMagePushingTheLimit },
        { 57761, &AuraBoostMagePushingTheLimit },
        { 74396, &AuraBoostMage74396 },
    };

    static AurasRow<AuraBoostPriestSite> const boostPriest[] =
    {
        { 552, &AuraBoostPriest552 },
        { 47585, &AuraBoostPriest47585 },
    };

    static AurasRow<AuraBoostHunterSite> const boostHunter[] =
    {
        { 34074, &AuraBoostHunter34074 },
        { 19574, &AuraBoostHunterBestialWrathImmunity },
        { 34471, &AuraBoostHunterBestialWrathImmunity },
        { 34027, &AuraBoostHunter34027 },
        { 35029, &AuraBoostHunter35029 },
        { 35030, &AuraBoostHunter35030 },
    };

    static AurasRow<AuraBoostDeathKnightSite> const boostDeathKnight[] =
    {
        { 49039, &AuraBoostDeathKnight49039 },
        { 48263, &AuraBoostDeathKnightPresence },
        { 48265, &AuraBoostDeathKnightPresence },
        { 48266, &AuraBoostDeathKnightPresence },
    };

    uint32 rows = RegisterAurasRows(registry, triggerSpellGeneric);
    rows += RegisterAurasRows(registry, triggerSpellMage);
    rows += RegisterAurasRows(registry, triggerSpellHunter);
    rows += RegisterAurasRows(registry, triggerSpellDruid);
    rows += RegisterAurasRows(registry, triggerSpellShaman);
    rows += RegisterAurasRows(registry, triggerSpellCustom);
    rows += RegisterAurasRows(registry, periodicDamageTick);
    rows += RegisterAurasRows(registry, periodicManaLeechTick);
    rows += RegisterAurasRows(registry, periodicDummyTickGeneric);
    rows += RegisterAurasRows(registry, periodicDummyTickMage);
    rows += RegisterAurasRows(registry, periodicDummyTickDruid);
    rows += RegisterAurasRows(registry, periodicDummyTickRogue);
    rows += RegisterAurasRows(registry, periodicDummyTickHunter);
    rows += RegisterAurasRows(registry, boostGeneric);
    rows += RegisterAurasRows(registry, boostMage);
    rows += RegisterAurasRows(registry, boostPriest);
    rows += RegisterAurasRows(registry, boostHunter);
    rows += RegisterAurasRows(registry, boostDeathKnight);
    registry.RegisterDefault<AuraTriggerSpellGenericSite>(&AuraTriggerSpellGenericDefault);
    registry.RegisterDefault<AuraTriggerSpellMageSite>(&AuraTriggerSpellMageDefault);
    registry.RegisterDefault<AuraTriggerSpellHunterSite>(&AuraTriggerSpellHunterDefault);
    registry.RegisterDefault<AuraTriggerSpellDruidSite>(&AuraTriggerSpellDruidDefault);
    registry.RegisterDefault<AuraTriggerSpellShamanSite>(&AuraTriggerSpellShamanDefault);
    registry.RegisterDefault<AuraPeriodicDamageTickSite>(&AuraPeriodicDamageTickDefault);
    registry.RegisterDefault<AuraPeriodicDummyTickGenericSite>(&AuraPeriodicDummyTickGenericDefault);
    registry.RegisterDefault<AuraPeriodicDummyTickMageSite>(&AuraPeriodicDummyTickMageDefault);
    registry.RegisterDefault<AuraPeriodicDummyTickDruidSite>(&AuraPeriodicDummyTickDruidDefault);
    registry.RegisterDefault<AuraPeriodicDummyTickRogueSite>(&AuraPeriodicDummyTickRogueDefault);
    registry.RegisterDefault<AuraPeriodicDummyTickHunterSite>(&AuraPeriodicDummyTickHunterDefault);
    registry.RegisterDefault<AuraBoostGenericSite>(&AuraBoostGenericDefault);
    registry.RegisterDefault<AuraBoostMageSite>(&AuraBoostMageDefault);
    registry.RegisterDefault<AuraBoostPriestSite>(&AuraBoostPriestDefault);
    registry.RegisterDefault<AuraBoostHunterSite>(&AuraBoostHunterDefault);
    return rows + 15;
}
