#include <sihd/util/Profiling.hpp>
#include <sihd/util/Logger.hpp>

namespace sihd::util
{

SIHD_LOGGER;

Timeit::Timeit(std::string_view fun_name)
{
    _begin = _clock.now();
    _fun_name = fun_name;
}

Timeit::~Timeit()
{
    time_t now = _clock.now();
    SIHD_LOG_DEBUG("Timeit[%s]: time %s", _fun_name.c_str(), Timestamp(now - _begin).timeoffset_str().c_str());
}

}