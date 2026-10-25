#ifndef __SIHD_WIFI_RADIOTAP_HPP__
#define __SIHD_WIFI_RADIOTAP_HPP__

#include <cstdint>
#include <optional>
#include <vector>

#include <sihd/util/ArrayView.hpp>
#include <sihd/util/Result.hpp>

namespace sihd::wifi
{

struct Radiotap
{
        struct Mcs
        {
                uint8_t index = 0;
                // 20 or 40, 0 when the card did not say
                uint16_t bandwidth_mhz = 0;
                bool short_gi = false;
                bool greenfield = false;
                bool ldpc = false;
                uint8_t stbc_streams = 0;
                // empty when the bandwidth or the guard interval is unknown, or the index is above 31
                std::optional<uint32_t> rate_kbps;
        };

        struct AntennaSignal
        {
                uint8_t antenna = 0;
                int8_t signal_dbm = 0;
        };

        // relative to the card, not wall-clock time
        std::optional<uint64_t> tsft_us;
        // absent when the rate is an index: see `mcs`
        std::optional<uint32_t> rate_kbps;
        std::optional<Mcs> mcs;
        std::optional<uint16_t> frequency_mhz;
        // raw bits as the driver reports them
        std::optional<uint16_t> channel_flags;
        std::optional<int8_t> signal_dbm;
        std::optional<int8_t> noise_dbm;
        std::optional<uint8_t> antenna;
        // one per extra radiotap namespace carrying an antenna index and a signal
        std::vector<AntennaSignal> antenna_signals;

        bool short_preamble = false;
        bool wep = false;
        bool fragmented = false;
        // `payload` drops the FCS: assumes the capture was not cut by its snaplen
        bool fcs_at_end = false;
        // dissect_radiotap() skips the padding between the 802.11 header and its body
        bool data_padding = false;
        bool bad_fcs = false;

        size_t header_size = 0;
        // view into the caller's buffer
        sihd::util::ArrUByteView payload;
};

// invalid_argument on a malformed header; a field the parser cannot size stops the walk and keeps what was read
sihd::util::Result<Radiotap> parse_radiotap(sihd::util::ArrUByteView bytes);

} // namespace sihd::wifi

#endif
