#ifndef __SIHD_WIFI_DISSECT_HPP__
#define __SIHD_WIFI_DISSECT_HPP__

#include <optional>

#include <sihd/util/ArrayView.hpp>
#include <sihd/util/Result.hpp>
#include <sihd/wifi/frame.hpp>
#include <sihd/wifi/radiotap.hpp>

namespace sihd::wifi
{

struct Packet
{
        // absent for dissect_80211()
        std::optional<Radiotap> radiotap;
        Frame frame;
};

// datalink 127 (DLT_IEEE802_11_RADIO): the fcs is dropped and the body skips the padding radiotap announces
sihd::util::Result<Packet> dissect_radiotap(sihd::util::ArrUByteView bytes);
// datalink 105 (DLT_IEEE802_11): the 802.11 frame alone
sihd::util::Result<Packet> dissect_80211(sihd::util::ArrUByteView bytes);

} // namespace sihd::wifi

#endif
