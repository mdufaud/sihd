#include <sys/ioctl.h>
#include <sys/socket.h>

#include <array>
#include <string>
#include <userns_child.hpp>

#include <gtest/gtest.h>

#include <sihd/net/Socket.hpp>
#include <sihd/pcap/Sniffer.hpp>
#include <sihd/sys/linux/ns.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/util/Logger.hpp>

#include <net/if.h>

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

Result<void> set_loopback_up()
{
    sihd::net::Socket socket;
    if (auto opened = socket.open(AF_INET, SOCK_DGRAM, 0); !opened)
        return opened;
    struct ifreq ifr {};
    std::string_view("lo").copy(ifr.ifr_name, IFNAMSIZ - 1);
    if (auto flags = os::ioctl(socket.socket(), SIOCGIFFLAGS, &ifr); !flags)
        return flags;
    ifr.ifr_flags = static_cast<short>(ifr.ifr_flags | IFF_UP);
    return os::ioctl(socket.socket(), SIOCSIFFLAGS, &ifr);
}

int inject_on_loopback()
{
    const std::string frame = "\x02\x00\x00\x00\x00\x01\x02\x00\x00\x00\x00\x02\x88\xb5sihd inject"s;

    if (!set_loopback_up())
        return child_loopback_down;

    Sniffer capture("capture");
    if (!capture.open("lo") || !capture.set_immediate(true) || !capture.set_timeout(5000) || !capture.activate()
        || !capture.set_filter("ether proto 0x88b5"))
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
    if (!capture.get_read_data(view) || std::string(view.data(), view.size()) != frame)
        return child_wrong_frame;
    return child_ok;
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
    Sniffer sniffer("sniffer");
    const std::string frame = "frame";
    ASSERT_TRUE(sniffer.open("lo").has_value());

    auto injected = sniffer.inject({frame.data(), frame.size()});

    ASSERT_FALSE(injected.has_value());
    EXPECT_EQ(injected.error().code, ErrorCode::not_initialized);
}

TEST_F(TestSnifferInject, test_inject_loopback)
{
    constexpr std::array<ns::Type, 1> types = {ns::Type::net};

    expect_in_userns(types, inject_on_loopback);
}

} // namespace test
