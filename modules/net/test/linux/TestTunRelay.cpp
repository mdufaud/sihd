#include <sys/socket.h>

#include <array>
#include <chrono>
#include <future>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <userns_child.hpp>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include <sihd/net/IpAddr.hpp>
#include <sihd/net/Socket.hpp>
#include <sihd/net/Tun.hpp>
#include <sihd/net/linux/TunRelay.hpp>
#include <sihd/sys/File.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/sys/linux/ns.hpp>
#include <sihd/util/Array.hpp>
#include <sihd/util/Defer.hpp>
#include <sihd/util/Handler.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Worker.hpp>
#include <sihd/util/build.hpp>

#include "netns_helpers.hpp"
#include "packet_forge.hpp"

namespace test
{
using namespace std::chrono_literals;
using namespace sihd::util;
using namespace sihd::net;
using namespace sihd::net::linux_;

namespace
{

constexpr int reply_timeout_milliseconds = 2000;
constexpr int silence_milliseconds = 300;
constexpr uint32_t client_isn = 1000;

const Endpoint app4 = {"10.0.0.2", 40000};
const Endpoint app6 = {"fd00::2", 40000};

struct Observed
{
        TunRelay::Direction direction;
        std::vector<uint8_t> packet;
};

struct Listener
{
        Socket socket;
        Endpoint endpoint;
};

Listener make_listener(const std::string & ip)
{
    const bool ipv6 = IpAddr(ip).is_ipv6();
    Listener listener;
    EXPECT_TRUE(listener.socket.open(ipv6 ? AF_INET6 : AF_INET, SOCK_STREAM, IPPROTO_TCP).has_value());
    EXPECT_TRUE(listener.socket.bind(IpAddr(ip, 0)).has_value());
    EXPECT_TRUE(listener.socket.listen(4).has_value());
    listener.endpoint = {ip, static_cast<uint16_t>(listener.socket.local_port().value_or(0))};
    return listener;
}

Socket accept_from(Listener & listener)
{
    Socket accepted(listener.socket.accept(reply_timeout_milliseconds).value_or(-1));
    EXPECT_TRUE(accepted.is_open());
    EXPECT_TRUE(accepted.set_recv_timeout(reply_timeout_milliseconds).has_value());
    return accepted;
}

Listener make_udp_server(const std::string & ip)
{
    const bool ipv6 = IpAddr(ip).is_ipv6();
    Listener server;
    EXPECT_TRUE(server.socket.open(ipv6 ? AF_INET6 : AF_INET, SOCK_DGRAM, IPPROTO_UDP).has_value());
    EXPECT_TRUE(server.socket.bind(IpAddr(ip, 0)).has_value());
    EXPECT_TRUE(server.socket.set_recv_timeout(reply_timeout_milliseconds).has_value());
    server.endpoint = {ip, static_cast<uint16_t>(server.socket.local_port().value_or(0))};
    return server;
}

std::string receive_request(Socket & server, IpAddr & from)
{
    std::array<char, 64> request {};
    const size_t size = server.receive_from(from, request.data(), request.size()).value_or(0);
    return std::string(request.data(), size);
}

std::string receive_string(Socket & socket)
{
    ArrChar buffer(4096);
    const size_t size = socket.receive(buffer).value_or(0);
    return std::string(buffer.data(), size);
}

constexpr size_t kernel_transfer_size = 1024 * 1024;
constexpr int kernel_timeout_milliseconds = 5000;

std::string pattern(size_t size, size_t seed)
{
    std::string data(size, '\0');
    for (size_t i = 0; i < size; ++i)
        data[i] = static_cast<char>((i + seed) % 251);
    return data;
}

std::string receive_until_end(Socket & socket)
{
    std::string received;
    std::array<char, 65536> buffer {};
    while (true)
    {
        auto size = socket.receive(buffer.data(), buffer.size());
        if (!size || *size == 0)
            return received;
        received.append(buffer.data(), *size);
    }
}

Result<Tun> open_app_namespace(Socket & tcp_client, Socket & udp_client)
{
    auto unshared = sihd::sys::linux_::ns::unshare(sihd::sys::linux_::ns::Type::net);
    SIHD_UNEXPECTED_RETURN(unshared);

    Tun tun;
    auto opened = tun.open("sihdtun0");
    SIHD_UNEXPECTED_RETURN(opened);
    auto configured = configure_tun("sihdtun0", "10.0.0.1");
    SIHD_UNEXPECTED_RETURN(configured);

    auto tcp_opened = tcp_client.open(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    SIHD_UNEXPECTED_RETURN(tcp_opened);
    auto udp_opened = udp_client.open(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    SIHD_UNEXPECTED_RETURN(udp_opened);
    return tun;
}

Result<void> open_relay_namespace(int relay_namespace)
{
    auto entered = sihd::sys::linux_::ns::setns(relay_namespace, sihd::sys::linux_::ns::Type::net);
    SIHD_UNEXPECTED_RETURN(entered);
    auto control = open_control();
    SIHD_UNEXPECTED_RETURN(control);
    auto loopback_up = set_up(*control, "lo", true);
    SIHD_UNEXPECTED_RETURN(loopback_up);
    return set_ipv4(*control, "lo:1", "10.0.0.2");
}

void exchange_tcp(Socket & client, Socket & listener)
{
    const int port = listener.local_port().value_or(0);
    const std::string upload = pattern(kernel_transfer_size, 1);
    const std::string download = pattern(kernel_transfer_size, 7);
    std::string uploaded;
    Worker server([&] {
        Socket accepted(listener.accept(kernel_timeout_milliseconds).value_or(-1));
        if (!accepted.set_recv_timeout(kernel_timeout_milliseconds))
            return false;
        uploaded = receive_until_end(accepted);
        return accepted.send_all(download).has_value() && accepted.shutdown().has_value();
    });
    EXPECT_TRUE(server.start_sync_worker("tcp-server"));

    EXPECT_TRUE(client.connect(IpAddr("10.0.0.2", port), kernel_timeout_milliseconds).has_value());
    EXPECT_TRUE(client.set_recv_timeout(kernel_timeout_milliseconds).has_value());
    EXPECT_TRUE(client.send_all(upload).has_value());
    EXPECT_TRUE(client.shutdown_send().has_value());
    const std::string downloaded = receive_until_end(client);
    server.stop_worker();

    EXPECT_EQ(uploaded.size(), upload.size());
    EXPECT_TRUE(uploaded == upload);
    EXPECT_EQ(downloaded.size(), download.size());
    EXPECT_TRUE(downloaded == download);
}

void exchange_udp(Socket & client, Socket & server)
{
    const int port = server.local_port().value_or(0);
    EXPECT_TRUE(server.set_recv_timeout(kernel_timeout_milliseconds).has_value());
    EXPECT_TRUE(client.set_recv_timeout(kernel_timeout_milliseconds).has_value());

    EXPECT_TRUE(client.send_to(IpAddr("10.0.0.2", port), "ping").has_value());
    IpAddr from;
    std::array<char, 64> request {};
    EXPECT_EQ(server.receive_from(from, request.data(), request.size()).value_or(0), 4u);
    EXPECT_TRUE(server.send_to(from, "pong").has_value());
    std::array<char, 64> reply {};
    const size_t reply_size = client.receive(reply.data(), reply.size()).value_or(0);

    EXPECT_EQ(std::string(reply.data(), reply_size), "pong");
}

int relay_kernel_traffic()
{
    sihd::sys::File relay_namespace("/proc/self/ns/net", "r");
    Socket tcp_client;
    Socket udp_client;
    auto tun = open_app_namespace(tcp_client, udp_client);
    if (!tun)
        return tun_failure(tun.error());
    if (!open_relay_namespace(relay_namespace.fd()))
    {
        ADD_FAILURE() << "namespace setup failed";
        return child_failed;
    }

    Socket listener;
    Socket udp_server;
    EXPECT_TRUE(listener.open(AF_INET, SOCK_STREAM, IPPROTO_TCP).has_value());
    EXPECT_TRUE(listener.bind(IpAddr("10.0.0.2", 0)).has_value());
    EXPECT_TRUE(listener.listen(1).has_value());
    EXPECT_TRUE(udp_server.open(AF_INET, SOCK_DGRAM, IPPROTO_UDP).has_value());
    EXPECT_TRUE(udp_server.bind(IpAddr("10.0.0.2", 0)).has_value());

    TunRelay relay("kernel-relay");
    EXPECT_TRUE(relay.set_tun(std::move(*tun)));
    Worker worker([&relay] { return relay.start(); });
    EXPECT_TRUE(worker.start_sync_worker("tun-relay"));
    Defer stop_relay([&] {
        relay.stop();
        worker.stop_worker();
    });
    EXPECT_TRUE(relay.wait_ready(std::chrono::seconds(2)));

    exchange_tcp(tcp_client, listener);
    exchange_udp(udp_client, udp_server);
    return child_result();
}

} // namespace

class TestTunRelay: public ::testing::Test
{
    protected:
        TestTunRelay() { LoggerManager::stream(); }

        virtual ~TestTunRelay() { LoggerManager::clear_loggers(); }

        void SetUp() override
        {
            this->attach_tun();
            _relay.add_observer(&_observer);
        }

        void attach_tun()
        {
            std::array<int, 2> pair {};
            ASSERT_EQ(::socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, pair.data()), 0);
            ASSERT_TRUE(sihd::sys::os::set_nonblocking(pair[0]).has_value());
            _peer = Socket(pair[1]);
            auto tun = Tun::adopt(pair[0]);
            ASSERT_TRUE(tun.has_value());
            ASSERT_TRUE(_relay.set_tun(std::move(*tun)));
        }

        void TearDown() override
        {
            _relay.stop();
            _worker.stop_worker();
        }

        void start_relay()
        {
            ASSERT_TRUE(_worker.start_sync_worker("tun-relay"));
            ASSERT_TRUE(_relay.wait_ready(std::chrono::seconds(2)));
        }

        void send(const std::vector<uint8_t> & packet) { ASSERT_TRUE(_peer.send_all(packet).has_value()); }

        std::optional<std::vector<uint8_t>> next_raw(int timeout_milliseconds = reply_timeout_milliseconds)
        {
            if (!_peer.set_recv_timeout(timeout_milliseconds))
                return std::nullopt;
            std::vector<uint8_t> packet(65536);
            auto received = _peer.receive(packet.data(), packet.size());
            if (!received)
                return std::nullopt;
            packet.resize(*received);
            return packet;
        }

        Decoded next_packet()
        {
            auto raw = this->next_raw();
            EXPECT_TRUE(raw.has_value());
            auto decoded = raw ? decode(*raw) : std::nullopt;
            EXPECT_TRUE(decoded.has_value());
            return decoded.value_or(Decoded {});
        }

        bool silent() { return !this->next_raw(silence_milliseconds).has_value(); }

        uint32_t handshake(const Endpoint & app, const Endpoint & remote, uint16_t mss = 1460, uint16_t window = 65535)
        {
            this->send(forge_tcp(app, remote, Segment {.seq = client_isn, .flags = syn, .window = window, .mss = mss}));
            const Decoded syn_ack = this->next_packet();
            EXPECT_EQ(syn_ack.segment.flags, syn | ack);
            this->send(forge_tcp(
                app,
                remote,
                Segment {.seq = client_isn + 1, .ack = syn_ack.segment.seq + 1, .flags = ack, .window = window}));
            return syn_ack.segment.seq;
        }

        std::vector<Observed> observed()
        {
            std::lock_guard lock(_observed_mutex);
            return _observed;
        }

        TunRelay _relay {"tun-relay"};
        Socket _peer;
        Worker _worker {[this] { return _relay.start(); }};
        std::mutex _observed_mutex;
        std::vector<Observed> _observed;
        Handler<TunRelay *> _observer {[this](TunRelay *relay) {
            const ArrUByteView packet = relay->packet();
            std::lock_guard lock(_observed_mutex);
            _observed.push_back(
                {relay->direction(), std::vector<uint8_t>(packet.data(), packet.data() + packet.size())});
        }};
};

TEST_F(TestTunRelay, test_udp_request_and_reply_ipv4)
{
    Listener server = make_udp_server("127.0.0.1");
    const Endpoint remote = server.endpoint;
    this->start_relay();

    this->send(forge_udp(app4, remote, "ping"));
    IpAddr from;
    const std::string request = receive_request(server.socket, from);
    ASSERT_TRUE(server.socket.send_to(from, "pong").has_value());
    const Decoded reply = this->next_packet();

    EXPECT_EQ(request, "ping");
    EXPECT_EQ(reply.version, 4);
    EXPECT_EQ(reply.protocol, protocol_udp);
    EXPECT_EQ(reply.src, remote);
    EXPECT_EQ(reply.dst, app4);
    EXPECT_EQ(reply.segment.payload, "pong");
    EXPECT_TRUE(reply.checksums_valid);
}

TEST_F(TestTunRelay, test_udp_request_and_reply_ipv6)
{
    Listener server = make_udp_server("::1");
    const Endpoint remote = server.endpoint;
    this->start_relay();

    this->send(forge_udp(app6, remote, "ping"));
    IpAddr from;
    ASSERT_EQ(receive_request(server.socket, from), "ping");
    ASSERT_TRUE(server.socket.send_to(from, "pong").has_value());
    const Decoded reply = this->next_packet();

    EXPECT_EQ(reply.version, 6);
    EXPECT_EQ(reply.src, remote);
    EXPECT_EQ(reply.dst, app6);
    EXPECT_EQ(reply.segment.payload, "pong");
    EXPECT_TRUE(reply.checksums_valid);
}

TEST_F(TestTunRelay, test_udp_flow_expires)
{
    Listener server = make_udp_server("127.0.0.1");
    const Endpoint remote = server.endpoint;
    constexpr auto udp_timeout = 100ms;
    ASSERT_TRUE(_relay.set_udp_timeout(Duration(udp_timeout)));
    this->start_relay();

    this->send(forge_udp(app4, remote, "ping"));
    IpAddr from;
    ASSERT_EQ(receive_request(server.socket, from), "ping");
    std::this_thread::sleep_for(udp_timeout * 4);
    ASSERT_TRUE(server.socket.send_to(from, "late").has_value());

    EXPECT_TRUE(this->silent());
}

TEST_F(TestTunRelay, test_unrelayed_packets_are_only_observed)
{
    Listener server = make_udp_server("127.0.0.1");
    const Endpoint remote = server.endpoint;
    std::vector<uint8_t> icmp = forge_udp(app4, remote, "icmp");
    icmp[ipv4_protocol_at] = protocol_icmp;
    const std::vector<uint8_t> garbage = {0x01, 0x02, 0x03};
    const std::vector<uint8_t> datagram = forge_udp(app4, remote, "udp");
    this->start_relay();

    this->send(garbage);
    this->send(icmp);
    this->send(datagram);
    IpAddr from;
    const std::string request = receive_request(server.socket, from);
    _relay.stop();
    _worker.stop_worker();
    const std::vector<Observed> observed = this->observed();

    EXPECT_EQ(request, "udp");
    ASSERT_EQ(observed.size(), 3u);
    EXPECT_EQ(observed[0].packet, garbage);
    EXPECT_EQ(observed[1].packet, icmp);
    EXPECT_EQ(observed[2].packet, datagram);
    EXPECT_EQ(observed[2].direction, TunRelay::Direction::from_tun);
}

TEST_F(TestTunRelay, test_observers_see_replies)
{
    Listener server = make_udp_server("127.0.0.1");
    const Endpoint remote = server.endpoint;
    this->start_relay();

    this->send(forge_udp(app4, remote, "ping"));
    IpAddr from;
    ASSERT_EQ(receive_request(server.socket, from), "ping");
    ASSERT_TRUE(server.socket.send_to(from, "pong").has_value());
    const std::optional<std::vector<uint8_t>> reply = this->next_raw();
    _relay.stop();
    _worker.stop_worker();
    const std::vector<Observed> observed = this->observed();

    ASSERT_TRUE(reply.has_value());
    ASSERT_EQ(observed.size(), 2u);
    EXPECT_EQ(observed[1].direction, TunRelay::Direction::to_tun);
    EXPECT_EQ(observed[1].packet, *reply);
}

TEST_F(TestTunRelay, test_tcp_handshake_ipv4)
{
    Listener listener = make_listener("127.0.0.1");
    this->start_relay();

    this->send(forge_tcp(app4, listener.endpoint, Segment {.seq = client_isn, .flags = syn, .mss = 1460}));
    const Decoded syn_ack = this->next_packet();
    Socket accepted = accept_from(listener);

    EXPECT_EQ(syn_ack.version, 4);
    EXPECT_EQ(syn_ack.src, listener.endpoint);
    EXPECT_EQ(syn_ack.dst, app4);
    EXPECT_EQ(syn_ack.segment.flags, syn | ack);
    EXPECT_EQ(syn_ack.segment.ack, client_isn + 1);
    EXPECT_EQ(syn_ack.segment.mss, 1460);
    EXPECT_TRUE(syn_ack.checksums_valid);
}

TEST_F(TestTunRelay, test_tcp_handshake_ipv6)
{
    Listener listener = make_listener("::1");
    this->start_relay();

    this->send(forge_tcp(app6, listener.endpoint, Segment {.seq = client_isn, .flags = syn, .mss = 1460}));
    const Decoded syn_ack = this->next_packet();
    Socket accepted = accept_from(listener);

    EXPECT_EQ(syn_ack.version, 6);
    EXPECT_EQ(syn_ack.src, listener.endpoint);
    EXPECT_EQ(syn_ack.dst, app6);
    EXPECT_EQ(syn_ack.segment.flags, syn | ack);
    EXPECT_EQ(syn_ack.segment.ack, client_isn + 1);
    EXPECT_EQ(syn_ack.segment.mss, 1440);
    EXPECT_TRUE(syn_ack.checksums_valid);
}

TEST_F(TestTunRelay, test_tcp_data_both_ways)
{
    Listener listener = make_listener("127.0.0.1");
    this->start_relay();
    const uint32_t relay_isn = this->handshake(app4, listener.endpoint);
    Socket accepted = accept_from(listener);

    this->send(
        forge_tcp(app4,
                  listener.endpoint,
                  Segment {.seq = client_isn + 1, .ack = relay_isn + 1, .flags = psh | ack, .payload = "hello"}));
    const Decoded hello_ack = this->next_packet();
    const std::string hello = receive_string(accepted);
    ASSERT_TRUE(accepted.send_all("world").has_value());
    const Decoded world = this->next_packet();

    EXPECT_EQ(hello, "hello");
    EXPECT_EQ(hello_ack.segment.flags, ack);
    EXPECT_EQ(hello_ack.segment.ack, client_isn + 6);
    EXPECT_EQ(world.segment.payload, "world");
    EXPECT_EQ(world.segment.seq, relay_isn + 1);
    EXPECT_EQ(world.segment.flags, psh | ack);
    EXPECT_TRUE(world.checksums_valid);
}

TEST_F(TestTunRelay, test_tcp_respects_client_mss_and_window)
{
    Listener listener = make_listener("127.0.0.1");
    this->start_relay();
    const uint32_t relay_isn = this->handshake(app4, listener.endpoint, 100, 250);
    Socket accepted = accept_from(listener);

    ASSERT_TRUE(accepted.send_all(std::string(400, 'x')).has_value());
    const Decoded first = this->next_packet();
    const Decoded second = this->next_packet();
    const Decoded third = this->next_packet();
    const bool window_full_silence = this->silent();
    this->send(forge_tcp(app4,
                         listener.endpoint,
                         Segment {.seq = client_isn + 1, .ack = relay_isn + 251, .flags = ack, .window = 250}));
    const Decoded fourth = this->next_packet();
    const Decoded fifth = this->next_packet();

    EXPECT_EQ(first.segment.payload.size(), 100u);
    EXPECT_EQ(second.segment.payload.size(), 100u);
    EXPECT_EQ(third.segment.payload.size(), 50u);
    EXPECT_EQ(third.segment.seq, relay_isn + 201);
    EXPECT_TRUE(window_full_silence);
    EXPECT_EQ(fourth.segment.seq, relay_isn + 251);
    EXPECT_EQ(fourth.segment.payload.size() + fifth.segment.payload.size(), 150u);
}

TEST_F(TestTunRelay, test_tcp_zero_window_is_probed_then_reopened)
{
    Listener listener = make_listener("127.0.0.1");
    ASSERT_TRUE(_relay.set_retransmit_timeout(Duration(250ms)));
    this->start_relay();
    const uint32_t relay_isn = this->handshake(app4, listener.endpoint, 1460, 0);
    Socket accepted = accept_from(listener);

    ASSERT_TRUE(accepted.send_all("blocked").has_value());
    const Decoded probe = this->next_packet();
    this->send(forge_tcp(app4, listener.endpoint, Segment {.seq = client_isn + 1, .ack = relay_isn + 1, .flags = ack}));
    const Decoded data = this->next_packet();

    EXPECT_EQ(probe.segment.flags, ack);
    EXPECT_EQ(probe.segment.seq, relay_isn);
    EXPECT_TRUE(probe.segment.payload.empty());
    EXPECT_EQ(data.segment.payload, "blocked");
}

TEST_F(TestTunRelay, test_tcp_retransmits_unacknowledged_data)
{
    Listener listener = make_listener("127.0.0.1");
    ASSERT_TRUE(_relay.set_retransmit_timeout(Duration(250ms)));
    this->start_relay();
    const uint32_t relay_isn = this->handshake(app4, listener.endpoint);
    Socket accepted = accept_from(listener);

    ASSERT_TRUE(accepted.send_all("data").has_value());
    const Decoded sent = this->next_packet();
    const Decoded resent = this->next_packet();
    this->send(forge_tcp(app4, listener.endpoint, Segment {.seq = client_isn + 1, .ack = relay_isn + 5, .flags = ack}));

    EXPECT_EQ(sent.segment.payload, "data");
    EXPECT_EQ(resent.segment.payload, "data");
    EXPECT_EQ(resent.segment.seq, sent.segment.seq);
    EXPECT_TRUE(this->silent());
}

TEST_F(TestTunRelay, test_tcp_out_of_order_gets_duplicate_ack)
{
    Listener listener = make_listener("127.0.0.1");
    this->start_relay();
    const uint32_t relay_isn = this->handshake(app4, listener.endpoint);
    Socket accepted = accept_from(listener);
    ASSERT_TRUE(accepted.set_recv_timeout(silence_milliseconds).has_value());

    this->send(
        forge_tcp(app4,
                  listener.endpoint,
                  Segment {.seq = client_isn + 11, .ack = relay_isn + 1, .flags = psh | ack, .payload = "late"}));
    const Decoded duplicate_ack = this->next_packet();
    ArrChar buffer(16);
    auto received = accepted.receive(buffer);

    EXPECT_EQ(duplicate_ack.segment.flags, ack);
    EXPECT_EQ(duplicate_ack.segment.ack, client_isn + 1);
    EXPECT_FALSE(received.has_value());
}

TEST_F(TestTunRelay, test_tcp_client_half_close)
{
    Listener listener = make_listener("127.0.0.1");
    this->start_relay();
    const uint32_t relay_isn = this->handshake(app4, listener.endpoint);
    Socket accepted = accept_from(listener);

    this->send(
        forge_tcp(app4, listener.endpoint, Segment {.seq = client_isn + 1, .ack = relay_isn + 1, .flags = fin | ack}));
    const Decoded fin_ack = this->next_packet();
    ArrChar buffer(16);
    auto end_of_stream = accepted.receive(buffer);
    ASSERT_TRUE(accepted.send_all("bye").has_value());
    const Decoded bye = this->next_packet();

    EXPECT_EQ(fin_ack.segment.flags, ack);
    EXPECT_EQ(fin_ack.segment.ack, client_isn + 2);
    EXPECT_EQ(end_of_stream.value_or(1), 0u);
    EXPECT_EQ(bye.segment.payload, "bye");
}

TEST_F(TestTunRelay, test_tcp_server_close_sends_fin)
{
    Listener listener = make_listener("127.0.0.1");
    this->start_relay();
    const uint32_t relay_isn = this->handshake(app4, listener.endpoint);
    Socket accepted = accept_from(listener);

    ASSERT_TRUE(accepted.close().has_value());
    const Decoded server_fin = this->next_packet();
    this->send(
        forge_tcp(app4, listener.endpoint, Segment {.seq = client_isn + 1, .ack = relay_isn + 2, .flags = fin | ack}));
    const Decoded last_ack = this->next_packet();

    EXPECT_EQ(server_fin.segment.flags, fin | ack);
    EXPECT_EQ(server_fin.segment.seq, relay_isn + 1);
    EXPECT_EQ(last_ack.segment.flags, ack);
    EXPECT_EQ(last_ack.segment.ack, client_isn + 2);
    EXPECT_TRUE(this->silent());
}

TEST_F(TestTunRelay, test_tcp_client_reset_closes_the_connection)
{
    Listener listener = make_listener("127.0.0.1");
    this->start_relay();
    const uint32_t relay_isn = this->handshake(app4, listener.endpoint);
    Socket accepted = accept_from(listener);

    this->send(forge_tcp(app4, listener.endpoint, Segment {.seq = client_isn + 1, .ack = relay_isn + 1, .flags = rst}));
    ArrChar buffer(16);
    auto end_of_stream = accepted.receive(buffer);

    EXPECT_EQ(end_of_stream.value_or(1), 0u);
    EXPECT_TRUE(this->silent());
}

TEST_F(TestTunRelay, test_tcp_refused_connection_resets)
{
    Listener listener = make_listener("127.0.0.1");
    const Endpoint closed = listener.endpoint;
    ASSERT_TRUE(listener.socket.close().has_value());
    this->start_relay();

    this->send(forge_tcp(app4, closed, Segment {.seq = client_isn, .flags = syn, .mss = 1460}));
    const Decoded reset = this->next_packet();

    EXPECT_EQ(reset.segment.flags, rst | ack);
    EXPECT_EQ(reset.segment.ack, client_isn + 1);
    EXPECT_EQ(reset.src, closed);
    EXPECT_TRUE(reset.checksums_valid);
}

TEST_F(TestTunRelay, test_tcp_unknown_flow_resets)
{
    this->start_relay();

    this->send(forge_tcp(app4, {"127.0.0.1", 9}, Segment {.seq = 5000, .ack = 7000, .flags = ack}));
    const Decoded reset = this->next_packet();

    EXPECT_EQ(reset.segment.flags, rst | ack);
    EXPECT_EQ(reset.segment.seq, 7000u);
}

TEST_F(TestTunRelay, test_setters_reject_invalid_values)
{
    EXPECT_FALSE(_relay.set_mtu(100));
    EXPECT_FALSE(_relay.set_mtu(70000));
    EXPECT_TRUE(_relay.set_mtu(1400));
    EXPECT_FALSE(_relay.set_udp_timeout(Duration(0)));
    EXPECT_FALSE(_relay.set_tcp_timeout(Duration(0)));
    EXPECT_FALSE(_relay.set_retransmit_timeout(Duration(0)));
}

TEST_F(TestTunRelay, test_setters_refused_while_running)
{
    this->start_relay();

    EXPECT_FALSE(_relay.set_tun(Tun()));
    EXPECT_FALSE(_relay.set_mtu(1400));
    EXPECT_FALSE(_relay.set_udp_timeout(Duration(1s)));
    EXPECT_FALSE(_relay.set_tcp_timeout(Duration(1s)));
    EXPECT_FALSE(_relay.set_retransmit_timeout(Duration(1s)));
}

TEST_F(TestTunRelay, test_relay_stops_when_the_tun_refuses_writes_then_restarts_on_a_new_tun)
{
    Listener server = make_udp_server("127.0.0.1");
    const Endpoint remote = server.endpoint;
    auto relay_run = std::async(std::launch::async, [this] { return _relay.start(); });
    Defer stop_relay([this] { _relay.stop(); });
    ASSERT_TRUE(_relay.wait_ready(std::chrono::seconds(2)));

    this->send(forge_udp(app4, remote, "ping"));
    IpAddr from;
    ASSERT_EQ(receive_request(server.socket, from), "ping");
    // a revoked android VPN leaves the tun down: its writes fail with EIO, here EPIPE
    ASSERT_TRUE(_peer.shutdown_receive().has_value());
    ASSERT_TRUE(server.socket.send_to(from, "pong").has_value());

    ASSERT_EQ(relay_run.wait_for(std::chrono::milliseconds(reply_timeout_milliseconds)), std::future_status::ready);

    this->attach_tun();
    this->start_relay();
    this->send(forge_udp(app4, remote, "ping"));
    ASSERT_EQ(receive_request(server.socket, from), "ping");
    ASSERT_TRUE(server.socket.send_to(from, "pong").has_value());

    EXPECT_EQ(this->next_packet().segment.payload, "pong");
}

TEST_F(TestTunRelay, test_start_without_tun_fails)
{
    TunRelay relay("no-tun");

    EXPECT_FALSE(relay.start());
}

TEST_F(TestTunRelay, test_kernel_client_through_the_tun)
{
    if constexpr (build::is_run_with_tsan)
        GTEST_SKIP() << "tsan aborts threads started in a child forked from a multi-threaded process";

    expect_in_userns(network_namespace, relay_kernel_traffic, tun_skipped_child_codes);
}

} // namespace test
