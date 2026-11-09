#include <chrono>
#include <future>
#include <thread>

#include <gtest/gtest.h>

#include <sihd/net/Pinger.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/sys/platform.hpp>
#include <sihd/sys/program.hpp>
#include <sihd/util/Array.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/ansi.hpp>
#include <sihd/util/build.hpp>
#include <sihd/util/time.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::net;
using namespace sihd::util;
using namespace sihd::sys;
class TestPinger: public ::testing::Test
{
    protected:
        TestPinger() { LoggerManager::stream(); }

        virtual ~TestPinger() { LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}

        static std::string raw_socket_skip_reason()
        {
            return "Must be root or have capabilities to do the test\nexecute command: 'sudo setcap cap_net_raw=pe "
                   + program::path() + "'\n";
        }
};

TEST_F(TestPinger, test_pinger)
{
    if constexpr (build::is_windows)
        GTEST_SKIP() << "raw ICMPv4 sockets not functional under wine/mingw";

    Pinger pinger("pinger");

    if (pinger.open(false).has_value() == false)
        GTEST_SKIP() << raw_socket_skip_reason();

    pinger.set_interval(200);
    pinger.set_ping_count(10);
    pinger.set_client({"8.8.8.8"});

    ASSERT_TRUE(pinger.start());
    ASSERT_TRUE(pinger.stop());

    auto result = pinger.result();
    SIHD_COUT("{}\n", result.str());

    EXPECT_EQ(result.transmitted, 10);
    EXPECT_EQ(result.received, 10);
}

TEST_F(TestPinger, test_pinger_ipv6)
{
    Pinger pinger("pinger");

    if (pinger.open(true).has_value() == false)
        GTEST_SKIP() << raw_socket_skip_reason();

    pinger.set_interval(200);
    pinger.set_ping_count(10);
    pinger.set_client(IpAddr("::1"));

    ASSERT_TRUE(pinger.start());
    ASSERT_TRUE(pinger.stop());

    auto result = pinger.result();
    SIHD_COUT("{}\n", result.str());

    EXPECT_EQ(result.transmitted, 10);
    EXPECT_EQ(result.received, 10);
}

TEST_F(TestPinger, test_pinger_ignores_forged_reply)
{
    if constexpr (build::is_windows)
        GTEST_SKIP() << "raw ICMPv4 sockets not functional under wine/mingw";

    Pinger pinger("pinger");
    if (pinger.open(false).has_value() == false)
        GTEST_SKIP() << raw_socket_skip_reason();

    // queued on the pinger socket before the first request: the pinger reads it first
    IcmpSender forger("forger");
    ASSERT_TRUE(forger.open_socket(false).has_value());
    forger.set_type(0);
    forger.set_code(0);
    // the pinger echo id is random and private: the zeroed payload fails the nonce check whatever the id
    forger.set_id(0xBEEF);
    forger.set_seq(1);
    forger.set_data_size(56u);
    ArrByte zeros(56);
    forger.set_data(zeros);
    ASSERT_TRUE(forger.send_to(IpAddr("127.0.0.1")).has_value());

    pinger.set_interval(100);
    pinger.set_ping_count(1);
    pinger.set_client({"127.0.0.1"});

    ASSERT_TRUE(pinger.start());
    ASSERT_TRUE(pinger.stop());

    const auto result = pinger.result();
    EXPECT_EQ(result.transmitted, 1);
    EXPECT_EQ(result.received, 1);
    EXPECT_LT(result.rtt.max, time::sec(1));
}

TEST_F(TestPinger, test_pinger_reply_timeout_is_a_deadline)
{
    if constexpr (build::is_windows)
        GTEST_SKIP() << "raw ICMPv4 sockets not functional under wine/mingw";

    Pinger pinger("pinger");
    if (pinger.open(false).has_value() == false)
        GTEST_SKIP() << raw_socket_skip_reason();
    pinger.set_ping_count(1);
    pinger.set_timeout(200);
    // documentation range: no host ever answers
    pinger.set_client({"192.0.2.1"});

    // every echo on the loopback wakes the pinger socket, for over a second
    IcmpSender chatter("chatter");
    ASSERT_TRUE(chatter.open_socket(false).has_value());
    chatter.set_echo();
    std::thread chat([&chatter] {
        for (int i = 0; i < 60; ++i)
        {
            (void)chatter.send_to(IpAddr("127.0.0.1"));
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    });

    const auto begin = std::chrono::steady_clock::now();
    const bool started = pinger.start();
    const auto elapsed = std::chrono::steady_clock::now() - begin;
    chat.join();

    ASSERT_TRUE(started);
    EXPECT_EQ(pinger.result().transmitted, 1);
    EXPECT_EQ(pinger.result().received, 0);
    EXPECT_LT(elapsed, std::chrono::seconds(1));
}

TEST_F(TestPinger, test_pinger_unix_socket_refused)
{
    Pinger pinger("pinger");

    const auto opened = pinger.open_unix();

    ASSERT_FALSE(opened.has_value());
    EXPECT_EQ(opened.error().code, ErrorCode::not_supported);
}

TEST_F(TestPinger, test_pinger_stops_right_after_ready)
{
    if constexpr (build::is_windows)
        GTEST_SKIP() << "raw ICMPv4 sockets not functional under wine/mingw";

    Pinger pinger("pinger");
    if (pinger.open(false).has_value() == false)
        GTEST_SKIP() << raw_socket_skip_reason();
    pinger.set_interval(50);
    pinger.set_ping_count(0);
    pinger.set_client({"127.0.0.1"});

    auto run = std::async(std::launch::async, [&pinger] { return pinger.start(); });
    ASSERT_TRUE(pinger.wait_ready(std::chrono::seconds(2)));

    // a lost stop never returns: the pinger pings forever
    EXPECT_TRUE(pinger.stop());
    EXPECT_TRUE(run.get());
}

} // namespace test
