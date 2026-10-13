#ifndef __SIHD_UTIL_RESULT_HPP__
#define __SIHD_UTIL_RESULT_HPP__

#include <expected>

#include <sihd/util/Error.hpp>

namespace sihd::util
{

template <typename T>
using Result = std::expected<T, Error>;

} // namespace sihd::util

#endif
