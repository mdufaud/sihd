#include <gtest/gtest.h>

#include <sihd/sys/console.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/build.hpp>

#if defined(__SIHD_WINDOWS__)
# include <fcntl.h>
# include <io.h>
#else
# include <unistd.h>
#endif

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;
using namespace sihd::sys;

namespace
{

bool make_pipe(int fds[2])
{
#if defined(__SIHD_WINDOWS__)
    return _pipe(fds, 4096, _O_BINARY) == 0;
#else
    return ::pipe(fds) == 0;
#endif
}

void close_pipe(int fds[2])
{
#if defined(__SIHD_WINDOWS__)
    _close(fds[0]);
    _close(fds[1]);
#else
    ::close(fds[0]);
    ::close(fds[1]);
#endif
}

} // namespace

class TestConsole: public ::testing::Test
{
    protected:
        TestConsole() { sihd::util::LoggerManager::stream(); }

        virtual ~TestConsole() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestConsole, test_console_size)
{
    if (console::is_interactive() == false)
        GTEST_SKIP() << "requires a tty";

    auto res = console::size(0);
    ASSERT_TRUE(res.has_value());
    SIHD_LOG(info, "Console size: {}x{}", res->cols, res->rows);
    EXPECT_GT(res->cols, 0);
    EXPECT_GT(res->rows, 0);
}

TEST_F(TestConsole, test_console_raw_mode)
{
    if (console::is_interactive() == false)
        GTEST_SKIP() << "requires a tty";

    console::RawMode raw(0);
    ASSERT_TRUE(raw.valid());
}

TEST_F(TestConsole, test_console_no_echo)
{
    if (console::is_interactive() == false)
        GTEST_SKIP() << "requires a tty";

    console::NoEcho no_echo(0);
    EXPECT_TRUE(no_echo.valid());
}

TEST_F(TestConsole, test_console_enable_vt)
{
    if constexpr (build::is_windows)
    {
        if (console::is_interactive() == false)
            GTEST_SKIP() << "requires a console";
    }
    EXPECT_TRUE(console::enable_vt(1).has_value());
}

TEST_F(TestConsole, test_console_not_a_console)
{
    int fds[2] = {-1, -1};
    ASSERT_TRUE(make_pipe(fds));

    auto res = console::size(fds[0]);
    EXPECT_FALSE(res.has_value());

    console::RawMode raw(fds[0]);
    EXPECT_FALSE(raw.valid());

    console::NoEcho no_echo(fds[0]);
    EXPECT_FALSE(no_echo.valid());

#if !defined(__SIHD_WINDOWS__)
    EXPECT_TRUE(console::enable_vt(fds[0]).has_value());
#endif

    close_pipe(fds);
}
} // namespace test
