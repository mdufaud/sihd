#include <cstddef>
#include <memory>
#include <regex.h>
#include <sys/types.h>
#include <unistd.h>

#include <sihd/sys/NamedFactory.hpp>
#include <sihd/util/Array.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/time.hpp>

#include <sihd/sys/os.hpp>

#include <sihd/net/Pinger.hpp>

#define ICMP_ECHO_REQUEST_LENGTH 56
#define ICMP6_ECHO_REQUEST_LENGTH 56

namespace sihd::net
{

SIHD_REGISTER_FACTORY(Pinger)

SIHD_LOGGER;

using namespace sihd::util;

namespace
{

void set_internal_data_size(IcmpSender & sender, std::unique_ptr<sihd::util::ArrByte> & data_ptr, size_t len)
{
    // Initialize data buffer now that socket type is known
    sender.set_data_size(len);
    data_ptr = std::make_unique<sihd::util::ArrByte>();
    data_ptr->resize(len);
    // set gibberish values like ping in packet
    char values = 10;
    for (size_t j = sizeof(time_t); j < len; ++j)
    {
        data_ptr->get(j) = values++;
    }
}

} // namespace

Pinger::Pinger(const std::string & name, sihd::util::Node *parent):
    sihd::util::Named(name, parent),
    _sender("icmp-sender")
{
    _ping_count = 0;

    _sender.add_observer(this);

    _stop = false;

    _ttl = 64;
    _ping_ms_interval = 1000;
    _sender.set_poll_timeout(1000);

    _clock_ptr = &sihd::util::Clock::default_clock;

    this->add_conf("ttl", &Pinger::set_ttl);
    this->add_conf("timeout", &Pinger::set_timeout);
    this->add_conf("interval", &Pinger::set_interval);
    this->add_conf("ping_count", &Pinger::set_ping_count);
    this->add_conf<std::string_view>("client",
                                     [this](std::string_view host) { return this->set_client(host); });
}

Pinger::~Pinger()
{
    if (this->is_running())
        this->stop();
}

bool Pinger::open_unix()
{
    set_internal_data_size(_sender, _data_ptr, ICMP_ECHO_REQUEST_LENGTH);
    return _sender.open_socket_unix();
}

bool Pinger::open(bool ipv6)
{
    if (ipv6)
        set_internal_data_size(_sender, _data_ptr, ICMP6_ECHO_REQUEST_LENGTH);
    else
        set_internal_data_size(_sender, _data_ptr, ICMP_ECHO_REQUEST_LENGTH);
    return _sender.open_socket(ipv6);
}

bool Pinger::set_ping_count(size_t n)
{
    _ping_count = n;
    return true;
}

bool Pinger::set_client(const IpAddr & client)
{
    _client = client;
    return true;
}

bool Pinger::set_ttl(int ttl)
{
    if (ttl < 0)
        return false;
    _ttl = ttl;
    return true;
}

bool Pinger::set_timeout(time_t milliseconds_interval)
{
    return _sender.set_poll_timeout(milliseconds_interval);
}

bool Pinger::set_interval(time_t milliseconds_interval)
{
    if (milliseconds_interval < 0)
        return false;
    _ping_ms_interval = milliseconds_interval;
    return true;
}

bool Pinger::on_stop()
{
    _sender.close();
    auto l = _waitable.guard();
    _stop = true;
    _waitable.notify();
    return true;
}

bool Pinger::on_start()
{
    if (_sender.socket_opened() == false)
    {
        SIHD_LOG_ERROR("Pinger: socket not opened");
        return false;
    }

    this->service_set_ready();

    // setup ping
    this->_clear_event();
    _result.clear();

    _expected_id = sihd::sys::os::pid() & 0xFFFF;
    _sender.set_id(_expected_id);
    _sender.set_ttl(_ttl);
    _sender.set_echo();

    _result.time_start = _clock_ptr->now();

    _stop = false;
    bool ret = true;
    size_t i = 0;
    while (_stop == false && (_ping_count == 0 || i < _ping_count))
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
        ret = _sender.send_to(_client);
        if (ret == false)
        {
            SIHD_LOG_ERROR("Pinger: failed sending to client {}", _client.hostname());
            break;
        }
        _result.transmitted += 1;
        this->_notify_send();
        SIHD_LOG(debug, "Pinger: sent ICMP echo request seq={} to {}", _current_seq, _client.str());

        // wait until seq number is received by polling
        _received_icmp_response = false;
        while (_stop == false && _received_icmp_response == false && _sender.poll())
            ;
        if (_received_icmp_response == false)
        {
            SIHD_LOG(debug,
                     "Pinger: timeout waiting for ICMP echo reply seq={} from {}",
                     _current_seq,
                     _client.str());
            this->_notify_timeout();
        }
        ++i;
    }
    _result.time_end = _clock_ptr->now();
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

    if (_current_seq != response.seq)
        return;

    // For IPv4 SOCK_RAW: also verify ID matches our PID (truncated to 16 bits)
    // For IPv6 SOCK_DGRAM: kernel assigns its own ID
    if (sender->socket().is_ipv6() && response.seq == 1)
    {
        // First response, store expected ID
        _expected_id = response.id;
    }

    if (_expected_id != response.id)
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
