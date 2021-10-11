#include <sihd/util/Waitable.hpp>
#include <sihd/util/Logger.hpp>

namespace sihd::util
{

LOGGER;

using namespace std::chrono;

Waitable::Waitable()
{
    _stop_waiting = false;
}

Waitable::~Waitable()
{
    _stop_waiting = true;
    this->notify_all();
}

void    Waitable::notify_all()
{
    _condition.notify_all();
}

void    Waitable::notify(int times)
{
    int i = 0;
    while (i < times)
    {
        _condition.notify_one();
        ++i;
    }
}

void    Waitable::infinite_wait()
{
    std::unique_lock lock(_mutex);
    _condition.wait(lock);
}

bool    Waitable::wait(std::time_t nano_timestamp)
{
    std::unique_lock lock(_mutex);
    return _condition.wait_until(lock, system_clock::from_time_t(nano_timestamp)) == std::cv_status::timeout;
}

bool    Waitable::wait_for(std::time_t nano_duration)
{
    std::unique_lock lock(_mutex);
    return _condition.wait_for(lock, std::chrono::nanoseconds(nano_duration)) == std::cv_status::timeout;
}

bool    Waitable::wait_loop(std::time_t nano_duration, uint32_t times)
{
    SteadyClock clock;
    std::time_t now = clock.now();
    std::time_t last = now;
    std::time_t until = now + nano_duration;
    bool timedout = false;

    uint32_t i = 0;
    while (i < times && now < until && _stop_waiting == false)
    {
        timedout = this->wait_for(nano_duration);
        now = clock.now();
        nano_duration -= (now - last);
        last = now;
        ++i;
    }
    return timedout;
}

std::time_t Waitable::wait_elapsed(std::time_t nano_duration)
{
    SteadyClock clock;
    std::time_t now = clock.now();
    this->wait_for(nano_duration);
    return clock.now() - now;
}

}