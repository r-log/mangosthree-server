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
        if (caster->GetEntry() != 33943 && caster->GetTypeId() == TYPEID_UNIT)
        {
            ((Creature*)caster)->UpdateEntry(33943);
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

    uint32 rows = RegisterProcRows(registry, dummyAuraProcGeneric);
    rows += RegisterProcRows(registry, dummyAuraProcMage);
    rows += RegisterProcRows(registry, dummyAuraProcWarlock);
    return rows;
}
