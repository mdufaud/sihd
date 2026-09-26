#ifndef __SIHD_UTIL_RATELIMITER_HPP__
#define __SIHD_UTIL_RATELIMITER_HPP__

#include <sihd/util/Duration.hpp>
#include <sihd/util/TokenBucket.hpp>

namespace sihd::util
{

class RateLimiter
{
    public:
        RateLimiter(size_t max_events, Duration window);

        void set_clock(IClock *clock);

        bool allow();
        // time until the next event is allowed, 0 if it already is
        Duration retry_after() const;
        // true when the limiter is back to its full budget, ie nothing was consumed since the last refill
        bool full() const;

        void reset();

    private:
        TokenBucket _bucket;
        size_t _max_events;
};

} // namespace sihd::util

#endif
