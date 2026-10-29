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

#include "spells/data/SpellTables.h"

#include <algorithm>
#include <cstddef>
#include <iterator>

namespace
{
    /// A spell and the most-targets range it fixes.
    struct SpellMaxTargetsRow
    {
        uint32 spellId;
        SpellMaxTargetsRange range;
    };

    template <std::size_t N>
    constexpr bool SortedBySpellId(SpellMaxTargetsRow const (&rows)[N])
    {
        for (std::size_t i = 1; i < N; ++i)
        {
            if (!(rows[i - 1].spellId < rows[i].spellId))
            {
                return false;
            }
        }
        return true;
    }

    template <std::size_t N>
    constexpr bool SortedBySpellId(uint32 const (&ids)[N])
    {
        for (std::size_t i = 1; i < N; ++i)
        {
            if (!(ids[i - 1] < ids[i]))
            {
                return false;
            }
        }
        return true;
    }

    /// The generic spells that fix how many targets their effects hit.
    constexpr SpellMaxTargetsRow MAX_TARGETS[] =
    {
        { 802, { 1, 1 } },   // Mutate Bug (AQ40, Emperor Vek'nilash)
        { 804, { 1, 1 } },   // Explode Bug (AQ40, Emperor Vek'lor)
        { 10258, { 2, 2 } },   // Awaken Vault Warder (Uldaman)
        { 23138, { 1, 1 } },   // Gate of Shazzrah (MC, Shazzrah)
        { 25991, { 15, 15 } },   // Poison Bolt Volley (AQ40, Pincess Huhuran)
        { 28542, { 2, 2 } },   // Life Drain (Naxx, Sapphiron)
        { 28560, { 1, 1 } },   // Summon Blizzard (Naxx, Sapphiron)
        { 28796, { 3, 3 } },   // Poison Bolt Volley (Naxx, Faerlina)
        { 29213, { 3, 3 } },   // Curse of the Plaguebringer (Naxx, Noth the Plaguebringer)
        { 30004, { 3, 3 } },   // Flame Wreath (Karazhan, Shade of Aran)
        { 30541, { 1, 1 } },   // Blaze (Magtheridon)
        { 30572, { 1, 1 } },   // Quake (Magtheridon)
        { 30769, { 1, 1 } },   // Pick Red Riding Hood (Karazhan, Big Bad Wolf)
        { 30835, { 1, 1 } },   // Infernal Relay (Karazhan, Prince Malchezaar)
        { 30843, { 5, 5 } },   // Enfeeble (Karazhan, Prince Malchezaar)
        { 31298, { 3, 3 } },   // Sleep (Hyjal Summit, Anetheron)
        { 31347, { 1, 1 } },   // Doom (Hyjal Summit, Azgalor)
        { 32312, { 1, 1 } },   // Move 1 (Karazhan, Chess Event)
        { 33711, { 1, 1 } },   // Murmur's Touch (Shadow Labyrinth, Murmur)
        { 37388, { 1, 1 } },   // Move 2 (Karazhan, Chess Event)
        { 37676, { 4, 4 } },   // Insidious Whisper (SSC, Leotheras the Blind)
        { 38028, { 4, 4 } },   // Watery Grave (SSC, Morogrim Tidewalker)
        { 38054, { 8, 8 } },   // Random Rocket Missile
        { 38794, { 1, 1 } },   // Murmur's Touch (h) (Shadow Labyrinth, Murmur)
        { 39338, { 1, 1 } },   // Karazhan - Chess, Medivh CHEAT: Hand of Medivh, Target Horde
        { 39341, { 3, 3 } },   // Karazhan - Chess, Medivh CHEAT: Fury of Medivh, Target Horde
        { 39342, { 1, 1 } },   // Karazhan - Chess, Medivh CHEAT: Hand of Medivh, Target Alliance
        { 39344, { 3, 3 } },   // Karazhan - Chess, Medivh CHEAT: Fury of Medivh, Target Alliance
        { 39992, { 3, 3 } },   // Needle Spine Targeting (BT, Warlord Najentus)
        { 40243, { 5, 5 } },   // Crushing Shadows (BT, Teron Gorefiend)
        { 40834, { 1, 1 } },   // Agonizing Flames (BT, Illidan Stormrage)
        { 40869, { 3, 3 } },   // Fatal Attraction (BT, Mother Shahraz)
        { 41303, { 3, 3 } },   // Soul Drain (BT, Reliquary of Souls)
        { 41376, { 3, 3 } },   // Spite (BT, Reliquary of Souls)
        { 41537, { 1, 1 } },   // Summon Enslaved Soul (BT, Reliquary of Souls)
        { 42005, { 5, 5 } },   // Bloodboil (BT, Gurtogg Bloodboil)
        { 44869, { 1, 1 } },   // Spectral Blast (SWP, Kalecgos)
        { 45391, { 1, 1 } },   // Summon Demonic Vapor (SWP, Felmyst)
        { 45641, { 5, 5 } },   // Fire Bloom (SWP, Kil'jaeden)
        { 45785, { 1, 1 } },   // Sinister Reflection Clone (SWP, Kil'jaeden)
        { 45863, { 1, 1 } },   // Cosmetic - Incinerate to Random Target (Borean Tundra)
        { 45892, { 1, 1 } },   // Sinister Reflection (SWP, Kil'jaeden)
        { 45976, { 1, 1 } },   // Open Portal (SWP, M'uru)
        { 46372, { 1, 1 } },   // Ice Spear Target Picker (Slave Pens, Ahune)
        { 46650, { 4, 4 } },   // Open Brutallus Back Door (SWP, Felmyst)
        { 46771, { 3, 5 } },   // Flame Sear (SWP, Grand Warlock Alythess)
        { 47669, { 1, 1 } },   // Awaken Subboss (Utgarde Pinnacle)
        { 48278, { 1, 1 } },   // Paralyze (Utgarde Pinnacle)
        { 50742, { 1, 1 } },   // Ooze Combine (Halls of Stone)
        { 50988, { 1, 1 } },   // Glare of the Tribunal (Halls of Stone)
        { 51003, { 1, 1 } },   // Summon Dark Matter Target (Halls of Stone)
        { 51146, { 1, 1 } },   // Summon Searing Gaze Target (Halls Of Stone)
        { 51904, { 3, 3 } },   // Summon Ghouls On Scarlet Crusade
        { 52438, { 1, 1 } },   // Summon Skittering Swarmer (Azjol Nerub,  Krik'thir the Gatewatcher)
        { 52449, { 1, 1 } },   // Summon Skittering Infector (Azjol Nerub,  Krik'thir the Gatewatcher)
        { 53457, { 1, 1 } },   // Impale (Azjol Nerub,  Anub'arak)
        { 54098, { 10, 10 } },   // Poison Bolt Volley (h) (Naxx, Faerlina)
        { 54148, { 1, 1 } },   // Ritual of the Sword (Utgarde Pinnacle, Svala)
        { 54522, { 3, 3 } },   // Summon Ghouls On Scarlet Crusade
        { 54835, { 10, 10 } },   // Curse of the Plaguebringer (h) (Naxx, Noth the Plaguebringer)
        { 55479, { 1, 1 } },   // Forced Obedience (Naxxramas, Razovius)
        { 55665, { 5, 5 } },   // Life Drain (h) (Naxx, Sapphiron)
        { 56140, { 1, 1 } },   // Summon Power Spark (Eye of Eternity, Malygos)
        { 57578, { 1, 1 } },   // Lava Strike (Obsidian Sanctum, Sartharion)
        { 58917, { 5, 5 } },   // Consume Minions
        { 59870, { 1, 1 } },   // Glare of the Tribunal (h) (Halls of Stone)
        { 60936, { 3, 3 } },   // Surge of Power (h) (Malygos)
        { 61693, { 3, 3 } },   // Arcane Storm (Malygos)
        { 61694, { 7, 7 } },   // Arcane Storm (h) (Malygos)
        { 61916, { 2, 3 } },   // Lightning Whirl (Ulduar, Stormcaller Brundir)
        { 62016, { 1, 1 } },   // Charge Orb (Ulduar, Thorim)
        { 62042, { 1, 1 } },   // Stormhammer (Ulduar, Thorim)
        { 62166, { 1, 1 } },   // Stone Grip (Ulduar, Kologarn)
        { 62301, { 1, 1 } },   // Cosmic Smash (Ulduar, Algalon)
        { 62374, { 1, 1 } },   // Pursued (Ulduar, Flame Leviathan)
        { 62476, { 2, 2 } },   // Icicle (Ulduar, Hodir)
        { 62477, { 3, 3 } },   // Icicle (h) (Ulduar, Hodir)
        { 62488, { 1, 1 } },   // Activate Construct (Ulduar, Ignis)
        { 62577, { 1, 1 } },   // Blizzard (Ulduar, Thorim)
        { 62603, { 1, 1 } },   // Blizzard (h) (Ulduar, Thorim)
        { 62797, { 1, 1 } },   // Storm Cloud (Ulduar, Hodir)
        { 62978, { 1, 1 } },   // Summon Guardian (Ulduar, Yogg Saron)
        { 63018, { 1, 1 } },   // Searing Light (Ulduar, XT-002)
        { 63024, { 1, 1 } },   // Gravity Bomb (Ulduar, XT-002)
        { 63482, { 3, 6 } },   // Lightning Whirl (h) (Ulduar, Stormcaller Brundir)
        { 63545, { 1, 1 } },   // Icicle (Ulduar, Hodir)
        { 63744, { 1, 1 } },   // Sara's Anger (Ulduar, Yogg-Saron)
        { 63745, { 1, 1 } },   // Sara's Blessing (Ulduar, Yogg-Saron)
        { 63747, { 1, 1 } },   // Sara's Fervor (Ulduar, Yogg-Saron)
        { 63795, { 1, 1 } },   // Psychosis (Ulduar, Yogg-Saron)
        { 63802, { 2, 2 } },   // Brain Link (Ulduar, Yogg-Saron)
        { 63820, { 1, 1 } },   // Summon Scrap Bot Trigger (Ulduar, Mimiron) use for Scrap Bots, hits npc 33856
        { 63830, { 1, 1 } },   // Malady of the Mind (Ulduar, Yogg-Saron)
        { 63981, { 3, 3 } },   // StoneGrip (h) (Ulduar, Kologarn)
        { 64218, { 1, 1 } },   // Overcharge (VoA, Emalon)
        { 64234, { 1, 1 } },   // Gravity Bomb (h) (Ulduar, XT-002)
        { 64402, { 1, 1 } },   // Rocket Strike (Ulduar, Mimiron)
        { 64425, { 1, 1 } },   // Summon Scrap Bot Trigger (Ulduar, Mimiron) use for Assault Bots, hits npc 33856
        { 64465, { 1, 1 } },   // Shadow Beacon (Ulduar, Yogg-Saron)
        { 64543, { 1, 1 } },   // Melt Ice (Ulduar, Hodir)
        { 64598, { 3, 3 } },   // Cosmic Smash (h) (Ulduar, Algalon)
        { 64604, { 5, 5 } },   // Nature Bomb (Ulduar, Freya)
        { 64620, { 3, 3 } },   // Summon Fire Bot Trigger (Ulduar, Mimiron) hits npc 33856
        { 64623, { 1, 1 } },   // Frost Bomb (Ulduar, Mimiron)
        { 65121, { 1, 1 } },   // Searing Light (h) (Ulduar, XT-002)
        { 65301, { 1, 1 } },   // Psychosis (Ulduar, Yogg-Saron)
        { 65872, { 1, 1 } },   // Pursuing Spikes (ToCrusader, Anub'arak)
        { 65950, { 1, 1 } },   // Touch of Light (ToCrusader, Val'kyr Twins)
        { 66001, { 1, 1 } },   // Touch of Darkness (ToCrusader, Val'kyr Twins)
        { 66013, { 2, 2 } },   // Penetrating Cold (10 man) (ToCrusader, Anub'arak)
        { 66152, { 1, 1 } },   // Bullet Controller Summon Periodic Trigger Light (ToCrusader)
        { 66153, { 1, 1 } },   // Bullet Controller Summon Periodic Trigger Dark (ToCrusader)
        { 66332, { 1, 1 } },   // Nerubian Burrower (Mode 0) (ToCrusader, Anub'arak)
        { 66336, { 1, 1 } },   // Mistress' Kiss (ToCrusader, Jaraxxus)
        { 66339, { 1, 1 } },   // Summon Scarab (ToCrusader, Anub'arak)
        { 67076, { 5, 5 } },   // Mistress' Kiss (Mode 1) (ToCrusader, Jaraxxus)
        { 67077, { 1, 1 } },   // Mistress' Kiss (Mode 2) (ToCrusader, Jaraxxus)
        { 67078, { 5, 5 } },   // Mistress' Kiss (Mode 3) (ToCrusader, Jaraxxus)
        { 67281, { 1, 1 } },   // Touch of Darkness (Mode 1)
        { 67282, { 1, 1 } },   // Touch of Darkness (Mode 2)
        { 67283, { 1, 1 } },   // Touch of Darkness (Mode 3)
        { 67296, { 1, 1 } },   // Touch of Light (Mode 1)
        { 67297, { 1, 1 } },   // Touch of Light (Mode 2)
        { 67298, { 1, 1 } },   // Touch of Light (Mode 3)
        { 67700, { 5, 5 } },   // Penetrating Cold (25 man)
        { 67755, { 2, 2 } },   // Nerubian Burrower (Mode 1) (ToCrusader, Anub'arak)
        { 67756, { 2, 2 } },   // Nerubian Burrower (Mode 2) (ToCrusader, Anub'arak)
        { 67757, { 4, 4 } },   // Nerubian Burrower (Mode 3) (ToCrusader, Anub'arak)
        { 68509, { 2, 2 } },   // Penetrating Cold (10 man heroic)
        { 68510, { 5, 5 } },   // Penetrating Cold (25 man, heroic)
        { 68912, { 1, 1 } },   // Wailing Souls (FoS)
        { 68950, { 1, 1 } },   // Fear (FoS)
        { 68987, { 1, 1 } },   // Pursuit (PoS)
        { 69048, { 1, 1 } },   // Mirrored Soul (FoS)
        { 69055, { 2, 2 } },   // Bone Slice (ICC, Lord Marrowgar)
        { 69057, { 1, 1 } },   // Bone Spike Graveyard (Icecrown Citadel, Lord Marrowgar) 10 man
        { 69140, { 1, 1 } },   // Coldflame (ICC, Marrowgar)
        { 69278, { 2, 2 } },   // Gas spore (ICC, Festergut)
        { 69674, { 1, 1 } },   // Mutated Infection (ICC, Rotface)
        { 70341, { 2, 2 } },   // Slime Puddle (ICC, Putricide)
        { 70450, { 1, 1 } },   // Blood Mirror
        { 70814, { 3, 3 } },   // Bone Slice (ICC, Lord Marrowgar, heroic)
        { 70826, { 3, 3 } },
        { 70837, { 1, 1 } },   // Blood Mirror
        { 70882, { 1, 1 } },   // Slime Spray Summon Trigger (ICC, Rotface)
        { 70920, { 1, 1 } },   // Unbound Plague Search Effect (ICC, Putricide)
        { 71221, { 4, 4 } },   // Gas spore (Mode 1) (ICC, Festergut)
        { 71224, { 1, 1 } },   // Mutated Infection (Mode 1)
        { 71336, { 2, 2 } },   // Pact of the Darkfallen
        { 71390, { 2, 2 } },   // Pact of the Darkfallen
        { 71445, { 1, 1 } },   // Twilight Bloodbolt
        { 71471, { 1, 1 } },   // Twilight Bloodbolt
        { 71837, { 1, 1 } },   // Vampiric Bite
        { 71861, { 1, 1 } },   // Swarming Shadows
        { 72088, { 1, 1 } },
        { 72089, { 3, 3 } },   // Bone Spike Graveyard (Icecrown Citadel, Lord Marrowgar) 25 man
        { 72091, { 1, 1 } },   // Frozen Orb (Vault of Archavon, Toravon)
        { 72095, { 3, 3 } },   // Frozen Orb (h) (Vault of Archavon, Toravon)
        { 72254, { 1, 1 } },   // Mark of Fallen Champion (target selection) (ICC, Deathbringer Saurfang)
        { 73022, { 1, 1 } },   // Mutated Infection (Mode 2)
        { 73023, { 1, 1 } },   // Mutated Infection (Mode 3)
        { 73142, { 1, 1 } },
        { 73143, { 3, 3 } },
        { 73144, { 1, 1 } },
        { 73145, { 3, 3 } },
    };

    /// The generic spells whose effects reach the instance visibility distance.
    constexpr uint32 INSTANCE_VISIBILITY_RADIUS[] =
    {
        69057,   // Bone Spike Graveyard (Icecrown Citadel, Lord Marrowgar encounter, 10N)
        70826,   // Bone Spike Graveyard (Icecrown Citadel, Lord Marrowgar encounter, 25N)
        72088,   // Bone Spike Graveyard (Icecrown Citadel, Lord Marrowgar encounter, 10H)
        72089,   // Bone Spike Graveyard (Icecrown Citadel, Lord Marrowgar encounter, 25H)
        72350,   // Fury of Frostmourne
        72351,   // Fury of Frostmourne
        72706,   // Achievement Check
        72830,   // Achievement Check
        73142,   // Bone Spike Graveyard (during Bone Storm) (Icecrown Citadel, Lord Marrowgar encounter, 10N)
        73143,   // Bone Spike Graveyard (during Bone Storm) (Icecrown Citadel, Lord Marrowgar encounter, 25N)
        73144,   // Bone Spike Graveyard (during Bone Storm) (Icecrown Citadel, Lord Marrowgar encounter, 10H)
        73145,   // Bone Spike Graveyard (during Bone Storm) (Icecrown Citadel, Lord Marrowgar encounter, 25H)
    };

    /// The druid spells whose effects take the contact distance.
    constexpr uint32 CONTACT_RADIUS[] =
    {
        49376,  // Feral Charge - Cat
    };

    /// The modifier auras that get exactly one charge.
    constexpr uint32 ONE_CHARGE_MODIFIER_AURAS[] =
    {
        17941,  // Shadow Trance
        22008,  // Netherwind Focus
        34936,  // Backlash
        48108,  // Hot Streak
        51124,  // Killing Machine
        54741,  // Firestarter
        57761,  // Fireball!
        64823,  // Elune's Wrath (Balance druid t8 set)
    };

    /// The max-health percent auras that set their target's health to the new maximum.
    constexpr uint32 HEALTH_TO_MAX_AURAS[] =
    {
        60430,  // Molten Fury
        64193,  // Heartbreak
        65737,  // Heartbreak
    };

    /// The auras whose holders start at the spell's maximum stack.
    constexpr uint32 MAX_STACK_AT_CREATE_AURAS[] =
    {
        24575,  // Brittle Armor
        24659,  // Unstable Power
        24662,  // Restless Strength
        26464,  // Mercurial Shield
        55166,  // Tidal Force
        62519,  // Attuned to Nature
        63050,  // Sanity
        64455,  // Feral Essence
        65294,  // Empowered
        71564,  // Deadly Precision
    };

    /// The overload spells: zero-cost triggered copies of the caster's spell that carry its cast time, which the
    /// triggering spell has already spent.
    constexpr uint32 OVERLOAD_SPELLS[] =
    {
        45284,  // Lightning Bolt overload
        45297,  // Chain Lightning overload
        77451,  // Lava Burst overload
    };

    /// The spells that may target a player on a taxi flight.
    constexpr uint32 TAXI_FLYER_TARGETS[] =
    {
        36573,  // Vision Guide
        42316,  // Alcaz Survey Credit
        42385,  // Alcaz Survey Aura
    };

    /// A spell id and the value it answers.
    struct SpellAnswer
    {
        uint32 spellId;
        uint32 answer;
    };

    constexpr SpellAnswer ARCANE_POTENCY[] =
    {
        { 31571, 57529 },
        { 31572, 57531 },
    };

    constexpr SpellAnswer SECOND_WIND[] =
    {
        { 29834, 29841 },
        { 29838, 29842 },
        { 42770, 42771 },
    };

    constexpr SpellAnswer RAPID_RECUPERATION[] =
    {
        { 53228, 56654 },
        { 53232, 58882 },
    };

    constexpr SpellAnswer FLAMETONGUE_WEAPON[] =
    {
        { 10400, 8026 },
    };

    constexpr SpellAnswer BLESSED_RECOVERY[] =
    {
        { 27811, 27813 },
        { 27815, 27817 },
        { 27816, 27818 },
    };

    constexpr SpellAnswer LIGHTNING_OVERLOAD[] =
    {
        { 403, 45284 },     // Lightning Bolt
        { 421, 45297 },     // Chain Lightning
    };

    constexpr SpellAnswer LIGHTNING_SHIELD_DAMAGE[] =
    {
        { 324, 26364 },
    };

    constexpr SpellAnswer THREAT_OF_THASSARIAN[] =
    {
        { 45462, 66216 },   // Plague Strike
        { 45902, 66215 },   // Blood Strike
        { 49020, 66198 },   // Obliterate
        { 49143, 66196 },   // Frost Strike
        { 49998, 66188 },   // Death Strike
        { 56815, 66217 },   // Rune Strike
    };

    constexpr SpellAnswer HOLY_SHOCK_OF_HEAL[] =
    {
        { 25914, 20473 },
    };

    constexpr SpellAnswer DEEP_WOUNDS_WEAPON_PERCENT[] =
    {
        { 12834, 16 },
        { 12849, 32 },
        { 12867, 48 },
    };

    constexpr uint32 BLAZING_SPEED_RANKS[] =
    {
        31641,
        31642,
    };

    template <std::size_t N>
    constexpr bool SortedBySpellId(SpellAnswer const (&rows)[N])
    {
        for (std::size_t i = 1; i < N; ++i)
        {
            if (!(rows[i - 1].spellId < rows[i].spellId))
            {
                return false;
            }
        }
        return true;
    }

    /// The answer of the row holding `spellId`, or 0 when no row holds it.
    template <std::size_t N>
    uint32 AnswerOf(SpellAnswer const (&rows)[N], uint32 spellId)
    {
        SpellAnswer const* end = std::end(rows);
        SpellAnswer const* row = std::lower_bound(std::begin(rows), end, spellId,
                                                  [](SpellAnswer const& r, uint32 id) { return r.spellId < id; });
        return row != end && row->spellId == spellId ? row->answer : 0;
    }

    /// Whether `ids`, sorted, holds `spellId`.
    template <std::size_t N>
    bool Holds(uint32 const (&ids)[N], uint32 spellId)
    {
        return std::binary_search(std::begin(ids), std::end(ids), spellId);
    }

    static_assert(SortedBySpellId(MAX_TARGETS), "max-targets rows are sorted by spell id, each once");
    static_assert(SortedBySpellId(INSTANCE_VISIBILITY_RADIUS), "instance visibility ids are sorted, each once");
    static_assert(SortedBySpellId(CONTACT_RADIUS), "contact radius ids are sorted, each once");
    static_assert(SortedBySpellId(ONE_CHARGE_MODIFIER_AURAS), "one-charge modifier ids are sorted, each once");
    static_assert(SortedBySpellId(HEALTH_TO_MAX_AURAS), "health-to-max ids are sorted, each once");
    static_assert(SortedBySpellId(MAX_STACK_AT_CREATE_AURAS), "max-stack ids are sorted, each once");
    static_assert(SortedBySpellId(OVERLOAD_SPELLS), "overload ids are sorted, each once");
    static_assert(SortedBySpellId(TAXI_FLYER_TARGETS), "taxi flyer ids are sorted, each once");
    static_assert(SortedBySpellId(ARCANE_POTENCY), "Arcane Potency rows are sorted, each once");
    static_assert(SortedBySpellId(SECOND_WIND), "Second Wind rows are sorted, each once");
    static_assert(SortedBySpellId(RAPID_RECUPERATION), "Rapid Recuperation rows are sorted, each once");
    static_assert(SortedBySpellId(FLAMETONGUE_WEAPON), "Flametongue Weapon rows are sorted, each once");
    static_assert(SortedBySpellId(BLESSED_RECOVERY), "Blessed Recovery rows are sorted, each once");
    static_assert(SortedBySpellId(LIGHTNING_OVERLOAD), "Lightning Overload rows are sorted, each once");
    static_assert(SortedBySpellId(LIGHTNING_SHIELD_DAMAGE), "Lightning Shield rows are sorted, each once");
    static_assert(SortedBySpellId(THREAT_OF_THASSARIAN), "Threat of Thassarian rows are sorted, each once");
    static_assert(SortedBySpellId(HOLY_SHOCK_OF_HEAL), "Holy Shock rows are sorted, each once");
    static_assert(SortedBySpellId(DEEP_WOUNDS_WEAPON_PERCENT), "Deep Wounds rows are sorted, each once");
    static_assert(SortedBySpellId(BLAZING_SPEED_RANKS), "Blazing Speed ranks are sorted, each once");
}

SpellMaxTargetsRange const* FindSpellMaxTargets(uint32 spellId)
{
    SpellMaxTargetsRow const* end = std::end(MAX_TARGETS);
    SpellMaxTargetsRow const* row = std::lower_bound(std::begin(MAX_TARGETS), end, spellId,
                                                     [](SpellMaxTargetsRow const& r, uint32 id) { return r.spellId < id; });
    return row != end && row->spellId == spellId ? &row->range : NULL;
}

bool HasInstanceVisibilityRadius(uint32 spellId)
{
    return Holds(INSTANCE_VISIBILITY_RADIUS, spellId);
}

bool HasContactRadius(uint32 spellId)
{
    return Holds(CONTACT_RADIUS, spellId);
}

bool IsOneChargeModifierAura(uint32 spellId)
{
    return Holds(ONE_CHARGE_MODIFIER_AURAS, spellId);
}

bool IsHealthToMaxAura(uint32 spellId)
{
    return Holds(HEALTH_TO_MAX_AURAS, spellId);
}

bool IsMaxStackAtCreateAura(uint32 spellId)
{
    return Holds(MAX_STACK_AT_CREATE_AURAS, spellId);
}

bool IsOverloadIgnoringCastTime(uint32 spellId)
{
    return Holds(OVERLOAD_SPELLS, spellId);
}

bool MayTargetTaxiFlyer(uint32 spellId)
{
    return Holds(TAXI_FLYER_TARGETS, spellId);
}

uint32 GetArcanePotencyProcSpell(uint32 rankSpellId)
{
    return AnswerOf(ARCANE_POTENCY, rankSpellId);
}

uint32 GetSecondWindProcSpell(uint32 rankSpellId)
{
    return AnswerOf(SECOND_WIND, rankSpellId);
}

uint32 GetRapidRecuperationProcSpell(uint32 rankSpellId)
{
    return AnswerOf(RAPID_RECUPERATION, rankSpellId);
}

uint32 GetFlametongueWeaponProcSpell(uint32 rankSpellId)
{
    return AnswerOf(FLAMETONGUE_WEAPON, rankSpellId);
}

uint32 GetBlessedRecoveryProcSpell(uint32 rankSpellId)
{
    return AnswerOf(BLESSED_RECOVERY, rankSpellId);
}

uint32 GetLightningOverloadSpell(uint32 procSpellId)
{
    return AnswerOf(LIGHTNING_OVERLOAD, procSpellId);
}

uint32 GetLightningShieldDamageSpell(uint32 rankSpellId)
{
    return AnswerOf(LIGHTNING_SHIELD_DAMAGE, rankSpellId);
}

uint32 GetThreatOfThassarianOffHandSpell(uint32 strikeSpellId)
{
    return AnswerOf(THREAT_OF_THASSARIAN, strikeSpellId);
}

uint32 GetHolyShockOfHeal(uint32 healSpellId)
{
    return AnswerOf(HOLY_SHOCK_OF_HEAL, healSpellId);
}

uint32 GetDeepWoundsWeaponDamagePercent(uint32 rankSpellId)
{
    return AnswerOf(DEEP_WOUNDS_WEAPON_PERCENT, rankSpellId);
}

bool IsBlazingSpeedRank(uint32 spellId)
{
    return Holds(BLAZING_SPEED_RANKS, spellId);
}
