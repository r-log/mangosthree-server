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

#ifndef MANGOS_HARNESS_TRACE_H
#define MANGOS_HARNESS_TRACE_H

#include "Platform/Define.h"

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

/**
 * The pure half of the harness's recorder (decoupling D4f0, design note
 * 2026-09-27-decoupling-d4f0-harness-quest-family.md §3; shared by the quest family and, since
 * decoupling D11, the spell family -- Recorder.h is the game half): what a packet is recorded as, the
 * decoders that read the stable fields out of the raw bytes, the MVTEST TRACE line and the
 * digest folded over those lines. No map, no player, no session: the recorder hands the bytes
 * in, and src/tests/HarnessTest.cpp pins every rule here against bytes laid out the way the
 * production writers lay them out.
 *
 * WHAT IS KEPT AND WHAT IS DROPPED. A packet goes into the trace as its stable content and
 * nothing else, so two runs of one build read alike and a change to what the server says
 * shows. Only two kinds of field are dropped: clocks (a date, a timer, a timestamp) and guids
 * a counter hands out (an item's, a spawned creature's). Everything else is kept, either
 * decoded field by field or folded into an FNV of the payload.
 */
namespace Harness
{
    namespace Trace
    {
        /// FNV-1a, 32 bits: the offset basis and the fold. The digest and every hashed payload
        /// use it, so one number is comparable with another printed the same way.
        const uint32 kFnvOffset = 2166136261u;
        uint32 Fnv1a(void const* data, size_t size, uint32 hash = kFnvOffset);
        /// Eight lower-case hex digits.
        std::string Hex32(uint32 value);

        /// How a packet is recorded, chosen by its opcode alone (the note's §3.3 table):
        ///   OpcodeOnly  the name and nothing else -- the update-object family, whose size moves
        ///               with the packed guid of an item the global counter numbered; what they
        ///               carry is pinned by the recorder's state delta instead.
        ///   Size        the name and the payload size -- every opcode whose writer has not been
        ///               read for clocks and counters.
        ///   Hash        the name, the size and the FNV of the payload -- writers read and found
        ///               free of both.
        ///   Decode      the stable fields, read out of the bytes by a decoder below.
        enum class Rule : uint8
        {
            OpcodeOnly,
            Size,
            Hash,
            Decode
        };
        Rule RuleFor(uint16 opcode);

        /// Who a guid is, for the decoders. The harness player's guid is the fixed one of the
        /// reserved block and a spawned creature's comes from the map's counter, so none is
        /// printed: a guid reads as its role -- "self", "giver", "target", "caster", "none" for 0
        /// -- or "other". The quest family names a giver; the spell family (decoupling D11) names
        /// the unit a cast is aimed at (`target`) and a unit other than the player that casts
        /// (`caster`). A role left at 0 never matches: 0 reads "none" before any role is asked.
        struct Roles
        {
            uint64 self = 0;
            uint64 giver = 0;
            uint64 target = 0;
            uint64 caster = 0;
        };
        char const* RoleOf(Roles const& roles, uint64 guid);

        // ---- the decoders -----------------------------------------------------------------
        //
        // Each reads a payload laid out as its production writer lays it out, and none of them
        // lets a byte go unaccounted for: the criteria, achievement and quest-giver decoders
        // require the bytes to end exactly where the layout does, so a writer that grows a field
        // shows up as a decode failure rather than as a silently shorter record; the spell, aura
        // and inventory-failure decoders stop at the end of their fixed head and record what
        // follows it as its length and FNV (`rest=`, `tail=`), since what follows varies with the
        // flags or the result. On success `out` holds the record's fields; on failure it is untouched and
        // the caller records the packet as undecoded, by its size alone.

        /// SMSG_CRITERIA_UPDATE (AchievementMgr::SendCriteriaUpdate): criteria id, the packed
        /// counter, the earner's packed guid, the failed flag; the date and both timers dropped.
        bool DecodeCriteriaUpdate(uint8 const* data, size_t size, Roles const& roles, std::string& out);
        /// SMSG_ACHIEVEMENT_EARNED (AchievementMgr::SendAchievementEarned): the earner's packed
        /// guid, the achievement id and the trailing word; the date dropped.
        bool DecodeAchievementEarned(uint8 const* data, size_t size, Roles const& roles, std::string& out);
        /// SMSG_SPELL_START and SMSG_SPELL_GO (Spell::SendSpellStart / SendSpellGo): the caster
        /// item-or-caster and caster roles, the spell id, the cast flags, START's cast time, GO's
        /// hit and miss lists as roles, the target mask with its unit or object target as a role,
        /// and -- when the mask names an item (TARGET_FLAG_ITEM or TARGET_FLAG_TRADE_ITEM) -- the
        /// item target as `itemTarget=item|none`: an item's guid comes from the global item
        /// counter, so it is read past and never hashed. Everything after that -- predicted power
        /// and runes, the missile and the source and destination bytes -- carries no clock and no
        /// counter and is recorded as `rest=<n>` and, when there is any, `restfnv=<hex>`. Dropped:
        /// the cast counter, m_timer and GO's timestamp, which sit in the fixed head.
        bool DecodeSpellCast(bool go, uint8 const* data, size_t size, Roles const& roles, std::string& out);
        /// SMSG_AURA_UPDATE (SpellAuraHolder::SendAuraUpdate, one aura per packet): the target as
        /// a role, the slot, the spell, the flags, the level and the stack, the caster as a role
        /// when the flags carry one (AFLAG_NOT_CASTER clear), then the durations and effect
        /// amounts as `rest=<n>` and `restfnv=<hex>`. The removal form -- the slot and a zero
        /// spell -- reads as the target, the slot and spell=0.
        bool DecodeAuraUpdate(uint8 const* data, size_t size, Roles const& roles, std::string& out);
        /// SMSG_QUESTGIVER_STATUS_MULTIPLE (Player::SendQuestGiverStatusMultiple): the count, then
        /// each giver as its role with its dialog status, in the packet's order.
        bool DecodeQuestGiverStatusMultiple(uint8 const* data, size_t size, Roles const& roles, std::string& out);

        // ---- the spell family's decoders (decoupling D11 PR 1): what scenario 930's cast can
        // send -- the damage log if the seeded roll hits, the miss log if it misses, the power
        // update of the mana it spends, and the three failure packets if the cast fails. Each
        // accounts for every byte: the first four require the payload to end where the writer's
        // layout ends; CAST_FAILED records its result-shaped tail as `tail=` and its FNV.

        /// SMSG_SPELLNONMELEEDAMAGELOG (Unit::SendSpellNonMeleeDamageLog, Unit.cpp:2617-2631):
        /// the target's and the attacker's packed guids as roles, the spell, the damage, the
        /// overkill, the school byte, the absorbed and resisted amounts, the physical-log and
        /// unused bytes, the blocked amount, the hit-info word and the extend flag. No clock, no
        /// counter: every field is kept.
        bool DecodeSpellDamageLog(uint8 const* data, size_t size, Roles const& roles, std::string& out);
        /// SMSG_SPELLLOGMISS (Unit::SendSpellMiss, Unit.cpp:2750-2758): the spell, the caster's
        /// raw guid as a role, the flag byte, the target count, then each target's raw guid as a
        /// role with its miss condition.
        bool DecodeSpellLogMiss(uint8 const* data, size_t size, Roles const& roles, std::string& out);
        /// SMSG_POWER_UPDATE (Unit::SetPower, UnitPower.cpp:154-160; the same layout in
        /// Unit::setPowerType, Unit.cpp:2967-2972): the unit's packed guid as a role, the count,
        /// then each (power type, value).
        bool DecodePowerUpdate(uint8 const* data, size_t size, Roles const& roles, std::string& out);
        /// SMSG_SPELL_FAILURE and SMSG_SPELL_FAILED_OTHER (Spell::SendInterrupted,
        /// SpellPackets.cpp:749-760, one layout for both): the caster's packed guid as a role,
        /// the cast count (dropped, as SPELL_START drops it), the spell and the result.
        bool DecodeSpellFailure(uint8 const* data, size_t size, Roles const& roles, std::string& out);
        /// SMSG_CAST_FAILED (Spell::SendCastResult, SpellPackets.cpp:121-221): the cast count
        /// (dropped), the spell, the result, then the result's own tail -- a spell focus, an area,
        /// totems, an item class -- as `tail=<n>` and its FNV.
        bool DecodeCastFailed(uint8 const* data, size_t size, std::string& out);

        /// SMSG_ITEM_PUSH_RESULT (Player::SendNewItem): the item entry and the count pushed, read
        /// out of the bytes: the player guid (8), received, created and shown (3 x 4), the bag (1)
        /// and the slot (4) come first, then the entry, the suffix factor, the random property and
        /// the count. False when the payload is not that writer's 45 bytes.
        bool ReadItemPush(uint8 const* data, size_t size, uint32& entry, uint32& count);
        /// SMSG_SPELLNONMELEEDAMAGELOG's target and attacker guids, spell, damage and overkill,
        /// read out of the bytes; false when the payload is not Unit::SendSpellNonMeleeDamageLog's
        /// layout (the one DecodeSpellDamageLog reads).
        bool ReadSpellDamage(uint8 const* data, size_t size, uint64& target, uint64& attacker, uint32& spell, uint32& damage,
                             uint32& overkill);
        /// SMSG_POWER_UPDATE's unit guid and its first (power type, value); false when the payload
        /// is not Unit::SetPower's layout or names no power.
        bool ReadPowerUpdate(uint8 const* data, size_t size, uint64& unit, uint8& power, uint32& value);

        /// The whole record of one packet, as it follows "pkt " in a TRACE line:
        /// "<name>", "<name> size=<n>", "<name> size=<n> fnv=<hex>", or "<name> <decoded fields>".
        /// A decoder that refuses the bytes gives "<name> undecoded size=<n>": no hash, because
        /// the bytes it could not place include the clocks the decoder exists to drop, and a hash
        /// of them would differ from run to run instead of failing the same way every time. An
        /// opcode whose table entry is STATUS_UNHANDLED never left the server -- SendPacket's
        /// socket branch refuses it -- and reads "<name> unhandled size=<n>".
        std::string PacketRecord(uint16 opcode, char const* name, uint8 const* data, size_t size, bool unhandled, Roles const& roles);

        /// A recorder's state at one moment (Recorder.h): keyed values, and named id sets in the
        /// order the family lists them.
        struct StateSnapshot
        {
            std::map<std::string, std::string> values;
            std::vector<std::pair<std::string, std::set<uint32> > > sets;
        };
        /// "+a +b -c": the ids `after` gained, then the ids it lost, each ascending; "" when equal.
        std::string SetDelta(std::set<uint32> const& before, std::set<uint32> const& after);
        /// The texts of the `state` lines a window's close prints, in order: every key of either
        /// side whose value differs, in key order, as "state <key> <old>-><new>" (a missing side
        /// reads "-"); then every set whose ids differ, in `after`'s order and then any only
        /// `before` has, as "state <name> <SetDelta>" (a missing side reads as empty).
        std::vector<std::string> StateDelta(StateSnapshot const& before, StateSnapshot const& after);
        /// The spell family's value of one aura holder: "eff=<mask> stack=<n> charges=<n>
        /// caster=<role> slot=<visible slot> dur=<remaining>/<maximum>" -- the durations in the
        /// stepped world's milliseconds, -1 for a permanent aura; the caster by its role.
        std::string AuraHolderValue(uint32 effectMask, uint32 stack, uint32 charges, char const* casterRole, uint32 slot,
                                    int32 duration, int32 maxDuration);

        /// Ruling 19's snapshot rule: a `snap` line is due after a packet when the snapshot `now`
        /// differs from `last`, the one printed last (or taken when the window opened), and `last`
        /// then becomes `now`. An unchanged state prints nothing.
        bool SnapDue(std::string& last, std::string const& now);

        /// "MVTEST TRACE <scenario> <seq> <window> <text>".
        std::string TraceLine(char const* scenario, uint32 seq, std::string const& window, std::string const& text);
        /// The digest fold of one TRACE line: FNV-1a over "<window> <text>\n", continuing from
        /// `digest`. The sequence number is left out, so a setup window's line count -- which is
        /// logged but never digested -- cannot move the digest of what follows it.
        uint32 DigestLine(uint32 digest, std::string const& window, std::string const& text);
    }
}

#endif
