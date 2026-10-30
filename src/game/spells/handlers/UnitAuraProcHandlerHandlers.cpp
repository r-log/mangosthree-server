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

#include "spells/handlers/UnitAuraProcHandlerHandlers.h"
#include "spells/handlers/SpellHandlerRegistry.h"
#include "Object/Unit.h"
#include "spells/auras/SpellAuras.h"
#include "spells/data/SpellMgr.h"
#include "Server/DBCStores.h"
#include "Utilities/Util.h"
#include "Log.h"
#include "Object/Creature.h"
#include "Object/ObjectLookup.h"
#include "Object/Pet.h"
#include "Object/Totem.h"
#include "spells/SpellModMgr.h"
#include "spells/pipeline/Spell.h"
#include <ctime>

/// `Unit`'s own list and map types, so the bodies can name them unqualified.
typedef Unit::AuraList AuraList;
typedef Unit::SpellAuraHolderMap SpellAuraHolderMap;

/// 9799, 25988: Eye for an Eye: triggers 25997 for the aura's percent of the damage, at most half the unit's maximum
/// health
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcGeneric9799(DummyAuraProcContext& ctx)
{
    // Eye for an Eye
    // return damage % to attacker but < 50% own total health
    ctx.basepoints[0] = ctx.triggerAmount * int32(ctx.damage) / 100;
    if (ctx.basepoints[0] > (int32)ctx.unit->GetMaxHealth() / 2)
    {
        ctx.basepoints[0] = (int32)ctx.unit->GetMaxHealth() / 2;
    }

    ctx.triggered_spell_id = 25997;
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// Sweeping Strikes (NPC spells may be)
}

/// 18765, 35429: Sweeping Strikes: triggers 26654 on a random enemy in melee range other than the victim; fails when
/// the proc spell is 26654 or no such enemy is found
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcGeneric18765(DummyAuraProcContext& ctx)
{
    // prevent chain of triggered spell from same triggered spell
    if (ctx.procSpell && ctx.procSpell->ID == 26654)
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }

    ctx.target = ctx.unit->SelectRandomUnfriendlyTarget(ctx.pVictim);
    if (!ctx.target)
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }

    ctx.triggered_spell_id = 26654;
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// Twisted Reflection (boss spell)
}

/// 21063: Twisted Reflection: triggers 21064
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcGeneric21063(DummyAuraProcContext& ctx)
{
    ctx.triggered_spell_id = 21064;
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// Unstable Power
}

/// 24658: Unstable Power: a proc spell other than 24659 removes one stack of 24659
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcGeneric24658(DummyAuraProcContext& ctx)
{
    if (!ctx.procSpell || ctx.procSpell->ID == 24659)
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }
    // Need remove one 24659 aura
    ctx.unit->RemoveAuraHolderFromStack(24659);
    return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_OK);
// Restless Strength
}

/// 24661: Restless Strength: removes one stack of 24662
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcGeneric24661(DummyAuraProcContext& ctx)
{
    // Need remove one 24662 aura
    ctx.unit->RemoveAuraHolderFromStack(24662);
    return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_OK);
// Adaptive Warding (Frostfire Regalia set)
}

/// 28764: Adaptive Warding: with Mage Armor on the unit, triggers the ward of the proc spell's school on the unit
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcGeneric28764(DummyAuraProcContext& ctx)
{
    if (!ctx.procSpell)
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }

    // find Mage Armor
    bool found = false;
    AuraList const& mRegenInterrupt = ctx.unit->GetAurasByType(SPELL_AURA_MOD_MANA_REGEN_INTERRUPT);
    for (AuraList::const_iterator iter = mRegenInterrupt.begin(); iter != mRegenInterrupt.end(); ++iter)
    {
        if (SpellEntry const* iterSpellProto = (*iter)->GetSpellProto())
        {
            SpellClassOptionsEntry const* iterClassOptions = iterSpellProto->GetSpellClassOptions();
            if (iterClassOptions && iterClassOptions->SpellClassSet == SPELLFAMILY_MAGE && (iterClassOptions->SpellClassMask & UI64LIT(0x10000000)))
            {
                found = true;
                break;
            }
        }
    }
    if (!found)
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }

    switch (GetFirstSchoolInMask(GetSpellSchoolMask(ctx.procSpell)))
    {
        case SPELL_SCHOOL_NORMAL:
        case SPELL_SCHOOL_HOLY:
            return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);  // ignored
        case SPELL_SCHOOL_FIRE:   ctx.triggered_spell_id = 28765; break;
        case SPELL_SCHOOL_NATURE: ctx.triggered_spell_id = 28768; break;
        case SPELL_SCHOOL_FROST:  ctx.triggered_spell_id = 28766; break;
        case SPELL_SCHOOL_SHADOW: ctx.triggered_spell_id = 28769; break;
        case SPELL_SCHOOL_ARCANE: ctx.triggered_spell_id = 28770; break;
        default:
            return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }

    ctx.target = ctx.unit;
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// Obsidian Armor (Justice Bearer`s Pauldrons shoulder)
}

/// 27539: Obsidian Armor: triggers the resistance of the proc spell's school on the unit
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcGeneric27539(DummyAuraProcContext& ctx)
{
    if (!ctx.procSpell)
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }

    switch (GetFirstSchoolInMask(GetSpellSchoolMask(ctx.procSpell)))
    {
        case SPELL_SCHOOL_NORMAL:
            return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);  // ignore
        case SPELL_SCHOOL_HOLY:   ctx.triggered_spell_id = 27536; break;
        case SPELL_SCHOOL_FIRE:   ctx.triggered_spell_id = 27533; break;
        case SPELL_SCHOOL_NATURE: ctx.triggered_spell_id = 27538; break;
        case SPELL_SCHOOL_FROST:  ctx.triggered_spell_id = 27534; break;
        case SPELL_SCHOOL_SHADOW: ctx.triggered_spell_id = 27535; break;
        case SPELL_SCHOOL_ARCANE: ctx.triggered_spell_id = 27540; break;
        default:
            return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }

    ctx.target = ctx.unit;
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// Mana Leech (Passive) (Priest Pet Aura)
}

/// 28305: Mana Leech (Passive): triggers 34650 on the owner
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcGeneric28305(DummyAuraProcContext& ctx)
{
    // Cast on owner
    ctx.target = ctx.unit->GetOwner();
    if (!ctx.target)
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }

    ctx.triggered_spell_id = 34650;
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// Divine purpose
}

/// 31871, 31872: absent from the client's spell data: at the aura's chance, removes every stun aura from the victim
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcGeneric31871(DummyAuraProcContext& ctx)
{
    // Roll chance
    if (!roll_chance_i(ctx.triggerAmount))
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }

    // Remove any stun effect on target
    SpellAuraHolderMap& Auras = ctx.pVictim->GetSpellAuraHolderMap();
    for (SpellAuraHolderMap::const_iterator iter = Auras.begin(); iter != Auras.end();)
    {
        if (iter->second->HasMechanic(MECHANIC_STUN))
        {
            ctx.pVictim->RemoveAurasDueToSpell(iter->second->GetId());
            iter = Auras.begin();
        }
        else
        {
            ++iter;
        }
    }
    return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_OK);
// Mark of Malice
}

/// 33493: Mark of Malice: at the last charge, triggers 33494 on the unit
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcGeneric33493(DummyAuraProcContext& ctx)
{
    // Cast finish spell at last charge
    if (ctx.triggeredByAura->GetHolder()->GetAuraCharges() > 1)
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }

    ctx.target = ctx.unit;
    ctx.triggered_spell_id = 33494;
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// Vampiric Aura (boss spell)
}

/// 38196: Vampiric Aura: triggers 31285 on the unit for three times the damage
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcGeneric38196(DummyAuraProcContext& ctx)
{
    ctx.basepoints[0] = 3 * ctx.damage;             // 300%
    if (ctx.basepoints[0] < 0)
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }

    ctx.triggered_spell_id = 31285;
    ctx.target = ctx.unit;
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// Aura of Madness (Darkmoon Card: Madness trinket)
//=====================================================
// 39511 Sociopath: +35 strength (Paladin, Rogue, Druid, Warrior)
// 40997 Delusional: +70 attack power (Rogue, Hunter, Paladin, Warrior, Druid)
// 40998 Kleptomania: +35 agility (Warrior, Rogue, Paladin, Hunter, Druid)
// 40999 Megalomania: +41 damage/healing (Druid, Shaman, Priest, Warlock, Mage, Paladin)
// 41002 Paranoia: +35 spell/melee/ranged crit strike rating (All classes)
// 41005 Manic: +35 haste (spell, melee and ranged) (All classes)
// 41009 Narcissism: +35 intellect (Druid, Shaman, Priest, Warlock, Mage, Paladin, Hunter)
// 41011 Martyr Complex: +35 stamina (All classes)
// 41406 Dementia: Every 5 seconds either gives you +5% damage/healing. (Druid, Shaman, Priest, Warlock, Mage, Paladin)
// 41409 Dementia: Every 5 seconds either gives you -5% damage/healing. (Druid, Shaman, Priest, Warlock, Mage, Paladin)
}

/// 39446: Aura of Madness: a player triggers a random buff of its class on itself, and sometimes says a line
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcGeneric39446(DummyAuraProcContext& ctx)
{
    if (ctx.unit->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }

    // Select class defined buff
    switch (ctx.unit->getClass())
    {
        case CLASS_PALADIN:                 // 39511,40997,40998,40999,41002,41005,41009,41011,41409
        case CLASS_DRUID:                   // 39511,40997,40998,40999,41002,41005,41009,41011,41409
        {
            uint32 RandomSpell[] = {39511, 40997, 40998, 40999, 41002, 41005, 41009, 41011, 41409};
            ctx.triggered_spell_id = RandomSpell[urand(0, countof(RandomSpell) - 1)];
            break;
        }
        case CLASS_ROGUE:                   // 39511,40997,40998,41002,41005,41011
        case CLASS_WARRIOR:                 // 39511,40997,40998,41002,41005,41011
        {
            uint32 RandomSpell[] = {39511, 40997, 40998, 41002, 41005, 41011};
            ctx.triggered_spell_id = RandomSpell[urand(0, countof(RandomSpell) - 1)];
            break;
        }
        case CLASS_PRIEST:                  // 40999,41002,41005,41009,41011,41406,41409
        case CLASS_SHAMAN:                  // 40999,41002,41005,41009,41011,41406,41409
        case CLASS_MAGE:                    // 40999,41002,41005,41009,41011,41406,41409
        case CLASS_WARLOCK:                 // 40999,41002,41005,41009,41011,41406,41409
        {
            uint32 RandomSpell[] = {40999, 41002, 41005, 41009, 41011, 41406, 41409};
            ctx.triggered_spell_id = RandomSpell[urand(0, countof(RandomSpell) - 1)];
            break;
        }
        case CLASS_HUNTER:                  // 40997,40999,41002,41005,41009,41011,41406,41409
        {
            uint32 RandomSpell[] = {40997, 40999, 41002, 41005, 41009, 41011, 41406, 41409};
            ctx.triggered_spell_id = RandomSpell[urand(0, countof(RandomSpell) - 1)];
            break;
        }
        default:
            return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }

    ctx.target = ctx.unit;
    if (roll_chance_i(10))
    {
        ctx.unit->Say("This is Madness!", LANG_UNIVERSAL);
    }
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// Sunwell Exalted Caster Neck (Shattered Sun Pendant of Acumen neck)
// cast 45479 Light's Wrath if Exalted by Aldor
// cast 45429 Arcane Bolt if Exalted by Scryers
}

/// 45481: Sunwell Exalted Caster Neck: a player exalted with the Aldor (932) triggers 45479 on itself, else one exalted
/// with the Scryers (934) triggers 45429 on a hostile target
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcGeneric45481(DummyAuraProcContext& ctx)
{
    if (ctx.unit->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }

    // Get Aldor reputation rank
    if (ctx.unit->GetReputationRank(932) == REP_EXALTED)
    {
        ctx.target = ctx.unit;
        ctx.triggered_spell_id = 45479;
        return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
    }
    // Get Scryers reputation rank
    if (ctx.unit->GetReputationRank(934) == REP_EXALTED)
    {
        // triggered at positive/self casts also, current attack target used then
        if (ctx.unit->IsFriendlyTo(ctx.target))
        {
            ctx.target = ctx.unit->getVictim();
            if (!ctx.target)
            {
                ctx.target = ObjectLookup::GetUnit(*ctx.unit, ctx.unit->GetSelectionGuid());
                if (!ctx.target)
                {
                    return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
                }
            }
            if (ctx.unit->IsFriendlyTo(ctx.target))
            {
                return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
            }
        }

        ctx.triggered_spell_id = 45429;
        return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
    }
    return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
// Sunwell Exalted Melee Neck (Shattered Sun Pendant of Might neck)
// cast 45480 Light's Strength if Exalted by Aldor
// cast 45428 Arcane Strike if Exalted by Scryers
}

/// 45482: Sunwell Exalted Melee Neck: a player exalted with the Aldor (932) triggers 45480 on itself, else one exalted
/// with the Scryers (934) triggers 45428
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcGeneric45482(DummyAuraProcContext& ctx)
{
    if (ctx.unit->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }

    // Get Aldor reputation rank
    if (ctx.unit->GetReputationRank(932) == REP_EXALTED)
    {
        ctx.target = ctx.unit;
        ctx.triggered_spell_id = 45480;
        return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
    }
    // Get Scryers reputation rank
    if (ctx.unit->GetReputationRank(934) == REP_EXALTED)
    {
        ctx.triggered_spell_id = 45428;
        return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
    }
    return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
// Sunwell Exalted Tank Neck (Shattered Sun Pendant of Resolve neck)
// cast 45431 Arcane Insight if Exalted by Aldor
// cast 45432 Light's Ward if Exalted by Scryers
}

/// 45483: Sunwell Exalted Tank Neck: a player exalted with the Aldor (932) triggers 45432 on itself, else one exalted
/// with the Scryers (934) triggers 45431 on itself
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcGeneric45483(DummyAuraProcContext& ctx)
{
    if (ctx.unit->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }

    // Get Aldor reputation rank
    if (ctx.unit->GetReputationRank(932) == REP_EXALTED)
    {
        ctx.target = ctx.unit;
        ctx.triggered_spell_id = 45432;
        return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
    }
    // Get Scryers reputation rank
    if (ctx.unit->GetReputationRank(934) == REP_EXALTED)
    {
        ctx.target = ctx.unit;
        ctx.triggered_spell_id = 45431;
        return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
    }
    return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
// Sunwell Exalted Healer Neck (Shattered Sun Pendant of Restoration neck)
// cast 45478 Light's Salvation if Exalted by Aldor
// cast 45430 Arcane Surge if Exalted by Scryers
}

/// 45484: Sunwell Exalted Healer Neck: a player exalted with the Aldor (932) triggers 45478 on itself, else one exalted
/// with the Scryers (934) triggers 45430
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcGeneric45484(DummyAuraProcContext& ctx)
{
    if (ctx.unit->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }

    // Get Aldor reputation rank
    if (ctx.unit->GetReputationRank(932) == REP_EXALTED)
    {
        ctx.target = ctx.unit;
        ctx.triggered_spell_id = 45478;
        return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
    }
    // Get Scryers reputation rank
    if (ctx.unit->GetReputationRank(934) == REP_EXALTED)
    {
        ctx.triggered_spell_id = 45430;
        return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
    }
    return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
/*
// Sunwell Exalted Caster Neck (??? neck)
// cast ??? Light's Wrath if Exalted by Aldor
// cast ??? Arcane Bolt if Exalted by Scryers*/
}

/// 46569: Sunwell Exalted Caster Neck: fails
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcGeneric46569(DummyAuraProcContext& ctx)
{
    return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);          // old unused version
// Living Seed
}

/// 48504: Living Seed: triggers 48503 on the unit for the aura's amount
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcGeneric48504(DummyAuraProcContext& ctx)
{
    ctx.triggered_spell_id = 48503;
    ctx.basepoints[0] = ctx.triggerAmount;
    ctx.target = ctx.unit;
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// Vampiric Touch (generic, used by some boss)
}

/// 52723, 60501: Vampiric Touch: triggers 52724 on the unit for half the damage
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcGeneric52723(DummyAuraProcContext& ctx)
{
    ctx.triggered_spell_id = 52724;
    ctx.basepoints[0] = ctx.damage / 2;
    ctx.target = ctx.unit;
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// Shadowfiend Death (Gain mana if pet dies with Glyph of Shadowfiend)
}

/// 57989: Shadowfiend Death: the player owner casts 58227 on itself
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcGeneric57989(DummyAuraProcContext& ctx)
{
    Unit* owner = ctx.unit->GetOwner();
    if (!owner || owner->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }

    // Glyph of Shadowfiend (need cast as self cast for owner, no hidden cooldown)
    owner->CastSpell(owner, 58227, true, ctx.castItem, ctx.triggeredByAura);
    return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_OK);
// Kill Command, pet aura
}

/// 63305: Grim Reprisal: a creature caster becomes entry 33943 and casts 64017; triggers 64039 for the damage
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcGeneric63305(DummyAuraProcContext& ctx)
{
    // also update caster entry if required
    if (Unit* caster = ctx.triggeredByAura->GetCaster())
    {
        Creature* casterCreature = caster->ToCreature();
        if (caster->GetEntry() != 33943 && casterCreature)
        {
            casterCreature->UpdateEntry(33943);
            caster->CastSpell(caster, 64017, true);
        }
    }

    ctx.triggered_spell_id = 64039;
    ctx.basepoints[EFFECT_INDEX_0] = ctx.damage;
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// Glyph of Life Tap
}

/// 58914: absent from the client's spell data: removes one stack of the owner's 34027; with more than one stack, the
/// aura loses one and the proc cannot trigger, else Grim Reprisal's body runs
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcGeneric58914(DummyAuraProcContext& ctx)
{
    // also decrease owner buff stack
    if (Unit* owner = ctx.unit->GetOwner())
    {
        owner->RemoveAuraHolderFromStack(34027);
    }

    // Remove only single aura from stack
    if (ctx.triggeredByAura->GetStackAmount() > 1 && !ctx.triggeredByAura->GetHolder()->ModStackAmount(-1))
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_CANT_TRIGGER);
    }
    return DummyAuraProcGeneric63305(ctx);
// Grim Reprisal
}

/// 63320: Glyph of Life Tap: triggers 63321
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcGeneric63320(DummyAuraProcContext& ctx)
{
    ctx.triggered_spell_id = 63321;
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// Retaliation
}

/// 65932: Retaliation: triggers 65934
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcGeneric65932(DummyAuraProcContext& ctx)
{
    ctx.triggered_spell_id = 65934;
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// Earth Shield
}

/// 66063: Earth Shield: triggers 66064
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcGeneric66063(DummyAuraProcContext& ctx)
{
    ctx.triggered_spell_id = 66064;
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// Meteor Fists
}

/// 66725, 68161: Meteor Fists: triggers 66765
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcGeneric66725(DummyAuraProcContext& ctx)
{
    ctx.triggered_spell_id = 66765;
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// Meteor Fists
}

/// 66808, 68160: Meteor Fists: triggers 66809
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcGeneric66808(DummyAuraProcContext& ctx)
{
    ctx.triggered_spell_id = 66809;
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// Shiny Shard of the Scale - Equip Effect
}

/// 69739: Shiny Shard of the Scale - Equip Effect: triggers 69734 after a positive spell, else 69730
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcGeneric69739(DummyAuraProcContext& ctx)
{
    // Cauterizing Heal or Searing Flame
    ctx.triggered_spell_id = (ctx.procFlag & PROC_FLAG_SUCCESSFUL_POSITIVE_SPELL) ? 69734 : 69730;
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// Purified Shard of the Scale - Equip Effect
}

/// 69755: Purified Shard of the Scale - Equip Effect: triggers 69733 after a positive spell, else 69729
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcGeneric69755(DummyAuraProcContext& ctx)
{
    // Cauterizing Heal or Searing Flame
    ctx.triggered_spell_id = (ctx.procFlag & PROC_FLAG_SUCCESSFUL_POSITIVE_SPELL) ? 69733 : 69729;
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// Item - Shadowmourne Legendary
}

/// 71903: Item - Shadowmourne Legendary: at the aura's chance, triggers Soul Fragment (71905) with its visuals; at a
/// full stack, removes it and casts Chaos Bane (71904)
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcGeneric71903(DummyAuraProcContext& ctx)
{
    if (!roll_chance_i(ctx.triggerAmount))
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }

    ctx.triggered_spell_id = 71905;             // Soul Fragment

    SpellAuraHolder* aurHolder = ctx.unit->GetSpellAuraHolder(ctx.triggered_spell_id);

    // will added first to stack
    if (!aurHolder)
    {
        ctx.unit->CastSpell(ctx.unit, 72521, true);       // Shadowmourne Visual Low
    }
    // half stack
    else if (aurHolder->GetStackAmount() + 1 == 6)
    {
        ctx.unit->CastSpell(ctx.unit, 72523, true);       // Shadowmourne Visual High
    }
    // full stack
    else if (aurHolder->GetStackAmount() + 1 >= aurHolder->GetSpellProto()->GetStackAmount())
    {
        ctx.unit->RemoveAurasDueToSpell(ctx.triggered_spell_id);
        ctx.unit->CastSpell(ctx.unit, 71904, true);       // Chaos Bane
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_OK);
    }
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// Blood Link
}

/// 72178: Blood Link: triggers 72195
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcGeneric72178(DummyAuraProcContext& ctx)
{
    ctx.triggered_spell_id = 72195;
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
}

/// one body for 5 labels: 11119, 11120, 12846: Ignite; 12847, 12848: absent from the client's spell data: triggers
/// 12654 for 4 to 20 percent of the damage by rank
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcMage11119(DummyAuraProcContext& ctx)
{
    // Ignite
    switch (ctx.dummySpell->ID)
    {
        case 11119: ctx.basepoints[0] = int32(0.04f * ctx.damage); break;
        case 11120: ctx.basepoints[0] = int32(0.08f * ctx.damage); break;
        case 12846: ctx.basepoints[0] = int32(0.12f * ctx.damage); break;
        case 12847: ctx.basepoints[0] = int32(0.16f * ctx.damage); break;
        case 12848: ctx.basepoints[0] = int32(0.20f * ctx.damage); break;
        default:
            sLog.outError("Unit::HandleDummyAuraProc: non handled spell id: %u (IG)", ctx.dummySpell->ID);
            return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }

    ctx.triggered_spell_id = 12654;
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// Glyph of Ice Block
}

/// 56372: Glyph of Ice Block: removes a player's cooldowns of category 35
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcMage56372(DummyAuraProcContext& ctx)
{
    if (ctx.unit->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }

    // not 100% safe with client version switches but for 3.1.3 no spells with cooldown that can have mage player except Frost Nova.
    ctx.unit->RemoveSpellCategoryCooldown(35, true);
    return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_OK);
// Glyph of Polymorph
}

/// 56375: Glyph of Polymorph: removes the periodic damage auras from a living victim
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcMage56375(DummyAuraProcContext& ctx)
{
    if (!ctx.pVictim || !ctx.pVictim->IsAlive())
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }

    ctx.pVictim->RemoveSpellsCausingAura(SPELL_AURA_PERIODIC_DAMAGE);
    ctx.pVictim->RemoveSpellsCausingAura(SPELL_AURA_PERIODIC_DAMAGE_PERCENT);
    return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_OK);
// Blessing of Ancient Kings
}

/// 64411: Blessing of Ancient Kings: after a positive spell, triggers 64413 for 15 percent of the damage
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcMage64411(DummyAuraProcContext& ctx)
{
    // for DOT procs
    if (!IsPositiveSpell(ctx.procSpell->ID))
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }

    ctx.triggered_spell_id = 64413;
    ctx.basepoints[0] = ctx.damage * 15 / 100;
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// Fingers of Frost
}

/// 74396: absent from the client's spell data: removes one stack of 74396
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcMage74396(DummyAuraProcContext& ctx)
{
    // Remove only single aura from stack and remove holder if its last stack
    ctx.unit->RemoveAuraHolderFromStack(74396);
    return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_OK);
}

/// one body for 3 labels: 18094, 18095: Nightfall, 56218: Glyph of Corruption: triggers 17941 on the unit
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcWarlock18094(DummyAuraProcContext& ctx)
{
    // Nightfall & Glyph of Corruption
    ctx.target = ctx.unit;
    ctx.triggered_spell_id = 17941;
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// Soul Leech
}

/// one body for 3 labels: 30293, 30295: Soul Leech; 30296: absent from the client's spell data: triggers 30294 on the
/// unit for the aura's percent of the damage
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcWarlock30293(DummyAuraProcContext& ctx)
{
    // health
    ctx.basepoints[0] = int32(ctx.damage * ctx.triggerAmount / 100);
    ctx.target = ctx.unit;
    ctx.triggered_spell_id = 30294;
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// Shadowflame (Voidheart Raiment set bonus)
}

/// 37377: Shadowflame: triggers 37379
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcWarlock37377(DummyAuraProcContext& ctx)
{
    ctx.triggered_spell_id = 37379;
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// Pet Healing (Corruptor Raiment or Rift Stalker Armor)
}

/// 37381: Pet Healing: triggers 37382 on the pet for the aura's percent of the damage
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcWarlock37381(DummyAuraProcContext& ctx)
{
    ctx.target = ctx.unit->GetPet();
    if (!ctx.target)
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }

    // heal amount
    ctx.basepoints[0] = ctx.damage * ctx.triggerAmount / 100;
    ctx.triggered_spell_id = 37382;
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// Shadowflame Hellfire (Voidheart Raiment set bonus)
}

/// 39437: Shadowflame Hellfire and RoF: triggers 37378
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcWarlock39437(DummyAuraProcContext& ctx)
{
    ctx.triggered_spell_id = 37378;
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// Siphon Life
}

/// 63108: Siphon Life: the unit's aura 56216, absent from the client's spell data, raises the amount read from the aura
/// by its own percent; triggers 63106 for that percent of the damage
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcWarlock63108(DummyAuraProcContext& ctx)
{
    // Glyph of Siphon Life
    if (Aura* aur = ctx.unit->GetAura(56216, EFFECT_INDEX_0))
    {
        ctx.triggerAmount += ctx.triggerAmount * aur->GetModifier()->m_amount / 100;
    }

    ctx.basepoints[0] = int32(ctx.damage * ctx.triggerAmount / 100);
    ctx.triggered_spell_id = 63106;
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
}

/// 15286: Vampiric Embrace: unless the unit damaged itself, casts 15290 on itself with base points from the aura's
/// percent of the damage, and the proc ends
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcPriest15286(DummyAuraProcContext& ctx)
{
    // Vampiric Embrace
    // Return if self damage
    if (ctx.unit == ctx.pVictim)
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }

    // Heal amount - Self/Team
    int32 team = ctx.triggerAmount * ctx.damage / 500;
    int32 self = ctx.triggerAmount * ctx.damage / 100 - team;
    ctx.unit->CastCustomSpell(ctx.unit, 15290, &team, &self, NULL, true, ctx.castItem, ctx.triggeredByAura);
    return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_OK);              // no hidden cooldown
// Priest Tier 6 Trinket (Ashtongue Talisman of Acumen)
}

/// 40438: Priest Tier 6 Trinket: triggers 40441 on the unit after Shadow Word: Pain, 40440 after Renew; fails otherwise
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcPriest40438(DummyAuraProcContext& ctx)
{
    // Shadow Word: Pain
    if (ctx.procClassOptions && ctx.procClassOptions->SpellClassMask & UI64LIT(0x0000000000008000))
    {
        ctx.triggered_spell_id = 40441;
    }
    // Renew
    else if (ctx.procClassOptions && ctx.procClassOptions->SpellClassMask & UI64LIT(0x0000000000000010))
    {
        ctx.triggered_spell_id = 40440;
    }
    else
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }

    ctx.target = ctx.unit;
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// Oracle Healing Bonus ("Garments of the Oracle" set)
}

/// 26169: Oracle Healing Bonus: triggers 26170 on the unit for a tenth of the damage
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcPriest26169(DummyAuraProcContext& ctx)
{
    // heal amount
    ctx.basepoints[0] = int32(ctx.damage * 10 / 100);
    ctx.target = ctx.unit;
    ctx.triggered_spell_id = 26170;
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// Frozen Shadoweave (Shadow's Embrace set) warning! its not only priest set
}

/// 39372: Frozen Shadoweave: after a frost or shadow proc spell, triggers 39373 on the unit for the aura's percent of
/// the damage; fails otherwise
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcPriest39372(DummyAuraProcContext& ctx)
{
    if (!ctx.procSpell || (GetSpellSchoolMask(ctx.procSpell) & (SPELL_SCHOOL_MASK_FROST | SPELL_SCHOOL_MASK_SHADOW)) == 0)
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }

    // heal amount
    ctx.basepoints[0] = ctx.damage * ctx.triggerAmount / 100;
    ctx.target = ctx.unit;
    ctx.triggered_spell_id = 39373;
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// Greater Heal (Vestments of Faith (Priest Tier 3) - 4 pieces bonus)
}

/// 28809: Greater Heal: triggers 28810
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcPriest28809(DummyAuraProcContext& ctx)
{
    ctx.triggered_spell_id = 28810;
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// Glyph of Dispel Magic
}

/// 55677: Glyph of Dispel Magic: for a friendly target, the base points are the aura's percent of its maximum health;
/// fails otherwise
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcPriest55677(DummyAuraProcContext& ctx)
{
    if (!ctx.target->IsFriendlyTo(ctx.unit))
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }

    ctx.basepoints[0] = int32(ctx.target->GetMaxHealth() * ctx.triggerAmount / 100);
    // triggered_spell_id in spell data
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// Glyph of Prayer of Healing
}

/// 55680: Glyph of Prayer of Healing: triggers 56161 for half the aura's percent of the damage
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcPriest55680(DummyAuraProcContext& ctx)
{
    ctx.basepoints[0] = int32(ctx.damage * ctx.triggerAmount  / 200);   // 10% each tick
    ctx.triggered_spell_id = 56161;             // Glyph of Prayer of Healing
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// Priest T10 Healer 2P Bonus
}

/// 70770: Item - Priest T10 Healer 2P Bonus: triggers 70772 for the aura's percent of the damage, split over its ticks
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcPriest70770(DummyAuraProcContext& ctx)
{
    ctx.triggered_spell_id = 70772;             // Blessed Healing
    ctx.basepoints[0] = int32(ctx.triggerAmount * ctx.damage / 100) / GetSpellAuraMaxTicks(ctx.triggered_spell_id);
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
}

/// 24932: Leader of the Pack: with a percent on the aura, and no 34299 cooldown on a proc with a cooldown, triggers
/// 34299 on the unit for that percent of its maximum health; when the unit cast the aura, it also casts 60889 on
/// itself; fails otherwise
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcDruid24932(DummyAuraProcContext& ctx)
{
    // Leader of the Pack
    // dummy m_amount store health percent (!=0 if Improved Leader of the Pack applied)
    int32 heal_percent = ctx.triggeredByAura->GetModifier()->m_amount;
    if (!heal_percent)
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }

    // check explicitly only to prevent mana cast when halth cast cooldown
    if (ctx.cooldown && ctx.m_spellCooldownMgr.HasSpellCooldown(34299, time(NULL)))
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }

    // health
    ctx.triggered_spell_id = 34299;
    ctx.basepoints[0] = ctx.unit->GetMaxHealth() * heal_percent / 100;
    ctx.target = ctx.unit;

    // mana to caster
    if (ctx.triggeredByAura->GetCasterGuid() == ctx.unit->GetObjectGuid())
    {
        if (SpellEntry const* manaCastEntry = sSpellStore.LookupEntry(60889))
        {
            int32 mana_percent = manaCastEntry->CalculateSimpleValue(EFFECT_INDEX_0) * heal_percent;
            ctx.unit->CastCustomSpell(ctx.unit, manaCastEntry, &mana_percent, NULL, NULL, true, ctx.castItem, ctx.triggeredByAura);
        }
    }
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// Healing Touch (Dreamwalker Raiment set)
}

/// 28719: Healing Touch: triggers 28742 on the unit for 30 percent of the proc spell's mana cost
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcDruid28719(DummyAuraProcContext& ctx)
{
    // mana back
    ctx.basepoints[0] = int32(ctx.procSpell->GetManaCost() * 30 / 100);
    ctx.target = ctx.unit;
    ctx.triggered_spell_id = 28742;
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// Healing Touch Refund (Idol of Longevity trinket)
}

/// 28847: Healing Touch Refund: triggers 28848 on the unit
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcDruid28847(DummyAuraProcContext& ctx)
{
    ctx.target = ctx.unit;
    ctx.triggered_spell_id = 28848;
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// Mana Restore (Malorne Raiment set / Malorne Regalia set)
}

/// 37288, 37295: Mana Restore: triggers 37238 on the unit
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcDruid37288(DummyAuraProcContext& ctx)
{
    ctx.target = ctx.unit;
    ctx.triggered_spell_id = 37238;
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// Druid Tier 6 Trinket
}

/// 40442: Druid Tier 6 Trinket: triggers on the unit 40445 after Starfire or 40446 after Rejuvenation at 25 percent,
/// 40452 after Mangle at 40 percent; fails otherwise
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcDruid40442(DummyAuraProcContext& ctx)
{
    float  chance;

    // Starfire
    if (ctx.procClassOptions && ctx.procClassOptions->SpellClassMask & UI64LIT(0x0000000000000004))
    {
        ctx.triggered_spell_id = 40445;
        chance = 25.0f;
    }
    // Rejuvenation
    else if (ctx.procClassOptions && ctx.procClassOptions->SpellClassMask & UI64LIT(0x0000000000000010))
    {
        ctx.triggered_spell_id = 40446;
        chance = 25.0f;
    }
    // Mangle (Bear) and Mangle (Cat)
    else if (ctx.procClassOptions && ctx.procClassOptions->SpellClassMask & UI64LIT(0x0000044000000000))
    {
        ctx.triggered_spell_id = 40452;
        chance = 40.0f;
    }
    else
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }

    if (!roll_chance_f(chance))
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }

    ctx.target = ctx.unit;
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// Maim Interrupt
}

/// 44835: Maim Interrupt: triggers 32747
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcDruid44835(DummyAuraProcContext& ctx)
{
    // Deadly Interrupt Effect
    ctx.triggered_spell_id = 32747;
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// Glyph of Rejuvenation
}

/// 54754: Glyph of Rejuvenation: on a victim at or below half health, triggers 54755 for the aura's percent of the
/// damage; fails otherwise
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcDruid54754(DummyAuraProcContext& ctx)
{
    // less 50% health
    if (ctx.pVictim->GetMaxHealth() < 2 * ctx.pVictim->GetHealth())
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }
    ctx.basepoints[0] = ctx.triggerAmount * ctx.damage / 100;
    ctx.triggered_spell_id = 54755;
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// Glyph of Rake
}

/// 54821: Glyph of Pounce: triggers 54820
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcDruid54821(DummyAuraProcContext& ctx)
{
    ctx.triggered_spell_id = 54820;
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// Item - Druid T10 Restoration 4P Bonus (Rejuvenation)
}

/// 70664: Item - Druid T10 Restoration 4P Bonus (Rejuvenation): for a player, the victim casts the proc spell on a
/// random friendly unit in the spell's radius, with the unit's radius mods applied, and the proc ends; fails without a
/// proc spell or such a unit
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcDruid70664(DummyAuraProcContext& ctx)
{
    if (!ctx.procSpell || ctx.unit->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }

    float radius;
    SpellEffectEntry const* spellEffect = ctx.procSpell->GetSpellEffect(EFFECT_INDEX_0);
    if (spellEffect && spellEffect->GetRadiusIndex())
    {
        radius = GetSpellRadius(sSpellRadiusStore.LookupEntry(spellEffect->GetRadiusIndex()));
    }
    else
    {
        radius = GetSpellMaxRange(sSpellRangeStore.LookupEntry(ctx.procSpell->RangeIndex));
    }

    ctx.unit->GetSpellMods()->ApplySpellMod(ctx.procSpell->ID, SPELLMOD_RADIUS, radius);

    Unit* second = ctx.pVictim->SelectRandomFriendlyTarget(ctx.pVictim, radius);

    if (!second)
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }

    ctx.pVictim->CastSpell(second, ctx.procSpell, true, NULL, ctx.triggeredByAura, ctx.unit->GetObjectGuid());
    return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_OK);
// Item - Druid T10 Balance 4P Bonus
}

/// 70723: Item - Druid T10 Balance 4P Bonus: triggers 71023 for the aura's percent of the damage, split over its ticks
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcDruid70723(DummyAuraProcContext& ctx)
{
    ctx.triggered_spell_id = 71023;             // Languish
    ctx.basepoints[0] = int32(ctx.triggerAmount * ctx.damage / 100) / GetSpellAuraMaxTicks(ctx.triggered_spell_id);
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
}

/// 23582: Clean Escape: after a proc spell with a first effect, triggers 23583; fails otherwise
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcRogue23582(DummyAuraProcContext& ctx)
{
    // Clean Escape
    // triggered spell have same masks and etc with main Vanish spell
    if (!ctx.procSpell || ctx.procSpell->GetSpellEffectIdByIndex(EFFECT_INDEX_0) == SPELL_EFFECT_NONE)
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }
    ctx.triggered_spell_id = 23583;
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// Deadly Throw Interrupt
}

/// 32748: Deadly Throw Interrupt: unless the victim is the unit, triggers 32747
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcRogue32748(DummyAuraProcContext& ctx)
{
    // Prevent cast Deadly Throw Interrupt on self from last effect (apply dummy) of Deadly Throw
    if (ctx.unit == ctx.pVictim)
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }

    ctx.triggered_spell_id = 32747;
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// Tricks of the trade
}

/// 57934: Tricks of the Trade: triggers 57933 on the unit's threat redirection target and casts 59628 on itself; fails
/// without that target
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcRogue57934(DummyAuraProcContext& ctx)
{
    ctx.triggered_spell_id = 57933;             // Tricks of the Trade, increased damage buff
    ctx.target = ctx.unit->GetHostileRefManager().GetThreatRedirectionTarget();
    if (!ctx.target)
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }

    ctx.unit->CastSpell(ctx.unit, 59628, true);           // Tricks of the Trade (caster timer)
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// Main Gauche (Combat Rogue mastery — spell 76806).
// Procs on main-hand auto-attack only; triggered spell
// 86392 is an off-hand weapon-percent damage hit. The
// proc chance is the mastery-scaled aura amount written
// by Player::UpdateMasteryAuras (StatSystem.cpp:899) on
// EFFECT_0, matching TC-Preservation's spell_rog_main_-
// gauche at scripts/Spells/spell_rogue.cpp:784 which
// hooks EFFECT_0 only. The DBC has a second SPELL_AURA_-
// DUMMY effect with a non-zero placeholder base value
// (200) that UpdateMasteryAuras skips per its
// CalculateSimpleValue guard; without this gate the
// handler would fire twice per hit and a 200%-clamped
// roll would proc 100% of the time. No CanProcFrom
// bypass needed in Unit::ProcDamageAndSpellFor because
// auto-attack procs pass procSpell=NULL and skip the
// bypass site entirely.
}

/// 76806: Main Gauche: on the first effect, a player's main-hand melee hit at the aura's chance triggers 86392 on the
/// victim; fails otherwise
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcRogue76806(DummyAuraProcContext& ctx)
{
    if (ctx.effIndex != EFFECT_INDEX_0)
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }
    if (ctx.unit->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }
    // main-hand auto-attack only (exclude off-hand)
    if (!(ctx.procFlag & PROC_FLAG_SUCCESSFUL_MELEE_HIT)
        || (ctx.procFlag & PROC_FLAG_SUCCESSFUL_OFFHAND_HIT))
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }
    // Roll the mastery-scaled proc chance
    if (!roll_chance_i(ctx.triggerAmount))
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }
    ctx.triggered_spell_id = 86392;
    ctx.target = ctx.pVictim;
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// Strikes of Opportunity (Arms Warrior mastery — spell 76838).
// Cased under SPELLFAMILY_ROGUE despite being a warrior
// mastery because mangosthree's DBC reports the spell with
// SpellFamilyName=8 (SPELLFAMILY_ROGUE). The outer
// switch(dummySpell->GetSpellFamilyName()) routes by that
// field, so the case must live wherever the dispatcher
// sends it — diagnosed in-game on 2026-05-17 with a
// temporary [DIAG HDA] log line that printed spellFamily=8
// for spell 76838 every time the handler was entered.
//
// The classification is plausibly Blizzard's choice
// (mechanic-by-family — the proc shape mirrors Main Gauche
// above) but may also be a DBC-extraction quirk. TC's
// canonical proc table lists 76838 with SpellFamilyName=4
// (WARRIOR), but that's a TC-maintained DB row and not a
// direct echo of the client DBC, so it's not conclusive.
// Either way, modifying SpellFamilyName via a runtime
// const_cast would risk corrupting any other spell that
// shares the same SpellClassOptions row, so we follow the
// data instead of patching it.
//
// **Future:** the SD3 mastery migration (see
// MASTERY_SD3_ROADMAP.md at workspace root) replaces this
// family-block dispatch with per-spell AuraScript hooks,
// identical to how TC structures the same handler. Once
// that lands the spell will live in spell_warrior.cpp
// where it semantically belongs, regardless of the DBC
// family-name value.
//
// Mechanics: procs on melee swings and melee abilities
// (PROC_FLAG_SUCCESSFUL_MELEE_HIT | _MELEE_SPELL_HIT) per
// the DBC procFlags=0x14. Triggered spell 76858
// ("Opportunity Strike") deals an extra melee weapon
// hit. Proc chance is the mastery-scaled aura amount
// written by Player::UpdateMasteryAuras on EFFECT_0,
// matching TC's spell_warr_strikes_of_opportunity at
// scripts/Spells/spell_warrior.cpp:828. EFFECT_INDEX_0
// gate handles the DBC's placeholder second SPELL_AURA_-
// DUMMY effect (Main Gauche pattern).
//
// CanProcFrom bypass entry IS needed in Unit::ProcDamage-
// AndSpellFor for 76838: melee ability casts (Mortal
// Strike, Slam, etc.) pass procSpell=m_spellInfo
// (non-NULL), so the upstream class-mask check would
// otherwise drop them. Self-chain via the triggered
// 76858's CAST_END event is caught by the framework-level
// CAST_END suppression added earlier in this series.
}

/// 76838: Strikes of Opportunity: on the first effect, at the aura's chance, triggers 76858 on the victim; fails
/// otherwise
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcRogue76838(DummyAuraProcContext& ctx)
{
    if (ctx.effIndex != EFFECT_INDEX_0)
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }
    if (!roll_chance_i(ctx.triggerAmount))
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }
    ctx.triggered_spell_id = 76858;
    ctx.target = ctx.pVictim;
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
}

/// 20185: absent from the client's spell data: on real damage, the victim casts 20267 on itself for the aura's percent
/// of its maximum health, and the proc ends; fails without damage
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcPaladin20185(DummyAuraProcContext& ctx)
{
    // Judgement of Light
    // only at real damage
    if (!ctx.damage)
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }

    ctx.basepoints[0] = int32(ctx.pVictim->GetMaxHealth() * ctx.triggeredByAura->GetModifier()->m_amount / 100);
    ctx.pVictim->CastCustomSpell(ctx.pVictim, 20267, &ctx.basepoints[0], NULL, NULL, true, NULL, ctx.triggeredByAura);
    return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_OK);
// Judgement of Wisdom
}

/// 20186: absent from the client's spell data: on real damage, a victim with mana casts 20268 on itself for 2 percent
/// of its base mana, and the proc ends; fails without damage
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcPaladin20186(DummyAuraProcContext& ctx)
{
    // only at real damage
    if (!ctx.damage)
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }

    if (ctx.pVictim->GetPowerType() == POWER_MANA)
    {
        // 2% of maximum base mana
        ctx.basepoints[0] = int32(ctx.pVictim->GetCreateMana() * 2 / 100);
        ctx.pVictim->CastCustomSpell(ctx.pVictim, 20268, &ctx.basepoints[0], NULL, NULL, true, NULL, ctx.triggeredByAura);
    }
    return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_OK);
// Heart of the Crusader (Rank 1)
}

/// 20335: absent from the client's spell data: triggers 21183
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcPaladin20335(DummyAuraProcContext& ctx)
{
    ctx.triggered_spell_id = 21183;
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// Heart of the Crusader (Rank 2)
}

/// 20336: absent from the client's spell data: triggers 54498
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcPaladin20336(DummyAuraProcContext& ctx)
{
    ctx.triggered_spell_id = 54498;
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// Heart of the Crusader (Rank 3)
}

/// 20337: absent from the client's spell data: triggers 54499
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcPaladin20337(DummyAuraProcContext& ctx)
{
    ctx.triggered_spell_id = 54499;
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
}

/// one body for 2 labels: 20911: Sanctuary; 25899: absent from the client's spell data: a unit with mana triggers 57319
/// on itself; fails otherwise
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcPaladin20911(DummyAuraProcContext& ctx)
{
    ctx.target = ctx.unit;
    switch (ctx.target->GetPowerType())
    {
        case POWER_MANA:
            ctx.triggered_spell_id = 57319;
            break;
        default:
            return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// Holy Power (Redemption Armor set)
}

/// 28789: Holy Power: triggers a buff by the victim's class (28795, 28793, 28791 or 28790); fails without a victim or
/// for another class
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcPaladin28789(DummyAuraProcContext& ctx)
{
    if (!ctx.pVictim)
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }

    // Set class defined buff
    switch (ctx.pVictim->getClass())
    {
        case CLASS_PALADIN:
        case CLASS_PRIEST:
        case CLASS_SHAMAN:
        case CLASS_DRUID:
            ctx.triggered_spell_id = 28795;     // Increases the friendly target's mana regeneration by $s1 per 5 sec. for $d.
            break;
        case CLASS_MAGE:
        case CLASS_WARLOCK:
            ctx.triggered_spell_id = 28793;     // Increases the friendly target's spell damage and healing by up to $s1 for $d.
            break;
        case CLASS_HUNTER:
        case CLASS_ROGUE:
            ctx.triggered_spell_id = 28791;     // Increases the friendly target's attack power by $s1 for $d.
            break;
        case CLASS_WARRIOR:
            ctx.triggered_spell_id = 28790;     // Increases the friendly target's armor
            break;
        default:
            return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// Spiritual Attunement
}

/// 31785, 33776: absent from the client's spell data: healed by another unit, triggers 31786 on the unit for the aura's
/// percent of the heal, at most its missing health; fails otherwise
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcPaladin31785(DummyAuraProcContext& ctx)
{
    // if healed by another unit (pVictim)
    if (ctx.unit == ctx.pVictim)
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }

    // dont count overhealing
    uint32 diff = ctx.unit->GetMaxHealth() - ctx.unit->GetHealth();
    if (!diff)
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }
    ctx.basepoints[0] = ctx.triggerAmount * (ctx.damage > diff ? diff : ctx.damage) / 100;
    ctx.target = ctx.unit;
    ctx.triggered_spell_id = 31786;
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// Seal of Vengeance (damage calc on apply aura)
}

/// 31801: Seal of Truth: on the first effect, triggers 31803 after a melee hit or 53595; at five stacks of the unit's
/// 31803 on the target, casts 42463 on it; fails on the other effects
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcPaladin31801(DummyAuraProcContext& ctx)
{
    if (ctx.effIndex != EFFECT_INDEX_0)         // effect 1,2 used by seal unleashing code
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }

    // At melee attack or Hammer of the Righteous spell damage considered as melee attack
    if ((ctx.procFlag & PROC_FLAG_SUCCESSFUL_MELEE_HIT) || (ctx.procSpell && ctx.procSpell->ID == 53595))
    {
        ctx.triggered_spell_id = 31803;         // Holy Vengeance
    }

    // Add 5-stack effect from Holy Vengeance
    uint32 stacks = 0;
    AuraList const& auras = ctx.target->GetAurasByType(SPELL_AURA_PERIODIC_DAMAGE);
    for (AuraList::const_iterator itr = auras.begin(); itr != auras.end(); ++itr)
    {
        if (((*itr)->GetId() == 31803) && (*itr)->GetCasterGuid() == ctx.unit->GetObjectGuid())
        {
            stacks = (*itr)->GetStackAmount();
            break;
        }
    }
    if (stacks >= 5)
    {
        ctx.unit->CastSpell(ctx.target, 42463, true, NULL, ctx.triggeredByAura);
    }
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// Judgements of the Wise
}

/// one body for 3 labels: 31876: Communion; 31877: absent from the client's spell data; 31878: Judgements of the Wise:
/// after a Judgement (category 1210), triggers 31930 on the unit and casts 57669 on itself; fails otherwise
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcPaladin31876(DummyAuraProcContext& ctx)
{
    // triggered only at casted Judgement spells, not at additional Judgement effects
    if (!ctx.procSpell || ctx.procSpell->GetCategory() != 1210)
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }

    ctx.target = ctx.unit;
    ctx.triggered_spell_id = 31930;

    // Replenishment
    ctx.unit->CastSpell(ctx.unit, 57669, true, NULL, ctx.triggeredByAura);
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// Paladin Tier 6 Trinket (Ashtongue Talisman of Zeal)
}

/// 40470: Paladin Tier 6 Trinket: triggers 40471 after Flash of Light or Holy Light at 15 percent, 40472 after a
/// Judgement at 50 percent; fails otherwise
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcPaladin40470(DummyAuraProcContext& ctx)
{
    if (!ctx.procSpell)
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }

    float  chance;

    // Flash of light/Holy light
    if (ctx.procClassOptions && ctx.procClassOptions->SpellClassMask & UI64LIT(0x00000000C0000000))
    {
        ctx.triggered_spell_id = 40471;
        chance = 15.0f;
    }
    // Judgement (any)
    else if (GetSpellSpecific(ctx.procSpell->ID) == SPELL_JUDGEMENT)
    {
        ctx.triggered_spell_id = 40472;
        chance = 50.0f;
    }
    else
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }

    if (!roll_chance_f(chance))
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }

    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// Light's Beacon (heal target area aura)
}

/// 53651: Light's Beacon: when the aura's caster is another unit holding the victim's 53563, that caster casts 53652 on
/// itself for the aura's percent of the heal, as from the victim, and the proc ends; fails otherwise
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcPaladin53651(DummyAuraProcContext& ctx)
{
    // not do bonus heal for explicit beacon focus healing
    if (ctx.unit->GetObjectGuid() == ctx.triggeredByAura->GetCasterGuid())
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }

    // beacon
    Unit* beacon = ctx.triggeredByAura->GetCaster();
    if (!beacon)
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }

    // find caster main aura at beacon
    Aura* dummy = NULL;
    Unit::AuraList const& baa = beacon->GetAurasByType(SPELL_AURA_PERIODIC_TRIGGER_SPELL);
    for (Unit::AuraList::const_iterator i = baa.begin(); i != baa.end(); ++i)
    {
        if ((*i)->GetId() == 53563 && (*i)->GetCasterGuid() == ctx.pVictim->GetObjectGuid())
        {
            dummy = (*i);
            break;
        }
    }

    // original heal must be form beacon caster
    if (!dummy)
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }

    ctx.triggered_spell_id = 53652;             // Beacon of Light
    ctx.basepoints[0] = ctx.triggeredByAura->GetModifier()->m_amount * ctx.damage / 100;

    // cast with original caster set but beacon to beacon for apply caster mods and avoid LoS check
    beacon->CastCustomSpell(beacon, ctx.triggered_spell_id, &ctx.basepoints[0], NULL, NULL, true, ctx.castItem, ctx.triggeredByAura, ctx.pVictim->GetObjectGuid());
    return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_OK);
// Seal of Corruption (damage calc on apply aura)
}

/// 53736: absent from the client's spell data: on the first effect, triggers 53742 after a melee hit or 53595; at five
/// stacks of the unit's 53742 on the target, casts 53739 on it; fails on the other effects
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcPaladin53736(DummyAuraProcContext& ctx)
{
    if (ctx.effIndex != EFFECT_INDEX_0)         // effect 1,2 used by seal unleashing code
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }

    // At melee attack or Hammer of the Righteous spell damage considered as melee attack
    if ((ctx.procFlag & PROC_FLAG_SUCCESSFUL_MELEE_HIT) || (ctx.procSpell && ctx.procSpell->ID == 53595))
    {
        ctx.triggered_spell_id = 53742;         // Blood Corruption
    }

    // Add 5-stack effect from Blood Corruption
    uint32 stacks = 0;
    AuraList const& auras = ctx.target->GetAurasByType(SPELL_AURA_PERIODIC_DAMAGE);
    for (AuraList::const_iterator itr = auras.begin(); itr != auras.end(); ++itr)
    {
        if (((*itr)->GetId() == 53742) && (*itr)->GetCasterGuid() == ctx.unit->GetObjectGuid())
        {
            stacks = (*itr)->GetStackAmount();
            break;
        }
    }
    if (stacks >= 5)
    {
        ctx.unit->CastSpell(ctx.target, 53739, true, NULL, ctx.triggeredByAura);
    }
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// Glyph of Holy Light
}

/// 54937: Glyph of Divine Favor: triggers 54968 for the aura's percent of the damage
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcPaladin54937(DummyAuraProcContext& ctx)
{
    ctx.triggered_spell_id = 54968;
    ctx.basepoints[0] = ctx.triggerAmount * ctx.damage / 100;
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// Sacred Shield (buff)
}

/// 58597: absent from the client's spell data: triggers 66922 on the unit for the damage spread over its ticks; fails
/// when 66922 or its first effect is missing
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcPaladin58597(DummyAuraProcContext& ctx)
{
    ctx.triggered_spell_id = 66922;
    SpellEntry const* triggeredEntry = sSpellStore.LookupEntry(ctx.triggered_spell_id);
    if (!triggeredEntry)
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }

    SpellEffectEntry const* spellEffect = triggeredEntry->GetSpellEffect(EFFECT_INDEX_0);
    if (!spellEffect)
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }

    ctx.basepoints[0] = int32(ctx.damage / (GetSpellDuration(triggeredEntry) / spellEffect->EffectAuraPeriod));
    ctx.target = ctx.unit;
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// Sacred Shield (talent rank)
}

/// 53601: absent from the client's spell data: makes the unit the target of the spell data's trigger spell
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcPaladin53601(DummyAuraProcContext& ctx)
{
    // triggered_spell_id in spell data
    ctx.target = ctx.unit;
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// Item - Paladin T8 Holy 2P Bonus
}

/// 64890: Item - Paladin T8 Holy 2P Bonus: triggers 64891 for the aura's percent of the damage, split over its ticks
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcPaladin64890(DummyAuraProcContext& ctx)
{
    ctx.triggered_spell_id = 64891;             // Holy Mending
    ctx.basepoints[0] = int32(ctx.triggerAmount * ctx.damage / 100) / GetSpellAuraMaxTicks(ctx.triggered_spell_id);
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// Anger Capacitor
}

/// 71406, 71545: Anger Capacitor: with a victim, triggers a stack of 71432; at its full stack (one fewer for 71545)
/// removes 71432 instead, casts 71433 or, with an off-hand weapon at half chance, 71434 on the victim, and the proc
/// ends; fails without a victim or 71432
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcPaladin71406(DummyAuraProcContext& ctx)
{
    if (!ctx.pVictim)
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }

    SpellEntry const* mote = sSpellStore.LookupEntry(71432);
    if (!mote)
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }
    uint32 maxStack = mote->GetStackAmount() - (ctx.dummySpell->ID == 71545 ? 1 : 0);

    SpellAuraHolder* aurHolder = ctx.unit->GetSpellAuraHolder(71432);
    if (aurHolder && uint32(aurHolder->GetStackAmount() + 1) >= maxStack)
    {
        ctx.unit->RemoveAurasDueToSpell(71432);       // Mote of Anger

        // Manifest Anger (main hand/off hand)
        ctx.unit->CastSpell(ctx.pVictim, !ctx.unit->haveOffhandWeapon() || roll_chance_i(50) ? 71433 : 71434, true);
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_OK);
    }
    else
    {
        ctx.triggered_spell_id = 71432;
    }

    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// Heartpierce, Item - Icecrown 25 Normal Dagger Proc
}

/// 71880: Item - Icecrown 25 Normal Dagger Proc: a player triggers 71882, 71883 or 71881 by its power (energy, rage or
/// mana); fails otherwise
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcPaladin71880(DummyAuraProcContext& ctx)
{
    if (ctx.unit->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }

    switch (ctx.unit->GetPowerType())
    {
        case POWER_ENERGY: ctx.triggered_spell_id = 71882; break;
        case POWER_RAGE:   ctx.triggered_spell_id = 71883; break;
        case POWER_MANA:   ctx.triggered_spell_id = 71881; break;
        default:
            return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// Illuminated Healing (Holy Paladin mastery — spell 76669)
// On direct heals, apply an absorb shield (86273) on the
// heal target with amount = mastery% (triggerAmount) of
// the heal value. Stacks with existing 86273 aura from
// the same caster up to 33% of caster's max HP. Mirrors
// TC-Preservation's spell_pal_illuminated_healing at
// scripts/Spells/spell_paladin.cpp:907-941.
//
// EFFECT_INDEX_0 gate handles any DBC placeholder on a
// later SPELL_AURA_DUMMY effect (Main Gauche pattern).
// CanProcFrom bypass entry is needed because heal-cast
// procs pass procSpell != NULL (the heal spell), so the
// upstream class-mask check would otherwise drop them.
}

/// 76669: Illuminated Healing: on the first effect, for a heal, the aura's percent of it is added to the victim's 86273
/// shield from the unit, which is refreshed, and the proc ends; else triggers 86273 on the victim for that amount; the
/// shield is at most 33 percent of the unit's maximum health; fails otherwise
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcPaladin76669(DummyAuraProcContext& ctx)
{
    if (ctx.effIndex != EFFECT_INDEX_0)
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }
    if (!ctx.pVictim)
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }
    if (ctx.damage == 0)
    {
        // damage param carries the heal amount on heal procs
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }

    int32 shieldAmount = int32(uint64(ctx.damage) * uint32(ctx.triggerAmount) / 100);
    if (shieldAmount <= 0)
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }

    int32 cap = int32(ctx.unit->GetMaxHealth() * 33 / 100);

    // Stack with existing 86273 aura from this caster on target
    if (SpellAuraHolder* existing = ctx.pVictim->GetSpellAuraHolder(86273, ctx.unit->GetObjectGuid()))
    {
        if (Aura* eff = existing->GetAuraByEffectIndex(EFFECT_INDEX_0))
        {
            int32 newAmount = std::min(cap, eff->GetModifier()->m_amount + shieldAmount);
            eff->GetModifier()->m_amount = newAmount;
            existing->RefreshHolder();
        }
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_OK);
    }

    // No existing aura — cast 86273 fresh with shield as basepoints
    ctx.basepoints[0] = std::min(cap, shieldAmount);
    ctx.triggered_spell_id = 86273;
    ctx.target = ctx.pVictim;
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// Heartpierce, Item - Icecrown 25 Heroic Dagger Proc
}

/// 71892: Item - Icecrown 25 Heroic Dagger Proc: a player triggers 71887, 71886 or 71888 by its power (energy, rage or
/// mana); fails otherwise
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcPaladin71892(DummyAuraProcContext& ctx)
{
    if (ctx.unit->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }

    switch (ctx.unit->GetPowerType())
    {
        case POWER_ENERGY: ctx.triggered_spell_id = 71887; break;
        case POWER_RAGE:   ctx.triggered_spell_id = 71886; break;
        case POWER_MANA:   ctx.triggered_spell_id = 71888; break;
        default:
            return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
}

/// 28823: Totemic Power: triggers a buff by the victim's class (28824, 28825, 28826 or 28827); fails without a victim
/// or for another class
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcShaman28823(DummyAuraProcContext& ctx)
{
    // Totemic Power (The Earthshatterer set)
    if (!ctx.pVictim)
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }

    // Set class defined buff
    switch (ctx.pVictim->getClass())
    {
        case CLASS_PALADIN:
        case CLASS_PRIEST:
        case CLASS_SHAMAN:
        case CLASS_DRUID:
            ctx.triggered_spell_id = 28824;     // Increases the friendly target's mana regeneration by $s1 per 5 sec. for $d.
            break;
        case CLASS_MAGE:
        case CLASS_WARLOCK:
            ctx.triggered_spell_id = 28825;     // Increases the friendly target's spell damage and healing by up to $s1 for $d.
            break;
        case CLASS_HUNTER:
        case CLASS_ROGUE:
            ctx.triggered_spell_id = 28826;     // Increases the friendly target's attack power by $s1 for $d.
            break;
        case CLASS_WARRIOR:
            ctx.triggered_spell_id = 28827;     // Increases the friendly target's armor
            break;
        default:
            return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// Lesser Healing Wave (Totem of Flowing Water Relic)
}

/// 28849: Lesser Healing Wave: triggers 28850 on the unit
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcShaman28849(DummyAuraProcContext& ctx)
{
    ctx.target = ctx.unit;
    ctx.triggered_spell_id = 28850;
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// Windfury Weapon (Passive) 1-5 Ranks
}

/// 33757: Windfury Weapon (Passive): a player's equipped weapon off the aura's cooldown adds that cooldown and attacks
/// the victim twice with 33750 (off hand) or 25504 (main hand) for the enchantment rank's attack power, and the proc
/// ends; fails otherwise
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcShaman33757(DummyAuraProcContext& ctx)
{
    if (ctx.unit->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }

    if (!ctx.castItem || !ctx.castItem->IsEquipped())
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }

    // custom cooldown processing case
    if (ctx.cooldown && ctx.m_spellCooldownMgr.HasSpellCooldown(ctx.dummySpell->ID, time(NULL)))
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }

    // Now amount of extra power stored in 1 effect of Enchant spell
    // Get it by item enchant id
    uint32 spellId;
    switch (ctx.castItem->GetEnchantmentId(EnchantmentSlot(TEMP_ENCHANTMENT_SLOT)))
    {
        case 283: spellId =  8232; break;   // 1 Rank
        case 284: spellId =  8235; break;   // 2 Rank
        case 525: spellId = 10486; break;   // 3 Rank
        case 1669: spellId = 16362; break;  // 4 Rank
        case 2636: spellId = 25505; break;  // 5 Rank
        case 3785: spellId = 58801; break;  // 6 Rank
        case 3786: spellId = 58803; break;  // 7 Rank
        case 3787: spellId = 58804; break;  // 8 Rank
        default:
        {
            sLog.outError("Unit::HandleDummyAuraProc: non handled item enchantment (rank?) %u for spell id: %u (Windfury)",
                          ctx.castItem->GetEnchantmentId(EnchantmentSlot(TEMP_ENCHANTMENT_SLOT)), ctx.dummySpell->ID);
            return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
        }
    }

    SpellEntry const* windfurySpellEntry = sSpellStore.LookupEntry(spellId);
    if (!windfurySpellEntry)
    {
        sLog.outError("Unit::HandleDummyAuraProc: nonexistent spell id: %u (Windfury)", spellId);
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }

    int32 extra_attack_power = ctx.unit->CalculateSpellDamage(ctx.pVictim, windfurySpellEntry, EFFECT_INDEX_1);

    // Totem of Splintering
    if (Aura* aura = ctx.unit->GetAura(60764, EFFECT_INDEX_0))
    {
        extra_attack_power += aura->GetModifier()->m_amount;
    }

    // Off-Hand case
    if (ctx.castItem->GetSlot() == EQUIPMENT_SLOT_OFFHAND)
    {
        // Value gained from additional AP
        ctx.basepoints[0] = int32(extra_attack_power / 14.0f * ctx.unit->GetAttackTime(OFF_ATTACK) / 1000 / 2);
        ctx.triggered_spell_id = 33750;
    }
    // Main-Hand case
    else
    {
        // Value gained from additional AP
        ctx.basepoints[0] = int32(extra_attack_power / 14.0f * ctx.unit->GetAttackTime(BASE_ATTACK) / 1000);
        ctx.triggered_spell_id = 25504;
    }

    // apply cooldown before cast to prevent processing itself
    if (ctx.cooldown)
    {
        ctx.m_spellCooldownMgr.AddSpellCooldown(ctx.dummySpell->ID, 0, time(NULL) + ctx.cooldown);
    }

    // Attack Twice
    for (uint32 i = 0; i < 2; ++i)
    {
        ctx.unit->CastCustomSpell(ctx.pVictim, ctx.triggered_spell_id, &ctx.basepoints[0], NULL, NULL, true, ctx.castItem, ctx.triggeredByAura);
    }

    return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_OK);
// Shaman Tier 6 Trinket
}

/// 40463: Shaman Tier 6 Trinket: triggers on the unit 40465 after Lightning Bolt (15 percent) or Lesser Healing Wave
/// (10 percent), 40466 after Stormstrike (50 percent); fails otherwise
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcShaman40463(DummyAuraProcContext& ctx)
{
    if (!ctx.procSpell)
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }

    float chance;
    if (ctx.procClassOptions && ctx.procClassOptions->SpellClassMask & UI64LIT(0x0000000000000001))
    {
        ctx.triggered_spell_id = 40465;         // Lightning Bolt
        chance = 15.0f;
    }
    else if (ctx.procClassOptions && ctx.procClassOptions->SpellClassMask & UI64LIT(0x0000000000000080))
    {
        ctx.triggered_spell_id = 40465;         // Lesser Healing Wave
        chance = 10.0f;
    }
    else if (ctx.procClassOptions && ctx.procClassOptions->SpellClassMask & UI64LIT(0x0000001000000000))
    {
        ctx.triggered_spell_id = 40466;         // Stormstrike
        chance = 50.0f;
    }
    else
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }

    if (!roll_chance_f(chance))
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }

    ctx.target = ctx.unit;
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// Glyph of Healing Wave
}

/// 55440: Glyph of Healing Wave: unless the unit healed itself, triggers 55533 on the unit for the aura's percent of
/// the heal
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcShaman55440(DummyAuraProcContext& ctx)
{
    // Not proc from self heals
    if (ctx.unit == ctx.pVictim)
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }
    ctx.basepoints[0] = ctx.triggerAmount * ctx.damage / 100;
    ctx.target = ctx.unit;
    ctx.triggered_spell_id = 55533;
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// Spirit Hunt
}

/// 58877: Spirit Hunt: triggers 58879 on the unit's owner for the aura's percent of the damage; fails without an owner
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcShaman58877(DummyAuraProcContext& ctx)
{
    // Cast on owner
    ctx.target = ctx.unit->GetOwner();
    if (!ctx.target)
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }
    ctx.basepoints[0] = ctx.triggerAmount * ctx.damage / 100;
    ctx.triggered_spell_id = 58879;
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// Glyph of Totem of Wrath
}

/// 63280: Glyph of Shamanistic Rage: triggers 63283 on the unit for the aura's percent of the healing aura its fire
/// totem holds; fails without the totem or that aura
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcShaman63280(DummyAuraProcContext& ctx)
{
    Totem* totem = ctx.unit->GetTotem(TOTEM_SLOT_FIRE);
    if (!totem)
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }

    // find totem aura bonus
    AuraList const& spellPower = totem->GetAurasByType(SPELL_AURA_NONE);
    for (AuraList::const_iterator i = spellPower.begin(); i != spellPower.end(); ++i)
    {
        // select proper aura for format aura type in spell proto
        SpellEffectEntry const* itrSpellEffect = (*i)->GetSpellProto()->GetSpellEffect((*i)->GetEffIndex());
        SpellClassOptionsEntry const* itrClassOptions = (*i)->GetSpellProto()->GetSpellClassOptions();
        if ((*i)->GetTarget()==totem && itrSpellEffect && itrSpellEffect->EffectAura == SPELL_AURA_MOD_HEALING_DONE &&
            itrClassOptions && itrClassOptions->SpellClassSet == SPELLFAMILY_SHAMAN && itrClassOptions->SpellClassMask & UI64LIT(0x0000000004000000))
        {
            ctx.basepoints[0] = ctx.triggerAmount * (*i)->GetModifier()->m_amount / 100;
            break;
        }
    }

    if (!ctx.basepoints[0])
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }

    ctx.basepoints[1] = ctx.basepoints[0];
    ctx.triggered_spell_id = 63283;             // Totem of Wrath, caster bonus
    ctx.target = ctx.unit;
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// Item - Shaman T8 Elemental 4P Bonus
}

/// 64928: Item - Shaman T8 Elemental 4P Bonus: triggers 64930 for the aura's percent of the damage, split over its
/// ticks
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcShaman64928(DummyAuraProcContext& ctx)
{
    ctx.triggered_spell_id = 64930;            // Electrified
    ctx.basepoints[0] = int32(ctx.triggerAmount * ctx.damage / 100) / GetSpellAuraMaxTicks(ctx.triggered_spell_id);
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// Item - Shaman T9 Elemental 4P Bonus (Lava Burst)
}

/// 67228: Item - Shaman T9 Elemental 4P Bonus (Lava Burst): triggers 71824 for the aura's percent of the damage, split
/// over its ticks
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcShaman67228(DummyAuraProcContext& ctx)
{
    ctx.triggered_spell_id = 71824;             // Lava Burst
    ctx.basepoints[0] = int32(ctx.triggerAmount * ctx.damage / 100) / GetSpellAuraMaxTicks(ctx.triggered_spell_id);
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// Item - Shaman T10 Restoration 4P Bonus
}

/// 70808: Item - Shaman T10 Restoration 4P Bonus: triggers 70809 for the aura's percent of the damage, split over its
/// ticks
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcShaman70808(DummyAuraProcContext& ctx)
{
    ctx.triggered_spell_id = 70809;             // Chained Heal
    ctx.basepoints[0] = int32(ctx.triggerAmount * ctx.damage / 100) / GetSpellAuraMaxTicks(ctx.triggered_spell_id);
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
}

/// 13877, 33735: Blade Flurry: triggers 22482 for the damage on a random hostile unit in melee range other than the
/// victim; fails without one
static SpellHandlerOutcome<SpellAuraProcResult> HasteAuraProcRogue13877(HasteAuraProcContext& ctx)
{
        // Blade Flurry
    ctx.target = ctx.unit->SelectRandomUnfriendlyTarget(ctx.pVictim);
    if (!ctx.target)
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }
    ctx.basepoints0 = ctx.damage;
    ctx.triggered_spell_id = 22482;
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
}

/// 54646: Focus Magic: triggers 54648 on the aura's caster; fails without a caster
static SpellHandlerOutcome<SpellAuraProcResult> SpellCritChanceAuraProcMage54646(SpellCritChanceAuraProcContext& ctx)
{
        // Focus Magic
    Unit* caster = ctx.triggeredByAura->GetCaster();
    if (!caster)
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }

    ctx.triggered_spell_id = 54648;
    ctx.target = caster;
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
}

/// 23780: Aegis of Preservation: triggers 23781
static SpellHandlerOutcome<SpellAuraProcResult> ProcTriggerSpellAuraProcGeneric23780(ProcTriggerSpellAuraProcContext& ctx)
{
    // case 191:                            // Elemental Response
    //    switch (procSpell->School)
    //    {
    //        case SPELL_SCHOOL_FIRE:  trigger_spell_id = 34192; break;
    //        case SPELL_SCHOOL_FROST: trigger_spell_id = 34193; break;
    //        case SPELL_SCHOOL_ARCANE:trigger_spell_id = 34194; break;
    //        case SPELL_SCHOOL_NATURE:trigger_spell_id = 34195; break;
    //        case SPELL_SCHOOL_SHADOW:trigger_spell_id = 34196; break;
    //        case SPELL_SCHOOL_HOLY:  trigger_spell_id = 34197; break;
    //        case SPELL_SCHOOL_NORMAL:trigger_spell_id = 34198; break;
    //    }
    //    break;
    // case 5301:  break;                   // Defensive State (DND)
    // case 7137:  break:                   // Shadow Charge (Rank 1)
    // case 7377:  break:                   // Take Immune Periodic Damage <Not Working>
    // case 13358: break;                   // Defensive State (DND)
    // case 16092: break;                   // Defensive State (DND)
    // case 18943: break;                   // Double Attack
    // case 19194: break;                   // Double Attack
    // case 19817: break;                   // Double Attack
    // case 19818: break;                   // Double Attack
    // case 22835: break;                   // Drunken Rage
    //    trigger_spell_id = 14822; break;
    ctx.trigger_spell_id = 23781;
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// case 24949: break;                   // Defensive State 2 (DND)
}

/// one body for 3 labels: 27522, 40336: Mana Drain Trigger; 46939: Black Bow of the Betrayer: a living unit casts 29471
/// on itself and 27526 on a living victim, and the proc ends
static SpellHandlerOutcome<SpellAuraProcResult> ProcTriggerSpellAuraProcGeneric27522(ProcTriggerSpellAuraProcContext& ctx)
{
    // On successful melee or ranged attack gain 8 mana and if possible drain 8 mana from the target.
    if (ctx.unit->IsAlive())
    {
        ctx.unit->CastSpell(ctx.unit, 29471, true, ctx.castItem, ctx.triggeredByAura);
    }
    if (ctx.pVictim && ctx.pVictim->IsAlive())
    {
        ctx.unit->CastSpell(ctx.pVictim, 27526, true, ctx.castItem, ctx.triggeredByAura);
    }
    return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_OK);
}

/// 31255: absent from the client's spell data: on a victim at or below a fifth of its maximum health, triggers 22588 on
/// the unit; fails otherwise
static SpellHandlerOutcome<SpellAuraProcResult> ProcTriggerSpellAuraProcGeneric31255(ProcTriggerSpellAuraProcContext& ctx)
{
    // whenever you deal damage to a target who is below 20% health.
    if (ctx.pVictim->GetHealth() > ctx.pVictim->GetMaxHealth() / 5)
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }

    ctx.target = ctx.unit;
    ctx.trigger_spell_id = 22588;
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// case 33207: break;                       // Gossip NPC Periodic - Fidget
}

/// 33896: Desperate Defense: triggers 33898
static SpellHandlerOutcome<SpellAuraProcResult> ProcTriggerSpellAuraProcGeneric33896(ProcTriggerSpellAuraProcContext& ctx)
{
    ctx.trigger_spell_id = 33898;
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// case 34082: break;                   // Advantaged State (DND)
// case 34783: break:                   // Spell Reflection
// case 35205: break:                   // Vanish
// case 35321: break;                   // Gushing Wound
// case 36096: break:                   // Spell Reflection
// case 36207: break:                   // Steal Weapon
// case 36576: break:                   // Shaleskin (Shaleskin Flayer, Shaleskin Ripper) 30023 trigger
// case 37030: break;                   // Chaotic Temperament
// case 38363: break;                   // Gushing Wound
// case 39215: break;                   // Gushing Wound
// case 40250: break;                   // Improved Duration
// case 40329: break;                   // Demo Shout Sensor
// case 40364: break;                   // Entangling Roots Sensor
// case 41054: break;                   // Copy Weapon
//    trigger_spell_id = 41055; break;
// case 41248: break;                   // Consuming Strikes
//    trigger_spell_id = 41249; break;
// case 42730: break:                   // Woe Strike
// case 43453: break:                   // Rune Ward
// case 43504: break;                   // Alterac Valley OnKill Proc Aura
// case 44326: break:                   // Pure Energy Passive
// case 44526: break;                   // Hate Monster (Spar) (30 sec)
// case 44527: break;                   // Hate Monster (Spar Buddy) (30 sec)
// case 44819: break;                   // Hate Monster (Spar Buddy) (>30% Health)
// case 44820: break;                   // Hate Monster (Spar) (<30%)
}

/// 45057: Evasive Maneuvers: fails unless the damage leaves the unit below the aura's percent of its maximum health;
/// the trigger spell is the spell data's
static SpellHandlerOutcome<SpellAuraProcResult> ProcTriggerSpellAuraProcGeneric45057(ProcTriggerSpellAuraProcContext& ctx)
{
    // reduce you below $s1% health (in fact in this specific case can proc from any attack while health in result less $s1%)
    if (int32(ctx.unit->GetHealth()) - int32(ctx.damage) >= int32(ctx.unit->GetMaxHealth() * ctx.triggerAmount / 100))
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// case 45903: break:                   // Offensive State
// case 46146: break:                   // [PH] Ahune  Spanky Hands
}

/// 43820: Charm of the Witch Doctor: makes the victim the target, with base points of the aura's second effect value as
/// a percent of the victim's base health
static SpellHandlerOutcome<SpellAuraProcResult> ProcTriggerSpellAuraProcGeneric43820(ProcTriggerSpellAuraProcContext& ctx)
{
    // Pct value stored in dummy
    ctx.basepoints[0] = ctx.pVictim->GetCreateHealth() * ctx.auraSpellInfo->CalculateSimpleValue(EFFECT_INDEX_1) / 100;
    ctx.target = ctx.pVictim;
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// case 45205: break;                   // Copy Offhand Weapon
// case 45343: break;                   // Dark Flame Aura
// case 47300: break;                   // Dark Flame Aura
// case 48876: break;                   // Beast's Mark
//    trigger_spell_id = 48877; break;
// case 49059: break;                   // Horde, Hate Monster (Spar Buddy) (>30% Health)
// case 50051: break;                   // Ethereal Pet Aura
// case 50689: break;                   // Blood Presence (Rank 1)
// case 50844: break;                   // Blood Mirror
// case 52856: break;                   // Charge
// case 54072: break;                   // Knockback Ball Passive
// case 54476: break;                   // Blood Presence
// case 54775: break;                   // Abandon Vehicle on Poly
}

/// 57345: Darkmoon Card: Greatness: triggers 60229, 60233, 60234 or 60235 by the highest of the unit's strength,
/// agility, intellect and spirit
static SpellHandlerOutcome<SpellAuraProcResult> ProcTriggerSpellAuraProcGeneric57345(ProcTriggerSpellAuraProcContext& ctx)
{
    float stat = 0.0f;
    // strength
    if (ctx.unit->GetStat(STAT_STRENGTH) > stat)
    {
        ctx.trigger_spell_id = 60229; stat = ctx.unit->GetStat(STAT_STRENGTH);
    }
    // agility
    if (ctx.unit->GetStat(STAT_AGILITY)  > stat)
    {
        ctx.trigger_spell_id = 60233; stat = ctx.unit->GetStat(STAT_AGILITY);
    }
    // intellect
    if (ctx.unit->GetStat(STAT_INTELLECT) > stat)
    {
        ctx.trigger_spell_id = 60234; stat = ctx.unit->GetStat(STAT_INTELLECT);
    }
    // spirit
    if (ctx.unit->GetStat(STAT_SPIRIT)   > stat)
    {
        ctx.trigger_spell_id = 60235;
    }
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// case 55580: break:                       // Mana Link
// case 57587: break:                       // Steal Ranged ()
// case 57594: break;                       // Copy Ranged Weapon
// case 59237: break;                       // Beast's Mark
//    trigger_spell_id = 59233; break;
// case 59288: break;                       // Infra-Green Shield
// case 59532: break;                       // Abandon Passengers on Poly
// case 59735: break:                       // Woe Strike
}

/// 64148: Diminsh Power: the aura's caster stops its spells other than melee, and the proc ends
static SpellHandlerOutcome<SpellAuraProcResult> ProcTriggerSpellAuraProcGeneric64148(ProcTriggerSpellAuraProcContext& ctx)
{
    if (Unit* caster = ctx.triggeredByAura->GetCaster())
    {
        caster->InterruptNonMeleeSpells(false);
    }
    return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_OK);
}

/// 64415: Val'anyr Hammer of Ancient Kings - Equip Effect: fails after a proc spell that is not positive; the trigger
/// spell is the spell data's
static SpellHandlerOutcome<SpellAuraProcResult> ProcTriggerSpellAuraProcGeneric64415(ProcTriggerSpellAuraProcContext& ctx)
{
    // for DOT procs
    if (!IsPositiveSpell(ctx.procSpell->ID))
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
}

/// 64440: Blade Warding: triggers 64442 with base points of its first effect value times the aura's stack
static SpellHandlerOutcome<SpellAuraProcResult> ProcTriggerSpellAuraProcGeneric64440(ProcTriggerSpellAuraProcContext& ctx)
{
    ctx.trigger_spell_id = 64442;

    // need scale damage base at stack size
    if (SpellEntry const* trigEntry = sSpellStore.LookupEntry(ctx.trigger_spell_id))
    {
        ctx.basepoints[EFFECT_INDEX_0] = trigEntry->CalculateSimpleValue(EFFECT_INDEX_0) * ctx.triggeredByAura->GetStackAmount();
    }

    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
}

/// 64568: Blood Reserve: when the damage takes the unit from at least 35 percent of its maximum health to below it,
/// triggers 64569 with base points of its first effect value times the aura's stack; fails otherwise
static SpellHandlerOutcome<SpellAuraProcResult> ProcTriggerSpellAuraProcGeneric64568(ProcTriggerSpellAuraProcContext& ctx)
{
    // When your health drops below 35% ....
    int32 health35 = int32(ctx.unit->GetMaxHealth() * 35 / 100);
    if (int32(ctx.unit->GetHealth()) - int32(ctx.damage) >= health35 || int32(ctx.unit->GetHealth()) < health35)
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }

    ctx.trigger_spell_id = 64569;

    // need scale damage base at stack size
    if (SpellEntry const* trigEntry = sSpellStore.LookupEntry(ctx.trigger_spell_id))
    {
        ctx.basepoints[EFFECT_INDEX_0] = trigEntry->CalculateSimpleValue(EFFECT_INDEX_0) * ctx.triggeredByAura->GetStackAmount();
    }

    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
}

/// 67702: Item - Coliseum 25 Normal Melee Trinket: triggers 67708 or 67703 by the higher of the unit's strength and
/// agility
static SpellHandlerOutcome<SpellAuraProcResult> ProcTriggerSpellAuraProcGeneric67702(ProcTriggerSpellAuraProcContext& ctx)
{
    float stat = 0.0f;
    // strength
    if (ctx.unit->GetStat(STAT_STRENGTH) > stat)
    {
        ctx.trigger_spell_id = 67708; stat = ctx.unit->GetStat(STAT_STRENGTH);
    }
    // agility
    if (ctx.unit->GetStat(STAT_AGILITY)  > stat)
    {
        ctx.trigger_spell_id = 67703;
    }
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
}

/// 67771: Item - Coliseum 25 Heroic Melee Trinket: triggers 67773 or 67772 by the higher of the unit's strength and
/// agility
static SpellHandlerOutcome<SpellAuraProcResult> ProcTriggerSpellAuraProcGeneric67771(ProcTriggerSpellAuraProcContext& ctx)
{
    float stat = 0.0f;
    // strength
    if (ctx.unit->GetStat(STAT_STRENGTH) > stat)
    {
        ctx.trigger_spell_id = 67773; stat = ctx.unit->GetStat(STAT_STRENGTH);
    }
    // agility
    if (ctx.unit->GetStat(STAT_AGILITY)  > stat)
    {
        ctx.trigger_spell_id = 67772;
    }
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
}

/// 69023: Mirrored Soul: the unit casts 69034 on the aura's caster for 45 percent of the damage, and the proc ends
static SpellHandlerOutcome<SpellAuraProcResult> ProcTriggerSpellAuraProcGeneric69023(ProcTriggerSpellAuraProcContext& ctx)
{
    int32 basepoints2 = (int32)(ctx.damage * 0.45f);
    if (Unit* caster = ctx.triggeredByAura->GetCaster())
        // Actually this spell should be sent with SMSG_SPELL_START
        ctx.unit->CastCustomSpell(caster, 69034, &basepoints2, NULL, NULL, true, NULL, ctx.triggeredByAura, ctx.unit->GetObjectGuid());

    return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_OK);
}

/// one body for 4 labels: 7099, 39647: Curse of Mending; 29494: Temptation; 20233: absent from the client's spell data:
/// makes the victim the target
static SpellHandlerOutcome<SpellAuraProcResult> ProcTriggerSpellAuraProcTriggered7099(ProcTriggerSpellAuraProcContext& ctx)
{
    // Cast positive spell on enemy target
    ctx.target = ctx.pVictim;
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// Combo points add triggers (need add combopoint only for main target, and after possible combopoints reset)
}

/// 15250: Setup: fails unless the victim is the unit's own victim
static SpellHandlerOutcome<SpellAuraProcResult> ProcTriggerSpellAuraProcTriggered15250(ProcTriggerSpellAuraProcContext& ctx)
{
    if (!ctx.pVictim || ctx.pVictim != ctx.unit->getVictim())  // applied only for main target
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();                                   // continue normal case
// Finishing moves that add combo points
}

/// one body for 3 labels: 14189: Seal Fate; 14157: Ruthlessness; 70802: Mayhem: adds the spell to the unit's current
/// spell as a triggered spell, and the proc ends; fails without a current spell
static SpellHandlerOutcome<SpellAuraProcResult> ProcTriggerSpellAuraProcTriggered14189(ProcTriggerSpellAuraProcContext& ctx)
{
    // Need add combopoint AFTER finishing move (or they get dropped in finish phase)
    if (Spell* spell = ctx.unit->GetCurrentSpell(CURRENT_GENERIC_SPELL))
    {
        spell->AddTriggeredSpell(ctx.trigger_spell_id);
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_OK);
    }
    return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
// Bloodthirst (($m/100)% of max health)
}

/// 23880: Bloodthirst: base points of the aura's percent of the unit's maximum health
static SpellHandlerOutcome<SpellAuraProcResult> ProcTriggerSpellAuraProcTriggered23880(ProcTriggerSpellAuraProcContext& ctx)
{
    ctx.basepoints[0] = int32(ctx.unit->GetMaxHealth() * ctx.triggerAmount / 100);
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// Shamanistic Rage triggered spell
}

/// 30824: absent from the client's spell data: base points of the aura's percent of the unit's attack power
static SpellHandlerOutcome<SpellAuraProcResult> ProcTriggerSpellAuraProcTriggered30824(ProcTriggerSpellAuraProcContext& ctx)
{
    ctx.basepoints[0] = int32(ctx.unit->GetTotalAttackPowerValue(BASE_ATTACK) * ctx.triggerAmount / 100);
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// Enlightenment (trigger only from mana cost spells)
}

/// 35095: Enlightenment: fails unless the proc spell costs mana
static SpellHandlerOutcome<SpellAuraProcResult> ProcTriggerSpellAuraProcTriggered35095(ProcTriggerSpellAuraProcContext& ctx)
{
    SpellPowerEntry const* spellPower = ctx.procSpell->GetSpellPower();
    if (!spellPower || !ctx.procSpell || ctx.procSpell->PowerType!=POWER_MANA || spellPower->ManaCost==0 && spellPower->PowerCost==0 && spellPower->ManaCostPerLevel==0)
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// Demonic Pact
}

/// 48090: absent from the client's spell data: for a player owner, the first two base points are the percent its talent
/// dummy aura (icon 3220) holds of the owner's spell power, less the owner's own 48090 bonus; fails without a player
/// owner or that power
static SpellHandlerOutcome<SpellAuraProcResult> ProcTriggerSpellAuraProcTriggered48090(ProcTriggerSpellAuraProcContext& ctx)
{
    // As the spell is proced from pet's attack - find owner
    Unit* owner = ctx.unit->GetOwner();
    if (!owner || owner->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }

    // This spell doesn't stack, but refreshes duration. So we receive current bonuses to minus them later.
    int32 curBonus = 0;
    if (Aura* aur = owner->GetAura(48090, EFFECT_INDEX_0))
    {
        curBonus = aur->GetModifier()->m_amount;
    }
    int32 spellDamage  = owner->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_MAGIC) - curBonus;
    if (spellDamage <= 0)
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }

    // percent stored in owner talent dummy
    AuraList const& dummyAuras = owner->GetAurasByType(SPELL_AURA_DUMMY);
    for (AuraList::const_iterator i = dummyAuras.begin(); i != dummyAuras.end(); ++i)
    {
        if ((*i)->GetSpellProto()->SpellIconID == 3220)
        {
            ctx.basepoints[0] = ctx.basepoints[1] = int32(spellDamage * (*i)->GetModifier()->m_amount / 100);
            break;
        }
    }
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// Sword and Board
}

/// 50227: Sword and Board: ends a player's category 1209 cooldown
static SpellHandlerOutcome<SpellAuraProcResult> ProcTriggerSpellAuraProcTriggered50227(ProcTriggerSpellAuraProcContext& ctx)
{
    // Remove cooldown on Shield Slam
    if (ctx.unit->GetTypeId() == TYPEID_PLAYER)
    {
        ctx.unit->RemoveSpellCategoryCooldown(1209, true);
    }
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// Maelstrom Weapon
}

/// 53817: Maelstrom Weapon: fails unless a roll at 20 percent per rank of the aura passes
static SpellHandlerOutcome<SpellAuraProcResult> ProcTriggerSpellAuraProcTriggered53817(ProcTriggerSpellAuraProcContext& ctx)
{
    // have rank dependent proc chance, ignore too often cases
    // PPM = 2.5 * (rank of talent),
    uint32 rank = sSpellMgr.GetSpellRank(ctx.auraSpellInfo->ID);
    // 5 rank -> 100% 4 rank -> 80% and etc from full rate
    if (!roll_chance_i(20 * rank))
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// Brain Freeze
}

/// 57761: Brain Freeze: fails without a proc spell, or after a mage proc spell of class mask 0x80 when the unit has no
/// override script aura 836, 988 or 989
static SpellHandlerOutcome<SpellAuraProcResult> ProcTriggerSpellAuraProcTriggered57761(ProcTriggerSpellAuraProcContext& ctx)
{
    if (!ctx.procSpell)
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }
    // For trigger from Blizzard need exist Improved Blizzard
    if (ctx.procClassOptions && ctx.procClassOptions->SpellClassSet==SPELLFAMILY_MAGE && (ctx.procClassOptions->SpellClassMask & UI64LIT(0x0000000000000080)))
    {
        bool found = false;
        AuraList const& mOverrideClassScript = ctx.unit->GetAurasByType(SPELL_AURA_OVERRIDE_CLASS_SCRIPTS);
        for (AuraList::const_iterator i = mOverrideClassScript.begin(); i != mOverrideClassScript.end(); ++i)
        {
            int32 script = (*i)->GetModifier()->m_miscvalue;
            if (script == 836 || script == 988 || script == 989)
            {
                found = true;
                break;
            }
        }
        if (!found)
        {
            return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
        }
    }
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// Astral Shift
}

/// 52179: Astral Shift: fails unless a proc spell with a stun, fear or silence mechanic hit or crit another unit
static SpellHandlerOutcome<SpellAuraProcResult> ProcTriggerSpellAuraProcTriggered52179(ProcTriggerSpellAuraProcContext& ctx)
{
    if (ctx.procSpell == 0 || !(ctx.procEx & (PROC_EX_NORMAL_HIT | PROC_EX_CRITICAL_HIT)) || ctx.unit == ctx.pVictim)
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }

    // Need stun, fear or silence mechanic
    if (!(GetAllSpellMechanicMask(ctx.procSpell) & IMMUNE_TO_SILENCE_AND_STUN_AND_FEAR_MASK))
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// Burning Determination
}

/// 54748: absent from the client's spell data: fails unless a proc spell has an interrupt or silence mechanic
static SpellHandlerOutcome<SpellAuraProcResult> ProcTriggerSpellAuraProcTriggered54748(ProcTriggerSpellAuraProcContext& ctx)
{
    if (!ctx.procSpell)
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }
    // Need Interrupt or Silenced mechanic
    if (!(GetAllSpellMechanicMask(ctx.procSpell) & IMMUNE_TO_INTERRUPT_AND_SILENCE_MASK))
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// Lock and Load
}

/// 56453: Lock and Load: fails unless the proc is a trap's activation and a roll at the aura's chance passes
static SpellHandlerOutcome<SpellAuraProcResult> ProcTriggerSpellAuraProcTriggered56453(ProcTriggerSpellAuraProcContext& ctx)
{
    // Proc only from trap activation (from periodic proc another aura of this spell)
    if (!(ctx.procFlags & PROC_FLAG_ON_TRAP_ACTIVATION) || !roll_chance_i(ctx.triggerAmount))
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// Freezing Fog (Rime triggered)
}

/// 59052: Freezing Fog: ends a player's category 1248 cooldown
static SpellHandlerOutcome<SpellAuraProcResult> ProcTriggerSpellAuraProcTriggered59052(ProcTriggerSpellAuraProcContext& ctx)
{
    // Howling Blast cooldown reset
    if (ctx.unit->GetTypeId() == TYPEID_PLAYER)
    {
        ctx.unit->RemoveSpellCategoryCooldown(1248, true);
    }
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
// Druid - Savage Defense
}

/// 62606: Savage Defense: base points of the aura's percent of the unit's attack power
static SpellHandlerOutcome<SpellAuraProcResult> ProcTriggerSpellAuraProcTriggered62606(ProcTriggerSpellAuraProcContext& ctx)
{
    ctx.basepoints[0] = int32(ctx.unit->GetTotalAttackPowerValue(BASE_ATTACK) * ctx.triggerAmount / 100);
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
}

/// One row of a site's table: a spell id and the handler registered for it.
template <class Site>
struct ProcRow
{
    uint32 spellId;
    typename SpellHandler<Site>::Function function;
};

/// Registers every row of one site's table on `registry`; answers the number of rows.
template <class Site, std::size_t N>
static uint32 RegisterProcRows(SpellHandlerRegistry& registry, ProcRow<Site> const (&rows)[N])
{
    for (ProcRow<Site> const& row : rows)
    {
        registry.Register<Site>(row.spellId, row.function);
    }
    return uint32(N);
}

uint32 RegisterUnitAuraProcHandlerHandlers(SpellHandlerRegistry& registry)
{
    static ProcRow<DummyAuraProcGenericSite> const dummyAuraProcGeneric[] =
    {
        { 9799, &DummyAuraProcGeneric9799 },
        { 25988, &DummyAuraProcGeneric9799 },
        { 18765, &DummyAuraProcGeneric18765 },
        { 35429, &DummyAuraProcGeneric18765 },
        { 21063, &DummyAuraProcGeneric21063 },
        { 24658, &DummyAuraProcGeneric24658 },
        { 24661, &DummyAuraProcGeneric24661 },
        { 28764, &DummyAuraProcGeneric28764 },
        { 27539, &DummyAuraProcGeneric27539 },
        { 28305, &DummyAuraProcGeneric28305 },
        { 31871, &DummyAuraProcGeneric31871 },
        { 31872, &DummyAuraProcGeneric31871 },
        { 33493, &DummyAuraProcGeneric33493 },
        { 38196, &DummyAuraProcGeneric38196 },
        { 39446, &DummyAuraProcGeneric39446 },
        { 45481, &DummyAuraProcGeneric45481 },
        { 45482, &DummyAuraProcGeneric45482 },
        { 45483, &DummyAuraProcGeneric45483 },
        { 45484, &DummyAuraProcGeneric45484 },
        { 46569, &DummyAuraProcGeneric46569 },
        { 48504, &DummyAuraProcGeneric48504 },
        { 52723, &DummyAuraProcGeneric52723 },
        { 60501, &DummyAuraProcGeneric52723 },
        { 57989, &DummyAuraProcGeneric57989 },
        { 58914, &DummyAuraProcGeneric58914 },
        { 63305, &DummyAuraProcGeneric63305 },
        { 63320, &DummyAuraProcGeneric63320 },
        { 65932, &DummyAuraProcGeneric65932 },
        { 66063, &DummyAuraProcGeneric66063 },
        { 66725, &DummyAuraProcGeneric66725 },
        { 68161, &DummyAuraProcGeneric66725 },
        { 66808, &DummyAuraProcGeneric66808 },
        { 68160, &DummyAuraProcGeneric66808 },
        { 69739, &DummyAuraProcGeneric69739 },
        { 69755, &DummyAuraProcGeneric69755 },
        { 71903, &DummyAuraProcGeneric71903 },
        { 72178, &DummyAuraProcGeneric72178 },
    };

    static ProcRow<DummyAuraProcMageSite> const dummyAuraProcMage[] =
    {
        { 11119, &DummyAuraProcMage11119 },
        { 11120, &DummyAuraProcMage11119 },
        { 12846, &DummyAuraProcMage11119 },
        { 12847, &DummyAuraProcMage11119 },
        { 12848, &DummyAuraProcMage11119 },
        { 56372, &DummyAuraProcMage56372 },
        { 56375, &DummyAuraProcMage56375 },
        { 64411, &DummyAuraProcMage64411 },
        { 74396, &DummyAuraProcMage74396 },
    };

    static ProcRow<DummyAuraProcWarlockSite> const dummyAuraProcWarlock[] =
    {
        { 18094, &DummyAuraProcWarlock18094 },
        { 18095, &DummyAuraProcWarlock18094 },
        { 56218, &DummyAuraProcWarlock18094 },
        { 30293, &DummyAuraProcWarlock30293 },
        { 30295, &DummyAuraProcWarlock30293 },
        { 30296, &DummyAuraProcWarlock30293 },
        { 37377, &DummyAuraProcWarlock37377 },
        { 37381, &DummyAuraProcWarlock37381 },
        { 39437, &DummyAuraProcWarlock39437 },
        { 63108, &DummyAuraProcWarlock63108 },
    };

    static ProcRow<DummyAuraProcPriestSite> const dummyAuraProcPriest[] =
    {
        { 15286, &DummyAuraProcPriest15286 },
        { 40438, &DummyAuraProcPriest40438 },
        { 26169, &DummyAuraProcPriest26169 },
        { 39372, &DummyAuraProcPriest39372 },
        { 28809, &DummyAuraProcPriest28809 },
        { 55677, &DummyAuraProcPriest55677 },
        { 55680, &DummyAuraProcPriest55680 },
        { 70770, &DummyAuraProcPriest70770 },
    };

    static ProcRow<DummyAuraProcDruidSite> const dummyAuraProcDruid[] =
    {
        { 24932, &DummyAuraProcDruid24932 },
        { 28719, &DummyAuraProcDruid28719 },
        { 28847, &DummyAuraProcDruid28847 },
        { 37288, &DummyAuraProcDruid37288 },
        { 37295, &DummyAuraProcDruid37288 },
        { 40442, &DummyAuraProcDruid40442 },
        { 44835, &DummyAuraProcDruid44835 },
        { 54754, &DummyAuraProcDruid54754 },
        { 54821, &DummyAuraProcDruid54821 },
        { 70664, &DummyAuraProcDruid70664 },
        { 70723, &DummyAuraProcDruid70723 },
    };

    static ProcRow<DummyAuraProcRogueSite> const dummyAuraProcRogue[] =
    {
        { 23582, &DummyAuraProcRogue23582 },
        { 32748, &DummyAuraProcRogue32748 },
        { 57934, &DummyAuraProcRogue57934 },
        { 76806, &DummyAuraProcRogue76806 },
        { 76838, &DummyAuraProcRogue76838 },
    };

    static ProcRow<DummyAuraProcPaladinSite> const dummyAuraProcPaladin[] =
    {
        { 20185, &DummyAuraProcPaladin20185 },
        { 20186, &DummyAuraProcPaladin20186 },
        { 20335, &DummyAuraProcPaladin20335 },
        { 20336, &DummyAuraProcPaladin20336 },
        { 20337, &DummyAuraProcPaladin20337 },
        { 20911, &DummyAuraProcPaladin20911 },
        { 25899, &DummyAuraProcPaladin20911 },
        { 28789, &DummyAuraProcPaladin28789 },
        { 31785, &DummyAuraProcPaladin31785 },
        { 33776, &DummyAuraProcPaladin31785 },
        { 31801, &DummyAuraProcPaladin31801 },
        { 31876, &DummyAuraProcPaladin31876 },
        { 31877, &DummyAuraProcPaladin31876 },
        { 31878, &DummyAuraProcPaladin31876 },
        { 40470, &DummyAuraProcPaladin40470 },
        { 53651, &DummyAuraProcPaladin53651 },
        { 53736, &DummyAuraProcPaladin53736 },
        { 54937, &DummyAuraProcPaladin54937 },
        { 58597, &DummyAuraProcPaladin58597 },
        { 53601, &DummyAuraProcPaladin53601 },
        { 64890, &DummyAuraProcPaladin64890 },
        { 71406, &DummyAuraProcPaladin71406 },
        { 71545, &DummyAuraProcPaladin71406 },
        { 71880, &DummyAuraProcPaladin71880 },
        { 76669, &DummyAuraProcPaladin76669 },
        { 71892, &DummyAuraProcPaladin71892 },
    };

    static ProcRow<DummyAuraProcShamanSite> const dummyAuraProcShaman[] =
    {
        { 28823, &DummyAuraProcShaman28823 },
        { 28849, &DummyAuraProcShaman28849 },
        { 33757, &DummyAuraProcShaman33757 },
        { 40463, &DummyAuraProcShaman40463 },
        { 55440, &DummyAuraProcShaman55440 },
        { 58877, &DummyAuraProcShaman58877 },
        { 63280, &DummyAuraProcShaman63280 },
        { 64928, &DummyAuraProcShaman64928 },
        { 67228, &DummyAuraProcShaman67228 },
        { 70808, &DummyAuraProcShaman70808 },
    };

    static ProcRow<HasteAuraProcRogueSite> const hasteAuraProcRogue[] =
    {
        { 13877, &HasteAuraProcRogue13877 },
        { 33735, &HasteAuraProcRogue13877 },
    };

    static ProcRow<SpellCritChanceAuraProcMageSite> const spellCritChanceAuraProcMage[] =
    {
        { 54646, &SpellCritChanceAuraProcMage54646 },
    };

    static ProcRow<ProcTriggerSpellAuraProcGenericSite> const procTriggerSpellAuraProcGeneric[] =
    {
        { 23780, &ProcTriggerSpellAuraProcGeneric23780 },
        { 27522, &ProcTriggerSpellAuraProcGeneric27522 },
        { 40336, &ProcTriggerSpellAuraProcGeneric27522 },
        { 46939, &ProcTriggerSpellAuraProcGeneric27522 },
        { 31255, &ProcTriggerSpellAuraProcGeneric31255 },
        { 33896, &ProcTriggerSpellAuraProcGeneric33896 },
        { 45057, &ProcTriggerSpellAuraProcGeneric45057 },
        { 43820, &ProcTriggerSpellAuraProcGeneric43820 },
        { 57345, &ProcTriggerSpellAuraProcGeneric57345 },
        { 64148, &ProcTriggerSpellAuraProcGeneric64148 },
        { 64415, &ProcTriggerSpellAuraProcGeneric64415 },
        { 64440, &ProcTriggerSpellAuraProcGeneric64440 },
        { 64568, &ProcTriggerSpellAuraProcGeneric64568 },
        { 67702, &ProcTriggerSpellAuraProcGeneric67702 },
        { 67771, &ProcTriggerSpellAuraProcGeneric67771 },
        { 69023, &ProcTriggerSpellAuraProcGeneric69023 },
    };

    static ProcRow<ProcTriggerSpellAuraProcTriggeredSite> const procTriggerSpellAuraProcTriggered[] =
    {
        { 7099, &ProcTriggerSpellAuraProcTriggered7099 },
        { 39647, &ProcTriggerSpellAuraProcTriggered7099 },
        { 29494, &ProcTriggerSpellAuraProcTriggered7099 },
        { 20233, &ProcTriggerSpellAuraProcTriggered7099 },
        { 15250, &ProcTriggerSpellAuraProcTriggered15250 },
        { 14189, &ProcTriggerSpellAuraProcTriggered14189 },
        { 14157, &ProcTriggerSpellAuraProcTriggered14189 },
        { 70802, &ProcTriggerSpellAuraProcTriggered14189 },
        { 23880, &ProcTriggerSpellAuraProcTriggered23880 },
        { 30824, &ProcTriggerSpellAuraProcTriggered30824 },
        { 35095, &ProcTriggerSpellAuraProcTriggered35095 },
        { 48090, &ProcTriggerSpellAuraProcTriggered48090 },
        { 50227, &ProcTriggerSpellAuraProcTriggered50227 },
        { 53817, &ProcTriggerSpellAuraProcTriggered53817 },
        { 57761, &ProcTriggerSpellAuraProcTriggered57761 },
        { 52179, &ProcTriggerSpellAuraProcTriggered52179 },
        { 54748, &ProcTriggerSpellAuraProcTriggered54748 },
        { 56453, &ProcTriggerSpellAuraProcTriggered56453 },
        { 59052, &ProcTriggerSpellAuraProcTriggered59052 },
        { 62606, &ProcTriggerSpellAuraProcTriggered62606 },
    };

    uint32 rows = RegisterProcRows(registry, dummyAuraProcGeneric);
    rows += RegisterProcRows(registry, dummyAuraProcMage);
    rows += RegisterProcRows(registry, dummyAuraProcWarlock);
    rows += RegisterProcRows(registry, dummyAuraProcPriest);
    rows += RegisterProcRows(registry, dummyAuraProcDruid);
    rows += RegisterProcRows(registry, dummyAuraProcRogue);
    rows += RegisterProcRows(registry, dummyAuraProcPaladin);
    rows += RegisterProcRows(registry, dummyAuraProcShaman);
    rows += RegisterProcRows(registry, hasteAuraProcRogue);
    rows += RegisterProcRows(registry, spellCritChanceAuraProcMage);
    rows += RegisterProcRows(registry, procTriggerSpellAuraProcGeneric);
    rows += RegisterProcRows(registry, procTriggerSpellAuraProcTriggered);
    return rows;
}
