#include <cstddef>
#include <cstdint>
#include <fuzz_helper.hpp>
#include <optional>
#include <string>
#include <vector>

#include <sihd/util/ArrayView.hpp>
#include <sihd/wifi/ies.hpp>

#include "wifi_fuzz_helper.hpp"
#include <fuzzer/FuzzedDataProvider.h>

using namespace sihd::util;
using namespace sihd::wifi;
using test::Bytes;

namespace
{
constexpr size_t suite_size = 4;
constexpr size_t count_size = 2;
constexpr size_t version_size = 2;
constexpr size_t rsn_fixed_size = version_size + suite_size;
constexpr size_t wps_field_size = 2;
constexpr size_t wps_header_size = 2 * wps_field_size;

constexpr uint16_t wps_version = 0x104a;
constexpr uint16_t wps_setup_state = 0x1044;
constexpr uint16_t wps_ap_setup_locked = 0x1057;
constexpr uint16_t wps_manufacturer = 0x1021;
constexpr uint16_t wps_model_number = 0x1024;
constexpr uint16_t wps_device_name = 0x1011;

struct WpsItem
{
        uint16_t id = 0;
        std::vector<uint8_t> value;
};

struct WpsWalk
{
        std::vector<WpsItem> items;
        bool complete = true;
};

uint16_t le16(const std::vector<uint8_t> & bytes, size_t offset)
{
    return static_cast<uint16_t>(bytes[offset] | (bytes[offset + 1] << 8));
}

uint16_t be16(const std::vector<uint8_t> & bytes, size_t offset)
{
    return static_cast<uint16_t>((bytes[offset] << 8) | bytes[offset + 1]);
}

ies::Suite suite_at(const std::vector<uint8_t> & bytes, size_t offset)
{
    return {.oui = {bytes[offset], bytes[offset + 1], bytes[offset + 2]}, .type = bytes[offset + 3]};
}

std::vector<uint8_t> bytes_of(ArrUByteView view)
{
    return std::vector<uint8_t>(view.begin(), view.end());
}

size_t check_suite_list(const std::vector<ies::Suite> & suites, const std::vector<uint8_t> & body, size_t offset)
{
    if (offset >= body.size())
    {
        test::fuzz_check(suites.empty(), "no bytes left, no suites");
        return offset;
    }
    const size_t first = offset + count_size;
    test::fuzz_check(first + suites.size() * suite_size <= body.size(), "the suites fit inside the element");
    test::fuzz_check(le16(body, offset) == suites.size(), "the suite count is the announced one");
    for (size_t i = 0; i < suites.size(); ++i)
        test::fuzz_check(suites[i] == suite_at(body, first + i * suite_size),
                         "a suite is the four bytes it was read from");
    return first + suites.size() * suite_size;
}

void check_rsn(const std::vector<uint8_t> & body)
{
    const Result<ies::Rsn> rsn = ies::parse_rsn(ArrUByteView(body));
    if (!rsn.has_value())
    {
        test::fuzz_check(rsn.error().code == ErrorCode::invalid_argument, "a damaged rsn is invalid_argument");
        return;
    }
    test::fuzz_check(body.size() >= rsn_fixed_size, "an rsn holds a version and a group cipher");
    test::fuzz_check(rsn->version == le16(body, 0), "the version is the first two bytes");
    test::fuzz_check(rsn->group == suite_at(body, version_size), "the group cipher follows the version");

    size_t offset = check_suite_list(rsn->pairwise, body, rsn_fixed_size);
    offset = check_suite_list(rsn->akms, body, offset);
    if (offset + count_size <= body.size())
        test::fuzz_check(rsn->capabilities == le16(body, offset), "the capabilities follow the akm suites");
    else
        test::fuzz_check(!rsn->capabilities.has_value(), "no bytes left, no capabilities");
}

void fuzz_rsn_round_trip(FuzzedDataProvider & provider)
{
    ies::Rsn rsn = test::consume_rsn(provider);
    const size_t parts = provider.ConsumeIntegralInRange<size_t>(0, 4);
    const Bytes body = test::exact_copy(test::encode_rsn(rsn, parts));

    const Result<ies::Rsn> parsed = ies::parse_rsn(ArrUByteView(body));
    test::fuzz_check(parsed.has_value(), "a well formed rsn parses");
    if (!parsed.has_value())
        return;
    test::check_rsn_equal(*parsed, rsn);

    if (parts <= 2)
    {
        const Bytes cut = test::exact_copy(Bytes(body.begin(), body.end() - 1));
        test::fuzz_check(!ies::parse_rsn(ArrUByteView(cut)).has_value(), "an rsn cut inside a part is refused");
    }
}

WpsWalk walk_wps(const std::vector<uint8_t> & bytes)
{
    WpsWalk walk;
    size_t offset = 0;
    while (offset < bytes.size())
    {
        if (offset + wps_header_size > bytes.size())
        {
            walk.complete = false;
            break;
        }
        const size_t length = be16(bytes, offset + wps_field_size);
        const size_t first = offset + wps_header_size;
        if (first + length > bytes.size())
        {
            walk.complete = false;
            break;
        }
        walk.items.push_back({be16(bytes, offset), {bytes.begin() + first, bytes.begin() + first + length}});
        offset = first + length;
    }
    return walk;
}

std::vector<const WpsItem *> items_with(const std::vector<WpsItem> & items, uint16_t id)
{
    std::vector<const WpsItem *> same;
    for (const WpsItem & item : items)
    {
        if (item.id == id)
            same.push_back(&item);
    }
    return same;
}

void check_text(const std::vector<WpsItem> & items, uint16_t id, const std::string & text)
{
    const std::vector<const WpsItem *> same = items_with(items, id);
    if (same.empty())
        test::fuzz_check(text.empty(), "a missing attribute leaves the text empty");
    else if (same.size() == 1)
        test::fuzz_check(text == std::string(same[0]->value.begin(), same[0]->value.end()),
                         "the text is the attribute value");
}

void check_byte(const std::vector<WpsItem> & items, uint16_t id, const std::optional<uint8_t> & field)
{
    const std::vector<const WpsItem *> same = items_with(items, id);
    if (same.empty())
        test::fuzz_check(!field.has_value(), "a missing attribute leaves the field empty");
    else if (same.size() == 1)
    {
        const std::vector<uint8_t> & value = same[0]->value;
        test::fuzz_check(field == (value.size() == 1 ? std::optional<uint8_t>(value[0]) : std::nullopt),
                         "a one byte attribute is the field");
    }
}

void check_locked(const std::vector<WpsItem> & items, bool locked)
{
    const std::vector<const WpsItem *> same = items_with(items, wps_ap_setup_locked);
    if (same.empty())
        test::fuzz_check(!locked, "a missing attribute does not lock the setup");
    else if (same.size() == 1)
        test::fuzz_check(locked == (same[0]->value.size() == 1 && same[0]->value[0] != 0),
                         "a non zero byte locks the setup");
}

void check_wps(const std::vector<uint8_t> & attributes)
{
    const ArrUByteView view(attributes);
    const WpsWalk expected = walk_wps(attributes);

    const std::vector<ies::WpsAttribute> found = ies::wps_attributes(view);
    test::fuzz_check(found.size() == expected.items.size(), "every whole attribute is listed");
    for (size_t i = 0; i < found.size(); ++i)
    {
        test::fuzz_check(found[i].id == expected.items[i].id, "an attribute keeps its id");
        test::fuzz_check(bytes_of(found[i].value) == expected.items[i].value, "an attribute keeps its value");
    }

    const Result<ies::Wps> wps = ies::parse_wps(view);
    test::fuzz_check(wps.has_value() == expected.complete, "wps parses exactly when no attribute is cut");
    if (!wps.has_value())
    {
        test::fuzz_check(wps.error().code == ErrorCode::invalid_argument, "a cut wps is invalid_argument");
        return;
    }
    check_byte(expected.items, wps_version, wps->version);
    check_byte(expected.items, wps_setup_state, wps->setup_state);
    check_locked(expected.items, wps->ap_setup_locked);
    check_text(expected.items, wps_manufacturer, wps->manufacturer);
    check_text(expected.items, ies::wps_model_name_attribute, wps->model_name);
    check_text(expected.items, wps_model_number, wps->model_number);
    check_text(expected.items, wps_device_name, wps->device_name);
}
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzedDataProvider provider(data, size);
    switch (provider.ConsumeIntegralInRange<int>(0, 2))
    {
        case 0:
            check_rsn(test::exact_copy(provider.ConsumeRemainingBytes<uint8_t>()));
            break;
        case 1:
            check_wps(test::exact_copy(provider.ConsumeRemainingBytes<uint8_t>()));
            break;
        default:
            fuzz_rsn_round_trip(provider);
            break;
    }
    return 0;
}
