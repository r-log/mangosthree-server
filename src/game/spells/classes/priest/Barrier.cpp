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

#include "spells/classes/priest/Barrier.h"
#include "spells/data/SpellMgr.h"
#include "ai/CreatureAI.h"
#include "entities/Creature.h"
#include "Map.h"
#include "DBCStores.h"

namespace
{
    uint32 const BARRIER_SPELL[] = { 62618 };
    uint32 const BARRIER_CREATURE = 33248;
    uint32 const BARRIER_AURA[] = { 81781 };
    uint32 const BARRIER_GLYPH_SPELL = 90785;
    uint32 const GLYPH_OF_POWER_WORD_BARRIER = 55689;

    /// The barriers one summon of 62618 makes.
    int32 const BARRIER_SUMMONS = 1;

    /// The name the barrier's creature AI is registered under.
    char const* const BARRIER_AI = "BarrierAI";

    /// The barrier's priest: the unit of `priestGuid` on `barrier`'s map, when he is in the world; NULL otherwise.
    Unit* BarrierPriest(Unit const& barrier, ObjectGuid const& priestGuid)
    {
        Map* map = barrier.FindMap();
        Unit* priest = map && !priestGuid.IsEmpty() ? map->GetUnit(priestGuid) : NULL;
        return priest && priest->IsInWorld() ? priest : NULL;
    }

    /// Power Word: Barrier's creature, the barrier. On its first update a barrier that 62618 made takes its life,
    /// 62618's duration; its summoner, when he is on its map, as its owner and its creator, and his faction as its
    /// own, so that the casts of its aura take the units friendly to him; and it channels 81781 at itself. On every
    /// update it counts its life down, and it goes (Priest::BarrierStands) when the life has run out or its priest is
    /// dead or not on its map. A barrier no cast of 62618 made does nothing. It attacks nothing, moves nowhere and
    /// evades nothing.
    class BarrierAI : public CreatureAI
    {
        public:
            explicit BarrierAI(Creature* barrier) : CreatureAI(barrier), m_lifeMs(0), m_started(false),
                m_standing(false) {}

            void Reset() override {}

            /// Starts the barrier on its first update, when the summon has set the spell that created it; then counts
            /// its life down and goes as the class comment says.
            void UpdateAI(uint32 const diff) override
            {
                if (!m_started)
                {
                    Start();
                }
                if (!m_standing)
                {
                    return;
                }
                m_lifeMs -= int32(diff);
                Unit* priest = BarrierPriest(*m_creature, m_creature->GetOwnerGuid());
                if (!Priest::BarrierStands(m_lifeMs, priest && priest->IsAlive()))
                {
                    m_standing = false;
                    m_creature->ForcedDespawn();
                }
            }

            void AttackStart(Unit* /*who*/) override {}
            void MoveInLineOfSight(Unit* /*who*/) override {}
            void EnterCombat(Unit* /*enemy*/) override {}
            void EnterEvadeMode() override {}

            /// PERMIT_BASE_SPECIAL for the barrier's creature, PERMIT_BASE_NO for any other.
            static int Permissible(Creature const* creature)
            {
                return Priest::IsBarrierCreature(creature->GetEntry()) ? PERMIT_BASE_SPECIAL : PERMIT_BASE_NO;
            }

        private:
            /// The barrier's start: for a barrier 62618 made, its life, 62618's duration; then, when its summoner is
            /// on its map, he as its owner and its creator, his faction as its own, and its channel of 81781 at
            /// itself, cast as a creature casts a spell of its own.
            void Start()
            {
                m_started = true;
                if (m_creature->GetUInt32Value(UNIT_CREATED_BY_SPELL) != BARRIER_SPELL[0])
                {
                    return;
                }
                m_standing = true;
                m_lifeMs = GetSpellDuration(sSpellStore.LookupEntry(BARRIER_SPELL[0]));
                Unit* priest = BarrierPriest(*m_creature, m_creature->GetSummonerGuid());
                if (!priest)
                {
                    return;
                }
                m_creature->SetOwnerGuid(priest->GetObjectGuid());
                m_creature->SetCreatorGuid(priest->GetObjectGuid());
                m_creature->setFaction(priest->getFaction());
                m_creature->CastSpell(m_creature, BARRIER_AURA[0], false);
            }

            int32 m_lifeMs;         ///< the milliseconds the barrier has left
            bool m_started;         ///< the first update has run
            bool m_standing;        ///< a barrier 62618 made that has not gone
    };

    /// 62618: Power Word: Barrier, at the targets of its effect: the summon's effect, whose targets are the units
    /// friendly to the priest within 7 yards of the point and which summons once for each of them, takes the priest
    /// alone, so that each cast summons once at the point, whoever stands there (BarrierSummonCount makes that summon
    /// one barrier). Return(): the list is final.
    /// Any other effect keeps its list.
    SpellHandlerOutcome<void> BarrierSummonTargets(SelectTargetsContext& ctx)
    {
        SpellEffectEntry const* effect = ctx.spell.GetSpellEffect(ctx.effIndex);
        if (!effect || effect->Effect != SPELL_EFFECT_SUMMON)
        {
            return SpellHandlerOutcome<void>::Continue();
        }
        ctx.targets.clear();
        ctx.targets.push_back(&ctx.caster);
        return SpellHandlerOutcome<void>::Return();
    }

    /// 62618: Power Word: Barrier, at the value of its effect: the summon's value, which the spell's scaling row
    /// raises with the priest's level (22.456 times the level's scaling value: 21225 at level 85, the barrier's health
    /// on retail), is the number of creatures the summon makes (Spell::EffectSummonType); it becomes one
    /// (Priest::BarrierSummonValue), so that the summon makes one barrier. Any other effect keeps its value.
    SpellHandlerOutcome<void> BarrierSummonCount(EffectValueContext& ctx)
    {
        SpellEffectEntry const* effect = ctx.spell.GetSpellEffect(ctx.effIndex);
        ctx.value = Priest::BarrierSummonValue(effect ? effect->Effect : 0, ctx.value);
        return SpellHandlerOutcome<void>::Continue();
    }

    /// 81781: the barrier's aura, on each tick of either of its periodic triggers: the tick casts its trigger spell
    /// as the aura's data names it (Priest::BarrierTickCasts), save the glyph's 90785, which the tick casts only while
    /// the barrier's priest (its owner, on its map) holds Glyph of Power Word: Barrier (55689). Return() ends the tick:
    /// nothing is cast.
    SpellHandlerOutcome<void> BarrierTick(PeriodicTickContext& ctx)
    {
        SpellEffectEntry const* effect = ctx.spell.GetSpellEffect(ctx.effIndex);
        uint32 const trigger = effect ? effect->EffectTriggerSpell : 0;
        Unit* priest = BarrierPriest(ctx.target, ctx.target.GetOwnerGuid());
        if (Priest::BarrierTickCasts(trigger, priest && priest->HasAura(GLYPH_OF_POWER_WORD_BARRIER)))
        {
            return SpellHandlerOutcome<void>::Continue();
        }
        return SpellHandlerOutcome<void>::Return();
    }
}

bool Priest::IsBarrierCreature(uint32 creatureEntry)
{
    return creatureEntry && creatureEntry == BARRIER_CREATURE;
}

bool Priest::BarrierTickCasts(uint32 triggerSpell, bool glyph)
{
    return triggerSpell != BARRIER_GLYPH_SPELL || glyph;
}

bool Priest::BarrierStands(int32 lifeMs, bool priest)
{
    return lifeMs > 0 && priest;
}

int32 Priest::BarrierSummonValue(uint32 effect, int32 value)
{
    return effect == SPELL_EFFECT_SUMMON ? BARRIER_SUMMONS : value;
}

uint32 Priest::RegisterBarrier(SpellHandlerRegistry& registry)
{
    CreatureAIRegistry& ais = CreatureAIRepository::Instance();
    if (!ais.HasItem(BARRIER_AI))
    {
        (new CreatureAIFactory<BarrierAI>(BARRIER_AI))->RegisterSelf();
    }

    uint32 rows = RegisterRanks<SelectTargetsHook>(registry, BARRIER_SPELL, &BarrierSummonTargets);
    rows += RegisterRanks<EffectValueHook>(registry, BARRIER_SPELL, &BarrierSummonCount);
    rows += RegisterRanks<PeriodicTickHook>(registry, BARRIER_AURA, &BarrierTick);
    return rows;
}
