#ifndef __SIHD_SSH_UTILS_HPP__
#define __SIHD_SSH_UTILS_HPP__

#include <sihd/util/Result.hpp>

namespace sihd::ssh::utils
{

sihd::util::Result<void> init();
bool is_initialized();
sihd::util::Result<void> finalize();

} // namespace sihd::ssh::utils

#endif
