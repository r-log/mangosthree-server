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

#include "Trace.h"
#include "Opcodes.h"

#include <cstdio>

namespace Harness
{
    namespace Trace
    {
        namespace
        {
            /// A bounds-checked little-endian reader over a payload. Every read reports whether
            /// the bytes were there; a decoder gives up on the first that is not.
            class Reader
            {
            public:
                Reader(uint8 const* data, size_t size) : m_data(data), m_size(size), m_pos(0) {}

                bool U8(uint8& v)
                {
                    if (m_pos + 1 > m_size) { return false; }
                    v = m_data[m_pos++];
                    return true;
                }
                bool U32(uint32& v)
                {
                    if (m_pos + 4 > m_size) { return false; }
                    v = uint32(m_data[m_pos]) | (uint32(m_data[m_pos + 1]) << 8) |
                        (uint32(m_data[m_pos + 2]) << 16) | (uint32(m_data[m_pos + 3]) << 24);
                    m_pos += 4;
                    return true;
                }
                bool U64(uint64& v)
                {
                    uint32 lo = 0, hi = 0;
                    if (!U32(lo) || !U32(hi)) { return false; }
                    v = uint64(lo) | (uint64(hi) << 32);
                    return true;
                }
                /// ByteBuffer::appendPackGUID's layout: a mask byte, then one byte for each set bit,
                /// lowest byte first.
                bool Packed(uint64& v)
                {
                    uint8 mask = 0;
                    if (!U8(mask)) { return false; }
                    v = 0;
                    for (uint8 i = 0; i < 8; ++i)
                    {
                        if (mask & (1 << i))
                        {
                            uint8 b = 0;
                            if (!U8(b)) { return false; }
                            v |= uint64(b) << (i * 8);
                        }
                    }
                    return true;
                }
                bool Skip(size_t n)
                {
                    if (m_pos + n > m_size) { return false; }
                    m_pos += n;
                    return true;
                }
                bool AtEnd() const { return m_pos == m_size; }
                size_t Pos() const { return m_pos; }

            private:
                uint8 const* m_data;
                size_t       m_size;
                size_t       m_pos;
            };

            struct RuleEntry
            {
                uint16 opcode;
                Rule   rule;
            };

            /// The note's §3.3 table, each writer read at master cc63db97b. Every opcode not
            /// listed is recorded by size. Hash means the writer was read and carries neither a
            /// clock nor a counter-numbered guid; the player guid some of them carry is the
            /// harness's fixed one.
            const RuleEntry kRules[] =
            {
                // the update-object family: the size moves with an item's packed guid
                { SMSG_UPDATE_OBJECT,               Rule::OpcodeOnly },
                { SMSG_COMPRESSED_UPDATE_OBJECT,    Rule::OpcodeOnly },
                { SMSG_DESTROY_OBJECT,              Rule::OpcodeOnly },
                // Player::SendNewItem: player guid, received/created/shown, bag, slot, entry,
                // suffix factor, random property, count, count held -- no item guid
                { SMSG_ITEM_PUSH_RESULT,            Rule::Hash },
                // Player::SendQuestUpdateAddCreatureOrGo: quest, entry, count, required, and the
                // guid the credit path passed (every harness credit passes an empty one)
                { SMSG_QUESTUPDATE_ADD_KILL,        Rule::Hash },
                // Player::SendQuestCompleteEvent: the quest id
                { SMSG_QUESTUPDATE_COMPLETE,        Rule::Hash },
                // Player::GiveLevel: the level, the health and power gains, the stat gains
                { SMSG_LEVELUP_INFO,                Rule::Hash },
                // Player::SendTalentsInfoData: the spec and talent lists and the glyphs
                { SMSG_TALENT_UPDATE,               Rule::Hash },
                // Player::SetTitle: the title's mask id and earned/lost
                { SMSG_TITLE_EARNED,                Rule::Hash },
                // BuildQuestCompletePacket (Player::SendQuestReward): six words and one byte of
                // bits, the 15595 layout -- quest, XP, money, skill and talents; no guid, no clock
                { SMSG_QUESTGIVER_QUEST_COMPLETE,   Rule::Hash },
                // Player::SendLogXPGain: the victim's guid (empty for a quest), the XP, the kind,
                // and for a kill the base XP and the group factor
                { SMSG_LOG_XPGAIN,                  Rule::Hash },
                // ReputationMgr::SendState: the referral bonus (0), the visual flag, the count and
                // each (list id, standing)
                { SMSG_SET_FACTION_STANDING,        Rule::Hash },
                // Player::learnSpell: the spell id and a zero
                { SMSG_LEARNED_SPELL,               Rule::Hash },
                // CurrencyMgr::ModifyCount: three bits, the counts and the currency id
                { SMSG_SET_CURRENCY,                Rule::Hash },
                // read for D4f0-2 at 07ef07c62 --
                // ReputationMgr::SendVisible: the faction's reputation list id
                { SMSG_SET_FACTION_VISIBLE,         Rule::Hash },
                // Player::SendInitialActionButtons: one packed action per button and a spec byte;
                // Player::SendLockActionButtons: the one byte 2
                { SMSG_ACTION_BUTTONS,              Rule::Hash },
                // Player::removeSpell: the spell id; or the spell and the rank that replaces it
                { SMSG_REMOVED_SPELL,               Rule::Hash },
                { SMSG_SUPERCEDED_SPELL,            Rule::Hash },
                // Player::SendSpellMod (PlayerSpellMod.cpp): the counts, the mod op, then each
                // (effect bit, value) -- no guid, no clock
                { SMSG_SET_FLAT_SPELL_MODIFIER,     Rule::Hash },
                { SMSG_SET_PCT_SPELL_MODIFIER,      Rule::Hash },
                // decoded: a date, timers, a cast counter or a guid from a counter sits among the
                // stable fields
                { SMSG_CRITERIA_UPDATE,             Rule::Decode },
                { SMSG_ACHIEVEMENT_EARNED,          Rule::Decode },
                { SMSG_SPELL_START,                 Rule::Decode },
                { SMSG_SPELL_GO,                    Rule::Decode },
                { SMSG_QUESTGIVER_STATUS_MULTIPLE,  Rule::Decode },
                // SpellAuraHolder::SendAuraUpdate: the target's packed guid and, unless the
                // aura's flags say the target cast it, the caster's -- a creature's comes from
                // the map's counter
                { SMSG_AURA_UPDATE,                 Rule::Decode },
                // Player::SendEquipError: the result, then two ITEM guids (numbered by the item
                // counter whenever an item is named) -- decoded so an item reads as a role
                { SMSG_INVENTORY_CHANGE_FAILURE,    Rule::Decode },
                // read for D11 PR 1 at 7b6a481ce -- what the spell family's first cast (scenario
                // 930) can send: each of the first five carries a guid the map's counter numbered
                // (the target, the attacker, the caster, the powered unit) among stable fields, so
                // it is decoded with the guid read as a role; SMSG_CAST_FAILED carries no guid and
                // is decoded for its result
                { SMSG_SPELLNONMELEEDAMAGELOG,      Rule::Decode },
                { SMSG_SPELLLOGMISS,                Rule::Decode },
                { SMSG_POWER_UPDATE,                Rule::Decode },
                { SMSG_SPELL_FAILURE,               Rule::Decode },
                { SMSG_SPELL_FAILED_OTHER,          Rule::Decode },
                { SMSG_CAST_FAILED,                 Rule::Decode },
            };

            void Append(std::string& out, char const* fmt, uint32 a)
            {
                char buf[64];
                snprintf(buf, sizeof(buf), fmt, a);
                out += buf;
            }

            /// Player::SendEquipError: the result byte; for a failure the two item guids (as
            /// "none" or "item": an item guid comes from the global counter), the bag-type byte,
            /// and the result's own tail (a required level or a limit category), hashed.
            bool DecodeInventoryFailure(uint8 const* data, size_t size, std::string& out)
            {
                Reader r(data, size);
                uint8 result = 0;
                if (!r.U8(result)) { return false; }
                std::string text;
                Append(text, "result=%u", result);
                if (r.AtEnd())
                {
                    out = text;
                    return true;
                }
                uint64 item = 0, item2 = 0;
                uint8 bagType = 0;
                if (!r.U64(item) || !r.U64(item2) || !r.U8(bagType)) { return false; }
                text += item ? " item=item" : " item=none";
                text += item2 ? " item2=item" : " item2=none";
                Append(text, " bag=%u", bagType);
                const size_t head = 1 + 8 + 8 + 1;
                Append(text, " tail=%u", uint32(size - head));
                text += " tailfnv=" + Hex32(Fnv1a(data + head, size - head));
                out = text;
                return true;
            }
        }

        uint32 Fnv1a(void const* data, size_t size, uint32 hash)
        {
            uint8 const* p = static_cast<uint8 const*>(data);
            for (size_t i = 0; i < size; ++i)
            {
                hash ^= p[i];
                hash *= 16777619u;
            }
            return hash;
        }

        std::string Hex32(uint32 value)
        {
            char buf[16];
            snprintf(buf, sizeof(buf), "%08x", value);
            return buf;
        }

        Rule RuleFor(uint16 opcode)
        {
            for (size_t i = 0; i < sizeof(kRules) / sizeof(kRules[0]); ++i)
            {
                if (kRules[i].opcode == opcode)
                {
                    return kRules[i].rule;
                }
            }
            return Rule::Size;
        }

        char const* RoleOf(Roles const& roles, uint64 guid)
        {
            if (!guid)
            {
                return "none";
            }
            if (guid == roles.self)
            {
                return "self";
            }
            if (guid == roles.giver)
            {
                return "giver";
            }
            if (guid == roles.target)
            {
                return "target";
            }
            if (guid == roles.caster)
            {
                return "caster";
            }
            return "other";
        }

        bool DecodeCriteriaUpdate(uint8 const* data, size_t size, Roles const& roles, std::string& out)
        {
            Reader r(data, size);
            uint32 id = 0, failed = 0, date = 0, t1 = 0, t2 = 0;
            uint64 counter = 0, player = 0;
            if (!r.U32(id) || !r.Packed(counter) || !r.Packed(player) || !r.U32(failed) ||
                !r.U32(date) || !r.U32(t1) || !r.U32(t2) || !r.AtEnd())
            {
                return false;
            }
            char buf[160];
            snprintf(buf, sizeof(buf), "criteria=%u counter=%llu failed=%u player=%s",
                     id, (unsigned long long)counter, failed, RoleOf(roles, player));
            out = buf;
            return true;
        }

        bool DecodeAchievementEarned(uint8 const* data, size_t size, Roles const& roles, std::string& out)
        {
            Reader r(data, size);
            uint64 player = 0;
            uint32 id = 0, date = 0, tail = 0;
            if (!r.Packed(player) || !r.U32(id) || !r.U32(date) || !r.U32(tail) || !r.AtEnd())
            {
                return false;
            }
            char buf[128];
            snprintf(buf, sizeof(buf), "achievement=%u player=%s tail=%u", id, RoleOf(roles, player), tail);
            out = buf;
            return true;
        }

        bool DecodeSpellCast(bool go, uint8 const* data, size_t size, Roles const& roles, std::string& out)
        {
            Reader r(data, size);
            uint64 source = 0, caster = 0;
            uint8 castCount = 0;
            uint32 spell = 0, flags = 0, timer = 0, fourth = 0;
            if (!r.Packed(source) || !r.Packed(caster) || !r.U8(castCount) || !r.U32(spell) ||
                !r.U32(flags) || !r.U32(timer) || !r.U32(fourth))
            {
                return false;
            }
            char buf[160];
            snprintf(buf, sizeof(buf), "spell=%u flags=0x%08x source=%s caster=%s",
                     spell, flags, RoleOf(roles, source), RoleOf(roles, caster));
            std::string text = buf;
            if (!go)
            {
                Append(text, " casttime=%u", fourth);   // m_casttime; GO's fourth word is its timestamp
            }
            else
            {
                // Spell::WriteSpellGoTargets: the hits as raw guids, then the misses with their
                // condition (and a reflect result after a reflect).
                uint8 hits = 0;
                if (!r.U8(hits)) { return false; }
                text += " hit=[";
                for (uint8 i = 0; i < hits; ++i)
                {
                    uint64 g = 0;
                    if (!r.U64(g)) { return false; }
                    text += (i ? "," : "");
                    text += RoleOf(roles, g);
                }
                text += "]";
                uint8 misses = 0;
                if (!r.U8(misses)) { return false; }
                text += " miss=[";
                for (uint8 i = 0; i < misses; ++i)
                {
                    uint64 g = 0;
                    uint8 condition = 0;
                    if (!r.U64(g) || !r.U8(condition)) { return false; }
                    text += (i ? "," : "");
                    text += RoleOf(roles, g);
                    Append(text, ":%u", condition);
                    if (condition == 11)   // SPELL_MISS_REFLECT carries its result
                    {
                        uint8 reflect = 0;
                        if (!r.U8(reflect)) { return false; }
                        Append(text, "/%u", reflect);
                    }
                }
                text += "]";
            }
            // SpellCastTargets::write: the mask, then one packed guid (or a zero byte) when the
            // mask names a unit, a corpse, an object or the unknown pguid flag.
            uint32 mask = 0;
            if (!r.U32(mask)) { return false; }
            Append(text, " mask=0x%x", mask);
            const uint32 pguidFlags = 0x00000002 | 0x00000200 | 0x00000800 | 0x00008000 | 0x00010000;
            if (mask & pguidFlags)
            {
                uint64 target = 0;
                if (!r.Packed(target)) { return false; }
                text += " target=";
                text += RoleOf(roles, target);
            }
            // SpellCastTargets::write: an item target's packed guid (or a zero byte) follows. The
            // item counter numbers it, so it reads as a role and is never part of the rest's hash
            // (the D4f0-1 final review's M-1).
            const uint32 itemFlags = 0x00000010 | 0x00001000;   // TARGET_FLAG_ITEM, TARGET_FLAG_TRADE_ITEM
            if (mask & itemFlags)
            {
                uint64 item = 0;
                if (!r.Packed(item)) { return false; }
                text += item ? " itemTarget=item" : " itemTarget=none";
            }
            // The rest of the packet -- predicted power and runes, the missile, the source and
            // destination locations -- carries no clock, and no counter-numbered guid while the
            // caster is off transports: a location target's packed transport guid is hashed with
            // it, and the harness player never stands on a transport.
            const size_t rest = size - r.Pos();
            Append(text, " rest=%u", uint32(rest));
            if (rest)
            {
                text += " restfnv=" + Hex32(Fnv1a(data + r.Pos(), rest));
            }
            out = text;
            return true;
        }

        bool ReadItemPush(uint8 const* data, size_t size, uint32& entry, uint32& count)
        {
            Reader r(data, size);
            uint64 player = 0;
            uint32 received = 0, created = 0, shown = 0, slot = 0, suffix = 0, property = 0, held = 0;
            uint8 bag = 0;
            return r.U64(player) && r.U32(received) && r.U32(created) && r.U32(shown) && r.U8(bag) &&
                   r.U32(slot) && r.U32(entry) && r.U32(suffix) && r.U32(property) && r.U32(count) &&
                   r.U32(held) && r.AtEnd();
        }

        bool ReadSpellDamage(uint8 const* data, size_t size, uint64& target, uint64& attacker, uint32& spell, uint32& damage,
                             uint32& overkill)
        {
            Reader r(data, size);
            uint32 absorb = 0, resist = 0, blocked = 0, hitInfo = 0;
            uint8 school = 0, physical = 0, unused = 0, extend = 0;
            return r.Packed(target) && r.Packed(attacker) && r.U32(spell) && r.U32(damage) && r.U32(overkill) &&
                   r.U8(school) && r.U32(absorb) && r.U32(resist) && r.U8(physical) && r.U8(unused) &&
                   r.U32(blocked) && r.U32(hitInfo) && r.U8(extend) && r.AtEnd();
        }

        bool ReadPowerUpdate(uint8 const* data, size_t size, uint64& unit, uint8& power, uint32& value)
        {
            Reader r(data, size);
            uint32 count = 0;
            if (!r.Packed(unit) || !r.U32(count) || !count || !r.U8(power) || !r.U32(value))
            {
                return false;
            }
            // the rest of the list, so a payload that is not the writer's layout is refused
            for (uint32 i = 1; i < count; ++i)
            {
                uint8 p = 0;
                uint32 v = 0;
                if (!r.U8(p) || !r.U32(v)) { return false; }
            }
            return r.AtEnd();
        }

        bool DecodeAuraUpdate(uint8 const* data, size_t size, Roles const& roles, std::string& out)
        {
            Reader r(data, size);
            uint64 target = 0;
            uint8 slot = 0;
            uint32 spell = 0;
            if (!r.Packed(target) || !r.U8(slot) || !r.U32(spell))
            {
                return false;
            }
            char buf[160];
            if (r.AtEnd())
            {
                // the removal form: the slot and a zero spell
                snprintf(buf, sizeof(buf), "target=%s slot=%u spell=%u", RoleOf(roles, target), uint32(slot), spell);
                out = buf;
                return spell == 0;
            }
            uint8 flagsLo = 0, flagsHi = 0, level = 0, stack = 0;
            if (!r.U8(flagsLo) || !r.U8(flagsHi) || !r.U8(level) || !r.U8(stack))
            {
                return false;
            }
            const uint32 flags = uint32(flagsLo) | (uint32(flagsHi) << 8);
            snprintf(buf, sizeof(buf), "target=%s slot=%u spell=%u flags=0x%x level=%u stack=%u",
                     RoleOf(roles, target), uint32(slot), spell, flags, uint32(level), uint32(stack));
            std::string text = buf;
            if (!(flags & 0x08))    // AFLAG_NOT_CASTER clear: the caster's packed guid follows
            {
                uint64 caster = 0;
                if (!r.Packed(caster)) { return false; }
                text += " caster=";
                text += RoleOf(roles, caster);
            }
            // The durations (AFLAG_DURATION) and the effect amounts (AFLAG_EFFECT_AMOUNT_SEND):
            // a duration counts down with the stepped world's own clock, the same in every run.
            const size_t rest = size - r.Pos();
            Append(text, " rest=%u", uint32(rest));
            if (rest)
            {
                text += " restfnv=" + Hex32(Fnv1a(data + r.Pos(), rest));
            }
            out = text;
            return true;
        }

        bool DecodeQuestGiverStatusMultiple(uint8 const* data, size_t size, Roles const& roles, std::string& out)
        {
            Reader r(data, size);
            uint32 count = 0;
            if (!r.U32(count)) { return false; }
            std::string text;
            Append(text, "count=%u", count);
            for (uint32 i = 0; i < count; ++i)
            {
                uint64 giver = 0;
                uint32 status = 0;
                if (!r.U64(giver) || !r.U32(status)) { return false; }
                text += " ";
                text += RoleOf(roles, giver);
                Append(text, ":%u", status);
            }
            if (!r.AtEnd()) { return false; }
            out = text;
            return true;
        }

        bool DecodeSpellDamageLog(uint8 const* data, size_t size, Roles const& roles, std::string& out)
        {
            Reader r(data, size);
            uint64 target = 0, attacker = 0;
            uint32 spell = 0, damage = 0, overkill = 0, absorb = 0, resist = 0, blocked = 0, hitInfo = 0;
            uint8 school = 0, physical = 0, unused = 0, extend = 0;
            if (!r.Packed(target) || !r.Packed(attacker) || !r.U32(spell) || !r.U32(damage) || !r.U32(overkill) ||
                !r.U8(school) || !r.U32(absorb) || !r.U32(resist) || !r.U8(physical) || !r.U8(unused) ||
                !r.U32(blocked) || !r.U32(hitInfo) || !r.U8(extend) || !r.AtEnd())
            {
                return false;
            }
            char buf[320];
            snprintf(buf, sizeof(buf),
                     "target=%s attacker=%s spell=%u damage=%u overkill=%u school=%u absorb=%u resist=%u physical=%u unused=%u blocked=%u hitInfo=0x%x extend=%u",
                     RoleOf(roles, target), RoleOf(roles, attacker), spell, damage, overkill, uint32(school), absorb, resist,
                     uint32(physical), uint32(unused), blocked, hitInfo, uint32(extend));
            out = buf;
            return true;
        }

        bool DecodeSpellLogMiss(uint8 const* data, size_t size, Roles const& roles, std::string& out)
        {
            Reader r(data, size);
            uint32 spell = 0, count = 0;
            uint64 caster = 0;
            uint8 flag = 0;
            if (!r.U32(spell) || !r.U64(caster) || !r.U8(flag) || !r.U32(count))
            {
                return false;
            }
            char buf[128];
            snprintf(buf, sizeof(buf), "spell=%u caster=%s flag=%u count=%u", spell, RoleOf(roles, caster), uint32(flag), count);
            std::string text = buf;
            text += " targets=[";
            for (uint32 i = 0; i < count; ++i)
            {
                uint64 target = 0;
                uint8 condition = 0;
                if (!r.U64(target) || !r.U8(condition)) { return false; }
                text += (i ? "," : "");
                text += RoleOf(roles, target);
                Append(text, ":%u", condition);
            }
            text += "]";
            if (!r.AtEnd()) { return false; }
            out = text;
            return true;
        }

        bool DecodePowerUpdate(uint8 const* data, size_t size, Roles const& roles, std::string& out)
        {
            Reader r(data, size);
            uint64 unit = 0;
            uint32 count = 0;
            if (!r.Packed(unit) || !r.U32(count))
            {
                return false;
            }
            std::string text = std::string("unit=") + RoleOf(roles, unit);
            Append(text, " count=%u", count);
            for (uint32 i = 0; i < count; ++i)
            {
                uint8 power = 0;
                uint32 value = 0;
                if (!r.U8(power) || !r.U32(value)) { return false; }
                Append(text, " power%u", uint32(power));
                Append(text, "=%u", value);
            }
            if (!r.AtEnd()) { return false; }
            out = text;
            return true;
        }

        bool DecodeSpellFailure(uint8 const* data, size_t size, Roles const& roles, std::string& out)
        {
            Reader r(data, size);
            uint64 caster = 0;
            uint8 castCount = 0, result = 0;
            uint32 spell = 0;
            if (!r.Packed(caster) || !r.U8(castCount) || !r.U32(spell) || !r.U8(result) || !r.AtEnd())
            {
                return false;
            }
            char buf[128];
            snprintf(buf, sizeof(buf), "caster=%s spell=%u result=%u", RoleOf(roles, caster), spell, uint32(result));
            out = buf;
            return true;
        }

        bool DecodeCastFailed(uint8 const* data, size_t size, std::string& out)
        {
            Reader r(data, size);
            uint8 castCount = 0, result = 0;
            uint32 spell = 0;
            if (!r.U8(castCount) || !r.U32(spell) || !r.U8(result))
            {
                return false;
            }
            // Everything after the result is the result's own tail, whose shape SendCastResult's
            // switch picks; it holds item, area, totem or skill ids, never a guid or a clock.
            const size_t head = 1 + 4 + 1;
            char buf[128];
            snprintf(buf, sizeof(buf), "spell=%u result=%u tail=%u", spell, uint32(result), uint32(size - head));
            std::string text = buf;
            if (size > head)
            {
                text += " tailfnv=" + Hex32(Fnv1a(data + head, size - head));
            }
            out = text;
            return true;
        }

        std::string PacketRecord(uint16 opcode, char const* name, uint8 const* data, size_t size, bool unhandled, Roles const& roles)
        {
            std::string text = name ? name : "?";
            if (unhandled)
            {
                Append(text, " unhandled size=%u", uint32(size));
                return text;
            }
            const Rule rule = RuleFor(opcode);
            switch (rule)
            {
                case Rule::OpcodeOnly:
                    return text;
                case Rule::Size:
                    Append(text, " size=%u", uint32(size));
                    return text;
                case Rule::Hash:
                    Append(text, " size=%u", uint32(size));
                    text += " fnv=" + Hex32(Fnv1a(data, size));
                    return text;
                case Rule::Decode:
                    break;
            }
            std::string fields;
            bool decoded = false;
            switch (opcode)
            {
                case SMSG_CRITERIA_UPDATE:            decoded = DecodeCriteriaUpdate(data, size, roles, fields); break;
                case SMSG_ACHIEVEMENT_EARNED:         decoded = DecodeAchievementEarned(data, size, roles, fields); break;
                case SMSG_SPELL_START:                decoded = DecodeSpellCast(false, data, size, roles, fields); break;
                case SMSG_SPELL_GO:                   decoded = DecodeSpellCast(true, data, size, roles, fields); break;
                case SMSG_QUESTGIVER_STATUS_MULTIPLE: decoded = DecodeQuestGiverStatusMultiple(data, size, roles, fields); break;
                case SMSG_INVENTORY_CHANGE_FAILURE:   decoded = DecodeInventoryFailure(data, size, fields); break;
                case SMSG_AURA_UPDATE:                decoded = DecodeAuraUpdate(data, size, roles, fields); break;
                case SMSG_SPELLNONMELEEDAMAGELOG:     decoded = DecodeSpellDamageLog(data, size, roles, fields); break;
                case SMSG_SPELLLOGMISS:               decoded = DecodeSpellLogMiss(data, size, roles, fields); break;
                case SMSG_POWER_UPDATE:               decoded = DecodePowerUpdate(data, size, roles, fields); break;
                case SMSG_SPELL_FAILURE:              decoded = DecodeSpellFailure(data, size, roles, fields); break;
                case SMSG_SPELL_FAILED_OTHER:         decoded = DecodeSpellFailure(data, size, roles, fields); break;
                case SMSG_CAST_FAILED:                decoded = DecodeCastFailed(data, size, fields); break;
                default: break;
            }
            if (!decoded)
            {
                // Size only: the bytes hold the clocks the decoder drops, so a hash of them would
                // change every run where the size fails the same way every time.
                Append(text, " undecoded size=%u", uint32(size));
                return text;
            }
            return text + " " + fields;
        }

        std::string SetDelta(std::set<uint32> const& before, std::set<uint32> const& after)
        {
            char buf[16];
            std::string out;
            for (std::set<uint32>::const_iterator i = after.begin(); i != after.end(); ++i)
            {
                if (!before.count(*i))
                {
                    snprintf(buf, sizeof(buf), "%u", *i);
                    out += (out.empty() ? "+" : " +") + std::string(buf);
                }
            }
            for (std::set<uint32>::const_iterator i = before.begin(); i != before.end(); ++i)
            {
                if (!after.count(*i))
                {
                    snprintf(buf, sizeof(buf), "%u", *i);
                    out += (out.empty() ? "-" : " -") + std::string(buf);
                }
            }
            return out;
        }

        namespace
        {
            /// The set named `name` in `sets`, or NULL.
            std::set<uint32> const* FindSet(std::vector<std::pair<std::string, std::set<uint32> > > const& sets, std::string const& name)
            {
                for (size_t i = 0; i < sets.size(); ++i)
                {
                    if (sets[i].first == name)
                    {
                        return &sets[i].second;
                    }
                }
                return NULL;
            }
        }

        std::vector<std::string> StateDelta(StateSnapshot const& before, StateSnapshot const& after)
        {
            std::vector<std::string> lines;
            // Every key of either side, in the map's order: a key that appeared or went reads "-"
            // on the side it is missing from.
            std::set<std::string> keys;
            for (std::map<std::string, std::string>::const_iterator i = before.values.begin(); i != before.values.end(); ++i)
            {
                keys.insert(i->first);
            }
            for (std::map<std::string, std::string>::const_iterator i = after.values.begin(); i != after.values.end(); ++i)
            {
                keys.insert(i->first);
            }
            for (std::set<std::string>::const_iterator k = keys.begin(); k != keys.end(); ++k)
            {
                std::map<std::string, std::string>::const_iterator b = before.values.find(*k);
                std::map<std::string, std::string>::const_iterator a = after.values.find(*k);
                const std::string was = b == before.values.end() ? "-" : b->second;
                const std::string is = a == after.values.end() ? "-" : a->second;
                if (was != is)
                {
                    lines.push_back("state " + *k + " " + was + "->" + is);
                }
            }
            // The id sets, in the order `after` lists them, then any `before` had and `after`
            // lacks: a set missing on one side reads as empty.
            const std::set<uint32> empty;
            for (size_t i = 0; i < after.sets.size(); ++i)
            {
                std::set<uint32> const* was = FindSet(before.sets, after.sets[i].first);
                const std::string delta = SetDelta(was ? *was : empty, after.sets[i].second);
                if (!delta.empty())
                {
                    lines.push_back("state " + after.sets[i].first + " " + delta);
                }
            }
            for (size_t i = 0; i < before.sets.size(); ++i)
            {
                if (!FindSet(after.sets, before.sets[i].first))
                {
                    const std::string delta = SetDelta(before.sets[i].second, empty);
                    if (!delta.empty())
                    {
                        lines.push_back("state " + before.sets[i].first + " " + delta);
                    }
                }
            }
            return lines;
        }

        std::string AuraHolderValue(uint32 effectMask, uint32 stack, uint32 charges, char const* casterRole, uint32 slot,
                                    int32 duration, int32 maxDuration)
        {
            char buf[192];
            snprintf(buf, sizeof(buf), "eff=0x%x stack=%u charges=%u caster=%s slot=%u dur=%d/%d",
                     effectMask, stack, charges, casterRole ? casterRole : "?", slot, duration, maxDuration);
            return buf;
        }

        bool SnapDue(std::string& last, std::string const& now)
        {
            if (now == last)
            {
                return false;
            }
            last = now;
            return true;
        }

        std::string TraceLine(char const* scenario, uint32 seq, std::string const& window, std::string const& text)
        {
            char buf[32];
            snprintf(buf, sizeof(buf), " %u ", seq);
            return std::string("MVTEST TRACE ") + scenario + buf + window + " " + text;
        }

        uint32 DigestLine(uint32 digest, std::string const& window, std::string const& text)
        {
            const std::string line = window + " " + text + "\n";
            return Fnv1a(line.data(), line.size(), digest);
        }
    }
}
