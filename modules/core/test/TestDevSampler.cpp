#include <atomic>
#include <string>
#include <thread>

#include <gtest/gtest.h>

#include <sihd/core/ChannelWaiter.hpp>
#include <sihd/core/Core.hpp>
#include <sihd/core/DevSampler.hpp>
#include <sihd/util/Array.hpp>
#include <sihd/util/Handler.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/ansi.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::core;
using namespace sihd::util;
class TestDevSampler: public ::testing::Test
{
    protected:
        TestDevSampler() { LoggerManager::stream(); }

        virtual ~TestDevSampler() { LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestDevSampler, test_devsampler)
{
    Core core;

    DevSampler *dev_ptr = core.add_child<DevSampler>("sampler");
    ASSERT_TRUE(dev_ptr->set_conf("frequency", 100.0));
    ASSERT_TRUE(dev_ptr->set_conf_str("sample", "..out_channel=..in_channel"));

    core.add_channel("in_channel", "int", 3);
    core.add_channel("out_channel", "int", 3);

    ASSERT_TRUE(core.init());
    ASSERT_TRUE(core.start());

    Channel *in_channel = core.get_channel("in_channel").value_or(nullptr);
    Channel *out_channel = core.get_channel("out_channel").value_or(nullptr);
    Channel *sample_channel = dev_ptr->get_channel("sample").value_or(nullptr);
    ASSERT_NE(in_channel, nullptr);
    ASSERT_NE(out_channel, nullptr);
    ASSERT_NE(sample_channel, nullptr);

    int out_channel_notif = 0;
    Handler<Channel *> out_channel_counter([&out_channel_notif](Channel *c) {
        (void)c;
        ++out_channel_notif;
    });
    out_channel->add_observer(&out_channel_counter);

    int sample_channel_notif = 0;
    Handler<Channel *> sample_channel_counter([&sample_channel_notif](Channel *c) {
        (void)c;
        ++sample_channel_notif;
    });
    sample_channel->add_observer(&sample_channel_counter);

    ChannelWaiter out_channel_waiter(out_channel);
    ChannelWaiter sample_channel_waiter(sample_channel);

    ASSERT_TRUE(in_channel->write(ArrInt({0, 0, 1})));
    EXPECT_TRUE(out_channel_waiter.prev_wait_for(Duration(time::sec(1))));
    EXPECT_EQ(out_channel->array()->str(','), "0,0,1");
    EXPECT_EQ(out_channel_notif, 1);

    ASSERT_TRUE(in_channel->write(ArrInt({1, 2, 3})));
    ASSERT_TRUE(in_channel->write(ArrInt({2, 3, 4})));
    ASSERT_TRUE(in_channel->write(ArrInt({3, 4, 5})));
    EXPECT_TRUE(out_channel_waiter.prev_wait_for(Duration(time::sec(1))));
    EXPECT_EQ(out_channel->array()->str(','), "3,4,5");
    EXPECT_EQ(out_channel_notif, 2);

    ASSERT_TRUE(in_channel->write(ArrInt({1, 2, 3})));
    ASSERT_TRUE(in_channel->write(ArrInt({2, 3, 4})));
    ASSERT_TRUE(in_channel->write(ArrInt({4, 5, 6})));
    EXPECT_TRUE(out_channel_waiter.prev_wait_for(Duration(time::sec(1))));
    EXPECT_EQ(out_channel->array()->str(','), "4,5,6");
    EXPECT_EQ(out_channel_notif, 3);

    ASSERT_TRUE(in_channel->write(ArrInt({1, 2, 3})));
    ASSERT_TRUE(in_channel->write(ArrInt({2, 3, 4})));
    ASSERT_TRUE(in_channel->write(ArrInt({5, 6, 7})));
    EXPECT_TRUE(out_channel_waiter.prev_wait_for(Duration(time::sec(1))));
    EXPECT_EQ(out_channel->array()->str(','), "5,6,7");
    EXPECT_EQ(out_channel_notif, 4);

    ASSERT_TRUE(core.stop());
}

TEST_F(TestDevSampler, test_devsampler_keeps_mappings_across_reset)
{
    Core core;
    DevSampler *dev_ptr = core.add_child<DevSampler>("sampler");
    ASSERT_TRUE(dev_ptr->set_conf("frequency", 100.0));
    ASSERT_TRUE(dev_ptr->set_conf_str("sample", "..out_channel=..in_channel"));
    core.add_channel("in_channel", "int", 3);
    core.add_channel("out_channel", "int", 3);
    ASSERT_TRUE(core.init());
    ASSERT_TRUE(core.start());
    ASSERT_TRUE(dev_ptr->stop());
    ASSERT_TRUE(dev_ptr->reset());

    ASSERT_TRUE(dev_ptr->init());
    ASSERT_TRUE(dev_ptr->start());
    Channel *in_channel = core.get_channel("in_channel").value_or(nullptr);
    Channel *out_channel = core.get_channel("out_channel").value_or(nullptr);
    ASSERT_NE(in_channel, nullptr);
    ASSERT_NE(out_channel, nullptr);
    ChannelWaiter out_channel_waiter(out_channel);
    ASSERT_TRUE(in_channel->write(ArrInt({0, 0, 1})));

    EXPECT_TRUE(out_channel_waiter.prev_wait_for(Duration(time::sec(1))));
    EXPECT_EQ(out_channel->array()->str(','), "0,0,1");
    ASSERT_TRUE(dev_ptr->stop());
}

TEST_F(TestDevSampler, test_devsampler_wrong_conf)
{
    Core core;
    DevSampler *dev_ptr = core.add_child<DevSampler>("sampler");
    EXPECT_FALSE(dev_ptr->set_conf_str("sample", "no_separator"));
    EXPECT_TRUE(dev_ptr->set_conf_str("sample", "..no_such_out=..no_such_in"));

    EXPECT_TRUE(core.init());
    EXPECT_FALSE(core.start());
}

TEST_F(TestDevSampler, test_devsampler_two_mappings)
{
    Core core;
    core.add_channel("in1", "int", 1);
    core.add_channel("out1", "int", 1);
    core.add_channel("in2", "int", 1);
    core.add_channel("out2", "int", 1);

    DevSampler *dev_ptr = core.add_child<DevSampler>("sampler");
    ASSERT_TRUE(dev_ptr->set_conf("frequency", 100.0));
    ASSERT_TRUE(dev_ptr->set_conf_str("sample", "..out1=..in1"));
    ASSERT_TRUE(dev_ptr->set_conf_str("sample", "..out2=..in2"));

    ASSERT_TRUE(core.init());
    ASSERT_TRUE(core.start());

    Channel *in1 = core.get_channel("in1").value_or(nullptr);
    Channel *out1 = core.get_channel("out1").value_or(nullptr);
    Channel *in2 = core.get_channel("in2").value_or(nullptr);
    Channel *out2 = core.get_channel("out2").value_or(nullptr);
    ASSERT_NE(in1, nullptr);
    ASSERT_NE(out1, nullptr);
    ASSERT_NE(in2, nullptr);
    ASSERT_NE(out2, nullptr);

    ChannelWaiter out1_waiter(out1);
    ChannelWaiter out2_waiter(out2);

    EXPECT_TRUE(in1->write(ArrInt({42})));
    EXPECT_TRUE(out1_waiter.prev_wait_for(Duration(time::sec(1))));
    EXPECT_EQ(out1->read<int>(0), 42);

    EXPECT_TRUE(in2->write(ArrInt({43})));
    EXPECT_TRUE(out2_waiter.prev_wait_for(Duration(time::sec(1))));
    EXPECT_EQ(out2->read<int>(0), 43);

    ASSERT_TRUE(core.stop());
}

TEST_F(TestDevSampler, test_devsampler_handles_writes_during_start)
{
    Core core;
    DevSampler *dev_ptr = core.add_child<DevSampler>("sampler");
    ASSERT_TRUE(dev_ptr->set_conf("frequency", 100.0));
    // many mappings keep on_start filling its map while the writer already notifies the first input
    for (int i = 0; i < 500; ++i)
    {
        core.add_channel("in" + std::to_string(i), "int", 1);
        core.add_channel("out" + std::to_string(i), "int", 1);
        ASSERT_TRUE(dev_ptr->set_conf_str("sample", "..out" + std::to_string(i) + "=..in" + std::to_string(i)));
    }
    ASSERT_TRUE(core.init());

    Channel *in0 = core.get_channel("in0").value_or(nullptr);
    Channel *out0 = core.get_channel("out0").value_or(nullptr);
    ASSERT_NE(in0, nullptr);
    ASSERT_NE(out0, nullptr);

    std::atomic<bool> started = false;
    std::thread writer([in0, &started] {
        int value = 0;
        while (started == false)
            in0->write<int>(0, ++value);
    });

    const bool start = core.start();
    started = true;
    writer.join();
    ChannelWaiter out0_waiter(out0);
    in0->write<int>(0, -1);

    EXPECT_TRUE(start);
    EXPECT_TRUE(out0_waiter.prev_wait_for(Duration(time::sec(1))));
    EXPECT_EQ(out0->read<int>(0), -1);
    ASSERT_TRUE(core.stop());
}

} // namespace test
