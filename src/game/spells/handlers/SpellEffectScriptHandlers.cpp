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


#include "spells/handlers/SpellEffectScriptHandlers.h"
#include "spells/handlers/SpellHandlerRegistry.h"
#include "WorldHandlers/Spell.h"
#include "Common/TimeConstants.h"
#include "Geometry/Vector3.h"
#include "Object/Creature.h"
#include "Object/ObjectMgr.h"
#include "Object/TemporarySummon.h"
#include "Server/DBCStores.h"
#include "Server/WorldSession.h"
#include "Tools/Language.h"
#include "Utilities/MathDefines.h"
#include "Utilities/Util.h"
#include "WorldHandlers/CellImpl.h"
#include "WorldHandlers/GridMap.h"
#include "WorldHandlers/GridNotifiers.h"
#include "WorldHandlers/GridNotifiersImpl.h"
#include "WorldHandlers/Map.h"
#include "WorldHandlers/SpellAuras.h"
#include "entities/player/Player.h"

/// 8856: Bending Shinbone
static SpellHandlerOutcome<void> EffectScriptGeneric8856(SpellEffectScriptContext& ctx)
{
    if (!ctx.itemTarget && ctx.m_caster->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    uint32 spell_id = 0;
    switch (urand(1, 5))
    {
        case 1:  spell_id = 8854; break;
        default: spell_id = 8855; break;
    }

    ctx.m_caster->CastSpell(ctx.m_caster, spell_id, true, NULL);
    return SpellHandlerOutcome<void>::Return();
}

/// 17512: Piccolo of the Flaming Fire
static SpellHandlerOutcome<void> EffectScriptGeneric17512(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.unitTarget->HandleEmoteCommand(EMOTE_STATE_DANCE);
    return SpellHandlerOutcome<void>::Return();
}

/// 20589: Escape Artist
static SpellHandlerOutcome<void> EffectScriptGeneric20589(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.unitTarget->RemoveSpellsCausingAura(SPELL_AURA_MOD_ROOT);
    ctx.unitTarget->RemoveSpellsCausingAura(SPELL_AURA_MOD_DECREASE_SPEED);
    return SpellHandlerOutcome<void>::Return();
}

/// one body for 13 labels: 22539, 22972, 22975, 22976, 22977, 22978, 22979, 22980, 22981, 22982, 22983, 22984 and
/// 22985: Shadow Flame
static SpellHandlerOutcome<void> EffectScriptGenericShadowFlame(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget || !ctx.unitTarget->IsAlive())
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // Onyxia Scale Cloak
    if (ctx.unitTarget->GetDummyAura(22683))
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // Shadow Flame
    ctx.m_caster->CastSpell(ctx.unitTarget, 22682, true);
    return SpellHandlerOutcome<void>::Return();
}

/// one body for 2 labels: 24194: Uther's Tribute, 24195: Grom's Tribute
static SpellHandlerOutcome<void> EffectScriptGenericTribute(SpellEffectScriptContext& ctx)
{
    if (ctx.m_caster->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    uint8 race = ctx.m_caster->getRace();
    uint32 spellId = 0;

    switch (ctx.m_spellInfo->ID)
    {
        case 24194:
            switch (race)
            {
                case RACE_HUMAN:            spellId = 24105; break;
                case RACE_DWARF:            spellId = 24107; break;
                case RACE_NIGHTELF:         spellId = 24108; break;
                case RACE_GNOME:            spellId = 24106; break;
                case RACE_DRAENEI:          spellId = 69533; break;
            }
            break;
        case 24195:
            switch (race)
            {
                case RACE_ORC:              spellId = 24104; break;
                case RACE_UNDEAD:           spellId = 24103; break;
                case RACE_TAUREN:           spellId = 24102; break;
                case RACE_TROLL:            spellId = 24101; break;
                case RACE_BLOODELF:         spellId = 69530; break;
            }
            break;
    }

    if (spellId)
    {
        ctx.m_caster->CastSpell(ctx.m_caster, spellId, true);
    }

    return SpellHandlerOutcome<void>::Return();
}

/// 24320: Poisonous Blood
static SpellHandlerOutcome<void> EffectScriptGeneric24320(SpellEffectScriptContext& ctx)
{
    ctx.unitTarget->CastSpell(ctx.unitTarget, 24321, true, NULL, NULL, ctx.m_caster->GetObjectGuid());
    return SpellHandlerOutcome<void>::Return();
}

/// 24324: Blood Siphon
static SpellHandlerOutcome<void> EffectScriptGeneric24324(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.unitTarget->CastSpell(ctx.m_caster, ctx.unitTarget->HasAura(24321) ? 24323 : 24322, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 24590: Brittle Armor
static SpellHandlerOutcome<void> EffectScriptGeneric24590(SpellEffectScriptContext& ctx)
{
    ctx.unitTarget->RemoveAuraHolderFromStack(24575);
    return SpellHandlerOutcome<void>::Return();
}

/// 24714: Trick
static SpellHandlerOutcome<void> EffectScriptGeneric24714(SpellEffectScriptContext& ctx)
{
    if (ctx.m_caster->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    if (roll_chance_i(14))                  // Trick (can be different critter models). 14% since below can have 1 of 6
    {
        ctx.m_caster->CastSpell(ctx.m_caster, 24753, true);
    }
    else                                    // Random Costume, 6 different (plus add. for gender)
    {
        ctx.m_caster->CastSpell(ctx.m_caster, 24720, true);
    }

    return SpellHandlerOutcome<void>::Return();
}

/// 24717: Pirate Costume
static SpellHandlerOutcome<void> EffectScriptGeneric24717(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // Pirate Costume (male or female)
    ctx.m_caster->CastSpell(ctx.unitTarget, ctx.unitTarget->getGender() == GENDER_MALE ? 24708 : 24709, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 24718: Ninja Costume
static SpellHandlerOutcome<void> EffectScriptGeneric24718(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // Ninja Costume (male or female)
    ctx.m_caster->CastSpell(ctx.unitTarget, ctx.unitTarget->getGender() == GENDER_MALE ? 24711 : 24710, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 24719: Leper Gnome Costume
static SpellHandlerOutcome<void> EffectScriptGeneric24719(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // Leper Gnome Costume (male or female)
    ctx.m_caster->CastSpell(ctx.unitTarget, ctx.unitTarget->getGender() == GENDER_MALE ? 24712 : 24713, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 24720: Random Costume
static SpellHandlerOutcome<void> EffectScriptGeneric24720(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    uint32 spellId = 0;

    switch (urand(0, 6))
    {
        case 0:
            spellId = ctx.unitTarget->getGender() == GENDER_MALE ? 24708 : 24709;
            break;
        case 1:
            spellId = ctx.unitTarget->getGender() == GENDER_MALE ? 24711 : 24710;
            break;
        case 2:
            spellId = ctx.unitTarget->getGender() == GENDER_MALE ? 24712 : 24713;
            break;
        case 3:
            spellId = 24723;
            break;
        case 4:
            spellId = 24732;
            break;
        case 5:
            spellId = ctx.unitTarget->getGender() == GENDER_MALE ? 24735 : 24736;
            break;
        case 6:
            spellId = 24740;
            break;
    }

    ctx.m_caster->CastSpell(ctx.unitTarget, spellId, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 24737: Ghost Costume
static SpellHandlerOutcome<void> EffectScriptGeneric24737(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // Ghost Costume (male or female)
    ctx.m_caster->CastSpell(ctx.unitTarget, ctx.unitTarget->getGender() == GENDER_MALE ? 24735 : 24736, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 24751: Trick or Treat
static SpellHandlerOutcome<void> EffectScriptGeneric24751(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // Tricked or Treated
    ctx.unitTarget->CastSpell(ctx.unitTarget, 24755, true);

    // Treat / Trick
    ctx.unitTarget->CastSpell(ctx.unitTarget, roll_chance_i(50) ? 24714 : 24715, true);
    return SpellHandlerOutcome<void>::Return();
}

/// one body for 8 labels: 25140, 25143, 25650, 25652, 29128, 29129, 35376 and 35727: Translocate
static SpellHandlerOutcome<void> EffectScriptGenericOrbTeleport(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    uint32 spellid;
    switch (ctx.m_spellInfo->ID)
    {
        case 25140: spellid =  32568; break;
        case 25143: spellid =  32572; break;
        case 25650: spellid =  30140; break;
        case 25652: spellid =  30141; break;
        case 29128: spellid =  32571; break;
        case 29129: spellid =  32569; break;
        case 35376: spellid =  25649; break;
        case 35727: spellid =  35730; break;
        default:
            return SpellHandlerOutcome<void>::Return();
    }

    ctx.unitTarget->CastSpell(ctx.unitTarget, spellid, false);
    return SpellHandlerOutcome<void>::Return();
}

/// 26004: Mistletoe
static SpellHandlerOutcome<void> EffectScriptGeneric26004(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.unitTarget->HandleEmote(EMOTE_ONESHOT_CHEER);
    return SpellHandlerOutcome<void>::Return();
}

/// 26137: Rotate Trigger
static SpellHandlerOutcome<void> EffectScriptGeneric26137(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.unitTarget->CastSpell(ctx.unitTarget, urand(0, 1) ? 26009 : 26136, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 26218: Mistletoe
static SpellHandlerOutcome<void> EffectScriptGeneric26218(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    uint32 spells[3] = {26206, 26207, 45036};

    ctx.m_caster->CastSpell(ctx.unitTarget, spells[urand(0, 2)], true);
    return SpellHandlerOutcome<void>::Return();
}

/// 26275: PX-238 Winter Wondervolt TRAP
static SpellHandlerOutcome<void> EffectScriptGeneric26275(SpellEffectScriptContext& ctx)
{
    uint32 spells[4] = {26272, 26157, 26273, 26274};

    // check presence
    for (int j = 0; j < 4; ++j)
        if (ctx.unitTarget->HasAura(spells[j], EFFECT_INDEX_0))
        {
            return SpellHandlerOutcome<void>::Return();
        }

    // cast
    ctx.unitTarget->CastSpell(ctx.unitTarget, spells[urand(0, 3)], true);
    return SpellHandlerOutcome<void>::Return();
}

/// 26465: Mercurial Shield
static SpellHandlerOutcome<void> EffectScriptGeneric26465(SpellEffectScriptContext& ctx)
{
    ctx.unitTarget->RemoveAuraHolderFromStack(26464);
    return SpellHandlerOutcome<void>::Return();
}

/// 26656: Black Qiraji Battle Tank
static SpellHandlerOutcome<void> EffectScriptGeneric26656(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // Prevent stacking of mounts
    ctx.unitTarget->RemoveSpellsCausingAura(SPELL_AURA_MOUNTED);

    // Two separate mounts depending on area id (allows use both in and out of specific instance)
    if (ctx.unitTarget->GetTerrain()->GetAreaId(ctx.unitTarget->Where().X(), ctx.unitTarget->Where().Y(), ctx.unitTarget->Where().Z()) == 3428)
    {
        ctx.unitTarget->CastSpell(ctx.unitTarget, 25863, false);
    }
    else
    {
        ctx.unitTarget->CastSpell(ctx.unitTarget, 26655, false);
    }

    return SpellHandlerOutcome<void>::Return();
}

/// 27687: Summon Bone Minions
static SpellHandlerOutcome<void> EffectScriptGeneric27687(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // Spells 27690, 27691, 27692, 27693 are missing from DBC
    // So we need to summon creature 16119 manually
    float x, y, z;
    float angle = ctx.unitTarget->Where().Facing();
    for (uint8 i = 0; i < 4; ++i)
    {
        FindFreeSpotNear(*ctx.unitTarget, ctx.unitTarget, x, y, z, ctx.unitTarget->Where().Extent(), INTERACTION_DISTANCE, angle + i * M_PI_F / 2);
        ctx.unitTarget->SummonCreature(16119, x, y, z, angle, TEMPSPAWN_TIMED_OOC_OR_DEAD_DESPAWN, 10 * MINUTE * IN_MILLISECONDS);
    }
    return SpellHandlerOutcome<void>::Return();
}

/// 27695: Summon Bone Mages
static SpellHandlerOutcome<void> EffectScriptGeneric27695(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.unitTarget->CastSpell(ctx.unitTarget, 27696, true);
    ctx.unitTarget->CastSpell(ctx.unitTarget, 27697, true);
    ctx.unitTarget->CastSpell(ctx.unitTarget, 27698, true);
    ctx.unitTarget->CastSpell(ctx.unitTarget, 27699, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 28352: Breath of Sargeras
static SpellHandlerOutcome<void> EffectScriptGeneric28352(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.unitTarget->CastSpell(ctx.unitTarget, 28342, true);
    return SpellHandlerOutcome<void>::Return();
}

/// one body for 3 labels: 28374, 54426 and 71123: Decimate
static SpellHandlerOutcome<void> EffectScriptGenericDecimate(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    float downToHealthPercent = (ctx.m_spellInfo->ID != 71123 ? 5 : ctx.effect->CalculateSimpleValue()) * 0.01f;

    int32 excess = ctx.unitTarget->GetHealth() - ctx.unitTarget->GetMaxHealth() * downToHealthPercent;
    if (excess > 0)
    {
        ctx.m_caster->CastCustomSpell(ctx.unitTarget, 28375, &excess, NULL, NULL, true);
    }
    return SpellHandlerOutcome<void>::Return();
}

/// 28560: Summon Blizzard
static SpellHandlerOutcome<void> EffectScriptGeneric28560(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.m_caster->SummonCreature(16474, ctx.unitTarget->Where().X(), ctx.unitTarget->Where().Y(), ctx.unitTarget->Where().Z(), 0.0f, TEMPSPAWN_TIMED_DESPAWN, 30000);
    return SpellHandlerOutcome<void>::Return();
}

/// one body for 9 labels: 28859, 29126, 29135, 29136, 29137, 29138, 29139, 46671 and 46672: Cleansing Flames
static SpellHandlerOutcome<void> EffectScriptGenericCleansingFlames(SpellEffectScriptContext& ctx)
{
    // Cleanse all magic, curse, disease and poison
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.unitTarget->RemoveAurasWithDispelType(DISPEL_MAGIC);
    ctx.unitTarget->RemoveAurasWithDispelType(DISPEL_CURSE);
    ctx.unitTarget->RemoveAurasWithDispelType(DISPEL_DISEASE);
    ctx.unitTarget->RemoveAurasWithDispelType(DISPEL_POISON);

    return SpellHandlerOutcome<void>::Return();
}

/// 29395: Break Kaliri Egg
static SpellHandlerOutcome<void> EffectScriptGeneric29395(SpellEffectScriptContext& ctx)
{
    uint32 creature_id = 0;
    uint32 rand = urand(0, 99);

    if (rand < 10)
    {
        creature_id = 17034;
    }
    else if (rand < 60)
    {
        creature_id = 17035;
    }
    else
    {
        creature_id = 17039;
    }

    if (WorldObject* pSource = ctx.spell->GetAffectiveCasterObject())
    {
        pSource->SummonCreature(creature_id, 0.0f, 0.0f, 0.0f, 0.0f, TEMPSPAWN_TIMED_OOC_OR_DEAD_DESPAWN, 120 * IN_MILLISECONDS);
    }
    return SpellHandlerOutcome<void>::Return();
}

/// 29830: Use Mirren's Drinking Hat
static SpellHandlerOutcome<void> EffectScriptGeneric29830(SpellEffectScriptContext& ctx)
{
    uint32 item = 0;
    switch (urand(1, 6))
    {
        case 1:
        case 2:
        case 3:
            item = 23584; break;            // Loch Modan Lager
        case 4:
        case 5:
            item = 23585; break;            // Stouthammer Lite
        case 6:
            item = 23586; break;            // Aerie Peak Pale Ale
    }

    if (item)
    {
        ctx.spell->DoCreateItem(ctx.effect,item);
    }

    return SpellHandlerOutcome<void>::Continue();
}

/// 30541: Blaze
static SpellHandlerOutcome<void> EffectScriptGeneric30541(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.unitTarget->CastSpell(ctx.unitTarget, 30542, true);
    return SpellHandlerOutcome<void>::Continue();
}

/// 30469: Nether Beam
static SpellHandlerOutcome<void> EffectScriptGeneric30469(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // The player and boss spells are different
    if (ctx.unitTarget->GetTypeId() == TYPEID_PLAYER)
    {
        switch (ctx.m_caster->GetEntry())
        {
            case 17367:
                if (ctx.unitTarget->HasAura(38638))
                {
                    return SpellHandlerOutcome<void>::Return();
                }

                ctx.m_caster->CastSpell(ctx.unitTarget, 30401, true);
                ctx.m_caster->CastSpell(ctx.unitTarget, 30422, true);
                break;
            case 17368:
                if (ctx.unitTarget->HasAura(38639))
                {
                    return SpellHandlerOutcome<void>::Return();
                }

                ctx.m_caster->CastSpell(ctx.unitTarget, 30402, true);
                ctx.m_caster->CastSpell(ctx.unitTarget, 30423, true);
                break;
            case 17369:
                if (ctx.unitTarget->HasAura(38637))
                {
                    return SpellHandlerOutcome<void>::Return();
                }

                ctx.m_caster->CastSpell(ctx.unitTarget, 30400, true);
                ctx.m_caster->CastSpell(ctx.unitTarget, 30421, true);
                break;
        }
    }
    // target boss
    else if (ctx.unitTarget->GetEntry() == 15689)
    {
        switch (ctx.m_caster->GetEntry())
        {
            case 17367:
                ctx.m_caster->CastSpell(ctx.unitTarget, 30464, true);
                ctx.m_caster->CastSpell(ctx.unitTarget, 30467, true);
                break;
            case 17368:
                ctx.m_caster->CastSpell(ctx.unitTarget, 30463, true);
                ctx.m_caster->CastSpell(ctx.unitTarget, 30468, true);
                break;
            case 17369:
                ctx.m_caster->CastSpell(ctx.unitTarget, 30465, true);
                ctx.m_caster->CastSpell(ctx.unitTarget, 30466, true);
                break;
        }
    }
    return SpellHandlerOutcome<void>::Return();
}

/// 30769: Pick Red Riding Hood
static SpellHandlerOutcome<void> EffectScriptGeneric30769(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // cast Little Red Riding Hood
    ctx.m_caster->CastSpell(ctx.unitTarget, 30768, true);
    return SpellHandlerOutcome<void>::Continue();
}

/// 30835: Infernal Relay
static SpellHandlerOutcome<void> EffectScriptGeneric30835(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.unitTarget->CastSpell(ctx.unitTarget, 30836, true, NULL, NULL, ctx.m_caster->GetObjectGuid());
    return SpellHandlerOutcome<void>::Continue();
}

/// 30918: Improved Sprint
static SpellHandlerOutcome<void> EffectScriptGeneric30918(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // Removes snares and roots.
    ctx.unitTarget->RemoveAurasAtMechanicImmunity(IMMUNE_TO_ROOT_AND_SNARE_MASK, 30918, true);
    return SpellHandlerOutcome<void>::Continue();
}

/// one body for 12 labels: 37142 and 37143: Karazhan - Chess NPC Action: Melee Attack: Conjured Water Elemental, 37147:
/// Karazhan - Chess NPC Action: Melee Attack: Human Cleric, 37149: Karazhan - Chess NPC Action: Melee Attack: Human
/// Conjurer, 37150: Karazhan - Chess NPC Action: Melee Attack: King Llane, 37220: Karazhan - Chess NPC Action: Melee
/// Attack: Summoned Daemon, 32227: Karazhan - Chess NPC Action: Melee Attack: Footman, 32228: Karazhan - Chess NPC
/// Action: Melee Attack: Grunt, 37337: Karazhan - Chess NPC Action: Melee Attack: Orc Necrolyte, 37339: Karazhan -
/// Chess NPC Action: Melee Attack: Orc Wolf, 37345: Karazhan - Chess NPC Action: Melee Attack: Orc Warlock, 37348:
/// Karazhan - Chess NPC Action: Melee Attack: Warchief Blackhand
static SpellHandlerOutcome<void> EffectScriptGenericChessMeleeAttack(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.m_caster->CastSpell(ctx.unitTarget, 32247, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 32301: Ping Shirrak
static SpellHandlerOutcome<void> EffectScriptGeneric32301(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // Cast Focus fire on caster
    ctx.unitTarget->CastSpell(ctx.m_caster, 32300, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 33676: Incite Chaos
static SpellHandlerOutcome<void> EffectScriptGeneric33676(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.m_caster->CastSpell(ctx.unitTarget, 33684, true);
    return SpellHandlerOutcome<void>::Return();
}

/// one body for 2 labels: 34653 and 36920: Fireball
static SpellHandlerOutcome<void> EffectScriptGenericFireball(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.unitTarget->CastSpell(ctx.unitTarget, ctx.unitTarget->GetMap()->IsRegularDifficulty() ? 23971 : 30928, true, NULL, NULL, ctx.m_caster->GetObjectGuid());
    return SpellHandlerOutcome<void>::Return();
}

/// 35865: Summon Nether Vapor
static SpellHandlerOutcome<void> EffectScriptGeneric35865(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    float x, y, z;
    for (uint8 i = 0; i < 4; ++i)
    {
        FindFreeSpotNear(*ctx.m_caster, ctx.m_caster, x, y, z, 0.0f, INTERACTION_DISTANCE, M_PI_F * .5f * i + M_PI_F * .25f);
        ctx.m_caster->SummonCreature(21002, x, y, z, 0, TEMPSPAWN_TIMED_OOC_OR_DEAD_DESPAWN, 30000);
    }
    return SpellHandlerOutcome<void>::Return();
}

/// 37431: Spout
static SpellHandlerOutcome<void> EffectScriptGeneric37431(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.unitTarget->CastSpell(ctx.unitTarget, urand(0, 1) ? 37429 : 37430, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 37775: Karazhan - Chess NPC Action - Poison Cloud
static SpellHandlerOutcome<void> EffectScriptGeneric37775(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.m_caster->CastSpell(ctx.unitTarget, 37469, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 37824: Karazhan - Chess NPC Action - Shadow Mend
static SpellHandlerOutcome<void> EffectScriptGeneric37824(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.m_caster->CastSpell(ctx.unitTarget, 37456, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 38358: Tidal Surge
static SpellHandlerOutcome<void> EffectScriptGeneric38358(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.unitTarget->CastSpell(ctx.unitTarget, 38353, true, NULL, NULL, ctx.m_caster->GetObjectGuid());
    return SpellHandlerOutcome<void>::Return();
}

/// one body for 2 labels: 39338: Karazhan - Chess, Medivh CHEAT: Hand of Medivh, Target Horde; 39342: Karazhan - Chess,
/// Medivh CHEAT: Hand of Medivh, Target Alliance
static SpellHandlerOutcome<void> EffectScriptGenericHandOfMedivh(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.m_caster->CastSpell(ctx.unitTarget, 39339, true);
    return SpellHandlerOutcome<void>::Return();
}

/// one body for 2 labels: 39341: Karazhan - Chess, Medivh CHEAT: Fury of Medivh, Target Horde; 39344: Karazhan - Chess,
/// Medivh CHEAT: Fury of Medivh, Target Alliance
static SpellHandlerOutcome<void> EffectScriptGenericFuryOfMedivh(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.m_caster->CastSpell(ctx.unitTarget, ctx.effect->CalculateSimpleValue(), true);
    return SpellHandlerOutcome<void>::Return();
}

/// one body for 3 labels: 41055 and 63416: Copy Weapon, 69891: Copy Weapon (No Threat)
static SpellHandlerOutcome<void> EffectScriptGenericCopyWeapon(SpellEffectScriptContext& ctx)
{
    if (ctx.m_caster->GetTypeId() != TYPEID_UNIT || !ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    if (Item* pItem = ((Player*)ctx.unitTarget)->GetInventoryMgr().GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND))
    {
        ((Creature*)ctx.m_caster)->SetVirtualItem(VIRTUAL_ITEM_SLOT_0, pItem->GetEntry());

        // Unclear what this spell should do
        ctx.unitTarget->CastSpell(ctx.m_caster, ctx.effect->CalculateSimpleValue(), true);
    }

    return SpellHandlerOutcome<void>::Return();
}

/// 41072: Bloodbolt
static SpellHandlerOutcome<void> EffectScriptGeneric41072(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.m_caster->CastSpell(ctx.unitTarget, ctx.effect->CalculateSimpleValue(), true);
    return SpellHandlerOutcome<void>::Return();
}

/// 41126: Flame Burst
static SpellHandlerOutcome<void> EffectScriptGeneric41126(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.unitTarget->CastSpell(ctx.unitTarget, 41131, true);
    return SpellHandlerOutcome<void>::Continue();
}

/// one body for 4 labels: 41213, 43416, 69222 and 73076: Throw Shield
static SpellHandlerOutcome<void> EffectScriptGenericThrowShield(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.unitTarget->CastSpell(ctx.m_caster, ctx.effect->CalculateSimpleValue(), true);
    return SpellHandlerOutcome<void>::Return();
}

/// 42281: Sprouting
static SpellHandlerOutcome<void> EffectScriptGeneric42281(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.unitTarget->RemoveAurasDueToSpell(42280);
    ctx.unitTarget->RemoveAurasDueToSpell(42294);
    ctx.unitTarget->CastSpell(ctx.unitTarget, 42285, true);
    ctx.unitTarget->CastSpell(ctx.unitTarget, 42291, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 42492: Cast Energized
static SpellHandlerOutcome<void> EffectScriptGeneric42492(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    uint32 questId = ctx.m_spellInfo->CalculateSimpleValue(EFFECT_INDEX_1);
    if (!questId || !GetQuestTemplateStore(questId) || !((Player*)ctx.unitTarget)->GetQuestStatusMgr().IsCurrentQuest(questId))
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.m_caster->CastSpell(ctx.m_caster, ctx.effect->CalculateSimpleValue(), true);
    return SpellHandlerOutcome<void>::Return();
}

/// 42578: Cannon Blast
static SpellHandlerOutcome<void> EffectScriptGeneric42578(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    int32 basePoints = ctx.effect->CalculateSimpleValue();
    ctx.unitTarget->CastCustomSpell(ctx.unitTarget, 42576, &basePoints, NULL, NULL, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 43365: The Cleansing: Shrine Cast
static SpellHandlerOutcome<void> EffectScriptGeneric43365(SpellEffectScriptContext& ctx)
{
    if (ctx.m_caster->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // Script Effect Player Cast Mirror Image
    ctx.m_caster->CastSpell(ctx.m_caster, 50217, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 43375: Mixing Vrykul Blood
static SpellHandlerOutcome<void> EffectScriptGeneric43375(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    uint32 triggeredSpell[] = {43376, 43378, 43970, 43377};

    ctx.unitTarget->CastSpell(ctx.unitTarget, triggeredSpell[urand(0, 3)], true);
    return SpellHandlerOutcome<void>::Return();
}

/// one body for 2 labels: 44323 and 44407: Hawk Hunting
static SpellHandlerOutcome<void> EffectScriptGenericHawkHunting(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_UNIT)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // check target entry specific to each spell
    if (ctx.m_spellInfo->ID == 44323 && ctx.unitTarget->GetEntry() != 24746)
    {
        return SpellHandlerOutcome<void>::Return();
    }
    if (ctx.m_spellInfo->ID == 44407 && ctx.unitTarget->GetEntry() != 24747)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.unitTarget->CastSpell(ctx.m_caster, ctx.effect->CalculateSimpleValue(), true);
    // despawn delay depends on the distance between caster and target
    ((Creature*)ctx.unitTarget)->ForcedDespawn(100 * ctx.unitTarget->Where().DistanceTo(ctx.m_caster->Where(), false));
    return SpellHandlerOutcome<void>::Return();
}

/// 44364: Rock Falcon Primer
static SpellHandlerOutcome<void> EffectScriptGeneric44364(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // Are there anything special with this, a random chance or condition?
    // Feeding Rock Falcon
    ctx.unitTarget->CastSpell(ctx.unitTarget, ctx.effect->CalculateSimpleValue(), true, NULL, NULL, ctx.unitTarget->GetObjectGuid(), ctx.m_spellInfo);
    return SpellHandlerOutcome<void>::Return();
}

/// 44462: The Way to His Heart...: Cast Quest Complete on Master
static SpellHandlerOutcome<void> EffectScriptGeneric44462(SpellEffectScriptContext& ctx)
{
    if (ctx.m_caster->GetTypeId() != TYPEID_UNIT)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    Creature* pQuestCow = NULL;

    float range = 20.0f;

    // search for a reef cow nearby
    MaNGOS::NearestCreatureEntryWithLiveStateInObjectRangeCheck u_check(*ctx.m_caster, 24797, true, false, range);
    MaNGOS::CreatureLastSearcher<MaNGOS::NearestCreatureEntryWithLiveStateInObjectRangeCheck> searcher(pQuestCow, u_check);

    Cell::VisitGridObjects(ctx.m_caster, searcher, range);

    // no cows found, so return
    if (!pQuestCow)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    if (!((Creature*)ctx.m_caster)->IsTemporarySummon())
    {
        return SpellHandlerOutcome<void>::Return();
    }

    if (const SpellEntry *pSpell = sSpellStore.LookupEntry(ctx.effect->CalculateSimpleValue()))
    {
        TemporarySummon* pSummon = (TemporarySummon*)ctx.m_caster;

        // all ok, so make summoner cast the quest complete
        if (Unit* pSummoner = pSummon->GetSummoner())
        {
            pSummoner->CastSpell(pSummoner, pSpell, true);
        }
    }

    return SpellHandlerOutcome<void>::Return();
}

/// 44876: Force Cast - Portal Effect: Sunwell Isle
static SpellHandlerOutcome<void> EffectScriptGeneric44876(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.unitTarget->CastSpell(ctx.unitTarget, 44870, true);
    return SpellHandlerOutcome<void>::Continue();
}

/// 44811: Spectral Realm
static SpellHandlerOutcome<void> EffectScriptGeneric44811(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // If the player can't be teleported, send him a notification
    if (ctx.unitTarget->HasAura(44867))
    {
        ((Player*)ctx.unitTarget)->GetSession()->SendNotification(LANG_FAIL_ENTER_SPECTRAL_REALM);
        return SpellHandlerOutcome<void>::Return();
    }

    // Teleport target to the spectral realm, add debuff and force faction
    ctx.unitTarget->CastSpell(ctx.unitTarget, 46019, true);
    ctx.unitTarget->CastSpell(ctx.unitTarget, 46021, true);
    ctx.unitTarget->CastSpell(ctx.unitTarget, 44845, true);
    ctx.unitTarget->CastSpell(ctx.unitTarget, 44852, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 45141: Burn
static SpellHandlerOutcome<void> EffectScriptGeneric45141(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.unitTarget->CastSpell(ctx.unitTarget, 46394, true, NULL, NULL, ctx.m_caster->GetObjectGuid());
    return SpellHandlerOutcome<void>::Return();
}

/// 45151: Burn
static SpellHandlerOutcome<void> EffectScriptGeneric45151(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->HasAura(46394))
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // Make the burn effect jump to another friendly target
    ctx.unitTarget->CastSpell(ctx.unitTarget, 46394, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 45185: Stomp
static SpellHandlerOutcome<void> EffectScriptGeneric45185(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // Remove the burn effect
    ctx.unitTarget->RemoveAurasDueToSpell(46394);
    return SpellHandlerOutcome<void>::Return();
}

/// 45204: Clone Me!
static SpellHandlerOutcome<void> EffectScriptGeneric45204(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.unitTarget->CastSpell(ctx.m_caster, ctx.effect->CalculateSimpleValue(), true);
    return SpellHandlerOutcome<void>::Return();
}

/// one body for 2 labels: 45206: Copy Off-hand Weapon, 69892: Copy Off-hand Weapon (No Threat)
static SpellHandlerOutcome<void> EffectScriptGenericCopyOffHandWeapon(SpellEffectScriptContext& ctx)
{
    if (ctx.m_caster->GetTypeId() != TYPEID_UNIT || !ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    if (Item* pItem = ((Player*)ctx.unitTarget)->GetInventoryMgr().GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND))
    {
        ((Creature*)ctx.m_caster)->SetVirtualItem(VIRTUAL_ITEM_SLOT_1, pItem->GetEntry());

        // Unclear what this spell should do
        ctx.unitTarget->CastSpell(ctx.m_caster, ctx.effect->CalculateSimpleValue(), true);
    }

    return SpellHandlerOutcome<void>::Return();
}

/// 45235: Blaze
static SpellHandlerOutcome<void> EffectScriptGeneric45235(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.unitTarget->CastSpell(ctx.unitTarget, 45236, true, NULL, NULL, ctx.m_caster->GetObjectGuid());
    return SpellHandlerOutcome<void>::Return();
}

/// 45260: Karazhan - Chess - Force Player to Kill Bunny
static SpellHandlerOutcome<void> EffectScriptGeneric45260(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.unitTarget->CastSpell(ctx.unitTarget, 45259, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 45313: Anchor Here
static SpellHandlerOutcome<void> EffectScriptGeneric45313(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_UNIT)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ((Creature*)ctx.unitTarget)->SetSpawn(
        Geometry::Vector3(ctx.unitTarget->Where().X(), ctx.unitTarget->Where().Y(),
                          ctx.unitTarget->Where().Z()),
        ctx.unitTarget->Where().Facing());
    return SpellHandlerOutcome<void>::Return();
}

/// 45625: Arcane Chains: Character Force Cast
static SpellHandlerOutcome<void> EffectScriptGeneric45625(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.unitTarget->CastSpell(ctx.unitTarget, ctx.effect->CalculateSimpleValue(), true);
    return SpellHandlerOutcome<void>::Return();
}

/// 45668: Crafty's Ultra-Advanced Proto-Typical Shortening Blaster
static SpellHandlerOutcome<void> EffectScriptGeneric45668(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_UNIT)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    if (roll_chance_i(25))                  // chance unknown, using 25
    {
        return SpellHandlerOutcome<void>::Return();
    }

    static uint32 const spellPlayer[5] =
    {
        45674,                              // Bigger!
        45675,                              // Shrunk
        45678,                              // Yellow
        45682,                              // Ghost
        45684                               // Polymorph
    };

    static uint32 const spellTarget[5] =
    {
        45673,                              // Bigger!
        45672,                              // Shrunk
        45677,                              // Yellow
        45681,                              // Ghost
        45683                               // Polymorph
    };

    ctx.m_caster->CastSpell(ctx.m_caster, spellPlayer[urand(0, 4)], true);
    ctx.unitTarget->CastSpell(ctx.unitTarget, spellTarget[urand(0, 4)], true);

    return SpellHandlerOutcome<void>::Return();
}

/// 45691: Hah... : Magnataur On Death 1
static SpellHandlerOutcome<void> EffectScriptGeneric45691(SpellEffectScriptContext& ctx)
{
    // assuming caster is creature, if not, then return
    if (ctx.m_caster->GetTypeId() != TYPEID_UNIT)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    Player* pPlayer = ((Creature*)ctx.m_caster)->GetOriginalLootRecipient();

    if (!pPlayer)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    if (pPlayer->HasAura(45674) || pPlayer->HasAura(45675) || pPlayer->HasAura(45678) || pPlayer->HasAura(45682) || pPlayer->HasAura(45684))
    {
        pPlayer->CastSpell(pPlayer, 45686, true);
    }

    ctx.m_caster->CastSpell(ctx.m_caster, 45685, true);

    return SpellHandlerOutcome<void>::Return();
}

/// 45713: Naked Caravan Guard - Master Transform
static SpellHandlerOutcome<void> EffectScriptGeneric45713(SpellEffectScriptContext& ctx)
{
    if (ctx.m_caster->GetTypeId() != TYPEID_UNIT)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    const CreatureInfo* cTemplate = NULL;

    switch (ctx.m_caster->GetEntry())
    {
        case 25342: cTemplate = ObjectMgr::GetCreatureTemplate(25340); break;
        case 25343: cTemplate = ObjectMgr::GetCreatureTemplate(25341); break;
    }

    if (!cTemplate)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    uint32 display_id = 0;

    // Spell is designed to be used in creature addon.
    // This makes it possible to set proper model before adding to map.
    // For later, spell is used in gossip (with following despawn,
    // so addon can reload the default model and data again).

    // It should be noted that additional spell id's have been seen in relation to this spell, but
    // those does not exist in client (45701 (regular spell), 45705-45712 (auras), 45459-45460 (auras)).
    // We can assume 45705-45712 are transform auras, used instead of hard coded models in the below code.

    // not in map yet OR no npc flags yet (restored after LoadCreatureAddon for respawn cases)
    if (!ctx.m_caster->Where().ShareFrame(ctx.m_caster->Where()) || ctx.m_caster->GetUInt32Value(UNIT_NPC_FLAGS) == UNIT_NPC_FLAG_NONE)
    {
        display_id = Creature::ChooseDisplayId(cTemplate);
        ((Creature*)ctx.m_caster)->LoadEquipment(((Creature*)ctx.m_caster)->GetEquipmentId());
    }
    else
    {
        ctx.m_caster->SetUInt32Value(UNIT_NPC_FLAGS, cTemplate->NpcFlags);
        ((Creature*)ctx.m_caster)->SetVirtualItem(VIRTUAL_ITEM_SLOT_0, 0);
        ((Creature*)ctx.m_caster)->SetVirtualItem(VIRTUAL_ITEM_SLOT_1, 0);

        switch (ctx.m_caster->GetDisplayId())
        {
            case 23246: display_id = 23245; break;
            case 23247: display_id = 23250; break;
            case 23248: display_id = 23251; break;
            case 23249: display_id = 23252; break;
            case 23124: display_id = 23253; break;
            case 23125: display_id = 23254; break;
            case 23126: display_id = 23255; break;
            case 23127: display_id = 23256; break;
        }
    }

    ctx.m_caster->SetDisplayId(display_id);
    return SpellHandlerOutcome<void>::Return();
}

/// 45714: Fog of Corruption
static SpellHandlerOutcome<void> EffectScriptGeneric45714(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget || ctx.m_caster->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.unitTarget->CastSpell(ctx.m_caster, ctx.effect->CalculateSimpleValue(), true);
    return SpellHandlerOutcome<void>::Return();
}

/// 45717: Fog of Corruption
static SpellHandlerOutcome<void> EffectScriptGeneric45717(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.unitTarget->CastSpell(ctx.unitTarget, 45726, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 45785: Sinister Reflection Clone
static SpellHandlerOutcome<void> EffectScriptGeneric45785(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.unitTarget->CastSpell(ctx.unitTarget, ctx.effect->CalculateSimpleValue(), true);
    return SpellHandlerOutcome<void>::Return();
}

/// 45833: Power of the Blue Flight
static SpellHandlerOutcome<void> EffectScriptGeneric45833(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.unitTarget->CastSpell(ctx.unitTarget, 45836, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 45892: Sinister Reflection
static SpellHandlerOutcome<void> EffectScriptGeneric45892(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // Summon 4 clones of the same player
    for (uint8 i = 0; i < 4; ++i)
    {
        ctx.unitTarget->CastSpell(ctx.unitTarget, 45891, true, NULL, NULL, ctx.m_caster->GetObjectGuid());
    }
    return SpellHandlerOutcome<void>::Return();
}

/// 45918: Soul Sever
static SpellHandlerOutcome<void> EffectScriptGeneric45918(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_PLAYER || !ctx.unitTarget->HasAura(45717))
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // kill all charmed targets
    ctx.unitTarget->CastSpell(ctx.unitTarget, 45917, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 45958: Signal Alliance
static SpellHandlerOutcome<void> EffectScriptGeneric45958(SpellEffectScriptContext& ctx)
{
    // "escort" aura not present, so let nothing happen
    if (!ctx.m_caster->HasAura(ctx.effect->CalculateSimpleValue()))
    {
        return SpellHandlerOutcome<void>::Return();
    }
    // "escort" aura is present so break; and let DB table dbscripts_on_spell be used and process further.
    return SpellHandlerOutcome<void>::Continue();
}

/// 46203: Goblin Weather Machine
static SpellHandlerOutcome<void> EffectScriptGeneric46203(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    uint32 spellId = 0;
    switch (urand(0, 3))
    {
        case 0: spellId = 46740; break;
        case 1: spellId = 46739; break;
        case 2: spellId = 46738; break;
        case 3: spellId = 46736; break;
    }
    ctx.unitTarget->CastSpell(ctx.unitTarget, spellId, true);
    return SpellHandlerOutcome<void>::Continue();
}

/// 46289: Negative Energy
static SpellHandlerOutcome<void> EffectScriptGeneric46289(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.m_caster->CastSpell(ctx.unitTarget, 46285, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 46430: Synch Health
static SpellHandlerOutcome<void> EffectScriptGeneric46430(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.unitTarget->SetHealth(ctx.m_caster->GetHealth());
    return SpellHandlerOutcome<void>::Return();
}

/// 46642: 5,000 Gold
static SpellHandlerOutcome<void> EffectScriptGeneric46642(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ((Player*)ctx.unitTarget)->ModifyMoney(5000 * GOLD);
    return SpellHandlerOutcome<void>::Continue();
}

/// 47097: Surge Needle Teleporter
static SpellHandlerOutcome<void> EffectScriptGeneric47097(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    if (ctx.unitTarget->GetTerrain()->GetAreaId(ctx.unitTarget->Where().X(), ctx.unitTarget->Where().Y(), ctx.unitTarget->Where().Z()) == 4157)
    {
        ctx.unitTarget->CastSpell(ctx.unitTarget, 47324, true);
    }
    else if (ctx.unitTarget->GetTerrain()->GetAreaId(ctx.unitTarget->Where().X(), ctx.unitTarget->Where().Y(), ctx.unitTarget->Where().Z()) == 4156)
    {
        ctx.unitTarget->CastSpell(ctx.unitTarget, 47325, true);
    }

    return SpellHandlerOutcome<void>::Continue();
}

/// 47311: Quest - Jormungar Explosion Spell Spawner
static SpellHandlerOutcome<void> EffectScriptGeneric47311(SpellEffectScriptContext& ctx)
{
    // Summons npc's. They are expected to summon GO from 47315
    // but there is no way to get the summoned, to trigger a spell
    // cast (workaround can be done with ai script).

    // Quest - Jormungar Explosion Summon Object
    for (int i = 0; i < 2; ++i)
    {
        ctx.m_caster->CastSpell(ctx.m_caster, 47309, true);
    }

    for (int i = 0; i < 2; ++i)
    {
        ctx.m_caster->CastSpell(ctx.m_caster, 47924, true);
    }

    for (int i = 0; i < 2; ++i)
    {
        ctx.m_caster->CastSpell(ctx.m_caster, 47925, true);
    }

    return SpellHandlerOutcome<void>::Return();
}

/// 47393: The Focus on the Beach: Quest Completion Script
static SpellHandlerOutcome<void> EffectScriptGeneric47393(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // Ley Line Information
    ctx.unitTarget->RemoveAurasDueToSpell(47391);
    return SpellHandlerOutcome<void>::Return();
}

/// 47615: Atop the Woodlands: Quest Completion Script
static SpellHandlerOutcome<void> EffectScriptGeneric47615(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // Ley Line Information
    ctx.unitTarget->RemoveAurasDueToSpell(47473);
    return SpellHandlerOutcome<void>::Return();
}

/// 47638: The End of the Line: Quest Completion Script
static SpellHandlerOutcome<void> EffectScriptGeneric47638(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // Ley Line Information
    ctx.unitTarget->RemoveAurasDueToSpell(47636);
    return SpellHandlerOutcome<void>::Return();
}

/// 47703: Unholy Union
static SpellHandlerOutcome<void> EffectScriptGeneric47703(SpellEffectScriptContext& ctx)
{
    ctx.m_caster->CastSpell(ctx.m_caster, 50254, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 47724: Frost Draw
static SpellHandlerOutcome<void> EffectScriptGeneric47724(SpellEffectScriptContext& ctx)
{
    ctx.m_caster->CastSpell(ctx.m_caster, 50239, true);
    return SpellHandlerOutcome<void>::Return();
}

/// one body for 2 labels: 47958 and 57083: Crystal Spikes
static SpellHandlerOutcome<void> EffectScriptGenericCrystalSpikes(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.unitTarget->CastSpell(ctx.unitTarget, 47954, true);
    ctx.unitTarget->CastSpell(ctx.unitTarget, 47955, true);
    ctx.unitTarget->CastSpell(ctx.unitTarget, 47956, true);
    ctx.unitTarget->CastSpell(ctx.unitTarget, 47957, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 48590: Avenging Spirits
static SpellHandlerOutcome<void> EffectScriptGeneric48590(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // Summon 4 spirits summoners
    ctx.unitTarget->CastSpell(ctx.unitTarget, 48586, true);
    ctx.unitTarget->CastSpell(ctx.unitTarget, 48587, true);
    ctx.unitTarget->CastSpell(ctx.unitTarget, 48588, true);
    ctx.unitTarget->CastSpell(ctx.unitTarget, 48589, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 48603: High Executor's Branding Iron
static SpellHandlerOutcome<void> EffectScriptGeneric48603(SpellEffectScriptContext& ctx)
{
    // Torture the Torturer: High Executor's Branding Iron Impact
    ctx.unitTarget->CastSpell(ctx.unitTarget, 48614, true);
    return SpellHandlerOutcome<void>::Return();
}

/// one body for 4 labels: 48724: The Denouncement: Commander Jordan On Death, 48726: The Denouncement: Lead Cannoneer
/// Zierhut On Death, 48728: The Denouncement: Blacksmith Goodman On Death, 48730: The Denouncement: Stable Master
/// Mercer On Death
static SpellHandlerOutcome<void> EffectScriptGenericDenouncementOnDeath(SpellEffectScriptContext& ctx)
{
    // Compelled
    if (!ctx.unitTarget || !ctx.m_caster->HasAura(48714))
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.unitTarget->CastSpell(ctx.unitTarget, ctx.effect->CalculateSimpleValue(), true);
    return SpellHandlerOutcome<void>::Return();
// Gender spells
}

/// one body for 4 labels: 48762: A Fall from Grace: Scarlet Raven Priest Image - Master, 45759: Warsong Orc Disguise,
/// 69672: Sunreaver Disguise, 69673: Silver Covenant Disguise
static SpellHandlerOutcome<void> EffectScriptGenericGenderDisguise(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    uint8 gender = ctx.unitTarget->getGender();
    uint32 spellId;
    switch (ctx.m_spellInfo->ID)
    {
        case 48762: spellId = (gender == GENDER_MALE ? 48763 : 48761); break;
        case 45759: spellId = (gender == GENDER_MALE ? 45760 : 45762); break;
        case 69672: spellId = (gender == GENDER_MALE ? 70974 : 70973); break;
        case 69673: spellId = (gender == GENDER_MALE ? 70972 : 70971); break;
        default: return SpellHandlerOutcome<void>::Return();
    }
    ctx.unitTarget->CastSpell(ctx.unitTarget, spellId, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 48810: Death's Door
static SpellHandlerOutcome<void> EffectScriptGeneric48810(SpellEffectScriptContext& ctx)
{
    if (ctx.m_caster->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // Spell effect order will summon creature first and then apply invisibility to caster.
    // This result in that summoner/summoned can not see each other and that is not expected.
    // Aura from 48814 can be used as a hack from creature_addon, but we can not get the
    // summoned to cast this from this spell effect since we have no way to get pointer to creature.
    // Most proper would be to summon to same visibility mask as summoner, and not use spell at all.

    // Binding Life
    ctx.m_caster->CastSpell(ctx.m_caster, 48809, true);

    // After (after: meaning creature does not have auras at creation)
    // creature is summoned and visible for player in map, it is expected to
    // gain two auras. First from 29266(aura slot0) and then from 48808(aura slot1).
    // We have no pointer to summoned, so only 48808 is possible from this spell effect.

    // Binding Death
    ctx.m_caster->CastSpell(ctx.m_caster, 48808, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 48811: Despawn Forgotten Soul
static SpellHandlerOutcome<void> EffectScriptGeneric48811(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_UNIT)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    if (!((Creature*)ctx.unitTarget)->IsTemporarySummon())
    {
        return SpellHandlerOutcome<void>::Return();
    }

    TemporarySummon* pSummon = (TemporarySummon*)ctx.unitTarget;

    Unit::AuraList const& images = ctx.unitTarget->GetAurasByType(SPELL_AURA_MIRROR_IMAGE);

    if (images.empty())
    {
        return SpellHandlerOutcome<void>::Return();
    }

    Unit* pCaster = images.front()->GetCaster();
    Unit* pSummoner = ctx.unitTarget->GetMap()->GetUnit(pSummon->GetSummonerGuid());

    if (pSummoner && pSummoner == pCaster)
    {
        pSummon->UnSummon();
    }

    return SpellHandlerOutcome<void>::Return();
}

/// 48917: Who Are They: Cast from Questgiver
static SpellHandlerOutcome<void> EffectScriptGeneric48917(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget || ctx.unitTarget->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // Male Shadowy Disguise / Female Shadowy Disguise
    ctx.unitTarget->CastSpell(ctx.unitTarget, ctx.unitTarget->getGender() == GENDER_MALE ? 38080 : 38081, true);
    // Shadowy Disguise
    ctx.unitTarget->CastSpell(ctx.unitTarget, 32756, true);
    return SpellHandlerOutcome<void>::Return();
}

/// one body for 2 labels: 49380 and 59803: Consume
static SpellHandlerOutcome<void> EffectScriptGenericConsume(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // Each target hit buffs the caster
    ctx.unitTarget->CastSpell(ctx.m_caster, ctx.m_spellInfo->ID == 49380 ? 49381 : 59805, true, NULL, NULL, ctx.m_caster->GetObjectGuid());
    return SpellHandlerOutcome<void>::Return();
}

/// 49405: Invader Taunt Trigger
static SpellHandlerOutcome<void> EffectScriptGeneric49405(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.unitTarget->CastSpell(ctx.m_caster, ctx.effect->CalculateSimpleValue(), true);
    return SpellHandlerOutcome<void>::Return();
}

/// 50217: The Cleansing: Script Effect Player Cast Mirror Image
static SpellHandlerOutcome<void> EffectScriptGeneric50217(SpellEffectScriptContext& ctx)
{
    // Summon Your Inner Turmoil
    ctx.m_caster->CastSpell(ctx.m_caster, 50167, true);

    // Spell 50218 has TARGET_SCRIPT, but other wild summons near may exist, and then target can become wrong
    // Only way to make this safe is to get the actual summoned by m_caster

    // Your Inner Turmoil's Mirror Image Aura
    ctx.m_caster->CastSpell(ctx.m_caster, 50218, true);

    return SpellHandlerOutcome<void>::Return();
}

/// 50218: The Cleansing: Your Inner Turmoil's Mirror Image Aura
static SpellHandlerOutcome<void> EffectScriptGeneric50218(SpellEffectScriptContext& ctx)
{
    if (!ctx.m_originalCaster || ctx.m_originalCaster->GetTypeId() != TYPEID_PLAYER || !ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // determine if and what weapons can be copied
    switch(ctx.effect->EffectIndex)
    {
        case EFFECT_INDEX_1:
            if (((Player*)ctx.m_originalCaster)->GetInventoryMgr().GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND))
            {
                ctx.unitTarget->CastSpell(ctx.m_originalCaster, ctx.effect->CalculateSimpleValue(), true);
            }

            return SpellHandlerOutcome<void>::Return();
        case EFFECT_INDEX_2:
            if (((Player*)ctx.m_originalCaster)->GetInventoryMgr().GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND))
            {
                ctx.unitTarget->CastSpell(ctx.m_originalCaster, ctx.effect->CalculateSimpleValue(), true);
            }

            return SpellHandlerOutcome<void>::Return();
        default:
            return SpellHandlerOutcome<void>::Return();
    }
    return SpellHandlerOutcome<void>::Return();
}

/// 50238: The Cleansing: Your Inner Turmoil's On Death Cast on Master
static SpellHandlerOutcome<void> EffectScriptGeneric50238(SpellEffectScriptContext& ctx)
{
    if (ctx.m_caster->GetTypeId() != TYPEID_UNIT)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    if (((Creature*)ctx.m_caster)->IsTemporarySummon())
    {
        TemporarySummon* pSummon = (TemporarySummon*)ctx.m_caster;

        if (pSummon->GetSummonerGuid().IsPlayer())
        {
            if (Player* pSummoner = sObjectMgr.GetPlayer(pSummon->GetSummonerGuid()))
            {
                pSummoner->CastSpell(pSummoner, ctx.effect->CalculateSimpleValue(), true);
            }
        }
    }

    return SpellHandlerOutcome<void>::Return();
}

/// 50252: Blood Draw
static SpellHandlerOutcome<void> EffectScriptGeneric50252(SpellEffectScriptContext& ctx)
{
    ctx.m_caster->CastSpell(ctx.m_caster, 50250, true);
    return SpellHandlerOutcome<void>::Return();
}

/// one body for 2 labels: 50255 and 59331: Poisoned Spear
static SpellHandlerOutcome<void> EffectScriptGenericPoisonedSpear(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.unitTarget->CastSpell(ctx.unitTarget, ctx.effect->CalculateSimpleValue(), true, NULL, NULL, ctx.m_originalCasterGUID);
    return SpellHandlerOutcome<void>::Return();
}

/// 50439: Script Cast Summon Image of Drakuru 05
static SpellHandlerOutcome<void> EffectScriptGeneric50439(SpellEffectScriptContext& ctx)
{
    // TODO: check if summon already exist, if it does in this instance, return.

    // Summon Drakuru
    ctx.m_caster->CastSpell(ctx.m_caster, 50446, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 50630: Eject All Passengers
static SpellHandlerOutcome<void> EffectScriptGeneric50630(SpellEffectScriptContext& ctx)
{
    ctx.m_caster->RemoveSpellsCausingAura(SPELL_AURA_CONTROL_VEHICLE);
    return SpellHandlerOutcome<void>::Return();
}

/// 50725: Vigilance
static SpellHandlerOutcome<void> EffectScriptGeneric50725(SpellEffectScriptContext& ctx)
{
    Unit* caster = ctx.spell->GetAffectiveCaster();
    if (!caster || caster->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ((Player*)caster)->RemoveSpellCategoryCooldown(82, true);
    return SpellHandlerOutcome<void>::Return();
}

/// one body for 8 labels: 6201: Create Healthstone; 6202, 5699, 11729, 11730, 27230, 47871, 47878: absent from the
/// client's spell data
static SpellHandlerOutcome<void> EffectScriptWarlockCreateHealthstone(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    uint32 itemtype;
    uint32 rank = 0;
    Unit::AuraList const& mDummyAuras = ctx.unitTarget->GetAurasByType(SPELL_AURA_DUMMY);
    for (Unit::AuraList::const_iterator i = mDummyAuras.begin(); i != mDummyAuras.end(); ++i)
    {
        if ((*i)->GetId() == 18692)
        {
            rank = 1;
            break;
        }
        else if ((*i)->GetId() == 18693)
        {
            rank = 2;
            break;
        }
    }

    static uint32 const itypes[8][3] =
    {
        { 5512, 19004, 19005},              // Minor Healthstone
        { 5511, 19006, 19007},              // Lesser Healthstone
        { 5509, 19008, 19009},              // Healthstone
        { 5510, 19010, 19011},              // Greater Healthstone
        { 9421, 19012, 19013},              // Major Healthstone
        {22103, 22104, 22105},              // Master Healthstone
        {36889, 36890, 36891},              // Demonic Healthstone
        {36892, 36893, 36894}               // Fel Healthstone
    };

    switch (ctx.m_spellInfo->ID)
    {
        case  6201:
            itemtype = itypes[0][rank]; break; // Minor Healthstone
        case  6202:
            itemtype = itypes[1][rank]; break; // Lesser Healthstone
        case  5699:
            itemtype = itypes[2][rank]; break; // Healthstone
        case 11729:
            itemtype = itypes[3][rank]; break; // Greater Healthstone
        case 11730:
            itemtype = itypes[4][rank]; break; // Major Healthstone
        case 27230:
            itemtype = itypes[5][rank]; break; // Master Healthstone
        case 47871:
            itemtype = itypes[6][rank]; break; // Demonic Healthstone
        case 47878:
            itemtype = itypes[7][rank]; break; // Fel Healthstone
        default:
            return SpellHandlerOutcome<void>::Return();
    }
    ctx.spell->DoCreateItem( ctx.effect, itemtype );
    return SpellHandlerOutcome<void>::Return();
}

/// 47193: Demonic Empowerment
static SpellHandlerOutcome<void> EffectScriptWarlock47193(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    uint32 entry = ctx.unitTarget->GetEntry();
    uint32 spellID;
    switch (entry)
    {
        case   416: spellID = 54444; break; // imp
        case   417: spellID = 54509; break; // fellhunter
        case  1860: spellID = 54443; break; // void
        case  1863: spellID = 54435; break; // succubus
        case 17252: spellID = 54508; break; // fellguard
        default:
            return SpellHandlerOutcome<void>::Return();
    }
    ctx.unitTarget->CastSpell(ctx.unitTarget, spellID, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 47422: Everlasting Affliction
static SpellHandlerOutcome<void> EffectScriptWarlock47422(SpellEffectScriptContext& ctx)
{
    // Need refresh caster corruption auras on target
    Unit::SpellAuraHolderMap& suAuras = ctx.unitTarget->GetSpellAuraHolderMap();
    for (Unit::SpellAuraHolderMap::iterator itr = suAuras.begin(); itr != suAuras.end(); ++itr)
    {
        SpellEntry const *spellInfo = (*itr).second->GetSpellProto();
        SpellClassOptionsEntry const* eaClassOptions = spellInfo->GetSpellClassOptions();
        if (eaClassOptions && eaClassOptions->SpellClassSet == SPELLFAMILY_WARLOCK &&
            (eaClassOptions->SpellClassMask & UI64LIT(0x0000000000000002)) &&
           (*itr).second->GetCasterGuid() == ctx.m_caster->GetObjectGuid())
           (*itr).second->RefreshHolder();
    }
    return SpellHandlerOutcome<void>::Return();
}

/// 63521: absent from the client's spell data
static SpellHandlerOutcome<void> EffectScriptWarlock63521(SpellEffectScriptContext& ctx)
{
    // Divine Plea, refresh on target (3 aura slots)
    if (SpellAuraHolder* holder = ctx.unitTarget->GetSpellAuraHolder(54428))
    {
        holder->RefreshHolder();
    }

    return SpellHandlerOutcome<void>::Return();
}

/// 47948: Pain and Suffering
static SpellHandlerOutcome<void> EffectScriptPriest47948(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // Refresh Shadow Word: Pain on target
    Unit::SpellAuraHolderMap& auras = ctx.unitTarget->GetSpellAuraHolderMap();
    for (Unit::SpellAuraHolderMap::iterator itr = auras.begin(); itr != auras.end(); ++itr)
    {
        SpellEntry const *spellInfo = (*itr).second->GetSpellProto();
        SpellClassOptionsEntry const* swpClassOptions = spellInfo->GetSpellClassOptions();
        if (swpClassOptions && swpClassOptions->SpellClassSet == SPELLFAMILY_PRIEST &&
            (swpClassOptions->SpellClassMask & UI64LIT(0x0000000000008000)) &&
            (*itr).second->GetCasterGuid() == ctx.m_caster->GetObjectGuid())
        {
            (*itr).second->RefreshHolder();
            return SpellHandlerOutcome<void>::Return();
        }
    }
    return SpellHandlerOutcome<void>::Return();
}

/// every other id: nothing
static SpellHandlerOutcome<void> EffectScriptPriestDefault(SpellEffectScriptContext& /*ctx*/)
{
    return SpellHandlerOutcome<void>::Continue();
}

/// 53209: Chimera Shot
static SpellHandlerOutcome<void> EffectScriptHunter53209(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    uint32 spellId = 0;
    int32 basePoint = 0;
    Unit* target = ctx.unitTarget;
    Unit::SpellAuraHolderMap& Auras = ctx.unitTarget->GetSpellAuraHolderMap();
    for (Unit::SpellAuraHolderMap::iterator i = Auras.begin(); i != Auras.end(); ++i)
    {
        SpellAuraHolder* holder = i->second;
        if (holder->GetCasterGuid() != ctx.m_caster->GetObjectGuid())
        {
            continue;
        }

        // Search only Serpent Sting, Viper Sting, Scorpid Sting auras
        SpellClassOptionsEntry const* stingClassOptions = holder->GetSpellProto()->GetSpellClassOptions();
        if (!stingClassOptions || !stingClassOptions->SpellClassMask.IsFitToFamilyMask(UI64LIT(0x000000800000C000)))
        {
            continue;
        }

        // Refresh aura duration
        holder->RefreshHolder();

        Aura* aura = holder->GetAuraByEffectIndex(EFFECT_INDEX_0);

        if (!aura)
        {
            continue;
        }

        // Serpent Sting - Instantly deals 40% of the damage done by your Serpent Sting.
        if (stingClassOptions->IsFitToFamilyMask(UI64LIT(0x0000000000004000)))
        {
            // m_amount already include RAP bonus
            basePoint = aura->GetModifier()->m_amount * aura->GetAuraMaxTicks() * 40 / 100;
            spellId = 53353;                // Chimera Shot - Serpent
        }

        // Viper Sting - Instantly restores mana to you equal to 60% of the total amount drained by your Viper Sting.
        if (stingClassOptions->IsFitToFamilyMask(UI64LIT(0x0000008000000000)))
        {
            uint32 target_max_mana = ctx.unitTarget->GetMaxPower(POWER_MANA);
            if (!target_max_mana)
            {
                continue;
            }

            // ignore non positive values (can be result apply spellmods to aura damage
            uint32 pdamage = aura->GetModifier()->m_amount > 0 ? aura->GetModifier()->m_amount : 0;

            // Special case: draining x% of mana (up to a maximum of 2*x% of the caster's maximum mana)
            uint32 maxmana = ctx.m_caster->GetMaxPower(POWER_MANA)  * pdamage * 2 / 100;

            pdamage = target_max_mana * pdamage / 100;
            if (pdamage > maxmana)
            {
                pdamage = maxmana;
            }

            pdamage *= 4;                   // total aura damage
            basePoint = pdamage * 60 / 100;
            spellId = 53358;                // Chimera Shot - Viper
            target = ctx.m_caster;
        }

        // Scorpid Sting - Attempts to Disarm the target for 10 sec. This effect cannot occur more than once per 1 minute.
        if (stingClassOptions->IsFitToFamilyMask(UI64LIT(0x0000000000008000)))
        {
            spellId = 53359;                // Chimera Shot - Scorpid
        }
        // ?? nothing say in spell desc (possibly need addition check)
        // if ((familyFlag & UI64LIT(0x0000010000000000)) || // dot
        //    (familyFlag & UI64LIT(0x0000100000000000)))   // stun
        //{
        //    spellId = 53366; // 53366 Chimera Shot - Wyvern
        //}
    }

    if (spellId)
    {
        ctx.m_caster->CastCustomSpell(target, spellId, &basePoint, 0, 0, false);
    }

    return SpellHandlerOutcome<void>::Return();
}

/// 53412: Invigoration
static SpellHandlerOutcome<void> EffectScriptHunter53412(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    Unit::AuraList const& auras = ctx.unitTarget->GetAurasByType(SPELL_AURA_DUMMY);
    for (Unit::AuraList::const_iterator i = auras.begin(); i != auras.end(); ++i)
    {
        // Invigoration (master talent)
        if ((*i)->GetModifier()->m_miscvalue == 8 && (*i)->GetSpellProto()->SpellIconID == 3487)
        {
            if (roll_chance_i((*i)->GetModifier()->m_amount))
            {
                ctx.unitTarget->CastSpell(ctx.unitTarget, 53398, true, NULL, (*i), ctx.m_caster->GetObjectGuid());
                break;
            }
        }
    }
    return SpellHandlerOutcome<void>::Return();
}

/// 53271: Master's Call
static SpellHandlerOutcome<void> EffectScriptHunter53271(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // script effect have in value, but this outdated removed part
    ctx.unitTarget->CastSpell(ctx.unitTarget, 62305, true);
    return SpellHandlerOutcome<void>::Return();
}

/// every other id: nothing
static SpellHandlerOutcome<void> EffectScriptHunterDefault(SpellEffectScriptContext& /*ctx*/)
{
    return SpellHandlerOutcome<void>::Continue();
}

/// 28698: Dreaming Glory
static SpellHandlerOutcome<void> EffectScriptPotion28698(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.unitTarget->CastSpell(ctx.unitTarget, 28694, true);
    return SpellHandlerOutcome<void>::Continue();
}

/// 28702: Netherbloom
static SpellHandlerOutcome<void> EffectScriptPotion28702(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // 25% chance of casting a random buff
    if (roll_chance_i(75))
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // triggered spells are 28703 to 28707
    // Note: some sources say, that there was the possibility of
    //       receiving a debuff. However, this seems to be removed by a patch.
    const uint32 spellid = 28703;

    // don't overwrite an existing aura
    for (uint8 i = 0; i < 5; ++i)
        if (ctx.unitTarget->HasAura(spellid + i, EFFECT_INDEX_0))
        {
            return SpellHandlerOutcome<void>::Return();
        }

    ctx.unitTarget->CastSpell(ctx.unitTarget, spellid + urand(0, 4), true);
    return SpellHandlerOutcome<void>::Continue();
}

/// 28720: Nightmare Vine
static SpellHandlerOutcome<void> EffectScriptPotion28720(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // 25% chance of casting Nightmare Pollen
    if (roll_chance_i(75))
    {
        return SpellHandlerOutcome<void>::Return();
    }

    ctx.unitTarget->CastSpell(ctx.unitTarget, 28721, true);
    return SpellHandlerOutcome<void>::Continue();
}

/// 50842: Pestilence
static SpellHandlerOutcome<void> EffectScriptDeathKnight50842(SpellEffectScriptContext& ctx)
{
    if (!ctx.unitTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    Unit* mainTarget = ctx.m_targets.getUnitTarget();
    if (!mainTarget)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // do only refresh diseases on main target if caster has Glyph of Disease
    if (mainTarget == ctx.unitTarget && !ctx.m_caster->HasAura(63334))
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // Blood Plague
    if (mainTarget->HasAura(55078))
    {
        ctx.m_caster->CastSpell(ctx.unitTarget, 55078, true);
    }

    // Frost Fever
    if (mainTarget->HasAura(55095))
    {
        ctx.m_caster->CastSpell(ctx.unitTarget, 55095, true);
    }

    return SpellHandlerOutcome<void>::Continue();
}

template <class Site>
struct SpellEffectScriptRow
{
    uint32 spellId;
    typename SpellHandler<Site>::Function function;
};

/// Registers every row of one site's table on `registry`; answers the number of rows.
template <class Site, std::size_t N>
static uint32 RegisterSpellEffectScriptRows(SpellHandlerRegistry& registry, SpellEffectScriptRow<Site> const (&rows)[N])
{
    for (SpellEffectScriptRow<Site> const& row : rows)
    {
        registry.Register<Site>(row.spellId, row.function);
    }
    return uint32(N);
}

uint32 RegisterSpellEffectScriptHandlers(SpellHandlerRegistry& registry)
{
    static SpellEffectScriptRow<SpellEffectScriptGenericSite> const generic[] =
    {
        { 8856, &EffectScriptGeneric8856 },
        { 17512, &EffectScriptGeneric17512 },
        { 20589, &EffectScriptGeneric20589 },
        { 22539, &EffectScriptGenericShadowFlame },
        { 22972, &EffectScriptGenericShadowFlame },
        { 22975, &EffectScriptGenericShadowFlame },
        { 22976, &EffectScriptGenericShadowFlame },
        { 22977, &EffectScriptGenericShadowFlame },
        { 22978, &EffectScriptGenericShadowFlame },
        { 22979, &EffectScriptGenericShadowFlame },
        { 22980, &EffectScriptGenericShadowFlame },
        { 22981, &EffectScriptGenericShadowFlame },
        { 22982, &EffectScriptGenericShadowFlame },
        { 22983, &EffectScriptGenericShadowFlame },
        { 22984, &EffectScriptGenericShadowFlame },
        { 22985, &EffectScriptGenericShadowFlame },
        { 24194, &EffectScriptGenericTribute },
        { 24195, &EffectScriptGenericTribute },
        { 24320, &EffectScriptGeneric24320 },
        { 24324, &EffectScriptGeneric24324 },
        { 24590, &EffectScriptGeneric24590 },
        { 24714, &EffectScriptGeneric24714 },
        { 24717, &EffectScriptGeneric24717 },
        { 24718, &EffectScriptGeneric24718 },
        { 24719, &EffectScriptGeneric24719 },
        { 24720, &EffectScriptGeneric24720 },
        { 24737, &EffectScriptGeneric24737 },
        { 24751, &EffectScriptGeneric24751 },
        { 25140, &EffectScriptGenericOrbTeleport },
        { 25143, &EffectScriptGenericOrbTeleport },
        { 25650, &EffectScriptGenericOrbTeleport },
        { 25652, &EffectScriptGenericOrbTeleport },
        { 29128, &EffectScriptGenericOrbTeleport },
        { 29129, &EffectScriptGenericOrbTeleport },
        { 35376, &EffectScriptGenericOrbTeleport },
        { 35727, &EffectScriptGenericOrbTeleport },
        { 26004, &EffectScriptGeneric26004 },
        { 26137, &EffectScriptGeneric26137 },
        { 26218, &EffectScriptGeneric26218 },
        { 26275, &EffectScriptGeneric26275 },
        { 26465, &EffectScriptGeneric26465 },
        { 26656, &EffectScriptGeneric26656 },
        { 27687, &EffectScriptGeneric27687 },
        { 27695, &EffectScriptGeneric27695 },
        { 28352, &EffectScriptGeneric28352 },
        { 28374, &EffectScriptGenericDecimate },
        { 54426, &EffectScriptGenericDecimate },
        { 71123, &EffectScriptGenericDecimate },
        { 28560, &EffectScriptGeneric28560 },
        { 28859, &EffectScriptGenericCleansingFlames },
        { 29126, &EffectScriptGenericCleansingFlames },
        { 29135, &EffectScriptGenericCleansingFlames },
        { 29136, &EffectScriptGenericCleansingFlames },
        { 29137, &EffectScriptGenericCleansingFlames },
        { 29138, &EffectScriptGenericCleansingFlames },
        { 29139, &EffectScriptGenericCleansingFlames },
        { 46671, &EffectScriptGenericCleansingFlames },
        { 46672, &EffectScriptGenericCleansingFlames },
        { 29395, &EffectScriptGeneric29395 },
        { 29830, &EffectScriptGeneric29830 },
        { 30541, &EffectScriptGeneric30541 },
        { 30469, &EffectScriptGeneric30469 },
        { 30769, &EffectScriptGeneric30769 },
        { 30835, &EffectScriptGeneric30835 },
        { 30918, &EffectScriptGeneric30918 },
        { 37142, &EffectScriptGenericChessMeleeAttack },
        { 37143, &EffectScriptGenericChessMeleeAttack },
        { 37147, &EffectScriptGenericChessMeleeAttack },
        { 37149, &EffectScriptGenericChessMeleeAttack },
        { 37150, &EffectScriptGenericChessMeleeAttack },
        { 37220, &EffectScriptGenericChessMeleeAttack },
        { 32227, &EffectScriptGenericChessMeleeAttack },
        { 32228, &EffectScriptGenericChessMeleeAttack },
        { 37337, &EffectScriptGenericChessMeleeAttack },
        { 37339, &EffectScriptGenericChessMeleeAttack },
        { 37345, &EffectScriptGenericChessMeleeAttack },
        { 37348, &EffectScriptGenericChessMeleeAttack },
        { 32301, &EffectScriptGeneric32301 },
        { 33676, &EffectScriptGeneric33676 },
        { 34653, &EffectScriptGenericFireball },
        { 36920, &EffectScriptGenericFireball },
        { 35865, &EffectScriptGeneric35865 },
        { 37431, &EffectScriptGeneric37431 },
        { 37775, &EffectScriptGeneric37775 },
        { 37824, &EffectScriptGeneric37824 },
        { 38358, &EffectScriptGeneric38358 },
        { 39338, &EffectScriptGenericHandOfMedivh },
        { 39342, &EffectScriptGenericHandOfMedivh },
        { 39341, &EffectScriptGenericFuryOfMedivh },
        { 39344, &EffectScriptGenericFuryOfMedivh },
        { 41055, &EffectScriptGenericCopyWeapon },
        { 63416, &EffectScriptGenericCopyWeapon },
        { 69891, &EffectScriptGenericCopyWeapon },
        { 41072, &EffectScriptGeneric41072 },
        { 41126, &EffectScriptGeneric41126 },
        { 41213, &EffectScriptGenericThrowShield },
        { 43416, &EffectScriptGenericThrowShield },
        { 69222, &EffectScriptGenericThrowShield },
        { 73076, &EffectScriptGenericThrowShield },
        { 42281, &EffectScriptGeneric42281 },
        { 42492, &EffectScriptGeneric42492 },
        { 42578, &EffectScriptGeneric42578 },
        { 43365, &EffectScriptGeneric43365 },
        { 43375, &EffectScriptGeneric43375 },
        { 44323, &EffectScriptGenericHawkHunting },
        { 44407, &EffectScriptGenericHawkHunting },
        { 44364, &EffectScriptGeneric44364 },
        { 44462, &EffectScriptGeneric44462 },
        { 44876, &EffectScriptGeneric44876 },
        { 44811, &EffectScriptGeneric44811 },
        { 45141, &EffectScriptGeneric45141 },
        { 45151, &EffectScriptGeneric45151 },
        { 45185, &EffectScriptGeneric45185 },
        { 45204, &EffectScriptGeneric45204 },
        { 45206, &EffectScriptGenericCopyOffHandWeapon },
        { 69892, &EffectScriptGenericCopyOffHandWeapon },
        { 45235, &EffectScriptGeneric45235 },
        { 45260, &EffectScriptGeneric45260 },
        { 45313, &EffectScriptGeneric45313 },
        { 45625, &EffectScriptGeneric45625 },
        { 45668, &EffectScriptGeneric45668 },
        { 45691, &EffectScriptGeneric45691 },
        { 45713, &EffectScriptGeneric45713 },
        { 45714, &EffectScriptGeneric45714 },
        { 45717, &EffectScriptGeneric45717 },
        { 45785, &EffectScriptGeneric45785 },
        { 45833, &EffectScriptGeneric45833 },
        { 45892, &EffectScriptGeneric45892 },
        { 45918, &EffectScriptGeneric45918 },
        { 45958, &EffectScriptGeneric45958 },
        { 46203, &EffectScriptGeneric46203 },
        { 46289, &EffectScriptGeneric46289 },
        { 46430, &EffectScriptGeneric46430 },
        { 46642, &EffectScriptGeneric46642 },
        { 47097, &EffectScriptGeneric47097 },
        { 47311, &EffectScriptGeneric47311 },
        { 47393, &EffectScriptGeneric47393 },
        { 47615, &EffectScriptGeneric47615 },
        { 47638, &EffectScriptGeneric47638 },
        { 47703, &EffectScriptGeneric47703 },
        { 47724, &EffectScriptGeneric47724 },
        { 47958, &EffectScriptGenericCrystalSpikes },
        { 57083, &EffectScriptGenericCrystalSpikes },
        { 48590, &EffectScriptGeneric48590 },
        { 48603, &EffectScriptGeneric48603 },
        { 48724, &EffectScriptGenericDenouncementOnDeath },
        { 48726, &EffectScriptGenericDenouncementOnDeath },
        { 48728, &EffectScriptGenericDenouncementOnDeath },
        { 48730, &EffectScriptGenericDenouncementOnDeath },
        { 48762, &EffectScriptGenericGenderDisguise },
        { 45759, &EffectScriptGenericGenderDisguise },
        { 69672, &EffectScriptGenericGenderDisguise },
        { 69673, &EffectScriptGenericGenderDisguise },
        { 48810, &EffectScriptGeneric48810 },
        { 48811, &EffectScriptGeneric48811 },
        { 48917, &EffectScriptGeneric48917 },
        { 49380, &EffectScriptGenericConsume },
        { 59803, &EffectScriptGenericConsume },
        { 49405, &EffectScriptGeneric49405 },
        { 50217, &EffectScriptGeneric50217 },
        { 50218, &EffectScriptGeneric50218 },
        { 50238, &EffectScriptGeneric50238 },
        { 50252, &EffectScriptGeneric50252 },
        { 50255, &EffectScriptGenericPoisonedSpear },
        { 59331, &EffectScriptGenericPoisonedSpear },
        { 50439, &EffectScriptGeneric50439 },
        { 50630, &EffectScriptGeneric50630 },
        { 50725, &EffectScriptGeneric50725 },
    };

    static SpellEffectScriptRow<SpellEffectScriptWarlockSite> const warlock[] =
    {
        { 6201, &EffectScriptWarlockCreateHealthstone },
        { 6202, &EffectScriptWarlockCreateHealthstone },
        { 5699, &EffectScriptWarlockCreateHealthstone },
        { 11729, &EffectScriptWarlockCreateHealthstone },
        { 11730, &EffectScriptWarlockCreateHealthstone },
        { 27230, &EffectScriptWarlockCreateHealthstone },
        { 47871, &EffectScriptWarlockCreateHealthstone },
        { 47878, &EffectScriptWarlockCreateHealthstone },
        { 47193, &EffectScriptWarlock47193 },
        { 47422, &EffectScriptWarlock47422 },
        { 63521, &EffectScriptWarlock63521 },
    };

    static SpellEffectScriptRow<SpellEffectScriptPriestSite> const priest[] =
    {
        { 47948, &EffectScriptPriest47948 },
    };

    static SpellEffectScriptRow<SpellEffectScriptHunterSite> const hunter[] =
    {
        { 53209, &EffectScriptHunter53209 },
        { 53412, &EffectScriptHunter53412 },
        { 53271, &EffectScriptHunter53271 },
    };

    static SpellEffectScriptRow<SpellEffectScriptPotionSite> const potion[] =
    {
        { 28698, &EffectScriptPotion28698 },
        { 28702, &EffectScriptPotion28702 },
        { 28720, &EffectScriptPotion28720 },
    };

    static SpellEffectScriptRow<SpellEffectScriptDeathKnightSite> const deathKnight[] =
    {
        { 50842, &EffectScriptDeathKnight50842 },
    };

    uint32 rows = RegisterSpellEffectScriptRows(registry, generic);
    rows += RegisterSpellEffectScriptRows(registry, warlock);
    rows += RegisterSpellEffectScriptRows(registry, priest);
    rows += RegisterSpellEffectScriptRows(registry, hunter);
    rows += RegisterSpellEffectScriptRows(registry, potion);
    rows += RegisterSpellEffectScriptRows(registry, deathKnight);
    registry.RegisterDefault<SpellEffectScriptPriestSite>(&EffectScriptPriestDefault);
    registry.RegisterDefault<SpellEffectScriptHunterSite>(&EffectScriptHunterDefault);
    return rows + 2;
}
