#ifndef __SIHD_SYS_PROGRAM_HPP__
#define __SIHD_SYS_PROGRAM_HPP__

#include <expected>
#include <string>
#include <string_view>

#include <sihd/util/Error.hpp>

namespace sihd::sys::program
{

std::string path();

std::expected<void *, sihd::util::Error> find_symbol(std::string_view symbol_name);

} // namespace sihd::sys::program

#endif
