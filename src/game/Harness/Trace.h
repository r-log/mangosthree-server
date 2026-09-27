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

#include <string>

/**
 * The pure half of the harness's reward recorder (decoupling D4f0, design note
 * 2026-09-27-decoupling-d4f0-harness-quest-family.md §3): what a packet is recorded as, the
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
        /// reserved block and the giver's comes from the map's counter, so neither is printed:
        /// a guid reads as its role -- "self", "giver", "none" for 0 -- or "other".
        struct Roles
        {
            uint64 self = 0;
            uint64 giver = 0;
        };
        char const* RoleOf(Roles const& roles, uint64 guid);

        // ---- the decoders -----------------------------------------------------------------
        //
        // Each reads a payload laid out as its production writer lays it out, and none of them
        // lets a byte go unaccounted for: the criteria, achievement and quest-giver decoders
        // require the bytes to end exactly where the layout does, so a writer that grows a field
        // shows up as a decode failure rather than as a silently shorter record; the spell and
        // inventory-failure decoders stop at the end of their fixed head and record what follows
        // it as its length and FNV (`rest=`, `tail=`), since what follows varies with the flags or
        // the result. On success `out` holds the record's fields; on failure it is untouched and
        // the caller records the packet as undecoded, by its size alone.

        /// SMSG_CRITERIA_UPDATE (AchievementMgr::SendCriteriaUpdate): criteria id, the packed
        /// counter, the earner's packed guid, the failed flag; the date and both timers dropped.
        bool DecodeCriteriaUpdate(uint8 const* data, size_t size, Roles const& roles, std::string& out);
        /// SMSG_ACHIEVEMENT_EARNED (AchievementMgr::SendAchievementEarned): the earner's packed
        /// guid, the achievement id and the trailing word; the date dropped.
        bool DecodeAchievementEarned(uint8 const* data, size_t size, Roles const& roles, std::string& out);
        /// SMSG_SPELL_START and SMSG_SPELL_GO (Spell::SendSpellStart / SendSpellGo): the caster
        /// item-or-caster and caster roles, the spell id, the cast flags, START's cast time, GO's
        /// hit and miss lists as roles, and the target mask with its unit or object target as a
        /// role. Everything after that first target -- predicted power and runes, the missile and
        /// destination bytes, item targets -- is deterministic and recorded as `rest=<n>` and,
        /// when there is any, `restfnv=<hex>`. Dropped: the cast counter, m_timer and GO's
        /// timestamp, which sit in the fixed head.
        bool DecodeSpellCast(bool go, uint8 const* data, size_t size, Roles const& roles, std::string& out);
        /// SMSG_QUESTGIVER_STATUS_MULTIPLE (Player::SendQuestGiverStatusMultiple): the count, then
        /// each giver as its role with its dialog status, in the packet's order.
        bool DecodeQuestGiverStatusMultiple(uint8 const* data, size_t size, Roles const& roles, std::string& out);

        /// The whole record of one packet, as it follows "pkt " in a TRACE line:
        /// "<name>", "<name> size=<n>", "<name> size=<n> fnv=<hex>", or "<name> <decoded fields>".
        /// A decoder that refuses the bytes gives "<name> undecoded size=<n>": no hash, because
        /// the bytes it could not place include the clocks the decoder exists to drop, and a hash
        /// of them would differ from run to run instead of failing the same way every time. An
        /// opcode whose table entry is STATUS_UNHANDLED never left the server -- SendPacket's
        /// socket branch refuses it -- and reads "<name> unhandled size=<n>".
        std::string PacketRecord(uint16 opcode, char const* name, uint8 const* data, size_t size, bool unhandled, Roles const& roles);

        /// "MVTEST TRACE <scenario> <seq> <window> <text>".
        std::string TraceLine(char const* scenario, uint32 seq, std::string const& window, std::string const& text);
        /// The digest fold of one TRACE line: FNV-1a over "<window> <text>\n", continuing from
        /// `digest`. The sequence number is left out, so a setup window's line count -- which is
        /// logged but never digested -- cannot move the digest of what follows it.
        uint32 DigestLine(uint32 digest, std::string const& window, std::string const& text);
    }
}

#endif
