#ifndef __SIHD_NET_LINUX_NETLINK_HPP__
#define __SIHD_NET_LINUX_NETLINK_HPP__

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <span>
#include <string>
#include <vector>

#include <sihd/net/IpAddr.hpp>
#include <sihd/net/Socket.hpp>
#include <sihd/util/Result.hpp>

namespace sihd::net::netlink
{

struct Attribute
{
        uint16_t type = 0;
        std::span<const uint8_t> data;
};

struct Message
{
        uint16_t type = 0;
        std::vector<uint8_t> payload;
};

uint8_t u8(const Attribute & attribute);
uint16_t u16(const Attribute & attribute);
uint32_t u32(const Attribute & attribute);
std::string str(const Attribute & attribute);
IpAddr ip(const Attribute & attribute, int family);
std::vector<Attribute> nested(const Attribute & attribute);

template <typename Header>
Header header(const Message & message)
{
    Header header {};
    std::memcpy(&header, message.payload.data(), std::min(sizeof(Header), message.payload.size()));
    return header;
}

std::vector<Attribute> attributes(const Message & message, size_t header_size);

class Request
{
    public:
        Request(uint16_t type, uint16_t flags);

        template <typename Header>
        void set_header(const Header & header)
        {
            this->append(&header, sizeof(Header));
        }

        void add_attribute(uint16_t type, const void *data, size_t size);
        void add_u32(uint16_t type, uint32_t value);
        void add_string(uint16_t type, const std::string & value);

        const std::vector<uint8_t> & finish(uint32_t sequence);

    private:
        void append(const void *data, size_t size);

        std::vector<uint8_t> _buffer;
};

class Netlink
{
    public:
        sihd::util::Result<void> open(int protocol);
        // binds to multicast groups: app sandboxes (android) refuse it
        sihd::util::Result<void> subscribe(uint32_t groups);
        sihd::util::Result<void> set_nonblocking();
        sihd::util::Result<std::vector<Message>> request(Request & request);
        sihd::util::Result<std::vector<Message>> receive();
        int fd() const;

    private:
        sihd::util::Result<bool> _read(std::vector<Message> & messages);

        Socket _socket;
        uint32_t _sequence = 0;
        std::vector<uint8_t> _buffer = std::vector<uint8_t>(65536);
};

} // namespace sihd::net::netlink

#endif
