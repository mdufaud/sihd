#include <atomic>
#include <vector>

#include <gtest/gtest.h>

#include <sihd/core/Core.hpp>
#include <sihd/core/DevBatcher.hpp>
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

class TestDevBatcher: public ::testing::Test
{
    protected:
        TestDevBatcher() { LoggerManager::stream(); }

        virtual ~TestDevBatcher() { LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}

        DevBatcher *wire(Core & core, int samples, int period_ms)
        {
            _source = core.add_channel("source", "int", 1);
            DevBatcher *batcher = core.add_child<DevBatcher>("batcher");
            EXPECT_TRUE(batcher->set_conf_int("samples", samples));
            EXPECT_TRUE(batcher->set_conf_int("period", period_ms));
            EXPECT_TRUE(batcher->add_link("source", "..source"));
            return batcher;
        }

        void start(Core & core, DevBatcher *batcher, const std::string & batch_name = "source_batch")
        {
            ASSERT_TRUE(core.init());
            ASSERT_TRUE(core.start());
            _batch = channel_of(batcher, batch_name);
            ASSERT_NE(_batch, nullptr);
        }

        Channel *_source = nullptr;
        Channel *_batch = nullptr;
};

TEST_F(TestDevBatcher, test_dev_batcher_writes_a_full_block)
{
    Core core;
    DevBatcher *batcher = this->wire(core, 4, 10000);
    this->start(core, batcher);

    for (int value = 1; value <= 4; ++value)
        ASSERT_TRUE(_source->write<int>(0, value));
    ASSERT_TRUE(core.stop());

    EXPECT_EQ(_batch->byte_size(), 16u);
    EXPECT_EQ(_batch->read<int>(0), 1);
    EXPECT_EQ(_batch->read<int>(1), 2);
    EXPECT_EQ(_batch->read<int>(2), 3);
    EXPECT_EQ(_batch->read<int>(3), 4);
}

TEST_F(TestDevBatcher, test_dev_batcher_notifies_a_shorter_block_with_the_same_start)
{
    Core core;
    DevBatcher *batcher = this->wire(core, 2, 10000);
    this->start(core, batcher);
    int notified = 0;
    Handler<Channel *> batch_observer([&](Channel *) { ++notified; });
    _batch->add_observer(&batch_observer);
    ASSERT_TRUE(_source->write<int>(0, 5));
    ASSERT_TRUE(_source->write<int>(0, 6));
    ASSERT_TRUE(core.stop());
    ASSERT_EQ(notified, 1);

    ASSERT_TRUE(core.start());
    ASSERT_TRUE(_source->write<int>(0, 5));
    ASSERT_TRUE(core.stop());

    EXPECT_EQ(notified, 2);
    EXPECT_EQ(_batch->byte_size(), 4u);
    EXPECT_EQ(_batch->read<int>(0), 5);
}

TEST_F(TestDevBatcher, test_dev_batcher_flushes_a_partial_block_at_the_end_of_the_period)
{
    Core core;
    DevBatcher *batcher = this->wire(core, 4, 20);
    this->start(core, batcher);

    Synchronizer both_written(2);
    size_t written_samples = 0;
    Handler<Channel *> batch_observer([&](Channel *notifier) {
        written_samples += notifier->byte_size() / sizeof(int);
        if (written_samples == 2)
            both_written.sync();
    });
    _batch->add_observer(&batch_observer);

    ASSERT_TRUE(_source->write<int>(0, 7));
    ASSERT_TRUE(_source->write<int>(0, 8));
    ASSERT_TRUE(both_written.sync(Duration(time::sec(2))));

    EXPECT_EQ(_batch->read<int>(_batch->byte_size() / sizeof(int) - 1), 8);
    EXPECT_TRUE(core.stop());
}

TEST_F(TestDevBatcher, test_dev_batcher_flushes_a_partial_block_when_stopped)
{
    Core core;
    DevBatcher *batcher = this->wire(core, 4, 10000);
    this->start(core, batcher);
    ASSERT_TRUE(_source->write<int>(0, 5));
    ASSERT_TRUE(_source->write<int>(0, 6));

    ASSERT_TRUE(core.stop());

    EXPECT_EQ(_batch->byte_size(), 8u);
    EXPECT_EQ(_batch->read<int>(0), 5);
    EXPECT_EQ(_batch->read<int>(1), 6);
}

TEST_F(TestDevBatcher, test_dev_batcher_batches_resizable_samples)
{
    Core core;
    Channel *packets = core.add_channel_resizable("packets", "byte", 1, 8);
    DevBatcher *batcher = core.add_child<DevBatcher>("batcher");
    ASSERT_TRUE(batcher->set_conf_int("samples", 2));
    ASSERT_TRUE(batcher->set_conf_int("period", 10000));
    ASSERT_TRUE(batcher->add_link("packets", "..packets"));
    this->start(core, batcher, "packets_batch");

    ASSERT_TRUE(packets->write(ArrByte {1, 2, 3}));
    ASSERT_TRUE(packets->write(ArrByte {4}));
    ASSERT_TRUE(core.stop());

    EXPECT_EQ(_batch->byte_size(), 4u);
    EXPECT_EQ(_batch->read<int8_t>(0), 1);
    EXPECT_EQ(_batch->read<int8_t>(3), 4);
}

TEST_F(TestDevBatcher, test_dev_batcher_start_fails_without_samples_or_with_a_link_to_no_channel)
{
    Core core;
    core.add_channel("source", "int", 1);
    core.add_child<Node>("not_a_channel");
    DevBatcher *batcher = core.add_child<DevBatcher>("batcher");
    ASSERT_TRUE(batcher->add_link("source", "..source"));
    DevBatcher *bad_link = core.add_child<DevBatcher>("bad_link");
    ASSERT_TRUE(bad_link->set_conf_int("samples", 2));
    ASSERT_TRUE(bad_link->add_link("device", "..not_a_channel"));
    ASSERT_TRUE(core.init());

    EXPECT_FALSE(batcher->set_conf_int("samples", 0));
    EXPECT_FALSE(batcher->start());
    EXPECT_FALSE(bad_link->start());
}

TEST_F(TestDevBatcher, test_dev_batcher_batches_each_linked_channel_apart)
{
    Core core;
    Channel *first = core.add_channel("first", "int", 1);
    Channel *second = core.add_channel("second", "double", 1);
    DevBatcher *batcher = core.add_child<DevBatcher>("batcher");
    ASSERT_TRUE(batcher->set_conf_int("samples", 2));
    ASSERT_TRUE(batcher->set_conf_int("period", 10000));
    ASSERT_TRUE(batcher->add_link("first", "..first"));
    ASSERT_TRUE(batcher->add_link("second", "..second"));
    ASSERT_TRUE(core.init());
    ASSERT_TRUE(core.start());

    ASSERT_TRUE(first->write<int>(0, 1));
    ASSERT_TRUE(second->write<double>(0, 0.5));
    ASSERT_TRUE(first->write<int>(0, 2));
    ASSERT_TRUE(core.stop());

    Channel *first_batch = channel_of(batcher, "first_batch");
    Channel *second_batch = channel_of(batcher, "second_batch");
    ASSERT_NE(first_batch, nullptr);
    ASSERT_NE(second_batch, nullptr);
    EXPECT_EQ(first_batch->byte_size(), 8u);
    EXPECT_EQ(first_batch->read<int>(1), 2);
    EXPECT_EQ(second_batch->data_type(), TYPE_DOUBLE);
    EXPECT_EQ(second_batch->byte_size(), 8u);
    EXPECT_EQ(second_batch->read<double>(0), 0.5);
}

TEST_F(TestDevBatcher, test_dev_batcher_restart)
{
    Core core;
    DevBatcher *batcher = this->wire(core, 2, 10000);
    this->start(core, batcher);
    ASSERT_TRUE(_source->write<int>(0, 1));
    ASSERT_TRUE(_source->write<int>(0, 2));
    ASSERT_TRUE(core.stop());
    ASSERT_EQ(_batch->read<int>(0), 1);
    ASSERT_EQ(_batch->read<int>(1), 2);

    ASSERT_TRUE(core.start());
    ASSERT_TRUE(_source->write<int>(0, 3));
    ASSERT_TRUE(_source->write<int>(0, 4));
    ASSERT_TRUE(core.stop());

    EXPECT_EQ(_batch->read<int>(0), 3);
    EXPECT_EQ(_batch->read<int>(1), 4);
}
} // namespace test
