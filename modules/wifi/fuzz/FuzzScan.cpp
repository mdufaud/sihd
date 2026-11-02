#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <fuzz_helper.hpp>
#include <vector>

#include <sihd/util/ArrayView.hpp>
#include <sihd/wifi/ies.hpp>
#include <sihd/wifi/scan.hpp>

#include "wifi_fuzz_helper.hpp"
#include <fuzzer/FuzzedDataProvider.h>

using namespace sihd::util;
using namespace sihd::wifi;
using test::Bytes;

namespace
{
// microsoft oui, wpa type
constexpr std::array<uint8_t, 4> wpa_header = {0x00, 0x50, 0xf2, 0x01};
constexpr uint8_t id_rsn = 48;
constexpr uint8_t id_vendor = 221;

// assigned akm selectors that tell nothing: 7 tdls, 10 apPeerKey, 21 pasn
constexpr std::array<uint8_t, 4> unclassified_akms = {0, 7, 10, 21};
constexpr std::array<uint8_t, 7> personal_akms = {2, 4, 6, 8, 9, 19, 20};
constexpr std::array<uint8_t, 10> enterprise_akms = {1, 3, 5, 11, 12, 13, 14, 15, 16, 17};
constexpr uint8_t owe_akm = 18;

struct Entry
{
        test::Element element;
        bool decides = false;
        bool is_rsn = false;
        Security security = Security::unknown;
};

bool announces_rsn_or_wpa(const std::vector<ies::Element> & elements)
{
    for (const ies::Element & element : elements)
    {
        if (element.id == ies::Id::rsn)
            return true;
        if (element.id == ies::Id::vendor && element.data.size() >= wpa_header.size()
            && std::equal(wpa_header.begin(), wpa_header.end(), element.data.begin()))
            return true;
    }
    return false;
}

void check_raw(const Bytes & content)
{
    const Bytes elements = test::exact_copy(content);
    const ArrUByteView view(elements);

    const Security without_privacy = security_of(view, false);
    const Security with_privacy = security_of(view, true);

    const Bytes copy = test::exact_copy(elements);
    test::fuzz_check(security_of(ArrUByteView(copy), false) == without_privacy, "security is deterministic");
    if (without_privacy == Security::open)
        test::fuzz_check(with_privacy == Security::wep, "the privacy bit turns open into wep");
    else
        test::fuzz_check(with_privacy == without_privacy, "the privacy bit only matters to an open network");

    const Result<std::vector<ies::Element>> parsed = ies::elements(view);
    if (!parsed.has_value())
    {
        test::fuzz_check(without_privacy == Security::unknown, "a damaged element leaves security unknown");
        return;
    }
    if (!announces_rsn_or_wpa(*parsed))
    {
        test::fuzz_check(without_privacy == Security::open, "no rsn nor wpa and no privacy is open");
        test::fuzz_check(with_privacy == Security::wep, "no rsn nor wpa but privacy is wep");
    }
}

uint8_t consume_akm_type(FuzzedDataProvider & provider)
{
    switch (provider.ConsumeIntegralInRange<int>(0, 4))
    {
        case 0:
            return unclassified_akms[provider.ConsumeIntegralInRange<size_t>(0, unclassified_akms.size() - 1)];
        case 1:
            return personal_akms[provider.ConsumeIntegralInRange<size_t>(0, personal_akms.size() - 1)];
        case 2:
            return enterprise_akms[provider.ConsumeIntegralInRange<size_t>(0, enterprise_akms.size() - 1)];
        case 3:
            return owe_akm;
        default:
            return provider.ConsumeIntegralInRange<uint8_t>(26, 255);
    }
}

std::vector<ies::Suite> consume_akms(FuzzedDataProvider & provider)
{
    std::vector<ies::Suite> akms(provider.ConsumeIntegralInRange<size_t>(0, 3));
    for (ies::Suite & akm : akms)
    {
        const int which = provider.ConsumeIntegralInRange<int>(0, 3);
        akm.oui = which == 0 ? ies::microsoft_oui : (which == 3 ? Oui {0x00, 0x0f, 0xad} : ies::ieee_oui);
        akm.type = consume_akm_type(provider);
    }
    return akms;
}

bool in_list(uint8_t type, const auto & list)
{
    return std::find(list.begin(), list.end(), type) != list.end();
}

struct Akms
{
        bool personal = false;
        bool enterprise = false;
        bool owe = false;
};

void note_akm(uint8_t type, bool microsoft, Akms & found)
{
    if (microsoft)
    {
        found.personal = found.personal || type == 2;
        found.enterprise = found.enterprise || type == 1;
        return;
    }
    found.personal = found.personal || in_list(type, personal_akms);
    found.enterprise = found.enterprise || in_list(type, enterprise_akms);
    found.owe = found.owe || type == owe_akm;
}

Security classify(const std::vector<ies::Suite> & akms, const Oui & oui)
{
    Akms found;
    for (const ies::Suite & akm : akms)
    {
        if (akm.oui == oui)
            note_akm(akm.type, oui == ies::microsoft_oui, found);
    }

    if (found.personal)
        return Security::personal;
    if (found.enterprise)
        return Security::enterprise;
    return found.owe ? Security::owe : Security::unknown;
}

Entry consume_security_entry(FuzzedDataProvider & provider, bool is_rsn)
{
    ies::Rsn rsn = test::consume_rsn(provider);
    rsn.akms = consume_akms(provider);
    const size_t parts = provider.ConsumeIntegralInRange<size_t>(0, 4);
    Bytes body = test::encode_rsn(rsn, parts);
    const bool damaged = parts <= 2 && provider.ConsumeIntegralInRange<int>(0, 4) == 0;
    if (damaged)
        body.pop_back();

    Entry entry;
    entry.decides = true;
    entry.is_rsn = is_rsn;
    entry.security = damaged ? Security::unknown : classify(rsn.akms, is_rsn ? ies::ieee_oui : ies::microsoft_oui);
    entry.element.id = is_rsn ? id_rsn : id_vendor;
    if (!is_rsn)
        entry.element.data.assign(wpa_header.begin(), wpa_header.end());
    test::put_bytes(entry.element.data, body);
    return entry;
}

Entry consume_noise_entry(FuzzedDataProvider & provider)
{
    Entry entry;
    entry.element.id = provider.ConsumeBool() ? id_vendor : provider.ConsumeIntegral<uint8_t>();
    if (entry.element.id == id_rsn)
        entry.element.id = 0;
    if (entry.element.id == id_vendor)
    {
        const Oui oui = provider.ConsumeBool() ? ies::microsoft_oui : ies::ieee_oui;
        entry.element.data.assign(oui.begin(), oui.end());
        entry.element.data.push_back(provider.ConsumeIntegralInRange<uint8_t>(2, 255));
    }
    test::put_bytes(entry.element.data, provider.ConsumeBytes<uint8_t>(provider.ConsumeIntegralInRange<size_t>(0, 8)));
    return entry;
}

void check_generated(FuzzedDataProvider & provider)
{
    std::vector<Entry> entries(provider.ConsumeIntegralInRange<size_t>(0, 6));
    for (Entry & entry : entries)
    {
        const int kind = provider.ConsumeIntegralInRange<int>(0, 3);
        entry = kind == 0 ? consume_noise_entry(provider) : consume_security_entry(provider, kind != 3);
    }

    std::vector<test::Element> elements;
    for (const Entry & entry : entries)
        elements.push_back(entry.element);
    Bytes bytes = test::encode_elements(elements);
    const bool damaged = provider.ConsumeIntegralInRange<int>(0, 5) == 0;
    if (damaged)
        test::put_bytes(bytes, provider.ConsumeBool() ? Bytes {0x00} : Bytes {0x00, 0x05, 0x01});
    const bool privacy = provider.ConsumeBool();

    // when an element is repeated the last one decides; an rsn element hides every wpa one
    Security expected = privacy ? Security::wep : Security::open;
    for (const bool rsn_wanted : {false, true})
    {
        for (const Entry & entry : entries)
        {
            if (entry.decides && entry.is_rsn == rsn_wanted)
                expected = entry.security;
        }
    }
    if (damaged)
        expected = Security::unknown;

    const Bytes input = test::exact_copy(bytes);
    test::fuzz_check(security_of(ArrUByteView(input), privacy) == expected,
                     "security follows the last rsn element, else the last wpa one, else the privacy bit");
}
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzedDataProvider provider(data, size);
    if (provider.ConsumeBool())
        check_generated(provider);
    else
        check_raw(provider.ConsumeRemainingBytes<uint8_t>());
    return 0;
}
