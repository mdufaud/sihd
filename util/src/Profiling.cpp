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
    SIHD_LOG_DEBUG("Timeit[{}]: time {}", _fun_name, Timestamp(now - _begin).timeoffset_str());
}

}