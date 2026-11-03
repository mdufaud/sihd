#include <sihd/util/build.hpp>

#if defined(__SIHD_LINUX__)

# include <netinet/in.h>

# include <algorithm>
# include <cstring>

# include <sihd/net/utils.hpp>
# include <sihd/util/endian.hpp>

# include "ip_packet.hpp"

using namespace sihd::util;

namespace sihd::net::linux_::ip_packet
{

namespace
{

constexpr size_t ipv4_header_size = 20;
constexpr size_t ipv6_header_size = 40;
constexpr size_t udp_header_size = 8;
constexpr size_t tcp_header_size = 20;
constexpr size_t tcp_mss_option_size = 4;
constexpr uint8_t tcp_option_end = 0;
constexpr uint8_t tcp_option_nop = 1;
constexpr uint8_t tcp_option_mss = 2;

constexpr uint8_t reply_ttl = 64;
constexpr uint16_t ipv4_dont_fragment = 0x4000;
constexpr uint16_t ipv4_fragment_mask = 0x3fff;

constexpr size_t ip_version_at = 0;
constexpr size_t ipv4_total_length_at = 2;
constexpr size_t ipv4_fragment_at = 6;
constexpr size_t ipv4_ttl_at = 8;
constexpr size_t ipv4_protocol_at = 9;
constexpr size_t ipv4_checksum_at = 10;
constexpr size_t ipv4_src_at = 12;
constexpr size_t ipv4_dst_at = 16;

constexpr size_t ipv6_payload_length_at = 4;
constexpr size_t ipv6_next_header_at = 6;
constexpr size_t ipv6_hop_limit_at = 7;
constexpr size_t ipv6_src_at = 8;
constexpr size_t ipv6_dst_at = 24;

constexpr size_t src_port_at = 0;
constexpr size_t dst_port_at = 2;
constexpr size_t udp_length_at = 4;
constexpr size_t udp_checksum_at = 6;

constexpr size_t tcp_seq_at = 4;
constexpr size_t tcp_ack_at = 8;
constexpr size_t tcp_data_offset_at = 12;
constexpr size_t tcp_flags_at = 13;
constexpr size_t tcp_window_at = 14;
constexpr size_t tcp_checksum_at = 16;
constexpr size_t tcp_option_length_at = 1;
constexpr size_t tcp_option_value_at = 2;

uint16_t be16(const uint8_t *data)
{
    return endian::read_be<uint16_t>(data, 2);
}

uint32_t be32(const uint8_t *data)
{
    return endian::read_be<uint32_t>(data, 4);
}

void put16(std::vector<uint8_t> & out, size_t offset, uint16_t value)
{
    endian::write_be(value, out.data() + offset, 2);
}

void put32(std::vector<uint8_t> & out, size_t offset, uint32_t value)
{
    endian::write_be(value, out.data() + offset, 4);
}

size_t address_size(uint8_t version)
{
    return version == 4 ? 4 : 16;
}

size_t ip_header_size(uint8_t version)
{
    return version == 4 ? ipv4_header_size : ipv6_header_size;
}

uint16_t read_mss_option(const uint8_t *options, size_t size)
{
    size_t offset = 0;
    while (offset < size && options[offset] != tcp_option_end)
    {
        if (options[offset] == tcp_option_nop)
        {
            ++offset;
            continue;
        }
        const size_t length_at = offset + tcp_option_length_at;
        if (length_at >= size || options[length_at] < 2 || offset + options[length_at] > size)
            break;
        if (options[offset] == tcp_option_mss && options[length_at] == tcp_mss_option_size)
            return be16(options + offset + tcp_option_value_at);
        offset += options[length_at];
    }
    return 0;
}

std::optional<Packet> parse_udp(const FlowKey & ip_key, const uint8_t *segment, size_t size)
{
    if (size < udp_header_size)
        return std::nullopt;
    const uint16_t length = be16(segment + udp_length_at);
    if (length < udp_header_size || length > size)
        return std::nullopt;
    UdpDatagram datagram {.key = ip_key, .payload = ArrUByteView(segment + udp_header_size, length - udp_header_size)};
    datagram.key.src_port = be16(segment + src_port_at);
    datagram.key.dst_port = be16(segment + dst_port_at);
    return datagram;
}

std::optional<Packet> parse_tcp(const FlowKey & ip_key, const uint8_t *segment, size_t size)
{
    if (size < tcp_header_size)
        return std::nullopt;
    const size_t header_size = static_cast<size_t>(segment[tcp_data_offset_at] >> 4) * 4;
    if (header_size < tcp_header_size || header_size > size)
        return std::nullopt;
    TcpPacket packet {.key = ip_key,
                      .segment = {.seq = be32(segment + tcp_seq_at),
                                  .ack = be32(segment + tcp_ack_at),
                                  .flags = segment[tcp_flags_at],
                                  .window = be16(segment + tcp_window_at),
                                  .mss = read_mss_option(segment + tcp_header_size, header_size - tcp_header_size),
                                  .payload = ArrUByteView(segment + header_size, size - header_size)}};
    packet.key.src_port = be16(segment + src_port_at);
    packet.key.dst_port = be16(segment + dst_port_at);
    return packet;
}

ArrUByteView read_ipv4_header(ArrUByteView data, FlowKey & key)
{
    const uint8_t *bytes = data.data();
    const size_t header_size = static_cast<size_t>(bytes[ip_version_at] & 0x0f) * 4;
    const size_t total = be16(bytes + ipv4_total_length_at);
    if (header_size < ipv4_header_size || total < header_size || total > data.size()
        || (be16(bytes + ipv4_fragment_at) & ipv4_fragment_mask) != 0)
        return {};

    key.version = 4;
    key.protocol = bytes[ipv4_protocol_at];
    std::copy_n(bytes + ipv4_src_at, 4, key.src.begin());
    std::copy_n(bytes + ipv4_dst_at, 4, key.dst.begin());
    return ArrUByteView(bytes + header_size, total - header_size);
}

ArrUByteView read_ipv6_header(ArrUByteView data, FlowKey & key)
{
    const uint8_t *bytes = data.data();
    const size_t payload = be16(bytes + ipv6_payload_length_at);
    if (ipv6_header_size + payload > data.size())
        return {};

    key.version = 6;
    key.protocol = bytes[ipv6_next_header_at];
    std::copy_n(bytes + ipv6_src_at, 16, key.src.begin());
    std::copy_n(bytes + ipv6_dst_at, 16, key.dst.begin());
    return ArrUByteView(bytes + ipv6_header_size, payload);
}

ArrUByteView read_ip_header(ArrUByteView data, FlowKey & key)
{
    if (data.size() >= ipv4_header_size && data.data()[ip_version_at] >> 4 == 4)
        return read_ipv4_header(data, key);
    if (data.size() >= ipv6_header_size && data.data()[ip_version_at] >> 4 == 6)
        return read_ipv6_header(data, key);
    return {};
}

void write_ip_header(std::vector<uint8_t> & out, const FlowKey & key, size_t transport_size)
{
    const size_t addr_size = address_size(key.version);
    if (key.version == 4)
    {
        out[ip_version_at] = 0x45;
        put16(out, ipv4_total_length_at, static_cast<uint16_t>(ipv4_header_size + transport_size));
        put16(out, ipv4_fragment_at, ipv4_dont_fragment);
        out[ipv4_ttl_at] = reply_ttl;
        out[ipv4_protocol_at] = key.protocol;
        std::copy_n(key.dst.begin(), addr_size, out.begin() + ipv4_src_at);
        std::copy_n(key.src.begin(), addr_size, out.begin() + ipv4_dst_at);
        put16(out, ipv4_checksum_at, utils::checksum(ArrUByteView(out.data(), ipv4_header_size)));
        return;
    }

    out[ip_version_at] = 0x60;
    put16(out, ipv6_payload_length_at, static_cast<uint16_t>(transport_size));
    out[ipv6_next_header_at] = key.protocol;
    out[ipv6_hop_limit_at] = reply_ttl;
    std::copy_n(key.dst.begin(), addr_size, out.begin() + ipv6_src_at);
    std::copy_n(key.src.begin(), addr_size, out.begin() + ipv6_dst_at);
}

uint16_t transport_checksum(const FlowKey & key, const uint8_t *segment, size_t size)
{
    const size_t addr_size = address_size(key.version);
    uint32_t sum = utils::checksum_add(ArrUByteView(key.src.data(), addr_size));
    sum = utils::checksum_add(ArrUByteView(key.dst.data(), addr_size), sum);
    sum += key.protocol;
    sum += static_cast<uint32_t>(size >> 16) + static_cast<uint32_t>(size & 0xffff);
    return utils::checksum(ArrUByteView(segment, size), sum);
}

} // namespace

std::optional<Packet> parse(ArrUByteView data)
{
    FlowKey key;
    const ArrUByteView segment = read_ip_header(data, key);
    if (segment.data() == nullptr)
        return std::nullopt;
    if (key.protocol == IPPROTO_UDP)
        return parse_udp(key, segment.data(), segment.size());
    if (key.protocol == IPPROTO_TCP)
        return parse_tcp(key, segment.data(), segment.size());
    return std::nullopt;
}

IpAddr destination(const FlowKey & key)
{
    if (key.version == 4)
    {
        sockaddr_in addr {};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(key.dst_port);
        std::memcpy(&addr.sin_addr, key.dst.data(), 4);
        return IpAddr(addr);
    }
    sockaddr_in6 addr {};
    addr.sin6_family = AF_INET6;
    addr.sin6_port = htons(key.dst_port);
    std::memcpy(&addr.sin6_addr, key.dst.data(), 16);
    return IpAddr(addr);
}

void build_udp_reply(std::vector<uint8_t> & out, const FlowKey & key, ArrUByteView payload)
{
    const size_t ip_size = ip_header_size(key.version);
    const size_t udp_size = udp_header_size + payload.size();
    out.assign(ip_size + udp_size, 0);
    write_ip_header(out, key, udp_size);

    put16(out, ip_size + src_port_at, key.dst_port);
    put16(out, ip_size + dst_port_at, key.src_port);
    put16(out, ip_size + udp_length_at, static_cast<uint16_t>(udp_size));
    std::copy_n(payload.data(), payload.size(), out.begin() + static_cast<ptrdiff_t>(ip_size + udp_header_size));
    // a computed 0 is sent as 0xffff: 0 means no checksum
    const uint16_t checksum = transport_checksum(key, out.data() + ip_size, udp_size);
    put16(out, ip_size + udp_checksum_at, checksum == 0 ? 0xffff : checksum);
}

void build_tcp_reply(std::vector<uint8_t> & out, const FlowKey & key, const TcpSegment & segment)
{
    const size_t ip_size = ip_header_size(key.version);
    const size_t header_size = tcp_header_size + (segment.mss != 0 ? tcp_mss_option_size : 0);
    const size_t tcp_size = header_size + segment.payload.size();
    out.assign(ip_size + tcp_size, 0);
    write_ip_header(out, key, tcp_size);

    put16(out, ip_size + src_port_at, key.dst_port);
    put16(out, ip_size + dst_port_at, key.src_port);
    put32(out, ip_size + tcp_seq_at, segment.seq);
    put32(out, ip_size + tcp_ack_at, segment.ack);
    out[ip_size + tcp_data_offset_at] = static_cast<uint8_t>(header_size / 4 << 4);
    out[ip_size + tcp_flags_at] = segment.flags;
    put16(out, ip_size + tcp_window_at, segment.window);
    if (segment.mss != 0)
    {
        out[ip_size + tcp_header_size] = tcp_option_mss;
        out[ip_size + tcp_header_size + tcp_option_length_at] = tcp_mss_option_size;
        put16(out, ip_size + tcp_header_size + tcp_option_value_at, segment.mss);
    }
    std::copy_n(segment.payload.data(),
                segment.payload.size(),
                out.begin() + static_cast<ptrdiff_t>(ip_size + header_size));
    put16(out, ip_size + tcp_checksum_at, transport_checksum(key, out.data() + ip_size, tcp_size));
}

} // namespace sihd::net::linux_::ip_packet

#endif
