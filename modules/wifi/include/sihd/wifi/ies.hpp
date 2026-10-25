#ifndef __SIHD_WIFI_IES_HPP__
#define __SIHD_WIFI_IES_HPP__

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <sihd/util/ArrayView.hpp>
#include <sihd/util/Result.hpp>
#include <sihd/wifi/frame.hpp>

namespace sihd::wifi::ies
{

inline constexpr Oui ieee_oui = {0x00, 0x0f, 0xac};
inline constexpr Oui microsoft_oui = {0x00, 0x50, 0xf2};

inline constexpr uint8_t microsoft_wpa_type = 1;
inline constexpr uint8_t microsoft_wps_type = 4;

enum class Id : uint8_t
{
    ssid = 0,
    rates = 1,
    ds_parameter = 3,
    tim = 5,
    country = 7,
    power_capability = 33,
    rsn = 48,
    ht_capabilities = 45,
    extended_rates = 50,
    ht_operation = 61,
    extended_capabilities = 127,
    vht_capabilities = 191,
    vht_operation = 192,
    vendor = 221,
    extension = 255
};

struct Element
{
        Id id = Id::ssid;
        // view into the buffer the elements were walked from
        sihd::util::ArrUByteView data;
};

struct Vendor
{
        Oui oui = {};
        uint8_t type = 0;
        sihd::util::ArrUByteView data;
};

struct Rate
{
        uint32_t kbps = 0;
        bool basic = false;
};

struct Suite
{
        Oui oui = {};
        uint8_t type = 0;

        bool operator==(const Suite &) const = default;
};

struct Rsn
{
        uint16_t version = 0;
        Suite group;
        std::vector<Suite> pairwise;
        std::vector<Suite> akms;
        std::optional<uint16_t> capabilities;
};

struct WpsAttribute
{
        uint16_t id = 0;
        sihd::util::ArrUByteView value;
};

inline constexpr uint16_t wps_model_name_attribute = 0x1023;

struct Wps
{
        std::optional<uint8_t> version;
        // 1 not configured, 2 configured
        std::optional<uint8_t> setup_state;
        bool ap_setup_locked = false;
        std::string manufacturer;
        std::string model_name;
        std::string model_number;
        std::string device_name;
};

// invalid_argument when an element runs past the end
sihd::util::Result<std::vector<Element>> elements(sihd::util::ArrUByteView information_elements);
// the complete elements up to the first one cut short, for a capture cut by its snaplen
std::vector<Element> readable_elements(sihd::util::ArrUByteView information_elements);
// the first element of this id
std::optional<Element> find(const std::vector<Element> & elements, Id id);
// empty unless the element is a vendor specific one (id 221) with room for an OUI and a type
std::optional<Vendor> vendor_of(const Element & element);
std::optional<Vendor> find_vendor(const std::vector<Element> & elements, const Oui & oui, uint8_t type);
std::vector<Vendor> vendors(const std::vector<Element> & elements);

std::string ssid(const std::vector<Element> & elements);
std::optional<uint8_t> channel(const std::vector<Element> & elements);
// bss membership selectors left out
std::vector<Rate> rates(const std::vector<Element> & elements);

sihd::util::Result<Rsn> parse_rsn(sihd::util::ArrUByteView body);
// empty when there is no rsn element, invalid_argument when it is truncated
sihd::util::Result<std::optional<Rsn>> rsn(const std::vector<Element> & elements);
// the wpa vendor element holds an rsn body
sihd::util::Result<std::optional<Rsn>> wpa(const std::vector<Element> & elements);
// the attributes of the wps vendor element, after its oui and type: invalid_argument when one runs past the end
sihd::util::Result<Wps> parse_wps(sihd::util::ArrUByteView attributes);
// the attributes before the first one cut short, uninterpreted
std::vector<WpsAttribute> wps_attributes(sihd::util::ArrUByteView attributes);
sihd::util::Result<std::optional<Wps>> wps(const std::vector<Element> & elements);

std::string cipher_str(const Suite & suite);
std::string akm_str(const Suite & suite);

} // namespace sihd::wifi::ies

#endif
