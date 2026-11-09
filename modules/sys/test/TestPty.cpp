#include <array>
#include <string>
#include <string_view>
#include <wait_for.hpp>

#include <gtest/gtest.h>

#include <sihd/sys/Poll.hpp>
#include <sihd/sys/Pty.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/build.hpp>
#include <sihd/util/time.hpp>

#if defined(__SIHD_LINUX__)
# include <unistd.h>
#endif

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;
using namespace sihd::sys;

class TestPty: public ::testing::Test
{
    protected:
        TestPty() { LoggerManager::stream(); }

        virtual ~TestPty() { LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}

        static std::string read_until(Pty & pty, std::string_view text)
        {
            Poll poll(1);
            poll.set_read_fd(pty.read_fd());
            std::string out;
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
            while (out.find(text) == std::string::npos)
            {
                const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                    deadline - std::chrono::steady_clock::now());
                if (remaining.count() <= 0 || poll.poll(static_cast<int>(remaining.count())) <= 0)
                    break;
                std::array<char, 256> buf {};
                const ssize_t bytes = pty.read(buf.data(), buf.size());
                if (bytes <= 0)
                    break;
                out.append(buf.data(), static_cast<size_t>(bytes));
            }
            return out;
        }

        static void write_command(Pty & pty, std::string_view cmd)
        {
            ASSERT_EQ(pty.write(cmd.data(), cmd.size()), static_cast<ssize_t>(cmd.size()));
        }

        static bool wait_end(Pty & pty)
        {
            return wait_for([&pty] { return !pty.is_running(); }, std::chrono::seconds(5));
        }
};

TEST_F(TestPty, test_pty_spawn_echo_and_exit)
{
    if constexpr (build::is_emscripten)
        GTEST_SKIP() << "emscripten has no pty";
    if constexpr (!build::is_linux)
        GTEST_SKIP() << "the commands are posix shell ones";

    auto pty = Pty::create();
    ASSERT_NE(pty.get(), nullptr);
    ASSERT_TRUE(pty->spawn());
    ASSERT_TRUE(pty->is_running());
    ASSERT_GE(pty->read_fd(), 0);

    ASSERT_NO_FATAL_FAILURE(write_command(*pty, "echo sihd_pty_ok\n"));
    EXPECT_NE(read_until(*pty, "sihd_pty_ok").find("sihd_pty_ok"), std::string::npos);

    ASSERT_NO_FATAL_FAILURE(write_command(*pty, "exit\n"));
    ASSERT_TRUE(wait_end(*pty));
    EXPECT_EQ(pty->wait(), 0);
}

TEST_F(TestPty, test_pty_resize_send_eof)
{
    if constexpr (build::is_emscripten)
        GTEST_SKIP() << "emscripten has no pty";
    if constexpr (!build::is_linux)
        GTEST_SKIP() << "the commands are posix shell ones";

    auto pty = Pty::create();
    ASSERT_NE(pty.get(), nullptr);
    ASSERT_TRUE(pty->spawn());

    PtySize bigger {.cols = 120, .rows = 40, .xpixel = 0, .ypixel = 0};
    EXPECT_TRUE(pty->resize(bigger));
    // an EOF sent before the shell set its line editing up is lost: its prompt shows once it is set up
    ASSERT_NO_FATAL_FAILURE(write_command(*pty, "PS1=\"sihd\"\"> \"; echo ready-$((1+1))\n"));
    const std::string out = read_until(*pty, "sihd> ");
    ASSERT_NE(out.find("ready-2"), std::string::npos);

    pty->send_eof();

    EXPECT_TRUE(wait_end(*pty));
}

TEST_F(TestPty, test_pty_output_after_eof)
{
    if constexpr (build::is_emscripten)
        GTEST_SKIP() << "emscripten has no pty";
    if constexpr (!build::is_linux)
        GTEST_SKIP() << "the commands are posix shell ones";

    auto pty = Pty::create();
    ASSERT_NE(pty.get(), nullptr);
    ASSERT_TRUE(pty->spawn());
    // the echoed command holds "done-$((1+1))": only the shell output holds "done-2"
    ASSERT_NO_FATAL_FAILURE(write_command(*pty, "cat; echo done-$((1+1)); exit 0\n"));

    pty->send_eof();
    const std::string out = read_until(*pty, "done-2");

    EXPECT_NE(out.find("done-2"), std::string::npos);
    ASSERT_TRUE(wait_end(*pty));
    EXPECT_EQ(pty->wait(), 0);
}

TEST_F(TestPty, test_pty_terminates_child)
{
    if constexpr (build::is_emscripten)
        GTEST_SKIP() << "emscripten has no pty";
    if constexpr (!build::is_linux)
        GTEST_SKIP() << "the commands are posix shell ones";

    auto pty = Pty::create();
    ASSERT_NE(pty.get(), nullptr);
    pty->set_args({"-c", "sleep 30"});
    ASSERT_TRUE(pty->spawn());
    ASSERT_TRUE(pty->is_running());

    pty->terminate();

    EXPECT_TRUE(wait_end(*pty));
    EXPECT_NE(pty->wait(), -1);
}

} // namespace test
