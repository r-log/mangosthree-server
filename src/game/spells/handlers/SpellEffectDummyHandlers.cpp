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
#include "WorldHandlers/Spell.h"
#include "Object/Pet.h"
#include "Object/SpellMgr.h"
#include "Server/DBCStores.h"
#include "Utilities/Util.h"
#include "entities/player/Player.h"
#include <algorithm>
#include <iterator>

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
            ((Player*)ctx.m_caster)->RemoveSpellCooldown((itr++)->first, true);
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
            ((Player*)ctx.m_caster)->RemoveSpellCooldown((itr++)->first,true);
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
            ((Player*)ctx.m_caster)->RemoveSpellCooldown((itr++)->first,true);
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
    ((Player*)ctx.m_caster)->SendAttackSwingCancelAttack();
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
        ((Player*)ctx.m_caster)->RemoveSpellCooldown(ctx.m_spellInfo->ID, true);
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

    uint32 rows = RegisterSpellEffectDummyRows(registry, mage);
    rows += RegisterSpellEffectDummyRows(registry, warrior);
    rows += RegisterSpellEffectDummyRows(registry, rogue);
    rows += RegisterSpellEffectDummyRows(registry, hunter);
    rows += RegisterSpellEffectDummyRows(registry, paladin);
    return rows;
}
