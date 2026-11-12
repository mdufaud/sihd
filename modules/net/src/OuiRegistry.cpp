#include <stdexcept>
#include <utility>
#include <vector>

#include <sihd/net/OuiRegistry.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/util/str.hpp>

namespace sihd::net
{

using namespace sihd::util;

namespace
{

constexpr uint8_t group_bit = 0x01;
constexpr uint8_t local_bit = 0x02;
constexpr size_t oui_size = 3;

// strips the separator after every two hexadecimal digits: "28-6F-B9" or "aa:bb:cc:dd:ee:ff"
std::optional<std::vector<uint8_t>> octets_from_text(std::string_view text, char separator)
{
    if (text.size() % 3 != 2)
        return std::nullopt;
    std::string digits;
    for (size_t index = 0; index < text.size(); ++index)
    {
        if (index % 3 != 2)
            digits += text[index];
        else if (text[index] != separator)
            return std::nullopt;
    }
    Result<std::vector<uint8_t>> octets = str::from_hex(digits);
    if (!octets)
        return std::nullopt;
    return std::move(*octets);
}

uint32_t key_from_octets(ArrUByteView octets)
{
    return (static_cast<uint32_t>(octets[0]) << 16) | (static_cast<uint32_t>(octets[1]) << 8) | octets[2];
}

} // namespace

OuiRegistry::OuiRegistry() = default;

OuiRegistry::OuiRegistry(const std::string & path)
{
    auto loaded = this->load(path);
    if (!loaded)
        throw std::runtime_error(loaded.error().message);
}

Result<void> OuiRegistry::load(const std::string & path)
{
    Result<std::vector<std::string>> lines = sys::fs::read_lines(path);
    SIHD_UNEXPECTED_RETURN(lines);

    // 28-6F-B9   (hex)\t\tNokia Shanghai Bell Co., Ltd.
    constexpr std::string_view hex_marker = "(hex)";
    std::unordered_map<uint32_t, std::string> vendors;
    for (const std::string & line : *lines)
    {
        const size_t marker = line.find(hex_marker);
        if (marker == std::string::npos)
            continue;
        const std::string_view prefix = str::trim(std::string_view(line).substr(0, marker));
        const std::optional<std::vector<uint8_t>> oui = octets_from_text(prefix, '-');
        if (!oui || oui->size() != oui_size)
            return std::unexpected(Error(ErrorCode::invalid_argument, "'{}': '{}' is not an oui", path, prefix));
        vendors[key_from_octets(*oui)] = str::trim(std::string_view(line).substr(marker + hex_marker.size()));
    }
    if (vendors.empty())
        return std::unexpected(Error(ErrorCode::invalid_argument, "'{}' holds no oui", path));
    _vendors = std::move(vendors);
    return {};
}

std::optional<std::string> OuiRegistry::vendor(ArrUByteView address) const
{
    if (address.size() < oui_size || (address[0] & (group_bit | local_bit)) != 0)
        return std::nullopt;
    const auto it = _vendors.find(key_from_octets(address));
    if (it == _vendors.end())
        return std::nullopt;
    return it->second;
}

std::optional<std::string> OuiRegistry::vendor(std::string_view text) const
{
    constexpr size_t mac_size = 6;
    const std::optional<std::vector<uint8_t>> address = octets_from_text(text, ':');
    if (!address || address->size() != mac_size)
        return std::nullopt;
    return this->vendor(ArrUByteView(*address));
}

} // namespace sihd::net
