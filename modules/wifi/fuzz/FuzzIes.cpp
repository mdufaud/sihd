#include <array>
#include <cstddef>
#include <cstdint>
#include <fuzz_helper.hpp>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <sihd/util/ArrayView.hpp>
#include <sihd/wifi/capabilities.hpp>
#include <sihd/wifi/ies.hpp>

#include "wifi_fuzz_helper.hpp"
#include <fuzzer/FuzzedDataProvider.h>

using namespace sihd::util;
using namespace sihd::wifi;
using test::Bytes;

namespace
{
constexpr size_t header_size = 2;
constexpr size_t vendor_header_size = 4;
constexpr uint8_t id_ssid = 0;
constexpr uint8_t id_rates = 1;
constexpr uint8_t id_ds_parameter = 3;
constexpr uint8_t id_extended_rates = 50;
constexpr uint8_t id_ht_operation = 61;
constexpr uint8_t id_rsn = 48;
constexpr uint8_t id_vendor = 221;
constexpr std::array<uint8_t, 14> interesting_ids = {0, 1, 3, 5, 7, 33, 45, 48, 50, 61, 127, 191, 221, 255};

struct Item
{
        uint8_t id = 0;
        size_t offset = 0;
        size_t size = 0;
};

struct Walk
{
        std::vector<Item> items;
        bool complete = true;
};

Walk walk_records(const Bytes & bytes)
{
    Walk walk;
    size_t offset = 0;
    while (offset < bytes.size())
    {
        if (offset + header_size > bytes.size() || offset + header_size + bytes[offset + 1] > bytes.size())
        {
            walk.complete = false;
            break;
        }
        walk.items.push_back({bytes[offset], offset + header_size, bytes[offset + 1]});
        offset += header_size + bytes[offset + 1];
    }
    return walk;
}

const Item *first_item(const Walk & walk, uint8_t id)
{
    for (const Item & item : walk.items)
    {
        if (item.id == id)
            return &item;
    }
    return nullptr;
}

bool is_vendor(const Item & item)
{
    return item.id == id_vendor && item.size >= vendor_header_size;
}

Oui oui_at(const Bytes & bytes, size_t offset)
{
    return {bytes[offset], bytes[offset + 1], bytes[offset + 2]};
}

void check_walk(const Walk & walk,
                const Bytes & bytes,
                const std::vector<ies::Element> & elements,
                const char *property)
{
    test::fuzz_check(elements.size() == walk.items.size(), property);
    for (size_t i = 0; i < elements.size() && i < walk.items.size(); ++i)
    {
        test::fuzz_check(static_cast<uint8_t>(elements[i].id) == walk.items[i].id, "an element keeps its id");
        test::fuzz_check(test::is_view_of(elements[i].data, bytes, walk.items[i].offset, walk.items[i].size),
                         "an element data is a view of its bytes");
    }
}

void check_rates(const Walk & walk, const Bytes & bytes, const std::vector<ies::Element> & elements)
{
    std::vector<std::pair<uint32_t, bool>> expected;
    for (const Item & item : walk.items)
    {
        if (item.id != id_rates && item.id != id_extended_rates)
            continue;
        for (size_t i = 0; i < item.size; ++i)
        {
            const uint8_t byte = bytes[item.offset + i];
            const bool basic = (byte & 0x80) != 0;
            // BSS membership selectors reuse the top rate values with the basic bit
            if (!(basic && (byte & 0x7f) >= 120))
                expected.push_back({(byte & 0x7fu) * 500u, basic});
        }
    }

    const std::vector<ies::Rate> rates = ies::rates(elements);
    test::fuzz_check(rates.size() == expected.size(), "every rate that is not a membership selector is listed");
    for (size_t i = 0; i < rates.size() && i < expected.size(); ++i)
        test::fuzz_check(rates[i].kbps == expected[i].first && rates[i].basic == expected[i].second,
                         "a rate is the low seven bits in 500 kb/s units with its basic bit");
}

void check_channel(const Walk & walk, const Bytes & bytes, const std::vector<ies::Element> & elements)
{
    std::optional<uint8_t> expected;
    for (const uint8_t id : {id_ds_parameter, id_ht_operation})
    {
        const Item *item = first_item(walk, id);
        if (!expected.has_value() && item != nullptr && item->size >= 1)
            expected = bytes[item->offset];
    }
    test::fuzz_check(ies::channel(elements) == expected,
                     "the channel is the first byte of the ds or ht operation element");
}

const Item *first_vendor(const std::vector<const Item *> & vendors, const Bytes & bytes, const Oui & oui, uint8_t type)
{
    for (const Item *item : vendors)
    {
        if (oui_at(bytes, item->offset) == oui && bytes[item->offset + 3] == type)
            return item;
    }
    return nullptr;
}

void check_vendors(const Walk & walk, const Bytes & bytes, const std::vector<ies::Element> & elements)
{
    std::vector<const Item *> vendors;
    for (const Item & item : walk.items)
    {
        if (is_vendor(item))
            vendors.push_back(&item);
    }

    const std::vector<ies::Vendor> found = ies::vendors(elements);
    test::fuzz_check(found.size() == vendors.size(), "a vendor element needs room for an oui and a type");
    for (size_t i = 0; i < found.size() && i < vendors.size(); ++i)
    {
        const Item & item = *vendors[i];
        test::fuzz_check(found[i].oui == oui_at(bytes, item.offset) && found[i].type == bytes[item.offset + 3],
                         "a vendor is its oui and its type");
        test::fuzz_check(
            test::is_view_of(found[i].data, bytes, item.offset + vendor_header_size, item.size - vendor_header_size),
            "the vendor data follows the type");
    }
    if (vendors.empty())
        return;

    for (const Item *item : vendors)
    {
        const Oui oui = oui_at(bytes, item->offset);
        const uint8_t type = bytes[item->offset + 3];
        const Item *first = first_vendor(vendors, bytes, oui, type);
        const std::optional<ies::Vendor> same = ies::find_vendor(elements, oui, type);
        test::fuzz_check(same.has_value()
                             && test::is_view_of(same->data,
                                                 bytes,
                                                 first->offset + vendor_header_size,
                                                 first->size - vendor_header_size),
                         "find_vendor returns the first vendor with that oui and type");

        Oui other = oui;
        other[0] = static_cast<uint8_t>(other[0] ^ 0xff);
        test::fuzz_check(ies::find_vendor(elements, other, type).has_value()
                             == (first_vendor(vendors, bytes, other, type) != nullptr),
                         "find_vendor only returns a vendor with that oui and type");
    }
}

void check_chains(const Walk & walk, const Bytes & bytes, const std::vector<ies::Element> & elements)
{
    const Item *rsn_item = first_item(walk, id_rsn);
    const Result<std::optional<ies::Rsn>> rsn = ies::rsn(elements);
    if (rsn_item == nullptr)
    {
        test::fuzz_check(rsn.has_value() && !rsn->has_value(), "no rsn element, no rsn");
        return;
    }
    const Bytes body(bytes.begin() + rsn_item->offset, bytes.begin() + rsn_item->offset + rsn_item->size);
    const Result<ies::Rsn> parsed = ies::parse_rsn(ArrUByteView(body));
    test::fuzz_check(rsn.has_value() == parsed.has_value(), "the first rsn element is parsed as an rsn body");
    if (rsn.has_value())
        test::fuzz_check(rsn->has_value(), "a parsed rsn element gives an rsn");
}

void check_elements(const Bytes & content)
{
    const Bytes bytes = test::exact_copy(content);
    const ArrUByteView view(bytes);
    const Walk walk = walk_records(bytes);

    const Result<std::vector<ies::Element>> strict = ies::elements(view);
    test::fuzz_check(strict.has_value() == walk.complete, "elements are accepted exactly when none is cut");
    if (strict.has_value())
        check_walk(walk, bytes, *strict, "every element is listed");
    else
        test::fuzz_check(strict.error().code == ErrorCode::invalid_argument, "a cut element is invalid_argument");

    const std::vector<ies::Element> readable = ies::readable_elements(view);
    check_walk(walk, bytes, readable, "every whole element is readable");

    const Item *ssid = first_item(walk, id_ssid);
    test::fuzz_check(
        ies::ssid(readable)
            == (ssid == nullptr ? std::string()
                                : std::string(bytes.begin() + ssid->offset, bytes.begin() + ssid->offset + ssid->size)),
        "the ssid is the data of the first ssid element");
    for (const Item & item : walk.items)
    {
        const std::optional<ies::Element> found = ies::find(readable, static_cast<ies::Id>(item.id));
        test::fuzz_check(found.has_value()
                             && test::is_view_of(found->data,
                                                 bytes,
                                                 first_item(walk, item.id)->offset,
                                                 first_item(walk, item.id)->size),
                         "find returns the first element with that id");
    }
    check_rates(walk, bytes, readable);
    check_channel(walk, bytes, readable);
    check_vendors(walk, bytes, readable);
    check_chains(walk, bytes, readable);

    (void)ies::wpa(readable);
    (void)ies::wps(readable);
    (void)ies::parse_rsn(view);
    (void)ies::parse_wps(view);
    (void)ies::wps_attributes(view);
}

Bytes consume_data(FuzzedDataProvider & provider, uint8_t id)
{
    Bytes data;
    if (id == id_vendor && provider.ConsumeBool())
    {
        const Oui oui = provider.ConsumeBool() ? ies::microsoft_oui
                                               : (provider.ConsumeBool() ? ies::ieee_oui : Oui {1, 2, 3});
        data.insert(data.end(), oui.begin(), oui.end());
        data.push_back(provider.ConsumeBool() ? 1 : provider.ConsumeIntegral<uint8_t>());
    }
    const size_t more = provider.ConsumeIntegralInRange<size_t>(0, 8);
    test::put_bytes(data, provider.ConsumeBytes<uint8_t>(more));
    return data;
}

Bytes build_elements(FuzzedDataProvider & provider)
{
    std::vector<test::Element> elements(provider.ConsumeIntegralInRange<size_t>(0, 6));
    for (test::Element & element : elements)
    {
        element.id = provider.ConsumeBool()
                         ? interesting_ids[provider.ConsumeIntegralInRange<size_t>(0, interesting_ids.size() - 1)]
                         : provider.ConsumeIntegral<uint8_t>();
        element.data = consume_data(provider, element.id);
    }
    Bytes bytes = test::encode_elements(elements);
    if (!bytes.empty() && provider.ConsumeIntegralInRange<int>(0, 3) == 0)
        bytes.resize(bytes.size() - provider.ConsumeIntegralInRange<size_t>(1, bytes.size()));
    return bytes;
}

void check_rsn_elements(FuzzedDataProvider & provider)
{
    ies::Rsn rsn = test::consume_rsn(provider);
    const Bytes body = test::encode_rsn(rsn, provider.ConsumeIntegralInRange<size_t>(0, 4));
    Bytes wpa_body(ies::microsoft_oui.begin(), ies::microsoft_oui.end());
    wpa_body.push_back(1);
    test::put_bytes(wpa_body, body);

    const Bytes bytes = test::exact_copy(
        test::encode_elements({{id_ssid, {'a'}}, {id_rsn, body}, {id_vendor, wpa_body}}));
    const std::vector<ies::Element> elements = ies::readable_elements(ArrUByteView(bytes));
    const Result<std::optional<ies::Rsn>> from_rsn = ies::rsn(elements);
    const Result<std::optional<ies::Rsn>> from_wpa = ies::wpa(elements);
    test::fuzz_check(from_rsn.has_value() && from_rsn->has_value(), "an rsn element gives an rsn");
    test::fuzz_check(from_wpa.has_value() && from_wpa->has_value(), "a wpa vendor element gives an rsn");
    test::check_rsn_equal(**from_rsn, rsn);
    test::check_rsn_equal(**from_wpa, rsn);
}

void check_wps(FuzzedDataProvider & provider)
{
    // version, setup state, ap setup locked, manufacturer, model name, model number, device name, unknown
    constexpr std::array<uint16_t, 8> attribute_ids =
        {0x104a, 0x1044, 0x1057, 0x1021, ies::wps_model_name_attribute, 0x1024, 0x1011, 0x1234};

    std::vector<std::pair<uint16_t, Bytes>> attributes(provider.ConsumeIntegralInRange<size_t>(0, 6));
    Bytes bytes;
    ies::Wps expected;
    for (auto & [id, value] : attributes)
    {
        id = attribute_ids[provider.ConsumeIntegralInRange<size_t>(0, attribute_ids.size() - 1)];
        value = provider.ConsumeBytes<uint8_t>(provider.ConsumeIntegralInRange<size_t>(0, 3));
        test::put_be(bytes, id, 2);
        test::put_be(bytes, value.size(), 2);
        test::put_bytes(bytes, value);

        const std::string text(value.begin(), value.end());
        if (id == 0x104a && value.size() == 1)
            expected.version = value[0];
        else if (id == 0x1044 && value.size() == 1)
            expected.setup_state = value[0];
        else if (id == 0x1057)
            expected.ap_setup_locked = value.size() == 1 && value[0] != 0;
        else if (id == 0x1021)
            expected.manufacturer = text;
        else if (id == ies::wps_model_name_attribute)
            expected.model_name = text;
        else if (id == 0x1024)
            expected.model_number = text;
        else if (id == 0x1011)
            expected.device_name = text;
    }

    const Bytes exact = test::exact_copy(bytes);
    const std::vector<ies::WpsAttribute> walked = ies::wps_attributes(ArrUByteView(exact));
    test::fuzz_check(walked.size() == attributes.size(), "every wps attribute is walked");
    for (size_t i = 0; i < walked.size(); ++i)
        test::fuzz_check(walked[i].id == attributes[i].first && test::bytes_of(walked[i].value) == attributes[i].second,
                         "a wps attribute is its big endian id and its value");

    const auto wps = ies::parse_wps(ArrUByteView(exact));
    test::fuzz_check(wps.has_value(), "well formed wps attributes parse");
    test::fuzz_check(wps->version == expected.version && wps->setup_state == expected.setup_state,
                     "the wps version and setup state hold one byte");
    test::fuzz_check(wps->ap_setup_locked == expected.ap_setup_locked, "the ap is locked by one non zero byte");
    test::fuzz_check(wps->manufacturer == expected.manufacturer && wps->model_name == expected.model_name
                         && wps->model_number == expected.model_number && wps->device_name == expected.device_name,
                     "the last wps text attribute wins");
}

const char *expected_cipher(const Oui & oui, uint8_t type)
{
    const bool ieee = oui == ies::ieee_oui;
    if (!ieee && oui != ies::microsoft_oui)
        return "unknown";
    switch (type)
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
            return ieee ? "gcmp" : "unknown";
        case 9:
            return ieee ? "gcmp256" : "unknown";
        case 10:
            return ieee ? "ccmp256" : "unknown";
        default:
            return "unknown";
    }
}

const char *expected_akm(const Oui & oui, uint8_t type)
{
    if (oui == ies::microsoft_oui)
        return type == 1 ? "8021x" : (type == 2 ? "psk" : "unknown");
    if (oui != ies::ieee_oui)
        return "unknown";
    constexpr std::array<const char *, 19> names = {"unknown",
                                                    "8021x",
                                                    "psk",
                                                    "ft_8021x",
                                                    "ft_psk",
                                                    "8021x_sha256",
                                                    "psk_sha256",
                                                    "unknown",
                                                    "sae",
                                                    "ft_sae",
                                                    "unknown",
                                                    "unknown",
                                                    "unknown",
                                                    "unknown",
                                                    "unknown",
                                                    "unknown",
                                                    "unknown",
                                                    "unknown",
                                                    "owe"};
    return type < names.size() ? names[type] : "unknown";
}

void check_suite_names(FuzzedDataProvider & provider)
{
    ies::Suite suite;
    const int which = provider.ConsumeIntegralInRange<int>(0, 2);
    suite.oui = which == 0 ? ies::ieee_oui
                           : (which == 1 ? ies::microsoft_oui : Oui {provider.ConsumeIntegral<uint8_t>(), 0x0f, 0xac});
    suite.type = provider.ConsumeIntegralInRange<uint8_t>(0, 30);
    test::fuzz_check(ies::cipher_str(suite) == expected_cipher(suite.oui, suite.type),
                     "a cipher has its standard name");
    test::fuzz_check(ies::akm_str(suite) == expected_akm(suite.oui, suite.type), "an akm has its standard name");
}
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzedDataProvider provider(data, size);
    switch (provider.ConsumeIntegralInRange<int>(0, 4))
    {
        case 0:
            check_elements(provider.ConsumeRemainingBytes<uint8_t>());
            break;
        case 1:
            check_elements(build_elements(provider));
            break;
        case 2:
            check_rsn_elements(provider);
            break;
        case 3:
            check_wps(provider);
            break;
        default:
            check_suite_names(provider);
            break;
    }
    return 0;
}
