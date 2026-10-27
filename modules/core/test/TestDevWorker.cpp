#include <atomic>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include <sihd/core/Core.hpp>
#include <sihd/core/DevWorker.hpp>
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

class TestDevWorker: public ::testing::Test
{
    protected:
        TestDevWorker() { LoggerManager::stream(); }

        virtual ~TestDevWorker() { LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestDevWorker, test_dev_worker_relays_on_its_thread_and_stamps_the_relay_write)
{
    Core core;
    Channel *source = core.add_channel("source", "byte", 3);
    DevWorker *worker = core.add_child<DevWorker>("worker");
    ASSERT_TRUE(worker->add_link("source", "..source"));
    ASSERT_TRUE(core.init());
    ASSERT_TRUE(core.start());
    Channel *out = channel_of(worker, "source_out");
    ASSERT_NE(out, nullptr);

    Synchronizer relayed(2);
    std::thread::id out_thread;
    Timestamp out_timestamp(0);
    int8_t out_first = 0;
    Handler<Channel *> out_observer([&](Channel *notifier) {
        out_thread = std::this_thread::get_id();
        out_timestamp = notifier->timestamp();
        out_first = notifier->read<int8_t>(0);
        relayed.sync();
    });
    out->add_observer(&out_observer);

    const ArrByte data = {11, 12, 13};
    ASSERT_TRUE(source->write(data));
    ASSERT_TRUE(relayed.sync(Duration(time::sec(2))));

    EXPECT_EQ(out_first, 11);
    EXPECT_EQ(out->read<int8_t>(2), 13);
    EXPECT_GE(out_timestamp, source->timestamp());
    EXPECT_NE(out_thread, std::this_thread::get_id());
    EXPECT_TRUE(core.stop());
}

TEST_F(TestDevWorker, test_dev_worker_relays_several_channels_on_one_thread)
{
    Core core;
    Channel *temperature = core.add_channel("temperature", "double", 1);
    Channel *packets = core.add_channel_resizable("packets", "byte", 1, 16);
    DevWorker *worker = core.add_child<DevWorker>("worker");

    ASSERT_TRUE(worker->add_link("temperature", "..temperature"));
    ASSERT_TRUE(worker->add_link("packets", "..packets"));
    ASSERT_TRUE(core.init());
    ASSERT_TRUE(core.start());
    Channel *temperature_out = channel_of(worker, "temperature_out");
    Channel *packets_out = channel_of(worker, "packets_out");
    ASSERT_NE(temperature_out, nullptr);
    ASSERT_NE(packets_out, nullptr);

    EXPECT_EQ(temperature_out->data_type(), TYPE_DOUBLE);
    EXPECT_TRUE(packets_out->resizable());
    EXPECT_EQ(packets_out->capacity(), 16u);

    Synchronizer temperature_relayed(2);
    Synchronizer packets_relayed(2);
    std::thread::id temperature_thread;
    std::thread::id packets_thread;
    Handler<Channel *> temperature_observer([&](Channel *) {
        temperature_thread = std::this_thread::get_id();
        temperature_relayed.sync();
    });
    Handler<Channel *> packets_observer([&](Channel *) {
        packets_thread = std::this_thread::get_id();
        packets_relayed.sync();
    });
    temperature_out->add_observer(&temperature_observer);
    packets_out->add_observer(&packets_observer);

    ASSERT_TRUE(temperature->write<double>(0, 21.5));
    ASSERT_TRUE(packets->write(ArrByte {1, 2, 3, 4, 5}));
    ASSERT_TRUE(temperature_relayed.sync(Duration(time::sec(2))));
    ASSERT_TRUE(packets_relayed.sync(Duration(time::sec(2))));

    EXPECT_EQ(temperature_out->read<double>(0), 21.5);
    EXPECT_EQ(packets_out->byte_size(), 5u);
    EXPECT_EQ(temperature_thread, packets_thread);
    EXPECT_NE(temperature_thread, std::this_thread::get_id());
    EXPECT_TRUE(core.stop());
}

TEST_F(TestDevWorker, test_dev_worker_resizable_out_takes_the_size_of_each_write)
{
    Core core;
    Channel *source = core.add_channel_resizable("source", "byte", 1, 16);
    DevWorker *worker = core.add_child<DevWorker>("worker");
    ASSERT_TRUE(worker->add_link("source", "..source"));
    ASSERT_TRUE(core.init());
    ASSERT_TRUE(core.start());
    Channel *out = channel_of(worker, "source_out");
    ASSERT_NE(out, nullptr);

    Synchronizer both_relayed(2);
    std::vector<size_t> relayed_sizes;
    Handler<Channel *> out_observer([&](Channel *notifier) {
        relayed_sizes.push_back(notifier->byte_size());
        if (relayed_sizes.size() == 2)
            both_relayed.sync();
    });
    out->add_observer(&out_observer);

    ASSERT_TRUE(source->write(ArrByte {1, 2, 3, 4, 5, 6}));
    ASSERT_TRUE(source->write(ArrByte {8, 9}));
    ASSERT_TRUE(both_relayed.sync(Duration(time::sec(2))));

    EXPECT_EQ(relayed_sizes[0], 6u);
    EXPECT_EQ(relayed_sizes[1], 2u);
    EXPECT_EQ(out->read<int8_t>(1), 9);
    EXPECT_TRUE(core.stop());
}

TEST_F(TestDevWorker, test_dev_worker_keeps_the_write_order)
{
    constexpr int last_value = 20;
    Core core;
    Channel *source = core.add_channel("source", "int", 1);
    DevWorker *worker = core.add_child<DevWorker>("worker");
    ASSERT_TRUE(worker->add_link("source", "..source"));
    ASSERT_TRUE(core.init());
    ASSERT_TRUE(core.start());
    Channel *out = channel_of(worker, "source_out");
    ASSERT_NE(out, nullptr);

    Synchronizer all_relayed(2);
    std::vector<int> relayed_values;
    Handler<Channel *> out_observer([&](Channel *notifier) {
        relayed_values.push_back(notifier->read<int>(0));
        if (relayed_values.back() == last_value)
            all_relayed.sync();
    });
    out->add_observer(&out_observer);

    for (int value = 1; value <= last_value; ++value)
        ASSERT_TRUE(source->write<int>(0, value));
    ASSERT_TRUE(all_relayed.sync(Duration(time::sec(2))));

    ASSERT_EQ(relayed_values.size(), 20u);
    for (int value = 1; value <= last_value; ++value)
        EXPECT_EQ(relayed_values[value - 1], value);
    EXPECT_TRUE(core.stop());
}

TEST_F(TestDevWorker, test_dev_worker_drops_the_newest_values_when_full)
{
    Core core;
    Channel *source = core.add_channel("source", "int", 1);
    DevWorker *worker = core.add_child<DevWorker>("worker");
    ASSERT_TRUE(worker->add_link("source", "..source"));
    ASSERT_TRUE(worker->set_conf_int("queue_size", 2));
    ASSERT_TRUE(core.init());
    ASSERT_TRUE(core.start());
    Channel *out = channel_of(worker, "source_out");
    Channel *dropped = channel_of(worker, "dropped");
    ASSERT_NE(out, nullptr);
    ASSERT_NE(dropped, nullptr);

    Synchronizer first_entered(2);
    Synchronizer first_release(2);
    std::atomic<bool> first_seen = false;
    Handler<Channel *> out_observer([&](Channel *) {
        if (first_seen.exchange(true))
            return;
        first_entered.sync();
        first_release.sync();
    });
    out->add_observer(&out_observer);

    Synchronizer dropped_published(2);
    std::atomic<bool> dropped_seen = false;
    Handler<Channel *> dropped_observer([&](Channel *) {
        if (!dropped_seen.exchange(true))
            dropped_published.sync();
    });
    dropped->add_observer(&dropped_observer);

    ASSERT_TRUE(source->write<int>(0, 1));
    ASSERT_TRUE(first_entered.sync(Duration(time::sec(2))));
    for (int value = 2; value <= 4; ++value)
        ASSERT_TRUE(source->write<int>(0, value));
    ASSERT_TRUE(first_release.sync(Duration(time::sec(2))));
    ASSERT_TRUE(dropped_published.sync(Duration(time::sec(2))));

    EXPECT_EQ(dropped->read<uint64_t>(0), 1u);
    EXPECT_TRUE(core.stop());
    EXPECT_EQ(out->read<int>(0), 3);
}

TEST_F(TestDevWorker, test_dev_worker_stop_writes_what_is_queued)
{
    Core core;
    Channel *source = core.add_channel("source", "int", 1);
    DevWorker *worker = core.add_child<DevWorker>("worker");
    ASSERT_TRUE(worker->add_link("source", "..source"));
    ASSERT_TRUE(core.init());
    ASSERT_TRUE(core.start());
    Channel *out = channel_of(worker, "source_out");
    ASSERT_NE(out, nullptr);

    for (int value = 1; value <= 10; ++value)
        ASSERT_TRUE(source->write<int>(0, value));
    ASSERT_TRUE(core.stop());

    EXPECT_EQ(out->read<int>(0), 10);
}

TEST_F(TestDevWorker, test_dev_worker_dropped_alias_is_not_relayed)
{
    Core core;
    core.add_channel("source", "int", 1);
    core.add_channel("dropped_alias", "ulong", 1);
    DevWorker *worker = core.add_child<DevWorker>("worker");
    ASSERT_TRUE(worker->add_link("source", "..source"));
    ASSERT_TRUE(worker->add_link("dropped", "..dropped_alias"));
    ASSERT_TRUE(core.init());
    ASSERT_TRUE(core.start());

    EXPECT_NE(channel_of(worker, "source_out"), nullptr);
    EXPECT_EQ(channel_of(worker, "dropped_out"), nullptr);
    EXPECT_TRUE(core.stop());
}

TEST_F(TestDevWorker, test_dev_worker_rejects_zero_queue_size)
{
    DevWorker worker("worker");

    EXPECT_FALSE(worker.set_conf_int("queue_size", 0));
}

TEST_F(TestDevWorker, test_dev_worker_start_fails_when_a_link_is_not_a_channel)
{
    Core core;
    core.add_child<Node>("not_a_channel");
    DevWorker *worker = core.add_child<DevWorker>("worker");
    ASSERT_TRUE(worker->add_link("device", "..not_a_channel"));
    ASSERT_TRUE(core.init());

    EXPECT_FALSE(core.start());
}

TEST_F(TestDevWorker, test_dev_worker_restart)
{
    Core core;
    Channel *source = core.add_channel("source", "int", 1);
    DevWorker *worker = core.add_child<DevWorker>("worker");
    ASSERT_TRUE(worker->add_link("source", "..source"));
    ASSERT_TRUE(core.init());
    ASSERT_TRUE(core.start());
    Channel *out = channel_of(worker, "source_out");
    ASSERT_NE(out, nullptr);

    Synchronizer first_relayed(2);
    Synchronizer second_relayed(2);
    std::atomic<int> phase = 0;
    Handler<Channel *> out_observer([&](Channel *) {
        if (phase.load() == 0)
            first_relayed.sync();
        else
            second_relayed.sync();
    });
    out->add_observer(&out_observer);
    ASSERT_TRUE(source->write<int>(0, 1));
    ASSERT_TRUE(first_relayed.sync(Duration(time::sec(2))));
    ASSERT_TRUE(core.stop());

    phase = 1;
    ASSERT_TRUE(core.start());
    ASSERT_TRUE(source->write<int>(0, 2));
    ASSERT_TRUE(second_relayed.sync(Duration(time::sec(2))));

    EXPECT_EQ(out->read<int>(0), 2);
    EXPECT_TRUE(core.stop());
}
} // namespace test
