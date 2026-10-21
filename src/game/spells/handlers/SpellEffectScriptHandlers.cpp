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
#include "Utilities/MathDefines.h"
#include "Utilities/Util.h"
#include "WorldHandlers/GridMap.h"
#include "WorldHandlers/SpellAuras.h"

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
