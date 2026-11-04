#include <clock_helper.hpp>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <sihd/core/Channel.hpp>
#include <sihd/core/TreeMonitor.hpp>
#include <sihd/tui/TreeTraceComponent.hpp>
#include <sihd/util/Logger.hpp>

#include "render_helper.hpp"

namespace test
{

using namespace sihd::util;
using namespace sihd::core;
using namespace sihd::tui;

class TestTreeTraceComponent: public ::testing::Test
{
    protected:
        TestTreeTraceComponent(): _refresher([this] { ++_posts; })
        {
            LoggerManager::stream();
            new Channel("speed", "int", &_root);
        }

        virtual ~TestTreeTraceComponent() { LoggerManager::clear_loggers(); }

        Node _root {"root"};
        TreeMonitor _monitor;
        int _posts = 0;
        Refresher _refresher;
};

TEST_F(TestTreeTraceComponent, test_trace_newest_first)
{
    FakeClock clock;
    _monitor.set_clock(&clock);
    ftxui::Component trace = TreeTraceComponent(_monitor, _refresher, 2);
    ASSERT_TRUE(_monitor.observe(&_root));

    Channel *speed = _root.get_child<Channel>("speed");
    ASSERT_NE(speed, nullptr);
    ASSERT_TRUE(speed->write<int>(0, 7));
    ASSERT_TRUE(_monitor.flush());
    EXPECT_EQ(_posts, 1);

    EXPECT_EQ(render_lines(trace->Render(), 32, 4),
              (std::vector<std::string> {"# v event   path       detail",
                                         "────────────────────────────────",
                                         "3   written root.speed +1ms:0us",
                                         "2   write   root.speed 7"}));
}

TEST_F(TestTreeTraceComponent, test_trace_keeps_at_least_one_event)
{
    FakeClock clock;
    _monitor.set_clock(&clock);
    ftxui::Component trace = TreeTraceComponent(_monitor, _refresher, 0);
    ASSERT_TRUE(_monitor.observe(&_root));

    Channel *speed = _root.get_child<Channel>("speed");
    ASSERT_NE(speed, nullptr);
    ASSERT_TRUE(speed->write<int>(0, 7));
    ASSERT_TRUE(_monitor.flush());

    EXPECT_EQ(render_lines(trace->Render(), 32, 3),
              (std::vector<std::string> {"# v event   path       detail",
                                         "────────────────────────────────",
                                         "3   written root.speed +1ms:0us"}));
}

} // namespace test
