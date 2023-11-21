#include <sys/types.h>
#include <unistd.h>

#include <sihd/util/Array.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>
#include <sihd/util/time.hpp>

#include <sihd/net/Pinger.hpp>

#define ICMP_ECHO_REQUEST_LENGTH 56
namespace sihd::net
{

SIHD_UTIL_REGISTER_FACTORY(Pinger)

SIHD_LOGGER;

using namespace sihd::util;

Pinger::Pinger(const std::string & name, sihd::util::Node *parent):
    sihd::util::Named(name, parent),
    _sender("icmp-sender")
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

    _data_ptr = std::make_unique<sihd::util::ArrByte>();
    _data_ptr->resize(ICMP_ECHO_REQUEST_LENGTH);

    // set gibberish values like ping in packet
    char values = 10;
    size_t i = sizeof(time_t);
    while (i < ICMP_ECHO_REQUEST_LENGTH)
    {
        _data_ptr->get(i) = values++;
        ++i;
    }
}

Pinger::~Pinger()
{
    this->stop();
}

bool Pinger::open_unix()
{
    return _sender.open_socket_unix();
}

bool Pinger::open(bool ipv6)
{
    return _sender.open_socket(ipv6);
}

bool Pinger::set_ttl(int ttl)
{
    _ttl = ttl;
    return true;
}

bool Pinger::set_timeout(time_t milliseconds_interval)
{
    return _sender.set_poll_timeout(milliseconds_interval);
}

bool Pinger::set_interval(time_t milliseconds_interval)
{
    _ping_ms_interval = milliseconds_interval;
    return true;
}

void Pinger::stop()
{
    _sender.stop();
    auto l = _waitable.guard();
    _stop = true;
    _waitable.notify();
}

bool Pinger::ping(const IpAddr & client, size_t number)
{
    if (_running.exchange(true))
        return false;
    // setup ping
    this->_clear_event();
    _result.clear();
    _stop = false;
    _sender.set_id(os::pid());
    _sender.set_ttl(_ttl);

    _result.time_start = _clock_ptr->now();

    bool ret = true;
    size_t i = 0;
    while (_stop == false && (number == 0 || i < number))
    {
        if (i > 0)
        {
            const time_t time_spent_sending_last_ping = _clock_ptr->now() - _result.last_time_sent;
            // wait between pings
            _waitable.wait_for(time::milliseconds(_ping_ms_interval) - time_spent_sending_last_ping,
                               [this] { return _stop.load(); });
            if (_stop)
                break;
        }

        // set sequence number and packet timestamp
        _current_seq = i + 1;
        _sender.set_seq(_current_seq);
        _result.last_time_sent = _clock_ptr->now();
        _data_ptr->copy_from_bytes(&_result.last_time_sent, sizeof(time_t));
        _sender.set_data(*_data_ptr);
        // send icmp echo
        ret = _sender.send_to(client);
        if (ret == false)
        {
            SIHD_LOG_ERROR("Pinger: failed sending to client {}", client.hostname());
            break;
        }
        _result.transmitted += 1;
        this->_notify_send();

        // wait until seq number is received by polling
        _received_icmp_response = false;
        while (_stop == false && _received_icmp_response == false && _sender.poll())
            ;
        if (_received_icmp_response == false)
            this->_notify_timeout();
        ++i;
    }
    _result.time_end = _clock_ptr->now();

    _running = false;
    return ret;
}

void Pinger::_notify_send()
{
    _event.sent = true;
    this->notify_observers(this);
    this->_clear_event();
}

void Pinger::handle(IcmpSender *sender)
{
    const IcmpResponse & response = sender->response();
    if (_current_seq != response.seq || os::pid() != response.id)
        return;
    _received_icmp_response = true;

    const time_t timestamp = ((time_t *)response.data)[0];
    const time_t now = _clock_ptr->now();
    const time_t triptime = now - timestamp;

    _result.received++;
    _result.last_time_received = now;
    _result.rtt.add_sample(triptime);

    _event.received = true;
    _event.icmp_response = response;
    _event.trip_time = triptime;

    this->notify_observers(this);
    this->_clear_event();
}

void Pinger::_notify_timeout()
{
    _event.timeout = true;
    this->notify_observers(this);
    this->_clear_event();
}

void Pinger::_clear_event()
{
    _event = PingEvent {};
}

void PingResult::clear()
{
    time_start = 0;
    last_time_sent = 0;
    last_time_received = 0;
    time_end = 0;
    transmitted = 0;
    received = 0;
    rtt.clear();
}

float PingResult::packet_loss() const
{
    return transmitted > 0 ? ((float)((float)(transmitted - rtt.samples) / (float)transmitted) * 100) : 0.0;
}

std::string PingResult::str() const
{
    return fmt::format("{} packets transmitted, {} received, {:.0f}% packet loss, time {:.3f}ms\n"
                       "rtt min/avg/max/mdev = {:.3f}/{:.3f}/{:.3f}/{:.3f} ms",
                       transmitted,
                       rtt.samples,
                       this->packet_loss(),
                       time::to_double_milliseconds(time_end - time_start),
                       time::to_double_milliseconds(rtt.min),
                       time::to_double_milliseconds(rtt.average()),
                       time::to_double_milliseconds(rtt.max),
                       time::to_double_milliseconds(rtt.standard_deviation()));
}

} // namespace sihd::net
