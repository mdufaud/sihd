#ifndef __SIHD_UTIL_PROFILING_HPP__
#define __SIHD_UTIL_PROFILING_HPP__

#include <source_location>
#include <string>
#include <string_view>

#include <sihd/util/Clocks.hpp>
#include <sihd/util/Stat.hpp>
#include <sihd/util/Timestamp.hpp>

namespace sihd::util
{

class Timeit
{
    public:
        Timeit(std::source_location loc = std::source_location::current());
        Timeit(std::string_view label, std::source_location loc = std::source_location::current());
        ~Timeit();

        Timestamp elapsed() const;

    protected:

    private:
        SteadyClock _clock;
        std::string _label;
        time_t _begin;
};

class Perf
{
    public:
        class Guard
        {
            public:
                Guard(Perf & perf): _perf(perf) { _perf.begin(); }
                ~Guard() noexcept
                {
                    try
                    {
                        _perf.end();
                    }
                    catch (...)
                    {
                    }
                }
                Guard(const Guard &) = delete;
                Guard & operator=(const Guard &) = delete;

            private:
                Perf & _perf;
        };

        Perf(std::source_location loc = std::source_location::current());
        Perf(std::string_view label, std::source_location loc = std::source_location::current());
        ~Perf();

        void begin();
        void end();

        void log() const;

        const Stat<time_t> & stat() const { return _stat; };

    protected:

    private:
        SteadyClock _clock;
        std::string _label;
        time_t _begin;
        Stat<time_t> _stat;
};

} // namespace sihd::util

#endif
