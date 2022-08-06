#include <sys/types.h>
#include <unistd.h>

#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>
#include <sihd/util/Time.hpp>

#include <sihd/net/Pinger.hpp>

#define ICMP_ECHO_REQUEST_LENGTH 56
namespace sihd::net
{

SIHD_UTIL_REGISTER_FACTORY(Pinger)

SIHD_LOGGER;

using namespace sihd::util;

Pinger::Pinger(const std::string & name, sihd::util::Node *parent):
    sihd::util::Named(name, parent), _sender("icmp-sender")
{
    _sender.set_data_size(ICMP_ECHO_REQUEST_LENGTH);
    _sender.set_echo();
    _sender.add_observer(this);

    _stop = false;
    _running = false;

    _ttl = 64;
    _ping_ms_interval = 1000;
    _sender.set_poll_timeout(1000);

    _clock_ptr = &sihd::util::Clock::default_clock;

    _data.resize(ICMP_ECHO_REQUEST_LENGTH);

    // set gibberish values like ping in packet
    char values = 10;
    size_t i = sizeof(time_t);
    while (i < ICMP_ECHO_REQUEST_LENGTH)
    {
        _data[i] = values++;
        ++i;
    }
}

Pinger::~Pinger()
{
    this->stop();
}

bool    Pinger::open_unix()
{
    return _sender.open_socket_unix();
}

bool    Pinger::open(bool ipv6)
{
    return _sender.open_socket(ipv6);
}

bool    Pinger::set_ttl(int ttl)
{
    _ttl = ttl;
    return true;
}

bool    Pinger::set_timeout(time_t milliseconds_interval)
{
    return _sender.set_poll_timeout(milliseconds_interval);
}

bool    Pinger::set_interval(time_t milliseconds_interval)
{
    _ping_ms_interval = milliseconds_interval;
    return true;
}

void    Pinger::stop()
{
    _stop = true;
    for (int i = 0; _running && i < 3; ++i)
    {
        _waitable.notify_all();
        Time::msleep(1);
    }
}

bool    Pinger::ping(const IpAddr & client, size_t number)
{
    if (_running.exchange(true))
        return false;
    // setup ping
    memset(&_sums, 0, sizeof(InternalPingRes));
    memset(&_result, 0, sizeof(PingResult));
    _stop = false;
    _sender.set_id(OS::pid());
    _sender.set_ttl(_ttl);

    _result.time_start = _clock_ptr->now();

    bool ret = true;
    size_t i = 0;
    while (_stop == false && (number == 0 || i < number))
    {
        if (i > 0)
        {
            // wait between pings
            _waitable.wait_for(Time::milliseconds(_ping_ms_interval) - (_clock_ptr->now() - _result.last_time_sent));
            if (_stop)
                break ;
        }
        // set sequence number and packet timestamp
        _current_seq = i + 1;
        _sender.set_seq(_current_seq);
        _result.last_time_sent = _clock_ptr->now();
        _data.copy_from_bytes(&_result.last_time_sent, sizeof(time_t));
        _sender.set_data(_data);
        // send icmp echo
        ret = _sender.send_to(client);
        if (ret == false)
            break ;
        _result.transmitted += 1;
        // wait until seq number is received by polling
        _received = false;
        while (_stop == false && _received == false && _sender.poll())
            ;
        ++i;
    }
    _result.time_end = _clock_ptr->now();

    // standard deviation
    if (_result.received > 0)
    {
        long long tmp_sum = _sums.sum / _result.received;
        long long tmp_sum2 = _sums.sum2 / _result.received;
        _result.rtt_mdev = sqrt(tmp_sum2 - tmp_sum * tmp_sum);
    }

    _running = false;
    return ret;
}

void    Pinger::handle(IcmpSender *sender)
{
    const IcmpResponse & response = sender->response();
    if (_current_seq != response.seq || OS::pid() != response.id)
        return ;
    _received = true;
    _result.received += 1;

    time_t timestamp = ((time_t *)response.data)[0];
    time_t now = _clock_ptr->now();
    int triptime = now - timestamp;

    if (_result.received == 1)
    {
        _result.rtt_max = triptime;
        _result.rtt_avg = triptime;
        _result.rtt_min = triptime;
    }
    else
    {
        _result.rtt_min = std::min(triptime, _result.rtt_min);
        _result.rtt_avg = _result.rtt_avg + ((triptime - _result.rtt_avg) / _result.received);
        _result.rtt_max = std::max(triptime, _result.rtt_max);
    }
    _sums.sum += triptime;
    _sums.sum2 += (long long)triptime * (long long)triptime;
}


float   PingResult::packet_loss() const
{
    return transmitted > 0 ? ((float)((float)(transmitted - received) / (float)transmitted) * 100) : 0.0;
}

std::string PingResult::str() const
{
    return fmt::format("{} packets transmitted, {} received, {:.0}% packet loss, time {:.3f}ms\n"
                        "rtt min/avg/max/mdev = {:.3f}/{:.3f}/{:.3f}/{:.3f} ms",
                        transmitted, received, this->packet_loss(),
                        Time::to_double_milliseconds(time_end - time_start),
                        Time::to_double_milliseconds(rtt_min),
                        Time::to_double_milliseconds(rtt_avg),
                        Time::to_double_milliseconds(rtt_max),
                        Time::to_double_milliseconds(rtt_mdev));
}

}