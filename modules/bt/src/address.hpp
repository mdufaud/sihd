#ifndef __SIHD_BT_ADDRESS_HPP__
#define __SIHD_BT_ADDRESS_HPP__

#include <cstdint>
#include <string>
#include <string_view>

#include <sihd/util/Result.hpp>

namespace sihd::bt
{

// "AA:BB:CC:DD:EE:FF" holds the 48 bits most significant byte first
sihd::util::Result<uint64_t> parse_address(std::string_view text);
std::string format_address(uint64_t address);

} // namespace sihd::bt

#endif
