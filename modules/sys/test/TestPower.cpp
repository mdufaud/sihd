#include <gtest/gtest.h>

#include <sihd/sys/power.hpp>
#include <sihd/util/Logger.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;
using namespace sihd::sys;

class TestPower: public ::testing::Test
{
    protected:
        TestPower() { LoggerManager::stream(); }

        virtual ~TestPower() { LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestPower, test_power_batteries)
{
    const auto batteries = power::batteries();
    for (const auto & battery : batteries)
    {
        EXPECT_FALSE(battery.name.empty());
        if (battery.capacity_percent.has_value())
        {
            EXPECT_LE(*battery.capacity_percent, 100);
            SIHD_LOG(debug, "battery '{}': {}%", battery.name, *battery.capacity_percent);
        }
    }
}

TEST_F(TestPower, test_power_ac_lines)
{
    const auto ac_lines = power::ac_lines();
    for (const auto & ac_line : ac_lines)
    {
        EXPECT_FALSE(ac_line.name.empty());
        SIHD_LOG(debug, "ac line '{}': {}", ac_line.name, ac_line.online ? "online" : "offline");
    }
}

} // namespace test
