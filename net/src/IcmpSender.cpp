#include <sihd/util/Array.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>

#include <sihd/net/IcmpSender.hpp>
#include <sihd/net/utils.hpp>

#if !defined(__SIHD_WINDOWS__)
# include <netinet/icmp6.h>   // icmpv6 macros
# include <netinet/ip_icmp.h> // icmp macros
#endif

namespace sihd::net
{

SIHD_UTIL_REGISTER_FACTORY(IcmpSender);

SIHD_LOGGER;

IcmpSender::IcmpSender(const std::string & name, sihd::util::Node *parent): sihd::util::Named(name, parent)
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
    _poll.set_timeout(milliseconds);
    return true;
}

bool IcmpSender::open_socket_unix()
{
    if (_socket.is_open())
        return false;
    return _socket.open(AF_UNIX, SOCK_RAW, IPPROTO_ICMP);
}

bool IcmpSender::open_socket(bool ipv6)
{
    if (_socket.is_open())
        return false;
    bool ret = _socket.open((ipv6 ? AF_INET6 : AF_INET), SOCK_RAW, (ipv6 ? (int)IPPROTO_ICMPV6 : (int)IPPROTO_ICMP));
    if (ret)
    {
        _socket.set_reuseaddr(true);
#pragma message("TODO IcmpSender ipv6")
        /*
        if (ipv6)
        {
            struct icmp6_filter filter;
            ICMP6_FILTER_SETBLOCKALL(&filter);
            ret = sihd::util::os::setsockopt(_socket, IPPROTO_ICMPV6, ICMP6_FILTER, (const void *)&filter,
        sizeof(filter));
        }
        */
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
    return _array_send_ptr->byte_resize(ICMP_MINLEN + byte_size);
}

bool IcmpSender::set_ttl(int ttl)
{
    return _socket.is_open() && _socket.set_ttl(ttl);
}

void IcmpSender::set_echo()
{
    if (_socket.is_ipv6())
    {
        this->set_type(ICMP6_ECHO_REQUEST);
        this->set_code(ICMP6_ECHO_REPLY);
    }
    else
    {
        this->set_type(ICMP_ECHO);
        this->set_code(ICMP_ECHOREPLY);
    }
}

void IcmpSender::set_type(int type)
{
    if (_socket.is_ipv6())
        icmp6()->icmp6_type = type;
    else
        icmp()->icmp_type = type;
}

void IcmpSender::set_code(int code)
{
    if (_socket.is_ipv6())
        icmp6()->icmp6_code = code;
    else
        icmp()->icmp_code = code;
}

void IcmpSender::set_id(pid_t id)
{
    if (_socket.is_ipv6())
        icmp6()->icmp6_id = htons(id);
    else
        icmp()->icmp_id = htons(id);
}

void IcmpSender::set_seq(int seq)
{
    if (_socket.is_ipv6())
        icmp6()->icmp6_seq = htons(seq);
    else
        icmp()->icmp_seq = htons(seq);
}

bool IcmpSender::set_data(sihd::util::ArrByteView view)
{
    bool ret;
    if (_socket.is_ipv6())
        ret = _array_send_ptr->copy_from_bytes(view, offsetof(struct icmp6_hdr, icmp6_data8));
    else
        ret = _array_send_ptr->copy_from_bytes(view, offsetof(struct icmp, icmp_data));
    return ret;
}

bool IcmpSender::send_to(const IpAddr & addr)
{
    if (_socket.is_ipv6())
    {
        icmp6()->icmp6_cksum = 0;
        icmp6()->icmp6_cksum = utils::checksum((unsigned short *)icmp6(), _array_send_ptr->size());
        if (icmp6()->icmp6_cksum == 0)
            icmp6()->icmp6_cksum = 0xffff;
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

void IcmpSender::handle(sihd::util::Poll *poll)
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
    _icmp_response.client.clear();
    ssize_t ret = _socket.receive_from(_icmp_response.client, *_array_rcv_ptr);
    if (ret > 0)
    {
        if (_socket.is_ipv6())
            this->_process_ipv6();
        else
            this->_process_ipv4();
    }
}

void IcmpSender::_process_ipv6()
{
#pragma message("TODO handle ipv6")
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
