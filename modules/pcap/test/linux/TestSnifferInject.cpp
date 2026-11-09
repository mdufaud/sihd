#include <array>
#include <string>
#include <thread>
#include <userns_child.hpp>

#include <gtest/gtest.h>

#include <sihd/pcap/Sniffer.hpp>
#include <sihd/sys/linux/ns.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/build.hpp>

#include "loopback.hpp"

namespace test
{
using namespace sihd::util;
using namespace sihd::sys;
using namespace sihd::sys::linux_;
using namespace sihd::pcap;
using namespace std::string_literals;

namespace
{

constexpr int child_ok = 0;
constexpr int child_loopback_down = 1;
constexpr int child_capture_failed = 2;
constexpr int child_injector_failed = 3;
constexpr int child_inject_failed = 4;
constexpr int child_wrong_inject_size = 5;
constexpr int child_no_packet = 6;
constexpr int child_wrong_frame = 7;
constexpr int child_loop_failed = 8;

int inject_on_loopback(int snaplen, size_t expected_size)
{
    const std::string frame = "\x02\x00\x00\x00\x00\x01\x02\x00\x00\x00\x00\x02\x88\xb5sihd inject"s;

    if (!set_loopback_up())
        return child_loopback_down;

    Sniffer capture("capture");
    if (!capture.open("lo") || !capture.set_snaplen(snaplen) || !capture.set_immediate(true)
        || !capture.set_timeout(5000) || !capture.activate() || !capture.set_filter("ether proto 0x88b5"))
        return child_capture_failed;

    Sniffer injector("injector");
    if (!injector.open("lo") || !injector.activate())
        return child_injector_failed;

    auto injected = injector.inject({frame.data(), frame.size()});
    if (!injected)
        return child_inject_failed;
    if (*injected != frame.size())
        return child_wrong_inject_size;

    auto received = capture.read_next();
    if (!received || !*received)
        return child_no_packet;
    ArrCharView view;
    if (!capture.get_read_data(view) || std::string(view.data(), view.size()) != frame.substr(0, expected_size)
        || capture.packet_len() != frame.size())
        return child_wrong_frame;
    return child_ok;
}

int inject_full_frame()
{
    return inject_on_loopback(65535, std::string::npos);
}

int inject_truncated_frame()
{
    return inject_on_loopback(16, 16);
}

int stop_capture_loop()
{
    if (!set_loopback_up())
        return child_loopback_down;

    Sniffer capture("capture");
    if (!capture.open("lo") || !capture.activate())
        return child_capture_failed;

    bool looped = false;
    std::thread loop([&] { looped = capture.start(); });
    const bool ready = capture.wait_ready(std::chrono::seconds(5));
    capture.stop();
    loop.join();
    return ready && looped ? child_ok : child_loop_failed;
}

} // namespace

class TestSnifferInject: public ::testing::Test
{
    protected:
        TestSnifferInject() { LoggerManager::stream(); }

        virtual ~TestSnifferInject() { LoggerManager::clear_loggers(); }
};

TEST_F(TestSnifferInject, test_inject_not_activated)
{
    if (os::is_run_by_qemu())
        GTEST_SKIP() << "qemu-user has no SIOCETHTOOL: pcap_create fails on ENOTTY";

    Sniffer sniffer("sniffer");
    const std::string frame = "frame";
    ASSERT_TRUE(sniffer.open("lo").has_value());

    auto injected = sniffer.inject({frame.data(), frame.size()});

    ASSERT_FALSE(injected.has_value());
    EXPECT_EQ(injected.error().code, ErrorCode::not_initialized);
}

TEST_F(TestSnifferInject, test_reopen_closes_previous_capture)
{
    if (os::is_run_by_qemu())
        GTEST_SKIP() << "qemu-user has no SIOCETHTOOL: pcap_create fails on ENOTTY";

    Sniffer sniffer("sniffer");
    ASSERT_TRUE(sniffer.open("lo").has_value());

    EXPECT_TRUE(sniffer.open("lo").has_value());

    EXPECT_TRUE(sniffer.is_open());
}

TEST_F(TestSnifferInject, test_stop_ends_capture_loop)
{
    if constexpr (build::is_run_with_tsan)
        GTEST_SKIP_("tsan kills a thread started in the forked child");

    constexpr std::array<ns::Type, 1> types = {ns::Type::net};

    expect_in_userns(types, stop_capture_loop);
}

TEST_F(TestSnifferInject, test_inject_loopback)
{
    constexpr std::array<ns::Type, 1> types = {ns::Type::net};

    expect_in_userns(types, inject_full_frame);
}

TEST_F(TestSnifferInject, test_capture_shorter_than_packet)
{
    constexpr std::array<ns::Type, 1> types = {ns::Type::net};

    expect_in_userns(types, inject_truncated_frame);
}

} // namespace test
