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

#pragma once

#include "Platform/Define.h"

namespace proto
{
    /**
     * @brief What this server CHOOSES to put on the second stream.
     *
     * Kept deliberately apart from OpcodeSlots.inc, which is generated from the
     * client binary and is a statement of fact: those 19 opcodes are the ones the
     * client's receive gate refuses to take anywhere else. This file is the
     * opposite kind of thing -- a judgement we made, which the client permits but
     * does not require, and which a regeneration of the table must never silently
     * revert.
     *
     * The permission is real and was read out of the client rather than assumed
     * (Wow-64.exe 15595, sub_1400AA9A0). Its receive path consults the gate table
     * ONLY to force an opcode onto connection 1:
     *
     *     if ((op & 0x90CC) == 4) {                       // disp1-routable
     *         k = <10-bit compaction of op>;
     *         if ((byte_1409A7980[k >> 3] >> (k & 7)) & 1)
     *             if (connection_index != 1)
     *                 return;                             // silently dropped
     *     }
     *
     * There is no symmetric branch pinning anything to connection 0. An opcode
     * outside those 19 is taken on whichever live connection carries it, so the
     * 19 are a floor and not a ceiling.
     *
     * Note what the failure mode is, because it decides how safe an experiment
     * here can be: a wrongly-placed opcode is DROPPED, not answered with a
     * disconnect. The three sources of disconnect reason 3 are a connection
     * absent from the client's four-slot array, a first packet that is not the
     * server banner, and sub_1400A9B60's check that the slot is 0 or 1 and no
     * longer staging. None of them looks at the opcode. (The live reason-3 on
     * 2026-08-30 was the third of those -- the second stream had not yet been
     * promoted by SMSG_RESUME_COMMS -- and not, as was assumed at the time, a
     * consequence of the opcodes' send_slot.)
     *
     * WHAT MAY BE ADDED HERE. The constraint is ordering, not permission. Both
     * connections feed one queue that the client dispatches by opcode alone, and
     * two TCP streams have no ordering relative to one another, so anything moved
     * here can overtake or fall behind the stream-0 traffic it relates to. An
     * opcode naming an object whose SMSG_UPDATE_OBJECT rides stream 0 can
     * therefore arrive before the client has that object, and be discarded --
     * more likely under load, which is exactly when the split is meant to pay.
     *
     * So the test for admission is not "is it latency-sensitive" but "is it
     * self-contained": does it name only things the client already knows about?
     * That is the property the 19 Blizzard chose all share -- loot the client
     * asked for, a duel both sides agreed to, an attack it started, an item
     * entering its own bags. Unsolicited packets naming arbitrary units
     * (SMSG_ATTACKER_STATE_UPDATE, SMSG_AI_REACTION, the SMSG_SPELL_*_LOG family)
     * do not have it, and are deliberately absent below.
     */

    /// True for an opcode this server elects to emit on stream 1 even though the
    /// client's gate would accept it on either. False when the policy is off.
    bool PrefersStreamOne(uint16 opcode);

    /**
     * @brief Turn the elective routing on or off.
     *
     * Configuration, not code, because the cost of being wrong is paid on a live
     * realm: if a moved opcode turns out to race something it depends on, the
     * symptom is a loot window that does not open, and an operator needs to be
     * able to put it back without a rebuild.
     *
     * The 19 the client requires are NOT affected -- those are law, and stay on
     * stream 1 whatever this says.
     *
     * Set once at startup before the listeners open; read from every thread that
     * sends. Safe to call at any time regardless.
     */
    void SetStreamOnePolicyEnabled(bool enabled);
    bool IsStreamOnePolicyEnabled();
}
