#include <algorithm>

#include <sihd/bt/Scanner.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>

namespace sihd::bt
{

using namespace sihd::util;

SIHD_NEW_LOGGER("sihd::bt");

SIHD_REGISTER_FACTORY(Scanner)

namespace
{

bool same_advertisement(const ScanResult & lhs, const ScanResult & rhs)
{
    return lhs.identifier == rhs.identifier && lhs.address_type == rhs.address_type && lhs.rssi == rhs.rssi
           && lhs.tx_power == rhs.tx_power && lhs.connectable == rhs.connectable
           && lhs.manufacturer_data == rhs.manufacturer_data && lhs.service_data == rhs.service_data
           && lhs.uuids == rhs.uuids && lhs.appearance == rhs.appearance && lhs.class_of_device == rhs.class_of_device;
}

} // namespace

Scanner::Scanner(const std::string & name, Node *parent): Named(name, parent)
{
    this->set_adapter(0);
    this->add_conf("adapter", &Scanner::set_adapter);
}

Scanner::~Scanner()
{
    if (this->is_running())
        this->stop();
}

bool Scanner::set_adapter(size_t index)
{
    _adapter_index = index;
    return true;
}

bool Scanner::set_filter(const ScanFilter & filter)
{
    if (this->is_running())
        return false;
    _filter = filter;
    return true;
}

bool Scanner::set_lost_timeout(Duration timeout)
{
    if (this->is_running() || timeout < Duration(0))
        return false;
    _lost_timeout = timeout;
    return true;
}

bool Scanner::scan_for(Duration duration)
{
    _deadline = _clock.now() + duration;
    const bool scanned = this->start();
    _deadline.reset();
    return scanned;
}

const std::map<std::string, ScanResult> & Scanner::results() const
{
    return _results;
}

std::optional<ScanResult> Scanner::last_result() const
{
    if (auto it = _results.find(_last_address); it != _results.end())
        return it->second;
    return std::nullopt;
}

Scanner::Event Scanner::last_event() const
{
    return _last_event;
}

bool Scanner::on_start()
{
    return this->_scan();
}

bool Scanner::on_stop()
{
    {
        // the service flag is already set: waking under the guard cannot slip between a check and a wait
        auto lock = _waitable.guard();
        if (_wake_scan)
            _wake_scan();
    }
    _waitable.notify_all();
    return true;
}

void Scanner::_publish(ScanResult && result)
{
    _heard.insert_or_assign(result.address, _clock.now());
    auto it = _results.find(result.address);
    if (it != _results.end() && same_advertisement(it->second, result))
        return;

    _last_event = it == _results.end() ? Event::found : Event::updated;
    result.timestamp = Clock::default_clock.now();
    _last_address = result.address;
    _results.insert_or_assign(result.address, std::move(result));
    this->notify_observers(this);
}

void Scanner::_forget_lost()
{
    if (_lost_timeout == Duration(0))
        return;
    const Timestamp now = _clock.now();
    for (auto it = _heard.begin(); it != _heard.end();)
    {
        if (now - it->second < _lost_timeout)
        {
            ++it;
            continue;
        }
        _last_address = it->first;
        _last_event = Event::lost;
        this->notify_observers(this);
        _results.erase(it->first);
        it = _heard.erase(it);
    }
}

std::optional<Duration> Scanner::_next_wait(Duration period) const
{
    const Timestamp now = _clock.now();
    Duration wait = period;
    if (_deadline.has_value())
    {
        if (now >= *_deadline)
            return std::nullopt;
        wait = std::min(wait, Duration(*_deadline - now));
    }
    if (_lost_timeout > Duration(0))
    {
        // a negative wait would block forever
        for (const auto & [address, heard] : _heard)
            wait = std::clamp(Duration(heard + _lost_timeout - now), Duration(0), wait);
    }
    return wait;
}

} // namespace sihd::bt
