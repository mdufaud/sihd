#include <thread>

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
using namespace sihd::util;
class TestDevPulsation: public ::testing::Test
{
    protected:
        TestDevPulsation() { LoggerManager::stream(); }

        virtual ~TestDevPulsation() { LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestDevPulsation, test_dev_pulsation)
{
    Core core;

    DevPulsation *dev_ptr = core.add_child<DevPulsation>("pulsation");
    EXPECT_TRUE(dev_ptr->set_conf("frequency", 1000.0));

    EXPECT_TRUE(core.init());
    EXPECT_TRUE(core.start());

    Channel *activate = dev_ptr->get_channel("activate").value_or(nullptr);
    Channel *beat = dev_ptr->get_channel("heartbeat").value_or(nullptr);
    ASSERT_NE(activate, nullptr);
    ASSERT_NE(beat, nullptr);

    {
        ChannelWaiter waiter(beat);

        activate->write(0, true);
        SIHD_LOG(debug, "Activated pulsation");
        EXPECT_TRUE(waiter.wait_for_nb(std::chrono::milliseconds(1000), 4));
    }

    EXPECT_TRUE(core.stop());
    EXPECT_TRUE(core.reset());

    DevPulsation *dev2_ptr = core.add_child<DevPulsation>("pulsation");
    EXPECT_TRUE(dev2_ptr->set_conf("frequency", 1000.0));
    EXPECT_TRUE(core.init());
    EXPECT_TRUE(core.start());

    activate = dev2_ptr->get_channel("activate").value_or(nullptr);
    beat = dev2_ptr->get_channel("heartbeat").value_or(nullptr);
    ASSERT_NE(activate, nullptr);
    ASSERT_NE(beat, nullptr);

    {
        ChannelWaiter waiter(beat);

        activate->write(0, true);
        SIHD_LOG(debug, "Activated pulsation");

        EXPECT_TRUE(waiter.wait_for_nb(std::chrono::milliseconds(1000), 4));
    }

    EXPECT_TRUE(core.stop());
    EXPECT_TRUE(core.reset());
}

TEST_F(TestDevPulsation, test_dev_pulsation_start_without_frequency)
{
    Core core;
    core.add_child<DevPulsation>("pulsation");
    EXPECT_TRUE(core.init());
    EXPECT_FALSE(core.start());
}

TEST_F(TestDevPulsation, test_dev_pulsation_pause_resume)
{
    Core core;
    DevPulsation *dev_ptr = core.add_child<DevPulsation>("pulsation");
    EXPECT_TRUE(dev_ptr->set_conf("frequency", 500.0));
    EXPECT_TRUE(core.init());
    EXPECT_TRUE(core.start());

    Channel *activate = dev_ptr->get_channel("activate").value_or(nullptr);
    Channel *beat = dev_ptr->get_channel("heartbeat").value_or(nullptr);
    ASSERT_NE(activate, nullptr);
    ASSERT_NE(beat, nullptr);

    {
        ChannelWaiter waiter(beat);

        activate->write(0, true);
        EXPECT_TRUE(waiter.wait_for_nb(std::chrono::milliseconds(1000), 4));

        activate->write(0, false);
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        const size_t beats_before = waiter.notifications();
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        EXPECT_EQ(waiter.notifications(), beats_before);

        activate->write(0, true);
        EXPECT_TRUE(waiter.wait_for_nb(std::chrono::milliseconds(1000), beats_before + 4));
    }

    EXPECT_TRUE(core.stop());
    EXPECT_TRUE(core.reset());
}
} // namespace test
