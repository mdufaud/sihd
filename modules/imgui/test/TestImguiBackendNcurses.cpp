#include <gtest/gtest.h>

#include <sihd/imgui/ImguiBackendNcurses.hpp>
#include <sihd/sys/Process.hpp>
#include <sihd/sys/env.hpp>
#include <sihd/util/Logger.hpp>

namespace test
{
using namespace sihd::imgui;
using namespace sihd::util;

class TestImguiBackendNcurses: public ::testing::Test
{
    protected:
        TestImguiBackendNcurses() { LoggerManager::stream(); }

        virtual ~TestImguiBackendNcurses() { LoggerManager::clear_loggers(); }
};

TEST_F(TestImguiBackendNcurses, test_init_refuses_an_unknown_terminal)
{
    constexpr int init_refused = 3;

    // ncurses may exit the process on a terminal it does not know: init runs in a child
    sihd::sys::Process child([]() -> int {
        (void)sihd::sys::env::set("TERM", "bogus");
        (void)sihd::sys::env::unset("COLORTERM");
        ImguiBackendNcurses backend;
        return backend.init() ? 0 : init_refused;
    });

    ASSERT_TRUE(child.execute());
    ASSERT_TRUE(child.wait_exit().value_or(false));

    EXPECT_EQ(child.return_code(), init_refused);
}

} // namespace test
