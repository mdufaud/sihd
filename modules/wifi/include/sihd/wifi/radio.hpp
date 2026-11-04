#ifndef __SIHD_WIFI_RADIO_HPP__
#define __SIHD_WIFI_RADIO_HPP__

#include <cstdint>
#include <string>

#include <sihd/util/Result.hpp>

namespace sihd::wifi
{

struct Radio
{
        enum class Mode
        {
            unknown,
            station,
            access_point,
            ad_hoc,
            mesh,
            monitor,
        };

        Mode mode = Mode::unknown;
        // 0 when unknown, windows only reports the channel
        uint32_t frequency_mhz = 0;
        // 0 when unknown
        uint8_t channel = 0;
};

// what an interface is set to, read only: not_found unless a wifi interface
sihd::util::Result<Radio> radio(const std::string & interface_name);
std::string mode_str(Radio::Mode mode);

} // namespace sihd::wifi

#endif
