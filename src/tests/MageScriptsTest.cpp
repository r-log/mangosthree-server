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
/// refusals of the proc scripts, the count of crits in a row, Hot Streak's chance and streak, the Tier 10 bonus at the
/// removal of Hot Streak's buff, Molten Shields' Blazing Speed at the removal of a Mage Ward and the script's refusals,
/// Ice Lance's damage-done multiplier, Blink's target count, Living Bomb's exits at the dummy aura hook, Pyromaniac's
/// leaves (a Frostfire Bolt burning only with its periodic damage among them) and its rows at the aura-apply and
/// aura-remove hooks, Combustion's exits at the dummy-effect hook, its tick and its DoT's stacking with the auras of
/// its icon, Impact's cleared Fire Blast cooldown, which DoTs Impact spreads, the exits of its spread at the
/// dummy-effect hook, Improved Flamestrike's chance, Blast Wave's refusals at the after-cast hook, Glyph of Frostfire's
/// hit without Molten Fury and its tick, and Frostfire Bolt's effect taken off at its holder's apply and its after-hit
/// exits; the orb's rows are counted here and tested in MageOrbTest. A body that casts, reads a level or creates an
/// item needs a live unit and a spell; the proc scripts' refusals run on bare Creatures (update fields allocated; no
/// map, template or AI), with the effect rows the scripts read added to the effect store as the client's data holds
/// them, and a holder put in the creature's holder map where a script asks whether it holds an aura. The cooldown
/// Impact clears is a player's: a player on a session with no socket, in no world, whose cooldown sinks the test
/// installs. Frostfire Bolt's apply runs on a holder made through its constructor on a bare creature, of three auras
/// whose apply and removal change nothing on the unit.

#include "TestHarness.h"
#include "spells/classes/mage/Mage.h"
#include "spells/handlers/SpellHandlerRegistry.h"
#include "spells/handlers/AurasHandlers.h"
#include "spells/handlers/SpellEffectDummyHandlers.h"
#include "spells/handlers/UnitAuraProcHandlerHandlers.h"
#include "spells/auras/SpellAuras.h"
#include "spells/data/SpellMgr.h"
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
#include <set>
#include <utility>
#include <vector>

namespace
{
    // The labels the class files register at each mage site, in their tables' order.
    uint32 const TRIGGER_SPELL_IDS[] = { 66 };
    uint32 const BOOST_IDS[] = { 74396 };
    uint32 const EFFECT_DUMMY_IDS[] = { 32826, 38194, 42955, 1459, 61316 };
    uint32 const DUMMY_PROC_IDS[] = { 56372, 56375, 64411, 74396 };

    // The ids each proc script holds at the proc hook: every mage spell with the talent's icon (or class mask) and an
    // aura of the type the script stands for, in the client's data.
    uint32 const MAGIC_ABSORPTION_IDS[] = { 29441, 29444 };
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

    // The ids the damage-done and target scripts hold: every mage spell with the arm's icon and a damage effect (Ice
    // Lance), and the one id the arm named (Blink).
    uint32 const ICE_LANCE_IDS[] = { 30455 };
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

    // The id Impact's spread holds at the dummy-effect hook: the spell the Impact buff casts at its Fire Blast's victim.
    uint32 const IMPACT_SPREAD_IDS[] = { 12355 };

    // The orb's ids: Flame Orb and the orb's scan at the dummy-effect hook, the orb's scan at the target hook
    // (MageOrbTest tests their bodies).
    uint32 const ORB_DUMMY_EFFECT_IDS[] = { 82731, 82734 };
    uint32 const ORB_TARGET_IDS[] = { 82734 };

    // The id the after-cast script holds: Blast Wave, which calls the free Flamestrike for Improved Flamestrike.
    uint32 const BLAST_WAVE_IDS[] = { 11113 };

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
            { 29441, 0, SPELL_AURA_DUMMY }, { 29441, 1, SPELL_AURA_MOD_RESISTANCE },
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
    // the target hook, Persistent Shield and Offensive State at the proc hook. Fire: Living Bomb at the dummy aura hook,
    // Hot Streak's buff and Mage Ward at the aura-remove hook, Pyromaniac's eight DoTs and two ranks at the aura-apply and
    // aura-remove hooks, Combustion and Impact's spread at the dummy-effect hook, Blast Wave at the after-cast hook, and
    // Frostfire Bolt at the after-hit hook. Frost: the 74396 boost and proc labels; Ice Lance at the damage-done hook. At
    // the proc hook, Fire: Ignite, Master of Elements, Hot Streak, Blazing Speed and Impact; Frost: Deep Freeze Immunity
    // State and Fingers of Frost; Arcane: Magic Absorption, Arcane Potency and Mana Shield. The orb: Flame Orb at the
    // dummy-effect hook, and the orb's scan at the target and dummy-effect hooks. 64 rows, 64 keys and 3 defaults, none
    // twice.
    SpellHandlerRegistry registry;
    CHECK_EQ(RegisterMageScripts(registry), uint32(67));
    CHECK_EQ(registry.Count(), std::size_t(64));
    CHECK_EQ(registry.CountDefaults(), std::size_t(3));
    CHECK_EQ(registry.CountAt(AuraTriggerSpellMageSite::Key), std::size_t(1));
    CHECK_EQ(registry.CountAt(AuraPeriodicDummyTickMageSite::Key), std::size_t(0));
    CHECK_EQ(registry.CountAt(AuraBoostMageSite::Key), std::size_t(1));
    CHECK_EQ(registry.CountAt(SpellEffectDummyMageSite::Key), std::size_t(5));
    CHECK_EQ(registry.CountAt(DummyAuraProcMageSite::Key), std::size_t(4));
    CHECK_EQ(registry.CountAt(SpellCritChanceAuraProcMageSite::Key), std::size_t(0));
    CHECK_EQ(registry.CountAt(OnProcHook::Key), std::size_t(21));
    CHECK_EQ(registry.CountAt(SelectTargetsHook::Key), std::size_t(2));
    CHECK_EQ(registry.CountAt(DamageDoneHook::Key), std::size_t(1));
    CHECK_EQ(registry.CountAt(DummyAuraHook::Key), std::size_t(1));
    CHECK_EQ(registry.CountAt(AuraApplyHook::Key), std::size_t(10));
    CHECK_EQ(registry.CountAt(AuraRemoveHook::Key), std::size_t(12));
    CHECK_EQ(registry.CountAt(DummyEffectHook::Key), std::size_t(4));
    CHECK_EQ(registry.CountAt(AfterCastHook::Key), std::size_t(1));
    CHECK_EQ(registry.CountAt(AfterHitHook::Key), std::size_t(1));
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
        { &Mage::RegisterFire, 37, 37, 0 },
        { &Mage::RegisterFrost, 7, 7, 0 },
        { &Mage::RegisterArcane, 5, 5, 0 },
        { &Mage::RegisterOrb, 3, 3, 0 },
    };
    for (PerFile const& file : files)
    {
        SpellHandlerRegistry own;
        CHECK_EQ(file.registerRows(own), file.answered);
        CHECK_EQ(own.Count(), file.keys);
        CHECK_EQ(own.CountDefaults(), file.defaults);
    }

    // Registering again on the same table changes nothing: every key and default is taken.
    CHECK_EQ(RegisterMageScripts(registry), uint32(67));
    CHECK_EQ(registry.Count(), std::size_t(64));
    CHECK_EQ(registry.CountDefaults(), std::size_t(3));

    // The game's table holds every one of these keys with the same body, and the same three defaults. The handler
    // files hold the mage sites' other rows: Mirror Image at the periodic dummy tick site, Brain Freeze's Pushing the
    // Limit label (57761) at the boost site, Cold Snap at the dummy-effect site and Focus Magic at the crit-chance proc
    // site. Hot Streak's buff (48108), Combustion (11129), and 28682 and 44401, which the client's data lacks, are no
    // label of the boost site.
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    std::vector<std::pair<uint32, uint32> > const mine = registry.Keys();
    std::vector<std::pair<uint32, uint32> > const all = game.Keys();
    CHECK_EQ(mine.size(), std::size_t(64));
    CHECK(std::includes(all.begin(), all.end(), mine.begin(), mine.end()));
    CheckRowsInTheGame<AuraTriggerSpellMageSite>(registry, TRIGGER_SPELL_IDS);
    CheckRowsInTheGame<AuraBoostMageSite>(registry, BOOST_IDS);
    CheckRowsInTheGame<SpellEffectDummyMageSite>(registry, EFFECT_DUMMY_IDS);
    CheckRowsInTheGame<DummyAuraProcMageSite>(registry, DUMMY_PROC_IDS);
    CHECK(game.FindDefault<AuraTriggerSpellMageSite>() == registry.FindDefault<AuraTriggerSpellMageSite>());
    CHECK(game.FindDefault<AuraPeriodicDummyTickMageSite>() == registry.FindDefault<AuraPeriodicDummyTickMageSite>());
    CHECK(game.FindDefault<AuraBoostMageSite>() == registry.FindDefault<AuraBoostMageSite>());
    CHECK_EQ(game.CountAt(AuraTriggerSpellMageSite::Key), std::size_t(1));
    CHECK_EQ(game.CountAt(AuraPeriodicDummyTickMageSite::Key), std::size_t(1));
    CHECK_EQ(game.CountAt(AuraBoostMageSite::Key), std::size_t(2));
    CHECK_EQ(game.CountAt(SpellEffectDummyMageSite::Key), std::size_t(6));
    CHECK_EQ(game.CountAt(DummyAuraProcMageSite::Key), std::size_t(4));
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

    // The Glyphs of Ice Block and Polymorph, Blessing of Ancient Kings and 74396 each have their own body at the dummy
    // proc site: 4 labels in 4 bodies, and no Ignite rank among them.
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
    CHECK_EQ(procBodies.size(), std::size_t(4));

    // The 74396 boost is the mage files' one label at the boost site, with a body of its own.
    std::set<SpellHandler<AuraBoostMageSite>::Function> boostBodies;
    for (uint32 spellId : BOOST_IDS)
    {
        boostBodies.insert(registry.Find<AuraBoostMageSite>(spellId));
    }
    CHECK(boostBodies.count(NULL) == 0);
    CHECK_EQ(boostBodies.size(), std::size_t(1));
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

    // Each talent's ids share one body, and each talent has its own: 21 ids in 12 bodies, the same in the game's table.
    std::set<SpellHandler<OnProcHook>::Function> bodies;
    bodies.insert(OneProcBody(registry, MAGIC_ABSORPTION_IDS));
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
    CHECK_EQ(bodies.size(), std::size_t(12));
    std::size_t const ids = sizeof(MAGIC_ABSORPTION_IDS) / sizeof(uint32) + sizeof(IGNITE_IDS) / sizeof(uint32) +
                            sizeof(MASTER_OF_ELEMENTS_IDS) / sizeof(uint32) +
                            sizeof(ARCANE_POTENCY_IDS) / sizeof(uint32) + sizeof(HOT_STREAK_IDS) / sizeof(uint32) +
                            sizeof(BLAZING_SPEED_IDS) / sizeof(uint32) + sizeof(MANA_SHIELD_IDS) / sizeof(uint32) +
                            sizeof(PERSISTENT_SHIELD_IDS) / sizeof(uint32) + sizeof(OFFENSIVE_STATE_IDS) / sizeof(uint32) +
                            sizeof(DEEP_FREEZE_IMMUNITY_IDS) / sizeof(uint32) + sizeof(FINGERS_OF_FROST_IDS) / sizeof(uint32) +
                            sizeof(IMPACT_IDS) / sizeof(uint32);
    CHECK_EQ(ids, std::size_t(21));
    CHECK_EQ(registry.CountAt(OnProcHook::Key), ids);
    CHECK_EQ(game.CountAt(OnProcHook::Key), ids);
    CHECK_EQ(game.CountAt(CheckProcHook::Key), std::size_t(0));

    // No script stands for spells no 4.3.4 proc reaches: Burnout's five ranks (no dummy aura), Combustion (11129 applies
    // no aura) and 28682 (absent from the client's data), and Arcane Blast's buff (36032 holds no damage-done aura).
    for (uint32 spellId : { 44449u, 44469u, 44470u, 44471u, 44472u, 11129u, 28682u, 36032u })
    {
        CHECK(game.Find<OnProcHook>(spellId) == NULL);
        CHECK(game.Find<CheckProcHook>(spellId) == NULL);
    }

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

    // Magic Absorption: an owner without mana is refused at the dummy effect; the resistance effect is its handler's.
    owner.SetByteValue(UNIT_FIELD_BYTES_0, 3, POWER_RAGE);
    SpellHandlerOutcome<SpellAuraProcResult> noMana = Proc(registry, owner, 29441, EFFECT_INDEX_0, holder, NULL, PROC_EX_RESIST, 2);
    CHECK(noMana.IsReturn());
    CHECK_EQ(int(noMana.GetValue()), int(SPELL_AURA_PROC_FAILED));
    CHECK(Proc(registry, owner, 29441, EFFECT_INDEX_1, holder, NULL, PROC_EX_RESIST, 0).IsContinue());

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

    // A crit of Living Bomb's explosion, of a Flame Orb hit or of Fire Power's explosion is refused, by its id.
    for (uint32 spellId : { 44461u, 82739u, 83619u })
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

TEST(MageScripts_IceLanceTriplesTheDamageDoneAtAFrozenTarget)
{
    SpellHandlerRegistry registry;
    RegisterMageScripts(registry);
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    for (uint32 spellId : ICE_LANCE_IDS)
    {
        CHECK(registry.Find<DamageDoneHook>(spellId) != NULL);
        CHECK(game.Find<DamageDoneHook>(spellId) == registry.Find<DamageDoneHook>(spellId));
    }
    CHECK_EQ(game.CountAt(DamageDoneHook::Key), sizeof(ICE_LANCE_IDS) / sizeof(uint32));

    // A caster with no aura that lets him treat a target as frozen: an unfrozen target leaves the multiplier, a frozen
    // one triples it; both continue.
    BareCreature caster;
    BareCreature victim;
    SpellRowStorage row;
    float doneTotalMod = 1.5f;
    DamageDoneContext unfrozen(caster, victim, SpellRow(row, 30455), doneTotalMod);
    CHECK(registry.Dispatch<DamageDoneHook>(30455, unfrozen).IsContinue());
    CHECK_EQ(doneTotalMod, 1.5f);
    victim.SetFlag(UNIT_FIELD_AURASTATE, 1 << (AURA_STATE_FROZEN - 1));
    CHECK(victim.IsFrozen());
    DamageDoneContext frozen(caster, victim, SpellRow(row, 30455), doneTotalMod);
    CHECK(registry.Dispatch<DamageDoneHook>(30455, frozen).IsContinue());
    CHECK_EQ(doneTotalMod, 4.5f);
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
    CHECK_EQ(game.CountAt(SelectTargetsHook::Key),
             sizeof(BLINK_IDS) / sizeof(uint32) + sizeof(ORB_TARGET_IDS) / sizeof(uint32));

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
                                                sizeof(PYROMANIAC_IDS) / sizeof(uint32));

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
    // cancel, a broken shield, a tracked aura's replacement, the caster's fourth bomb and a delete do not.
    CHECK(Mage::LivingBombExplodes(AURA_REMOVE_BY_EXPIRE));
    AuraRemoveMode const silent[] =
    {
        AURA_REMOVE_BY_DISPEL, AURA_REMOVE_BY_DEATH, AURA_REMOVE_BY_DEFAULT, AURA_REMOVE_BY_STACK, AURA_REMOVE_BY_CANCEL,
        AURA_REMOVE_BY_SHIELD_BREAK, AURA_REMOVE_BY_TRACKING, AURA_REMOVE_BY_TARGET_LIMIT, AURA_REMOVE_BY_DELETE,
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
    // plain removal, a re-application, a death, a delete, a tracked aura's replacement and a target limit.
    CHECK(Mage::MageWardTriggersBlazingSpeed(AURA_REMOVE_BY_SHIELD_BREAK, true));
    CHECK(!Mage::MageWardTriggersBlazingSpeed(AURA_REMOVE_BY_SHIELD_BREAK, false));
    AuraRemoveMode const silent[] =
    {
        AURA_REMOVE_BY_EXPIRE, AURA_REMOVE_BY_CANCEL, AURA_REMOVE_BY_DISPEL, AURA_REMOVE_BY_DEFAULT,
        AURA_REMOVE_BY_STACK, AURA_REMOVE_BY_DEATH, AURA_REMOVE_BY_DELETE, AURA_REMOVE_BY_TRACKING,
        AURA_REMOVE_BY_TARGET_LIMIT,
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
             sizeof(PYROMANIAC_DOT_IDS) / sizeof(uint32) + sizeof(PYROMANIAC_IDS) / sizeof(uint32));

    // The DoTs' bodies count only for a caster they may touch: an apply with no caster or a caster that is no player,
    // and a removal by re-application or with no caster, change nothing and let the holder's boosts run.
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
        AuraRemoveContext refreshed(target, &caster, SpellRow(row, spellId), AuraHandle(holder), AURA_REMOVE_BY_STACK);
        CHECK(registry.Dispatch<AuraRemoveHook>(spellId, refreshed).IsContinue());
        for (AuraRemoveMode mode : modes)
        {
            AuraRemoveContext removed(target, NULL, SpellRow(row, spellId), AuraHandle(holder), mode);
            CHECK(registry.Dispatch<AuraRemoveHook>(spellId, removed).IsContinue());
        }
    }

    // The talent's bodies take the haste off the mage, which holds none here, and let the holder's boosts run.
    BareCreature mage;
    for (uint32 spellId : PYROMANIAC_IDS)
    {
        AuraApplyContext learned(mage, &mage, SpellRow(row, spellId), AuraHandle(holder), true);
        CHECK(registry.Dispatch<AuraApplyHook>(spellId, learned).IsContinue());
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
    CHECK_EQ(Mage::CombustionTick(pyroblastAndBomb, 2, &ignite, 1.0f), int32(1400));

    // Ignite's tick under 8 mastery points holds Flashburn's 1.224 (2000 x 1.224 = 2448); the multiplier divided out
    // gives the same 1400, which the engine's Flashburn on 83853 then multiplies once. Without the division Ignite's
    // share would count 1224 a second and the tick 1624, and Flashburn would reach it twice.
    Mage::DotShare const masteredIgnite = { 2448, 2000 };
    CHECK_EQ(Mage::CombustionTick(pyroblastAndBomb, 2, &masteredIgnite, 1.224f), int32(1400));
    CHECK_EQ(Mage::CombustionTick(pyroblastAndBomb, 2, &masteredIgnite, 1.0f), int32(1624));

    // A hasted Pyroblast ticks every 2400 ms, so it gives more a second: 1000 + 208.33 + 233.33 = 1441.67, truncated
    // once after the sum.
    Mage::DotShare const hasted[] = { { 500, 2400 }, { 700, 3000 } };
    CHECK_EQ(Mage::CombustionTick(hasted, 2, &ignite, 1.0f), int32(1441));

    // Nothing to combine gives a tick of 0; Ignite alone gives its share a second.
    CHECK_EQ(Mage::CombustionTick(NULL, 0, NULL, 1.0f), int32(0));
    Mage::DotShare const igniteAlone = { 1300, 2000 };
    CHECK_EQ(Mage::CombustionTick(NULL, 0, &igniteAlone, 1.0f), int32(650));

    // A share with no period adds nothing, a multiplier of 0 or below leaves Ignite's share undivided, and a sum below
    // 0 is 0.
    Mage::DotShare const noPeriod[] = { { 500, 0 }, { 700, 3000 } };
    CHECK_EQ(Mage::CombustionTick(noPeriod, 2, NULL, 1.0f), int32(233));
    CHECK_EQ(Mage::CombustionTick(NULL, 0, &igniteAlone, 0.0f), int32(650));
    CHECK_EQ(Mage::CombustionTick(NULL, 0, &igniteAlone, -1.0f), int32(650));
    Mage::DotShare const negative[] = { { -900, 3000 } };
    CHECK_EQ(Mage::CombustionTick(negative, 1, NULL, 1.0f), int32(0));
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
    // Blast Wave holds the after-cast row, in the game's table too; the free Flamestrike and the talent's ranks hold
    // none.
    SpellHandlerRegistry registry;
    RegisterMageScripts(registry);
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    for (uint32 spellId : BLAST_WAVE_IDS)
    {
        CHECK(registry.Find<AfterCastHook>(spellId) != NULL);
        CHECK(game.Find<AfterCastHook>(spellId) == registry.Find<AfterCastHook>(spellId));
    }
    CHECK_EQ(game.CountAt(AfterCastHook::Key), sizeof(BLAST_WAVE_IDS) / sizeof(uint32));
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
    AfterCastContext noTalent(caster, blastWave, 2, &aimedAt, true, 10.0f, 20.0f, 5.0f);
    CHECK(body(noTalent).IsContinue());
    CHECK(caster.GetCurrentSpell(CURRENT_GENERIC_SPELL) == NULL);

    // With rank 2 held: none or one enemy hit, nothing; two enemies hit by a cast with neither a destination nor a
    // unit target, nothing, since the cast has no point of its own.
    HolderStorage talentStorage;
    caster.HoldAura(84674, EmptyHolder(talentStorage));
    for (uint32 enemies : { 0u, 1u })
    {
        AfterCastContext few(caster, blastWave, enemies, &aimedAt, true, 10.0f, 20.0f, 5.0f);
        CHECK(body(few).IsContinue());
        CHECK(caster.GetCurrentSpell(CURRENT_GENERIC_SPELL) == NULL);
    }
    AfterCastContext pointless(caster, blastWave, 2, NULL, false, 0.0f, 0.0f, 0.0f);
    CHECK(body(pointless).IsContinue());
    CHECK(caster.GetCurrentSpell(CURRENT_GENERIC_SPELL) == NULL);

    // A smoke case: two enemies hit with rank 2 held and a point runs the chance and the cast of 88148 at the point.
    // The test's store holds no 88148, so the cast ends at its lookup and builds nothing, and this case cannot tell
    // an attempt from none; scenario 980 pins the cast itself. The caster holds only the talent afterwards.
    AfterCastContext two(caster, blastWave, 2, &aimedAt, true, 10.0f, 20.0f, 5.0f);
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

TEST(MageScripts_FrostfireGlyphShareLeavesOutMoltenFurySoEachTickTakesItOnce)
{
    // A 3000 hit that Molten Fury's third rank raised by 12% at a target under 35% health deals 3360; the share is
    // taken from the 3000 before it, the nearest whole hit, and from 3000 itself, Molten Fury's first rank's 3120 and
    // no bonus at all.
    CHECK_EQ(Mage::FrostfireGlyphHit(3360, 1.12f), uint32(3000));
    CHECK_EQ(Mage::FrostfireGlyphHit(3120, 1.04f), uint32(3000));
    CHECK_EQ(Mage::FrostfireGlyphHit(3000, 1.0f), uint32(3000));
    CHECK_EQ(Mage::FrostfireGlyphHit(0, 1.12f), uint32(0));
    CHECK_EQ(Mage::FrostfireGlyphHit(3000, 0.0f), uint32(3000));

    // At 30% health the tick takes the 12% once, as Aura::PeriodicTick multiplies it: one stack int(22 x 1.12) = 24,
    // three int(66 x 1.12) = 73. A share taken from the raised 3360 would tick int(25 x 1.12) = 28 and int(75 x 1.12)
    // = 84, the bonus twice.
    float const moltenFury = 1.12f;
    int32 const oneStack = Mage::FrostfireGlyphTick(Mage::FrostfireGlyphHit(3360, moltenFury), 3, 4, 1, 1.0f);
    int32 const threeStacks = Mage::FrostfireGlyphTick(Mage::FrostfireGlyphHit(3360, moltenFury), 3, 4, 3, 1.0f);
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
    SpellAuraHolder* withoutGlyph = AuraHolder(spell, target);
    AuraApplyContext plainApply(target, &plain, SpellRow(row, kFrostfireBolt), AuraHandle(*withoutGlyph), true);
    CHECK(registry.Dispatch<AuraApplyHook>(kFrostfireBolt, plainApply).IsContinue());
    CHECK_EQ(EffectsHeld(*withoutGlyph), uint32(0x3));
    delete withoutGlyph;

    // An apply that is not real, and one whose caster cannot be found, take nothing off.
    SpellAuraHolder* untouched = AuraHolder(spell, target);
    AuraApplyContext notReal(target, &glyphed, SpellRow(row, kFrostfireBolt), AuraHandle(*untouched), false);
    CHECK(registry.Dispatch<AuraApplyHook>(kFrostfireBolt, notReal).IsContinue());
    AuraApplyContext noCaster(target, NULL, SpellRow(row, kFrostfireBolt), AuraHandle(*untouched), true);
    CHECK(registry.Dispatch<AuraApplyHook>(kFrostfireBolt, noCaster).IsContinue());
    CHECK_EQ(EffectsHeld(*untouched), uint32(0x7));
    delete untouched;
    CHECK(target.GetSpellAuraHolderMap().empty());
}

TEST(MageScripts_FrostfireBoltsAfterHitHoldsAnUnglyphedBoltAtOneStack)
{
    SpellHandlerRegistry registry;
    RegisterMageScripts(registry);
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    CHECK(registry.Find<AfterHitHook>(kFrostfireBolt) != NULL);
    CHECK(game.Find<AfterHitHook>(kFrostfireBolt) == registry.Find<AfterHitHook>(kFrostfireBolt));
    CHECK_EQ(game.CountAt(AfterHitHook::Key), std::size_t(1));

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

    // With Glyph of Frostfire on the caster the stacks stay; the client data's glyph is not in this binary's spell
    // store, so no share is written either.
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
