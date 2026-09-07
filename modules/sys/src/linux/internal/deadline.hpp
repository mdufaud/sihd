#ifndef __SIHD_SYS_LINUX_DEADLINE_HPP__
#define __SIHD_SYS_LINUX_DEADLINE_HPP__

#include <sihd/util/Duration.hpp>
#include <sihd/util/Timestamp.hpp>

namespace sihd::sys::internal
{

sihd::util::Timestamp steady_now();

sihd::util::Timestamp deadline_from_now(sihd::util::Duration duration);

int poll_timeout_ms(sihd::util::Timestamp deadline);

} // namespace sihd::sys::internal

#endif
