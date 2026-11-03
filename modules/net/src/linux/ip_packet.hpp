#ifndef __SIHD_NET_LINUX_IP_PACKET_HPP__
#define __SIHD_NET_LINUX_IP_PACKET_HPP__

#include <array>
#include <compare>
#include <cstdint>
#include <optional>
#include <variant>
#include <vector>

#include <sihd/net/IpAddr.hpp>
#include <sihd/util/ArrayView.hpp>

namespace sihd::net::linux_::ip_packet
{

inline constexpr uint8_t tcp_fin = 0x01;
inline constexpr uint8_t tcp_syn = 0x02;
inline constexpr uint8_t tcp_rst = 0x04;
inline constexpr uint8_t tcp_psh = 0x08;
inline constexpr uint8_t tcp_ack = 0x10;

// as the client sent it: src is the app, dst the remote peer
struct FlowKey
{
        uint8_t version = 0;
        uint8_t protocol = 0;
        std::array<uint8_t, 16> src {};
        std::array<uint8_t, 16> dst {};
        uint16_t src_port = 0;
        uint16_t dst_port = 0;

        auto operator<=>(const FlowKey &) const = default;
};

struct TcpSegment
{
        uint32_t seq = 0;
        uint32_t ack = 0;
        uint8_t flags = 0;
        uint16_t window = 0;
        // 0 when the segment carries no MSS option
        uint16_t mss = 0;
        sihd::util::ArrUByteView payload;
};

struct TcpPacket
{
        FlowKey key;
        TcpSegment segment;
};

struct UdpDatagram
{
        FlowKey key;
        sihd::util::ArrUByteView payload;
};

using Packet = std::variant<TcpPacket, UdpDatagram>;

// nullopt for what is not relayed: other protocols, fragments, IPv6 extension headers, truncated packets
std::optional<Packet> parse(sihd::util::ArrUByteView data);

IpAddr destination(const FlowKey & key);

// replies travel from key.dst to key.src
void build_udp_reply(std::vector<uint8_t> & out, const FlowKey & key, sihd::util::ArrUByteView payload);
void build_tcp_reply(std::vector<uint8_t> & out, const FlowKey & key, const TcpSegment & segment);

} // namespace sihd::net::linux_::ip_packet

#endif
