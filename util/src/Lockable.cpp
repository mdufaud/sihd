#include <sihd/util/Clocks.hpp>
#include <sihd/util/Lockable.hpp>
#include <sihd/util/Logger.hpp>

namespace sihd::util
{

SIHD_LOGGER;

using namespace std::chrono;

Lockable::Lockable(): _unique_lock(_mutex, std::defer_lock) {}

Lockable::~Lockable()
{
    this->unlock();
}

bool Lockable::infinite_lock()
{
    try
    {
        _unique_lock.lock();
    }
    catch (std::system_error & err)
    {
        (void)err;
        return false;
    }
    return true;
}

bool Lockable::lock()
{
    try
    {
        return _unique_lock.try_lock();
    }
    catch (std::system_error & err)
    {
        (void)err;
    }
    return false;
}

bool Lockable::lock_for(time_t nano_duration)
{
    return _unique_lock.try_lock_for(std::chrono::nanoseconds(nano_duration));
}

bool Lockable::lock_until(time_t nano_timestamp)
{
    return _unique_lock.try_lock_until(system_clock::from_time_t(nano_timestamp));
}

time_t Lockable::lock_elapsed(time_t nano_duration)
{
    SystemClock clock;
    time_t before = clock.now();
    this->lock_for(nano_duration);
    return clock.now() - before;
}

bool Lockable::unlock()
{
    try
    {
        _unique_lock.unlock();
    }
    catch (std::system_error & err)
    {
        (void)err;
        return false;
    }
    return true;
}

} // namespace sihd::util