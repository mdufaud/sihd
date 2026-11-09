#include <array>
#include <cstring>
#include <string>

#include <gtest/gtest.h>

#include <sihd/sys/Pty.hpp>
#include <sihd/util/Logger.hpp>
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
            std::string out;
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
            while (out.find(text) == std::string::npos && std::chrono::steady_clock::now() < deadline)
            {
                std::array<char, 256> buf {};
                const ssize_t bytes = pty.read(buf.data(), buf.size());
                if (bytes > 0)
                    out.append(buf.data(), (size_t)bytes);
                else if (bytes < 0)
                    break;
                else
                    time::msleep(10);
            }
            return out;
        }

        static void wait_end(Pty & pty)
        {
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
            while (pty.is_running() && std::chrono::steady_clock::now() < deadline)
                time::msleep(10);
        }
};

#if defined(__SIHD_LINUX__)
TEST_F(TestPty, test_pty_spawn_echo_and_exit)
{
    if constexpr (build::is_emscripten)
        GTEST_SKIP() << "emscripten has no pty";

    auto pty = Pty::create();
    ASSERT_NE(pty.get(), nullptr);
    ASSERT_TRUE(pty->spawn());
    ASSERT_TRUE(pty->is_running());
    ASSERT_GE(pty->read_fd(), 0);

    const char cmd[] = "echo sihd_pty_ok\n";
    ASSERT_EQ(pty->write(cmd, strlen(cmd)), (ssize_t)strlen(cmd));

    std::string out;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (out.find("sihd_pty_ok") == std::string::npos && std::chrono::steady_clock::now() < deadline)
    {
        char buf[256];
        const ssize_t bytes = pty->read(buf, sizeof(buf));
        if (bytes > 0)
            out.append(buf, (size_t)bytes);
        else
            time::msleep(10);
    }
    EXPECT_NE(out.find("sihd_pty_ok"), std::string::npos);

    ASSERT_EQ(pty->write("exit\n", 5), 5);
    const auto exit_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (pty->is_running() && std::chrono::steady_clock::now() < exit_deadline)
        time::msleep(10);
    EXPECT_FALSE(pty->is_running());
    if (!pty->is_running())
    {
        EXPECT_EQ(pty->wait(), 0);
    }
}
#endif

#if defined(__SIHD_LINUX__)
TEST_F(TestPty, test_pty_resize_send_eof)
{
    if constexpr (build::is_emscripten)
        GTEST_SKIP() << "emscripten has no pty";

    auto pty = Pty::create();
    ASSERT_NE(pty.get(), nullptr);
    ASSERT_TRUE(pty->spawn());

    PtySize bigger {.cols = 120, .rows = 40, .xpixel = 0, .ypixel = 0};
    EXPECT_TRUE(pty->resize(bigger));
    // an EOF sent before the shell set its line editing up is lost: wait for it to answer
    const char cmd[] = "echo ready-$((1+1))\n";
    ASSERT_EQ(pty->write(cmd, strlen(cmd)), (ssize_t)strlen(cmd));
    ASSERT_NE(read_until(*pty, "ready-2").find("ready-2"), std::string::npos);

    pty->send_eof();
    wait_end(*pty);

    EXPECT_FALSE(pty->is_running());
}
#endif

#if defined(__SIHD_LINUX__)
TEST_F(TestPty, test_pty_output_after_eof)
{
    if constexpr (build::is_emscripten)
        GTEST_SKIP() << "emscripten has no pty";

    auto pty = Pty::create();
    ASSERT_NE(pty.get(), nullptr);
    ASSERT_TRUE(pty->spawn());
    // the echoed command holds "done-$((1+1))": only the shell output holds "done-2"
    const char cmd[] = "cat; echo done-$((1+1)); exit 0\n";
    ASSERT_EQ(pty->write(cmd, strlen(cmd)), (ssize_t)strlen(cmd));

    pty->send_eof();
    const std::string out = read_until(*pty, "done-2");
    wait_end(*pty);

    EXPECT_NE(out.find("done-2"), std::string::npos);
    ASSERT_FALSE(pty->is_running());
    EXPECT_EQ(pty->wait(), 0);
}
#endif

#if defined(__SIHD_LINUX__)
TEST_F(TestPty, test_pty_terminates_child)
{
    if constexpr (build::is_emscripten)
        GTEST_SKIP() << "emscripten has no pty";

    auto pty = Pty::create();
    ASSERT_NE(pty.get(), nullptr);
    pty->set_args({"-c", "sleep 30"});
    ASSERT_TRUE(pty->spawn());
    ASSERT_TRUE(pty->is_running());

    pty->terminate();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (pty->is_running() && std::chrono::steady_clock::now() < deadline)
        time::msleep(10);
    EXPECT_FALSE(pty->is_running());
    EXPECT_NE(pty->wait(), -1);
}
#endif

} // namespace test
