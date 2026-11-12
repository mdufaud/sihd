#include <ranges>
#include <string_view>
#include <utility>
#include <vector>

#include <fmt/format.h>

#include <sihd/bt/AssignedNumbers.hpp>
#include <sihd/bt/taxonomy.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/util/str.hpp>

#include "uuid.hpp"

namespace sihd::bt
{

using namespace sihd::util;

namespace
{

using Table = std::unordered_map<uint16_t, std::string>;

struct Entry
{
        size_t line;
        std::vector<std::pair<std::string_view, std::string_view>> fields;
};

std::unexpected<Error> line_error(const std::string & path, size_t line, std::string_view what)
{
    return std::unexpected(Error(ErrorCode::invalid_argument, "'{}' line {}: {}", path, line, what));
}

// a plain scalar, 'it''s' or "say \"hi\""
std::optional<std::string> scalar_from_text(std::string_view text)
{
    if (text.empty() || (text.front() != '\'' && text.front() != '"'))
        return std::string(text);
    const char quote = text.front();
    if (text.size() < 2 || text.back() != quote)
        return std::nullopt;

    const char escape = quote == '\'' ? '\'' : '\\';
    const std::string_view inner = text.substr(1, text.size() - 2);
    std::string value;
    for (size_t index = 0; index < inner.size(); ++index)
    {
        char character = inner[index];
        if (character == escape)
        {
            if (index + 1 == inner.size())
                return std::nullopt;
            character = inner[++index];
            if (character != quote && character != escape)
                return std::nullopt;
        }
        else if (character == quote)
            return std::nullopt;
        value += character;
    }
    return value;
}

// ponytail: reads the flat lists of the sig files only, a nested sig file calls for ryml
Result<std::vector<Entry>>
    entries_from_text(const std::string & path, std::string_view content, std::string_view list_key)
{
    const std::string list_line = fmt::format("{}:", list_key);
    std::vector<Entry> entries;
    bool listed = false;
    size_t number = 0;
    for (const auto & part : std::views::split(content, '\n'))
    {
        ++number;
        const std::string_view line = str::trim(std::string_view(part.begin(), part.end()));
        if (line.empty() || line.starts_with('#'))
            continue;
        if (!listed)
        {
            if (line != list_line)
                return line_error(path, number, fmt::format("expected '{}'", list_line));
            listed = true;
            continue;
        }

        std::string_view field = line;
        if (field.starts_with("- "))
        {
            entries.push_back(Entry {number, {}});
            field.remove_prefix(2);
        }
        const size_t colon = field.find(": ");
        if (entries.empty() || colon == std::string_view::npos)
            return line_error(path, number, "not a key: value of a flat list entry");
        entries.back().fields.emplace_back(field.substr(0, colon), str::trim(field.substr(colon + 2)));
    }
    if (entries.empty())
        return std::unexpected(Error(ErrorCode::invalid_argument, "'{}' holds no '{}' entry", path, list_line));
    return entries;
}

Result<Table> table_from_entries(const std::string & path, const std::vector<Entry> & entries, std::string_view id_key)
{
    Table table;
    for (const Entry & entry : entries)
    {
        std::optional<uint16_t> id;
        std::optional<std::string> name;
        for (const auto & [key, value] : entry.fields)
        {
            if (key == id_key)
            {
                const Result<uint16_t> parsed = str::convert_from_string<uint16_t>(value, 0);
                if (parsed)
                    id = *parsed;
            }
            else if (key == "name")
                name = scalar_from_text(value);
        }
        if (!id || !name)
            return line_error(path, entry.line, fmt::format("expected a 16 bits '{}' and a 'name'", id_key));
        table.insert_or_assign(*id, std::move(*name));
    }
    return table;
}

Result<Table> read_table(const std::string & path, std::string_view list_key, std::string_view id_key)
{
    const Result<std::string> content = sys::fs::read_all(path);
    SIHD_UNEXPECTED_RETURN(content);
    const Result<std::vector<Entry>> entries = entries_from_text(path, *content, list_key);
    SIHD_UNEXPECTED_RETURN(entries);
    return table_from_entries(path, *entries, id_key);
}

std::optional<std::string> name_from_table(const Table & table, uint16_t id)
{
    const auto it = table.find(id);
    if (it == table.end())
        return std::nullopt;
    return it->second;
}

// the sig name of the company owning the beacon format, as its company identifier or member uuid
std::string_view owner_from_beacon(taxonomy::Beacon beacon)
{
    switch (beacon)
    {
        case taxonomy::Beacon::swift_pair:
            return "Microsoft";
        case taxonomy::Beacon::eddystone:
        case taxonomy::Beacon::fast_pair:
            return "Google LLC";
        default:
            return "Apple, Inc.";
    }
}

} // namespace

AssignedNumbers::AssignedNumbers() = default;

Result<void> AssignedNumbers::load_company_identifiers(const std::string & path)
{
    Result<Table> table = read_table(path, "company_identifiers", "value");
    SIHD_UNEXPECTED_RETURN(table);
    _companies = std::move(*table);
    return {};
}

Result<void> AssignedNumbers::load_member_uuids(const std::string & path)
{
    Result<Table> table = read_table(path, "uuids", "uuid");
    SIHD_UNEXPECTED_RETURN(table);
    _members = std::move(*table);
    return {};
}

Result<void> AssignedNumbers::load_service_uuids(const std::string & path)
{
    Result<Table> table = read_table(path, "uuids", "uuid");
    SIHD_UNEXPECTED_RETURN(table);
    _services = std::move(*table);
    return {};
}

std::optional<std::string> AssignedNumbers::company(uint16_t id) const
{
    return name_from_table(_companies, id);
}

std::optional<std::string> AssignedNumbers::member(uint16_t uuid) const
{
    return name_from_table(_members, uuid);
}

std::optional<std::string> AssignedNumbers::service(uint16_t uuid) const
{
    return name_from_table(_services, uuid);
}

std::optional<std::string> AssignedNumbers::vendor(const ScanResult & result) const
{
    for (const auto & [id, data] : result.manufacturer_data)
    {
        if (std::optional<std::string> name = this->company(id))
            return name;
    }
    std::vector<std::string_view> uuids(result.uuids.begin(), result.uuids.end());
    for (const auto & [uuid, data] : result.service_data)
        uuids.emplace_back(uuid);
    for (const std::string_view uuid : uuids)
    {
        const std::optional<uint16_t> short_uuid = short_from_uuid(uuid);
        if (!short_uuid)
            continue;
        if (std::optional<std::string> name = this->member(*short_uuid))
            return name;
    }
    const std::optional<taxonomy::Beacon> beacon = taxonomy::beacon(result);
    if (!beacon)
        return std::nullopt;
    return std::string(owner_from_beacon(*beacon));
}

} // namespace sihd::bt
