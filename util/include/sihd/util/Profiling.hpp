#ifndef __SIHD_UTIL_PROFILING_HPP__
# define __SIHD_UTIL_PROFILING_HPP__

# include <string>

# include <sihd/util/Clocks.hpp>

namespace sihd::util
{

class Timeit
{
    public:
        Timeit(std::string && fun_name);
        virtual ~Timeit();

    protected:

    private:
        std::string _fun_name;
        time_t _begin;
        SteadyClock _clock;
};

}

#endif
