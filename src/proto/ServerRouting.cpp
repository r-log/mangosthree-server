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

#include "ServerRouting.h"

#include "Opcodes.h"

#include <atomic>

namespace proto
{
    namespace
    {
        /// Default on: the whole point of opening a second stream is to use it.
        /// Set from SecondStream.ExtraRouting at startup.
        std::atomic<bool> s_policyEnabled{true};
    }

    void SetStreamOnePolicyEnabled(bool enabled)
    {
        s_policyEnabled.store(enabled, std::memory_order_relaxed);
    }

    bool IsStreamOnePolicyEnabled()
    {
        return s_policyEnabled.load(std::memory_order_relaxed);
    }

    bool PrefersStreamOne(uint16 opcode)
    {
        if (!s_policyEnabled.load(std::memory_order_relaxed))
        {
            return false;
        }

        // The loot tail.
        //
        // Six loot opcodes are already on stream 1 because the client's gate
        // insists (LOOT_RESPONSE, LOOT_MONEY_NOTIFY, LOOT_CLEAR_MONEY,
        // LOOT_ITEM_NOTIFY, LOOT_REMOVED, LOOT_RELEASE_RESPONSE). These are the
        // rest of the same conversation, which Blizzard left unpinned and this
        // fork sent down the busy stream -- so a loot roll queued behind the
        // position feed while the six packets around it did not.
        //
        // Every one is self-contained in the sense the header describes: it is
        // only ever sent to a client that has a loot window open on an object it
        // asked to loot, so the objects and items named are already known to it.
        // Nothing here can outrun a creation it depends on.
        //
        // Listed by name; the values are in Opcodes.h and the client's own view
        // of them is in OpcodeSlots.inc, where each reads as recv_slot 0 -- which
        // is what makes putting them here a choice rather than a correction.
        switch (opcode)
        {
            case SMSG_LOOT_MASTER_LIST:        // 0x0325
            case SMSG_LOOT_CURRENCY_REMOVED:   // 0x1DB4
            case SMSG_LOOT_START_ROLL:         // 0x2227
            case SMSG_LOOT_ALL_PASSED:         // 0x6237
            case SMSG_LOOT_ROLL:               // 0x6507
            case SMSG_LOOT_ROLL_WON:           // 0x6617
            case SMSG_LOOT_LIST:               // 0x6807
                return true;

            default:
                return false;
        }

        // Deliberately NOT here, though they were considered: the combat log
        // family. SMSG_ATTACKER_STATE_UPDATE, SMSG_AI_REACTION,
        // SMSG_DAMAGE_CALC_LOG and the SMSG_SPELL_*_LOG opcodes are unsolicited
        // and name arbitrary units, so on a stream that is nearly empty they can
        // overtake the SMSG_UPDATE_OBJECT that creates those units on stream 0
        // and be dropped for an object the client does not have yet. They are the
        // biggest prize and the biggest risk; they need the ordering question
        // answered first, and measurement either side of it.
        //
        // SMSG_LOOT_UPDATE (0x14FE) is absent for a different reason: it passes
        // neither of the client's dispatch gates ((op & 0x90CC) != 4 and
        // (op & 0x92E8) != 0x10A0), so the 15595 client discards it on any
        // stream. Moving it would change nothing. That it is sent at all looks
        // like a separate pre-existing bug, and is not one this file should fix.
    }
}
