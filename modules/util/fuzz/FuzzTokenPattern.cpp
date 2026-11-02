#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

#include <sihd/util/TokenPattern.hpp>

#include "fuzz_helper.hpp"
#include <fuzzer/FuzzedDataProvider.h>

using namespace sihd::util;

namespace
{
constexpr std::string_view previous_pattern = "keep {previous}";
constexpr std::string_view name_chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_";
constexpr size_t max_width = 4096;
constexpr size_t max_value_size = 32;
constexpr size_t max_reserve = 1024 * 1024;
constexpr std::array<size_t, 8> widths = {0, 1, 7, 4095, max_width, max_width + 1, 5000, 99999};
constexpr std::array<size_t, 5> field_counts = {1, 2, 3, max_reserve / max_width, max_reserve / max_width + 1};

bool is_align(char c)
{
    return c == '<' || c == '>' || c == '^';
}

bool consume_number(std::string_view & spec, size_t & number)
{
    const size_t digits = std::min(spec.find_first_not_of("0123456789"), spec.size());
    if (digits == 0)
        return false;
    if (std::from_chars(spec.data(), spec.data() + digits, number).ec != std::errc())
        return false;
    spec.remove_prefix(digits);
    return true;
}

bool parse_reference_spec(std::string_view spec, TokenPattern::Options & options)
{
    if (spec.size() >= 2 && is_align(spec[1]))
    {
        options.fill = spec[0];
        options.align = spec[1];
        spec.remove_prefix(2);
    }
    else if (!spec.empty() && is_align(spec[0]))
    {
        options.align = spec[0];
        spec.remove_prefix(1);
    }
    if (!spec.empty() && spec[0] >= '0' && spec[0] <= '9' && !consume_number(spec, options.width))
        return false;
    if (!spec.empty() && spec[0] == '.')
    {
        spec.remove_prefix(1);
        if (!consume_number(spec, options.precision))
            return false;
    }
    return spec.empty();
}

void check_field_options(const TokenPattern::Token & token)
{
    TokenPattern::Options expected;
    const bool classical = parse_reference_spec(token.raw_spec, expected);
    test::fuzz_check(token.classical == classical, "classical tells if the spec is [[fill]align][width][.precision]");
    test::fuzz_check(token.options.width <= max_width, "a field never keeps a width over the limit");
    if (!classical)
        return;
    test::fuzz_check(token.options.fill == expected.fill && token.options.align == expected.align,
                     "a classical spec sets the fill and the align");
    test::fuzz_check(token.options.width == expected.width && token.options.precision == expected.precision,
                     "a classical spec sets the width and the precision");
}

size_t check_field(std::string_view text, size_t cursor, const TokenPattern::Token & token)
{
    test::fuzz_check(token.offset == cursor && text[cursor] == '{', "a field offset is its opening brace");
    ++cursor;
    test::fuzz_check(!token.name.empty() && token.name.find_first_not_of(name_chars) == std::string_view::npos,
                     "a field name holds name characters");
    test::fuzz_check(text.substr(cursor, token.name.size()) == token.name, "a field name is the pattern text");
    cursor += token.name.size();
    if (text[cursor] == ':')
    {
        ++cursor;
        test::fuzz_check(text.substr(cursor, token.raw_spec.size()) == token.raw_spec,
                         "a raw spec is the pattern text");
        cursor += token.raw_spec.size();
    }
    test::fuzz_check(cursor < text.size() && text[cursor] == '}', "a field ends with a closing brace");
    check_field_options(token);
    return cursor + 1;
}

void check_tokens(const TokenPattern & pattern)
{
    const std::string_view text = pattern.pattern();
    size_t cursor = 0;
    size_t reserve_hint = 0;
    for (const TokenPattern::Token & token : pattern.tokens())
    {
        if (!token.name.empty())
        {
            cursor = check_field(text, cursor, token);
            reserve_hint += token.options.width;
            continue;
        }

        const bool escaped_brace = token.text == "{" || token.text == "}";
        test::fuzz_check(text.substr(cursor, token.text.size()) == token.text, "a text token is the pattern text");
        cursor += token.text.size();

        if (escaped_brace)
        {
            test::fuzz_check(cursor < text.size() && text[cursor] == token.text.front(),
                             "a brace in a text token comes from a doubled brace");
            ++cursor;
        }
        reserve_hint += token.text.size();
    }
    test::fuzz_check(cursor == text.size(), "the tokens cover the whole pattern");
    test::fuzz_check(pattern.reserve_hint() == reserve_hint, "reserve_hint sums the text and the widths");
}

std::string reference_render(const TokenPattern & pattern, std::string_view value)
{
    std::string rendered;
    for (const TokenPattern::Token & token : pattern.tokens())
    {
        if (token.name.empty())
        {
            rendered += token.text;
            continue;
        }
        const TokenPattern::Options & options = token.options;
        const std::string_view shown = options.precision != 0 ? value.substr(0, options.precision) : value;
        const size_t pad = options.width > shown.size() ? options.width - shown.size() : 0;
        const size_t left = options.align == '>' ? pad : (options.align == '^' ? pad / 2 : 0);
        rendered.append(left, options.fill);
        rendered += shown;
        rendered.append(pad - left, options.fill);
    }
    return rendered;
}

std::string render(const TokenPattern & pattern, std::string_view value)
{
    std::string rendered;
    pattern.render(rendered, [value](const TokenPattern::Token &) { return value; });
    return rendered;
}

std::string escape_braces(std::string_view text)
{
    std::string escaped;
    for (const char c : text)
    {
        if (c == '{' || c == '}')
            escaped += c;
        escaped += c;
    }
    return escaped;
}

struct BuiltField
{
        std::string spec;
        TokenPattern::Options options;
};

BuiltField consume_built_field(FuzzedDataProvider & provider)
{
    BuiltField built;
    if (provider.ConsumeBool())
    {
        built.options.fill = "*-_0 x"[provider.ConsumeIntegralInRange<size_t>(0, 5)];
        built.options.align = "<>^"[provider.ConsumeIntegralInRange<size_t>(0, 2)];
        built.spec += built.options.fill;
        built.spec += built.options.align;
    }
    else if (provider.ConsumeBool())
    {
        built.options.align = "<>^"[provider.ConsumeIntegralInRange<size_t>(0, 2)];
        built.spec += built.options.align;
    }

    built.options.width = provider.PickValueInArray(widths);
    if (built.options.width != 0 || provider.ConsumeBool())
        built.spec += std::to_string(built.options.width);

    if (provider.ConsumeBool())
    {
        built.options.precision = provider.ConsumeIntegralInRange<size_t>(1, 40);
        built.spec += '.' + std::to_string(built.options.precision);
    }
    return built;
}

void fuzz_built_fields(FuzzedDataProvider & provider)
{
    const BuiltField built = consume_built_field(provider);
    const size_t count = provider.PickValueInArray(field_counts);
    const test::ExactText value(provider.ConsumeBytesAsString(max_value_size));

    std::string text;
    for (size_t i = 0; i < count; ++i)
        text += "{f" + std::to_string(i) + ":" + built.spec + "}";

    TokenPattern pattern;
    test::fuzz_check(pattern.compile(previous_pattern).has_value(), "the previous pattern compiles");
    const bool compiled = pattern.compile(text).has_value();
    test::fuzz_check(compiled == (built.options.width <= max_width && built.options.width * count <= max_reserve),
                     "a field width and the reserve are accepted up to their limits");
    test::fuzz_check(pattern.pattern() == (compiled ? std::string_view(text) : previous_pattern),
                     "a refused compile keeps the previous pattern");
    check_tokens(pattern);
    if (!compiled)
        return;

    test::fuzz_check(pattern.tokens().size() == count, "every built field is a token");
    for (const TokenPattern::Token & token : pattern.tokens())
    {
        test::fuzz_check(token.classical && token.raw_spec == built.spec, "a built spec is classical");
        test::fuzz_check(token.options.fill == built.options.fill && token.options.align == built.options.align
                             && token.options.width == built.options.width
                             && token.options.precision == built.options.precision,
                         "a built spec gives back its parts");
    }
    if (count <= 3)
        test::fuzz_check(render(pattern, value) == reference_render(pattern, value),
                         "render pads and truncates the value");
}

void fuzz_raw(FuzzedDataProvider & provider)
{
    const size_t value_size = provider.ConsumeIntegralInRange<size_t>(0, max_value_size);
    const test::ExactText value(provider.ConsumeBytesAsString(value_size));
    const test::ExactText text(provider.ConsumeRemainingBytesAsString());

    TokenPattern pattern;
    test::fuzz_check(pattern.compile(previous_pattern).has_value(), "the previous pattern compiles");
    const bool compiled = pattern.compile(text).has_value();
    test::fuzz_check(pattern.pattern() == (compiled ? text.view() : previous_pattern),
                     "a refused compile keeps the previous pattern");
    check_tokens(pattern);
    test::fuzz_check(render(pattern, value) == reference_render(pattern, value), "render pads and truncates the value");

    const TokenPattern moved = std::move(pattern);
    check_tokens(moved);

    TokenPattern literal;
    test::fuzz_check(literal.compile(escape_braces(value)).has_value(), "a text with doubled braces compiles");
    test::fuzz_check(render(literal, "") == value.view(), "doubled braces render to the text");
}
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzedDataProvider provider(data, size);
    if (provider.ConsumeBool())
        fuzz_built_fields(provider);
    else
        fuzz_raw(provider);
    return 0;
}
