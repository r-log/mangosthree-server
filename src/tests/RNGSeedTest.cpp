// P0-D: the random generator behind urand can be seeded; the harness seeds it per scenario.
#include "TestHarness.h"
#include "RNGen.h"
#include "Util.h"

#include <vector>

TEST(RNG_seed_reproduces_the_sequence)
{
    std::vector<uint32> first;
    RNG::Seed(7);
    for (int i = 0; i < 16; ++i)
    {
        first.push_back(urand(0, 1000000));
    }
    std::vector<uint32> second;
    RNG::Seed(7);
    for (int i = 0; i < 16; ++i)
    {
        second.push_back(urand(0, 1000000));
    }
    CHECK(first == second);

    std::vector<uint32> other;
    RNG::Seed(8);
    for (int i = 0; i < 16; ++i)
    {
        other.push_back(urand(0, 1000000));
    }
    CHECK(first != other);
}

namespace
{
    uint32 IndexDraw(size_t size)
    {
        return urand(0, size - 1);
    }

    bool DrawsExactly(std::vector<uint32> const& draws, uint32 values)
    {
        std::vector<bool> seen(values, false);
        for (size_t i = 0; i < draws.size(); ++i)
        {
            if (draws[i] >= values)
            {
                return false;
            }
            seen[draws[i]] = true;
        }
        for (uint32 v = 0; v < values; ++v)
        {
            if (!seen[v])
            {
                return false;
            }
        }
        return true;
    }
}

TEST(RNG_index_draw_covers_zero_to_size_minus_one)
{
    RNG::Seed(11);
    for (size_t size = 1; size <= 5; ++size)
    {
        std::vector<uint32> draws;
        for (int i = 0; i < 2000; ++i)
        {
            draws.push_back(IndexDraw(size));
        }
        CHECK(DrawsExactly(draws, uint32(size)));
    }
}

TEST(RNG_switch_draws_cover_their_cases)
{
    RNG::Seed(12);
    std::vector<uint32> two, three, four;
    for (int i = 0; i < 2000; ++i)
    {
        two.push_back(urand(0, 1));
        three.push_back(urand(0, 2));
        four.push_back(urand(0, 3));
    }
    CHECK(DrawsExactly(two, 2));
    CHECK(DrawsExactly(three, 3));
    CHECK(DrawsExactly(four, 4));
}

TEST(RNG_event_word_leaves_chance_and_pick_untied)
{
    RNG::Seed(13);
    std::vector<bool> pairs(300, false);
    for (int i = 0; i < 30000; ++i)
    {
        uint32 rnd = rand32();
        pairs[(rnd % 100) * 3 + rnd % 3] = true;
    }
    uint32 seen = 0;
    for (size_t i = 0; i < pairs.size(); ++i)
    {
        seen += pairs[i] ? 1 : 0;
    }
    CHECK_EQ(seen, 300u);
}

TEST(RNG_seed_reproduces_the_site_draws)
{
    std::vector<uint32> first, second;
    RNG::Seed(14);
    for (int i = 0; i < 16; ++i)
    {
        first.push_back(rand32());
        first.push_back(urand(0, 3));
        first.push_back(IndexDraw(7));
    }
    RNG::Seed(14);
    for (int i = 0; i < 16; ++i)
    {
        second.push_back(rand32());
        second.push_back(urand(0, 3));
        second.push_back(IndexDraw(7));
    }
    CHECK(first == second);
}
