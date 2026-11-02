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

#include "spells/handlers/AuraDummyHandlers.h"
#include "spells/handlers/SpellHandlerRegistry.h"
#include "Object/Creature.h"
#include "data/world/ObjectMgr.h"
#include "entities/motion/MotionMaster.h"
#include "Maps/Map.h"
#include "spells/data/SpellMgr.h"
#include "spells/auras/SpellAuras.h"
#include "entities/player/Player.h"

/// SPELLFAMILY_GENERIC, AT APPLY 1515: Tame Beast
static SpellHandlerOutcome<void> AuraDummyApplyGeneric1515(AuraDummyApplyGenericContext& ctx)
{
    // FIX_ME: this is 2.0.12 threat effect replaced in 2.1.x by dummy aura, must be checked for correctness
    if (ctx.target->CanHaveThreatList())
        if (Unit* caster = ctx.aura->GetCaster())
        {
            ctx.target->AddThreat(caster, 10.0f, false, GetSpellSchoolMask(ctx.aura->GetSpellProto()), ctx.aura->GetSpellProto());
        }
    return SpellHandlerOutcome<void>::Return();
}

/// SPELLFAMILY_GENERIC, AT APPLY 7057: Haunting Spirits
static SpellHandlerOutcome<void> AuraDummyApplyGeneric7057(AuraDummyApplyGenericContext& ctx)
{
    // expected to tick with 30 sec period (tick part see in Aura::PeriodicTick)
    ctx.aura->SetPeriodic(true);
    ctx.aura->GetModifier()->periodictime = 30 * IN_MILLISECONDS;
    ctx.m_periodicTimer = ctx.aura->GetModifier()->periodictime;
    return SpellHandlerOutcome<void>::Return();
}

/// SPELLFAMILY_GENERIC, AT APPLY 10255: Stoned
static SpellHandlerOutcome<void> AuraDummyApplyGeneric10255(AuraDummyApplyGenericContext& ctx)
{
    if (Unit* caster = ctx.aura->GetCaster())
    {
        if (caster->GetTypeId() != TYPEID_UNIT)
        {
            return SpellHandlerOutcome<void>::Return();
        }

        caster->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
        caster->GetMotionMaster()->Inhibit(Motion::Inhibition::Rooted, Motion::ControlClaim(10255, 0, caster->GetObjectGuid().GetCounter()));
    }
    return SpellHandlerOutcome<void>::Return();
}

/// SPELLFAMILY_GENERIC, AT APPLY 13139: Net-o-Matic
static SpellHandlerOutcome<void> AuraDummyApplyGeneric13139(AuraDummyApplyGenericContext& ctx)
{
    // root to self part of (root_target->charge->root_self sequence
    if (Unit* caster = ctx.aura->GetCaster())
    {
        caster->CastSpell(caster, 13138, true, NULL, ctx.aura);
    }
    return SpellHandlerOutcome<void>::Return();
}

/// SPELLFAMILY_GENERIC, AT APPLY, one body for 4 labels: 28832: Mark of Korth'azz, 28833: Mark of Blaumeux,
/// 28834: Mark of Rivendare, 28835: Mark of Zeliek
static SpellHandlerOutcome<void> AuraDummyApplyGenericMarkOfTheHorsemen(AuraDummyApplyGenericContext& ctx)
{
    int32 damage = 0;

    switch (ctx.aura->GetStackAmount())
    {
        case 1:
            return SpellHandlerOutcome<void>::Return();
        case 2: damage =   500; break;
        case 3: damage =  1500; break;
        case 4: damage =  4000; break;
        case 5: damage = 12500; break;
        default:
            damage = 14000 + 1000 * ctx.aura->GetStackAmount();
            break;
    }

    if (Unit* caster = ctx.aura->GetCaster())
    {
        caster->CastCustomSpell(ctx.target, 28836, &damage, NULL, NULL, true, NULL, ctx.aura);
    }
    return SpellHandlerOutcome<void>::Return();
}

/// SPELLFAMILY_GENERIC, AT APPLY 31606: Stormcrow Amulet
static SpellHandlerOutcome<void> AuraDummyApplyGeneric31606(AuraDummyApplyGenericContext& ctx)
{
    CreatureInfo const* cInfo = ObjectMgr::GetCreatureTemplate(17970);

    // we must assume db or script set display id to native at ending flight (if not, target is stuck with this model)
    if (cInfo)
    {
        ctx.target->SetDisplayId(Creature::ChooseDisplayId(cInfo));
    }

    return SpellHandlerOutcome<void>::Return();
}

/// SPELLFAMILY_GENERIC, AT APPLY, one body for 3 labels: 32045: Soul Charge, 32051: Soul Charge, 32052: Soul Charge
static SpellHandlerOutcome<void> AuraDummyApplyGenericSoulCharge(AuraDummyApplyGenericContext& ctx)
{
    // max duration is 2 minutes, but expected to be random duration
    // real time randomness is unclear, using max 30 seconds here
    // see further down for expire of this aura
    ctx.aura->GetHolder()->SetAuraDuration(urand(1, 30)*IN_MILLISECONDS);
    return SpellHandlerOutcome<void>::Return();
}

/// SPELLFAMILY_GENERIC, AT APPLY 33326: Stolen Soul Dispel
static SpellHandlerOutcome<void> AuraDummyApplyGeneric33326(AuraDummyApplyGenericContext& ctx)
{
    ctx.target->RemoveAurasDueToSpell(32346);
    return SpellHandlerOutcome<void>::Return();
}

/// SPELLFAMILY_GENERIC, AT APPLY 36587: Vision Guide
static SpellHandlerOutcome<void> AuraDummyApplyGeneric36587(AuraDummyApplyGenericContext& ctx)
{
    ctx.target->CastSpell(ctx.target, 36573, true, NULL, ctx.aura);
    return SpellHandlerOutcome<void>::Return();
// Gender spells
}

/// SPELLFAMILY_GENERIC, AT APPLY, one body for 3 labels: 38224: Illidari Agent Illusion, 37096: Blood Elf Illusion,
/// 46354: Blood Elf Illusion
static SpellHandlerOutcome<void> AuraDummyApplyGenericGenderIllusion(AuraDummyApplyGenericContext& ctx)
{
    uint8 gender = ctx.target->getGender();
    uint32 spellId;
    switch (ctx.aura->GetId())
    {
        case 38224: spellId = (gender == GENDER_MALE ? 38225 : 38227); break;
        case 37096: spellId = (gender == GENDER_MALE ? 37093 : 37095); break;
        case 46354: spellId = (gender == GENDER_MALE ? 46355 : 46356); break;
        default: return SpellHandlerOutcome<void>::Return();
    }
    ctx.target->CastSpell(ctx.target, spellId, true, NULL, ctx.aura);
    return SpellHandlerOutcome<void>::Return();
}

/// SPELLFAMILY_GENERIC, AT APPLY 39850: Rocket Blast
static SpellHandlerOutcome<void> AuraDummyApplyGeneric39850(AuraDummyApplyGenericContext& ctx)
{
    if (roll_chance_i(20))              // backfire stun
    {
        ctx.target->CastSpell(ctx.target, 51581, true, NULL, ctx.aura);
    }
    return SpellHandlerOutcome<void>::Return();
}

/// SPELLFAMILY_GENERIC, AT APPLY 43873: Headless Horseman Laugh
static SpellHandlerOutcome<void> AuraDummyApplyGeneric43873(AuraDummyApplyGenericContext& ctx)
{
    ctx.target->PlayDistanceSound(11965);
    return SpellHandlerOutcome<void>::Return();
}

/// SPELLFAMILY_GENERIC, AT APPLY 45963: Call Alliance Deserter
static SpellHandlerOutcome<void> AuraDummyApplyGeneric45963(AuraDummyApplyGenericContext& ctx)
{
    // Escorting Alliance Deserter
    if (ctx.target->GetMiniPet())
    {
        ctx.target->CastSpell(ctx.target, 45957, true);
    }

    return SpellHandlerOutcome<void>::Return();
}

/// SPELLFAMILY_GENERIC, AT APPLY 46637: Break Ice
static SpellHandlerOutcome<void> AuraDummyApplyGeneric46637(AuraDummyApplyGenericContext& ctx)
{
    ctx.target->CastSpell(ctx.target, 46638, true, NULL, ctx.aura);
    return SpellHandlerOutcome<void>::Return();
}

/// SPELLFAMILY_GENERIC, AT APPLY 46699: Requires No Ammo
static SpellHandlerOutcome<void> AuraDummyApplyGeneric46699(AuraDummyApplyGenericContext& ctx)
{
    if (Player* targetPlayer = ctx.target->ToPlayer())
        // not use ammo and not allow use
        targetPlayer->RemoveAmmo();
    return SpellHandlerOutcome<void>::Return();
}

/// SPELLFAMILY_GENERIC, AT APPLY 47190: Toalu'u's Spiritual Incense
static SpellHandlerOutcome<void> AuraDummyApplyGeneric47190(AuraDummyApplyGenericContext& ctx)
{
    ctx.target->CastSpell(ctx.target, 47189, true, NULL, ctx.aura);
    // allow script to process further (text)
    return SpellHandlerOutcome<void>::Continue();
}

/// SPELLFAMILY_GENERIC, AT APPLY 47563: Freezing Cloud
static SpellHandlerOutcome<void> AuraDummyApplyGeneric47563(AuraDummyApplyGenericContext& ctx)
{
    ctx.target->CastSpell(ctx.target, 47574, true, NULL, ctx.aura);
    return SpellHandlerOutcome<void>::Return();
}

/// SPELLFAMILY_GENERIC, AT APPLY 47593: Freezing Cloud
static SpellHandlerOutcome<void> AuraDummyApplyGeneric47593(AuraDummyApplyGenericContext& ctx)
{
    ctx.target->CastSpell(ctx.target, 47594, true, NULL, ctx.aura);
    return SpellHandlerOutcome<void>::Return();
}

/// SPELLFAMILY_GENERIC, AT APPLY 48143: Forgotten Aura
static SpellHandlerOutcome<void> AuraDummyApplyGeneric48143(AuraDummyApplyGenericContext& ctx)
{
    // See Death's Door
    ctx.target->CastSpell(ctx.target, 48814, true, NULL, ctx.aura);
    return SpellHandlerOutcome<void>::Return();
}

/// SPELLFAMILY_GENERIC, AT APPLY 51405: Digging for Treasure
static SpellHandlerOutcome<void> AuraDummyApplyGeneric51405(AuraDummyApplyGenericContext& ctx)
{
    ctx.target->HandleEmote(EMOTE_STATE_WORK);
    // Pet will be following owner, this makes him stop
    ctx.target->GetMotionMaster()->Inhibit(Motion::Inhibition::Stunned, Motion::ControlClaim(51405, 0, ctx.aura->GetCasterGuid().GetCounter()));
    return SpellHandlerOutcome<void>::Return();
}

/// SPELLFAMILY_GENERIC, AT APPLY, one body for 2 labels: 61187: Twilight Shift, 61190: Twilight Shift
static SpellHandlerOutcome<void> AuraDummyApplyGenericTwilightShift(AuraDummyApplyGenericContext& ctx)
{
    ctx.target->RemoveAurasDueToSpell(57620);
    ctx.target->CastSpell(ctx.target, 61885, true, NULL, ctx.aura);
    return SpellHandlerOutcome<void>::Return();
}

/// SPELLFAMILY_GENERIC, AT APPLY 62061: Festive Holiday Mount
static SpellHandlerOutcome<void> AuraDummyApplyGeneric62061(AuraDummyApplyGenericContext& ctx)
{
    if (ctx.target->HasAuraType(SPELL_AURA_MOUNTED))
        // Reindeer Transformation
        ctx.target->CastSpell(ctx.target, 25860, true, NULL, ctx.aura);
    return SpellHandlerOutcome<void>::Return();
}

/// SPELLFAMILY_GENERIC, AT APPLY 62109: Tails Up: Aura
static SpellHandlerOutcome<void> AuraDummyApplyGeneric62109(AuraDummyApplyGenericContext& ctx)
{
    ctx.target->setFaction(1990);           // Ambient (hostile)
    ctx.target->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_OOC_NOT_ATTACKABLE);
    return SpellHandlerOutcome<void>::Return();
}

/// SPELLFAMILY_GENERIC, AT APPLY 63122: Clear Insane
static SpellHandlerOutcome<void> AuraDummyApplyGeneric63122(AuraDummyApplyGenericContext& ctx)
{
    ctx.target->RemoveAurasDueToSpell(ctx.aura->GetSpellProto()->CalculateSimpleValue(ctx.aura->GetEffIndex()));
    return SpellHandlerOutcome<void>::Return();
}

/// SPELLFAMILY_GENERIC, AT APPLY 63624: Learn a Second Talent Specialization
static SpellHandlerOutcome<void> AuraDummyApplyGeneric63624(AuraDummyApplyGenericContext& ctx)
{
    // Teach Learn Talent Specialization Switches, required for client triggered casts, allow after 30 sec delay
    if (Player* targetPlayer = ctx.target->ToPlayer())
    {
        targetPlayer->learnSpell(63680, false);
    }
    return SpellHandlerOutcome<void>::Return();
}

/// SPELLFAMILY_GENERIC, AT APPLY 63651: Revert to One Talent Specialization
static SpellHandlerOutcome<void> AuraDummyApplyGeneric63651(AuraDummyApplyGenericContext& ctx)
{
    // Teach Learn Talent Specialization Switches, remove
    if (Player* targetPlayer = ctx.target->ToPlayer())
    {
        targetPlayer->removeSpell(63680);
    }
    return SpellHandlerOutcome<void>::Return();
}

/// SPELLFAMILY_GENERIC, AT APPLY 64132: Constrictor Tentacle
static SpellHandlerOutcome<void> AuraDummyApplyGeneric64132(AuraDummyApplyGenericContext& ctx)
{
    if (ctx.target->GetTypeId() == TYPEID_PLAYER)
    {
        ctx.target->CastSpell(ctx.target, 64133, true, NULL, ctx.aura);
    }
    return SpellHandlerOutcome<void>::Return();
}

/// SPELLFAMILY_GENERIC, AT APPLY 65684: Dark Essence
static SpellHandlerOutcome<void> AuraDummyApplyGeneric65684(AuraDummyApplyGenericContext& ctx)
{
    ctx.target->RemoveAurasDueToSpell(65686);
    return SpellHandlerOutcome<void>::Return();
}

/// SPELLFAMILY_GENERIC, AT APPLY 65686: Light Essence
static SpellHandlerOutcome<void> AuraDummyApplyGeneric65686(AuraDummyApplyGenericContext& ctx)
{
    ctx.target->RemoveAurasDueToSpell(65684);
    return SpellHandlerOutcome<void>::Return();
}

/// SPELLFAMILY_GENERIC, AT APPLY 68912: Wailing Souls
static SpellHandlerOutcome<void> AuraDummyApplyGeneric68912(AuraDummyApplyGenericContext& ctx)
{
    if (Unit* caster = ctx.aura->GetCaster())
    {
        caster->SetTargetGuid(ctx.target->GetObjectGuid());

        // TODO - this is confusing, it seems the boss should channel this aura, and start casting the next spell
        caster->CastSpell(caster, 68899, false);
    }
    return SpellHandlerOutcome<void>::Return();
}

/// SPELLFAMILY_GENERIC, AT APPLY 70623: Jaina's Call
static SpellHandlerOutcome<void> AuraDummyApplyGeneric70623(AuraDummyApplyGenericContext& ctx)
{
    if (ctx.target->GetTypeId() == TYPEID_PLAYER)
    {
        ctx.target->CastSpell(ctx.target, 70525, true, NULL, ctx.aura);
    }
    return SpellHandlerOutcome<void>::Return();
}

/// SPELLFAMILY_GENERIC, AT APPLY 70638: Call of Sylvanas
static SpellHandlerOutcome<void> AuraDummyApplyGeneric70638(AuraDummyApplyGenericContext& ctx)
{
    if (ctx.target->GetTypeId() == TYPEID_PLAYER)
    {
        ctx.target->CastSpell(ctx.target, 70639, true, NULL, ctx.aura);
    }
    return SpellHandlerOutcome<void>::Return();
}

/// SPELLFAMILY_GENERIC, AT APPLY 71563: Deadly Precision
static SpellHandlerOutcome<void> AuraDummyApplyGeneric71563(AuraDummyApplyGenericContext& ctx)
{
    ctx.target->CastSpell(ctx.target, 71564, true, NULL, ctx.aura);
    return SpellHandlerOutcome<void>::Return();
}

/// SPELLFAMILY_WARRIOR 41099: Battle Stance
static SpellHandlerOutcome<void> AuraDummyApplyWarrior41099(AuraDummyApplyContext& ctx)
{
    Creature* targetCreature = ctx.target->ToCreature();
    if (!targetCreature)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // Stance Cooldown
    ctx.target->CastSpell(ctx.target, 41102, true, NULL, ctx.aura);

    // Battle Aura
    ctx.target->CastSpell(ctx.target, 41106, true, NULL, ctx.aura);

    // equipment
    targetCreature->SetVirtualItem(VIRTUAL_ITEM_SLOT_0, 32614);
    targetCreature->SetVirtualItem(VIRTUAL_ITEM_SLOT_1, 0);
    targetCreature->SetVirtualItem(VIRTUAL_ITEM_SLOT_2, 0);
    return SpellHandlerOutcome<void>::Return();
}

/// SPELLFAMILY_WARRIOR 41100: Berserker Stance
static SpellHandlerOutcome<void> AuraDummyApplyWarrior41100(AuraDummyApplyContext& ctx)
{
    Creature* targetCreature = ctx.target->ToCreature();
    if (!targetCreature)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // Stance Cooldown
    ctx.target->CastSpell(ctx.target, 41102, true, NULL, ctx.aura);

    // Berserker Aura
    ctx.target->CastSpell(ctx.target, 41107, true, NULL, ctx.aura);

    // equipment
    targetCreature->SetVirtualItem(VIRTUAL_ITEM_SLOT_0, 32614);
    targetCreature->SetVirtualItem(VIRTUAL_ITEM_SLOT_1, 0);
    targetCreature->SetVirtualItem(VIRTUAL_ITEM_SLOT_2, 0);
    return SpellHandlerOutcome<void>::Return();
}

/// SPELLFAMILY_WARRIOR 41101: Defensive Stance
static SpellHandlerOutcome<void> AuraDummyApplyWarrior41101(AuraDummyApplyContext& ctx)
{
    Creature* targetCreature = ctx.target->ToCreature();
    if (!targetCreature)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // Stance Cooldown
    ctx.target->CastSpell(ctx.target, 41102, true, NULL, ctx.aura);

    // Defensive Aura
    ctx.target->CastSpell(ctx.target, 41105, true, NULL, ctx.aura);

    // equipment
    targetCreature->SetVirtualItem(VIRTUAL_ITEM_SLOT_0, 32604);
    targetCreature->SetVirtualItem(VIRTUAL_ITEM_SLOT_1, 31467);
    targetCreature->SetVirtualItem(VIRTUAL_ITEM_SLOT_2, 0);
    return SpellHandlerOutcome<void>::Return();
}

/// SPELLFAMILY_WARRIOR 53790: Defensive Stance
static SpellHandlerOutcome<void> AuraDummyApplyWarrior53790(AuraDummyApplyContext& ctx)
{
    Creature* targetCreature = ctx.target->ToCreature();
    if (!targetCreature)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // Stance Cooldown
    ctx.target->CastSpell(ctx.target, 59526, true, NULL, ctx.aura);

    // Defensive Aura
    ctx.target->CastSpell(ctx.target, 41105, true, NULL, ctx.aura);

    // equipment
    targetCreature->SetVirtualItem(VIRTUAL_ITEM_SLOT_0, 43625);
    targetCreature->SetVirtualItem(VIRTUAL_ITEM_SLOT_1, 39384);
    targetCreature->SetVirtualItem(VIRTUAL_ITEM_SLOT_2, 0);
    return SpellHandlerOutcome<void>::Return();
}

/// SPELLFAMILY_WARRIOR 53791: Berserker Stance
static SpellHandlerOutcome<void> AuraDummyApplyWarrior53791(AuraDummyApplyContext& ctx)
{
    Creature* targetCreature = ctx.target->ToCreature();
    if (!targetCreature)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // Stance Cooldown
    ctx.target->CastSpell(ctx.target, 59526, true, NULL, ctx.aura);

    // Berserker Aura
    ctx.target->CastSpell(ctx.target, 41107, true, NULL, ctx.aura);

    // equipment
    targetCreature->SetVirtualItem(VIRTUAL_ITEM_SLOT_0, 43625);
    targetCreature->SetVirtualItem(VIRTUAL_ITEM_SLOT_1, 43625);
    targetCreature->SetVirtualItem(VIRTUAL_ITEM_SLOT_2, 0);
    return SpellHandlerOutcome<void>::Return();
}

/// SPELLFAMILY_WARRIOR 53792: Battle Stance
static SpellHandlerOutcome<void> AuraDummyApplyWarrior53792(AuraDummyApplyContext& ctx)
{
    Creature* targetCreature = ctx.target->ToCreature();
    if (!targetCreature)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // Stance Cooldown
    ctx.target->CastSpell(ctx.target, 59526, true, NULL, ctx.aura);

    // Battle Aura
    ctx.target->CastSpell(ctx.target, 41106, true, NULL, ctx.aura);

    // equipment
    targetCreature->SetVirtualItem(VIRTUAL_ITEM_SLOT_0, 43623);
    targetCreature->SetVirtualItem(VIRTUAL_ITEM_SLOT_1, 0);
    targetCreature->SetVirtualItem(VIRTUAL_ITEM_SLOT_2, 0);
    return SpellHandlerOutcome<void>::Return();
}

/// SPELLFAMILY_HUNTER, AT APPLY 34026: Kill Command
static SpellHandlerOutcome<void> AuraDummyApplyHunter34026(AuraDummyApplyContext& ctx)
{
    ctx.target->CastSpell(ctx.target, 34027, true, NULL, ctx.aura);
    return SpellHandlerOutcome<void>::Return();
}

/// SPELLFAMILY_SHAMAN, AT APPLY 55198: Tidal Force
static SpellHandlerOutcome<void> AuraDummyApplyShaman55198(AuraDummyApplyContext& ctx)
{
    ctx.target->CastSpell(ctx.target, 55166, true, NULL, ctx.aura);
    return SpellHandlerOutcome<void>::Return();
}

/// AT REMOVE, quest tame 19548: Tame Ice Claw Bear
static SpellHandlerOutcome<void> AuraDummyQuestTame19548(AuraDummyQuestTameContext& ctx)
{
    ctx.finalSpellId = 19597;
    return SpellHandlerOutcome<void>::Continue();
}

/// AT REMOVE, quest tame 19674: Tame Large Crag Boar
static SpellHandlerOutcome<void> AuraDummyQuestTame19674(AuraDummyQuestTameContext& ctx)
{
    ctx.finalSpellId = 19677;
    return SpellHandlerOutcome<void>::Continue();
}

/// AT REMOVE, quest tame 19687: Tame Snow Leopard
static SpellHandlerOutcome<void> AuraDummyQuestTame19687(AuraDummyQuestTameContext& ctx)
{
    ctx.finalSpellId = 19676;
    return SpellHandlerOutcome<void>::Continue();
}

/// AT REMOVE, quest tame 19688: Tame Adult Plainstrider
static SpellHandlerOutcome<void> AuraDummyQuestTame19688(AuraDummyQuestTameContext& ctx)
{
    ctx.finalSpellId = 19678;
    return SpellHandlerOutcome<void>::Continue();
}

/// AT REMOVE, quest tame 19689: Tame Prairie Stalker
static SpellHandlerOutcome<void> AuraDummyQuestTame19689(AuraDummyQuestTameContext& ctx)
{
    ctx.finalSpellId = 19679;
    return SpellHandlerOutcome<void>::Continue();
}

/// AT REMOVE, quest tame 19692: Tame Swoop
static SpellHandlerOutcome<void> AuraDummyQuestTame19692(AuraDummyQuestTameContext& ctx)
{
    ctx.finalSpellId = 19680;
    return SpellHandlerOutcome<void>::Continue();
}

/// AT REMOVE, quest tame 19693: Tame Webwood Lurker
static SpellHandlerOutcome<void> AuraDummyQuestTame19693(AuraDummyQuestTameContext& ctx)
{
    ctx.finalSpellId = 19684;
    return SpellHandlerOutcome<void>::Continue();
}

/// AT REMOVE, quest tame 19694: Tame Dire Mottled Boar
static SpellHandlerOutcome<void> AuraDummyQuestTame19694(AuraDummyQuestTameContext& ctx)
{
    ctx.finalSpellId = 19681;
    return SpellHandlerOutcome<void>::Continue();
}

/// AT REMOVE, quest tame 19696: Tame Surf Crawler
static SpellHandlerOutcome<void> AuraDummyQuestTame19696(AuraDummyQuestTameContext& ctx)
{
    ctx.finalSpellId = 19682;
    return SpellHandlerOutcome<void>::Continue();
}

/// AT REMOVE, quest tame 19697: Tame Armored Scorpid
static SpellHandlerOutcome<void> AuraDummyQuestTame19697(AuraDummyQuestTameContext& ctx)
{
    ctx.finalSpellId = 19683;
    return SpellHandlerOutcome<void>::Continue();
}

/// AT REMOVE, quest tame 19699: Tame Nightsaber Stalker
static SpellHandlerOutcome<void> AuraDummyQuestTame19699(AuraDummyQuestTameContext& ctx)
{
    ctx.finalSpellId = 19685;
    return SpellHandlerOutcome<void>::Continue();
}

/// AT REMOVE, quest tame 19700: Tame Strigid Screecher
static SpellHandlerOutcome<void> AuraDummyQuestTame19700(AuraDummyQuestTameContext& ctx)
{
    ctx.finalSpellId = 19686;
    return SpellHandlerOutcome<void>::Continue();
}

/// AT REMOVE, quest tame 30646: Tame Barbed Crawler
static SpellHandlerOutcome<void> AuraDummyQuestTame30646(AuraDummyQuestTameContext& ctx)
{
    ctx.finalSpellId = 30647;
    return SpellHandlerOutcome<void>::Continue();
}

/// AT REMOVE, quest tame 30653: Tame Greater Timberstrider
static SpellHandlerOutcome<void> AuraDummyQuestTame30653(AuraDummyQuestTameContext& ctx)
{
    ctx.finalSpellId = 30648;
    return SpellHandlerOutcome<void>::Continue();
}

/// AT REMOVE, quest tame 30654: Tame Nightstalker
static SpellHandlerOutcome<void> AuraDummyQuestTame30654(AuraDummyQuestTameContext& ctx)
{
    ctx.finalSpellId = 30652;
    return SpellHandlerOutcome<void>::Continue();
}

/// AT REMOVE, quest tame 30099: Tame Crazed Dragonhawk
static SpellHandlerOutcome<void> AuraDummyQuestTame30099(AuraDummyQuestTameContext& ctx)
{
    ctx.finalSpellId = 30100;
    return SpellHandlerOutcome<void>::Continue();
}

/// AT REMOVE, quest tame 30102: Tame Elder Springpaw
static SpellHandlerOutcome<void> AuraDummyQuestTame30102(AuraDummyQuestTameContext& ctx)
{
    ctx.finalSpellId = 30103;
    return SpellHandlerOutcome<void>::Continue();
}

/// AT REMOVE, quest tame 30105: Tame Mistbat
static SpellHandlerOutcome<void> AuraDummyQuestTame30105(AuraDummyQuestTameContext& ctx)
{
    ctx.finalSpellId = 30104;
    return SpellHandlerOutcome<void>::Continue();
}


/// AT REMOVE 10255: Stoned
static SpellHandlerOutcome<void> AuraDummyRemove10255(AuraDummyRemoveContext& ctx)
{
    if (Unit* caster = ctx.aura->GetCaster())
    {
        if (caster->GetTypeId() != TYPEID_UNIT)
        {
            return SpellHandlerOutcome<void>::Return();
        }

        // see dummy effect of spell 10254 for removal of flags etc
        caster->CastSpell(caster, 10254, true);
    }
    return SpellHandlerOutcome<void>::Return();
}

/// AT REMOVE 12479: Hex of Jammal'an
static SpellHandlerOutcome<void> AuraDummyRemove12479(AuraDummyRemoveContext& ctx)
{
    ctx.target->CastSpell(ctx.target, 12480, true, NULL, ctx.aura);
    return SpellHandlerOutcome<void>::Return();
}

/// AT REMOVE 12774: (DND) Belnistrasz Idol Shutdown Visual
static SpellHandlerOutcome<void> AuraDummyRemove12774(AuraDummyRemoveContext& ctx)
{
    if (ctx.aura->GetRemoveMode() == AURA_REMOVE_BY_DEATH)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // Idom Rool Camera Shake <- wtf, don't drink while making spellnames?
    if (Unit* caster = ctx.aura->GetCaster())
    {
        caster->CastSpell(caster, 12816, true);
    }

    return SpellHandlerOutcome<void>::Return();
}

/// AT REMOVE 28169: Mutating Injection
static SpellHandlerOutcome<void> AuraDummyRemove28169(AuraDummyRemoveContext& ctx)
{
    // Mutagen Explosion
    ctx.target->CastSpell(ctx.target, 28206, true, NULL, ctx.aura);
    // Poison Cloud
    ctx.target->CastSpell(ctx.target, 28240, true, NULL, ctx.aura);
    return SpellHandlerOutcome<void>::Return();
}

/// AT REMOVE 32045: Soul Charge
static SpellHandlerOutcome<void> AuraDummyRemove32045(AuraDummyRemoveContext& ctx)
{
    if (ctx.aura->GetRemoveMode() == AURA_REMOVE_BY_EXPIRE)
    {
        ctx.target->CastSpell(ctx.target, 32054, true, NULL, ctx.aura);
    }

    return SpellHandlerOutcome<void>::Return();
}

/// AT REMOVE 32051: Soul Charge
static SpellHandlerOutcome<void> AuraDummyRemove32051(AuraDummyRemoveContext& ctx)
{
    if (ctx.aura->GetRemoveMode() == AURA_REMOVE_BY_EXPIRE)
    {
        ctx.target->CastSpell(ctx.target, 32057, true, NULL, ctx.aura);
    }

    return SpellHandlerOutcome<void>::Return();
}

/// AT REMOVE 32052: Soul Charge
static SpellHandlerOutcome<void> AuraDummyRemove32052(AuraDummyRemoveContext& ctx)
{
    if (ctx.aura->GetRemoveMode() == AURA_REMOVE_BY_EXPIRE)
    {
        ctx.target->CastSpell(ctx.target, 32053, true, NULL, ctx.aura);
    }

    return SpellHandlerOutcome<void>::Return();
}

/// AT REMOVE 32286: Focus Target Visual
static SpellHandlerOutcome<void> AuraDummyRemove32286(AuraDummyRemoveContext& ctx)
{
    if (ctx.aura->GetRemoveMode() == AURA_REMOVE_BY_EXPIRE)
    {
        ctx.target->CastSpell(ctx.target, 32301, true, NULL, ctx.aura);
    }

    return SpellHandlerOutcome<void>::Return();
}

/// AT REMOVE, one body for 2 labels: 35079: Misdirection, triggered buff; 59628: Tricks of the Trade, triggered buff
static SpellHandlerOutcome<void> AuraDummyRemoveThreatRedirection(AuraDummyRemoveContext& ctx)
{
    if (Unit* pCaster = ctx.aura->GetCaster())
    {
        pCaster->GetHostileRefManager().ResetThreatRedirection();
    }
    return SpellHandlerOutcome<void>::Return();
}

/// AT REMOVE 36730: Flame Strike
static SpellHandlerOutcome<void> AuraDummyRemove36730(AuraDummyRemoveContext& ctx)
{
    ctx.target->CastSpell(ctx.target, 36731, true, NULL, ctx.aura);
    return SpellHandlerOutcome<void>::Return();
}

/// AT REMOVE 41099: Battle Stance
static SpellHandlerOutcome<void> AuraDummyRemove41099(AuraDummyRemoveContext& ctx)
{
    // Battle Aura
    ctx.target->RemoveAurasDueToSpell(41106);
    return SpellHandlerOutcome<void>::Return();
}

/// AT REMOVE 41100: Berserker Stance
static SpellHandlerOutcome<void> AuraDummyRemove41100(AuraDummyRemoveContext& ctx)
{
    // Berserker Aura
    ctx.target->RemoveAurasDueToSpell(41107);
    return SpellHandlerOutcome<void>::Return();
}

/// AT REMOVE 41101: Defensive Stance
static SpellHandlerOutcome<void> AuraDummyRemove41101(AuraDummyRemoveContext& ctx)
{
    // Defensive Aura
    ctx.target->RemoveAurasDueToSpell(41105);
    return SpellHandlerOutcome<void>::Return();
}

/// AT REMOVE 42454: Captured Totem
static SpellHandlerOutcome<void> AuraDummyRemove42454(AuraDummyRemoveContext& ctx)
{
    if (ctx.aura->GetRemoveMode() == AURA_REMOVE_BY_DEFAULT)
    {
        if (ctx.target->GetDeathState() != CORPSE)
        {
            return SpellHandlerOutcome<void>::Return();
        }

        Unit* pCaster = ctx.aura->GetCaster();

        if (!pCaster)
        {
            return SpellHandlerOutcome<void>::Return();
        }

        // Captured Totem Test Credit
        if (Player* pPlayer = pCaster->GetCharmerOrOwnerPlayerOrPlayerItself())
        {
            pPlayer->CastSpell(pPlayer, 42455, true);
        }
    }

    return SpellHandlerOutcome<void>::Return();
}

/// AT REMOVE 42517: Beam to Zelfrax
static SpellHandlerOutcome<void> AuraDummyRemove42517(AuraDummyRemoveContext& ctx)
{
    // expecting target to be a dummy creature
    Creature* pSummon = ctx.target->SummonCreature(23864, 0.0f, 0.0f, 0.0f, ctx.target->Where().Facing(), TEMPSPAWN_DEAD_DESPAWN, 0);

    Unit* pCaster = ctx.aura->GetCaster();

    if (pSummon && pCaster)
    {
        pSummon->GetMotionMaster()->MovePoint(0, pCaster->Where().X(), pCaster->Where().Y(), pCaster->Where().Z());
    }

    return SpellHandlerOutcome<void>::Return();
}

/// AT REMOVE 43969: Feathered Charm
static SpellHandlerOutcome<void> AuraDummyRemove43969(AuraDummyRemoveContext& ctx)
{
    // Steelfeather Quest Credit, Are there any requirements for this, like area?
    if (ctx.aura->GetRemoveMode() == AURA_REMOVE_BY_EXPIRE)
    {
        ctx.target->CastSpell(ctx.target, 43984, true);
    }

    return SpellHandlerOutcome<void>::Return();
}

/// AT REMOVE 44191: Flame Strike
static SpellHandlerOutcome<void> AuraDummyRemove44191(AuraDummyRemoveContext& ctx)
{
    if (ctx.target->GetMap()->IsDungeon())
    {
        uint32 spellId = ctx.target->GetMap()->IsRegularDifficulty() ? 44190 : 46163;

        ctx.target->CastSpell(ctx.target, spellId, true, NULL, ctx.aura);
    }
    return SpellHandlerOutcome<void>::Return();
}

/// AT REMOVE 45934: Dark Fiend
static SpellHandlerOutcome<void> AuraDummyRemove45934(AuraDummyRemoveContext& ctx)
{
    // Kill target if dispelled
    if (ctx.aura->GetRemoveMode() == AURA_REMOVE_BY_DISPEL)
    {
        ctx.target->DealDamage(ctx.target, ctx.target->GetHealth(), NULL, DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NORMAL, NULL, false);
    }
    return SpellHandlerOutcome<void>::Return();
}

/// AT REMOVE 45963: Call Alliance Deserter
static SpellHandlerOutcome<void> AuraDummyRemove45963(AuraDummyRemoveContext& ctx)
{
    // Escorting Alliance Deserter
    ctx.target->RemoveAurasDueToSpell(45957);
    return SpellHandlerOutcome<void>::Return();
}

/// AT REMOVE 46308: Burning Winds
static SpellHandlerOutcome<void> AuraDummyRemove46308(AuraDummyRemoveContext& ctx)
{
    // casted only at creatures at spawn
    ctx.target->CastSpell(ctx.target, 47287, true, NULL, ctx.aura);
    return SpellHandlerOutcome<void>::Return();
}

/// AT REMOVE 46637: Break Ice
static SpellHandlerOutcome<void> AuraDummyRemove46637(AuraDummyRemoveContext& ctx)
{
    ctx.target->CastSpell(ctx.target, 47030, true, NULL, ctx.aura);
    return SpellHandlerOutcome<void>::Return();
}

/// AT REMOVE 48385: Create Spirit Fount Beam
static SpellHandlerOutcome<void> AuraDummyRemove48385(AuraDummyRemoveContext& ctx)
{
    ctx.target->CastSpell(ctx.target, ctx.target->GetMap()->IsRegularDifficulty() ? 48380 : 59320, true);
    return SpellHandlerOutcome<void>::Return();
}

/// AT REMOVE 50141: Blood Oath
static SpellHandlerOutcome<void> AuraDummyRemove50141(AuraDummyRemoveContext& ctx)
{
    // Blood Oath
    if (ctx.aura->GetRemoveMode() == AURA_REMOVE_BY_EXPIRE)
    {
        ctx.target->CastSpell(ctx.target, 50001, true, NULL, ctx.aura);
    }

    return SpellHandlerOutcome<void>::Return();
}

/// AT REMOVE 51405: Digging for Treasure
static SpellHandlerOutcome<void> AuraDummyRemove51405(AuraDummyRemoveContext& ctx)
{
    const uint32 spell_list[7] =
    {
        51441,                                  // hare
        51397,                                  // crystal
        51398,                                  // armor
        51400,                                  // gem
        51401,                                  // platter
        51402,                                  // treasure
        51443                                   // bug
    };

    ctx.target->CastSpell(ctx.target, spell_list[urand(0, 6)], true);

    ctx.target->HandleEmote(EMOTE_STATE_NONE);
    ctx.target->GetMotionMaster()->Uninhibit(Motion::Inhibition::Stunned, Motion::ControlClaim(51405, 0, ctx.aura->GetCasterGuid().GetCounter()));
    return SpellHandlerOutcome<void>::Return();
}

/// AT REMOVE 51870: Collect Hair Sample
static SpellHandlerOutcome<void> AuraDummyRemove51870(AuraDummyRemoveContext& ctx)
{
    if (Unit* pCaster = ctx.aura->GetCaster())
    {
        if (ctx.aura->GetRemoveMode() == AURA_REMOVE_BY_EXPIRE)
        {
            pCaster->CastSpell(ctx.target, 51872, true, NULL, ctx.aura);
        }
    }

    return SpellHandlerOutcome<void>::Return();
}

/// AT REMOVE 52098: Charge Up
static SpellHandlerOutcome<void> AuraDummyRemove52098(AuraDummyRemoveContext& ctx)
{
    if (ctx.aura->GetRemoveMode() == AURA_REMOVE_BY_EXPIRE)
    {
        ctx.target->CastSpell(ctx.target, 52092, true, NULL, ctx.aura);
    }

    return SpellHandlerOutcome<void>::Return();
}

/// AT REMOVE 53039: Deploy Parachute
static SpellHandlerOutcome<void> AuraDummyRemove53039(AuraDummyRemoveContext& ctx)
{
    // Crusader Parachute
    ctx.target->RemoveAurasDueToSpell(53031);
    return SpellHandlerOutcome<void>::Return();
}

/// AT REMOVE 53790: Defensive Stance
static SpellHandlerOutcome<void> AuraDummyRemove53790(AuraDummyRemoveContext& ctx)
{
    // Defensive Aura
    ctx.target->RemoveAurasDueToSpell(41105);
    return SpellHandlerOutcome<void>::Return();
}

/// AT REMOVE 53791: Berserker Stance
static SpellHandlerOutcome<void> AuraDummyRemove53791(AuraDummyRemoveContext& ctx)
{
    // Berserker Aura
    ctx.target->RemoveAurasDueToSpell(41107);
    return SpellHandlerOutcome<void>::Return();
}

/// AT REMOVE 53792: Battle Stance
static SpellHandlerOutcome<void> AuraDummyRemove53792(AuraDummyRemoveContext& ctx)
{
    // Battle Aura
    ctx.target->RemoveAurasDueToSpell(41106);
    return SpellHandlerOutcome<void>::Return();
}

/// AT REMOVE 56511: Towers of Certain Doom: Tower Bunny Smoke Flare Effect
static SpellHandlerOutcome<void> AuraDummyRemove56511(AuraDummyRemoveContext& ctx)
{
    // Towers of Certain Doom: Skorn Cannonfire
    if (ctx.aura->GetRemoveMode() == AURA_REMOVE_BY_DEFAULT)
    {
        ctx.target->CastSpell(ctx.target, 43069, true);
    }

    return SpellHandlerOutcome<void>::Return();
}

/// AT REMOVE 61900: Electrical Charge
static SpellHandlerOutcome<void> AuraDummyRemove61900(AuraDummyRemoveContext& ctx)
{
    if (ctx.aura->GetRemoveMode() == AURA_REMOVE_BY_DEATH)
    {
        ctx.target->CastSpell(ctx.target, ctx.aura->GetSpellProto()->CalculateSimpleValue(EFFECT_INDEX_0), true);
    }

    return SpellHandlerOutcome<void>::Return();
}

/// AT REMOVE 68839: Corrupt Soul
static SpellHandlerOutcome<void> AuraDummyRemove68839(AuraDummyRemoveContext& ctx)
{
    // Knockdown Stun
    ctx.target->CastSpell(ctx.target, 68848, true, NULL, ctx.aura);
    // Draw Corrupted Soul
    ctx.target->CastSpell(ctx.target, 68846, true, NULL, ctx.aura);
    return SpellHandlerOutcome<void>::Return();
}

/// SPELLFAMILY_GENERIC, AT APPLY & REMOVE 11196: Recently Bandaged
static SpellHandlerOutcome<void> AuraDummyApplyRemoveGeneric11196(AuraDummyApplyRemoveContext& ctx)
{
    ctx.target->ApplySpellImmune(ctx.aura->GetId(), IMMUNITY_MECHANIC, ctx.aura->GetMiscValue(), ctx.apply);
    return SpellHandlerOutcome<void>::Return();
}

/// SPELLFAMILY_GENERIC, AT APPLY & REMOVE 24658: Unstable Power
static SpellHandlerOutcome<void> AuraDummyApplyRemoveGeneric24658(AuraDummyApplyRemoveContext& ctx)
{
    if (ctx.apply)
    {
        Unit* caster = ctx.aura->GetCaster();
        if (!caster)
        {
            return SpellHandlerOutcome<void>::Return();
        }

        caster->CastSpell(ctx.target, 24659, true, NULL, NULL, ctx.aura->GetCasterGuid());
    }
    else
    {
        ctx.target->RemoveAurasDueToSpell(24659);
    }
    return SpellHandlerOutcome<void>::Return();
}

/// SPELLFAMILY_GENERIC, AT APPLY & REMOVE 24661: Restless Strength
static SpellHandlerOutcome<void> AuraDummyApplyRemoveGeneric24661(AuraDummyApplyRemoveContext& ctx)
{
    if (ctx.apply)
    {
        Unit* caster = ctx.aura->GetCaster();
        if (!caster)
        {
            return SpellHandlerOutcome<void>::Return();
        }

        caster->CastSpell(ctx.target, 24662, true, NULL, NULL, ctx.aura->GetCasterGuid());
    }
    else
    {
        ctx.target->RemoveAurasDueToSpell(24662);
    }
    return SpellHandlerOutcome<void>::Return();
}

/// SPELLFAMILY_GENERIC, AT APPLY & REMOVE, one body for 16 labels: 29266: Permanent Feign Death,
/// 31261: Permanent Feign Death (Root), 37493: Feign Death, 52593: Bloated Abomination Feign Death,
/// 55795: Falling Dragon Feign Death, 57626: Feign Death, 57685: Permanent Feign Death,
/// 58768: Permanent Feign Death (Freeze Jumpend), 58806: Permanent Feign Death (Drowned Anim),
/// 58951: Permanent Feign Death, 64461: Permanent Feign Death (No Anim) (Root),
/// 65985: Permanent Feign Death (Root Silence Pacify), 70592: Permanent Feign Death,
/// 70628: Permanent Feign Death, 70630: Frozen Aftermath - Feign Death, 71598: Feign Death
static SpellHandlerOutcome<void> AuraDummyApplyRemoveGenericFeignDeath(AuraDummyApplyRemoveContext& ctx)
{
    // Unclear what the difference really is between them.
    // Some has effect1 that makes the difference, however not all.
    // Some appear to be used depending on creature location, in water, at solid ground, in air/suspended, etc
    // For now, just handle all the same way
    if (ctx.target->GetTypeId() == TYPEID_UNIT)
    {
        // The aura's own identity: two of these on one creature are two feign sources.
        ctx.target->SetFeignDeath(ctx.apply, ctx.aura->GetCasterGuid(), ctx.aura->GetId());
    }

    return SpellHandlerOutcome<void>::Return();
}

/// SPELLFAMILY_GENERIC, AT APPLY & REMOVE 40133: Summon Fire Elemental
static SpellHandlerOutcome<void> AuraDummyApplyRemoveGeneric40133(AuraDummyApplyRemoveContext& ctx)
{
    Unit* caster = ctx.aura->GetCaster();
    if (!caster)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    Unit* owner = caster->GetOwner();
    if (owner && owner->GetTypeId() == TYPEID_PLAYER)
    {
        if (ctx.apply)
        {
            owner->CastSpell(owner, 8985, true);
        }
        else
        {
            owner->RemovePet(PET_SAVE_REAGENTS);
        }
    }
    return SpellHandlerOutcome<void>::Return();
}

/// SPELLFAMILY_GENERIC, AT APPLY & REMOVE 40132: Summon Earth Elemental
static SpellHandlerOutcome<void> AuraDummyApplyRemoveGeneric40132(AuraDummyApplyRemoveContext& ctx)
{
    Unit* caster = ctx.aura->GetCaster();
    if (!caster)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    Unit* owner = caster->GetOwner();
    if (owner && owner->GetTypeId() == TYPEID_PLAYER)
    {
        if (ctx.apply)
        {
            owner->CastSpell(owner, 19704, true);
        }
        else
        {
            owner->RemovePet(PET_SAVE_REAGENTS);
        }
    }
    return SpellHandlerOutcome<void>::Return();
}

/// SPELLFAMILY_GENERIC, AT APPLY & REMOVE 40214: Dragonmaw Illusion
static SpellHandlerOutcome<void> AuraDummyApplyRemoveGeneric40214(AuraDummyApplyRemoveContext& ctx)
{
    if (ctx.apply)
    {
        ctx.target->CastSpell(ctx.target, 40216, true);
        ctx.target->CastSpell(ctx.target, 42016, true);
    }
    else
    {
        ctx.target->RemoveAurasDueToSpell(40216);
        ctx.target->RemoveAurasDueToSpell(42016);
    }
    return SpellHandlerOutcome<void>::Return();
}

/// SPELLFAMILY_GENERIC, AT APPLY & REMOVE 42515: Jarl Beam
static SpellHandlerOutcome<void> AuraDummyApplyRemoveGeneric42515(AuraDummyApplyRemoveContext& ctx)
{
    // aura animate dead (fainted) state for the duration, but we need to animate the death itself (correct way below?)
    if (Unit* pCaster = ctx.aura->GetCaster())
    {
        pCaster->ApplyModFlag(UNIT_FIELD_FLAGS_2, UNIT_FLAG2_FEIGN_DEATH, ctx.apply);
    }

    // Beam to Zelfrax at remove
    if (!ctx.apply)
    {
        ctx.target->CastSpell(ctx.target, 42517, true);
    }
    return SpellHandlerOutcome<void>::Return();
}

/// SPELLFAMILY_GENERIC, AT APPLY & REMOVE, one body for 2 labels: 42583: Claw Rage, 68987: Pursuit
static SpellHandlerOutcome<void> AuraDummyApplyRemoveGenericFixateTarget(AuraDummyApplyRemoveContext& ctx)
{
    Unit* caster = ctx.aura->GetCaster();
    if (!caster || ctx.target->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    if (ctx.apply)
    {
        caster->FixateTarget(ctx.target);
    }
    else if (ctx.target->GetObjectGuid() == caster->GetFixateTargetGuid())
    {
        caster->FixateTarget(NULL);
    }

    return SpellHandlerOutcome<void>::Return();
}

/// SPELLFAMILY_GENERIC, AT APPLY & REMOVE 47178: Plague Effect Self
static SpellHandlerOutcome<void> AuraDummyApplyRemoveGeneric47178(AuraDummyApplyRemoveContext& ctx)
{
    ctx.target->SetFeared(ctx.apply, ctx.aura->GetCasterGuid(), ctx.aura->GetId(), 0, uint8(ctx.aura->GetEffIndex()));
    return SpellHandlerOutcome<void>::Return();
}

/// SPELLFAMILY_GENERIC, AT APPLY & REMOVE, one body for 2 labels: 56422: Nerubian Submerge, 70733: Stoneform
static SpellHandlerOutcome<void> AuraDummyApplyRemoveGenericNotSelectable(AuraDummyApplyRemoveContext& ctx)
{
    // not known if there are other things todo, only flag are confirmed valid
    ctx.target->ApplyModFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE, ctx.apply);
    return SpellHandlerOutcome<void>::Return();
}

/// SPELLFAMILY_GENERIC, AT APPLY & REMOVE 58204: LK Intro VO
static SpellHandlerOutcome<void> AuraDummyApplyRemoveGeneric58204(AuraDummyApplyRemoveContext& ctx)
{
    if (Player* targetPlayer = ctx.target->ToPlayer())
    {
        // Play part 1
        if (ctx.apply)
        {
            ctx.target->PlayDirectSound(14970, targetPlayer);
        }
        // continue in 58205
        else
        {
            ctx.target->CastSpell(ctx.target, 58205, true);
        }
    }
    return SpellHandlerOutcome<void>::Return();
}

/// SPELLFAMILY_GENERIC, AT APPLY & REMOVE 58205: LK Intro VO
static SpellHandlerOutcome<void> AuraDummyApplyRemoveGeneric58205(AuraDummyApplyRemoveContext& ctx)
{
    if (Player* targetPlayer = ctx.target->ToPlayer())
    {
        // Play part 2
        if (ctx.apply)
        {
            ctx.target->PlayDirectSound(14971, targetPlayer);
        }
        // Play part 3
        else
        {
            ctx.target->PlayDirectSound(14972, targetPlayer);
        }
    }
    return SpellHandlerOutcome<void>::Return();
}

/// SPELLFAMILY_GENERIC, AT APPLY & REMOVE, one body for 2 labels: 27978: Shroud of Death,
/// 40131: Copy of Shroud of Death
static SpellHandlerOutcome<void> AuraDummyApplyRemoveGenericShroudOfDeath(AuraDummyApplyRemoveContext& ctx)
{
    if (ctx.apply)
    {
        ctx.target->m_AuraFlags |= UNIT_AURAFLAG_ALIVE_INVISIBLE;
    }
    else
    {
        ctx.target->m_AuraFlags &= ~UNIT_AURAFLAG_ALIVE_INVISIBLE;
    }
    return SpellHandlerOutcome<void>::Return();
}

/// SPELLFAMILY_GENERIC, AT APPLY & REMOVE, one body for 2 labels: 66936: Submerge, 66948: Submerge
static SpellHandlerOutcome<void> AuraDummyApplyRemoveGenericSubmerge(AuraDummyApplyRemoveContext& ctx)
{
    if (ctx.apply)
    {
        ctx.target->CastSpell(ctx.target, 66969, true);
    }
    else
    {
        ctx.target->RemoveAurasDueToSpell(66969);
    }
    return SpellHandlerOutcome<void>::Return();
}

/// SPELLFAMILY_GENERIC, AT APPLY & REMOVE 6606: Self Visual - Sleep Until Cancelled (DND)
static SpellHandlerOutcome<void> AuraDummyApplyRemoveGeneric6606(AuraDummyApplyRemoveContext& ctx)
{
    if (ctx.apply)
    {
        ctx.target->SetStandState(UNIT_STAND_STATE_SLEEP);
        ctx.target->GetMotionMaster()->Inhibit(Motion::Inhibition::Rooted, Motion::ControlClaim(6606, 0, ctx.aura->GetCasterGuid().GetCounter()));
    }
    else
    {
        ctx.target->GetMotionMaster()->Uninhibit(Motion::Inhibition::Rooted, Motion::ControlClaim(6606, 0, ctx.aura->GetCasterGuid().GetCounter()));
        ctx.target->SetStandState(UNIT_STAND_STATE_STAND);
    }

    return SpellHandlerOutcome<void>::Return();
}

/// SPELLFAMILY_GENERIC, AT APPLY & REMOVE, one body for 4 labels: 35356: Spawn Feign Death, 35357: Spawn Feign Death,
/// 42557: Feign Death, 51329: Feign Death
static SpellHandlerOutcome<void> AuraDummyApplyRemoveGenericSpawnFeignDeath(AuraDummyApplyRemoveContext& ctx)
{
    if (ctx.target->GetTypeId() == TYPEID_UNIT)
    {
        // Flags not set like it's done in SetFeignDeath()
        // UNIT_DYNFLAG_DEAD does not appear with these spells.
        // All of the spells appear to be present at spawn and not used to feign in combat or similar.
        if (ctx.apply)
        {
            ctx.target->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_UNK_29);
            ctx.target->SetFlag(UNIT_FIELD_FLAGS_2, UNIT_FLAG2_FEIGN_DEATH);

            ctx.target->GetMotionMaster()->Inhibit(Motion::Inhibition::Dead, Motion::ControlClaim(ctx.aura->GetId(), 0, ctx.aura->GetCasterGuid().GetCounter()));
        }
        else
        {
            ctx.target->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_UNK_29);
            ctx.target->RemoveFlag(UNIT_FIELD_FLAGS_2, UNIT_FLAG2_FEIGN_DEATH);

            ctx.target->GetMotionMaster()->Uninhibit(Motion::Inhibition::Dead, Motion::ControlClaim(ctx.aura->GetId(), 0, ctx.aura->GetCasterGuid().GetCounter()));
        }
    }
    return SpellHandlerOutcome<void>::Return();
}

/// SPELLFAMILY_GENERIC, AT APPLY & REMOVE 43874: Scourge Mur'gul Camp: Force Shield Arcane Purple x3
static SpellHandlerOutcome<void> AuraDummyApplyRemoveGeneric43874(AuraDummyApplyRemoveContext& ctx)
{
    ctx.target->ApplyModFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_OOC_NOT_ATTACKABLE, ctx.apply);
    if (ctx.apply)
    {
        ctx.target->GetMotionMaster()->Inhibit(Motion::Inhibition::Rooted, Motion::ControlClaim(43874, 0, ctx.aura->GetCasterGuid().GetCounter()));
    }
    else
    {
        ctx.target->GetMotionMaster()->Uninhibit(Motion::Inhibition::Rooted, Motion::ControlClaim(43874, 0, ctx.aura->GetCasterGuid().GetCounter()));
    }
    return SpellHandlerOutcome<void>::Return();
}

/// SPELLFAMILY_DRUID 52610: Savage Roar
static SpellHandlerOutcome<void> AuraDummyDruid52610(AuraDummyApplyRemoveContext& ctx)
{
    if (ctx.apply)
    {
        if (ctx.target->GetShapeshiftForm() != FORM_CAT)
        {
            return SpellHandlerOutcome<void>::Return();
        }

        ctx.target->CastSpell(ctx.target, 62071, true);
    }
    else
    {
        ctx.target->RemoveAurasDueToSpell(62071);
    }
    return SpellHandlerOutcome<void>::Return();
}

/// SPELLFAMILY_DRUID 61336: Survival Instincts
static SpellHandlerOutcome<void> AuraDummyDruid61336(AuraDummyApplyRemoveContext& ctx)
{
    if (ctx.apply)
    {
        if (!ctx.target->IsInFeralForm())
        {
            return SpellHandlerOutcome<void>::Return();
        }

        int32 bp0 = int32(ctx.target->GetMaxHealth() * ctx.aura->GetModifier()->m_amount / 100);
        ctx.target->CastCustomSpell(ctx.target, 50322, &bp0, NULL, NULL, true);
    }
    else
    {
        ctx.target->RemoveAurasDueToSpell(50322);
    }
    return SpellHandlerOutcome<void>::Return();
}

/// SPELLFAMILY_ROGUE 57934: Tricks of the Trade
static SpellHandlerOutcome<void> AuraDummyRogue57934(AuraDummyApplyRemoveContext& ctx)
{
    if (ctx.apply)
    {
        ctx.aura->GetHolder()->SetAuraCharges(1);     // not have proper charges set in spell data
    }
    else
    {
        // used for direct in code aura removes and spell proc event charges expire
        if (ctx.aura->GetRemoveMode() != AURA_REMOVE_BY_DEFAULT)
        {
            ctx.target->GetHostileRefManager().ResetThreatRedirection();
        }
    }
    return SpellHandlerOutcome<void>::Return();
}

/// SPELLFAMILY_HUNTER 34477: Misdirection
static SpellHandlerOutcome<void> AuraDummyHunter34477(AuraDummyApplyRemoveContext& ctx)
{
    if (ctx.apply)
    {
        ctx.aura->GetHolder()->SetAuraCharges(1);     // not have proper charges set in spell data
    }
    else
    {
        // used for direct in code aura removes and spell proc event charges expire
        if (ctx.aura->GetRemoveMode() != AURA_REMOVE_BY_DEFAULT)
        {
            ctx.target->GetHostileRefManager().ResetThreatRedirection();
        }
    }
    return SpellHandlerOutcome<void>::Return();
}

/// SPELLFAMILY_PALADIN 20911: Blessing of Sanctuary
static SpellHandlerOutcome<void> AuraDummyPaladin20911(AuraDummyApplyRemoveContext& ctx)
{
    if (ctx.apply)
    {
        ctx.target->CastSpell(ctx.target, 67480, true, NULL, ctx.aura);
    }
    else
    {
        ctx.target->RemoveAurasDueToSpell(67480);
    }
    return SpellHandlerOutcome<void>::Return();
}

template <class Site>
struct AuraDummyRow
{
    uint32 spellId;
    typename SpellHandler<Site>::Function function;
};

/// Registers every row of one site's table on `registry`; answers the number of rows.
template <class Site, std::size_t N>
static uint32 RegisterAuraDummyRows(SpellHandlerRegistry& registry, AuraDummyRow<Site> const (&rows)[N])
{
    for (AuraDummyRow<Site> const& row : rows)
    {
        registry.Register<Site>(row.spellId, row.function);
    }
    return uint32(N);
}

uint32 RegisterAuraDummyHandlers(SpellHandlerRegistry& registry)
{
    static AuraDummyRow<AuraDummyApplyGenericSite> const genericApply[] =
    {
        { 1515, &AuraDummyApplyGeneric1515 },
        { 7057, &AuraDummyApplyGeneric7057 },
        { 10255, &AuraDummyApplyGeneric10255 },
        { 13139, &AuraDummyApplyGeneric13139 },
        { 28832, &AuraDummyApplyGenericMarkOfTheHorsemen },
        { 28833, &AuraDummyApplyGenericMarkOfTheHorsemen },
        { 28834, &AuraDummyApplyGenericMarkOfTheHorsemen },
        { 28835, &AuraDummyApplyGenericMarkOfTheHorsemen },
        { 31606, &AuraDummyApplyGeneric31606 },
        { 32045, &AuraDummyApplyGenericSoulCharge },
        { 32051, &AuraDummyApplyGenericSoulCharge },
        { 32052, &AuraDummyApplyGenericSoulCharge },
        { 33326, &AuraDummyApplyGeneric33326 },
        { 36587, &AuraDummyApplyGeneric36587 },
        { 38224, &AuraDummyApplyGenericGenderIllusion },
        { 37096, &AuraDummyApplyGenericGenderIllusion },
        { 46354, &AuraDummyApplyGenericGenderIllusion },
        { 39850, &AuraDummyApplyGeneric39850 },
        { 43873, &AuraDummyApplyGeneric43873 },
        { 45963, &AuraDummyApplyGeneric45963 },
        { 46637, &AuraDummyApplyGeneric46637 },
        { 46699, &AuraDummyApplyGeneric46699 },
        { 47190, &AuraDummyApplyGeneric47190 },
        { 47563, &AuraDummyApplyGeneric47563 },
        { 47593, &AuraDummyApplyGeneric47593 },
        { 48143, &AuraDummyApplyGeneric48143 },
        { 51405, &AuraDummyApplyGeneric51405 },
        { 61187, &AuraDummyApplyGenericTwilightShift },
        { 61190, &AuraDummyApplyGenericTwilightShift },
        { 62061, &AuraDummyApplyGeneric62061 },
        { 62109, &AuraDummyApplyGeneric62109 },
        { 63122, &AuraDummyApplyGeneric63122 },
        { 63624, &AuraDummyApplyGeneric63624 },
        { 63651, &AuraDummyApplyGeneric63651 },
        { 64132, &AuraDummyApplyGeneric64132 },
        { 65684, &AuraDummyApplyGeneric65684 },
        { 65686, &AuraDummyApplyGeneric65686 },
        { 68912, &AuraDummyApplyGeneric68912 },
        { 70623, &AuraDummyApplyGeneric70623 },
        { 70638, &AuraDummyApplyGeneric70638 },
        { 71563, &AuraDummyApplyGeneric71563 },
    };

    static AuraDummyRow<AuraDummyApplyWarriorSite> const warriorApply[] =
    {
        { 41099, &AuraDummyApplyWarrior41099 },
        { 41100, &AuraDummyApplyWarrior41100 },
        { 41101, &AuraDummyApplyWarrior41101 },
        { 53790, &AuraDummyApplyWarrior53790 },
        { 53791, &AuraDummyApplyWarrior53791 },
        { 53792, &AuraDummyApplyWarrior53792 },
    };

    static AuraDummyRow<AuraDummyApplyHunterSite> const hunterApply[] =
    {
        { 34026, &AuraDummyApplyHunter34026 },
    };

    static AuraDummyRow<AuraDummyApplyShamanSite> const shamanApply[] =
    {
        { 55198, &AuraDummyApplyShaman55198 },
    };

    static AuraDummyRow<AuraDummyQuestTameSite> const questTame[] =
    {
        { 19548, &AuraDummyQuestTame19548 },
        { 19674, &AuraDummyQuestTame19674 },
        { 19687, &AuraDummyQuestTame19687 },
        { 19688, &AuraDummyQuestTame19688 },
        { 19689, &AuraDummyQuestTame19689 },
        { 19692, &AuraDummyQuestTame19692 },
        { 19693, &AuraDummyQuestTame19693 },
        { 19694, &AuraDummyQuestTame19694 },
        { 19696, &AuraDummyQuestTame19696 },
        { 19697, &AuraDummyQuestTame19697 },
        { 19699, &AuraDummyQuestTame19699 },
        { 19700, &AuraDummyQuestTame19700 },
        { 30646, &AuraDummyQuestTame30646 },
        { 30653, &AuraDummyQuestTame30653 },
        { 30654, &AuraDummyQuestTame30654 },
        { 30099, &AuraDummyQuestTame30099 },
        { 30102, &AuraDummyQuestTame30102 },
        { 30105, &AuraDummyQuestTame30105 },
    };

    static AuraDummyRow<AuraDummyRemoveSite> const removal[] =
    {
        { 10255, &AuraDummyRemove10255 },
        { 12479, &AuraDummyRemove12479 },
        { 12774, &AuraDummyRemove12774 },
        { 28169, &AuraDummyRemove28169 },
        { 32045, &AuraDummyRemove32045 },
        { 32051, &AuraDummyRemove32051 },
        { 32052, &AuraDummyRemove32052 },
        { 32286, &AuraDummyRemove32286 },
        { 35079, &AuraDummyRemoveThreatRedirection },
        { 59628, &AuraDummyRemoveThreatRedirection },
        { 36730, &AuraDummyRemove36730 },
        { 41099, &AuraDummyRemove41099 },
        { 41100, &AuraDummyRemove41100 },
        { 41101, &AuraDummyRemove41101 },
        { 42454, &AuraDummyRemove42454 },
        { 42517, &AuraDummyRemove42517 },
        { 43969, &AuraDummyRemove43969 },
        { 44191, &AuraDummyRemove44191 },
        { 45934, &AuraDummyRemove45934 },
        { 45963, &AuraDummyRemove45963 },
        { 46308, &AuraDummyRemove46308 },
        { 46637, &AuraDummyRemove46637 },
        { 48385, &AuraDummyRemove48385 },
        { 50141, &AuraDummyRemove50141 },
        { 51405, &AuraDummyRemove51405 },
        { 51870, &AuraDummyRemove51870 },
        { 52098, &AuraDummyRemove52098 },
        { 53039, &AuraDummyRemove53039 },
        { 53790, &AuraDummyRemove53790 },
        { 53791, &AuraDummyRemove53791 },
        { 53792, &AuraDummyRemove53792 },
        { 56511, &AuraDummyRemove56511 },
        { 61900, &AuraDummyRemove61900 },
        { 68839, &AuraDummyRemove68839 },
    };

    static AuraDummyRow<AuraDummyApplyRemoveGenericSite> const genericApplyRemove[] =
    {
        { 11196, &AuraDummyApplyRemoveGeneric11196 },
        { 24658, &AuraDummyApplyRemoveGeneric24658 },
        { 24661, &AuraDummyApplyRemoveGeneric24661 },
        { 29266, &AuraDummyApplyRemoveGenericFeignDeath },
        { 31261, &AuraDummyApplyRemoveGenericFeignDeath },
        { 37493, &AuraDummyApplyRemoveGenericFeignDeath },
        { 52593, &AuraDummyApplyRemoveGenericFeignDeath },
        { 55795, &AuraDummyApplyRemoveGenericFeignDeath },
        { 57626, &AuraDummyApplyRemoveGenericFeignDeath },
        { 57685, &AuraDummyApplyRemoveGenericFeignDeath },
        { 58768, &AuraDummyApplyRemoveGenericFeignDeath },
        { 58806, &AuraDummyApplyRemoveGenericFeignDeath },
        { 58951, &AuraDummyApplyRemoveGenericFeignDeath },
        { 64461, &AuraDummyApplyRemoveGenericFeignDeath },
        { 65985, &AuraDummyApplyRemoveGenericFeignDeath },
        { 70592, &AuraDummyApplyRemoveGenericFeignDeath },
        { 70628, &AuraDummyApplyRemoveGenericFeignDeath },
        { 70630, &AuraDummyApplyRemoveGenericFeignDeath },
        { 71598, &AuraDummyApplyRemoveGenericFeignDeath },
        { 40133, &AuraDummyApplyRemoveGeneric40133 },
        { 40132, &AuraDummyApplyRemoveGeneric40132 },
        { 40214, &AuraDummyApplyRemoveGeneric40214 },
        { 42515, &AuraDummyApplyRemoveGeneric42515 },
        { 42583, &AuraDummyApplyRemoveGenericFixateTarget },
        { 68987, &AuraDummyApplyRemoveGenericFixateTarget },
        { 47178, &AuraDummyApplyRemoveGeneric47178 },
        { 56422, &AuraDummyApplyRemoveGenericNotSelectable },
        { 70733, &AuraDummyApplyRemoveGenericNotSelectable },
        { 58204, &AuraDummyApplyRemoveGeneric58204 },
        { 58205, &AuraDummyApplyRemoveGeneric58205 },
        { 27978, &AuraDummyApplyRemoveGenericShroudOfDeath },
        { 40131, &AuraDummyApplyRemoveGenericShroudOfDeath },
        { 66936, &AuraDummyApplyRemoveGenericSubmerge },
        { 66948, &AuraDummyApplyRemoveGenericSubmerge },
        { 6606, &AuraDummyApplyRemoveGeneric6606 },
        { 35356, &AuraDummyApplyRemoveGenericSpawnFeignDeath },
        { 35357, &AuraDummyApplyRemoveGenericSpawnFeignDeath },
        { 42557, &AuraDummyApplyRemoveGenericSpawnFeignDeath },
        { 51329, &AuraDummyApplyRemoveGenericSpawnFeignDeath },
        { 43874, &AuraDummyApplyRemoveGeneric43874 },
    };

    static AuraDummyRow<AuraDummyDruidSite> const druid[] =
    {
        { 52610, &AuraDummyDruid52610 },
        { 61336, &AuraDummyDruid61336 },
    };

    static AuraDummyRow<AuraDummyRogueSite> const rogue[] =
    {
        { 57934, &AuraDummyRogue57934 },
    };

    static AuraDummyRow<AuraDummyHunterSite> const hunter[] =
    {
        { 34477, &AuraDummyHunter34477 },
    };

    static AuraDummyRow<AuraDummyPaladinSite> const paladin[] =
    {
        { 20911, &AuraDummyPaladin20911 },
    };

    uint32 rows = RegisterAuraDummyRows(registry, genericApply);
    rows += RegisterAuraDummyRows(registry, warriorApply);
    rows += RegisterAuraDummyRows(registry, hunterApply);
    rows += RegisterAuraDummyRows(registry, shamanApply);
    rows += RegisterAuraDummyRows(registry, questTame);
    rows += RegisterAuraDummyRows(registry, removal);
    rows += RegisterAuraDummyRows(registry, genericApplyRemove);
    rows += RegisterAuraDummyRows(registry, druid);
    rows += RegisterAuraDummyRows(registry, rogue);
    rows += RegisterAuraDummyRows(registry, hunter);
    rows += RegisterAuraDummyRows(registry, paladin);
    return rows;
}
