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

/// Decoupling D4k: a death knight's runes, initialised, spent, converted, restored, regenerated and
/// sent with no character.
///
/// Before this PR RuneMgr held a pointer to its owner and read the owner's class, auras and haste
/// rating and wrote the owner's regeneration fields and session. Now the reads are parameters and
/// the writes are callbacks, so every case builds a RuneMgr from nothing, and a Wire records what
/// the callbacks received, in call order: each packet (opcode and bytes), each regeneration write
/// (rune type and the float's bits) and each convert-aura drop.
///
/// The convert auras are opaque identities: three suitably aligned blocks of a local array stand
/// for three auras. Aura is only forward-declared here, as in the manager, so nothing in this file
/// could dereference one -- which is what the manager promises.
///
/// A fresh RuneMgr is uninitialised, as it always was (the owner calls Init() at creation, at login
/// and on `.reset level` / `.reset stats`), so every case starts with Init() for a death knight.
///
/// The expected values are derived by hand in the comments; the float goldens are IEEE single
/// precision, each operation rounded to nearest. That is what every build computes: x86-64 does
/// float arithmetic in SSE2 by default, the 32-bit builds are forced there (/arch:SSE2,
/// -msse2 -mfpmath=sse, MangosPlatform.cmake), and on AArch64, where GCC may contract a*b+c into a
/// fused multiply-add, none of these expressions has that shape.

#include "TestHarness.h"
#include "RuneMgr.h"
#include "SharedDefines.h"
#include "WorldPacket.h"

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace
{
    // Three stand-in auras. The manager stores, compares and hands back these pointers only. Each
    // is the start of its own block aligned for any object type, so the converted pointer values
    // are well defined even though Aura's own alignment is not visible here.
    alignas(std::max_align_t) char s_auraStorage[3][sizeof(std::max_align_t)];
    Aura const* const kAuraA = reinterpret_cast<Aura const*>(&s_auraStorage[0][0]);
    Aura const* const kAuraB = reinterpret_cast<Aura const*>(&s_auraStorage[1][0]);
    Aura const* const kAuraC = reinterpret_cast<Aura const*>(&s_auraStorage[2][0]);

    std::string AuraName(Aura const* aura)
    {
        if (aura == kAuraA)
        {
            return "A";
        }
        if (aura == kAuraB)
        {
            return "B";
        }
        if (aura == kAuraC)
        {
            return "C";
        }
        return aura ? "?" : "NULL";
    }

    uint32 Bits(float value)
    {
        uint32 bits;
        std::memcpy(&bits, &value, sizeof(bits));
        return bits;
    }

    std::string PacketText(WorldPacket const& packet)
    {
        static const char* digits = "0123456789abcdef";
        std::string text;
        uint16 opcode = packet.GetOpcode();
        for (int shift = 12; shift >= 0; shift -= 4)
        {
            text += digits[(opcode >> shift) & 0x0F];
        }
        text += ":";
        text += testing::BytesToHex(packet.contents(), packet.size());
        return text;
    }

    /// What the callbacks received, in call order.
    struct Wire
    {
        std::vector<WorldPacket> packets;
        std::vector<std::string> events;                // "4f14:0103" per packet, "drop A" per drop
        std::vector<std::pair<uint32, uint32> > regen;  // (rune type, the value's bits)

        RuneMgr::PacketSink Sink()
        {
            return [this](WorldPacket const* packet)
            {
                packets.push_back(*packet);
                events.push_back(PacketText(*packet));
            };
        }

        RuneMgr::RegenSink Regen()
        {
            return [this](uint32 runeType, float value)
            {
                regen.push_back(std::make_pair(runeType, Bits(value)));
            };
        }

        RuneMgr::AuraDrop Drop()
        {
            return [this](Aura const* aura)
            {
                events.push_back("drop " + AuraName(aura));
            };
        }

        void Clear()
        {
            packets.clear();
            events.clear();
            regen.clear();
        }
    };

    /// A death knight's runes as Init() leaves them.
    void InitDeathKnight(RuneMgr& runes)
    {
        Wire ignored;
        runes.Init(CLASS_DEATH_KNIGHT, ignored.Regen());
    }

    RuneMgr::ConvertAuraFacts Facts(bool nonPassive, bool bloodOfTheNorthHeld, bool convertsRunes)
    {
        RuneMgr::ConvertAuraFacts facts;
        facts.nonPassive = nonPassive;
        facts.bloodOfTheNorthHeld = bloodOfTheNorthHeld;
        facts.convertsRunes = convertsRunes;
        return facts;
    }

    // IEEE single-precision bit patterns used below.
    const uint32 kBits0_1 = 0x3DCCCCCD;      // 0.1f = 1000 / 10000
    const uint32 kBits0_125 = 0x3E000000;    // 0.125f = 1000 / 8000
    const uint32 kBits0_25 = 0x3E800000;     // 0.25f = 1000 / 4000
    const uint32 kBits0_4 = 0x3ECCCCCD;      // 0.4f = 1000 / 2500
    const uint32 kBitsInf = 0x7F800000;      // +infinity = 1000 / 0
}

TEST(RuneMgr_InitBuildsTheSixDeathKnightRunes)
{
    RuneMgr runes;
    Wire wire;
    runes.Init(CLASS_DEATH_KNIGHT, wire.Regen());

    // runeSlotTypes: two blood, two unholy, two frost.
    const RuneType expected[MAX_RUNES] = { RUNE_BLOOD, RUNE_BLOOD, RUNE_UNHOLY, RUNE_UNHOLY, RUNE_FROST, RUNE_FROST };
    for (uint8 i = 0; i < MAX_RUNES; ++i)
    {
        CHECK_EQ(int(runes.GetBaseRune(i)), int(expected[i]));
        CHECK_EQ(int(runes.GetCurrentRune(i)), int(expected[i]));
        CHECK_EQ(int(runes.GetRuneCooldown(i)), 0);
        CHECK_EQ(int(runes.GetBaseRuneCooldown(i)), 0);
        CHECK(runes.GetRuneConvertAura(i) == NULL);
    }
    // Every slot usable: bits 0-5.
    CHECK_EQ(int(runes.GetRunesState()), 0x3F);

    // The four regeneration rates, in rune-type order, each 0.1f; no packet.
    REQUIRE(wire.regen.size() == NUM_RUNE_TYPES);
    for (uint32 i = 0; i < NUM_RUNE_TYPES; ++i)
    {
        CHECK_EQ(wire.regen[i].first, i);
        CHECK_EQ(wire.regen[i].second, kBits0_1);
    }
    CHECK(wire.events.empty());
}

TEST(RuneMgr_InitLeavesAnyOtherClassUntouched)
{
    RuneMgr runes;
    InitDeathKnight(runes);

    // A state Init() would reset.
    runes.SetBaseRuneCooldown(2, 10000);
    runes.SetRuneCooldown(2, 5000);
    runes.SetCurrentRune(1, RUNE_DEATH);
    runes.SetRuneConvertAura(1, kAuraA);

    const uint8 others[] = { CLASS_WARRIOR, CLASS_PALADIN, CLASS_HUNTER, CLASS_ROGUE, CLASS_PRIEST,
                             CLASS_SHAMAN, CLASS_MAGE, CLASS_WARLOCK, CLASS_DRUID, 0 };
    for (size_t c = 0; c < sizeof(others) / sizeof(others[0]); ++c)
    {
        Wire wire;
        runes.Init(others[c], wire.Regen());
        CHECK(wire.regen.empty());
        CHECK(wire.events.empty());
        CHECK_EQ(int(runes.GetRuneCooldown(2)), 5000);
        CHECK_EQ(int(runes.GetBaseRuneCooldown(2)), 10000);
        CHECK_EQ(int(runes.GetCurrentRune(1)), int(RUNE_DEATH));
        CHECK(runes.GetRuneConvertAura(1) == kAuraA);
        CHECK_EQ(int(runes.GetRunesState()), 0x3B);    // slot 2 on cooldown
    }

    // A death knight again: everything back to the Init() state.
    Wire wire;
    runes.Init(CLASS_DEATH_KNIGHT, wire.Regen());
    CHECK_EQ(int(runes.GetRuneCooldown(2)), 0);
    CHECK_EQ(int(runes.GetBaseRuneCooldown(2)), 0);
    CHECK_EQ(int(runes.GetCurrentRune(1)), int(RUNE_BLOOD));
    CHECK(runes.GetRuneConvertAura(1) == NULL);
    CHECK_EQ(int(runes.GetRunesState()), 0x3F);
    CHECK_EQ(int(wire.regen.size()), int(NUM_RUNE_TYPES));
}

TEST(RuneMgr_TheStateMaskFollowsSetRuneCooldown)
{
    RuneMgr runes;
    InitDeathKnight(runes);

    // A non-zero cooldown clears the slot's bit, zero sets it again.
    runes.SetRuneCooldown(2, 5000);
    CHECK_EQ(int(runes.GetRunesState()), 0x3B);        // 0x3F & ~0x04
    runes.SetRuneCooldown(5, 1);
    CHECK_EQ(int(runes.GetRunesState()), 0x1B);        // & ~0x20
    runes.SetRuneCooldown(2, 0);
    CHECK_EQ(int(runes.GetRunesState()), 0x1F);        // | 0x04
    runes.SetRuneCooldown(5, 0);
    CHECK_EQ(int(runes.GetRunesState()), 0x3F);
    CHECK_EQ(int(runes.GetRuneCooldown(2)), 0);

    // The base cooldown and the rune types do not touch the mask.
    runes.SetBaseRuneCooldown(0, 10000);
    runes.SetCurrentRune(0, RUNE_DEATH);
    runes.SetBaseRune(0, RUNE_FROST);
    CHECK_EQ(int(runes.GetRunesState()), 0x3F);
    CHECK_EQ(int(runes.GetBaseRune(0)), int(RUNE_FROST));

    // The last-used mask is indexed by rune TYPE on the way in, by bit on the way out.
    runes.ClearLastUsedRuneMask();
    for (uint8 i = 0; i < 8; ++i)
    {
        CHECK(!runes.IsLastUsedRune(i));
    }
    runes.SetLastUsedRune(RUNE_UNHOLY);
    runes.SetLastUsedRune(RUNE_DEATH);
    CHECK(!runes.IsLastUsedRune(0));
    CHECK(runes.IsLastUsedRune(1));
    CHECK(!runes.IsLastUsedRune(2));
    CHECK(runes.IsLastUsedRune(3));
    runes.ClearLastUsedRuneMask();
    CHECK(!runes.IsLastUsedRune(1));
    CHECK(!runes.IsLastUsedRune(3));
}

TEST(RuneMgr_CooldownFractionEdges)
{
    RuneMgr runes;
    InitDeathKnight(runes);

    // No cooldown: 255, whatever the base.
    runes.SetBaseRuneCooldown(0, 10000);
    runes.SetRuneCooldown(0, 0);
    CHECK_EQ(int(runes.GetRuneCooldownFraction(0)), 255);

    // Zero base: 255, whatever the cooldown.
    runes.SetBaseRuneCooldown(0, 0);
    runes.SetRuneCooldown(0, 1000);
    CHECK_EQ(int(runes.GetRuneCooldownFraction(0)), 255);

    // Cooldown equal to the base (just spent): 0.
    runes.SetBaseRuneCooldown(0, 10000);
    runes.SetRuneCooldown(0, 10000);
    CHECK_EQ(int(runes.GetRuneCooldownFraction(0)), 0);

    // Partial: uint8(float(base - cd) / base * 255), truncated.
    struct Row
    {
        uint16 base;
        uint16 cd;
        int expected;
    };
    const Row rows[] =
    {
        { 10000, 2500, 191 },   // 7500/10000 = 0.75 exactly; * 255 = 191.25 -> 191
        { 10000, 5000, 127 },   // 0.5; 127.5 -> 127
        { 10000, 7500, 63 },    // 0.25; 63.75 -> 63
        { 1500, 500, 170 },     // 1000/1500 = 0.6666667f; * 255 = 170.0000051 rounds to 170.0f -> 170
        { 1500, 1000, 85 },     // 500/1500 = 0.33333334f; * 255 = 85.0000025 rounds to 85.0f -> 85
        { 10000, 1, 254 },      // 0.9999f; 254.9745 -> 254
        { 10000, 9999, 0 },     // 0.0001f; 0.0255 -> 0: a rune 1 ms into its cooldown reads as just spent
    };
    for (size_t r = 0; r < sizeof(rows) / sizeof(rows[0]); ++r)
    {
        runes.SetBaseRuneCooldown(3, rows[r].base);
        runes.SetRuneCooldown(3, rows[r].cd);
        CHECK_EQ(int(runes.GetRuneCooldownFraction(3)), rows[r].expected);
    }
}

TEST(RuneMgr_ActivateRunesPicksInSlotOrderUpToTheCount)
{
    RuneMgr runes;
    InitDeathKnight(runes);
    for (uint8 i = 0; i < MAX_RUNES; ++i)
    {
        runes.SetRuneCooldown(i, 5000);
    }
    CHECK_EQ(int(runes.GetRunesState()), 0x00);

    // One blood rune: the first blood slot on cooldown, slot 0.
    CHECK(runes.ActivateRunes(RUNE_BLOOD, 1));
    CHECK_EQ(int(runes.GetRuneCooldown(0)), 0);
    CHECK_EQ(int(runes.GetRuneCooldown(1)), 5000);
    CHECK_EQ(int(runes.GetRunesState()), 0x01);

    // Five asked, one blood rune left on cooldown: slot 1 only, and still "modified".
    CHECK(runes.ActivateRunes(RUNE_BLOOD, 5));
    CHECK_EQ(int(runes.GetRuneCooldown(1)), 0);
    CHECK_EQ(int(runes.GetRunesState()), 0x03);

    // None left on cooldown: nothing to do.
    CHECK(!runes.ActivateRunes(RUNE_BLOOD, 1));

    // A count of zero does nothing.
    CHECK(!runes.ActivateRunes(RUNE_UNHOLY, 0));
    CHECK_EQ(int(runes.GetRunesState()), 0x03);

    // A ready rune of the type is skipped and does not use up the count: slot 2 ready, slot 3 not.
    runes.SetRuneCooldown(2, 0);
    CHECK(runes.ActivateRunes(RUNE_UNHOLY, 1));
    CHECK_EQ(int(runes.GetRuneCooldown(3)), 0);
    CHECK_EQ(int(runes.GetRunesState()), 0x0F);

    // The CURRENT type decides: slot 4 is a death rune now, so frost reaches slot 5 only.
    runes.SetCurrentRune(4, RUNE_DEATH);
    CHECK(runes.ActivateRunes(RUNE_FROST, 2));
    CHECK_EQ(int(runes.GetRuneCooldown(4)), 5000);
    CHECK_EQ(int(runes.GetRuneCooldown(5)), 0);
    CHECK_EQ(int(runes.GetRunesState()), 0x2F);
    CHECK(runes.ActivateRunes(RUNE_DEATH, 2));
    CHECK_EQ(int(runes.GetRuneCooldown(4)), 0);
    CHECK_EQ(int(runes.GetRunesState()), 0x3F);
}

TEST(RuneMgr_BaseRuneSlotsOnCooldown)
{
    RuneMgr runes;
    InitDeathKnight(runes);

    CHECK(!runes.IsBaseRuneSlotsOnCooldown(RUNE_BLOOD));
    CHECK(!runes.IsBaseRuneSlotsOnCooldown(RUNE_UNHOLY));
    CHECK(!runes.IsBaseRuneSlotsOnCooldown(RUNE_FROST));
    // No slot has death as its BASE type: vacuously all on cooldown.
    CHECK(runes.IsBaseRuneSlotsOnCooldown(RUNE_DEATH));

    runes.SetRuneCooldown(0, 3000);
    CHECK(!runes.IsBaseRuneSlotsOnCooldown(RUNE_BLOOD));    // slot 1 still ready
    runes.SetRuneCooldown(1, 3000);
    CHECK(runes.IsBaseRuneSlotsOnCooldown(RUNE_BLOOD));
    CHECK(!runes.IsBaseRuneSlotsOnCooldown(RUNE_UNHOLY));

    // The base type decides, not the current one.
    runes.SetCurrentRune(1, RUNE_DEATH);
    CHECK(runes.IsBaseRuneSlotsOnCooldown(RUNE_BLOOD));
    runes.SetRuneCooldown(1, 0);
    CHECK(!runes.IsBaseRuneSlotsOnCooldown(RUNE_BLOOD));
}

TEST(RuneMgr_ConvertRuneSetsTheCurrentTypeAndSendsItsPacket)
{
    RuneMgr runes;
    InitDeathKnight(runes);
    Wire wire;

    runes.ConvertRune(1, RUNE_DEATH, wire.Sink());
    CHECK_EQ(int(runes.GetCurrentRune(1)), int(RUNE_DEATH));
    CHECK_EQ(int(runes.GetBaseRune(1)), int(RUNE_BLOOD));
    CHECK(runes.GetRuneConvertAura(1) == NULL);             // ConvertRune alone stores no aura

    runes.ConvertRune(4, RUNE_FROST, wire.Sink());          // already frost: still a packet
    REQUIRE(wire.packets.size() == 2);
    // uint8 index, uint8 new type.
    CHECK(wire.packets[0].GetOpcode() == SMSG_CONVERT_RUNE);
    CHECK_HEX(wire.packets[0].contents(), wire.packets[0].size(), "0103");
    CHECK(wire.packets[1].GetOpcode() == SMSG_CONVERT_RUNE);
    CHECK_HEX(wire.packets[1].contents(), wire.packets[1].size(), "0402");
}

TEST(RuneMgr_RemoveRunesByAuraEffectRestoresEverySlotOfThatAura)
{
    RuneMgr runes;
    InitDeathKnight(runes);
    Wire wire;

    // A converted slots 1 and 4, B converted slot 2.
    runes.SetRuneConvertAura(1, kAuraA);
    runes.ConvertRune(1, RUNE_DEATH, wire.Sink());
    runes.SetRuneConvertAura(4, kAuraA);
    runes.ConvertRune(4, RUNE_DEATH, wire.Sink());
    runes.SetRuneConvertAura(2, kAuraB);
    runes.ConvertRune(2, RUNE_DEATH, wire.Sink());
    wire.Clear();

    // A: slot 1 back to blood, then slot 4 back to frost, in slot order; both forget A.
    runes.RemoveRunesByAuraEffect(kAuraA, wire.Sink());
    REQUIRE(wire.events.size() == 2);
    CHECK_STR(wire.events[0], "4f14:0100");
    CHECK_STR(wire.events[1], "4f14:0402");
    CHECK_EQ(int(runes.GetCurrentRune(1)), int(RUNE_BLOOD));
    CHECK_EQ(int(runes.GetCurrentRune(4)), int(RUNE_FROST));
    CHECK(runes.GetRuneConvertAura(1) == NULL);
    CHECK(runes.GetRuneConvertAura(4) == NULL);
    CHECK_EQ(int(runes.GetCurrentRune(2)), int(RUNE_DEATH));
    CHECK(runes.GetRuneConvertAura(2) == kAuraB);

    // An aura no slot holds: nothing.
    wire.Clear();
    runes.RemoveRunesByAuraEffect(kAuraC, wire.Sink());
    CHECK(wire.events.empty());

    // KEPT: NULL matches every slot WITHOUT a convert aura -- 0, 1, 3, 4, 5 -- and restores each
    // to its base type with a packet; slot 2 keeps B.
    runes.RemoveRunesByAuraEffect(NULL, wire.Sink());
    REQUIRE(wire.events.size() == 5);
    CHECK_STR(wire.events[0], "4f14:0000");
    CHECK_STR(wire.events[1], "4f14:0100");
    CHECK_STR(wire.events[2], "4f14:0301");
    CHECK_STR(wire.events[3], "4f14:0402");
    CHECK_STR(wire.events[4], "4f14:0502");
    CHECK(runes.GetRuneConvertAura(2) == kAuraB);
}

TEST(RuneMgr_RestoreBaseRuneFactTable)
{
    // Every combination: the slot's aura (NULL or A) x the three facts x `other` (another slot, 5,
    // still holds A). Derivation, in the body's order:
    //   aura && nonPassive          -> keep (return before anything);
    //   aura && bloodOfTheNorthHeld -> keep;
    //   otherwise restore: packet (index, base), aura cleared;
    //   then !aura || !convertsRunes -> stop; another slot holds it -> stop; else drop the aura.
    struct Row
    {
        bool withAura;
        bool nonPassive;
        bool bloodOfTheNorth;
        bool convertsRunes;
        bool other;
        bool restored;
        bool dropped;
    };
    const Row rows[] =
    {
        // NULL aura: the facts are never consulted (the owner passes false for NULL; true here
        // proves the manager's own `aura &&` guards): always restored, never dropped.
        { false, false, false, false, false, true,  false },
        { false, false, false, false, true,  true,  false },
        { false, false, false, true,  false, true,  false },
        { false, false, false, true,  true,  true,  false },
        { false, false, true,  false, false, true,  false },
        { false, false, true,  false, true,  true,  false },
        { false, false, true,  true,  false, true,  false },
        { false, false, true,  true,  true,  true,  false },
        { false, true,  false, false, false, true,  false },
        { false, true,  false, false, true,  true,  false },
        { false, true,  false, true,  false, true,  false },
        { false, true,  false, true,  true,  true,  false },
        { false, true,  true,  false, false, true,  false },
        { false, true,  true,  false, true,  true,  false },
        { false, true,  true,  true,  false, true,  false },
        { false, true,  true,  true,  true,  true,  false },
        // non-passive: kept, whatever else holds.
        { true,  true,  false, false, false, false, false },
        { true,  true,  false, false, true,  false, false },
        { true,  true,  false, true,  false, false, false },
        { true,  true,  false, true,  true,  false, false },
        { true,  true,  true,  false, false, false, false },
        { true,  true,  true,  false, true,  false, false },
        { true,  true,  true,  true,  false, false, false },
        { true,  true,  true,  true,  true,  false, false },
        // passive, Blood of the North held: kept.
        { true,  false, true,  false, false, false, false },
        { true,  false, true,  false, true,  false, false },
        { true,  false, true,  true,  false, false, false },
        { true,  false, true,  true,  true,  false, false },
        // passive, not Blood of the North, not a convert-rune aura: restored, aura stays on its target.
        { true,  false, false, false, false, true,  false },
        { true,  false, false, false, true,  true,  false },
        // a convert-rune aura another slot still holds: restored, not dropped.
        { true,  false, false, true,  true,  true,  false },
        // a convert-rune aura no slot holds any more: restored, then dropped.
        { true,  false, false, true,  false, true,  true  },
    };
    CHECK_EQ(int(sizeof(rows) / sizeof(rows[0])), 32);

    for (size_t r = 0; r < sizeof(rows) / sizeof(rows[0]); ++r)
    {
        Row const& row = rows[r];
        RuneMgr runes;
        InitDeathKnight(runes);
        Wire wire;

        // Slot 2 (base unholy) is a death rune, converted by A when the row has an aura.
        runes.ConvertRune(2, RUNE_DEATH, wire.Sink());
        if (row.withAura)
        {
            runes.SetRuneConvertAura(2, kAuraA);
        }
        if (row.other)
        {
            runes.SetRuneConvertAura(5, kAuraA);
            runes.ConvertRune(5, RUNE_DEATH, wire.Sink());
        }
        wire.Clear();

        runes.RestoreBaseRune(2, Facts(row.nonPassive, row.bloodOfTheNorth, row.convertsRunes), wire.Sink(), wire.Drop());

        std::vector<std::string> expected;
        if (row.restored)
        {
            expected.push_back("4f14:0201");                 // index 2, base unholy
        }
        if (row.dropped)
        {
            expected.push_back("drop A");                    // after the packet
        }
        CHECK_EQ(int(wire.events.size()), int(expected.size()));
        for (size_t e = 0; e < wire.events.size() && e < expected.size(); ++e)
        {
            CHECK_STR(wire.events[e], expected[e]);
        }

        CHECK_EQ(int(runes.GetCurrentRune(2)), int(row.restored ? RUNE_UNHOLY : RUNE_DEATH));
        CHECK(runes.GetRuneConvertAura(2) == ((row.withAura && !row.restored) ? kAuraA : NULL));
        if (row.other)
        {
            CHECK(runes.GetRuneConvertAura(5) == kAuraA);    // the other slot is untouched
            CHECK_EQ(int(runes.GetCurrentRune(5)), int(RUNE_DEATH));
        }
        if (wire.events.size() != expected.size())
        {
            std::printf("  (RestoreBaseRune row %u)\n", unsigned(r));
        }
    }
}

TEST(RuneMgr_UpdateRuneRegenChoosesTheSlotAndPinsTheFloat)
{
    RuneMgr runes;
    InitDeathKnight(runes);
    Wire wire;

    // The unholy pair (slots 2 and 3) with distinct base cooldowns, so the chosen slot shows:
    // 1000 / 4000 = 0.25f from slot 2, 1000 / 8000 = 0.125f from slot 3 (auraMod 1, haste 0:
    // hastePct = (100 - 0) / 100 = 1, cooldown *= 1 / 1).
    runes.SetBaseRuneCooldown(2, 4000);
    runes.SetBaseRuneCooldown(3, 8000);

    struct Row
    {
        uint16 cd2;
        uint16 cd3;
        uint32 regenIndex;
        uint32 bits;
    };
    const Row rows[] =
    {
        { 0,    0,    RUNE_UNHOLY, kBits0_1 },      // none on cooldown: RUNE_BASE_COOLDOWN 10000, slot 2's type
        { 1000, 3000, RUNE_UNHOLY, kBits0_25 },     // both, first shorter: the first slot (else branch)
        { 3000, 1000, RUNE_UNHOLY, kBits0_125 },    // both, second shorter: the second slot
        { 2000, 2000, RUNE_UNHOLY, kBits0_25 },     // both, equal: the first slot
        { 0,    2000, RUNE_UNHOLY, kBits0_125 },    // second only: the second slot
        { 2000, 0,    RUNE_UNHOLY, kBits0_25 },     // first only: the first slot
    };
    for (size_t r = 0; r < sizeof(rows) / sizeof(rows[0]); ++r)
    {
        runes.SetRuneCooldown(2, rows[r].cd2);
        runes.SetRuneCooldown(3, rows[r].cd3);
        wire.Clear();
        runes.UpdateRuneRegen(RUNE_UNHOLY, 1.0f, 0.0f, wire.Regen());
        REQUIRE(wire.regen.size() == 1);
        CHECK_EQ(wire.regen[0].first, rows[r].regenIndex);
        CHECK_EQ(wire.regen[0].second, rows[r].bits);
    }

    // The field written is the chosen slot's CURRENT type, in each of the three branches. Slot 2
    // converted to death, slot 3 still unholy: the two branches that pick the first slot write the
    // death field (3), the one that picks the second writes unholy's (1).
    runes.SetCurrentRune(2, RUNE_DEATH);
    const Row converted[] =
    {
        { 0,    0,    RUNE_DEATH,  kBits0_1 },      // none on cooldown: slot 2's type, 1000 / 10000
        { 1000, 3000, RUNE_DEATH,  kBits0_25 },     // both, first shorter: slot 2, 1000 / 4000
        { 3000, 1000, RUNE_UNHOLY, kBits0_125 },    // both, second shorter: slot 3, 1000 / 8000
    };
    for (size_t r = 0; r < sizeof(converted) / sizeof(converted[0]); ++r)
    {
        runes.SetRuneCooldown(2, converted[r].cd2);
        runes.SetRuneCooldown(3, converted[r].cd3);
        wire.Clear();
        runes.UpdateRuneRegen(RUNE_UNHOLY, 1.0f, 0.0f, wire.Regen());
        REQUIRE(wire.regen.size() == 1);
        CHECK_EQ(wire.regen[0].first, converted[r].regenIndex);
        CHECK_EQ(wire.regen[0].second, converted[r].bits);
    }

    // Slot 3 converted instead: the second-slot branch writes the death field.
    runes.SetCurrentRune(2, RUNE_UNHOLY);
    runes.SetCurrentRune(3, RUNE_DEATH);
    runes.SetRuneCooldown(2, 3000);
    runes.SetRuneCooldown(3, 1000);
    wire.Clear();
    runes.UpdateRuneRegen(RUNE_UNHOLY, 1.0f, 0.0f, wire.Regen());
    REQUIRE(wire.regen.size() == 1);
    CHECK_EQ(wire.regen[0].first, uint32(RUNE_DEATH));
    CHECK_EQ(wire.regen[0].second, kBits0_125);

    // The death rune has no pair of its own: nothing is written, whatever the inputs.
    wire.Clear();
    runes.UpdateRuneRegen(RUNE_DEATH, 1.0f, 0.0f, wire.Regen());
    runes.UpdateRuneRegen(RUNE_DEATH, 2.53f, 17.3f, wire.Regen());
    CHECK(wire.regen.empty());

    // The loop steps over pairs: frost finds slots 4 and 5 (both ready: 10000, frost's field).
    wire.Clear();
    runes.UpdateRuneRegen(RUNE_FROST, 1.0f, 0.0f, wire.Regen());
    REQUIRE(wire.regen.size() == 1);
    CHECK_EQ(wire.regen[0].first, uint32(RUNE_FROST));
    CHECK_EQ(wire.regen[0].second, kBits0_1);

    // The arithmetic, on the blood pair (both ready: cooldown 10000).
    struct Math
    {
        float auraMod;
        float hasteRating;
        uint32 bits;
    };
    const Math math[] =
    {
        // hastePct = (100 - 50) / 100 = 0.5; 0.5 / 2 = 0.25; 10000 * 0.25 = 2500; 1000 / 2500 = 0.4f.
        { 2.0f, 50.0f, kBits0_4 },
        // hastePct = (100 - 150) / 100 = -0.5 < 0 -> 1.0; 10000 * (1 / 1) = 10000 -> 0.1f.
        { 1.0f, 150.0f, kBits0_1 },
        // KEPT: hastePct = 0 is not < 0; cooldown 10000 * 0 = 0; 1000 / 0 = +inf.
        { 1.0f, 100.0f, kBitsInf },
        // auraMod as the owner builds it for amounts 15, 10, 100 (a regeneration aura, Unholy
        // Presence, Runic Corruption): 1 * (115/100) * (110/100) * (200/100) = 2.53f (0x4021EB85);
        // hasteRating 17.3f (0x418A6666): hastePct = 82.7f / 100 = 0.82699996f (0x3F53B645);
        // 0.82699996f / 2.53f = 0.32687744f (0x3EA75C7B); * 10000 = 3268.7744f (0x454C4C64);
        // 1000 / 3268.7744f = 0.30592507f (0x3E9CA236). Every step single precision, round to nearest.
        { 1.0f * ((100.0f + 15) / 100.0f) * ((100.0f + 10) / 100.0f) * ((100.0f + 100) / 100.0f), 17.3f, 0x3E9CA236 },
    };
    CHECK_EQ(Bits(math[3].auraMod), 0x4021EB85u);
    for (size_t m = 0; m < sizeof(math) / sizeof(math[0]); ++m)
    {
        wire.Clear();
        runes.UpdateRuneRegen(RUNE_BLOOD, math[m].auraMod, math[m].hasteRating, wire.Regen());
        REQUIRE(wire.regen.size() == 1);
        CHECK_EQ(wire.regen[0].first, uint32(RUNE_BLOOD));
        CHECK_EQ(wire.regen[0].second, math[m].bits);
    }

    // UpdateRuneRegen reads the state and changes none of it.
    CHECK_EQ(int(runes.GetRuneCooldown(2)), 3000);
    CHECK_EQ(int(runes.GetCurrentRune(3)), int(RUNE_DEATH));
    CHECK(wire.events.empty());
}

TEST(RuneMgr_ResyncAndAddRunePowerBytes)
{
    RuneMgr runes;
    InitDeathKnight(runes);
    Wire wire;

    // slot 0: base 10000, cd 2500 -> 191 (0xbf); slot 1: ready -> 255;
    // slot 2: cd == base -> 0; slot 3: death rune, base 10000, cd 5000 -> 127 (0x7f);
    // slot 4: zero base, cd 1000 -> 255; slot 5: ready -> 255.
    runes.SetBaseRuneCooldown(0, 10000);
    runes.SetRuneCooldown(0, 2500);
    runes.SetBaseRuneCooldown(2, 10000);
    runes.SetRuneCooldown(2, 10000);
    runes.SetCurrentRune(3, RUNE_DEATH);
    runes.SetBaseRuneCooldown(3, 10000);
    runes.SetRuneCooldown(3, 5000);
    runes.SetRuneCooldown(4, 1000);

    runes.ResyncRunes(wire.Sink());
    REQUIRE(wire.packets.size() == 1);
    CHECK(wire.packets[0].GetOpcode() == SMSG_RESYNC_RUNES);
    // uint32 MAX_RUNES, then (uint8 current type, uint8 cooldown fraction) per slot.
    CHECK_HEX(wire.packets[0].contents(), wire.packets[0].size(), "06000000" "00bf" "00ff" "0100" "037f" "02ff" "02ff");

    // uint32 mask 1 << index.
    runes.AddRunePower(0, wire.Sink());
    runes.AddRunePower(3, wire.Sink());
    runes.AddRunePower(5, wire.Sink());
    REQUIRE(wire.packets.size() == 4);
    CHECK(wire.packets[1].GetOpcode() == SMSG_ADD_RUNE_POWER);
    CHECK_HEX(wire.packets[1].contents(), wire.packets[1].size(), "01000000");
    CHECK(wire.packets[2].GetOpcode() == SMSG_ADD_RUNE_POWER);
    CHECK_HEX(wire.packets[2].contents(), wire.packets[2].size(), "08000000");
    CHECK(wire.packets[3].GetOpcode() == SMSG_ADD_RUNE_POWER);
    CHECK_HEX(wire.packets[3].contents(), wire.packets[3].size(), "20000000");

    // Neither packet changes the state.
    CHECK_EQ(int(runes.GetRuneCooldown(0)), 2500);
    CHECK_EQ(int(runes.GetRunesState()), 0x22);    // slots 1 and 5 ready
}
