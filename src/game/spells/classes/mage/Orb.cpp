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

#include "spells/classes/mage/Mage.h"
#include "spells/data/SpellMgr.h"
#include "spells/SpellScriptQueries.h"
#include "ai/CreatureAI.h"
#include "entities/Creature.h"
#include "Map.h"
#include "entities/motion/MotionMaster.h"
#include "DBCStores.h"
#include "Utilities/Util.h"

namespace
{
    /// The orbs of the mage's spells, one row each: Flame Orb.
    Mage::OrbSpells const ORBS[] =
    {
        { 82731, 84765, 44214, 82690, 82734, 82739, 82736, 83619, { 18459, 18460, 54734 } },
    };

    /// The name the orb's creature AI is registered under.
    char const* const ORB_AI = "OrbAI";

    /// The id of the point the orb flies to.
    uint32 const ORB_PATH_POINT = 1;

    /// How far from its point the orb may stand for its flight to count as ended there, in yards.
    float const ORB_PATH_END = 1.0f;

    /// The orb's owner when he may have the orb act for him (Mage::OrbOwnerActs): looked up on the orb's own map by
    /// the guid of his summon; NULL otherwise.
    Unit* ActingOwner(Creature const& orb)
    {
        Map* map = orb.FindMap();
        Unit* owner = map ? map->GetUnit(orb.GetSummonerGuid()) : NULL;
        bool const found = owner != NULL;
        if (!Mage::OrbOwnerActs(found, found && owner->IsAlive(), found && owner->IsInWorld(),
                                found && owner->FindMap() == map))
        {
            return NULL;
        }
        return owner;
    }

    /// 82731: Flame Orb: the mage casts the summon of his orb (84765), triggered. The script ends the effect.
    SpellHandlerOutcome<void> FlameOrbEffect(DummyEffectContext& ctx)
    {
        Mage::OrbSpells const* orb = Mage::OrbSpellsFor(ctx.spell.ID);
        if (ctx.effIndex == EFFECT_INDEX_0 && orb)
        {
            ctx.CastTriggered(ctx.caster, orb->summonSpell);
        }
        return SpellHandlerOutcome<void>::Return();
    }

    /// 82734: the orb's scan: of the enemies within its radius keeps the closest one the orb may strike
    /// (Mage::ClosestOrbTarget), or none.
    SpellHandlerOutcome<void> OrbScanTargets(SelectTargetsContext& ctx)
    {
        Unit* closest = Mage::ClosestOrbTarget(ctx.caster, ctx.targets);
        ctx.targets.clear();
        if (closest)
        {
            ctx.targets.push_back(closest);
        }
        return SpellHandlerOutcome<void>::Continue();
    }

    /// 82734: the orb's scan, for the enemy it kept. On its first enemy the orb snares itself (82736) and counts
    /// that scan as scan 0. While it holds the snare, each scan with an enemy counts one more, and on the scans
    /// Mage::OrbHitDue names (scan 0 and every second scan of the 500 ms aura after it, for the 30 scans the snare
    /// lasts) the orb casts its hit (82739) at the enemy, with its summoner as the original caster, when its
    /// summoner is alive and in the world on its map (Mage::OrbOwnerActs). The aura's period carries its remainder
    /// from tick to tick, so the hits keep one second apart on any update rate. The count is the script word of
    /// effect 0 of the orb's own aura (82690), and the word of its effect 1 is 1 once an enemy was found. An orb
    /// without that aura does nothing. The script ends the effect.
    SpellHandlerOutcome<void> OrbScanEffect(DummyEffectContext& ctx)
    {
        Creature* orb = ctx.caster.ToCreature();
        Mage::OrbSpells const* spells = orb ? Mage::OrbSpellsFor(orb->GetUInt32Value(UNIT_CREATED_BY_SPELL)) : NULL;
        if (ctx.effIndex != EFFECT_INDEX_0 || !ctx.target || !spells)
        {
            return SpellHandlerOutcome<void>::Return();
        }
        int32* scans = ScriptWordOf(*orb, spells->orbAura, EFFECT_INDEX_0);
        int32* found = ScriptWordOf(*orb, spells->orbAura, EFFECT_INDEX_1);
        if (!scans || !found)
        {
            return SpellHandlerOutcome<void>::Return();
        }
        if (!*found)
        {
            *found = 1;
            *scans = 0;
            orb->CastSpell(orb, spells->snareSpell, true);
        }
        if (!orb->HasAura(spells->snareSpell))
        {
            return SpellHandlerOutcome<void>::Return();
        }
        uint32 const scan = uint32((*scans)++);
        if (!Mage::OrbHitDue(scan, Mage::OrbCadenceFor(*spells)))
        {
            return SpellHandlerOutcome<void>::Return();
        }
        Unit* summoner = ActingOwner(*orb);
        if (!summoner)
        {
            return SpellHandlerOutcome<void>::Return();
        }
        orb->CastSpell(ctx.target, spells->hitSpell, true, NULL, NULL, summoner->GetObjectGuid());
        return SpellHandlerOutcome<void>::Return();
    }

    /// The orb of a mage, owned by him and of his level: flies straight ahead from where it was summoned at its run
    /// speed, slowed by its snare when it finds an enemy; when the snare runs out (its firing is over), or for an orb
    /// that found nothing at its summon's end (the summon's own end, or its life's when it is in combat), when its
    /// summoner is alive and on its map, rolls the proc chance of the summoner's explosion talent for the explosion
    /// (83619), cast with the summoner as the original caster, and goes. It attacks nothing, follows nothing and evades
    /// nothing.
    class OrbAI : public CreatureAI
    {
        public:
            explicit OrbAI(Creature* orb) : CreatureAI(orb), m_spells(NULL), m_lifeMs(0), m_ended(false), m_snared(false) {}

            /// The orb's start: its row, by the spell that created it; its summoner as its owner, so the orb counts as
            /// his and its scan takes the units he may attack, as his own area spells take them; its summoner's level,
            /// as a totem takes its owner's, so its hit and its explosion roll against their target by his level; its
            /// life, the summon spell's duration; and its flight to the point its life at its run speed takes it
            /// straight ahead, at its height. An orb of no row ends here and does nothing.
            void Reset() override
            {
                m_spells = Mage::OrbSpellsFor(m_creature->GetUInt32Value(UNIT_CREATED_BY_SPELL));
                m_ended = m_spells == NULL;
                if (m_ended)
                {
                    return;
                }
                m_creature->SetOwnerGuid(m_creature->GetSummonerGuid());
                if (Unit* summoner = ActingOwner(*m_creature))
                {
                    m_creature->SetLevel(summoner->getLevel());
                }
                m_lifeMs = GetSpellDuration(sSpellStore.LookupEntry(m_spells->summonSpell));
                float const distance = float(m_lifeMs > 0 ? m_lifeMs : 0) / 1000.0f * m_creature->GetSpeed(MOVE_RUN);
                m_goal = m_creature->Where().PointAhead(distance);
                FlyToGoal();
            }

            /// Starts the orb on its first update, when the summon has set the spell that created it; then counts its
            /// life down, notes its snare, and ends it when the snare has run out or the life has (Mage::OrbEndDue).
            void UpdateAI(uint32 const diff) override
            {
                if (m_ended)
                {
                    return;
                }
                if (!m_spells)
                {
                    Reset();
                    if (m_ended)
                    {
                        return;
                    }
                }
                m_lifeMs -= int32(diff);
                bool const snared = m_creature->HasAura(m_spells->snareSpell);
                if (Mage::OrbEndDue(m_snared, snared, m_lifeMs))
                {
                    End(true);
                    return;
                }
                m_snared = m_snared || snared;
            }

            /// The summon's own end (TemporarySummon::UnSummon tells the orb's AI first), which an orb that found
            /// nothing reaches before its life runs out: the orb ends there, and the summon takes it away.
            void SummonedCreatureDespawn(Creature* summoned) override
            {
                if (summoned == m_creature && !m_ended && m_spells)
                {
                    End(false);
                }
            }

            /// Flies on to the point when the flight ended short of it.
            void MovementInform(Motion::Kind kind, uint32 id) override
            {
                if (kind == Motion::Kind::Point && id == ORB_PATH_POINT && !m_ended && m_spells &&
                    !m_creature->Where().WithinDist(m_goal, ORB_PATH_END))
                {
                    FlyToGoal();
                }
            }

            void AttackStart(Unit* /*who*/) override {}
            void MoveInLineOfSight(Unit* /*who*/) override {}
            void EnterCombat(Unit* /*enemy*/) override {}
            void EnterEvadeMode() override {}

            /// PERMIT_BASE_SPECIAL for a creature of the orb table, PERMIT_BASE_NO for any other.
            static int Permissible(Creature const* creature)
            {
                return Mage::IsOrbCreature(creature->GetEntry()) ? PERMIT_BASE_SPECIAL : PERMIT_BASE_NO;
            }

        private:
            /// A straight flight to the point, with no path generated.
            void FlyToGoal()
            {
                m_creature->GetMotionMaster()->MovePoint(ORB_PATH_POINT, m_goal.x, m_goal.y, m_goal.z, false);
            }

            /// The orb's end: the explosion, when its summoner is alive on its map, knows one of the row's explosion
            /// talents and the talent's chance rolls; then, with `despawn`, the orb despawns.
            void End(bool despawn)
            {
                m_ended = true;
                if (Unit* owner = ActingOwner(*m_creature))
                {
                    uint32 talent = 0;
                    for (uint32 spellId : m_spells->explosionTalents)
                    {
                        if (spellId && owner->HasAura(spellId))
                        {
                            talent = spellId;
                            break;
                        }
                    }
                    if (talent && roll_chance_i(int(Mage::OrbExplosionChance(talent))))
                    {
                        m_creature->CastSpell(m_creature, m_spells->explosionSpell, true, NULL, NULL, owner->GetObjectGuid());
                    }
                }
                if (despawn)
                {
                    m_creature->ForcedDespawn();
                }
            }

            Mage::OrbSpells const* m_spells;    ///< the orb's row; NULL until its start
            int32 m_lifeMs;                     ///< the milliseconds the orb has left
            bool m_ended;                       ///< the orb ended, or had no row
            bool m_snared;                      ///< the orb has held its snare: it found an enemy
            Geometry::Vector3 m_goal;           ///< the point the orb flies to
    };
}

bool Mage::OrbOwnerActs(bool found, bool alive, bool inWorld, bool onOrbsMap)
{
    return found && alive && inWorld && onOrbsMap;
}

bool Mage::OrbEndDue(bool wasSnared, bool snaredNow, int32 lifeMs)
{
    return (wasSnared && !snaredNow) || lifeMs <= 0;
}

Mage::OrbSpells const* Mage::OrbSpellsFor(uint32 spellId)
{
    for (OrbSpells const& orb : ORBS)
    {
        if (spellId && (orb.castSpell == spellId || orb.summonSpell == spellId))
        {
            return &orb;
        }
    }
    return NULL;
}

bool Mage::IsOrbCreature(uint32 creatureEntry)
{
    for (OrbSpells const& orb : ORBS)
    {
        if (creatureEntry && orb.creature == creatureEntry)
        {
            return true;
        }
    }
    return false;
}

Unit* Mage::ClosestOrbTarget(WorldObject const& from, std::list<Unit*> const& candidates)
{
    Unit* closest = NULL;
    float closestDistance = 0.0f;
    for (Unit* unit : candidates)
    {
        if (!unit || !unit->IsAlive() || unit->GetCreatureType() == CREATURE_TYPE_CRITTER || unit->IsPolymorphed() ||
            !from.Where().ShareFrame(unit->Where()))
        {
            continue;
        }
        float const distance = from.Where().DistanceTo(unit->Where());
        if (!closest || distance < closestDistance)
        {
            closest = unit;
            closestDistance = distance;
        }
    }
    return closest;
}

Mage::OrbCadence Mage::OrbCadenceFor(OrbSpells const& spells)
{
    OrbCadence cadence = { 0, 0 };
    SpellEntry const* aura = sSpellStore.LookupEntry(spells.orbAura);
    SpellEffectEntry const* tick = aura ? aura->GetSpellEffect(EFFECT_INDEX_0) : NULL;
    SpellEntry const* scan = sSpellStore.LookupEntry(spells.scanSpell);
    int32 const interval = scan ? scan->CalculateSimpleValue(EFFECT_INDEX_0) : 0;
    int32 const snare = GetSpellDuration(sSpellStore.LookupEntry(spells.snareSpell));
    if (!tick || !tick->EffectAuraPeriod || interval <= 0 || snare <= 0)
    {
        return cadence;
    }
    uint32 const scansPerHit = uint32(interval) / tick->EffectAuraPeriod;
    cadence.scansPerHit = scansPerHit ? scansPerHit : 1;
    cadence.window = uint32(snare) / tick->EffectAuraPeriod;
    return cadence;
}

bool Mage::OrbHitDue(uint32 scan, OrbCadence const& cadence)
{
    return cadence.scansPerHit && scan < cadence.window && scan % cadence.scansPerHit == 0;
}

uint32 Mage::OrbExplosionChance(uint32 talentSpell)
{
    for (OrbSpells const& orb : ORBS)
    {
        for (uint32 talent : orb.explosionTalents)
        {
            if (talentSpell && talent == talentSpell)
            {
                SpellEntry const* spell = sSpellStore.LookupEntry(talentSpell);
                return spell ? spell->GetProcChance() : 0;
            }
        }
    }
    return 0;
}

int32 Mage::OrbCostShare(uint32 procSpellId, Unit const& mage)
{
    for (OrbSpells const& orb : ORBS)
    {
        if (!procSpellId || orb.hitSpell != procSpellId)
        {
            continue;
        }
        SpellEntry const* cast = sSpellStore.LookupEntry(orb.castSpell);
        OrbCadence const cadence = OrbCadenceFor(orb);
        uint32 const hits = cadence.scansPerHit ? (cadence.window + cadence.scansPerHit - 1) / cadence.scansPerHit : 0;
        if (!cast || !hits)
        {
            return 0;
        }
        int32 const cost = int32(cast->GetManaCost() + cast->GetManaCostPercentage() * mage.GetCreateMana() / 100);
        return cost / int32(hits);
    }
    return 0;
}

uint32 Mage::RegisterOrb(SpellHandlerRegistry& registry)
{
    CreatureAIRegistry& ais = CreatureAIRepository::Instance();
    if (!ais.HasItem(ORB_AI))
    {
        (new CreatureAIFactory<OrbAI>(ORB_AI))->RegisterSelf();
    }

    uint32 rows = 0;
    for (OrbSpells const& orb : ORBS)
    {
        RegisterScript<DummyEffectHook>(registry, orb.castSpell, &FlameOrbEffect);
        RegisterScript<SelectTargetsHook>(registry, orb.scanSpell, &OrbScanTargets);
        RegisterScript<DummyEffectHook>(registry, orb.scanSpell, &OrbScanEffect);
        rows += 3;
    }
    return rows;
}
