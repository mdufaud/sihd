#ifndef __SIHD_WIFI_DATA_HPP__
#define __SIHD_WIFI_DATA_HPP__

#include <cstdint>

#include <sihd/util/ArrayView.hpp>
#include <sihd/util/Result.hpp>
#include <sihd/wifi/frame.hpp>

namespace sihd::wifi
{

// the LLC/SNAP header of an unencrypted data body, then its payload
struct Llc
{
        Oui oui = {};
        uint16_t ethertype = 0;
        sihd::util::ArrUByteView payload;
};

// a protected frame is encrypted and holds no llc header; not_supported when the header is not snap
sihd::util::Result<Llc> parse_llc_snap(sihd::util::ArrUByteView body);

} // namespace sihd::wifi

#endif
