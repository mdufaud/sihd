#include "HtmlScanner.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cstdint>
#include <utility>

#include <sihd/util/str.hpp>

using namespace sihd::util;

namespace sihd::http::htmlscan
{

namespace
{

constexpr std::string_view whitespace = " \t\n\v\f\r";
constexpr std::string_view attribute_name_end = " \t\n\v\f\r=>/";
constexpr std::string_view unquoted_value_end = " \t\n\v\f\r>";

struct ValueRead
{
        std::string_view value;
        size_t cursor = 0;
};

struct AttributeRead
{
        Attribute attribute;
        size_t cursor = 0;
};

size_t or_end(std::string_view markup, size_t found)
{
    return found == std::string_view::npos ? markup.size() : found;
}

constexpr std::array<std::pair<std::string_view, std::string_view>, 5> named_references = {{
    {"amp", "&"},
    {"lt", "<"},
    {"gt", ">"},
    {"quot", "\""},
    {"apos", "'"},
}};

// "#x10FFFF" is the longest reference body decoded
constexpr size_t max_reference_size = 8;

std::string to_utf8(uint32_t codepoint)
{
    if (codepoint < 0x80)
        return std::string(1, static_cast<char>(codepoint));
    // the lead byte marks the length, each continuation byte carries 6 bits
    constexpr std::array<uint32_t, 4> lead_marks {0x00, 0xC0, 0xE0, 0xF0};
    const int continuations = codepoint < 0x800 ? 1 : (codepoint < 0x10000 ? 2 : 3);
    std::string out(1, static_cast<char>(lead_marks[continuations] | (codepoint >> (6 * continuations))));
    for (int shift = 6 * (continuations - 1); shift >= 0; shift -= 6)
        out += static_cast<char>(0x80 | ((codepoint >> shift) & 0x3F));
    return out;
}

// the character of a "#123" or "#x7B" body, nullopt for NUL, a surrogate or past unicode
std::optional<uint32_t> numeric_reference(std::string_view body)
{
    if (body.size() < 2 || body[0] != '#')
        return std::nullopt;
    const bool hex = body[1] == 'x' || body[1] == 'X';
    const std::string_view digits = body.substr(hex ? 2 : 1);
    uint32_t codepoint = 0;
    const auto [end, ec] = std::from_chars(digits.data(), digits.data() + digits.size(), codepoint, hex ? 16 : 10);
    if (digits.empty() || ec != std::errc() || end != digits.data() + digits.size())
        return std::nullopt;
    if (codepoint == 0 || codepoint > 0x10FFFF || (codepoint >= 0xD800 && codepoint <= 0xDFFF))
        return std::nullopt;
    return codepoint;
}

// numeric references and the five xml named ones: nullopt for any other, which stays as written
std::optional<std::string> reference_text(std::string_view body)
{
    const auto named = std::ranges::find(named_references, body, [](const auto & ref) { return ref.first; });
    if (named != named_references.end())
        return std::string(named->second);
    const std::optional<uint32_t> codepoint = numeric_reference(body);
    if (!codepoint)
        return std::nullopt;
    return to_utf8(*codepoint);
}

std::string decode_references(std::string_view value)
{
    std::string decoded;
    decoded.reserve(value.size());
    size_t cursor = 0;
    while (cursor < value.size())
    {
        const size_t ampersand = value.find('&', cursor);
        decoded.append(value.substr(cursor, ampersand - cursor));
        if (ampersand == std::string_view::npos)
            break;

        const size_t semicolon = value.substr(ampersand + 1, max_reference_size + 1).find(';');
        const std::optional<std::string> text = semicolon == std::string_view::npos
                                                    ? std::nullopt
                                                    : reference_text(value.substr(ampersand + 1, semicolon));
        decoded.append(text.value_or("&"));
        cursor = text ? ampersand + semicolon + 2 : ampersand + 1;
    }
    return decoded;
}

// reads the value following an attribute name, nullopt when a quote never closes
std::optional<ValueRead> read_value(std::string_view markup, size_t cursor)
{
    size_t value_cursor = or_end(markup, markup.find_first_not_of(whitespace, cursor));
    if (value_cursor >= markup.size() || markup[value_cursor] != '=')
        return ValueRead {{}, cursor};

    value_cursor = or_end(markup, markup.find_first_not_of(whitespace, value_cursor + 1));
    if (value_cursor < markup.size() && (markup[value_cursor] == '"' || markup[value_cursor] == '\''))
    {
        const size_t value_end = markup.find(markup[value_cursor], value_cursor + 1);
        if (value_end == std::string_view::npos)
            return std::nullopt;
        return ValueRead {markup.substr(value_cursor + 1, value_end - value_cursor - 1), value_end + 1};
    }

    const size_t value_end = or_end(markup, markup.find_first_of(unquoted_value_end, value_cursor));
    return ValueRead {markup.substr(value_cursor, value_end - value_cursor), value_end};
}

std::optional<AttributeRead> read_attribute(std::string_view markup, size_t cursor)
{
    const size_t name_end = or_end(markup, markup.find_first_of(attribute_name_end, cursor + 1));

    AttributeRead read;
    read.attribute.name = str::to_lower_copy(markup.substr(cursor, name_end - cursor));

    const std::optional<ValueRead> value = read_value(markup, name_end);
    if (!value)
        return std::nullopt;
    read.attribute.value = decode_references(value->value);
    read.cursor = value->cursor;
    return read;
}

// reads the attributes up to the closing '>', nullopt when the tag never closes
std::optional<size_t> read_attributes(std::string_view markup, size_t cursor, Tag & tag)
{
    while (true)
    {
        cursor = markup.find_first_not_of(" \t\n\v\f\r/", cursor);
        if (cursor == std::string_view::npos)
            return std::nullopt;
        if (markup[cursor] == '>')
            return cursor + 1;

        std::optional<AttributeRead> read = read_attribute(markup, cursor);
        if (!read)
            return std::nullopt;
        tag.attributes.push_back(std::move(read->attribute));
        cursor = read->cursor;
    }
}

size_t tag_name_end(std::string_view markup, size_t cursor)
{
    while (cursor < markup.size() && std::isalnum(static_cast<unsigned char>(markup[cursor])) != 0)
        ++cursor;
    return cursor;
}

} // namespace

std::optional<std::string_view> Tag::attribute(std::string_view attribute_name) const
{
    for (const Attribute & attr : attributes)
    {
        if (attr.name == attribute_name)
            return attr.value;
    }
    return std::nullopt;
}

void scan_tags(std::string_view markup, const std::function<void(const Tag &)> & on_tag)
{
    size_t pos = 0;
    while ((pos = markup.find('<', pos)) != std::string_view::npos)
    {
        const size_t name_start = pos + 1;
        const size_t name_end = tag_name_end(markup, name_start);
        if (name_end == name_start)
        {
            pos = name_end;
            continue;
        }

        Tag tag;
        tag.name = str::to_lower_copy(markup.substr(name_start, name_end - name_start));

        const std::optional<size_t> tag_end = read_attributes(markup, name_end, tag);
        if (!tag_end)
            return;

        const size_t text_end = markup.find('<', *tag_end);
        tag.trailing_text = markup.substr(*tag_end,
                                          text_end == std::string_view::npos ? text_end : text_end - *tag_end);
        pos = *tag_end;
        on_tag(tag);
    }
}

} // namespace sihd::http::htmlscan
