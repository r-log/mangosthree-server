// The allowed-mover set (design v2 §7, F1): which units one client may move and
// the one it has selected. Pure; the session owns one and the handlers ask it.
#include "TestHarness.h"
#include "Authority.h"

using namespace Motion;

namespace
{
    const uint64 kPlayer  = 0x0400000000000001ULL;
    const uint64 kVehicle = 0xF150000000000002ULL;
    const uint64 kOther   = 0x0400000000000003ULL;
}

TEST(MotionAuthority_starts_empty_and_lets_nothing_move_or_ack)
{
    Authority a;
    CHECK(a.Members().empty());
    CHECK_EQ(a.Selected(), uint64(0));
    CHECK(!a.MovesAs(kPlayer));
    CHECK(!a.MayAck(kPlayer));
    CHECK_EQ(a.Counters().notActive, 1u);
    CHECK_EQ(a.Counters().notMember, 1u);
}

TEST(MotionAuthority_add_makes_a_member_and_selects_it)
{
    Authority a;
    a.Add(kPlayer);
    CHECK(a.IsMember(kPlayer));
    CHECK_EQ(a.Selected(), kPlayer);
    CHECK_EQ(a.Members().size(), size_t(1));
    CHECK_EQ(a.Counters().added, 1u);
    // A second member is added and becomes the selection; the first stays a member.
    a.Add(kVehicle);
    CHECK(a.IsMember(kPlayer));
    CHECK(a.IsMember(kVehicle));
    CHECK_EQ(a.Selected(), kVehicle);
    CHECK_EQ(a.Members().size(), size_t(2));
    CHECK_EQ(a.Members()[0], kPlayer);
    CHECK_EQ(a.Members()[1], kVehicle);
    CHECK_EQ(a.Counters().added, 2u);
}

TEST(MotionAuthority_adding_a_member_again_only_reselects_it)
{
    Authority a;
    a.Add(kPlayer);
    a.Add(kVehicle);
    a.Add(kPlayer);
    CHECK_EQ(a.Members().size(), size_t(2));
    CHECK_EQ(a.Selected(), kPlayer);
    CHECK_EQ(a.Counters().added, 2u);
}

TEST(MotionAuthority_remove_of_the_selected_member_clears_the_selection)
{
    Authority a;
    a.Add(kPlayer);
    a.Add(kVehicle);
    CHECK(a.Remove(kVehicle));
    CHECK(!a.IsMember(kVehicle));
    CHECK(a.IsMember(kPlayer));
    CHECK_EQ(a.Selected(), uint64(0));
    CHECK_EQ(a.Counters().removed, 1u);
    CHECK(!a.MovesAs(kPlayer));   // a member, not selected
}

TEST(MotionAuthority_remove_of_another_member_keeps_the_selection_and_of_a_stranger_counts_nothing)
{
    Authority a;
    a.Add(kPlayer);
    a.Add(kVehicle);
    a.Select(kPlayer);
    CHECK(a.Remove(kVehicle));
    CHECK_EQ(a.Selected(), kPlayer);
    CHECK(!a.Remove(kOther));
    CHECK_EQ(a.Counters().removed, 1u);
}

TEST(MotionAuthority_select_takes_a_member_and_refuses_a_stranger)
{
    Authority a;
    a.Add(kPlayer);
    a.Add(kVehicle);
    CHECK(a.Select(kPlayer));
    CHECK_EQ(a.Selected(), kPlayer);
    CHECK_EQ(a.Counters().selected, 1u);
    CHECK(!a.Select(kOther));
    CHECK_EQ(a.Selected(), kPlayer);
    CHECK_EQ(a.Counters().badSelect, 1u);
}

TEST(MotionAuthority_deselect_clears_the_selected_guid_only)
{
    Authority a;
    a.Add(kPlayer);
    a.Add(kVehicle);
    CHECK(!a.Deselect(kPlayer));   // a member, not the selected one
    CHECK_EQ(a.Counters().badDeselect, 1u);
    CHECK_EQ(a.Selected(), kVehicle);
    CHECK(a.Deselect(kVehicle));
    CHECK_EQ(a.Selected(), uint64(0));
    CHECK_EQ(a.Counters().deselected, 1u);
    CHECK(!a.Deselect(kVehicle));
    CHECK_EQ(a.Counters().badDeselect, 2u);
}

TEST(MotionAuthority_movement_needs_the_selected_unit_and_an_ack_needs_membership)
{
    Authority a;
    a.Add(kPlayer);
    a.Add(kVehicle);
    CHECK(a.MovesAs(kVehicle));
    CHECK(!a.MovesAs(kPlayer));
    CHECK(!a.MovesAs(kOther));
    CHECK_EQ(a.Counters().notActive, 2u);
    CHECK(a.MayAck(kPlayer));
    CHECK(a.MayAck(kVehicle));
    CHECK(!a.MayAck(kOther));
    CHECK_EQ(a.Counters().notMember, 1u);
}

TEST(MotionAuthority_clear_removes_every_member_and_unresolved_counts)
{
    Authority a;
    a.Add(kPlayer);
    a.Add(kVehicle);
    a.Unresolved();
    a.Clear();
    CHECK(a.Members().empty());
    CHECK_EQ(a.Selected(), uint64(0));
    CHECK_EQ(a.Counters().removed, 2u);
    CHECK_EQ(a.Counters().unresolved, 1u);
}
