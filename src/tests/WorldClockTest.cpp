// P0-D: the process clock behind getMSTime(). Real mode reads the steady clock; a
// stepped run moves the counter by hand; leaving keeps an offset so time never runs
// backwards. Nothing here touches a map or the world.
#include "TestHarness.h"
#include "WorldClock.h"
#include "Timer.h"

#include <chrono>
#include <thread>

TEST(WorldClock_stepped_time_moves_only_by_Step)
{
    const uint32 before = WorldClock::NowMs();
    WorldClock::EnterStepped();
    const uint32 start = WorldClock::NowMs();
    CHECK(start >= before);
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    CHECK_EQ(WorldClock::NowMs(), start);                 // real time passed, the counter did not
    WorldClock::Step(50);
    WorldClock::Step(50);
    WorldClock::Step(50);
    CHECK_EQ(WorldClock::NowMs(), start + 150);
    CHECK_EQ(getMSTime(), start + 150);                   // getMSTime is the same source
    CHECK(WorldClock::IsStepped());
    WorldClock::LeaveStepped();
    CHECK(!WorldClock::IsStepped());
}

TEST(WorldClock_leaving_never_runs_backwards)
{
    WorldClock::EnterStepped();
    const uint32 start = WorldClock::NowMs();
    for (int i = 0; i < 200; ++i)                         // ten virtual seconds in no real time
    {
        WorldClock::Step(50);
    }
    const uint32 virt = WorldClock::NowMs();
    CHECK_EQ(virt, start + 10000);
    WorldClock::LeaveStepped();
    const uint32 after = WorldClock::NowMs();
    CHECK(after >= virt);                                 // the offset carries the counter's lead
    CHECK(WorldClock::OffsetMs() >= 9000);                // ten seconds minus whatever the loop took
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    CHECK(WorldClock::NowMs() >= after);
}

TEST(WorldClock_unix_seconds_follow_the_milliseconds)
{
    WorldClock::EnterStepped();
    const time_t s0 = WorldClock::NowUnix();
    const uint32 m0 = WorldClock::NowMs();
    for (int i = 0; i < 60; ++i)                          // three virtual seconds
    {
        WorldClock::Step(50);
    }
    const time_t s1 = WorldClock::NowUnix();
    const uint32 m1 = WorldClock::NowMs();
    CHECK_EQ(uint32(s1 - s0), (m1 / 1000) - (m0 / 1000));
    WorldClock::LeaveStepped();
}
