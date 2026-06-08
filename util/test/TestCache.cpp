#include <gtest/gtest.h>

#include <sihd/util/Cache.hpp>
#include <sihd/util/Logger.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;
using namespace sihd::util;
class TestCache: public ::testing::Test
{
    protected:
        TestCache() { sihd::util::LoggerManager::stream(); }

        virtual ~TestCache() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestCache, test_override_ttl)
{
    struct StructTest
    {
            int a = 0;
            std::string b;
    };

    Cache<int, StructTest> cache;

    auto cache_function = []() -> StructTest {
        static int counter = 0;
        return StructTest {.a = ++counter, .b = "test"};
    };

    constexpr bool lazy = false;
    cache.set(1, cache_function, std::chrono::seconds(1), lazy);

    auto stats = cache.entry_stats(1);
    ASSERT_TRUE(stats);
    EXPECT_EQ(stats->hit, 0u);
    EXPECT_EQ(stats->miss, 0u);

    auto & value1 = cache.get(1);
    EXPECT_EQ(value1.a, 1);
    EXPECT_EQ(value1.b, "test");

    stats = cache.entry_stats(1);
    ASSERT_TRUE(stats);
    EXPECT_EQ(stats->hit, 1u);
    EXPECT_EQ(stats->miss, 0u);

    constexpr std::chrono::nanoseconds override_data_cache_duration_ns(500);
    auto & value2 = cache.get(1, override_data_cache_duration_ns);
    EXPECT_EQ(value2.a, 2);
    EXPECT_EQ(value2.b, "test");

    stats = cache.entry_stats(1);
    ASSERT_TRUE(stats);
    EXPECT_EQ(stats->hit, 1u);
    EXPECT_EQ(stats->miss, 1u);
}

TEST_F(TestCache, test_lazy)
{
    Cache<std::string, int> cache;

    auto cache_function = []() -> int {
        static int counter = 0;
        return ++counter;
    };

    constexpr bool lazy = true;
    cache.set("one", cache_function, std::chrono::nanoseconds(1), lazy);

    auto stats = cache.entry_stats("one");
    ASSERT_TRUE(stats);
    EXPECT_EQ(stats->hit, 0u);
    EXPECT_EQ(stats->miss, 0u);

    auto & value1 = cache.get("one");
    EXPECT_EQ(value1, 1);

    stats = cache.entry_stats("one");
    ASSERT_TRUE(stats);
    EXPECT_EQ(stats->hit, 0u);
    EXPECT_EQ(stats->miss, 1u);

    EXPECT_EQ(cache.get("one"), 2);
}

TEST_F(TestCache, test_get_optional)
{
    Cache<int, int> cache;
    int counter = 0;
    cache.set(1, [&] { return ++counter; }, std::chrono::seconds(60));

    EXPECT_FALSE(cache.get_optional(99));

    auto opt = cache.get_optional(1);
    ASSERT_TRUE(opt);
    EXPECT_EQ(opt->get(), 1);

    opt->get() = 42;
    EXPECT_EQ(cache.get(1), 42);

    constexpr std::chrono::nanoseconds zero_ttl(0);
    auto opt2 = cache.get_optional(1, zero_ttl);
    ASSERT_TRUE(opt2);
    EXPECT_EQ(opt2->get(), 2);
}

TEST_F(TestCache, test_invalidate)
{
    Cache<std::string, int> cache;
    int counter = 0;
    cache.set("a", [&] { return ++counter; }, std::chrono::seconds(60));
    cache.set("b", [&] { return ++counter; }, std::chrono::seconds(60));

    cache.get("a");
    cache.get("b");
    EXPECT_EQ(cache.entry_stats("a")->hit, 1u);
    EXPECT_EQ(cache.entry_stats("b")->hit, 1u);

    cache.invalidate("a");
    cache.get("a");
    EXPECT_EQ(cache.entry_stats("a")->miss, 1u);
    EXPECT_EQ(cache.entry_stats("b")->hit, 1u);

    cache.invalidate_all();
    cache.get("a");
    cache.get("b");
    EXPECT_EQ(cache.entry_stats("a")->miss, 2u);
    EXPECT_EQ(cache.entry_stats("b")->miss, 1u);
}

TEST_F(TestCache, test_erase_and_clear)
{
    Cache<int, int> cache;
    cache.set(1, [] { return 1; });
    cache.set(2, [] { return 2; });
    cache.set(3, [] { return 3; });
    EXPECT_EQ(cache.size(), 3u);

    cache.erase(2);
    EXPECT_EQ(cache.size(), 2u);
    EXPECT_FALSE(cache.entry_stats(2));

    cache.clear();
    EXPECT_EQ(cache.size(), 0u);
    EXPECT_FALSE(cache.entry_stats(1));
    EXPECT_FALSE(cache.entry_stats(3));
}

TEST_F(TestCache, test_contains)
{
    Cache<std::string, int> cache;
    EXPECT_FALSE(cache.contains("x"));
    cache.set("x", [] { return 1; });
    EXPECT_TRUE(cache.contains("x"));
    cache.erase("x");
    EXPECT_FALSE(cache.contains("x"));
}

TEST_F(TestCache, test_missing_key_throws)
{
    Cache<int, int> cache;

    EXPECT_THROW(cache.get(42), std::out_of_range);
    EXPECT_THROW(cache.invalidate(42), std::out_of_range);
    EXPECT_FALSE(cache.entry_stats(42));
    EXPECT_FALSE(cache.get_optional(42));
}

TEST_F(TestCache, test_infinite_ttl)
{
    Cache<int, int> cache;
    int counter = 0;
    cache.set(1, [&] { return ++counter; }, Timestamp(-1));

    cache.get(1);
    cache.get(1);
    cache.get(1);

    auto stats = cache.entry_stats(1);
    ASSERT_TRUE(stats);
    EXPECT_EQ(stats->hit, 3u);
    EXPECT_EQ(stats->miss, 0u);
}

TEST_F(TestCache, test_stats)
{
    Cache<int, int> cache;
    int counter = 0;
    cache.set(1, [&] { return ++counter; }, std::chrono::seconds(60));
    cache.set(2, [&] { return ++counter; }, std::chrono::seconds(60));

    cache.get(1);
    cache.get(1);
    cache.get(2);
    cache.get(2, std::chrono::nanoseconds(0));

    auto agg = cache.stats();
    EXPECT_EQ(agg.hit, 3u);
    EXPECT_EQ(agg.miss, 1u);
}

TEST_F(TestCache, test_get_cached)
{
    Cache<int, int> cache;

    // missing key returns nullopt
    EXPECT_FALSE(cache.get_cached(1));

    int counter = 0;
    constexpr bool lazy = true;
    cache.set(1, [&] { return ++counter; }, std::chrono::seconds(60), lazy);

    // lazy entry exposes its default cached value without refreshing or touching stats
    auto cached = cache.get_cached(1);
    ASSERT_TRUE(cached);
    EXPECT_EQ(cached->get(), 0);

    auto stats = cache.entry_stats(1);
    ASSERT_TRUE(stats);
    EXPECT_EQ(stats->hit, 0u);
    EXPECT_EQ(stats->miss, 0u);

    // after a real get, get_cached returns the refreshed value
    EXPECT_EQ(cache.get(1), 1);
    EXPECT_EQ(cache.get_cached(1)->get(), 1);
}

TEST_F(TestCache, test_refresh_and_stale_entries)
{
    Cache<int, int> cache;
    int counter = 0;
    cache.set(1, [&] { return ++counter; }, std::chrono::seconds(60));       // eager, fresh, value 1
    cache.set(2, [&] { return ++counter; }, std::chrono::seconds(60), true); // lazy, never refreshed

    // only the lazy/never-refreshed entry is stale
    auto stale = cache.stale_entries();
    ASSERT_EQ(stale.size(), 1u);
    EXPECT_EQ(stale[0], 2);

    // refresh_stale only refreshes the stale entry
    cache.refresh_stale();
    EXPECT_EQ(cache.get_cached(1)->get(), 1);
    EXPECT_EQ(cache.get_cached(2)->get(), 2);
    EXPECT_TRUE(cache.stale_entries().empty());

    // refresh_all refreshes everything regardless of freshness (values become 3 and 4)
    cache.refresh_all();
    EXPECT_EQ(cache.get_cached(1)->get() + cache.get_cached(2)->get(), 7);

    // refresh does not affect hit/miss stats
    auto agg = cache.stats();
    EXPECT_EQ(agg.hit, 0u);
    EXPECT_EQ(agg.miss, 0u);
}

} // namespace test
