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

    const time_t duration = std::max(0L, _clock.now() - _begin);
    const bool show_nano = duration < time::micro(1);

    SIHD_LOG(debug, "Time<{}>: {}", _fun_name, Timestamp(duration).timeoffset_str(show_total_parenthesis, show_nano));
}

Perf::Perf(const char *fun_name): _fun_name(fun_name), _begin(0) {}

Perf::~Perf() {}

void Perf::begin()
{
    _stat.samples++;
    _begin = _clock.now();
}

void Perf::end()
{
    if (_stat.samples == 0)
        throw std::runtime_error("Cannot call end() before calling begin()");

    const time_t diff = std::max(0L, _clock.now() - _begin);

    if (_stat.samples == 1)
        _stat.min = diff;
    else
        _stat.min = std::min(_stat.min, diff);
    _stat.sum += diff;
    _stat.square += (diff * diff);
    _stat.max = std::max(_stat.max, diff);
}

void Perf::log() const
{
    constexpr bool show_total_parenthesis = false;

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
                   Timestamp(_stat.min).timeoffset_str(show_total_parenthesis, show_min_nano),
                   Timestamp(_stat.max).timeoffset_str(show_total_parenthesis, show_max_nano),
                   Timestamp(average).timeoffset_str(show_total_parenthesis, show_average_nano),
                   Timestamp(variance).timeoffset_str(show_total_parenthesis, show_variance_nano),
                   Timestamp(standard_deviation).timeoffset_str(show_total_parenthesis, show_standard_deviation_nano));
}

time_t Perf::Stat::average() const
{
    return sum / samples;
}

time_t Perf::Stat::variance() const
{
    const time_t average = this->average();
    return square / samples - (average * average);
}

time_t Perf::Stat::standard_deviation() const
{
    return sqrt(this->variance());
}

} // namespace sihd::util
