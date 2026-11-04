#include <gtest/gtest.h>

#include <sihd/util/Logger.hpp>
#include <sihd/wifi/frequency.hpp>

namespace test
{
using namespace sihd::util;
using namespace sihd::wifi;

class TestFrequency: public ::testing::Test
{
    protected:
        TestFrequency() { LoggerManager::stream(); }

        virtual ~TestFrequency() { LoggerManager::clear_loggers(); }
};

TEST_F(TestFrequency, test_frequency_channels_of_each_band)
{
    EXPECT_EQ(channel_from_frequency(2412), 1);
    EXPECT_EQ(channel_from_frequency(2472), 13);
    EXPECT_EQ(channel_from_frequency(2484), 14);
    EXPECT_EQ(channel_from_frequency(5180), 36);
    EXPECT_EQ(channel_from_frequency(5825), 165);
    EXPECT_EQ(channel_from_frequency(5955), 1);
    EXPECT_EQ(channel_from_frequency(7115), 233);
}

TEST_F(TestFrequency, test_frequency_outside_the_bands)
{
    EXPECT_EQ(channel_from_frequency(0), std::nullopt);
    EXPECT_EQ(channel_from_frequency(2400), std::nullopt);
    EXPECT_EQ(channel_from_frequency(2413), std::nullopt);
    EXPECT_EQ(channel_from_frequency(5950), std::nullopt);
    EXPECT_EQ(channel_from_frequency(60480), std::nullopt);
}

} // namespace test
