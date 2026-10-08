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

#include "spells/handlers/SpellEffectDamageTeleportHandlers.h"
#include "spells/handlers/SpellHandlerRegistry.h"
#include "WorldHandlers/Spell.h"
#include "Object/Pet.h"
#include "Object/SpellMgr.h"
#include "Utilities/Util.h"
#include "WorldHandlers/SpellAuras.h"
#include "entities/player/Player.h"
#include <algorithm>

/// 24340 to 109184, sixty spells: Meteor, Shard of the Fallen Star, Malevolent Cleave, Dive Bomb, Saber Lash,
/// Brutal Strike, Meteor Slash, Sonic Screech, Ooze Eruption, Chaos Bane, Scorching Blast, Caustic Slime, Twilight
/// Meteorite, Sleet Storm, Blackout, Demon Repellent Ray, Flame Scythe, Stomp, Seething Hate and Twilight
/// Instability: the damage is divided among the targets the effect hits
static SpellHandlerOutcome<void> EffectSchoolDmgDivideAmongTargets(SpellEffectSchoolDmgContext& ctx)
{
    // Meteor like spells (divided damage to targets)
    uint32 count = 0;
    for(Spell::TargetList::const_iterator ihit = ctx.m_UniqueTargetInfo.begin(); ihit != ctx.m_UniqueTargetInfo.end(); ++ihit)
        if (ihit->effectMask & (1<<ctx.effect->EffectIndex))
        {
            ++count;
        }

    ctx.damage /= count;                    // divide to all targets
    return SpellHandlerOutcome<void>::Continue();
// percent from health with min
}

/// 25599: Thundercrash: the damage is half the target's health, at least 200
static SpellHandlerOutcome<void> EffectSchoolDmgThundercrash(SpellEffectSchoolDmgContext& ctx)
{
    ctx.damage = ctx.unitTarget->GetHealth() / 2;
    if (ctx.damage < 200)
    {
        ctx.damage = 200;
    }
    return SpellHandlerOutcome<void>::Continue();
// Intercept (warrior spell trigger)
}

/// 20253, 61491: Intercept: the damage gains 12% of the caster's attack power
static SpellHandlerOutcome<void> EffectSchoolDmgIntercept(SpellEffectSchoolDmgContext& ctx)
{
    ctx.damage += uint32(ctx.m_caster->GetTotalAttackPowerValue(BASE_ATTACK) * 0.12f);
    return SpellHandlerOutcome<void>::Continue();
// percent max target health
}

/// 29142: Eyesore Blaster; 35139: Throw Boom's Doom; 49882: Leviroth Self-Impale; 55269: Deathly Stare: the
/// damage is that percent of the target's maximum health
static SpellHandlerOutcome<void> EffectSchoolDmgPercentOfMaxHealth(SpellEffectSchoolDmgContext& ctx)
{
    ctx.damage = ctx.damage * ctx.unitTarget->GetMaxHealth() / 100;
    return SpellHandlerOutcome<void>::Continue();
// Lightning Strike
}

/// 37841: Lightning Strike: a player target under Repolarized Magneto Sphere (37830) is given the kill credit of
/// creature 21910
static SpellHandlerOutcome<void> EffectSchoolDmgLightningStrike(SpellEffectSchoolDmgContext& ctx)
{
    if (ctx.unitTarget->GetTypeId() == TYPEID_PLAYER && ctx.unitTarget->HasAura(37830)) // Repolarized Magneto Sphere
    {
        ((Player*)ctx.unitTarget)->KilledMonsterCredit(21910);
    }
    return SpellHandlerOutcome<void>::Continue();
// Cataclysmic Bolt
// Cataclysmic Bolt
}

/// 38441: Cataclysmic Bolt: the damage is half the target's maximum health
static SpellHandlerOutcome<void> EffectSchoolDmgCataclysmicBolt(SpellEffectSchoolDmgContext& ctx)
{
    ctx.damage = ctx.unitTarget->GetMaxHealth() / 2;
    return SpellHandlerOutcome<void>::Continue();
// Touch the Nightmare
}

/// 50341: Touch the Nightmare: the third effect's damage is 30% of the target's maximum health
static SpellHandlerOutcome<void> EffectSchoolDmgTouchTheNightmare(SpellEffectSchoolDmgContext& ctx)
{
    if (SpellEffectIndex(ctx.effect->EffectIndex) == EFFECT_INDEX_2)
    {
        ctx.damage = int32(ctx.unitTarget->GetMaxHealth() * 0.3f);
    }
    return SpellHandlerOutcome<void>::Continue();
// Tympanic Tantrum
}

/// 62775: Tympanic Tantrum: the damage is a tenth of the target's maximum health
static SpellHandlerOutcome<void> EffectSchoolDmgTympanicTantrum(SpellEffectSchoolDmgContext& ctx)
{
    ctx.damage = ctx.unitTarget->GetMaxHealth() / 10;
    return SpellHandlerOutcome<void>::Continue();
// Hand of Rekoning (name not have typos ;) )
}

/// 67485: Hand of Reckoning, absent from the client's spell data: the damage gains half the caster's attack
/// power
static SpellHandlerOutcome<void> EffectSchoolDmgHandOfReckoning(SpellEffectSchoolDmgContext& ctx)
{
    ctx.damage += uint32(0.5f * ctx.m_caster->GetTotalAttackPowerValue(BASE_ATTACK));
    return SpellHandlerOutcome<void>::Continue();
// Magic Bane normal (Forge of Souls - Bronjahm)
}

/// 68793: Magic's Bane: the damage gains half the target's maximum mana, at most 10000 in all
static SpellHandlerOutcome<void> EffectSchoolDmgMagicBaneNormal(SpellEffectSchoolDmgContext& ctx)
{
    ctx.damage += uint32(ctx.unitTarget->GetMaxPower(POWER_MANA) / 2);
    ctx.damage = std::min(ctx.damage, 10000);
    return SpellHandlerOutcome<void>::Continue();
// Magic Bane heroic (Forge of Souls - Bronjahm)
}

/// 69050: Magic's Bane, the heroic rank: the damage gains half the target's maximum mana, at most 15000 in all
static SpellHandlerOutcome<void> EffectSchoolDmgMagicBaneHeroic(SpellEffectSchoolDmgContext& ctx)
{
    ctx.damage += uint32(ctx.unitTarget->GetMaxPower(POWER_MANA) / 2);
    ctx.damage = std::min(ctx.damage, 15000);
    return SpellHandlerOutcome<void>::Continue();
}

/// 18461, triggered by Vanish (1856, 27617, 44290): the target sheds roots, slows and stalking; a player target
/// has Stealth's (1784) cooldown reset and is given Stealth by the caster; the effect ends
static SpellHandlerOutcome<void> EffectTriggerSpellVanish(SpellEffectTriggerSpellContext& ctx)
{
    ctx.unitTarget->RemoveSpellsCausingAura(SPELL_AURA_MOD_ROOT);
    ctx.unitTarget->RemoveSpellsCausingAura(SPELL_AURA_MOD_DECREASE_SPEED);
    ctx.unitTarget->RemoveSpellsCausingAura(SPELL_AURA_MOD_STALKED);

    // if this spell is given to NPC it must handle rest by it's own AI
    if (ctx.unitTarget->GetTypeId() != TYPEID_PLAYER)
    {
        return SpellHandlerOutcome<void>::Return();
    }

    uint32 spellId = 1784;
    // reset cooldown on it if needed
    if (((Player*)ctx.unitTarget)->HasSpellCooldown(spellId))
    {
        ((Player*)ctx.unitTarget)->RemoveSpellCooldown(spellId);
    }

    ctx.m_caster->CastSpell(ctx.unitTarget, spellId, true);
    return SpellHandlerOutcome<void>::Return();
}

/// 29284, triggered by Brittle Armor (24574): the caster casts Brittle Armor (24575) on the target; the effect
/// ends
static SpellHandlerOutcome<void> EffectTriggerSpellBrittleArmor(SpellEffectTriggerSpellContext& ctx)
{
    ctx.m_caster->CastSpell(ctx.unitTarget, 24575, true, ctx.m_CastItem, NULL, ctx.m_originalCasterGUID);
    return SpellHandlerOutcome<void>::Return();
}

/// 29286, triggered by Mercurial Shield (26463): the caster casts Mercurial Shield (26464) on the target; the
/// effect ends
static SpellHandlerOutcome<void> EffectTriggerSpellMercurialShield(SpellEffectTriggerSpellContext& ctx)
{
    ctx.m_caster->CastSpell(ctx.unitTarget, 26464, true, ctx.m_CastItem, NULL, ctx.m_originalCasterGUID);
    return SpellHandlerOutcome<void>::Return();
}

/// 31980, triggered by Righteous Defense (31789): the caster casts Righteous Defense (31790) on the target; the
/// effect ends
static SpellHandlerOutcome<void> EffectTriggerSpellRighteousDefense(SpellEffectTriggerSpellContext& ctx)
{
    ctx.m_caster->CastSpell(ctx.unitTarget, 31790, true, ctx.m_CastItem, NULL, ctx.m_originalCasterGUID);
    return SpellHandlerOutcome<void>::Return();
}

/// 35729, triggered by Cloak of Shadows (31224, 39666, 65961): each harmful aura on the target that is not
/// passive, not death-persistent and not physical is removed from the caster; the effect ends
static SpellHandlerOutcome<void> EffectTriggerSpellCloakOfShadows(SpellEffectTriggerSpellContext& ctx)
{
    Unit::SpellAuraHolderMap& Auras = ctx.unitTarget->GetSpellAuraHolderMap();
    for (Unit::SpellAuraHolderMap::iterator iter = Auras.begin(); iter != Auras.end(); ++iter)
    {
        // Remove all harmful spells on you except positive/passive/physical auras
        if (!iter->second->IsPositive() &&
                !iter->second->IsPassive() &&
                !iter->second->IsDeathPersistent() &&
                (GetSpellSchoolMask(iter->second->GetSpellProto()) & SPELL_SCHOOL_MASK_NORMAL) == 0)
        {
            ctx.m_caster->RemoveAurasDueToSpell(iter->second->GetSpellProto()->ID);
            iter = Auras.begin();
        }
    }
    return SpellHandlerOutcome<void>::Return();
}

/// 41967, triggered by Shadowfiend (34433): the target's pet casts Mana Leech (28305) on itself; the effect ends
static SpellHandlerOutcome<void> EffectTriggerSpellShadowfiend(SpellEffectTriggerSpellContext& ctx)
{
    if (Unit* pet = ctx.unitTarget->GetPet())
    {
        pet->CastSpell(pet, 28305, true);
    }
    return SpellHandlerOutcome<void>::Return();
}

/// 58832: Mirror Image: a caster with Glyph of Mirror Image (63093) also casts Mirror Image (65047) on itself;
/// the triggered spell is then cast
static SpellHandlerOutcome<void> EffectTriggerSpellMirrorImage(SpellEffectTriggerSpellContext& ctx)
{
    // Glyph of Mirror Image
    if (ctx.m_caster->HasAura(63093))
    {
        ctx.m_caster->CastSpell(ctx.m_caster, 65047, true, ctx.m_CastItem, NULL, ctx.m_originalCasterGUID);
    }
    return SpellHandlerOutcome<void>::Continue();
}

/// 48129, 60320, 60321: Scroll of Recall I to III: a player target above the scroll's level (40, 70, 80) casts
/// Lost! (60444) and one of its faction's eight spells from 60323 or 60328, and the effect ends; otherwise the
/// teleport goes on
static SpellHandlerOutcome<void> EffectTeleportUnitsScrollOfRecall(SpellEffectTeleportRecallContext& ctx)
{
    uint32 failAtLevel = 0;
    switch (ctx.m_spellInfo->ID)
    {
        case 48129: failAtLevel = 40; break;
        case 60320: failAtLevel = 70; break;
        case 60321: failAtLevel = 80; break;
    }

    if (ctx.unitTarget->getLevel() > failAtLevel && ctx.unitTarget->GetTypeId() == TYPEID_PLAYER)
    {
        ctx.unitTarget->CastSpell(ctx.unitTarget, 60444, true);
        // TODO: Unclear use of probably related spell 60322
        uint32 spellId = (((Player*)ctx.unitTarget)->GetTeam() == ALLIANCE ? 60323 : 60328) + urand(0, 7);
        ctx.unitTarget->CastSpell(ctx.unitTarget, spellId, true);
        return SpellHandlerOutcome<void>::Return();
    }
    return SpellHandlerOutcome<void>::Continue();
}

/// 23442: Everlook Transporter: after the teleport, one time in four the caster casts Evil Twin (23445) and one
/// time in six Transporter Malfunction (23449); the effect ends
static SpellHandlerOutcome<void> EffectTeleportUnitsDimensionalRipperEverlook(SpellEffectTeleportPostContext& ctx)
{
        // Dimensional Ripper - Everlook
    int32 r = irand(0, 119);
    if (r >= 70)                                    // 7/12 success
    {
        if (r < 100)                                // 4/12 evil twin
        {
            ctx.m_caster->CastSpell(ctx.m_caster, 23445, true);
        }
        else                                        // 1/12 fire
        {
            ctx.m_caster->CastSpell(ctx.m_caster, 23449, true);
        }
    }
    return SpellHandlerOutcome<void>::Return();
// Ultrasafe Transporter: Toshley's Station
}

/// 36941: Toshley's Station Transporter: after the teleport, half the time the caster casts one of seven
/// malfunctions at random; the effect ends
static SpellHandlerOutcome<void> EffectTeleportUnitsUltrasafeTransporter(SpellEffectTeleportPostContext& ctx)
{
    if (roll_chance_i(50))                          // 50% success
    {
        int32 rand_eff = urand(1, 7);
        switch (rand_eff)
        {
            case 1:
                // soul split - evil
                ctx.m_caster->CastSpell(ctx.m_caster, 36900, true);
                break;
            case 2:
                // soul split - good
                ctx.m_caster->CastSpell(ctx.m_caster, 36901, true);
                break;
            case 3:
                // Increase the size
                ctx.m_caster->CastSpell(ctx.m_caster, 36895, true);
                break;
            case 4:
                // Decrease the size
                ctx.m_caster->CastSpell(ctx.m_caster, 36893, true);
                break;
            case 5:
                // Transform
            {
                if (((Player*)ctx.m_caster)->GetTeam() == ALLIANCE)
                {
                    ctx.m_caster->CastSpell(ctx.m_caster, 36897, true);
                }
                else
                {
                    ctx.m_caster->CastSpell(ctx.m_caster, 36899, true);
                }
                break;
            }
            case 6:
                // chicken
                ctx.m_caster->CastSpell(ctx.m_caster, 36940, true);
                break;
            case 7:
                // evil twin
                ctx.m_caster->CastSpell(ctx.m_caster, 23445, true);
                break;
        }
    }
    return SpellHandlerOutcome<void>::Return();
// Dimensional Ripper - Area 52
}

/// 36890: Area52 Transporter: after the teleport, half the time the caster casts one of four malfunctions at
/// random; the effect ends
static SpellHandlerOutcome<void> EffectTeleportUnitsDimensionalRipperArea52(SpellEffectTeleportPostContext& ctx)
{
    if (roll_chance_i(50))                          // 50% success
    {
        int32 rand_eff = urand(1, 4);
        switch (rand_eff)
        {
            case 1:
                // soul split - evil
                ctx.m_caster->CastSpell(ctx.m_caster, 36900, true);
                break;
            case 2:
                // soul split - good
                ctx.m_caster->CastSpell(ctx.m_caster, 36901, true);
                break;
            case 3:
                // Increase the size
                ctx.m_caster->CastSpell(ctx.m_caster, 36895, true);
                break;
            case 4:
                // Transform
            {
                if (((Player*)ctx.m_caster)->GetTeam() == ALLIANCE)
                {
                    ctx.m_caster->CastSpell(ctx.m_caster, 36897, true);
                }
                else
                {
                    ctx.m_caster->CastSpell(ctx.m_caster, 36899, true);
                }
                break;
            }
        }
    }
    return SpellHandlerOutcome<void>::Return();
}

template <class Site>
struct SpellEffectDamageTeleportRow
{
    uint32 spellId;
    typename SpellHandler<Site>::Function function;
};

/// Registers every row of one site's table on `registry`; answers the number of rows.
template <class Site, std::size_t N>
static uint32 RegisterSpellEffectDamageTeleportRows(SpellHandlerRegistry& registry,
                                                    SpellEffectDamageTeleportRow<Site> const (&rows)[N])
{
    for (SpellEffectDamageTeleportRow<Site> const& row : rows)
    {
        registry.Register<Site>(row.spellId, row.function);
    }
    return uint32(N);
}

uint32 RegisterSpellEffectDamageTeleportHandlers(SpellHandlerRegistry& registry)
{
    static SpellEffectDamageTeleportRow<SpellEffectSchoolDmgSite> const schoolDamage[] =
    {
        { 24340, &EffectSchoolDmgDivideAmongTargets },
        { 26558, &EffectSchoolDmgDivideAmongTargets },
        { 28884, &EffectSchoolDmgDivideAmongTargets },
        { 36837, &EffectSchoolDmgDivideAmongTargets },
        { 38903, &EffectSchoolDmgDivideAmongTargets },
        { 41276, &EffectSchoolDmgDivideAmongTargets },
        { 57467, &EffectSchoolDmgDivideAmongTargets },
        { 26789, &EffectSchoolDmgDivideAmongTargets },
        { 31436, &EffectSchoolDmgDivideAmongTargets },
        { 35181, &EffectSchoolDmgDivideAmongTargets },
        { 40810, &EffectSchoolDmgDivideAmongTargets },
        { 43267, &EffectSchoolDmgDivideAmongTargets },
        { 43268, &EffectSchoolDmgDivideAmongTargets },
        { 42384, &EffectSchoolDmgDivideAmongTargets },
        { 45150, &EffectSchoolDmgDivideAmongTargets },
        { 64422, &EffectSchoolDmgDivideAmongTargets },
        { 64688, &EffectSchoolDmgDivideAmongTargets },
        { 70492, &EffectSchoolDmgDivideAmongTargets },
        { 72505, &EffectSchoolDmgDivideAmongTargets },
        { 71904, &EffectSchoolDmgDivideAmongTargets },
        { 72624, &EffectSchoolDmgDivideAmongTargets },
        { 72625, &EffectSchoolDmgDivideAmongTargets },
        { 77679, &EffectSchoolDmgDivideAmongTargets },
        { 92968, &EffectSchoolDmgDivideAmongTargets },
        { 92969, &EffectSchoolDmgDivideAmongTargets },
        { 92970, &EffectSchoolDmgDivideAmongTargets },
        { 82935, &EffectSchoolDmgDivideAmongTargets },
        { 88915, &EffectSchoolDmgDivideAmongTargets },
        { 88916, &EffectSchoolDmgDivideAmongTargets },
        { 88917, &EffectSchoolDmgDivideAmongTargets },
        { 86014, &EffectSchoolDmgDivideAmongTargets },
        { 92863, &EffectSchoolDmgDivideAmongTargets },
        { 92864, &EffectSchoolDmgDivideAmongTargets },
        { 92865, &EffectSchoolDmgDivideAmongTargets },
        { 86367, &EffectSchoolDmgDivideAmongTargets },
        { 93135, &EffectSchoolDmgDivideAmongTargets },
        { 93136, &EffectSchoolDmgDivideAmongTargets },
        { 93137, &EffectSchoolDmgDivideAmongTargets },
        { 86825, &EffectSchoolDmgDivideAmongTargets },
        { 92879, &EffectSchoolDmgDivideAmongTargets },
        { 92880, &EffectSchoolDmgDivideAmongTargets },
        { 92881, &EffectSchoolDmgDivideAmongTargets },
        { 88942, &EffectSchoolDmgDivideAmongTargets },
        { 95172, &EffectSchoolDmgDivideAmongTargets },
        { 89348, &EffectSchoolDmgDivideAmongTargets },
        { 95178, &EffectSchoolDmgDivideAmongTargets },
        { 98474, &EffectSchoolDmgDivideAmongTargets },
        { 100212, &EffectSchoolDmgDivideAmongTargets },
        { 100213, &EffectSchoolDmgDivideAmongTargets },
        { 100214, &EffectSchoolDmgDivideAmongTargets },
        { 103414, &EffectSchoolDmgDivideAmongTargets },
        { 108571, &EffectSchoolDmgDivideAmongTargets },
        { 109033, &EffectSchoolDmgDivideAmongTargets },
        { 109034, &EffectSchoolDmgDivideAmongTargets },
        { 105069, &EffectSchoolDmgDivideAmongTargets },
        { 108094, &EffectSchoolDmgDivideAmongTargets },
        { 106375, &EffectSchoolDmgDivideAmongTargets },
        { 109182, &EffectSchoolDmgDivideAmongTargets },
        { 109183, &EffectSchoolDmgDivideAmongTargets },
        { 109184, &EffectSchoolDmgDivideAmongTargets },
        { 25599, &EffectSchoolDmgThundercrash },
        { 20253, &EffectSchoolDmgIntercept },
        { 61491, &EffectSchoolDmgIntercept },
        { 29142, &EffectSchoolDmgPercentOfMaxHealth },
        { 35139, &EffectSchoolDmgPercentOfMaxHealth },
        { 49882, &EffectSchoolDmgPercentOfMaxHealth },
        { 55269, &EffectSchoolDmgPercentOfMaxHealth },
        { 37841, &EffectSchoolDmgLightningStrike },
        { 38441, &EffectSchoolDmgCataclysmicBolt },
        { 50341, &EffectSchoolDmgTouchTheNightmare },
        { 62775, &EffectSchoolDmgTympanicTantrum },
        { 67485, &EffectSchoolDmgHandOfReckoning },
        { 68793, &EffectSchoolDmgMagicBaneNormal },
        { 69050, &EffectSchoolDmgMagicBaneHeroic },
    };

    static SpellEffectDamageTeleportRow<SpellEffectTriggerSpellSite> const triggerSpell[] =
    {
        { 18461, &EffectTriggerSpellVanish },
        { 29284, &EffectTriggerSpellBrittleArmor },
        { 29286, &EffectTriggerSpellMercurialShield },
        { 31980, &EffectTriggerSpellRighteousDefense },
        { 35729, &EffectTriggerSpellCloakOfShadows },
        { 41967, &EffectTriggerSpellShadowfiend },
        { 58832, &EffectTriggerSpellMirrorImage },
    };

    static SpellEffectDamageTeleportRow<SpellEffectTeleportRecallSite> const teleportRecall[] =
    {
        { 48129, &EffectTeleportUnitsScrollOfRecall },
        { 60320, &EffectTeleportUnitsScrollOfRecall },
        { 60321, &EffectTeleportUnitsScrollOfRecall },
    };

    static SpellEffectDamageTeleportRow<SpellEffectTeleportPostSite> const teleportPost[] =
    {
        { 23442, &EffectTeleportUnitsDimensionalRipperEverlook },
        { 36941, &EffectTeleportUnitsUltrasafeTransporter },
        { 36890, &EffectTeleportUnitsDimensionalRipperArea52 },
    };

    uint32 rows = RegisterSpellEffectDamageTeleportRows(registry, schoolDamage);
    rows += RegisterSpellEffectDamageTeleportRows(registry, triggerSpell);
    rows += RegisterSpellEffectDamageTeleportRows(registry, teleportRecall);
    rows += RegisterSpellEffectDamageTeleportRows(registry, teleportPost);
    return rows;
}
