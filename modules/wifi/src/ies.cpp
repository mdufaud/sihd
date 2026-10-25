#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <ranges>
#include <string_view>
#include <vector>

#include <sihd/util/endian.hpp>
#include <sihd/wifi/ies.hpp>
#include <sihd/wifi/scan.hpp>

using namespace sihd::util;

namespace sihd::wifi
{

namespace
{

constexpr size_t suite_size = 4;
constexpr size_t count_size = 2;
constexpr size_t version_size = 2;
constexpr size_t rsn_capabilities_size = 2;
constexpr size_t vendor_header_size = 4;
constexpr size_t element_field_size = 1;
constexpr size_t wps_field_size = 2;

constexpr uint8_t rate_basic_bit = 0x80;
constexpr uint8_t rate_value_mask = 0x7f;
// BSS membership selectors take the values 120 (UHR PHY) to 127 (HT PHY) with the basic bit set
constexpr uint8_t first_membership_selector = 120;
constexpr uint32_t rate_unit_kbps = 500;

constexpr uint16_t wps_version = 0x104a;
constexpr uint16_t wps_setup_state = 0x1044;
constexpr uint16_t wps_ap_setup_locked = 0x1057;
constexpr uint16_t wps_manufacturer = 0x1021;
constexpr uint16_t wps_model_number = 0x1024;
constexpr uint16_t wps_device_name = 0x1011;

constexpr std::array<uint8_t, 9> ieee_psk_and_sae_akms = {2, 4, 6, 8, 9, 19, 20, 24, 25};
constexpr std::array<uint8_t, 12> ieee_8021x_akms = {1, 3, 5, 11, 12, 13, 14, 15, 16, 17, 22, 23};
constexpr uint8_t ieee_owe_akm = 18;

constexpr uint8_t microsoft_8021x_akm = 1;
constexpr uint8_t microsoft_psk_akm = 2;

struct Akms
{
        bool personal = false;
        bool enterprise = false;
        bool owe = false;
};

template <typename Item>
struct Walk
{
        std::vector<Item> items;
        std::optional<Error> cut;
};

// a list of (id, length, data) records whose id and length take `field_size` big endian bytes each
template <typename Item>
Walk<Item> walk_tlv(ArrUByteView bytes, size_t field_size, std::string_view name)
{
    const size_t header_size = 2 * field_size;
    Walk<Item> walk;
    size_t offset = 0;
    while (offset < bytes.size())
    {
        if (offset + header_size > bytes.size())
        {
            walk.cut = Error(ErrorCode::invalid_argument, "{} header runs past the end", name);
            break;
        }

        const uint16_t id = endian::read_be<uint16_t>(bytes.data() + offset, field_size);
        const size_t length = endian::read_be<uint16_t>(bytes.data() + offset + field_size, field_size);
        offset += header_size;

        if (offset + length > bytes.size())
        {
            walk.cut = Error(ErrorCode::invalid_argument, "{} {} runs past the end", name, id);
            break;
        }

        walk.items.push_back({static_cast<decltype(Item::id)>(id), ArrUByteView(bytes.data() + offset, length)});
        offset += length;
    }
    return walk;
}

void classify_ieee_akm(uint8_t type, Akms & akms)
{
    if (std::ranges::find(ieee_psk_and_sae_akms, type) != ieee_psk_and_sae_akms.end())
        akms.personal = true;
    else if (std::ranges::find(ieee_8021x_akms, type) != ieee_8021x_akms.end())
        akms.enterprise = true;
    else if (type == ieee_owe_akm)
        akms.owe = true;
}

void classify_microsoft_akm(uint8_t type, Akms & akms)
{
    if (type == microsoft_8021x_akm)
        akms.enterprise = true;
    else if (type == microsoft_psk_akm)
        akms.personal = true;
}

Security akm_security(const ies::Rsn & rsn, const Oui & oui)
{
    Akms akms;
    for (const ies::Suite & suite : rsn.akms)
    {
        if (suite.oui != oui)
            continue;
        if (oui == ies::ieee_oui)
            classify_ieee_akm(suite.type, akms);
        else
            classify_microsoft_akm(suite.type, akms);
    }
    if (akms.personal)
        return Security::personal;
    if (akms.enterprise)
        return Security::enterprise;
    if (akms.owe)
        return Security::owe;
    return Security::unknown;
}

template <typename T, typename Found>
Result<std::optional<T>> parse_data(const std::optional<Found> & found, Result<T> (*parser)(ArrUByteView))
{
    if (found.has_value() == false)
        return std::nullopt;
    Result<T> parsed = parser(found->data);
    SIHD_UNEXPECTED_RETURN(parsed);
    return std::move(*parsed);
}

std::optional<ies::Element> find_last(const std::vector<ies::Element> & elements, ies::Id id)
{
    for (const ies::Element & element : elements | std::views::reverse)
    {
        if (element.id == id)
            return element;
    }
    return std::nullopt;
}

std::optional<ies::Vendor> find_last_vendor(const std::vector<ies::Element> & elements, const Oui & oui, uint8_t type)
{
    for (const ies::Element & element : elements | std::views::reverse)
    {
        const std::optional<ies::Vendor> vendor = ies::vendor_of(element);
        if (vendor.has_value() && vendor->oui == oui && vendor->type == type)
            return vendor;
    }
    return std::nullopt;
}

// nullopt when the elements carry none: a truncated one still decides, as unknown
std::optional<Security> element_security(const Result<std::optional<ies::Rsn>> & element, const Oui & oui)
{
    if (element.has_value() == false)
        return Security::unknown;
    if (element->has_value() == false)
        return std::nullopt;
    return akm_security(**element, oui);
}

ies::Suite suite_at(const uint8_t *data)
{
    ies::Suite suite;
    std::copy_n(data, suite.oui.size(), suite.oui.begin());
    suite.type = data[suite.oui.size()];
    return suite;
}

Result<std::vector<ies::Suite>> suites_at(ArrUByteView body, size_t & offset)
{
    if (offset + count_size > body.size())
        return std::unexpected(Error(ErrorCode::invalid_argument, "suite count runs past the element"));
    const size_t count = endian::read_le<uint16_t>(body.data() + offset, count_size);
    offset += count_size;
    if (offset + count * suite_size > body.size())
        return std::unexpected(Error(ErrorCode::invalid_argument, "{} suites run past the element", count));

    std::vector<ies::Suite> suites;
    for (size_t index = 0; index < count; ++index)
        suites.push_back(suite_at(body.data() + offset + index * suite_size));
    offset += count * suite_size;
    return suites;
}

std::string wps_text(ArrUByteView value)
{
    return std::string(value.begin(), value.end());
}

void store_wps_attribute(uint16_t type, ArrUByteView value, ies::Wps & wps)
{
    switch (type)
    {
        case wps_version:
            if (value.size() == 1)
                wps.version = value[0];
            break;
        case wps_setup_state:
            if (value.size() == 1)
                wps.setup_state = value[0];
            break;
        case wps_ap_setup_locked:
            wps.ap_setup_locked = value.size() == 1 && value[0] != 0;
            break;
        case wps_manufacturer:
            wps.manufacturer = wps_text(value);
            break;
        case ies::wps_model_name_attribute:
            wps.model_name = wps_text(value);
            break;
        case wps_model_number:
            wps.model_number = wps_text(value);
            break;
        case wps_device_name:
            wps.device_name = wps_text(value);
            break;
        default:
            break;
    }
}

void append_rates(const ies::Element & element, std::vector<ies::Rate> & rates)
{
    for (const uint8_t byte : element.data)
    {
        const uint8_t value = byte & rate_value_mask;
        const bool basic = (byte & rate_basic_bit) != 0;
        if (basic && value >= first_membership_selector)
            continue;
        rates.push_back({.kbps = value * rate_unit_kbps, .basic = basic});
    }
}

} // namespace

std::string security_str(Security security)
{
    switch (security)
    {
        case Security::unknown:
            return "unknown";
        case Security::open:
            return "open";
        case Security::owe:
            return "owe";
        case Security::wep:
            return "wep";
        case Security::personal:
            return "personal";
        case Security::enterprise:
            return "enterprise";
    }
    return "unknown";
}

Security security_of(ArrUByteView information_elements, bool privacy)
{
    const Result<std::vector<ies::Element>> parsed = ies::elements(information_elements);
    if (parsed.has_value() == false)
        return Security::unknown;

    // when an element is repeated the last one decides
    const auto rsn_element = parse_data(find_last(*parsed, ies::Id::rsn), &ies::parse_rsn);
    if (const std::optional<Security> rsn = element_security(rsn_element, ies::ieee_oui))
        return *rsn;
    const auto wpa_element = parse_data(find_last_vendor(*parsed, ies::microsoft_oui, ies::microsoft_wpa_type),
                                        &ies::parse_rsn);
    if (const std::optional<Security> wpa = element_security(wpa_element, ies::microsoft_oui))
        return *wpa;
    return privacy ? Security::wep : Security::open;
}

Result<std::vector<ies::Element>> ies::elements(ArrUByteView information_elements)
{
    Walk<Element> walk = walk_tlv<Element>(information_elements, element_field_size, "element");
    if (walk.cut.has_value())
        return std::unexpected(std::move(*walk.cut));
    return std::move(walk.items);
}

std::vector<ies::Element> ies::readable_elements(ArrUByteView information_elements)
{
    return walk_tlv<Element>(information_elements, element_field_size, "element").items;
}

std::optional<ies::Element> ies::find(const std::vector<Element> & elements, Id id)
{
    const auto found = std::ranges::find(elements, id, &Element::id);
    if (found == elements.end())
        return std::nullopt;
    return *found;
}

std::optional<ies::Vendor> ies::vendor_of(const Element & element)
{
    if (element.id != Id::vendor || element.data.size() < vendor_header_size)
        return std::nullopt;
    Vendor vendor;
    std::copy_n(element.data.data(), vendor.oui.size(), vendor.oui.begin());
    vendor.type = element.data[vendor.oui.size()];
    vendor.data = ArrUByteView(element.data.data() + vendor_header_size, element.data.size() - vendor_header_size);
    return vendor;
}

std::optional<ies::Vendor> ies::find_vendor(const std::vector<Element> & elements, const Oui & oui, uint8_t type)
{
    for (const Element & element : elements)
    {
        const std::optional<Vendor> vendor = vendor_of(element);
        if (vendor.has_value() && vendor->oui == oui && vendor->type == type)
            return vendor;
    }
    return std::nullopt;
}

std::vector<ies::Vendor> ies::vendors(const std::vector<Element> & elements)
{
    std::vector<Vendor> result;
    for (const Element & element : elements)
    {
        if (const std::optional<Vendor> vendor = vendor_of(element))
            result.push_back(*vendor);
    }
    return result;
}

std::string ies::ssid(const std::vector<Element> & elements)
{
    const std::optional<Element> element = find(elements, Id::ssid);
    return element.has_value() ? std::string(element->data.begin(), element->data.end()) : std::string();
}

std::optional<uint8_t> ies::channel(const std::vector<Element> & elements)
{
    for (const Id id : {Id::ds_parameter, Id::ht_operation})
    {
        const std::optional<Element> element = find(elements, id);
        if (element.has_value() && element->data.size() >= 1)
            return element->data[0];
    }
    return std::nullopt;
}

std::vector<ies::Rate> ies::rates(const std::vector<Element> & elements)
{
    std::vector<Rate> result;
    for (const Element & element : elements)
    {
        if (element.id == Id::rates || element.id == Id::extended_rates)
            append_rates(element, result);
    }
    return result;
}

Result<ies::Rsn> ies::parse_rsn(ArrUByteView body)
{
    if (body.size() < version_size + suite_size)
        return std::unexpected(Error(ErrorCode::invalid_argument, "rsn needs a version and a group cipher"));

    Rsn rsn;
    rsn.version = endian::read_le<uint16_t>(body.data(), version_size);
    rsn.group = suite_at(body.data() + version_size);
    size_t offset = version_size + suite_size;
    if (offset == body.size())
        return rsn;

    Result<std::vector<Suite>> pairwise = suites_at(body, offset);
    SIHD_UNEXPECTED_RETURN(pairwise);
    rsn.pairwise = std::move(*pairwise);
    if (offset == body.size())
        return rsn;

    Result<std::vector<Suite>> akms = suites_at(body, offset);
    SIHD_UNEXPECTED_RETURN(akms);
    rsn.akms = std::move(*akms);
    if (offset + rsn_capabilities_size <= body.size())
        rsn.capabilities = endian::read_le<uint16_t>(body.data() + offset, rsn_capabilities_size);
    return rsn;
}

Result<std::optional<ies::Rsn>> ies::rsn(const std::vector<Element> & elements)
{
    return parse_data(find(elements, Id::rsn), &parse_rsn);
}

Result<std::optional<ies::Rsn>> ies::wpa(const std::vector<Element> & elements)
{
    return parse_data(find_vendor(elements, microsoft_oui, microsoft_wpa_type), &parse_rsn);
}

Result<ies::Wps> ies::parse_wps(ArrUByteView attributes)
{
    Walk<WpsAttribute> walk = walk_tlv<WpsAttribute>(attributes, wps_field_size, "wps attribute");
    if (walk.cut.has_value())
        return std::unexpected(std::move(*walk.cut));

    Wps wps;
    for (const WpsAttribute & attribute : walk.items)
        store_wps_attribute(attribute.id, attribute.value, wps);
    return wps;
}

std::vector<ies::WpsAttribute> ies::wps_attributes(ArrUByteView attributes)
{
    return walk_tlv<WpsAttribute>(attributes, wps_field_size, "wps attribute").items;
}

Result<std::optional<ies::Wps>> ies::wps(const std::vector<Element> & elements)
{
    return parse_data(find_vendor(elements, microsoft_oui, microsoft_wps_type), &parse_wps);
}

std::string ies::cipher_str(const Suite & suite)
{
    if (suite.oui != ieee_oui && suite.oui != microsoft_oui)
        return "unknown";
    switch (suite.type)
    {
        case 1:
            return "wep40";
        case 2:
            return "tkip";
        case 4:
            return "ccmp";
        case 5:
            return "wep104";
        case 8:
            return suite.oui == ieee_oui ? "gcmp" : "unknown";
        case 9:
            return suite.oui == ieee_oui ? "gcmp256" : "unknown";
        case 10:
            return suite.oui == ieee_oui ? "ccmp256" : "unknown";
        default:
            return "unknown";
    }
}

std::string ies::akm_str(const Suite & suite)
{
    if (suite.oui == microsoft_oui)
    {
        if (suite.type == microsoft_8021x_akm)
            return "8021x";
        return suite.type == microsoft_psk_akm ? "psk" : "unknown";
    }
    if (suite.oui != ieee_oui)
        return "unknown";
    switch (suite.type)
    {
        case 1:
            return "8021x";
        case 2:
            return "psk";
        case 3:
            return "ft_8021x";
        case 4:
            return "ft_psk";
        case 5:
            return "8021x_sha256";
        case 6:
            return "psk_sha256";
        case 8:
            return "sae";
        case 9:
            return "ft_sae";
        case 18:
            return "owe";
        default:
            return "unknown";
    }
}

} // namespace sihd::wifi
