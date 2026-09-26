#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

#include <sihd/util/TokenBucket.hpp>

namespace sihd::util
{

TokenBucket::TokenBucket(double capacity, double refill_tokens, Duration refill_interval):
    _clock_ptr(nullptr),
    _capacity(capacity),
    _tokens(capacity),
    _refill_tokens(refill_tokens),
    _refill_interval(refill_interval.get()),
    _last(this->_now())
{
    if (capacity <= 0)
        throw std::invalid_argument("TokenBucket capacity must be positive");
    if (refill_tokens <= 0)
        throw std::invalid_argument("TokenBucket refill tokens must be positive");
    if (_refill_interval <= 0)
        throw std::invalid_argument("TokenBucket refill interval must be positive");
}

void TokenBucket::set_clock(IClock *clock)
{
    _clock_ptr = clock;
    // _last belongs to the previous clock's timeline
    _last = this->_now();
}

Timestamp TokenBucket::_now() const
{
    return _clock_ptr != nullptr ? _clock_ptr->now() : _steady_clock.now();
}

double TokenBucket::_refilled(Timestamp now) const
{
    const time::UnixTime elapsed = now.get() - _last.get();
    if (elapsed <= 0)
        return _tokens;
    const double refilled = _tokens + double(elapsed) * _refill_tokens / double(_refill_interval);
    return std::min(refilled, _capacity);
}

bool TokenBucket::try_consume(double tokens)
{
    if (tokens < 0 || !std::isfinite(tokens))
        return false;
    const Timestamp now = this->_now();
    _tokens = this->_refilled(now);
    _last = now;
    if (_tokens < tokens)
        return false;
    _tokens -= tokens;
    return true;
}

double TokenBucket::tokens() const
{
    return this->_refilled(this->_now());
}

Duration TokenBucket::time_for(double tokens) const
{
    if (!std::isfinite(tokens))
        return Duration(std::numeric_limits<time::UnixTime>::max());
    const double current = this->_refilled(this->_now());
    if (current >= tokens)
        return Duration(0);
    const double nano = (tokens - current) * double(_refill_interval) / _refill_tokens;
    if (nano >= double(std::numeric_limits<time::UnixTime>::max()))
        return Duration(std::numeric_limits<time::UnixTime>::max());
    return Duration(time::UnixTime(std::ceil(nano)));
}

void TokenBucket::reset()
{
    _tokens = _capacity;
    _last = this->_now();
}

} // namespace sihd::util
