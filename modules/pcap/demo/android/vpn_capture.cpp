#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <span>
#include <string>
#include <string_view>

#include <fmt/format.h>

#include <sihd/net/IpAddr.hpp>
#include <sihd/net/Tun.hpp>
#include <sihd/net/linux/TunRelay.hpp>
#include <sihd/pcap/PcapWriter.hpp>
#include <sihd/pcap/utils.hpp>
#include <sihd/sys/App.hpp>
#include <sihd/sys/EventPoll.hpp>
#include <sihd/util/Handler.hpp>
#include <sihd/util/Worker.hpp>

using namespace sihd::util;
using namespace sihd::net;
using namespace sihd::net::linux_;
using namespace sihd::pcap;

namespace
{

constexpr std::string_view commands = "commands: start (print packets), capture (print + pcap), stop (quit)";

constexpr size_t ipv4_header_min = 20;
constexpr size_t ipv6_header_size = 40;
constexpr uint8_t protocol_icmp = 1;
constexpr uint8_t protocol_tcp = 6;
constexpr uint8_t protocol_udp = 17;
constexpr uint8_t protocol_icmpv6 = 58;
constexpr size_t tcp_flags_at = 13;

uint16_t read_port(ArrUByteView packet, size_t offset)
{
    return static_cast<uint16_t>((packet[offset] << 8) | packet[offset + 1]);
}

std::string tcp_flags(uint8_t flags)
{
    std::string letters;
    for (const auto [bit, letter] : {std::pair {0x02, 'S'},
                                     std::pair {0x01, 'F'},
                                     std::pair {0x04, 'R'},
                                     std::pair {0x08, 'P'},
                                     std::pair {0x10, 'A'}})
    {
        if (flags & bit)
            letters += letter;
    }
    return letters;
}

// one short line per packet for a phone screen: the remote end only, the local end is always the VPN address
std::string summarize(ArrUByteView packet, TunRelay::Direction direction)
{
    const bool outgoing = direction == TunRelay::Direction::from_tun;
    const char arrow = outgoing ? '>' : '<';
    if (packet.empty())
        return fmt::format("{} ? {}", arrow, packet.size());

    const int version = packet[0] >> 4;
    size_t header = 0;
    uint8_t protocol = 0;
    IpAddr remote;
    if (version == 4 && packet.size() >= ipv4_header_min)
    {
        header = static_cast<size_t>(packet[0] & 0x0f) * 4;
        protocol = packet[9];
        remote = IpAddr::from_bytes(AF_INET, std::span(packet.data() + (outgoing ? 16 : 12), 4));
    }
    else if (version == 6 && packet.size() >= ipv6_header_size)
    {
        header = ipv6_header_size;
        protocol = packet[6];
        remote = IpAddr::from_bytes(AF_INET6, std::span(packet.data() + (outgoing ? 24 : 8), 16));
    }
    else
    {
        return fmt::format("{} ? {}", arrow, packet.size());
    }

    const std::string host = remote.is_ipv6() ? fmt::format("[{}]", remote.str()) : remote.str();
    const bool has_ports = (protocol == protocol_tcp || protocol == protocol_udp) && packet.size() >= header + 4;
    if (!has_ports)
    {
        if (protocol == protocol_icmp || protocol == protocol_icmpv6)
            return fmt::format("{} icmp {} {}", arrow, host, packet.size());
        return fmt::format("{} ip{} {} {}", arrow, protocol, host, packet.size());
    }

    const uint16_t port = read_port(packet, outgoing ? header + 2 : header);
    if (protocol == protocol_udp)
        return fmt::format("{} udp {}:{} {}", arrow, host, port, packet.size());
    const std::string flags = packet.size() > header + tcp_flags_at ? tcp_flags(packet[header + tcp_flags_at])
                                                                    : std::string("?");
    return fmt::format("{} tcp {}:{} {} {}", arrow, host, port, flags, packet.size());
}

// the relay thread writes packets while the commands open and close the file
class Capture
{
    public:
        explicit Capture(const std::string & path): _path(path) {}

        bool open()
        {
            std::lock_guard lock(_mutex);
            if (!_writer.is_open())
            {
                if (auto opened = _writer.open(_path, utils::datalink("RAW")); !opened)
                {
                    fmt::print(stderr, "{}\n", opened.error().message);
                    return false;
                }
                _written = 0;
            }
            fmt::print("capturing to {}\n", _path);
            return true;
        }

        void write(ArrUByteView packet)
        {
            std::lock_guard lock(_mutex);
            if (!_writer.is_open() || _writer.write(ArrCharView(packet.data(), packet.size())) <= 0)
                return;
            ++_written;
            // android kills the process without notice: the file stays readable at any time
            _writer.flush();
        }

        void close()
        {
            std::lock_guard lock(_mutex);
            if (!_writer.is_open())
                return;
            _writer.close();
            fmt::print("{} packets saved to {}\n", _written, _path);
        }

    private:
        std::mutex _mutex;
        PcapWriter _writer {"vpn-capture"};
        std::string _path;
        size_t _written = 0;
};

bool run_command(const std::string & command, Capture & capture, std::atomic<bool> & printing)
{
    if (command == "stop")
        return false;
    if (command == "start")
    {
        capture.close();
        printing = true;
        fmt::print("printing packets\n");
    }
    else if (command == "capture")
    {
        if (capture.open())
            printing = true;
    }
    else
    {
        fmt::print("{}\n", commands);
    }
    return true;
}

void relay(sihd::sys::App & app, int tun_fd, const std::string & output)
{
    Capture capture(output);
    std::atomic<bool> printing = false;

    Handler<TunRelay *> printer([&capture, &printing](TunRelay *relay) {
        if (!printing)
            return;
        const ArrUByteView packet = relay->packet();
        fmt::print("{}\n", summarize(packet, relay->direction()));
        capture.write(packet);
    });

    TunRelay relay("vpn-relay");
    auto tun = Tun::adopt(tun_fd);
    if (!tun)
    {
        fmt::print(stderr, "{}\n", tun.error().message);
        app.exit(EXIT_FAILURE);
        return;
    }
    relay.add_observer(&printer);

    sihd::sys::EventPoll input;
    // unbuffered: a second line of one read stays in the fd and wakes the poll again
    if (!relay.set_tun(std::move(*tun)) || !input.set_read_fd(STDIN_FILENO)
        || std::setvbuf(stdin, nullptr, _IONBF, 0) != 0)
    {
        app.exit(EXIT_FAILURE);
        return;
    }

    Worker worker([&relay, &input] {
        const bool relayed = relay.start();
        // a revoked VPN ends the relay: the program stops without waiting for a command
        input.request_stop();
        return relayed;
    });
    if (!worker.start_sync_worker("vpn-relay"))
    {
        app.exit(EXIT_FAILURE);
        return;
    }

    fmt::print("relay running\n{}\n", commands);
    // the relay ending wakes the poll with no event
    while (input.poll() > 0)
    {
        std::string command;
        if (!std::getline(std::cin, command) || !run_command(command, capture, printing))
            break;
    }

    relay.stop();
    worker.stop_worker();
    capture.close();
    fmt::print("relay stopped\n");
}

} // namespace

int main(int argc, char **argv)
{
    sihd::sys::App app({
        .name = "vpn_capture",
        .description = "Relay the tun of an android VpnService, print a line per packet and capture them on command",
    });

    int tun_fd = -1;
    std::string output = "capture.pcap";
    app.root().bind("tun-fd", tun_fd, "descriptor handed by VpnService.Builder.establish()");
    app.root().bind("output", output, "pcap file written by the capture command, raw IP link type");
    app.root().on_run([&] {
        if (tun_fd < 0)
        {
            fmt::print(stderr, "--tun-fd is required\n");
            app.exit(EXIT_FAILURE);
            return;
        }
        relay(app, tun_fd, output);
    });

    return app.run(argc, argv);
}
