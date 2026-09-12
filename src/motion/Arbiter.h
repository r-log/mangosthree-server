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

#ifndef MANGOS_MOTION_ARBITER_H
#define MANGOS_MOTION_ARBITER_H

#include "Platform/Define.h"
#include <array>
#include <optional>
#include <vector>

/**
 * The movement kernel's arbiter (design v2 §4): which behaviour a unit runs,
 * and why the others stopped. Seven layers, one entry each except Control,
 * which holds a set of claims by identity (§4.1); three policies between
 * them (§4.2); every public operation is a transaction whose generation
 * decides whether a request issued from inside a hook survives (§4.3); a
 * fixed ring of decisions for the GM dump. Nothing here knows a unit, a
 * driver, a map or a clock: the shell above (P3-B) delivers the events this
 * class accumulates and tells it when a leg or a timer ended. The legacy
 * MovementGeneratorType conversions live with the shim, outside this seam.
 */
namespace Motion
{
    enum class Kind : uint8
    {
        Idle, Wander, Patrol, Follow,      ///< Default layer
        Chase,                             ///< Combat
        Point, FlyLand, Home, AssistRun,   ///< Scripted
        Distract, AssistDistract,          ///< Distract
        Fear, Confused,                    ///< Control (claims)
        Effect,                            ///< Forced
        Taxi,                              ///< Taxi
        Count
    };

    enum class Layer : uint8 { Default, Combat, Scripted, Distract, Control, Forced, Taxi, Count };

    enum class Policy : uint8 { Supersede, Suspend, Override };

    enum class FinishReason : uint8
    {
        Arrived, Cut, Blocked, Expired, Superseded, Overridden, Cleared, Cancelled, TargetLost, Died
    };

    /// Kernel-external events the policy table maps (§4.2 event rows).
    enum class ExternalEvent : uint8 { CombatStarted };

    /// What the outermost transaction is, which decides the fate of requests
    /// issued from inside it (§4.3 generations).
    enum class TransactionKind : uint8 { Normal, Clear, ClearAll, Death };

    Layer       LayerOf(Kind kind);                      ///< the §4.1 table
    Policy      PolicyOf(Kind kind, bool resumeCombat);  ///< the §4.2 table; Point flips to Suspend with resumeCombat
    bool        SelfExpiring(Kind kind);                 ///< Home, Distract, Effect: cancelled by any other-layer request
    char const* KindName(Kind kind);
    char const* LayerName(Layer layer);
    char const* ReasonName(FinishReason reason);

    /// One request to run a behaviour.
    struct MoveRequest
    {
        Kind   kind;
        uint32 id = 0;               ///< the MovementInform id the shim projects, 0 when none
        bool   resumeCombat = false; ///< Point only: Suspend instead of Override (D2)
        uint64 claim = 0;            ///< Control only: the identity of the claim (the aura); never 0 there
    };

    /// One held entry, wherever it sits.
    struct Held
    {
        Kind   kind;
        uint32 id;         ///< MovementInform id, 0 when none
        uint32 seq;        ///< arrival order; newest wins ties and tells a resume from a fresh start
        uint64 claim;      ///< Control entries only, else 0
        uint32 generation; ///< the transaction that created it
        bool   doomed;     ///< created inside a discarding transaction: finished at its commit
    };

    /// One selection event, drained by the shell after a mutating call.
    struct Event
    {
        enum class Kind : uint8 { Finished, Suspended, Resumed, DefaultSwapped };
        Kind         kind;
        Motion::Kind who;
        uint32       id;
        FinishReason reason;   ///< Finished/DefaultSwapped: why; Suspended: Cut; Resumed: Arrived
        uint64       claim;    ///< Control entries only, else 0
    };

    /// One line of the decision ring: what was asked, what was selected before and after.
    struct Decision
    {
        enum class Op : uint8
        {
            InstallDefault, Request, Clear, ClearAll, ExpireSelected, Expire, FinishSelected,
            CancelControl, Release, Notify, Die, Commit
        };
        Op           op;
        Motion::Kind kind;        ///< the operation's kind argument (Idle when none)
        uint32       id;
        uint64       claim;
        bool         hadBefore;
        Held         before;
        bool         hadAfter;
        Held         after;
        uint32       generation;
    };
}

#endif
