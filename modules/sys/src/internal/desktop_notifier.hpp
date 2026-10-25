#ifndef __SIHD_SYS_INTERNAL_DESKTOP_NOTIFIER_HPP__
#define __SIHD_SYS_INTERNAL_DESKTOP_NOTIFIER_HPP__

#include <optional>

#include <sihd/util/Duration.hpp>
#include <sihd/util/Result.hpp>

namespace sihd::sys::internal
{

sihd::util::Result<void> check_timeout(const std::optional<sihd::util::Duration> & timeout);

} // namespace sihd::sys::internal

#endif
