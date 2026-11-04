#include <sihd/wifi/frequency.hpp>

namespace sihd::wifi
{

namespace
{

constexpr uint32_t channel_spacing_mhz = 5;
// channel 14 is off the 5 MHz grid of the 2.4 GHz band
constexpr uint32_t channel_14_mhz = 2484;
constexpr uint32_t band_2ghz_first_mhz = 2412;
constexpr uint32_t band_2ghz_last_mhz = 2472;
constexpr uint32_t band_2ghz_base_mhz = 2407;
constexpr uint32_t band_5ghz_first_mhz = 5160;
constexpr uint32_t band_5ghz_last_mhz = 5885;
constexpr uint32_t band_5ghz_base_mhz = 5000;
constexpr uint32_t band_6ghz_first_mhz = 5955;
constexpr uint32_t band_6ghz_last_mhz = 7115;
constexpr uint32_t band_6ghz_base_mhz = 5950;

std::optional<uint8_t> channel_in_band(uint32_t frequency_mhz, uint32_t first, uint32_t last, uint32_t base)
{
    if (frequency_mhz < first || frequency_mhz > last || (frequency_mhz - base) % channel_spacing_mhz != 0)
        return std::nullopt;
    return static_cast<uint8_t>((frequency_mhz - base) / channel_spacing_mhz);
}

} // namespace

std::optional<uint8_t> channel_from_frequency(uint32_t frequency_mhz)
{
    if (frequency_mhz == channel_14_mhz)
        return 14;
    if (frequency_mhz < band_5ghz_first_mhz)
        return channel_in_band(frequency_mhz, band_2ghz_first_mhz, band_2ghz_last_mhz, band_2ghz_base_mhz);
    if (frequency_mhz < band_6ghz_first_mhz)
        return channel_in_band(frequency_mhz, band_5ghz_first_mhz, band_5ghz_last_mhz, band_5ghz_base_mhz);
    return channel_in_band(frequency_mhz, band_6ghz_first_mhz, band_6ghz_last_mhz, band_6ghz_base_mhz);
}

} // namespace sihd::wifi
