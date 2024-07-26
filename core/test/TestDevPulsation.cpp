#include <gtest/gtest.h>

#include <sihd/core/ChannelWaiter.hpp>
#include <sihd/core/Core.hpp>
#include <sihd/core/DevPulsation.hpp>
#include <sihd/util/Handler.hpp>
#include <sihd/util/Logger.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::core;
class TestDevPulsation: public ::testing::Test
{
    protected:
        TestDevPulsation() { sihd::util::LoggerManager::basic(); }

        virtual ~TestDevPulsation() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestDevPulsation, test_dev_pulsation)
{
    Core core;

    DevPulsation *dev_ptr = core.add_child<DevPulsation>("pulsation");
    // 1000hz = 1000/s = 1ms
    EXPECT_TRUE(dev_ptr->set_conf("frequency", 1000.0));

    EXPECT_TRUE(core.init());
    EXPECT_TRUE(core.start());

    Channel *activate = dev_ptr->get_channel("activate");
    Channel *beat = dev_ptr->get_channel("heartbeat");
    ASSERT_NE(activate, nullptr);
    ASSERT_NE(beat, nullptr);

    {
        ChannelWaiter waiter(beat);

        activate->write(0, true);
        SIHD_LOG(debug, "Activated pulsation");
        // wait for 4 pulsations
        EXPECT_TRUE(waiter.wait_for_nb(std::chrono::milliseconds(1000), 4));
    }

    EXPECT_TRUE(core.stop());
    EXPECT_TRUE(core.reset());

    // reset
    DevPulsation *dev2_ptr = core.add_child<DevPulsation>("pulsation");
    EXPECT_TRUE(dev2_ptr->set_conf("frequency", 1000.0));
    EXPECT_TRUE(core.init());
    EXPECT_TRUE(core.start());

    activate = dev2_ptr->get_channel("activate");
    beat = dev2_ptr->get_channel("heartbeat");
    ASSERT_NE(activate, nullptr);
    ASSERT_NE(beat, nullptr);

    {
        ChannelWaiter waiter(beat);

        activate->write(0, true);
        SIHD_LOG(debug, "Activated pulsation");
        // wait for 4 pulsation

        EXPECT_TRUE(waiter.wait_for_nb(std::chrono::milliseconds(1000), 4));
    }

    EXPECT_TRUE(core.stop());
    EXPECT_TRUE(core.reset());
}
} // namespace test
