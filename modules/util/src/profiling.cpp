#include <stdexcept>

#include <sihd/util/Logger.hpp>
#include <sihd/util/Timestamp.hpp>
#include <sihd/util/profiling.hpp>

namespace sihd::util
{

SIHD_LOGGER;

Timeit::Timeit(std::source_location loc): _label(loc.function_name()), _begin(_clock.now()) {}

Timeit::Timeit(std::string_view label, std::source_location /*loc*/): _label(label), _begin(_clock.now()) {}

Timeit::~Timeit()
{
    constexpr bool show_total_parenthesis = false;

    const Duration duration = std::max(Duration(0), _clock.now() - _begin);
    const bool show_nano = duration < std::chrono::microseconds(1);

    SIHD_LOG(debug,
             "Time<{}>: {}",
             _label,
             duration.str(show_total_parenthesis, show_nano));
}

Duration Timeit::elapsed() const
{
    return std::max(Duration(0), _clock.now() - _begin);
}

Perf::Perf(std::source_location loc): _label(loc.function_name()), _begin(0) {}

Perf::Perf(std::string_view label, std::source_location /*loc*/): _label(label), _begin(0) {}

Perf::~Perf() = default;

void Perf::begin()
{
    _begin = _clock.now();
}

void Perf::end()
{
    if (_begin == 0)
        throw std::runtime_error("Cannot call end() before calling begin()");

    const Duration diff = std::max(Duration(0), _clock.now() - _begin);

    _stat.add_sample(diff.nanoseconds());
}

void Perf::log() const
{
    constexpr bool show_total = false;

    const time::UnixTime average = _stat.average();
    const time::UnixTime variance = _stat.variance();
    const time::UnixTime standard_deviation = _stat.standard_deviation();

    const bool show_min_nano = _stat.min < time::micro(1);
    const bool show_max_nano = _stat.max < time::micro(1);
    const bool show_average_nano = average < time::micro(1);
    const bool show_variance_nano = variance < time::micro(1);
    const bool show_standard_deviation_nano = standard_deviation < time::micro(1);

    SIHD_LOG_DEBUG("Perf<{}>: [samples: {}] [min: {}] [max: {}] [avg/var/dev: {} / {} / {}]",
                   _label,
                   _stat.samples,
                   Duration(_stat.min).str(show_total, show_min_nano),
                   Duration(_stat.max).str(show_total, show_max_nano),
                   Duration(average).str(show_total, show_average_nano),
                   Duration(variance).str(show_total, show_variance_nano),
                   Duration(standard_deviation).str(show_total, show_standard_deviation_nano));
}

} // namespace sihd::util
