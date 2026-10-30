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

#include "Utilities/MathDefines.h"
#include "Unit.h"
#include "combat/MeleeChances.h"
#include "combat/WeaponDamage.h"
#include "Log.h"
#include "Opcodes.h"
#include "WorldPacket.h"
#include "ObjectMgr.h"
#include "GridMap.h"
#include "ObjectGuid.h"
#include "spells/data/SpellMgr.h"
#include "QuestDef.h"
#include "Player.h"
#include "Creature.h"
#include "MotionMaster.h"
#include "spells/pipeline/Spell.h"
#include "spells/auras/SpellAuras.h"
#include "Formulas.h"
#include "Util.h"
#include "GridNotifiersImpl.h"
#include "movement/MoveSpline.h"
#include "GameTime.h"

#include <math.h>
#include <stdarg.h>

/**
 * @brief Performs one melee attack update against a victim.
 *
 * @param pVictim The attack victim.
 * @param attType The attack type to use.
 * @param extra True when this is an extra attack proc.
 */
void Unit::AttackerStateUpdate(Unit* pVictim, WeaponAttackType attType, bool extra)
{
    if (CannotReact() || HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PACIFIED))
    {
        return;
    }

    if (!pVictim->IsAlive())
    {
        return;
    }

    if (IsNonMeleeSpellCasted(false))
    {
        return;
    }

    if (attType == RANGED_ATTACK)
    {
        return;                                              // ignore ranged case
    }

    uint32 extraAttacks = m_extraAttacks;

    // melee attack spell casted at main hand attack only
    if (attType == BASE_ATTACK && m_currentSpells[CURRENT_MELEE_SPELL])
    {
        m_currentSpells[CURRENT_MELEE_SPELL]->cast();

        // not recent extra attack only at any non extra attack (melee spell case)
        if (!extra && extraAttacks)
        {
            while (m_extraAttacks)
            {
                AttackerStateUpdate(pVictim, BASE_ATTACK, true);
                if (m_extraAttacks > 0)
                {
                    --m_extraAttacks;
                }
            }
        }
        return;
    }

    RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_MELEE_ATTACK);

    // attack can be redirected to another target
    pVictim = SelectMagnetTarget(pVictim);

    CalcDamageInfo damageInfo;
    CalculateMeleeDamage(pVictim, &damageInfo, attType);
    // Send log damage message to client
    DealDamageMods(pVictim, damageInfo.damage, &damageInfo.absorb);
    SendAttackStateUpdate(&damageInfo);
    ProcDamageAndSpell(damageInfo.target, damageInfo.procAttacker, damageInfo.procVictim, damageInfo.procEx, damageInfo.damage, damageInfo.attackType);
    DealMeleeDamage(&damageInfo, true);

    if (GetTypeId() == TYPEID_PLAYER)
        DEBUG_FILTER_LOG(LOG_FILTER_COMBAT, "AttackerStateUpdate: (Player) %u attacked %u (TypeId: %u) for %u dmg, absorbed %u, blocked %u, resisted %u.",
                         GetGUIDLow(), pVictim->GetGUIDLow(), pVictim->GetTypeId(), damageInfo.damage, damageInfo.absorb, damageInfo.blocked_amount, damageInfo.resist);
    else
        DEBUG_FILTER_LOG(LOG_FILTER_COMBAT, "AttackerStateUpdate: (NPC)    %u attacked %u (TypeId: %u) for %u dmg, absorbed %u, blocked %u, resisted %u.",
                         GetGUIDLow(), pVictim->GetGUIDLow(), pVictim->GetTypeId(), damageInfo.damage, damageInfo.absorb, damageInfo.blocked_amount, damageInfo.resist);

    if (Unit* owner = GetOwner())
    {
        if (owner->GetTypeId() == TYPEID_UNIT)
        {
            owner->SetInCombatWith(pVictim);
            owner->AddThreat(pVictim);
            pVictim->SetInCombatWith(owner);
        }
    }
    for (GuidSet::const_iterator itr = m_guardianPets.begin(); itr != m_guardianPets.end(); ++itr)
    {
        if (Unit* pet = (Unit*)GetMap()->GetPet(*itr))
        {
            pet->SetInCombatWith(pVictim);
            pet->AddThreat(pVictim);
            pVictim->SetInCombatWith(pet);
        }
    }

    // if damage pVictim call AI reaction
    pVictim->AttackedBy(this);

    // extra attack only at any non extra attack (normal case)
    if (!extra && extraAttacks)
    {
        while (m_extraAttacks)
        {
            AttackerStateUpdate(pVictim, BASE_ATTACK, true);
            if (m_extraAttacks > 0)
            {
                --m_extraAttacks;
            }
        }
    }
}

/**
 * @brief Rolls a melee hit outcome using current combat chances.
 *
 * @param pVictim The victim being attacked.
 * @param attType The attack type to evaluate.
 * @return The resulting melee hit outcome.
 */
MeleeHitOutcome Unit::RollMeleeOutcomeAgainst(const Unit* pVictim, WeaponAttackType attType) const
{
    // This is only wrapper

    // Miss chance based on melee
    float miss_chance = MeleeMissChanceCalc(pVictim, attType);

    // Critical hit chance
    float crit_chance = GetUnitCriticalChance(attType, pVictim);

    // stunned target can not dodge and this is check in GetUnitDodgeChance() (returned 0 in this case)
    float dodge_chance = pVictim->GetUnitDodgeChance();
    float block_chance = pVictim->GetUnitBlockChance();
    float parry_chance = pVictim->GetUnitParryChance();

    // Useful if want to specify crit & miss chances for melee, else it could be removed
    DEBUG_FILTER_LOG(LOG_FILTER_COMBAT, "MELEE OUTCOME: miss %f crit %f dodge %f parry %f block %f", miss_chance, crit_chance, dodge_chance, parry_chance, block_chance);

    return RollMeleeOutcomeAgainst(pVictim, attType, int32(crit_chance * 100), int32(miss_chance * 100), int32(dodge_chance * 100), int32(parry_chance * 100), int32(block_chance * 100));
}

/**
 * @brief Rolls a melee hit outcome using explicit precomputed chances.
 *
 * @param pVictim The victim being attacked.
 * @param attType The attack type to evaluate.
 * @param crit_chance The critical chance in hundredths of a percent.
 * @param miss_chance The miss chance in hundredths of a percent.
 * @param dodge_chance The dodge chance in hundredths of a percent.
 * @param parry_chance The parry chance in hundredths of a percent.
 * @param block_chance The block chance in hundredths of a percent.
 * @return The resulting melee hit outcome.
 */
MeleeHitOutcome Unit::RollMeleeOutcomeAgainst(const Unit* pVictim, WeaponAttackType attType, int32 crit_chance, int32 miss_chance, int32 dodge_chance, int32 parry_chance, int32 block_chance) const
{
    if (pVictim->IsInEvadeMode())
    {
        return MELEE_HIT_EVADE;
    }

    int32 attackerMaxSkillValueForLevel = GetMaxSkillValueForLevel(pVictim);
    int32 victimMaxSkillValueForLevel = pVictim->GetMaxSkillValueForLevel(this);

    // bonus from skills is 0.04%
    int32 skillBonus  = 4 * (attackerMaxSkillValueForLevel - victimMaxSkillValueForLevel);
    int32 sum = 0, tmp = 0;
    int32 roll = urand(0, 10000);

    DEBUG_FILTER_LOG(LOG_FILTER_COMBAT, "RollMeleeOutcomeAgainst: skill bonus of %d for attacker", skillBonus);
    DEBUG_FILTER_LOG(LOG_FILTER_COMBAT, "RollMeleeOutcomeAgainst: rolled %d, miss %d, dodge %d, parry %d, block %d, crit %d",
                     roll, miss_chance, dodge_chance, parry_chance, block_chance, crit_chance);

    tmp = miss_chance;

    if (tmp > 0 && roll < (sum += tmp))
    {
        DEBUG_FILTER_LOG(LOG_FILTER_COMBAT, "RollMeleeOutcomeAgainst: MISS");
        return MELEE_HIT_MISS;
    }

    // always crit against a sitting target (except 0 crit chance)
    if (pVictim->GetTypeId() == TYPEID_PLAYER && crit_chance > 0 && !pVictim->IsStandState())
    {
        DEBUG_FILTER_LOG(LOG_FILTER_COMBAT, "RollMeleeOutcomeAgainst: CRIT (sitting victim)");
        return MELEE_HIT_CRIT;
    }

    bool from_behind = !pVictim->Where().HasInArc(this->Where(), M_PI_F);

    if (from_behind)
    {
        DEBUG_FILTER_LOG(LOG_FILTER_COMBAT, "RollMeleeOutcomeAgainst: attack came from behind.");
    }

    // Dodge chance

    // only players can't dodge if attacker is behind
    if (pVictim->GetTypeId() != TYPEID_PLAYER || !from_behind)
    {
        // Reduce dodge chance by attacker expertise rating
        dodge_chance -= GetMeleeRollExpertiseReduction(attType);

        // Modify dodge chance by attacker SPELL_AURA_MOD_COMBAT_RESULT_CHANCE
        dodge_chance += GetTotalAuraModifierByMiscValue(SPELL_AURA_MOD_COMBAT_RESULT_CHANCE, VICTIMSTATE_DODGE) * 100;

        tmp = dodge_chance;
        if ((tmp > 0)                                       // check if unit _can_ dodge
                && ((tmp -= skillBonus) > 0)
                && roll < (sum += tmp))
        {
            DEBUG_FILTER_LOG(LOG_FILTER_COMBAT, "RollMeleeOutcomeAgainst: DODGE <%d, %d)", sum - tmp, sum);
            return MELEE_HIT_DODGE;
        }
    }

    // parry chances
    // check if attack comes from behind, nobody can parry or block if attacker is behind if not have
    if (!from_behind || pVictim->HasAuraType(SPELL_AURA_MOD_PARRY_FROM_BEHIND_PERCENT))
    {
        // Reduce parry chance by attacker expertise rating
        parry_chance -= GetMeleeRollExpertiseReduction(attType);

        if (parry_chance > 0 && pVictim->MayParry())
        {
            parry_chance -= skillBonus;

            // if (from_behind) -- only 100% currently and not 100% sure way value apply
            //    parry_chance = int32(parry_chance * (pVictim->GetTotalAuraMultiplier(SPELL_AURA_MOD_PARRY_FROM_BEHIND_PERCENT) - 1);

            if (parry_chance > 0 &&                         // check if unit _can_ parry
                (roll < (sum += parry_chance)))
            {
                DEBUG_FILTER_LOG(LOG_FILTER_COMBAT, "RollMeleeOutcomeAgainst: PARRY <%d, %d)", sum - parry_chance, sum);
                return MELEE_HIT_PARRY;
            }
        }
    }

    // Cata 4.0.1: glancing chance = 6% + 1.2% per skill point of (victim_max_skill - attacker_max_skill),
    // capped at 40%. Yields 6%/12%/18%/24% at +0/+1/+2/+3 levels and 40% at the +6 NPC ceiling.
    // Only white melee from a player/pet attacker against a higher-level mob can glance.
    if (attType != RANGED_ATTACK &&
        (GetTypeId() == TYPEID_PLAYER || IsPet()) &&
        pVictim->GetTypeId() != TYPEID_PLAYER && !pVictim->IsPet() &&
        getLevel() < pVictim->GetLevelForTarget(this))
    {
        // cap possible value (with bonuses > max skill)
        int32 skill = attackerMaxSkillValueForLevel;

        tmp = 600 + (victimMaxSkillValueForLevel - skill) * 120;
        tmp = tmp > 4000 ? 4000 : tmp;
        if (roll < (sum += tmp))
        {
            DEBUG_FILTER_LOG(LOG_FILTER_COMBAT, "RollMeleeOutcomeAgainst: GLANCING <%d, %d)", sum - tmp, sum);
            return MELEE_HIT_GLANCING;
        }
    }

    // block chances
    // check if attack comes from behind, nobody can parry or block if attacker is behind
    if (!from_behind)
    {
        if (pVictim->MayBlock())
        {
            tmp = block_chance;
            if ((tmp > 0)                                   // check if unit _can_ block
                && ((tmp -= skillBonus) > 0)
                && (roll < (sum += tmp)))
            {
                DEBUG_FILTER_LOG(LOG_FILTER_COMBAT, "RollMeleeOutcomeAgainst: BLOCK <%d, %d)", sum - tmp, sum);
                return MELEE_HIT_BLOCK;
            }
        }
    }

    // Critical chance
    tmp = crit_chance;

    if (tmp > 0 && roll < (sum += tmp))
    {
        DEBUG_FILTER_LOG(LOG_FILTER_COMBAT, "RollMeleeOutcomeAgainst: CRIT <%d, %d)", sum - tmp, sum);
        return MELEE_HIT_CRIT;
    }

    // Crushing blow (post-3.0.2 rules, retained through Cata 4.3.4).
    // Mobs can crush only if 4+ levels above the victim. Raid bosses are
    // standardized to +3 levels, so they cannot crush max-level players.
    //
    // Canonical Blizzard formula:
    //   chance% = max(skill_diff, 20) * 2% - 15%
    //   where skill_diff = attackerLevel*5 - defenderLevel*5
    //
    // Verified against warcraft.wiki.gg/wiki/Crushing_blow (2025-11-27)
    // and the Cataclysm-Preservation TrinityCore 4.3.4 reference impl.
    if ((getLevel() - 4) >= pVictim->getLevel() && !IsNonMeleeSpellCasted(false))
    {
        // skill_diff = attacker_skill - victim_skill, both = level*5.
        // 3.0.2 floor: minimum 20 skill points (= 4-level diff).
        int32 crush_chance = attackerMaxSkillValueForLevel - victimMaxSkillValueForLevel;
        if (crush_chance < 20)
            crush_chance = 20;
        crush_chance = crush_chance * 200 - 1500;  // basis points; 10000 = 100%

        if (crush_chance > 0 && roll < (sum += crush_chance))
        {
            if (MayCrush())
            {
                DEBUG_FILTER_LOG(LOG_FILTER_COMBAT, "RollMeleeOutcomeAgainst: CRUSHING <%d, %d)", sum - crush_chance, sum);
                return MELEE_HIT_CRUSHING;
            }
        }
    }

    DEBUG_FILTER_LOG(LOG_FILTER_COMBAT, "RollMeleeOutcomeAgainst: NORMAL");
    return MELEE_HIT_NORMAL;
}

/**
 * @brief Whether a creature charms the unit, so that its melee hits may crush.
 */
bool Unit::MayCrush() const
{
    Unit* charmer = GetCharmer();
    return GetCharmerGuid() && charmer && charmer->GetTypeId() == TYPEID_UNIT;
}

/**
 * @brief Rolls a weapon damage amount for an attack type.
 *
 * @param attType The attack type to use.
 * @param normalized True to use normalized player weapon damage.
 * @return The randomized damage amount.
 */
uint32 Unit::CalculateDamage(WeaponAttackType attType, bool normalized)
{
    // CalculateMinMaxDamage runs only for a normalized player attack; the six
    // UNIT_FIELD reads are plain array indices, so they are gathered unconditionally.
    bool const isNormalizedPlayer = (normalized && GetTypeId() == TYPEID_PLAYER);
    float playerMinDamage = 0.0f;
    float playerMaxDamage = 0.0f;
    if (isNormalizedPlayer)
    {
        CalculateMinMaxDamage(attType, normalized, playerMinDamage, playerMaxDamage);
    }

    Combat::WeaponDamageRange const range = Combat::SelectWeaponDamageRange(attType, isNormalizedPlayer,
            playerMinDamage, playerMaxDamage,
            GetFloatValue(UNIT_FIELD_MINRANGEDDAMAGE), GetFloatValue(UNIT_FIELD_MAXRANGEDDAMAGE),
            GetFloatValue(UNIT_FIELD_MINDAMAGE), GetFloatValue(UNIT_FIELD_MAXDAMAGE),
            GetFloatValue(UNIT_FIELD_MINOFFHANDDAMAGE), GetFloatValue(UNIT_FIELD_MAXOFFHANDDAMAGE));
    float min_damage = range.min_damage;
    float max_damage = range.max_damage;

    return urand((uint32)min_damage, (uint32)max_damage);
}

/**
 * @brief Calculates the low-level spell coefficient penalty.
 *
 * @param spellProto The spell entry being evaluated.
 * @return The scaling penalty multiplier.
 */
float Unit::CalculateLevelPenalty(SpellEntry const* spellProto) const
{
    uint32 spellLevel = spellProto->GetSpellLevel();
    if (spellLevel <= 0 || spellLevel > spellProto->GetMaxLevel())
    {
        return 1.0f;
    }

    float LvlPenalty = 0.0f;

    if (spellLevel < 20)
    {
        LvlPenalty = (20.0f - spellLevel) * 3.75f;
    }
    float LvlFactor = (float(spellLevel) + 6.0f) / float(getLevel());
    if (LvlFactor > 1.0f)
    {
        LvlFactor = 1.0f;
    }

    return (100.0f - LvlPenalty) * LvlFactor / 100.0f;
}

/**
 * @brief Sends an attack-start packet for melee combat.
 *
 * @param pVictim The victim being attacked.
 */
void Unit::SendMeleeAttackStart(Unit* pVictim)
{
    WorldPacket data(SMSG_ATTACKSTART, 8 + 8);
    data << GetObjectGuid();
    data << pVictim->GetObjectGuid();

    SendMessageToSet(&data, true);
    DEBUG_FILTER_LOG(LOG_FILTER_COMBAT, "WORLD: Sent SMSG_ATTACKSTART: %s -> %s", GetGuidStr().c_str(), pVictim->GetGuidStr().c_str());
}

/**
 * @brief Sends an attack-stop packet for melee combat.
 *
 * @param victim The victim no longer being attacked.
 */
void Unit::SendMeleeAttackStop(Unit* victim)
{
    if (!victim)
    {
        return;
    }

    WorldPacket data(SMSG_ATTACKSTOP, (4 + 16));            // we guess size
    data << GetPackGUID();
    data << victim->GetPackGUID();                          // can be 0x00...
    data << uint32(0);                                      // can be 0x1
    SendMessageToSet(&data, true);
    DETAIL_FILTER_LOG(LOG_FILTER_COMBAT, "%s %u stopped attacking %s %u", (GetTypeId() == TYPEID_PLAYER ? "player" : "creature"), GetGUIDLow(), (victim->GetTypeId() == TYPEID_PLAYER ? "player" : "creature"), victim->GetGUIDLow());

    /*if (victim->GetTypeId() == TYPEID_UNIT)
    ((Creature*)victim)->AI().EnterEvadeMode(this);*/
}

/**
 * @brief Tells a player's own client that its melee and ranged attack is cancelled.
 */
void Unit::SendAttackSwingCancelAttack()
{
    if (ClientCallbacks const* client = m_clientCallbacks)
    {
        ReportClientFact(client->combatCancelled, CombatCancelledFact());
    }
}

/**
 * @brief Tells a player's own client that its auto-repeat spell on `target` is cancelled.
 *
 * @param target The unit whose guid the client is told.
 */
void Unit::SendAutoRepeatCancel(Unit* target)
{
    if (ClientCallbacks const* client = m_clientCallbacks)
    {
        AutoRepeatCancelledFact fact;
        fact.target = target->GetObjectGuid();
        ReportClientFact(client->autoRepeatCancelled, fact);
    }
}

/**
 * @brief Tells a player's own client a melee swing error that differs from the last one it was
 * told, and remembers it.
 *
 * @param swingError 0 for none, 1 out of reach, 2 facing the wrong way.
 */
void Unit::ReportSwingError(uint8 swingError)
{
    ClientCallbacks const* client = m_clientCallbacks;
    if (!client || swingError == m_lastSwingError)
    {
        return;
    }

    if (swingError == 1)
    {
        ReportClientFact(client->swingOutOfReach, SwingOutOfReachFact());
    }
    else if (swingError == 2)
    {
        ReportClientFact(client->swingBadFacing, SwingBadFacingFact());
    }
    m_lastSwingError = swingError;
}

/**
 * @brief Checks whether an incoming melee or ranged spell can be blocked.
 *
 * @param pCaster The attacking caster.
 * @param spellEntry The spell entry being resolved.
 * @param attackType The related attack type.
 * @return True if the spell is blocked; otherwise, false.
 */
bool Unit::IsSpellBlocked(Unit* pCaster, SpellEntry const* spellEntry, WeaponAttackType attackType)
{
    if (!Where().HasInArc(pCaster->Where(), M_PI_F))
    {
        return false;
    }

    if (spellEntry)
    {
        // Some spells can not be blocked
        if (spellEntry->HasAttribute(SPELL_ATTR_IMPOSSIBLE_DODGE_PARRY_BLOCK))
        {
            return false;
        }
    }

    /*
    // Ignore combat result aura (parry/dodge check on prepare)
    AuraList const& ignore = GetAurasByType(SPELL_AURA_IGNORE_COMBAT_RESULT);
    for(AuraList::const_iterator i = ignore.begin(); i != ignore.end(); ++i)
    {
        if (!(*i)->isAffectedOnSpell(spellProto))
        {
            continue;
        }
        if ((*i)->GetModifier()->m_miscvalue == ???)
        {
            return false;
        }
    }
    */

    if (!MayBlock())
    {
        return false;
    }

    float blockChance = GetUnitBlockChance();
    // For each point of difference between the attacker's level and the defender's level,
    // the block chance is modified by 0.5% if the target is a mob and 0.2% if the target is a player.
    blockChance += (getLevel() - pCaster->GetLevelForTarget(this)) * (pCaster->GetTypeId() == TYPEID_PLAYER ? 0.2f : 0.5f);
    if (blockChance < 0.0f)
    {
        blockChance = 0.0f;
    }

    return roll_chance_f(blockChance);
}

// Melee based spells can be miss, parry or dodge on this step
// Crit or block - determined on damage calculation phase! (and can be both in some time)
float Unit::MeleeSpellMissChance(Unit* pVictim, WeaponAttackType attType, int32 skillDiff, SpellEntry const* spell)
{
    // Calculate hit chance (more correct for chance mod)
    float hitChance = 0.0f;

    // PvP - PvE melee chances
    // TODO: implement diminishing returns for defense from player's defense rating
    // pure skill diff is not sufficient since 3.x anymore, but exact formulas hard to research
    if (pVictim->GetTypeId() == TYPEID_PLAYER)
    {
        hitChance = 95.0f + skillDiff * 0.04f;
    }
    else if (skillDiff < -10)
    {
        hitChance = 94.0f + (skillDiff + 10) * 0.4f;
    }
    else
    {
        hitChance = 95.0f + skillDiff * 0.1f;
    }

    // Hit chance depends from victim auras
    if (attType == RANGED_ATTACK)
    {
        hitChance += pVictim->GetTotalAuraModifier(SPELL_AURA_MOD_ATTACKER_RANGED_HIT_CHANCE);
    }
    else
    {
        hitChance += pVictim->GetTotalAuraModifier(SPELL_AURA_MOD_ATTACKER_MELEE_HIT_CHANCE);
    }

    // Spellmod from SPELLMOD_RESIST_MISS_CHANCE
    if (SpellModMgr* modOwner = GetSpellMods())
    {
        modOwner->ApplySpellMod(spell->ID, SPELLMOD_RESIST_MISS_CHANCE, hitChance);
    }

    // Miss = 100 - hit
    float missChance = 100.0f - hitChance;

    // Bonuses from attacker aura and ratings
    if (attType == RANGED_ATTACK)
    {
        missChance -= m_modRangedHitChance;
    }
    else
    {
        missChance -= m_modMeleeHitChance;
    }

    // Limit miss chance from 0 to 60%
    if (missChance < 0.0f)
    {
        return 0.0f;
    }
    if (missChance > 60.0f)
    {
        return 60.0f;
    }
    return missChance;
}

// Melee based spells hit result calculations
SpellMissInfo Unit::MeleeSpellHitResult(Unit* pVictim, SpellEntry const* spell)
{
    WeaponAttackType attType = BASE_ATTACK;

    if (spell->GetDmgClass() == SPELL_DAMAGE_CLASS_RANGED)
    {
        attType = RANGED_ATTACK;
    }

    // bonus from skills is 0.04% per skill Diff
    int32 attackerWeaponSkill = GetMaxSkillValueForLevel();
    int32 skillDiff = attackerWeaponSkill - int32(pVictim->GetMaxSkillValueForLevel(this));

    //is this to get a better spread and not have to resort to floats?
    uint32 roll = urand(0, 10000);

    uint32 missChance = uint32(MeleeSpellMissChance(pVictim, attType, skillDiff, spell) * 100.0f);
    // Roll miss
    uint32 tmp = spell->HasAttribute(SPELL_ATTR_EX3_CANT_MISS) ? 0 : missChance;
    if (roll < tmp)
    {
        return SPELL_MISS_MISS;
    }

    // Chance resist mechanic (select max value from every mechanic spell effect)
    int32 resist_mech = 0;
    // Get effects mechanic and chance
    for (int eff = 0; eff < MAX_EFFECT_INDEX; ++eff)
    {
        int32 effect_mech = GetEffectMechanic(spell, SpellEffectIndex(eff));
        if (effect_mech)
        {
            int32 temp = pVictim->GetTotalAuraModifierByMiscValue(SPELL_AURA_MOD_MECHANIC_RESISTANCE, effect_mech);
            if (resist_mech < temp * 100)
            {
                resist_mech = temp * 100;
            }
        }
    }
    // Roll chance
    tmp += resist_mech;
    if (roll < tmp)
    {
        return SPELL_MISS_RESIST;
    }

    bool canDodge = true;
    bool canParry = true;

    // Same spells can not be parry/dodge
    if (spell->HasAttribute(SPELL_ATTR_IMPOSSIBLE_DODGE_PARRY_BLOCK))
    {
        return SPELL_MISS_NONE;
    }

    bool from_behind = !pVictim->Where().HasInArc(this->Where(), M_PI_F);

    // Ranged attack cannot be parry/dodge only deflect
    if (attType == RANGED_ATTACK)
    {
        // only if in front or special ability
        if (!from_behind || pVictim->HasAuraType(SPELL_AURA_MOD_PARRY_FROM_BEHIND_PERCENT))
        {
            int32 deflect_chance = pVictim->GetTotalAuraModifier(SPELL_AURA_DEFLECT_SPELLS) * 100;

            // if (from_behind) -- only 100% currently and not 100% sure way value apply
            //    deflect_chance = int32(deflect_chance * (pVictim->GetTotalAuraMultiplier(SPELL_AURA_MOD_PARRY_FROM_BEHIND_PERCENT) - 1);

            tmp += deflect_chance;
            if (roll < tmp)
            {
                return SPELL_MISS_DEFLECT;
            }
        }
        return SPELL_MISS_NONE;
    }

    // Check for attack from behind
    if (from_behind)
    {
        // Can`t dodge from behind in PvP (but its possible in PvE)
        if (GetTypeId() == TYPEID_PLAYER && pVictim->GetTypeId() == TYPEID_PLAYER)
        {
            canDodge = false;
        }
        // Can`t parry without special ability
        if (!pVictim->HasAuraType(SPELL_AURA_MOD_PARRY_FROM_BEHIND_PERCENT))
        {
            canParry = false;
        }
    }
    if (!pVictim->MayParry())
    {
        canParry = false;
    }
    // Ignore combat result aura
    AuraList const& ignore = GetAurasByType(SPELL_AURA_IGNORE_COMBAT_RESULT);
    for (AuraList::const_iterator i = ignore.begin(); i != ignore.end(); ++i)
    {
        if (!(*i)->isAffectedOnSpell(spell))
        {
            continue;
        }
        switch ((*i)->GetModifier()->m_miscvalue)
        {
            case MELEE_HIT_DODGE: canDodge = false; break;
            case MELEE_HIT_BLOCK: break; // Block check in hit step
            case MELEE_HIT_PARRY: canParry = false; break;
            default:
                DEBUG_LOG("Spell %u SPELL_AURA_IGNORE_COMBAT_RESULT have unhandled state %d", (*i)->GetId(), (*i)->GetModifier()->m_miscvalue);
                break;
        }
    }

    if (canDodge)
    {
        // Roll dodge
        int32 dodgeChance = int32(pVictim->GetUnitDodgeChance() * 100.0f) - skillDiff * 4;
        // Reduce enemy dodge chance by SPELL_AURA_MOD_COMBAT_RESULT_CHANCE
        dodgeChance += GetTotalAuraModifierByMiscValue(SPELL_AURA_MOD_COMBAT_RESULT_CHANCE, VICTIMSTATE_DODGE) * 100;
        // Reduce dodge chance by attacker expertise rating
        dodgeChance -= GetMeleeSpellExpertiseReduction(attType);
        if (dodgeChance < 0)
        {
            dodgeChance = 0;
        }

        tmp += dodgeChance;
        if (roll < tmp)
        {
            return SPELL_MISS_DODGE;
        }
    }

    if (canParry)
    {
        // Roll parry
        int32 parryChance = int32(pVictim->GetUnitParryChance() * 100.0f)  - skillDiff * 4;
        // Reduce parry chance by attacker expertise rating
        parryChance -= GetMeleeSpellExpertiseReduction(attType);
        if (parryChance < 0)
        {
            parryChance = 0;
        }

        // if (from_behind) -- only 100% currently and not 100% sure way value apply
        //    parryChance = int32(parryChance * (pVictim->GetTotalAuraMultiplier(SPELL_AURA_MOD_PARRY_FROM_BEHIND_PERCENT) - 1));

        tmp += parryChance;
        if (roll < tmp)
        {
            return SPELL_MISS_PARRY;
        }
    }

    return SPELL_MISS_NONE;
}

// TODO need use unit spell resistances in calculations
SpellMissInfo Unit::MagicSpellHitResult(Unit* pVictim, SpellEntry const* spell)
{
    // Can`t miss on dead target (on skinning for example)
    if (!pVictim->IsAlive())
    {
        return SPELL_MISS_NONE;
    }

    SpellSchoolMask schoolMask = GetSpellSchoolMask(spell);
    // both levels are read through the virtual GetLevelForTarget, the victim's first
    bool const victimIsPlayer = (pVictim->GetTypeId() == TYPEID_PLAYER);
    uint32 const victimLevel = pVictim->GetLevelForTarget(this);
    uint32 const attackerLevel = GetLevelForTarget(pVictim);

    int32 modHitChance = Combat::MagicSpellBaseHitChance(victimIsPlayer, victimLevel, attackerLevel);

    // Spellmod from SPELLMOD_RESIST_MISS_CHANCE
    if (SpellModMgr* modOwner = GetSpellMods())
    {
        modOwner->ApplySpellMod(spell->ID, SPELLMOD_RESIST_MISS_CHANCE, modHitChance);
    }
    // Chance hit from victim SPELL_AURA_MOD_ATTACKER_SPELL_HIT_CHANCE auras
    modHitChance += pVictim->GetTotalAuraModifierByMiscMask(SPELL_AURA_MOD_ATTACKER_SPELL_HIT_CHANCE, schoolMask);
    // Reduce spell hit chance for dispel mechanic spells from victim SPELL_AURA_MOD_DISPEL_RESIST
    if (IsDispelSpell(spell))
    {
        modHitChance -= pVictim->GetTotalAuraModifier(SPELL_AURA_MOD_DISPEL_RESIST);
    }
    // Chance resist mechanic (select max value from every mechanic spell effect)
    int32 resist_mech = 0;
    // Get effects mechanic and chance
    for (int eff = 0; eff < MAX_EFFECT_INDEX; ++eff)
    {
        int32 effect_mech = GetEffectMechanic(spell, SpellEffectIndex(eff));
        if (effect_mech)
        {
            int32 temp = pVictim->GetTotalAuraModifierByMiscValue(SPELL_AURA_MOD_MECHANIC_RESISTANCE, effect_mech);
            if (resist_mech < temp)
            {
                resist_mech = temp;
            }
        }
    }
    // Apply mod
    modHitChance -= resist_mech;

    int32 HitChance = modHitChance * 100;
    // Increase hit chance from attacker SPELL_AURA_MOD_SPELL_HIT_CHANCE and attacker ratings
    HitChance += int32(m_modSpellHitChance * 100.0f);


    if (HitChance <  100)
    {
        HitChance =  100;
    }
    if (HitChance > 10000)
    {
        HitChance = 10000;
    }

    int32 tmp = spell->HasAttribute(SPELL_ATTR_EX3_CANT_MISS) ? 0 : (10000 - HitChance);

    // Why isn't this urand aswell just as in MeleeSpellHitResult?
    int32 rand = irand(0, 10000);

    if (rand < tmp)
    {
        return SPELL_MISS_MISS;
    }

    bool from_behind = !pVictim->Where().HasInArc(this->Where(), M_PI_F);

    // cast by caster in front of victim or behind with special ability
    if (!from_behind || pVictim->HasAuraType(SPELL_AURA_MOD_PARRY_FROM_BEHIND_PERCENT))
    {
        int32 deflect_chance = pVictim->GetTotalAuraModifier(SPELL_AURA_DEFLECT_SPELLS) * 100;

        // if (from_behind) -- only 100% currently and not 100% sure way value apply
        //    deflect_chance = int32(deflect_chance * (pVictim->GetTotalAuraMultiplier(SPELL_AURA_MOD_PARRY_FROM_BEHIND_PERCENT)) - 1);

        tmp += deflect_chance;
        if (rand < tmp)
        {
            return SPELL_MISS_DEFLECT;
        }
    }

    return SPELL_MISS_NONE;
}

// Calculate spell hit result can be:
// Every spell can: Evade/Immune/Reflect/Sucesful hit
// For melee based spells:
//   Miss
//   Dodge
//   Parry
// For spells
//   Resist
SpellMissInfo Unit::SpellHitResult(Unit* pVictim, SpellEntry const* spell, bool CanReflect, SpellHitFor hitFor)
{
    // Return evade for units in evade mode
    if (pVictim->IsInEvadeMode())
    {
        return SPELL_MISS_EVADE;
    }

    // Check for immune
    if (pVictim->IsImmuneToSpell(spell, this == pVictim) && !spell->HasAttribute(SPELL_ATTR_UNAFFECTED_BY_INVULNERABILITY))
    {
        return SPELL_MISS_IMMUNE;
    }

    // All positive spells can`t miss
    // TODO: client not show miss log for this spells - so need find info for this in dbc and use it!
    if (IsPositiveSpell(spell->ID))
    {
        return SPELL_MISS_NONE;
    }

    // Check for immune (use charges)
    if (pVictim->IsImmunedToDamage(GetSpellSchoolMask(spell)) && !spell->HasAttribute(SPELL_ATTR_UNAFFECTED_BY_INVULNERABILITY))
    {
        return SPELL_MISS_IMMUNE;
    }

    // Try victim reflect spell
    if (CanReflect)
    {
        int32 reflectchance = pVictim->GetTotalAuraModifier(SPELL_AURA_REFLECT_SPELLS);
        Unit::AuraList const& mReflectSpellsSchool = pVictim->GetAurasByType(SPELL_AURA_REFLECT_SPELLS_SCHOOL);
        for (Unit::AuraList::const_iterator i = mReflectSpellsSchool.begin(); i != mReflectSpellsSchool.end(); ++i)
            if ((*i)->GetModifier()->m_miscvalue & GetSpellSchoolMask(spell))
            {
                reflectchance += (*i)->GetModifier()->m_amount;
            }
        if (reflectchance > 0 && roll_chance_i(reflectchance))
        {
            // Start triggers for remove charges if need (trigger only for victim, and mark as active spell)
            ProcDamageAndSpell(pVictim, PROC_FLAG_NONE, PROC_FLAG_TAKEN_NEGATIVE_SPELL_HIT, PROC_EX_REFLECT, 1, BASE_ATTACK, spell);
            return SPELL_MISS_REFLECT;
        }
    }

    // A cast's own spell on its caster is not rolled against him; a reflected one still is
    if (pVictim == this && hitFor == SpellHitFor::Cast)
    {
        return SPELL_MISS_NONE;
    }

    switch (spell->GetDmgClass())
    {
        case SPELL_DAMAGE_CLASS_NONE:
            return SPELL_MISS_NONE;
        case SPELL_DAMAGE_CLASS_MAGIC:
            return MagicSpellHitResult(pVictim, spell);
        case SPELL_DAMAGE_CLASS_MELEE:
        case SPELL_DAMAGE_CLASS_RANGED:
            return MeleeSpellHitResult(pVictim, spell);
    }
    return SPELL_MISS_NONE;
}

/**
 * @brief Calculates the melee miss chance against a victim.
 *
 * @param pVictim The victim being attacked.
 * @param attType The attack type to evaluate.
 * @return The miss chance percentage.
 */
float Unit::MeleeMissChanceCalc(const Unit* pVictim, WeaponAttackType attType) const
{
    bool const hasVictim = pVictim != NULL;

    bool hasOffhandWeapon = false;
    bool isNormalSpellActive = false;
    bool hasMeleeSpell = false;
    uint16 attackerSkill = 0;
    uint16 victimDefenseSkill = 0;
    bool victimIsPlayer = false;
    int32 victimRangedHitChanceMod = 0;
    int32 victimMeleeHitChanceMod = 0;
    if (hasVictim)
    {
        hasOffhandWeapon = haveOffhandWeapon();
        if (hasOffhandWeapon && attType != RANGED_ATTACK)
        {
            for (uint32 i = CURRENT_FIRST_NON_MELEE_SPELL; i < CURRENT_MAX_SPELL; ++i)
            {
                if (m_currentSpells[i] && (GetSpellSchoolMask(m_currentSpells[i]->m_spellInfo) & SPELL_SCHOOL_MASK_NORMAL))
                {
                    isNormalSpellActive = true;
                    break;
                }
            }
            hasMeleeSpell = m_currentSpells[CURRENT_MELEE_SPELL] != NULL;
        }

        attackerSkill = GetMaxSkillValueForLevel(pVictim);
        victimDefenseSkill = pVictim->GetMaxSkillValueForLevel(this);
        victimIsPlayer = pVictim->GetTypeId() == TYPEID_PLAYER;

        // Each arm below walks one aura list, as the original does: gathering both
        // would double the work on every swing.
        if (attType == RANGED_ATTACK)
        {
            victimRangedHitChanceMod = pVictim->GetTotalAuraModifier(SPELL_AURA_MOD_ATTACKER_RANGED_HIT_CHANCE);
        }
        else
        {
            victimMeleeHitChanceMod = pVictim->GetTotalAuraModifier(SPELL_AURA_MOD_ATTACKER_MELEE_HIT_CHANCE);
        }
    }

    return Combat::MeleeMissChance(hasVictim, attType, hasOffhandWeapon, isNormalSpellActive,
                                   hasMeleeSpell, attackerSkill, victimDefenseSkill,
                                   victimIsPlayer, m_modRangedHitChance, m_modMeleeHitChance,
                                   victimRangedHitChanceMod, victimMeleeHitChanceMod);
}

/**
 * @brief Gets the unit's current dodge chance.
 *
 * @return The dodge chance percentage.
 */
float Unit::GetUnitDodgeChance() const
{
    bool const isStunned = Blocked(Motion::ReasonStunned);
    bool const isPlayer = GetTypeId() == TYPEID_PLAYER;

    float playerDodgePercentage = 0.0f;
    bool isTotem = false;
    int32 dodgeAuraMod = 0;
    if (!isStunned)                                                                     // the early return's guard
    {
        if (isPlayer)
        {
            playerDodgePercentage = GetFloatValue(PLAYER_DODGE_PERCENTAGE);
        }
        else
        {
            isTotem = IsTotem();
            if (!isTotem)
            {
                dodgeAuraMod = GetTotalAuraModifier(SPELL_AURA_MOD_DODGE_PERCENT);
            }
        }
    }

    return Combat::UnitDodgeChance(isStunned, isPlayer, playerDodgePercentage, isTotem,
                                   dodgeAuraMod);
}

/**
 * @brief Gets the unit's current parry chance.
 *
 * @return The parry chance percentage.
 */
float Unit::GetUnitParryChance() const
{
    bool const isCastingNonMeleeSpell = IsNonMeleeSpellCasted(false);
    bool const isStunned = Blocked(Motion::ReasonStunned);
    bool const isPlayer = GetTypeId() == TYPEID_PLAYER;
    bool const isCreature = GetTypeId() == TYPEID_UNIT;

    bool canParry = false;
    bool hasParryWeapon = false;
    float playerParryPercentage = 0.0f;
    uint32 creatureType = 0;
    int32 parryAuraMod = 0;
    if (!isCastingNonMeleeSpell && !isStunned)                                          // the early return's guard
    {
        if (Player const* player = ToPlayer())
        {
            canParry = player->CanParry();
            if (canParry)
            {
                Item* tmpitem = player->GetWeaponForAttack(BASE_ATTACK, true, true);
                if (!tmpitem)
                {
                    tmpitem = player->GetWeaponForAttack(OFF_ATTACK, true, true);
                }

                if (tmpitem)
                {
                    hasParryWeapon = true;
                    playerParryPercentage = GetFloatValue(PLAYER_PARRY_PERCENTAGE);
                }
            }
        }
        else if (isCreature)
        {
            creatureType = GetCreatureType();
            if (creatureType == CREATURE_TYPE_HUMANOID)
            {
                parryAuraMod = GetTotalAuraModifier(SPELL_AURA_MOD_PARRY_PERCENT);
            }
        }
    }

    return Combat::UnitParryChance(isCastingNonMeleeSpell, isStunned, isPlayer, isCreature,
                                   canParry, hasParryWeapon, playerParryPercentage,
                                   creatureType, parryAuraMod);
}

/**
 * @brief Gets the unit's current block chance.
 *
 * @return The block chance percentage.
 */
float Unit::GetUnitBlockChance() const
{
    bool const isCastingNonMeleeSpell = IsNonMeleeSpellCasted(false);
    bool const isStunned = Blocked(Motion::ReasonStunned);
    bool const isPlayer = GetTypeId() == TYPEID_PLAYER;

    bool canBlock = false;
    bool canUseOffhandWeapon = false;
    bool hasUnbrokenOffhandItem = false;
    float playerBlockPercentage = 0.0f;
    bool isTotem = false;
    int32 blockAuraMod = 0;
    if (!isCastingNonMeleeSpell && !isStunned)                                              // the early return's guard
    {
        if (Player const* player = ToPlayer())
        {
            canBlock = player->CanBlock();
            if (canBlock)                                                                   // the body's && short-circuits here
            {
                canUseOffhandWeapon = player->CanUseEquippedWeapon(OFF_ATTACK);
            }
            if (canBlock && canUseOffhandWeapon)
            {
                Item* tmpitem = player->GetInventoryMgr().GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND);
                if (tmpitem && !tmpitem->IsBroken())
                {
                    hasUnbrokenOffhandItem = true;
                    playerBlockPercentage = GetFloatValue(PLAYER_BLOCK_PERCENTAGE);
                }
            }
        }
        else
        {
            isTotem = IsTotem();
            if (!isTotem)
            {
                blockAuraMod = GetTotalAuraModifier(SPELL_AURA_MOD_BLOCK_CHANCE_PERCENT);
            }
        }
    }

    return Combat::UnitBlockChance(isCastingNonMeleeSpell, isStunned, isPlayer, canBlock,
                                   canUseOffhandWeapon, hasUnbrokenOffhandItem,
                                   playerBlockPercentage, isTotem, blockAuraMod);
}

/**
 * @brief Gets the unit's critical hit chance against a victim.
 *
 * @param attackType The attack type being evaluated.
 * @param pVictim The victim used for defensive adjustments.
 * @return The critical hit chance percentage.
 */
float Unit::GetUnitCriticalChance(WeaponAttackType attackType, const Unit* pVictim) const
{
    bool const isPlayer = GetTypeId() == TYPEID_PLAYER;

    float playerOffhandCrit = 0.0f;
    float playerMainhandCrit = 0.0f;
    float playerRangedCrit = 0.0f;
    int32 critAuraMod = 0;
    if (isPlayer)
    {
        playerOffhandCrit = GetFloatValue(PLAYER_OFFHAND_CRIT_PERCENTAGE);
        playerMainhandCrit = GetFloatValue(PLAYER_CRIT_PERCENTAGE);
        playerRangedCrit = GetFloatValue(PLAYER_RANGED_CRIT_PERCENTAGE);
    }
    else
    {
        critAuraMod = GetTotalAuraModifier(SPELL_AURA_MOD_CRIT_PERCENT);
    }

    // Each arm below walks one aura list, as the original does.
    int32 victimRangedCritMod = 0;
    int32 victimMeleeCritMod = 0;
    if (attackType == RANGED_ATTACK)
    {
        victimRangedCritMod = pVictim->GetTotalAuraModifier(SPELL_AURA_MOD_ATTACKER_RANGED_CRIT_CHANCE);
    }
    else
    {
        victimMeleeCritMod = pVictim->GetTotalAuraModifier(SPELL_AURA_MOD_ATTACKER_MELEE_CRIT_CHANCE);
    }

    int32 const victimSpellAndWeaponCritMod =
        pVictim->GetTotalAuraModifier(SPELL_AURA_MOD_ATTACKER_SPELL_AND_WEAPON_CRIT_CHANCE);

    return Combat::UnitCriticalChance(attackType, isPlayer, playerOffhandCrit, playerMainhandCrit,
                                      playerRangedCrit, critAuraMod, victimRangedCritMod,
                                      victimMeleeCritMod, victimSpellAndWeaponCritMod);
}

/**
 * @brief Processes pending melee swings against the current victim.
 *
 * @return True if melee attacks were valid and in range; otherwise, false.
 */
bool Unit::UpdateMeleeAttackingState()
{
    Unit* victim = getVictim();
    if (!victim || IsNonMeleeSpellCasted(false))
    {
        return false;
    }

    if (!isAttackReady(BASE_ATTACK) && !(isAttackReady(OFF_ATTACK) && haveOffhandWeapon()))
    {
        return false;
    }

    uint8 swingError = 0;
    if (!InMeleeReach(*this, *victim))
    {
        setAttackTimer(BASE_ATTACK, 100);
        setAttackTimer(OFF_ATTACK, 100);
        swingError = 1;
    }
    // 120 degrees of radiant range
    else if (!Where().HasInArc(victim->Where(), 2 * M_PI_F / 3))
    {
        setAttackTimer(BASE_ATTACK, 100);
        setAttackTimer(OFF_ATTACK, 100);
        swingError = 2;
    }
    else
    {
        if (isAttackReady(BASE_ATTACK))
        {
            // prevent base and off attack in same time, delay attack at 0.2 sec
            if (haveOffhandWeapon())
            {
                if (getAttackTimer(OFF_ATTACK) < ATTACK_DISPLAY_DELAY)
                {
                    setAttackTimer(OFF_ATTACK, ATTACK_DISPLAY_DELAY);
                }
            }
            AttackerStateUpdate(victim, BASE_ATTACK);
            resetAttackTimer(BASE_ATTACK);
        }
        if (haveOffhandWeapon() && isAttackReady(OFF_ATTACK))
        {
            // prevent base and off attack in same time, delay attack at 0.2 sec
            uint32 base_att = getAttackTimer(BASE_ATTACK);
            if (base_att < ATTACK_DISPLAY_DELAY)
            {
                setAttackTimer(BASE_ATTACK, ATTACK_DISPLAY_DELAY);
            }
            // do attack
            AttackerStateUpdate(victim, OFF_ATTACK);
            resetAttackTimer(OFF_ATTACK);
        }
    }

    ReportSwingError(swingError);

    return swingError == 0;
}

/**
 * @brief Checks whether the unit currently has a usable offhand weapon.
 *
 * @return True if an offhand weapon can be used; otherwise, false.
 */
bool Unit::haveOffhandWeapon() const
{
    if (!CanUseEquippedWeapon(OFF_ATTACK))
    {
        return false;
    }

    return GetWeaponForAttack(OFF_ATTACK, true, true);
}

/**
 * @brief Resets an attack timer based on attack speed modifiers.
 *
 * @param type The attack type whose timer should be reset.
 */
void Unit::resetAttackTimer(WeaponAttackType type)
{
    m_attackTimer[type] = uint32(GetAttackTime(type) * m_modAttackSpeedPct[type]);
}

/**
 * @brief Calculates the combat reach between this unit and a victim.
 *
 * @param pVictim The victim being checked.
 * @param forMeleeRange True to enforce the minimum melee distance.
 * @param flat_mod Additional flat reach modifier.
 * @return The resulting combat reach.
 */
float CombatReachBetween(Unit const& attacker, Unit const& victim, bool forMeleeRange,
                         float flat_mod)
{
    // The measured values show BASE_MELEE_OFFSET in (1.3224, 1.342)
    float reach = attacker.GetFloatValue(UNIT_FIELD_COMBATREACH) +
                  victim.GetFloatValue(UNIT_FIELD_COMBATREACH) +
                  BASE_MELEERANGE_OFFSET + flat_mod;

    if (forMeleeRange && reach < ATTACK_DISTANCE)
    {
        reach = ATTACK_DISTANCE;
    }

    return reach;
}

float CombatDistanceBetween(Unit const& attacker, Unit const& target, bool forMeleeRange)
{
    if (!attacker.Where().ShareFrame(target.Where()))
    {
        return Geometry::Placement::Unreachable();
    }

    // Combat reach, not the bounding extents: the component measures centre to centre and
    // the game rule supplies the gap that counts as touching.
    const float reach = CombatReachBetween(attacker, target, forMeleeRange, 0.0f);
    const float centres = (attacker.Where().Pos() - target.Where().Pos()).magnitude();
    return Geometry::Placement::Gap(centres, reach);
}

bool InMeleeReach(Unit const& attacker, Unit const& victim, float flat_mod)
{
    const float reach = CombatReachBetween(attacker, victim, true, flat_mod);

    // Combat reach replaces the extents entirely here, so the component is asked about
    // bare points rather than about two sized objects.
    return attacker.Where().WithinDist(victim.Where().Pos(), reach) &&
           attacker.Where().ShareFrame(victim.Where());
}

/**
 * @brief Sends a prepared non-melee spell damage log packet.
 *
 * @param log The prepared spell damage log data.
 */
void Unit::SendSpellNonMeleeDamageLog(SpellNonMeleeDamage* log)
{
    uint32 targetHealth = log->target->GetHealth();
    uint32 overkill = log->damage > targetHealth ? log->damage - targetHealth : 0;

    WorldPacket data(SMSG_SPELLNONMELEEDAMAGELOG, (16 + 4 + 4 + 4 + 1 + 4 + 4 + 1 + 1 + 4 + 4 + 1)); // we guess size
    data << log->target->GetPackGUID();
    data << log->attacker->GetPackGUID();
    data << uint32(log->SpellID);
    data << uint32(log->damage);                            // damage amount
    data << uint32(overkill);                               // overkill
    data << uint8(log->schoolMask);                         // damage school
    data << uint32(log->absorb);                            // AbsorbedDamage
    data << uint32(log->resist);                            // resist
    data << uint8(log->physicalLog);                        // if 1, then client show spell name (example: %s's ranged shot hit %s for %u school or %s suffers %u school damage from %s's spell_name
    data << uint8(log->unused);                             // unused
    data << uint32(log->blocked);                           // blocked
    data << uint32(log->HitInfo);
    data << uint8(0);                                       // flag to use extend data
    SendMessageToSet(&data, true);
}

/**
 * @brief Builds and sends a non-melee spell damage log packet.
 *
 * @param target The damage target.
 * @param SpellID The spell identifier.
 * @param Damage The original damage amount.
 * @param damageSchoolMask The damage school mask.
 * @param AbsorbedDamage The absorbed amount.
 * @param Resist The resisted amount.
 * @param PhysicalDamage True when the log should be treated as physical.
 * @param Blocked The blocked amount.
 * @param CriticalHit True when the hit was critical.
 */
void Unit::SendSpellNonMeleeDamageLog(Unit* target, uint32 SpellID, uint32 Damage, SpellSchoolMask damageSchoolMask, uint32 AbsorbedDamage, uint32 Resist, bool PhysicalDamage, uint32 Blocked, bool CriticalHit)
{
    SpellNonMeleeDamage log(this, target, SpellID, damageSchoolMask);
    log.damage = Damage - AbsorbedDamage - Resist - Blocked;
    log.absorb = AbsorbedDamage;
    log.resist = Resist;
    log.physicalLog = PhysicalDamage;
    log.blocked = Blocked;
    log.HitInfo = SPELL_HIT_TYPE_UNK1 | SPELL_HIT_TYPE_UNK3 | SPELL_HIT_TYPE_UNK6;
    if (CriticalHit)
    {
        log.HitInfo |= SPELL_HIT_TYPE_CRIT;
    }
    SendSpellNonMeleeDamageLog(&log);
}

/**
 * @brief Sends a periodic aura tick log packet to nearby clients.
 *
 * @param pInfo The periodic aura log information.
 */
void Unit::SendPeriodicAuraLog(SpellPeriodicAuraLogInfo* pInfo)
{
    Aura* aura = pInfo->aura;
    Modifier* mod = aura->GetModifier();

    WorldPacket data(SMSG_PERIODICAURALOG, 30);
    data << aura->GetTarget()->GetPackGUID();
    data << aura->GetCasterGuid().WriteAsPacked();
    data << uint32(aura->GetId());                          // spellId
    data << uint32(1);                                      // count
    data << uint32(mod->m_auraname);                        // auraId
    switch (mod->m_auraname)
    {
        case SPELL_AURA_PERIODIC_DAMAGE:
        case SPELL_AURA_PERIODIC_DAMAGE_PERCENT:
            data << uint32(pInfo->damage);                  // damage
            data << uint32(pInfo->overDamage);              // overkill?
            data << uint32(GetSpellSchoolMask(aura->GetSpellProto()));
            data << uint32(pInfo->absorb);                  // absorb
            data << uint32(pInfo->resist);                  // resist
            data << uint8(pInfo->critical ? 1 : 0);         // new 3.1.2 critical flag
            break;
        case SPELL_AURA_PERIODIC_HEAL:
        case SPELL_AURA_OBS_MOD_HEALTH:
            data << uint32(pInfo->damage);                  // damage
            data << uint32(pInfo->overDamage);              // overheal?
            data << uint32(pInfo->absorb);                  // absorb
            data << uint8(pInfo->critical ? 1 : 0);         // new 3.1.2 critical flag
            break;
        case SPELL_AURA_OBS_MOD_MANA:
        case SPELL_AURA_PERIODIC_ENERGIZE:
            data << uint32(mod->m_miscvalue);               // power type
            data << uint32(pInfo->damage);                  // damage
            break;
        case SPELL_AURA_PERIODIC_MANA_LEECH:
            data << uint32(mod->m_miscvalue);               // power type
            data << uint32(pInfo->damage);                  // amount
            data << float(pInfo->multiplier);               // gain multiplier
            break;
        default:
            sLog.outError("Unit::SendPeriodicAuraLog: unknown aura %u", uint32(mod->m_auraname));
            return;
    }

    aura->GetTarget()->SendMessageToSet(&data, true);
}

/**
 * @brief Sends a spell miss packet for a target.
 *
 * @param target The spell target.
 * @param spellID The spell identifier.
 * @param missInfo The miss reason.
 */
void Unit::SendSpellMiss(Unit* target, uint32 spellID, SpellMissInfo missInfo)
{
    WorldPacket data(SMSG_SPELLLOGMISS, (4 + 8 + 1 + 4 + 8 + 1));
    data << uint32(spellID);
    data << GetObjectGuid();
    data << uint8(0);                                       // can be 0 or 1, flag
    data << uint32(1);                                      // target count
    // for(i = 0; i < target count; ++i)
    data << target->GetObjectGuid();                        // target GUID
    data << uint8(missInfo);
    // end loop
    SendMessageToSet(&data, true);
}

/**
 * @brief Reports a spell whose damage resistance stopped in full.
 *
 * Distinct from \ref Unit::SendSpellMiss, which reports that a spell never
 * landed. This one lands and is then resisted down to nothing, which the client
 * shows as its own combat text type: its reader for opcode 0x0426 builds a
 * combat text entry and, when the local player is the target, raises
 * COMBAT_TEXT_UPDATE with the message type "SPELL_RESIST".
 *
 * Two flat uint64 GUIDs, not packed -- the client reads eight bytes for each.
 * The trailing byte is a log-format selector the client reads and discards.
 *
 * @param target The unit the spell was resisted by.
 * @param spellID The spell that was resisted.
 */
void Unit::SendSpellDamageResist(Unit* target, uint32 spellID)
{
    WorldPacket data(SMSG_PROCRESIST, 8 + 8 + 4 + 1);
    data << uint64(GetObjectGuid().GetRawValue());
    data << uint64(target->GetObjectGuid().GetRawValue());
    data << uint32(spellID);
    data << uint8(0);                                       // log format: 0 default, 1 debug
    SendMessageToSet(&data, true);
}

/**
 * @brief Sends an attacker state update using prepared melee damage data.
 *
 * @param damageInfo The prepared melee damage information.
 */
void Unit::SendAttackStateUpdate(CalcDamageInfo* damageInfo)
{
    DEBUG_FILTER_LOG(LOG_FILTER_COMBAT, "WORLD: Sending SMSG_ATTACKERSTATEUPDATE");

    uint32 targetHealth = damageInfo->target->GetHealth();
    uint32 overkill = damageInfo->damage > targetHealth ? damageInfo->damage - targetHealth : 0;

    uint32 count = 1;
    WorldPacket data(SMSG_ATTACKERSTATEUPDATE, (16 + 45));  // we guess size
    data << (uint32)damageInfo->HitInfo;
    data << damageInfo->attacker->GetPackGUID();
    data << damageInfo->target->GetPackGUID();
    data << (uint32)(damageInfo->damage);     // Full damage
    data << uint32(overkill);                               // overkill value
    data << uint8(count);                                   // Sub damage count

    for (uint32 i = 0; i < count; ++i)
    {
        data << uint32(damageInfo->damageSchoolMask);       // School of sub damage
        data << float(damageInfo->damage);                  // sub damage
        data << uint32(damageInfo->damage);                 // Sub Damage
    }

    if (damageInfo->HitInfo & (HITINFO_ABSORB | HITINFO_ABSORB2))
    {
        for (uint32 i = 0; i < count; ++i)
        {
            data << uint32(damageInfo->absorb);             // Absorb
        }
    }

    if (damageInfo->HitInfo & (HITINFO_RESIST | HITINFO_RESIST2))
    {
        for (uint32 i = 0; i < count; ++i)
        {
            data << uint32(damageInfo->resist);             // Resist
        }
    }

    data << uint8(damageInfo->TargetState);
    data << uint32(0);                                      // unknown, usually seen with -1, 0 and 1000
    data << uint32(0);                                      // spell id, seen with heroic strike and disarm as examples.
    // HITINFO_NOACTION normally set if spell

    if (damageInfo->HitInfo & HITINFO_BLOCK)
    {
        data << uint32(damageInfo->blocked_amount);
    }

    if (damageInfo->HitInfo & HITINFO_UNK22)
    {
        data << uint32(0);                                  // count of some sort?
    }

    if (damageInfo->HitInfo & HITINFO_UNK0)
    {
        data << uint32(0);
        data << float(0);
        data << float(0);
        data << float(0);
        data << float(0);
        data << float(0);
        data << float(0);
        data << float(0);
        data << float(0);
        for (uint8 i = 0; i < 5; ++i)
        {
            data << float(0);
            data << float(0);
        }
        data << uint32(0);
    }

    SendMessageToSet(&data, true);  /**/
}

void Unit::SendAttackStateUpdate(uint32 HitInfo, Unit* target, uint8 /*SwingType*/, SpellSchoolMask damageSchoolMask, uint32 Damage, uint32 AbsorbDamage, uint32 Resist, VictimState TargetState, uint32 BlockedAmount)
{
    CalcDamageInfo dmgInfo;
    dmgInfo.HitInfo = HitInfo;
    dmgInfo.attacker = this;
    dmgInfo.target = target;
    dmgInfo.damage = Damage - AbsorbDamage - Resist - BlockedAmount;
    dmgInfo.damageSchoolMask = damageSchoolMask;
    dmgInfo.absorb = AbsorbDamage;
    dmgInfo.resist = Resist;
    dmgInfo.TargetState = TargetState;
    dmgInfo.blocked_amount = BlockedAmount;
    SendAttackStateUpdate(&dmgInfo);
}

/**
 * @brief Builds and sends an attacker state update from raw damage values.
 *
 * @param HitInfo The hit information flags.
 * @param target The attack target.
 * @param damageSchoolMask The damage school mask.
 * @param Damage The original damage amount.
 * @param AbsorbDamage The absorbed amount.
 * @param Resist The resisted amount.
 * @param TargetState The victim state.
 * @param BlockedAmount The blocked amount.
 */
void Unit::SendAttackStateUpdate(uint32 HitInfo, Unit* target, SpellSchoolMask damageSchoolMask, uint32 Damage, uint32 AbsorbDamage, uint32 Resist, VictimState TargetState, uint32 BlockedAmount)
{
    CalcDamageInfo dmgInfo;
    dmgInfo.HitInfo = HitInfo;
    dmgInfo.attacker = this;
    dmgInfo.target = target;
    dmgInfo.damage = Damage - AbsorbDamage - Resist - BlockedAmount;
    dmgInfo.damageSchoolMask = damageSchoolMask;
    dmgInfo.absorb = AbsorbDamage;
    dmgInfo.resist = Resist;
    dmgInfo.TargetState = TargetState;
    dmgInfo.blocked_amount = BlockedAmount;
    SendAttackStateUpdate(&dmgInfo);
}

/**
 * @brief Starts attacking a victim.
 *
 * @param victim The victim to attack.
 * @param meleeAttack True to begin melee attacking immediately.
 * @return True if the attack started; otherwise, false.
 */
bool Unit::Attack(Unit* victim, bool meleeAttack)
{
    if (!victim || victim == this)
    {
        return false;
    }

    // dead units can neither attack nor be attacked
    if (!IsAlive() || !victim->IsInWorld() || !victim->IsAlive())
    {
        return false;
    }

    // player can not attack in mount state or on a taxi (no UNIT_FLAG_MOUNT on a passenger since P5-B family 5)
    if (GetTypeId() == TYPEID_PLAYER && (IsMounted() || IsTaxiFlying()))
    {
        return false;
    }

    // nobody can attack a game master in GM mode or an evading creature
    if (victim->isGameMaster() || victim->IsInEvadeMode())
    {
        return false;
    }

    // remove SPELL_AURA_MOD_UNATTACKABLE at attack (in case non-interruptible spells stun aura applied also that not let attack)
    if (HasAuraType(SPELL_AURA_MOD_UNATTACKABLE))
    {
        RemoveSpellsCausingAura(SPELL_AURA_MOD_UNATTACKABLE);
    }

    // in fighting already
    if (m_attacking)
    {
        if (m_attacking == victim)
        {
            // switch to melee attack from ranged/magic
            if (meleeAttack)
            {
                if (!hasUnitState(UNIT_STAT_MELEE_ATTACKING))
                {
                    addUnitState(UNIT_STAT_MELEE_ATTACKING);
                    SendMeleeAttackStart(victim);
                }
                return true;
            }
            return false;
        }

        // remove old target data
        AttackStop(true);
    }
    // new battle
    else
    {
        // set position before any AI calls/assistance
        AnchorCombat();
    }

    // The event row: a new combat drops a running distract (a no-op otherwise). Before the
    // victim is assigned, so a cancelled assistance distract's finalizer sees no victim and
    // does not stop and restart this very attack from inside it.
    GetMotionMaster()->CombatStarted();

    // Set our target — but NOT for PACIFIED creatures (training dummies, etc.).
    // The Cata 4.3.4 client auto-rotates a unit's model toward its UNIT_FIELD_TARGET
    // (a client-rendering behavior dating back to Vanilla), so even though the
    // server never sends an orientation packet for the dummy, the client locally
    // tracks whatever GUID lives in that field. Setting the target here is what
    // makes a hit-then-walked-around dummy spin to face the attacker: combat entry
    // → AttackStart → Attack → SetTargetGuid → client sees the field update →
    // client rotates the model. PACIFIED is supposed to mean "this unit doesn't
    // fight back" — that includes not visually fixating on its attacker.
    //
    // The TYPEID_UNIT guard is required: a player with a PACIFY-bearing debuff
    // (Sap, Polymorph, Repentance) must still get their UNIT_FIELD_TARGET set,
    // because in retail Cata the player's selected target persists across the CC
    // and queued attacks resume on break. Restricting the skip to creatures
    // avoids any PvP regression.
    if (!(GetTypeId() == TYPEID_UNIT && HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PACIFIED)))
    {
        SetTargetGuid(victim->GetObjectGuid());
    }

    if (meleeAttack)
    {
        addUnitState(UNIT_STAT_MELEE_ATTACKING);
    }

    m_attacking = victim;
    m_attacking->_addAttacker(this);

    AttackStarted();

    // delay offhand weapon attack to next attack time
    if (haveOffhandWeapon())
    {
        resetAttackTimer(OFF_ATTACK);
    }

    if (meleeAttack)
    {
        SendMeleeAttackStart(victim);
    }

    return true;
}

/**
 * @brief Stops the current attack target and clears attack state.
 *
 * @param targetSwitch True when stopping due to switching targets.
 * @return True if an attack was stopped; otherwise, false.
 */
bool Unit::AttackStop(bool targetSwitch /*=false*/)
{
    if (!m_attacking)
    {
        return false;
    }

    Unit* victim = m_attacking;

    m_attacking->_removeAttacker(this);
    m_attacking = NULL;

    // Clear our target
    SetTargetGuid(ObjectGuid());

    clearUnitState(UNIT_STAT_MELEE_ATTACKING);

    InterruptSpell(CURRENT_MELEE_SPELL);

    // reset only at real combat stop
    if (!targetSwitch)
    {
        AttackStopped();
    }

    SendMeleeAttackStop(victim);

    return true;
}

/**
 * @brief Stops combat, attackers, and optionally active casts.
 *
 * @param includingCast True to interrupt non-melee spells too.
 */
void Unit::CombatStop(bool includingCast)
{
    if (includingCast && IsNonMeleeSpellCasted(false))
    {
        InterruptNonMeleeSpells(false);
    }

    AttackStop();
    RemoveAllAttackers();

    SendAttackSwingCancelAttack();      // melee and ranged forced attack cancel
    CombatStopped();

    ClearInCombat();
}

/**
 * @brief Forces all attackers to stop attacking this unit.
 */
void Unit::RemoveAllAttackers()
{
    while (!m_attackers.empty())
    {
        AttackerSet::iterator iter = m_attackers.begin();
        if (!(*iter)->AttackStop())
        {
            sLog.outError("WORLD: Unit has an attacker that isn't attacking it!");
            m_attackers.erase(iter);
        }
    }
}

/**
 * @brief Sends a heal log packet to nearby clients.
 *
 * @param pVictim The healed unit.
 * @param SpellID The spell identifier.
 * @param Damage The healing amount.
 * @param critical True if the heal crit.
 */
void Unit::SendHealSpellLog(Unit* pVictim, uint32 SpellID, uint32 Damage, uint32 OverHeal, bool critical, uint32 absorb)
{
    // we guess size
    WorldPacket data(SMSG_SPELLHEALLOG, (8 + 8 + 4 + 4 + 1));
    data << pVictim->GetPackGUID();
    data << GetPackGUID();
    data << uint32(SpellID);
    data << uint32(Damage);
    data << uint32(OverHeal);
    data << uint32(absorb);
    data << uint8(critical ? 1 : 0);
    data << uint8(0);                                       // unused in client?
    SendMessageToSet(&data, true);
}

/**
 * @brief Puts the unit into combat with an enemy using PvP-aware rules.
 *
 * @param enemy The enemy unit.
 */
void Unit::SetInCombatWith(Unit* enemy)
{
    Unit* eOwner = enemy->GetCharmerOrOwnerOrSelf();
    if (eOwner->IsPvP())
    {
        SetInCombatState(true, enemy);
        return;
    }

    // check for duel
    Player const* enemyPlayer = eOwner->ToPlayer();
    if (enemyPlayer && enemyPlayer->duel)
    {
        if (Player const* myOwner = GetCharmerOrOwnerPlayerOrPlayerItself())
        {
            if (myOwner->IsInDuelWith(enemyPlayer))
            {
                SetInCombatState(true, enemy);
                return;
            }
        }
    }

    SetInCombatState(false, enemy);
}

/**
 * @brief Sets the unit into combat and triggers combat-entry side effects.
 *
 * @param PvP True for PvP combat rules.
 * @param enemy The enemy that caused combat.
 */
void Unit::SetInCombatState(bool PvP, Unit* enemy)
{
    // only alive units can be in combat
    if (!IsAlive())
    {
        return;
    }

    if (PvP)
    {
        m_CombatTimer = 5000;
    }

    bool wasOutOfCombat = !HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_IN_COMBAT);

    SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_IN_COMBAT);

    if (IsCharmed() || IsPet())
    {
        SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PET_IN_COMBAT);
    }

    // interrupt all delayed non-combat casts
    for (uint32 i = CURRENT_FIRST_NON_MELEE_SPELL; i < CURRENT_MAX_SPELL; ++i)
    {
        if (Spell* spell = GetCurrentSpell(CurrentSpellTypes(i)))
        {
            if (IsNonCombatSpell(spell->m_spellInfo))
            {
                InterruptSpell(CurrentSpellTypes(i), false);
            }
        }
    }

    if (getRace() == RACE_WORGEN && !IsInWorgenForm(true) && HasWorgenForm())
    {
        CastSpell(this, 97709, true);   // cast Altered Form
    }

    if (wasOutOfCombat)
    {
        EnteredCombat(enemy);
    }
}

/**
 * @brief Clears combat flags and combat-related state on the unit.
 */
void Unit::ClearInCombat()
{
    m_CombatTimer = 0;
    RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_IN_COMBAT);

    if (IsCharmed() || IsPet())
    {
        RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PET_IN_COMBAT);
    }

    LeftCombat();
    UpdatePotionCooldown();
}

/**
 * @brief Clears combo point ownership references targeting this unit.
 */
void Unit::ClearComboPointHolders()
{
    while (!m_ComboPointHolders.empty())
    {
        uint32 lowguid = *m_ComboPointHolders.begin();

        Player* plr = sObjectMgr.GetPlayer(ObjectGuid(HIGHGUID_PLAYER, lowguid));
        if (plr && plr->GetComboTargetGuid() == GetObjectGuid())// recheck for safe
        {
            plr->ClearComboPoints();                         // remove also guid from m_ComboPointHolders;
        }
        else
        {
            m_ComboPointHolders.erase(lowguid);              // or remove manually
        }
    }
}

/**
 * @brief Clears all reactive timers and related aura states.
 */
void Unit::ClearAllReactives()
{
    for (int i = 0; i < MAX_REACTIVE; ++i)
    {
        m_reactiveTimer[i] = 0;
    }

    if (HasAuraState(AURA_STATE_DEFENSE))
    {
        ModifyAuraState(AURA_STATE_DEFENSE, false);
    }
    if (getClass() == CLASS_HUNTER && HasAuraState(AURA_STATE_HUNTER_PARRY))
    {
        ModifyAuraState(AURA_STATE_HUNTER_PARRY, false);
    }

    if (getClass() == CLASS_WARRIOR && GetTypeId() == TYPEID_PLAYER)
    {
        ClearComboPoints();
    }
}

/**
 * @brief Updates reactive ability timers and expires associated states.
 *
 * @param p_time The elapsed update time in milliseconds.
 */
void Unit::UpdateReactives(uint32 p_time)
{
    for (int i = 0; i < MAX_REACTIVE; ++i)
    {
        ReactiveType reactive = ReactiveType(i);

        if (!m_reactiveTimer[reactive])
        {
            continue;
        }

        if (m_reactiveTimer[reactive] <= p_time)
        {
            m_reactiveTimer[reactive] = 0;

            switch (reactive)
            {
                case REACTIVE_DEFENSE:
                    if (HasAuraState(AURA_STATE_DEFENSE))
                    {
                        ModifyAuraState(AURA_STATE_DEFENSE, false);
                    }
                    break;
                case REACTIVE_HUNTER_PARRY:
                    if (getClass() == CLASS_HUNTER && HasAuraState(AURA_STATE_HUNTER_PARRY))
                    {
                        ModifyAuraState(AURA_STATE_HUNTER_PARRY, false);
                    }
                    break;
                case REACTIVE_OVERPOWER:
                    if (getClass() == CLASS_WARRIOR && GetTypeId() == TYPEID_PLAYER)
                    {
                        ClearComboPoints();
                    }
                    break;
                default:
                    break;
            }
        }
        else
        {
            m_reactiveTimer[reactive] -= p_time;
        }
    }
}

/**
 * @brief Gets the attack power damage multiplier for an attack type.
 *
 * @param attType The attack type.
 * @param normalized True to use normalized weapon speed rules.
 * @return The attack power multiplier.
 */
float Unit::GetAPMultiplier(WeaponAttackType attType, bool normalized)
{
    uint32 const attackTime = GetAttackTime(attType);
    bool const isPlayer = GetTypeId() == TYPEID_PLAYER;

    bool hasWeapon = false;
    uint32 weaponInventoryType = 0;
    uint32 weaponSubClass = 0;
    if (normalized && isPlayer)
    {
        if (Item* Weapon = GetWeaponForAttack(attType, true, false))
        {
            hasWeapon = true;
            weaponInventoryType = Weapon->GetProto()->InventoryType;
            weaponSubClass = Weapon->GetProto()->SubClass;
        }
    }

    return Combat::APMultiplier(attackTime, isPlayer, normalized, hasWeapon,
                                weaponInventoryType, weaponSubClass);
}

/**
 * @brief Notifies AI and pets that the unit was attacked.
 *
 * @param attacker The attacking unit.
 */
void Unit::AttackedBy(Unit* attacker)
{
    // trigger AI reaction
    if (GetTypeId() == TYPEID_UNIT && AI())
    {
        AI()->AttackedBy(attacker);
    }

    // do not pet reaction for self inflicted damage (like environmental)
    if (attacker == this)
    {
        return;
    }

    // trigger pet AI reaction
    if (Unit* pet = _GetPet())
    {
        pet->AttackedBy(attacker);
    }
}

struct CombatStopWithPetsHelper
{
    explicit CombatStopWithPetsHelper(bool _includingCast) : includingCast(_includingCast) {}
    void operator()(Unit* unit) const { unit->CombatStop(includingCast); }
    bool includingCast;
};

/**
 * @brief Stops combat for the unit and all controlled pets or guardians.
 *
 * @param includingCast True to interrupt non-melee spells too.
 */
void Unit::CombatStopWithPets(bool includingCast)
{
    CombatStop(includingCast);
    CallForAllControlledUnits(CombatStopWithPetsHelper(includingCast), CONTROLLED_PET | CONTROLLED_GUARDIANS | CONTROLLED_CHARM);
}

struct IsAttackingPlayerHelper
{
    explicit IsAttackingPlayerHelper() {}
    bool operator()(Unit const* unit) const { return unit->isAttackingPlayer(); }
};

/**
 * @brief Checks whether the unit or any controlled unit is attacking a player.
 *
 * @return True if a player is being attacked; otherwise, false.
 */
bool Unit::isAttackingPlayer() const
{
    if (hasUnitState(UNIT_STAT_ATTACK_PLAYER))
    {
        return true;
    }

    return CheckAllControlledUnits(IsAttackingPlayerHelper(), CONTROLLED_PET | CONTROLLED_TOTEMS | CONTROLLED_GUARDIANS | CONTROLLED_CHARM);
}

struct StopAttackFactionHelper
{
    explicit StopAttackFactionHelper(uint32 _faction_id) : faction_id(_faction_id) {}
    void operator()(Unit* unit) const { unit->StopAttackFaction(faction_id); }
    uint32 faction_id;
};

/**
 * @brief Stops attacking units from a specific faction.
 *
 * @param faction_id The faction identifier to stop attacking.
 */
void Unit::StopAttackFaction(uint32 faction_id)
{
    if (Unit* victim = getVictim())
    {
        if (victim->getFactionTemplateEntry()->Faction == faction_id)
        {
            AttackStop();
            if (IsNonMeleeSpellCasted(false))
            {
                InterruptNonMeleeSpells(false);
            }

            // melee and ranged forced attack cancel
            if (GetTypeId() == TYPEID_PLAYER)
            {
                SendAttackSwingCancelAttack();
            }
        }
    }

    AttackerSet const& attackers = getAttackers();
    for (AttackerSet::const_iterator itr = attackers.begin(); itr != attackers.end();)
    {
        if ((*itr)->getFactionTemplateEntry()->Faction == faction_id)
        {
            (*itr)->AttackStop();
            itr = attackers.begin();
        }
        else
        {
            ++itr;
        }
    }

    GetHostileRefManager().deleteReferencesForFaction(faction_id);

    CallForAllControlledUnits(StopAttackFactionHelper(faction_id), CONTROLLED_PET | CONTROLLED_GUARDIANS | CONTROLLED_CHARM);
}
