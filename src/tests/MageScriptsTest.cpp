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

/// Tests of the mage's spell scripts: the rows RegisterMageScripts registers, each file's share, the bodies that labels
/// share, the dummy-effect labels that end the effect with no unit target, the proc hook's ids per proc script, the
/// refusals of the proc scripts, Offensive State's refusal of Arcane Missiles, the pairing of Arcane Missiles Aurastate
/// with Arcane Missiles! and the channel's use of the buff, Arcane Blast's buff spent only at the cast of another
/// Arcane damage spell, Arcane Potency's buffs spent only by a spell that deals or triggers damage, Ignite's munching
/// leaves against the roll-over, Clearcasting used up only by a spell its cost modifier makes cheaper, the slows
/// Torment the Weak counts and its multiplier, Nether Vortex's Slow and its refusals, Focus Magic's buff cast by its
/// mage at himself and its proc-trigger effect that casts nothing, Invocation's buff from a Counterspell that
/// interrupted a cast alone, Improved Polymorph's stun and marker when damage breaks a Polymorph,
/// Incanter's Absorption's knockback when damage breaks a Mana Shield, Improved Mana Gem's amount after the Mana Gem's
/// spell, the count of crits in a row, Hot Streak's chance and streak, the Tier 10 bonus at the removal of Hot Streak's
/// buff, Molten Shields' Blazing Speed at the removal of a Mage Ward and the script's refusals, the frozen query,
/// Shatter's crit factor and the damage-done multipliers of Frostburn, Shatter and Ice Lance, the two spell lists those
/// scripts hold against the class masks of the talents, Blink's target count, Living Bomb's exits at the dummy aura
/// hook, Pyromaniac's leaves (a Frostfire Bolt burning only with its periodic damage among them) and its rows at the
/// aura-apply and aura-remove hooks, Combustion's exits at the dummy-effect hook, its tick and its DoT's stacking with
/// the auras of its icon, Impact's cleared Fire Blast cooldown, which DoTs Impact spreads, the exits of its spread at
/// the dummy-effect hook, Improved Flamestrike's chance, Blast Wave's refusals at the after-cast hook, Glyph of
/// Frostfire's hit without Molten Fury and its tick, Frostfire Bolt's effect taken off at its holder's apply and its
/// after-hit exits, the Early Frost lockout a Frostbolt cast starts, Piercing Chill's count, its pick of the nearest
/// others and its chill's target list, the exits of Frostbolt's and Cone of Cold's after-hit scripts, Permafrost's heal
/// and its refusals, and Improved Cone of Cold's freeze by rank; the orb's rows are counted here and tested in
/// MageOrbTest, the ring's in MageRingTest. A body that casts, reads a level or creates an item needs a live unit and a
/// spell; the proc scripts' refusals run on bare Creatures (update fields allocated; no map, template or AI), with the
/// effect rows the scripts read added to the effect store as the client's data holds them, and a holder put in the
/// creature's holder map where a script asks whether it holds an aura. The cooldown Impact clears is a player's: a
/// player on a session with no socket, in no world, whose cooldown sinks the test installs. Frostfire Bolt's apply runs
/// on a holder made through its constructor on a bare creature, of three auras whose apply and removal change nothing
/// on the unit.

#include "TestHarness.h"
#include "combat/SpellBonus.h"
#include "spells/classes/mage/Mage.h"
#include "spells/handlers/SpellHandlerRegistry.h"
#include "spells/handlers/AurasHandlers.h"
#include "spells/handlers/SpellEffectDummyHandlers.h"
#include "spells/handlers/UnitAuraProcHandlerHandlers.h"
#include "spells/auras/SpellAuras.h"
#include "spells/pipeline/Spell.h"
#include "spells/data/SpellMgr.h"
#include "spells/SpellScriptQueries.h"
#include "spells/PeriodicTiming.h"
#include "Creature.h"
#include "Player.h"
#include "WorldSession.h"
#include "Auth/BigNumber.h"
#include "DBCStores.h"
#include "DBCStructure.h"
#include "data/ServerSpellStore.h"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <list>
#include <memory>
#include <set>
#include <utility>
#include <vector>

namespace
{
    // The labels the class files register at each mage site, in their tables' order.
    uint32 const TRIGGER_SPELL_IDS[] = { 66 };
    uint32 const EFFECT_DUMMY_IDS[] = { 32826, 38194, 42955, 1459, 61316 };
    uint32 const DUMMY_PROC_IDS[] = { 56372, 56375, 64411 };

    // The ids each proc script holds at the proc hook: every mage spell with the talent's icon (or class mask) and an
    // aura of the type the script stands for, in the client's data.
    uint32 const IGNITE_IDS[] = { 11119, 11120, 12846 };
    uint32 const MASTER_OF_ELEMENTS_IDS[] = { 29074, 29075 };
    uint32 const ARCANE_POTENCY_IDS[] = { 31571, 31572 };
    uint32 const HOT_STREAK_IDS[] = { 44445 };
    uint32 const BLAZING_SPEED_IDS[] = { 31641, 31642 };
    uint32 const MANA_SHIELD_IDS[] = { 1463 };
    uint32 const PERSISTENT_SHIELD_IDS[] = { 26467 };
    uint32 const OFFENSIVE_STATE_IDS[] = { 79684 };
    uint32 const DEEP_FREEZE_IMMUNITY_IDS[] = { 71761 };
    uint32 const FINGERS_OF_FROST_IDS[] = { 44543, 44545, 83074 };

    // Brain Freeze's ranks, which the check-proc hook holds with Fingers of Frost's body.
    uint32 const BRAIN_FREEZE_IDS[] = { 44546, 44548, 44549 };

    // Permafrost's ranks, whose dummy effect's proc heals the caster's pet.
    uint32 const PERMAFROST_IDS[] = { 11175, 12569, 12571 };

    // The ids the Frost launch script holds at the launch hook: Ice Lance, Deep Freeze and Frostfire Bolt, which use
    // Fingers of Frost's charges, and Fireball, which uses Brain Freeze's buff with Frostfire Bolt.
    uint32 const FROST_LAUNCH_IDS[] = { 30455, 44572, 44614, 133 };

    // The id the Frost cast-start script holds: Frostbolt, whose cast starts Early Frost's lockout.
    uint32 const FROSTBOLT_CAST_START_IDS[] = { 116 };

    // The id the target script holds: the one id the arm named (Blink).
    uint32 const BLINK_IDS[] = { 38194 };

    // The id the dummy aura script holds: the one mage spell with Living Bomb's class mask and a dummy aura.
    uint32 const LIVING_BOMB_IDS[] = { 44457 };

    // The id the aura-remove script holds: Hot Streak's buff, which gives the Tier 10 two-piece bonus's haste when it is
    // used up.
    uint32 const HOT_STREAK_BUFF_IDS[] = { 48108 };

    // The id Molten Shields' aura-remove script holds: Mage Ward, whose break by damage brings Blazing Speed.
    uint32 const MAGE_WARD_IDS[] = { 543 };

    // The ids Pyromaniac's scripts hold at the aura-apply and aura-remove hooks: the eight DoTs it counts (Flamestrike's
    // ground effect, Pyroblast, Ignite, Living Bomb, Frostfire Bolt, Combustion, the free Flamestrike's ground effect,
    // Pyroblast!) and the talent's two ranks.
    uint32 const PYROMANIAC_DOT_IDS[] = { 2120, 11366, 12654, 44457, 44614, 83853, 88148, 92315 };
    uint32 const PYROMANIAC_IDS[] = { 34293, 34295 };

    // The id the dummy-effect script holds: Combustion, whose script effect puts its DoT (83853) on the target.
    uint32 const COMBUSTION_IDS[] = { 11129 };

    // Impact's ids at the proc hook: the talent's two ranks.
    uint32 const IMPACT_IDS[] = { 11103, 12357 };

    // The id Clearcasting's script holds at the check-proc hook: the buff, whose charge only a spell it makes cheaper
    // takes.
    uint32 const CLEARCASTING_IDS[] = { 12536 };

    // The id Impact's spread holds at the dummy-effect hook: the spell the Impact buff casts at its Fire Blast's victim.
    uint32 const IMPACT_SPREAD_IDS[] = { 12355 };

    // The orb's ids: Flame Orb and the orb's scan at the dummy-effect hook, the orb's scan at the target hook
    // (MageOrbTest tests their bodies).
    uint32 const ORB_DUMMY_EFFECT_IDS[] = { 82731, 82734 };
    uint32 const ORB_TARGET_IDS[] = { 82734 };

    // The ring's freeze, whose targets and removal the ring's scripts hold.
    uint32 const RING_FREEZE_IDS[] = { 82691 };

    // The id the after-cast script holds: Blast Wave, which calls the free Flamestrike for Improved Flamestrike.
    uint32 const BLAST_WAVE_IDS[] = { 11113 };

    // The id Arcane Missiles' pairing holds at the aura-apply and aura-remove hooks: Arcane Missiles!, which Arcane
    // Missiles Aurastate (79808) follows; and the id the channel's script holds at the aura-apply hook: Arcane
    // Missiles, whose cast uses Arcane Missiles! up.
    uint32 const ARCANE_MISSILES_BUFF_IDS[] = { 79683 };
    uint32 const ARCANE_MISSILES_IDS[] = { 5143 };

    // The id the check-proc script of Arcane Blast's buff holds: the buff, which only the end of the mage's own cast of
    // an Arcane damage spell other than Arcane Blast and Arcane Explosion spends.
    uint32 const ARCANE_BLAST_BUFF_IDS[] = { 36032 };

    // The ids the check-proc script of Arcane Potency's buffs holds: the buff of each rank, whose charges a spell that
    // neither deals nor triggers damage never spends.
    uint32 const ARCANE_POTENCY_BUFF_IDS[] = { 57529, 57531 };

    // The ids Torment the Weak's script holds at the damage-done hook: its three ranks, which the engine runs it for
    // as dummy auras of the caster.
    uint32 const TORMENT_THE_WEAK_IDS[] = { 29447, 55339, 55340 };

    // The ids Nether Vortex's script holds at the proc hook: its two ranks.
    uint32 const NETHER_VORTEX_IDS[] = { 86181, 86209 };

    // The ids Focus Magic's script holds at the proc hook: the spell, whose proc-trigger effect it ends.
    uint32 const FOCUS_MAGIC_IDS[] = { 54646 };

    // The ids Invocation's script holds at the check-proc hook: its two ranks, whose buff only a Counterspell that
    // interrupted a cast gives.
    uint32 const INVOCATION_IDS[] = { 84722, 84723 };

    // The ids Improved Polymorph's script holds at the aura-remove hook: every form of Polymorph a mage casts, the
    // spells of the Arcane skill line with Polymorph's class mask and a confuse and a transform aura.
    uint32 const POLYMORPH_IDS[] = { 118, 28271, 28272, 61025, 61305, 61721, 61780 };

    // The id Incanter's Absorption's script holds at the aura-remove hook: Mana Shield, whose break by damage knocks
    // the enemies around the mage back.
    uint32 const MANA_SHIELD_BREAK_IDS[] = { 1463 };

    // The id Improved Mana Gem's script holds at the after-cast hook: Replenish Mana, the Mana Gem's spell.
    uint32 const REPLENISH_MANA_IDS[] = { 5405 };

    // Every id of `ids` is a key of `Site` on `registry`, and the game's table holds the same body for it.
    template <class Site, std::size_t N>
    void CheckRowsInTheGame(SpellHandlerRegistry const& registry, uint32 const (&ids)[N])
    {
        SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
        for (uint32 spellId : ids)
        {
            CHECK(registry.Find<Site>(spellId) != NULL);
            CHECK(game.Find<Site>(spellId) == registry.Find<Site>(spellId));
        }
    }

    // A SpellEffect.dbc row in raw storage, as the loader makes it; zeroed, with the effect index set.
    struct EffectRow
    {
        alignas(SpellEffectEntry) unsigned char bytes[sizeof(SpellEffectEntry)];
    };

    EffectRow s_effectRow = {};

    SpellEffectEntry const* DummyEffect(uint32 effectIndex)
    {
        SpellEffectEntry* row = reinterpret_cast<SpellEffectEntry*>(s_effectRow.bytes);
        row->EffectIndex = effectIndex;
        return row;
    }

    // Every id of `ids` holds one body at the proc hook, on `registry` and in the game's table alike; answers it.
    template <std::size_t N>
    SpellHandler<OnProcHook>::Function OneProcBody(SpellHandlerRegistry const& registry, uint32 const (&ids)[N])
    {
        SpellHandler<OnProcHook>::Function body = registry.Find<OnProcHook>(ids[0]);
        CHECK(body != NULL);
        for (uint32 spellId : ids)
        {
            CHECK(registry.Find<OnProcHook>(spellId) == body);
            CHECK(SpellHandlerRegistry::Game().Find<OnProcHook>(spellId) == body);
        }
        return body;
    }

    // A creature with its update fields allocated and no world, map, template or AI.
    class BareCreature : public Creature
    {
        public:
            BareCreature()
            {
                _InitValues();
            }

            // Unit::CleanupsBeforeDelete tears down an in-world unit only while the update fields exist. The holders
            // HoldAura put in the map are the test's own storage and leave the map first.
            ~BareCreature()
            {
                m_auras.Holders().clear();
                for (AuraType type : m_markedTypes)
                {
                    m_auras.ByType(type).clear();
                }
                delete[] m_uint32Values;
                m_uint32Values = NULL;
            }

            // Puts `holder` in the holder map under `spellId`, so Unit::HasAura(spellId) finds it.
            void HoldAura(uint32 spellId, SpellAuraHolder& holder)
            {
                m_auras.AddHolder(spellId, &holder);
            }

            // Puts an entry with no aura in the list of `type`, so Unit::HasAuraType(type) answers true; the list is
            // emptied before the creature goes.
            void MarkAuraType(AuraType type)
            {
                m_auras.ByType(type).push_back(NULL);
                m_markedTypes.push_back(type);
            }

            // Puts `aura` in the list of `type`, so the unit's queries of that aura type read its amount; the list is
            // emptied before the creature goes.
            void ListAura(AuraType type, Aura* aura)
            {
                m_auras.ByType(type).push_back(aura);
                m_markedTypes.push_back(type);
            }

        private:
            std::vector<AuraType> m_markedTypes;
    };

    // A player with its update fields allocated, on a session with no socket, in no world.
    class SessionPlayer : public Player
    {
        public:
            explicit SessionPlayer(WorldSession& session) : Player(&session)
            {
                Object::_Create(1, 0, HIGHGUID_PLAYER);
            }

            ~SessionPlayer()
            {
                delete[] m_uint32Values;
                m_uint32Values = NULL;
            }
    };

    struct SpellRowStorage
    {
        alignas(SpellEntry) unsigned char bytes[sizeof(SpellEntry)];
    };

    struct HolderStorage
    {
        alignas(SpellAuraHolder) unsigned char bytes[sizeof(SpellAuraHolder)];
    };

    // A Spell.dbc row with only its id, which the proc context names; it is not in the spell store.
    SpellEntry const& SpellRow(SpellRowStorage& storage, uint32 spellId)
    {
        for (unsigned char& byte : storage.bytes)
        {
            byte = 0;
        }
        SpellEntry* row = reinterpret_cast<SpellEntry*>(storage.bytes);
        row->ID = spellId;
        return *row;
    }

    // A holder's storage without its constructor, every byte 0: the aura handle reads its script words there.
    SpellAuraHolder& EmptyHolder(HolderStorage& storage)
    {
        for (unsigned char& byte : storage.bytes)
        {
            byte = 0;
        }
        return *reinterpret_cast<SpellAuraHolder*>(storage.bytes);
    }

    // The effect rows of the client's data the proc scripts read, (spell, index, aura type) each, put into the effect
    // store once for the rest of the run; no other test uses these ids.
    void AddProcEffectRows()
    {
        struct Row
        {
            uint32 spellId;
            uint32 effectIndex;
            uint32 aura;
        };
        static Row const rows[] =
        {
            { 29074, 0, SPELL_AURA_DUMMY },
            { 12846, 0, SPELL_AURA_DUMMY },
            { 31571, 0, SPELL_AURA_DUMMY },
            { 44445, 0, SPELL_AURA_DUMMY }, { 44445, 1, SPELL_AURA_DUMMY },
            { 79684, 0, SPELL_AURA_PROC_TRIGGER_SPELL },
            { 31641, 0, SPELL_AURA_PROC_TRIGGER_SPELL },
            { 1463, 0, SPELL_AURA_MANA_SHIELD },
            { 26467, 0, SPELL_AURA_PROC_TRIGGER_SPELL },
            { 71761, 0, SPELL_AURA_PROC_TRIGGER_SPELL },
            { 83074, 0, SPELL_AURA_PROC_TRIGGER_SPELL },
            { 86181, 0, SPELL_AURA_DUMMY },
            { 86209, 0, SPELL_AURA_DUMMY },
            { 12571, 0, SPELL_AURA_DUMMY }, { 12571, 1, SPELL_AURA_ADD_FLAT_MODIFIER },
            { 54646, 0, SPELL_AURA_MOD_SPELL_CRIT_CHANCE }, { 54646, 1, SPELL_AURA_PROC_TRIGGER_SPELL },
        };
        static SpellEffectEntry effects[sizeof(rows) / sizeof(rows[0])];
        static bool added = false;
        if (added)
        {
            return;
        }
        added = true;
        for (std::size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); ++i)
        {
            effects[i] = SpellEffectEntry();
            effects[i].SpellID = rows[i].spellId;
            effects[i].EffectIndex = rows[i].effectIndex;
            effects[i].Effect = SPELL_EFFECT_APPLY_AURA;
            effects[i].EffectAura = rows[i].aura;
            AddSpellEffectEntry(&effects[i]);
        }
    }

    // Ignite's damage over time (12654) as the client's data holds it: effect 0 a periodic damage aura at the target
    // every 2000 ms, and duration index 35, 4000 ms; so the DoT ticks twice. Put into the spell, effect and duration
    // stores once for the rest of the run; no other test reads them.
    void AddIgniteDot()
    {
        static ServerSpell dot;
        static SpellDurationEntry duration;
        static bool added = false;
        if (added)
        {
            return;
        }
        added = true;
        duration.ID = 35;
        duration.Duration[0] = 4000;
        duration.Duration[1] = 0;
        duration.Duration[2] = 4000;
        sSpellDurationStore.SetEntry(duration.ID, &duration);
        dot.Entry().ID = 12654;
        dot.Entry().DurationIndex = duration.ID;
        dot.effect.SpellID = 12654;
        dot.effect.EffectIndex = EFFECT_INDEX_0;
        dot.effect.Effect = SPELL_EFFECT_APPLY_AURA;
        dot.effect.EffectAura = SPELL_AURA_PERIODIC_DAMAGE;
        dot.effect.EffectAuraPeriod = 2000;
        dot.effect.ImplicitTarget_0 = TARGET_CHAIN_DAMAGE;
        sSpellStore.SetEntry(dot.Entry().ID, &dot.Entry());
        AddSpellEffectEntry(&dot.effect);
    }

    // Pyromaniac's two ranks (34293, 34295) as the client's data holds them: effect 0 a dummy aura on the caster of 5
    // and 10, the haste each rank gives. Put into the spell and effect stores once for the rest of the run; no other
    // test reads them.
    void AddPyromaniacRanks()
    {
        static ServerSpell ranks[2];
        static bool added = false;
        if (added)
        {
            return;
        }
        added = true;
        uint32 const ids[2] = { 34293, 34295 };
        int32 const haste[2] = { 5, 10 };
        for (std::size_t i = 0; i < 2; ++i)
        {
            ranks[i].Entry().ID = ids[i];
            ranks[i].effect.SpellID = ids[i];
            ranks[i].effect.EffectIndex = EFFECT_INDEX_0;
            ranks[i].effect.Effect = SPELL_EFFECT_APPLY_AURA;
            ranks[i].effect.EffectAura = SPELL_AURA_DUMMY;
            ranks[i].effect.EffectBasePoints = haste[i];
            ranks[i].effect.ImplicitTarget_0 = TARGET_SELF;
            sSpellStore.SetEntry(ids[i], &ranks[i].Entry());
            AddSpellEffectEntry(&ranks[i].effect);
        }
    }

    // Improved Flamestrike's two ranks (84673, 84674) as the client's data holds them: effect 0 a dummy aura on the
    // caster of 50 and 100, the chance each rank gives Blast Wave to call the free Flamestrike. Put into the spell and
    // effect stores once for the rest of the run; no other test reads them. The free Flamestrike (88148) is not put in.
    void AddImprovedFlamestrikeRanks()
    {
        static ServerSpell ranks[2];
        static bool added = false;
        if (added)
        {
            return;
        }
        added = true;
        uint32 const ids[2] = { 84673, 84674 };
        int32 const chance[2] = { 50, 100 };
        for (std::size_t i = 0; i < 2; ++i)
        {
            ranks[i].Entry().ID = ids[i];
            ranks[i].effect.SpellID = ids[i];
            ranks[i].effect.EffectIndex = EFFECT_INDEX_0;
            ranks[i].effect.Effect = SPELL_EFFECT_APPLY_AURA;
            ranks[i].effect.EffectAura = SPELL_AURA_DUMMY;
            ranks[i].effect.EffectBasePoints = chance[i];
            ranks[i].effect.ImplicitTarget_0 = TARGET_SELF;
            sSpellStore.SetEntry(ids[i], &ranks[i].Entry());
            AddSpellEffectEntry(&ranks[i].effect);
        }
    }

    // Impact's proc-trigger effect as the client's data holds it, effect 0 of both ranks with the Impact buff (64343) as
    // its trigger spell, put into the effect store once for the rest of the run; no other test uses these ids. The
    // buff itself is not in the test's spell store.
    void AddImpactEffectRows()
    {
        static SpellEffectEntry effects[sizeof(IMPACT_IDS) / sizeof(IMPACT_IDS[0])];
        static bool added = false;
        if (added)
        {
            return;
        }
        added = true;
        for (std::size_t i = 0; i < sizeof(IMPACT_IDS) / sizeof(IMPACT_IDS[0]); ++i)
        {
            effects[i] = SpellEffectEntry();
            effects[i].SpellID = IMPACT_IDS[i];
            effects[i].EffectIndex = EFFECT_INDEX_0;
            effects[i].Effect = SPELL_EFFECT_APPLY_AURA;
            effects[i].EffectAura = SPELL_AURA_PROC_TRIGGER_SPELL;
            effects[i].EffectTriggerSpell = 64343;
            effects[i].ImplicitTarget_0 = TARGET_SELF;
            AddSpellEffectEntry(&effects[i]);
        }
    }

    // Effect rows shaped as the client's data holds the spells a proc's damage check reads, under test-only ids, put
    // into the effect store once for the rest of the run: a direct damage spell (Fireball, Fire Blast), a periodic
    // damage aura beside a dummy (Living Bomb), a periodic percent damage and a periodic leech aura, a weapon damage
    // effect, and the spells that deal none (Slow's three auras, Polymorph's confuse, Counterspell's interrupt, Deep
    // Freeze's stun) and one that only triggers a spell.
    uint32 const ROW_DIRECT_DAMAGE = 9420001;
    uint32 const ROW_PERIODIC_DAMAGE_WITH_DUMMY = 9420002;
    uint32 const ROW_PERIODIC_PERCENT = 9420003;
    uint32 const ROW_PERIODIC_LEECH = 9420004;
    uint32 const ROW_WEAPON_DAMAGE = 9420005;
    uint32 const ROW_SLOW_AURAS = 9420006;
    uint32 const ROW_CONFUSE = 9420007;
    uint32 const ROW_INTERRUPT = 9420008;
    uint32 const ROW_STUN = 9420009;
    uint32 const ROW_TRIGGERS_DAMAGE = 9420010;

    void AddDamageCheckRows()
    {
        struct Row
        {
            uint32 spellId;
            uint32 effectIndex;
            uint32 effect;
            uint32 aura;
        };
        static Row const rows[] =
        {
            { ROW_DIRECT_DAMAGE, 0, SPELL_EFFECT_SCHOOL_DAMAGE, 0 },
            { ROW_PERIODIC_DAMAGE_WITH_DUMMY, 0, SPELL_EFFECT_APPLY_AURA, SPELL_AURA_PERIODIC_DAMAGE },
            { ROW_PERIODIC_DAMAGE_WITH_DUMMY, 1, SPELL_EFFECT_DUMMY, 0 },
            { ROW_PERIODIC_PERCENT, 0, SPELL_EFFECT_APPLY_AURA, SPELL_AURA_PERIODIC_DAMAGE_PERCENT },
            { ROW_PERIODIC_LEECH, 0, SPELL_EFFECT_APPLY_AURA, SPELL_AURA_PERIODIC_LEECH },
            { ROW_WEAPON_DAMAGE, 0, SPELL_EFFECT_NORMALIZED_WEAPON_DMG, 0 },
            { ROW_SLOW_AURAS, 0, SPELL_EFFECT_APPLY_AURA, SPELL_AURA_MOD_DECREASE_SPEED },
            { ROW_SLOW_AURAS, 1, SPELL_EFFECT_APPLY_AURA, SPELL_AURA_MOD_RANGED_HASTE },
            { ROW_SLOW_AURAS, 2, SPELL_EFFECT_APPLY_AURA, SPELL_AURA_MOD_CASTING_SPEED_NOT_STACK },
            { ROW_CONFUSE, 0, SPELL_EFFECT_APPLY_AURA, SPELL_AURA_MOD_CONFUSE },
            { ROW_INTERRUPT, 0, SPELL_EFFECT_INTERRUPT_CAST, 0 },
            { ROW_STUN, 0, SPELL_EFFECT_APPLY_AURA, SPELL_AURA_MOD_STUN },
            { ROW_TRIGGERS_DAMAGE, 0, SPELL_EFFECT_TRIGGER_SPELL, 0 },
        };
        static SpellEffectEntry effects[sizeof(rows) / sizeof(rows[0])];
        static bool added = false;
        if (added)
        {
            return;
        }
        added = true;
        for (std::size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); ++i)
        {
            effects[i] = SpellEffectEntry();
            effects[i].SpellID = rows[i].spellId;
            effects[i].EffectIndex = rows[i].effectIndex;
            effects[i].Effect = rows[i].effect;
            effects[i].EffectAura = rows[i].aura;
            AddSpellEffectEntry(&effects[i]);
        }
    }

    // Spells shaped as the client's data holds the ones a check one level down reads, under test-only ids: a channel
    // whose effect 0 is a dummy aura and whose effect 1 a periodic trigger of a missile that deals direct damage
    // (Arcane Missiles, 5143, and its missile, 7268), a spell that triggers one that only stuns, and one that triggers
    // an id the spell store lacks. The two triggered spells are put into the spell and effect stores, the three that
    // trigger them into the effect store, once for the rest of the run; no other test uses these ids.
    uint32 const ROW_MISSILE_CHANNEL = 9420011;
    uint32 const ROW_MISSILE = 9420012;
    uint32 const ROW_TRIGGERS_STUN = 9420013;
    uint32 const ROW_STUN_ONLY = 9420014;
    uint32 const ROW_TRIGGERS_ABSENT = 9420015;
    uint32 const ROW_ABSENT = 9420016;

    void AddTriggerCheckRows()
    {
        static ServerSpell triggered[2];
        static SpellEffectEntry effects[4];
        static bool added = false;
        if (added)
        {
            return;
        }
        added = true;
        uint32 const triggeredIds[2] = { ROW_MISSILE, ROW_STUN_ONLY };
        for (std::size_t i = 0; i < 2; ++i)
        {
            triggered[i].Entry().ID = triggeredIds[i];
            triggered[i].effect.SpellID = triggeredIds[i];
            triggered[i].effect.EffectIndex = EFFECT_INDEX_0;
            triggered[i].effect.ImplicitTarget_0 = TARGET_CHAIN_DAMAGE;
        }
        triggered[0].effect.Effect = SPELL_EFFECT_SCHOOL_DAMAGE;
        triggered[1].effect.Effect = SPELL_EFFECT_APPLY_AURA;
        triggered[1].effect.EffectAura = SPELL_AURA_MOD_STUN;
        for (ServerSpell& spell : triggered)
        {
            sSpellStore.SetEntry(spell.Entry().ID, &spell.Entry());
            AddSpellEffectEntry(&spell.effect);
        }

        struct Row
        {
            uint32 spellId;
            uint32 effectIndex;
            uint32 effect;
            uint32 aura;
            uint32 triggerSpell;
        };
        static Row const rows[] =
        {
            { ROW_MISSILE_CHANNEL, 0, SPELL_EFFECT_APPLY_AURA, SPELL_AURA_DUMMY, 0 },
            { ROW_MISSILE_CHANNEL, 1, SPELL_EFFECT_APPLY_AURA, SPELL_AURA_PERIODIC_TRIGGER_SPELL, ROW_MISSILE },
            { ROW_TRIGGERS_STUN, 0, SPELL_EFFECT_TRIGGER_SPELL, 0, ROW_STUN_ONLY },
            { ROW_TRIGGERS_ABSENT, 0, SPELL_EFFECT_TRIGGER_SPELL, 0, ROW_ABSENT },
        };
        for (std::size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); ++i)
        {
            effects[i] = SpellEffectEntry();
            effects[i].SpellID = rows[i].spellId;
            effects[i].EffectIndex = rows[i].effectIndex;
            effects[i].Effect = rows[i].effect;
            effects[i].EffectAura = rows[i].aura;
            effects[i].EffectTriggerSpell = rows[i].triggerSpell;
            AddSpellEffectEntry(&effects[i]);
        }
    }

    // A proc of `spellId`'s effect `effIndex` on `owner`, with the event's spell, flags and the aura's amount given;
    // the event's other unit and its damage too, when given.
    SpellHandlerOutcome<SpellAuraProcResult> Proc(SpellHandlerRegistry const& registry, Unit& owner, uint32 spellId,
                                                  SpellEffectIndex effIndex, SpellAuraHolder& holder, SpellEntry const* procSpell,
                                                  uint32 procEx, int32 amount, Unit* victim = NULL, uint32 damage = 900)
    {
        SpellRowStorage row;
        SpellCooldownMgr cooldowns;
        OnProcContext ctx(owner, victim, SpellRow(row, spellId), effIndex, AuraHandle(holder), procSpell, 0x10000, procEx,
                          damage, amount, ObjectGuid(), 0, NULL, cooldowns);
        return registry.Dispatch<OnProcHook>(spellId, ctx);
    }
}

TEST(MageScripts_TheTableRegistersEveryRowOnce)
{
    // The class-wide file: Invisibility at the trigger-spell site; Polymorph Cast Visual, Blink, Conjure Refreshment
    // and the two raid buffs at the dummy-effect site; the Glyphs of Ice Block and Polymorph and Blessing of Ancient
    // Kings at the dummy proc site; the defaults of the trigger-spell, periodic dummy tick and boost sites; Blink at
    // the target hook, Persistent Shield and Offensive State at the proc hook. Fire: Living Bomb at the dummy aura
    // hook, Hot Streak's buff and Mage Ward at the aura-remove hook, Pyromaniac's eight DoTs and two ranks at the
    // aura-apply and aura-remove hooks, Combustion and Impact's spread at the dummy-effect hook, Blast Wave at the
    // after-cast hook, and Frostfire Bolt at the after-hit hook. Frost: the 22 spells Frostburn's mask covers at the
    // damage-done hook, the 26 Shatter's covers at the crit-chance hook, Ice Lance, Deep Freeze, Frostfire Bolt and
    // Fireball at the launch hook, Frostbolt at the cast-start hook, Frostbolt and Cone of Cold at the after-hit hook,
    // and Piercing Chill's chill at the target hook. At the proc hook, Fire: Ignite, Master of Elements, Hot Streak,
    // Blazing Speed and Impact; Frost: Deep Freeze Immunity State, Fingers of Frost and Permafrost's three ranks;
    // Arcane: Arcane Potency, Mana Shield, Nether Vortex and Focus Magic. Arcane also: Arcane Missiles! at the
    // aura-apply and aura-remove hooks, Arcane Missiles at the aura-apply hook, Torment the Weak's three ranks at the
    // damage-done hook, the seven forms of Polymorph and Mana Shield at the aura-remove hook and Replenish Mana at the
    // after-cast hook. The orb: Flame Orb at the dummy-effect hook, and the orb's scan at the target and dummy-effect
    // hooks. The ring: Ring of Frost at the periodic-tick hook, and the ring's freeze at the target and aura-remove
    // hooks. Impact, Clearcasting, Arcane Blast's buff, Arcane Potency's two buffs, Fingers of Frost, Brain Freeze and
    // Invocation's two ranks also at the check-proc hook. 156 rows, 153 keys and 3 defaults, none twice.
    SpellHandlerRegistry registry;
    CHECK_EQ(RegisterMageScripts(registry), uint32(156));
    CHECK_EQ(registry.Count(), std::size_t(153));
    CHECK_EQ(registry.CountDefaults(), std::size_t(3));
    CHECK_EQ(registry.CountAt(AuraTriggerSpellMageSite::Key), std::size_t(1));
    CHECK_EQ(registry.CountAt(AuraPeriodicDummyTickMageSite::Key), std::size_t(0));
    CHECK_EQ(registry.CountAt(AuraBoostMageSite::Key), std::size_t(0));
    CHECK_EQ(registry.CountAt(SpellEffectDummyMageSite::Key), std::size_t(5));
    CHECK_EQ(registry.CountAt(DummyAuraProcMageSite::Key), std::size_t(3));
    CHECK_EQ(registry.CountAt(SpellCritChanceAuraProcMageSite::Key), std::size_t(0));
    CHECK_EQ(registry.CountAt(OnProcHook::Key), std::size_t(25));
    CHECK_EQ(registry.CountAt(CheckProcHook::Key), std::size_t(14));
    CHECK_EQ(registry.CountAt(SelectTargetsHook::Key), std::size_t(4));
    CHECK_EQ(registry.CountAt(DamageDoneHook::Key), std::size_t(25));
    CHECK_EQ(registry.CountAt(CritChanceHook::Key), std::size_t(26));
    CHECK_EQ(registry.CountAt(DummyAuraHook::Key), std::size_t(1));
    CHECK_EQ(registry.CountAt(AuraApplyHook::Key), std::size_t(12));
    CHECK_EQ(registry.CountAt(AuraRemoveHook::Key), std::size_t(22));
    CHECK_EQ(registry.CountAt(PeriodicTickHook::Key), std::size_t(1));
    CHECK_EQ(registry.CountAt(DummyEffectHook::Key), std::size_t(4));
    CHECK_EQ(registry.CountAt(AfterCastHook::Key), std::size_t(2));
    CHECK_EQ(registry.CountAt(LaunchHook::Key), std::size_t(4));
    CHECK_EQ(registry.CountAt(CastStartHook::Key), std::size_t(1));
    CHECK_EQ(registry.CountAt(AfterHitHook::Key), std::size_t(3));
    CHECK(registry.FindDefault<AuraTriggerSpellMageSite>() != NULL);
    CHECK(registry.FindDefault<AuraPeriodicDummyTickMageSite>() != NULL);
    CHECK(registry.FindDefault<AuraBoostMageSite>() != NULL);
    CHECK(registry.FindDefault<SpellEffectDummyMageSite>() == NULL);
    CHECK(registry.FindDefault<DummyAuraProcMageSite>() == NULL);

    // Each file on a table of its own: what it answers, its keys and its defaults.
    struct PerFile
    {
        uint32 (*registerRows)(SpellHandlerRegistry&);
        uint32 answered;
        std::size_t keys;
        std::size_t defaults;
    };
    PerFile const files[] =
    {
        { &Mage::RegisterClassWide, 15, 12, 3 },
        { &Mage::RegisterFire, 40, 40, 0 },
        { &Mage::RegisterFrost, 69, 69, 0 },
        { &Mage::RegisterArcane, 26, 26, 0 },
        { &Mage::RegisterOrb, 3, 3, 0 },
        { &Mage::RegisterRing, 3, 3, 0 },
    };
    for (PerFile const& file : files)
    {
        SpellHandlerRegistry own;
        CHECK_EQ(file.registerRows(own), file.answered);
        CHECK_EQ(own.Count(), file.keys);
        CHECK_EQ(own.CountDefaults(), file.defaults);
    }

    // Registering again on the same table changes nothing: every key and default is taken.
    CHECK_EQ(RegisterMageScripts(registry), uint32(156));
    CHECK_EQ(registry.Count(), std::size_t(153));
    CHECK_EQ(registry.CountDefaults(), std::size_t(3));

    // The game's table holds every one of these keys with the same body, and the same three defaults. The handler
    // files hold the mage sites' other rows: Mirror Image at the periodic dummy tick site, Brain Freeze's Pushing the
    // Limit label (57761) at the boost site, Cold Snap at the dummy-effect site and Focus Magic at the crit-chance proc
    // site. Hot Streak's buff (48108), Combustion (11129), and 28682 and 44401, which the client's data lacks, are no
    // label of the boost site.
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    std::vector<std::pair<uint32, uint32> > const mine = registry.Keys();
    std::vector<std::pair<uint32, uint32> > const all = game.Keys();
    CHECK_EQ(mine.size(), std::size_t(153));
    CHECK(std::includes(all.begin(), all.end(), mine.begin(), mine.end()));
    CheckRowsInTheGame<AuraTriggerSpellMageSite>(registry, TRIGGER_SPELL_IDS);
    CheckRowsInTheGame<SpellEffectDummyMageSite>(registry, EFFECT_DUMMY_IDS);
    CheckRowsInTheGame<DummyAuraProcMageSite>(registry, DUMMY_PROC_IDS);
    CHECK(game.FindDefault<AuraTriggerSpellMageSite>() == registry.FindDefault<AuraTriggerSpellMageSite>());
    CHECK(game.FindDefault<AuraPeriodicDummyTickMageSite>() == registry.FindDefault<AuraPeriodicDummyTickMageSite>());
    CHECK(game.FindDefault<AuraBoostMageSite>() == registry.FindDefault<AuraBoostMageSite>());
    CHECK_EQ(game.CountAt(AuraTriggerSpellMageSite::Key), std::size_t(1));
    CHECK_EQ(game.CountAt(AuraPeriodicDummyTickMageSite::Key), std::size_t(1));
    CHECK_EQ(game.CountAt(AuraBoostMageSite::Key), std::size_t(1));
    CHECK_EQ(game.CountAt(SpellEffectDummyMageSite::Key), std::size_t(6));
    CHECK_EQ(game.CountAt(DummyAuraProcMageSite::Key), std::size_t(3));
    CheckRowsInTheGame<LaunchHook>(registry, FROST_LAUNCH_IDS);
    CheckRowsInTheGame<CastStartHook>(registry, FROSTBOLT_CAST_START_IDS);
    CHECK_EQ(game.CountAt(SpellCritChanceAuraProcMageSite::Key), std::size_t(1));
    CHECK(game.Find<AuraPeriodicDummyTickMageSite>(55342) != NULL);
    CHECK(registry.Find<AuraPeriodicDummyTickMageSite>(55342) == NULL);
    CHECK(game.Find<AuraBoostMageSite>(57761) != NULL);
    CHECK(registry.Find<AuraBoostMageSite>(57761) == NULL);
    for (uint32 spellId : { 48108u, 11129u, 28682u, 44401u })
    {
        CHECK(game.Find<AuraBoostMageSite>(spellId) == NULL);
    }
    CHECK(game.Find<SpellEffectDummyMageSite>(11958) != NULL);
    CHECK(registry.Find<SpellEffectDummyMageSite>(11958) == NULL);
    CHECK(game.Find<SpellCritChanceAuraProcMageSite>(54646) != NULL);
    CHECK(registry.Find<SpellCritChanceAuraProcMageSite>(54646) == NULL);
}

TEST(MageScripts_TheIgniteRanksShareOneBodyAtTheProcHook)
{
    SpellHandlerRegistry registry;
    RegisterMageScripts(registry);

    // The three Ignite ranks of the client's data share one body at the proc hook, on this table and the game's; the
    // two ids the client's data lacks (12847, 12848) are a key of no site and no hook.
    SpellHandler<OnProcHook>::Function ignite = OneProcBody(registry, IGNITE_IDS);
    CHECK(ignite != NULL);
    for (std::pair<uint32, uint32> const& key : SpellHandlerRegistry::Game().Keys())
    {
        CHECK(key.second != 12847u && key.second != 12848u);
    }

    // The Glyphs of Ice Block and Polymorph and Blessing of Ancient Kings each have their own body at the dummy proc
    // site: 3 labels in 3 bodies, and no Ignite rank among them.
    typedef SpellHandler<DummyAuraProcMageSite>::Function ProcBody;
    for (uint32 spellId : IGNITE_IDS)
    {
        CHECK(registry.Find<DummyAuraProcMageSite>(spellId) == NULL);
    }
    std::set<ProcBody> procBodies;
    for (uint32 spellId : DUMMY_PROC_IDS)
    {
        procBodies.insert(registry.Find<DummyAuraProcMageSite>(spellId));
    }
    CHECK(procBodies.count(NULL) == 0);
    CHECK_EQ(procBodies.size(), std::size_t(3));

    // No mage file holds a label at the boost site; 74396, which the client's data lacks, is a key of no site.
    CHECK_EQ(registry.CountAt(AuraBoostMageSite::Key), std::size_t(0));
    for (std::pair<uint32, uint32> const& key : SpellHandlerRegistry::Game().Keys())
    {
        CHECK(key.second != 74396u);
    }
}

TEST(MageScripts_TheEffectDummySiteHoldsItsFiveLabelsAndNoDefault)
{
    SpellHandlerRegistry registry;
    RegisterMageScripts(registry);
    CHECK_EQ(registry.CountAt(SpellEffectDummyMageSite::Key), std::size_t(5));

    // Five labels, four bodies: the two raid buffs share one; every other label has its own.
    typedef SpellHandler<SpellEffectDummyMageSite>::Function MageBody;
    MageBody raidBuff = registry.Find<SpellEffectDummyMageSite>(1459);
    CHECK(raidBuff != NULL);
    CHECK(registry.Find<SpellEffectDummyMageSite>(61316) == raidBuff);
    std::set<MageBody> bodies;
    for (uint32 spellId : { 32826u, 38194u, 42955u })
    {
        bodies.insert(registry.Find<SpellEffectDummyMageSite>(spellId));
    }
    CHECK(bodies.count(NULL) == 0);
    CHECK(bodies.count(raidBuff) == 0);
    CHECK_EQ(bodies.size(), std::size_t(3));

    // No default: an id with no row (the warrior's Last Stand) finds nothing, and its dispatch is a miss, so the
    // family's case goes on to the Conjure Mana Gem check with nothing changed.
    alignas(16) static unsigned char objects[1][16];
    Spell* spell = reinterpret_cast<Spell*>(objects[0]);
    Unit* caster = NULL;
    Unit* target = NULL;
    int32 damage = 90;
    SpellEffectEntry const* effect = DummyEffect(1);
    SpellEffectDummyMageContext ctx(spell, caster, target, damage, effect);
    CHECK(registry.FindDefault<SpellEffectDummyMageSite>() == NULL);
    CHECK(registry.Find<SpellEffectDummyMageSite>(12975) == NULL);
    SpellHandlerOutcome<void> miss = registry.Dispatch<SpellEffectDummyMageSite>(12975, ctx);
    CHECK(miss.IsMiss());
    CHECK(!miss.IsReturn());
    CHECK(ctx.spell == spell);
    CHECK(caster == NULL);
    CHECK(target == NULL);
    CHECK_EQ(damage, int32(90));
    CHECK_EQ(effect->EffectIndex, uint32(1));

    // The bodies that need no live unit end the effect with no unit target: Polymorph Cast Visual, Blink and the raid
    // buffs. Conjure Refreshment reads the target's level and creates an item through the spell; it needs a live unit
    // and a spell.
    for (uint32 spellId : { 32826u, 38194u, 1459u, 61316u })
    {
        SpellHandlerOutcome<void> noTarget = registry.Dispatch<SpellEffectDummyMageSite>(spellId, ctx);
        CHECK(noTarget.IsReturn());
        CHECK(!noTarget.IsContinue());
    }
    CHECK(caster == NULL);
    CHECK(target == NULL);
    CHECK_EQ(damage, int32(90));

    // The game's table holds the same bodies, with Cold Snap's row from the handler file, and no default.
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    CheckRowsInTheGame<SpellEffectDummyMageSite>(registry, EFFECT_DUMMY_IDS);
    CHECK(game.FindDefault<SpellEffectDummyMageSite>() == NULL);
}

TEST(MageScripts_TheProcHookHoldsTheIdsOfEachProcTalentOnce)
{
    SpellHandlerRegistry registry;
    RegisterMageScripts(registry);
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();

    // Each talent's ids share one body, and each talent has its own: 25 ids in 14 bodies, the same in the game's table.
    std::set<SpellHandler<OnProcHook>::Function> bodies;
    bodies.insert(OneProcBody(registry, IGNITE_IDS));
    bodies.insert(OneProcBody(registry, MASTER_OF_ELEMENTS_IDS));
    bodies.insert(OneProcBody(registry, ARCANE_POTENCY_IDS));
    bodies.insert(OneProcBody(registry, HOT_STREAK_IDS));
    bodies.insert(OneProcBody(registry, BLAZING_SPEED_IDS));
    bodies.insert(OneProcBody(registry, MANA_SHIELD_IDS));
    bodies.insert(OneProcBody(registry, PERSISTENT_SHIELD_IDS));
    bodies.insert(OneProcBody(registry, OFFENSIVE_STATE_IDS));
    bodies.insert(OneProcBody(registry, DEEP_FREEZE_IMMUNITY_IDS));
    bodies.insert(OneProcBody(registry, FINGERS_OF_FROST_IDS));
    bodies.insert(OneProcBody(registry, IMPACT_IDS));
    bodies.insert(OneProcBody(registry, NETHER_VORTEX_IDS));
    bodies.insert(OneProcBody(registry, PERMAFROST_IDS));
    bodies.insert(OneProcBody(registry, FOCUS_MAGIC_IDS));
    CHECK_EQ(bodies.size(), std::size_t(14));
    std::size_t const ids = sizeof(IGNITE_IDS) / sizeof(uint32) +
                            sizeof(MASTER_OF_ELEMENTS_IDS) / sizeof(uint32) +
                            sizeof(ARCANE_POTENCY_IDS) / sizeof(uint32) + sizeof(HOT_STREAK_IDS) / sizeof(uint32) +
                            sizeof(BLAZING_SPEED_IDS) / sizeof(uint32) + sizeof(MANA_SHIELD_IDS) / sizeof(uint32) +
                            sizeof(PERSISTENT_SHIELD_IDS) / sizeof(uint32) + sizeof(OFFENSIVE_STATE_IDS) / sizeof(uint32) +
                            sizeof(DEEP_FREEZE_IMMUNITY_IDS) / sizeof(uint32) + sizeof(FINGERS_OF_FROST_IDS) / sizeof(uint32) +
                            sizeof(IMPACT_IDS) / sizeof(uint32) + sizeof(NETHER_VORTEX_IDS) / sizeof(uint32) +
                            sizeof(PERMAFROST_IDS) / sizeof(uint32) + sizeof(FOCUS_MAGIC_IDS) / sizeof(uint32);
    CHECK_EQ(ids, std::size_t(25));
    CHECK_EQ(registry.CountAt(OnProcHook::Key), ids);
    CHECK_EQ(game.CountAt(OnProcHook::Key), ids);
    CHECK_EQ(game.CountAt(CheckProcHook::Key),
             (sizeof(IMPACT_IDS) + sizeof(CLEARCASTING_IDS) + sizeof(ARCANE_BLAST_BUFF_IDS) +
              sizeof(ARCANE_POTENCY_BUFF_IDS) + sizeof(FINGERS_OF_FROST_IDS) + sizeof(BRAIN_FREEZE_IDS) +
              sizeof(INVOCATION_IDS)) / sizeof(uint32));

    // No script stands for spells no 4.3.4 proc reaches: Burnout's five ranks (no dummy aura), Combustion (11129
    // applies no aura), 28682 (absent from the client's data) and Magic Absorption's two ranks (29441, 29444), a talent
    // no 4.3.4 tree, trainer or spell gives. Arcane Blast's buff (36032) holds no proc script: its check-proc script
    // lets the engine's own handling spend its charge.
    for (uint32 spellId : { 44449u, 44469u, 44470u, 44471u, 44472u, 11129u, 28682u, 29441u, 29444u })
    {
        CHECK(game.Find<OnProcHook>(spellId) == NULL);
        CHECK(game.Find<CheckProcHook>(spellId) == NULL);
    }
    CHECK(game.Find<OnProcHook>(36032) == NULL);

    // Improved Hot Streak (44446, 44448) holds no script: its procs reach the mage dummy proc site, which has no row
    // for them and casts nothing; Hot Streak's script reads its rank from the caster's auras by id.
    for (uint32 spellId : { 44446u, 44448u })
    {
        CHECK(game.Find<OnProcHook>(spellId) == NULL);
        CHECK(game.Find<CheckProcHook>(spellId) == NULL);
    }

    // The proc-trigger scripts end the proc themselves (Mage::CastProcTriggerSpell): no row and no default of the
    // legacy site of trigger spells, which the proc-trigger handler dispatches for the spell it casts, answers for the
    // spells they cast: Blazing Speed's 31643, Persistent Shield's 26470, Deep Freeze's damage 71757 and Fingers of
    // Frost's 44544.
    for (uint32 spellId : { 31643u, 26470u, 71757u, 44544u })
    {
        CHECK(game.Find<ProcTriggerSpellAuraProcTriggeredSite>(spellId) == NULL);
    }
    CHECK(game.FindDefault<ProcTriggerSpellAuraProcTriggeredSite>() == NULL);

    // No script stands for the holders of icon 32 (Ice Barrier 11426 and 98864, Glyph of Ice Barrier 63095): no row of
    // the mage boost site, the aura-apply or the aura-remove hook names them, and the site's default returns for them.
    for (uint32 spellId : { 11426u, 98864u, 63095u })
    {
        CHECK(game.Find<AuraBoostMageSite>(spellId) == NULL);
        CHECK(game.Find<AuraApplyHook>(spellId) == NULL);
        CHECK(game.Find<AuraRemoveHook>(spellId) == NULL);
    }
    CHECK(game.FindDefault<AuraBoostMageSite>() != NULL);
}

TEST(MageScripts_TheProcScriptsKeepTheirEarlyRefusals)
{
    AddProcEffectRows();
    SpellHandlerRegistry registry;
    RegisterMageScripts(registry);
    BareCreature owner;
    HolderStorage storage;
    SpellAuraHolder& holder = EmptyHolder(storage);

    // Master of Elements and Arcane Potency: an event with no spell is refused.
    SpellHandlerOutcome<SpellAuraProcResult> noSpell = Proc(registry, owner, 29074, EFFECT_INDEX_0, holder, NULL, PROC_EX_CRITICAL_HIT, 15);
    CHECK(noSpell.IsReturn());
    CHECK_EQ(int(noSpell.GetValue()), int(SPELL_AURA_PROC_FAILED));
    noSpell = Proc(registry, owner, 31571, EFFECT_INDEX_0, holder, NULL, PROC_EX_NONE, 7);
    CHECK(noSpell.IsReturn());
    CHECK_EQ(int(noSpell.GetValue()), int(SPELL_AURA_PROC_FAILED));

    // Hot Streak: effect 1 does nothing; effect 0 refuses an event with no spell; a crit of Pyroblast! (92315) neither
    // counts for nor breaks the streak; a hit that does not crit starts the count again. None of these casts.
    AuraHandle(holder).ScriptWord(EFFECT_INDEX_0) = 1;
    SpellHandlerOutcome<SpellAuraProcResult> streak = Proc(registry, owner, 44445, EFFECT_INDEX_1, holder, NULL, PROC_EX_CRITICAL_HIT, 25);
    CHECK(streak.IsReturn());
    CHECK_EQ(int(streak.GetValue()), int(SPELL_AURA_PROC_OK));
    CHECK_EQ(AuraHandle(holder).ScriptWord(EFFECT_INDEX_0), int32(1));
    streak = Proc(registry, owner, 44445, EFFECT_INDEX_0, holder, NULL, PROC_EX_CRITICAL_HIT, 33);
    CHECK(streak.IsReturn());
    CHECK_EQ(int(streak.GetValue()), int(SPELL_AURA_PROC_FAILED));
    CHECK_EQ(AuraHandle(holder).ScriptWord(EFFECT_INDEX_0), int32(1));
    SpellRowStorage eventRow;
    streak = Proc(registry, owner, 44445, EFFECT_INDEX_0, holder, &SpellRow(eventRow, 92315), PROC_EX_CRITICAL_HIT, 33);
    CHECK(streak.IsReturn());
    CHECK_EQ(int(streak.GetValue()), int(SPELL_AURA_PROC_OK));
    CHECK_EQ(AuraHandle(holder).ScriptWord(EFFECT_INDEX_0), int32(1));
    streak = Proc(registry, owner, 44445, EFFECT_INDEX_0, holder, &SpellRow(eventRow, 133), PROC_EX_NORMAL_HIT, 33);
    CHECK(streak.IsReturn());
    CHECK_EQ(int(streak.GetValue()), int(SPELL_AURA_PROC_OK));
    CHECK_EQ(AuraHandle(holder).ScriptWord(EFFECT_INDEX_0), int32(0));

    // Blazing Speed: with no Blazing Speed buff (31643) in the spell store the proc is refused, as the proc-trigger
    // handler refuses a trigger spell the store lacks.
    SpellHandlerOutcome<SpellAuraProcResult> noBuff = Proc(registry, owner, 31641, EFFECT_INDEX_0, holder, NULL, PROC_EX_NORMAL_HIT, 0);
    CHECK(noBuff.IsReturn());
    CHECK_EQ(int(noBuff.GetValue()), int(SPELL_AURA_PROC_FAILED));

    // Mana Shield (Incanter's Regalia): an owner that is no player is refused.
    SpellHandlerOutcome<SpellAuraProcResult> notPlayer = Proc(registry, owner, 1463, EFFECT_INDEX_0, holder, NULL, PROC_EX_NORMAL_HIT, 0);
    CHECK(notPlayer.IsReturn());
    CHECK_EQ(int(notPlayer.GetValue()), int(SPELL_AURA_PROC_FAILED));

    // Persistent Shield: with no 26470 in the spell store the proc is refused, as the proc-trigger handler refuses.
    SpellHandlerOutcome<SpellAuraProcResult> noShield = Proc(registry, owner, 26467, EFFECT_INDEX_0, holder, NULL, PROC_EX_NORMAL_HIT, 0);
    CHECK(noShield.IsReturn());
    CHECK_EQ(int(noShield.GetValue()), int(SPELL_AURA_PROC_FAILED));

    // Deep Freeze Immunity State: an event with no creature victim is refused.
    SpellHandlerOutcome<SpellAuraProcResult> noCreature = Proc(registry, owner, 71761, EFFECT_INDEX_0, holder, NULL, PROC_EX_NORMAL_HIT, 0);
    CHECK(noCreature.IsReturn());
    CHECK_EQ(int(noCreature.GetValue()), int(SPELL_AURA_PROC_FAILED));

    // Fingers of Frost: an amount of 0 is a chance of 0, so the roll refuses the proc.
    SpellHandlerOutcome<SpellAuraProcResult> noChance = Proc(registry, owner, 83074, EFFECT_INDEX_0, holder, NULL, PROC_EX_NORMAL_HIT, 0);
    CHECK(noChance.IsReturn());
    CHECK_EQ(int(noChance.GetValue()), int(SPELL_AURA_PROC_FAILED));

    // Offensive State: the aura's amount is its chance, so an amount of 0 is refused by the roll; an owner that holds
    // Hot Streak (44445) cannot trigger it at all, whatever the amount, and no charge or cooldown follows.
    noChance = Proc(registry, owner, 79684, EFFECT_INDEX_0, holder, NULL, PROC_EX_NORMAL_HIT, 0);
    CHECK(noChance.IsReturn());
    CHECK_EQ(int(noChance.GetValue()), int(SPELL_AURA_PROC_FAILED));
    HolderStorage hotStreakStorage;
    owner.HoldAura(44445, EmptyHolder(hotStreakStorage));
    CHECK(owner.HasAura(44445));
    for (int32 amount : { 0, 40, 100 })
    {
        SpellHandlerOutcome<SpellAuraProcResult> silent = Proc(registry, owner, 79684, EFFECT_INDEX_0, holder, NULL, PROC_EX_NORMAL_HIT, amount);
        CHECK(silent.IsReturn());
        CHECK_EQ(int(silent.GetValue()), int(SPELL_AURA_PROC_CANT_TRIGGER));
    }
}

TEST(MageScripts_OffensiveStateNeverRollsForArcaneMissiles)
{
    // The decision: Arcane Missiles (5143) and its missiles (7268) are refused by id; Arcane Blast, Fireball, Arcane
    // Missiles! itself and an event with no spell id are not.
    CHECK(Mage::OffensiveStateRefuses(5143));
    CHECK(Mage::OffensiveStateRefuses(7268));
    for (uint32 spellId : { 30451u, 133u, 79683u, 0u })
    {
        CHECK(!Mage::OffensiveStateRefuses(spellId));
    }

    // The proc script: at an amount of 100 the channel's cast and its missiles cannot trigger it, so no charge or
    // cooldown follows; Arcane Blast at the same amount passes the roll and goes on to the cast of the trigger spell,
    // which the test's effect row does not name, so the proc fails there.
    AddProcEffectRows();
    SpellHandlerRegistry registry;
    RegisterMageScripts(registry);
    BareCreature owner;
    HolderStorage storage;
    SpellAuraHolder& holder = EmptyHolder(storage);
    for (uint32 spellId : { 5143u, 7268u })
    {
        SpellRowStorage eventRow;
        SpellHandlerOutcome<SpellAuraProcResult> refused =
            Proc(registry, owner, 79684, EFFECT_INDEX_0, holder, &SpellRow(eventRow, spellId), PROC_EX_NORMAL_HIT, 100);
        CHECK(refused.IsReturn());
        CHECK_EQ(int(refused.GetValue()), int(SPELL_AURA_PROC_CANT_TRIGGER));
    }
    SpellRowStorage arcaneBlastRow;
    SpellHandlerOutcome<SpellAuraProcResult> rolled =
        Proc(registry, owner, 79684, EFFECT_INDEX_0, holder, &SpellRow(arcaneBlastRow, 30451), PROC_EX_NORMAL_HIT, 100);
    CHECK(rolled.IsReturn());
    CHECK_EQ(int(rolled.GetValue()), int(SPELL_AURA_PROC_FAILED));
}

TEST(MageScripts_ArcaneMissilesAurastateFollowsArcaneMissilesBuff)
{
    // The decision: the aurastate (79808) is cast on a mage who holds Arcane Missiles! (79683) without it, taken off a
    // mage who holds it without the buff, and left as it is when he holds both or neither.
    CHECK_EQ(int(Mage::ArcaneMissilesAurastateStep(true, false)), int(Mage::AURASTATE_APPLY));
    CHECK_EQ(int(Mage::ArcaneMissilesAurastateStep(false, true)), int(Mage::AURASTATE_REMOVE));
    CHECK_EQ(int(Mage::ArcaneMissilesAurastateStep(true, true)), int(Mage::AURASTATE_KEEP));
    CHECK_EQ(int(Mage::ArcaneMissilesAurastateStep(false, false)), int(Mage::AURASTATE_KEEP));

    // The rows: the buff at the aura-apply and aura-remove hooks and the channel at the aura-apply hook, the buff's
    // apply and the channel's each a body of its own, the same in the game's table. The aurastate and the missiles
    // hold no row.
    SpellHandlerRegistry registry;
    RegisterMageScripts(registry);
    CheckRowsInTheGame<AuraApplyHook>(registry, ARCANE_MISSILES_BUFF_IDS);
    CheckRowsInTheGame<AuraRemoveHook>(registry, ARCANE_MISSILES_BUFF_IDS);
    CheckRowsInTheGame<AuraApplyHook>(registry, ARCANE_MISSILES_IDS);
    CHECK(registry.Find<AuraApplyHook>(79683) != registry.Find<AuraApplyHook>(5143));
    CHECK(registry.Find<AuraRemoveHook>(5143) == NULL);
    for (uint32 spellId : { 79808u, 7268u })
    {
        CHECK(SpellHandlerRegistry::Game().Find<AuraApplyHook>(spellId) == NULL);
        CHECK(SpellHandlerRegistry::Game().Find<AuraRemoveHook>(spellId) == NULL);
    }

    // The buff's hooks leave a mage alone when he holds both or neither, whatever the removal's mode, and let the
    // holder's boosts run. A mage who holds the buff without the aurastate goes on to its cast, which the test's
    // spell store lacks, so nothing is cast.
    BareCreature mage;
    SpellRowStorage buffRow;
    HolderStorage storage;
    SpellAuraHolder& holder = EmptyHolder(storage);
    AuraRemoveMode const modes[] =
    {
        AURA_REMOVE_BY_DEFAULT, AURA_REMOVE_BY_EXPIRE, AURA_REMOVE_BY_DISPEL, AURA_REMOVE_BY_CANCEL, AURA_REMOVE_BY_DEATH,
        AURA_REMOVE_BY_STACK,
    };
    for (AuraRemoveMode mode : modes)
    {
        AuraRemoveContext neither(mage, &mage, SpellRow(buffRow, 79683), AuraHandle(holder), mode);
        CHECK(registry.Dispatch<AuraRemoveHook>(79683, neither).IsContinue());
    }
    CHECK(!mage.HasAura(79808));
    HolderStorage buffStorage;
    HolderStorage aurastateStorage;
    mage.HoldAura(79683, EmptyHolder(buffStorage));
    CHECK(sSpellStore.LookupEntry(79808) == NULL);
    AuraApplyContext withoutAurastate(mage, &mage, SpellRow(buffRow, 79683), AuraHandle(holder), true);
    CHECK(registry.Dispatch<AuraApplyHook>(79683, withoutAurastate).IsContinue());
    CHECK(!mage.HasAura(79808));
    mage.HoldAura(79808, EmptyHolder(aurastateStorage));
    AuraApplyContext both(mage, &mage, SpellRow(buffRow, 79683), AuraHandle(holder), true);
    CHECK(registry.Dispatch<AuraApplyHook>(79683, both).IsContinue());
    for (AuraRemoveMode mode : modes)
    {
        AuraRemoveContext stillHeld(mage, &mage, SpellRow(buffRow, 79683), AuraHandle(holder), mode);
        CHECK(registry.Dispatch<AuraRemoveHook>(79683, stillHeld).IsContinue());
    }
    CHECK(mage.HasAura(79683));
    CHECK(mage.HasAura(79808));

    // The channel's script acts only for its holder on the mage himself: its holder on the target, or one whose caster
    // cannot be found, takes nothing off the unit it is on.
    BareCreature target;
    HolderStorage targetBuffStorage;
    target.HoldAura(79683, EmptyHolder(targetBuffStorage));
    SpellRowStorage channelRow;
    AuraApplyContext onTarget(target, &mage, SpellRow(channelRow, 5143), AuraHandle(holder), true);
    CHECK(registry.Dispatch<AuraApplyHook>(5143, onTarget).IsContinue());
    AuraApplyContext noCaster(target, NULL, SpellRow(channelRow, 5143), AuraHandle(holder), true);
    CHECK(registry.Dispatch<AuraApplyHook>(5143, noCaster).IsContinue());
    CHECK(target.HasAura(79683));
}

TEST(MageScripts_ArcaneMissilesAimAtTheUnitTheChannelStartedOn)
{
    // The missiles' implicit target 77 is his channel object while Arcane Missiles (5143) is his channel, whatever his
    // target; a channel object of another channel, one that is no unit, or none, with no channel of 5143 cast, is none.
    const ObjectGuid camelA(HIGHGUID_UNIT, uint32(50249), uint32(1));
    const ObjectGuid camelB(HIGHGUID_UNIT, uint32(50249), uint32(2));
    const ObjectGuid bobber(HIGHGUID_GAMEOBJECT, uint32(35591), uint32(3));
    BareCreature mage;
    mage.SetTargetGuid(camelA);
    CHECK(SingleEnemyTargetAura::ChannelTargetOf(mage, 5143).IsEmpty());

    mage.SetUInt32Value(UNIT_CHANNEL_SPELL, 5143);
    mage.SetChannelObjectGuid(camelA);
    CHECK(SingleEnemyTargetAura::ChannelTargetOf(mage, 5143) == camelA);
    mage.SetTargetGuid(camelB);
    CHECK(SingleEnemyTargetAura::ChannelTargetOf(mage, 5143) == camelA);
    mage.SetTargetGuid(ObjectGuid());
    CHECK(SingleEnemyTargetAura::ChannelTargetOf(mage, 5143) == camelA);
    CHECK(SingleEnemyTargetAura::ChannelTargetOf(mage, 47758).IsEmpty());

    mage.SetTargetGuid(camelB);
    mage.SetChannelObjectGuid(bobber);
    CHECK(SingleEnemyTargetAura::ChannelTargetOf(mage, 5143).IsEmpty());
    mage.SetChannelObjectGuid(ObjectGuid());
    CHECK(SingleEnemyTargetAura::ChannelTargetOf(mage, 5143).IsEmpty());
}

namespace
{
    uint32 const ROW_OWN_TARGET_CHANNEL = 9420017;

    // A channel's Spell.dbc row in the spell store, where a Spell's constructor checks it: only its id and the channel
    // attribute, so its cast takes the caster's channel slot. Put there once for the rest of the run; no other test
    // reads it.
    SpellEntry const* OwnTargetChannelRow()
    {
        static SpellRowStorage storage;
        static bool added = false;
        SpellEntry* row = reinterpret_cast<SpellEntry*>(storage.bytes);
        if (!added)
        {
            added = true;
            SpellRow(storage, ROW_OWN_TARGET_CHANNEL);
            row->AttributesEx = SPELL_ATTR_EX_CHANNELED_1;
            sSpellStore.SetEntry(ROW_OWN_TARGET_CHANNEL, row);
        }
        return row;
    }

    // A bare creature that a unit guid of its own names.
    class NamedCreature : public BareCreature
    {
        public:
            explicit NamedCreature(uint32 low)
            {
                Object::_Create(low, 50249, HIGHGUID_UNIT);
            }
    };
}

TEST(MageScripts_AChannelWithNoChannelObjectAimsAtItsOwnUnitTarget)
{
    // With his channel object empty, as the server leaves it for a channel at himself, the target is the unit target of
    // the channel he is casting when its id is the one asked for: the Camel it was cast at, or himself for a channel at
    // himself. A channel with no unit target, another spell's id, and no channel at all give none; a unit in his
    // channel object for that channel answers first.
    std::unique_ptr<Spell> channel;
    NamedCreature mage(1);
    NamedCreature camel(2);
    CHECK(!camel.GetObjectGuid().IsEmpty());
    CHECK(SingleEnemyTargetAura::ChannelTargetOf(mage, ROW_OWN_TARGET_CHANNEL).IsEmpty());

    channel.reset(new Spell(&mage, OwnTargetChannelRow(), false));
    mage.SetCurrentCastedSpell(channel.get());
    CHECK(mage.GetCurrentSpell(CURRENT_CHANNELED_SPELL) == channel.get());
    CHECK(SingleEnemyTargetAura::ChannelTargetOf(mage, ROW_OWN_TARGET_CHANNEL).IsEmpty());

    channel->m_targets.setUnitTarget(&camel);
    CHECK(SingleEnemyTargetAura::ChannelTargetOf(mage, ROW_OWN_TARGET_CHANNEL) == camel.GetObjectGuid());
    CHECK(SingleEnemyTargetAura::ChannelTargetOf(mage, 5143).IsEmpty());

    channel->m_targets.setUnitTarget(&mage);
    CHECK(SingleEnemyTargetAura::ChannelTargetOf(mage, ROW_OWN_TARGET_CHANNEL) == mage.GetObjectGuid());

    mage.SetUInt32Value(UNIT_CHANNEL_SPELL, ROW_OWN_TARGET_CHANNEL);
    mage.SetChannelObjectGuid(camel.GetObjectGuid());
    CHECK(SingleEnemyTargetAura::ChannelTargetOf(mage, ROW_OWN_TARGET_CHANNEL) == camel.GetObjectGuid());
}

TEST(MageScripts_ArcaneBlastBuffEndsOnlyAtTheCastOfAnotherArcaneDamageSpell)
{
    // The end of the mage's own cast of an Arcane damage spell ends the buff: Arcane Missiles (5143), Arcane Barrage
    // (44425), and by its school any other, one of several schools Arcane among them.
    for (uint32 spellId : { 5143u, 44425u, ROW_DIRECT_DAMAGE })
    {
        CHECK(Mage::ArcaneBlastBuffEnds(spellId, SPELL_SCHOOL_MASK_ARCANE, true, true, true));
    }
    CHECK(Mage::ArcaneBlastBuffEnds(ROW_DIRECT_DAMAGE, SPELL_SCHOOL_MASK_ARCANE | SPELL_SCHOOL_MASK_FIRE, true, true,
                                    true));

    // Arcane Blast (30451) and Arcane Explosion (1449) never end it.
    for (uint32 spellId : { 30451u, 1449u })
    {
        CHECK(!Mage::ArcaneBlastBuffEnds(spellId, SPELL_SCHOOL_MASK_ARCANE, true, true, true));
    }

    // Nor does a hit, a triggered cast (Arcane Missiles' missile, 7268), a spell of other schools (Fireball, 133;
    // Frostfire Bolt, 44614), or an Arcane spell that deals no damage (Slow, 31589; Polymorph, 118; Counterspell,
    // 2139).
    CHECK(!Mage::ArcaneBlastBuffEnds(44425, SPELL_SCHOOL_MASK_ARCANE, true, false, true));
    CHECK(!Mage::ArcaneBlastBuffEnds(7268, SPELL_SCHOOL_MASK_ARCANE, true, true, false));
    CHECK(!Mage::ArcaneBlastBuffEnds(133, SPELL_SCHOOL_MASK_FIRE, true, true, true));
    CHECK(!Mage::ArcaneBlastBuffEnds(44614, SPELL_SCHOOL_MASK_FIRE | SPELL_SCHOOL_MASK_FROST, true, true, true));
    for (uint32 spellId : { 31589u, 118u, 2139u })
    {
        CHECK(!Mage::ArcaneBlastBuffEnds(spellId, SPELL_SCHOOL_MASK_ARCANE, false, true, true));
    }
}

TEST(MageScripts_DealsOrTriggersDamageReadsOneTriggerDown)
{
    AddDamageCheckRows();
    AddTriggerCheckRows();
    SpellRowStorage row;

    // A spell's own damage rows count, as Spells::DealsDamage reads them.
    for (uint32 spellId : { ROW_DIRECT_DAMAGE, ROW_PERIODIC_DAMAGE_WITH_DUMMY, ROW_PERIODIC_PERCENT, ROW_PERIODIC_LEECH,
                            ROW_WEAPON_DAMAGE })
    {
        CHECK(Mage::DealsOrTriggersDamage(SpellRow(row, spellId)));
    }

    // A channel whose periodic trigger casts a damaging missile counts through it.
    CHECK(!Spells::DealsDamage(SpellRow(row, ROW_MISSILE_CHANNEL)));
    CHECK(Mage::DealsOrTriggersDamage(SpellRow(row, ROW_MISSILE_CHANNEL)));

    // A spell that deals none and triggers none that does is not counted: Slow's auras, a confuse, an interrupt, a
    // stun, a trigger with no spell named, a trigger of a stun alone and a trigger of a spell the store lacks.
    CHECK(sSpellStore.LookupEntry(ROW_ABSENT) == NULL);
    for (uint32 spellId : { ROW_SLOW_AURAS, ROW_CONFUSE, ROW_INTERRUPT, ROW_STUN, ROW_TRIGGERS_DAMAGE,
                            ROW_TRIGGERS_STUN, ROW_TRIGGERS_ABSENT, 0u })
    {
        CHECK(!Mage::DealsOrTriggersDamage(SpellRow(row, spellId)));
    }
}

TEST(MageScripts_ArcaneBlastBuffDropsEveryEventButAnOwnCastsEnd)
{
    // The row: the buff at the check-proc hook, a body of its own, the same in the game's table; no proc script.
    SpellHandlerRegistry registry;
    RegisterMageScripts(registry);
    CheckRowsInTheGame<CheckProcHook>(registry, ARCANE_BLAST_BUFF_IDS);
    CHECK(registry.Find<OnProcHook>(36032) == NULL);
    CHECK(registry.Find<CheckProcHook>(36032) != registry.Find<CheckProcHook>(11103));

    // On a mage with no cast in his current spell slots, as when the event's spell is a triggered one, every event of
    // an Arcane damage spell is dropped: the end of its cast, a hit and a crit; and so is an event with no spell.
    AddDamageCheckRows();
    BareCreature mage;
    HolderStorage storage;
    SpellAuraHolder& holder = EmptyHolder(storage);
    SpellRowStorage buffRow;
    SpellRowStorage eventRow;
    SpellRow(eventRow, ROW_DIRECT_DAMAGE);
    reinterpret_cast<SpellEntry*>(eventRow.bytes)->SchoolMask = SPELL_SCHOOL_MASK_ARCANE;
    SpellEntry const& arcaneDamage = *reinterpret_cast<SpellEntry const*>(eventRow.bytes);
    CHECK(Mage::DealsOrTriggersDamage(arcaneDamage));
    CHECK(mage.FindCurrentSpellBySpellId(ROW_DIRECT_DAMAGE) == NULL);
    for (uint32 procEx : { uint32(PROC_EX_CAST_END), uint32(PROC_EX_NORMAL_HIT), uint32(PROC_EX_CRITICAL_HIT) })
    {
        CheckProcContext ctx(mage, NULL, SpellRow(buffRow, 36032), AuraHandle(holder), &arcaneDamage, 0x10000, procEx,
                             0);
        SpellHandlerOutcome<bool> const outcome = registry.Dispatch<CheckProcHook>(36032, ctx);
        CHECK(outcome.IsReturn());
        CHECK(!outcome.GetValue());
    }
    CheckProcContext noSpell(mage, NULL, SpellRow(buffRow, 36032), AuraHandle(holder), NULL, 0x10000, PROC_EX_CAST_END,
                             0);
    SpellHandlerOutcome<bool> const outcome = registry.Dispatch<CheckProcHook>(36032, noSpell);
    CHECK(outcome.IsReturn());
    CHECK(!outcome.GetValue());
}

TEST(MageScripts_ArcanePotencyBuffsDropTheEventsOfSpellsThatDealOrTriggerNoDamage)
{
    // The rows: both buffs at the check-proc hook, one body of their own, the same in the game's table; no proc script.
    SpellHandlerRegistry registry;
    RegisterMageScripts(registry);
    CheckRowsInTheGame<CheckProcHook>(registry, ARCANE_POTENCY_BUFF_IDS);
    CHECK(registry.Find<CheckProcHook>(57529) == registry.Find<CheckProcHook>(57531));
    CHECK(registry.Find<CheckProcHook>(57531) != registry.Find<CheckProcHook>(36032));
    for (uint32 buffId : ARCANE_POTENCY_BUFF_IDS)
    {
        CHECK(registry.Find<OnProcHook>(buffId) == NULL);
    }

    AddDamageCheckRows();
    AddTriggerCheckRows();
    BareCreature mage;
    HolderStorage storage;
    SpellAuraHolder& holder = EmptyHolder(storage);
    SpellRowStorage buffRow;
    SpellRowStorage eventRow;
    for (uint32 buffId : ARCANE_POTENCY_BUFF_IDS)
    {
        // The end of a cast of a spell that deals no damage and triggers none that does is dropped, as the cast of
        // Arcane Blast's buff (36032) is: Slow's auras, a confuse, a stun, a trigger of a stun alone.
        for (uint32 spellId : { ROW_SLOW_AURAS, ROW_CONFUSE, ROW_STUN, ROW_TRIGGERS_STUN })
        {
            SpellEntry const& event = SpellRow(eventRow, spellId);
            CHECK(!Mage::DealsOrTriggersDamage(event));
            CheckProcContext ctx(mage, NULL, SpellRow(buffRow, buffId), AuraHandle(holder), &event, 0x10000,
                                 PROC_EX_CAST_END, 0);
            SpellHandlerOutcome<bool> const outcome = registry.Dispatch<CheckProcHook>(buffId, ctx);
            CHECK(outcome.IsReturn());
            CHECK(!outcome.GetValue());
        }

        // The end of a cast of a spell that deals damage, or of a channel whose missile does, and an event with no
        // spell go on to the engine's proc handling.
        for (uint32 spellId : { ROW_DIRECT_DAMAGE, ROW_PERIODIC_DAMAGE_WITH_DUMMY, ROW_MISSILE_CHANNEL })
        {
            SpellEntry const& event = SpellRow(eventRow, spellId);
            CHECK(Mage::DealsOrTriggersDamage(event));
            CheckProcContext ctx(mage, NULL, SpellRow(buffRow, buffId), AuraHandle(holder), &event, 0x10000,
                                 PROC_EX_CAST_END, 0);
            CHECK(registry.Dispatch<CheckProcHook>(buffId, ctx).IsContinue());
        }
        CheckProcContext noSpell(mage, NULL, SpellRow(buffRow, buffId), AuraHandle(holder), NULL, 0x10000,
                                 PROC_EX_CAST_END, 0);
        CHECK(registry.Dispatch<CheckProcHook>(buffId, noSpell).IsContinue());
    }
}

TEST(MageScripts_IgniteBanksNothingForWhatIsNoDirectFireCritOrBelowATickAPoint)
{
    AddProcEffectRows();
    AddIgniteDot();
    SpellHandlerRegistry registry;
    RegisterMageScripts(registry);
    BareCreature owner;
    BareCreature victim;
    HolderStorage storage;
    SpellAuraHolder& holder = EmptyHolder(storage);
    SpellRowStorage fireballRow;
    SpellEntry const& fireball = SpellRow(fireballRow, 133);

    // The DoT's two ticks come from the client's data as the stores hold it: 4000 ms over a period of 2000.
    SpellEntry const* dot = sSpellStore.LookupEntry(12654);
    CHECK(dot != NULL);
    CHECK_EQ(GetSpellDuration(dot), int32(4000));
    CHECK(dot && dot->GetSpellEffect(EFFECT_INDEX_0) && dot->GetSpellEffect(EFFECT_INDEX_0)->EffectAuraPeriod == 2000);
    CHECK(victim.IsAlive());

    // An effect that is not the dummy (12846 has no effect 1) is left to the proc handler of its own aura type.
    CHECK(Proc(registry, owner, 12846, EFFECT_INDEX_1, holder, &fireball, PROC_EX_CRITICAL_HIT, 40, &victim, 10000).IsContinue());

    // A hit that did not crit is refused.
    SpellHandlerOutcome<SpellAuraProcResult> outcome =
        Proc(registry, owner, 12846, EFFECT_INDEX_0, holder, &fireball, PROC_EX_NORMAL_HIT, 40, &victim, 10000);
    CHECK(outcome.IsReturn());
    CHECK_EQ(int(outcome.GetValue()), int(SPELL_AURA_PROC_FAILED));

    // A crit of Molten Armor's hit, of Living Bomb's explosion, of a Flame Orb hit or of Fire Power's explosion is
    // refused, by its id.
    for (uint32 spellId : { 34913u, 44461u, 82739u, 83619u })
    {
        SpellRowStorage notIgniting;
        outcome = Proc(registry, owner, 12846, EFFECT_INDEX_0, holder, &SpellRow(notIgniting, spellId), PROC_EX_CRITICAL_HIT, 40,
                       &victim, 10000);
        CHECK(outcome.IsReturn());
        CHECK_EQ(int(outcome.GetValue()), int(SPELL_AURA_PROC_FAILED));
    }

    // A crit whose share is below one point a tick is refused: 40% of 1 is 0, and 40% of 4 is 1, below two ticks.
    for (uint32 damage : { 1u, 4u })
    {
        outcome = Proc(registry, owner, 12846, EFFECT_INDEX_0, holder, &fireball, PROC_EX_CRITICAL_HIT, 40, &victim, damage);
        CHECK(outcome.IsReturn());
        CHECK_EQ(int(outcome.GetValue()), int(SPELL_AURA_PROC_FAILED));
    }

    // A crit with no victim is refused, and so is one for an aura whose amount is no percent.
    outcome = Proc(registry, owner, 12846, EFFECT_INDEX_0, holder, &fireball, PROC_EX_CRITICAL_HIT, 40, NULL, 10000);
    CHECK(outcome.IsReturn());
    CHECK_EQ(int(outcome.GetValue()), int(SPELL_AURA_PROC_FAILED));
    outcome = Proc(registry, owner, 12846, EFFECT_INDEX_0, holder, &fireball, PROC_EX_CRITICAL_HIT, 0, &victim, 10000);
    CHECK(outcome.IsReturn());
    CHECK_EQ(int(outcome.GetValue()), int(SPELL_AURA_PROC_FAILED));

    // Nothing was cast: the victim holds no Ignite.
    CHECK(victim.GetSpellAuraHolder(12654) == NULL);
}

TEST(MageScripts_IgniteMunchesACritOfTheUpdateOfItsLastBankBeforeItTicks)
{
    // An Ignite banked in update 5000 with two ticks: a crit of that update, while both ticks are left, replaces the
    // banked share.
    CHECK(Mage::IgniteMunches(5000, 2, 5000, 2));

    // A crit of the next update adds to it, and so does one of the same update after the Ignite has ticked.
    CHECK(!Mage::IgniteMunches(5000, 2, 5050, 2));
    CHECK(!Mage::IgniteMunches(5000, 3, 5000, 2));

    // An Ignite no crit banked in (a copy Impact spread) never munches.
    CHECK(!Mage::IgniteMunches(0, 0, 0, 0));
    CHECK(!Mage::IgniteMunches(5000, 0, 5000, 0));
}

TEST(MageScripts_AMunchedIgniteRollsOverTheLaterShareAndOnlyWhatTheIgniteBeforeTheEarlierHadLeft)
{
    // Ignite's two ticks, and the three of an Ignite that replaces one before its next tick and carries that tick's
    // time.
    uint32 const shareTicks = 2;
    uint32 const refreshed = 3;

    // On a target with no Ignite the earlier crit banks 1000 a tick. A later crit of 1500 a tick in another update
    // rolls both over: (2 x 1500 + 2 x 1000) / 3 = 1666 a tick.
    CHECK_EQ(Spells::RolledOverTick(1500, shareTicks, 1000, shareTicks, refreshed), int32(1666));

    // In the same update the earlier Ignite first carries what was left before it, nothing, so the later one ticks for
    // (2 x 1500) / 3 = 1000: the later share alone.
    int32 const nothingLeft = Mage::IgniteMunchedTick(0, shareTicks);
    CHECK_EQ(nothingLeft, int32(0));
    CHECK_EQ(Spells::RolledOverTick(1500, shareTicks, nothingLeft, shareTicks, refreshed), int32(1000));

    // On a target whose Ignite has one tick of 600 left, the earlier crit's Ignite rolls it over, (2 x 1000 + 600) / 3
    // = 866 a tick, and banks 600 as the leftover. A later crit of 1500 a tick in another update carries all of it:
    // (2 x 1500 + 3 x 866) / 3 = 1866.
    int32 const leftover = 600;
    int32 const earlier = Spells::RolledOverTick(1000, shareTicks, 600, 1, refreshed);
    CHECK_EQ(earlier, int32(866));
    CHECK_EQ(Spells::RolledOverTick(1500, shareTicks, earlier, refreshed, refreshed), int32(1866));

    // In the same update the earlier Ignite first carries 600 / 3 = 200 a tick, so the later one ticks for
    // (2 x 1500 + 3 x 200) / 3 = 1200, what the later crit alone gives on the 600 left.
    int32 const munched = Mage::IgniteMunchedTick(leftover, refreshed);
    CHECK_EQ(munched, int32(200));
    CHECK_EQ(Spells::RolledOverTick(1500, shareTicks, munched, refreshed, refreshed), int32(1200));
    CHECK_EQ(Spells::RolledOverTick(1500, shareTicks, 600, 1, refreshed), int32(1200));

    // A leftover its ticks do not divide drops the remainder: 601 over three is 200. No tick left carries nothing.
    CHECK_EQ(Mage::IgniteMunchedTick(601, refreshed), int32(200));
    CHECK_EQ(Mage::IgniteMunchedTick(600, 0), int32(0));
}

namespace
{
    // The ids of the Clearcasting test's own rows: two modifiers on Clearcasting's class mask, spells of the client's
    // kinds that no 4.3.4 id stands for, and the first of its class options and power rows.
    const uint32 kRaisingCostModifier = 94520;
    const uint32 kCastTimeModifier = 94521;
    const uint32 kFlatCostSpell = 94522;
    const uint32 kGenericCostSpell = 94523;
    const uint32 kPriestCostSpell = 94524;
    const uint32 kRageCostSpell = 94525;
    const uint32 kFirstClearcastingOptions = 94530;
    const uint32 kFirstClearcastingPower = 94545;

    // A spell the Clearcasting test casts: its class options (family 0: none), its power and mana cost, and whether
    // Clearcasting makes it cheaper.
    struct ClearcastingEvent
    {
        uint32 spellId;
        uint32 family;
        uint32 maskA;
        uint32 maskB;
        uint32 maskC;
        uint32 powerType;
        uint32 manaCost;
        uint32 manaCostPct;
        bool reduced;
    };

    // As the client's data holds them: Fireball (133), Pyroblast (11366) and Flame Orb (82731), each a percent of base
    // mana; a mage spell of a flat mana cost; Pyroblast! (92315), the free Flamestrike (88148) and Blizzard's damage
    // (42208), none with a cost; Mage Ward (543), outside Clearcasting's mask; a spell with no class options and a
    // cost, as an item's spell is; a priest spell and a mage spell of a rage cost, on Fireball's mask bit.
    ClearcastingEvent const CLEARCASTING_EVENTS[] =
    {
        { 133, SPELLFAMILY_MAGE, 0x00000001, 0, 0x00000008, POWER_MANA, 0, 9, true },
        { 11366, SPELLFAMILY_MAGE, 0x00400000, 0, 0x00001008, POWER_MANA, 0, 17, true },
        { 82731, SPELLFAMILY_MAGE, 0, 0, 0x00000880, POWER_MANA, 0, 6, true },
        { kFlatCostSpell, SPELLFAMILY_MAGE, 0x00000001, 0, 0, POWER_MANA, 100, 0, true },
        { 92315, SPELLFAMILY_MAGE, 0x00400000, 0, 0x00000008, POWER_MANA, 0, 0, false },
        { 88148, SPELLFAMILY_MAGE, 0x00000004, 0, 0, POWER_MANA, 0, 0, false },
        { 42208, SPELLFAMILY_MAGE, 0x00080080, 0, 0, POWER_MANA, 0, 0, false },
        { 543, SPELLFAMILY_MAGE, 0x00000008, 0, 0x00000008, POWER_MANA, 0, 16, false },
        { kGenericCostSpell, 0, 0, 0, 0, POWER_MANA, 0, 5, false },
        { kPriestCostSpell, SPELLFAMILY_PRIEST, 0x00000001, 0, 0, POWER_MANA, 0, 9, false },
        { kRageCostSpell, SPELLFAMILY_MAGE, 0x00000001, 0, 0, POWER_RAGE, 100, 0, false },
    };
    std::size_t const CLEARCASTING_EVENT_COUNT = sizeof(CLEARCASTING_EVENTS) / sizeof(CLEARCASTING_EVENTS[0]);

    ServerSpell s_clearcasting;
    ServerSpell s_raisingCostModifier;
    ServerSpell s_castTimeModifier;
    ServerSpell s_clearcastingEvents[CLEARCASTING_EVENT_COUNT];
    SpellClassOptionsEntry s_clearcastingOptions[CLEARCASTING_EVENT_COUNT + 1];
    SpellPowerEntry s_clearcastingPower[CLEARCASTING_EVENT_COUNT];

    // `spell` as a mage spell whose effect 0 is a percent modifier of `op` by `amount` on Clearcasting's class mask
    // (A 0x20C212F7, B 0x00029040, C 0x00000800), its effect put into the effect store.
    void StoreClearcastingModifier(ServerSpell& spell, uint32 spellId, int32 op, int32 amount)
    {
        spell.Entry().ID = spellId;
        spell.Entry().ClassOptionsID = kFirstClearcastingOptions;
        spell.effect.SpellID = spellId;
        spell.effect.EffectIndex = EFFECT_INDEX_0;
        spell.effect.Effect = SPELL_EFFECT_APPLY_AURA;
        spell.effect.EffectAura = SPELL_AURA_ADD_PCT_MODIFIER;
        spell.effect.EffectMiscValue_0 = op;
        spell.effect.EffectBasePoints = amount;
        spell.effect.EffectSpellClassMask = ClassFamilyMask(0x20C212F7 | (uint64(0x00029040) << 32), 0x00000800);
        AddSpellEffectEntry(&spell.effect);
    }

    // Clearcasting (12536, a cost modifier of -1000), a cost modifier of +150 and a cast-time modifier of -100 on its
    // mask, and the spells of CLEARCASTING_EVENTS with their class options and power rows, put into the stores once.
    void StoreClearcastingSpells()
    {
        static bool stored = false;
        if (stored)
        {
            return;
        }
        stored = true;
        s_clearcastingOptions[0].SpellClassMask = ClassFamilyMask(uint64(0x00000002) << 32, 0x00000008);
        s_clearcastingOptions[0].SpellClassSet = SPELLFAMILY_MAGE;
        sSpellClassOptionsStore.SetEntry(kFirstClearcastingOptions, &s_clearcastingOptions[0]);
        StoreClearcastingModifier(s_clearcasting, 12536, SPELLMOD_COST, -1000);
        StoreClearcastingModifier(s_raisingCostModifier, kRaisingCostModifier, SPELLMOD_COST, 150);
        StoreClearcastingModifier(s_castTimeModifier, kCastTimeModifier, SPELLMOD_CASTING_TIME, -100);
        for (std::size_t i = 0; i < CLEARCASTING_EVENT_COUNT; ++i)
        {
            ClearcastingEvent const& event = CLEARCASTING_EVENTS[i];
            SpellEntry& row = s_clearcastingEvents[i].Entry();
            row.ID = event.spellId;
            row.PowerType = event.powerType;
            if (event.family)
            {
                uint32 const optionsId = kFirstClearcastingOptions + 1 + uint32(i);
                s_clearcastingOptions[i + 1].SpellClassMask =
                    ClassFamilyMask(event.maskA | (uint64(event.maskB) << 32), event.maskC);
                s_clearcastingOptions[i + 1].SpellClassSet = event.family;
                sSpellClassOptionsStore.SetEntry(optionsId, &s_clearcastingOptions[i + 1]);
                row.ClassOptionsID = optionsId;
            }
            if (event.manaCost || event.manaCostPct)
            {
                uint32 const powerId = kFirstClearcastingPower + uint32(i);
                s_clearcastingPower[i] = SpellPowerEntry();
                s_clearcastingPower[i].ManaCost = event.manaCost;
                s_clearcastingPower[i].PowerCost = event.manaCostPct;
                sSpellPowerStore.SetEntry(powerId, &s_clearcastingPower[i]);
                row.PowerDisplayID = powerId;
            }
        }
    }
}

TEST(MageScripts_ClearcastingIsTakenOnlyByASpellItMakesCheaper)
{
    StoreClearcastingSpells();
    SpellHandlerRegistry registry;
    RegisterMageScripts(registry);
    CheckRowsInTheGame<CheckProcHook>(registry, CLEARCASTING_IDS);

    BareCreature owner;
    HolderStorage storage;
    SpellAuraHolder& holder = EmptyHolder(storage);
    SpellEntry const& clearcasting = s_clearcasting.Entry();

    // A spell of the modifier's family and class mask with a mana cost, flat or a percent of base mana, is made
    // cheaper, and the check-proc row lets its cast's end go on to the other checks. Every other spell's is refused,
    // so the charge stays: Pyroblast!, the free Flamestrike and Blizzard's damage cost nothing, Mage Ward is outside
    // the mask, and a spell with no class options, a priest spell and a rage cost are not Clearcasting's.
    for (std::size_t i = 0; i < CLEARCASTING_EVENT_COUNT; ++i)
    {
        ClearcastingEvent const& event = CLEARCASTING_EVENTS[i];
        SpellEntry const* const spell = &s_clearcastingEvents[i].Entry();
        CHECK(IsCostReducedByModifier(&clearcasting, EFFECT_INDEX_0, spell) == event.reduced);
        CheckProcContext ctx(owner, NULL, clearcasting, AuraHandle(holder), spell, 0x10000, PROC_EX_CAST_END, 0);
        SpellHandlerOutcome<bool> const outcome = registry.Dispatch<CheckProcHook>(12536, ctx);
        CHECK(outcome.IsContinue() == event.reduced);
        CHECK(outcome.IsReturn() == !event.reduced);
        CHECK(!outcome.IsReturn() || !outcome.GetValue());
    }

    // An event with no spell is refused.
    CheckProcContext melee(owner, NULL, clearcasting, AuraHandle(holder), NULL, 0x10000, PROC_EX_CAST_END, 0);
    SpellHandlerOutcome<bool> const noSpell = registry.Dispatch<CheckProcHook>(12536, melee);
    CHECK(noSpell.IsReturn());
    CHECK(!noSpell.GetValue());

    // Only a cost modifier below 0 makes a spell cheaper: a cost modifier above 0, a cast-time modifier and an effect
    // the modifier does not have make even Fireball no cheaper.
    SpellEntry const* const fireball = &s_clearcastingEvents[0].Entry();
    CHECK(!IsCostReducedByModifier(&s_raisingCostModifier.Entry(), EFFECT_INDEX_0, fireball));
    CHECK(!IsCostReducedByModifier(&s_castTimeModifier.Entry(), EFFECT_INDEX_0, fireball));
    CHECK(!IsCostReducedByModifier(&clearcasting, EFFECT_INDEX_1, fireball));
}

TEST(MageScripts_BlinkKeepsOneTarget)
{
    SpellHandlerRegistry registry;
    RegisterMageScripts(registry);
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    for (uint32 spellId : BLINK_IDS)
    {
        CHECK(registry.Find<SelectTargetsHook>(spellId) != NULL);
        CHECK(game.Find<SelectTargetsHook>(spellId) == registry.Find<SelectTargetsHook>(spellId));
    }
    // The target hook also holds Piercing Chill's chill (83154).
    CHECK_EQ(game.CountAt(SelectTargetsHook::Key), sizeof(BLINK_IDS) / sizeof(uint32) +
                                                   sizeof(ORB_TARGET_IDS) / sizeof(uint32) +
                                                   sizeof(RING_FREEZE_IDS) / sizeof(uint32) + 1);

    // Blink sets the count the cut after the hook keeps to one and continues, so the cut runs; the list is untouched.
    BareCreature caster;
    BareCreature first;
    BareCreature second;
    SpellRowStorage row;
    std::list<Unit*> targets;
    targets.push_back(&first);
    targets.push_back(&second);
    uint32 maxTargets = 0;
    SelectTargetsContext ctx(caster, SpellRow(row, 38194), EFFECT_INDEX_0, TARGET_ALL_ENEMY_IN_AREA, targets, maxTargets);
    CHECK(registry.Dispatch<SelectTargetsHook>(38194, ctx).IsContinue());
    CHECK_EQ(maxTargets, uint32(1));
    CHECK_EQ(targets.size(), std::size_t(2));
}

TEST(MageScripts_LivingBombRunsOnTheDummyAuraHook)
{
    SpellHandlerRegistry registry;
    RegisterMageScripts(registry);
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    // Its explosion is the dummy aura script's; its holder's apply and removal are Pyromaniac's, as for every DoT
    // Pyromaniac counts.
    for (uint32 spellId : LIVING_BOMB_IDS)
    {
        CHECK(registry.Find<DummyAuraHook>(spellId) != NULL);
        CHECK(game.Find<DummyAuraHook>(spellId) == registry.Find<DummyAuraHook>(spellId));
        CHECK(registry.Find<AuraApplyHook>(spellId) == registry.Find<AuraApplyHook>(PYROMANIAC_DOT_IDS[0]));
        CHECK(registry.Find<AuraRemoveHook>(spellId) == registry.Find<AuraRemoveHook>(PYROMANIAC_DOT_IDS[0]));
        CHECK(game.Find<AuraApplyHook>(spellId) == registry.Find<AuraApplyHook>(spellId));
        CHECK(game.Find<AuraRemoveHook>(spellId) == registry.Find<AuraRemoveHook>(spellId));
    }
    CHECK_EQ(game.CountAt(DummyAuraHook::Key), sizeof(LIVING_BOMB_IDS) / sizeof(uint32));
    CHECK_EQ(game.CountAt(AuraRemoveHook::Key), sizeof(HOT_STREAK_BUFF_IDS) / sizeof(uint32) +
                                                sizeof(MAGE_WARD_IDS) / sizeof(uint32) +
                                                sizeof(PYROMANIAC_DOT_IDS) / sizeof(uint32) +
                                                sizeof(PYROMANIAC_IDS) / sizeof(uint32) +
                                                sizeof(ARCANE_MISSILES_BUFF_IDS) / sizeof(uint32) +
                                                sizeof(POLYMORPH_IDS) / sizeof(uint32) +
                                                sizeof(MANA_SHIELD_BREAK_IDS) / sizeof(uint32) +
                                                sizeof(RING_FREEZE_IDS) / sizeof(uint32));

    // At the apply the script goes on to the handler's other steps. At an un-apply for any reason but the aura running
    // out it casts nothing and ends the handler. The amount, the explosion's spell, stays as it is.
    BareCreature target;
    SpellRowStorage row;
    HolderStorage storage;
    SpellAuraHolder& holder = EmptyHolder(storage);
    int32 amount = 44461;
    DummyAuraContext applied(target, NULL, SpellRow(row, 44457), EFFECT_INDEX_1, AuraHandle(holder), true,
                             AURA_REMOVE_BY_DEFAULT, amount, NULL);
    CHECK(registry.Dispatch<DummyAuraHook>(44457, applied).IsContinue());
    AuraRemoveMode const silent[] =
    {
        AURA_REMOVE_BY_DEFAULT, AURA_REMOVE_BY_STACK, AURA_REMOVE_BY_CANCEL, AURA_REMOVE_BY_DISPEL, AURA_REMOVE_BY_DEATH,
        AURA_REMOVE_BY_DELETE, AURA_REMOVE_BY_SHIELD_BREAK, AURA_REMOVE_BY_TRACKING, AURA_REMOVE_BY_TARGET_LIMIT,
        AURA_REMOVE_BY_DAMAGE,
    };
    for (AuraRemoveMode mode : silent)
    {
        DummyAuraContext removed(target, NULL, SpellRow(row, 44457), EFFECT_INDEX_1, AuraHandle(holder), false, mode,
                                 amount, NULL);
        CHECK(registry.Dispatch<DummyAuraHook>(44457, removed).IsReturn());
    }
    CHECK_EQ(amount, int32(44461));
}

TEST(MageScripts_LivingBombExplodesOnlyWhenItsAuraRunsOut)
{
    // The aura running out explodes the bomb; a dispel, its target's death, a plain removal, a re-application, a
    // cancel, a broken shield, a tracked aura's replacement, the caster's fourth bomb, a delete and a break by damage
    // do not.
    CHECK(Mage::LivingBombExplodes(AURA_REMOVE_BY_EXPIRE));
    AuraRemoveMode const silent[] =
    {
        AURA_REMOVE_BY_DISPEL, AURA_REMOVE_BY_DEATH, AURA_REMOVE_BY_DEFAULT, AURA_REMOVE_BY_STACK, AURA_REMOVE_BY_CANCEL,
        AURA_REMOVE_BY_SHIELD_BREAK, AURA_REMOVE_BY_TRACKING, AURA_REMOVE_BY_TARGET_LIMIT, AURA_REMOVE_BY_DELETE,
        AURA_REMOVE_BY_DAMAGE,
    };
    for (AuraRemoveMode mode : silent)
    {
        CHECK(!Mage::LivingBombExplodes(mode));
    }
}

TEST(MageScripts_HotStreakRollsOnTheSecondCritInARow)
{
    // crit, crit: the second rolls and the count starts again.
    int32 crits = 0;
    CHECK(!Mage::CountCritInARow(crits, true));
    CHECK_EQ(crits, int32(1));
    CHECK(Mage::CountCritInARow(crits, true));
    CHECK_EQ(crits, int32(0));

    // crit, hit, crit: the hit starts the count again, so the third does not roll.
    CHECK(!Mage::CountCritInARow(crits, true));
    CHECK(!Mage::CountCritInARow(crits, false));
    CHECK_EQ(crits, int32(0));
    CHECK(!Mage::CountCritInARow(crits, true));

    // crit, crit, crit, crit: the second and the fourth roll.
    CHECK(Mage::CountCritInARow(crits, true));
    CHECK(!Mage::CountCritInARow(crits, true));
    CHECK(Mage::CountCritInARow(crits, true));
    CHECK_EQ(crits, int32(0));
}

TEST(MageScripts_HotStreakChanceFallsWithTheSheetCrit)
{
    // 95 less 2.73 per point of sheet spell crit: 25% gives 26.75, 30% gives 13.1, and about 34.8% reaches 0; a sheet
    // crit of 100 or more leaves only Improved Hot Streak's roll, and a sheet crit below 0 counts as 0.
    struct Vector
    {
        float sheetCrit;
        float chance;
    };
    Vector const vectors[] =
    {
        { 0.0f, 95.0f }, { 25.0f, 26.75f }, { 30.0f, 13.1f }, { 34.8f, 0.0f }, { 100.0f, 0.0f }, { -100.0f, 95.0f },
    };
    for (Vector const& v : vectors)
    {
        CHECK(std::fabs(Mage::HotStreakChance(v.sheetCrit) - v.chance) < 0.01f);
    }
    CHECK(Mage::HotStreakChance(34.8f) >= 0.0f);
}

TEST(MageScripts_HotStreakStepRollsTheBaseChanceFirstAndImprovedOnTheSecondCrit)
{
    // A hit that does not crit clears the count and gives nothing, whatever the rolls.
    int32 crits = 0;
    CHECK(!Mage::HotStreakStep(crits, false, true, true));
    CHECK_EQ(crits, int32(0));
    crits = 1;
    CHECK(!Mage::HotStreakStep(crits, false, false, false));
    CHECK_EQ(crits, int32(0));

    // A crit whose base roll passed gives Hot Streak and clears the count, first in a row or second.
    crits = 0;
    CHECK(Mage::HotStreakStep(crits, true, true, false));
    CHECK_EQ(crits, int32(0));
    crits = 1;
    CHECK(Mage::HotStreakStep(crits, true, true, false));
    CHECK_EQ(crits, int32(0));

    // Otherwise the first crit counts, whatever Improved Hot Streak's roll says.
    crits = 0;
    CHECK(!Mage::HotStreakStep(crits, true, false, false));
    CHECK_EQ(crits, int32(1));
    crits = 0;
    CHECK(!Mage::HotStreakStep(crits, true, false, true));
    CHECK_EQ(crits, int32(1));

    // The second crit in a row clears the count whether or not Improved Hot Streak's roll passed, and gives Hot Streak
    // when it did.
    crits = 1;
    CHECK(Mage::HotStreakStep(crits, true, false, true));
    CHECK_EQ(crits, int32(0));
    crits = 1;
    CHECK(!Mage::HotStreakStep(crits, true, false, false));
    CHECK_EQ(crits, int32(0));
}

TEST(MageScripts_PushingTheLimitComesOnlyWhenHotStreakIsUsedUpWithTheTier10Bonus)
{
    // The decision: the buff used up (the default mode, in which casting Pyroblast! removes it) by a caster holding
    // Item - Mage T10 2P Bonus (70752); its running out, a dispel, a cancel, a re-application or a death gives nothing.
    CHECK(Mage::PushingTheLimitOnRemove(AURA_REMOVE_BY_DEFAULT, true));
    CHECK(!Mage::PushingTheLimitOnRemove(AURA_REMOVE_BY_DEFAULT, false));
    AuraRemoveMode const silent[] =
    {
        AURA_REMOVE_BY_EXPIRE, AURA_REMOVE_BY_DISPEL, AURA_REMOVE_BY_CANCEL, AURA_REMOVE_BY_STACK, AURA_REMOVE_BY_DEATH,
    };
    for (AuraRemoveMode mode : silent)
    {
        CHECK(!Mage::PushingTheLimitOnRemove(mode, true));
    }

    // The script stands for 48108 at the aura-remove hook, in the game's table too, and ends the removal's boosts (the
    // Pushing the Limit row of the mage boost site is gone). With no caster, a caster without the bonus, or a caster
    // with it and a removal other than the default, it casts nothing.
    SpellHandlerRegistry registry;
    RegisterMageScripts(registry);
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    for (uint32 spellId : HOT_STREAK_BUFF_IDS)
    {
        CHECK(registry.Find<AuraRemoveHook>(spellId) != NULL);
        CHECK(game.Find<AuraRemoveHook>(spellId) == registry.Find<AuraRemoveHook>(spellId));
        CHECK(game.Find<AuraBoostMageSite>(spellId) == NULL);
    }
    BareCreature target;
    BareCreature caster;
    SpellRowStorage row;
    HolderStorage storage;
    SpellAuraHolder& holder = EmptyHolder(storage);
    AuraRemoveContext noCaster(target, NULL, SpellRow(row, 48108), AuraHandle(holder), AURA_REMOVE_BY_DEFAULT);
    CHECK(registry.Dispatch<AuraRemoveHook>(48108, noCaster).IsReturn());
    AuraRemoveContext noBonus(target, &caster, SpellRow(row, 48108), AuraHandle(holder), AURA_REMOVE_BY_DEFAULT);
    CHECK(registry.Dispatch<AuraRemoveHook>(48108, noBonus).IsReturn());
    HolderStorage bonusStorage;
    caster.HoldAura(70752, EmptyHolder(bonusStorage));
    for (AuraRemoveMode mode : silent)
    {
        AuraRemoveContext ranOut(target, &caster, SpellRow(row, 48108), AuraHandle(holder), mode);
        CHECK(registry.Dispatch<AuraRemoveHook>(48108, ranOut).IsReturn());
    }
}

TEST(MageScripts_AMageWardThatDamageUsesUpBringsBlazingSpeedOnlyWithMoltenShields)
{
    // The decision: the ward's last point used up by damage (the shield-break mode) with a caster who knows Molten
    // Shields; a break for a caster without the talent gives nothing, and so do its running out, a cancel, a dispel, a
    // plain removal, a re-application, a death, a delete, a tracked aura's replacement, a target limit and a break of a
    // crowd control by damage.
    CHECK(Mage::MageWardTriggersBlazingSpeed(AURA_REMOVE_BY_SHIELD_BREAK, true));
    CHECK(!Mage::MageWardTriggersBlazingSpeed(AURA_REMOVE_BY_SHIELD_BREAK, false));
    AuraRemoveMode const silent[] =
    {
        AURA_REMOVE_BY_EXPIRE, AURA_REMOVE_BY_CANCEL, AURA_REMOVE_BY_DISPEL, AURA_REMOVE_BY_DEFAULT,
        AURA_REMOVE_BY_STACK, AURA_REMOVE_BY_DEATH, AURA_REMOVE_BY_DELETE, AURA_REMOVE_BY_TRACKING,
        AURA_REMOVE_BY_TARGET_LIMIT, AURA_REMOVE_BY_DAMAGE,
    };
    for (AuraRemoveMode mode : silent)
    {
        CHECK(!Mage::MageWardTriggersBlazingSpeed(mode, true));
        CHECK(!Mage::MageWardTriggersBlazingSpeed(mode, false));
    }

    // The script stands for 543 at the aura-remove hook, in the game's table too, and lets the removal's boosts run
    // every time. A ward that ran out, a break with no caster (one the server cannot find) and a break whose caster
    // lacks Molten Shields (11094) cast nothing; a break whose caster holds 11094 goes on to the cast of Blazing Speed
    // (31643), which the test's spell store lacks, so nothing is cast.
    SpellHandlerRegistry registry;
    RegisterMageScripts(registry);
    CheckRowsInTheGame<AuraRemoveHook>(registry, MAGE_WARD_IDS);
    BareCreature mage;
    SpellRowStorage row;
    HolderStorage storage;
    SpellAuraHolder& holder = EmptyHolder(storage);
    AuraRemoveContext ranOut(mage, &mage, SpellRow(row, 543), AuraHandle(holder), AURA_REMOVE_BY_EXPIRE);
    CHECK(registry.Dispatch<AuraRemoveHook>(543, ranOut).IsContinue());
    AuraRemoveContext noCaster(mage, NULL, SpellRow(row, 543), AuraHandle(holder), AURA_REMOVE_BY_SHIELD_BREAK);
    CHECK(registry.Dispatch<AuraRemoveHook>(543, noCaster).IsContinue());
    AuraRemoveContext noTalent(mage, &mage, SpellRow(row, 543), AuraHandle(holder), AURA_REMOVE_BY_SHIELD_BREAK);
    CHECK(registry.Dispatch<AuraRemoveHook>(543, noTalent).IsContinue());
    HolderStorage talentStorage;
    mage.HoldAura(11094, EmptyHolder(talentStorage));
    CHECK(mage.HasAura(11094));
    AuraRemoveContext broken(mage, &mage, SpellRow(row, 543), AuraHandle(holder), AURA_REMOVE_BY_SHIELD_BREAK);
    CHECK(registry.Dispatch<AuraRemoveHook>(543, broken).IsContinue());
    CHECK(sSpellStore.LookupEntry(31643) == NULL);
    CHECK(!mage.HasAura(31643));
}

TEST(MageScripts_IncantersAbsorptionKnocksBackOnlyWhenDamageBreaksAManaShield)
{
    // The leaf: a shield that damage used up, of a mage who knows the talent; no other removal, and never without it.
    CHECK(Mage::IncantersAbsorptionKnocksBack(AURA_REMOVE_BY_SHIELD_BREAK, true));
    CHECK(!Mage::IncantersAbsorptionKnocksBack(AURA_REMOVE_BY_SHIELD_BREAK, false));
    AuraRemoveMode const silent[] =
    {
        AURA_REMOVE_BY_EXPIRE, AURA_REMOVE_BY_CANCEL, AURA_REMOVE_BY_DISPEL, AURA_REMOVE_BY_DEFAULT,
        AURA_REMOVE_BY_STACK, AURA_REMOVE_BY_DEATH, AURA_REMOVE_BY_DELETE, AURA_REMOVE_BY_TRACKING,
        AURA_REMOVE_BY_TARGET_LIMIT, AURA_REMOVE_BY_DAMAGE,
    };
    for (AuraRemoveMode mode : silent)
    {
        CHECK(!Mage::IncantersAbsorptionKnocksBack(mode, true));
        CHECK(!Mage::IncantersAbsorptionKnocksBack(mode, false));
    }

    // The script stands for Mana Shield at the aura-remove hook, in the game's table too, beside its proc script, and
    // lets the removal's boosts run every time. A shield cancelled or run out and a break of a mage who knows neither
    // rank cast nothing; a break of a mage who holds rank 2 (44395) goes on to the cast of 86261, which the test's
    // spell store lacks, so nothing is cast.
    SpellHandlerRegistry registry;
    RegisterMageScripts(registry);
    CheckRowsInTheGame<AuraRemoveHook>(registry, MANA_SHIELD_BREAK_IDS);
    CHECK(registry.Find<OnProcHook>(1463) != NULL);
    BareCreature mage;
    SpellRowStorage row;
    HolderStorage storage;
    SpellAuraHolder& holder = EmptyHolder(storage);
    AuraRemoveContext cancelled(mage, &mage, SpellRow(row, 1463), AuraHandle(holder), AURA_REMOVE_BY_CANCEL);
    CHECK(registry.Dispatch<AuraRemoveHook>(1463, cancelled).IsContinue());
    AuraRemoveContext ranOut(mage, &mage, SpellRow(row, 1463), AuraHandle(holder), AURA_REMOVE_BY_EXPIRE);
    CHECK(registry.Dispatch<AuraRemoveHook>(1463, ranOut).IsContinue());
    AuraRemoveContext noTalent(mage, &mage, SpellRow(row, 1463), AuraHandle(holder), AURA_REMOVE_BY_SHIELD_BREAK);
    CHECK(registry.Dispatch<AuraRemoveHook>(1463, noTalent).IsContinue());
    HolderStorage talentStorage;
    mage.HoldAura(44395, EmptyHolder(talentStorage));
    CHECK(mage.HasAura(44395));
    AuraRemoveContext broken(mage, &mage, SpellRow(row, 1463), AuraHandle(holder), AURA_REMOVE_BY_SHIELD_BREAK);
    CHECK(registry.Dispatch<AuraRemoveHook>(1463, broken).IsContinue());
    CHECK(sSpellStore.LookupEntry(86261) == NULL);
    CHECK(!mage.HasAura(86261));
}

TEST(MageScripts_ImprovedManaGemGivesTheRanksPercentOfMaximumMana)
{
    // The leaf: the rank's percent of the maximum mana, truncated; nothing for a percent of 0 or less.
    CHECK_EQ(Mage::ImprovedManaGemAmount(100000, 2), int32(2000));
    CHECK_EQ(Mage::ImprovedManaGemAmount(100000, 1), int32(1000));
    CHECK_EQ(Mage::ImprovedManaGemAmount(99999, 2), int32(1999));
    CHECK_EQ(Mage::ImprovedManaGemAmount(49, 2), int32(0));
    CHECK_EQ(Mage::ImprovedManaGemAmount(0, 2), int32(0));
    CHECK_EQ(Mage::ImprovedManaGemAmount(100000, 0), int32(0));
    CHECK_EQ(Mage::ImprovedManaGemAmount(100000, -2), int32(0));
    CHECK_EQ(Mage::ImprovedManaGemAmount(4294967295u, 2), int32(85899345));

    // The script stands for Replenish Mana at the after-cast hook, in the game's table too; the talent's ranks, its
    // buff and Conjure Mana Gem hold no after-cast row. A Mana Gem's cast by a caster without the talent, and by one
    // whose rank's holder holds no aura, casts nothing. A smoke case: Energy Cloak's cast and a cast from no item of
    // that caster cast nothing too, but the rank's empty holder gives a percent of 0 and the test's store holds no
    // 83098, so these cases cannot tell the item check from none; the next test's leaf and scenario 1106 pin it.
    SpellHandlerRegistry registry;
    RegisterMageScripts(registry);
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    CheckRowsInTheGame<AfterCastHook>(registry, REPLENISH_MANA_IDS);
    for (uint32 spellId : { 31584u, 31585u, 83098u, 759u })
    {
        CHECK(game.Find<AfterCastHook>(spellId) == NULL);
    }
    BareCreature caster;
    SpellRowStorage row;
    SpellEntry const& replenishMana = SpellRow(row, 5405);
    AfterCastContext noTalent(caster, replenishMana, 0, NULL, false, 0.0f, 0.0f, 0.0f, 36799);
    CHECK(registry.Dispatch<AfterCastHook>(5405, noTalent).IsContinue());
    HolderStorage talentStorage;
    caster.HoldAura(31585, EmptyHolder(talentStorage));
    AfterCastContext emptyRank(caster, replenishMana, 0, NULL, false, 0.0f, 0.0f, 0.0f, 36799);
    CHECK(registry.Dispatch<AfterCastHook>(5405, emptyRank).IsContinue());
    for (uint32 itemEntry : { 9397u, 0u })
    {
        AfterCastContext notAGem(caster, replenishMana, 0, NULL, false, 0.0f, 0.0f, 0.0f, itemEntry);
        CHECK(registry.Dispatch<AfterCastHook>(5405, notAGem).IsContinue());
    }
    CHECK(!caster.HasAura(83098));
    CHECK_EQ(caster.GetSpellAuraHolderMap().size(), std::size_t(1));
}

TEST(MageScripts_ImprovedManaGemComesFromTheManaGemsAlone)
{
    // "Mana gained from your Mana Gem": the Mana Gem and the six older conjured gems of its cooldown category, the
    // items of the 15595 client that cast Replenish Mana (5405) besides Energy Cloak.
    for (uint32 itemEntry : { 36799u, 5514u, 5513u, 8007u, 8008u, 22044u, 33312u })
    {
        CHECK(Mage::IsManaGem(itemEntry));
    }

    // Energy Cloak, a rare cloak whose use casts 5405 too, a cast from no item, a Healthstone (5512) and a Conjured
    // Mana Strudel (43523), a conjured item that restores mana with another spell.
    for (uint32 itemEntry : { 9397u, 0u, 5512u, 43523u })
    {
        CHECK(!Mage::IsManaGem(itemEntry));
    }
}

TEST(MageScripts_PyromaniacCountsTheMagesFireDamageOverTime)
{
    // The holders of the talent's text, "Fire damage over time from your spells": Flamestrike's ground effect,
    // Pyroblast, Ignite, Living Bomb, Frostfire Bolt, Combustion, the free Flamestrike's ground effect and Pyroblast!.
    for (uint32 spellId : PYROMANIAC_DOT_IDS)
    {
        CHECK(Mage::PyromaniacCounts(spellId));
    }

    // Direct hits and the haste itself count for nothing: Fireball, Fire Blast, Combustion's own cast, Living Bomb's
    // explosion, Flame Orb's hit, Fire Power's explosion and Pyromaniac's haste.
    for (uint32 spellId : { 133u, 2136u, 11129u, 44461u, 82739u, 83619u, 83582u })
    {
        CHECK(!Mage::PyromaniacCounts(spellId));
    }
}

TEST(MageScripts_PyromaniacCountsAFrostfireBoltOnlyWhileItDealsDamageOverTime)
{
    // Frostfire Bolt's holder makes its enemy burn with its periodic damage (the glyphed bolt) and not without it (the
    // unglyphed bolt, which only slows).
    CHECK(Mage::PyromaniacBurns(44614, true));
    CHECK(!Mage::PyromaniacBurns(44614, false));

    // Every other DoT the talent counts makes its enemy burn whatever the flag, and a spell it does not count never.
    for (uint32 spellId : PYROMANIAC_DOT_IDS)
    {
        if (spellId != 44614)
        {
            CHECK(Mage::PyromaniacBurns(spellId, true));
            CHECK(Mage::PyromaniacBurns(spellId, false));
        }
    }
    for (uint32 spellId : { 133u, 2136u, 44461u, 83582u })
    {
        CHECK(!Mage::PyromaniacBurns(spellId, true));
        CHECK(!Mage::PyromaniacBurns(spellId, false));
    }
}

TEST(MageScripts_PyromaniacsHasteIsUpFromThreeBurningEnemies)
{
    for (uint32 targets : { 0u, 1u, 2u })
    {
        CHECK(!Mage::PyromaniacActive(targets));
    }
    for (uint32 targets : { 3u, 4u, 9u })
    {
        CHECK(Mage::PyromaniacActive(targets));
    }
}

TEST(MageScripts_PyromaniacsHasteIsTheRanksEffectValue)
{
    // Rank 1 gives its effect 0 value of 5, rank 2 its 10; the haste spell (83582) and no spell are no rank.
    AddPyromaniacRanks();
    CHECK_EQ(Mage::PyromaniacHaste(34293), int32(5));
    CHECK_EQ(Mage::PyromaniacHaste(34295), int32(10));
    CHECK_EQ(Mage::PyromaniacHaste(83582), int32(0));
    CHECK_EQ(Mage::PyromaniacHaste(0), int32(0));
}

TEST(MageScripts_InvisibilityFadeTakesATenthPerTick)
{
    // Only the fade (66) takes threat; the state it ends in (32612), its trigger id and other spells take none.
    CHECK_EQ(Mage::InvisibilityThreatPercent(66), int32(-10));
    CHECK_EQ(Mage::InvisibilityThreatPercent(32612), int32(0));
    CHECK_EQ(Mage::InvisibilityThreatPercent(35009), int32(0));
    CHECK_EQ(Mage::InvisibilityThreatPercent(2136), int32(0));
    CHECK_EQ(Mage::InvisibilityThreatPercent(0), int32(0));
}

TEST(MageScripts_PyromaniacFindsNoBurningEnemyForAMageNoEnemyHates)
{
    // No unit's threat list holds a bare creature, so its walk finds no enemy.
    BareCreature mage;
    CHECK_EQ(Mage::PyromaniacTargets(mage), uint32(0));
}

TEST(MageScripts_PyromaniacRunsOnTheAuraApplyAndRemoveHooks)
{
    SpellHandlerRegistry registry;
    RegisterMageScripts(registry);
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();

    // At the remove hook the eight DoTs share one body; at the apply hook the seven but Frostfire Bolt share it, and
    // Frostfire Bolt's apply is a row of its own, whose body takes one effect off the holder and then counts as theirs
    // does. The two ranks share one body at each hook. The same in the game's table.
    SpellHandler<AuraApplyHook>::Function const dotApply = registry.Find<AuraApplyHook>(PYROMANIAC_DOT_IDS[0]);
    SpellHandler<AuraRemoveHook>::Function const dotRemove = registry.Find<AuraRemoveHook>(PYROMANIAC_DOT_IDS[0]);
    SpellHandler<AuraApplyHook>::Function const frostfireApply = registry.Find<AuraApplyHook>(44614);
    SpellHandler<AuraApplyHook>::Function const talentApply = registry.Find<AuraApplyHook>(PYROMANIAC_IDS[0]);
    SpellHandler<AuraRemoveHook>::Function const talentRemove = registry.Find<AuraRemoveHook>(PYROMANIAC_IDS[0]);
    CHECK(dotApply != NULL);
    CHECK(dotRemove != NULL);
    CHECK(frostfireApply != NULL);
    CHECK(talentApply != NULL);
    CHECK(talentRemove != NULL);
    CHECK(talentApply != dotApply);
    CHECK(talentApply != frostfireApply);
    CHECK(frostfireApply != dotApply);
    CHECK(talentRemove != dotRemove);
    for (uint32 spellId : PYROMANIAC_DOT_IDS)
    {
        CHECK(registry.Find<AuraApplyHook>(spellId) == (spellId == 44614 ? frostfireApply : dotApply));
        CHECK(registry.Find<AuraRemoveHook>(spellId) == dotRemove);
        CHECK(game.Find<AuraApplyHook>(spellId) == registry.Find<AuraApplyHook>(spellId));
        CHECK(game.Find<AuraRemoveHook>(spellId) == dotRemove);
    }
    for (uint32 spellId : PYROMANIAC_IDS)
    {
        CHECK(registry.Find<AuraApplyHook>(spellId) == talentApply);
        CHECK(registry.Find<AuraRemoveHook>(spellId) == talentRemove);
        CHECK(game.Find<AuraApplyHook>(spellId) == talentApply);
        CHECK(game.Find<AuraRemoveHook>(spellId) == talentRemove);
    }
    CHECK_EQ(game.CountAt(AuraApplyHook::Key),
             sizeof(PYROMANIAC_DOT_IDS) / sizeof(uint32) + sizeof(PYROMANIAC_IDS) / sizeof(uint32) +
             sizeof(ARCANE_MISSILES_BUFF_IDS) / sizeof(uint32) + sizeof(ARCANE_MISSILES_IDS) / sizeof(uint32));

    // The DoTs' bodies count only for a caster they may touch: an apply with no caster or a caster that is no player,
    // the call before the add, and a removal by re-application or with no caster, change nothing and let the holder's
    // boosts run.
    BareCreature target;
    BareCreature caster;
    SpellRowStorage row;
    HolderStorage storage;
    SpellAuraHolder& holder = EmptyHolder(storage);
    AuraRemoveMode const modes[] =
    {
        AURA_REMOVE_BY_DEFAULT, AURA_REMOVE_BY_STACK, AURA_REMOVE_BY_DISPEL, AURA_REMOVE_BY_DEATH, AURA_REMOVE_BY_EXPIRE,
        AURA_REMOVE_BY_TARGET_LIMIT,
    };
    for (uint32 spellId : PYROMANIAC_DOT_IDS)
    {
        AuraApplyContext noCaster(target, NULL, SpellRow(row, spellId), AuraHandle(holder), true);
        CHECK(registry.Dispatch<AuraApplyHook>(spellId, noCaster).IsContinue());
        AuraApplyContext creatureCaster(target, &caster, SpellRow(row, spellId), AuraHandle(holder), true);
        CHECK(registry.Dispatch<AuraApplyHook>(spellId, creatureCaster).IsContinue());
        AuraApplyContext beforeAdd(target, &caster, SpellRow(row, spellId), AuraHandle(holder), false);
        CHECK(registry.Dispatch<AuraApplyHook>(spellId, beforeAdd).IsContinue());
        AuraRemoveContext refreshed(target, &caster, SpellRow(row, spellId), AuraHandle(holder), AURA_REMOVE_BY_STACK);
        CHECK(registry.Dispatch<AuraRemoveHook>(spellId, refreshed).IsContinue());
        for (AuraRemoveMode mode : modes)
        {
            AuraRemoveContext removed(target, NULL, SpellRow(row, spellId), AuraHandle(holder), mode);
            CHECK(registry.Dispatch<AuraRemoveHook>(spellId, removed).IsContinue());
        }
    }

    // The talent's bodies take the haste off the mage, which holds none here, at the add and not before it, and let
    // the holder's boosts run.
    BareCreature mage;
    for (uint32 spellId : PYROMANIAC_IDS)
    {
        AuraApplyContext learned(mage, &mage, SpellRow(row, spellId), AuraHandle(holder), true);
        CHECK(registry.Dispatch<AuraApplyHook>(spellId, learned).IsContinue());
        AuraApplyContext beforeAdd(mage, &mage, SpellRow(row, spellId), AuraHandle(holder), false);
        CHECK(registry.Dispatch<AuraApplyHook>(spellId, beforeAdd).IsContinue());
        AuraRemoveContext unlearned(mage, &mage, SpellRow(row, spellId), AuraHandle(holder), AURA_REMOVE_BY_DEFAULT);
        CHECK(registry.Dispatch<AuraRemoveHook>(spellId, unlearned).IsContinue());
        CHECK(!mage.HasAura(83582));
    }
}

TEST(MageScripts_CombustionRunsOnTheDummyEffectHook)
{
    SpellHandlerRegistry registry;
    RegisterMageScripts(registry);
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    for (uint32 spellId : COMBUSTION_IDS)
    {
        CHECK(registry.Find<DummyEffectHook>(spellId) != NULL);
        CHECK(game.Find<DummyEffectHook>(spellId) == registry.Find<DummyEffectHook>(spellId));
        CHECK(game.Find<AuraBoostMageSite>(spellId) == NULL);
    }
    CHECK_EQ(game.CountAt(DummyEffectHook::Key),
             sizeof(COMBUSTION_IDS) / sizeof(uint32) + sizeof(IMPACT_SPREAD_IDS) / sizeof(uint32) +
                 sizeof(ORB_DUMMY_EFFECT_IDS) / sizeof(uint32));
    CHECK(game.Find<AuraBoostMageSite>(28682) == NULL);

    // With no unit target the script casts nothing and ends the effect, so the mage family switch does not run; the
    // effect's value stays as it is.
    BareCreature caster;
    SpellRowStorage row;
    int32 value = 100;
    int32 word = 0;
    DummyEffectContext noTarget(caster, NULL, NULL, NULL, SpellRow(row, 11129), EFFECT_INDEX_0, value, ObjectGuid(), NULL,
                                word);
    CHECK(registry.Dispatch<DummyEffectHook>(11129, noTarget).IsReturn());
    CHECK_EQ(value, int32(100));

    // An effect other than the script effect, with a unit target, ends the effect too and casts nothing: the target
    // holds no aura afterwards.
    BareCreature target;
    DummyEffectContext otherEffect(caster, &target, NULL, NULL, SpellRow(row, 11129), EFFECT_INDEX_1, value, ObjectGuid(),
                                   &target, word);
    CHECK(registry.Dispatch<DummyEffectHook>(11129, otherEffect).IsReturn());
    CHECK(target.GetSpellAuraHolderMap().empty());
    CHECK_EQ(value, int32(100));
}

TEST(MageScripts_CombustionTickIsThePerSecondSumWithIgnitesShareBeforeMastery)
{
    // Ignite 2000 every 2 s, Pyroblast 500 and Living Bomb 700 every 3 s: 1000 + 166.67 + 233.33 = 1400 a second.
    Mage::DotShare const pyroblastAndBomb[] = { { 500, 3000 }, { 700, 3000 } };
    Mage::DotShare const ignite = { 2000, 2000 };
    CHECK_EQ(Mage::CombustionTick(pyroblastAndBomb, 2, &ignite), int32(1400));

    // Ignite's share is its tick before mastery and is not divided by a held mastery: an Ignite that hit at 1.224
    // still gives 1400, not 1000 / 1.224 + 400, so the engine's Flashburn on 83853 reaches it once, at the cast.
    Mage::DotShare igniteHitAtMastery = { 2000, 2000 };
    igniteHitAtMastery.heldMastery = 1.224f;
    CHECK_EQ(Mage::CombustionTick(pyroblastAndBomb, 2, &igniteHitAtMastery), int32(1400));

    // A hasted Pyroblast ticks every 2400 ms, so it gives more a second: 1000 + 208.33 + 233.33 = 1441.67, truncated
    // once after the sum.
    Mage::DotShare const hasted[] = { { 500, 2400 }, { 700, 3000 } };
    CHECK_EQ(Mage::CombustionTick(hasted, 2, &ignite), int32(1441));

    // Nothing to combine gives a tick of 0; Ignite alone gives its share a second.
    CHECK_EQ(Mage::CombustionTick(NULL, 0, NULL), int32(0));
    Mage::DotShare const igniteAlone = { 1300, 2000 };
    CHECK_EQ(Mage::CombustionTick(NULL, 0, &igniteAlone), int32(650));

    // A share with no period adds nothing, and a sum below 0 is 0.
    Mage::DotShare const noPeriod[] = { { 500, 0 }, { 700, 3000 } };
    CHECK_EQ(Mage::CombustionTick(noPeriod, 2, NULL), int32(233));
    Mage::DotShare const negative[] = { { -900, 3000 } };
    CHECK_EQ(Mage::CombustionTick(negative, 1, NULL), int32(0));
}

TEST(MageScripts_CombustionTickRaisesEachShareByItsLowHealthBonusButNotIgnites)
{
    // Pyroblast 487 and Living Bomb 700 every 3 s at Molten Fury's 1.12: 162.33 x 1.12 + 233.33 x 1.12 = 443.1.
    Mage::DotShare const ignite = { 2000, 2000, 1.12f };
    Mage::DotShare const raised[] = { { 487, 3000, 1.12f }, { 700, 3000, 1.12f } };
    CHECK_EQ(Mage::CombustionTick(raised, 2, NULL), int32(443));

    // Ignite's share, 1000 a second, takes none of it: the sum is 1443.1.
    CHECK_EQ(Mage::CombustionTick(raised, 2, &ignite), int32(1443));

    // A share left at the default, or given 0 or below, counts as it is: 162.33 + 233.33.
    Mage::DotShare const plain[] = { { 487, 3000 }, { 700, 3000, 0.0f } };
    CHECK_EQ(Mage::CombustionTick(plain, 2, NULL), int32(395));
    Mage::DotShare const below[] = { { 487, 3000, -1.0f } };
    CHECK_EQ(Mage::CombustionTick(below, 1, NULL), int32(162));

    // Each share has its own bonus: one raised, one not.
    Mage::DotShare const mixed[] = { { 600, 3000, 1.12f }, { 600, 3000 } };
    CHECK_EQ(Mage::CombustionTick(mixed, 2, NULL), int32(424));
}

namespace
{
    // A DoT's tick as the done damage of a DoT gives it: (base + coefficient x spell power) x Flashburn, truncated.
    int32 DoneTick(double base, double coefficient, double spellPower, double flashburn)
    {
        return int32((base + coefficient * spellPower) * flashburn);
    }

    // Combustion's tick as the engine gives it: the sum the script passes, then the new DoT's own Flashburn, truncated.
    int32 EngineTick(int32 sum, double flashburn)
    {
        return int32(double(sum) * flashburn);
    }
}

TEST(MageScripts_CombustionTickBuildsLivingBombAndPyroblastFromSpellPowerAndMasteryWithFlashburnOnce)
{
    // Living Bomb (500 + 0.3 x spell power) and Pyroblast (280 + 0.11 x spell power) rebuilt at the cast over their
    // unhasted 3 s period, each tick holding the cast's Flashburn and divided by it, so that the engine's Flashburn on
    // 83853 gives it back once. Ignite's tick before its mastery is 2000 every 2 s.
    double const spellPowers[] = { 3000.0, 5200.0 };
    double const masteries[] = { 1.0, 1.224, 1.31 };
    for (double spellPower : spellPowers)
    {
        for (double flashburn : masteries)
        {
            int32 const bomb = DoneTick(500.0, 0.3, spellPower, flashburn);
            int32 const pyroblast = DoneTick(280.0, 0.11, spellPower, flashburn);
            Mage::DotShare shares[] = { { pyroblast, 3000 }, { bomb, 3000 } };
            shares[0].heldMastery = float(flashburn);
            shares[1].heldMastery = float(flashburn);
            Mage::DotShare const ignite = { 2000, 2000 };
            int32 const tick = EngineTick(Mage::CombustionTick(shares, 2, &ignite), flashburn);

            // Ignite/2 x M + LB(SP, M)/3 + Pyroblast(SP, M)/3: Flashburn once in each term.
            double const expected = 1000.0 * flashburn + double(bomb) / 3.0 + double(pyroblast) / 3.0;
            CHECK(double(tick) >= expected - 2.0 && double(tick) <= expected + 0.001);
        }
    }

    // More spell power moves the shares: +1200 raises the sum by (0.3 + 0.11) x 1200 / 3 = 164 before the engine's
    // Flashburn.
    Mage::DotShare low[] = { { DoneTick(280.0, 0.11, 3000.0, 1.224), 3000 }, { DoneTick(500.0, 0.3, 3000.0, 1.224), 3000 } };
    Mage::DotShare high[] = { { DoneTick(280.0, 0.11, 4200.0, 1.224), 3000 }, { DoneTick(500.0, 0.3, 4200.0, 1.224), 3000 } };
    for (Mage::DotShare& share : low)
    {
        share.heldMastery = 1.224f;
    }
    for (Mage::DotShare& share : high)
    {
        share.heldMastery = 1.224f;
    }
    int32 const gain = Mage::CombustionTick(high, 2, NULL) - Mage::CombustionTick(low, 2, NULL);
    CHECK(gain >= 163 && gain <= 165);

    // A share holding Flashburn that is not divided would carry it twice once the engine's own is on it.
    Mage::DotShare undivided[] = { { 1224, 3000 } };
    Mage::DotShare divided[] = { { 1224, 3000 } };
    divided[0].heldMastery = 1.224f;
    CHECK_EQ(Mage::CombustionTick(undivided, 1, NULL), int32(408));
    CHECK_EQ(Mage::CombustionTick(divided, 1, NULL), int32(333));

    // A held mastery of 0 or below leaves the share as it is.
    divided[0].heldMastery = 0.0f;
    CHECK_EQ(Mage::CombustionTick(divided, 1, NULL), int32(408));
    divided[0].heldMastery = -1.0f;
    CHECK_EQ(Mage::CombustionTick(divided, 1, NULL), int32(408));
}

TEST(MageScripts_CombustionTickOverTheUnhastedPeriodNotTheHastedOne)
{
    // Each share is over the spell's own 3000 ms, whatever period a hasted aura ticks at: 900 and 1500 over 3 s.
    Mage::DotShare const unhasted[] = { { 900, 3000 }, { 1500, 3000 } };
    CHECK_EQ(Mage::CombustionTick(unhasted, 2, NULL), int32(800));

    // The hasted period the aura holds, taken as the period, would put the shares 20% high: 2400 over 2.5 s.
    Mage::DotShare const hastedPeriod[] = { { 900, 2500 }, { 1500, 2500 } };
    CHECK_EQ(Mage::CombustionTick(hastedPeriod, 2, NULL), int32(960));
}

namespace
{
    // The class options and the Spell.dbc rows of the stacking test, kept alive for the rest of the run.
    const uint32 kOptionsCombustionDot = 94207;
    const uint32 kOptionsResistanceAura = 94208;
    const uint32 kOtherIconSpell = 94209;

    SpellClassOptionsEntry s_optionsCombustionDot;
    SpellClassOptionsEntry s_optionsResistanceAura;
    ServerSpell s_combustionDot;
    ServerSpell s_resistanceAura;
    ServerSpell s_otherIconSpell;

    // `spell` as one aura effect of `auraType` at its effect 0 with icon 33 and visual `visual`, put into the spell and
    // effect stores; `classOptionsId` 0 makes it a generic spell.
    void StoreIconSpell(ServerSpell& spell, uint32 spellId, uint32 classOptionsId, uint32 visual, uint32 auraType)
    {
        spell.Entry().ID = spellId;
        spell.Entry().ClassOptionsID = classOptionsId;
        spell.Entry().SpellIconID = 33;
        spell.Entry().SpellVisualID[0] = visual;
        spell.effect.SpellID = spellId;
        spell.effect.EffectIndex = EFFECT_INDEX_0;
        spell.effect.Effect = SPELL_EFFECT_APPLY_AURA;
        spell.effect.EffectAura = auraType;
        spell.effect.ImplicitTarget_0 = TARGET_CHAIN_DAMAGE;
        sSpellStore.SetEntry(spellId, &spell.Entry());
        AddSpellEffectEntry(&spell.effect);
    }

    // Combustion's DoT (83853: mage, class mask C 0x408, a periodic damage aura, icon 33, visual 19968), Resistance
    // Aura (19891: paladin, a resistance aura, icon 33, visual 321) and a generic resistance aura of icon 33, put into
    // the stores once.
    void StoreIcon33Spells()
    {
        static bool stored = false;
        if (stored)
        {
            return;
        }
        stored = true;
        s_optionsCombustionDot.SpellClassMask = ClassFamilyMask(0, 0x408);
        s_optionsCombustionDot.SpellClassSet = SPELLFAMILY_MAGE;
        s_optionsResistanceAura.SpellClassMask = ClassFamilyMask(0, 0);
        s_optionsResistanceAura.SpellClassSet = SPELLFAMILY_PALADIN;
        sSpellClassOptionsStore.SetEntry(kOptionsCombustionDot, &s_optionsCombustionDot);
        sSpellClassOptionsStore.SetEntry(kOptionsResistanceAura, &s_optionsResistanceAura);
        StoreIconSpell(s_combustionDot, 83853, kOptionsCombustionDot, 19968, SPELL_AURA_PERIODIC_DAMAGE);
        StoreIconSpell(s_resistanceAura, 19891, kOptionsResistanceAura, 321, SPELL_AURA_MOD_RESISTANCE);
        StoreIconSpell(s_otherIconSpell, kOtherIconSpell, 0, 0, SPELL_AURA_MOD_RESISTANCE);
    }
}

TEST(MageScripts_CombustionsDoTStacksWithTheOtherAurasOfItsIcon)
{
    // Two auras of one icon with no spell modifier effect do not stack (the generic same-icon rule), so a new holder
    // of one removes the other's from its unit. Combustion's DoT is exempt, both ways: it neither removes Resistance
    // Aura (or any other aura of icon 33) from its target nor is removed by it.
    StoreIcon33Spells();
    CHECK(sSpellStore.LookupEntry(83853) != NULL);
    CHECK(!sSpellMgr.IsNoStackSpellDueToSpell(83853, 19891));
    CHECK(!sSpellMgr.IsNoStackSpellDueToSpell(19891, 83853));
    CHECK(!sSpellMgr.IsNoStackSpellDueToSpell(83853, kOtherIconSpell));
    CHECK(!sSpellMgr.IsNoStackSpellDueToSpell(kOtherIconSpell, 83853));

    // The rule itself stands for the other auras of the icon.
    CHECK(sSpellMgr.IsNoStackSpellDueToSpell(19891, kOtherIconSpell));
    CHECK(sSpellMgr.IsNoStackSpellDueToSpell(kOtherIconSpell, 19891));
}

namespace
{
    // The class options and the Spell.dbc rows of the Arcane Missiles stacking test: mage spells of class mask 0 and
    // icon 1952, as Arcane Missiles! (79683) and Arcane Missiles Aurastate (79808) are; the test rows carry no effects.
    // The test takes the rows out of the spell store again.
    const uint32 kOptionsArcaneMissilesAuras = 94210;
    const uint32 kOtherIcon1952Spell = 94211;

    SpellClassOptionsEntry s_optionsArcaneMissilesAuras;
    ServerSpell s_arcaneMissilesBuff;
    ServerSpell s_arcaneMissilesAurastate;
    ServerSpell s_otherIcon1952Spell;

    void StoreIcon1952Spell(ServerSpell& spell, uint32 spellId)
    {
        spell.Entry().ID = spellId;
        spell.Entry().ClassOptionsID = kOptionsArcaneMissilesAuras;
        spell.Entry().SpellIconID = 1952;
        sSpellStore.SetEntry(spellId, &spell.Entry());
    }
}

TEST(MageScripts_ArcaneMissilesBuffAndItsAurastateStackWithEachOther)
{
    // The two share icon 1952 and hold no spell modifier effect, so the generic same-icon rule would have a new holder
    // of either remove the other's from the mage. They are exempt from it, both ways.
    s_optionsArcaneMissilesAuras.SpellClassMask = ClassFamilyMask(0, 0);
    s_optionsArcaneMissilesAuras.SpellClassSet = SPELLFAMILY_MAGE;
    sSpellClassOptionsStore.SetEntry(kOptionsArcaneMissilesAuras, &s_optionsArcaneMissilesAuras);
    StoreIcon1952Spell(s_arcaneMissilesBuff, 79683);
    StoreIcon1952Spell(s_arcaneMissilesAurastate, 79808);
    StoreIcon1952Spell(s_otherIcon1952Spell, kOtherIcon1952Spell);
    CHECK(!sSpellMgr.IsNoStackSpellDueToSpell(79683, 79808));
    CHECK(!sSpellMgr.IsNoStackSpellDueToSpell(79808, 79683));

    // The rule itself stands for another aura of the icon.
    CHECK(sSpellMgr.IsNoStackSpellDueToSpell(79808, kOtherIcon1952Spell));
    CHECK(sSpellMgr.IsNoStackSpellDueToSpell(kOtherIcon1952Spell, 79683));

    sSpellStore.SetEntry(79683, NULL);
    sSpellStore.SetEntry(79808, NULL);
    sSpellStore.SetEntry(kOtherIcon1952Spell, NULL);
    CHECK(sSpellStore.LookupEntry(79808) == NULL);
}

namespace
{
    // The class options and the Spell.dbc rows of the Fingers of Frost stacking test: mage spells of icon 2947, a
    // passive one as the talent's ranks (44543, 44545, 83074) are and one that is not, as the buff (44544) is; the
    // test rows carry no effects. The test takes the rows out of the spell store again.
    const uint32 kOptionsFingersOfFrost = 94270;
    const uint32 kFingersTalentRow = 94271;
    const uint32 kFingersBuffRow = 94272;

    SpellClassOptionsEntry s_optionsFingersOfFrost;
    ServerSpell s_fingersTalentRow;
    ServerSpell s_fingersBuffRow;

    void StoreIcon2947Spell(ServerSpell& spell, uint32 spellId, uint32 attributes)
    {
        spell.Entry().ID = spellId;
        spell.Entry().ClassOptionsID = kOptionsFingersOfFrost;
        spell.Entry().SpellIconID = 2947;
        spell.Entry().Attributes = attributes;
        sSpellStore.SetEntry(spellId, &spell.Entry());
    }
}

TEST(MageScripts_FingersOfFrostsTalentAndItsBuffStandTogether)
{
    // The mage spells of icon 2947 that are auras: the talent's three ranks, passive, of which a mage holds one, and
    // the buff, not passive. A passive aura and one that is not stand together before any rule of their icon.
    s_optionsFingersOfFrost.SpellClassMask = ClassFamilyMask(0, 0);
    s_optionsFingersOfFrost.SpellClassSet = SPELLFAMILY_MAGE;
    sSpellClassOptionsStore.SetEntry(kOptionsFingersOfFrost, &s_optionsFingersOfFrost);
    StoreIcon2947Spell(s_fingersTalentRow, kFingersTalentRow, SPELL_ATTR_PASSIVE);
    StoreIcon2947Spell(s_fingersBuffRow, kFingersBuffRow, 0);
    CHECK(!sSpellMgr.IsNoStackSpellDueToSpell(kFingersTalentRow, kFingersBuffRow));
    CHECK(!sSpellMgr.IsNoStackSpellDueToSpell(kFingersBuffRow, kFingersTalentRow));

    sSpellStore.SetEntry(kFingersTalentRow, NULL);
    sSpellStore.SetEntry(kFingersBuffRow, NULL);
    CHECK(sSpellStore.LookupEntry(kFingersBuffRow) == NULL);
}

TEST(MageScripts_ImpactSpreadsPyroblastIgniteAndCombustionAlwaysAndLivingBombToTwoEnemies)
{
    // Pyroblast, Pyroblast!, Ignite and Combustion always, and the count of Living Bombs stays as it is.
    int32 bombs = 0;
    for (uint32 spellId : { 11366u, 92315u, 12654u, 83853u })
    {
        CHECK(Mage::ImpactSpreads(spellId, bombs));
        CHECK_EQ(bombs, int32(0));
    }

    // Living Bomb at the first two enemies, each counted; not at a third, and the count stays at two.
    CHECK(Mage::ImpactSpreads(44457, bombs));
    CHECK_EQ(bombs, int32(1));
    CHECK(Mage::ImpactSpreads(44457, bombs));
    CHECK_EQ(bombs, int32(2));
    CHECK(!Mage::ImpactSpreads(44457, bombs));
    CHECK_EQ(bombs, int32(2));
    CHECK(Mage::ImpactSpreads(11366, bombs));
    CHECK_EQ(bombs, int32(2));

    // Not Flamestrike's DoT (2120), Living Bomb's explosion (44461) or Frostfire Bolt (44614), and none of them counts.
    int32 none = 0;
    for (uint32 spellId : { 2120u, 44461u, 44614u })
    {
        CHECK(!Mage::ImpactSpreads(spellId, none));
    }
    CHECK_EQ(none, int32(0));
}

TEST(MageScripts_ImpactClearsFireBlastsCooldownWhenItsProcTriggerEffectProcs)
{
    AddImpactEffectRows();
    SpellHandlerRegistry registry;
    RegisterMageScripts(registry);
    OneProcBody(registry, IMPACT_IDS);

    // A player whose Fire Blast (2136) is on cooldown; what his cooldown sink is told is kept.
    WorldSession session(1, "impact", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    SessionPlayer mage(session);
    static std::vector<uint32> s_cleared;
    s_cleared.clear();
    CooldownSinks sinks;
    sinks.cleared = [](CooldownsClearedFact const& fact)
    {
        s_cleared.insert(s_cleared.end(), fact.spellIds.begin(), fact.spellIds.end());
    };
    mage.SetCooldownSinks(sinks);
    time_t const now = time(NULL);
    mage.GetSpellCooldownMgr().AddSpellCooldown(2136, 0, now + 8);
    CHECK(mage.GetSpellCooldownMgr().HasSpellCooldown(2136, now));

    // An effect other than the proc-trigger one goes on to its handler and leaves the cooldown.
    HolderStorage storage;
    CHECK(Proc(registry, mage, 11103, EFFECT_INDEX_1, EmptyHolder(storage), NULL, PROC_EX_NORMAL_HIT, 0).IsContinue());
    CHECK(mage.GetSpellCooldownMgr().HasSpellCooldown(2136, now));
    CHECK(s_cleared.empty());

    // The proc-trigger effect of either rank clears the cooldown and tells the client, then casts the effect's trigger
    // spell, the Impact buff (64343), which the test's spell store does not hold: the proc fails there.
    for (uint32 spellId : IMPACT_IDS)
    {
        mage.GetSpellCooldownMgr().AddSpellCooldown(2136, 0, now + 8);
        s_cleared.clear();
        SpellHandlerOutcome<SpellAuraProcResult> const proc =
            Proc(registry, mage, spellId, EFFECT_INDEX_0, EmptyHolder(storage), NULL, PROC_EX_NORMAL_HIT, 0);
        CHECK(proc.IsReturn());
        CHECK_EQ(int(proc.GetValue()), int(SPELL_AURA_PROC_FAILED));
        CHECK(!mage.GetSpellCooldownMgr().HasSpellCooldown(2136, now));
        CHECK_EQ(s_cleared.size(), std::size_t(1));
        CHECK(!s_cleared.empty() && s_cleared[0] == 2136);
    }
}

TEST(MageScripts_DealsDamageReadsTheSpellsOwnEffectRows)
{
    AddDamageCheckRows();
    SpellRowStorage row;
    for (uint32 spellId : { ROW_DIRECT_DAMAGE, ROW_PERIODIC_DAMAGE_WITH_DUMMY, ROW_PERIODIC_PERCENT, ROW_PERIODIC_LEECH,
                            ROW_WEAPON_DAMAGE })
    {
        CHECK(Spells::DealsDamage(SpellRow(row, spellId)));
    }
    for (uint32 spellId : { ROW_SLOW_AURAS, ROW_CONFUSE, ROW_INTERRUPT, ROW_STUN, ROW_TRIGGERS_DAMAGE, 0u })
    {
        CHECK(!Spells::DealsDamage(SpellRow(row, spellId)));
    }
}

TEST(MageScripts_ImpactRollsOnlyFromSpellsThatDealDamage)
{
    AddDamageCheckRows();
    SpellHandlerRegistry registry;
    RegisterMageScripts(registry);
    BareCreature owner;
    HolderStorage storage;
    SpellAuraHolder& holder = EmptyHolder(storage);
    SpellRowStorage impactRow;
    SpellRowStorage eventRow;

    for (uint32 impact : IMPACT_IDS)
    {
        CHECK(registry.Find<CheckProcHook>(impact) != NULL);

        // The spells that deal damage, Living Bomb's cast among them, go on to the other checks.
        for (uint32 spellId : { ROW_DIRECT_DAMAGE, ROW_PERIODIC_DAMAGE_WITH_DUMMY })
        {
            CheckProcContext ctx(owner, NULL, SpellRow(impactRow, impact), AuraHandle(holder), &SpellRow(eventRow, spellId),
                                 0x10000, PROC_EX_NORMAL_HIT, 0);
            CHECK(registry.Dispatch<CheckProcHook>(impact, ctx).IsContinue());
        }

        // A hit that deals none (Slow, Polymorph, Counterspell, Deep Freeze) is refused.
        for (uint32 spellId : { ROW_SLOW_AURAS, ROW_CONFUSE, ROW_INTERRUPT, ROW_STUN })
        {
            CheckProcContext ctx(owner, NULL, SpellRow(impactRow, impact), AuraHandle(holder), &SpellRow(eventRow, spellId),
                                 0x10000, PROC_EX_NORMAL_HIT, 0);
            SpellHandlerOutcome<bool> const outcome = registry.Dispatch<CheckProcHook>(impact, ctx);
            CHECK(outcome.IsReturn());
            CHECK(!outcome.GetValue());
        }

        // An event with no spell goes on.
        CheckProcContext melee(owner, NULL, SpellRow(impactRow, impact), AuraHandle(holder), NULL, 0x10000,
                               PROC_EX_NORMAL_HIT, 0);
        CHECK(registry.Dispatch<CheckProcHook>(impact, melee).IsContinue());
    }
}

TEST(MageScripts_ImpactsSpreadEndsItsEffectAndCountsTheLivingBombsOverTheCast)
{
    SpellHandlerRegistry registry;
    RegisterMageScripts(registry);
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    for (uint32 spellId : IMPACT_SPREAD_IDS)
    {
        CHECK(registry.Find<DummyEffectHook>(spellId) != NULL);
        CHECK(game.Find<DummyEffectHook>(spellId) == registry.Find<DummyEffectHook>(spellId));
    }
    CHECK_EQ(game.CountAt(DummyEffectHook::Key),
             sizeof(COMBUSTION_IDS) / sizeof(uint32) + sizeof(IMPACT_SPREAD_IDS) / sizeof(uint32) +
                 sizeof(ORB_DUMMY_EFFECT_IDS) / sizeof(uint32));

    BareCreature caster;
    BareCreature stunned;
    BareCreature beside;
    SpellRowStorage row;
    SpellEntry const& spell = SpellRow(row, 12355);
    int32 value = 0;
    int32 word = 0;

    // The stun's effect, the stunned target itself, a cast with no unit target and an effect with none: the script
    // ends the effect and counts nothing.
    DummyEffectContext stun(caster, &stunned, NULL, NULL, spell, EFFECT_INDEX_0, value, ObjectGuid(), &stunned, word);
    CHECK(registry.Dispatch<DummyEffectHook>(12355, stun).IsReturn());
    DummyEffectContext self(caster, &stunned, NULL, NULL, spell, EFFECT_INDEX_1, value, ObjectGuid(), &stunned, word);
    CHECK(registry.Dispatch<DummyEffectHook>(12355, self).IsReturn());
    DummyEffectContext noCastTarget(caster, &beside, NULL, NULL, spell, EFFECT_INDEX_1, value, ObjectGuid(), NULL, word);
    CHECK(registry.Dispatch<DummyEffectHook>(12355, noCastTarget).IsReturn());
    DummyEffectContext noTarget(caster, NULL, NULL, NULL, spell, EFFECT_INDEX_1, value, ObjectGuid(), &stunned, word);
    CHECK(registry.Dispatch<DummyEffectHook>(12355, noTarget).IsReturn());
    CHECK_EQ(word, int32(0));

    // An enemy under a confuse or a fear aura is left alone: nothing is tried, so nothing counts.
    BareCreature confused;
    confused.MarkAuraType(SPELL_AURA_MOD_CONFUSE);
    BareCreature feared;
    feared.MarkAuraType(SPELL_AURA_MOD_FEAR);
    for (BareCreature* skipped : { &confused, &feared })
    {
        DummyEffectContext ctx(caster, skipped, NULL, NULL, spell, EFFECT_INDEX_1, value, ObjectGuid(), &stunned, word);
        CHECK(registry.Dispatch<DummyEffectHook>(12355, ctx).IsReturn());
    }
    CHECK_EQ(word, int32(0));

    // Three enemies beside a stunned target that holds nothing of the caster's: nothing is copied, but Living Bomb is
    // tried at the first two and counts there, not at the third, the cast's word holding the count across its targets.
    BareCreature second;
    BareCreature third;
    for (BareCreature* enemy : { &beside, &second, &third })
    {
        DummyEffectContext ctx(caster, enemy, NULL, NULL, spell, EFFECT_INDEX_1, value, ObjectGuid(), &stunned, word);
        CHECK(registry.Dispatch<DummyEffectHook>(12355, ctx).IsReturn());
        CHECK(enemy->GetSpellAuraHolderMap().empty());
    }
    CHECK_EQ(word, int32(2));

    // Another cast: the stunned target holds the caster's Pyroblast, and the enemy beside it already holds his own. The
    // copy is refused, and the enemy keeps the one holder it had; Living Bomb is tried and counts in this cast's word.
    int32 nextWord = 0;
    HolderStorage stunnedPyroblast;
    HolderStorage besidePyroblast;
    SpellAuraHolder& theirs = EmptyHolder(besidePyroblast);
    stunned.HoldAura(11366, EmptyHolder(stunnedPyroblast));
    beside.HoldAura(11366, theirs);
    DummyEffectContext atBeside(caster, &beside, NULL, NULL, spell, EFFECT_INDEX_1, value, ObjectGuid(), &stunned,
                                nextWord);
    CHECK(registry.Dispatch<DummyEffectHook>(12355, atBeside).IsReturn());
    CHECK(beside.GetSpellAuraHolder(11366) == &theirs);
    CHECK_EQ(beside.GetSpellAuraHolderMap().size(), std::size_t(1));
    CHECK_EQ(nextWord, int32(1));
    CHECK_EQ(word, int32(2));
    CHECK_EQ(value, int32(0));
}

TEST(MageScripts_ImprovedFlamestrikesChanceIsTheRanksEffectValue)
{
    // Rank 1's effect 0 value is 50 and rank 2's 100, as the client's data holds them; the chance is read from the
    // store. Blast Wave, the free Flamestrike (88148) and no spell are no rank.
    AddImprovedFlamestrikeRanks();
    SpellEntry const* rank1 = sSpellStore.LookupEntry(84673);
    SpellEntry const* rank2 = sSpellStore.LookupEntry(84674);
    CHECK(rank1 != NULL);
    CHECK(rank2 != NULL);
    if (rank1 && rank2)
    {
        CHECK_EQ(rank1->CalculateSimpleValue(EFFECT_INDEX_0), int32(50));
        CHECK_EQ(rank2->CalculateSimpleValue(EFFECT_INDEX_0), int32(100));
    }
    CHECK_EQ(Mage::ImprovedFlamestrikeChance(84673), int32(50));
    CHECK_EQ(Mage::ImprovedFlamestrikeChance(84674), int32(100));
    CHECK_EQ(Mage::ImprovedFlamestrikeChance(11113), int32(0));
    CHECK_EQ(Mage::ImprovedFlamestrikeChance(88148), int32(0));
    CHECK_EQ(Mage::ImprovedFlamestrikeChance(0), int32(0));
}

TEST(MageScripts_BlastWaveRunsOnTheAfterCastHookAndCallsNothingBelowTwoEnemies)
{
    // Blast Wave holds an after-cast row, in the game's table too; the free Flamestrike and the talent's ranks hold
    // none. The hook's other row is Replenish Mana's.
    SpellHandlerRegistry registry;
    RegisterMageScripts(registry);
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    for (uint32 spellId : BLAST_WAVE_IDS)
    {
        CHECK(registry.Find<AfterCastHook>(spellId) != NULL);
        CHECK(game.Find<AfterCastHook>(spellId) == registry.Find<AfterCastHook>(spellId));
    }
    CHECK_EQ(game.CountAt(AfterCastHook::Key), (sizeof(BLAST_WAVE_IDS) + sizeof(REPLENISH_MANA_IDS)) / sizeof(uint32));
    for (uint32 spellId : { 88148u, 84673u, 84674u, 2120u })
    {
        CHECK(game.Find<AfterCastHook>(spellId) == NULL);
    }

    // The body through hand-built contexts, as the cast's end calls it. Every one answers Continue, and the caster has
    // started no cast of his own.
    AddImprovedFlamestrikeRanks();
    SpellHandler<AfterCastHook>::Function const body = registry.Find<AfterCastHook>(11113);
    CHECK(body != NULL);
    if (!body)
    {
        return;
    }
    BareCreature caster;
    BareCreature aimedAt;
    SpellRowStorage row;
    SpellEntry const& blastWave = SpellRow(row, 11113);

    // Two enemies hit by a caster without Improved Flamestrike: nothing.
    AfterCastContext noTalent(caster, blastWave, 2, &aimedAt, true, 10.0f, 20.0f, 5.0f, 0);
    CHECK(body(noTalent).IsContinue());
    CHECK(caster.GetCurrentSpell(CURRENT_GENERIC_SPELL) == NULL);

    // With rank 2 held: none or one enemy hit, nothing; two enemies hit by a cast with neither a destination nor a
    // unit target, nothing, since the cast has no point of its own.
    HolderStorage talentStorage;
    caster.HoldAura(84674, EmptyHolder(talentStorage));
    for (uint32 enemies : { 0u, 1u })
    {
        AfterCastContext few(caster, blastWave, enemies, &aimedAt, true, 10.0f, 20.0f, 5.0f, 0);
        CHECK(body(few).IsContinue());
        CHECK(caster.GetCurrentSpell(CURRENT_GENERIC_SPELL) == NULL);
    }
    AfterCastContext pointless(caster, blastWave, 2, NULL, false, 0.0f, 0.0f, 0.0f, 0);
    CHECK(body(pointless).IsContinue());
    CHECK(caster.GetCurrentSpell(CURRENT_GENERIC_SPELL) == NULL);

    // A smoke case: two enemies hit with rank 2 held and a point runs the chance and the cast of 88148 at the point.
    // The test's store holds no 88148, so the cast ends at its lookup and builds nothing, and this case cannot tell
    // an attempt from none; scenario 980 pins the cast itself. The caster holds only the talent afterwards.
    AfterCastContext two(caster, blastWave, 2, &aimedAt, true, 10.0f, 20.0f, 5.0f, 0);
    CHECK(body(two).IsContinue());
    CHECK(caster.GetCurrentSpell(CURRENT_GENERIC_SPELL) == NULL);
    CHECK_EQ(caster.GetSpellAuraHolderMap().size(), std::size_t(1));
}

TEST(MageScripts_FrostfireGlyphTickIsTheStacksShareOfTheHitWithMastery)
{
    // 3% of a 3000 hit over four ticks: 90 / 4 = 22 a tick for one stack, 66 for three.
    CHECK_EQ(Mage::FrostfireGlyphTick(3000, 3, 4, 1, 1.0f), int32(22));
    CHECK_EQ(Mage::FrostfireGlyphTick(3000, 3, 4, 3, 1.0f), int32(66));

    // A holder that ticks five times in its time left (a hasted one): 90 / 5 = 18 a tick, 54 for three stacks.
    CHECK_EQ(Mage::FrostfireGlyphTick(3000, 3, 5, 3, 1.0f), int32(54));

    // Flashburn's 22.4% at 8 mastery points: 66 x 1.224 = 80.78, truncated.
    CHECK_EQ(Mage::FrostfireGlyphTick(3000, 3, 4, 3, 1.224f), int32(80));

    // A share below one point a tick deals nothing, and so do no hit and no tick.
    CHECK_EQ(Mage::FrostfireGlyphTick(100, 3, 4, 3, 1.0f), int32(0));
    CHECK_EQ(Mage::FrostfireGlyphTick(0, 3, 4, 1, 1.0f), int32(0));
    CHECK_EQ(Mage::FrostfireGlyphTick(3000, 3, 0, 1, 1.0f), int32(0));
}

TEST(MageScripts_ASharesHitIsTheDealtDamageOverTheFactorsItsTicksTakeAgain)
{
    // The nearest whole hit before the factor; the damage as it is for a factor of 0 or below.
    CHECK_EQ(Mage::ShareBase(10000, 1.08f), uint32(9259));
    CHECK_EQ(Mage::ShareBase(10000, 0.9f), uint32(11111));
    CHECK_EQ(Mage::ShareBase(10000, 1.0f), uint32(10000));
    CHECK_EQ(Mage::ShareBase(10000, 0.0f), uint32(10000));

    // Ignite's 40% over two ticks of a 10000 crit on a target that takes 8% more: the share is cut from 9259, so a tick
    // is 1851 and the tick that takes the 8% again is 1999; cut from the 10000 dealt it would be 2000 and 2160.
    int32 const tick = Combat::DotShareTick(float(Mage::ShareBase(10000, 1.08f)), 40, 2);
    CHECK_EQ(tick, int32(1851));
    CHECK_EQ(uint32(float(tick) * 1.08f), uint32(1999));
    CHECK_EQ(Combat::DotShareTick(10000.0f, 40, 2), int32(2000));
    CHECK_EQ(uint32(2000 * 1.08f), uint32(2160));

    // The glyph divides by the caster's low-health bonus and the target's taken effects together: a 3240 hit that took
    // 1.12 and 1.08 comes back to 2679, whose 3% over four ticks is 20.
    CHECK_EQ(Mage::ShareBase(3240, 1.12f * 1.08f), uint32(2679));
    CHECK_EQ(Combat::DotShareTick(float(Mage::ShareBase(3240, 1.12f * 1.08f)), 3, 4), int32(20));
}

TEST(MageScripts_FrostfireGlyphShareLeavesOutMoltenFurySoEachTickTakesItOnce)
{
    // A 3000 hit that Molten Fury's third rank raised by 12% at a target under 35% health deals 3360; the share is
    // taken from the 3000 before it, the nearest whole hit, and from 3000 itself, Molten Fury's first rank's 3120 and
    // no bonus at all.
    CHECK_EQ(Mage::ShareBase(3360, 1.12f), uint32(3000));
    CHECK_EQ(Mage::ShareBase(3120, 1.04f), uint32(3000));
    CHECK_EQ(Mage::ShareBase(3000, 1.0f), uint32(3000));
    CHECK_EQ(Mage::ShareBase(0, 1.12f), uint32(0));
    CHECK_EQ(Mage::ShareBase(3000, 0.0f), uint32(3000));

    // At 30% health the tick takes the 12% once, as Aura::PeriodicTick multiplies it: one stack int(22 x 1.12) = 24,
    // three int(66 x 1.12) = 73. A share taken from the raised 3360 would tick int(25 x 1.12) = 28 and int(75 x 1.12)
    // = 84, the bonus twice.
    float const moltenFury = 1.12f;
    int32 const oneStack = Mage::FrostfireGlyphTick(Mage::ShareBase(3360, moltenFury), 3, 4, 1, 1.0f);
    int32 const threeStacks = Mage::FrostfireGlyphTick(Mage::ShareBase(3360, moltenFury), 3, 4, 3, 1.0f);
    CHECK_EQ(oneStack, int32(22));
    CHECK_EQ(threeStacks, int32(66));
    CHECK_EQ(uint32(uint32(oneStack) * moltenFury), uint32(24));
    CHECK_EQ(uint32(uint32(threeStacks) * moltenFury), uint32(73));
    CHECK_EQ(uint32(uint32(Mage::FrostfireGlyphTick(3360, 3, 4, 1, 1.0f)) * moltenFury), uint32(28));
    CHECK_EQ(uint32(uint32(Mage::FrostfireGlyphTick(3360, 3, 4, 3, 1.0f)) * moltenFury), uint32(84));
}

namespace
{
    const uint32 kFrostfireBolt = 44614;
    const uint32 kGlyphOfFrostfire = 61205;
    const uint32 kThreeAuraSpell = 94261;
    const uint32 kSlowAuraSpell = 94262;

    // A spell's records in the spell and effect stores, kept alive for the rest of the run.
    struct StoredAuras
    {
        ServerSpell spell;
        SpellEffectEntry more[MAX_EFFECT_INDEX - 1];
    };

    // Puts `spellId` into the stores once with `effects` effects, each an aura at the target whose apply and removal
    // change nothing on the unit (SPELL_AURA_MOD_DAMAGE_FROM_CASTER), of `basePoints`; no other test reads it.
    SpellEntry const* StoreAuras(StoredAuras& rows, uint32 spellId, uint32 effects, int32 basePoints)
    {
        if (rows.spell.Entry().ID == spellId)
        {
            return &rows.spell.Entry();
        }
        rows.spell.Entry().ID = spellId;
        for (uint32 eff = 0; eff < effects && eff < MAX_EFFECT_INDEX; ++eff)
        {
            SpellEffectEntry& row = eff == 0 ? rows.spell.effect : rows.more[eff - 1];
            row = SpellEffectEntry();
            row.SpellID = spellId;
            row.EffectIndex = eff;
            row.Effect = SPELL_EFFECT_APPLY_AURA;
            row.EffectAura = SPELL_AURA_MOD_DAMAGE_FROM_CASTER;
            row.EffectBasePoints = basePoints;
            row.ImplicitTarget_0 = TARGET_CHAIN_DAMAGE;
            AddSpellEffectEntry(&row);
        }
        sSpellStore.SetEntry(spellId, &rows.spell.Entry());
        return &rows.spell.Entry();
    }

    // A holder of `spell` on `target`, made through its constructor with no caster, so its caster's guid is the
    // target's, with an aura made through CreateAura at every effect the spell has, of the effect's base points.
    SpellAuraHolder* AuraHolder(SpellEntry const* spell, Unit& target)
    {
        SpellAuraHolder* holder = new SpellAuraHolder(spell, &target, NULL, NULL, NULL);
        for (int32 eff = 0; eff < MAX_EFFECT_INDEX; ++eff)
        {
            SpellEffectEntry const* row = spell->GetSpellEffect(SpellEffectIndex(eff));
            if (!row)
            {
                continue;
            }
            int32 basePoints = row->EffectBasePoints;
            holder->AddAura(CreateAura(spell, SpellEffectIndex(eff), &basePoints, holder, &target), SpellEffectIndex(eff));
        }
        return holder;
    }

    // Which effects of `holder` hold an aura, as a mask of the effect indexes.
    uint32 EffectsHeld(SpellAuraHolder const& holder)
    {
        uint32 mask = 0;
        for (int32 eff = 0; eff < MAX_EFFECT_INDEX; ++eff)
        {
            mask |= holder.GetAuraByEffectIndex(SpellEffectIndex(eff)) ? 1u << eff : 0u;
        }
        return mask;
    }
}

TEST(MageScripts_FrostfireBoltsApplyTakesTheSlowOffWithTheGlyphAndTheDamageOffWithout)
{
    SpellHandlerRegistry registry;
    RegisterMageScripts(registry);
    static StoredAuras rows;
    SpellEntry const* spell = StoreAuras(rows, kThreeAuraSpell, MAX_EFFECT_INDEX, 5);
    SpellRowStorage row;
    BareCreature target;

    // A caster with Glyph of Frostfire: the holder loses the slow, effect 0, and keeps effects 1 and 2. The caster is a
    // creature, so Pyromaniac counts nothing, and the holder's boosts still run.
    BareCreature glyphed;
    HolderStorage glyph;
    glyphed.HoldAura(kGlyphOfFrostfire, EmptyHolder(glyph));
    SpellAuraHolder* withGlyph = AuraHolder(spell, target);
    AuraApplyContext glyphedApply(target, &glyphed, SpellRow(row, kFrostfireBolt), AuraHandle(*withGlyph), true);
    CHECK(registry.Dispatch<AuraApplyHook>(kFrostfireBolt, glyphedApply).IsContinue());
    CHECK_EQ(EffectsHeld(*withGlyph), uint32(0x6));
    delete withGlyph;

    // A caster without it: the holder loses the periodic damage, effect 2, and keeps the slow.
    BareCreature plain;
    // A holder that keeps its slow keeps its time.
    SpellAuraHolder* withoutGlyph = AuraHolder(spell, target);
    withoutGlyph->SetAuraDuration(9000);
    AuraApplyContext plainApply(target, &plain, SpellRow(row, kFrostfireBolt), AuraHandle(*withoutGlyph), true);
    CHECK(registry.Dispatch<AuraApplyHook>(kFrostfireBolt, plainApply).IsContinue());
    CHECK_EQ(EffectsHeld(*withoutGlyph), uint32(0x3));
    CHECK_EQ(withoutGlyph->GetAuraDuration(), int32(9000));
    delete withoutGlyph;

    // An apply whose caster cannot be found takes nothing off and caps nothing, at the add and before it.
    SpellAuraHolder* untouched = AuraHolder(spell, target);
    AuraApplyContext noCaster(target, NULL, SpellRow(row, kFrostfireBolt), AuraHandle(*untouched), true);
    CHECK(registry.Dispatch<AuraApplyHook>(kFrostfireBolt, noCaster).IsContinue());
    AuraApplyContext noCasterBefore(target, NULL, SpellRow(row, kFrostfireBolt), AuraHandle(*untouched), false);
    CHECK(registry.Dispatch<AuraApplyHook>(kFrostfireBolt, noCasterBefore).IsContinue());
    CHECK_EQ(EffectsHeld(*untouched), uint32(0x7));
    CHECK_EQ(untouched->GetStackCap(), uint32(0));
    delete untouched;
    CHECK(target.GetSpellAuraHolderMap().empty());
}

TEST(MageScripts_FrostfireBoltsCallBeforeTheAddTakesTheGlyphedSlowOffAndCapsTheUnglyphedStack)
{
    SpellHandlerRegistry registry;
    RegisterMageScripts(registry);
    static StoredAuras rows;
    SpellEntry const* spell = StoreAuras(rows, kThreeAuraSpell, MAX_EFFECT_INDEX, 5);
    SpellRowStorage row;
    BareCreature target;

    // The call before the add, with the holder not on its target. A caster with Glyph of Frostfire: the holder loses
    // the slow, effect 0, there, so the stacking checks never see a slow of his; its stack keeps the spell's limit.
    BareCreature glyphed;
    HolderStorage glyph;
    glyphed.HoldAura(kGlyphOfFrostfire, EmptyHolder(glyph));
    SpellAuraHolder* withGlyph = AuraHolder(spell, target);
    AuraApplyContext glyphedBefore(target, &glyphed, SpellRow(row, kFrostfireBolt), AuraHandle(*withGlyph), false);
    CHECK(registry.Dispatch<AuraApplyHook>(kFrostfireBolt, glyphedBefore).IsContinue());
    CHECK_EQ(EffectsHeld(*withGlyph), uint32(0x6));
    CHECK_EQ(withGlyph->GetStackCap(), uint32(0));

    // The call at the add then finds no slow to take: the holder keeps effects 1 and 2.
    AuraApplyContext glyphedApply(target, &glyphed, SpellRow(row, kFrostfireBolt), AuraHandle(*withGlyph), true);
    CHECK(registry.Dispatch<AuraApplyHook>(kFrostfireBolt, glyphedApply).IsContinue());
    CHECK_EQ(EffectsHeld(*withGlyph), uint32(0x6));
    delete withGlyph;

    // A caster without it: the holder keeps its three effects until the add, and its stack is capped at one, so his
    // own re-cast refreshes it instead of adding a stack.
    BareCreature plain;
    SpellAuraHolder* withoutGlyph = AuraHolder(spell, target);
    AuraApplyContext plainBefore(target, &plain, SpellRow(row, kFrostfireBolt), AuraHandle(*withoutGlyph), false);
    CHECK(registry.Dispatch<AuraApplyHook>(kFrostfireBolt, plainBefore).IsContinue());
    CHECK_EQ(EffectsHeld(*withoutGlyph), uint32(0x7));
    CHECK_EQ(withoutGlyph->GetStackCap(), uint32(1));
    delete withoutGlyph;
    CHECK(target.GetSpellAuraHolderMap().empty());
}

namespace
{
    const uint32 kPeriodicSpell = 94263;
    const int32 kGlyphPercent = 3;

    struct StoredPeriodic
    {
        ServerSpell spell;
    };

    // Puts `spellId` into the stores once with a periodic damage aura at effect 0 every 3000 ms; no other test reads it.
    SpellEntry const* StorePeriodic(StoredPeriodic& rows, uint32 spellId)
    {
        if (rows.spell.Entry().ID == spellId)
        {
            return &rows.spell.Entry();
        }
        rows.spell.Entry().ID = spellId;
        rows.spell.effect.SpellID = spellId;
        rows.spell.effect.EffectIndex = EFFECT_INDEX_0;
        rows.spell.effect.Effect = SPELL_EFFECT_APPLY_AURA;
        rows.spell.effect.EffectAura = SPELL_AURA_PERIODIC_DAMAGE;
        rows.spell.effect.EffectAuraPeriod = 3000;
        rows.spell.effect.ImplicitTarget_0 = TARGET_CHAIN_DAMAGE;
        sSpellStore.SetEntry(spellId, &rows.spell.Entry());
        AddSpellEffectEntry(&rows.spell.effect);
        return &rows.spell.Entry();
    }

    // A holder of `spell` on `target` with one aura, at `eff`, of `amount`.
    SpellAuraHolder* HolderWithOneAura(SpellEntry const* spell, Unit& target, SpellEffectIndex eff, int32 amount)
    {
        SpellAuraHolder* holder = new SpellAuraHolder(spell, &target, NULL, NULL, NULL);
        int32 basePoints = amount;
        holder->AddAura(CreateAura(spell, eff, &basePoints, holder, &target), eff);
        return holder;
    }
}

TEST(MageScripts_FrostfireBoltsHolderWithoutASlowRunsOut)
{
    SpellHandlerRegistry registry;
    RegisterMageScripts(registry);
    static StoredAuras rows;
    SpellEntry const* spell = StoreAuras(rows, kThreeAuraSpell, MAX_EFFECT_INDEX, 5);
    SpellRowStorage row;
    BareCreature target;
    BareCreature plain;
    BareCreature glyphed;
    HolderStorage glyph;
    glyphed.HoldAura(kGlyphOfFrostfire, EmptyHolder(glyph));

    // A target immune to the slow: the holder reaches the hook with effect 2 alone. Without the glyph that last effect
    // is not removed, and the holder is left with no time, so the unit removes it as expired with no tick, and
    // Pyromaniac and Combustion pass over it.
    SpellAuraHolder* immune = HolderWithOneAura(spell, target, EFFECT_INDEX_2, 5);
    immune->SetAuraDuration(9000);
    AuraApplyContext plainApply(target, &plain, SpellRow(row, kFrostfireBolt), AuraHandle(*immune), true);
    CHECK(registry.Dispatch<AuraApplyHook>(kFrostfireBolt, plainApply).IsContinue());
    CHECK_EQ(EffectsHeld(*immune), uint32(0x4));
    CHECK_EQ(immune->GetAuraDuration(), int32(0));

    // The after-hit takes the time again when the holder was given it back.
    immune->SetAuraDuration(9000);
    AfterHitContext plainHit(plain, target, SpellRow(row, kFrostfireBolt), 3000, false, 0, 0, false);
    target.HoldAura(kFrostfireBolt, *immune);
    CHECK(registry.Dispatch<AfterHitHook>(kFrostfireBolt, plainHit).IsContinue());
    CHECK_EQ(immune->GetAuraDuration(), int32(0));
    delete immune;

    // With the glyph the periodic damage is the bolt's, and its time stays.
    SpellAuraHolder* burning = HolderWithOneAura(spell, target, EFFECT_INDEX_2, 5);
    burning->SetAuraDuration(12000);
    AuraApplyContext glyphedApply(target, &glyphed, SpellRow(row, kFrostfireBolt), AuraHandle(*burning), true);
    CHECK(registry.Dispatch<AuraApplyHook>(kFrostfireBolt, glyphedApply).IsContinue());
    CHECK_EQ(burning->GetAuraDuration(), int32(12000));
    delete burning;
}

TEST(MageScripts_FrostfireBoltsFourthGlyphedBoltRewritesTheBurnWithItsOwnHitAtThreeStacks)
{
    SpellHandlerRegistry registry;
    RegisterMageScripts(registry);
    static StoredAuras glyphRows;
    static StoredPeriodic periodicRows;
    StoreAuras(glyphRows, kGlyphOfFrostfire, MAX_EFFECT_INDEX, kGlyphPercent);
    SpellEntry const* periodic = StorePeriodic(periodicRows, kPeriodicSpell);
    SpellRowStorage row;
    BareCreature target;
    BareCreature glyphed;
    HolderStorage glyph;
    glyphed.HoldAura(kGlyphOfFrostfire, EmptyHolder(glyph));

    // The caster's holder at three stacks with the third bolt's tick of 69 and the time that bolt left it.
    SpellAuraHolder* holder = HolderWithOneAura(periodic, target, EFFECT_INDEX_0, 1);
    holder->SetStackAmount(3);
    holder->SetAuraMaxDuration(12000);
    holder->SetAuraDuration(12000);
    target.HoldAura(kFrostfireBolt, *holder);
    AuraHandle(*holder).SetAmount(EFFECT_INDEX_0, 69);
    CHECK_EQ(holder->GetStackAmount(), uint32(3));

    // The fourth bolt's hit, 2000 (1900 dealt and 100 absorbed): the stacks stay at three and the tick is the newest
    // hit's share times three, 3 x (int(2000 x 3 / 100) / 4) = 45, below the 69 it replaces, not the 15 of one
    // stack; a hit of 4000 gives 3 x (120 / 4) = 90.
    AfterHitContext fourth(glyphed, target, SpellRow(row, kFrostfireBolt), 1900, false, 100, 0, false);
    CHECK(registry.Dispatch<AfterHitHook>(kFrostfireBolt, fourth).IsContinue());
    CHECK_EQ(holder->GetStackAmount(), uint32(3));
    CHECK_EQ(AuraHandle(*holder).Amount(EFFECT_INDEX_0), int32(45));
    AfterHitContext bigger(glyphed, target, SpellRow(row, kFrostfireBolt), 4000, false, 0, 0, false);
    CHECK(registry.Dispatch<AfterHitHook>(kFrostfireBolt, bigger).IsContinue());
    CHECK_EQ(holder->GetStackAmount(), uint32(3));
    CHECK_EQ(AuraHandle(*holder).Amount(EFFECT_INDEX_0), int32(90));
    delete holder;
}

TEST(MageScripts_FrostfireBoltsAfterHitHoldsAnUnglyphedBoltAtOneStack)
{
    SpellHandlerRegistry registry;
    RegisterMageScripts(registry);
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    CHECK(registry.Find<AfterHitHook>(kFrostfireBolt) != NULL);
    CHECK(game.Find<AfterHitHook>(kFrostfireBolt) == registry.Find<AfterHitHook>(kFrostfireBolt));
    CHECK_EQ(game.CountAt(AfterHitHook::Key), std::size_t(3));

    // A heal, and a hit on a target that holds no Frostfire Bolt of the caster, change nothing.
    BareCreature caster;
    BareCreature target;
    SpellRowStorage row;
    AfterHitContext heal(caster, target, SpellRow(row, kFrostfireBolt), 3000, false, 0, 0, true);
    CHECK(registry.Dispatch<AfterHitHook>(kFrostfireBolt, heal).IsContinue());
    AfterHitContext nothingHeld(caster, target, SpellRow(row, kFrostfireBolt), 3000, false, 0, 0, false);
    CHECK(registry.Dispatch<AfterHitHook>(kFrostfireBolt, nothingHeld).IsContinue());
    CHECK(target.GetSpellAuraHolderMap().empty());

    // The caster's holder of the bolt on the target, its one aura a slow of -40, stacked to three by the engine: -120.
    // The bare creatures' guids are all empty, so the holder is the caster's.
    static StoredAuras rows;
    SpellEntry const* slow = StoreAuras(rows, kSlowAuraSpell, 1, -40);
    SpellAuraHolder* holder = AuraHolder(slow, target);
    holder->SetStackAmount(3);
    target.HoldAura(kFrostfireBolt, *holder);
    CHECK_EQ(AuraHandle(*holder).Amount(EFFECT_INDEX_0), int32(-120));

    // With Glyph of Frostfire on the caster the stacks stay; the glyph's row is not in the spell store here, and the
    // holder has no periodic damage, so no share is written either.
    BareCreature glyphed;
    HolderStorage glyph;
    glyphed.HoldAura(kGlyphOfFrostfire, EmptyHolder(glyph));
    AfterHitContext glyphedHit(glyphed, target, SpellRow(row, kFrostfireBolt), 3000, false, 0, 0, false);
    CHECK(registry.Dispatch<AfterHitHook>(kFrostfireBolt, glyphedHit).IsContinue());
    CHECK_EQ(holder->GetStackAmount(), uint32(3));
    CHECK_EQ(AuraHandle(*holder).Amount(EFFECT_INDEX_0), int32(-120));

    // Without the glyph the holder is set back to one stack, and the slow to its own -40.
    AfterHitContext plainHit(caster, target, SpellRow(row, kFrostfireBolt), 3000, false, 0, 0, false);
    CHECK(registry.Dispatch<AfterHitHook>(kFrostfireBolt, plainHit).IsContinue());
    CHECK_EQ(holder->GetStackAmount(), uint32(1));
    CHECK_EQ(AuraHandle(*holder).Amount(EFFECT_INDEX_0), int32(-40));

    // At one stack a second hit changes nothing.
    CHECK(registry.Dispatch<AfterHitHook>(kFrostfireBolt, plainHit).IsContinue());
    CHECK_EQ(holder->GetStackAmount(), uint32(1));
    CHECK_EQ(AuraHandle(*holder).Amount(EFFECT_INDEX_0), int32(-40));
    delete holder;
}

namespace
{
    // A mage spell with a damage effect and its class mask, as the client's data holds them.
    struct MaskedSpell
    {
        uint32 id;
        uint32 a;
        uint32 b;
        uint32 c;
    };

    MaskedSpell const MAGE_DAMAGE_SPELLS[] =
    {
        { 116, 0x20, 0x0, 0x0 }, { 120, 0x200, 0x0, 0x0 }, { 122, 0x40, 0x0, 0x0 }, { 133, 0x1, 0x0, 0x8 },
        { 1449, 0x1000, 0x0, 0x0 }, { 2120, 0x4, 0x0, 0x0 }, { 2136, 0x2, 0x0, 0x0 }, { 2948, 0x10, 0x0, 0x0 },
        { 7268, 0x200000, 0x0, 0x0 }, { 11113, 0x0, 0x40, 0x0 }, { 11129, 0x0, 0x4000000, 0x8 },
        { 11366, 0x400000, 0x0, 0x1008 }, { 12654, 0x8000000, 0x0, 0x8 }, { 30451, 0x20000000, 0x0, 0x0 },
        { 30455, 0x20000, 0x0, 0x0 }, { 31661, 0x800000, 0x0, 0x0 }, { 34913, 0x0, 0x8, 0x0 },
        { 42208, 0x80080, 0x0, 0x0 }, { 44425, 0x0, 0x8000, 0x0 }, { 44457, 0x0, 0x20000, 0x8 },
        { 44461, 0x0, 0x10000, 0x0 }, { 44614, 0x0, 0x1000, 0x8 }, { 71757, 0x0, 0x0, 0x20 },
        { 82739, 0x0, 0x0, 0x80 }, { 83619, 0x0, 0x0, 0x0 }, { 83853, 0x0, 0x0, 0x408 },
        { 84721, 0x0, 0x0, 0x4080 }, { 88148, 0x4, 0x0, 0x0 }, { 92315, 0x400000, 0x0, 0x8 },
        { 95969, 0x0, 0x0, 0x4080 },
    };

    // The class masks of Shatter's and Frostburn's first effect.
    MaskedSpell const SHATTER_MASK = { 0, 0x20EA1277, 0x139048, 0xA0 };
    MaskedSpell const FROSTBURN_MASK = { 0, 0x20EA1277, 0x119048, 0x20 };

    bool MaskMeets(MaskedSpell const& talent, MaskedSpell const& spell)
    {
        return (talent.a & spell.a) != 0 || (talent.b & spell.b) != 0 || (talent.c & spell.c) != 0;
    }

    uint32 const kShatterRankOne = 11170;
    uint32 const kShatterRankTwo = 12982;
    uint32 const kFrostburn = 76613;
    uint32 const kFingersOfFrost = 44544;
    uint32 const kFrostbolt = 116;
    uint32 const kIceLance = 30455;
    uint32 const kDeepFreeze = 44572;
    uint32 const kDeepFreezeDamage = 71757;
    uint32 const kFrostfireBoltSpell = 44614;
    uint32 const kBrainFreezeBuff = 57761;

    bool Near(float value, float expected)
    {
        return std::fabs(value - expected) < 0.0001f;
    }

    void Freeze(Unit& unit)
    {
        unit.SetFlag(UNIT_FIELD_AURASTATE, 1 << (AURA_STATE_FROZEN - 1));
    }
}

TEST(MageScripts_TheTwoFrostListsHoldExactlyTheSpellsTheTalentMasksCover)
{
    // Every mage damage spell of the table: listed for Shatter exactly when Shatter's class mask meets its own, and for
    // Frostburn exactly when Frostburn's does; the lists hold 26 and 22 of the table's spells. Arcane Brilliance's
    // buff (1459) and a spell of another class are on neither.
    std::size_t shatter = 0;
    std::size_t frostburn = 0;
    for (MaskedSpell const& spell : MAGE_DAMAGE_SPELLS)
    {
        CHECK_EQ(Mage::ShatterCovers(spell.id), MaskMeets(SHATTER_MASK, spell));
        CHECK_EQ(Mage::FrostburnCovers(spell.id), MaskMeets(FROSTBURN_MASK, spell));
        shatter += Mage::ShatterCovers(spell.id) ? 1 : 0;
        frostburn += Mage::FrostburnCovers(spell.id) ? 1 : 0;
    }
    CHECK_EQ(shatter, std::size_t(26));
    CHECK_EQ(frostburn, std::size_t(22));
    for (uint32 spellId : { 1459u, 1u, 0u })
    {
        CHECK(!Mage::ShatterCovers(spellId));
        CHECK(!Mage::FrostburnCovers(spellId));
    }

    // The hooks hold the same ids, the same body for every id of a list, and the game's table the same; the
    // damage-done hook also holds Torment the Weak's three ranks, none of them a spell of the table.
    SpellHandlerRegistry registry;
    RegisterMageScripts(registry);
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    CHECK_EQ(registry.CountAt(CritChanceHook::Key), std::size_t(26));
    CHECK_EQ(registry.CountAt(DamageDoneHook::Key), std::size_t(22 + sizeof(TORMENT_THE_WEAK_IDS) / sizeof(uint32)));
    CHECK_EQ(game.CountAt(CritChanceHook::Key), std::size_t(26));
    CHECK_EQ(game.CountAt(DamageDoneHook::Key), std::size_t(22 + sizeof(TORMENT_THE_WEAK_IDS) / sizeof(uint32)));
    SpellHandler<CritChanceHook>::Function const crit = registry.Find<CritChanceHook>(kFrostbolt);
    SpellHandler<DamageDoneHook>::Function const done = registry.Find<DamageDoneHook>(kFrostbolt);
    CHECK(crit != NULL);
    CHECK(done != NULL);
    for (MaskedSpell const& spell : MAGE_DAMAGE_SPELLS)
    {
        CHECK(registry.Find<CritChanceHook>(spell.id) == (Mage::ShatterCovers(spell.id) ? crit : NULL));
        CHECK(registry.Find<DamageDoneHook>(spell.id) == (Mage::FrostburnCovers(spell.id) ? done : NULL));
        CHECK(game.Find<CritChanceHook>(spell.id) == registry.Find<CritChanceHook>(spell.id));
        CHECK(game.Find<DamageDoneHook>(spell.id) == registry.Find<DamageDoneHook>(spell.id));
    }
}

TEST(MageScripts_ShatterMultipliesTheCritChanceByTwoOrThree)
{
    CHECK_EQ(Mage::ShatterFactor(false, false), 1.0f);
    CHECK_EQ(Mage::ShatterFactor(true, false), 2.0f);
    CHECK_EQ(Mage::ShatterFactor(false, true), 3.0f);
    CHECK_EQ(Mage::ShatterFactor(true, true), 3.0f);
}

TEST(MageScripts_ASpellCountsItsTargetAsFrozenByTheStateOrByAFingersChargeForTwoSpells)
{
    BareCreature caster;
    BareCreature victim;
    BareCreature fingers;
    HolderStorage fingersStorage;
    fingers.HoldAura(kFingersOfFrost, EmptyHolder(fingersStorage));
    SpellRowStorage row;

    // An unfrozen target: Ice Lance and Deep Freeze count it as frozen under Fingers of Frost alone; Frostbolt, a
    // Frostfire Bolt without Brain Freeze's buff and the rest do not, and nothing does without the charge. Deep
    // Freeze's damage does not, with the charge or without.
    for (uint32 spellId : { kIceLance, kDeepFreeze })
    {
        CHECK_EQ(int(Mage::CountsAsFrozen(fingers, victim, SpellRow(row, spellId), SPELL_DIRECT_DAMAGE, 0)),
                 int(Mage::FINGERS));
        CHECK_EQ(int(Mage::CountsAsFrozen(caster, victim, SpellRow(row, spellId), SPELL_DIRECT_DAMAGE, 0)),
                 int(Mage::NOT_FROZEN));
    }
    for (uint32 spellId : { kFrostbolt, kFrostfireBoltSpell, 122u, 133u })
    {
        CHECK_EQ(int(Mage::CountsAsFrozen(fingers, victim, SpellRow(row, spellId), SPELL_DIRECT_DAMAGE, 0)),
                 int(Mage::NOT_FROZEN));
        CHECK_EQ(int(Mage::CountsAsFrozen(caster, victim, SpellRow(row, spellId), SPELL_DIRECT_DAMAGE, 0)),
                 int(Mage::NOT_FROZEN));
    }
    CHECK_EQ(int(Mage::CountsAsFrozen(caster, victim, SpellRow(row, kDeepFreezeDamage), SPELL_DIRECT_DAMAGE, 0)),
             int(Mage::NOT_FROZEN));
    CHECK_EQ(int(Mage::CountsAsFrozen(fingers, victim, SpellRow(row, kDeepFreezeDamage), SPELL_DIRECT_DAMAGE, 0)),
             int(Mage::NOT_FROZEN));

    // The Frostfire Bolt Brain Freeze makes instant counts it as frozen under Fingers of Frost, and not without it; one
    // cast in full does not, under a buff that came during its cast too.
    BareCreature brainFreeze;
    BareCreature both;
    HolderStorage brainFreezeStorage;
    HolderStorage bothFingers;
    HolderStorage bothBrainFreeze;
    brainFreeze.HoldAura(kBrainFreezeBuff, EmptyHolder(brainFreezeStorage));
    both.HoldAura(kFingersOfFrost, EmptyHolder(bothFingers));
    both.HoldAura(kBrainFreezeBuff, EmptyHolder(bothBrainFreeze));
    CHECK_EQ(int(Mage::CountsAsFrozen(both, victim, SpellRow(row, kFrostfireBoltSpell), SPELL_DIRECT_DAMAGE, 0)),
             int(Mage::FINGERS));
    CHECK_EQ(int(Mage::CountsAsFrozen(both, victim, SpellRow(row, kFrostfireBoltSpell), SPELL_DIRECT_DAMAGE, 2500)),
             int(Mage::NOT_FROZEN));
    CHECK_EQ(int(Mage::CountsAsFrozen(brainFreeze, victim, SpellRow(row, kFrostfireBoltSpell), SPELL_DIRECT_DAMAGE, 0)),
             int(Mage::NOT_FROZEN));
    CHECK_EQ(int(Mage::CountsAsFrozen(both, victim, SpellRow(row, kFrostbolt), SPELL_DIRECT_DAMAGE, 0)),
             int(Mage::NOT_FROZEN));

    // The bolt's periodic effect counts the target by its own state alone: under both buffs it does not count an
    // unfrozen target as frozen, and neither does any other periodic effect.
    for (uint32 spellId : { kFrostfireBoltSpell, kIceLance, kDeepFreezeDamage })
    {
        CHECK_EQ(int(Mage::CountsAsFrozen(both, victim, SpellRow(row, spellId), DOT, 0)), int(Mage::NOT_FROZEN));
    }

    // A frozen target: every spell counts it as frozen, with the charge or without, and so does a periodic effect.
    Freeze(victim);
    CHECK(victim.IsFrozen());
    for (uint32 spellId : { kIceLance, kDeepFreeze, kDeepFreezeDamage, kFrostbolt, kFrostfireBoltSpell, 122u })
    {
        CHECK_EQ(int(Mage::CountsAsFrozen(fingers, victim, SpellRow(row, spellId), SPELL_DIRECT_DAMAGE, 0)),
                 int(Mage::REALLY_FROZEN));
        CHECK_EQ(int(Mage::CountsAsFrozen(caster, victim, SpellRow(row, spellId), SPELL_DIRECT_DAMAGE, 0)),
                 int(Mage::REALLY_FROZEN));
    }
    CHECK_EQ(int(Mage::CountsAsFrozen(both, victim, SpellRow(row, kFrostfireBoltSpell), DOT, 0)),
             int(Mage::REALLY_FROZEN));
}

TEST(MageScripts_ShatterRaisesTheCritChanceOfASpellThatCountsItsTargetAsFrozen)
{
    SpellHandlerRegistry registry;
    RegisterMageScripts(registry);
    BareCreature none;
    BareCreature rankOne;
    BareCreature rankTwo;
    BareCreature victim;
    HolderStorage oneStorage;
    HolderStorage twoStorage;
    rankOne.HoldAura(kShatterRankOne, EmptyHolder(oneStorage));
    rankTwo.HoldAura(kShatterRankTwo, EmptyHolder(twoStorage));
    SpellRowStorage row;

    // An unfrozen target leaves the chance as it is.
    float chance = 20.0f;
    CritChanceContext unfrozen(rankTwo, victim, SpellRow(row, kFrostbolt), SPELL_DIRECT_DAMAGE, chance);
    CHECK(registry.Dispatch<CritChanceHook>(kFrostbolt, unfrozen).IsContinue());
    CHECK_EQ(chance, 20.0f);

    // A frozen one: 20 becomes 40 with the first rank, 60 with the second and stays 20 without Shatter; 35 becomes 70
    // and 105 (the engine caps the chance later).
    Freeze(victim);
    struct Vector
    {
        BareCreature* caster;
        float chance;
        float expected;
    };
    Vector const vectors[] =
    {
        { &rankOne, 20.0f, 40.0f }, { &rankTwo, 20.0f, 60.0f }, { &none, 20.0f, 20.0f },
        { &rankOne, 35.0f, 70.0f }, { &rankTwo, 35.0f, 105.0f },
    };
    for (Vector const& v : vectors)
    {
        float value = v.chance;
        CritChanceContext frozen(*v.caster, victim, SpellRow(row, kFrostbolt), SPELL_DIRECT_DAMAGE, value);
        CHECK(registry.Dispatch<CritChanceHook>(kFrostbolt, frozen).IsContinue());
        CHECK(Near(value, v.expected));
    }

    // Flame Orb's hit (82739) is on the list: it takes the factor the same way.
    float orbHit = 10.0f;
    CritChanceContext orb(rankTwo, victim, SpellRow(row, 82739), SPELL_DIRECT_DAMAGE, orbHit);
    CHECK(registry.Dispatch<CritChanceHook>(82739, orb).IsContinue());
    CHECK(Near(orbHit, 30.0f));

    // A Frostfire Bolt of a caster with the second rank, Fingers of Frost and Brain Freeze's buff: its hit at an
    // unfrozen target takes the factor, a tick of its periodic effect does not; a tick at a frozen target does.
    BareCreature boltMage;
    BareCreature unfrozenVictim;
    HolderStorage boltShatter;
    HolderStorage boltFingers;
    HolderStorage boltBrainFreeze;
    boltMage.HoldAura(kShatterRankTwo, EmptyHolder(boltShatter));
    boltMage.HoldAura(kFingersOfFrost, EmptyHolder(boltFingers));
    boltMage.HoldAura(kBrainFreezeBuff, EmptyHolder(boltBrainFreeze));
    struct TickVector
    {
        Unit* victim;
        DamageEffectType type;
        float expected;
    };
    TickVector const ticks[] =
    {
        { &unfrozenVictim, SPELL_DIRECT_DAMAGE, 60.0f }, { &unfrozenVictim, DOT, 20.0f }, { &victim, DOT, 60.0f },
    };
    for (TickVector const& v : ticks)
    {
        float value = 20.0f;
        CritChanceContext bolt(boltMage, *v.victim, SpellRow(row, kFrostfireBoltSpell), v.type, value);
        CHECK(registry.Dispatch<CritChanceHook>(kFrostfireBoltSpell, bolt).IsContinue());
        CHECK(Near(value, v.expected));
    }

    // Deep Freeze's damage (71757) takes the factor at a frozen target alone: at an unfrozen one the chance stays as
    // it is, whether the caster holds a Fingers of Frost charge or not.
    BareCreature deepFreezeMage;
    HolderStorage deepFreezeShatter;
    HolderStorage deepFreezeFingers;
    deepFreezeMage.HoldAura(kShatterRankTwo, EmptyHolder(deepFreezeShatter));
    deepFreezeMage.HoldAura(kFingersOfFrost, EmptyHolder(deepFreezeFingers));
    for (BareCreature* caster : { &rankTwo, &deepFreezeMage })
    {
        float unfrozenChance = 20.0f;
        CritChanceContext atUnfrozen(*caster, unfrozenVictim, SpellRow(row, kDeepFreezeDamage), SPELL_DIRECT_DAMAGE,
                                     unfrozenChance);
        CHECK(registry.Dispatch<CritChanceHook>(kDeepFreezeDamage, atUnfrozen).IsContinue());
        CHECK_EQ(unfrozenChance, 20.0f);
        float frozenChance = 20.0f;
        CritChanceContext atFrozen(*caster, victim, SpellRow(row, kDeepFreezeDamage), SPELL_DIRECT_DAMAGE,
                                   frozenChance);
        CHECK(registry.Dispatch<CritChanceHook>(kDeepFreezeDamage, atFrozen).IsContinue());
        CHECK(Near(frozenChance, 60.0f));
    }

    // The engine adds the spell's flat crit-chance modifiers after the hook, and Shatter multiplies them too: a sheet
    // chance of 20 with Glyph of Frostbolt's 5 and the second rank leaves 70 at the hook and ends at 75; without the
    // glyph it leaves 60. The first rank with the glyph ends at 50.
    CHECK(Near(Mage::ShatterCritChance(20.0f, 3.0f, 5.0f), 70.0f));
    CHECK(Near(Mage::ShatterCritChance(20.0f, 3.0f, 5.0f) + 5.0f, 75.0f));
    CHECK(Near(Mage::ShatterCritChance(20.0f, 3.0f, 0.0f), 60.0f));
    CHECK(Near(Mage::ShatterCritChance(20.0f, 2.0f, 5.0f) + 5.0f, 50.0f));
    CHECK(Near(Mage::ShatterCritChance(20.0f, 1.0f, 5.0f), 20.0f));
}

TEST(MageScripts_FrostburnShatterAndIceLanceMultiplyTheDamageOnceEachForAFrozenTarget)
{
    SpellHandlerRegistry registry;
    RegisterMageScripts(registry);
    static StoredAuras shatterOne;
    static StoredAuras shatterTwo;
    static StoredAuras frostburnRow;
    static StoredAuras iceLanceRow;
    StoreAuras(shatterOne, kShatterRankOne, 2, 10);
    StoreAuras(shatterTwo, kShatterRankTwo, 2, 20);
    SpellEntry const* frostburn = StoreAuras(frostburnRow, kFrostburn, 1, 5);
    StoreAuras(iceLanceRow, kIceLance, 2, 100);

    BareCreature plain;
    BareCreature frostMage;
    BareCreature shatterMage;
    BareCreature fingersMage;
    BareCreature victim;
    SpellRowStorage row;
    HolderStorage oneStorage;
    HolderStorage twoStorage;
    HolderStorage fingersStorage;
    SpellAuraHolder* frostburnHolder = HolderWithOneAura(frostburn, frostMage, EFFECT_INDEX_0, 5);
    frostMage.HoldAura(kFrostburn, *frostburnHolder);
    shatterMage.HoldAura(kShatterRankTwo, EmptyHolder(twoStorage));
    fingersMage.HoldAura(kFingersOfFrost, EmptyHolder(fingersStorage));
    BareCreature rankOneMage;
    rankOneMage.HoldAura(kShatterRankOne, EmptyHolder(oneStorage));

    // An unfrozen target takes none of it.
    float mod = 1.5f;
    DamageDoneContext unfrozen(frostMage, victim, SpellRow(row, kFrostbolt), SPELL_DIRECT_DAMAGE, mod);
    CHECK(registry.Dispatch<DamageDoneHook>(kFrostbolt, unfrozen).IsContinue());
    CHECK_EQ(mod, 1.5f);

    Freeze(victim);
    struct Vector
    {
        BareCreature* caster;
        uint32 spellId;
        float expected;
    };
    Vector const vectors[] =
    {
        { &frostMage, 133, 1.05f },                 // Frostburn's 5 percent alone
        { &frostMage, 116, 1.05f },                 // no Shatter: no Frostbolt bonus
        { &frostMage, kIceLance, 2.1f },            // Ice Lance doubles, then Frostburn
        { &frostMage, kDeepFreezeDamage, 1.05f },   // Deep Freeze's damage takes Frostburn
        { &shatterMage, kFrostbolt, 1.2f },         // Shatter's second rank: 20 percent, no Frostburn
        { &rankOneMage, kFrostbolt, 1.1f },         // the first rank: 10 percent
        { &shatterMage, 133, 1.0f },                // only Frostbolt takes the Shatter percent
        { &plain, kIceLance, 2.0f },                // Ice Lance doubles with no talent
        { &plain, 2136, 1.0f },
    };
    for (Vector const& v : vectors)
    {
        float value = 1.0f;
        DamageDoneContext frozen(*v.caster, victim, SpellRow(row, v.spellId), SPELL_DIRECT_DAMAGE, value);
        CHECK(registry.Dispatch<DamageDoneHook>(v.spellId, frozen).IsContinue());
        CHECK(Near(value, v.expected));
    }

    // Under Fingers of Frost at an unfrozen target, Ice Lance and Deep Freeze count it as frozen: Ice Lance doubles,
    // and deals 25 percent more for the charge it uses; at a frozen target the charge adds the same 25 percent.
    BareCreature unfrozenVictim;
    float lance = 1.0f;
    DamageDoneContext fingersLance(fingersMage, unfrozenVictim, SpellRow(row, kIceLance), SPELL_DIRECT_DAMAGE, lance);
    CHECK(registry.Dispatch<DamageDoneHook>(kIceLance, fingersLance).IsContinue());
    CHECK(Near(lance, 2.5f));
    float frozenLance = 1.0f;
    DamageDoneContext fingersFrozenLance(fingersMage, victim, SpellRow(row, kIceLance), SPELL_DIRECT_DAMAGE,
                                         frozenLance);
    CHECK(registry.Dispatch<DamageDoneHook>(kIceLance, fingersFrozenLance).IsContinue());
    CHECK(Near(frozenLance, 2.5f));
    float bolt = 1.0f;
    DamageDoneContext fingersBolt(fingersMage, unfrozenVictim, SpellRow(row, kFrostbolt), SPELL_DIRECT_DAMAGE, bolt);
    CHECK(registry.Dispatch<DamageDoneHook>(kFrostbolt, fingersBolt).IsContinue());
    CHECK_EQ(bolt, 1.0f);

    // Deep Freeze's damage takes Frostburn at an unfrozen target too, and nothing more: no Frostburn, no bonus.
    float deepFreeze = 1.0f;
    DamageDoneContext unfrozenDeepFreeze(frostMage, unfrozenVictim, SpellRow(row, kDeepFreezeDamage),
                                         SPELL_DIRECT_DAMAGE, deepFreeze);
    CHECK(registry.Dispatch<DamageDoneHook>(kDeepFreezeDamage, unfrozenDeepFreeze).IsContinue());
    CHECK(Near(deepFreeze, 1.05f));
    float fingersDeepFreeze = 1.0f;
    DamageDoneContext noFrostburn(fingersMage, unfrozenVictim, SpellRow(row, kDeepFreezeDamage), SPELL_DIRECT_DAMAGE,
                                  fingersDeepFreeze);
    CHECK(registry.Dispatch<DamageDoneHook>(kDeepFreezeDamage, noFrostburn).IsContinue());
    CHECK_EQ(fingersDeepFreeze, 1.0f);

    // A Frostfire Bolt under Fingers of Frost and Brain Freeze's buff: its hit at an unfrozen target takes Frostburn,
    // the amount of its periodic effect does not; at a frozen target the periodic amount takes it too.
    BareCreature boltMage;
    HolderStorage boltFingers;
    HolderStorage boltBrainFreeze;
    SpellAuraHolder* boltFrostburn = HolderWithOneAura(frostburn, boltMage, EFFECT_INDEX_0, 5);
    boltMage.HoldAura(kFrostburn, *boltFrostburn);
    boltMage.HoldAura(kFingersOfFrost, EmptyHolder(boltFingers));
    boltMage.HoldAura(kBrainFreezeBuff, EmptyHolder(boltBrainFreeze));
    struct TypeVector
    {
        Unit* victim;
        DamageEffectType type;
        float expected;
    };
    TypeVector const types[] =
    {
        { &unfrozenVictim, SPELL_DIRECT_DAMAGE, 1.05f }, { &unfrozenVictim, DOT, 1.0f }, { &victim, DOT, 1.05f },
    };
    for (TypeVector const& v : types)
    {
        float value = 1.0f;
        DamageDoneContext ffb(boltMage, *v.victim, SpellRow(row, kFrostfireBoltSpell), v.type, value);
        CHECK(registry.Dispatch<DamageDoneHook>(kFrostfireBoltSpell, ffb).IsContinue());
        CHECK(Near(value, v.expected));
    }

    // A Frostburn of no amount (a mage without the Mastery passive) multiplies nothing.
    AuraHandle(*frostburnHolder).SetAmount(EFFECT_INDEX_0, 0);
    float none = 1.0f;
    DamageDoneContext noAmount(frostMage, victim, SpellRow(row, 133), SPELL_DIRECT_DAMAGE, none);
    CHECK(registry.Dispatch<DamageDoneHook>(133, noAmount).IsContinue());
    CHECK_EQ(none, 1.0f);
    delete frostburnHolder;
    delete boltFrostburn;
}

TEST(MageScripts_FingersOfFrostRefusesFrostArmorsChillAndTheFirstRankOrbButNotAGlyphedFrostfireBolt)
{
    CHECK(Mage::RefusesChillSource(7321));
    CHECK(Mage::RefusesChillSource(95969));
    for (uint32 spellId : { 116u, 120u, 12484u, 12485u, 84721u, 44614u, 83154u })
    {
        CHECK(!Mage::RefusesChillSource(spellId));
    }

    // The check-proc hook of the three ranks: Frost Armor's Chilled and the first-rank orb hit are dropped; a Frostbolt
    // and a Frostfire Bolt go on, the bolt with Glyph of Frostfire on the owner too.
    SpellHandlerRegistry registry;
    RegisterMageScripts(registry);
    BareCreature owner;
    BareCreature glyphed;
    HolderStorage holder;
    HolderStorage glyph;
    glyphed.HoldAura(kGlyphOfFrostfire, EmptyHolder(glyph));
    SpellRowStorage talentRow;
    SpellRowStorage eventRow;
    for (uint32 talent : FINGERS_OF_FROST_IDS)
    {
        struct Vector
        {
            Unit const* owner;
            uint32 procSpell;
            bool refused;
        };
        Vector const vectors[] =
        {
            { &owner, 7321, true }, { &owner, 95969, true }, { &owner, 116, false }, { &owner, 12484, false },
            { &owner, 44614, false }, { &glyphed, 44614, false }, { &glyphed, 116, false },
        };
        for (Vector const& v : vectors)
        {
            CheckProcContext ctx(*v.owner, NULL, SpellRow(talentRow, talent), AuraHandle(EmptyHolder(holder)),
                                 &SpellRow(eventRow, v.procSpell), 0x10000, PROC_EX_CAST_END, 0);
            SpellHandlerOutcome<bool> outcome = registry.Dispatch<CheckProcHook>(talent, ctx);
            CHECK_EQ(outcome.IsReturn() && !outcome.GetValue(), v.refused);
            CHECK_EQ(outcome.IsContinue(), !v.refused);
        }
    }
}

TEST(MageScripts_IceLanceDoublesAtAFrozenTargetAndTakesAQuarterMoreWithAFingersCharge)
{
    CHECK(Near(Mage::IceLanceMultiplier(false, false, 100, 25), 1.0f));
    CHECK(Near(Mage::IceLanceMultiplier(true, false, 100, 25), 2.0f));
    CHECK(Near(Mage::IceLanceMultiplier(true, true, 100, 25), 2.5f));
    CHECK(Near(Mage::IceLanceMultiplier(true, true, 0, 25), 1.25f));
}

TEST(MageScripts_ASpellUsesAFingersChargeByTheBuffNotByTheTargetsState)
{
    BareCreature none;
    BareCreature fingers;
    BareCreature both;
    HolderStorage fingersStorage;
    HolderStorage bothFingers;
    HolderStorage bothBrainFreeze;
    fingers.HoldAura(kFingersOfFrost, EmptyHolder(fingersStorage));
    both.HoldAura(kFingersOfFrost, EmptyHolder(bothFingers));
    both.HoldAura(kBrainFreezeBuff, EmptyHolder(bothBrainFreeze));
    SpellRowStorage row;
    for (uint32 spellId : { kIceLance, kDeepFreeze })
    {
        CHECK(Mage::UsesFingers(fingers, SpellRow(row, spellId), 0));
        CHECK(Mage::UsesFingers(fingers, SpellRow(row, spellId), 1500));
        CHECK(Mage::UsesFingers(both, SpellRow(row, spellId), 0));
        CHECK(!Mage::UsesFingers(none, SpellRow(row, spellId), 0));
    }
    // A Frostfire Bolt uses one only when Brain Freeze's buff made it instant: not one cast in full, under a buff that
    // came during its cast either.
    CHECK(!Mage::UsesFingers(fingers, SpellRow(row, kFrostfireBoltSpell), 0));
    CHECK(Mage::UsesFingers(both, SpellRow(row, kFrostfireBoltSpell), 0));
    CHECK(!Mage::UsesFingers(both, SpellRow(row, kFrostfireBoltSpell), 2500));
    CHECK(!Mage::UsesFingers(both, SpellRow(row, kFrostfireBoltSpell), 1));
    for (uint32 spellId : { kFrostbolt, kDeepFreezeDamage, 133u, 120u })
    {
        CHECK(!Mage::UsesFingers(both, SpellRow(row, spellId), 0));
    }
}

TEST(MageScripts_FingersOfFrostsUsersTakeTheChargeAtTheLaunchHookAndNowhereElse)
{
    // Ice Lance, Deep Freeze, Frostfire Bolt and Fireball share one body at the launch hook, in the game's table too,
    // and hold no after-cast row; no other Frost spell holds a launch row.
    SpellHandlerRegistry registry;
    RegisterMageScripts(registry);
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    SpellHandler<LaunchHook>::Function const body = registry.Find<LaunchHook>(kIceLance);
    CHECK(body != NULL);
    for (uint32 spellId : FROST_LAUNCH_IDS)
    {
        CHECK(registry.Find<LaunchHook>(spellId) == body);
        CHECK(game.Find<LaunchHook>(spellId) == body);
        CHECK(game.Find<AfterCastHook>(spellId) == NULL);
    }
    CHECK_EQ(game.CountAt(LaunchHook::Key), sizeof(FROST_LAUNCH_IDS) / sizeof(uint32));
    for (uint32 spellId : { kFrostbolt, kDeepFreezeDamage, 120u, 122u, 11113u })
    {
        CHECK(game.Find<LaunchHook>(spellId) == NULL);
    }

    // The damage-done hook, which reckons a travelling spell's hit at its launch, leaves Fingers of Frost's holder as
    // it is: the charge is taken at the launch hook alone.
    BareCreature victim;
    SpellRowStorage row;
    BareCreature fingersMage;
    HolderStorage fingersStorage;
    SpellAuraHolder& fingersHolder = EmptyHolder(fingersStorage);
    fingersMage.HoldAura(kFingersOfFrost, fingersHolder);
    for (uint32 spellId : { kIceLance, kFrostfireBoltSpell, kFrostbolt })
    {
        float mod = 1.0f;
        DamageDoneContext ctx(fingersMage, victim, SpellRow(row, spellId), SPELL_DIRECT_DAMAGE, mod);
        CHECK(registry.Dispatch<DamageDoneHook>(spellId, ctx).IsContinue());
    }
    CHECK_EQ(AuraHandle(fingersHolder).ScriptWord(EFFECT_INDEX_0), int32(0));

    // At the launch hook a caster without either buff is passed by, and so is a Frostfire Bolt cast with Fingers of
    // Frost but without Brain Freeze's buff: neither uses a charge.
    BareCreature none;
    for (uint32 spellId : FROST_LAUNCH_IDS)
    {
        LaunchContext launch(none, SpellRow(row, spellId), 0);
        CHECK(registry.Dispatch<LaunchHook>(spellId, launch).IsContinue());
    }
    CHECK(!Mage::UseFingersCharge(none));
    LaunchContext hardCast(fingersMage, SpellRow(row, kFrostfireBoltSpell), 2750);
    CHECK(registry.Dispatch<LaunchHook>(kFrostfireBoltSpell, hardCast).IsContinue());
    CHECK(fingersMage.HasAura(kFingersOfFrost));

    // A Fireball and a Frostfire Bolt that took their cast time keep Brain Freeze's buff, which came during the cast.
    BareCreature brainFreezeMage;
    HolderStorage brainFreezeStorage;
    brainFreezeMage.HoldAura(kBrainFreezeBuff, EmptyHolder(brainFreezeStorage));
    for (uint32 spellId : { 133u, kFrostfireBoltSpell })
    {
        LaunchContext tookItsTime(brainFreezeMage, SpellRow(row, spellId), 2500);
        CHECK(registry.Dispatch<LaunchHook>(spellId, tookItsTime).IsContinue());
        CHECK(brainFreezeMage.HasAura(kBrainFreezeBuff));
    }

    // A Frostfire Bolt cast in full under both buffs keeps both: the buff did not make it instant, so it uses no
    // charge either.
    BareCreature bothMage;
    HolderStorage bothFingers;
    HolderStorage bothBrainFreeze;
    bothMage.HoldAura(kFingersOfFrost, EmptyHolder(bothFingers));
    bothMage.HoldAura(kBrainFreezeBuff, EmptyHolder(bothBrainFreeze));
    LaunchContext fullBolt(bothMage, SpellRow(row, kFrostfireBoltSpell), 2500);
    CHECK(registry.Dispatch<LaunchHook>(kFrostfireBoltSpell, fullBolt).IsContinue());
    CHECK(bothMage.HasAura(kFingersOfFrost));
    CHECK(bothMage.HasAura(kBrainFreezeBuff));
}

TEST(MageScripts_BrainFreezesBuffIsUsedByTheFireballOrFrostfireBoltItMadeInstant)
{
    // A Fireball or a Frostfire Bolt that went off as it started, cast while the buff is held: the buff made it
    // instant, so it uses the buff. One that took its cast time, another spell, or a caster without the buff: no.
    BareCreature none;
    BareCreature brainFreeze;
    HolderStorage brainFreezeStorage;
    brainFreeze.HoldAura(kBrainFreezeBuff, EmptyHolder(brainFreezeStorage));
    SpellRowStorage row;
    for (uint32 spellId : { 133u, kFrostfireBoltSpell })
    {
        CHECK(Mage::UsesBrainFreeze(brainFreeze, SpellRow(row, spellId), 0));
        CHECK(!Mage::UsesBrainFreeze(brainFreeze, SpellRow(row, spellId), 2500));
        CHECK(!Mage::UsesBrainFreeze(brainFreeze, SpellRow(row, spellId), 1));
        CHECK(!Mage::UsesBrainFreeze(none, SpellRow(row, spellId), 0));
    }
    for (uint32 spellId : { kFrostbolt, kIceLance, kDeepFreeze, 2136u, 11366u, 92315u })
    {
        CHECK(!Mage::UsesBrainFreeze(brainFreeze, SpellRow(row, spellId), 0));
    }
}

TEST(MageScripts_BrainFreezeRefusesFrostArmorsChillAndTheFirstRankOrb)
{
    // The check-proc hook of the three ranks, Fingers of Frost's body: Frost Armor's Chilled and the first-rank orb hit
    // are dropped; Frostbolt, Cone of Cold, Blizzard's Chilled and the second-rank orb hit go on.
    SpellHandlerRegistry registry;
    RegisterMageScripts(registry);
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    BareCreature owner;
    HolderStorage holder;
    SpellRowStorage talentRow;
    SpellRowStorage eventRow;
    for (uint32 talent : BRAIN_FREEZE_IDS)
    {
        CHECK(registry.Find<CheckProcHook>(talent) == registry.Find<CheckProcHook>(FINGERS_OF_FROST_IDS[0]));
        CHECK(game.Find<CheckProcHook>(talent) == registry.Find<CheckProcHook>(talent));
        CHECK(game.Find<OnProcHook>(talent) == NULL);
        struct Vector
        {
            uint32 procSpell;
            bool refused;
        };
        Vector const vectors[] =
        {
            { 7321, true }, { 95969, true }, { 116, false }, { 120, false }, { 12484, false }, { 84721, false },
        };
        for (Vector const& v : vectors)
        {
            CheckProcContext ctx(owner, NULL, SpellRow(talentRow, talent), AuraHandle(EmptyHolder(holder)),
                                 &SpellRow(eventRow, v.procSpell), 0x10000, PROC_EX_CAST_END, 0);
            SpellHandlerOutcome<bool> outcome = registry.Dispatch<CheckProcHook>(talent, ctx);
            CHECK_EQ(outcome.IsReturn() && !outcome.GetValue(), v.refused);
            CHECK_EQ(outcome.IsContinue(), !v.refused);
        }
    }
}

TEST(MageScripts_AHotStreakOrBrainFreezeMageGetsNoArcaneMissiles)
{
    // Hot Streak and each rank of Brain Freeze stop Arcane Missiles!; Fingers of Frost and no talent do not.
    struct Holding
    {
        uint32 spellId;
        bool suppresses;
    };
    Holding const holdings[] =
    {
        { 44445, true }, { 44546, true }, { 44548, true }, { 44549, true }, { 44543, false }, { 0, false },
    };
    for (Holding const& h : holdings)
    {
        BareCreature mage;
        HolderStorage storage;
        if (h.spellId)
        {
            mage.HoldAura(h.spellId, EmptyHolder(storage));
        }
        CHECK_EQ(Mage::SuppressesArcaneMissiles(mage), h.suppresses);
    }

    // Offensive State's proc for a mage with Brain Freeze's second rank: it cannot trigger, whatever the amount, so no
    // charge or cooldown follows; Arcane Missiles is refused as before. Without a talent the roll at 0 refuses it.
    AddProcEffectRows();
    SpellHandlerRegistry registry;
    RegisterMageScripts(registry);
    HolderStorage storage;
    SpellAuraHolder& holder = EmptyHolder(storage);
    BareCreature brainFreeze;
    HolderStorage brainFreezeStorage;
    brainFreeze.HoldAura(44548, EmptyHolder(brainFreezeStorage));
    SpellRowStorage eventRow;
    for (uint32 spellId : { 116u, 44614u, 5143u })
    {
        for (int32 amount : { 0, 40, 100 })
        {
            SpellHandlerOutcome<SpellAuraProcResult> silent =
                Proc(registry, brainFreeze, 79684, EFFECT_INDEX_0, holder, &SpellRow(eventRow, spellId),
                     PROC_EX_NORMAL_HIT, amount);
            CHECK(silent.IsReturn());
            CHECK_EQ(int(silent.GetValue()), int(SPELL_AURA_PROC_CANT_TRIGGER));
        }
    }
    BareCreature plain;
    SpellHandlerOutcome<SpellAuraProcResult> rolled =
        Proc(registry, plain, 79684, EFFECT_INDEX_0, holder, &SpellRow(eventRow, 116), PROC_EX_NORMAL_HIT, 0);
    CHECK(rolled.IsReturn());
    CHECK_EQ(int(rolled.GetValue()), int(SPELL_AURA_PROC_FAILED));
}

namespace
{
    // The class options of a mage spell, and a test-only spell of one aura that the tests list under a slow's aura
    // type, put into the stores once for the rest of the run; no other test uses these ids.
    const uint32 kOptionsMageSpell = 94212;
    const uint32 kListedSlowSpell = 94213;

    SpellClassOptionsEntry s_optionsMageSpell;

    // A Spell.dbc row with only its id, its school mask and the mage's class options.
    SpellEntry const& MageSpellRow(SpellRowStorage& storage, uint32 spellId, uint32 schoolMask)
    {
        static bool stored = false;
        if (!stored)
        {
            stored = true;
            s_optionsMageSpell.SpellClassMask = ClassFamilyMask(0, 0);
            s_optionsMageSpell.SpellClassSet = SPELLFAMILY_MAGE;
            sSpellClassOptionsStore.SetEntry(kOptionsMageSpell, &s_optionsMageSpell);
        }
        SpellRow(storage, spellId);
        SpellEntry* row = reinterpret_cast<SpellEntry*>(storage.bytes);
        row->SchoolMask = schoolMask;
        row->ClassOptionsID = kOptionsMageSpell;
        return *row;
    }

    // The test-only spell whose one aura, of -50, the tests list under a slow's aura type.
    SpellEntry const* ListedSlowSpell()
    {
        static StoredAuras rows;
        return StoreAuras(rows, kListedSlowSpell, 1, -50);
    }
}

TEST(MageScripts_TormentTheWeakCountsMovementAttackAndCastSlows)
{
    // A negative amount of a movement-speed, an attack-speed or a cast-speed aura, or of one of all speeds (all haste,
    // and the creature slows of all speeds), counts.
    uint32 const slows[] =
    {
        SPELL_AURA_MOD_DECREASE_SPEED, SPELL_AURA_MOD_ATTACKSPEED, SPELL_AURA_MOD_ATTACKSPEED_2, SPELL_AURA_MOD_MELEE_HASTE,
        SPELL_AURA_MOD_RANGED_HASTE, SPELL_AURA_MOD_MELEE_RANGED_HASTE, SPELL_AURA_HASTE_RANGED,
        SPELL_AURA_MOD_MELEE_ATTACK_SPEED, SPELL_AURA_MOD_RANGED_ATTACK_SPEED, SPELL_AURA_MOD_CASTING_SPEED_NOT_STACK,
        SPELL_AURA_HASTE_SPELLS, SPELL_AURA_HASTE_ALL, SPELL_AURA_SLOW_ALL,
    };
    for (uint32 type : slows)
    {
        CHECK(Mage::CountsAsSlow(type, -1));
        CHECK(Mage::CountsAsSlow(type, -60));
        CHECK(!Mage::CountsAsSlow(type, 0));
        CHECK(!Mage::CountsAsSlow(type, 30));
    }

    // A root, a stun, a speed aura of another kind, ranged ammunition haste, a damage-taken aura and a dummy do not.
    for (uint32 type : { SPELL_AURA_MOD_ROOT, SPELL_AURA_MOD_STUN, SPELL_AURA_MOD_INCREASE_SPEED, SPELL_AURA_MOD_SPEED_NOT_STACK,
                         SPELL_AURA_MOD_RANGED_AMMO_HASTE, SPELL_AURA_MOD_DAMAGE_PERCENT_TAKEN, SPELL_AURA_DUMMY })
    {
        CHECK(!Mage::CountsAsSlow(type, -50));
    }
}

TEST(MageScripts_IsSnaredOrSlowedReadsTheAurasOnTheUnit)
{
    SpellEntry const* spell = ListedSlowSpell();
    BareCreature plain;
    CHECK(!Mage::IsSnaredOrSlowed(plain));

    // One aura on the unit, listed under the type: a movement slow as Hamstring's, a cast slow as Mind-numbing
    // Poison's and an attack slow as Thunder Clap's each slow it; a speed aura of a positive amount and a root do not.
    struct Listed
    {
        AuraType type;
        int32 amount;
        bool slowed;
    };
    Listed const cases[] =
    {
        { SPELL_AURA_MOD_DECREASE_SPEED, -50, true }, { SPELL_AURA_HASTE_SPELLS, -30, true },
        { SPELL_AURA_MOD_ATTACKSPEED_2, -20, true }, { SPELL_AURA_MOD_DECREASE_SPEED, 30, false },
        { SPELL_AURA_MOD_ROOT, -50, false },
    };
    for (Listed const& listed : cases)
    {
        BareCreature unit;
        SpellAuraHolder* holder = HolderWithOneAura(spell, unit, EFFECT_INDEX_0, listed.amount);
        unit.ListAura(listed.type, holder->GetAuraByEffectIndex(EFFECT_INDEX_0));
        CHECK_EQ(Mage::IsSnaredOrSlowed(unit), listed.slowed);
        delete holder;
    }
}

TEST(MageScripts_TormentTheWeakMultipliesTheMagesArcaneSpellsAtASlowedTarget)
{
    // 1 + the rank's percent for a mage spell whose school mask holds Arcane, at a target that is snared or slowed.
    CHECK(Near(Mage::TormentTheWeakMultiplier(6, SPELLFAMILY_MAGE, SPELL_SCHOOL_MASK_ARCANE, true), 1.06f));
    CHECK(Near(Mage::TormentTheWeakMultiplier(4, SPELLFAMILY_MAGE, SPELL_SCHOOL_MASK_ARCANE, true), 1.04f));
    CHECK(Near(Mage::TormentTheWeakMultiplier(2, SPELLFAMILY_MAGE, SPELL_SCHOOL_MASK_ARCANE, true), 1.02f));
    CHECK(Near(Mage::TormentTheWeakMultiplier(6, SPELLFAMILY_MAGE, SPELL_SCHOOL_MASK_ARCANE | SPELL_SCHOOL_MASK_FIRE, true),
               1.06f));

    // 1 at a target that is not, for a spell of other schools (Fireball's, Frostfire Bolt's), for a spell of no
    // mage, and with no percent.
    CHECK_EQ(Mage::TormentTheWeakMultiplier(6, SPELLFAMILY_MAGE, SPELL_SCHOOL_MASK_ARCANE, false), 1.0f);
    CHECK_EQ(Mage::TormentTheWeakMultiplier(6, SPELLFAMILY_MAGE, SPELL_SCHOOL_MASK_FIRE, true), 1.0f);
    CHECK_EQ(Mage::TormentTheWeakMultiplier(6, SPELLFAMILY_MAGE, SPELL_SCHOOL_MASK_FIRE | SPELL_SCHOOL_MASK_FROST, true), 1.0f);
    CHECK_EQ(Mage::TormentTheWeakMultiplier(6, SPELLFAMILY_GENERIC, SPELL_SCHOOL_MASK_ARCANE, true), 1.0f);
    CHECK_EQ(Mage::TormentTheWeakMultiplier(0, SPELLFAMILY_MAGE, SPELL_SCHOOL_MASK_ARCANE, true), 1.0f);
}

TEST(MageScripts_TormentTheWeaksRanksRaiseTheDamageAtTheDamageDoneHook)
{
    // The rows: the three ranks at the damage-done hook, one body, not Frostburn's, the same in the game's table.
    SpellHandlerRegistry registry;
    RegisterMageScripts(registry);
    CheckRowsInTheGame<DamageDoneHook>(registry, TORMENT_THE_WEAK_IDS);
    SpellHandler<DamageDoneHook>::Function const torment = registry.Find<DamageDoneHook>(55340);
    for (uint32 rank : TORMENT_THE_WEAK_IDS)
    {
        CHECK(registry.Find<DamageDoneHook>(rank) == torment);
    }
    CHECK(torment != registry.Find<DamageDoneHook>(30451));

    // A mage who holds the third rank (6), as the engine runs the row for the dummy aura he holds: his Arcane spell
    // at a snared Camel deals 6% more, on top of the multiplier so far.
    static StoredAuras rankRows;
    SpellEntry const* rank = StoreAuras(rankRows, 55340, 1, 6);
    SpellEntry const* slow = ListedSlowSpell();
    BareCreature mage;
    SpellAuraHolder* talent = HolderWithOneAura(rank, mage, EFFECT_INDEX_0, 6);
    mage.HoldAura(55340, *talent);
    BareCreature snared;
    SpellAuraHolder* snare = HolderWithOneAura(slow, snared, EFFECT_INDEX_0, -50);
    snared.ListAura(SPELL_AURA_MOD_DECREASE_SPEED, snare->GetAuraByEffectIndex(EFFECT_INDEX_0));
    BareCreature unslowed;
    BareCreature plain;
    SpellRowStorage arcaneRow;
    SpellRowStorage fireRow;
    SpellEntry const& arcane = MageSpellRow(arcaneRow, 30451, SPELL_SCHOOL_MASK_ARCANE);
    SpellEntry const& fire = MageSpellRow(fireRow, 133, SPELL_SCHOOL_MASK_FIRE);

    float mod = 1.5f;
    DamageDoneContext slowed(mage, snared, arcane, SPELL_DIRECT_DAMAGE, mod);
    CHECK(registry.Dispatch<DamageDoneHook>(55340, slowed).IsContinue());
    CHECK(Near(mod, 1.5f * 1.06f));

    // Nothing at a Camel that is not slowed, for his Fire spell, or for a caster without the talent.
    float notSlowed = 1.0f;
    DamageDoneContext freeCtx(mage, unslowed, arcane, SPELL_DIRECT_DAMAGE, notSlowed);
    CHECK(registry.Dispatch<DamageDoneHook>(55340, freeCtx).IsContinue());
    CHECK_EQ(notSlowed, 1.0f);
    float fireMod = 1.0f;
    DamageDoneContext fireCtx(mage, snared, fire, SPELL_DIRECT_DAMAGE, fireMod);
    CHECK(registry.Dispatch<DamageDoneHook>(55340, fireCtx).IsContinue());
    CHECK_EQ(fireMod, 1.0f);
    float plainMod = 1.0f;
    DamageDoneContext plainCtx(plain, snared, arcane, SPELL_DIRECT_DAMAGE, plainMod);
    CHECK(registry.Dispatch<DamageDoneHook>(55340, plainCtx).IsContinue());
    CHECK_EQ(plainMod, 1.0f);
    delete talent;
    delete snare;
}

TEST(MageScripts_OnlyTormentTheWeaksDamageDoneRowsSitOnADummyAura)
{
    // The engine runs a damage-done row for the damage spell and again for each dummy aura its caster holds, keyed by
    // that aura's spell, so a row keyed by a spell that applies a dummy aura would also run for every other spell of
    // a unit holding it. Every damage-done key of the game's table with the auras its spell applies (any effect, any
    // target; Spell.dbc and SpellEffect.dbc of build 15595): only Torment the Weak's three ranks apply
    // SPELL_AURA_DUMMY. A new key fails here until its auras are listed.
    struct KeyAuras
    {
        uint32 spellId;
        uint32 auras[2];
    };
    KeyAuras const keys[] =
    {
        { 116, { 33, 118 } },   { 120, { 33, 118 } },   { 122, { 26, 0 } },     { 133, { 0, 0 } },
        { 1449, { 0, 0 } },     { 2120, { 3, 0 } },     { 2136, { 0, 0 } },     { 2948, { 0, 0 } },
        { 7268, { 0, 0 } },     { 11113, { 33, 0 } },   { 11366, { 3, 0 } },    { 29447, { 4, 0 } },
        { 30451, { 0, 0 } },    { 30455, { 0, 0 } },    { 31661, { 5, 0 } },    { 34913, { 0, 0 } },
        { 42208, { 0, 0 } },    { 44425, { 0, 0 } },    { 44461, { 0, 0 } },    { 44614, { 33, 3 } },
        { 55339, { 4, 0 } },    { 55340, { 4, 0 } },    { 71757, { 0, 0 } },    { 88148, { 3, 0 } },
        { 92315, { 3, 0 } },
    };
    std::set<uint32> listed;
    for (KeyAuras const& key : keys)
    {
        listed.insert(key.spellId);
        bool const dummy = key.auras[0] == uint32(SPELL_AURA_DUMMY) || key.auras[1] == uint32(SPELL_AURA_DUMMY);
        bool torment = false;
        for (uint32 rank : TORMENT_THE_WEAK_IDS)
        {
            torment = torment || rank == key.spellId;
        }
        CHECK(dummy == torment);
    }

    std::set<uint32> registered;
    for (std::pair<uint32, uint32> const& key : SpellHandlerRegistry::Game().Keys())
    {
        if (key.first == DamageDoneHook::Key)
        {
            registered.insert(key.second);
        }
    }
    CHECK_EQ(registered.size(), std::size_t(25));
    CHECK(registered == listed);
}

TEST(MageScripts_NetherVortexSlowsOnlyAtADamagingArcaneBlastHitWithNoSlowOut)
{
    // An Arcane Blast hit or crit that dealt damage, with none of his Slow out; a partly absorbed hit too.
    CHECK(Mage::NetherVortexSlows(30451, PROC_EX_NORMAL_HIT, 900, false));
    CHECK(Mage::NetherVortexSlows(30451, PROC_EX_CRITICAL_HIT, 900, false));
    CHECK(Mage::NetherVortexSlows(30451, PROC_EX_NORMAL_HIT | PROC_EX_ABSORB, 900, false));

    // Never while his Slow is out, for a hit that dealt nothing, the end of the cast, a miss, a resist or an immunity.
    CHECK(!Mage::NetherVortexSlows(30451, PROC_EX_NORMAL_HIT, 900, true));
    CHECK(!Mage::NetherVortexSlows(30451, PROC_EX_NORMAL_HIT | PROC_EX_ABSORB, 0, false));
    for (uint32 procEx : { uint32(PROC_EX_CAST_END), uint32(PROC_EX_MISS), uint32(PROC_EX_RESIST),
                           uint32(PROC_EX_IMMUNE), uint32(PROC_EX_NONE) })
    {
        CHECK(!Mage::NetherVortexSlows(30451, procEx, 900, false));
    }

    // Never for another spell: Fireball, Arcane Barrage, Arcane Missiles and its missile, Arcane Explosion, Frostfire
    // Bolt, or an event with no spell.
    for (uint32 spellId : { 133u, 44425u, 5143u, 7268u, 1449u, 44614u, 0u })
    {
        CHECK(!Mage::NetherVortexSlows(spellId, PROC_EX_NORMAL_HIT, 900, false));
    }
}

TEST(MageScripts_HasSlowOutReadsHisSingleTargetAura)
{
    // Slow (31589) among the single-target auras he tracks; another spell there, or Slow in another tracked list, is
    // not.
    BareCreature mage;
    CHECK(!Mage::HasSlowOut(mage));
    SpellRowStorage slowRow;
    SpellRowStorage polymorphRow;
    SpellEntry const& slow = SpellRow(slowRow, 31589);
    SpellEntry const& polymorph = SpellRow(polymorphRow, 118);
    Unit::TrackedAuraTargetMap& single = mage.GetTrackedAuraTargets(TRACK_AURA_TYPE_SINGLE_TARGET);
    single.insert(std::make_pair(&polymorph, ObjectGuid()));
    CHECK(!Mage::HasSlowOut(mage));
    single.insert(std::make_pair(&slow, ObjectGuid()));
    CHECK(Mage::HasSlowOut(mage));
    single.clear();
    Unit::TrackedAuraTargetMap& limited = mage.GetTrackedAuraTargets(TRACK_AURA_TYPE_TARGET_LIMIT);
    limited.insert(std::make_pair(&slow, ObjectGuid()));
    CHECK(!Mage::HasSlowOut(mage));
    limited.clear();
}

TEST(MageScripts_NetherVortexRefusesEveryEventButAnArcaneBlastHitWithNoSlowOut)
{
    AddProcEffectRows();
    SpellHandlerRegistry registry;
    RegisterMageScripts(registry);
    BareCreature mage;
    BareCreature camel;
    HolderStorage storage;
    SpellAuraHolder& holder = EmptyHolder(storage);
    SpellRowStorage blastRow;
    SpellRowStorage fireballRow;
    SpellRowStorage barrageRow;
    SpellEntry const& blast = SpellRow(blastRow, 30451);
    SpellEntry const& fireball = SpellRow(fireballRow, 133);
    SpellEntry const& barrage = SpellRow(barrageRow, 44425);

    // Refused, nothing cast and no charge: a Fireball or Arcane Barrage hit, Arcane Blast's cast end, its miss, its hit
    // with no target, and an event with no spell; for both ranks.
    struct Event
    {
        SpellEntry const* spell;
        uint32 procEx;
        Unit* victim;
        uint32 damage;
    };
    Event const refused[] =
    {
        { &fireball, PROC_EX_NORMAL_HIT, &camel, 900 }, { &barrage, PROC_EX_CRITICAL_HIT, &camel, 900 },
        { &blast, PROC_EX_CAST_END, &camel, 0 }, { &blast, PROC_EX_MISS, &camel, 0 },
        { &blast, PROC_EX_NORMAL_HIT, NULL, 900 }, { NULL, PROC_EX_NORMAL_HIT, &camel, 900 },
    };
    for (uint32 rank : NETHER_VORTEX_IDS)
    {
        for (Event const& event : refused)
        {
            SpellHandlerOutcome<SpellAuraProcResult> outcome = Proc(registry, mage, rank, EFFECT_INDEX_0, holder,
                                                                    event.spell, event.procEx, 1, event.victim,
                                                                    event.damage);
            CHECK(outcome.IsReturn());
            CHECK_EQ(int(outcome.GetValue()), int(SPELL_AURA_PROC_CANT_TRIGGER));
        }
    }

    // While his Slow is out, Arcane Blast's hit is refused too.
    SpellRowStorage slowRow;
    Unit::TrackedAuraTargetMap& single = mage.GetTrackedAuraTargets(TRACK_AURA_TYPE_SINGLE_TARGET);
    single.insert(std::make_pair(&SpellRow(slowRow, 31589), ObjectGuid()));
    SpellHandlerOutcome<SpellAuraProcResult> slowOut = Proc(registry, mage, 86209, EFFECT_INDEX_0, holder, &blast,
                                                            PROC_EX_NORMAL_HIT, 1, &camel, 900);
    CHECK(slowOut.IsReturn());
    CHECK_EQ(int(slowOut.GetValue()), int(SPELL_AURA_PROC_CANT_TRIGGER));
    single.clear();

    // With none out, the hit goes on to the cast of Slow, which the test's spell store lacks: FAILED, nothing cast.
    CHECK(sSpellStore.LookupEntry(31589) == NULL);
    if (sSpellStore.LookupEntry(31589) == NULL)
    {
        SpellHandlerOutcome<SpellAuraProcResult> reached = Proc(registry, mage, 86209, EFFECT_INDEX_0, holder, &blast,
                                                                PROC_EX_NORMAL_HIT, 1, &camel, 900);
        CHECK(reached.IsReturn());
        CHECK_EQ(int(reached.GetValue()), int(SPELL_AURA_PROC_FAILED));
    }
}

TEST(MageScripts_FocusMagicsBuffIsCastByItsMageAtHimself)
{
    // Focus Magic's buff (54648) has one effect, at its caster: the mage who cast Focus Magic casts it, so it lands on
    // him and never on the unit Focus Magic is on, whose crit procs it.
    BareCreature mage;
    BareCreature holder;
    CHECK(FocusMagicBuffCaster(holder, &mage) == &mage);

    // No mage to cast it, or a Focus Magic its holder cast himself: nobody casts the buff and the proc fails.
    CHECK(FocusMagicBuffCaster(holder, NULL) == NULL);
    CHECK(FocusMagicBuffCaster(holder, &holder) == NULL);
}

TEST(MageScripts_FocusMagicsProcTriggerEffectCastsNothing)
{
    // The row: Focus Magic at the proc hook, the same body in the game's table; its crit-chance effect's legacy row,
    // which casts the buff for the mage, stays in the game's table.
    AddProcEffectRows();
    SpellHandlerRegistry registry;
    RegisterMageScripts(registry);
    CheckRowsInTheGame<OnProcHook>(registry, FOCUS_MAGIC_IDS);
    CHECK(SpellHandlerRegistry::Game().Find<SpellCritChanceAuraProcMageSite>(54646) != NULL);

    // A crit of the unit's Fireball: effect 1, the client's trigger of the buff, ends its proc casting nothing; effect
    // 0, the unit's +3% spell crit, goes on to the crit-chance proc.
    BareCreature holderUnit;
    BareCreature camel;
    HolderStorage storage;
    SpellAuraHolder& holder = EmptyHolder(storage);
    SpellRowStorage fireballRow;
    SpellEntry const& fireball = SpellRow(fireballRow, 133);
    SpellHandlerOutcome<SpellAuraProcResult> trigger = Proc(registry, holderUnit, 54646, EFFECT_INDEX_1, holder, &fireball,
                                                            PROC_EX_CRITICAL_HIT, 0, &camel, 900);
    CHECK(trigger.IsReturn());
    CHECK_EQ(int(trigger.GetValue()), int(SPELL_AURA_PROC_CANT_TRIGGER));
    SpellHandlerOutcome<SpellAuraProcResult> crit = Proc(registry, holderUnit, 54646, EFFECT_INDEX_0, holder, &fireball,
                                                         PROC_EX_CRITICAL_HIT, 3, &camel, 900);
    CHECK(crit.IsContinue());
}

TEST(MageScripts_InvocationGrantsOnlyForACounterspellThatInterruptedACast)
{
    // The fact a hit's interrupt effect gives its proc event: PROC_EX_INTERRUPT, alone or beside the hit's kind.
    CHECK(SpellMgr::InterruptedACast(PROC_EX_INTERRUPT));
    CHECK(SpellMgr::InterruptedACast(PROC_EX_NORMAL_HIT | PROC_EX_INTERRUPT));
    for (uint32 procEx : { uint32(PROC_EX_NONE), uint32(PROC_EX_NORMAL_HIT), uint32(PROC_EX_CRITICAL_HIT),
                           uint32(PROC_EX_CAST_END), uint32(PROC_EX_MISS), uint32(PROC_EX_RESIST),
                           uint32(PROC_EX_IMMUNE) })
    {
        CHECK(!SpellMgr::InterruptedACast(procEx));
    }

    // A hit of Counterspell (2139) that interrupted a cast gives the buff.
    CHECK(Mage::InvocationGrants(2139, PROC_EX_NORMAL_HIT | PROC_EX_INTERRUPT));

    // Its hit on a target that casts nothing, its miss, its resist, its immunity and the end of its cast do not.
    for (uint32 procEx : { uint32(PROC_EX_NORMAL_HIT), uint32(PROC_EX_MISS), uint32(PROC_EX_RESIST),
                           uint32(PROC_EX_IMMUNE), uint32(PROC_EX_CAST_END), uint32(PROC_EX_NONE) })
    {
        CHECK(!Mage::InvocationGrants(2139, procEx));
    }

    // Nor does a hit of any other spell, an interrupt of another class among them (Pummel, 6552; Kick, 1766; Wind
    // Shear, 57994), or an event with no spell.
    for (uint32 spellId : { 133u, 30451u, 118u, 6552u, 1766u, 57994u, 0u })
    {
        CHECK(!Mage::InvocationGrants(spellId, PROC_EX_NORMAL_HIT | PROC_EX_INTERRUPT));
        CHECK(!Mage::InvocationGrants(spellId, PROC_EX_NORMAL_HIT));
    }
}

TEST(MageScripts_InvocationAcceptsACounterspellThatInterruptedACastAndDropsEveryOtherEvent)
{
    // The rows: the two ranks at the check-proc hook, one body, not Arcane Blast's buff's, the same in the game's
    // table; no proc script on the ranks or on the buff (87098), and no check-proc script on the buff.
    SpellHandlerRegistry registry;
    RegisterMageScripts(registry);
    CheckRowsInTheGame<CheckProcHook>(registry, INVOCATION_IDS);
    SpellHandler<CheckProcHook>::Function const invocation = registry.Find<CheckProcHook>(84723);
    CHECK(registry.Find<CheckProcHook>(84722) == invocation);
    CHECK(invocation != registry.Find<CheckProcHook>(36032));
    for (uint32 spellId : { 84722u, 84723u, 87098u })
    {
        CHECK(registry.Find<OnProcHook>(spellId) == NULL);
    }
    CHECK(registry.Find<CheckProcHook>(87098) == NULL);

    // Both ranks answer true for a Counterspell hit that interrupted a cast, so the engine's proc goes on though the
    // hit dealt no damage, and false for every other event: a Counterspell hit on a target that casts nothing, the end
    // of its cast, its miss, a Fireball hit with or without the interrupt mark, and an event with no spell.
    BareCreature mage;
    BareCreature camel;
    HolderStorage storage;
    SpellAuraHolder& holder = EmptyHolder(storage);
    SpellRowStorage rankRow;
    SpellRowStorage counterspellRow;
    SpellRowStorage fireballRow;
    SpellEntry const& counterspell = SpellRow(counterspellRow, 2139);
    SpellEntry const& fireball = SpellRow(fireballRow, 133);
    struct Event
    {
        SpellEntry const* spell;
        uint32 procEx;
        bool accepted;
    };
    Event const events[] =
    {
        { &counterspell, PROC_EX_NORMAL_HIT | PROC_EX_INTERRUPT, true },
        { &counterspell, PROC_EX_NORMAL_HIT, false },
        { &counterspell, PROC_EX_CAST_END, false },
        { &counterspell, PROC_EX_MISS, false },
        { &fireball, PROC_EX_NORMAL_HIT, false },
        { &fireball, PROC_EX_NORMAL_HIT | PROC_EX_INTERRUPT, false },
        { NULL, PROC_EX_NORMAL_HIT | PROC_EX_INTERRUPT, false },
    };
    for (uint32 rank : INVOCATION_IDS)
    {
        for (Event const& event : events)
        {
            CheckProcContext ctx(mage, &camel, SpellRow(rankRow, rank), AuraHandle(holder), event.spell, 0x10000,
                                 event.procEx, 0);
            SpellHandlerOutcome<bool> const outcome = registry.Dispatch<CheckProcHook>(rank, ctx);
            CHECK(outcome.IsReturn());
            CHECK_EQ(outcome.GetValue(), event.accepted);
        }
    }
}

TEST(MageScripts_ImprovedPolymorphStunsOnlyWhenDamageBreaksAPolymorphOnAnUnmarkedTarget)
{
    // Damage broke the Polymorph of a caster who knows a rank, on a target without the marker: the second rank's 3 s
    // stun (83047), the first rank's 1.5 s one (83046); a caster who holds both takes the second's.
    CHECK_EQ(Mage::ImprovedPolymorphStun(AURA_REMOVE_BY_DAMAGE, false, true, false), uint32(83047));
    CHECK_EQ(Mage::ImprovedPolymorphStun(AURA_REMOVE_BY_DAMAGE, true, false, false), uint32(83046));
    CHECK_EQ(Mage::ImprovedPolymorphStun(AURA_REMOVE_BY_DAMAGE, true, true, false), uint32(83047));

    // None for a caster who knows neither rank, and none while the target holds Improved Polymorph Marker (87515).
    CHECK_EQ(Mage::ImprovedPolymorphStun(AURA_REMOVE_BY_DAMAGE, false, false, false), uint32(0));
    CHECK_EQ(Mage::ImprovedPolymorphStun(AURA_REMOVE_BY_DAMAGE, false, true, true), uint32(0));
    CHECK_EQ(Mage::ImprovedPolymorphStun(AURA_REMOVE_BY_DAMAGE, true, false, true), uint32(0));

    // None for a Polymorph that ran out, was dispelled, cancelled, removed by the plain call, replaced, taken off at a
    // death or by a delete, or by any other removal.
    AuraRemoveMode const silent[] =
    {
        AURA_REMOVE_BY_DEFAULT, AURA_REMOVE_BY_STACK, AURA_REMOVE_BY_CANCEL, AURA_REMOVE_BY_DISPEL, AURA_REMOVE_BY_DEATH,
        AURA_REMOVE_BY_DELETE, AURA_REMOVE_BY_SHIELD_BREAK, AURA_REMOVE_BY_EXPIRE, AURA_REMOVE_BY_TRACKING,
        AURA_REMOVE_BY_TARGET_LIMIT,
    };
    for (AuraRemoveMode mode : silent)
    {
        CHECK_EQ(Mage::ImprovedPolymorphStun(mode, true, true, false), uint32(0));
    }
}

TEST(MageScripts_PolymorphsRemovalRunsImprovedPolymorph)
{
    // The rows: the seven forms at the aura-remove hook, one body, the same in the game's table. The forms no mage
    // learns (Polymorph - Penguin 59634, the other Turkey 71319, SelfSheep 53325), the talent's ranks, its stuns and
    // its marker hold no row there or at the aura-apply hook.
    SpellHandlerRegistry registry;
    RegisterMageScripts(registry);
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    CheckRowsInTheGame<AuraRemoveHook>(registry, POLYMORPH_IDS);
    SpellHandler<AuraRemoveHook>::Function const polymorph = registry.Find<AuraRemoveHook>(118);
    for (uint32 spellId : POLYMORPH_IDS)
    {
        CHECK(registry.Find<AuraRemoveHook>(spellId) == polymorph);
    }
    for (uint32 spellId : { 59634u, 71319u, 53325u, 11210u, 12592u, 83046u, 83047u, 87515u })
    {
        CHECK(game.Find<AuraRemoveHook>(spellId) == NULL);
        CHECK(game.Find<AuraApplyHook>(spellId) == NULL);
    }

    // The script lets the removal's boosts run every time. A Polymorph that ran out, was dispelled, cancelled or
    // removed by the plain call casts nothing, and so does a break by damage whose caster cannot be found or knows no
    // rank, or whose target holds the marker. A break by damage of a caster who knows the second rank, on a target
    // without the marker, goes on to the casts of the stun and the marker, which the test's spell store lacks, so
    // nothing is cast.
    BareCreature mage;
    BareCreature camel;
    SpellRowStorage row;
    HolderStorage storage;
    SpellAuraHolder& holder = EmptyHolder(storage);
    AuraRemoveMode const notDamage[] =
    {
        AURA_REMOVE_BY_EXPIRE, AURA_REMOVE_BY_DISPEL, AURA_REMOVE_BY_CANCEL, AURA_REMOVE_BY_DEFAULT,
    };
    for (AuraRemoveMode mode : notDamage)
    {
        AuraRemoveContext removed(camel, &mage, SpellRow(row, 118), AuraHandle(holder), mode);
        CHECK(registry.Dispatch<AuraRemoveHook>(118, removed).IsContinue());
    }
    AuraRemoveContext noCaster(camel, NULL, SpellRow(row, 118), AuraHandle(holder), AURA_REMOVE_BY_DAMAGE);
    CHECK(registry.Dispatch<AuraRemoveHook>(118, noCaster).IsContinue());
    AuraRemoveContext noTalent(camel, &mage, SpellRow(row, 118), AuraHandle(holder), AURA_REMOVE_BY_DAMAGE);
    CHECK(registry.Dispatch<AuraRemoveHook>(118, noTalent).IsContinue());
    HolderStorage talentStorage;
    mage.HoldAura(12592, EmptyHolder(talentStorage));
    CHECK(mage.HasAura(12592));
    BareCreature marked;
    HolderStorage markerStorage;
    marked.HoldAura(87515, EmptyHolder(markerStorage));
    AuraRemoveContext onMarked(marked, &mage, SpellRow(row, 61305), AuraHandle(holder), AURA_REMOVE_BY_DAMAGE);
    CHECK(registry.Dispatch<AuraRemoveHook>(61305, onMarked).IsContinue());
    CHECK(sSpellStore.LookupEntry(83047) == NULL);
    CHECK(sSpellStore.LookupEntry(87515) == NULL);
    AuraRemoveContext broken(camel, &mage, SpellRow(row, 118), AuraHandle(holder), AURA_REMOVE_BY_DAMAGE);
    CHECK(registry.Dispatch<AuraRemoveHook>(118, broken).IsContinue());
    CHECK(!camel.HasAura(83047));
    CHECK(!camel.HasAura(87515));
}

namespace
{
    // Piercing Chill's two ranks (83156, 83157) as the client's data holds them: effect 0 a dummy aura on the caster of
    // 1 and 2, the other enemies a Frostbolt crit chills. Put into the spell and effect stores once for the rest of the
    // run; no other test reads them.
    void AddPiercingChillRanks()
    {
        static ServerSpell ranks[2];
        static bool added = false;
        if (added)
        {
            return;
        }
        added = true;
        uint32 const ids[2] = { 83156, 83157 };
        int32 const others[2] = { 1, 2 };
        for (std::size_t i = 0; i < 2; ++i)
        {
            ranks[i].Entry().ID = ids[i];
            ranks[i].effect.SpellID = ids[i];
            ranks[i].effect.EffectIndex = EFFECT_INDEX_0;
            ranks[i].effect.Effect = SPELL_EFFECT_APPLY_AURA;
            ranks[i].effect.EffectAura = SPELL_AURA_DUMMY;
            ranks[i].effect.EffectBasePoints = others[i];
            ranks[i].effect.ImplicitTarget_0 = TARGET_SELF;
            sSpellStore.SetEntry(ids[i], &ranks[i].Entry());
            AddSpellEffectEntry(&ranks[i].effect);
        }
    }

    // Places `unit` in map 0's world frame at (x, y, 0), in no world.
    void PlaceAt(Unit& unit, float x, float y)
    {
        unit.Place().EnterFrame(Geometry::Frame::World(0, 0), Geometry::Vector3(x, y, 0.0f), 0.0f);
    }

    uint32 const FROSTBOLT_AND_CONE_OF_COLD_IDS[] = { 116, 120 };
    uint32 const PIERCING_CHILL_CHILL_IDS[] = { 83154 };
}

TEST(MageScripts_PiercingChillsCountIsTheRanksEffectValue)
{
    // Rank 1's effect 0 value is 1 and rank 2's 2, as the client's data holds them; the count is read from the store.
    // Piercing Ice, Frostbolt and no spell are no rank.
    AddPiercingChillRanks();
    CHECK_EQ(Mage::PiercingChillCount(83156), int32(1));
    CHECK_EQ(Mage::PiercingChillCount(83157), int32(2));
    CHECK_EQ(Mage::PiercingChillCount(11151), int32(0));
    CHECK_EQ(Mage::PiercingChillCount(116), int32(0));
    CHECK_EQ(Mage::PiercingChillCount(0), int32(0));
}

TEST(MageScripts_NearestOthersKeepsTheNearestEnemiesButNeverTheTarget)
{
    // A target at the origin and four enemies at 3, 5, 8 and 12 yards from it, listed out of order with the target
    // among them, as the engine's area fill lists them.
    BareCreature target;
    BareCreature at3;
    BareCreature at5;
    BareCreature at8;
    BareCreature at12;
    PlaceAt(target, 0.0f, 0.0f);
    PlaceAt(at3, 0.0f, 3.0f);
    PlaceAt(at5, -5.0f, 0.0f);
    PlaceAt(at8, 8.0f, 0.0f);
    PlaceAt(at12, 0.0f, -12.0f);
    auto filled = [&]()
    {
        std::list<Unit*> list;
        list.push_back(&at8);
        list.push_back(&target);
        list.push_back(&at12);
        list.push_back(&at5);
        list.push_back(&at3);
        return list;
    };

    // Rank 1: the one at 3 yards. Rank 2: the ones at 3 and 5 yards, nearest first.
    std::list<Unit*> one = filled();
    Mage::NearestOthers(one, &target, target, 1);
    CHECK_EQ(one.size(), std::size_t(1));
    CHECK(one.front() == &at3);
    std::list<Unit*> two = filled();
    Mage::NearestOthers(two, &target, target, 2);
    CHECK_EQ(two.size(), std::size_t(2));
    CHECK(two.front() == &at3);
    CHECK(two.back() == &at5);

    // A count above the others leaves every other, the target never among them; a count of 0 empties the list.
    std::list<Unit*> all = filled();
    Mage::NearestOthers(all, &target, target, 9);
    CHECK_EQ(all.size(), std::size_t(4));
    CHECK(std::find(all.begin(), all.end(), &target) == all.end());
    CHECK(all.back() == &at12);
    std::list<Unit*> none = filled();
    Mage::NearestOthers(none, &target, target, 0);
    CHECK(none.empty());

    // The target alone, and a unit listed twice: nothing is left of the first, and the second counts once.
    std::list<Unit*> alone(1, &target);
    Mage::NearestOthers(alone, &target, target, 2);
    CHECK(alone.empty());
    std::list<Unit*> twice;
    twice.push_back(&at5);
    twice.push_back(&at5);
    twice.push_back(&at8);
    Mage::NearestOthers(twice, &target, target, 2);
    CHECK_EQ(twice.size(), std::size_t(2));
    CHECK(twice.front() == &at5);
    CHECK(twice.back() == &at8);
}

TEST(MageScripts_PiercingChillsChillKeepsTheCastsTargetAloneAndMakesTheListFinal)
{
    SpellHandlerRegistry registry;
    RegisterMageScripts(registry);
    CheckRowsInTheGame<SelectTargetsHook>(registry, PIERCING_CHILL_CHILL_IDS);
    BareCreature mage;
    BareCreature chilled;
    BareCreature beside;
    BareCreature frostboltTarget;
    SpellRowStorage row;

    // Both of the chill's effects: the cast's target is kept once, every other unit the engine found leaves, the cut
    // count is untouched and the list is final.
    for (SpellEffectIndex eff : { EFFECT_INDEX_0, EFFECT_INDEX_2 })
    {
        std::list<Unit*> targets;
        targets.push_back(&beside);
        targets.push_back(&chilled);
        targets.push_back(&frostboltTarget);
        targets.push_back(&chilled);
        uint32 maxTargets = 0;
        SelectTargetsContext ctx(mage, SpellRow(row, 83154), eff, TARGET_ALL_ENEMY_IN_AREA_INSTANT, targets, maxTargets,
                                 &chilled);
        CHECK(registry.Dispatch<SelectTargetsHook>(83154, ctx).IsReturn());
        CHECK_EQ(targets.size(), std::size_t(1));
        CHECK(targets.front() == &chilled);
        CHECK_EQ(maxTargets, uint32(0));
    }

    // A cast with no unit target, and one whose target the engine did not find: nothing is left.
    std::list<Unit*> noTarget;
    noTarget.push_back(&beside);
    uint32 maxTargets = 0;
    SelectTargetsContext untargeted(mage, SpellRow(row, 83154), EFFECT_INDEX_0, TARGET_ALL_ENEMY_IN_AREA_INSTANT,
                                    noTarget, maxTargets);
    CHECK(registry.Dispatch<SelectTargetsHook>(83154, untargeted).IsReturn());
    CHECK(noTarget.empty());
    std::list<Unit*> notFound;
    notFound.push_back(&beside);
    SelectTargetsContext missing(mage, SpellRow(row, 83154), EFFECT_INDEX_0, TARGET_ALL_ENEMY_IN_AREA_INSTANT,
                                 notFound, maxTargets, &chilled);
    CHECK(registry.Dispatch<SelectTargetsHook>(83154, missing).IsReturn());
    CHECK(notFound.empty());
}

TEST(MageScripts_FrostboltChillsNothingWithoutACritTheTalentOrAnEnemyInReach)
{
    // Frostbolt and Cone of Cold hold one body each at the after-hit hook, on this table and the game's.
    AddPiercingChillRanks();
    SpellHandlerRegistry registry;
    RegisterMageScripts(registry);
    CheckRowsInTheGame<AfterHitHook>(registry, FROSTBOLT_AND_CONE_OF_COLD_IDS);
    CHECK(registry.Find<AfterHitHook>(116) != registry.Find<AfterHitHook>(120));
    SpellRowStorage row;
    BareCreature mage;
    BareCreature target;

    // A hit that is no crit and a heal: nothing is tried. A crit with no rank of the talent: nothing is tried.
    AfterHitContext plain(mage, target, SpellRow(row, 116), 1000, false, 0, 0, false);
    CHECK(registry.Dispatch<AfterHitHook>(116, plain).IsContinue());
    AfterHitContext heal(mage, target, SpellRow(row, 116), 1000, true, 0, 0, true);
    CHECK(registry.Dispatch<AfterHitHook>(116, heal).IsContinue());
    AfterHitContext untalented(mage, target, SpellRow(row, 116), 1000, true, 0, 0, false);
    CHECK(registry.Dispatch<AfterHitHook>(116, untalented).IsContinue());

    // A crit of a mage with rank 2 at a target in no world: no enemy is found around it, so nothing is cast and the
    // target holds nothing.
    HolderStorage talentStorage;
    mage.HoldAura(83157, EmptyHolder(talentStorage));
    CHECK(mage.HasAura(83157));
    AfterHitContext crit(mage, target, SpellRow(row, 116), 1000, true, 0, 0, false);
    CHECK(registry.Dispatch<AfterHitHook>(116, crit).IsContinue());
    CHECK(target.GetSpellAuraHolderMap().empty());
}

TEST(MageScripts_PermafrostHealsTheRanksPercentOfTheHitTruncated)
{
    CHECK_EQ(Mage::PermafrostHeal(800, 15), int32(120));
    CHECK_EQ(Mage::PermafrostHeal(800, 10), int32(80));
    CHECK_EQ(Mage::PermafrostHeal(800, 5), int32(40));
    CHECK_EQ(Mage::PermafrostHeal(801, 15), int32(120));
    CHECK_EQ(Mage::PermafrostHeal(806, 15), int32(120));
    CHECK_EQ(Mage::PermafrostHeal(807, 15), int32(121));
    CHECK_EQ(Mage::PermafrostHeal(800, 0), int32(0));
    CHECK_EQ(Mage::PermafrostHeal(800, -5), int32(0));
    CHECK_EQ(Mage::PermafrostHeal(0, 15), int32(0));
    CHECK_EQ(Mage::PermafrostHeal(4000000000u, 15), int32(600000000));
}

TEST(MageScripts_PermafrostsProcHealsNothingWithoutAPetAndLeavesItsModifiersAlone)
{
    // The three ranks share one body at the proc hook. Effect 1 of rank 3, a flat modifier, is its handler's; the dummy
    // effect of a mage with no pet fails with nothing cast.
    AddProcEffectRows();
    SpellHandlerRegistry registry;
    RegisterMageScripts(registry);
    CHECK(OneProcBody(registry, PERMAFROST_IDS) != NULL);
    BareCreature owner;
    BareCreature victim;
    HolderStorage storage;
    SpellAuraHolder& holder = EmptyHolder(storage);
    CHECK(Proc(registry, owner, 12571, EFFECT_INDEX_1, holder, NULL, PROC_EX_NORMAL_HIT, -10, &victim, 800)
              .IsContinue());
    SpellHandlerOutcome<SpellAuraProcResult> noPet =
        Proc(registry, owner, 12571, EFFECT_INDEX_0, holder, NULL, PROC_EX_NORMAL_HIT, 15, &victim, 800);
    CHECK(noPet.IsReturn());
    CHECK_EQ(int(noPet.GetValue()), int(SPELL_AURA_PROC_FAILED));
    CHECK(owner.GetPet() == NULL);
}

TEST(MageScripts_ImprovedConeOfColdsFreezeIsTheRanksOwn)
{
    CHECK_EQ(Mage::ImprovedConeOfColdFreeze(11190), uint32(83301));
    CHECK_EQ(Mage::ImprovedConeOfColdFreeze(12489), uint32(83302));
    CHECK_EQ(Mage::ImprovedConeOfColdFreeze(120), uint32(0));
    CHECK_EQ(Mage::ImprovedConeOfColdFreeze(83301), uint32(0));
    CHECK_EQ(Mage::ImprovedConeOfColdFreeze(0), uint32(0));
}

TEST(MageScripts_ConeOfColdFreezesNothingForAHealOrWithoutTheTalent)
{
    SpellHandlerRegistry registry;
    RegisterMageScripts(registry);
    SpellRowStorage row;
    BareCreature mage;
    BareCreature target;

    // A heal, and a hit of a mage with no rank of the talent: nothing is cast, and the target holds nothing.
    AfterHitContext heal(mage, target, SpellRow(row, 120), 900, false, 0, 0, true);
    CHECK(registry.Dispatch<AfterHitHook>(120, heal).IsContinue());
    AfterHitContext untalented(mage, target, SpellRow(row, 120), 900, false, 0, 0, false);
    CHECK(registry.Dispatch<AfterHitHook>(120, untalented).IsContinue());
    CHECK(target.GetSpellAuraHolderMap().empty());

    // Rank 2 with its freeze not in the spell store: nothing is cast.
    CHECK(sSpellStore.LookupEntry(83302) == NULL);
    HolderStorage talentStorage;
    mage.HoldAura(12489, EmptyHolder(talentStorage));
    AfterHitContext talented(mage, target, SpellRow(row, 120), 900, true, 0, 0, false);
    CHECK(registry.Dispatch<AfterHitHook>(120, talented).IsContinue());
    CHECK(target.GetSpellAuraHolderMap().empty());
}

TEST(MageScripts_AFrostboltCastStartsTheLockoutOfTheRankTheMageKnowsUnlessOneIsUp)
{
    // The two ranks' texts: "This effect becomes inactive for $83162d after use" (83049) and "$83239d" (83050).
    CHECK_EQ(Mage::EarlyFrostLockout(false, true, false), uint32(83239));
    CHECK_EQ(Mage::EarlyFrostLockout(true, false, false), uint32(83162));
    CHECK_EQ(Mage::EarlyFrostLockout(true, true, false), uint32(83239));
    CHECK_EQ(Mage::EarlyFrostLockout(false, false, false), uint32(0));
    CHECK_EQ(Mage::EarlyFrostLockout(false, true, true), uint32(0));
    CHECK_EQ(Mage::EarlyFrostLockout(true, false, true), uint32(0));

    // The script runs at the cast-start hook for Frostbolt alone, and nowhere else for it.
    SpellHandlerRegistry registry;
    RegisterMageScripts(registry);
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    CHECK(registry.Find<CastStartHook>(116) != NULL);
    CHECK(game.Find<CastStartHook>(116) == registry.Find<CastStartHook>(116));
    CHECK(game.Find<LaunchHook>(116) == NULL);
    CHECK(game.Find<AfterCastHook>(116) == NULL);
    CHECK_EQ(game.CountAt(CastStartHook::Key), std::size_t(1));
}
