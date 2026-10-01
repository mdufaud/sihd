#ifndef __SIHD_SSH_UTILS_HPP__
#define __SIHD_SSH_UTILS_HPP__

#include <expected>

#include <sihd/util/Error.hpp>

namespace sihd::ssh::utils
{

std::expected<void, sihd::util::Error> init();
bool is_initialized();
std::expected<void, sihd::util::Error> finalize();

} // namespace sihd::ssh::utils

#endif
