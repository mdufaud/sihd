#ifndef __SIHD_CRYPTO_ERROR_HPP__
#define __SIHD_CRYPTO_ERROR_HPP__

#include <expected>
#include <string_view>

#include <sihd/util/Error.hpp>

namespace sihd::crypto
{

// drains the OpenSSL error queue: the latest entry is the failing call's error
std::unexpected<sihd::util::Error> make_error(std::string_view context);

} // namespace sihd::crypto

#endif
