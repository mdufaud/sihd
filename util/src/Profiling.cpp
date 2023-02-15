#include <sihd/util/Profiling.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Timestamp.hpp>

namespace sihd::util
{

SIHD_LOGGER;

Timeit::Timeit(std::string && fun_name)
{
    _begin = _clock.now();
    _fun_name = std::move(fun_name);
}

Timeit::~Timeit()
{
    time_t now = _clock.now();
    SIHD_LOG_DEBUG("Timeit[{}]: time {}", _fun_name, Timestamp(now - _begin).timeoffset_str());
}

}
