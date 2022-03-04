#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Files.hpp>
#include <sihd/util/OS.hpp>
#include <sihd/util/Term.hpp>
#include <sihd/util/Handler.hpp>
#include <sihd/core/DevSampler.hpp>
#include <sihd/core/Core.hpp>

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
                _base_test_dir = sihd::util::Files::combine({
                    test_path == nullptr ? "unit_test" : test_path,
                    "core",
                    "devsampler"
                });
                _cwd = sihd::util::OS::get_cwd();
                sihd::util::LoggerManager::basic();
                sihd::util::Files::make_directories(_base_test_dir);
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

        // sync on first TODO ChannelWaitARR

        ASSERT_TRUE(in_channel->write(ArrInt({1, 2, 3})));
        ASSERT_TRUE(in_channel->write(ArrInt({2, 3, 4})));
        ASSERT_TRUE(in_channel->write(ArrInt({3, 4, 5})));
        SIHD_TRACE(out_channel->array()->to_string(','));
        usleep(1E4);
        ASSERT_TRUE(in_channel->write(ArrInt({1, 2, 3})));
        ASSERT_TRUE(in_channel->write(ArrInt({2, 3, 4})));
        ASSERT_TRUE(in_channel->write(ArrInt({3, 4, 5})));
        SIHD_TRACE(out_channel->array()->to_string(','));
        usleep(1E4);
        ASSERT_TRUE(in_channel->write(ArrInt({1, 2, 3})));
        ASSERT_TRUE(in_channel->write(ArrInt({2, 3, 4})));
        ASSERT_TRUE(in_channel->write(ArrInt({3, 4, 5})));
        SIHD_TRACE(out_channel->array()->to_string(','));
    }

}