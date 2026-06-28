#ifndef __SIHD_UTIL_FMT_HPP__
#define __SIHD_UTIL_FMT_HPP__

#include <fmt/format.h>
#include <fmt/ranges.h>

#include "sihd/util/Array.hpp"
#include "sihd/util/ArrayView.hpp"

// Formatter specialization for Array<char> to format as string instead of range
template <>
struct fmt::formatter<sihd::util::Array<char>>: fmt::formatter<std::string>
{
        template <typename FormatContext>
        auto format(const sihd::util::Array<char> & arr, FormatContext & ctx) const
        {
            return fmt::formatter<std::string>::format(arr.cpp_str(), ctx);
        }
};

// Formatter specialization for Array<char> to format as string instead of range
template <>
struct fmt::formatter<sihd::util::ArrayView<char>>: fmt::formatter<std::string>
{
        template <typename FormatContext>
        auto format(const sihd::util::ArrayView<char> & arr, FormatContext & ctx) const
        {
            return fmt::formatter<std::string>::format(arr.cpp_str(), ctx);
        }
};

#endif