#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <sihd/core/Channel.hpp>
#include <sihd/tui/channel_view.hpp>
#include <sihd/util/Array.hpp>
#include <sihd/util/Logger.hpp>

#include "render_helper.hpp"

namespace test
{

using namespace sihd::util;
using namespace sihd::core;
using namespace sihd::tui;

class TestChannelView: public ::testing::Test
{
    protected:
        TestChannelView() { LoggerManager::stream(); }

        virtual ~TestChannelView() { LoggerManager::clear_loggers(); }
};

TEST_F(TestChannelView, test_channel_str)
{
    Channel numbers("numbers", "int", 3);
    ASSERT_TRUE(numbers.write(ArrInt {1, -2, 3}));
    Channel name("name", "char", 8);
    ASSERT_TRUE(name.write(ArrChar {'h', 'o', 'm', 'e', '\0', 'x'}));

    EXPECT_EQ(channel_str(numbers), "1 -2 3");
    EXPECT_EQ(channel_str(name), "home");
}

TEST_F(TestChannelView, test_channel_gauge)
{
    Channel level("level", "double");
    ASSERT_TRUE(level.write<double>(0, 5.0));

    EXPECT_EQ(render_lines(channel_gauge(level, 0.0, 10.0), 10, 1)[0], "█████");
}

TEST_F(TestChannelView, test_channel_sparkline_samples_each_frame)
{
    Channel rate("rate", "int");
    ftxui::Component sparkline = ChannelSparkline(rate, 3);

    for (int value : {0, 7, 3, 7})
    {
        ASSERT_TRUE(rate.write<int>(0, value));
        sparkline->Render();
    }

    EXPECT_EQ(render_lines(sparkline->Render(), 10, 1)[0], "▁██");
}

} // namespace test
