#include <array>
#include <cstddef>
#include <cstdint>
#include <fuzz_helper.hpp>
#include <optional>
#include <string>
#include <vector>

#include <sihd/util/ArrayView.hpp>
#include <sihd/wifi/capabilities.hpp>

#include <fuzzer/FuzzedDataProvider.h>

using namespace sihd::util;
using namespace sihd::wifi;

namespace
{
// where the last field read ends: the ht mcs set at 19, the vht tx map at 12, country environment and tim control at 3
constexpr size_t ht_needed_size = 19;
constexpr size_t vht_needed_size = 12;
constexpr size_t country_needed_size = 3;
constexpr size_t tim_needed_size = 3;

constexpr size_t ht_mcs_set_offset = 3;
constexpr size_t ht_rx_highest_offset = 13;
constexpr size_t vht_streams_max = 8;
constexpr size_t country_header_size = 3;
constexpr size_t country_triplet_size = 3;
constexpr uint8_t country_last_channel_number = 200;
constexpr uint8_t extension_he_capabilities = 35;
constexpr uint8_t extension_eht_capabilities = 108;

struct Kind
{
        ies::Id id;
        size_t needed;
};

constexpr std::array<uint16_t, 4> vht_mpdu_lengths = {3895, 7991, 11454, 0};

uint32_t read_le(ArrUByteView bytes, size_t offset, size_t width)
{
    uint32_t value = 0;
    for (size_t i = 0; i < width; ++i)
        value |= static_cast<uint32_t>(bytes[offset + i]) << (8 * i);
    return value;
}

const ies::Element *first_of(const std::vector<ies::Element> & elements, ies::Id id)
{
    for (const ies::Element & element : elements)
    {
        if (element.id == id)
            return &element;
    }
    return nullptr;
}

std::vector<ies::Element> isolated(const std::vector<ies::Element> & elements,
                                   std::vector<std::vector<uint8_t>> & storage)
{
    std::vector<ies::Element> copies;
    for (const ies::Element & element : elements)
    {
        storage.emplace_back(element.data.begin(), element.data.end());
        copies.push_back({element.id, ArrUByteView(storage.back().data(), storage.back().size())});
    }
    return copies;
}

template <typename T>
const T *decoded(const Result<std::optional<T>> & result, const ies::Element *element, size_t needed_size)
{
    if (element == nullptr)
    {
        test::fuzz_check(result.has_value() && !result->has_value(), "a missing element decodes to nothing");
        return nullptr;
    }
    if (!result.has_value())
    {
        test::fuzz_check(result.error().code == ErrorCode::invalid_argument, "a short element is invalid_argument");
        test::fuzz_check(element->data.size() < needed_size, "an element holding its fields decodes");
        return nullptr;
    }
    test::fuzz_check(element->data.size() >= needed_size, "an element too short for its fields is refused");
    test::fuzz_check(result->has_value(), "a present element decodes to a value");
    return &**result;
}

uint8_t highest_stream(uint16_t map)
{
    uint8_t highest = 0;
    for (size_t stream = 1; stream <= vht_streams_max; ++stream)
    {
        if (((map >> (2 * (stream - 1))) & 3) != 3)
            highest = static_cast<uint8_t>(stream);
    }
    return highest;
}

void check_ht(const std::vector<ies::Element> & elements)
{
    const ies::Element *element = first_of(elements, ies::Id::ht_capabilities);
    const Result<std::optional<ies::HtCapabilities>> result = ies::ht_capabilities(elements);
    const ies::HtCapabilities *ht = decoded(result, element, ht_needed_size);
    if (ht == nullptr)
        return;

    const ArrUByteView body = element->data;
    const uint32_t info = read_le(body, 0, 2);
    test::fuzz_check(ht->info == info, "the info field is the first two bytes");
    test::fuzz_check(ht->ldpc == ((info & 0x1) != 0), "ldpc is bit 0");
    test::fuzz_check(ht->width_40mhz == ((info & 0x2) != 0), "40 mhz is bit 1");
    test::fuzz_check(ht->spatial_multiplexing_power_save == ((info >> 2) & 0x3), "smps is bits 2 and 3");
    test::fuzz_check(ht->greenfield == ((info & 0x10) != 0), "greenfield is bit 4");
    test::fuzz_check(ht->short_gi_20mhz == ((info & 0x20) != 0), "short gi at 20 mhz is bit 5");
    test::fuzz_check(ht->short_gi_40mhz == ((info & 0x40) != 0), "short gi at 40 mhz is bit 6");
    test::fuzz_check(ht->tx_stbc == ((info & 0x80) != 0), "tx stbc is bit 7");
    test::fuzz_check(ht->rx_stbc_streams == ((info >> 8) & 0x3), "rx stbc is bits 8 and 9");
    test::fuzz_check(ht->max_amsdu_7935 == ((info & 0x800) != 0), "the large a-msdu is bit 11");
    test::fuzz_check(ht->dsss_cck_40mhz == ((info & 0x1000) != 0), "dsss/cck at 40 mhz is bit 12");
    test::fuzz_check(ht->forty_mhz_intolerant == ((info & 0x4000) != 0), "40 mhz intolerant is bit 14");
    test::fuzz_check(ht->ampdu_length_exponent == (body[2] & 0x3), "the a-mpdu exponent is bits 0 and 1");
    test::fuzz_check(ht->ampdu_min_spacing == ((body[2] >> 2) & 0x7), "the a-mpdu spacing is bits 2 to 4");
    test::fuzz_check(ht->rx_highest_rate_mbps == (read_le(body, ht_rx_highest_offset, 2) & 0x3ff),
                     "the highest rate is ten bits");

    uint8_t streams = 0;
    for (uint8_t stream = 1; stream <= 4; ++stream)
    {
        if (body[ht_mcs_set_offset + stream - 1] != 0)
            streams = stream;
    }
    test::fuzz_check(ht->rx_spatial_streams == streams, "the streams are the highest mcs group with a bit set");
}

void check_vht(const std::vector<ies::Element> & elements)
{
    const ies::Element *element = first_of(elements, ies::Id::vht_capabilities);
    const Result<std::optional<ies::VhtCapabilities>> result = ies::vht_capabilities(elements);
    const ies::VhtCapabilities *vht = decoded(result, element, vht_needed_size);
    if (vht == nullptr)
        return;

    const ArrUByteView body = element->data;
    const uint32_t info = read_le(body, 0, 4);
    const uint32_t width_set = (info >> 2) & 0x3;
    test::fuzz_check(vht->info == info, "the info field is the first four bytes");
    test::fuzz_check(vht->max_mpdu_length == vht_mpdu_lengths[info & 0x3], "the mpdu length is bits 0 and 1");
    test::fuzz_check(vht->width_160mhz == (width_set == 1 || width_set == 2), "160 mhz comes with set 1 and 2");
    test::fuzz_check(vht->width_80plus80mhz == (width_set == 2), "80+80 mhz comes with set 2");
    test::fuzz_check(vht->rx_ldpc == ((info & 0x10) != 0), "rx ldpc is bit 4");
    test::fuzz_check(vht->short_gi_80mhz == ((info & 0x20) != 0), "short gi at 80 mhz is bit 5");
    test::fuzz_check(vht->short_gi_160mhz == ((info & 0x40) != 0), "short gi at 160 mhz is bit 6");
    test::fuzz_check(vht->tx_stbc == ((info & 0x80) != 0), "tx stbc is bit 7");
    test::fuzz_check(vht->rx_stbc_streams == ((info >> 8) & 0x7), "rx stbc is bits 8 to 10");
    test::fuzz_check(vht->su_beamformer == ((info & 0x800) != 0), "su beamformer is bit 11");
    test::fuzz_check(vht->su_beamformee == ((info & 0x1000) != 0), "su beamformee is bit 12");
    test::fuzz_check(vht->mu_beamformer == ((info & 0x80000) != 0), "mu beamformer is bit 19");
    test::fuzz_check(vht->mu_beamformee == ((info & 0x100000) != 0), "mu beamformee is bit 20");
    test::fuzz_check(vht->ampdu_length_exponent == ((info >> 23) & 0x7), "the a-mpdu exponent is bits 23 to 25");
    test::fuzz_check(vht->rx_mcs_map == read_le(body, 4, 2), "the rx map follows the info");
    test::fuzz_check(vht->tx_mcs_map == read_le(body, 8, 2), "the tx map follows the rx data rate");
    test::fuzz_check(vht->rx_spatial_streams == highest_stream(vht->rx_mcs_map), "rx streams follow the rx map");
    test::fuzz_check(vht->tx_spatial_streams == highest_stream(vht->tx_mcs_map), "tx streams follow the tx map");
}

void check_country(const std::vector<ies::Element> & elements)
{
    const ies::Element *element = first_of(elements, ies::Id::country);
    const Result<std::optional<ies::Country>> result = ies::country(elements);
    const ies::Country *country = decoded(result, element, country_needed_size);
    if (country == nullptr)
        return;

    const ArrUByteView body = element->data;
    test::fuzz_check(country->code == std::string(body.begin(), body.begin() + 2), "the code is the first two bytes");
    test::fuzz_check(country->environment == static_cast<char>(body[2]), "the environment is the third byte");

    std::vector<ies::Country::Channels> expected;
    for (size_t offset = country_header_size; offset + country_triplet_size <= body.size();
         offset += country_triplet_size)
    {
        if (body[offset] <= country_last_channel_number)
            expected.push_back({body[offset], body[offset + 1], static_cast<int8_t>(body[offset + 2])});
    }
    test::fuzz_check(country->channels.size() == expected.size(), "every channel triplet is listed");
    for (size_t i = 0; i < expected.size(); ++i)
    {
        test::fuzz_check(country->channels[i].first == expected[i].first, "a triplet keeps its first channel");
        test::fuzz_check(country->channels[i].count == expected[i].count, "a triplet keeps its channel count");
        test::fuzz_check(country->channels[i].max_power_dbm == expected[i].max_power_dbm, "a triplet keeps its power");
    }
}

void check_tim(const std::vector<ies::Element> & elements)
{
    const ies::Element *element = first_of(elements, ies::Id::tim);
    const Result<std::optional<ies::Tim>> result = ies::tim(elements);
    const ies::Tim *tim = decoded(result, element, tim_needed_size);
    if (tim == nullptr)
        return;

    const ArrUByteView body = element->data;
    test::fuzz_check(tim->dtim_count == body[0], "the count is the first byte");
    test::fuzz_check(tim->dtim_period == body[1], "the period is the second byte");
    test::fuzz_check(tim->multicast_buffered == ((body[2] & 0x1) != 0), "multicast is bit 0 of the bitmap control");
}

void check_standards(const std::vector<ies::Element> & elements)
{
    bool he = false;
    bool eht = false;
    for (const ies::Element & element : elements)
    {
        if (element.id != ies::Id::extension || element.data.size() < 1)
            continue;
        he = he || element.data[0] == extension_he_capabilities;
        eht = eht || element.data[0] == extension_eht_capabilities;
    }

    const ies::Standards standards = ies::standards(elements);
    test::fuzz_check(standards.ht == (first_of(elements, ies::Id::ht_capabilities) != nullptr),
                     "ht comes with its element");
    test::fuzz_check(standards.vht == (first_of(elements, ies::Id::vht_capabilities) != nullptr),
                     "vht comes with its element");
    test::fuzz_check(standards.he == he, "he comes with its extension");
    test::fuzz_check(standards.eht == eht, "eht comes with its extension");
}

void check_capabilities(const std::vector<uint8_t> & bytes)
{
    std::vector<std::vector<uint8_t>> storage;
    const std::vector<ies::Element> elements = isolated(ies::readable_elements(ArrUByteView(bytes)), storage);

    check_ht(elements);
    check_vht(elements);
    check_country(elements);
    check_tim(elements);
    check_standards(elements);
}

void fuzz_single_element(FuzzedDataProvider & provider)
{
    constexpr std::array<Kind, 5> kinds = {{{ies::Id::ht_capabilities, ht_needed_size},
                                            {ies::Id::vht_capabilities, vht_needed_size},
                                            {ies::Id::country, country_needed_size},
                                            {ies::Id::tim, tim_needed_size},
                                            {ies::Id::extension, 1}}};
    const Kind kind = kinds[provider.ConsumeIntegralInRange<size_t>(0, kinds.size() - 1)];
    const size_t around = kind.needed - 1 + provider.ConsumeIntegralInRange<size_t>(0, 2);
    const size_t size = provider.ConsumeBool() ? around : provider.ConsumeIntegralInRange<size_t>(0, 255);
    const std::vector<uint8_t> body = test::consume_exact_bytes(provider, size);

    std::vector<uint8_t> bytes = {static_cast<uint8_t>(kind.id), static_cast<uint8_t>(body.size())};
    bytes.insert(bytes.end(), body.begin(), body.end());
    check_capabilities(bytes);
}
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzedDataProvider provider(data, size);
    if (provider.ConsumeBool())
        fuzz_single_element(provider);
    else
        check_capabilities(provider.ConsumeRemainingBytes<uint8_t>());
    return 0;
}
