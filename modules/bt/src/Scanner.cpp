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
           && lhs.uuids == rhs.uuids;
}

} // namespace

Scanner::Scanner(const std::string & name, Node *parent): Named(name, parent), _stop_requested(false)
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

bool Scanner::on_start()
{
    const bool scanned = this->_scan();
    // reset once the loop ended: a stop() landing before the loop started is not lost
    _stop_requested = false;
    return scanned;
}

bool Scanner::on_stop()
{
    {
        auto lock = _waitable.guard();
        _stop_requested = true;
        if (_wake_scan)
            _wake_scan();
    }
    _waitable.notify_all();
    return true;
}

void Scanner::_publish(ScanResult && result)
{
    auto it = _results.find(result.address);
    if (it != _results.end() && same_advertisement(it->second, result))
        return;

    result.timestamp = Clock::default_clock.now();
    _last_address = result.address;
    _results.insert_or_assign(result.address, std::move(result));
    this->notify_observers(this);
}

std::optional<Duration> Scanner::_next_wait(Duration period) const
{
    if (!_deadline.has_value())
        return period;

    const Timestamp now = _clock.now();
    if (now >= *_deadline)
        return std::nullopt;
    return std::min(period, Duration(*_deadline - now));
}

} // namespace sihd::bt
