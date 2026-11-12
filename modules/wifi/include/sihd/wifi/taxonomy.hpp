#ifndef __SIHD_WIFI_TAXONOMY_HPP__
#define __SIHD_WIFI_TAXONOMY_HPP__

#include <array>
#include <optional>
#include <string>
#include <string_view>

#include <sihd/util/ArrayView.hpp>
#include <sihd/util/Result.hpp>
#include <sihd/wifi/frame.hpp>

namespace sihd::wifi::taxonomy
{

namespace detail
{

struct OuiVendor
{
        Oui oui;
        std::string_view name;
};

// microsoft, the wi-fi alliance and the ieee are left out: every device sends their elements
inline constexpr std::array<OuiVendor, 6> vendor_ouis = {{
    {{0x00, 0x17, 0xf2}, "Apple"},
    {{0x00, 0x10, 0x18}, "Broadcom"},
    {{0x00, 0x03, 0x7f}, "Atheros"},
    {{0x00, 0x0c, 0xe7}, "MediaTek"},
    {{0x00, 0xe0, 0x4c}, "Realtek"},
    {{0x00, 0x40, 0x96}, "Cisco"},
}};

} // namespace detail

constexpr std::optional<std::string_view> vendor_from_oui(const Oui & oui)
{
    for (const detail::OuiVendor & known : detail::vendor_ouis)
    {
        if (known.oui == oui)
            return known.name;
    }
    return std::nullopt;
}

// the first maker known by vendor_from_oui among the vendor elements: it outlives a randomized mac address
std::optional<std::string_view> vendor(sihd::util::ArrUByteView information_elements);

// one half of a signature, from the elements of a single frame; a damaged element ends the list
std::string element_signature(sihd::util::ArrUByteView information_elements);

// the whole `wifi4|probe:...|assoc:...` line; invalid_argument when one half yields nothing
sihd::util::Result<std::string> signature(sihd::util::ArrUByteView probe_request_elements,
                                          sihd::util::ArrUByteView assoc_request_elements);

} // namespace sihd::wifi::taxonomy

#endif
