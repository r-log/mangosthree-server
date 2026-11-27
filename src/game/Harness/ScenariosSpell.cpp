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

#include "Scenario.h"
#include "Harness.h"
#include "RecordedScenario.h"
#include "SpellRecorder.h"
#include "QuestFixture.h"
#include "SpellScenario.h"
#include "StagedCasterScenario.h"
#include "Creature.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "PlayerRegistry.h"
#include "ScriptMgr.h"
#include "spells/pipeline/Spell.h"
#include "spells/pipeline/SpellRules.h"
#include "spells/auras/SpellAuras.h"
#include "spells/data/SpellMgr.h"
#include "combat/HitInheritance.h"
#include "spells/SpellScriptHooks.h"
#include "spells/handlers/SpellHandlerRegistry.h"
#include "combat/SpellBonus.h"
#include "data/GameConfig.h"
#include "DBCStores.h"
#include "Opcodes.h"
#include "Pet.h"
#include "UpdateData.h"
#include "HarnessAI.h"
#include "WorldSession.h"
#include "WorldPacket.h"
#include "Map.h"
#include "MapManager.h"
#include "MapPhase.h"
#include "maps/CellImpl.h"
#include "maps/GridNotifiers.h"
#include "maps/GridNotifiersImpl.h"
#include "movement/MoveSpline.h"
#include "session/handlers/entities/MovementHandlers.h"
#include "session/handlers/entities/PetHandlers.h"
#include "session/handlers/spells/SpellHandlers.h"
#include "session/packets/spells/CastTargetsPacket.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

// The spell family (decoupling D11, the Unit reopening note 2026-09-28-unit-reopening.md, section
// 3(a)), orders 930 on, up to 1199, after the quest family: what a cast does, recorded on the quest family's
// machinery (Recorder.h) -- every packet the harness player is sent, the watched units' spell state
// at the end of every window and a snap line after every packet -- folded into one
// `digest=<fnv32>` category per scenario, so D11's moves (the handler registry, the switches) can
// claim byte identity for what the family reaches, and only that.
//
// WHAT IT IS NOT. The harness never writes to the character database: a spell bound to an SD3
// hook or a DB spell script, a creature with an AI script, and every achievement that mails are
// refused before any server call and before the spawn; what only the spawned units can answer (a
// holder on either one that can proc) is refused after the spawn and before the cast. A refusal
// prints every category as INVALID(reason). No scenario re-attempts a cast or a proc while a wall-clock cooldown it set is
// uncleared (cooldowns end on time(NULL), which the stepped clock does not move).
//
// THE SCENARIOS. 930 a direct-damage cast on a spawned creature (D11's PR 1). D11's PR 2: 931 an
// aura applied, applied again and removed, and HandleAuraDummy's warrior stance labels; 932 a
// periodic aura ticking to its expiry; 933 one aura each through HandleDummyAuraProc and
// HandleProcTriggerSpellAuraProc, with their charges; 934 the player's cooldowns set and cleared by
// call; 935 a cast interrupted by a stun and by the player mover; 936 Hellfire cast on its own
// caster, which hits him on every cast; 937 Mortar Shot at a unit outside its radius, which hits
// that unit alone; 938 the Creature cooldown model of pets and charmed creatures, recorded as it
// is; 950 a mage staged to level 85 at a fixed spell power casts Fireball five times at a level-85
// target; 951, 952 and 954 a staged Fire mage's proc talents: Hot Streak, Master of Elements and
// Blazing Speed; 955 Combustion's DoT, the per-second sum of his Pyroblast DoT and Ignite on the
// target; 956 Living Bomb's explosion at expiry, and none at a dispel or a removal; 957 Ice Lance at a
// frozen target, an unfrozen one, and with Fingers of Frost;
// 958-960 a warrior, a paladin and a death knight staged to level 85 at a fixed attack power cast Heroic
// Throw, Hammer of Wrath and Death Coil five times at a level-85 target; 961 a priest staged the same way
// with a fixed spell power casts Mortar Shot, a physical spell on the spell path, five times at it; 962 a
// hasted mage's Pyroblast! and its ticks; 963 Pyroblast! cast again before its periodic effect's next tick; 964
// Ignite's ticks under Flashburn on a staged Fire mage with mastery rating; 965 Living Bomb on four targets from
// one mage, and a refresh; 966 Ignite banking two Fireball crits that overlap; 967 a staged Fire mage's Pyroblast
// applies Critical Mass and his Arcane Blast crit gives back mana through Master of Elements; 968 an Impact Fire Blast
// stuns one Camel and copies the mage's DoTs to the two beside it, not the far one; 969 a killing blow on a Fire mage
// with Cauterize, and the burn that follows; 970 Living Bomb's explosion rolls no Arcane Missiles!, and a bomb whose
// target dies does not explode; 971 a staged Fire mage's Flame Orb burns a Camel once a second while it is snared and
// explodes when the snare runs out; 972 a staged Fire mage's three Living Bombs give him Pyromaniac's haste, and the
// first one's end takes it off; 973 the orb of a Fire mage without Fire Power skips a critter and a polymorphed
// enemy and hits the next one; 974 the orb of a mage cast from a height glides to its hover height and flies level;
// 975 the orb of a mage facing downhill glides when the floor falls away under it in flight and flies on after it
// lands; 976 the orb of a mage facing a rising plain climbs with it and stays a yard over the ground; 980 a Blast
// Wave on one Camel calls nothing, on three calls the free Flamestrike of Improved Flamestrike, and
// 991 the same with its first rank, whose free Flamestrike is instant too; 983 a Fire mage with Molten Fury's third
// rank: his Pyroblast's DoT on a target that falls under 35% health between two ticks, and his Fire Blast at it there;
// 984 four Frostfire Bolts under Glyph of Frostfire stack their DoT to three, each tick its stacks' share of the latest
// hit, and do not slow; 977 an unglyphed Frostfire Bolt on a target immune to its slow leaves no DoT, no tick and no
// holder; 985 the Fire passives that change a hit's damage, a cooldown and a debuff; 986 the Fire passives that change
// how a cast goes; 987 six Fire glyphs; 988 Molten Armor's damage on each melee hit a Fire mage takes; 989 the instant
// Pyroblast accepted, refused, swapped in by the cast handler and not swapped; 981 Blazing Speed keeps a Fire mage's
// snare without Molten Shields; 982 with Molten Shields it removes the snare, and a Mage Ward a blow breaks brings
// Blazing Speed; 990 Invisibility's fade takes a tenth of a Fire mage's threat from the Camel each second, its end
// drops the mage from the list, and a cancelled fade keeps what its ticks took; 978 a Fire mage's Combustion under 35%
// health ticks with the Molten Fury of the Pyroblast DoT it combines; 941 Ignite and Glyph of Frostfire's DoT take their
// share of a hit before the target's damage-taken effects, on a Camel under Curse of the Elements and on a bare one.
// 945 Combustion builds its Living Bomb and Pyroblast shares from the mage's spell power and mastery at the cast, over the
// DoTs' unhasted period, with Flashburn once in each share and the Camel's damage-taken effects once in the tick.
// 949 Combustion takes its Ignite and Glyph of Frostfire parts at the mastery the mage has when it casts, after mastery
// rating was added between their hits and the cast.
// 942 a staged Fire mage's Slow and Polymorph casts roll Arcane Missiles! as they end and their hits never roll Impact,
// a Slow that misses rolls Arcane Missiles! and not Impact, and Living Bomb's cast rolls Impact.
// 948 Molten Armor's hit rolls Impact and Arcane Missiles!, never ignites, never starts another Molten Armor, and does
// not break the Polymorph, Blind or Frost Nova on the Camel it hits.
// 943 a hit-capped Frost mage's Water Elemental casts twenty Waterbolts at a Camel three levels above it and none
// misses; 944 a hit-capped Fire mage's Flame Orb hits a Camel three levels above it fifteen times and none misses;
// 946 and 947 are the same with a hit rating below the cap, with the pet's and the orb's share in proportion. 992 a
// staged Frost mage's Ice Lance does double damage to a Camel that Frost Nova froze, and 2000 rolls of
// Unit::IsSpellCrit for Frostbolt crit at the share Shatter's second rank gives the
// sheet chance and Glyph of Frostbolt's 5 together. 993 a staged Frost mage's Frostbolts give Fingers of Frost and
// Replenishment and his Fire Blasts no charge; by call, Blizzard's Chilled and a Frostfire Bolt, bare or glyphed, give
// charges and Frost Armor's Chilled none; Ice Block's glyph resets Frost Nova. 994 Fingers of Frost's two charges:
// each Ice Lance, Deep Freeze and Brain Freeze Frostfire Bolt takes one as it is cast, a missed Ice Lance too, a copy
// cast while the first Ice Lance flies finds none, a bolt keeps a charge it gets after its launch, the glyphed bolt's
// periodic damage put on the Camel after it too, Ice Lance deals a quarter more with one at an unfrozen and at a frozen
// Camel, and the bolt's tick takes no Shatter under the buffs. 995 a staged Frost mage with Brain Freeze: the instant
// Fireball and Frostfire Bolts take Brain Freeze's buff as they are cast, a missed one too, a Fireball cast in full
// keeps the buff that came during its cast, he gets no Arcane Missiles!, and Frost Armor's Chilled gives him no buff.
// 998 a second mage's glyphed Frostfire Bolt on a Camel a first mage's unglyphed bolt slows: the first mage's slow
// stays, and the second mage's holder is sent from the start with its periodic damage alone. 999 a mage's second
// unglyphed Frostfire Bolt on a Camel his first one slows refreshes his slow in one aura update, at one stack.
// 1010 a staged Frost mage's Ring of Frost forms in 3 seconds and freezes the units in its band, a Camel and a hostile
// player standing there as it forms, and a Camel moved in later; never one at its centre, one the pole hides from
// it or one immune to the freeze, which shows Immune once; the player's freeze lasts 8 s and halves his Deep Freeze;
// a dispelled freeze leaves its 3 s marker, after which the unit is frozen again; a second ring ends the first; a hit
// breaks the freeze by a chance that grows with its damage, a shield's part counted, and a reactive hit never does;
// the ring shows the mage and his friend the friendly ring and the hostile player the hostile one.
// 1025 Polymorph and Wyvern Sting on the Camel break on a direct hit and on a periodic tick, not on Molten Armor's
// hit, a heal or a hit a shield takes whole, and Hex breaks on a chance per hit.
// 1026 a root, a fear and Hex on the Camel count the damage a shield absorbed and the damage its damage-reduction
// effects took off a hit toward the chance that the hit breaks them.
// 1027 two Ignite crits of a Fire mage that land in the same server update feed Ignite once, the later share in
// place of the earlier, two that land a few updates apart both feed it, and Pyroblast! leaves Clearcasting on.
// 1028 Clearcasting on a Fire mage is used up by a Fireball and kept through Blood Fury, the free Flamestrike and
// Blizzard's damage, none of which it makes cheaper.
// 996 a staged Frost mage's Deep Freeze at a target immune to stuns deals its damage once, with Frostburn and the
// glyph, uses one Fingers of Frost charge, and the damage's crit chance takes Shatter's factor at a frozen target
// alone. 1000 a staged Frost mage's mastery is 0 without the Mastery passive, his rating and Frost Specialization's -6
// included, and 2 points and his rating's with it; Frostburn follows it.
// 1011 a staged Frost mage's Frostbolt starts Early Frost's lockout as it begins, an interrupted one too; under the
// lockout his Frostbolt takes its full cast time and global cooldown; his global cooldown takes the talent's modifier
// first and haste and the 1.0 to 1.5 s hold only above one second, so the second rank's is 0.9 s with or without
// haste; Glyph of Icy Veins removes a creature's and a player's cast slow and keeps Power Infusion.
// 1012 a staged Frost mage's Frostbolt crit with Piercing Chill 2 chills, in one chill cast, its target and the Camel
// nearest it, at three yards, and never a third Camel at nine yards or one beyond; a hit that is no crit chills none;
// each chill's hit is a roll of Fingers of Frost's row and each chill cast's end one of Brain Freeze's.
// 1013 Permafrost 3 heals the Water Elemental through its own heal for 15% of each Frostbolt and Cone of Cold hit, a
// crit's included, and nothing for Ice Lance and Fire Blast. 1014 Improved Cone of Cold 2 has each Camel the cone hits
// freeze itself for the mage for four seconds, in the frozen state, but a creature immune to roots, which shows Immune
// instead; an Ice Lance with a Fingers of Frost charge then finds its target frozen and spends the charge.
// 1015 a staged Frost mage's Ice Barrier broken by damage roots, with Shattered Barrier, the two Camels within ten
// yards of him and not the third; replaced, dispelled, cancelled or run out it roots none; Glyph of Ice Barrier adds
// 30% to the whole absorb. 1016 Reactive Barrier casts the free Ice Barrier for a swing that leaves him below half
// health, also when he was below half already and when an absorb took the swing whole, and never while Ice Barrier's
// cooldown runs; a spell's hit an absorb takes whole while he is below half casts it too.
// 1017 a staged Frost mage's Flame Orb button casts the Frostfire Orb, whose hits are frostfire, chill their target
// at the talent's second rank only and never ignite; Cold Snap makes it ready again, and it skips a polymorphed enemy;
// the second rank's hits roll his Fingers of Frost and the first rank's do not.
// 1018 a staged Frost mage's fresh Water Elemental starts with Waterbolt's autocast on and Freeze's off, takes the
// client's Assist button, fires Waterbolt on its own at what he attacks, each bolt taking 0.833 of its spell power,
// which is 0.4 of his frost spell power, and turns invisible with him and back with him. 1019 an Assist Water
// Elemental attacks what he attacks, switches to a new target only after he has attacked it for three seconds and
// drops the switch when he attacks its own target again; a Defensive one keeps its target and does not take his; an
// idle Assist one does not attack a Camel he polymorphs.
// 1020 a staged Frost mage's Ice Floes 3 takes 20% off the cooldowns of Frost Nova, Cone of Cold, Ice Block, Ice
// Barrier, Icy Veins and Cold Snap, category cooldowns included, and nothing off Deep Freeze, Ring of Frost and Summon
// Water Elemental; Cold Snap ends all eight and keeps its own; Hypothermia outlasts Cold Snap and refuses Ice Block;
// Glyph of Ice Block ends Frost Nova's cooldown at Ice Block's cast; Frost Nova, Deep Freeze's damage at a creature
// immune to stuns and Cone of Cold deal the client's numbers. 1021 the Frost passives and glyphs on Frostbolt, Ice
// Lance and Cone of Cold: their hits, Permafrost's slows and healing effects, Enduring Winter's cost, Ice Shards'
// range, Piercing Ice's crit, and an Ice Lance at a frozen Camel doubled and raised by Frostburn's 5%. 1022 Blizzard
// under 25% haste ticks eight times in a 6.4 s channel and Ice Shards chills the Camel. 1023 Icy Veins speeds his
// casts by 20% and keeps a hit from pushing them back, and with its glyph takes a snare off him. 1024 Frost Armor
// with its glyphs lasts an hour, cuts his physical damage taken by 15%, chills the Camel that hits him and gives 2%
// mana every 5 s; Ice Barrier absorbs 8069 and 0.87 of his spell power, and a hit it absorbs whole does not push his
// cast back.
// 1029 a staged Frost mage with Improved Freeze 3: his Water Elemental's Freeze, sent as the client's pet bar sends a
// cast at a point, freezes the two Camels around the point and gives him two charges of Fingers of Frost through one
// roll for the cast; cast at a point where no enemy stands, it gives none.
// 1116 a staged Frost mage's Fingers of Frost rolls when a chill of its row lands: by call, a hit's proc event gives
// charges and a cast end's none; ten Frostbolts at a Camel in Ice Block give none; a Cone of Cold and a Blizzard tick
// give at most one charge for each Camel they chill, Blizzard's Chilled among them. 1117 1012's mage and Camels: a
// Frostbolt crit that kills its target chills, in one chill cast, the two Camels nearest where it stood, the second
// at nine yards, and never a third at nine and a half yards or one beyond.
// 1118 a staged Frost mage's Offensive State rolls Arcane Missiles! as each of his harmful casts ends and never at a
// hit: by call, the cast end of a Frostbolt or of Shattered Barrier's freeze gives it, and a Frostbolt's hit or the
// cast end of Frost Armor's Chilled, Blizzard's Chilled, Piercing Chill's chill or Ring of Frost's freeze none; his
// Counterspell at a Camel that casts nothing and his Ice Barrier broken with no Camel in reach of its freeze give it,
// and his Cone of Cold at two Camels gives it once.
// 1120 a staged Frost mage's Frostfire Orb gives him neither Fingers of Frost nor Brain Freeze at the talent's first
// rank, and at its second rank Fingers of Frost from its hits and Brain Freeze from the ends of their casts.
// 1100 a staged Arcane mage's Arcane Missiles: refused without Arcane Missiles!, cast with it, which it uses up, three
// missiles and then five with Improved Arcane Missiles and Missile Barrage, no Arcane Missiles! from the channel, and the
// aura the spell asks for kept on him exactly while Arcane Missiles! is.
// 1101 a staged Arcane mage's Arcane Blast stacks its buff four times, each stack adding to the next Arcane Blast's
// cost, taking from its cast time and adding to its damage and to Arcane Explosion's, more with Glyph of Arcane Blast;
// Arcane Explosion keeps the stacks, and Arcane Barrage and Arcane Missiles take them off at their cast.
// 1103 a staged Arcane mage's Arcane Blast deals 6% more to a Camel under a movement, a cast or an attack slow with
// Torment the Weak 3, and with Nether Vortex 2 slows its target while none of his Slow is out: never a second Camel
// while one holds it, never by refreshing it, never from Fireball or Arcane Barrage.
// 1104 a staged Arcane mage's Counterspell that interrupts a Camel's cast gives him Invocation's buff, at 13% with
// Glyph of Arcane Blast, one at a Camel that casts nothing gives none and a Fireball hit none, and Improved
// Counterspell silences the Camel at every Counterspell.
// 1105 a staged Arcane mage's Polymorph that damage breaks stuns the Camel for Improved Polymorph's rank and marks it,
// a second break while the mark holds stuns nothing, and a Polymorph that runs out, is dispelled or is removed stuns
// nothing and marks nothing.
// 1115 a staged Arcane mage's Polymorph with Glyph of Polymorph takes his Living Bomb and another caster's Blaze off
// the Camel as it lands, and holds past their next tick; without the glyph both stay, and their tick breaks it.
// 1102 a staged Arcane mage's damaging hits give Clearcasting at most once in 15 s, his helpful spells and another
// class's spell none; Clearcasting makes an Arcane Blast free; Clearcasting and Presence of Mind each give Arcane
// Potency's two charges, one taken by each Arcane Blast, Arcane Explosion or Arcane Missiles cast, and both Arcane
// Blasts that take one hit with its crit chance; Presence of Mind makes an Arcane Blast instant and refuses Arcane
// Power and is refused by it; Arcane Missiles! cast during the channel stays through its missiles.
// 1119 a staged Arcane mage's Arcane Missiles and Blizzard each take one charge of Arcane Potency's buff as the channel
// starts and cast its hidden aura (33713) on him, whose crit chance reaches the missiles and Blizzard's ticks for its
// 10 s, after the buff is gone too.
// 1107 a staged Arcane mage with Improved Blink gets its speed from Blink and not from Arcane Brilliance or Mage Armor.
// 1106 a staged Arcane mage's Mana Shield and Mage Ward give Incanter's Absorption's buff at 20% of what they absorb,
// an absorb while it runs adds the old buff's share of the time it has left, Ice Barrier's absorb gives none, a Mana
// Shield that damage uses up knocks back the Camels within 6 yards and not one at 10, a cancelled one knocks back none,
// the use of his Mana Gem gives Improved Mana Gem's buff at 2% of his maximum mana on both of its effects, also with
// its last charge, and the use of Energy Cloak, which casts the same spell, gives none.
// 1122 a staged mage of level 85 casts Ritual of Refreshment and opens the Refreshment Portal of its third rank, whose
// table gives the Conjured Mana Cake.
// 1108 a staged Arcane mage's Invisibility fades in over 3 s, with Prismatic Cloak 3 makes him invisible at once, and
// ends either way when he casts a spell.
// 1121 a staged Arcane mage's Invisibility with Glyph of Invisibility casts Invisibility Speed on him, which raises his
// run speed by its own 40% while he is invisible and goes when his Invisibility is cancelled.
// 1109 a staged Arcane mage's Arcane Missiles fly at the Camel the channel started on: a switch of his selection to
// another Camel during the channel moves no missile, and a cast at a Camel he has not selected fires all its missiles.
// 1110 a staged Arcane mage's Mana Adept adds 12% at full mana, 6% at half and 1.5% a mastery point, Arcane
// Specialization 25% to his Arcane damage and none to Fire, Arcane Tactics 3%, Netherwind Presence 3% haste, Arcane
// Barrage deals its band and 4% more with its glyph, and Glyph of Arcane Missiles adds 5% crit to the missiles.
// 1111 a staged Arcane mage's Arcane Power adds 20% damage and 10% cost for 15 s, refuses Presence of Mind and is
// refused by it; under Glyph of Arcane Power, Blink and Mana Shield start no global cooldown and Mirror Image's is 0;
// Improved Arcane Explosion's first rank makes Arcane Explosion's global cooldown 1.25 s, hasted and held at 1.0 s,
// and its second 1.0 s, neither hasted nor slowed; Arcane Flows and Glyph of Mana Shield shorten their spells'
// cooldowns.
// 1112 a staged Arcane mage's hasted Arcane Missiles keep their three missiles in a shorter channel, and his
// Evocation gives him 15% of his mana four times, hasted in a shorter channel too, and 10% of
// his health each tick with its glyph; Mana Shield absorbs its amount at one mana a point; Mage Armor gives 3% of his
// mana every 5 s, 3.6% with its glyph, and shortens a harmful magic effect on him by 35%.
// 1113 a staged Arcane mage's Slow holds one target, moves to a second and reaches 40 yards with its glyph; his Focus
// Magic goes on another mage and not on him, that mage's hit that does not crit gives him no 54648, and his crit gives
// him 54648 and gives the other mage none; each hit of his Flame Orb casts Arcane Missiles!.
// 1114 a staged Arcane mage's Mirror Image summons (the Arcane images' and those without the glyph) give him three
// images each, his guardians of his level that look like him and take 75% of his spell power; they cast their
// spells at the Camel he selects, Arcane Blast or Frostbolt with Fire Blast, never swing, and go after 30 s; a third
// summon's images cast nothing at the Camel he polymorphs right after it, and the Polymorph holds; a fourth summon's,
// the Fire images', base mana, Fireballs and lowest mana over their 30 s are recorded; a fifth summon's images stop
// the Frostbolts they are casting at the Camel when he polymorphs it, nothing of theirs hits it after, and the
// Polymorph holds.
// 1123 a staged Frost mage's Time Warp hastes him and his Water Elemental and puts Temporal Displacement on both; a
// second Time Warp hastes neither while they hold it, a third neither while he holds Sated and the pet Temporal
// Displacement; with the lockouts taken off, his Bloodlust hastes both and puts Sated on both.
// 1124 a staged Fire mage's Fade takes his threat on the Camel to 0 for its 10 s and keeps him on the Camel's list,
// holds off the threat his Fire Blast earns under it and gives all of it back when it ends; a second Fade does the
// same.
// 1125 a staged Arcane mage gets the Run Speed Marker with Improved Blink's buff, Sprint and Darkflight; it refuses a
// second Sprint while it stands, and goes when each buff ends, long before its own 30 s.
// The guard (Qualify, SpellScenario.h) runs in the Prepare of every scenario of the family but 930 (SpellDirectDamage),
// the scenario's own Prepare or its base class's (StagedCasterScenario through MageProcScenario, AttackPowerScenario),
// before any server call and before the spawn: it fingerprints every spell the scenario reaches and refuses the
// random-pick labels, the random chain targets and any script binding. A scenario is guarded because its Prepare
// calls the guard; no list names them.
// WHERE THEY ARE. This file holds 930-939 and the attack power scenarios 958-961; the mage's scenarios are in
// ScenariosMageFire.cpp, ScenariosMageFrost.cpp and ScenariosMageArcane.cpp, each registered by its own
// Register<Topic>Scenarios. The numbers above are the family's, wherever the scenario's file.
namespace Harness
{
    /**
     * S930 `spell-direct-damage`: a human priest at his created level casts Smite (585) at a
     * spawned, silenced Mountain Cougar (2961) ten yards in front of him, through the CAST_SPELL
     * handler's calls, and the record follows the cast to its end.
     *
     * Smite qualifies (CheckSpellPlan): one SpellEffect row, SCHOOL_DAMAGE at A=6 (TARGET_CHAIN_DAMAGE)
     * and B=0; SPELLFAMILY_PRIEST, so GetSpellRangeAndRadius runs both family switches to their
     * `default:` and none of its per-spell labels; no SD3 binding, no DBS_ON_SPELL chain, no
     * spell_script_target row (mangos3 read on 2026-09-29, and checked again here at run time);
     * a cast time (1.5 s) with no missile speed, so the hit lands inside the cast's own update;
     * a human priest knows it at creation (playercreateinfo_spell). The cougar is level 3 fixed,
     * runs no AI (AIName empty) and binds no script; it is silenced, so it never answers.
     *
     * Pins: SPELL_START and its cast time; the seeded hit roll (SPELL_GO's hit and miss lists); the
     * damage log against the health the target lost (or the miss log, if the roll misses); the mana
     * spent against Spell::CalculatePowerCost; and the snapshot after the cast -- the four
     * current-spell slots empty, the stored cooldown keys, both units' combat state, health, power
     * and holders.
     */
    class SpellDirectDamage : public SpellScenario
    {
    public:
        SpellDirectDamage()
            : SpellScenario("spell-direct-damage", 930,
                            { "template", "castStarted", "hitRoll", "damageLog", "powerSpent", "stateAfter", "noPersistence", "digest" }) {}

        void Prepare() override
        {
            struct St
            {
                ObjectGuid player;
                ObjectGuid target;
                std::string templateOk;
                uint32 levelAtSpawn = 0;
                std::set<uint32> achievementsAtSpawn;
                bool dealsDamage = false;     // the plan's, for noPersistence's damage-criteria backstop
                bool cast = false;
                uint32 castId = 0;
                std::string procSelf, procTarget;
                uint32 holdersSelf = 0, holdersTarget = 0;
                uint32 manaBefore = 0, manaMax = 0, cost = 0;
                uint32 targetHealthBefore = 0, targetHealthMax = 0;
                uint32 slotSpell = 0, slotState = 0;
                uint32 expectedCastTime = 0;
                bool settled = false;
                uint32 manaAfter = 0, targetHealthAfter = 0;
                bool targetAlive = false;
                std::string slotsAfter;
                std::string cooldownsAfter;
                bool selfCombat = false, targetCombat = false;
                std::string selfVictim, targetVictim;
                uint32 holdersSelfAfter = 0, holdersTargetAfter = 0;
            };

            const uint32 spellId = 585;       // Smite
            const uint32 cougar = 2961;       // Mountain Cougar

            // ---- the guard: the spell and the creature, then the achievement closure -------------
            std::map<std::string, int64> fingerprint;
            fingerprint["eff0.present"] = 1;
            fingerprint["eff0.Effect"] = SPELL_EFFECT_SCHOOL_DAMAGE;
            fingerprint["eff0.EffectBasePoints"] = 12;
            fingerprint["eff0.EffectBonusCoefficient"] = 1062937297;    // 0.856
            fingerprint["eff0.EffectChainAmplitude"] = 1065353216;      // 1.0
            fingerprint["eff0.EffectDieSides"] = 5;
            fingerprint["eff0.EffectRealPointsPerLevel"] = 1056964608;  // 0.5
            fingerprint["eff0.ImplicitTarget_0"] = TARGET_CHAIN_DAMAGE;
            fingerprint["eff1.present"] = 0;
            fingerprint["eff2.present"] = 0;
            const SpellPreCheck spellPre = CheckSpellPlan(spellId, cougar, fingerprint);
            if (!spellPre.refusal.empty())
            {
                Log("template refused: %s", spellPre.refusal.c_str());
                Verdict(Invalid(spellPre.refusal));
                return;
            }
            QuestPlan plan;
            plan.quest = 0;                   // no quest: the closure and the level-mail checks only
            plan.classId = CLASS_PRIEST;
            plan.casts.insert(spellId);
            plan.dealsDamage = true;          // DAMAGE_DONE and HIGHEST_HIT_DEALT read as reached
            const QuestPreCheck pre = CheckQuestPlan(plan, std::map<std::string, int64>());
            if (!pre.refusal.empty())
            {
                Log("template refused: %s", pre.refusal.c_str());
                Verdict(Invalid(pre.refusal));
                return;
            }
            Log("template: spell %u, %u SpellEffect fields as recorded (fnv %s); %s; start level %u; %u mail-rewarded achievements walked, none reachable",
                spellId, spellPre.fields, Trace::Hex32(spellPre.fnv).c_str(), spellPre.facts.c_str(), pre.startLevel, pre.mailTrees);

            // ---- the priest, and the cougar ten yards in front of him ----------------------------
            Player* p = SpawnPlayer(P0.x, P0.y, Ground(P0.x, P0.y, P0.z), 0.0f, CLASS_PRIEST);
            if (!p)
            {
                Verdict(Invalid("spawn failed"));
                return;
            }
            const float tx = P0.x + 10.0f;
            Creature* target = Spawn(cougar, tx, P0.y, Ground(tx, P0.y, P0.z), 3.14159f);
            if (!target)
            {
                Verdict(Invalid("the target (2961) did not spawn"));
                return;
            }
            Silence(target);

            auto st = std::make_shared<St>();
            st->player = p->GetObjectGuid();
            st->target = target->GetObjectGuid();
            char text[640];
            snprintf(text, sizeof(text), "OK(spell %u: %u SpellEffect fields as recorded, fnv %s, every field and zero compared; one SCHOOL_DAMAGE row at A=6 B=0; %s; no SD3 binding, no DB spell script, no spell_script_target; target %u runs no AI and binds no script; levels %u..%u cross no mail level; %u mail-rewarded achievements unreachable)",
                     spellId, spellPre.fields, Trace::Hex32(spellPre.fnv).c_str(), spellPre.facts.c_str(), cougar,
                     pre.startLevel, pre.levelBound, pre.mailTrees);
            st->templateOk = text;
            st->levelAtSpawn = p->getLevel();
            st->achievementsAtSpawn = Achievements(p);
            st->dealsDamage = plan.dealsDamage;

            SpellWatch watch;
            watch.target = st->target;
            m_rec.Begin(Name(), p, watch);

            // ---- the cast, as the CAST_SPELL handler makes it ------------------------------------
            At(300, [this, st]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                Creature* target = Get(st->target);
                SpellEntry const* info = sSpellStore.LookupEntry(spellId);
                if (!p || !target || !info) { return; }
                m_rec.Open("cast");
                // What a proc could reach: a holder with proc flags on either unit. None means no
                // proc handler (HandleDummyAuraProc, HandleProcTriggerSpellAuraProc, ...) runs.
                st->procSelf = ProcHolders(p);
                st->procTarget = ProcHolders(target);
                st->holdersSelf = uint32(p->GetSpellAuraHolderMap().size());
                st->holdersTarget = uint32(target->GetSpellAuraHolderMap().size());
                m_rec.Note("holders that can proc: self [" + st->procSelf + "] of " + U(st->holdersSelf) +
                           ", target [" + st->procTarget + "] of " + U(st->holdersTarget));
                if (!st->procSelf.empty() || !st->procTarget.empty())
                {
                    // the scenario claims no proc handler is reached: refused before the cast
                    const std::string why = "holders that can proc: self [" + st->procSelf + "], target [" + st->procTarget +
                                            "] -- the scenario claims no proc handler is reached";
                    Log("refused before the cast: %s", why.c_str());
                    m_rec.End();
                    Verdict(Invalid(why));
                    return;
                }
                st->manaBefore = p->GetPower(POWER_MANA);
                st->manaMax = p->GetMaxPower(POWER_MANA);
                st->cost = Spell::CalculatePowerCost(info, p);
                st->expectedCastTime = GetSpellCastTime(info);
                st->targetHealthBefore = target->GetHealth();
                st->targetHealthMax = target->GetMaxHealth();
                m_rec.Note("mana " + U(st->manaBefore) + "/" + U(st->manaMax) + ", cost " + U(st->cost) +
                           "; target health " + U(st->targetHealthBefore) + "/" + U(st->targetHealthMax));
                st->castId = CastAsHandler(p, spellId, target, 1);
                st->cast = st->castId != 0;
                if (Spell* current = p->GetCurrentSpell(CURRENT_GENERIC_SPELL))
                {
                    st->slotSpell = current->m_spellInfo->ID;
                    st->slotState = current->getState();
                }
                m_rec.Note("generic slot " + U(st->slotSpell) + " state " + U(st->slotState));
                m_rec.Open("cast+tick");
            });

            // ---- the snapshot after the cast ----------------------------------------------------
            At(2300, [this, st]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                Creature* target = Get(st->target);
                if (!p || !target || !st->cast) { return; }
                m_rec.Open("after");
                st->settled = true;
                st->manaAfter = p->GetPower(POWER_MANA);
                st->targetHealthAfter = target->GetHealth();
                st->targetAlive = target->IsAlive();
                for (uint32 slot = 0; slot < CURRENT_MAX_SPELL; ++slot)
                {
                    Spell* s = p->GetCurrentSpell(slot);
                    st->slotsAfter += (slot ? "," : "") + (s ? U(s->m_spellInfo->ID) : std::string("none"));
                }
                std::string cds;
                auto const& cooldowns = p->GetSpellCooldownMgr().GetSpellCooldownMap();
                for (auto c = cooldowns.begin(); c != cooldowns.end(); ++c)
                {
                    cds += (cds.empty() ? "" : ",") + U(c->first);
                }
                st->cooldownsAfter = cds;
                st->selfCombat = p->IsInCombat();
                st->targetCombat = target->IsInCombat();
                st->selfVictim = Trace::RoleOf(m_rec.Roles(), p->getVictim() ? p->getVictim()->GetObjectGuid().GetRawValue() : 0);
                st->targetVictim = Trace::RoleOf(m_rec.Roles(), target->getVictim() ? target->getVictim()->GetObjectGuid().GetRawValue() : 0);
                st->holdersSelfAfter = uint32(p->GetSpellAuraHolderMap().size());
                st->holdersTargetAfter = uint32(target->GetSpellAuraHolderMap().size());
                m_rec.Note("slots " + st->slotsAfter + "; cooldown keys [" + st->cooldownsAfter + "]; mana " + U(st->manaAfter) +
                           "; target health " + U(st->targetHealthAfter));
                m_rec.Open("after+tick");
            });

            // ---- the verdict -----------------------------------------------------------------
            At(2400, [this, st]()
            {
                m_rec.End();   // the sink comes off before the verdict
                Player* p = sPlayerRegistry.Find(st->player);
                if (!p)
                {
                    Verdict(Invalid("the harness player was gone at the verdict"));
                    return;
                }
                char started[512], roll[384], damage[384], power[384], after[640];
                Trace::Roles const& roles = m_rec.Roles();

                // --- castStarted: SPELL_START in the cast window, the generic slot preparing
                std::vector<Recorder::Seen const*> starts = m_rec.SeenIn("cast", SMSG_SPELL_START);
                std::string startRecord;
                if (starts.size() == 1)
                {
                    Trace::DecodeSpellCast(false, starts[0]->payload.empty() ? NULL : &starts[0]->payload[0], starts[0]->payload.size(), roles, startRecord);
                }
                char wantStart[160];
                snprintf(wantStart, sizeof(wantStart), "spell=%u flags=", spellId);
                char wantTime[64];
                snprintf(wantTime, sizeof(wantTime), " source=self caster=self casttime=%u mask=0x2 target=target", st->expectedCastTime);
                if (!st->cast)
                {
                    snprintf(started, sizeof(started), "INVALID(the cast step ran=%d; the spellbook refused %u)", st->castId ? 1 : 0, spellId);
                }
                else if (starts.size() != 1 || startRecord.compare(0, strlen(wantStart), wantStart) != 0 ||
                         startRecord.find(wantTime) == std::string::npos || m_rec.CountIn("cast", SMSG_CAST_FAILED) ||
                         st->slotSpell != spellId || st->slotState != SPELL_STATE_PREPARING)
                {
                    snprintf(started, sizeof(started), "BUG(SPELL_START x%u [%s], CAST_FAILED x%u, generic slot %u state %u)",
                             uint32(starts.size()), startRecord.c_str(), m_rec.CountIn("cast", SMSG_CAST_FAILED), st->slotSpell, st->slotState);
                }
                else
                {
                    snprintf(started, sizeof(started), "OK(one SPELL_START: %s; the generic slot holds %u preparing; no holder on either unit can proc (%u and %u holders))",
                             startRecord.c_str(), st->slotSpell, st->holdersSelf, st->holdersTarget);
                }

                // --- hitRoll: one SPELL_GO, the target in the hit list or the miss list
                std::vector<Recorder::Seen const*> gos = m_rec.SeenIn("cast+tick", SMSG_SPELL_GO);
                std::string goRecord;
                if (gos.size() == 1)
                {
                    Trace::DecodeSpellCast(true, gos[0]->payload.empty() ? NULL : &gos[0]->payload[0], gos[0]->payload.size(), roles, goRecord);
                }
                const bool hit = goRecord.find(" hit=[target] miss=[]") != std::string::npos;
                const bool missed = goRecord.find(" hit=[] miss=[target:") != std::string::npos;
                if (!st->settled)
                {
                    snprintf(roll, sizeof(roll), "INVALID(the snapshot step did not run)");
                }
                else if (gos.size() != 1 || (!hit && !missed))
                {
                    snprintf(roll, sizeof(roll), "BUG(SPELL_GO x%u [%s])", uint32(gos.size()), goRecord.c_str());
                }
                else
                {
                    snprintf(roll, sizeof(roll), "OK(%s: %s)", hit ? "the seeded roll hit" : "the seeded roll missed", goRecord.c_str());
                }

                // --- damageLog: the damage log against the health the target lost, or the miss log
                std::vector<Recorder::Seen const*> logs = m_rec.SeenIn("cast+tick", SMSG_SPELLNONMELEEDAMAGELOG);
                std::vector<Recorder::Seen const*> misses = m_rec.SeenIn("cast+tick", SMSG_SPELLLOGMISS);
                std::string logRecord;
                uint64 logTarget = 0, logAttacker = 0;
                uint32 logSpell = 0, logDamage = 0, logOverkill = 0;
                bool logRead = false;
                if (logs.size() == 1 && !logs[0]->payload.empty())
                {
                    logRead = Trace::ReadSpellDamage(&logs[0]->payload[0], logs[0]->payload.size(), logTarget, logAttacker, logSpell, logDamage, logOverkill);
                    Trace::DecodeSpellDamageLog(&logs[0]->payload[0], logs[0]->payload.size(), roles, logRecord);
                }
                const uint32 lost = st->targetHealthBefore - st->targetHealthAfter;
                if (!st->settled)
                {
                    snprintf(damage, sizeof(damage), "INVALID(the snapshot step did not run)");
                }
                else if (hit)
                {
                    if (!logRead || misses.size() || logSpell != spellId || logTarget != roles.target || logAttacker != roles.self ||
                        logDamage != lost || logOverkill || !st->targetAlive)
                    {
                        snprintf(damage, sizeof(damage), "BUG(damage logs x%u [%s], miss logs x%u; target health %u -> %u, alive %d)",
                                 uint32(logs.size()), logRecord.c_str(), uint32(misses.size()), st->targetHealthBefore, st->targetHealthAfter,
                                 st->targetAlive ? 1 : 0);
                    }
                    else
                    {
                        snprintf(damage, sizeof(damage), "OK(one damage log: %s; the target lost exactly that: %u -> %u of %u)",
                                 logRecord.c_str(), st->targetHealthBefore, st->targetHealthAfter, st->targetHealthMax);
                    }
                }
                else
                {
                    std::string missRecord;
                    if (misses.size() == 1 && !misses[0]->payload.empty())
                    {
                        Trace::DecodeSpellLogMiss(&misses[0]->payload[0], misses[0]->payload.size(), roles, missRecord);
                    }
                    if (misses.size() != 1 || logs.size() || lost)
                    {
                        snprintf(damage, sizeof(damage), "BUG(miss logs x%u [%s], damage logs x%u; target health %u -> %u)",
                                 uint32(misses.size()), missRecord.c_str(), uint32(logs.size()), st->targetHealthBefore, st->targetHealthAfter);
                    }
                    else
                    {
                        snprintf(damage, sizeof(damage), "OK(one miss log: %s; the target lost nothing)", missRecord.c_str());
                    }
                }

                // --- powerSpent: the first power update of the cast's tick against the cost
                uint32 powerUpdates = 0, firstValue = 0;
                bool firstRead = false;
                std::vector<Recorder::Seen const*> powers = m_rec.SeenIn("cast+tick", SMSG_POWER_UPDATE);
                for (size_t i = 0; i < powers.size(); ++i)
                {
                    uint64 unit = 0;
                    uint8 type = 0;
                    uint32 value = 0;
                    if (!powers[i]->payload.empty() &&
                        Trace::ReadPowerUpdate(&powers[i]->payload[0], powers[i]->payload.size(), unit, type, value) &&
                        unit == roles.self && type == POWER_MANA)
                    {
                        if (!powerUpdates++)
                        {
                            firstValue = value;
                            firstRead = true;
                        }
                    }
                }
                if (!st->settled)
                {
                    snprintf(power, sizeof(power), "INVALID(the snapshot step did not run)");
                }
                else if (!st->cost || !firstRead || firstValue != st->manaBefore - st->cost)
                {
                    snprintf(power, sizeof(power), "BUG(mana %u, cost %u; %u power updates for self, the first %u)",
                             st->manaBefore, st->cost, powerUpdates, firstValue);
                }
                else
                {
                    snprintf(power, sizeof(power), "OK(mana %u -> %u: the cost %u (Spell::CalculatePowerCost), in the first of %u power updates; %u at the snapshot)",
                             st->manaBefore, firstValue, st->cost, powerUpdates, st->manaAfter);
                }

                // --- stateAfter: the slots, the cooldown keys, combat, the holders
                if (!st->settled)
                {
                    snprintf(after, sizeof(after), "INVALID(the snapshot step did not run)");
                }
                else if (st->slotsAfter != "none,none,none,none" || st->holdersTargetAfter != st->holdersTarget ||
                         st->holdersSelfAfter != st->holdersSelf || !st->targetAlive)
                {
                    snprintf(after, sizeof(after), "BUG(slots %s; holders self %u -> %u, target %u -> %u; target alive %d)",
                             st->slotsAfter.c_str(), st->holdersSelf, st->holdersSelfAfter, st->holdersTarget, st->holdersTargetAfter,
                             st->targetAlive ? 1 : 0);
                }
                else
                {
                    snprintf(after, sizeof(after), "OK(the four current-spell slots empty; stored cooldown keys [%s]; self in combat %d, victim %s; target in combat %d, victim %s, health %u/%u; holders unchanged, self %u, target %u)",
                             st->cooldownsAfter.c_str(), st->selfCombat ? 1 : 0, st->selfVictim.c_str(), st->targetCombat ? 1 : 0,
                             st->targetVictim.c_str(), st->targetHealthAfter, st->targetHealthMax, st->holdersSelfAfter, st->holdersTargetAfter);
                }

                Verdict(Compose({ st->templateOk, started, roll, damage, power, after,
                                  NoPersistence(p, st->achievementsAtSpawn, st->levelAtSpawn, false, st->dealsDamage), DigestValue("the cast") }));
            });
        }
    };

    /**
     * S931 `spell-aura-apply-reapply-remove`: a spawned, silenced Mountain Cougar (2961) ten yards
     * in front of a human priest casts Web (745, the harness's root) at him, casts it again while
     * the first holds, and the aura is then taken off by call (Unit::RemoveAurasDueToSpell). Then
     * the cougar raises the priest family's creature stances on itself -- Defensive Stance 41101
     * and Defensive Stance 53790, each applied, applied again and taken off -- which is how the
     * scenario reaches Aura::HandleAuraDummy's warrior labels (SpellAuraDummy.cpp:402 and :421 at
     * apply, :704 and :864 in the removal block).
     *
     * Web qualifies: its two SpellEffect rows are an APPLY_AURA MOD_ROOT at A=6 and a
     * TRIGGER_SPELL of 11920 (Net Guard, a 20 s DUMMY aura with no label anywhere) at A=6; no SD3
     * binding, no DBS_ON_SPELL chain, no spell_script_target; SPELLFAMILY_GENERIC but none of
     * GetSpellRangeAndRadius's dynamic labels. The stances: one APPLY_AURA DUMMY at A=1,
     * SPELLFAMILY_WARRIOR, DefenseType 0 (so the self-cast draws no hit roll, UnitCombat.cpp:950);
     * their case bodies cast 41102 or 59526 (Stance Cooldown, a DUMMY effect with no label) and
     * 41105 (Defensive Aura, an area aura on friends in 40 yd) on the cougar, and set its three
     * virtual items; the removal block takes 41105 off. Every one of those spells is fingerprinted.
     *
     * Pins: the AURA_UPDATE slots of the apply, the re-application AS IT IS TODAY (retail, reference
     * section 15.8 item 7: a re-application compares the new duration with the remaining one; the
     * core's behaviour is recorded, not judged) and the removal; the holders and the visible slots
     * after each window; the stance labels' side effects.
     */
    class SpellAuraApplyReapplyRemove : public SpellScenario
    {
    public:
        SpellAuraApplyReapplyRemove()
            : SpellScenario("spell-aura-apply-reapply-remove", 931,
                            { "template", "webApplied", "webReapplied", "webRemoved", "stanceLabels", "noPersistence", "digest" }) {}

        void Prepare() override
        {
            struct St
            {
                ObjectGuid player, cougar;
                std::string templateOk;
                uint32 levelAtSpawn = 0;
                std::set<uint32> achievementsAtSpawn;
                bool dealsDamage = false;
                QuestPlan plan;               // for noPersistence's backstops
                std::string procSelf, procCougar;
                std::string webAfterApply, webAfterReapply, webAfterRemove, guardAfterRemove;
                uint32 slotAfterApply = 255, slotAfterReapply = 255;
                std::string stance1Apply, stance1Again, stance1Removed, aura41105Applied, aura41105Removed;
                std::string stance2Apply, stance2Removed, aura41105Applied2, aura41105Removed2;
                bool ran[8] = { false, false, false, false, false, false, false, false };
            };
            const uint32 WEB = 745, NET_GUARD = 11920, STANCE_A = 41101, STANCE_B = 53790, DEF_AURA = 41105;
            const uint32 cougar = 2961;

            std::vector<SpellPrint> prints = {
                // 745: 49 fields, fnv 43845d6b
                { 745, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 26 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 64 }, { "eff1.EffectBasePoints", 1 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectTriggerSpell", 11920 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 11920: 26 fields, fnv 49f88bfc
                { 11920, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 41101: 26 fields, fnv ba87eed9
                { 41101, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 41102: 26 fields, fnv 1b2f1172
                { 41102, { { "eff0.present", 1 }, { "eff0.Effect", 3 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 41105: 49 fields, fnv a67d9114
                { 41105, { { "eff0.present", 1 }, { "eff0.Effect", 128 }, { "eff0.EffectAura", 87 }, { "eff0.EffectBasePoints", -25 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 127 }, { "eff0.EffectRadiusIndex_0", 23 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 79 }, { "eff1.EffectBasePoints", -10 }, { "eff1.EffectBonusCoefficient", 1065353216 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 127 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 53790: 26 fields, fnv ba87eed9
                { 53790, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 59526: 26 fields, fnv 1b2f1172
                { 59526, { { "eff0.present", 1 }, { "eff0.Effect", 3 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
            QuestPlan plan;
            plan.quest = 0;
            plan.classId = CLASS_PRIEST;
            std::string templateOk;
            if (!Qualify(prints, { cougar }, plan, templateOk))
            {
                return;
            }

            Player* p = SpawnPlayer(P0.x, P0.y, Ground(P0.x, P0.y, P0.z), 0.0f, CLASS_PRIEST);
            if (!p)
            {
                Verdict(Invalid("spawn failed"));
                return;
            }
            StartTimeSync(p);
            const float cx = P0.x + 10.0f;
            Creature* c = Spawn(cougar, cx, P0.y, Ground(cx, P0.y, P0.z), 3.14159f);
            if (!c)
            {
                Verdict(Invalid("the cougar (2961) did not spawn"));
                return;
            }
            Silence(c);
            Park(c);

            auto st = std::make_shared<St>();
            st->player = p->GetObjectGuid();
            st->cougar = c->GetObjectGuid();
            st->templateOk = templateOk;
            st->levelAtSpawn = p->getLevel();
            st->achievementsAtSpawn = Achievements(p);
            st->dealsDamage = plan.dealsDamage;
            st->plan = plan;

            SpellWatch watch;
            watch.caster = st->cougar;
            m_rec.Begin(Name(), p, watch);

            // ---- Web: applied, applied again, taken off ------------------------------------
            At(300, [this, st]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                Creature* c = Get(st->cougar);
                if (!p || !c) { return; }
                m_rec.Open("web");
                st->procSelf = ProcHolders(p);
                st->procCougar = ProcHolders(c);
                m_rec.Note("holders that can proc: self [" + st->procSelf + "], caster [" + st->procCougar + "]");
                c->CastSpell(p, WEB, false);
                m_rec.Note("the cougar casts Web at the priest");
                m_rec.Open("web+tick");
                st->ran[0] = true;
            });
            At(1800, [this, st]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                Creature* c = Get(st->cougar);
                if (!p || !c) { return; }
                st->webAfterApply = HolderOf(p, WEB);
                if (SpellAuraHolder* h = p->GetSpellAuraHolder(WEB)) { st->slotAfterApply = h->GetAuraSlot(); }
                m_rec.Open("web-again");
                m_rec.Note("Web on the priest: " + st->webAfterApply);
                c->CastSpell(p, WEB, false);
                m_rec.Note("the cougar casts Web at him again");
                m_rec.Open("web-again+tick");
                st->ran[1] = true;
            });
            At(3300, [this, st]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                if (!p) { return; }
                st->webAfterReapply = HolderOf(p, WEB);
                if (SpellAuraHolder* h = p->GetSpellAuraHolder(WEB)) { st->slotAfterReapply = h->GetAuraSlot(); }
                m_rec.Open("web-removed");
                m_rec.Note("Web on the priest: " + st->webAfterReapply);
                p->RemoveAurasDueToSpell(WEB);
                st->webAfterRemove = HolderOf(p, WEB);
                st->guardAfterRemove = HolderOf(p, NET_GUARD);
                m_rec.Note("after RemoveAurasDueToSpell(745): Web " + st->webAfterRemove + ", Net Guard " + st->guardAfterRemove);
                m_rec.Open("web-removed+tick");
                st->ran[2] = true;
            });

            // ---- the stances: HandleAuraDummy's warrior labels --------------------------------
            At(3600, [this, st]()
            {
                Creature* c = Get(st->cougar);
                if (!c) { return; }
                m_rec.Open("stance");
                SelfCast(c, STANCE_A);
                st->stance1Apply = HolderOf(c, STANCE_A);
                st->aura41105Applied = HolderOf(c, DEF_AURA);
                m_rec.Note("41101 on the cougar: " + st->stance1Apply + "; 41105: " + st->aura41105Applied);
                m_rec.Open("stance+tick");
                st->ran[3] = true;
            });
            At(3900, [this, st]()
            {
                Creature* c = Get(st->cougar);
                if (!c) { return; }
                m_rec.Open("stance-again");
                SelfCast(c, STANCE_A);
                st->stance1Again = HolderOf(c, STANCE_A);
                m_rec.Note("41101 on the cougar: " + st->stance1Again + "; 41105: " + HolderOf(c, DEF_AURA));
                m_rec.Open("stance-again+tick");
                st->ran[4] = true;
            });
            At(4200, [this, st]()
            {
                Creature* c = Get(st->cougar);
                if (!c) { return; }
                m_rec.Open("stance-removed");
                c->RemoveAurasDueToSpell(STANCE_A);
                st->stance1Removed = HolderOf(c, STANCE_A);
                st->aura41105Removed = HolderOf(c, DEF_AURA);
                m_rec.Note("after RemoveAurasDueToSpell(41101): 41101 " + st->stance1Removed + ", 41105 " + st->aura41105Removed);
                m_rec.Open("stance-removed+tick");
                st->ran[5] = true;
            });
            At(4500, [this, st]()
            {
                Creature* c = Get(st->cougar);
                if (!c) { return; }
                m_rec.Open("stance2");
                SelfCast(c, STANCE_B);
                st->stance2Apply = HolderOf(c, STANCE_B);
                st->aura41105Applied2 = HolderOf(c, DEF_AURA);
                m_rec.Note("53790 on the cougar: " + st->stance2Apply + "; 41105: " + st->aura41105Applied2);
                m_rec.Open("stance2+tick");
                st->ran[6] = true;
            });
            At(4800, [this, st]()
            {
                Creature* c = Get(st->cougar);
                if (!c) { return; }
                m_rec.Open("stance2-removed");
                c->RemoveAurasDueToSpell(STANCE_B);
                st->stance2Removed = HolderOf(c, STANCE_B);
                st->aura41105Removed2 = HolderOf(c, DEF_AURA);
                m_rec.Note("after RemoveAurasDueToSpell(53790): 53790 " + st->stance2Removed + ", 41105 " + st->aura41105Removed2);
                m_rec.Open("stance2-removed+tick");
                st->ran[7] = true;
            });

            // ---- the verdict -----------------------------------------------------------------
            At(5100, [this, st]()
            {
                m_rec.End();
                Player* p = sPlayerRegistry.Find(st->player);
                if (!p)
                {
                    Verdict(Invalid("the harness player was gone at the verdict"));
                    return;
                }
                std::string applied, reapplied, removed, stances;
                const bool allRan = st->ran[0] && st->ran[1] && st->ran[2] && st->ran[3] && st->ran[4] && st->ran[5] && st->ran[6] && st->ran[7];
                if (!allRan)
                {
                    const std::string why = "INVALID(a step did not run)";
                    Verdict(Compose({ st->templateOk, why, why, why, why, why, DigestValue("the first cast") }));
                    return;
                }
                if (!st->procSelf.empty() || !st->procCougar.empty())
                {
                    Verdict(Invalid("holders that can proc: self [" + st->procSelf + "], caster [" + st->procCougar + "]"));
                    return;
                }
                const std::vector<std::string> goes1 = Records("web", SMSG_SPELL_GO);
                const std::vector<std::string> ups1 = Records("web+tick", SMSG_AURA_UPDATE);
                applied = "OK(SPELL_GO " + Joined(goes1) + "; AURA_UPDATE " + Joined(ups1) + "; holder " + st->webAfterApply + " slot " + U(st->slotAfterApply) + ")";
                if (goes1.empty() || st->webAfterApply == "none")
                {
                    applied = "BUG(Web did not hold after the first cast: SPELL_GO " + Joined(goes1) + ", holder " + st->webAfterApply + ")";
                }
                const std::vector<std::string> goes2 = Records("web-again", SMSG_SPELL_GO);
                const std::vector<std::string> ups2 = Records("web-again+tick", SMSG_AURA_UPDATE);
                reapplied = "OK(as it is today: SPELL_GO " + Joined(goes2) + "; AURA_UPDATE " + Joined(ups2) + "; holder " + st->webAfterReapply + " slot " +
                            U(st->slotAfterReapply) + ")";
                const std::vector<std::string> ups3 = Records("web-removed", SMSG_AURA_UPDATE);
                removed = "OK(AURA_UPDATE " + Joined(ups3) + "; Web " + st->webAfterRemove + ", Net Guard " + st->guardAfterRemove + ")";
                if (st->webAfterRemove != "none")
                {
                    removed = "BUG(Web still held after the call: " + st->webAfterRemove + ")";
                }
                stances = "OK(41101: " + st->stance1Apply + " with 41105 " + st->aura41105Applied + "; again " + st->stance1Again + "; removed " +
                          st->stance1Removed + " with 41105 " + st->aura41105Removed + "; 53790: " + st->stance2Apply + " with 41105 " + st->aura41105Applied2 +
                          "; removed " + st->stance2Removed + " with 41105 " + st->aura41105Removed2 + ")";
                if (st->stance1Apply == "none" || st->stance1Removed != "none" || st->stance2Apply == "none" || st->stance2Removed != "none")
                {
                    stances = "BUG" + stances.substr(2);
                }
                Verdict(Compose({ st->templateOk, applied, reapplied, removed, stances,
                                  NoPersistence(p, st->achievementsAtSpawn, st->levelAtSpawn, false, st->dealsDamage, &st->plan), DigestValue("the first cast") }));
            });
        }
    };

    /**
     * S932 `spell-periodic-to-expiry`: a human priest at level 4 (the `.reset level` sequence, in a
     * setup window) who has learned Shadow Word: Pain (589, Player::learnSpell, in the same setup
     * window) casts it at a spawned, silenced Mountain Cougar (2961) ten yards in front of him,
     * through the CAST_SPELL handler's calls, and the record follows the aura through its six
     * ticks to its expiry. Each tick lands in a window of its own, opened half a period before the
     * tick is due, so the record pins one periodic log per tick against the stepped clock.
     *
     * Shadow Word: Pain qualifies: one SpellEffect row, APPLY_AURA PERIODIC_DAMAGE, period 3000 ms,
     * at A=6; SPELLFAMILY_PRIEST; 18 s; no SD3 binding, no DBS_ON_SPELL chain, no
     * spell_script_target; no proc flags (its SpellAuraOptions row has chance 100 and mask 0). The
     * cougar has 71 health at level 3; six ticks do not kill it.
     *
     * Pins: the aura's AURA_UPDATE at the cast and at the expiry, one SMSG_PERIODICAURALOG per tick
     * window with its damage, the target's health after each tick, and the holder gone after the
     * expiry. It reaches no HandleAuraDummy label: Shadow Word: Pain carries no DUMMY aura, and a
     * periodic spell that did would reach one only through its own case body (931 reaches the
     * warrior labels). The cast is made at 400 ms, a step whose seeded hit roll lands.
     */
    class SpellPeriodicToExpiry : public SpellScenario
    {
    public:
        SpellPeriodicToExpiry()
            : SpellScenario("spell-periodic-to-expiry", 932,
                            { "template", "applied", "oneLogPerTick", "expired", "noPersistence", "digest" }) {}

        void Prepare() override
        {
            struct St
            {
                ObjectGuid player, target;
                std::string templateOk;
                uint32 levelAtSpawn = 0, levelSet = 0;
                bool noReachLevel = false;
                std::set<uint32> achievementsAtSpawn;
                bool dealsDamage = false;
                QuestPlan plan;               // for noPersistence's backstops
                bool cast = false, settled = false;
                std::string procSelf, procTarget;
                std::string holderAfterCast, holderAfterExpiry;
                std::vector<uint32> healthAfterTick;
                uint32 healthBefore = 0;
            };
            const uint32 SWP = 589;
            const uint32 cougar = 2961;
            const uint32 level = 4;
            const uint32 kCastAt = 400, kPeriod = 3000, kTicks = 6;

            std::vector<SpellPrint> prints = {
                // 589: 26 fields, fnv cb49aeac
                { 589, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 3 }, { "eff0.EffectAuraPeriod", 3000 }, { "eff0.EffectBasePoints", 5 }, { "eff0.EffectBonusCoefficient", 1042603311 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
            QuestPlan plan;
            plan.quest = 0;
            plan.classId = CLASS_PRIEST;
            plan.level = level;
            plan.spells.insert(SWP);
            plan.casts.insert(SWP);
            plan.dealsDamage = true;
            std::string templateOk;
            QuestPreCheck pre;
            if (!Qualify(prints, { cougar }, plan, templateOk, &pre))
            {
                return;
            }

            Player* p = SpawnPlayer(P0.x, P0.y, Ground(P0.x, P0.y, P0.z), 0.0f, CLASS_PRIEST);
            if (!p)
            {
                Verdict(Invalid("spawn failed"));
                return;
            }
            StartTimeSync(p);
            const float tx = P0.x + 10.0f;
            Creature* target = Spawn(cougar, tx, P0.y, Ground(tx, P0.y, P0.z), 3.14159f);
            if (!target)
            {
                Verdict(Invalid("the target (2961) did not spawn"));
                return;
            }
            Silence(target);
            Park(target);

            auto st = std::make_shared<St>();
            st->player = p->GetObjectGuid();
            st->target = target->GetObjectGuid();
            st->templateOk = templateOk;
            st->levelAtSpawn = p->getLevel();
            st->noReachLevel = pre.levelBound == pre.startLevel;
            st->achievementsAtSpawn = Achievements(p);
            st->dealsDamage = plan.dealsDamage;
            st->plan = plan;

            SpellWatch watch;
            watch.target = st->target;
            m_rec.Begin(Name(), p, watch);
            m_rec.Open("level", false);           // a setup window: logged, never digested
            SetLevelAsResetDoes(p, level);
            p->learnSpell(SWP, false);
            st->levelSet = p->getLevel();

            At(kCastAt, [this, st]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                Creature* target = Get(st->target);
                if (!p || !target) { return; }
                m_rec.Open("cast");
                st->procSelf = ProcHolders(p);
                st->procTarget = ProcHolders(target);
                m_rec.Note("holders that can proc: self [" + st->procSelf + "], target [" + st->procTarget + "]");
                if (!st->procSelf.empty() || !st->procTarget.empty())
                {
                    m_rec.End();
                    Verdict(Invalid("holders that can proc: self [" + st->procSelf + "], target [" + st->procTarget + "]"));
                    return;
                }
                st->healthBefore = target->GetHealth();
                st->cast = CastAsHandler(p, SWP, target, 1) != 0;
                st->holderAfterCast = HolderOf(target, SWP);
                m_rec.Note("Shadow Word: Pain on the target: " + st->holderAfterCast);
                m_rec.Open("cast+tick");
            });
            for (uint32 k = 1; k <= kTicks; ++k)
            {
                // opened half a period before tick k is due: tick k lands inside window "tick<k>"
                At(kCastAt + kPeriod * k - kPeriod / 2, [this, st, k]()
                {
                    Player* p = sPlayerRegistry.Find(st->player);
                    Creature* target = Get(st->target);
                    if (!p || !target || !st->cast) { return; }
                    if (k > 1)
                    {
                        st->healthAfterTick.push_back(target->GetHealth());
                    }
                    m_rec.Open("tick" + U(k));
                });
            }
            At(kCastAt + kPeriod * kTicks + kPeriod / 2, [this, st]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                Creature* target = Get(st->target);
                if (!p || !target || !st->cast) { return; }
                st->healthAfterTick.push_back(target->GetHealth());
                st->holderAfterExpiry = HolderOf(target, SWP);
                st->settled = true;
                m_rec.Open("after");
                m_rec.Note("Shadow Word: Pain on the target: " + st->holderAfterExpiry);
                m_rec.Open("after+tick");
            });

            At(kCastAt + kPeriod * kTicks + kPeriod / 2 + 300, [this, st]()
            {
                m_rec.End();
                Player* p = sPlayerRegistry.Find(st->player);
                if (!p)
                {
                    Verdict(Invalid("the harness player was gone at the verdict"));
                    return;
                }
                std::string applied, perTick, expired;
                if (!st->cast || !st->settled)
                {
                    const std::string why = "INVALID(the cast or the last step did not run)";
                    Verdict(Compose({ st->templateOk, why, why, why, why, DigestValue("the cast") }));
                    return;
                }
                const std::vector<std::string> ups = Records("cast", SMSG_AURA_UPDATE);
                const std::vector<std::string> gos = Records("cast", SMSG_SPELL_GO);
                applied = (st->holderAfterCast != "none" && gos.size() == 1) ? "OK" : "BUG";
                applied += "(SPELL_GO " + Joined(gos) + "; AURA_UPDATE " + Joined(ups) + "; holder " + st->holderAfterCast + "; level " + U(st->levelSet) + ")";
                bool oneEach = Records("cast+tick", SMSG_PERIODICAURALOG).empty();
                std::string logs;
                for (uint32 k = 1; k <= kTicks; ++k)
                {
                    const std::vector<std::string> l = Records("tick" + U(k), SMSG_PERIODICAURALOG);
                    oneEach = oneEach && l.size() == 1;
                    logs += (k > 1 ? " " : "") + U(k) + ":" + Joined(l) + "->h" + (k - 1 < st->healthAfterTick.size() ? U(st->healthAfterTick[k - 1]) : std::string("?"));
                }
                oneEach = oneEach && Records("after+tick", SMSG_PERIODICAURALOG).empty() && Records("after", SMSG_PERIODICAURALOG).empty();
                perTick = std::string(oneEach ? "OK" : "BUG") + "(from health " + U(st->healthBefore) + ": " + logs + ")";
                const std::vector<std::string> upsLast = Records("tick" + U(kTicks), SMSG_AURA_UPDATE);
                expired = std::string(st->holderAfterExpiry == "none" ? "OK" : "BUG") + "(the holder after the last tick: " + st->holderAfterExpiry +
                          "; AURA_UPDATE in the last tick window " + Joined(upsLast) + ")";
                Verdict(Compose({ st->templateOk, applied, perTick, expired,
                                  NoPersistence(p, st->achievementsAtSpawn, st->levelSet, st->noReachLevel, st->dealsDamage, &st->plan), DigestValue("the cast") }));
            });
        }
    };

    /// Moves `p` to the point `at` of the map `mapId` the way Scenario::SpawnPlayer places a player:
    /// the map's own removal, then SetMap and Map::Add.
    /// @return whether `p` is in the world on that map
    bool MoveToMap(Player* p, uint32 mapId, Pt const& at)
    {
        Map* from = p->GetMap();
        Map* to = sMapMgr.FindMap(mapId, 0);
        if (!from || !to)
        {
            return false;
        }
        from->Remove(p, false);
        p->SetMap(to);
        p->Place().MoveTo(at.x, at.y, at.z, 0.0f);
        const bool added = to->Add(p);
        return added && p->IsInWorld() && p->GetMapId() == mapId;
    }
    /**
     * S939 `spell-periodic-off-map`: S932's priest and cougar, with the priest gone to another map
     * between the cast and the first tick. The priest casts Shadow Word: Pain (589) at the silenced
     * cougar, again a tenth of a second later while the hit roll misses (each step draws its own seeded
     * roll; the missed attempt's global cooldown is cancelled by call), then moves to Deepholm (map
     * 646) the way Scenario::SpawnPlayer places a player: the map's own removal, then SetMap and
     * Map::Add. No creature or game object is spawned within 800 yards of the spot there, and no vessel
     * route leaves or enters that map, so no other unit's updates reach the priest: a vessel crossing
     * between maps is announced to every player on them at a moment of the wall clock, which no seed
     * controls. A harness player never logged in (no social list), so the worldport's arrival packets
     * are not theirs to send. The aura keeps ticking on the cougar while its caster stands on the other
     * map, through its six ticks to its expiry, and the priest moves back to the harness map before the
     * verdict.
     *
     * Pins: the aura applied on the harness map, the priest in the world on map 646 before the first
     * tick, the cougar's health falling at each of the six ticks (the last one at the expiry), the
     * holder gone after the expiry, and the ownership violations counted while the aura ran
     * (MapPhase::Violations()): none, because an aura whose caster is a player on another map waits
     * for the world thread (OffMapAuraUpdates).
     */
    class SpellPeriodicOffMap : public SpellScenario
    {
    public:
        SpellPeriodicOffMap()
            : SpellScenario("spell-periodic-off-map", 939,
                            { "template", "applied", "offMap", "everyTick", "expired", "ownership", "noPersistence", "digest" }) {}

        void Prepare() override
        {
            struct St
            {
                ObjectGuid player, target;
                std::string templateOk;
                uint32 levelSet = 0;
                bool noReachLevel = false;
                std::set<uint32> achievementsAtSpawn;
                bool dealsDamage = false;
                QuestPlan plan;
                bool cast = false, moved = false, settled = false, back = false;
                std::string holderAfterCast, holderAfterExpiry, mapAfterMove;
                uint32 healthBefore = 0;
                std::vector<uint32> healthAfterTick;
                uint32 violationsBefore = 0, violationsAfter = 0;
            };
            const uint32 SWP = 589;
            const uint32 cougar = 2961;
            const uint32 level = 4;
            const uint32 kCastAt = 400, kCastRetry = 100, kCastAttempts = 7, kMoveAt = 1400, kPeriod = 3000, kTicks = 6;
            const uint32 kOtherMap = 646, kHarnessMap = GetMap() ? GetMap()->GetId() : 1;
            const Pt kQuietSpot = { -1362.0f, 1299.0f, 300.0f };

            std::vector<SpellPrint> prints = {
                // 589: 26 fields, fnv cb49aeac
                { 589, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 3 }, { "eff0.EffectAuraPeriod", 3000 }, { "eff0.EffectBasePoints", 5 }, { "eff0.EffectBonusCoefficient", 1042603311 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
            QuestPlan plan;
            plan.quest = 0;
            plan.classId = CLASS_PRIEST;
            plan.level = level;
            plan.spells.insert(SWP);
            plan.casts.insert(SWP);
            plan.dealsDamage = true;
            std::string templateOk;
            QuestPreCheck pre;
            if (!Qualify(prints, { cougar }, plan, templateOk, &pre))
            {
                return;
            }

            Player* p = SpawnPlayer(P0.x, P0.y, Ground(P0.x, P0.y, P0.z), 0.0f, CLASS_PRIEST);
            if (!p)
            {
                Verdict(Invalid("spawn failed"));
                return;
            }
            StartTimeSync(p);
            const float tx = P0.x + 10.0f;
            Creature* target = Spawn(cougar, tx, P0.y, Ground(tx, P0.y, P0.z), 3.14159f);
            if (!target)
            {
                Verdict(Invalid("the target (2961) did not spawn"));
                return;
            }
            Silence(target);
            Park(target);

            auto st = std::make_shared<St>();
            st->player = p->GetObjectGuid();
            st->target = target->GetObjectGuid();
            st->templateOk = templateOk;
            st->noReachLevel = pre.levelBound == pre.startLevel;
            st->achievementsAtSpawn = Achievements(p);
            st->dealsDamage = plan.dealsDamage;
            st->plan = plan;

            SpellWatch watch;
            watch.target = st->target;
            m_rec.Begin(Name(), p, watch);
            m_rec.Open("level", false);           // a setup window: logged, never digested
            SetLevelAsResetDoes(p, level);
            p->learnSpell(SWP, false);
            st->levelSet = p->getLevel();

            for (uint32 attempt = 0; attempt < kCastAttempts; ++attempt)
            {
            At(kCastAt + kCastRetry * attempt, [this, st, attempt]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                Creature* target = Get(st->target);
                if (!p || !target || st->cast) { return; }
                if (attempt == 0)
                {
                    m_rec.Open("cast");
                }
                else
                {
                    // the missed attempt's global cooldown would refuse this one
                    p->GetGlobalCooldownMgr().CancelGlobalCooldown(sSpellStore.LookupEntry(SWP));
                }
                const std::string procSelf = ProcHolders(p), procTarget = ProcHolders(target);
                if (!procSelf.empty() || !procTarget.empty())
                {
                    m_rec.End();
                    Verdict(Invalid("holders that can proc: self [" + procSelf + "], target [" + procTarget + "]"));
                    return;
                }
                st->healthBefore = target->GetHealth();
                st->violationsBefore = MapPhase::Violations();
                CastAsHandler(p, SWP, target, 1);
                st->holderAfterCast = HolderOf(target, SWP);
                st->cast = st->holderAfterCast != "none";
                m_rec.Note("Shadow Word: Pain on the target after attempt " + U(attempt + 1) + ": " + st->holderAfterCast);
            });
            }
            At(kMoveAt, [this, st, kQuietSpot]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                if (!p || !st->cast) { return; }
                m_rec.Open("move", false);        // the map change's own packets: logged, never digested
                st->moved = MoveToMap(p, kOtherMap, kQuietSpot);
                st->mapAfterMove = U(p->GetMapId()) + (p->IsInWorld() ? " in world" : " not in world");
                m_rec.Note("the priest after the transfer: map " + st->mapAfterMove);
                m_rec.Open("away");
            });
            for (uint32 k = 2; k <= kTicks; ++k)
            {
                // half a period before tick k is due: the health tick k-1 left
                At(kCastAt + kPeriod * k - kPeriod / 2, [this, st]()
                {
                    Creature* target = Get(st->target);
                    if (!target || !st->moved) { return; }
                    st->healthAfterTick.push_back(target->GetHealth());
                });
            }
            At(kCastAt + kPeriod * kTicks + kPeriod / 2, [this, st, kHarnessMap]()
            {
                Creature* target = Get(st->target);
                if (!target || !st->moved) { return; }
                st->healthAfterTick.push_back(target->GetHealth());
                st->holderAfterExpiry = HolderOf(target, SWP);
                st->violationsAfter = MapPhase::Violations();
                st->settled = true;
                m_rec.Open("after");
                m_rec.Note("Shadow Word: Pain on the target: " + st->holderAfterExpiry);
                if (Player* p = sPlayerRegistry.Find(st->player))
                {
                    m_rec.Open("back", false);
                    st->back = MoveToMap(p, kHarnessMap, P0);
                }
            });

            At(kCastAt + kPeriod * kTicks + kPeriod / 2 + 300, [this, st]()
            {
                m_rec.End();
                Player* p = sPlayerRegistry.Find(st->player);
                if (!p)
                {
                    Verdict(Invalid("the harness player was gone at the verdict"));
                    return;
                }
                if (!st->cast || !st->settled)
                {
                    const std::string why = "INVALID(the cast, the transfer or the last step did not run)";
                    Verdict(Compose({ st->templateOk, why, why, why, why, why, why, DigestValue("the cast") }));
                    return;
                }
                const std::string applied = std::string(st->holderAfterCast != "none" ? "OK" : "BUG") + "(holder " + st->holderAfterCast + ")";
                const std::string offMap = std::string(st->moved && st->back ? "OK" : "BUG") + "(the priest on map " + st->mapAfterMove + (st->back ? ", then back" : ", not back") + ")";
                bool falling = st->healthAfterTick.size() == kTicks;
                std::string healths = U(st->healthBefore);
                uint32 last = st->healthBefore;
                for (size_t k = 0; k < st->healthAfterTick.size(); ++k)
                {
                    falling = falling && st->healthAfterTick[k] < last;
                    last = st->healthAfterTick[k];
                    healths += "->" + U(st->healthAfterTick[k]);
                }
                const std::string everyTick = std::string(falling ? "OK" : "BUG") + "(health " + healths + ")";
                const std::string expired = std::string(st->holderAfterExpiry == "none" ? "OK" : "BUG") + "(the holder after the last tick: " + st->holderAfterExpiry + ")";
                const uint32 violations = st->violationsAfter - st->violationsBefore;
                const std::string ownership = std::string(violations == 0 ? "OK" : "BUG") + "(" + U(violations) + " ownership violations while the aura ran)";
                Verdict(Compose({ st->templateOk, applied, offMap, everyTick, expired, ownership,
                                  NoPersistence(p, st->achievementsAtSpawn, st->levelSet, st->noReachLevel, st->dealsDamage, &st->plan), DigestValue("the cast") }));
            });
        }
    };

    /**
     * S933 `spell-proc-charges`: two auras with charges on a human priest, each proc'd by a spell a
     * spawned, silenced Mountain Cougar (2961) ten yards in front of him lands on him -- Burning
     * Shadows (40739), one point of fire damage with no SpellCategories row, so no hit roll
     * (DefenseType 0, UnitCombat.cpp:950) and PROC_FLAG_TAKEN_NEGATIVE_SPELL_HIT on the victim
     * (SpellTargetList.cpp:514-515).
     *
     *  - Earth Shield (66063, the generic NPC one): a DUMMY aura, 6 charges, proc mask 0x222a8, no
     *    spell_proc_event row. Its proc runs Unit::HandleDummyAuraProc's generic label 66063
     *    (UnitAuraProcHandler.cpp:1180), `triggered_spell_id = 66064; break;`, and the handler's
     *    tail casts 66064 (a HEAL at A=1: the priest heals himself). Two hits, two procs.
     *  - Lightning Shield rank 1 (324): a PROC_TRIGGER_SPELL aura, 3 charges, proc mask 0x222a8,
     *    spell_proc_event cooldown 3 s. Its proc runs Unit::HandleProcTriggerSpellAuraProc's shaman
     *    label 324 (UnitAuraProcHandler.cpp:4286, under the class-mask test 0x400), `trigger_spell_id
     *    = 26364`, and the tail casts 26364 (SCHOOL_DAMAGE at A=6) at the cougar and stores the proc
     *    cooldown under 26364's key (UnitAuraProcHandler.cpp:4648-4651). That key is a wall-clock
     *    cooldown, so before the second hit the scenario clears it by call
     *    (Player::RemoveSpellCooldown(26364, true)) -- the rule: no proc re-attempted while a
     *    cooldown it set is uncleared.
     *
     * Every spell qualifies by the guard: none binds SD3 or a DB script, no case body reached picks
     * at random (the two bodies are one assignment each), no random chain. The auras are put on
     * the priest by a triggered self-cast (a positive spell draws no roll).
     *
     * Pins: each proc's result on the holder -- the charges, the triggered spell's log, the health
     * -- and the proc cooldown's stored key, set and cleared.
     */
    class SpellProcCharges : public SpellScenario
    {
    public:
        SpellProcCharges()
            : SpellScenario("spell-proc-charges", 933,
                            { "template", "dummyProc", "triggerProc", "procCooldown", "noPersistence", "digest" }) {}

        void Prepare() override
        {
            struct St
            {
                ObjectGuid player, cougar;
                std::string templateOk;
                uint32 levelAtSpawn = 0;
                std::set<uint32> achievementsAtSpawn;
                bool dealsDamage = false;
                QuestPlan plan;               // for noPersistence's backstops
                std::string procSelfBefore, procCougar;
                std::string es0, es1, es2, esGone, ls0, ls1, ls2, lsGone;
                std::string keysAfterHit3, keysAfterClear, keysAfterHit4;
                bool ran[9] = { false, false, false, false, false, false, false, false, false };
            };
            const uint32 EARTH_SHIELD = 66063, LIGHTNING_SHIELD = 324, POKE = 40739, LS_DAMAGE = 26364;
            const uint32 cougar = 2961;

            std::vector<SpellPrint> prints = {
                // 66063: 26 fields, fnv 3789ff4f
                { 66063, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 21 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 66064: 26 fields, fnv 93fa5235
                { 66064, { { "eff0.present", 1 }, { "eff0.Effect", 10 }, { "eff0.EffectBasePoints", 9249 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 1501 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 324: 26 fields, fnv 93c3c97a
                { 324, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 13 }, { "eff0.EffectBonusCoefficient", 1049146425 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 26364 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 26364: 26 fields, fnv f29e2e12
                { 26364, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 13 }, { "eff0.EffectBonusCoefficient", 1049146425 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 40739: 26 fields, fnv 3284c579
                { 40739, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
            QuestPlan plan;
            plan.quest = 0;
            plan.classId = CLASS_PRIEST;
            plan.dealsDamage = true;        // 26364, cast by the priest's Lightning Shield, hits the cougar
            plan.takesDamage = true;        // Burning Shadows hits the priest
            plan.heals = true;              // 66064, cast by his Earth Shield, heals him
            std::string templateOk;
            if (!Qualify(prints, { cougar }, plan, templateOk))
            {
                return;
            }

            Player* p = SpawnPlayer(P0.x, P0.y, Ground(P0.x, P0.y, P0.z), 0.0f, CLASS_PRIEST);
            if (!p)
            {
                Verdict(Invalid("spawn failed"));
                return;
            }
            StartTimeSync(p);
            const float cx = P0.x + 10.0f;
            Creature* c = Spawn(cougar, cx, P0.y, Ground(cx, P0.y, P0.z), 3.14159f);
            if (!c)
            {
                Verdict(Invalid("the cougar (2961) did not spawn"));
                return;
            }
            Silence(c);
            Park(c);

            auto st = std::make_shared<St>();
            st->player = p->GetObjectGuid();
            st->cougar = c->GetObjectGuid();
            st->templateOk = templateOk;
            st->levelAtSpawn = p->getLevel();
            st->achievementsAtSpawn = Achievements(p);
            st->dealsDamage = plan.dealsDamage;
            st->plan = plan;

            SpellWatch watch;
            watch.caster = st->cougar;
            m_rec.Begin(Name(), p, watch);

            // one hit: the cougar's Burning Shadows on the priest, in window `name`
            auto hit = [this, st](char const* name, uint32 index)
            {
                Player* p = sPlayerRegistry.Find(st->player);
                Creature* c = Get(st->cougar);
                if (!p || !c) { return; }
                m_rec.Open(name);
                c->CastSpell(p, POKE, false);
                m_rec.Note("the cougar casts Burning Shadows at the priest");
                m_rec.Open(std::string(name) + "+tick");
                st->ran[index] = true;
            };

            At(300, [this, st]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                Creature* c = Get(st->cougar);
                if (!p || !c) { return; }
                m_rec.Open("earth-shield");
                st->procSelfBefore = ProcHolders(p);
                st->procCougar = ProcHolders(c);
                m_rec.Note("holders that can proc before: self [" + st->procSelfBefore + "], caster [" + st->procCougar + "]");
                p->CastSpell(p, EARTH_SHIELD, true);
                st->es0 = HolderOf(p, EARTH_SHIELD);
                m_rec.Note("Earth Shield on the priest: " + st->es0);
                m_rec.Open("earth-shield+tick");
                st->ran[0] = true;
            });
            At(600, [hit]() { hit("hit1", 1); });
            At(900, [st, hit]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                if (p) { st->es1 = HolderOf(p, EARTH_SHIELD); }
                hit("hit2", 2);
            });
            At(1200, [this, st]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                if (!p) { return; }
                st->es2 = HolderOf(p, EARTH_SHIELD);
                m_rec.Open("earth-shield-removed");
                m_rec.Note("Earth Shield on the priest: " + st->es2);
                p->RemoveAurasDueToSpell(EARTH_SHIELD);
                st->esGone = HolderOf(p, EARTH_SHIELD);
                m_rec.Open("earth-shield-removed+tick");
                st->ran[3] = true;
            });
            At(1500, [this, st]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                if (!p) { return; }
                m_rec.Open("lightning-shield");
                p->CastSpell(p, LIGHTNING_SHIELD, true);
                st->ls0 = HolderOf(p, LIGHTNING_SHIELD);
                m_rec.Note("Lightning Shield on the priest: " + st->ls0);
                m_rec.Open("lightning-shield+tick");
                st->ran[4] = true;
            });
            At(1800, [hit]() { hit("hit3", 5); });
            At(2100, [this, st]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                if (!p) { return; }
                st->ls1 = HolderOf(p, LIGHTNING_SHIELD);
                st->keysAfterHit3 = CooldownKeys(p);
                m_rec.Open("proc-cooldown-cleared");
                m_rec.Note("Lightning Shield: " + st->ls1 + "; stored cooldown keys [" + st->keysAfterHit3 + "]");
                p->RemoveSpellCooldown(LS_DAMAGE, true);
                st->keysAfterClear = CooldownKeys(p);
                m_rec.Note("after RemoveSpellCooldown(26364, true): keys [" + st->keysAfterClear + "]");
                m_rec.Open("proc-cooldown-cleared+tick");
                st->ran[6] = true;
            });
            At(2400, [hit]() { hit("hit4", 7); });
            At(2700, [this, st]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                if (!p) { return; }
                st->ls2 = HolderOf(p, LIGHTNING_SHIELD);
                st->keysAfterHit4 = CooldownKeys(p);
                m_rec.Open("lightning-shield-removed");
                m_rec.Note("Lightning Shield: " + st->ls2 + "; stored cooldown keys [" + st->keysAfterHit4 + "]");
                p->RemoveAurasDueToSpell(LIGHTNING_SHIELD);
                st->lsGone = HolderOf(p, LIGHTNING_SHIELD);
                m_rec.Open("lightning-shield-removed+tick");
                st->ran[8] = true;
            });

            At(3000, [this, st]()
            {
                m_rec.End();
                Player* p = sPlayerRegistry.Find(st->player);
                if (!p)
                {
                    Verdict(Invalid("the harness player was gone at the verdict"));
                    return;
                }
                bool allRan = true;
                for (int i = 0; i < 9; ++i)
                {
                    allRan = allRan && (i == 2 || st->ran[i]);
                }
                if (!allRan || !st->procSelfBefore.empty() || !st->procCougar.empty())
                {
                    const std::string why = !allRan ? "INVALID(a step did not run)"
                                                    : "INVALID(holders that could proc before the shields: self [" + st->procSelfBefore + "], caster [" + st->procCougar + "])";
                    Verdict(Compose({ st->templateOk, why, why, why, why, DigestValue("the first shield") }));
                    return;
                }
                const std::vector<std::string> heal1 = Records("hit1", SMSG_SPELLHEALLOG);
                const std::vector<std::string> heal2 = Records("hit2", SMSG_SPELLHEALLOG);
                std::string dummy = "(Earth Shield " + st->es0 + " -> " + st->es1 + " -> " + st->es2 + ", removed: " + st->esGone + "; heals " + Joined(heal1) +
                                    " then " + Joined(heal2) + ")";
                const bool dummyOk = st->es0 != "none" && heal1.size() == 1 && heal2.size() == 1 && st->esGone == "none";
                dummy = (dummyOk ? "OK" : "BUG") + dummy;
                const std::vector<std::string> dmg3 = Records("hit3", SMSG_SPELLNONMELEEDAMAGELOG);
                const std::vector<std::string> dmg4 = Records("hit4", SMSG_SPELLNONMELEEDAMAGELOG);
                std::string trigger = "(Lightning Shield " + st->ls0 + " -> " + st->ls1 + " -> " + st->ls2 + ", removed: " + st->lsGone + "; damage logs " +
                                      Joined(dmg3) + " then " + Joined(dmg4) + ")";
                const bool triggerOk = st->ls0 != "none" && st->lsGone == "none";
                trigger = (triggerOk ? "OK" : "BUG") + trigger;
                const std::vector<std::string> clears = Records("proc-cooldown-cleared", SMSG_CLEAR_COOLDOWNS);
                std::string cooldown = "(stored keys after the first Lightning Shield proc [" + st->keysAfterHit3 + "]; CLEAR_COOLDOWNS " + Joined(clears) +
                                       "; after the call [" + st->keysAfterClear + "]; after the second proc [" + st->keysAfterHit4 + "])";
                const bool cooldownOk = st->keysAfterHit3.find("26364") != std::string::npos && st->keysAfterClear.find("26364") == std::string::npos &&
                                        clears.size() == 1;
                cooldown = (cooldownOk ? "OK" : "BUG") + cooldown;
                Verdict(Compose({ st->templateOk, dummy, trigger, cooldown,
                                  NoPersistence(p, st->achievementsAtSpawn, st->levelAtSpawn, false, st->dealsDamage, &st->plan), DigestValue("the first shield") }));
            });
        }
    };

    /**
     * S934 `spell-cooldowns-set-and-cleared`: a human priest at his created level sets every kind
     * of player cooldown the core stores, and the scenario clears each by call before anything is
     * cast again -- cooldowns end on time(NULL), which the stepped world does not move, so no
     * scenario may wait one out or re-attempt a cast inside one.
     *
     *  - a CATEGORY cooldown: Every Man for Himself (59752, the human racial he knows at creation;
     *    category 1182, category recovery 120 s, no recovery) through the CAST_SPELL handler's calls:
     *    Spell::SendSpellCooldown -> SpellCooldownMgr::AddSpellAndCategoryCooldowns stores the
     *    spell's own key and one for every other spell of the category;
     *  - a SPELL cooldown that starts when an aura ends: Inner Focus (89485, recovery 45 s,
     *    SPELL_ATTR_DISABLED_WHILE_ACTIVE), learned in a setup window and cast through the
     *    handler; its aura's apply stores the key with the infinity mark (SpellAuras.cpp:4239-4246)
     *    and its removal by call sends SMSG_COOLDOWN_EVENT and stores the real end
     *    (SpellAuras.cpp:4538-4545, SpellCooldownMgr.cpp:154-164);
     *  - a SCHOOL LOCKOUT: while he casts Smite (585) at the silenced cougar, the cougar casts
     *    Interrupt (32747, INTERRUPT_CAST at A=6, no SpellCategories row: no hit roll), whose
     *    effect (Spell::EffectInterruptCast) calls Player::ProhibitSpellSchool: SMSG_SPELL_COOLDOWN
     *    with every known holy spell and a stored key for each (Player.cpp:4626-4653).
     *
     * Then the clears, each by call: Player::RemoveSpellCategoryCooldown(1182, true) (one
     * SMSG_CLEAR_COOLDOWNS per key of the category), Player::RemoveSpellCooldown(89485, true) (one),
     * Player::RemoveAllSpellCooldown (one packet with every key left). Only then is Every Man for
     * Himself cast again, and the category's keys come back.
     *
     * Pins: the three cooldown packets, byte for byte through their decoders, and the stored keys
     * of GetSpellCooldownMap() after every window (never HasSpellCooldown).
     */
    class SpellCooldownsSetAndCleared : public SpellScenario
    {
    public:
        SpellCooldownsSetAndCleared()
            : SpellScenario("spell-cooldowns-set-and-cleared", 934,
                            { "template", "categoryCooldown", "cooldownEvent", "schoolLockout", "clearedByCall", "recastAfterClear", "noPersistence", "digest" }) {}

        void Prepare() override
        {
            struct St
            {
                ObjectGuid player, cougar;
                std::string templateOk;
                uint32 levelAtSpawn = 0;
                std::set<uint32> achievementsAtSpawn;
                bool dealsDamage = false;
                QuestPlan plan;               // for noPersistence's backstops
                std::string procSelf, procCougar;
                uint32 cast1 = 0, castFocus = 0, castSmite = 0, cast2 = 0;
                std::string keysAfterCategory, keysAfterFocus, focusHolder, keysAfterFocusRemoved, keysAfterLockout, smiteSlot;
                std::string keysAfterClearCategory, keysAfterClearSpell, keysAfterClearAll, keysAfterRecast;
                bool ran[8] = { false, false, false, false, false, false, false, false };
            };
            const uint32 EVERY_MAN = 59752, INNER_FOCUS = 89485, SMITE = 585, INTERRUPT = 32747;
            const uint32 CATEGORY = 1182;
            const uint32 cougar = 2961;

            std::vector<SpellPrint> prints = {
                // 59752: 26 fields, fnv 1ebd40a2
                { 59752, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 77 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 1 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 89485: 49 fields, fnv f5058701
                { 89485, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 108 }, { "eff0.EffectBasePoints", -100 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 14 }, { "eff0.EffectSpellClassMask.Flags", 17179875840 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 107 }, { "eff1.EffectBasePoints", 25 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 7 }, { "eff1.EffectSpellClassMask.Flags", 17179875840 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 585: 26 fields, fnv 47a76d3f
                { 585, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 12 }, { "eff0.EffectBonusCoefficient", 1062937297 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 5 }, { "eff0.EffectRealPointsPerLevel", 1056964608 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 32747: 26 fields, fnv ea673d74
                { 32747, { { "eff0.present", 1 }, { "eff0.Effect", 68 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
            QuestPlan plan;
            plan.quest = 0;
            plan.classId = CLASS_PRIEST;
            plan.spells.insert(INNER_FOCUS);
            plan.casts.insert(EVERY_MAN);
            plan.casts.insert(INNER_FOCUS);
            plan.casts.insert(SMITE);
            std::string templateOk;
            if (!Qualify(prints, { cougar }, plan, templateOk))
            {
                return;
            }

            Player* p = SpawnPlayer(P0.x, P0.y, Ground(P0.x, P0.y, P0.z), 0.0f, CLASS_PRIEST);
            if (!p)
            {
                Verdict(Invalid("spawn failed"));
                return;
            }
            StartTimeSync(p);
            const float cx = P0.x + 10.0f;
            Creature* c = Spawn(cougar, cx, P0.y, Ground(cx, P0.y, P0.z), 3.14159f);
            if (!c)
            {
                Verdict(Invalid("the cougar (2961) did not spawn"));
                return;
            }
            Silence(c);
            Park(c);

            auto st = std::make_shared<St>();
            st->player = p->GetObjectGuid();
            st->cougar = c->GetObjectGuid();
            st->templateOk = templateOk;
            st->levelAtSpawn = p->getLevel();
            st->achievementsAtSpawn = Achievements(p);
            st->dealsDamage = plan.dealsDamage;
            st->plan = plan;

            SpellWatch watch;
            watch.caster = st->cougar;
            m_rec.Begin(Name(), p, watch);
            m_rec.Open("learn", false);          // a setup window: logged, never digested
            p->learnSpell(INNER_FOCUS, false);

            At(300, [this, st]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                Creature* c = Get(st->cougar);
                if (!p || !c) { return; }
                m_rec.Open("category");
                st->procSelf = ProcHolders(p);
                st->procCougar = ProcHolders(c);
                m_rec.Note("holders that can proc: self [" + st->procSelf + "], caster [" + st->procCougar + "]; stored keys [" + CooldownKeys(p) + "]");
                st->cast1 = CastAsHandler(p, EVERY_MAN, p, 1);
                st->keysAfterCategory = CooldownKeys(p);
                m_rec.Note("stored keys [" + st->keysAfterCategory + "]");
                m_rec.Open("category+tick");
                st->ran[0] = true;
            });
            At(600, [this, st]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                if (!p) { return; }
                m_rec.Open("focus");
                st->castFocus = CastAsHandler(p, INNER_FOCUS, p, 2);
                st->focusHolder = HolderOf(p, INNER_FOCUS);
                st->keysAfterFocus = CooldownKeys(p);
                m_rec.Note("Inner Focus: " + st->focusHolder + "; stored keys [" + st->keysAfterFocus + "]");
                m_rec.Open("focus+tick");
                st->ran[1] = true;
            });
            At(900, [this, st]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                if (!p) { return; }
                m_rec.Open("focus-removed");
                p->RemoveAurasDueToSpell(INNER_FOCUS);
                st->keysAfterFocusRemoved = CooldownKeys(p);
                m_rec.Note("after RemoveAurasDueToSpell(89485): Inner Focus " + HolderOf(p, INNER_FOCUS) + "; stored keys [" + st->keysAfterFocusRemoved + "]");
                m_rec.Open("focus-removed+tick");
                st->ran[2] = true;
            });
            At(1200, [this, st]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                Creature* c = Get(st->cougar);
                if (!p || !c) { return; }
                m_rec.Open("smite");
                st->castSmite = CastAsHandler(p, SMITE, c, 3);
                Spell* current = p->GetCurrentSpell(CURRENT_GENERIC_SPELL);
                st->smiteSlot = current ? U(current->m_spellInfo->ID) + " state " + U(current->getState()) : std::string("none");
                m_rec.Note("generic slot " + st->smiteSlot);
                m_rec.Open("interrupt");
                c->CastSpell(p, INTERRUPT, false);
                st->keysAfterLockout = CooldownKeys(p);
                m_rec.Note("after the cougar's Interrupt: generic slot " +
                           (p->GetCurrentSpell(CURRENT_GENERIC_SPELL) ? U(p->GetCurrentSpell(CURRENT_GENERIC_SPELL)->m_spellInfo->ID) : std::string("none")) +
                           "; stored keys [" + st->keysAfterLockout + "]");
                m_rec.Open("interrupt+tick");
                st->ran[3] = true;
            });
            At(1500, [this, st]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                if (!p) { return; }
                m_rec.Open("clear-category");
                p->RemoveSpellCategoryCooldown(CATEGORY, true);
                st->keysAfterClearCategory = CooldownKeys(p);
                m_rec.Note("after RemoveSpellCategoryCooldown(1182, true): stored keys [" + st->keysAfterClearCategory + "]");
                st->ran[4] = true;
            });
            At(1800, [this, st]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                if (!p) { return; }
                m_rec.Open("clear-spell");
                p->RemoveSpellCooldown(INNER_FOCUS, true);
                st->keysAfterClearSpell = CooldownKeys(p);
                m_rec.Note("after RemoveSpellCooldown(89485, true): stored keys [" + st->keysAfterClearSpell + "]");
                st->ran[5] = true;
            });
            At(2100, [this, st]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                if (!p) { return; }
                m_rec.Open("clear-all");
                p->RemoveAllSpellCooldown();
                st->keysAfterClearAll = CooldownKeys(p);
                m_rec.Note("after RemoveAllSpellCooldown(): stored keys [" + st->keysAfterClearAll + "]");
                st->ran[6] = true;
            });
            At(2400, [this, st]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                if (!p) { return; }
                m_rec.Open("recast");
                st->cast2 = CastAsHandler(p, EVERY_MAN, p, 4);
                st->keysAfterRecast = CooldownKeys(p);
                m_rec.Note("stored keys [" + st->keysAfterRecast + "]");
                m_rec.Open("recast+tick");
                st->ran[7] = true;
            });

            At(2700, [this, st]()
            {
                m_rec.End();
                Player* p = sPlayerRegistry.Find(st->player);
                if (!p)
                {
                    Verdict(Invalid("the harness player was gone at the verdict"));
                    return;
                }
                bool allRan = true;
                for (int i = 0; i < 8; ++i)
                {
                    allRan = allRan && st->ran[i];
                }
                if (!allRan || !st->procSelf.empty() || !st->procCougar.empty())
                {
                    const std::string why = !allRan ? "INVALID(a step did not run)"
                                                    : "INVALID(holders that can proc: self [" + st->procSelf + "], caster [" + st->procCougar + "])";
                    Verdict(Compose({ st->templateOk, why, why, why, why, why, why, DigestValue("the first cast") }));
                    return;
                }
                auto has = [](std::string const& keys, uint32 id) { return ("," + keys + ",").find("," + U(id) + ",") != std::string::npos; };
                std::string category = "(stored keys after the cast [" + st->keysAfterCategory + "]; SPELL_GO " + Joined(Records("category", SMSG_SPELL_GO)) + ")";
                category = ((st->cast1 && has(st->keysAfterCategory, EVERY_MAN)) ? "OK" : "BUG") + category;
                const std::vector<std::string> events = Records("focus-removed", SMSG_COOLDOWN_EVENT);
                std::string event = "(Inner Focus " + st->focusHolder + ", stored keys [" + st->keysAfterFocus + "]; at the removal COOLDOWN_EVENT " + Joined(events) +
                                    ", keys [" + st->keysAfterFocusRemoved + "])";
                event = ((st->castFocus && events.size() == 1 && has(st->keysAfterFocusRemoved, INNER_FOCUS)) ? "OK" : "BUG") + event;
                const std::vector<std::string> lockouts = Records("interrupt", SMSG_SPELL_COOLDOWN);
                std::string lockout = "(Smite in the generic slot: " + st->smiteSlot + "; SPELL_COOLDOWN " + Joined(lockouts) + "; SPELL_FAILURE " +
                                      Joined(Records("interrupt", SMSG_SPELL_FAILURE)) + "; stored keys [" + st->keysAfterLockout + "])";
                lockout = ((st->castSmite && lockouts.size() == 1) ? "OK" : "BUG") + lockout;
                std::string cleared = "(category 1182: CLEAR_COOLDOWNS " + Joined(Records("clear-category", SMSG_CLEAR_COOLDOWNS)) + ", keys [" +
                                      st->keysAfterClearCategory + "]; spell 89485: " + Joined(Records("clear-spell", SMSG_CLEAR_COOLDOWNS)) + ", keys [" +
                                      st->keysAfterClearSpell + "]; all: " + Joined(Records("clear-all", SMSG_CLEAR_COOLDOWNS)) + ", keys [" + st->keysAfterClearAll + "])";
                cleared = ((!has(st->keysAfterClearCategory, EVERY_MAN) && !has(st->keysAfterClearSpell, INNER_FOCUS) && st->keysAfterClearAll.empty()) ? "OK" : "BUG") + cleared;
                std::string recast = "(the second cast after the clears: SPELL_GO " + Joined(Records("recast", SMSG_SPELL_GO)) + "; CAST_FAILED " +
                                     Joined(Records("recast", SMSG_CAST_FAILED)) + "; stored keys [" + st->keysAfterRecast + "])";
                recast = ((st->cast2 && has(st->keysAfterRecast, EVERY_MAN)) ? "OK" : "BUG") + recast;
                Verdict(Compose({ st->templateOk, category, event, lockout, cleared, recast,
                                  NoPersistence(p, st->achievementsAtSpawn, st->levelAtSpawn, false, st->dealsDamage, &st->plan), DigestValue("the first cast") }));
            });
        }
    };

    /**
     * S935 `spell-cast-interrupted`: a human priest at his created level begins Smite (585, 1.5 s
     * cast) at a spawned, silenced Mountain Cougar (2961) ten yards in front of him, twice, and each
     * time something ends it before it lands:
     *
     *  - Bash (5211): a second silenced cougar three yards behind him casts it at him, triggered as
     *    the harness's creatures cast it (Scenario::SelfCast's convention). Its SpellCategories row
     *    has DefenseType 2 (MELEE), so the stun draws the seeded melee roll against the priest
     *    (UnitCombat.cpp:640-700) -- recorded as it falls. The stun (MOD_STUN) is taken off by call
     *    before the second cast, so nothing waits out its four seconds.
     *  - the player mover: one movement word down the handler the client's own packets go down
     *    (MovementHandlers::HandleMoverRelocation) with MOVEFLAG_FORWARD and a place one yard west; the
     *    cast's own update sees the caster off the cast position (SpellCast.cpp:935-951) and, as
     *    Smite's SpellInterrupts row carries SPELL_INTERRUPT_FLAG_MOVEMENT, cancels it. A second
     *    word, no flags, stops him.
     *
     * On retail both interrupt (reference section 2 item 3: "A cast in progress is interrupted"
     * by a stun; section 1 item 9: any translation interrupts a cast-time spell). The core's
     * behaviour is recorded, not judged.
     *
     * Pins: the failure packets of each interrupt (SMSG_SPELL_FAILURE and SMSG_SPELL_FAILED_OTHER,
     * SMSG_CAST_FAILED if any), the generic slot before and after, the mana (the cost is taken at
     * the cast, not at the start), the stun's holder.
     */
    class SpellCastInterrupted : public SpellScenario
    {
    public:
        SpellCastInterrupted()
            : SpellScenario("spell-cast-interrupted", 935,
                            { "template", "interruptedByStun", "interruptedByMove", "noPersistence", "digest" }) {}

        void Prepare() override
        {
            struct St
            {
                ObjectGuid player, target, basher;
                std::string templateOk;
                uint32 levelAtSpawn = 0;
                std::set<uint32> achievementsAtSpawn;
                bool dealsDamage = false;
                QuestPlan plan;               // for noPersistence's backstops
                std::string procSelf, procTarget, procBasher;
                uint32 cast1 = 0, cast2 = 0;
                std::string slot1Before, slot1After, stun, slot2Before, slot2After;
                uint32 mana1Before = 0, mana1After = 0, mana2Before = 0, mana2After = 0;
                bool ran[5] = { false, false, false, false, false };
            };
            const uint32 SMITE = 585, BASH = 5211;
            const uint32 cougar = 2961;

            std::vector<SpellPrint> prints = {
                // 585: 26 fields, fnv 47a76d3f
                { 585, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 12 }, { "eff0.EffectBonusCoefficient", 1062937297 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 5 }, { "eff0.EffectRealPointsPerLevel", 1056964608 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 5211: 26 fields, fnv 67da24d9
                { 5211, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 12 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
            QuestPlan plan;
            plan.quest = 0;
            plan.classId = CLASS_PRIEST;
            plan.casts.insert(SMITE);
            // the mover's word explores the area of the place it moves him to
            // (Player::CheckAreaExploreAndOutdoor): read off the map's terrain before any server call
            Load(P0.x - 1.0f, P0.y);
            Map* map = GetMap();
            const uint32 area = map ? map->GetTerrain()->GetAreaId(P0.x - 1.0f, P0.y, Ground(P0.x - 1.0f, P0.y, P0.z)) : 0;
            if (!area)
            {
                Verdict(Invalid("no area under the place the movement word goes to"));
                return;
            }
            plan.exploredAreas.insert(area);
            std::string templateOk;
            if (!Qualify(prints, { cougar }, plan, templateOk))
            {
                return;
            }

            Player* p = SpawnPlayer(P0.x, P0.y, Ground(P0.x, P0.y, P0.z), 0.0f, CLASS_PRIEST);
            if (!p)
            {
                Verdict(Invalid("spawn failed"));
                return;
            }
            StartTimeSync(p);
            const float tx = P0.x + 10.0f;
            Creature* target = Spawn(cougar, tx, P0.y, Ground(tx, P0.y, P0.z), 3.14159f);
            const float bx = P0.x - 3.0f;
            Creature* basher = Spawn(cougar, bx, P0.y, Ground(bx, P0.y, P0.z), 0.0f);
            if (!target || !basher)
            {
                Verdict(Invalid("a cougar (2961) did not spawn"));
                return;
            }
            Silence(target);
            Park(target);
            Silence(basher);
            Park(basher);

            auto st = std::make_shared<St>();
            st->player = p->GetObjectGuid();
            st->target = target->GetObjectGuid();
            st->basher = basher->GetObjectGuid();
            st->templateOk = templateOk;
            st->levelAtSpawn = p->getLevel();
            st->achievementsAtSpawn = Achievements(p);
            st->dealsDamage = plan.dealsDamage;
            st->plan = plan;

            SpellWatch watch;
            watch.target = st->target;
            watch.caster = st->basher;
            m_rec.Begin(Name(), p, watch);

            auto slot = [](Player* p)
            {
                Spell* s = p->GetCurrentSpell(CURRENT_GENERIC_SPELL);
                return s ? U(s->m_spellInfo->ID) + " state " + U(s->getState()) : std::string("none");
            };

            At(300, [this, st, slot]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                Creature* target = Get(st->target);
                Creature* basher = Get(st->basher);
                if (!p || !target || !basher) { return; }
                m_rec.Open("cast1");
                st->procSelf = ProcHolders(p);
                st->procTarget = ProcHolders(target);
                st->procBasher = ProcHolders(basher);
                m_rec.Note("holders that can proc: self [" + st->procSelf + "], target [" + st->procTarget + "], caster [" + st->procBasher + "]");
                st->mana1Before = p->GetPower(POWER_MANA);
                st->cast1 = CastAsHandler(p, SMITE, target, 1);
                st->slot1Before = slot(p);
                m_rec.Note("generic slot " + st->slot1Before);
                m_rec.Open("cast1+tick");
                st->ran[0] = true;
            });
            At(800, [this, st, slot]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                Creature* basher = Get(st->basher);
                if (!p || !basher) { return; }
                m_rec.Open("bash");
                basher->CastSpell(p, BASH, true);
                st->stun = HolderOf(p, BASH);
                st->slot1After = slot(p);
                st->mana1After = p->GetPower(POWER_MANA);
                m_rec.Note("after the Bash: stun " + st->stun + "; generic slot " + st->slot1After);
                m_rec.Open("bash+tick");
                st->ran[1] = true;
            });
            At(1100, [this, st]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                if (!p) { return; }
                m_rec.Open("stun-removed");
                p->RemoveAurasDueToSpell(BASH);
                m_rec.Note("after RemoveAurasDueToSpell(5211): stun " + HolderOf(p, BASH));
                m_rec.Open("stun-removed+tick");
                st->ran[2] = true;
            });
            At(2200, [this, st, slot]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                Creature* target = Get(st->target);
                if (!p || !target) { return; }
                m_rec.Open("cast2");
                st->mana2Before = p->GetPower(POWER_MANA);
                st->cast2 = CastAsHandler(p, SMITE, target, 2);
                st->slot2Before = slot(p);
                m_rec.Note("generic slot " + st->slot2Before);
                m_rec.Open("cast2+tick");
                st->ran[3] = true;
            });
            At(2500, [this, st]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                if (!p) { return; }
                m_rec.Open("move");
                MovementInfo word = p->ClientStatus();
                word.SetMovementFlags(MOVEFLAG_FORWARD);
                word.ChangePosition(p->Where().X() - 1.0f, p->Where().Y(), p->Where().Z(), p->Where().Facing());
                MovementHandlers::HandleMoverRelocation(*SessionOf(*p), p, word);
                m_rec.Note("one movement word: MOVEFLAG_FORWARD, one yard west");
                m_rec.Open("move+tick");
                st->ran[4] = true;
            });
            At(2800, [this, st, slot]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                if (!p) { return; }
                st->slot2After = slot(p);
                st->mana2After = p->GetPower(POWER_MANA);
                m_rec.Open("stop");
                MovementInfo word = p->ClientStatus();
                word.SetMovementFlags(MOVEFLAG_NONE);
                word.ChangePosition(p->Where().X(), p->Where().Y(), p->Where().Z(), p->Where().Facing());
                MovementHandlers::HandleMoverRelocation(*SessionOf(*p), p, word);
                m_rec.Note("generic slot " + st->slot2After + "; one movement word: no flags, where he stands");
                m_rec.Open("stop+tick");
            });

            At(3100, [this, st]()
            {
                m_rec.End();
                Player* p = sPlayerRegistry.Find(st->player);
                if (!p)
                {
                    Verdict(Invalid("the harness player was gone at the verdict"));
                    return;
                }
                bool allRan = true;
                for (int i = 0; i < 5; ++i)
                {
                    allRan = allRan && st->ran[i];
                }
                if (!allRan || !st->procSelf.empty() || !st->procTarget.empty() || !st->procBasher.empty())
                {
                    const std::string why = !allRan ? "INVALID(a step did not run)"
                                                    : "INVALID(holders that can proc: self [" + st->procSelf + "], target [" + st->procTarget + "], caster [" +
                                                          st->procBasher + "])";
                    Verdict(Compose({ st->templateOk, why, why, why, DigestValue("the first cast") }));
                    return;
                }
                std::string stun = "(Smite " + st->slot1Before + " -> after the Bash " + st->slot1After + "; stun " + st->stun + "; SPELL_GO " +
                                   Joined(Records("bash", SMSG_SPELL_GO)) + "; SPELL_FAILURE " + Joined(Records("bash", SMSG_SPELL_FAILURE)) +
                                   "; SPELL_FAILED_OTHER " + Joined(Records("bash", SMSG_SPELL_FAILED_OTHER)) + "; mana " + U(st->mana1Before) + " -> " +
                                   U(st->mana1After) + ")";
                stun = ((st->cast1 && st->slot1Before.find("585 ") == 0) ? "OK" : "BUG") + stun;
                std::string move = "(Smite " + st->slot2Before + " -> after the move " + st->slot2After + "; SPELL_FAILURE " +
                                   Joined(Records("move+tick", SMSG_SPELL_FAILURE)) + "; SPELL_FAILED_OTHER " + Joined(Records("move+tick", SMSG_SPELL_FAILED_OTHER)) +
                                   "; CAST_FAILED " + Joined(Records("move+tick", SMSG_CAST_FAILED)) + "; mana " + U(st->mana2Before) + " -> " + U(st->mana2After) + ")";
                move = ((st->cast2 && st->slot2Before.find("585 ") == 0) ? "OK" : "BUG") + move;
                Verdict(Compose({ st->templateOk, stun, move,
                                  NoPersistence(p, st->achievementsAtSpawn, st->levelAtSpawn, false, st->dealsDamage, &st->plan), DigestValue("the first cast") }));
            });
        }
    };

    /**
     * S936 `spell-self-roll-hellfire` -- a spell cast on its own caster takes no hit roll against
     * him: a human warlock at level 30 (the `.reset level` sequence) who has learned
     * Hellfire (1949, Player::learnSpell, both in a setup window) casts it kHellfireCasts times
     * through the CAST_SPELL handler's calls, one cast every 1.6 s (past its 1.5 s global cooldown),
     * and the channel is ended by call (Unit::InterruptSpell(CURRENT_CHANNELED_SPELL)) 200 ms after
     * the cast, before the first 1 s tick, so no tick of its self damage or of its area trigger ever
     * runs. His mana is set to its maximum by call before each cast (Hellfire costs 64% of base
     * mana).
     *
     * Hellfire's two effects are both APPLY_AURA at A=1 (TARGET_SELF): PERIODIC_TRIGGER_SPELL of
     * 5857 (the area damage) and PERIODIC_DAMAGE on the caster. They collapse into one target
     * entry, the caster, whose hit result Unit::SpellHitResult answers SPELL_MISS_NONE for without
     * the magic roll (MagicSpellHitResult: 96% at level difference 0), so every cast holds both
     * auras. Pins: `selfMissCount` is OK only when none of the N casts missed its caster (a single
     * self-miss reads INVALID), and `oneRollBothEffects` that no cast ever holds one of the two
     * auras without the other.
     *
     * THE SEED AND N. A channel with no cast time is cast in the map update after the handler's
     * SpellStart (Spell::update), under the map hook's RNG::Seed(TickSeed(base, 936, elapsed))
     * (Harness.cpp:519, SeedMapUpdate); the harness's seed base is fixed (0x4D56) and the update
     * runs the same objects in the same order every run, so a roll there would be a function of
     * the cast's elapsed time alone. N = kHellfireCasts = 50: under this seed a magic roll against
     * the caster would miss casts 42 and 45, so the 50 casts would read a self-miss if the roll
     * came back. The record states N, the seed base and the pattern.
     */
    class SpellSelfRollHellfire : public SpellScenario
    {
    public:
        SpellSelfRollHellfire()
            : SpellScenario("spell-self-roll-hellfire", 936,
                            { "template", "oneRollBothEffects", "selfMissCount", "noPersistence", "digest" }) {}

        void Prepare() override
        {
            struct St
            {
                ObjectGuid player;
                std::string templateOk;
                uint32 levelAtSpawn = 0, levelSet = 0;
                bool noReachLevel = false;
                std::set<uint32> achievementsAtSpawn;
                bool dealsDamage = false;
                QuestPlan plan;               // for noPersistence's backstops
                std::string procSelf;
                std::vector<uint32> castIds;
                std::vector<std::string> holders, holdersAfterStop;
                uint32 ran = 0;
            };
            const uint32 HELLFIRE = 1949;
            const uint32 level = 30;

            std::vector<SpellPrint> prints = {
                // 1949: 49 fields, fnv 4cf4460f
                { 1949, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 23 }, { "eff0.EffectAuraPeriod", 1000 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 5857 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 3 }, { "eff1.EffectAuraPeriod", 1000 }, { "eff1.EffectBasePoints", 1 }, { "eff1.EffectBonusCoefficient", 1036160860 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 5857: 26 fields, fnv 0bdae182
                { 5857, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectBonusCoefficient", 1036160860 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectRadiusIndex_1", 13 }, { "eff0.ImplicitTarget_0", 18 }, { "eff0.ImplicitTarget_1", 16 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
            QuestPlan plan;
            plan.quest = 0;
            plan.classId = CLASS_WARLOCK;
            plan.level = level;
            plan.spells.insert(HELLFIRE);
            plan.casts.insert(HELLFIRE);
            std::string templateOk;
            QuestPreCheck pre;
            if (!Qualify(prints, {}, plan, templateOk, &pre))
            {
                return;
            }

            Player* p = SpawnPlayer(P0.x, P0.y, Ground(P0.x, P0.y, P0.z), 0.0f, CLASS_WARLOCK);
            if (!p)
            {
                Verdict(Invalid("spawn failed"));
                return;
            }
            StartTimeSync(p);
            auto st = std::make_shared<St>();
            st->player = p->GetObjectGuid();
            st->templateOk = templateOk;
            st->levelAtSpawn = p->getLevel();
            st->noReachLevel = pre.levelBound == pre.startLevel;
            st->achievementsAtSpawn = Achievements(p);
            st->dealsDamage = plan.dealsDamage;
            st->plan = plan;

            SpellWatch watch;
            m_rec.Begin(Name(), p, watch);
            m_rec.Open("level", false);           // a setup window: logged, never digested
            SetLevelAsResetDoes(p, level);
            p->learnSpell(HELLFIRE, false);
            st->levelSet = p->getLevel();
            st->procSelf = ProcHolders(p);

            for (uint32 k = 0; k < kHellfireCasts; ++k)
            {
                At(300 + 1600 * k, [this, st, k]()
                {
                    Player* p = sPlayerRegistry.Find(st->player);
                    if (!p || !st->procSelf.empty()) { return; }
                    char name[32];
                    snprintf(name, sizeof(name), "cast%02u", k + 1);
                    m_rec.Open(name);
                    p->SetPower(POWER_MANA, p->GetMaxPower(POWER_MANA));
                    st->castIds.push_back(CastAsHandler(p, HELLFIRE, p, uint8(k + 1)));
                    m_rec.Open(std::string(name) + "+tick");
                });
                // The channel's instant cast lands in the next map update (Spell::update), where
                // the roll is drawn under the map hook's seed; 200 ms on, before the first 1 s tick
                // of either aura, the holder is read and the channel ended.
                At(300 + 1600 * k + 200, [this, st, k]()
                {
                    Player* p = sPlayerRegistry.Find(st->player);
                    if (!p || !st->procSelf.empty() || st->castIds.size() != k + 1) { return; }
                    char name[32];
                    snprintf(name, sizeof(name), "cast%02u-stop", k + 1);
                    st->holders.push_back(HolderOf(p, HELLFIRE));
                    m_rec.Open(name);
                    m_rec.Note("Hellfire on himself: " + st->holders.back());
                    p->InterruptSpell(CURRENT_CHANNELED_SPELL);
                    st->holdersAfterStop.push_back(HolderOf(p, HELLFIRE));
                    m_rec.Note("after InterruptSpell(CURRENT_CHANNELED_SPELL): " + st->holdersAfterStop.back());
                    m_rec.Open(std::string(name) + "+tick");
                    ++st->ran;
                });
            }

            At(300 + 1600 * kHellfireCasts, [this, st]()
            {
                m_rec.End();
                Player* p = sPlayerRegistry.Find(st->player);
                if (!p)
                {
                    Verdict(Invalid("the harness player was gone at the verdict"));
                    return;
                }
                if (!st->procSelf.empty())
                {
                    Verdict(Invalid("holders that can proc: self [" + st->procSelf + "]"));
                    return;
                }
                if (st->ran != kHellfireCasts)
                {
                    const std::string why = "INVALID(" + U(st->ran) + " of " + U(kHellfireCasts) + " casts ran)";
                    Verdict(Compose({ st->templateOk, why, why, why, DigestValue("the first cast") }));
                    return;
                }
                uint32 held = 0, missed = 0, split = 0, lingering = 0, refused = 0;
                std::string pattern;
                for (uint32 k = 0; k < kHellfireCasts; ++k)
                {
                    if (!st->castIds[k])
                    {
                        ++refused;
                        pattern += "R";
                        continue;
                    }
                    if (st->holders[k] == "none")
                    {
                        ++missed;
                        pattern += "m";
                    }
                    else if (st->holders[k].find("eff 0x3 ") == 0)
                    {
                        ++held;
                        pattern += "H";
                    }
                    else
                    {
                        ++split;
                        pattern += "S";
                    }
                    lingering += st->holdersAfterStop[k] != "none" ? 1 : 0;
                }
                std::string both = "(" + U(held) + " casts held both effects (mask 0x3), " + U(missed) + " held neither, " + U(split) + " held one without the other; " +
                                   U(lingering) + " holders outlived the stop)";
                both = ((split == 0 && refused == 0 && lingering == 0) ? "OK" : "BUG") + both;
                std::string count = "(N=" + U(kHellfireCasts) + ", seed base 0x4D56, order 936: " + U(missed) + " self-misses, the casts in order " + pattern + ")";
                count = ((refused == 0 && missed == 0) ? "OK" : "INVALID") + count;
                Verdict(Compose({ st->templateOk, both, count,
                                  NoPersistence(p, st->achievementsAtSpawn, st->levelSet, st->noReachLevel, st->dealsDamage, &st->plan), DigestValue("the first cast") }));
            });
        }

    private:
        static const uint32 kHellfireCasts = 50;
    };

    /**
     * S937 `spell-mortar-shot-fallthrough` -- a table-coordinate target A=17 with an area B=8 fills
     * around the explicit target only: a human priest at his created level who has been given
     * Mortar Shot (16786)
     * by Player::learnSpell in a setup window -- the harness's own means, as the taxi family gives
     * its flight licence (ScenariosTaxi.cpp:1048) and the quest fixture's closure counts it
     * (QuestPlan::spells) -- casts it through the CAST_SPELL handler's calls at a spawned, silenced
     * Mountain Cougar (2961, `target`) thirty yards in front of him, outside the spell's 8-yard
     * radius (SpellRadius row 14). A second silenced cougar (`caster` role here: the bystander)
     * stands four yards behind him, inside that radius around HIM.
     *
     * Mortar Shot is SCHOOL_DAMAGE with targets A=17 (TARGET_TABLE_X_Y_Z_COORDINATES) and B=8
     * (TARGET_AREAEFFECT_CUSTOM), 15 points, range 100 yd, no SpellCategories row (no hit roll), no
     * spell_target_position, script_binding, db_scripts or spell_script_target row. In
     * Spell::FillTargetMap the A=17 case's inner switch (SpellTargetList.cpp:269-286) takes its
     * `default:` -- SetTargetMap(A), which finds no spell_target_position and logs it, then
     * SetTargetMap(B=8), which fills around the cast's destination: SpellCastTargets::setUnitTarget
     * put that at the explicit target's position without the DEST_LOCATION flag (Spell.cpp:129-142),
     * so the far cougar is in -- and the A=17 case ends there. TARGET_SELF2's B=8 case, which puts a
     * destination at the caster and fills around HIM (the bystander and the priest himself), does
     * not run for this pair. A spell with A=17 and B=0 cannot pin this (A, 18, A: only a duplicate
     * log line).
     *
     * Pins: `goDestinationAndHits` is OK only when SPELL_GO's target mask (read out of its bytes) is
     * TARGET_FLAG_UNIT alone, with no DEST_LOCATION and so no destination, and its hit list is
     * exactly [target], the caster and the bystander absent; BUG otherwise, with a destination's
     * distance to the caster and to the explicit target when one is there. Then the damage logs and
     * the health each unit lost.
     */
    class SpellMortarShotFallthrough : public SpellScenario
    {
    public:
        SpellMortarShotFallthrough()
            : SpellScenario("spell-mortar-shot-fallthrough", 937,
                            { "template", "goDestinationAndHits", "damageAndHealth", "noPersistence", "digest" }) {}

        void Prepare() override
        {
            struct St
            {
                ObjectGuid player, target, bystander;
                std::string templateOk;
                uint32 levelAtSpawn = 0;
                std::set<uint32> achievementsAtSpawn;
                bool dealsDamage = false;
                QuestPlan plan;               // for noPersistence's backstops
                std::string procSelf, procTarget, procBystander;
                uint32 cast = 0;
                uint32 selfBefore = 0, targetBefore = 0, bystanderBefore = 0;
                uint32 selfAfter = 0, targetAfter = 0, bystanderAfter = 0;
                float casterX = 0.0f, casterY = 0.0f, targetX = 0.0f, targetY = 0.0f;
                bool settled = false;
            };
            const uint32 MORTAR_SHOT = 16786;
            const uint32 cougar = 2961;

            std::vector<SpellPrint> prints = {
                // 16786: 26 fields, fnv 949067eb
                { 16786, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 15 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectRadiusIndex_0", 14 }, { "eff0.ImplicitTarget_0", 17 }, { "eff0.ImplicitTarget_1", 8 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
            QuestPlan plan;
            plan.quest = 0;
            plan.classId = CLASS_PRIEST;
            plan.spells.insert(MORTAR_SHOT);
            plan.casts.insert(MORTAR_SHOT);
            plan.dealsDamage = true;
            plan.takesDamage = true;        // the fill around the caster takes the caster in
            std::string templateOk;
            if (!Qualify(prints, { cougar }, plan, templateOk))
            {
                return;
            }
            if (sSpellMgr.GetSpellTargetPosition(MORTAR_SHOT))
            {
                Verdict(Invalid("spell 16786 has a spell_target_position row: its A=17 would take a destination from it"));
                return;
            }

            Player* p = SpawnPlayer(P0.x, P0.y, Ground(P0.x, P0.y, P0.z), 0.0f, CLASS_PRIEST);
            if (!p)
            {
                Verdict(Invalid("spawn failed"));
                return;
            }
            StartTimeSync(p);
            const float tx = P0.x + 30.0f;
            Creature* target = Spawn(cougar, tx, P0.y, Ground(tx, P0.y, P0.z), 3.14159f);
            const float bx = P0.x - 4.0f;
            Creature* bystander = Spawn(cougar, bx, P0.y, Ground(bx, P0.y, P0.z), 0.0f);
            if (!target || !bystander)
            {
                Verdict(Invalid("a cougar (2961) did not spawn"));
                return;
            }
            Silence(target);
            Park(target);
            Silence(bystander);
            Park(bystander);

            auto st = std::make_shared<St>();
            st->player = p->GetObjectGuid();
            st->target = target->GetObjectGuid();
            st->bystander = bystander->GetObjectGuid();
            st->templateOk = templateOk;
            st->levelAtSpawn = p->getLevel();
            st->achievementsAtSpawn = Achievements(p);
            st->dealsDamage = plan.dealsDamage;
            st->plan = plan;

            SpellWatch watch;
            watch.target = st->target;
            watch.caster = st->bystander;
            m_rec.Begin(Name(), p, watch);
            m_rec.Open("learn", false);          // a setup window: logged, never digested
            p->learnSpell(MORTAR_SHOT, false);

            At(300, [this, st]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                Creature* target = Get(st->target);
                Creature* bystander = Get(st->bystander);
                if (!p || !target || !bystander) { return; }
                m_rec.Open("shot");
                st->procSelf = ProcHolders(p);
                st->procTarget = ProcHolders(target);
                st->procBystander = ProcHolders(bystander);
                m_rec.Note("holders that can proc: self [" + st->procSelf + "], target [" + st->procTarget + "], caster [" + st->procBystander + "]");
                if (!st->procSelf.empty() || !st->procTarget.empty() || !st->procBystander.empty())
                {
                    m_rec.End();
                    Verdict(Invalid("holders that can proc: self [" + st->procSelf + "], target [" + st->procTarget + "], caster [" + st->procBystander + "]"));
                    return;
                }
                st->selfBefore = p->GetHealth();
                st->targetBefore = target->GetHealth();
                st->bystanderBefore = bystander->GetHealth();
                st->casterX = p->Where().X();
                st->casterY = p->Where().Y();
                st->targetX = target->Where().X();
                st->targetY = target->Where().Y();
                st->cast = CastAsHandler(p, MORTAR_SHOT, target, 1);
                m_rec.Open("shot+tick");
            });
            At(1000, [this, st]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                Creature* target = Get(st->target);
                Creature* bystander = Get(st->bystander);
                if (!p || !target || !bystander || !st->cast) { return; }
                st->selfAfter = p->GetHealth();
                st->targetAfter = target->GetHealth();
                st->bystanderAfter = bystander->GetHealth();
                st->settled = true;
                m_rec.Open("after");
                m_rec.Note("health: self " + U(st->selfBefore) + " -> " + U(st->selfAfter) + ", target " + U(st->targetBefore) + " -> " + U(st->targetAfter) +
                           ", bystander " + U(st->bystanderBefore) + " -> " + U(st->bystanderAfter));
            });

            At(1300, [this, st]()
            {
                m_rec.End();
                Player* p = sPlayerRegistry.Find(st->player);
                if (!p)
                {
                    Verdict(Invalid("the harness player was gone at the verdict"));
                    return;
                }
                if (!st->cast || !st->settled)
                {
                    const std::string why = "INVALID(the cast or the snapshot step did not run)";
                    Verdict(Compose({ st->templateOk, why, why, why, DigestValue("the cast") }));
                    return;
                }
                const std::vector<std::string> gos = Records("shot", SMSG_SPELL_GO);
                const std::vector<std::string> logs = Records("shot", SMSG_SPELLNONMELEEDAMAGELOG);
                std::vector<Recorder::Seen const*> goSeen = m_rec.SeenIn("shot", SMSG_SPELL_GO);
                uint32 mask = 0;
                const bool readable = goSeen.size() == 1 && !goSeen[0]->payload.empty() &&
                                      GoMask(&goSeen[0]->payload[0], goSeen[0]->payload.size(), mask);
                float dx = 0.0f, dy = 0.0f, dz = 0.0f;
                const bool dest = readable && GoDestination(&goSeen[0]->payload[0], goSeen[0]->payload.size(), dx, dy, dz);
                char where[192];
                if (!readable)
                {
                    snprintf(where, sizeof(where), "the target mask could not be read");
                }
                else if (dest)
                {
                    snprintf(where, sizeof(where), "mask 0x%x, a destination %.1f yd from the caster and %.1f yd from the explicit target", mask,
                             std::sqrt((dx - st->casterX) * (dx - st->casterX) + (dy - st->casterY) * (dy - st->casterY)),
                             std::sqrt((dx - st->targetX) * (dx - st->targetX) + (dy - st->targetY) * (dy - st->targetY)));
                }
                else
                {
                    snprintf(where, sizeof(where), "mask 0x%x, no destination in it", mask);
                }
                // the decoded record names the hit list by role: the explicit target alone
                const bool targetOnly = gos.size() == 1 && gos[0].find(" hit=[target] miss=[] ") != std::string::npos;
                std::string go = "(SPELL_GO " + Joined(gos) + "; " + where + ")";
                go = ((readable && mask == kTargetFlagUnit && !dest && targetOnly) ? "OK" : "BUG") + go;
                // one damage log per unit that lost health, and each lost exactly its log's damage
                std::string health = "(damage logs " + Joined(logs) + "; health: self " + U(st->selfBefore) + " -> " + U(st->selfAfter) + ", target " +
                                     U(st->targetBefore) + " -> " + U(st->targetAfter) + ", bystander " + U(st->bystanderBefore) + " -> " +
                                     U(st->bystanderAfter) + ")";
                uint32 lost = 0, logged = 0;
                lost += st->selfBefore - st->selfAfter;
                lost += st->targetBefore - st->targetAfter;
                lost += st->bystanderBefore - st->bystanderAfter;
                std::vector<Recorder::Seen const*> logSeen = m_rec.SeenIn("shot", SMSG_SPELLNONMELEEDAMAGELOG);
                for (size_t i = 0; i < logSeen.size(); ++i)
                {
                    uint64 t = 0, a = 0;
                    uint32 spell = 0, damage = 0, overkill = 0;
                    if (!logSeen[i]->payload.empty() &&
                        Trace::ReadSpellDamage(&logSeen[i]->payload[0], logSeen[i]->payload.size(), t, a, spell, damage, overkill))
                    {
                        logged += damage;
                    }
                }
                health = (lost == logged ? "OK" : "BUG") + health;
                Verdict(Compose({ st->templateOk, go, health,
                                  NoPersistence(p, st->achievementsAtSpawn, st->levelAtSpawn, false, st->dealsDamage, &st->plan), DigestValue("the cast") }));
            });
        }

    private:
        static const uint32 kTargetFlagUnit = 0x2;             // TARGET_FLAG_UNIT
        static const uint32 kTargetFlagDestLocation = 0x40;    // TARGET_FLAG_DEST_LOCATION

        /// A byte reader over one SPELL_GO payload, as GoMask and GoDestination walk it.
        struct GoReader
        {
            uint8 const* data;
            size_t size;
            size_t pos;

            bool U8(uint8& v) { if (pos + 1 > size) { return false; } v = data[pos++]; return true; }
            bool U32(uint32& v) { if (pos + 4 > size) { return false; } memcpy(&v, data + pos, 4); pos += 4; return true; }
            bool Packed()
            {
                uint8 m = 0;
                if (!U8(m)) { return false; }
                for (int i = 0; i < 8; ++i)
                {
                    uint8 b = 0;
                    if ((m & (1 << i)) && !U8(b)) { return false; }
                }
                return true;
            }

            /// BuildSpellGoPacket's fixed head and its hit and miss lists (as
            /// Trace::DecodeSpellCast reads them), then SpellCastTargets::write's mask.
            bool Mask(uint32& mask)
            {
                uint8 castCount = 0, hits = 0, misses = 0;
                uint32 spell = 0, flags = 0, timer = 0, stamp = 0;
                if (!Packed() || !Packed() || !U8(castCount) || !U32(spell) || !U32(flags) || !U32(timer) || !U32(stamp) || !U8(hits))
                {
                    return false;
                }
                pos += 8 * size_t(hits);
                if (!U8(misses))
                {
                    return false;
                }
                for (uint8 i = 0; i < misses; ++i)
                {
                    uint8 condition = 0, reflect = 0;
                    pos += 8;
                    if (!U8(condition) || (condition == 11 && !U8(reflect)))
                    {
                        return false;
                    }
                }
                return U32(mask);
            }
        };

        /// The target mask SPELL_GO carries, read out of its bytes. False when the payload is too
        /// short to reach it.
        static bool GoMask(uint8 const* data, size_t size, uint32& mask)
        {
            GoReader r = { data, size, 0 };
            return r.Mask(mask);
        }

        /// The destination SPELL_GO carries, read out of its bytes: the mask (GoMask), the unit's
        /// packed guid (TARGET_FLAG_UNIT) and, for TARGET_FLAG_DEST_LOCATION, the transport's
        /// packed guid and the x, y, z floats. False when the mask names no destination, or a
        /// target this reader does not walk.
        static bool GoDestination(uint8 const* data, size_t size, float& x, float& y, float& z)
        {
            GoReader r = { data, size, 0 };
            uint32 mask = 0;
            if (!r.Mask(mask) || (mask & ~(kTargetFlagUnit | kTargetFlagDestLocation)) || !(mask & kTargetFlagDestLocation))
            {
                return false;
            }
            if ((mask & kTargetFlagUnit) && !r.Packed())
            {
                return false;
            }
            uint32 fx = 0, fy = 0, fz = 0;
            if (!r.Packed() || !r.U32(fx) || !r.U32(fy) || !r.U32(fz))
            {
                return false;
            }
            memcpy(&x, &fx, 4);
            memcpy(&y, &fy, 4);
            memcpy(&z, &fz, 4);
            return true;
        }
    };

    /**
     * S938 `spell-creature-cooldowns` -- today's Creature cooldown model, RECORDED AS IT IS: the
     * spell's end is max(recovery, category recovery) and the category is stored as a start time
     * (Creature::AddCreatureSpellCooldown, CreatureSpellCooldown.cpp:65-83; GetSpellRecoveryTime,
     * SpellMgr.h:128-134), written by the pet and charm cast paths and read by Spell::CheckPetCast
     * (SpellChecks.cpp:1986-1989). The fold into the player's manager changes this record on
     * purpose IF the 4.3.4 evidence supports it; the scenario itself takes no position.
     *
     * A human warlock at level 20 (the `.reset level` sequence) is given a Voidwalker (1860) built in
     * memory the way Spell::DoSummonPet builds a fresh minion, minus its database save (the
     * recipe of ScenariosControl.cpp's BuildPet, with the save's owner gate closed at the end the
     * same way), and the pet learns Torment (3716: category 36, category recovery 5 s, no recovery,
     * a 1.5 s global cooldown) in a setup window. Then, through the CMSG_PET_CAST_SPELL handler's
     * calls (PetCastAsHandler):
     *  - the pet casts Torment at a silenced Mountain Cougar (2961) within its five yards:
     *    CheckPetCast answers OK, AddCreatureSpellCooldown stores the spell key and the category key;
     *  - 1.7 s later (past the pet's global cooldown, which runs on the stepped clock) the pet
     *    casts it again: CheckPetCast answers from the stored key, before anything else is started.
     *    The answer reads time(NULL) against the stored end, so the scenario first reads the wall
     *    clock once and requires, from the stored values and never printing them, the end to lie
     *    more than a second ahead of it -- the answer is then fixed. Otherwise (a stalled run) the
     *    second cast is NOT made and every category reads INVALID: a real second cast must never
     *    reach the digest. This is the one re-attempt inside an uncleared cooldown the family
     *    makes (note 3(a)), by the brief's design, and this guard is what makes it one;
     *  - the pet is taken back (a setup window, EndPet's order: the owner guid first), since a
     *    player holding a pet cannot charm (Spell::CheckCast: SPELL_FAILED_ALREADY_HAVE_CHARM);
     *  - the warlock charms a second, spawned Voidwalker (21835, Gizlock's Dummy Charm Effect, no
     *    SpellCategories row: no hit roll) and it casts Torment through the same handler, as a
     *    charmed creature (the handler's GetCharmGuid branch); the charm is then taken off in a
     *    setup window, and the factory AI its removal installs is silenced at once.
     * Every caster faces its target from the start, so no cast turns anybody (a turn is an
     * SMSG_MONSTER_MOVE, recorded by its size alone). The pet's passive holders 35695, 35697 and
     * 91702 carry proc flags with a proc chance of 0: roll_chance_f(0) never passes
     * (UnitAuraProcHandler.cpp:519-540), so the scenario counts only holders with a chance
     * (LiveProcHolders) and names the others in the record.
     *
     * Pins: the stored keys of both of Creature's maps for the pet and the charmed creature, the
     * relation between the spell's stored end and the category's stored start (max(recovery,
     * category recovery) seconds apart, or one less when the two time(NULL) reads straddle a
     * second), and what CheckPetCast answers on the second cast, through SMSG_PET_CAST_FAILED.
     */
    class SpellCreatureCooldowns : public SpellScenario
    {
    public:
        SpellCreatureCooldowns()
            : SpellScenario("spell-creature-cooldowns", 938,
                            { "template", "petFirstCast", "petSecondCastRefused", "charmedCast", "noPersistence", "digest" }) {}

        void Prepare() override
        {
            struct St
            {
                ObjectGuid player, pet, target, charmed;
                std::string templateOk;
                uint32 levelAtSpawn = 0, levelSet = 0;
                bool noReachLevel = false;
                std::set<uint32> achievementsAtSpawn;
                bool dealsDamage = false;
                QuestPlan plan;               // for noPersistence's backstops
                std::string procSelf, procPet, procTarget, procCharmed;
                int first = -2, second = -2, charmedResult = -2;
                std::string petKeysAfterFirst, petCatKeysAfterFirst, petKeysAfterSecond, charmedKeys, charmedCatKeys;
                bool storedGapOk = false, endAhead = false, charmedGapOk = false, charmOn = false;
                bool ran[4] = { false, false, false, false };
            };
            const uint32 TORMENT = 3716, CHARM = 21835;
            const uint32 VOIDWALKER = 1860, cougar = 2961;
            const uint32 level = 20;

            std::vector<SpellPrint> prints = {
                // 3716: 26 fields, fnv 3284c579
                { 3716, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 21835: 26 fields, fnv 1ffcc170
                { 21835, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 6 }, { "eff0.EffectBasePoints", 100 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 25 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
            QuestPlan plan;
            plan.quest = 0;
            plan.classId = CLASS_WARLOCK;
            plan.level = level;
            plan.dealsDamage = true;        // the pet's Torment -- its damage is credited to the owner
            std::string templateOk;
            QuestPreCheck pre;
            if (!Qualify(prints, { VOIDWALKER, cougar }, plan, templateOk, &pre))
            {
                return;
            }

            Player* p = SpawnPlayer(P0.x, P0.y, Ground(P0.x, P0.y, P0.z), 0.0f, CLASS_WARLOCK);
            if (!p)
            {
                Verdict(Invalid("spawn failed"));
                return;
            }
            StartTimeSync(p);
            const float tx = P0.x + 5.0f;
            Creature* target = Spawn(cougar, tx, P0.y, Ground(tx, P0.y, P0.z), 3.14159f);
            const float vy = P0.y + 4.0f;
            // facing the cougar, so the cast turns nobody (a creature turning to its target sends a
            // spline, SMSG_MONSTER_MOVE, whose rule the record cannot change: 930's spawn line holds it)
            Creature* charmed = Spawn(VOIDWALKER, P0.x + 4.0f, vy, Ground(P0.x + 4.0f, vy, P0.z), Facing(P0.x + 4.0f, vy, tx, P0.y));
            if (!target || !charmed)
            {
                Verdict(Invalid("a creature (2961 or 1860) did not spawn"));
                return;
            }
            Silence(target);
            Park(target);
            Silence(charmed);
            Park(charmed);

            auto st = std::make_shared<St>();
            st->player = p->GetObjectGuid();
            st->target = target->GetObjectGuid();
            st->charmed = charmed->GetObjectGuid();
            st->templateOk = templateOk;
            st->levelAtSpawn = p->getLevel();
            st->noReachLevel = pre.levelBound == pre.startLevel;
            st->achievementsAtSpawn = Achievements(p);
            st->dealsDamage = plan.dealsDamage;
            st->plan = plan;

            // the pet is built before the recorder starts, so its guid can be watched; its own
            // setup (the level, the stats, the spell) runs in the recorder's setup windows below
            SetLevelAsResetDoes(p, level);
            Pet* pet = BuildVoidwalker(p, VOIDWALKER);
            if (!pet)
            {
                Verdict(Invalid("the voidwalker pet could not be built"));
                return;
            }
            st->pet = pet->GetObjectGuid();
            st->levelSet = p->getLevel();

            SpellWatch watch;
            watch.target = st->target;
            watch.caster = st->charmed;
            watch.pet = st->pet;
            watch.creatureCooldowns = true;
            m_rec.Begin(Name(), p, watch);
            m_rec.Open("learn", false);          // a setup window: logged, never digested
            pet->learnSpell(TORMENT);

            At(300, [this, st]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                Pet* pet = FindPet(st->pet);
                Creature* target = Get(st->target);
                Creature* charmed = Get(st->charmed);
                if (!p || !pet || !target || !charmed) { return; }
                m_rec.Open("pet-cast1");
                st->procSelf = LiveProcHolders(p);
                st->procPet = LiveProcHolders(pet);
                st->procTarget = LiveProcHolders(target);
                st->procCharmed = LiveProcHolders(charmed);
                m_rec.Note("holders with proc flags but proc chance 0 (roll_chance_f(0) never passes): pet [" + DeadProcHolders(pet) + "]");
                m_rec.Note("holders that can proc (flags and a chance): self [" + st->procSelf + "], pet [" + st->procPet + "], target [" + st->procTarget +
                           "], caster [" + st->procCharmed + "]");
                st->first = PetCastAsHandler(p, pet, TORMENT, target, 1);
                st->petKeysAfterFirst = Keys(pet->m_CreatureSpellCooldowns);
                st->petCatKeysAfterFirst = Keys(pet->m_CreatureCategoryCooldowns);
                st->storedGapOk = GapIsMax(pet, TORMENT);
                m_rec.Note("the pet's stored keys: spells [" + st->petKeysAfterFirst + "], categories [" + st->petCatKeysAfterFirst + "]");
                m_rec.Open("pet-cast1+tick");
                st->ran[0] = true;
            });
            At(2000, [this, st, TORMENT]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                Pet* pet = FindPet(st->pet);
                Creature* target = Get(st->target);
                if (!p || !pet || !target) { return; }
                m_rec.Open("pet-cast2");
                // ONE read of the wall clock decides whether the second cast is made at all.
                // Spell::CheckPetCast reads its own time(NULL) inside the core
                // (Creature::HasSpellCooldown, CreatureSpellCooldown.cpp:122-126), which the harness
                // cannot hand a value; so the stored end must lie more than a second past this read:
                // the check's own read, microseconds later in this same call, then sees it ahead too,
                // and the two cannot disagree unless the call itself takes over a second.
                const time_t now = time(NULL);
                auto end = pet->m_CreatureSpellCooldowns.find(TORMENT);
                st->endAhead = end != pet->m_CreatureSpellCooldowns.end() && end->second > now + 1;
                m_rec.Note(std::string("the stored end is ahead of the wall clock: ") + (st->endAhead ? "yes" : "no"));
                if (!st->endAhead)
                {
                    // A stalled run: the wall clock passed (or is about to pass) the stored end, so
                    // CheckPetCast's answer is not fixed and a real second cast could land in the
                    // digest. The cast is NOT made; the pet is taken back (its save's owner gate
                    // closed first, as always) and every category reads INVALID.
                    m_rec.End();
                    EndVoidwalker(p, pet);
                    Verdict(Invalid("the wall clock passed the pet's stored cooldown end before the second cast (a stalled run): CheckPetCast's answer is not fixed, and the cast was not made"));
                    return;
                }
                st->second = PetCastAsHandler(p, pet, TORMENT, target, 2);
                st->petKeysAfterSecond = Keys(pet->m_CreatureSpellCooldowns);
                m_rec.Open("pet-cast2+tick");
                st->ran[1] = true;
            });
            At(2300, [this, st]()
            {
                // the pet taken back before the charm: a player holding a pet cannot charm
                // (Spell::CheckCast answers SPELL_FAILED_ALREADY_HAVE_CHARM)
                m_rec.Open("pet-released", false);   // a setup window: the pet's end
                EndVoidwalker(sPlayerRegistry.Find(st->player), FindPet(st->pet));
            });
            At(2600, [this, st]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                Creature* charmed = Get(st->charmed);
                if (!p || !charmed) { return; }
                m_rec.Open("charm");
                p->CastSpell(charmed, CHARM, true);
                st->charmOn = charmed->GetCharmerGuid() == p->GetObjectGuid() && p->GetCharmGuid() == charmed->GetObjectGuid();
                m_rec.Note(std::string("charmed by the warlock: ") + (st->charmOn ? "yes" : "no"));
                m_rec.Open("charm+tick");
                st->ran[2] = true;
            });
            At(2900, [this, st]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                Creature* charmed = Get(st->charmed);
                Creature* target = Get(st->target);
                if (!p || !charmed || !target || !st->charmOn) { return; }
                m_rec.Open("charmed-cast");
                st->charmedResult = PetCastAsHandler(p, charmed, TORMENT, target, 3);
                st->charmedKeys = Keys(charmed->m_CreatureSpellCooldowns);
                st->charmedCatKeys = Keys(charmed->m_CreatureCategoryCooldowns);
                st->charmedGapOk = GapIsMax(charmed, TORMENT);
                m_rec.Note("the charmed creature's stored keys: spells [" + st->charmedKeys + "], categories [" + st->charmedCatKeys + "]");
                m_rec.Open("charmed-cast+tick");
                st->ran[3] = true;
            });
            At(3200, [this, st]()
            {
                Creature* charmed = Get(st->charmed);
                m_rec.Open("charm-released", false); // a setup window: the charm taken back
                if (charmed)
                {
                    // The charm's removal hands the creature a fresh factory AI
                    // (Unit::ResetControlState), which turns on the warlock at once. The recording
                    // decorator goes back over it and is silenced the moment the call returns, and
                    // the combat it started is stopped, so nothing swings before the verdict.
                    charmed->RemoveAurasDueToSpell(CHARM);
                    charmed->SetAI(new HarnessAI(charmed, charmed->AI(), this));
                    Silence(charmed);
                    charmed->CombatStop(true);
                }
            });

            At(3500, [this, st]()
            {
                m_rec.End();
                Player* p = sPlayerRegistry.Find(st->player);
                if (!p)
                {
                    Verdict(Invalid("the harness player was gone at the verdict"));
                    return;
                }
                const bool allRan = st->ran[0] && st->ran[1] && st->ran[2] && st->ran[3];
                const bool procs = !st->procSelf.empty() || !st->procPet.empty() || !st->procTarget.empty() || !st->procCharmed.empty();
                if (!allRan || procs)
                {
                    const std::string why = !allRan ? "INVALID(a step did not run)"
                                                    : "INVALID(holders that can proc: self [" + st->procSelf + "], pet [" + st->procPet + "], target [" + st->procTarget +
                                                          "], caster [" + st->procCharmed + "])";
                    Verdict(Compose({ st->templateOk, why, why, why, why, DigestValue("the first cast") }));
                    return;
                }
                std::string first = "(CheckPetCast " + U(uint32(st->first)) + "; SPELL_GO " + Joined(Records("pet-cast1", SMSG_SPELL_GO)) +
                                    "; the pet's stored keys: spells [" + st->petKeysAfterFirst + "], categories [" + st->petCatKeysAfterFirst +
                                    "]; the spell's stored end is max(recovery, category recovery) after the category's stored start: " +
                                    (st->storedGapOk ? "yes" : "no") + ")";
                first = ((st->first == SPELL_CAST_OK && st->storedGapOk) ? "OK" : "BUG") + first;
                // endAhead is true here: a stalled run printed its all-INVALID verdict at the cast step
                std::string second = "(CheckPetCast " + U(uint32(st->second)) + "; PET_CAST_FAILED " + Joined(Records("pet-cast2", SMSG_PET_CAST_FAILED)) +
                                     "; CLEAR_COOLDOWNS " + Joined(Records("pet-cast2", SMSG_CLEAR_COOLDOWNS)) + "; SPELL_START " +
                                     Joined(Records("pet-cast2", SMSG_SPELL_START)) + "; the pet's stored spell keys [" + st->petKeysAfterSecond + "])";
                second = ((st->second == SPELL_FAILED_NOT_READY) ? "OK" : "BUG") + second;
                std::string charmed = "(CheckPetCast " + U(uint32(st->charmedResult)) + "; SPELL_GO " + Joined(Records("charmed-cast", SMSG_SPELL_GO)) +
                                      "; the charmed creature's stored keys: spells [" + st->charmedKeys + "], categories [" + st->charmedCatKeys +
                                      "]; end max(recovery, category recovery) after the start: " + (st->charmedGapOk ? "yes" : "no") + ")";
                charmed = ((st->charmedResult == SPELL_CAST_OK && st->charmedGapOk) ? "OK" : "BUG") + charmed;
                Verdict(Compose({ st->templateOk, first, second, charmed,
                                  NoPersistence(p, st->achievementsAtSpawn, st->levelSet, st->noReachLevel, st->dealsDamage, &st->plan), DigestValue("the first cast") }));
            });
        }

    private:
        /// The keys of one of Creature's cooldown maps, ascending.
        template <class M>
        static std::string Keys(M const& map)
        {
            std::string out;
            for (auto i = map.begin(); i != map.end(); ++i)
            {
                out += (out.empty() ? "" : ",") + U(i->first);
            }
            return out;
        }

        /// Today's model, read off the stored values without printing any: the spell's stored end
        /// minus the category's stored start is max(recovery, category recovery) in seconds -- or
        /// one less, when the two time(NULL) reads of AddCreatureSpellCooldown straddle a second.
        static bool GapIsMax(Creature const* c, uint32 spellId)
        {
            SpellEntry const* info = sSpellStore.LookupEntry(spellId);
            if (!info)
            {
                return false;
            }
            auto end = c->m_CreatureSpellCooldowns.find(spellId);
            auto start = c->m_CreatureCategoryCooldowns.find(info->GetCategory());
            if (end == c->m_CreatureSpellCooldowns.end() || start == c->m_CreatureCategoryCooldowns.end())
            {
                return false;
            }
            const int64 gap = int64(end->second) - int64(start->second);
            const int64 max = int64(GetSpellRecoveryTime(info) / IN_MILLISECONDS);
            return gap == max || gap == max - 1;
        }

        Pet* FindPet(ObjectGuid guid) const
        {
            Map* map = GetMap();
            return map ? map->GetPet(guid) : NULL;
        }

        /// The orientation from (x, y) towards (tx, ty), in [0, 2 pi).
        static float Facing(float x, float y, float tx, float ty)
        {
            float o = std::atan2(ty - y, tx - x);
            return o < 0.0f ? o + 2.0f * float(M_PI) : o;
        }

        /// Scenario::BuildOwnedPet (ScenariosControl.cpp's BuildPet recipe, shared), for the
        /// Voidwalker: three yards east and three south of the owner, facing east along the line
        /// the cougar stands on, so its cast turns nobody.
        Pet* BuildVoidwalker(Player* owner, uint32 entry)
        {
            if (!owner)
            {
                return NULL;
            }
            const float x = owner->Where().X() + 3.0f;
            const float y = owner->Where().Y() - 3.0f;
            return BuildOwnedPet(owner, entry, x, y, Facing(x, y, owner->Where().X() + 5.0f, owner->Where().Y()));
        }

        /// ScenariosControl.cpp's EndPet order: the owner guid cleared first (SavePetToDB's owner
        /// gate, PetDatabase.cpp:407, closed for every path out), then the charm, the pet guid, the
        /// removal.
        void EndVoidwalker(Player* p, Pet* pet)
        {
            if (!pet)
            {
                return;
            }
            pet->SetOwnerGuid(ObjectGuid());
            if (p)
            {
                if (p->GetCharmGuid() == pet->GetObjectGuid())
                {
                    p->ResetControlState(false);
                }
                if (p->GetPetGuid() == pet->GetObjectGuid())
                {
                    p->SetPet(NULL);
                }
            }
            pet->Unsummon(PET_SAVE_NOT_IN_SLOT);
        }
    };

    /**
     * What the attack power scenarios share (958-961): a human of the scenario's class staged to level 85 with
     * kAttackPower added to his melee and his ranged attack power, the scenario's spell power and its spells
     * (Scenario::StagePlayer), and a Camel (50249) staged to level 85 kTargetYards in front of him
     * (Scenario::SpawnTarget). Then kCasts casts of the scenario's spell at the Camel through the CAST_SPELL handler's
     * calls, one every CastEveryMs(), each read kReadAfterMs after it. Before each cast his power is set to its maximum
     * and the spell's cooldown taken off him by call (a cooldown ends on the wall clock, which the stepped clock does
     * not move), and for a spell that asks for a wounded target the Camel's health is set to TargetHealthPct() percent
     * of its maximum, as it is once at the staging, before the map update that sets its health aura states. A cast is
     * not made when a holder on either unit that a proc can reach is not one the guard fingerprinted. Each cast's rolls
     * are drawn in map updates under the map hook's RNG::Seed(TickSeed(base, order, elapsed)), so every run draws the
     * same ones.
     *
     * Categories: `template`; `staged` his level, his melee and ranged attack power with the part the plan added to
     * each, the spell power of the damaging spell's school, his physical damage-done field (the flat physical
     * damage-done auras), his main hand item, his power, the Camel's level, health and armor and the holders that can
     * proc, OK when the level, the two added parts and the Camel's level are the plan's; `casts` each cast's SPELL_GO
     * spells and the damage log of the damaging spell with the Camel's health lost equal to its amount, a BUG when a
     * cast was refused or not read, or neither hit nor missed; `amounts` the amounts in cast order (a crit marked `c`,
     * a miss `miss`); `noPersistence`; `digest`.
     */
    class AttackPowerScenario : public SpellScenario
    {
    public:
        AttackPowerScenario(char const* name, int order)
            : SpellScenario(name, order, { "template", "staged", "casts", "amounts", "noPersistence", "digest" }) {}

        void Prepare() override
        {
            const uint32 level = 85;
            const std::vector<SpellPrint> prints = Prints();

            QuestPlan plan;
            plan.quest = 0;
            plan.classId = Class();
            plan.level = level;
            plan.dealsDamage = true;
            for (size_t i = 0; i < prints.size(); ++i)
            {
                if (!prints[i].fields.count("absent"))
                {
                    plan.casts.insert(prints[i].spell);
                }
            }
            const std::vector<uint32> spells = Spells();
            plan.spells.insert(spells.begin(), spells.end());
            std::string templateOk;
            QuestPreCheck pre;
            if (!Qualify(prints, { kCamel }, plan, templateOk, &pre))
            {
                return;
            }

            Player* p = SpawnPlayer(P0.x, P0.y, Ground(P0.x, P0.y, P0.z), 0.0f, Class());
            if (!p)
            {
                Verdict(Invalid("spawn failed"));
                return;
            }
            const float tx = P0.x + kTargetYards;
            Creature* target = SpawnTarget(kCamel, level, tx, P0.y, Ground(tx, P0.y, P0.z), 3.14159f);
            if (!target)
            {
                Verdict(Invalid("the target (50249) did not spawn"));
                return;
            }
            StartTimeSync(p);

            auto st = std::make_shared<St>();
            st->player = p->GetObjectGuid();
            st->target = target->GetObjectGuid();
            st->templateOk = templateOk;
            st->noReachLevel = pre.levelBound == pre.startLevel;
            st->achievementsAtSpawn = Achievements(p);
            st->dealsDamage = plan.dealsDamage;
            st->plan = plan;
            for (size_t i = 0; i < prints.size(); ++i)
            {
                st->printed.insert(prints[i].spell);
            }
            st->casts.resize(kCasts);

            SpellWatch watch;
            watch.target = st->target;
            watch.combatLogFields = true;
            m_rec.Begin(Name(), p, watch);
            m_rec.Open("stage", false);           // a setup window: logged, never digested
            StagePlan stage;
            stage.level = level;
            stage.attackPower = kAttackPower;
            stage.spellPower = SpellPower();
            stage.spells = spells;
            st->staged = StagePlayer(*p, stage);
            st->levelSet = p->getLevel();
            if (const uint32 pct = TargetHealthPct())
            {
                // before the first map update, which sets the Camel's health aura states the spell asks for
                target->SetHealth(target->GetMaxHealth() * pct / 100);
            }
            ReadStage(*st, p, target, level);

            for (uint32 k = 0; k < kCasts; ++k)
            {
                At(300 + CastEveryMs() * k, [this, st, k]() { RunCast(*st, k); });
                At(300 + CastEveryMs() * k + kReadAfterMs, [this, st, k]() { ReadCast(*st, k); });
            }
            At(300 + CastEveryMs() * kCasts, [this, st]() { Finish(*st); });
        }

    protected:
        static constexpr uint32 kCamel = 50249;
        static constexpr int32 kAttackPower = 1000;
        static constexpr float kTargetYards = 20.0f;
        static constexpr uint32 kCasts = 5;
        static constexpr uint32 kReadAfterMs = 3600;

        /// The class of the human SpawnPlayer builds.
        virtual uint8 Class() const = 0;
        /// The spell he casts.
        virtual uint32 CastSpell() const = 0;
        /// The spell whose damage log carries each cast's amount: the cast spell, or the spell it casts.
        virtual uint32 HitSpell() const { return CastSpell(); }
        /// The spells to stage.
        virtual std::vector<uint32> Spells() const { return std::vector<uint32>(1, CastSpell()); }
        /// The spell power to stage.
        virtual int32 SpellPower() const { return 0; }
        /// The percent of its maximum the Camel's health is set to before each cast; 0 leaves it.
        virtual uint32 TargetHealthPct() const { return 0; }
        /// The time from one cast to the next.
        virtual uint32 CastEveryMs() const { return 4100; }
        /// Every spell the scenario reaches: the casts, the holders that can proc and what they cast.
        virtual std::vector<SpellPrint> Prints() const = 0;

    private:
        /// One cast, as its two steps saw it.
        struct Cast
        {
            bool ran = false, refused = false, read = false;
            uint32 castId = 0;
            uint32 healthBefore = 0, healthAfter = 0;
            bool logRead = false, crit = false, missed = false;
            uint32 amount = 0, failed = 0;
            std::string spells;                 ///< the SPELL_GO spells of its windows, in order
            std::string hitLine;
        };

        struct St
        {
            ObjectGuid player;
            ObjectGuid target;
            std::string templateOk;
            uint32 levelSet = 0;
            bool noReachLevel = false;
            std::set<uint32> achievementsAtSpawn;
            bool dealsDamage = false;
            QuestPlan plan;
            bool staged = false;
            std::string stagedText;
            bool stagedOk = false;
            std::set<uint32> printed;
            std::vector<Cast> casts;
        };

        void ReadStage(St& st, Player* p, Creature* target, uint32 level)
        {
            SpellEntry const* hit = sSpellStore.LookupEntry(HitSpell());
            const int32 spellPower = hit ? p->SpellBaseDamageBonusDone(GetSpellSchoolMask(hit)) : 0;
            const float melee = p->GetTotalAttackPowerValue(BASE_ATTACK);
            const float ranged = p->GetTotalAttackPowerValue(RANGED_ATTACK);
            const float meleeAdded = p->GetModifierValue(UNIT_MOD_ATTACK_POWER, TOTAL_VALUE);
            const float rangedAdded = p->GetModifierValue(UNIT_MOD_ATTACK_POWER_RANGED, TOTAL_VALUE);
            Item* mainHand = p->GetWeaponForAttack(BASE_ATTACK, true, false);
            char buf[512];
            snprintf(buf, sizeof(buf),
                     "level %u; attack power %.2f melee, %.2f ranged, of which %.2f and %.2f staged; spell power %d (school mask %u); physical damage-done field %d; main hand item %u; power %u/%u; target %u level %u, health %u/%u, armor %u",
                     p->getLevel(), melee, ranged, meleeAdded, rangedAdded, spellPower, hit ? GetSpellSchoolMask(hit) : 0,
                     p->GetInt32Value(PLAYER_FIELD_MOD_DAMAGE_DONE_POS),
                     mainHand ? mainHand->GetEntry() : 0, p->GetPower(p->GetPowerType()), p->GetMaxPower(p->GetPowerType()),
                     target->GetEntry(), target->getLevel(), target->GetHealth(), target->GetMaxHealth(), target->GetArmor());
            st.stagedText = std::string(buf) + "; holders that can proc: self [" + ProcHolders(p) + "] (by chance [" + LiveProcHolders(p) +
                            "]), target [" + ProcHolders(target) + "]";
            st.stagedOk = st.staged && p->getLevel() == level && meleeAdded == float(kAttackPower) && rangedAdded == float(kAttackPower) &&
                          target->getLevel() == level;
            m_rec.Note("staged: " + st.stagedText);
        }

        void RunCast(St& st, uint32 k)
        {
            Player* p = sPlayerRegistry.Find(st.player);
            Creature* target = Get(st.target);
            if (!p || !target || !st.staged)
            {
                return;
            }
            Cast& c = st.casts[k];
            const std::string name = "cast" + U(k + 1);
            m_rec.Open(name);
            c.ran = true;
            m_rec.Note("holders that can proc: self [" + ProcHolders(p) + "], target [" + ProcHolders(target) + "]");
            const std::string unprintedSelf = ProcHolders(p, st.printed);
            const std::string unprintedTarget = ProcHolders(target, st.printed);
            if (!unprintedSelf.empty() || !unprintedTarget.empty())
            {
                c.refused = true;
                m_rec.Note("holders that can proc and are not fingerprinted: self [" + unprintedSelf + "], target [" + unprintedTarget +
                           "]; not cast");
                m_rec.Open(name + "+tick");
                return;
            }
            p->SetPower(p->GetPowerType(), p->GetMaxPower(p->GetPowerType()));
            p->RemoveSpellCooldown(CastSpell());
            if (const uint32 pct = TargetHealthPct())
            {
                target->SetHealth(target->GetMaxHealth() * pct / 100);
            }
            c.healthBefore = target->GetHealth();
            m_rec.Note("power " + U(p->GetPower(p->GetPowerType())) + "/" + U(p->GetMaxPower(p->GetPowerType())) + "; target health " +
                       U(c.healthBefore) + "/" + U(target->GetMaxHealth()));
            c.castId = CastAsHandler(p, CastSpell(), target, uint8(k + 1));
            m_rec.Open(name + "+tick");
        }

        void ReadCast(St& st, uint32 k)
        {
            Player* p = sPlayerRegistry.Find(st.player);
            Creature* target = Get(st.target);
            Cast& c = st.casts[k];
            if (!p || !target || !c.ran || c.refused)
            {
                return;
            }
            const std::string name = "cast" + U(k + 1) + "-read";
            m_rec.Open(name);
            c.read = true;
            c.healthAfter = target->GetHealth();
            m_rec.Note("target health " + U(c.healthAfter) + "/" + U(target->GetMaxHealth()) + "; power " + U(p->GetPower(p->GetPowerType())));
            m_rec.Open(name + "+tick");
        }

        /// Each cast's windows as the record holds them: the SPELL_GO packets, the damage log of HitSpell(), CAST_FAILED.
        void ReadSeen(St& st) const
        {
            Trace::Roles const& roles = m_rec.Roles();
            for (uint32 k = 0; k < st.casts.size(); ++k)
            {
                Cast& c = st.casts[k];
                const std::string base = "cast" + U(k + 1);
                const char* suffixes[] = { "", "+tick", "-read", "-read+tick" };
                for (const char* suffix : suffixes)
                {
                    const std::string window = base + suffix;
                    c.failed += m_rec.CountIn(window, SMSG_CAST_FAILED);
                    std::vector<Recorder::Seen const*> gos = m_rec.SeenIn(window, SMSG_SPELL_GO);
                    for (size_t i = 0; i < gos.size(); ++i)
                    {
                        std::string go;
                        uint32 spell = 0;
                        if (gos[i]->payload.empty() || !Trace::DecodeSpellCast(true, &gos[i]->payload[0], gos[i]->payload.size(), roles, go) ||
                            sscanf(go.c_str(), "spell=%u", &spell) != 1)
                        {
                            continue;
                        }
                        c.spells += (c.spells.empty() ? "" : "+") + U(spell);
                        if (spell == HitSpell())
                        {
                            c.missed = c.missed || go.find(" hit=[] miss=[target:") != std::string::npos;
                        }
                    }
                    std::vector<Recorder::Seen const*> logs = m_rec.SeenIn(window, SMSG_SPELLNONMELEEDAMAGELOG);
                    for (size_t i = 0; i < logs.size(); ++i)
                    {
                        if (logs[i]->payload.empty())
                        {
                            continue;
                        }
                        std::vector<std::string> fields = Trace::CombatLogFields(SMSG_SPELLNONMELEEDAMAGELOG, &logs[i]->payload[0],
                                                                                 logs[i]->payload.size(), roles);
                        uint32 spell = 0, amount = 0, crit = 0, absorbed = 0, resisted = 0;
                        if (fields.size() == 1 &&
                            sscanf(fields[0].c_str(), "hit self.hit.%u=%u/%u/%u/%u", &spell, &amount, &crit, &absorbed, &resisted) == 5 &&
                            spell == HitSpell() && !c.logRead)
                        {
                            c.logRead = true;
                            c.crit = crit != 0;
                            c.amount = amount;
                            c.hitLine = fields[0];
                        }
                    }
                }
            }
        }

        void Finish(St& st)
        {
            m_rec.End();   // the sink comes off before the verdict
            Player* p = sPlayerRegistry.Find(st.player);
            if (!p)
            {
                Verdict(Invalid("the harness player was gone at the verdict"));
                return;
            }
            const std::string persistence = NoPersistence(p, st.achievementsAtSpawn, st.levelSet, st.noReachLevel, st.dealsDamage, &st.plan);
            if (!st.staged)
            {
                const std::string why = "INVALID(StagePlayer refused; its ERR line names the step)";
                Verdict(Compose({ st.templateOk, why, why, why, persistence, DigestValue("the first cast") }));
                return;
            }
            ReadSeen(st);
            std::string values, castText, castBug;
            for (uint32 k = 0; k < st.casts.size(); ++k)
            {
                Cast const& c = st.casts[k];
                std::string value;
                std::string problem;
                if (!c.ran || c.refused)
                {
                    value = "refused";
                    problem = "refused before the cast, a holder the guard did not fingerprint can proc";
                }
                else if (!c.castId)
                {
                    value = "unknown";
                    problem = "the spellbook refused " + U(CastSpell());
                }
                else if (!c.read)
                {
                    value = "unread";
                    problem = "the read step did not run";
                }
                else if (c.logRead)
                {
                    value = U(c.amount) + (c.crit ? "c" : "");
                    if (c.amount != c.healthBefore - c.healthAfter)
                    {
                        problem = c.hitLine + ", but the target's health " + U(c.healthBefore) + " -> " + U(c.healthAfter);
                    }
                }
                else if (c.missed)
                {
                    value = "miss";
                    if (c.healthBefore != c.healthAfter)
                    {
                        problem = "a miss, but the target's health " + U(c.healthBefore) + " -> " + U(c.healthAfter);
                    }
                }
                else
                {
                    value = "none";
                    problem = "no damage log of " + U(HitSpell()) + " and no miss; SPELL_GO [" + c.spells + "], CAST_FAILED x" + U(c.failed);
                }
                if (!problem.empty() && castBug.empty())
                {
                    castBug = "cast " + U(k + 1) + ": " + problem;
                }
                values += (k ? "," : "") + value;
                castText += (k ? "; " : "") + U(k + 1) + " [" + c.spells + "] " + (c.logRead ? c.hitLine.substr(4) : value) + ", health " +
                            U(c.healthBefore) + "->" + U(c.healthAfter);
            }
            const std::string staged = std::string(st.stagedOk ? "OK" : "BUG") + "(" + st.stagedText + ")";
            const std::string casts = castBug.empty()
                                      ? "OK(" + U(st.casts.size()) + " casts of " + U(CastSpell()) + ", each its SPELL_GO spells and the damage log of " +
                                        U(HitSpell()) + "; the target lost each amount: " + castText + ")"
                                      : "BUG(" + castBug + ")";
            const std::string amounts = values + "(" + U(HitSpell()) + " by cast, a crit marked c)";
            Verdict(Compose({ st.templateOk, staged, casts, amounts, persistence, DigestValue("the first cast") }));
        }
    };

    /**
     * S958 `warrior-heroic-throw-ap`: Heroic Throw (57755) by a warrior. One SCHOOL_DAMAGE effect, physical, its damage
     * class melee; Spell.dbc's attack power coefficient for it is 0.75. Its cooldown is taken off him before each cast.
     * Every warrior is created with Defensive State (5301, which names Defensive State 5302, not in the client's data)
     * and Victorious State (32215, which casts 32216 on a kill): both are fingerprinted.
     */
    class WarriorHeroicThrowAp : public AttackPowerScenario
    {
    public:
        WarriorHeroicThrowAp() : AttackPowerScenario("warrior-heroic-throw-ap", 958) {}

    protected:
        uint8 Class() const override { return CLASS_WARRIOR; }
        uint32 CastSpell() const override { return 57755; }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 57755: 26 fields, fnv 0f6d1699
                { 57755, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 12 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 5301: 49 fields, fnv de636d32
                { 5301, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 5302 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 49 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                { 5302, { { "absent", 1 } } },
                // 32215: 26 fields, fnv 661fa801
                { 32215, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 32216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 32216: 26 fields, fnv ba87eed9
                { 32216, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
        }
    };

    /**
     * S959 `paladin-hammer-of-wrath-ap`: Hammer of Wrath (24275) by a paladin staged with kSpellPower spell power as
     * well. One SCHOOL_DAMAGE effect, holy, its damage class melee; Spell.dbc's attack power coefficient for it is
     * 0.39. It asks for a target under 20% of its health, so the Camel's health is set to 15% of its maximum before each
     * cast, more than one crit takes. One cast every 6100 ms, longer than its 6 s cooldown, which is also taken off him
     * before each cast.
     */
    class PaladinHammerOfWrathAp : public AttackPowerScenario
    {
    public:
        PaladinHammerOfWrathAp() : AttackPowerScenario("paladin-hammer-of-wrath-ap", 959) {}

    protected:
        uint8 Class() const override { return CLASS_PALADIN; }
        uint32 CastSpell() const override { return 24275; }
        int32 SpellPower() const override { return kSpellPower; }
        uint32 TargetHealthPct() const override { return 15; }
        uint32 CastEveryMs() const override { return 6100; }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 24275: 26 fields, fnv 3bacb3fd
                { 24275, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 350 }, { "eff0.EffectBonusCoefficient", 1039113650 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 37 }, { "eff0.EffectRealPointsPerLevel", 1075419546 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
        }

    private:
        static constexpr int32 kSpellPower = 1000;
    };

    /**
     * S960 `deathknight-death-coil-ap`: Death Coil (47541) by a death knight. Its dummy effect casts Death Coil 47632
     * at an enemy with its own amount (SpellEffectDummy.cpp), whose one SCHOOL_DAMAGE effect, shadow, its damage
     * class magic, holds no coefficient in either client file; its attack power share is spell_bonus_data's row for
     * 47632. His runic power is set to its maximum before each cast. Every death knight is created with Offensive State
     * (45903, which names 45904, not in the client's data) and Death Strike Enabler (89832), and at level 85 holds Rune
     * Strike's passive (56816, which casts 56817 on him): all three are fingerprinted.
     */
    class DeathKnightDeathCoilAp : public AttackPowerScenario
    {
    public:
        DeathKnightDeathCoilAp() : AttackPowerScenario("deathknight-death-coil-ap", 960) {}

    protected:
        uint8 Class() const override { return CLASS_DEATH_KNIGHT; }
        uint32 CastSpell() const override { return 47541; }
        uint32 HitSpell() const override { return 47632; }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 47541: 26 fields, fnv 00dd029a
                { 47541, { { "eff0.present", 1 }, { "eff0.Effect", 3 }, { "eff0.EffectBasePoints", 167 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 25 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 47632: 26 fields, fnv 111117a2
                { 47632, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 600 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 45903: 26 fields, fnv 666c5c65
                { 45903, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 45904 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                { 45904, { { "absent", 1 } } },
                // 89832: 26 fields, fnv ba87eed9
                { 89832, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 56816: 26 fields, fnv 3c5ee47e
                { 56816, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 56817 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 56817: 26 fields, fnv ba87eed9
                { 56817, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
        }
    };

    /**
     * S961 `priest-mortar-shot-physical`: Mortar Shot (16786) by a priest staged with kSpellPower spell power. One
     * SCHOOL_DAMAGE effect, physical, its damage class none, so its damage takes the spell path; SpellEffect.dbc holds
     * 1.0 for it and Spell.dbc no attack power coefficient. Its targets are filled around the explicit target (A=17 with
     * B=8, as 937 shows), the Camel alone. It has no hit roll, no cooldown, no cast time and no missile.
     */
    class PriestMortarShotPhysical : public AttackPowerScenario
    {
    public:
        PriestMortarShotPhysical() : AttackPowerScenario("priest-mortar-shot-physical", 961) {}

    protected:
        uint8 Class() const override { return CLASS_PRIEST; }
        uint32 CastSpell() const override { return 16786; }
        int32 SpellPower() const override { return kSpellPower; }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 16786: 26 fields, fnv 949067eb
                { 16786, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 15 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectRadiusIndex_0", 14 }, { "eff0.ImplicitTarget_0", 17 }, { "eff0.ImplicitTarget_1", 8 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
        }

    private:
        static constexpr int32 kSpellPower = 1000;
    };

    void RegisterSpellScenarios(Runner& r)
    {
        r.Register(new SpellDirectDamage());
        r.Register(new SpellAuraApplyReapplyRemove());
        r.Register(new SpellPeriodicToExpiry());
        r.Register(new SpellProcCharges());
        r.Register(new SpellCooldownsSetAndCleared());
        r.Register(new SpellCastInterrupted());
        r.Register(new SpellSelfRollHellfire());
        r.Register(new SpellMortarShotFallthrough());
        r.Register(new SpellCreatureCooldowns());
        r.Register(new SpellPeriodicOffMap());
        r.Register(new WarriorHeroicThrowAp());
        r.Register(new PaladinHammerOfWrathAp());
        r.Register(new DeathKnightDeathCoilAp());
        r.Register(new PriestMortarShotPhysical());
    }
}
