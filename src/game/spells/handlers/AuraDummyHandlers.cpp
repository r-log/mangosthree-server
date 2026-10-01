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
#include "WorldHandlers/SpellAuras.h"

/// SPELLFAMILY_WARRIOR 41099: Battle Stance
static SpellHandlerOutcome<void> AuraDummyApplyWarrior41099(AuraDummyApplyContext& ctx)
{
    if (ctx.target->GetTypeId() != TYPEID_UNIT)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // Stance Cooldown
    ctx.target->CastSpell(ctx.target, 41102, true, NULL, ctx.aura);

    // Battle Aura
    ctx.target->CastSpell(ctx.target, 41106, true, NULL, ctx.aura);

    // equipment
    ((Creature*)ctx.target)->SetVirtualItem(VIRTUAL_ITEM_SLOT_0, 32614);
    ((Creature*)ctx.target)->SetVirtualItem(VIRTUAL_ITEM_SLOT_1, 0);
    ((Creature*)ctx.target)->SetVirtualItem(VIRTUAL_ITEM_SLOT_2, 0);
    return SpellHandlerOutcome<void>::Return();
}

/// SPELLFAMILY_WARRIOR 41100: Berserker Stance
static SpellHandlerOutcome<void> AuraDummyApplyWarrior41100(AuraDummyApplyContext& ctx)
{
    if (ctx.target->GetTypeId() != TYPEID_UNIT)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // Stance Cooldown
    ctx.target->CastSpell(ctx.target, 41102, true, NULL, ctx.aura);

    // Berserker Aura
    ctx.target->CastSpell(ctx.target, 41107, true, NULL, ctx.aura);

    // equipment
    ((Creature*)ctx.target)->SetVirtualItem(VIRTUAL_ITEM_SLOT_0, 32614);
    ((Creature*)ctx.target)->SetVirtualItem(VIRTUAL_ITEM_SLOT_1, 0);
    ((Creature*)ctx.target)->SetVirtualItem(VIRTUAL_ITEM_SLOT_2, 0);
    return SpellHandlerOutcome<void>::Return();
}

/// SPELLFAMILY_WARRIOR 41101: Defensive Stance
static SpellHandlerOutcome<void> AuraDummyApplyWarrior41101(AuraDummyApplyContext& ctx)
{
    if (ctx.target->GetTypeId() != TYPEID_UNIT)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // Stance Cooldown
    ctx.target->CastSpell(ctx.target, 41102, true, NULL, ctx.aura);

    // Defensive Aura
    ctx.target->CastSpell(ctx.target, 41105, true, NULL, ctx.aura);

    // equipment
    ((Creature*)ctx.target)->SetVirtualItem(VIRTUAL_ITEM_SLOT_0, 32604);
    ((Creature*)ctx.target)->SetVirtualItem(VIRTUAL_ITEM_SLOT_1, 31467);
    ((Creature*)ctx.target)->SetVirtualItem(VIRTUAL_ITEM_SLOT_2, 0);
    return SpellHandlerOutcome<void>::Return();
}

/// SPELLFAMILY_WARRIOR 53790: Defensive Stance
static SpellHandlerOutcome<void> AuraDummyApplyWarrior53790(AuraDummyApplyContext& ctx)
{
    if (ctx.target->GetTypeId() != TYPEID_UNIT)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // Stance Cooldown
    ctx.target->CastSpell(ctx.target, 59526, true, NULL, ctx.aura);

    // Defensive Aura
    ctx.target->CastSpell(ctx.target, 41105, true, NULL, ctx.aura);

    // equipment
    ((Creature*)ctx.target)->SetVirtualItem(VIRTUAL_ITEM_SLOT_0, 43625);
    ((Creature*)ctx.target)->SetVirtualItem(VIRTUAL_ITEM_SLOT_1, 39384);
    ((Creature*)ctx.target)->SetVirtualItem(VIRTUAL_ITEM_SLOT_2, 0);
    return SpellHandlerOutcome<void>::Return();
}

/// SPELLFAMILY_WARRIOR 53791: Berserker Stance
static SpellHandlerOutcome<void> AuraDummyApplyWarrior53791(AuraDummyApplyContext& ctx)
{
    if (ctx.target->GetTypeId() != TYPEID_UNIT)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // Stance Cooldown
    ctx.target->CastSpell(ctx.target, 59526, true, NULL, ctx.aura);

    // Berserker Aura
    ctx.target->CastSpell(ctx.target, 41107, true, NULL, ctx.aura);

    // equipment
    ((Creature*)ctx.target)->SetVirtualItem(VIRTUAL_ITEM_SLOT_0, 43625);
    ((Creature*)ctx.target)->SetVirtualItem(VIRTUAL_ITEM_SLOT_1, 43625);
    ((Creature*)ctx.target)->SetVirtualItem(VIRTUAL_ITEM_SLOT_2, 0);
    return SpellHandlerOutcome<void>::Return();
}

/// SPELLFAMILY_WARRIOR 53792: Battle Stance
static SpellHandlerOutcome<void> AuraDummyApplyWarrior53792(AuraDummyApplyContext& ctx)
{
    if (ctx.target->GetTypeId() != TYPEID_UNIT)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    // Stance Cooldown
    ctx.target->CastSpell(ctx.target, 59526, true, NULL, ctx.aura);

    // Battle Aura
    ctx.target->CastSpell(ctx.target, 41106, true, NULL, ctx.aura);

    // equipment
    ((Creature*)ctx.target)->SetVirtualItem(VIRTUAL_ITEM_SLOT_0, 43623);
    ((Creature*)ctx.target)->SetVirtualItem(VIRTUAL_ITEM_SLOT_1, 0);
    ((Creature*)ctx.target)->SetVirtualItem(VIRTUAL_ITEM_SLOT_2, 0);
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
    static AuraDummyRow<AuraDummyApplyWarriorSite> const warriorApply[] =
    {
        { 41099, &AuraDummyApplyWarrior41099 },
        { 41100, &AuraDummyApplyWarrior41100 },
        { 41101, &AuraDummyApplyWarrior41101 },
        { 53790, &AuraDummyApplyWarrior53790 },
        { 53791, &AuraDummyApplyWarrior53791 },
        { 53792, &AuraDummyApplyWarrior53792 },
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
        { 43969, &AuraDummyRemove43969 },
        { 45934, &AuraDummyRemove45934 },
        { 45963, &AuraDummyRemove45963 },
        { 46308, &AuraDummyRemove46308 },
        { 46637, &AuraDummyRemove46637 },
        { 50141, &AuraDummyRemove50141 },
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

    static AuraDummyRow<AuraDummyApplyRemoveGenericSite> const feignDeath[] =
    {
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
    };

    static AuraDummyRow<AuraDummyDruidSite> const druid[] =
    {
        { 52610, &AuraDummyDruid52610 },
        { 61336, &AuraDummyDruid61336 },
    };

    uint32 rows = RegisterAuraDummyRows(registry, warriorApply);
    rows += RegisterAuraDummyRows(registry, questTame);
    rows += RegisterAuraDummyRows(registry, removal);
    rows += RegisterAuraDummyRows(registry, feignDeath);
    rows += RegisterAuraDummyRows(registry, druid);
    return rows;
}
