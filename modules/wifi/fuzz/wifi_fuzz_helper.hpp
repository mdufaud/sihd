#ifndef __SIHD_WIFI_FUZZ_HELPER_HPP__
#define __SIHD_WIFI_FUZZ_HELPER_HPP__

#include <cstddef>
#include <cstdint>
#include <fuzz_helper.hpp>
#include <vector>

#include <sihd/util/ArrayView.hpp>
#include <sihd/wifi/ies.hpp>

#include <fuzzer/FuzzedDataProvider.h>

namespace test
{

using Bytes = std::vector<uint8_t>;

inline void put_le(Bytes & out, uint64_t value, size_t width)
{
    for (size_t i = 0; i < width; ++i)
        out.push_back(static_cast<uint8_t>(value >> (8 * i)));
}

inline void put_be(Bytes & out, uint64_t value, size_t width)
{
    for (size_t i = width; i > 0; --i)
        out.push_back(static_cast<uint8_t>(value >> (8 * (i - 1))));
}

inline void put_bytes(Bytes & out, const Bytes & more)
{
    out.insert(out.end(), more.begin(), more.end());
}

inline uint64_t le_at(const Bytes & bytes, size_t offset, size_t width)
{
    uint64_t value = 0;
    for (size_t i = 0; i < width; ++i)
        value |= static_cast<uint64_t>(bytes[offset + i]) << (8 * i);
    return value;
}

inline uint64_t be_at(const Bytes & bytes, size_t offset, size_t width)
{
    uint64_t value = 0;
    for (size_t i = 0; i < width; ++i)
        value = (value << 8) | bytes[offset + i];
    return value;
}

// the allocation is exactly the size of the content, so ASan sees a read past it
inline Bytes exact_copy(const Bytes & bytes)
{
    return Bytes(bytes.begin(), bytes.end());
}

inline Bytes bytes_of(sihd::util::ArrUByteView view)
{
    return Bytes(view.begin(), view.end());
}

inline bool is_view_of(sihd::util::ArrUByteView view, const Bytes & parent, size_t offset, size_t size)
{
    return view.size() == size && (size == 0 || view.data() == parent.data() + offset);
}

struct Element
{
        uint8_t id = 0;
        Bytes data;
};

inline Bytes encode_elements(const std::vector<Element> & elements)
{
    Bytes out;
    for (const Element & element : elements)
    {
        out.push_back(element.id);
        out.push_back(static_cast<uint8_t>(element.data.size()));
        put_bytes(out, element.data);
    }
    return out;
}

inline void put_suite(Bytes & out, const sihd::wifi::ies::Suite & suite)
{
    out.insert(out.end(), suite.oui.begin(), suite.oui.end());
    out.push_back(suite.type);
}

inline void put_suites(Bytes & out, const std::vector<sihd::wifi::ies::Suite> & suites)
{
    put_le(out, suites.size(), 2);
    for (const sihd::wifi::ies::Suite & suite : suites)
        put_suite(out, suite);
}

inline sihd::wifi::ies::Suite consume_suite(FuzzedDataProvider & provider)
{
    sihd::wifi::ies::Suite suite;
    for (uint8_t & byte : suite.oui)
        byte = provider.ConsumeIntegral<uint8_t>();
    suite.type = provider.ConsumeIntegral<uint8_t>();
    return suite;
}

inline std::vector<sihd::wifi::ies::Suite> consume_suites(FuzzedDataProvider & provider)
{
    std::vector<sihd::wifi::ies::Suite> suites(provider.ConsumeIntegralInRange<size_t>(0, 4));
    for (sihd::wifi::ies::Suite & suite : suites)
        suite = consume_suite(provider);
    return suites;
}

inline sihd::wifi::ies::Rsn consume_rsn(FuzzedDataProvider & provider)
{
    sihd::wifi::ies::Rsn rsn;
    rsn.version = provider.ConsumeIntegral<uint16_t>();
    rsn.group = consume_suite(provider);
    rsn.pairwise = consume_suites(provider);
    rsn.akms = consume_suites(provider);
    if (provider.ConsumeBool())
        rsn.capabilities = provider.ConsumeIntegral<uint16_t>();
    return rsn;
}

// parts: 0 group only, 1 +pairwise, 2 +akms, 3 +capabilities, 4 +one stray byte; `rsn` keeps what a parser reads back
inline Bytes encode_rsn(sihd::wifi::ies::Rsn & rsn, size_t parts)
{
    Bytes out;
    put_le(out, rsn.version, 2);
    put_suite(out, rsn.group);
    if (parts < 1)
        rsn.pairwise.clear();
    else
        put_suites(out, rsn.pairwise);
    if (parts < 2)
        rsn.akms.clear();
    else
        put_suites(out, rsn.akms);
    if (parts == 3 && rsn.capabilities.has_value())
        put_le(out, *rsn.capabilities, 2);
    if (parts == 4)
        out.push_back(0xa5);
    if (parts != 3)
        rsn.capabilities.reset();
    return out;
}

inline void check_rsn_equal(const sihd::wifi::ies::Rsn & got, const sihd::wifi::ies::Rsn & want)
{
    fuzz_check(got.version == want.version, "the version round trips");
    fuzz_check(got.group == want.group, "the group cipher round trips");
    fuzz_check(got.pairwise == want.pairwise, "the pairwise suites round trip");
    fuzz_check(got.akms == want.akms, "the akm suites round trip");
    fuzz_check(got.capabilities == want.capabilities, "the capabilities round trip");
}

} // namespace test

#endif
