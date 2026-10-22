#include "deadline.hpp"

#include <sihd/util/Clocks.hpp>
#include <sihd/util/time.hpp>

using namespace sihd::util;

namespace sihd::sys::internal
{

Timestamp steady_now()
{
    SteadyClock clock;
    return clock.now();
}

Timestamp deadline_from_now(Duration duration)
{
    return steady_now() + duration;
}

int poll_timeout_ms(Timestamp deadline)
{
    const time::UnixTime left = deadline - steady_now();
    if (left <= 0)
        return 0;
    const int milliseconds = static_cast<int>(time::to_milli(left));
    // 1 ms floor: poll(0) would spin.
    return milliseconds > 0 ? milliseconds : 1;
}

std::optional<timespec> realtime_deadline(Timestamp begin, Duration timeout)
{
    const Duration remaining = timeout - (steady_now() - begin);
    if (remaining <= 0)
        return std::nullopt;
    // mq_*/sem_* take an absolute CLOCK_REALTIME deadline
    return time::to_ts(SystemClock().now().get() + remaining.get());
}

} // namespace sihd::sys::internal
