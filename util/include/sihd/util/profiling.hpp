#ifndef __SIHD_UTIL_PROFILING_HPP__
#define __SIHD_UTIL_PROFILING_HPP__

#include <string>

#include <sihd/util/Clocks.hpp>
#include <sihd/util/Location.hpp>
#include <sihd/util/Stat.hpp>

namespace sihd::util
{

class Timeit
{
    public:
        Timeit(const char *fun_name);
        ~Timeit();

    protected:

    private:
        SteadyClock _clock;
        const char *_fun_name;
        time_t _begin;
};

class Perf
{
    public:
        Perf(const char *fun_name);
        ~Perf();

        void begin();
        void end();

        void log() const;

        const Stat<time_t> & stat() const { return _stat; };

    protected:

    private:
        SteadyClock _clock;
        const char *_fun_name;
        time_t _begin;
        Stat<time_t> _stat;
};

} // namespace sihd::util

#endif
