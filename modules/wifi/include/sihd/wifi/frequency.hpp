#ifndef __SIHD_WIFI_FREQUENCY_HPP__
#define __SIHD_WIFI_FREQUENCY_HPP__

#include <cstdint>
#include <optional>

namespace sihd::wifi
{

// channel number of a center frequency in the 2.4, 5 or 6 GHz band, empty outside them
std::optional<uint8_t> channel_from_frequency(uint32_t frequency_mhz);

} // namespace sihd::wifi

#endif
