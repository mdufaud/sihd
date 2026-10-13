#ifndef __SIHD_SYS_PROGRAM_HPP__
#define __SIHD_SYS_PROGRAM_HPP__

#include <string>
#include <string_view>

#include <sihd/util/Result.hpp>

namespace sihd::sys::program
{

std::string path();

sihd::util::Result<void *> find_symbol(std::string_view symbol_name);

} // namespace sihd::sys::program

#endif
