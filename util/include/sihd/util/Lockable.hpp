#ifndef __SIHD_UTIL_LOCKABLE_HPP__
#define __SIHD_UTIL_LOCKABLE_HPP__

#include <mutex>

namespace sihd::util
{

class Lockable
{
    public:
        Lockable();
        virtual ~Lockable();

        bool infinite_lock();
        bool lock();
        bool lock_for(time_t nano_duration);
        bool lock_until(time_t nano_timestamp);
        time_t lock_elapsed(time_t nano_duration);
        bool unlock();

        std::timed_mutex & mutex() { return _mutex; }
        std::lock_guard<std::timed_mutex> lock_guard() { return std::lock_guard(_mutex); }

    protected:

    private:
        std::timed_mutex _mutex;
        std::unique_lock<std::timed_mutex> _unique_lock;
};

} // namespace sihd::util

#endif