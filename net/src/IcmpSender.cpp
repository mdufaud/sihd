#include <cstdint>
#include <sihd/util/Array.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/sys/NamedFactory.hpp>

#include <sihd/net/IcmpSender.hpp>
#include <sihd/net/utils.hpp>
#include <stdexcept>

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

    _array_rcv_ptr->resize(256);
    _array_send_ptr->resize(ICMP_MINLEN);

    this->add_conf("poll_timeout", &IcmpSender::set_poll_timeout);
}

IcmpSender::~IcmpSender()
{
    if (this->is_running())
        this->stop();
    this->close();
}

bool IcmpSender::set_poll_timeout(int milliseconds)
{
    return _poll.set_timeout(milliseconds);
}

bool IcmpSender::open_socket_unix()
{
    if (_socket.is_open())
        return false;
    _config_applied = false;
    return _socket.open(AF_UNIX, SOCK_RAW, IPPROTO_ICMP);
}

bool IcmpSender::open_socket(bool ipv6)
{
    if (_socket.is_open())
        return false;
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

    bool ret = _socket.open((ipv6 ? AF_INET6 : AF_INET),
                            _socket_type,
                            (ipv6 ? (int)IPPROTO_ICMPV6 : (int)IPPROTO_ICMP));
    if (ret)
    {
        _socket.set_reuseaddr(true);
    }
    return ret;
}

bool IcmpSender::close()
{
    _socket.shutdown();
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
        _socket.set_ttl(_ttl);

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
            _array_send_ptr->copy_from_bytes(*_data_to_set, sizeof(struct icmp6_hdr));
        else
            _array_send_ptr->copy_from_bytes(*_data_to_set, offsetof(struct icmp, icmp_data));
    }

    _config_applied = true;
}

bool IcmpSender::send_to(const IpAddr & addr)
{
    if (!_socket.is_open())
    {
        SIHD_LOG(error, "IcmpSender: cannot send - socket not opened");
        return false;
    }

    if (_config_applied == false)
        this->_apply_config();

    if (_socket.is_ipv6())
    {
        icmp6()->icmp6_cksum = 0;
        if (_socket_type == SOCK_RAW)
        {
            // For SOCK_RAW, calculate the checksum manually
            icmp6()->icmp6_cksum = utils::checksum((unsigned short *)icmp6(), _array_send_ptr->size());
            if (icmp6()->icmp6_cksum == 0)
                icmp6()->icmp6_cksum = 0xffff;
        }
        // For SOCK_DGRAM, kernel calculates checksum automatically (icmp6_cksum = 0)
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
                this->close();
            }
        }
    }
}

void IcmpSender::_read_socket()
{
    _icmp_response.client = IpAddr();

    struct sockaddr_storage addr_storage;
    socklen_t addr_len = sizeof(addr_storage);

    ssize_t ret = ::recvfrom(_socket.socket(),
                             _array_rcv_ptr->buf(),
                             _array_rcv_ptr->byte_capacity(),
                             0,
                             (struct sockaddr *)&addr_storage,
                             &addr_len);

    if (ret > 0)
    {
        _array_rcv_ptr->byte_resize(ret);

        // Extract IP address from sockaddr
        if (addr_storage.ss_family == AF_INET6)
        {
            struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)&addr_storage;
            char buf[INET6_ADDRSTRLEN];
            inet_ntop(AF_INET6, &sin6->sin6_addr, buf, sizeof(buf));
            _icmp_response.client = IpAddr(buf, true);
        }
        else
        {
            struct sockaddr_in *sin = (struct sockaddr_in *)&addr_storage;
            char buf[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &sin->sin_addr, buf, sizeof(buf));
            _icmp_response.client = IpAddr(buf, false);
        }

        if (_socket.is_ipv6())
            this->_process_ipv6();
        else
            this->_process_ipv4();
    }
}

void IcmpSender::_process_ipv6()
{
    struct icmp6_hdr *icmp6hdr = (struct icmp6_hdr *)_array_rcv_ptr->buf();
    uint8_t ttl = 0;

    // On Linux with SOCK_DGRAM, kernel strips IPv6 header
    // With SOCK_RAW, IPv6 header might be present
    // Check if first byte looks like IPv6 version (0x6X)
    if (_socket_type == SOCK_RAW && _array_rcv_ptr->byte_size() >= sizeof(struct ip6_hdr))
    {
        uint8_t version = (*_array_rcv_ptr->buf() >> 4) & 0x0F;
        if (version == 6)
        {
            // IPv6 header is present
            struct ip6_hdr *ip6hdr = (struct ip6_hdr *)_array_rcv_ptr->buf();
            ttl = ip6hdr->ip6_hlim;
            icmp6hdr = (struct icmp6_hdr *)((char *)ip6hdr + sizeof(struct ip6_hdr));
        }
    }

    if (icmp6hdr->icmp6_type == ICMP6_ECHO_REPLY || icmp6hdr->icmp6_type == ICMP6_ECHO_REQUEST
        || (icmp6hdr->icmp6_type == ICMP6_TIME_EXCEEDED && icmp6hdr->icmp6_code == ICMP6_TIME_EXCEED_TRANSIT))
    {
        if (icmp6hdr->icmp6_type == ICMP6_TIME_EXCEEDED && icmp6hdr->icmp6_code == ICMP6_TIME_EXCEED_TRANSIT)
        {
            // get original packet from the error message
            icmp6hdr
                = (struct icmp6_hdr *)((char *)icmp6hdr + sizeof(struct icmp6_hdr) + sizeof(struct ip6_hdr));
        }
        _icmp_response.data = (char *)(icmp6hdr + 1);
        _icmp_response.size
            = _array_rcv_ptr->byte_size() - ((char *)(icmp6hdr + 1) - (char *)_array_rcv_ptr->buf());
        _icmp_response.type = icmp6hdr->icmp6_type;
        _icmp_response.code = icmp6hdr->icmp6_code;
        _icmp_response.ttl = ttl;
        _icmp_response.id = ntohs(icmp6hdr->icmp6_id);
        _icmp_response.seq = ntohs(icmp6hdr->icmp6_seq);

        this->notify_observers(this);
    }
}

void IcmpSender::_process_ipv4()
{
    struct ip *iphdr = (struct ip *)_array_rcv_ptr->buf();
    size_t iphdr_len = iphdr->ip_hl << 2;
    struct icmp *icmphdr = (struct icmp *)((char *)iphdr + iphdr_len);

    if (iphdr->ip_p == IPPROTO_ICMP)
    {
        if (icmphdr->icmp_type == ICMP_ECHOREPLY || icmphdr->icmp_type == ICMP_ECHO
            || (icmphdr->icmp_type == ICMP_TIME_EXCEEDED && icmphdr->icmp_code == ICMP_TIMXCEED_INTRANS))
        {
            if (icmphdr->icmp_type == ICMP_TIME_EXCEEDED && icmphdr->icmp_code == ICMP_TIMXCEED_INTRANS)
            {
                // get original packet
                iphdr = &icmphdr->icmp_ip;
                iphdr_len = iphdr->ip_hl << 2;
                icmphdr = (struct icmp *)((char *)iphdr + iphdr_len);
            }
            // id is the same as original packet and it is our type
            _icmp_response.data = icmphdr->icmp_data;
            _icmp_response.size
                = _array_rcv_ptr->byte_size() - ((char *)icmphdr->icmp_data - (char *)_array_rcv_ptr->buf());
            _icmp_response.type = icmphdr->icmp_type;
            _icmp_response.code = icmphdr->icmp_code;
            _icmp_response.ttl = iphdr->ip_ttl;
            _icmp_response.id = ntohs(icmphdr->icmp_id);
            _icmp_response.seq = ntohs(icmphdr->icmp_seq);

            this->notify_observers(this);
        }
    }
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
