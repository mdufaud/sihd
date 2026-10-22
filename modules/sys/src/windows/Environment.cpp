#include "internal/environment.hpp"

#include <algorithm>
#include <cctype>
#include <map>

#include <fmt/format.h>

namespace sihd::sys::internal
{

namespace
{

struct CaseInsensitiveLess
{
        bool operator()(const std::string & left, const std::string & right) const
        {
            return std::lexicographical_compare(left.begin(),
                                                left.end(),
                                                right.begin(),
                                                right.end(),
                                                [](char lhs, char rhs) {
                                                    return std::tolower(static_cast<unsigned char>(lhs))
                                                           < std::tolower(static_cast<unsigned char>(rhs));
                                                });
        }
};

} // namespace

std::string to_windows_block(const Environment & env)
{
    const std::map<std::string, std::string, CaseInsensitiveLess> sorted(env.entries().begin(), env.entries().end());
    std::string block;
    for (const auto & [key, value] : sorted)
    {
        block += fmt::format("{}={}", key, value);
        block.push_back('\0');
    }
    block.push_back('\0');
    return block;
}

} // namespace sihd::sys::internal
