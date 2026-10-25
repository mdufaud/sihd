#include <algorithm>
#include <optional>
#include <string_view>

#include <fmt/format.h>

#include <sihd/util/endian.hpp>
#include <sihd/util/str.hpp>
#include <sihd/wifi/ies.hpp>
#include <sihd/wifi/taxonomy.hpp>

using namespace sihd::util;

namespace sihd::wifi::taxonomy
{

namespace
{

constexpr size_t wps_model_name_size = 32;
// hostapd's MAX_EXTCAP: a longer element still gives the same signature
constexpr size_t extcap_size_max = 254;
constexpr size_t power_capability_size = 2;

constexpr size_t ht_info_size = 2;
constexpr size_t ht_ampdu_offset = 2;
constexpr size_t ht_mcs_offset = 3;
constexpr size_t ht_mcs_end = 7;
constexpr size_t vht_info_size = 4;
constexpr size_t vht_rx_mcs_offset = 4;
constexpr size_t vht_rx_mcs_end = 8;
constexpr size_t vht_tx_mcs_offset = 8;
constexpr size_t vht_tx_mcs_end = 12;
constexpr uint32_t ht_mcs_low_16_bits_mask = 0xffff;

struct Details
{
        std::optional<std::string> htcap;
        std::optional<std::string> htagg;
        std::optional<std::string> htmcs;
        std::optional<std::string> vhtcap;
        std::optional<std::string> vhtrxmcs;
        std::optional<std::string> vhttxmcs;
        std::optional<std::string> txpow;
        std::optional<std::string> extcap;
        std::optional<std::string> wps;
};

// letters and digits stay, the rest becomes `_` so the name cannot break the line
std::string clean(ArrUByteView text)
{
    std::string result;
    for (const uint8_t byte : text)
    {
        const bool alphanumeric = (byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z')
                                  || (byte >= '0' && byte <= '9');
        result.push_back(alphanumeric ? static_cast<char>(byte) : '_');
    }
    return result;
}

std::string wps_model_name(const ies::Vendor & vendor)
{
    for (const ies::WpsAttribute & attribute : ies::wps_attributes(vendor.data))
    {
        if (attribute.id == ies::wps_model_name_attribute)
            return clean(ArrUByteView(attribute.value.data(), std::min(attribute.value.size(), wps_model_name_size)));
    }
    return {};
}

void read_vendor_details(const ies::Vendor & vendor, Details & details)
{
    if (vendor.oui != ies::microsoft_oui || vendor.type != ies::microsoft_wps_type)
        return;
    const std::string name = wps_model_name(vendor);
    if (name.empty() == false)
        details.wps = name;
}

void read_details(const ies::Element & element, Details & details)
{
    const uint8_t *data = element.data.data();
    const size_t size = element.data.size();
    switch (element.id)
    {
        case ies::Id::ht_capabilities:
            if (size >= ht_info_size)
                details.htcap = fmt::format("{:04x}", endian::read_le<uint16_t>(data, 2));
            if (size > ht_ampdu_offset)
                details.htagg = fmt::format("{:02x}", data[ht_ampdu_offset]);
            if (size >= ht_mcs_end)
            {
                const uint32_t mcs = endian::read_le<uint32_t>(data + ht_mcs_offset, 4);
                details.htmcs = fmt::format("{:08x}", mcs & ht_mcs_low_16_bits_mask);
            }
            break;
        case ies::Id::vht_capabilities:
            if (size >= vht_info_size)
                details.vhtcap = fmt::format("{:08x}", endian::read_le<uint32_t>(data, 4));
            if (size >= vht_rx_mcs_end)
                details.vhtrxmcs = fmt::format("{:08x}", endian::read_le<uint32_t>(data + vht_rx_mcs_offset, 4));
            if (size >= vht_tx_mcs_end)
                details.vhttxmcs = fmt::format("{:08x}", endian::read_le<uint32_t>(data + vht_tx_mcs_offset, 4));
            break;
        case ies::Id::extended_capabilities:
            details.extcap = str::hexdump(data, std::min(size, extcap_size_max), '\0');
            break;
        case ies::Id::power_capability:
            if (size == power_capability_size)
                details.txpow = fmt::format("{:04x}", endian::read_le<uint16_t>(data, 2));
            break;
        default:
            break;
    }
}

void append_detail(std::string & text, std::string_view name, const std::optional<std::string> & value)
{
    if (value.has_value())
        text += fmt::format(",{}:{}", name, *value);
}

} // namespace

std::string element_signature(ArrUByteView information_elements)
{
    std::string text;
    Details details;
    for (const ies::Element & element : ies::readable_elements(information_elements))
    {
        if (text.empty() == false)
            text += ',';
        if (const std::optional<ies::Vendor> vendor = ies::vendor_of(element))
        {
            read_vendor_details(*vendor, details);
            text += fmt::format("{}({:02x}{:02x}{:02x},{})",
                                static_cast<unsigned>(element.id),
                                vendor->oui[0],
                                vendor->oui[1],
                                vendor->oui[2],
                                vendor->type);
            continue;
        }
        read_details(element, details);
        text += fmt::format("{}", static_cast<unsigned>(element.id));
    }
    if (text.empty())
        return text;

    append_detail(text, "htcap", details.htcap);
    append_detail(text, "htagg", details.htagg);
    append_detail(text, "htmcs", details.htmcs);
    append_detail(text, "vhtcap", details.vhtcap);
    append_detail(text, "vhtrxmcs", details.vhtrxmcs);
    append_detail(text, "vhttxmcs", details.vhttxmcs);
    append_detail(text, "txpow", details.txpow);
    append_detail(text, "extcap", details.extcap);
    append_detail(text, "wps", details.wps);
    return text;
}

Result<std::string> signature(ArrUByteView probe_request_elements, ArrUByteView assoc_request_elements)
{
    const std::string probe = element_signature(probe_request_elements);
    const std::string assoc = element_signature(assoc_request_elements);
    if (probe.empty() || assoc.empty())
        return std::unexpected(
            Error(ErrorCode::invalid_argument,
                  "a signature needs the elements of a probe request and of an association request"));
    return fmt::format("wifi4|probe:{}|assoc:{}", probe, assoc);
}

} // namespace sihd::wifi::taxonomy
