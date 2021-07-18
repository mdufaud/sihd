#include <sihd/util/Profiling.hpp>
#include <sihd/util/Logger.hpp>

namespace sihd::util
{

LOGGER;

Timeit::Timeit(const std::string & fun_name)
{
    _begin = _clock.now();
    _fun_name = fun_name;
}

Timeit::~Timeit()
{
    std::time_t now = _clock.now();
    LOG_DEBUG("Timeit[%s]: time %lu micro", _fun_name.c_str(), time::to_micro(now - _begin));
}

}