#ifndef __SIHD_UTIL_TOKENBUCKET_HPP__
#define __SIHD_UTIL_TOKENBUCKET_HPP__

#include <sihd/util/Clocks.hpp>
#include <sihd/util/Duration.hpp>
#include <sihd/util/time.hpp>

namespace sihd::util
{

// no timer thread: tokens are refilled lazily on every call
class TokenBucket
{
    public:
        TokenBucket(double capacity, double refill_tokens, Duration refill_interval);

        void set_clock(IClock *clock);

        bool try_consume(double tokens = 1.0);
        double tokens() const;
        // time until tokens are available, 0 if they already are - tokens must not exceed capacity
        Duration time_for(double tokens) const;

        void reset();

    private:
        Timestamp _now() const;
        double _refilled(Timestamp now) const;

        SteadyClock _steady_clock;
        IClock *_clock_ptr;
        double _capacity;
        double _tokens;
        double _refill_tokens;
        time::UnixTime _refill_interval;
        Timestamp _last;
};

} // namespace sihd::util

#endif
