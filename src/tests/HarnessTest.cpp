// The GM harness's pure parts (movement P0-C): the step timeline every scenario
// runs on, the verdict line, and the rule the teardown classifies an owned player by;
// and (decoupling D4f0) the reward recorder's rule table, decoders, line and digest, and the
// achievement closure's table of modelled criteria types.
// Nothing here touches a map.
#include "TestHarness.h"
#include "Timeline.h"
#include "Ownership.h"
#include "Trace.h"
#include "QuestFixture.h"
#include "DBCEnums.h"
#include "WorldPacket.h"
#include "Opcodes.h"

#include <cstdio>
#include <string>
#include <vector>

TEST(HarnessTimeline_runs_due_steps_in_offset_order_once)
{
    Harness::Timeline t;
    std::vector<int> order;
    t.At(500, [&order]() { order.push_back(2); });
    t.At(200, [&order]() { order.push_back(1); });
    t.At(500, [&order]() { order.push_back(3); });   // a tie keeps insertion order
    CHECK_EQ(t.Pending(), size_t(3));
    t.Advance(100);
    CHECK(order.empty());
    t.Advance(300);
    CHECK_EQ(order.size(), size_t(1));
    CHECK_EQ(order[0], 1);
    t.Advance(600);
    CHECK_EQ(order.size(), size_t(3));
    CHECK_EQ(order[1], 2);
    CHECK_EQ(order[2], 3);
    CHECK(t.Idle());
    t.Advance(900);
    CHECK_EQ(order.size(), size_t(3));
}

TEST(HarnessTimeline_a_step_scheduled_from_a_step_is_relative_to_that_moment)
{
    Harness::Timeline t;
    std::vector<uint32> firedAt;
    t.At(1000, [&t, &firedAt]()
    {
        firedAt.push_back(t.Now());
        t.At(300, [&t, &firedAt]() { firedAt.push_back(t.Now()); });
    });
    t.Advance(1000);
    CHECK_EQ(firedAt.size(), size_t(1));
    CHECK_EQ(t.Pending(), size_t(1));
    t.Advance(1200);
    CHECK_EQ(firedAt.size(), size_t(1));
    t.Advance(1300);
    CHECK_EQ(firedAt.size(), size_t(2));
    CHECK_EQ(firedAt[1], 1300u);
}

TEST(HarnessTimeline_a_late_tick_runs_every_step_it_passed)
{
    Harness::Timeline t;
    int n = 0;
    for (uint32 i = 1; i <= 16; ++i)
    {
        t.At(1700 + i * 200, [&n]() { ++n; });
    }
    t.Advance(1700);
    CHECK_EQ(n, 0);
    t.Advance(2500);   // 1900, 2100, 2300, 2500 due
    CHECK_EQ(n, 4);
    t.Advance(9000);
    CHECK_EQ(n, 16);
}

TEST(HarnessVerdictLine_has_the_old_shape)
{
    CHECK_STR(Harness::VerdictLine("jump-over-point", "B1=OK(jump completed server-side) | B5=OK(point leg resumed)").c_str(),
              "VERDICT jump-over-point B1=OK(jump completed server-side) | B5=OK(point leg resumed)");
}

TEST(HarnessDist2_is_planar)
{
    CHECK_EQ(Harness::Dist2(0.0f, 0.0f, 3.0f, 4.0f), 5.0f);
    CHECK_EQ(Harness::Dist2(-3122.6f, -261.3f, -3122.6f, -261.3f), 0.0f);
}

// The teardown's own rule: what a scenario still owns of a harness player by the time
// Runner::End reaches its record. Addresses stand in for the objects because that is
// exactly what the rule is about -- the question is asked where following the pointer
// would be a use-after-free, so nothing here may be more than an address.
namespace
{
    void const* const kOwnedPlayer = reinterpret_cast<void const*>(0x1000);
    void const* const kAnotherPlayer = reinterpret_cast<void const*>(0x2000);
}

TEST(HarnessOwnership_the_registered_object_is_the_one_we_own)
{
    CHECK(Harness::ClassifyOwnership(kOwnedPlayer, kOwnedPlayer) == Harness::Ownership::Held);
}

TEST(HarnessOwnership_nothing_registered_means_somebody_else_destroyed_him)
{
    // Map::Remove(player, true) -> Map::DeleteFromWorld unregisters and then deletes, so an
    // empty answer is the only sign the teardown gets that its Player* is now freed memory.
    CHECK(Harness::ClassifyOwnership(kOwnedPlayer, NULL) == Harness::Ownership::Destroyed);
}

TEST(HarnessOwnership_a_different_object_on_the_guid_is_not_ours_to_touch)
{
    // The harness hands out guids from one small reserved block and restarts at the bottom
    // of it for every scenario, so a later player can answer an earlier one's guid. Presence
    // is not identity: tearing THAT one down would be worse than the leak it avoided.
    // And this outcome says nothing about whether OUR player is still alive -- he may be, and
    // being updated every tick -- so the teardown frees nothing on the strength of it, not
    // even the session it allocated itself.
    CHECK(Harness::ClassifyOwnership(kOwnedPlayer, kAnotherPlayer) == Harness::Ownership::Replaced);
    // And the classification is not symmetric in some accidental way: swap the roles and it
    // is still the record's pointer that decides.
    CHECK(Harness::ClassifyOwnership(kAnotherPlayer, kOwnedPlayer) == Harness::Ownership::Replaced);
}

TEST(HarnessOwnership_an_empty_record_never_reads_as_held)
{
    // Unreachable through SpawnPlayer, which records only a player it built; pinned anyway,
    // because the one outcome that must never come out of a NULL record is the branch that
    // dereferences it.
    CHECK(Harness::ClassifyOwnership(NULL, NULL) == Harness::Ownership::Destroyed);
    CHECK(Harness::ClassifyOwnership(NULL, kOwnedPlayer) == Harness::Ownership::Replaced);
}

TEST(HarnessSeed_derives_from_the_base_and_the_order)
{
    CHECK_EQ(Harness::SeedFor(0x4D56, 0), uint32(0x4D56));
    CHECK_EQ(Harness::SeedFor(0x4D56, 25), uint32(0x4D56 + 25));
    CHECK_EQ(Harness::SeedFor(2, 25), uint32(27));
    CHECK(Harness::SeedFor(2, 25) != Harness::SeedFor(3, 25));
    CHECK(Harness::TickSeed(0x4D56, 3, 0) != Harness::TickSeed(0x4D56, 3, 50));
    CHECK(Harness::TickSeed(0x4D56, 3, 100) != Harness::TickSeed(0x4D56, 4, 100));
    CHECK_EQ(Harness::TickSeed(7, 2, 150), Harness::TickSeed(7, 2, 150));
    CHECK(Harness::StepSeed(0x4D56, 3, 100) != Harness::TickSeed(0x4D56, 3, 100));
    CHECK(Harness::StepSeed(0x4D56, 3, 0) != Harness::StepSeed(0x4D56, 3, 50));
    CHECK_EQ(Harness::StepSeed(7, 2, 150), Harness::StepSeed(7, 2, 150));
}

// ---- decoupling D4f0: the reward recorder's pure half (Harness/Trace.h) ------------------------
//
// Every payload below is laid out by the same statements its production writer uses -- the
// writer is named above each -- so a decoder that drifts from its writer fails here, not in a
// harness run. The row packets these tests spell (SMSG_CRITERIA_UPDATE, SMSG_ACHIEVEMENT_EARNED,
// SMSG_SET_FACTION_STANDING) are observed, never built for a client: CheckStateOwnership.cmake
// allows exactly these names in this file.
namespace
{
    const uint64 kSelf = 0x0000000000F00000ULL;    // a harness player's guid: the reserved block
    const uint64 kGiver = 0xF130000105000123ULL;   // a spawned creature's: the map's counter in the low bits
    const uint64 kOther = 0xF130000105000999ULL;

    Harness::Trace::Roles TestRoles()
    {
        Harness::Trace::Roles r;
        r.self = kSelf;
        r.giver = kGiver;
        return r;
    }

    std::string Decoded(uint16 opcode, WorldPacket const& p)
    {
        return Harness::Trace::PacketRecord(opcode, "OP", p.contents(), p.size(), false, TestRoles());
    }

    /// AchievementMgr::SendCriteriaUpdate, statement for statement.
    WorldPacket CriteriaUpdate(uint32 id, uint64 counter, uint64 player, bool failed, uint32 now, uint32 date)
    {
        WorldPacket data(SMSG_CRITERIA_UPDATE, 8 + 4 + 8);
        data << uint32(id);
        data.appendPackGUID(counter);
        data.appendPackGUID(player);                 // GetPlayer()->GetPackGUID()
        data << uint32(failed ? 1 : 0);
        data << uint32(now);                         // secsToTimeBitFields(now)
        data << uint32(now - date);                  // timer 1
        data << uint32(now - date);                  // timer 2
        return data;
    }
}

TEST(HarnessTrace_fnv1a_is_the_published_32_bit_function)
{
    CHECK_EQ(Harness::Trace::Fnv1a("", 0), 0x811c9dc5u);
    CHECK_EQ(Harness::Trace::Fnv1a("a", 1), 0xe40c292cu);
    CHECK_EQ(Harness::Trace::Fnv1a("foobar", 6), 0xbf9cf968u);
    // continuing a fold is the same as folding the concatenation
    CHECK_EQ(Harness::Trace::Fnv1a("bar", 3, Harness::Trace::Fnv1a("foo", 3)), 0xbf9cf968u);
    CHECK_STR(Harness::Trace::Hex32(0xbf9cf968u).c_str(), "bf9cf968");
    CHECK_STR(Harness::Trace::Hex32(0x1u).c_str(), "00000001");
}

TEST(HarnessTrace_the_rule_table_follows_the_note)
{
    using Harness::Trace::Rule;
    CHECK(Harness::Trace::RuleFor(SMSG_UPDATE_OBJECT) == Rule::OpcodeOnly);
    CHECK(Harness::Trace::RuleFor(SMSG_COMPRESSED_UPDATE_OBJECT) == Rule::OpcodeOnly);
    CHECK(Harness::Trace::RuleFor(SMSG_DESTROY_OBJECT) == Rule::OpcodeOnly);
    CHECK(Harness::Trace::RuleFor(SMSG_ITEM_PUSH_RESULT) == Rule::Hash);
    CHECK(Harness::Trace::RuleFor(SMSG_QUESTUPDATE_ADD_KILL) == Rule::Hash);
    CHECK(Harness::Trace::RuleFor(SMSG_QUESTUPDATE_COMPLETE) == Rule::Hash);
    CHECK(Harness::Trace::RuleFor(SMSG_LEVELUP_INFO) == Rule::Hash);
    CHECK(Harness::Trace::RuleFor(SMSG_TALENT_UPDATE) == Rule::Hash);
    CHECK(Harness::Trace::RuleFor(SMSG_TITLE_EARNED) == Rule::Hash);
    CHECK(Harness::Trace::RuleFor(SMSG_QUESTGIVER_QUEST_COMPLETE) == Rule::Hash);
    CHECK(Harness::Trace::RuleFor(SMSG_LOG_XPGAIN) == Rule::Hash);
    CHECK(Harness::Trace::RuleFor(SMSG_SET_FACTION_STANDING) == Rule::Hash);
    CHECK(Harness::Trace::RuleFor(SMSG_LEARNED_SPELL) == Rule::Hash);
    CHECK(Harness::Trace::RuleFor(SMSG_SET_CURRENCY) == Rule::Hash);
    CHECK(Harness::Trace::RuleFor(SMSG_CRITERIA_UPDATE) == Rule::Decode);
    CHECK(Harness::Trace::RuleFor(SMSG_ACHIEVEMENT_EARNED) == Rule::Decode);
    CHECK(Harness::Trace::RuleFor(SMSG_SPELL_START) == Rule::Decode);
    CHECK(Harness::Trace::RuleFor(SMSG_SPELL_GO) == Rule::Decode);
    CHECK(Harness::Trace::RuleFor(SMSG_QUESTGIVER_STATUS_MULTIPLE) == Rule::Decode);
    CHECK(Harness::Trace::RuleFor(SMSG_INVENTORY_CHANGE_FAILURE) == Rule::Decode);
    // anything whose writer has not been read goes by size
    CHECK(Harness::Trace::RuleFor(SMSG_MESSAGECHAT) == Rule::Size);
}

TEST(HarnessTrace_a_guid_reads_as_its_role)
{
    CHECK_STR(Harness::Trace::RoleOf(TestRoles(), kSelf), "self");
    CHECK_STR(Harness::Trace::RoleOf(TestRoles(), kGiver), "giver");
    CHECK_STR(Harness::Trace::RoleOf(TestRoles(), 0), "none");
    CHECK_STR(Harness::Trace::RoleOf(TestRoles(), kOther), "other");
}

TEST(HarnessTrace_criteria_update_keeps_id_counter_flag_and_drops_the_clocks)
{
    WorldPacket a = CriteriaUpdate(9682, 250, kSelf, false, 0x12345678, 0x12345000);
    CHECK_STR(Decoded(SMSG_CRITERIA_UPDATE, a).c_str(), "OP criteria=9682 counter=250 failed=0 player=self");
    // another moment, another date: the same record
    WorldPacket b = CriteriaUpdate(9682, 250, kSelf, false, 0x22222222, 0x11111111);
    CHECK_STR(Decoded(SMSG_CRITERIA_UPDATE, b).c_str(), Decoded(SMSG_CRITERIA_UPDATE, a).c_str());
    // a counter of the same packed width is still told apart (F4: size alone would not)
    WorldPacket c = CriteriaUpdate(9682, 251, kSelf, false, 0x12345678, 0x12345000);
    CHECK(a.size() == c.size());
    CHECK(Decoded(SMSG_CRITERIA_UPDATE, a) != Decoded(SMSG_CRITERIA_UPDATE, c));
    // a big counter, a failed flag, another earner
    WorldPacket d = CriteriaUpdate(17485, 0x1234567890ULL, kOther, true, 1, 1);
    CHECK_STR(Decoded(SMSG_CRITERIA_UPDATE, d).c_str(), "OP criteria=17485 counter=78187493520 failed=1 player=other");
}

TEST(HarnessTrace_a_decoder_refuses_bytes_that_do_not_end_where_the_writer_does)
{
    WorldPacket a = CriteriaUpdate(9682, 250, kSelf, false, 1, 1);
    std::string out;
    CHECK(Harness::Trace::DecodeCriteriaUpdate(a.contents(), a.size(), TestRoles(), out));
    CHECK(!Harness::Trace::DecodeCriteriaUpdate(a.contents(), a.size() - 1, TestRoles(), out));
    WorldPacket longer(a);
    longer << uint8(0);
    CHECK(!Harness::Trace::DecodeCriteriaUpdate(longer.contents(), longer.size(), TestRoles(), out));
    // and the record says so, by the size alone: the bytes it could not place hold the clocks,
    // so a hash of them would differ from run to run
    char want[64];
    snprintf(want, sizeof(want), "OP undecoded size=%u", uint32(longer.size()));
    CHECK_STR(Decoded(SMSG_CRITERIA_UPDATE, longer).c_str(), want);
}

TEST(HarnessTrace_achievement_earned_keeps_the_id_and_drops_the_date)
{
    // AchievementMgr::SendAchievementEarned
    WorldPacket data(SMSG_ACHIEVEMENT_EARNED, 8 + 4 + 8);
    data.appendPackGUID(kSelf);                       // GetPlayer()->GetPackGUID()
    data << uint32(889);
    data << uint32(0x0BADF00D);                       // secsToTimeBitFields(time(NULL))
    data << uint32(0);
    CHECK_STR(Decoded(SMSG_ACHIEVEMENT_EARNED, data).c_str(), "OP achievement=889 player=self tail=0");
}

TEST(HarnessTrace_spell_start_keeps_spell_flags_and_roles)
{
    // Spell::SendSpellStart for a self-cast, no item: caster twice, the cast count, the spell,
    // the flags, m_timer, m_casttime, then SpellCastTargets::write with a unit target.
    WorldPacket data(SMSG_SPELL_START, 8 + 8 + 4 + 4 + 2);
    data.appendPackGUID(kSelf);
    data.appendPackGUID(kSelf);
    data << uint8(3);                                 // m_cast_count: dropped
    data << uint32(52382);
    data << uint32(0x0000000A);
    data << uint32(777);                              // m_timer: dropped
    data << uint32(0);                                // m_casttime
    data << uint32(0x00000002);                       // TARGET_FLAG_UNIT
    data.appendPackGUID(kSelf);
    const size_t head = data.size();
    data << uint32(1234);                             // predicted power: after the target, kept as rest
    const std::string restFnv = Harness::Trace::Hex32(Harness::Trace::Fnv1a(data.contents() + head, 4));
    CHECK_STR(Decoded(SMSG_SPELL_START, data).c_str(),
              ("OP spell=52382 flags=0x0000000a source=self caster=self casttime=0 mask=0x2 target=self rest=4 restfnv=" + restFnv).c_str());
    // a different predicted power is a different record: the rest is not dropped
    WorldPacket other(data);
    other.put<uint32>(head, 1235);
    CHECK(Decoded(SMSG_SPELL_START, other) != Decoded(SMSG_SPELL_START, data));
}

TEST(HarnessTrace_spell_go_keeps_the_hit_and_miss_roles_and_drops_the_timestamp)
{
    // Spell::SendSpellGo + WriteSpellGoTargets: two hits (self, giver), one reflected miss.
    WorldPacket a(SMSG_SPELL_GO, 50);
    a.appendPackGUID(kSelf);
    a.appendPackGUID(kSelf);
    a << uint8(0);
    a << uint32(52382);
    a << uint32(0x00000100);
    a << uint32(0);                                   // m_timer
    a << uint32(123456);                              // GameTime::GetGameTimeMS(): dropped
    a << uint8(2);
    a << uint64(kSelf);
    a << uint64(kGiver);
    a << uint8(1);
    a << uint64(kOther);
    a << uint8(11);                                   // SPELL_MISS_REFLECT
    a << uint8(4);                                    // its result
    a << uint32(0);                                   // an empty target mask
    const std::string first = Decoded(SMSG_SPELL_GO, a);
    CHECK_STR(first.c_str(),
              "OP spell=52382 flags=0x00000100 source=self caster=self hit=[self,giver] miss=[other:11/4] mask=0x0 rest=0");
    // the same cast a moment later reads the same
    WorldPacket b(SMSG_SPELL_GO, 50);
    b.appendPackGUID(kSelf);
    b.appendPackGUID(kSelf);
    b << uint8(9);
    b << uint32(52382);
    b << uint32(0x00000100);
    b << uint32(0);
    b << uint32(999999);
    b << uint8(2);
    b << uint64(kSelf);
    b << uint64(kGiver);
    b << uint8(1);
    b << uint64(kOther);
    b << uint8(11);
    b << uint8(4);
    b << uint32(0);
    CHECK_STR(Decoded(SMSG_SPELL_GO, b).c_str(), first.c_str());
}

TEST(HarnessTrace_questgiver_status_multiple_reads_the_giver_as_a_role)
{
    // Player::SendQuestGiverStatusMultiple: the count, then (raw guid, status) per giver.
    WorldPacket a(SMSG_QUESTGIVER_STATUS_MULTIPLE, 4);
    a << uint32(0);
    a << uint64(kGiver);
    a << uint32(5);
    a.put<uint32>(0, 1);
    CHECK_STR(Decoded(SMSG_QUESTGIVER_STATUS_MULTIPLE, a).c_str(), "OP count=1 giver:5");
    // the giver the recorder names reads the same whatever number the counter gave it
    Harness::Trace::Roles moved = TestRoles();
    moved.giver = kOther;
    WorldPacket b(SMSG_QUESTGIVER_STATUS_MULTIPLE, 4);
    b << uint32(1);
    b << uint64(kOther);
    b << uint32(5);
    CHECK_STR(Harness::Trace::PacketRecord(SMSG_QUESTGIVER_STATUS_MULTIPLE, "OP", b.contents(), b.size(), false, moved).c_str(),
              "OP count=1 giver:5");
    WorldPacket none(SMSG_QUESTGIVER_STATUS_MULTIPLE, 4);
    none << uint32(0);
    CHECK_STR(Decoded(SMSG_QUESTGIVER_STATUS_MULTIPLE, none).c_str(), "OP count=0");
}

TEST(HarnessTrace_inventory_failure_reads_its_items_as_roles)
{
    // Player::SendEquipError(EQUIP_ERR_ITEM_NOT_FOUND, NULL, NULL, itemid): the result, two empty
    // item guids, the bag-type byte, and no tail for that result.
    WorldPacket a(SMSG_INVENTORY_CHANGE_FAILURE, 1 + 8 + 8 + 1);
    a << uint8(23);
    a << uint64(0);
    a << uint64(0);
    a << uint8(0);
    CHECK_STR(Decoded(SMSG_INVENTORY_CHANGE_FAILURE, a).c_str(),
              "OP result=23 item=none item2=none bag=0 tail=0 tailfnv=811c9dc5");
    // an item guid comes from the counter: it reads as "item", whichever number it had
    WorldPacket b(SMSG_INVENTORY_CHANGE_FAILURE, 1 + 8 + 8 + 1);
    b << uint8(1);
    b << uint64(0x4000000000001234ULL);
    b << uint64(0);
    b << uint8(0);
    b << uint32(20);                                  // EQUIP_ERR_CANT_EQUIP_LEVEL_I: the required level
    WorldPacket c(SMSG_INVENTORY_CHANGE_FAILURE, 1 + 8 + 8 + 1);
    c << uint8(1);
    c << uint64(0x4000000000005678ULL);
    c << uint64(0);
    c << uint8(0);
    c << uint32(20);
    CHECK_STR(Decoded(SMSG_INVENTORY_CHANGE_FAILURE, b).c_str(), Decoded(SMSG_INVENTORY_CHANGE_FAILURE, c).c_str());
    CHECK(Decoded(SMSG_INVENTORY_CHANGE_FAILURE, b).find("item=item item2=none bag=0 tail=4") != std::string::npos);
    // EQUIP_ERR_OK is the result byte alone
    WorldPacket ok(SMSG_INVENTORY_CHANGE_FAILURE, 1);
    ok << uint8(0);
    CHECK_STR(Decoded(SMSG_INVENTORY_CHANGE_FAILURE, ok).c_str(), "OP result=0");
}

TEST(HarnessTrace_hashed_sized_opcode_only_and_unhandled_records)
{
    // Player::SendQuestUpdateAddCreatureOrGo with the empty guid every harness credit passes
    WorldPacket kill(SMSG_QUESTUPDATE_ADD_KILL, 4 * 4 + 8);
    kill << uint32(52) << uint32(118) << uint32(3) << uint32(8) << uint64(0);
    const std::string rec = Harness::Trace::PacketRecord(SMSG_QUESTUPDATE_ADD_KILL, "SMSG_QUESTUPDATE_ADD_KILL",
                                                         kill.contents(), kill.size(), false, TestRoles());
    CHECK_STR(rec.c_str(), ("SMSG_QUESTUPDATE_ADD_KILL size=24 fnv=" +
                            Harness::Trace::Hex32(Harness::Trace::Fnv1a(kill.contents(), kill.size()))).c_str());
    WorldPacket chat(SMSG_MESSAGECHAT, 8);
    chat << uint32(1) << uint32(2);
    CHECK_STR(Harness::Trace::PacketRecord(SMSG_MESSAGECHAT, "SMSG_MESSAGECHAT", chat.contents(), chat.size(), false, TestRoles()).c_str(),
              "SMSG_MESSAGECHAT size=8");
    CHECK_STR(Harness::Trace::PacketRecord(SMSG_UPDATE_OBJECT, "SMSG_UPDATE_OBJECT", chat.contents(), chat.size(), false, TestRoles()).c_str(),
              "SMSG_UPDATE_OBJECT");
    CHECK_STR(Harness::Trace::PacketRecord(SMSG_ITEM_PUSH_RESULT, "X", chat.contents(), chat.size(), true, TestRoles()).c_str(),
              "X unhandled size=8");
}

TEST(HarnessTrace_the_line_and_the_digest)
{
    CHECK_STR(Harness::Trace::TraceLine("quest-kill-choice-reward", 7, "credit#3", "pkt SMSG_X size=4").c_str(),
              "MVTEST TRACE quest-kill-choice-reward 7 credit#3 pkt SMSG_X size=4");
    // the digest is FNV-1a over "<window> <text>\n" per line, the sequence number left out
    const uint32 one = Harness::Trace::DigestLine(Harness::Trace::kFnvOffset, "accept", "call CanAddQuest=1");
    CHECK_EQ(one, Harness::Trace::Fnv1a("accept call CanAddQuest=1\n", 26));
    const uint32 ab = Harness::Trace::DigestLine(one, "reward", "pkt B");
    const uint32 ba = Harness::Trace::DigestLine(Harness::Trace::DigestLine(Harness::Trace::kFnvOffset, "reward", "pkt B"),
                                                 "accept", "call CanAddQuest=1");
    CHECK(ab != ba);                                  // the order is part of the digest
    CHECK(Harness::Trace::DigestLine(Harness::Trace::kFnvOffset, "reward", "pkt B") !=
          Harness::Trace::DigestLine(Harness::Trace::kFnvOffset, "reward+tick", "pkt B"));   // and the window
}

// ---- decoupling D4f0-2: the decoder paths the six further scenarios added --------------------

namespace
{
    /// Player::SendNewItem, statement for statement: the player guid, received, created, shown,
    /// the bag, the slot (or -1 onto a stack), the entry, the suffix factor, the random property,
    /// the count pushed and the count held.
    WorldPacket ItemPush(uint32 entry, uint32 count, uint32 held)
    {
        WorldPacket data(SMSG_ITEM_PUSH_RESULT, (8 + 4 + 4 + 4 + 1 + 4 + 4 + 4 + 4 + 4));
        data << uint64(kSelf);
        data << uint32(1);
        data << uint32(0);
        data << uint32(1);
        data << uint8(255);
        data << uint32(23);
        data << uint32(entry);
        data << uint32(0);
        data << uint32(0);
        data << uint32(count);
        data << uint32(held);
        return data;
    }

    /// Spell::SendSpellGo's head for a self-cast with one hit and no miss, then
    /// SpellCastTargets::write's mask and, as the mask says, a unit target and an item target.
    WorldPacket SpellGoWithTargets(uint32 mask, uint64 unit, uint64 item, uint32 tail)
    {
        WorldPacket data(SMSG_SPELL_GO, 50);
        data.appendPackGUID(kSelf);
        data.appendPackGUID(kSelf);
        data << uint8(0);
        data << uint32(8690);
        data << uint32(0x00000100);
        data << uint32(0);
        data << uint32(4242);                         // GameTime::GetGameTimeMS(): dropped
        data << uint8(1);
        data << uint64(kSelf);
        data << uint8(0);
        data << uint32(mask);
        if (mask & 0x00000002)
        {
            data.appendPackGUID(unit);
        }
        if (mask & (0x00000010 | 0x00001000))
        {
            if (item)
            {
                data.appendPackGUID(item);
            }
            else
            {
                data << uint8(0);
            }
        }
        data << uint32(tail);                         // something after the targets: kept as rest
        return data;
    }

    /// SpellAuraHolder::SendAuraUpdate + BuildUpdatePacket for one aura.
    WorldPacket AuraUpdate(uint64 target, uint8 slot, uint32 spell, uint16 flags, uint64 caster, uint32 amount)
    {
        WorldPacket data(SMSG_AURA_UPDATE);
        data.appendPackGUID(target);
        data << uint8(slot);
        data << uint32(spell);
        data << uint16(flags);
        data << uint8(30);                            // the aura's level
        data << uint8(1);                             // the stack
        if (!(flags & 0x08))
        {
            data.appendPackGUID(caster);
        }
        if (flags & 0x40)
        {
            data << int32(amount);
        }
        return data;
    }
}

TEST(HarnessTrace_item_push_reads_the_entry_and_the_count)
{
    WorldPacket a = ItemPush(3297, 1, 4);
    uint32 entry = 0, count = 0;
    CHECK(Harness::Trace::ReadItemPush(a.contents(), a.size(), entry, count));
    CHECK_EQ(entry, 3297u);
    CHECK_EQ(count, 1u);
    // not SendNewItem's 45 bytes: refused, in either direction
    CHECK(!Harness::Trace::ReadItemPush(a.contents(), a.size() - 1, entry, count));
    WorldPacket longer(a);
    longer << uint8(0);
    CHECK(!Harness::Trace::ReadItemPush(longer.contents(), longer.size(), entry, count));
}

TEST(HarnessTrace_a_spell_item_target_reads_as_a_role_and_stays_out_of_the_rest)
{
    // An item's guid comes from the global item counter: two numbers, one record (the D4f0-1
    // final review's M-1) -- and whatever follows the item is still kept as the rest.
    const std::string a = Decoded(SMSG_SPELL_GO, SpellGoWithTargets(0x10, 0, 0x4000000000001234ULL, 77));
    const std::string b = Decoded(SMSG_SPELL_GO, SpellGoWithTargets(0x10, 0, 0x4000000000005678ULL, 77));
    CHECK_STR(a.c_str(), b.c_str());
    CHECK(a.find(" mask=0x10 itemTarget=item rest=4 restfnv=") != std::string::npos);
    CHECK(a != Decoded(SMSG_SPELL_GO, SpellGoWithTargets(0x10, 0, 0x4000000000001234ULL, 78)));
    // the zero byte SpellCastTargets::write puts for a missing item
    CHECK(Decoded(SMSG_SPELL_GO, SpellGoWithTargets(0x10, 0, 0, 77)).find(" itemTarget=none rest=4") != std::string::npos);
    // a unit and a trade item: the unit first, then the item
    CHECK(Decoded(SMSG_SPELL_GO, SpellGoWithTargets(0x1002, kGiver, 0x4000000000001234ULL, 77))
          .find(" mask=0x1002 target=giver itemTarget=item rest=4") != std::string::npos);
    // no item flag: nothing is read as an item
    CHECK(Decoded(SMSG_SPELL_GO, SpellGoWithTargets(0x2, kSelf, 0, 77)).find(" target=self rest=4 restfnv=") != std::string::npos);
}

TEST(HarnessTrace_aura_update_reads_target_and_caster_as_roles)
{
    // the target cast it (AFLAG_NOT_CASTER): no caster guid; three effect amounts become the rest
    WorldPacket own = AuraUpdate(kSelf, 7, 12852, 0x08 | 0x10 | 0x40 | 0x01, 0, 5);
    const std::string ownRecord = Decoded(SMSG_AURA_UPDATE, own);
    CHECK(ownRecord.find("OP target=self slot=7 spell=12852 flags=0x59 level=30 stack=1 rest=4 restfnv=") == 0);
    // a caster from the map's counter reads as its role, whatever number it was given
    WorldPacket byGiver = AuraUpdate(kSelf, 7, 12852, 0x10, kGiver, 0);
    CHECK_STR(Decoded(SMSG_AURA_UPDATE, byGiver).c_str(), "OP target=self slot=7 spell=12852 flags=0x10 level=30 stack=1 caster=giver rest=0");
    Harness::Trace::Roles moved = TestRoles();
    moved.giver = kOther;
    WorldPacket byMoved = AuraUpdate(kSelf, 7, 12852, 0x10, kOther, 0);
    CHECK_STR(Harness::Trace::PacketRecord(SMSG_AURA_UPDATE, "OP", byMoved.contents(), byMoved.size(), false, moved).c_str(),
              Decoded(SMSG_AURA_UPDATE, byGiver).c_str());
    // the removal form: the slot and a zero spell
    WorldPacket removed(SMSG_AURA_UPDATE);
    removed.appendPackGUID(kSelf);
    removed << uint8(7);
    removed << uint32(0);
    CHECK_STR(Decoded(SMSG_AURA_UPDATE, removed).c_str(), "OP target=self slot=7 spell=0");
    // a short head is refused, by the size alone
    CHECK(Decoded(SMSG_AURA_UPDATE, AuraUpdate(kSelf, 7, 12852, 0x10, kGiver, 0)).find("undecoded") == std::string::npos);
    WorldPacket cut(SMSG_AURA_UPDATE);
    cut.appendPackGUID(kSelf);
    cut << uint8(7) << uint32(12852) << uint8(0x10);
    CHECK(Decoded(SMSG_AURA_UPDATE, cut).find("undecoded size=") != std::string::npos);
}

TEST(HarnessTrace_the_rules_d4f0_2_added)
{
    using Harness::Trace::Rule;
    CHECK(Harness::Trace::RuleFor(SMSG_SET_FACTION_VISIBLE) == Rule::Hash);
    CHECK(Harness::Trace::RuleFor(SMSG_ACTION_BUTTONS) == Rule::Hash);
    CHECK(Harness::Trace::RuleFor(SMSG_REMOVED_SPELL) == Rule::Hash);
    CHECK(Harness::Trace::RuleFor(SMSG_SUPERCEDED_SPELL) == Rule::Hash);
    CHECK(Harness::Trace::RuleFor(SMSG_SET_FLAT_SPELL_MODIFIER) == Rule::Hash);
    CHECK(Harness::Trace::RuleFor(SMSG_SET_PCT_SPELL_MODIFIER) == Rule::Hash);
    CHECK(Harness::Trace::RuleFor(SMSG_AURA_UPDATE) == Rule::Decode);
}

TEST(HarnessClosure_models_exactly_the_types_of_its_one_table)
{
    // The achievement closure's table (QuestFixture.cpp kJudges) is the one list both the judge
    // and ClosureModelsCriteriaType read: these 30 types and nothing else.
    const uint32 modelled[] =
    {
        ACHIEVEMENT_CRITERIA_TYPE_COMPLETE_ACHIEVEMENT, ACHIEVEMENT_CRITERIA_TYPE_COMPLETE_QUEST,
        ACHIEVEMENT_CRITERIA_TYPE_COMPLETE_QUEST_COUNT, ACHIEVEMENT_CRITERIA_TYPE_COMPLETE_DAILY_QUEST,
        ACHIEVEMENT_CRITERIA_TYPE_COMPLETE_QUESTS_IN_ZONE, ACHIEVEMENT_CRITERIA_TYPE_MONEY_FROM_QUEST_REWARD,
        ACHIEVEMENT_CRITERIA_TYPE_OWN_ITEM, ACHIEVEMENT_CRITERIA_TYPE_EQUIP_ITEM, ACHIEVEMENT_CRITERIA_TYPE_EQUIP_EPIC_ITEM,
        ACHIEVEMENT_CRITERIA_TYPE_KILL_CREATURE, ACHIEVEMENT_CRITERIA_TYPE_GAIN_REPUTATION,
        ACHIEVEMENT_CRITERIA_TYPE_GAIN_EXALTED_REPUTATION, ACHIEVEMENT_CRITERIA_TYPE_REACH_LEVEL,
        ACHIEVEMENT_CRITERIA_TYPE_REACH_SKILL_LEVEL, ACHIEVEMENT_CRITERIA_TYPE_LEARN_SKILL_LEVEL,
        ACHIEVEMENT_CRITERIA_TYPE_LEARN_SKILLLINE_SPELLS, ACHIEVEMENT_CRITERIA_TYPE_LEARN_SKILL_LINE,
        ACHIEVEMENT_CRITERIA_TYPE_LEARN_SPELL, ACHIEVEMENT_CRITERIA_TYPE_CAST_SPELL, ACHIEVEMENT_CRITERIA_TYPE_CAST_SPELL2,
        ACHIEVEMENT_CRITERIA_TYPE_BE_SPELL_TARGET, ACHIEVEMENT_CRITERIA_TYPE_BE_SPELL_TARGET2,
        ACHIEVEMENT_CRITERIA_TYPE_CURRENCY_EARNED, ACHIEVEMENT_CRITERIA_TYPE_KNOWN_FACTIONS,
        ACHIEVEMENT_CRITERIA_TYPE_GAIN_REVERED_REPUTATION, ACHIEVEMENT_CRITERIA_TYPE_GAIN_HONORED_REPUTATION,
        ACHIEVEMENT_CRITERIA_TYPE_HIGHEST_GOLD_VALUE_OWNED, ACHIEVEMENT_CRITERIA_TYPE_RECEIVE_EPIC_ITEM,
        // decoupling D11: the damage the spell family's casts deal
        ACHIEVEMENT_CRITERIA_TYPE_DAMAGE_DONE, ACHIEVEMENT_CRITERIA_TYPE_HIGHEST_HIT_DEALT,
    };
    const uint32 count = sizeof(modelled) / sizeof(modelled[0]);
    CHECK_EQ(count, 30u);
    for (uint32 i = 0; i < count; ++i)
    {
        CHECK(Harness::ClosureModelsCriteriaType(modelled[i]));
    }
    uint32 yes = 0;
    for (uint32 type = 0; type < ACHIEVEMENT_CRITERIA_TYPE_TOTAL + 10; ++type)
    {
        yes += Harness::ClosureModelsCriteriaType(type) ? 1 : 0;
    }
    CHECK_EQ(yes, count);
    // types the mail-reward trees hold and the closure does not model (the D4f0-1 task review's M-2)
    CHECK(!Harness::ClosureModelsCriteriaType(ACHIEVEMENT_CRITERIA_TYPE_LOOT_ITEM));
}

TEST(HarnessTrace_a_snap_line_is_due_only_when_the_state_changed)
{
    // Ruling 19: `last` is the snapshot taken when the window opened, or the one printed last.
    std::string last = "q52=3/0/0x0 m=0 xp=0 l=1";
    CHECK(!Harness::Trace::SnapDue(last, "q52=3/0/0x0 m=0 xp=0 l=1"));   // a packet that moved nothing tracked
    CHECK(Harness::Trace::SnapDue(last, "q52=3/0/- m=0 xp=0 l=1"));      // the slot cleared: a line
    CHECK_STR(last.c_str(), "q52=3/0/- m=0 xp=0 l=1");                  // ... and it is the new reference
    CHECK(!Harness::Trace::SnapDue(last, "q52=3/0/- m=0 xp=0 l=1"));
    CHECK(Harness::Trace::SnapDue(last, "q52=3/0/0x0 m=0 xp=0 l=1"));    // flipped back: a line again
}

// ---- decoupling D11 PR 1: the spell family's decoders and the state snapshot -------------------
//
// Each payload is laid out by its production writer's own statements, cited above each helper, at
// 7b6a481ce. The spell family watches its units by role: the player is `self`, the unit a cast is
// aimed at `target`, a unit other than the player that casts `caster`.

namespace
{
    const uint64 kTarget = 0xF130000B91000042ULL;  // a spawned creature (2961) from the map's counter
    const uint64 kCaster = 0xF130000B91000077ULL;

    Harness::Trace::Roles SpellRoles()
    {
        Harness::Trace::Roles r;
        r.self = kSelf;
        r.target = kTarget;
        r.caster = kCaster;
        return r;
    }

    std::string SpellDecoded(uint16 opcode, WorldPacket const& p, Harness::Trace::Roles const& roles = SpellRoles())
    {
        return Harness::Trace::PacketRecord(opcode, "OP", p.contents(), p.size(), false, roles);
    }

    /// Unit::SendSpellNonMeleeDamageLog(SpellNonMeleeDamage*), Unit.cpp:2617-2631, statement for
    /// statement, every field given.
    WorldPacket DamageLogOf(uint64 target, uint64 attacker, uint32 spell, uint32 damage, uint32 overkill, uint8 school,
                            uint32 absorb, uint32 resist, uint8 physical, uint8 unused, uint32 blocked, uint32 hitInfo,
                            uint8 extend)
    {
        WorldPacket data(SMSG_SPELLNONMELEEDAMAGELOG, (16 + 4 + 4 + 4 + 1 + 4 + 4 + 1 + 1 + 4 + 4 + 1));
        data.appendPackGUID(target);                  // log->target->GetPackGUID()
        data.appendPackGUID(attacker);                // log->attacker->GetPackGUID()
        data << uint32(spell);
        data << uint32(damage);
        data << uint32(overkill);
        data << uint8(school);                        // log->schoolMask
        data << uint32(absorb);
        data << uint32(resist);
        data << uint8(physical);                      // physicalLog
        data << uint8(unused);
        data << uint32(blocked);
        data << uint32(hitInfo);
        data << uint8(extend);                        // the extend-data flag
        return data;
    }

    /// A holy hit with 3 resisted, the rest of the fields 0 as the server sends them.
    WorldPacket DamageLog(uint64 target, uint64 attacker, uint32 spell, uint32 damage, uint32 overkill, uint32 hitInfo)
    {
        return DamageLogOf(target, attacker, spell, damage, overkill, 2, 0, 3, 0, 0, 0, hitInfo, 0);
    }

    /// Unit::SendSpellMiss, Unit.cpp:2750-2758: the spell, the caster's raw guid, a flag byte, the
    /// target count (always 1 there), then the target's raw guid and the miss condition.
    WorldPacket MissLog(uint32 spell, uint64 caster, uint64 target, uint8 missInfo)
    {
        WorldPacket data(SMSG_SPELLLOGMISS, (4 + 8 + 1 + 4 + 8 + 1));
        data << uint32(spell);
        data << uint64(caster);                       // GetObjectGuid()
        data << uint8(0);
        data << uint32(1);
        data << uint64(target);                       // target->GetObjectGuid()
        data << uint8(missInfo);
        return data;
    }

    /// Unit::SetPower, UnitPower.cpp:154-160: the unit's packed guid, the count, then (type, value).
    WorldPacket PowerUpdate(uint64 unit, uint8 power, uint32 value)
    {
        WorldPacket data(SMSG_POWER_UPDATE);
        data.appendPackGUID(unit);                    // GetPackGUID()
        data << uint32(1);
        data << uint8(power);
        data << uint32(value);
        return data;
    }

    /// Spell::SendInterrupted, SpellPackets.cpp:749-760: one layout for both opcodes.
    WorldPacket Interrupted(uint16 opcode, uint64 caster, uint8 castCount, uint32 spell, uint8 result)
    {
        WorldPacket data(opcode, (8 + 4 + 1));
        data.appendPackGUID(caster);                  // m_caster->GetPackGUID()
        data << uint8(castCount);
        data << uint32(spell);
        data << uint8(result);
        return data;
    }
}

TEST(HarnessTrace_the_rules_d11_added)
{
    using Harness::Trace::Rule;
    CHECK(Harness::Trace::RuleFor(SMSG_SPELLNONMELEEDAMAGELOG) == Rule::Decode);
    CHECK(Harness::Trace::RuleFor(SMSG_SPELLLOGMISS) == Rule::Decode);
    CHECK(Harness::Trace::RuleFor(SMSG_POWER_UPDATE) == Rule::Decode);
    CHECK(Harness::Trace::RuleFor(SMSG_SPELL_FAILURE) == Rule::Decode);
    CHECK(Harness::Trace::RuleFor(SMSG_SPELL_FAILED_OTHER) == Rule::Decode);
    CHECK(Harness::Trace::RuleFor(SMSG_CAST_FAILED) == Rule::Decode);
    // the energize and periodic logs come with the scenarios that send them (931-938)
    CHECK(Harness::Trace::RuleFor(SMSG_SPELLENERGIZELOG) == Rule::Size);
    CHECK(Harness::Trace::RuleFor(SMSG_PERIODICAURALOG) == Rule::Size);
}

TEST(HarnessTrace_the_spell_roles_read_after_the_quest_roles)
{
    Harness::Trace::Roles r = SpellRoles();
    CHECK_STR(Harness::Trace::RoleOf(r, kSelf), "self");
    CHECK_STR(Harness::Trace::RoleOf(r, kTarget), "target");
    CHECK_STR(Harness::Trace::RoleOf(r, kCaster), "caster");
    CHECK_STR(Harness::Trace::RoleOf(r, kGiver), "other");
    CHECK_STR(Harness::Trace::RoleOf(r, 0), "none");
    // an unset role never claims a guid: the quest family's roles read exactly as before
    CHECK_STR(Harness::Trace::RoleOf(TestRoles(), kTarget), "other");
    CHECK_STR(Harness::Trace::RoleOf(TestRoles(), kGiver), "giver");
}

TEST(HarnessTrace_the_damage_log_keeps_every_field_with_its_units_as_roles)
{
    WorldPacket a = DamageLog(kTarget, kSelf, 585, 17, 0, 0x00000024);
    CHECK_STR(SpellDecoded(SMSG_SPELLNONMELEEDAMAGELOG, a).c_str(),
              "OP target=target attacker=self spell=585 damage=17 overkill=0 school=2 absorb=0 resist=3 physical=0 unused=0 blocked=0 hitInfo=0x24 extend=0");
    // the target reads the same whatever number the map's counter gave it
    Harness::Trace::Roles moved = SpellRoles();
    moved.target = 0xF130000B91000999ULL;
    WorldPacket b = DamageLog(moved.target, kSelf, 585, 17, 0, 0x00000024);
    CHECK_STR(SpellDecoded(SMSG_SPELLNONMELEEDAMAGELOG, b, moved).c_str(), SpellDecoded(SMSG_SPELLNONMELEEDAMAGELOG, a).c_str());
    // a critical hit (SPELL_HIT_TYPE_CRIT, 0x2) and another amount are other records
    CHECK(SpellDecoded(SMSG_SPELLNONMELEEDAMAGELOG, DamageLog(kTarget, kSelf, 585, 17, 0, 0x26)) != SpellDecoded(SMSG_SPELLNONMELEEDAMAGELOG, a));
    CHECK(SpellDecoded(SMSG_SPELLNONMELEEDAMAGELOG, DamageLog(kTarget, kSelf, 585, 18, 0, 0x24)) != SpellDecoded(SMSG_SPELLNONMELEEDAMAGELOG, a));
    // the reader the scenario uses for its categories
    uint64 target = 0, attacker = 0;
    uint32 spell = 0, damage = 0, overkill = 0;
    CHECK(Harness::Trace::ReadSpellDamage(a.contents(), a.size(), target, attacker, spell, damage, overkill));
    CHECK(target == kTarget);
    CHECK(attacker == kSelf);
    CHECK_EQ(spell, 585u);
    CHECK_EQ(damage, 17u);
    CHECK_EQ(overkill, 0u);
    // every field its own non-zero value, so no field can be read in another's place: while
    // absorb, physical, unused, blocked and extend were all 0, a decoder reading absorb and
    // blocked swapped still passed (the D11-1 task review's M-3)
    WorldPacket all = DamageLogOf(kTarget, kSelf, 585, 17, 11, 2, 261, 3, 1, 6, 519, 0x24, 9);
    CHECK_STR(SpellDecoded(SMSG_SPELLNONMELEEDAMAGELOG, all).c_str(),
              "OP target=target attacker=self spell=585 damage=17 overkill=11 school=2 absorb=261 resist=3 physical=1 unused=6 blocked=519 hitInfo=0x24 extend=9");
    CHECK(Harness::Trace::ReadSpellDamage(all.contents(), all.size(), target, attacker, spell, damage, overkill));
    CHECK_EQ(damage, 17u);
    CHECK_EQ(overkill, 11u);
    // one byte short or long: refused, by the size alone
    std::string out;
    CHECK(!Harness::Trace::DecodeSpellDamageLog(a.contents(), a.size() - 1, SpellRoles(), out));
    WorldPacket longer(a);
    longer << uint8(0);
    CHECK(!Harness::Trace::DecodeSpellDamageLog(longer.contents(), longer.size(), SpellRoles(), out));
    CHECK(!Harness::Trace::ReadSpellDamage(longer.contents(), longer.size(), target, attacker, spell, damage, overkill));
    CHECK(SpellDecoded(SMSG_SPELLNONMELEEDAMAGELOG, longer).find("OP undecoded size=") == 0);
}

TEST(HarnessTrace_the_miss_log_reads_caster_and_target_as_roles)
{
    WorldPacket a = MissLog(585, kSelf, kTarget, 2);   // SPELL_MISS_RESIST
    CHECK_STR(SpellDecoded(SMSG_SPELLLOGMISS, a).c_str(), "OP spell=585 caster=self flag=0 count=1 targets=[target:2]");
    WorldPacket b = MissLog(585, kCaster, kSelf, 1);   // a creature's miss on the player
    CHECK_STR(SpellDecoded(SMSG_SPELLLOGMISS, b).c_str(), "OP spell=585 caster=caster flag=0 count=1 targets=[self:1]");
    std::string out;
    CHECK(!Harness::Trace::DecodeSpellLogMiss(a.contents(), a.size() - 1, SpellRoles(), out));
    WorldPacket longer(a);
    longer << uint8(0);
    CHECK(!Harness::Trace::DecodeSpellLogMiss(longer.contents(), longer.size(), SpellRoles(), out));
}

TEST(HarnessTrace_the_power_update_reads_its_unit_as_a_role)
{
    WorldPacket a = PowerUpdate(kSelf, 0, 43);        // POWER_MANA
    CHECK_STR(SpellDecoded(SMSG_POWER_UPDATE, a).c_str(), "OP unit=self count=1 power0=43");
    CHECK_STR(SpellDecoded(SMSG_POWER_UPDATE, PowerUpdate(kTarget, 1, 0)).c_str(), "OP unit=target count=1 power1=0");
    uint64 unit = 0;
    uint8 power = 9;
    uint32 value = 0;
    CHECK(Harness::Trace::ReadPowerUpdate(a.contents(), a.size(), unit, power, value));
    CHECK(unit == kSelf);
    CHECK_EQ(uint32(power), 0u);
    CHECK_EQ(value, 43u);
    std::string out;
    CHECK(!Harness::Trace::DecodePowerUpdate(a.contents(), a.size() - 1, SpellRoles(), out));
    WorldPacket longer(a);
    longer << uint8(0);
    CHECK(!Harness::Trace::DecodePowerUpdate(longer.contents(), longer.size(), SpellRoles(), out));
    CHECK(!Harness::Trace::ReadPowerUpdate(longer.contents(), longer.size(), unit, power, value));
    // a count of zero names no power: the reader refuses it, the decoder records it
    WorldPacket none(SMSG_POWER_UPDATE);
    none.appendPackGUID(kSelf);
    none << uint32(0);
    CHECK(!Harness::Trace::ReadPowerUpdate(none.contents(), none.size(), unit, power, value));
    CHECK_STR(SpellDecoded(SMSG_POWER_UPDATE, none).c_str(), "OP unit=self count=0");
}

TEST(HarnessTrace_the_failure_packets_keep_spell_and_result_and_drop_the_cast_count)
{
    WorldPacket a = Interrupted(SMSG_SPELL_FAILURE, kSelf, 1, 585, 38);   // SPELL_FAILED_INTERRUPTED
    CHECK_STR(SpellDecoded(SMSG_SPELL_FAILURE, a).c_str(), "OP caster=self spell=585 result=38");
    WorldPacket b = Interrupted(SMSG_SPELL_FAILED_OTHER, kSelf, 7, 585, 38);
    CHECK_STR(SpellDecoded(SMSG_SPELL_FAILED_OTHER, b).c_str(), "OP caster=self spell=585 result=38");
    std::string out;
    CHECK(!Harness::Trace::DecodeSpellFailure(a.contents(), a.size() - 1, SpellRoles(), out));
    WorldPacket longer(a);
    longer << uint8(0);
    CHECK(!Harness::Trace::DecodeSpellFailure(longer.contents(), longer.size(), SpellRoles(), out));

    // Spell::SendCastResult, SpellPackets.cpp:121-221: the cast count, the spell, the result, then
    // the result's own tail -- none for most results, a word for SPELL_FAILED_NOT_READY
    WorldPacket plain(SMSG_CAST_FAILED, (4 + 1 + 2));
    plain << uint8(1);
    plain << uint32(585);
    plain << uint8(92);
    CHECK_STR(SpellDecoded(SMSG_CAST_FAILED, plain).c_str(), "OP spell=585 result=92 tail=0");
    WorldPacket notReady(SMSG_CAST_FAILED, (4 + 1 + 2));
    notReady << uint8(2);
    notReady << uint32(585);
    notReady << uint8(143);
    notReady << uint32(0);
    CHECK_STR(SpellDecoded(SMSG_CAST_FAILED, notReady).c_str(),
              ("OP spell=585 result=143 tail=4 tailfnv=" + Harness::Trace::Hex32(Harness::Trace::Fnv1a(notReady.contents() + 6, 4))).c_str());
    WorldPacket cut(SMSG_CAST_FAILED, 4);
    cut << uint8(1) << uint32(585);
    CHECK(SpellDecoded(SMSG_CAST_FAILED, cut).find("OP undecoded size=5") == 0);
}

TEST(HarnessTrace_the_state_delta_prints_values_in_key_order_then_sets_in_list_order)
{
    // The recorder's window close (Recorder::CloseWindow), as the quest family has always printed it.
    Harness::Trace::StateSnapshot before, after;
    before.values["money"] = "0";
    before.values["level"] = "1";
    before.values["gone"] = "x";
    before.sets.push_back(std::make_pair(std::string("spells"), std::set<uint32>{ 1, 2 }));
    before.sets.push_back(std::make_pair(std::string("achievements"), std::set<uint32>()));
    after.values["money"] = "250";
    after.values["level"] = "1";
    after.values["added"] = "y";
    after.sets.push_back(std::make_pair(std::string("spells"), std::set<uint32>{ 2, 3 }));
    after.sets.push_back(std::make_pair(std::string("achievements"), std::set<uint32>{ 6 }));
    const std::vector<std::string> lines = Harness::Trace::StateDelta(before, after);
    CHECK_EQ(uint32(lines.size()), 5u);
    CHECK_STR(lines[0].c_str(), "state added -->y");
    CHECK_STR(lines[1].c_str(), "state gone x->-");
    CHECK_STR(lines[2].c_str(), "state money 0->250");
    CHECK_STR(lines[3].c_str(), "state spells +3 -1");
    CHECK_STR(lines[4].c_str(), "state achievements +6");
    // nothing moved: nothing printed
    CHECK_EQ(uint32(Harness::Trace::StateDelta(after, after).size()), 0u);
    // from nothing (the spawn window): every value, and every non-empty set, in that order
    const std::vector<std::string> spawn = Harness::Trace::StateDelta(Harness::Trace::StateSnapshot(), after);
    CHECK_EQ(uint32(spawn.size()), 5u);
    CHECK_STR(spawn[3].c_str(), "state spells +2 +3");
    // a set the new state no longer lists reads as emptied, after the listed ones
    Harness::Trace::StateSnapshot dropped;
    dropped.sets.push_back(std::make_pair(std::string("achievements"), std::set<uint32>{ 6 }));
    const std::vector<std::string> gone = Harness::Trace::StateDelta(after, dropped);
    CHECK_STR(gone.back().c_str(), "state spells -2 -3");
    CHECK_STR(Harness::Trace::SetDelta(std::set<uint32>{ 5 }, std::set<uint32>{ 5 }).c_str(), "");
}

TEST(HarnessTrace_the_spell_snapshot_writes_a_holder_without_a_guid_or_a_clock)
{
    // SpellRecorder's `<role>.aura.<spell>#<k>` value: the caster by its role, the durations in
    // the stepped world's milliseconds (-1 for a permanent aura).
    CHECK_STR(Harness::Trace::AuraHolderValue(0x1, 1, 0, "self", 255, -1, -1).c_str(),
              "eff=0x1 stack=1 charges=0 caster=self slot=255 dur=-1/-1");
    CHECK_STR(Harness::Trace::AuraHolderValue(0x3, 2, 1, Harness::Trace::RoleOf(SpellRoles(), kCaster), 4, 4900, 5000).c_str(),
              "eff=0x3 stack=2 charges=1 caster=caster slot=4 dur=4900/5000");
    // a cooldown key set, as the recorder stores the manager's map keys
    Harness::Trace::StateSnapshot a, b;
    a.sets.push_back(std::make_pair(std::string("self.cooldowns"), std::set<uint32>()));
    b.sets.push_back(std::make_pair(std::string("self.cooldowns"), std::set<uint32>{ 93002 }));
    CHECK_STR(Harness::Trace::StateDelta(a, b)[0].c_str(), "state self.cooldowns +93002");
}
