#ifndef __SIHD_UTIL_STOPWATCH_HPP__
#define __SIHD_UTIL_STOPWATCH_HPP__

#include <sihd/util/Clocks.hpp>
#include <sihd/util/Timestamp.hpp>

namespace sihd::util
{

class Stopwatch
{
    public:
        Stopwatch() { this->reset(); }
        ~Stopwatch() = default;

        Timestamp time() const { return _clock.now() - _before; }

        void reset() { _before = _clock.now(); }

    protected:

    private:
        SteadyClock _clock;
        time_t _before;
};

} // namespace sihd::util

#endif
