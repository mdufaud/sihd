#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/util/FS.hpp>
#include <sihd/util/OS.hpp>
#include <sihd/util/Term.hpp>
#include <sihd/util/Handler.hpp>
#include <sihd/core/DevSampler.hpp>
#include <sihd/core/Core.hpp>
#include <sihd/core/ChannelWaiter.hpp>

namespace test
{
    SIHD_LOGGER;
    using namespace sihd::core;
    using namespace sihd::util;
    class TestDevSampler: public ::testing::Test
    {
        protected:
            TestDevSampler()
            {
                char *test_path = getenv("TEST_PATH");
                _base_test_dir = sihd::util::FS::combine({
                    test_path == nullptr ? "unit_test" : test_path,
                    "core",
                    "devsampler"
                });
                _cwd = sihd::util::OS::cwd();
                sihd::util::LoggerManager::basic();
                sihd::util::FS::make_directories(_base_test_dir);
            }

            virtual ~TestDevSampler()
            {
                sihd::util::LoggerManager::clear_loggers();
            }

            virtual void SetUp()
            {
            }

            virtual void TearDown()
            {
            }

            std::string _cwd;
            std::string _base_test_dir;
    };

    TEST_F(TestDevSampler, test_devsampler)
    {
        Core core;

        DevSampler *dev_ptr = core.add_child<DevSampler>("sampler");
        // 1000hz = 1000/s = 1ms
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
        sihd::util::Handler<Channel *> counter([&notif] (Channel *c)
        {
            (void)c;
            ++notif;
        });
        out_channel->add_observer(&counter);

        ChannelWaiter waiter(out_channel);
        ASSERT_TRUE(in_channel->write(ArrInt({0, 0, 1})));
        waiter.wait_for(sihd::util::Time::sec(1));

        ASSERT_TRUE(in_channel->write(ArrInt({1, 2, 3})));
        ASSERT_TRUE(in_channel->write(ArrInt({2, 3, 4})));
        ASSERT_TRUE(in_channel->write(ArrInt({3, 4, 5})));
        waiter.wait_for(sihd::util::Time::sec(1));
        EXPECT_EQ(out_channel->array()->str(','), "3,4,5");
        EXPECT_EQ(notif, 2);

        ASSERT_TRUE(in_channel->write(ArrInt({1, 2, 3})));
        ASSERT_TRUE(in_channel->write(ArrInt({2, 3, 4})));
        ASSERT_TRUE(in_channel->write(ArrInt({4, 5, 6})));
        waiter.wait_for(sihd::util::Time::sec(1));
        EXPECT_EQ(out_channel->array()->str(','), "4,5,6");
        EXPECT_EQ(notif, 3);

        ASSERT_TRUE(in_channel->write(ArrInt({1, 2, 3})));
        ASSERT_TRUE(in_channel->write(ArrInt({2, 3, 4})));
        ASSERT_TRUE(in_channel->write(ArrInt({5, 6, 7})));
        waiter.wait_for(sihd::util::Time::sec(1));
        EXPECT_EQ(out_channel->array()->str(','), "5,6,7");
        EXPECT_EQ(notif, 4);
    }

}