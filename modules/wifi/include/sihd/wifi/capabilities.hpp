#ifndef __SIHD_WIFI_CAPABILITIES_HPP__
#define __SIHD_WIFI_CAPABILITIES_HPP__

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <sihd/util/Result.hpp>
#include <sihd/wifi/ies.hpp>

namespace sihd::wifi::ies
{

struct HtCapabilities
{
        uint16_t info = 0;
        bool ldpc = false;
        bool width_40mhz = false;
        // raw: 0 static, 1 dynamic, 3 disabled
        uint8_t spatial_multiplexing_power_save = 0;
        bool greenfield = false;
        bool short_gi_20mhz = false;
        bool short_gi_40mhz = false;
        bool tx_stbc = false;
        uint8_t rx_stbc_streams = 0;
        bool max_amsdu_7935 = false;
        bool dsss_cck_40mhz = false;
        bool forty_mhz_intolerant = false;
        // the longest A-MPDU is 2^(13 + exponent) - 1 bytes
        uint8_t ampdu_length_exponent = 0;
        // raw 0 to 7, see docs/protocols/wifi-frames.md
        uint8_t ampdu_min_spacing = 0;
        // 1 to 4, 0 when no group is set
        uint8_t rx_spatial_streams = 0;
        // 0 when the device does not say
        uint16_t rx_highest_rate_mbps = 0;
};

struct VhtCapabilities
{
        uint32_t info = 0;
        // 0 for the reserved value
        uint16_t max_mpdu_length = 0;
        bool width_160mhz = false;
        bool width_80plus80mhz = false;
        bool rx_ldpc = false;
        bool short_gi_80mhz = false;
        bool short_gi_160mhz = false;
        bool tx_stbc = false;
        uint8_t rx_stbc_streams = 0;
        bool su_beamformer = false;
        bool su_beamformee = false;
        bool mu_beamformer = false;
        bool mu_beamformee = false;
        // the longest A-MPDU is 2^(13 + exponent) - 1 bytes
        uint8_t ampdu_length_exponent = 0;
        uint16_t rx_mcs_map = 0;
        uint16_t tx_mcs_map = 0;
        // 0 to 8: the highest stream whose field in the map is not "not supported"
        uint8_t rx_spatial_streams = 0;
        uint8_t tx_spatial_streams = 0;
};

struct Standards
{
        bool ht = false;
        bool vht = false;
        bool he = false;
        bool eht = false;
};

struct Country
{
        struct Channels
        {
                uint8_t first = 0;
                uint8_t count = 0;
                int8_t max_power_dbm = 0;
        };

        std::string code;
        // ' ' indoor and outdoor, 'O' outdoor only, 'I' indoor only, 'X' a non-country entity
        char environment = ' ';
        // triplets that announce an operating class instead are left out
        std::vector<Channels> channels;
};

struct Tim
{
        uint8_t dtim_count = 0;
        uint8_t dtim_period = 0;
        bool multicast_buffered = false;
};

// empty when the frame lacks the element, invalid_argument when it is shorter than the fields read
sihd::util::Result<std::optional<HtCapabilities>> ht_capabilities(const std::vector<Element> & elements);
sihd::util::Result<std::optional<VhtCapabilities>> vht_capabilities(const std::vector<Element> & elements);
sihd::util::Result<std::optional<Country>> country(const std::vector<Element> & elements);
sihd::util::Result<std::optional<Tim>> tim(const std::vector<Element> & elements);
Standards standards(const std::vector<Element> & elements);

} // namespace sihd::wifi::ies

#endif
