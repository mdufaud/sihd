#include <gtest/gtest.h>

#include <sihd/core/ChannelWaiter.hpp>
#include <sihd/core/Core.hpp>
#include <sihd/core/DevSampler.hpp>

#include <sihd/util/Array.hpp>
#include <sihd/util/Handler.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/util/platform.hpp>
#include <sihd/util/term.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::core;
using namespace sihd::util;
using namespace sihd::sys;
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

    Channel *in_channel = core.get_channel("in_channel");
    Channel *out_channel = core.get_channel("out_channel");
    Channel *sample_channel = dev_ptr->get_channel("sample");
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
    EXPECT_TRUE(out_channel_waiter.prev_wait_for(time::sec(1)));
    EXPECT_EQ(out_channel->array()->str(','), "0,0,1");
    EXPECT_EQ(out_channel_notif, 1);

    ASSERT_TRUE(in_channel->write(ArrInt({1, 2, 3})));
    ASSERT_TRUE(in_channel->write(ArrInt({2, 3, 4})));
    ASSERT_TRUE(in_channel->write(ArrInt({3, 4, 5})));
    EXPECT_TRUE(out_channel_waiter.prev_wait_for(time::sec(1)));
    EXPECT_EQ(out_channel->array()->str(','), "3,4,5");
    EXPECT_EQ(out_channel_notif, 2);

    ASSERT_TRUE(in_channel->write(ArrInt({1, 2, 3})));
    ASSERT_TRUE(in_channel->write(ArrInt({2, 3, 4})));
    ASSERT_TRUE(in_channel->write(ArrInt({4, 5, 6})));
    EXPECT_TRUE(out_channel_waiter.prev_wait_for(time::sec(1)));
    EXPECT_EQ(out_channel->array()->str(','), "4,5,6");
    EXPECT_EQ(out_channel_notif, 3);

    ASSERT_TRUE(in_channel->write(ArrInt({1, 2, 3})));
    ASSERT_TRUE(in_channel->write(ArrInt({2, 3, 4})));
    ASSERT_TRUE(in_channel->write(ArrInt({5, 6, 7})));
    EXPECT_TRUE(out_channel_waiter.prev_wait_for(time::sec(1)));
    EXPECT_EQ(out_channel->array()->str(','), "5,6,7");
    EXPECT_EQ(out_channel_notif, 4);

    ASSERT_TRUE(core.stop());
}

} // namespace test
