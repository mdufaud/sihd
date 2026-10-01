#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <sihd/http/HttpHeader.hpp>
#include <sihd/http/Multipart.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/str.hpp>

using sihd::util::Error;
using enum sihd::util::ErrorCode;

namespace sihd::http
{

SIHD_LOGGER;

bool Multipart::Part::is_file() const
{
    return path.empty() == false || filename.empty() == false;
}

namespace
{

constexpr size_t max_parts = 10000;

// a name or filename travels in a content-disposition header line: control bytes or
// quotes would let a part inject headers or break out of the quoted value
bool valid_field_identifier(std::string_view str)
{
    for (const char c : str)
    {
        if (c == '\r' || c == '\n' || c == '\0' || c == '"')
            return false;
    }
    return true;
}

struct Delimiter
{
        size_t data_end;
        size_t after_line;
        bool closing;
};

// transport padding is allowed between a boundary and its line ending
std::optional<size_t> skip_delimiter_end(std::string_view body, size_t pos)
{
    while (pos < body.size() && (body[pos] == ' ' || body[pos] == '\t'))
        ++pos;
    return sihd::util::str::starts_with(body.substr(pos), "\r\n") ? std::optional<size_t>(pos + 2) : std::nullopt;
}

// boundary-like bytes inside part data are not delimiters unless they end a line
std::optional<Delimiter> next_delimiter(std::string_view body, std::string_view delimiter, size_t from)
{
    const std::string marker = "\r\n" + std::string(delimiter);
    while (true)
    {
        const size_t pos = body.find(marker, from);
        if (pos == std::string_view::npos)
            return std::nullopt;

        const size_t after_boundary = pos + marker.size();
        if (sihd::util::str::starts_with(body.substr(after_boundary), "--"))
            return Delimiter {.data_end = pos, .after_line = after_boundary + 2, .closing = true};

        const std::optional<size_t> after_line = skip_delimiter_end(body, after_boundary);
        if (after_line.has_value())
            return Delimiter {.data_end = pos, .after_line = *after_line, .closing = false};

        from = after_boundary;
    }
}

// returns the offset of the part data, past its headers
std::optional<size_t> parse_part_headers(std::string_view body, size_t pos, Multipart::Part & part)
{
    HttpHeader headers;
    while (true)
    {
        const size_t eol = body.find("\r\n", pos);
        if (eol == std::string_view::npos)
            return std::nullopt;
        const std::string_view line = body.substr(pos, eol - pos);
        pos = eol + 2;
        if (line.empty())
            break;
        if (headers.add_header_from_str(line).has_value() == false)
            return std::nullopt;
    }

    const std::string_view disposition = headers.find("content-disposition");
    if (auto name = header_param(disposition, "name"); name.has_value())
        part.name = std::move(*name);
    if (auto filename = header_param(disposition, "filename"); filename.has_value())
        part.filename = std::move(*filename);
    part.content_type = std::string(headers.content_type().value_or(""));
    return pos;
}

} // namespace

bool Multipart::is_content_type(std::string_view content_type)
{
    const size_t params = content_type.find(';');
    if (params != std::string_view::npos)
        content_type = content_type.substr(0, params);
    return sihd::util::str::iequals(sihd::util::str::trim(content_type), "multipart/form-data");
}

std::optional<std::string> Multipart::boundary(std::string_view content_type)
{
    if (Multipart::is_content_type(content_type) == false)
        return std::nullopt;
    return header_param(content_type, "boundary");
}

std::expected<Multipart, sihd::util::Error> Multipart::parse(std::string_view body, std::string_view content_type)
{
    const std::optional<std::string> boundary = Multipart::boundary(content_type);
    if (boundary.has_value() == false)
        return std::unexpected(Error(invalid_argument, "no boundary in content type '{}'", content_type));

    const std::string delimiter = "--" + *boundary;

    // the opening delimiter must start a line, preamble or not
    size_t pos = body.find(delimiter);
    while (pos != std::string_view::npos && pos != 0 && body[pos - 1] != '\n')
        pos = body.find(delimiter, pos + 1);
    if (pos == std::string_view::npos)
        return std::unexpected(Error(invalid_argument, "boundary '{}' not found in body", delimiter));
    pos += delimiter.size();

    Multipart multipart;
    if (sihd::util::str::starts_with(body.substr(pos), "--"))
        return multipart;

    const std::optional<size_t> headers_pos = skip_delimiter_end(body, pos);
    if (headers_pos.has_value() == false)
        return std::unexpected(Error(invalid_argument, "malformed part headers"));
    pos = *headers_pos;

    while (true)
    {
        if (multipart._parts.size() >= max_parts)
            return std::unexpected(Error(overflow, "more than {} parts", max_parts));
        Multipart::Part part;
        const std::optional<size_t> data_pos = parse_part_headers(body, pos, part);
        if (data_pos.has_value() == false)
            return std::unexpected(Error(invalid_argument, "malformed part headers at offset {}", pos));
        pos = *data_pos;

        const std::optional<Delimiter> next = next_delimiter(body, delimiter, pos);
        if (next.has_value() == false)
            return std::unexpected(Error(invalid_argument, "no closing delimiter found"));
        part.data.assign(body.substr(pos, next->data_end - pos));
        multipart._parts.push_back(std::move(part));
        if (next->closing)
            return multipart;
        pos = next->after_line;
    }
}

void Multipart::add_field(std::string name, std::string value, std::string content_type)
{
    if (valid_field_identifier(name) == false)
    {
        SIHD_LOG(error, "Multipart: field name contains control characters");
        return;
    }
    Multipart::Part part;
    part.name = std::move(name);
    part.data = std::move(value);
    part.content_type = std::move(content_type);
    _parts.push_back(std::move(part));
}

void Multipart::add_file(std::string name, std::string path, std::string filename, std::string content_type)
{
    if (valid_field_identifier(name) == false
        || (filename.empty() == false && valid_field_identifier(filename) == false))
    {
        SIHD_LOG(error, "Multipart: file name contains control characters");
        return;
    }
    Multipart::Part part;
    part.name = std::move(name);
    part.path = std::move(path);
    part.filename = std::move(filename);
    part.content_type = std::move(content_type);
    _parts.push_back(std::move(part));
}

void Multipart::clear()
{
    _parts.clear();
}

bool Multipart::empty() const
{
    return _parts.empty();
}

const std::vector<Multipart::Part> & Multipart::parts() const
{
    return _parts;
}

std::optional<std::string_view> Multipart::value(std::string_view name) const
{
    for (const Multipart::Part & part : _parts)
    {
        if (part.name == name)
            return part.data;
    }
    return std::nullopt;
}

const Multipart::Part *Multipart::file(std::string_view name) const
{
    for (const Multipart::Part & part : _parts)
    {
        if (part.name == name && part.is_file())
            return &part;
    }
    return nullptr;
}

} // namespace sihd::http
