#ifndef __SIHD_UTIL_PROFILING_HPP__
#define __SIHD_UTIL_PROFILING_HPP__

#include <string>

#include <sihd/util/Clocks.hpp>
#include <sihd/util/Location.hpp>

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
        struct Stat
        {
                size_t samples = 0;
                time_t min = 0;
                time_t sum = 0;
                time_t square = 0;
                time_t max = 0;

                time_t average() const;
                time_t variance() const;
                time_t standard_deviation() const;
        };

    public:
        Perf(const char *fun_name);
        ~Perf();

        void begin();
        void end();

        void log() const;

        Stat stat() const { return _stat; };

    protected:

    private:
        SteadyClock _clock;
        const char *_fun_name;
        time_t _begin;
        Stat _stat;
};

} // namespace sihd::util

#endif
