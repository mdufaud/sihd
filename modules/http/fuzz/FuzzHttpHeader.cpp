#include <charconv>
#include <cstddef>
#include <cstdint>
#include <fuzz_helper.hpp>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <sihd/http/HttpHeader.hpp>
#include <sihd/util/str.hpp>

#include <fuzzer/FuzzedDataProvider.h>

using namespace sihd::http;
using sihd::util::str::to_upper_copy;

namespace
{
constexpr std::string_view name_chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-";
constexpr std::string_view padding_chars = " \t";
constexpr std::string_view quoted_chars = "abc 123;=,/.";
constexpr std::string_view digit_chars = "0123456789";
constexpr std::string_view space_chars = " \t\n\v\f\r";
constexpr std::string_view line_ends[] = {"", "\n", "\r\n"};

std::optional<size_t> expected_length(std::string_view value)
{
    const size_t first = value.find_first_not_of(space_chars);
    if (first == std::string_view::npos)
        return std::nullopt;
    const std::string_view digits = value.substr(first, value.find_last_not_of(space_chars) - first + 1);
    size_t length = 0;
    const char *end = digits.data() + digits.size();
    const auto [parsed_end, error] = std::from_chars(digits.data(), end, length);
    if (error != std::errc() || parsed_end != end)
        return std::nullopt;
    return length;
}

void fuzz_add_raw(FuzzedDataProvider & provider)
{
    const std::vector<char> line = provider.ConsumeRemainingBytes<char>();

    HttpHeader header;
    const bool added = header.add_header_from_str(test::view_of(line)).has_value();
    test::fuzz_check(header.headers().size() == static_cast<size_t>(added),
                     "a header line adds exactly one header, or none when refused");
}

void fuzz_add_round_trip(FuzzedDataProvider & provider)
{
    const std::string name = test::consume_text(provider, name_chars, 1, 12);
    const std::string value = "v" + provider.ConsumeRandomLengthString(48) + "v";
    const std::string line = name + test::consume_text(provider, padding_chars, 0, 2) + ":"
                             + test::consume_text(provider, padding_chars, 0, 2) + value
                             + std::string(
                                 line_ends[provider.ConsumeIntegralInRange<size_t>(0, std::size(line_ends) - 1)]);

    HttpHeader header;
    test::fuzz_check(header.add_header_from_str(line).has_value(), "a header line with a name and a value is accepted");
    test::fuzz_check(header.headers().size() == 1, "a header line adds one header");
    test::fuzz_check(header.find(name) == value, "an added header is found back with its value");
    test::fuzz_check(header.find(to_upper_copy(name)) == value, "header names are case insensitive");
    test::fuzz_check(header.get(name) == value, "get returns the added value");

    const std::string other = test::consume_word(provider, 1, 8);
    header.set_header(to_upper_copy(name), other);
    test::fuzz_check(header.headers().size() == 1, "setting a header again overwrites it");
    test::fuzz_check(header.find(name) == other, "the overwriting value is found back");

    header.remove_header(name);
    test::fuzz_check(header.headers().empty() && header.find(name).empty(), "a removed header is gone");
}

void fuzz_set_round_trip(FuzzedDataProvider & provider)
{
    const std::string name = provider.ConsumeRandomLengthString(24);
    const std::string value = provider.ConsumeRandomLengthString(48);

    HttpHeader header;
    header.set_header(name, value);
    test::fuzz_check(header.find(name) == value, "a set header is found back by the same name");
    test::fuzz_check(header.get(name) == value, "get returns the set value");
}

void fuzz_header_param_round_trip(FuzzedDataProvider & provider)
{
    std::string value = "text/plain";
    std::vector<std::pair<std::string, std::string>> params;
    for (size_t i = provider.ConsumeIntegralInRange<size_t>(1, 4); i > 0; --i)
    {
        std::string name = "p" + std::to_string(params.size()) + test::consume_word(provider, 0, 6);
        const bool quoted = provider.ConsumeBool();
        std::string param_value = quoted ? "q" + test::consume_text(provider, quoted_chars, 0, 12)
                                         : test::consume_word(provider, 1, 8);
        value += test::consume_text(provider, padding_chars, 0, 2) + ";"
                 + test::consume_text(provider, padding_chars, 0, 2) + name + "="
                 + (quoted ? "\"" + param_value + "\"" : param_value);
        params.emplace_back(std::move(name), std::move(param_value));
    }

    for (const auto & [name, expected] : params)
    {
        test::fuzz_check(header_param(value, name) == expected, "a parameter is found back with its value");
        test::fuzz_check(header_param(value, to_upper_copy(name)) == expected, "parameter names are case insensitive");
    }
    test::fuzz_check(!header_param(value, "absent").has_value(), "a missing parameter is not found");
}

void fuzz_header_param_raw(FuzzedDataProvider & provider)
{
    const std::string param = provider.ConsumeRandomLengthString(16);
    const std::vector<char> value = provider.ConsumeRemainingBytes<char>();

    const std::optional<std::string> found = header_param(test::view_of(value), param);
    test::fuzz_check(!found.has_value() || !found->empty(), "a found parameter is never empty");
}

void fuzz_content_length(FuzzedDataProvider & provider)
{
    const size_t length = provider.ConsumeIntegral<size_t>();
    HttpHeader header;
    header.set_content_length(length);
    test::fuzz_check(header.content_length() == length, "a set content length is read back");

    const std::string value = test::consume_text(provider, space_chars, 0, 2)
                              + test::consume_text(provider, digit_chars, 0, 22)
                              + (provider.ConsumeBool() ? provider.ConsumeRandomLengthString(2) : std::string())
                              + test::consume_text(provider, space_chars, 0, 2);
    header.set_header("content-length:", value);
    test::fuzz_check(header.content_length() == expected_length(value),
                     "a content length is a trimmed run of digits that fits size_t");
}
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzedDataProvider provider(data, size);
    switch (provider.ConsumeIntegralInRange<int>(0, 5))
    {
        case 0:
            fuzz_add_raw(provider);
            break;
        case 1:
            fuzz_add_round_trip(provider);
            break;
        case 2:
            fuzz_set_round_trip(provider);
            break;
        case 3:
            fuzz_header_param_round_trip(provider);
            break;
        case 4:
            fuzz_header_param_raw(provider);
            break;
        default:
            fuzz_content_length(provider);
            break;
    }
    return 0;
}
