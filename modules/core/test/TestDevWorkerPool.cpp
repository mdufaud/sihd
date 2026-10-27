#include <atomic>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include <sihd/core/Core.hpp>
#include <sihd/core/DevWorkerPool.hpp>
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

class TestDevWorkerPool: public ::testing::Test
{
    protected:
        TestDevWorkerPool() { LoggerManager::stream(); }

        virtual ~TestDevWorkerPool() { LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestDevWorkerPool, test_dev_worker_pool_relays_a_channel_while_another_one_blocks)
{
    Core core;
    Channel *slow = core.add_channel("slow", "int", 1);
    Channel *fast = core.add_channel("fast", "int", 1);
    DevWorkerPool *pool = core.add_child<DevWorkerPool>("pool");

    ASSERT_TRUE(pool->set_conf_int("threads", 2));
    ASSERT_TRUE(pool->add_link("slow", "..slow"));
    ASSERT_TRUE(pool->add_link("fast", "..fast"));
    ASSERT_TRUE(core.init());
    ASSERT_TRUE(core.start());

    Channel *slow_out = channel_of(pool, "slow_out");
    Channel *fast_out = channel_of(pool, "fast_out");
    ASSERT_NE(slow_out, nullptr);
    ASSERT_NE(fast_out, nullptr);

    Synchronizer slow_entered(2);
    Synchronizer slow_release(2);
    Synchronizer fast_relayed(2);
    std::atomic<bool> slow_seen = false;
    std::thread::id slow_thread;
    std::thread::id fast_thread;
    Handler<Channel *> slow_observer([&](Channel *) {
        if (slow_seen.exchange(true))
            return;
        slow_thread = std::this_thread::get_id();
        slow_entered.sync();
        slow_release.sync();
    });
    Handler<Channel *> fast_observer([&](Channel *) {
        fast_thread = std::this_thread::get_id();
        fast_relayed.sync();
    });

    slow_out->add_observer(&slow_observer);
    fast_out->add_observer(&fast_observer);

    ASSERT_TRUE(slow->write<int>(0, 1));
    ASSERT_TRUE(slow_entered.sync(Duration(time::sec(2))));
    ASSERT_TRUE(fast->write<int>(0, 2));
    ASSERT_TRUE(fast_relayed.sync(Duration(time::sec(2))));
    ASSERT_TRUE(slow_release.sync(Duration(time::sec(2))));

    EXPECT_NE(slow_thread, fast_thread);
    EXPECT_EQ(fast_out->read<int>(0), 2);
    EXPECT_TRUE(core.stop());
}

TEST_F(TestDevWorkerPool, test_dev_worker_pool_keeps_the_order_of_each_channel)
{
    constexpr int last_value = 50;
    Core core;
    Channel *first = core.add_channel("first", "int", 1);
    Channel *second = core.add_channel("second", "int", 1);
    DevWorkerPool *pool = core.add_child<DevWorkerPool>("pool");
    ASSERT_TRUE(pool->set_conf_int("threads", 4));
    ASSERT_TRUE(pool->add_link("first", "..first"));
    ASSERT_TRUE(pool->add_link("second", "..second"));
    ASSERT_TRUE(core.init());
    ASSERT_TRUE(core.start());

    Channel *first_out = channel_of(pool, "first_out");
    Channel *second_out = channel_of(pool, "second_out");
    ASSERT_NE(first_out, nullptr);
    ASSERT_NE(second_out, nullptr);

    std::vector<int> first_values;
    std::vector<int> second_values;
    Handler<Channel *> first_observer([&](Channel *notifier) { first_values.push_back(notifier->read<int>(0)); });
    Handler<Channel *> second_observer([&](Channel *notifier) { second_values.push_back(notifier->read<int>(0)); });
    first_out->add_observer(&first_observer);
    second_out->add_observer(&second_observer);

    for (int value = 1; value <= last_value; ++value)
    {
        ASSERT_TRUE(first->write<int>(0, value));
        ASSERT_TRUE(second->write<int>(0, -value));
    }
    ASSERT_TRUE(core.stop());

    ASSERT_EQ(first_values.size(), 50u);
    ASSERT_EQ(second_values.size(), 50u);
    for (int value = 1; value <= last_value; ++value)
    {
        EXPECT_EQ(first_values[value - 1], value);
        EXPECT_EQ(second_values[value - 1], -value);
    }
}

TEST_F(TestDevWorkerPool, test_dev_worker_pool_rejects_zero_threads)
{
    DevWorkerPool pool("pool");

    EXPECT_FALSE(pool.set_conf_int("threads", 0));
}

TEST_F(TestDevWorkerPool, test_dev_worker_pool_restart)
{
    Core core;
    Channel *source = core.add_channel("source", "int", 1);
    DevWorkerPool *pool = core.add_child<DevWorkerPool>("pool");

    ASSERT_TRUE(pool->set_conf_int("threads", 2));
    ASSERT_TRUE(pool->add_link("source", "..source"));
    ASSERT_TRUE(core.init());
    ASSERT_TRUE(core.start());

    Channel *out = channel_of(pool, "source_out");
    ASSERT_NE(out, nullptr);

    ASSERT_TRUE(source->write<int>(0, 1));
    ASSERT_TRUE(core.stop());
    ASSERT_EQ(out->read<int>(0), 1);

    ASSERT_TRUE(core.start());
    ASSERT_TRUE(source->write<int>(0, 2));
    ASSERT_TRUE(core.stop());

    EXPECT_EQ(out->read<int>(0), 2);
}
} // namespace test
