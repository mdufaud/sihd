#include "ies.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <vector>

#include <sihd/util/endian.hpp>
#include <sihd/wifi/scan.hpp>

using namespace sihd::util;

namespace sihd::wifi
{

namespace
{

constexpr uint8_t element_ssid = 0;
constexpr uint8_t element_rsn = 48;
constexpr uint8_t element_vendor = 221;
constexpr size_t element_header_size = 2;
constexpr size_t suite_size = 4;
constexpr size_t count_size = 2;
constexpr size_t version_and_group_cipher_size = 2 + suite_size;

using Oui = std::array<uint8_t, 3>;

constexpr Oui ieee_oui = {0x00, 0x0f, 0xac};
constexpr std::array<uint8_t, 9> ieee_psk_and_sae_akms = {2, 4, 6, 8, 9, 19, 20, 24, 25};
constexpr std::array<uint8_t, 12> ieee_8021x_akms = {1, 3, 5, 11, 12, 13, 14, 15, 16, 17, 22, 23};
constexpr uint8_t ieee_owe_akm = 18;

constexpr Oui microsoft_oui = {0x00, 0x50, 0xf2};
constexpr uint8_t microsoft_wpa_type = 1;
constexpr uint8_t microsoft_8021x_akm = 1;
constexpr uint8_t microsoft_psk_akm = 2;

struct Element
{
        uint8_t id = 0;
        ArrUByteView data;
};

struct Akms
{
        bool personal = false;
        bool enterprise = false;
        bool owe = false;
};

// nullopt when an element runs past the end
std::optional<std::vector<Element>> elements_of(ArrUByteView ies)
{
    std::vector<Element> elements;
    size_t offset = 0;
    while (offset < ies.size())
    {
        if (offset + element_header_size > ies.size())
            return std::nullopt;
        const uint8_t id = ies[offset];
        const size_t length = ies[offset + 1];
        offset += element_header_size;
        if (offset + length > ies.size())
            return std::nullopt;
        elements.push_back({.id = id, .data = ArrUByteView(ies.data() + offset, length)});
        offset += length;
    }
    return elements;
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

// the akm suites of a body starting at its version: nullopt when truncated
std::optional<Security> suites_security(ArrUByteView body, const Oui & oui)
{
    size_t offset = version_and_group_cipher_size;
    if (offset + count_size > body.size())
        return std::nullopt;
    const size_t pairwise_count = endian::read_le<uint16_t>(body.data() + offset, count_size);
    offset += count_size + pairwise_count * suite_size;
    if (offset + count_size > body.size())
        return std::nullopt;
    const size_t akm_count = endian::read_le<uint16_t>(body.data() + offset, count_size);
    offset += count_size;
    if (offset + akm_count * suite_size > body.size())
        return std::nullopt;

    Akms akms;
    for (size_t index = 0; index < akm_count; ++index)
    {
        const uint8_t *suite = body.data() + offset + index * suite_size;
        if (!std::equal(oui.begin(), oui.end(), suite))
            continue;
        if (oui == ieee_oui)
            classify_ieee_akm(suite[oui.size()], akms);
        else
            classify_microsoft_akm(suite[oui.size()], akms);
    }
    if (akms.personal)
        return Security::personal;
    if (akms.enterprise)
        return Security::enterprise;
    if (akms.owe)
        return Security::owe;
    return Security::unknown;
}

bool is_wpa_element(const Element & element)
{
    return element.id == element_vendor && element.data.size() > microsoft_oui.size()
           && std::equal(microsoft_oui.begin(), microsoft_oui.end(), element.data.data())
           && element.data[microsoft_oui.size()] == microsoft_wpa_type;
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
    const std::optional<std::vector<Element>> elements = elements_of(information_elements);
    if (!elements.has_value())
        return Security::unknown;

    std::optional<Security> rsn;
    std::optional<Security> wpa;
    for (const Element & element : *elements)
    {
        if (element.id == element_rsn)
            rsn = suites_security(element.data, ieee_oui).value_or(Security::unknown);
        else if (is_wpa_element(element))
        {
            const size_t header = microsoft_oui.size() + 1;
            const ArrUByteView body(element.data.data() + header, element.data.size() - header);
            wpa = suites_security(body, microsoft_oui).value_or(Security::unknown);
        }
    }
    if (rsn.has_value())
        return *rsn;
    if (wpa.has_value())
        return *wpa;
    return privacy ? Security::wep : Security::open;
}

std::string ies::ssid(ArrUByteView information_elements)
{
    const std::optional<std::vector<Element>> elements = elements_of(information_elements);
    if (!elements.has_value())
        return {};
    const auto found = std::ranges::find(*elements, element_ssid, &Element::id);
    if (found == elements->end())
        return {};
    return std::string(found->data.begin(), found->data.end());
}

} // namespace sihd::wifi
