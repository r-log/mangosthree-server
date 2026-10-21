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
#include "Utilities/Util.h"
#include "WorldHandlers/SpellAuras.h"

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

    uint32 rows = RegisterSpellEffectScriptRows(registry, warlock);
    rows += RegisterSpellEffectScriptRows(registry, priest);
    rows += RegisterSpellEffectScriptRows(registry, hunter);
    rows += RegisterSpellEffectScriptRows(registry, potion);
    rows += RegisterSpellEffectScriptRows(registry, deathKnight);
    registry.RegisterDefault<SpellEffectScriptPriestSite>(&EffectScriptPriestDefault);
    registry.RegisterDefault<SpellEffectScriptHunterSite>(&EffectScriptHunterDefault);
    return rows + 2;
}
