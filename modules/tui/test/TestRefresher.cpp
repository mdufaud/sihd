#include <ftxui/dom/elements.hpp>
#include <gtest/gtest.h>

#include <sihd/tui/Refresher.hpp>
#include <sihd/util/Logger.hpp>

namespace test
{

using namespace sihd::util;
using namespace sihd::tui;

class TestRefresher: public ::testing::Test
{
    protected:
        TestRefresher(): _refresher([this] { ++_posts; }) { LoggerManager::stream(); }

        virtual ~TestRefresher() { LoggerManager::clear_loggers(); }

        int _posts = 0;
        Refresher _refresher;
};

TEST_F(TestRefresher, test_refresher_coalesces_until_rendered)
{
    _refresher.request();
    _refresher.request();
    EXPECT_EQ(_posts, 1);

    _refresher.rendered();
    _refresher.request();
    EXPECT_EQ(_posts, 2);
}

TEST_F(TestRefresher, test_refreshed_component_marks_the_frame)
{
    ftxui::Component component = RefreshOnRender(ftxui::Renderer([] { return ftxui::text("x"); }), _refresher);
    _refresher.request();

    component->Render();
    _refresher.request();

    EXPECT_EQ(_posts, 2);
}

} // namespace test
