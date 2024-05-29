#include <gtest/gtest.h>

#include <sihd/core/ChannelWaiter.hpp>
#include <sihd/core/Core.hpp>
#include <sihd/core/DevSampler.hpp>

#include <sihd/util/Array.hpp>
#include <sihd/util/Handler.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/fs.hpp>
#include <sihd/util/os.hpp>
#include <sihd/util/term.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::core;
using namespace sihd::util;
class TestDevSampler: public ::testing::Test
{
    protected:
        TestDevSampler() { sihd::util::LoggerManager::basic(); }

        virtual ~TestDevSampler() { sihd::util::LoggerManager::clear_loggers(); }

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
    ASSERT_NE(in_channel, nullptr);
    ASSERT_NE(out_channel, nullptr);

    int notif = 0;
    sihd::util::Handler<Channel *> counter([&notif](Channel *c) {
        (void)c;
        ++notif;
    });
    out_channel->add_observer(&counter);

    ChannelWaiter waiter(out_channel);
    ASSERT_TRUE(in_channel->write(ArrInt({0, 0, 1})));
    waiter.wait_for(sihd::util::time::sec(1));
    EXPECT_EQ(out_channel->array()->str(','), "0,0,1");
    EXPECT_EQ(notif, 1);

    ASSERT_TRUE(in_channel->write(ArrInt({1, 2, 3})));
    ASSERT_TRUE(in_channel->write(ArrInt({2, 3, 4})));
    ASSERT_TRUE(in_channel->write(ArrInt({3, 4, 5})));
    waiter.wait_for(sihd::util::time::sec(1));
    EXPECT_EQ(out_channel->array()->str(','), "3,4,5");
    EXPECT_EQ(notif, 2);

    ASSERT_TRUE(in_channel->write(ArrInt({1, 2, 3})));
    ASSERT_TRUE(in_channel->write(ArrInt({2, 3, 4})));
    ASSERT_TRUE(in_channel->write(ArrInt({4, 5, 6})));
    waiter.wait_for(sihd::util::time::sec(1));
    EXPECT_EQ(out_channel->array()->str(','), "4,5,6");
    EXPECT_EQ(notif, 3);

    ASSERT_TRUE(in_channel->write(ArrInt({1, 2, 3})));
    ASSERT_TRUE(in_channel->write(ArrInt({2, 3, 4})));
    ASSERT_TRUE(in_channel->write(ArrInt({5, 6, 7})));
    waiter.wait_for(sihd::util::time::sec(1));
    EXPECT_EQ(out_channel->array()->str(','), "5,6,7");
    EXPECT_EQ(notif, 4);
}

} // namespace test
