#include <sihd/util/build.hpp>

#if defined(__SIHD_LINUX__)

# include <sys/socket.h>

# include <cerrno>
# include <system_error>

# include <sihd/util/Error.hpp>

# include "netlink.hpp"
# include <linux/netlink.h>

using namespace sihd::util;

namespace sihd::net::netlink
{

namespace
{

constexpr size_t align(size_t size)
{
    return (size + NLMSG_ALIGNTO - 1) & ~static_cast<size_t>(NLMSG_ALIGNTO - 1);
}

constexpr size_t attribute_header_size = align(sizeof(nlattr));

std::vector<Attribute> parse_attributes(std::span<const uint8_t> data)
{
    std::vector<Attribute> attributes;
    size_t offset = 0;
    while (offset + sizeof(nlattr) <= data.size())
    {
        nlattr header {};
        std::memcpy(&header, data.data() + offset, sizeof(nlattr));
        if (header.nla_len < sizeof(nlattr) || offset + header.nla_len > data.size())
            break;
        attributes.push_back({
            .type = static_cast<uint16_t>(header.nla_type & NLA_TYPE_MASK),
            .data = data.subspan(offset + attribute_header_size, header.nla_len - attribute_header_size),
        });
        offset += align(header.nla_len);
    }
    return attributes;
}

Error kernel_error(int error_number)
{
    const ErrorCode code = error_number == ENETUNREACH || error_number == EHOSTUNREACH ? ErrorCode::not_found
                                                                                       : error_errno(error_number);
    return Error(code, "netlink: {}", std::generic_category().message(error_number));
}

template <typename T>
T read_integer(std::span<const uint8_t> data)
{
    T value = 0;
    std::memcpy(&value, data.data(), std::min(sizeof(T), data.size()));
    return value;
}

union NetlinkAddress
{
        sockaddr base;
        sockaddr_nl netlink;
};

} // namespace

uint8_t u8(const Attribute & attribute)
{
    return read_integer<uint8_t>(attribute.data);
}

uint16_t u16(const Attribute & attribute)
{
    return read_integer<uint16_t>(attribute.data);
}

uint32_t u32(const Attribute & attribute)
{
    return read_integer<uint32_t>(attribute.data);
}

std::string str(const Attribute & attribute)
{
    std::string value(attribute.data.begin(), attribute.data.end());
    if (const size_t nul = value.find('\0'); nul != std::string::npos)
        value.resize(nul);
    return value;
}

IpAddr ip(const Attribute & attribute, int family)
{
    return IpAddr::from_bytes(family, attribute.data);
}

std::vector<Attribute> nested(const Attribute & attribute)
{
    return parse_attributes(attribute.data);
}

std::vector<Attribute> attributes(const Message & message, size_t header_size)
{
    const size_t offset = align(header_size);
    if (offset >= message.payload.size())
        return {};
    return parse_attributes(std::span(message.payload).subspan(offset));
}

Request::Request(uint16_t type, uint16_t flags)
{
    nlmsghdr header {};
    header.nlmsg_type = type;
    header.nlmsg_flags = flags;
    this->append(&header, sizeof(header));
}

void Request::add_attribute(uint16_t type, const void *data, size_t size)
{
    nlattr header {};
    header.nla_type = type;
    header.nla_len = static_cast<uint16_t>(attribute_header_size + size);
    this->append(&header, sizeof(header));
    this->append(data, size);
}

void Request::add_u32(uint16_t type, uint32_t value)
{
    this->add_attribute(type, &value, sizeof(value));
}

void Request::add_string(uint16_t type, const std::string & value)
{
    this->add_attribute(type, value.c_str(), value.size() + 1);
}

const std::vector<uint8_t> & Request::finish(uint32_t sequence)
{
    nlmsghdr header {};
    std::memcpy(&header, _buffer.data(), sizeof(header));
    header.nlmsg_len = static_cast<uint32_t>(_buffer.size());
    header.nlmsg_seq = sequence;
    std::memcpy(_buffer.data(), &header, sizeof(header));
    return _buffer;
}

void Request::append(const void *data, size_t size)
{
    const size_t offset = _buffer.size();
    _buffer.resize(offset + align(size), 0);
    std::memcpy(_buffer.data() + offset, data, size);
}

Result<void> Netlink::open(int protocol)
{
    return _socket.open(AF_NETLINK, SOCK_RAW | SOCK_CLOEXEC, protocol);
}

Result<void> Netlink::subscribe(uint32_t groups)
{
    NetlinkAddress address {};
    address.netlink.nl_family = AF_NETLINK;
    address.netlink.nl_groups = groups;
    return _socket.bind(&address.base, sizeof(sockaddr_nl));
}

Result<void> Netlink::set_nonblocking()
{
    return _socket.set_blocking(false);
}

Result<std::vector<Message>> Netlink::request(Request & request)
{
    const std::vector<uint8_t> & buffer = request.finish(++_sequence);
    auto sent = _socket.send_all({buffer.data(), buffer.size()});
    SIHD_UNEXPECTED_RETURN(sent);

    std::vector<Message> messages;
    while (true)
    {
        auto done = this->_read(messages);
        SIHD_UNEXPECTED_RETURN(done);
        if (*done)
            return messages;
    }
}

Result<std::vector<Message>> Netlink::receive()
{
    std::vector<Message> messages;
    auto done = this->_read(messages);
    SIHD_UNEXPECTED_RETURN(done);
    return messages;
}

int Netlink::fd() const
{
    return _socket.socket();
}

Result<bool> Netlink::_read(std::vector<Message> & messages)
{
    auto received = _socket.receive(_buffer.data(), _buffer.size());
    SIHD_UNEXPECTED_RETURN(received);

    bool done = false;
    size_t offset = 0;
    while (offset + sizeof(nlmsghdr) <= *received)
    {
        nlmsghdr header {};
        std::memcpy(&header, _buffer.data() + offset, sizeof(header));
        if (header.nlmsg_len < sizeof(nlmsghdr) || offset + header.nlmsg_len > *received)
            break;

        const uint8_t *payload = _buffer.data() + offset + align(sizeof(nlmsghdr));
        const size_t payload_size = header.nlmsg_len - align(sizeof(nlmsghdr));
        if (header.nlmsg_type == NLMSG_ERROR)
        {
            nlmsgerr error {};
            std::memcpy(&error, payload, std::min(sizeof(error), payload_size));
            if (error.error != 0)
                return std::unexpected(kernel_error(-error.error));
            done = true;
        }
        else if (header.nlmsg_type == NLMSG_DONE)
            done = true;
        else
        {
            messages.push_back(
                {.type = header.nlmsg_type, .payload = std::vector<uint8_t>(payload, payload + payload_size)});
            if ((header.nlmsg_flags & NLM_F_MULTI) == 0)
                done = true;
        }
        offset += align(header.nlmsg_len);
    }
    return done;
}

} // namespace sihd::net::netlink

#endif
