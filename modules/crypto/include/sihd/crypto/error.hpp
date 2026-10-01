#ifndef __SIHD_CRYPTO_ERROR_HPP__
#define __SIHD_CRYPTO_ERROR_HPP__

#include <expected>
#include <string_view>
#include <utility>

#include <fmt/core.h>

#include <sihd/util/Error.hpp>

namespace sihd::crypto
{

// drains the OpenSSL error queue: the latest entry is the failing call's error
std::unexpected<sihd::util::Error> make_error(std::string_view context);

template <typename... Args>
    requires(sizeof...(Args) != 0)
std::unexpected<sihd::util::Error> make_error(fmt::format_string<Args...> format, Args &&...args)
{
    return make_error(fmt::format(format, std::forward<Args>(args)...));
}

} // namespace sihd::crypto

#endif
