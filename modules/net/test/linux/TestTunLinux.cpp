#include <sys/socket.h>

#include <array>
#include <map>
#include <string>
#include <string_view>
#include <userns_child.hpp>

#include <gtest/gtest.h>

#include <sihd/net/IpAddr.hpp>
#include <sihd/net/Socket.hpp>
#include <sihd/net/Tun.hpp>
#include <sihd/sys/EventPoll.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/sys/linux/ns.hpp>
#include <sihd/util/Array.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/time.hpp>

#include "netns_helpers.hpp"

namespace test
{
using namespace sihd::util;
using namespace sihd::sys;
using namespace sihd::sys::linux_;
using namespace sihd::net;
using namespace std::literals;

namespace
{

constexpr int receiver_port = 4300;

// 10.0.0.2:9 -> 10.0.0.1:4300 "pong", UDP checksum disabled
constexpr std::string_view pong_packet = "\x45\x00\x00\x20\x00\x00\x40\x00\x40\x11\x26\xcb\x0a\x00\x00\x02"
                                         "\x0a\x00\x00\x01\x00\x09\x10\xcc\x00\x0c\x00\x00pong"sv;

bool wait_readable(int fd)
{
    EventPoll poll;
    return poll.set_read_fd(fd) && poll.poll(2000) > 0;
}

int check_packets_cross_the_tun()
{
    Tun tun;
    auto opened = tun.open("sihdtun0");
    if (!opened)
        return tun_failure(opened.error());
    EXPECT_TRUE(configure_tun("sihdtun0", "10.0.0.1").has_value());

    std::array<char, 2048> packet {};
    auto nothing_yet = tun.read(packet.data(), packet.size());
    EXPECT_EQ(nothing_yet ? ErrorCode::none : nothing_yet.error().code, ErrorCode::would_block);

    Socket sender;
    EXPECT_TRUE(sender.open(AF_INET, SOCK_DGRAM, IPPROTO_UDP).has_value());
    EXPECT_TRUE(sender.send_to(IpAddr("10.0.0.2", 9), "ping").has_value());

    EXPECT_TRUE(tun.wait_readable(Duration(time::seconds(2))).value_or(false));
    auto read = tun.read(packet.data(), packet.size());
    EXPECT_EQ(read.value_or(0), 32u);
    EXPECT_EQ(packet[0], '\x45');
    EXPECT_EQ(packet[9], '\x11');
    EXPECT_EQ(std::string(packet.data() + 16, 4), "\x0a\x00\x00\x02"s);
    EXPECT_EQ(std::string(packet.data() + 28, 4), "ping");

    Socket receiver;
    EXPECT_TRUE(receiver.open(AF_INET, SOCK_DGRAM, IPPROTO_UDP).has_value());
    EXPECT_TRUE(receiver.bind(IpAddr("10.0.0.1", receiver_port)).has_value());

    EXPECT_EQ(tun.write({pong_packet.data(), pong_packet.size()}).value_or(0), pong_packet.size());

    EXPECT_TRUE(wait_readable(receiver.socket()));
    ArrChar received(16);
    EXPECT_EQ(receiver.receive(received).value_or(0), 4u);
    EXPECT_EQ(std::string(received.data(), 4), "pong");
    return child_result();
}

} // namespace

class TestTunLinux: public ::testing::Test
{
    protected:
        TestTunLinux() { LoggerManager::stream(); }

        virtual ~TestTunLinux() { LoggerManager::clear_loggers(); }
};

TEST_F(TestTunLinux, test_tun_packets_cross_the_interface)
{
    expect_in_userns(network_namespace, check_packets_cross_the_tun, tun_skipped_child_codes);
}

TEST_F(TestTunLinux, test_tun_adopts_a_packet_descriptor)
{
    std::array<int, 2> pair {};
    ASSERT_EQ(::socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_NONBLOCK, 0, pair.data()), 0);
    auto tun = Tun::adopt(pair[0]);
    ASSERT_TRUE(tun.has_value());
    Socket peer(pair[1]);
    std::array<char, 16> packet {};

    ASSERT_TRUE(peer.send_all("first").has_value());
    ASSERT_TRUE(peer.send_all("second").has_value());
    auto first = tun->read(packet.data(), packet.size());
    auto second = tun->read(packet.data(), packet.size());
    auto drained = tun->read(packet.data(), packet.size());

    EXPECT_EQ(tun->fd().value_or(-1), pair[0]);

    EXPECT_EQ(first.value_or(0), 5u);
    EXPECT_EQ(second.value_or(0), 6u);
    EXPECT_EQ(std::string(packet.data(), 6), "second");
    ASSERT_FALSE(drained.has_value());
    EXPECT_EQ(drained.error().code, ErrorCode::would_block);
}

TEST_F(TestTunLinux, test_tun_waits_for_a_packet)
{
    std::array<int, 2> pair {};
    ASSERT_EQ(::socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_NONBLOCK, 0, pair.data()), 0);
    auto tun = Tun::adopt(pair[0]);
    ASSERT_TRUE(tun.has_value());
    Socket peer(pair[1]);

    auto idle = tun->wait_readable(Duration(time::milli(10)));
    ASSERT_TRUE(peer.send_all("packet").has_value());
    auto pending = tun->wait_readable(Duration(time::seconds(2)));

    EXPECT_EQ(idle.value_or(true), false);
    EXPECT_EQ(pending.value_or(false), true);
}

} // namespace test
