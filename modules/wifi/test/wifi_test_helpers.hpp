#ifndef __SIHD_WIFI_TEST_HELPERS_HPP__
#define __SIHD_WIFI_TEST_HELPERS_HPP__

#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include <sihd/util/Result.hpp>
#include <sihd/wifi/ies.hpp>

namespace test
{

template <typename T>
sihd::util::ErrorCode failure_of(const sihd::util::Result<T> & result)
{
    return result.has_value() ? sihd::util::ErrorCode::none : result.error().code;
}

inline std::vector<sihd::wifi::ies::Element> elements_of(sihd::util::ArrUByteView bytes)
{
    const sihd::util::Result<std::vector<sihd::wifi::ies::Element>> elements = sihd::wifi::ies::elements(bytes);
    if (elements.has_value() == false)
    {
        ADD_FAILURE() << elements.error().message;
        return {};
    }
    return *elements;
}

inline std::vector<sihd::wifi::ies::Element> elements_of(const std::vector<uint8_t> & bytes)
{
    return elements_of(sihd::util::ArrUByteView(bytes));
}

} // namespace test

#endif
