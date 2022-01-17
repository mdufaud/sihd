#ifndef __SIHD_UTIL_PROFILING_HPP__
# define __SIHD_UTIL_PROFILING_HPP__

# include <sihd/util/Clocks.hpp>
# include <sihd/util/time.hpp>
# include <string>

namespace sihd::util
{

class Timeit
{
    public:
        Timeit(const std::string & fun_name = "");
        virtual ~Timeit();

    protected:

    private:
        std::string _fun_name;
        std::time_t _begin;
        SteadyClock _clock;
};

}

#endif