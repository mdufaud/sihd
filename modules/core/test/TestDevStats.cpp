#include <atomic>

#include <gtest/gtest.h>

#include <sihd/core/Core.hpp>
#include <sihd/core/DevStats.hpp>
#include <sihd/util/Array.hpp>
#include <sihd/util/Handler.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Synchronizer.hpp>
#include <sihd/util/time.hpp>

#include "test_helper.hpp"

namespace test
{
SIHD_LOGGER;
using namespace sihd::core;
using namespace sihd::util;

class TestDevStats: public ::testing::Test
{
    protected:
        TestDevStats() { LoggerManager::stream(); }

        virtual ~TestDevStats() { LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}

        DevStats *wire(Core & core, const std::string & type, int period_ms)
        {
            _source = core.add_channel("source", type, 1);
            DevStats *stats = core.add_child<DevStats>("stats");
            EXPECT_TRUE(stats->add_link("source", "..source"));
            EXPECT_TRUE(stats->set_conf_int("period", period_ms));
            return stats;
        }

        Channel *_source = nullptr;
};

TEST_F(TestDevStats, test_dev_stats_counts_writes_and_bytes)
{
    Core core;
    DevStats *stats = this->wire(core, "int", 20);
    ASSERT_TRUE(core.init());
    ASSERT_TRUE(core.start());
    Channel *count = channel_of(stats, "source_count");
    ASSERT_NE(count, nullptr);
    Synchronizer counted(2);
    std::atomic<bool> signaled = false;
    Handler<Channel *> count_observer([&](Channel *notifier) {
        if (notifier->read<uint64_t>(0) == 3 && !signaled.exchange(true))
            counted.sync();
    });
    count->add_observer(&count_observer);

    for (int value = 1; value <= 3; ++value)
        ASSERT_TRUE(_source->write<int>(0, value));
    ASSERT_TRUE(counted.sync(Duration(time::sec(2))));

    EXPECT_EQ(first_value_of<uint64_t>(stats, "source_bytes"), 12u);
    EXPECT_GT(first_value_of<double>(stats, "source_rate"), 0.0);
    EXPECT_GT(first_value_of<double>(stats, "source_byte_rate"), 0.0);
    EXPECT_TRUE(core.stop());
}

TEST_F(TestDevStats, test_dev_stats_computes_min_max_and_mean)
{
    Core core;
    DevStats *stats = this->wire(core, "double", 20);
    ASSERT_TRUE(core.init());
    ASSERT_TRUE(core.start());
    Channel *count = channel_of(stats, "source_count");
    ASSERT_NE(count, nullptr);
    Synchronizer counted(2);
    std::atomic<bool> signaled = false;
    Handler<Channel *> count_observer([&](Channel *notifier) {
        if (notifier->read<uint64_t>(0) == 3 && !signaled.exchange(true))
            counted.sync();
    });
    count->add_observer(&count_observer);

    ASSERT_TRUE(_source->write<double>(0, 1.0));
    ASSERT_TRUE(_source->write<double>(0, 3.0));
    ASSERT_TRUE(_source->write<double>(0, 8.0));
    ASSERT_TRUE(counted.sync(Duration(time::sec(2))));

    EXPECT_EQ(first_value_of<double>(stats, "source_min"), 1.0);
    EXPECT_EQ(first_value_of<double>(stats, "source_max"), 8.0);
    EXPECT_EQ(first_value_of<double>(stats, "source_mean"), 4.0);
    EXPECT_TRUE(core.stop());
}

TEST_F(TestDevStats, test_dev_stats_publishes_the_totals_when_stopped)
{
    Core core;
    DevStats *stats = this->wire(core, "int", 10000);
    ASSERT_TRUE(core.init());
    ASSERT_TRUE(core.start());
    ASSERT_TRUE(_source->write<int>(0, 5));
    ASSERT_TRUE(_source->write<int>(0, 6));

    ASSERT_TRUE(core.stop());

    EXPECT_EQ(first_value_of<uint64_t>(stats, "source_count"), 2u);
    EXPECT_EQ(first_value_of<uint64_t>(stats, "source_bytes"), 8u);
    EXPECT_EQ(first_value_of<double>(stats, "source_min"), 5.0);
    EXPECT_EQ(first_value_of<double>(stats, "source_max"), 6.0);
}

TEST_F(TestDevStats, test_dev_stats_ignores_values_of_byte_channels)
{
    Core core;
    DevStats *stats = this->wire(core, "byte", 10000);
    ASSERT_TRUE(core.init());
    ASSERT_TRUE(core.start());
    ASSERT_TRUE(_source->write<int8_t>(0, 9));

    ASSERT_TRUE(core.stop());

    EXPECT_EQ(first_value_of<uint64_t>(stats, "source_count"), 1u);
    EXPECT_EQ(first_value_of<uint64_t>(stats, "source_bytes"), 1u);
    EXPECT_EQ(first_value_of<double>(stats, "source_max"), 0.0);
}

TEST_F(TestDevStats, test_dev_stats_keeps_one_total_per_linked_channel)
{
    Core core;
    Channel *first = core.add_channel("first", "int", 1);
    Channel *second = core.add_channel("second", "double", 2);
    DevStats *stats = core.add_child<DevStats>("stats");
    ASSERT_TRUE(stats->add_link("first", "..first"));
    ASSERT_TRUE(stats->add_link("second", "..second"));
    ASSERT_TRUE(stats->set_conf_int("period", 10000));
    ASSERT_TRUE(core.init());
    ASSERT_TRUE(core.start());

    ASSERT_TRUE(first->write<int>(0, 4));
    ASSERT_TRUE(first->write<int>(0, 5));
    ASSERT_TRUE(second->write<double>(1, 2.5));
    ASSERT_TRUE(core.stop());

    EXPECT_EQ(first_value_of<uint64_t>(stats, "first_count"), 2u);
    EXPECT_EQ(first_value_of<double>(stats, "first_max"), 5.0);
    EXPECT_EQ(first_value_of<uint64_t>(stats, "second_count"), 1u);
    EXPECT_EQ(first_value_of<uint64_t>(stats, "second_bytes"), 16u);
    EXPECT_EQ(first_value_of<double>(stats, "second_max"), 2.5);
}

TEST_F(TestDevStats, test_dev_stats_start_fails_when_a_link_is_not_a_channel)
{
    Core core;
    core.add_child<Node>("not_a_channel");
    DevStats *stats = core.add_child<DevStats>("stats");
    ASSERT_TRUE(stats->add_link("device", "..not_a_channel"));
    ASSERT_TRUE(core.init());

    EXPECT_FALSE(core.start());
}

TEST_F(TestDevStats, test_dev_stats_restart_resets_the_totals)
{
    Core core;
    DevStats *stats = this->wire(core, "int", 10000);
    ASSERT_TRUE(core.init());
    ASSERT_TRUE(core.start());
    ASSERT_TRUE(_source->write<int>(0, 1));
    ASSERT_TRUE(_source->write<int>(0, 2));
    ASSERT_TRUE(core.stop());
    ASSERT_EQ(first_value_of<uint64_t>(stats, "source_count"), 2u);

    ASSERT_TRUE(core.start());
    ASSERT_TRUE(_source->write<int>(0, 3));
    ASSERT_TRUE(core.stop());

    EXPECT_EQ(first_value_of<uint64_t>(stats, "source_count"), 1u);
    EXPECT_EQ(first_value_of<double>(stats, "source_min"), 3.0);
}
} // namespace test
