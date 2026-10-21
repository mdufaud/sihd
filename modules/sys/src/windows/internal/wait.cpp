#include "wait.hpp"

using sihd::util::Duration;

namespace sihd::sys::internal
{

DWORD deadline_ms(Duration remaining)
{
    if (remaining.get() < 0)
        return INFINITE;
    if (remaining.get() <= 0)
        return 0;
    const int64_t milliseconds = (remaining.get() + 999999) / 1000000;
    return milliseconds >= static_cast<int64_t>(INFINITE) ? INFINITE - 1 : static_cast<DWORD>(milliseconds);
}

} // namespace sihd::sys::internal
