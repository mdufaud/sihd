#include <cstdint>
#include <stdexcept>

#include <sihd/net/IcmpSender.hpp>
#include <sihd/net/utils.hpp>
#include <sihd/sys/NamedFactory.hpp>
#include <sihd/util/Array.hpp>
#include <sihd/util/Logger.hpp>

#if !defined(__SIHD_WINDOWS__)
# include <arpa/inet.h>     // inet_ntop
# include <netinet/icmp6.h> // icmpv6 macros
# include <netinet/ip6.h>
# include <netinet/ip_icmp.h> // icmp macros
#else
# include <ws2ipdef.h> // icmpv6 macros
# include <ws2tcpip.h>
#endif

#if defined(__SIHD_WINDOWS__)
struct ip6_hdr
{
        union
        {
                struct ip6_hdrctl
                {
                        uint32_t ip6_un1_flow; /* 4 bits version, 8 bits TC,
                                  20 bits flow-ID */
                        uint16_t ip6_un1_plen; /* payload length */
                        uint8_t ip6_un1_nxt;   /* next header */
                        uint8_t ip6_un1_hlim;  /* hop limit */
                } ip6_un1;
                uint8_t ip6_un2_vfc; /* 4 bits version, top 4 bits tclass */
        } ip6_ctlun;
        struct in6_addr ip6_src; /* source address */
        struct in6_addr ip6_dst; /* destination address */
};
# define ip6_vfc ip6_ctlun.ip6_un2_vfc
# define ip6_flow ip6_ctlun.ip6_un1.ip6_un1_flow
# define ip6_plen ip6_ctlun.ip6_un1.ip6_un1_plen
# define ip6_nxt ip6_ctlun.ip6_un1.ip6_un1_nxt
# define ip6_hlim ip6_ctlun.ip6_un1.ip6_un1_hlim
# define ip6_hops ip6_ctlun.ip6_un1.ip6_un1_hlim
#endif

namespace sihd::net
{

SIHD_REGISTER_FACTORY(IcmpSender);

SIHD_LOGGER;

namespace
{

bool in_bounds(size_t offset, size_t size, size_t total)
{
    return offset + size <= total;
}

} // namespace

IcmpSender::IcmpSender(const std::string & name, sihd::util::Node *parent):
    sihd::util::Named(name, parent),
    _config_applied(false),
    _echo_mode(false),
    _type(-1),
    _code(-1),
    _ttl(-1),
    _id(0),
    _seq(0),
    _socket_type(SOCK_RAW)
{
    _array_rcv_ptr = std::make_unique<util::ArrByte>();
    _array_send_ptr = std::make_unique<util::ArrByte>();

    _poll.set_timeout(1);
    _poll.set_limit(1);
    _poll.add_observer(this);
    _poll.set_service_wait_stop(true);

    _array_rcv_ptr->resize(2048);
    _array_send_ptr->resize(ICMP_MINLEN);

    this->add_conf("poll_timeout", &IcmpSender::set_poll_timeout);
    this->add_conf("echo", &IcmpSender::_set_conf_echo);
    this->add_conf("type", &IcmpSender::_set_conf_type);
    this->add_conf("code", &IcmpSender::_set_conf_code);
    this->add_conf("ttl", &IcmpSender::_set_conf_ttl);
    this->add_conf("id", &IcmpSender::_set_conf_id);
    this->add_conf("seq", &IcmpSender::_set_conf_seq);
    this->add_conf("data_size", &IcmpSender::_set_conf_data_size);
}

IcmpSender::~IcmpSender()
{
    if (this->is_running())
        this->stop();
    (void)this->close();
}

bool IcmpSender::set_poll_timeout(int milliseconds)
{
    return _poll.set_timeout(milliseconds);
}

bool IcmpSender::_set_conf_echo(bool active)
{
    if (active)
        this->set_echo();
    return true;
}

bool IcmpSender::_set_conf_type(int type)
{
    this->set_type(type);
    return true;
}

bool IcmpSender::_set_conf_code(int code)
{
    this->set_code(code);
    return true;
}

bool IcmpSender::_set_conf_ttl(int ttl)
{
    this->set_ttl(ttl);
    return true;
}

bool IcmpSender::_set_conf_id(int id)
{
    this->set_id(static_cast<uint16_t>(id));
    return true;
}

bool IcmpSender::_set_conf_seq(int seq)
{
    this->set_seq(seq);
    return true;
}

bool IcmpSender::_set_conf_data_size(int byte_size)
{
    return this->set_data_size(static_cast<size_t>(byte_size));
}

std::expected<void, sihd::util::Error> IcmpSender::open_socket_unix()
{
    if (_socket.is_open())
        return std::unexpected(
            sihd::util::Error(sihd::util::ErrorCode::already_exists, "IcmpSender: socket already open"));
    _config_applied = false;
    return _socket.open(AF_UNIX, SOCK_RAW, IPPROTO_ICMP);
}

std::expected<void, sihd::util::Error> IcmpSender::open_socket(bool ipv6)
{
    if (_socket.is_open())
        return std::unexpected(
            sihd::util::Error(sihd::util::ErrorCode::already_exists, "IcmpSender: socket already open"));
    _config_applied = false;

    // Default: use SOCK_RAW for both IPv4 and IPv6 (cross-platform)
    // Exception: On Linux, use SOCK_DGRAM for IPv6 echo mode (kernel handles checksums)
    _socket_type = SOCK_RAW;
#if defined(__SIHD_LINUX__)
    if (ipv6 && _echo_mode)
    {
        _socket_type = SOCK_DGRAM;
    }
#endif

    auto res = _socket.open((ipv6 ? AF_INET6 : AF_INET),
                            _socket_type,
                            (ipv6 ? (int)IPPROTO_ICMPV6 : (int)IPPROTO_ICMP));
    if (res)
    {
        (void)_socket.set_reuseaddr(true);
    }
    return res;
}

std::expected<void, sihd::util::Error> IcmpSender::close()
{
    (void)_socket.shutdown();
    return _socket.close();
}

bool IcmpSender::set_data_size(size_t byte_size)
{
    _config_applied = false;
    return _array_send_ptr->byte_resize(ICMP_MINLEN + byte_size);
}

void IcmpSender::set_ttl(int ttl)
{
    _config_applied = false;
    _ttl = ttl;
}

void IcmpSender::set_echo()
{
    _config_applied = false;
    _echo_mode = true;
}

void IcmpSender::set_type(int type)
{
    _config_applied = false;
    _type = type;
    _echo_mode = false;
}

void IcmpSender::set_code(int code)
{
    _config_applied = false;
    _code = code;
    _echo_mode = false;
}

void IcmpSender::set_id(uint16_t id)
{
    _config_applied = false;
    _id = id;
}

void IcmpSender::set_seq(int seq)
{
    _config_applied = false;
    _seq = seq;
}

bool IcmpSender::set_data(sihd::util::ArrByteView view)
{
    _config_applied = false;
    if (!_data_to_set)
        _data_to_set = std::make_unique<util::ArrByte>();
    return _data_to_set && _data_to_set->copy_from_bytes(view);
}

void IcmpSender::_apply_config()
{
    const bool is_ipv6 = _socket.is_ipv6();

    // Apply TTL
    if (_ttl >= 0)
        (void)_socket.set_ttl(_ttl);

    // Apply echo mode or explicit type/code
    if (_echo_mode)
    {
        if (is_ipv6)
        {
            _type = ICMP6_ECHO_REQUEST;
            _code = ICMP_ECHOREPLY; // Not ICMP6_ECHO_REPLY
        }
        else
        {
            _type = ICMP_ECHO;
            _code = ICMP_ECHOREPLY;
        }
    }

    if (_type >= 0)
    {
        if (is_ipv6)
            icmp6()->icmp6_type = _type;
        else
            icmp()->icmp_type = _type;
    }
    if (_code >= 0)
    {
        if (is_ipv6)
            icmp6()->icmp6_code = _code;
        else
            icmp()->icmp_code = _code;
    }

    // Apply ID
    if (is_ipv6)
        icmp6()->icmp6_id = htons(_id);
    else
        icmp()->icmp_id = htons(_id);

    // Apply sequence
    if (is_ipv6)
        icmp6()->icmp6_seq = htons(_seq);
    else
        icmp()->icmp_seq = htons(_seq);

    // Apply data
    if (_data_to_set && _data_to_set->size() > 0)
    {
        if (is_ipv6)
            _array_send_ptr->copy_from_bytes(*_data_to_set, {(ssize_t)sizeof(struct icmp6_hdr)});
        else
            _array_send_ptr->copy_from_bytes(*_data_to_set, {(ssize_t)offsetof(struct icmp, icmp_data)});
    }

    _config_applied = true;
}

std::expected<void, sihd::util::Error> IcmpSender::send_to(const IpAddr & addr)
{
    if (!_socket.is_open())
        return std::unexpected(
            sihd::util::Error(sihd::util::ErrorCode::not_initialized, "IcmpSender: cannot send - socket not opened"));

    if (_config_applied == false)
        this->_apply_config();

    if (_socket.is_ipv6())
    {
        // the v6 checksum needs the pseudo-header: only the kernel knows it (IPV6_CHECKSUM)
        icmp6()->icmp6_cksum = 0;
    }
    else
    {
        icmp()->icmp_cksum = 0;
        icmp()->icmp_cksum = utils::checksum((unsigned short *)icmp(), _array_send_ptr->size());
        if (icmp()->icmp_cksum == 0)
            icmp()->icmp_cksum = 0xffff;
    }
    return _socket.send_all_to(addr, *_array_send_ptr);
}

bool IcmpSender::on_stop()
{
    _poll.stop();
    _poll.clear_fds();
    return true;
}

void IcmpSender::_setup_poll()
{
    _poll.clear_fds();
    _poll.set_read_fd(_socket.socket());
}

bool IcmpSender::on_start()
{
    this->_setup_poll();
    this->service_set_ready();
    std::lock_guard lock(_poll_mutex);
    return _poll.start();
}

bool IcmpSender::poll(int milliseconds)
{
    this->_setup_poll();
    return _poll.poll(milliseconds) > 0;
}

bool IcmpSender::poll()
{
    this->_setup_poll();
    return _poll.poll(_poll.timeout()) > 0;
}

void IcmpSender::handle(sihd::sys::Poll *poll)
{
    auto events = poll->events();
    if (events.size() > 0)
    {
        auto event = events[0];
        if (event.fd == _socket.socket())
        {
            if (event.readable || event.closed)
            {
                this->_read_socket();
            }
            else if (event.error)
            {
                poll->clear_fd(event.fd);
                (void)this->close();
            }
        }
    }
}

void IcmpSender::_read_socket()
{
    _icmp_response.client = IpAddr();

    struct sockaddr_storage addr_storage;
    socklen_t addr_len = sizeof(addr_storage);

    auto ret = this->_socket.receive_from((struct sockaddr *)&addr_storage,
                                          &addr_len,
                                          _array_rcv_ptr->buf(),
                                          _array_rcv_ptr->byte_capacity());
    if (ret && ret.value() > 0)
    {
        _array_rcv_ptr->byte_resize(ret.value());

        // Extract IP address from sockaddr
        if (addr_storage.ss_family == AF_INET6)
        {
            struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)&addr_storage;
            _icmp_response.client = IpAddr(*reinterpret_cast<sockaddr *>(sin6));
        }
        else
        {
            struct sockaddr_in *sin = (struct sockaddr_in *)&addr_storage;
            _icmp_response.client = IpAddr(*reinterpret_cast<sockaddr *>(sin));
        }

        if (_socket.is_ipv6())
            this->_process_ipv6();
        else
            this->_process_ipv4();
    }
}

void IcmpSender::_process_ipv6()
{
    const size_t total = _array_rcv_ptr->byte_size();
    const uint8_t *base = _array_rcv_ptr->buf();
    size_t offset = 0;
    uint8_t ttl = 0;

    // On Linux with SOCK_DGRAM, kernel strips IPv6 header
    // With SOCK_RAW, IPv6 header might be present
    // Check if first byte looks like IPv6 version (0x6X)
    if (_socket_type == SOCK_RAW && total >= sizeof(struct ip6_hdr) && ((base[0] >> 4) & 0x0F) == 6)
    {
        const struct ip6_hdr *ip6hdr = (const struct ip6_hdr *)base;
        ttl = ip6hdr->ip6_hlim;
        offset = sizeof(struct ip6_hdr);
    }

    if (in_bounds(offset, sizeof(struct icmp6_hdr), total) == false)
        return;
    const struct icmp6_hdr *icmp6hdr = (const struct icmp6_hdr *)(base + offset);

    const bool time_exceeded = (icmp6hdr->icmp6_type == ICMP6_TIME_EXCEEDED
                                && icmp6hdr->icmp6_code == ICMP6_TIME_EXCEED_TRANSIT);
    if (icmp6hdr->icmp6_type != ICMP6_ECHO_REPLY && icmp6hdr->icmp6_type != ICMP6_ECHO_REQUEST && !time_exceeded)
        return;

    if (time_exceeded)
    {
        offset += sizeof(struct icmp6_hdr) + sizeof(struct ip6_hdr);
        if (in_bounds(offset, sizeof(struct icmp6_hdr), total) == false)
            return;
        icmp6hdr = (const struct icmp6_hdr *)(base + offset);
        if (icmp6hdr->icmp6_type != ICMP6_ECHO_REPLY && icmp6hdr->icmp6_type != ICMP6_ECHO_REQUEST)
            return;
    }

    const size_t data_offset = offset + sizeof(struct icmp6_hdr);
    _icmp_response.data = (char *)base + data_offset;
    _icmp_response.size = total - data_offset;
    _icmp_response.type = icmp6hdr->icmp6_type;
    _icmp_response.code = icmp6hdr->icmp6_code;
    _icmp_response.ttl = ttl;
    _icmp_response.id = ntohs(icmp6hdr->icmp6_id);
    _icmp_response.seq = ntohs(icmp6hdr->icmp6_seq);

    this->notify_observers(this);
}

void IcmpSender::_process_ipv4()
{
    const size_t total = _array_rcv_ptr->byte_size();
    const uint8_t *base = _array_rcv_ptr->buf();

    if (total < sizeof(struct ip))
        return;
    const struct ip *iphdr = (const struct ip *)base;
    if (iphdr->ip_p != IPPROTO_ICMP || iphdr->ip_hl < 5)
        return;
    const size_t icmp_offset = (size_t)(iphdr->ip_hl << 2);
    if (in_bounds(icmp_offset, ICMP_MINLEN, total) == false)
        return;
    const struct icmp *icmphdr = (const struct icmp *)(base + icmp_offset);
    // some raw delivery paths skip kernel checksum validation
    if (utils::checksum((uint16_t *)icmphdr, (int)(total - icmp_offset)) != 0)
        return;

    const bool time_exceeded = (icmphdr->icmp_type == ICMP_TIME_EXCEEDED
                                && icmphdr->icmp_code == ICMP_TIMXCEED_INTRANS);
    if (icmphdr->icmp_type != ICMP_ECHOREPLY && icmphdr->icmp_type != ICMP_ECHO && !time_exceeded)
        return;

    if (time_exceeded)
    {
        const size_t inner_offset = icmp_offset + ICMP_MINLEN;
        if (in_bounds(inner_offset, sizeof(struct ip), total) == false)
            return;
        const struct ip *inner_ip = (const struct ip *)(base + inner_offset);
        if (inner_ip->ip_hl < 5)
            return;
        const size_t inner_icmp_offset = inner_offset + (size_t)(inner_ip->ip_hl << 2);
        if (in_bounds(inner_icmp_offset, ICMP_MINLEN, total) == false)
            return;
        const struct icmp *inner_icmp = (const struct icmp *)(base + inner_icmp_offset);
        // rfc792 allows quoting as little as ip header + 8 bytes: the inner checksum may not verify
        if (inner_icmp->icmp_type != ICMP_ECHOREPLY && inner_icmp->icmp_type != ICMP_ECHO)
            return;
        iphdr = inner_ip;
        icmphdr = inner_icmp;
    }

    const size_t data_offset = (size_t)((const uint8_t *)icmphdr->icmp_data - base);
    _icmp_response.data = (char *)base + data_offset;
    _icmp_response.size = total - data_offset;
    _icmp_response.type = icmphdr->icmp_type;
    _icmp_response.code = icmphdr->icmp_code;
    _icmp_response.ttl = iphdr->ip_ttl;
    _icmp_response.id = ntohs(icmphdr->icmp_id);
    _icmp_response.seq = ntohs(icmphdr->icmp_seq);

    this->notify_observers(this);
}

struct icmp *IcmpSender::icmp()
{
    return (struct icmp *)_array_send_ptr->buf();
}

struct icmp6_hdr *IcmpSender::icmp6()
{
    return (struct icmp6_hdr *)_array_send_ptr->buf();
}

} // namespace sihd::net
