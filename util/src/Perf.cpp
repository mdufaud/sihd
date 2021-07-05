#include <sihd/util/Perf.hpp>
#include <sihd/util/Logger.hpp>

namespace sihd::util
{

LOGGER;

Perf::Perf(const std::string & fun_name)
{
    _begin = _clock.now();
    _fun_name = fun_name;
}

Perf::~Perf()
{
    std::time_t now = _clock.now();
    LOG_DEBUG("Perf[%s]: time %lu micro", _fun_name.c_str(), time::to_micro(now - _begin));
}

}