#ifndef __SIHD_NET_TEST_PACKET_FORGE_HPP__
#define __SIHD_NET_TEST_PACKET_FORGE_HPP__

#include <sys/socket.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <sihd/net/IpAddr.hpp>
#include <sihd/net/utils.hpp>

namespace test
{

struct Endpoint
{
        std::string ip;
        uint16_t port = 0;

        bool operator==(const Endpoint &) const = default;
};

struct Segment
{
        uint32_t seq = 0;
        uint32_t ack = 0;
        uint8_t flags = 0;
        uint16_t window = 65535;
        uint16_t mss = 0;
        std::string payload {};
};

inline constexpr uint8_t protocol_icmp = 1;
inline constexpr uint8_t protocol_tcp = 6;
inline constexpr uint8_t protocol_udp = 17;
inline constexpr size_t ipv4_protocol_at = 9;
inline constexpr size_t udp_checksum_at = 6;
inline constexpr size_t tcp_checksum_at = 16;

inline constexpr uint8_t fin = 0x01;
inline constexpr uint8_t syn = 0x02;
inline constexpr uint8_t rst = 0x04;
inline constexpr uint8_t psh = 0x08;
inline constexpr uint8_t ack = 0x10;

struct Decoded
{
        int version = 0;
        uint8_t protocol = 0;
        Endpoint src;
        Endpoint dst;
        Segment segment;
        bool checksums_valid = false;
};

namespace forge_detail
{

inline std::string address_str(int version, const uint8_t *bytes)
{
    const size_t size = version == 4 ? 4 : 16;
    return sihd::net::IpAddr::from_bytes(version == 4 ? AF_INET : AF_INET6, {bytes, size}).str();
}

inline void push16(std::vector<uint8_t> & out, uint16_t value)
{
    out.push_back(static_cast<uint8_t>(value >> 8));
    out.push_back(static_cast<uint8_t>(value));
}

inline void push32(std::vector<uint8_t> & out, uint32_t value)
{
    push16(out, static_cast<uint16_t>(value >> 16));
    push16(out, static_cast<uint16_t>(value));
}

inline uint16_t get16(const std::vector<uint8_t> & data, size_t offset)
{
    return static_cast<uint16_t>(data[offset] << 8 | data[offset + 1]);
}

inline uint32_t get32(const std::vector<uint8_t> & data, size_t offset)
{
    return static_cast<uint32_t>(get16(data, offset)) << 16 | get16(data, offset + 2);
}

inline uint16_t pseudo_checksum(int version,
                                const uint8_t *src,
                                const uint8_t *dst,
                                uint8_t protocol,
                                const uint8_t *segment,
                                size_t size)
{
    const size_t addr_size = version == 4 ? 4 : 16;
    uint32_t sum = sihd::net::utils::checksum_add({src, addr_size});
    sum = sihd::net::utils::checksum_add({dst, addr_size}, sum);
    sum += protocol + static_cast<uint32_t>(size);
    return sihd::net::utils::checksum({segment, size}, sum);
}

inline std::vector<uint8_t> wrap(const Endpoint & src,
                                 const Endpoint & dst,
                                 uint8_t protocol,
                                 std::vector<uint8_t> transport,
                                 size_t checksum_at)
{
    const std::vector<uint8_t> src_bytes = sihd::net::IpAddr(src.ip).bytes();
    const std::vector<uint8_t> dst_bytes = sihd::net::IpAddr(dst.ip).bytes();
    const int version = src_bytes.size() == 16 ? 6 : 4;
    const uint16_t checksum = pseudo_checksum(version,
                                              src_bytes.data(),
                                              dst_bytes.data(),
                                              protocol,
                                              transport.data(),
                                              transport.size());
    transport[checksum_at] = static_cast<uint8_t>(checksum >> 8);
    transport[checksum_at + 1] = static_cast<uint8_t>(checksum);

    std::vector<uint8_t> packet;
    if (version == 4)
    {
        packet = {0x45, 0x00};
        push16(packet, static_cast<uint16_t>(20 + transport.size()));
        packet.insert(packet.end(), {0x00, 0x01, 0x00, 0x00, 0x40, protocol, 0x00, 0x00});
        packet.insert(packet.end(), src_bytes.begin(), src_bytes.end());
        packet.insert(packet.end(), dst_bytes.begin(), dst_bytes.end());
        constexpr size_t header_checksum_at = 10;
        const uint16_t header_checksum = sihd::net::utils::checksum(packet);
        packet[header_checksum_at] = static_cast<uint8_t>(header_checksum >> 8);
        packet[header_checksum_at + 1] = static_cast<uint8_t>(header_checksum);
    }
    else
    {
        packet = {0x60, 0x00, 0x00, 0x00};
        push16(packet, static_cast<uint16_t>(transport.size()));
        packet.insert(packet.end(), {protocol, 0x40});
        packet.insert(packet.end(), src_bytes.begin(), src_bytes.end());
        packet.insert(packet.end(), dst_bytes.begin(), dst_bytes.end());
    }
    packet.insert(packet.end(), transport.begin(), transport.end());
    return packet;
}

} // namespace forge_detail

inline std::vector<uint8_t> forge_udp(const Endpoint & src, const Endpoint & dst, const std::string & payload)
{
    std::vector<uint8_t> udp;
    forge_detail::push16(udp, src.port);
    forge_detail::push16(udp, dst.port);
    forge_detail::push16(udp, static_cast<uint16_t>(8 + payload.size()));
    forge_detail::push16(udp, 0);
    udp.insert(udp.end(), payload.begin(), payload.end());
    return forge_detail::wrap(src, dst, protocol_udp, std::move(udp), udp_checksum_at);
}

inline std::vector<uint8_t> forge_tcp(const Endpoint & src, const Endpoint & dst, const Segment & segment)
{
    std::vector<uint8_t> tcp;
    forge_detail::push16(tcp, src.port);
    forge_detail::push16(tcp, dst.port);
    forge_detail::push32(tcp, segment.seq);
    forge_detail::push32(tcp, segment.ack);
    tcp.push_back(segment.mss != 0 ? 0x60 : 0x50);
    tcp.push_back(segment.flags);
    forge_detail::push16(tcp, segment.window);
    forge_detail::push16(tcp, 0);
    forge_detail::push16(tcp, 0);
    if (segment.mss != 0)
    {
        tcp.insert(tcp.end(), {0x02, 0x04});
        forge_detail::push16(tcp, segment.mss);
    }
    tcp.insert(tcp.end(), segment.payload.begin(), segment.payload.end());
    return forge_detail::wrap(src, dst, protocol_tcp, std::move(tcp), tcp_checksum_at);
}

inline std::optional<Decoded> decode(const std::vector<uint8_t> & packet)
{
    using namespace forge_detail;
    if (packet.empty())
        return std::nullopt;
    Decoded decoded;
    decoded.version = packet[0] >> 4;
    size_t header_size = 0;
    size_t transport_size = 0;
    const uint8_t *src = nullptr;
    const uint8_t *dst = nullptr;
    bool header_valid = true;
    if (decoded.version == 4 && packet.size() >= 20)
    {
        header_size = static_cast<size_t>(packet[0] & 0x0f) * 4;
        transport_size = get16(packet, 2) - header_size;
        decoded.protocol = packet[ipv4_protocol_at];
        src = packet.data() + 12;
        dst = packet.data() + 16;
        header_valid = sihd::net::utils::checksum({packet.data(), header_size}) == 0;
    }
    else if (decoded.version == 6 && packet.size() >= 40)
    {
        header_size = 40;
        transport_size = get16(packet, 4);
        decoded.protocol = packet[6];
        src = packet.data() + 8;
        dst = packet.data() + 24;
    }
    else
        return std::nullopt;
    if (header_size + transport_size != packet.size())
        return std::nullopt;

    decoded.src = {address_str(decoded.version, src), get16(packet, header_size)};
    decoded.dst = {address_str(decoded.version, dst), get16(packet, header_size + 2)};
    const uint8_t *transport = packet.data() + header_size;
    const bool transport_valid = pseudo_checksum(decoded.version, src, dst, decoded.protocol, transport, transport_size)
                                 == 0;
    decoded.checksums_valid = header_valid && transport_valid;

    size_t payload_at = header_size + 8;
    if (decoded.protocol == protocol_tcp)
    {
        decoded.segment.seq = get32(packet, header_size + 4);
        decoded.segment.ack = get32(packet, header_size + 8);
        decoded.segment.flags = packet[header_size + 13];
        decoded.segment.window = get16(packet, header_size + 14);
        const size_t tcp_header_size = static_cast<size_t>(packet[header_size + 12] >> 4) * 4;
        if (tcp_header_size >= 24 && packet[header_size + 20] == 0x02)
            decoded.segment.mss = get16(packet, header_size + 22);
        payload_at = header_size + tcp_header_size;
    }
    decoded.segment.payload.assign(packet.begin() + static_cast<ptrdiff_t>(payload_at), packet.end());
    return decoded;
}

} // namespace test

#endif
