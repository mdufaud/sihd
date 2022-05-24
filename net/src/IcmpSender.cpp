#include <sihd/net/IcmpSender.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>

#include <sys/types.h>
#include <unistd.h>

#define ICMP_ECHO_REQUEST_LENGTH 56

namespace sihd::net
{

SIHD_UTIL_REGISTER_FACTORY(IcmpSender);

SIHD_LOGGER;

IcmpSender::IcmpSender(const std::string & name, sihd::util::Node *parent):
    sihd::util::Named(name, parent)
{
    _poll.set_timeout(1);
    _poll.set_limit(1);
    _poll.add_observer(this);

    _array_rcv.resize(256);
    _array_send.resize(ICMP_MINLEN + ICMP_ECHO_REQUEST_LENGTH);

    _clock_ptr = &sihd::util::Clock::default_clock;

    this->add_conf("poll_timeout", &IcmpSender::set_poll_timeout);
}

IcmpSender::~IcmpSender()
{
}

bool    IcmpSender::set_poll_timeout(int milliseconds)
{
    _poll.set_timeout(milliseconds);
    return true;
}

bool    IcmpSender::open_socket_unix()
{
    if (_socket.is_open())
        return false;
    return _socket.open(AF_UNIX, SOCK_RAW, IPPROTO_ICMP);
}

bool    IcmpSender::open_socket(bool ipv6)
{
    if (_socket.is_open())
        return false;
    bool ret = _socket.open((ipv6 ? AF_INET6 : AF_INET), SOCK_RAW, (ipv6 ? (int)IPPROTO_ICMPV6 : (int)IPPROTO_ICMP));
    if (ret)
    {
        _socket.set_reuseaddr(true);
        this->set_ttl(15);
        this->set_id(getpid());
        // TODO
        /*
        if (ipv6)
        {
            struct icmp6_filter filter;
            ICMP6_FILTER_SETBLOCKALL(&filter);
            ret = sihd::util::OS::setsockopt(_socket, IPPROTO_ICMPV6, ICMP6_FILTER, (const void *)&filter, sizeof(filter));
        }
        */
    }
    return ret;
}

bool    IcmpSender::close()
{
    _poll.clear_fds();
    _socket.shutdown();
    return _socket.close();
}

bool    IcmpSender::set_ttl(int ttl)
{
    return _socket.set_ttl(ttl);
}

void    IcmpSender::set_echo()
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

void    IcmpSender::set_type(int type)
{
    if (_socket.is_ipv6())
        icmp()->icmp_type = type;
    else
        icmp6()->icmp6_type = type;
}

void    IcmpSender::set_code(int code)
{
    if (_socket.is_ipv6())
        icmp()->icmp_code = code;
    else
        icmp6()->icmp6_code = code;
}

void    IcmpSender::set_id(pid_t id)
{
    if (_socket.is_ipv6())
        icmp()->icmp_id = id;
    else
        icmp6()->icmp6_id = id;
}

void    IcmpSender::set_seq(int seq)
{
    if (_socket.is_ipv6())
        icmp()->icmp_seq = seq;
    else
        icmp6()->icmp6_seq = seq;
}

bool    IcmpSender::set_data(sihd::util::ArrViewByte view)
{
    bool ret = true;
    if (_array_send.size() < view.size())
        ret = _array_send.resize(view.size());
    return ret && _array_send.copy_from(view);
}

void    IcmpSender::_prepare_send()
{
    if (_socket.is_ipv6())
    {
        //TODO
    }
    else
    {
        time_t now = _clock_ptr->now();
        memcpy(icmp()->icmp_data, &now, sizeof(time_t));
        icmp()->icmp_cksum = this->_in_cksum((unsigned short *)icmp(), _array_send.size());
    }
}

void    IcmpSender::_after_send()
{
    if (_socket.is_ipv6())
        icmp()->icmp_seq += 1;
    else
        icmp6()->icmp6_seq +=1 ;
}

ssize_t IcmpSender::send_to(const IpAddr & addr)
{
    this->_prepare_send();
    bool ret = _socket.send_all_to(addr, _array_send);
    if (ret)
        this->_after_send();
    return ret;
}

bool    IcmpSender::stop()
{
    _poll.stop();
    _poll.wait_stop();
    return true;
}

void    IcmpSender::_setup_poll()
{
    _poll.clear_fds();
    _poll.set_read_fd(_socket.socket());
}

bool    IcmpSender::run()
{
    this->_setup_poll();
    std::lock_guard lock(_poll_mutex);
    return _poll.run();
}

bool    IcmpSender::poll(int milliseconds)
{
    this->_setup_poll();
    return _poll.poll(milliseconds) > 0;
}

bool    IcmpSender::poll()
{
    this->_setup_poll();
    return _poll.poll(_poll.timeout()) > 0;
}

void    IcmpSender::handle(sihd::util::Poll *poll)
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

void    IcmpSender::_read_socket()
{
    _icmp_response.client.clear();
    _socket.receive_from(_icmp_response.client, _array_rcv);
    if (_socket.is_ipv6())
        this->_process_ipv6();
    else
        this->_process_ipv4();
}

void    IcmpSender::_process_ipv6()
{
    //TODO
}

void    IcmpSender::_process_ipv4()
{
    struct ip *iphdr = (struct ip *)_array_rcv.buf();
    size_t iphdr_len = iphdr->ip_hl << 2;
    struct icmp *icmphdr = (struct icmp *)(_array_rcv.buf() + iphdr_len);

    SIHD_TRACE(sizeof(*iphdr));
    SIHD_TRACE(sizeof(*icmphdr));
    SIHD_TRACE(_array_rcv.size());

    if (iphdr->ip_p == IPPROTO_ICMP)
    {
        if (icmphdr->icmp_type == ICMP_ECHOREPLY
            || (icmphdr->icmp_type == ICMP_TIME_EXCEEDED
                && icmphdr->icmp_code == ICMP_TIMXCEED_INTRANS))
        {
            if (icmphdr->icmp_type == ICMP_TIME_EXCEEDED
                && icmphdr->icmp_code == ICMP_TIMXCEED_INTRANS)
            {
                // get original packet
                iphdr = &icmphdr->icmp_ip;
                iphdr_len = iphdr->ip_hl << 2;
                icmphdr = (struct icmp *)((char *)iphdr + iphdr_len);
            }
            // id is the same as original packet and it is our type
            _icmp_response.reached = (icmphdr->icmp_id == icmp()->icmp_id) && icmphdr->icmp_type == ICMP_ECHOREPLY;
            _icmp_response.type = icmphdr->icmp_type;
            _icmp_response.code = icmphdr->icmp_code;
            _icmp_response.ttl = iphdr->ip_ttl;
            _icmp_response.id = icmphdr->icmp_id;
            _icmp_response.seq = icmphdr->icmp_seq;
            _icmp_response.timestamp = (time_t)(*icmphdr->icmp_data);
            this->notify_observers(&_icmp_response);
        }
    }
}

//Source code from the book "UNIX Network Programming"
uint16_t IcmpSender::_in_cksum(uint16_t *addr, int len)
{
	int nleft = len;
	uint32_t sum = 0;
	uint16_t *w = addr;
	uint16_t answer = 0;

	/*
	* Our algorithm is simple, using a 32 bit accumulator (sum), we add
	* sequential 16 bit words to it, and at the end, fold back all the
	* carry bits from the top 16 bits into the lower 16 bits.
	*/
	while (nleft > 1)
	{
		sum += *w++;
		nleft -= 2;
	}

	/* 4mop ​​up an odd byte, if necessary */
	if (nleft == 1)
    {
		*(unsigned char *)(&answer) = *(unsigned char *)w;
		sum += answer;
	}

	/* 4add back carry outs from top 16 bits to low 16 bits */
	sum = (sum >> 16) + (sum & 0xffff);/* add hi 16 to low 16 */
	sum += (sum >> 16);/* add carry */
	answer = ~sum;/* truncate to 16 bits */
	return answer;
}

struct icmp     *IcmpSender::icmp()
{
    return (struct icmp *)_array_send.buf();
}

struct icmp6_hdr    *IcmpSender::icmp6()
{
    return (struct icmp6_hdr *)_array_send.buf();
}

}