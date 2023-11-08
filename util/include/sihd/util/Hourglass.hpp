#ifndef __SIHD_UTIL_HOURGLASS_HPP__
#define __SIHD_UTIL_HOURGLASS_HPP__

#include <sihd/util/Clocks.hpp>
#include <sihd/util/Timestamp.hpp>

namespace sihd::util
{

class Hourglass
{
    public:
        Hourglass() { this->reset(); }
        virtual ~Hourglass() {}

        void reset() { _before = _clock.now(); }

        Timestamp mesure() { return _clock.now() - _before; }

    protected:

    private:
        SteadyClock _clock;
        time_t _before;
};

} // namespace sihd::util

#endif
