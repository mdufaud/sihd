#include <sys/ioctl.h>
#include <sys/socket.h>

#include <array>
#include <string>

#include <gtest/gtest.h>

#include <sihd/net/Socket.hpp>
#include <sihd/pcap/Sniffer.hpp>
#include <sihd/sys/Process.hpp>
#include <sihd/sys/fs.hpp>
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
constexpr int child_error_base = 10;

int child_code(ErrorCode code)
{
    return child_error_base + static_cast<int>(code);
}

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
    auto apparmor_restrict = fs::read_all("/proc/sys/kernel/apparmor_restrict_unprivileged_userns");
    if (apparmor_restrict && apparmor_restrict->starts_with('1'))
        GTEST_SKIP() << "user namespaces confined by apparmor";

    Process proc([]() -> int {
        constexpr std::array<ns::Type, 2> types = {ns::Type::user, ns::Type::net};
        auto unshared = ns::unshare(types);
        if (!unshared)
            return child_code(unshared.error().code);
        return inject_on_loopback();
    });
    proc.set_force_fork(true);

    ASSERT_TRUE(proc.execute());
    ASSERT_TRUE(proc.wait_exit().value_or(false));

    const int code = static_cast<int>(proc.return_code());
    if (code == child_code(ErrorCode::permission_denied))
        GTEST_SKIP() << "user namespaces denied";
    if (code == child_code(ErrorCode::invalid_argument))
        GTEST_SKIP() << "process is not single-threaded (qemu-user)";
    EXPECT_EQ(code, child_ok);
}

} // namespace test
