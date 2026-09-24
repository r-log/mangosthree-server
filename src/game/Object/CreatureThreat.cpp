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

/**
 * @file CreatureThreat.cpp
 * @brief The threat methods only a Creature can execute: hostile-target selection
 *        and taunt application/fade-out.
 *
 * Decoupling D5e (server #134). Moved unchanged from Unit (UnitThreat.cpp), where each
 * opened with MANGOS_ASSERT(GetTypeId() == TYPEID_UNIT) and then downcast this to a
 * Creature to reach AI(), SetCannotReachTarget() and the evade hooks. The asserts are
 * kept -- they are now trivially true -- and the downcasts on this are gone. Unit's
 * private m_ThreatManager and m_HostileRefManager are reached through the public
 * GetThreatManager()/GetHostileRefManager() accessors that stay on Unit; nothing else in
 * any body changed. No behaviour change.
 */

#include "Utilities/Errors.h"
#include "Creature.h"
#include "Unit.h"
#include "Player.h"
#include "CreatureAI.h"
#include "SpellAuras.h"
#include "InstanceData.h"
#include "CreatureLinkingMgr.h"
#include "MotionMaster.h"
#include "Map.h"

//======================================================================

void Creature::TauntApply(Unit* taunter)
{
    MANGOS_ASSERT(GetTypeId() == TYPEID_UNIT);

    if (!taunter || (taunter->GetTypeId() == TYPEID_PLAYER && ((Player*)taunter)->isGameMaster()))
    {
        return;
    }

    if (!CanHaveThreatList())
    {
        return;
    }

    Unit* target = getVictim();

    if (target && target == taunter)
    {
        return;
    }

    // Only attack taunter if this is a valid target
    if (!(Blocked(Motion::ReasonStunned) || IsFeigningDeath()) && !IsSecondChoiceTarget(taunter, true))
    {
        if (GetTargetGuid() || !target)
        {
            SetInFront(taunter);
        }

        if (AI())
        {
            AI()->AttackStart(taunter);
        }
    }

    GetThreatManager().tauntApply(taunter);
}

//======================================================================

void Creature::TauntFadeOut(Unit* taunter)
{
    MANGOS_ASSERT(GetTypeId() == TYPEID_UNIT);

    if (!taunter || (taunter->GetTypeId() == TYPEID_PLAYER && ((Player*)taunter)->isGameMaster()))
    {
        return;
    }

    if (!CanHaveThreatList())
    {
        return;
    }

    Unit* target = getVictim();

    if (!target || target != taunter)
    {
        return;
    }

    if (GetThreatManager().isThreatListEmpty())
    {
        m_fixateTargetGuid.Clear();

        if (AI())
        {
            AI()->EnterEvadeMode();
        }

        if (InstanceData* mapInstance = GetInstanceData())
        {
            mapInstance->OnCreatureEvade(this);
        }

        if (m_isCreatureLinkingTrigger)
        {
            GetMap()->GetCreatureLinkingHolder()->DoCreatureLinkingEvent(LINKING_EVENT_EVADE, this);
        }

        return;
    }

    GetThreatManager().tauntFadeOut(taunter);
    target = GetThreatManager().getHostileTarget();

    if (target && target != taunter)
    {
        if (GetTargetGuid())
        {
            SetInFront(target);
        }

        if (AI())
        {
            AI()->AttackStart(target);
        }
    }
}

//======================================================================

bool Creature::SelectHostileTarget()
{
    // function provides main threat functionality
    // next-victim-selection algorithm and evade mode are called
    // threat list sorting etc.

    MANGOS_ASSERT(GetTypeId() == TYPEID_UNIT);

    if (!this->IsAlive())
    {
        return false;
    }

    // This function only useful once AI has been initialized
    if (!AI())
    {
        return false;
    }

    Unit* target = NULL;
    Unit* oldTarget = getVictim();

    // first check if we should fixate a target
    if (m_fixateTargetGuid)
    {
        if (oldTarget && oldTarget->GetObjectGuid() == m_fixateTargetGuid)
        {
            target = oldTarget;
        }
        else
        {
            Unit* pFixateTarget = GetMap()->GetUnit(m_fixateTargetGuid);
            if (pFixateTarget && pFixateTarget->IsAlive() && !IsSecondChoiceTarget(pFixateTarget, true))
            {
                target = pFixateTarget;
            }
        }
    }
    // then checking if we have some taunt on us
    if (!target)
    {
        const AuraList& tauntAuras = GetAurasByType(SPELL_AURA_MOD_TAUNT);
        Unit* caster;

        // Find first available taunter target
        // Auras are pushed_back, last caster will be on the end
        for (AuraList::const_reverse_iterator aura = tauntAuras.rbegin(); aura != tauntAuras.rend(); ++aura)
        {
            if ((caster = (*aura)->GetCaster()) && caster->Where().ShareFrame(this->Where()) &&
                caster->IsTargetableForAttack() && caster->isInAccessablePlaceFor(this) &&
                !IsSecondChoiceTarget(caster, true))
            {
                target = caster;
                break;
            }
        }
    }

    // No valid fixate target, taunt aura or taunt aura caster is dead, standard target selection
    if (!target && !GetThreatManager().isThreatListEmpty())
    {
        target = GetThreatManager().getHostileTarget();
    }

    if (target)
    {
        if (!(Blocked(Motion::ReasonStunned) || IsFeigningDeath()))
        {
            // PACIFIED creatures (training dummies, etc.) keep their spawn
            // orientation. PACIFIED already gates attack initiation, so visual
            // auto-facing here is purely cosmetic AND it falsifies the angle-of-
            // attack rules in RollMeleeOutcomeAgainst (the from-behind gate for
            // parry/block) for any player testing combat against the creature.
            if (!HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PACIFIED))
            {
                SetInFront(target);
            }
            if (oldTarget != target)
            {
                AI()->AttackStart(target);
            }

            // check if currently selected target is reachable
            // NOTE: path alrteady generated from AttackStart()
            if (!GetMotionMaster()->IsReachable())
            {
                // remove all taunts
                RemoveSpellsCausingAura(SPELL_AURA_MOD_TAUNT);

                if (GetThreatManager().getThreatList().size() < 2)
                {
                    // only one target in list: keep trying and evade once the
                    // no-path grace timer expires (see Creature::Update)
                    SetCannotReachTarget(true);
                }
                else
                {
                    // remove unreachable target from our threat list
                    // next iteration we will select next possible target
                    GetHostileRefManager().deleteReference(target);
                    GetThreatManager().modifyThreatPercent(target, -101);

                    // remove target from current attacker, do not exit combat settings
                    AttackStop(true);

                    return false;
                }
            }
            else
            {
                SetCannotReachTarget(false);
            }
        }
        return true;
    }

    // no target but something prevent go to evade mode
    if (!IsInCombat() || HasAuraType(SPELL_AURA_MOD_TAUNT))
    {
        return false;
    }

    // last case when creature don't must go to evade mode:
    // it in combat but attacker not make any damage and not enter to aggro radius to have record in threat list
    // for example at owner command to pet attack some far away creature
    // Note: creature not have targeted movement generator but have attacker in this case
    // (what runs now: a chase masked by a fear or an effect still scans its attackers, as the
    // stack's top-based check did, instead of evading out from under the mask)
    if (GetMotionMaster()->ActiveKind() != Motion::Kind::Chase)
    {
        for (AttackerSet::const_iterator itr = m_attackers.begin(); itr != m_attackers.end(); ++itr)
        {
            if ((*itr)->Where().ShareFrame(this->Where()) && (*itr)->IsTargetableForAttack() && (*itr)->isInAccessablePlaceFor(this))
            {
                return false;
            }
        }
    }

    // enter in evade mode in other case
    m_fixateTargetGuid.Clear();
    AI()->EnterEvadeMode();

    if (InstanceData* mapInstance = GetInstanceData())
    {
        mapInstance->OnCreatureEvade(this);
    }

    if (m_isCreatureLinkingTrigger)
    {
        GetMap()->GetCreatureLinkingHolder()->DoCreatureLinkingEvent(LINKING_EVENT_EVADE, this);
    }

    return false;
}
