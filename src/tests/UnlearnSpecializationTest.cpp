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

/// A profession specialization unlearned from the spellbook: the client's index mapped to the
/// specialization spell, and the opcode row that removes the spell from a player who knows it,
/// keeps the profession skill and sends the client one SMSG_REMOVED_SPELL.
///
/// The player is a probe seated on a socketless session (no map, no world). The spell and the skill
/// line it needs are seeded with DBCStorage::SetEntry, as SpellModMgrTest.cpp does. The session's
/// spell notices are installed for each case, so the removal's packet reaches the session's sink.

#include "TestHarness.h"

#include "Auth/BigNumber.h"
#include "DBCStores.h"
#include "DBCStructure.h"
#include "OpcodeTable.h"
#include "Opcodes.h"
#include "Player.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "entities/player/spells/PlayerSpellNotices.h"
#include "session/handlers/entities/SkillHandlers.h"
#include "session/packets/entities/PlayerSpellPackets.h"

#include <vector>

namespace
{
    uint32 const kElixirMaster = 28677;
    uint32 const kPotionMaster = 28675;
    uint16 const kAlchemyValue = 475;
    uint16 const kAlchemyMax = 525;

    // Spell.dbc rows as the loader makes them (SpellEntry has no default constructor).
    struct SpellRowStorage
    {
        alignas(SpellEntry) unsigned char bytes[sizeof(SpellEntry)];
    };

    SpellRowStorage s_elixirMasterRow = {};
    SkillLineEntry s_alchemyLine = {};

    void SeedStores()
    {
        static bool seeded = false;
        if (seeded)
        {
            return;
        }
        seeded = true;

        SpellEntry* elixirMaster = reinterpret_cast<SpellEntry*>(s_elixirMasterRow.bytes);
        elixirMaster->ID = kElixirMaster;
        sSpellStore.SetEntry(kElixirMaster, elixirMaster);

        s_alchemyLine.ID = SKILL_ALCHEMY;
        s_alchemyLine.CategoryID = SKILL_CATEGORY_PROFESSION;
        sSkillLineStore.SetEntry(SKILL_ALCHEMY, &s_alchemyLine);
    }

    void CapturePacket(void* context, WorldPacket const& packet)
    {
        static_cast<std::vector<WorldPacket>*>(context)->push_back(packet);
    }

    /// The session's spell notices installed while it lives.
    class InstalledSpellNotices
    {
        public:
            InstalledSpellNotices() : m_previous(PlayerSpellNotices::Install(&m_notices)) { }
            ~InstalledSpellNotices() { PlayerSpellNotices::Install(m_previous); }

        private:
            SessionPlayerSpellNotices m_notices;
            PlayerSpellNotices* m_previous;
    };

    /// A human warrior with Alchemy at 475 who knows Elixir Master, seated on `session` while it lives.
    class SpecializedPlayer : public Player
    {
        public:
            explicit SpecializedPlayer(WorldSession& session) : Player(&session)
            {
                Object::_Create(1, 0, HIGHGUID_PLAYER);
                SetByteValue(UNIT_FIELD_BYTES_0, 0, RACE_HUMAN);
                SetByteValue(UNIT_FIELD_BYTES_0, 1, CLASS_WARRIOR);
                session.SetPlayer(this);

                SetSkill(SKILL_ALCHEMY, kAlchemyValue, kAlchemyMax);
                learnSpell(kElixirMaster, false);
            }

            ~SpecializedPlayer()
            {
                SessionOf(*this)->SetPlayer(nullptr);
                delete[] m_uint32Values;
                m_uint32Values = NULL;
            }
    };

    /// Sends the client's unlearn request for `index` through the opcode table's row and answers whether the
    /// row read it to its end.
    bool SendUnlearn(WorldSession& session, uint32 index)
    {
        WorldPacket request(CMSG_UNLEARN_SPECIALIZATION, 4);
        request << uint32(index);
        opcodeTable[CMSG_UNLEARN_SPECIALIZATION].handler(session, request);
        return request.rpos() == request.size();
    }
}

TEST(UnlearnSpecialization_EachIndexNamesTheClientsSpecializationSpell)
{
    CHECK_EQ(SpecializationSpellOfIndex(0), uint32(20219));    // Gnomish Engineer
    CHECK_EQ(SpecializationSpellOfIndex(1), uint32(20222));    // Goblin Engineer
    CHECK_EQ(SpecializationSpellOfIndex(2), uint32(28677));    // Elixir Master
    CHECK_EQ(SpecializationSpellOfIndex(3), uint32(28675));    // Potion Master
    CHECK_EQ(SpecializationSpellOfIndex(4), uint32(28672));    // Transmutation Master

    CHECK_EQ(SpecializationSpellOfIndex(5), uint32(0));
    CHECK_EQ(SpecializationSpellOfIndex(7), uint32(0));
    CHECK_EQ(SpecializationSpellOfIndex(0xFFFFFFFF), uint32(0));
}

TEST(UnlearnSpecialization_TheRowRemovesAKnownSpecializationAndKeepsTheSkill)
{
    SeedStores();
    InitializeOpcodes();
    InstalledSpellNotices notices;

    std::vector<WorldPacket> sent;
    WorldSession session(1, "alchemist", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    session.SetSocketlessSink(&CapturePacket, &sent);
    {
        SpecializedPlayer player(session);
        CHECK(player.HasSpell(kElixirMaster));
        CHECK_EQ(player.GetSkillValue(SKILL_ALCHEMY), kAlchemyValue);

        CHECK(SendUnlearn(session, 2));

        CHECK(!player.HasSpell(kElixirMaster));
        CHECK(player.HasSkill(SKILL_ALCHEMY));
        CHECK_EQ(player.GetSkillValue(SKILL_ALCHEMY), kAlchemyValue);
        CHECK_EQ(player.GetMaxSkillValue(SKILL_ALCHEMY), kAlchemyMax);

        // The client is told once that the specialization left its spell book.
        CHECK_EQ(sent.size(), std::size_t(1));
        if (sent.size() == 1)
        {
            CHECK_EQ(sent[0].GetOpcode(), uint16(SMSG_REMOVED_SPELL));
            CHECK_EQ(sent[0].size(), std::size_t(4));
            WorldPacket removed(sent[0]);
            removed.rpos(0);
            uint32 spellId = 0;
            removed >> spellId;
            CHECK_EQ(spellId, kElixirMaster);
        }

        // A second request for the same index finds nothing left to remove.
        CHECK(SendUnlearn(session, 2));
        CHECK_EQ(sent.size(), std::size_t(1));
    }
    session.SetSocketlessSink(nullptr, nullptr);
}

TEST(UnlearnSpecialization_AnIndexOutsideTheTableOrASpecializationNotKnownChangesNothing)
{
    SeedStores();
    InitializeOpcodes();
    InstalledSpellNotices notices;

    std::vector<WorldPacket> sent;
    WorldSession session(1, "alchemist", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    session.SetSocketlessSink(&CapturePacket, &sent);
    {
        SpecializedPlayer player(session);
        CHECK(!player.HasSpell(kPotionMaster));

        // Index 7 names no specialization; index 3 names Potion Master, which this player does not know.
        CHECK(SendUnlearn(session, 7));
        CHECK(SendUnlearn(session, 3));
        CHECK(SendUnlearn(session, 0xFFFFFFFF));

        CHECK(player.HasSpell(kElixirMaster));
        CHECK_EQ(player.GetSkillValue(SKILL_ALCHEMY), kAlchemyValue);
        CHECK(sent.empty());
    }
    session.SetSocketlessSink(nullptr, nullptr);
}
