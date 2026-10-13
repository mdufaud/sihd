#include "deadline.hpp"

#include <sihd/util/Clocks.hpp>
#include <sihd/util/time.hpp>

namespace sihd::sys::internal
{

sihd::util::Timestamp steady_now()
{
    sihd::util::SteadyClock clock;
    return clock.now();
}

sihd::util::Timestamp deadline_from_now(sihd::util::Duration duration)
{
    return steady_now() + duration;
}

int poll_timeout_ms(sihd::util::Timestamp deadline)
{
    const sihd::util::time::UnixTime left = deadline - steady_now();
    if (left <= 0)
        return 0;
    const int milliseconds = static_cast<int>(sihd::util::time::to_milli(left));
    // 1 ms floor: poll(0) would spin.
    return milliseconds > 0 ? milliseconds : 1;
}

std::optional<timespec> realtime_deadline(sihd::util::Timestamp begin, sihd::util::Duration timeout)
{
    const sihd::util::Duration remaining = timeout - (steady_now() - begin);
    if (remaining <= 0)
        return std::nullopt;
    // mq_*/sem_* take an absolute CLOCK_REALTIME deadline
    return sihd::util::time::to_ts(sihd::util::SystemClock().now().get() + remaining.get());
}

} // namespace sihd::sys::internal
