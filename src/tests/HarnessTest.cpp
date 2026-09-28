// The GM harness's pure parts (movement P0-C): the step timeline every scenario
// runs on, the verdict line, and the rule the teardown classifies an owned player by;
// and (decoupling D4f0) the reward recorder's rule table, decoders, line and digest.
// Nothing here touches a map.
#include "TestHarness.h"
#include "Timeline.h"
#include "Ownership.h"
#include "Trace.h"
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
