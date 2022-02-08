#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/core/Core.hpp>
#include <sihd/core/ChannelWaiter.hpp>
#include <sihd/core/DevPulsation.hpp>

namespace test
{
    SIHD_LOGGER;
    using namespace sihd::core;
    class TestDevPulsation:   public ::testing::Test
    {
        protected:
            TestDevPulsation()
            {
                sihd::util::LoggerManager::basic();
            }

            virtual ~TestDevPulsation()
            {
                sihd::util::LoggerManager::clear_loggers();
            }

            virtual void SetUp()
            {
            }

            virtual void TearDown()
            {
            }
    };

    TEST_F(TestDevPulsation, test_dev_pulsation)
    {
        Core core;

        DevPulsation dev("pulsation", &core);
        dev.set_parent_ownership(false);
        // 1000hz = 1000/s = 1ms
        EXPECT_TRUE(dev.set_conf("frequency", 1000.0));

        EXPECT_TRUE(core.init());
        EXPECT_TRUE(core.start());

        Channel *activate = dev.get_channel("activate");
        Channel *beat = dev.get_channel("heartbeat");
        EXPECT_NE(activate, nullptr);
        EXPECT_NE(beat, nullptr);
        if (activate != nullptr && beat != nullptr)
        {
            activate->write(0, true);
            SIHD_LOG(debug, "Activated pulsation");
            // wait for 4 pulsation in 6 ms
            ChannelWaiter waiter(beat);
            EXPECT_TRUE(waiter.wait_for(sihd::util::time::milli(6), 4));
        }
        EXPECT_TRUE(core.stop());
        EXPECT_TRUE(core.reset());

        // reset
        DevPulsation dev2("pulsation", &core);
        dev2.set_parent_ownership(false);
        EXPECT_TRUE(dev2.set_conf("frequency", 1000.0));
        EXPECT_TRUE(core.init());
        EXPECT_TRUE(core.start());

        activate = dev2.get_channel("activate");
        beat = dev2.get_channel("heartbeat");
        EXPECT_NE(activate, nullptr);
        EXPECT_NE(beat, nullptr);
        if (activate != nullptr && beat != nullptr)
        {
            activate->write(0, true);
            SIHD_LOG(debug, "Activated pulsation");
            // wait for 4 pulsation in 6 ms
            ChannelWaiter waiter(beat);
            EXPECT_TRUE(waiter.wait_for(sihd::util::time::milli(6), 4));
        }
        EXPECT_TRUE(core.stop());
        EXPECT_TRUE(core.reset());
    }
}