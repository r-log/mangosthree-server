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

#include "spells/classes/priest/holy/Lightwell.h"
#include "spells/auras/SpellAuras.h"
#include "spells/data/SpellMgr.h"
#include "ai/CreatureAI.h"
#include "entities/Creature.h"
#include "Map.h"
#include "DBCStores.h"

namespace
{
    uint32 const LIGHTWELL_SPELL = 724;
    uint32 const LIGHTWELL_CREATURE = 31897;
    uint32 const LIGHTWELL_CLICK[] = { 60123 };
    uint32 const LIGHTWELL_CHARGES = 59907;
    uint32 const LIGHTWELL_RENEW[] = { 7001 };

    /// The name the Lightwell's creature AI is registered under.
    char const* const LIGHTWELL_AI = "LightwellAI";

    /// The Lightwell's priest: the unit of `priestGuid` on `well`'s map, when he is in the world; NULL otherwise.
    Unit* LightwellPriest(Unit const& well, ObjectGuid const& priestGuid)
    {
        Map* map = well.FindMap();
        Unit* priest = map && !priestGuid.IsEmpty() ? map->GetUnit(priestGuid) : NULL;
        return priest && priest->IsInWorld() ? priest : NULL;
    }

    /// The charges `well` holds of Lightwell Charges (59907); 0 when it holds none.
    uint32 LightwellChargesOf(Unit const& well)
    {
        SpellAuraHolder* holder = well.GetSpellAuraHolder(LIGHTWELL_CHARGES);
        return holder ? AuraHandle(*holder).Charges() : 0;
    }

    /// The Lightwell's creature. On its first update a Lightwell that 724 made takes its summoner, when he is on its map,
    /// as its owner and its creator, and casts Lightwell Charges (59907) at itself with him as the original caster, so
    /// that the aura is his and his Glyph of Lightwell adds its charges. On every update after it, it goes
    /// (Priest::LightwellStands) when it holds no charge: a Lightwell with no priest on its map holds none. Its own
    /// life, 724's duration, is the summon's timer. A Lightwell no cast of 724 made does nothing. It attacks nothing,
    /// moves nowhere and evades nothing, and it takes no part in its priest's combat.
    class LightwellAI : public CreatureAI
    {
        public:
            explicit LightwellAI(Creature* well) : CreatureAI(well), m_started(false), m_standing(false) {}

            void Reset() override {}

            /// Starts the Lightwell on its first update, when the summon has set the spell that created it; on the later
            /// ones it goes when it holds no charge.
            void UpdateAI(uint32 const /*diff*/) override
            {
                if (!m_started)
                {
                    Start();
                    return;
                }
                if (m_standing && !Priest::LightwellStands(LightwellChargesOf(*m_creature)))
                {
                    m_standing = false;
                    m_creature->ForcedDespawn();
                }
            }

            void AttackStart(Unit* /*who*/) override {}
            void MoveInLineOfSight(Unit* /*who*/) override {}
            void EnterCombat(Unit* /*enemy*/) override {}
            void EnterEvadeMode() override {}

            /// PERMIT_BASE_SPECIAL for the Lightwell's creature, PERMIT_BASE_NO for any other.
            static int Permissible(Creature const* creature)
            {
                return Priest::IsLightwellCreature(creature->GetEntry()) ? PERMIT_BASE_SPECIAL : PERMIT_BASE_NO;
            }

        private:
            /// The Lightwell's start: for a Lightwell 724 made, then, when its summoner is on its map, he as its owner
            /// and its creator, and its cast of 59907 at itself for him, triggered.
            void Start()
            {
                m_started = true;
                if (m_creature->GetUInt32Value(UNIT_CREATED_BY_SPELL) != LIGHTWELL_SPELL)
                {
                    return;
                }
                m_standing = true;
                Unit* priest = LightwellPriest(*m_creature, m_creature->GetSummonerGuid());
                if (!priest)
                {
                    return;
                }
                m_creature->SetOwnerGuid(priest->GetObjectGuid());
                m_creature->SetCreatorGuid(priest->GetObjectGuid());
                m_creature->CastSpell(m_creature, LIGHTWELL_CHARGES, true, NULL, NULL, priest->GetObjectGuid());
            }

            bool m_started;         ///< the first update has run
            bool m_standing;        ///< a Lightwell 724 made that has not gone
    };

    /// 60123: Lightwell Renew's script spell, cast by the Lightwell at the player who clicked it: the click. A Lightwell
    /// with its priest on its map answers a living player friendly to him (Priest::LightwellServes) by casting
    /// Lightwell Renew (7001) at him with the priest as the original caster, and spends one charge of 59907. The
    /// script ends the effect. A cast of 60123 by any unit but the Lightwell is left to the engine.
    SpellHandlerOutcome<void> LightwellClick(DummyEffectContext& ctx)
    {
        Unit& well = ctx.caster;
        if (!Priest::IsLightwellCreature(well.GetEntry()))
        {
            return SpellHandlerOutcome<void>::Continue();
        }
        Unit* priest = LightwellPriest(well, well.GetOwnerGuid());
        Unit* clicker = ctx.target;
        bool const friendly = priest && clicker && clicker->IsAlive() && priest->IsFriendlyTo(clicker);
        if (!Priest::LightwellServes(priest != NULL, friendly, LightwellChargesOf(well)))
        {
            return SpellHandlerOutcome<void>::Return();
        }
        well.CastSpell(clicker, LIGHTWELL_RENEW[0], true, NULL, NULL, priest->GetObjectGuid(), &ctx.spell);
        if (SpellAuraHolder* charges = well.GetSpellAuraHolder(LIGHTWELL_CHARGES))
        {
            AuraHandle(*charges).DropCharge();
        }
        return SpellHandlerOutcome<void>::Return();
    }

    /// 7001: Lightwell Renew: its proc charge removes it on a hit its holder takes; the script lets a hit of at least
    /// 724's effect 1 percent of his maximum health through (Priest::LightwellHitCancels) and drops the holder from the
    /// proc for a smaller one, so that the charge stays.
    SpellHandlerOutcome<bool> LightwellRenewCheckProc(CheckProcContext& ctx)
    {
        SpellEntry const* lightwell = sSpellStore.LookupEntry(LIGHTWELL_SPELL);
        int32 const percent = lightwell ? lightwell->CalculateSimpleValue(EFFECT_INDEX_1) : 0;
        return SpellHandlerOutcome<bool>::Return(
            Priest::LightwellHitCancels(ctx.damage, ctx.owner.GetMaxHealth(), percent));
    }
}

bool Priest::IsLightwellCreature(uint32 creatureEntry)
{
    return creatureEntry && creatureEntry == LIGHTWELL_CREATURE;
}

bool Priest::LightwellStands(uint32 charges)
{
    return charges > 0;
}

bool Priest::LightwellServes(bool priest, bool friendly, uint32 charges)
{
    return priest && friendly && charges > 0;
}

bool Priest::LightwellHitCancels(uint32 damage, uint32 maxHealth, int32 percent)
{
    return percent > 0 && maxHealth > 0 && uint64(damage) * 100 >= uint64(maxHealth) * uint64(percent);
}

uint32 Priest::RegisterLightwell(SpellHandlerRegistry& registry)
{
    CreatureAIRegistry& ais = CreatureAIRepository::Instance();
    if (!ais.HasItem(LIGHTWELL_AI))
    {
        (new CreatureAIFactory<LightwellAI>(LIGHTWELL_AI))->RegisterSelf();
    }

    uint32 rows = RegisterRanks<DummyEffectHook>(registry, LIGHTWELL_CLICK, &LightwellClick);
    rows += RegisterRanks<CheckProcHook>(registry, LIGHTWELL_RENEW, &LightwellRenewCheckProc);
    return rows;
}
