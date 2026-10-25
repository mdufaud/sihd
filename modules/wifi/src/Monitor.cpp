#include <sihd/util/time.hpp>
#include <sihd/wifi/Monitor.hpp>

using namespace sihd::util;

namespace sihd::wifi
{

namespace
{

const Duration blocking_slice(time::hours(1));

} // namespace

Monitor::Monitor(Duration run_timeout): Monitor()
{
    this->set_run_timeout(run_timeout);
}

void Monitor::set_run_timeout(Duration timeout)
{
    _run_timeout = timeout;
}

const std::vector<Monitor::Event> & Monitor::events() const
{
    return _events;
}

bool Monitor::run()
{
    _events.clear();
    const bool blocking = _run_timeout < Duration(0);
    Duration wait = blocking ? blocking_slice : _run_timeout;

    while (true)
    {
        auto received = this->_receive(wait);
        if (!received.has_value())
            return false;
        if (received->has_value())
        {
            for (const Event & event : **received)
                this->_publish(event);
            wait = Duration(0);
            continue;
        }
        if (!blocking || !_events.empty())
            break;
        wait = blocking_slice;
    }

    if (!_events.empty())
        this->notify_observers(this);
    return true;
}

void Monitor::_publish(const Event & event)
{
    if (event.type == Event::Type::state_changed)
    {
        if (_last_state == event.state)
            return;
        _last_state = event.state;
    }
    _events.emplace_back(event);
}

} // namespace sihd::wifi
