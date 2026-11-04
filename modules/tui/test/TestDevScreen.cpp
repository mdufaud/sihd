#include <gtest/gtest.h>

#include <sihd/core/Core.hpp>
#include <sihd/tui/DevScreen.hpp>
#include <sihd/util/Logger.hpp>

namespace test
{

using namespace sihd::util;
using namespace sihd::core;
using namespace sihd::tui;

class TestDevScreen: public ::testing::Test
{
    protected:
        TestDevScreen(): _refresher([this] { ++_posts; }) { LoggerManager::stream(); }

        virtual ~TestDevScreen() { LoggerManager::clear_loggers(); }

        int _posts = 0;
        Refresher _refresher;
};

TEST_F(TestDevScreen, test_devscreen_requests_a_frame_on_linked_writes)
{
    Core core;
    Channel *tick = core.add_channel("tick", "uint");
    Channel *count = core.add_channel("count", "int");
    DevScreen *screen = core.add_child<DevScreen>("screen");
    screen->set_refresher(&_refresher);
    ASSERT_TRUE(screen->add_link("tick", "..tick"));
    ASSERT_TRUE(screen->add_link("count", "..count"));
    ASSERT_TRUE(core.init());
    ASSERT_TRUE(core.start());

    ASSERT_TRUE(tick->write<uint32_t>(0, 1));
    ASSERT_TRUE(count->write<int>(0, 5));
    EXPECT_EQ(_posts, 1);

    _refresher.rendered();
    ASSERT_TRUE(count->write<int>(0, 6));
    EXPECT_EQ(_posts, 2);

    ASSERT_TRUE(core.stop());
    _refresher.rendered();
    ASSERT_TRUE(count->write<int>(0, 7));
    EXPECT_EQ(_posts, 2);
}

TEST_F(TestDevScreen, test_devscreen_needs_a_refresher)
{
    Core core;
    core.add_child<DevScreen>("screen");
    ASSERT_TRUE(core.init());

    EXPECT_FALSE(core.start());
}

} // namespace test
