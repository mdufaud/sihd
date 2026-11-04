#include <array>
#include <atomic>
#include <string>
#include <userns_child.hpp>

#include <gtest/gtest.h>

#include <sihd/core/Core.hpp>
#include <sihd/pcap/CapturedPacket.hpp>
#include <sihd/pcap/DevCapture.hpp>
#include <sihd/pcap/Sniffer.hpp>
#include <sihd/sys/linux/ns.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/util/Handler.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Synchronizer.hpp>

#include "loopback.hpp"

namespace test
{
using namespace sihd::util;
using namespace sihd::sys;
using namespace sihd::sys::linux_;
using namespace sihd::core;
using namespace sihd::pcap;
using namespace std::string_literals;

namespace
{

constexpr int child_ok = 0;
constexpr int child_loopback_down = 1;
constexpr int child_start_failed = 2;
constexpr int child_injector_failed = 3;
constexpr int child_no_packet = 4;

int capture_injected_frame()
{
    const std::string frame = "\x02\x00\x00\x00\x00\x01\x02\x00\x00\x00\x00\x02\x88\xb5sihd capture"s;

    if (!set_loopback_up())
        return child_loopback_down;

    Core core;
    DevCapture *capture = core.add_child<DevCapture>("capture");
    std::atomic<bool> seen = false;
    Synchronizer received(2);
    Handler<Channel *> observer([&](Channel *channel) {
        const Result<CapturedPacket> packet = CapturedPacket::decode(
            ArrUByteView(channel->data(), channel->byte_size()));
        if (packet && std::string(packet->data().begin(), packet->data().end()) == frame
            && seen.exchange(true) == false)
            received.sync();
    });
    if (!capture->set_interface("lo") || !core.init())
        return child_start_failed;
    capture->get_channel("packet").value()->add_observer(&observer);
    if (!core.start() || !capture->get_channel("activate").value()->write<bool>(0, true))
        return child_start_failed;

    Sniffer injector("injector");
    if (!injector.open("lo") || !injector.activate() || !injector.inject({frame.data(), frame.size()}))
        return child_injector_failed;
    const bool got = received.sync(Duration(time::sec(5)));

    (void)core.stop();
    return got ? child_ok : child_no_packet;
}

} // namespace

class TestDevCaptureLinux: public ::testing::Test
{
    protected:
        TestDevCaptureLinux() { LoggerManager::stream(); }

        virtual ~TestDevCaptureLinux() { LoggerManager::clear_loggers(); }
};

TEST_F(TestDevCaptureLinux, test_devcapture_captures_a_live_interface)
{
    if (os::is_run_by_qemu())
        GTEST_SKIP() << "qemu-user has no SIOCETHTOOL: pcap_create fails on ENOTTY";
    constexpr std::array<ns::Type, 1> types = {ns::Type::net};

    expect_in_userns(types, capture_injected_frame);
}

} // namespace test
