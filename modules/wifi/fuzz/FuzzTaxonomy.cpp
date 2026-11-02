#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <fuzz_helper.hpp>
#include <optional>
#include <string>
#include <vector>

#include <sihd/util/ArrayView.hpp>
#include <sihd/wifi/ies.hpp>
#include <sihd/wifi/taxonomy.hpp>

#include "wifi_fuzz_helper.hpp"
#include <fuzzer/FuzzedDataProvider.h>

using namespace sihd::util;
using namespace sihd::wifi;
using test::Bytes;

namespace
{
constexpr size_t element_header_size = 2;
constexpr uint8_t id_ht_capabilities = 45;
constexpr uint8_t id_vht_capabilities = 191;
constexpr uint8_t id_extended_capabilities = 127;
constexpr uint8_t id_power_capability = 33;
constexpr uint8_t id_vendor = 221;
constexpr uint16_t wps_model_name = 0x1023;
constexpr size_t wps_model_name_max = 32;
constexpr size_t extcap_max = 254;

constexpr std::array<size_t, 8> ht_sizes = {0, 1, 2, 3, 6, 7, 8, 26};
constexpr std::array<size_t, 8> vht_sizes = {0, 3, 4, 7, 8, 11, 12, 13};

struct Half
{
        Bytes bytes;
        std::string expected;
};

size_t whole_elements_size(const std::vector<uint8_t> & bytes)
{
    size_t offset = 0;
    while (offset + element_header_size <= bytes.size()
           && offset + element_header_size + bytes[offset + 1] <= bytes.size())
        offset += element_header_size + bytes[offset + 1];
    return offset;
}

std::string signature_of(const std::vector<uint8_t> & elements)
{
    return taxonomy::element_signature(ArrUByteView(elements));
}

void check_half(const std::vector<uint8_t> & elements)
{
    const std::string signature = signature_of(elements);

    const std::vector<uint8_t> copy(elements.begin(), elements.end());
    test::fuzz_check(signature_of(copy) == signature, "a signature is deterministic");

    const size_t whole = whole_elements_size(elements);
    test::fuzz_check(signature.empty() == (whole == 0), "a signature is empty exactly when no element is whole");
    const std::vector<uint8_t> prefix(elements.begin(), elements.begin() + whole);
    test::fuzz_check(signature_of(prefix) == signature, "a damaged element ends the list");

    for (const char c : signature)
        test::fuzz_check(c >= ' ' && c <= '~' && c != '|', "a signature is printable and holds no half separator");
}

void check_line(const std::vector<uint8_t> & probe, const std::vector<uint8_t> & assoc)
{
    const std::string probe_signature = signature_of(probe);
    const std::string assoc_signature = signature_of(assoc);

    const Result<std::string> line = taxonomy::signature(ArrUByteView(probe), ArrUByteView(assoc));
    test::fuzz_check(line.has_value() == (!probe_signature.empty() && !assoc_signature.empty()),
                     "a line needs both halves");
    if (!line.has_value())
    {
        test::fuzz_check(line.error().code == ErrorCode::invalid_argument, "a missing half is invalid_argument");
        return;
    }
    test::fuzz_check(*line == "wifi4|probe:" + probe_signature + "|assoc:" + assoc_signature,
                     "the line is the two halves");
}

std::string hex(uint64_t value, int digits)
{
    std::array<char, 17> text = {};
    std::snprintf(text.data(), text.size(), "%0*llx", digits, static_cast<unsigned long long>(value));
    return text.data();
}

bool is_detail_id(uint8_t id)
{
    return id == id_ht_capabilities || id == id_vht_capabilities || id == id_extended_capabilities
           || id == id_power_capability || id == id_vendor;
}

// the details of a signature are told by a single element each: this harness never repeats one
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

void describe_ht(const Bytes & data, Details & details)
{
    if (data.size() >= 2)
        details.htcap = hex(test::le_at(data, 0, 2), 4);
    if (data.size() >= 3)
        details.htagg = hex(data[2], 2);
    if (data.size() >= 7)
        details.htmcs = hex(test::le_at(data, 3, 4) & 0xffff, 8);
}

void describe_vht(const Bytes & data, Details & details)
{
    if (data.size() >= 4)
        details.vhtcap = hex(test::le_at(data, 0, 4), 8);
    if (data.size() >= 8)
        details.vhtrxmcs = hex(test::le_at(data, 4, 4), 8);
    if (data.size() >= 12)
        details.vhttxmcs = hex(test::le_at(data, 8, 4), 8);
}

std::string describe_extcap(const Bytes & data)
{
    std::string text;
    for (size_t i = 0; i < data.size() && i < extcap_max; ++i)
        text += hex(data[i], 2);
    return text;
}

std::string model_name_text(const Bytes & value)
{
    std::string text;
    for (size_t i = 0; i < value.size() && i < wps_model_name_max; ++i)
    {
        const uint8_t byte = value[i];
        const bool alphanumeric = (byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z')
                                  || (byte >= '0' && byte <= '9');
        text.push_back(alphanumeric ? static_cast<char>(byte) : '_');
    }
    return text;
}

std::string consume_wps_attributes(FuzzedDataProvider & provider, Bytes & out)
{
    std::optional<std::string> name;
    for (size_t count = provider.ConsumeIntegralInRange<size_t>(0, 3); count > 0; --count)
    {
        const uint16_t id = provider.ConsumeBool() ? wps_model_name : provider.ConsumeIntegral<uint16_t>();
        const Bytes value = provider.ConsumeBytes<uint8_t>(provider.ConsumeIntegralInRange<size_t>(0, 40));
        test::put_be(out, id, 2);
        test::put_be(out, value.size(), 2);
        test::put_bytes(out, value);
        if (id == wps_model_name && !name.has_value())
            name = model_name_text(value);
    }
    if (provider.ConsumeIntegralInRange<int>(0, 3) == 0)
        test::put_bytes(out, {0x10, wps_model_name & 0xff, 0x00, 0x09, 'a'});
    return name.has_value() ? *name : std::string();
}

uint8_t consume_plain_id(FuzzedDataProvider & provider)
{
    uint8_t id = provider.ConsumeIntegral<uint8_t>();
    while (is_detail_id(id))
        ++id;
    return id;
}

struct Built
{
        test::Element element;
        std::string token;
};

Built consume_plain(FuzzedDataProvider & provider)
{
    Built built;
    built.element.id = consume_plain_id(provider);
    built.element.data = provider.ConsumeBytes<uint8_t>(provider.ConsumeIntegralInRange<size_t>(0, 6));
    built.token = std::to_string(built.element.id);
    return built;
}

template <size_t N>
Bytes consume_sized(FuzzedDataProvider & provider, const std::array<size_t, N> & sizes)
{
    return test::consume_exact_bytes(provider, sizes[provider.ConsumeIntegralInRange<size_t>(0, N - 1)]);
}

Built consume_listed(FuzzedDataProvider & provider, size_t kind, Details & details)
{
    Built built;
    switch (kind)
    {
        case 0:
            built.element = {id_ht_capabilities, consume_sized(provider, ht_sizes)};
            describe_ht(built.element.data, details);
            break;
        case 1:
            built.element = {id_vht_capabilities, consume_sized(provider, vht_sizes)};
            describe_vht(built.element.data, details);
            break;
        case 2:
            built.element = {id_extended_capabilities,
                             test::consume_exact_bytes(
                                 provider,
                                 provider.ConsumeBool() ? 255 : provider.ConsumeIntegralInRange<size_t>(0, 12))};
            details.extcap = describe_extcap(built.element.data);
            break;
        default:
            built.element = {id_power_capability,
                             test::consume_exact_bytes(provider, provider.ConsumeIntegralInRange<size_t>(0, 3))};
            if (built.element.data.size() == 2)
                details.txpow = hex(test::le_at(built.element.data, 0, 2), 4);
            break;
    }
    built.token = std::to_string(built.element.id);
    return built;
}

Built consume_vendor(FuzzedDataProvider & provider, bool wps, Details & details)
{
    Built built;
    built.element.id = id_vendor;
    Oui oui = ies::microsoft_oui;
    uint8_t type = ies::microsoft_wps_type;
    if (!wps)
    {
        oui = provider.ConsumeBool() ? ies::microsoft_oui : ies::ieee_oui;
        type = provider.ConsumeBool() ? 1 : provider.ConsumeIntegralInRange<uint8_t>(5, 255);
        if (provider.ConsumeIntegralInRange<int>(0, 3) == 0)
        {
            built.element.data = provider.ConsumeBytes<uint8_t>(provider.ConsumeIntegralInRange<size_t>(0, 3));
            built.token = "221";
            return built;
        }
    }

    built.element.data.assign(oui.begin(), oui.end());
    built.element.data.push_back(type);
    const std::string name = consume_wps_attributes(provider, built.element.data);
    built.token = "221(" + hex((oui[0] << 16) | (oui[1] << 8) | oui[2], 6) + "," + std::to_string(type) + ")";
    if (wps && !name.empty())
        details.wps = name;
    return built;
}

void append_detail(std::string & text, const char *name, const std::optional<std::string> & value)
{
    if (value.has_value())
        text += std::string(",") + name + ":" + *value;
}

std::string with_details(std::string text, const Details & details)
{
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

Built consume_element(FuzzedDataProvider & provider, std::array<bool, 5> & used, Details & details)
{
    const size_t kind = provider.ConsumeIntegralInRange<size_t>(0, 6);
    const bool repeated = kind < used.size() && used[kind];
    if (kind < used.size())
        used[kind] = true;

    if (repeated || kind == 6)
        return consume_plain(provider);
    if (kind < 4)
        return consume_listed(provider, kind, details);
    return consume_vendor(provider, kind == 4, details);
}

Half consume_half(FuzzedDataProvider & provider)
{
    std::vector<test::Element> elements;
    std::string ids;
    Details details;
    std::array<bool, 5> used = {};
    for (size_t count = provider.ConsumeIntegralInRange<size_t>(0, 6); count > 0; --count)
    {
        const Built built = consume_element(provider, used, details);
        elements.push_back(built.element);
        ids += (ids.empty() ? "" : ",") + built.token;
    }

    Half half;
    half.bytes = test::encode_elements(elements);
    half.expected = with_details(ids, details);
    if (provider.ConsumeIntegralInRange<int>(0, 3) == 0)
        test::put_bytes(half.bytes, provider.ConsumeBool() ? Bytes {0x00} : Bytes {0x00, 0x05, 0x01});
    return half;
}

void check_generated(FuzzedDataProvider & provider)
{
    const Half probe = consume_half(provider);
    const Half assoc = consume_half(provider);
    const Bytes probe_bytes = test::exact_copy(probe.bytes);
    const Bytes assoc_bytes = test::exact_copy(assoc.bytes);

    test::fuzz_check(signature_of(probe_bytes) == probe.expected, "a signature lists the ids then the details");
    test::fuzz_check(signature_of(assoc_bytes) == assoc.expected, "a signature lists the ids then the details");
    const Result<std::string> line = taxonomy::signature(ArrUByteView(probe_bytes), ArrUByteView(assoc_bytes));
    test::fuzz_check(line.has_value() == (!probe.expected.empty() && !assoc.expected.empty()),
                     "a line needs both halves");
    if (line.has_value())
        test::fuzz_check(*line == "wifi4|probe:" + probe.expected + "|assoc:" + assoc.expected,
                         "the line is the version then the two halves");
}
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzedDataProvider provider(data, size);
    if (provider.ConsumeBool())
    {
        check_generated(provider);
        return 0;
    }

    const size_t probe_size = provider.ConsumeIntegralInRange<size_t>(0, provider.remaining_bytes());
    const std::vector<uint8_t> probe = provider.ConsumeBytes<uint8_t>(probe_size);
    const std::vector<uint8_t> assoc = provider.ConsumeRemainingBytes<uint8_t>();

    check_half(probe);
    check_half(assoc);
    check_line(probe, assoc);
    return 0;
}
