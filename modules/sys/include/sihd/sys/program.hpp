#ifndef __SIHD_SYS_PROGRAM_HPP__
#define __SIHD_SYS_PROGRAM_HPP__

#include <string>

#include <sihd/util/Result.hpp>

namespace sihd::sys::program
{

std::string path();

sihd::util::Result<void *> find_symbol(const std::string & symbol_name);

} // namespace sihd::sys::program

#endif
