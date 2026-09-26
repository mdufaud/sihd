#include <sihd/util/RateLimiter.hpp>

namespace sihd::util
{

RateLimiter::RateLimiter(size_t max_events, Duration window):
    _bucket(double(max_events), double(max_events), window),
    _max_events(max_events)
{
}

void RateLimiter::set_clock(IClock *clock)
{
    _bucket.set_clock(clock);
}

bool RateLimiter::allow()
{
    return _bucket.try_consume();
}

Duration RateLimiter::retry_after() const
{
    return _bucket.time_for(1.0);
}

bool RateLimiter::full() const
{
    // the lazy refill clamps at capacity: the comparison is exact, a fractional
    // refill reads below capacity until the clamp engages
    return _bucket.tokens() >= double(_max_events);
}

void RateLimiter::reset()
{
    _bucket.reset();
}

} // namespace sihd::util
