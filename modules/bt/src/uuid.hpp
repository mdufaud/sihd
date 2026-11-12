#ifndef __SIHD_BT_UUID_HPP__
#define __SIHD_BT_UUID_HPP__

#include <cstdint>
#include <optional>
#include <string_view>

namespace sihd::bt
{

// 0000xxxx-0000-1000-8000-00805f9b34fb, the bluetooth base uuid holding a 16 bits uuid, either case
std::optional<uint16_t> short_from_uuid(std::string_view uuid);

} // namespace sihd::bt

#endif
