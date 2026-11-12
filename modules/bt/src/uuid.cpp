#include "uuid.hpp"

#include <vector>

#include <sihd/util/str.hpp>

namespace sihd::bt
{

using namespace sihd::util;

std::optional<uint16_t> short_from_uuid(std::string_view uuid)
{
    constexpr size_t uuid_size = 36;
    constexpr std::string_view base_prefix = "0000";
    constexpr std::string_view base_suffix = "-0000-1000-8000-00805f9b34fb";
    if (uuid.size() != uuid_size || !uuid.starts_with(base_prefix)
        || !str::iequals(uuid.substr(base_prefix.size() + 4), base_suffix))
        return std::nullopt;
    const Result<std::vector<uint8_t>> bytes = str::from_hex(uuid.substr(base_prefix.size(), 4));
    if (!bytes)
        return std::nullopt;
    return static_cast<uint16_t>(((*bytes)[0] << 8) | (*bytes)[1]);
}

} // namespace sihd::bt
