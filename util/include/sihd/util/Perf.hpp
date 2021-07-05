#ifndef __SIHD_UTIL_PERF_HPP__
# define __SIHD_UTIL_PERF_HPP__

# include <sihd/util/Clocks.hpp>
# include <sihd/util/time.hpp>
# include <string>

namespace sihd::util
{

class Perf
{
    public:
        Perf(const std::string & fun_name = "");
        virtual ~Perf();

    protected:
    
    private:
        std::string _fun_name;
        std::time_t _begin;
        SteadyClock _clock;
};

}

#endif 