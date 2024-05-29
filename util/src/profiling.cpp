#include <sihd/util/Logger.hpp>
#include <sihd/util/Timestamp.hpp>
#include <sihd/util/profiling.hpp>

namespace sihd::util
{

SIHD_LOGGER;

Timeit::Timeit(const char *fun_name): _fun_name(fun_name), _begin(_clock.now()) {}

Timeit::~Timeit()
{
    constexpr bool show_total_parenthesis = false;

    const time_t duration = std::max(time_t {0}, _clock.now() - _begin);
    const bool show_nano = duration < time::micro(1);

    SIHD_LOG(debug, "Time<{}>: {}", _fun_name, Timestamp(duration).timeoffset_str(show_total_parenthesis, show_nano));
}

Perf::Perf(const char *fun_name): _fun_name(fun_name), _begin(0) {}

Perf::~Perf() {}

void Perf::begin()
{
    _begin = _clock.now();
}

void Perf::end()
{
    if (_begin == 0)
        throw std::runtime_error("Cannot call end() before calling begin()");

    const time_t diff = std::max(time_t {0}, _clock.now() - _begin);

    _stat.add_sample(diff);
}

void Perf::log() const
{
    constexpr bool show_total = false;

    const time_t average = _stat.average();
    const time_t variance = _stat.variance();
    const time_t standard_deviation = _stat.standard_deviation();

    const bool show_min_nano = _stat.min < time::micro(1);
    const bool show_max_nano = _stat.max < time::micro(1);
    const bool show_average_nano = average < time::micro(1);
    const bool show_variance_nano = variance < time::micro(1);
    const bool show_standard_deviation_nano = standard_deviation < time::micro(1);

    SIHD_LOG_DEBUG("Perf<{}>: [samples: {}] [min: {}] [max: {}] [avg/var/dev: {} / {} / {}]",
                   _fun_name,
                   _stat.samples,
                   Timestamp(_stat.min).timeoffset_str(show_total, show_min_nano),
                   Timestamp(_stat.max).timeoffset_str(show_total, show_max_nano),
                   Timestamp(average).timeoffset_str(show_total, show_average_nano),
                   Timestamp(variance).timeoffset_str(show_total, show_variance_nano),
                   Timestamp(standard_deviation).timeoffset_str(show_total, show_standard_deviation_nano));
}

} // namespace sihd::util
