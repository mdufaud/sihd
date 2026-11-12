#include "address.hpp"

#include <charconv>

#include <fmt/format.h>

namespace sihd::bt
{

using namespace sihd::util;

Result<uint64_t> parse_address(std::string_view text)
{
    uint64_t value = 0;
    bool valid = text.size() == 17;
    for (size_t i = 0; valid && i < 6; ++i)
    {
        const std::string_view digits = text.substr(i * 3, 2);
        uint8_t byte = 0;
        const auto [end, ec] = std::from_chars(digits.data(), digits.data() + digits.size(), byte, 16);
        valid = ec == std::errc() && end == digits.data() + digits.size() && (i == 5 || text[i * 3 + 2] == ':');
        value = (value << 8) | byte;
    }
    if (!valid)
        return std::unexpected(Error(ErrorCode::invalid_argument, "invalid bluetooth address: {}", text));
    return value;
}

std::string format_address(uint64_t address)
{
    return fmt::format("{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}",
                       (address >> 40) & 0xff,
                       (address >> 32) & 0xff,
                       (address >> 24) & 0xff,
                       (address >> 16) & 0xff,
                       (address >> 8) & 0xff,
                       address & 0xff);
}

} // namespace sihd::bt
