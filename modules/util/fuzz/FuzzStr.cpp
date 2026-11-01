#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <sihd/util/Result.hpp>
#include <sihd/util/num.hpp>
#include <sihd/util/str.hpp>

#include "fuzz_helper.hpp"
#include <fuzzer/FuzzedDataProvider.h>

namespace
{
using namespace sihd::util;

constexpr std::string_view
    random_charset = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz\n\t ()[]{}'123456789!@#$%^&*_+";
constexpr std::string_view glob_pattern_alphabet = "abx*?";
constexpr std::string_view glob_text_alphabet = "abx";
constexpr size_t max_text_size = 96;

void fuzz_base64(FuzzedDataProvider & provider)
{
    const std::vector<uint8_t> bytes = provider.ConsumeBytes<uint8_t>(provider.ConsumeIntegralInRange<size_t>(0, 256));
    const std::string encoded = str::to_base64(bytes.data(), bytes.size());
    test::fuzz_check(encoded.size() == ((bytes.size() + 2) / 3) * 4, "base64 pads to whole groups");
    const auto decoded = str::from_base64(encoded);
    test::fuzz_check(decoded.has_value(), "base64 output decodes");
    test::fuzz_check(*decoded == bytes, "base64 round trips");

    (void)str::from_base64(provider.ConsumeRemainingBytesAsString());
}

void fuzz_generate_random(FuzzedDataProvider & provider)
{
    const size_t length = provider.ConsumeIntegralInRange<size_t>(0, 4096);
    const std::string text = str::generate_random(length);
    test::fuzz_check(text.size() == length, "generate_random gives the requested size");
    for (const char c : text)
        test::fuzz_check(random_charset.find(c) != std::string_view::npos, "generate_random stays in its charset");
    test::fuzz_check(str::generate_random_printable(length).size() == length,
                     "generate_random_printable gives the requested size");
}

void fuzz_number_text(FuzzedDataProvider & provider)
{
    const uint64_t value = provider.ConsumeIntegral<uint64_t>();
    const uint16_t base = provider.ConsumeIntegralInRange<uint16_t>(2, 36);
    const std::string text = str::num_str(value, base);
    test::fuzz_check(!text.empty(), "num_str writes something in a valid base");
    test::fuzz_check(text.size() == num::size(value, base), "num::size matches the digits of num_str");
    const auto parsed = str::to_unsigned(text, base);
    test::fuzz_check(parsed.has_value() && *parsed == value, "num_str round trips through to_unsigned");

    (void)str::to_unsigned(provider.ConsumeRandomLengthString(32), base);
    (void)str::to_signed(provider.ConsumeRandomLengthString(32), base);
    (void)str::to_double(provider.ConsumeRandomLengthString(32));
    (void)str::to_bool(provider.ConsumeRandomLengthString(8));
}

std::string consume_text(FuzzedDataProvider & provider, std::string_view alphabet, size_t max_size)
{
    std::string text(provider.ConsumeIntegralInRange<size_t>(0, max_size), '\0');
    for (char & c : text)
        c = alphabet[provider.ConsumeIntegralInRange<size_t>(0, alphabet.size() - 1)];
    return text;
}

std::string converted(std::string_view text, auto convert)
{
    std::string result(text);
    for (char & c : result)
        c = static_cast<char>(convert(static_cast<unsigned char>(c)));
    return result;
}

int upper(int c)
{
    return std::toupper(c);
}

int lower(int c)
{
    return std::tolower(c);
}

bool is_space(char c)
{
    return std::isspace(static_cast<unsigned char>(c)) != 0;
}

std::string reference_hex(const std::vector<uint8_t> & bytes, char delimiter)
{
    std::string text;
    char digits[3];
    for (size_t i = 0; i < bytes.size(); ++i)
    {
        if (delimiter != '\0' && i > 0)
            text += delimiter;
        std::snprintf(digits, sizeof(digits), "%02x", bytes[i]);
        text += digits;
    }
    return text;
}

void fuzz_hex(FuzzedDataProvider & provider)
{
    const std::vector<uint8_t> bytes = test::consume_exact_bytes(provider,
                                                                 provider.ConsumeIntegralInRange<size_t>(0, 64));
    const char delimiter = provider.ConsumeIntegralInRange<char>(1, 127);
    const std::string plain = str::hexdump(bytes.data(), bytes.size(), '\0');
    test::fuzz_check(plain == reference_hex(bytes, '\0'), "hexdump writes two lowercase digits per byte");
    test::fuzz_check(str::hexdump(bytes.data(), bytes.size(), delimiter) == reference_hex(bytes, delimiter),
                     "hexdump puts the delimiter between the bytes");
    for (const std::string & text : {plain, converted(plain, upper)})
    {
        const Result<std::vector<uint8_t>> decoded = str::from_hex(text);
        test::fuzz_check(decoded.has_value() && *decoded == bytes, "from_hex reverses hexdump in either case");
    }

    const test::ExactText raw(provider.ConsumeBytesAsString(max_text_size));
    const bool only_digits = std::ranges::all_of(raw.view(), [](char c) {
        return std::isxdigit(static_cast<unsigned char>(c)) != 0;
    });
    const Result<std::vector<uint8_t>> parsed = str::from_hex(raw);
    test::fuzz_check(parsed.has_value() == (only_digits && raw.view().size() % 2 == 0),
                     "from_hex accepts exactly an even count of hex digits");
    if (parsed.has_value())
        test::fuzz_check(str::hexdump(parsed->data(), parsed->size(), '\0') == converted(raw.view(), lower),
                         "from_hex reads every digit");
}

bool reference_glob(std::string_view text, std::string_view pattern)
{
    std::vector<std::vector<bool>> matches(text.size() + 1, std::vector<bool>(pattern.size() + 1, false));
    matches[0][0] = true;
    for (size_t p = 1; p <= pattern.size(); ++p)
        matches[0][p] = matches[0][p - 1] && pattern[p - 1] == '*';
    for (size_t t = 1; t <= text.size(); ++t)
    {
        for (size_t p = 1; p <= pattern.size(); ++p)
        {
            if (pattern[p - 1] == '*')
                matches[t][p] = matches[t][p - 1] || matches[t - 1][p];
            else
                matches[t][p] = matches[t - 1][p - 1] && (pattern[p - 1] == '?' || pattern[p - 1] == text[t - 1]);
        }
    }
    return matches[text.size()][pattern.size()];
}

void fuzz_glob(FuzzedDataProvider & provider)
{
    const std::string pattern = consume_text(provider, glob_pattern_alphabet, 12);
    const std::string text = consume_text(provider, glob_text_alphabet, 24);
    const bool expected = reference_glob(text, pattern);
    test::fuzz_check(str::glob_match(text, pattern) == expected, "glob_match agrees with a reference for * and ?");
    test::fuzz_check(str::glob_match(converted(text, upper), pattern, true) == expected,
                     "ignore_case folds the case of the text");

    const test::ExactText raw_pattern(provider.ConsumeBytesAsString(32));
    const test::ExactText raw_text(provider.ConsumeBytesAsString(32));
    (void)str::glob_match(raw_text, raw_pattern, provider.ConsumeBool());
}

void fuzz_regex(FuzzedDataProvider & provider)
{
    const size_t max_size = provider.ConsumeIntegralInRange<size_t>(0, 80);
    const size_t pattern_size = provider.ConsumeIntegralInRange<size_t>(0, 40);
    const test::ExactText pattern(provider.ConsumeBytesAsString(pattern_size));
    const test::ExactText subject(provider.ConsumeBytesAsString(64));

    const Result<bool> matched = str::regex_match(subject, pattern, max_size);
    const Result<std::vector<std::string>> found = str::regex_search(subject, pattern, max_size);
    if (subject.view().size() > max_size)
        test::fuzz_check(!matched.has_value() && !found.has_value(), "a subject over max_size is rejected");
}

std::vector<std::string> reference_split(std::string_view text, auto is_delimiter)
{
    std::vector<std::string> tokens;
    std::string token;
    for (const char c : text)
    {
        if (!is_delimiter(c))
            token += c;
        else if (!token.empty())
            tokens.push_back(std::exchange(token, {}));
    }
    if (!token.empty())
        tokens.push_back(std::move(token));
    return tokens;
}

std::string_view reference_unquote(std::string_view text)
{
    while (!text.empty() && is_space(text.front()))
        text.remove_prefix(1);
    while (!text.empty() && is_space(text.back()))
        text.remove_suffix(1);
    if (text.size() >= 2 && (text.front() == '"' || text.front() == '\'') && text.back() == text.front())
        return text.substr(1, text.size() - 2);
    return text;
}

std::string reference_remove_escape(std::string_view text)
{
    std::string result;
    bool escaped = false;
    for (const char c : text)
    {
        if (!escaped && c == '\\')
        {
            escaped = true;
            continue;
        }
        result += c;
        escaped = false;
    }
    return result;
}

void fuzz_text(FuzzedDataProvider & provider)
{
    const char delimiter = provider.ConsumeIntegral<char>();
    const test::ExactText text(provider.ConsumeBytesAsString(max_text_size));

    const std::vector<std::string> tokens = str::split(text, delimiter);
    test::fuzz_check(tokens == reference_split(text, [delimiter](char c) { return c == delimiter; }),
                     "split cuts on the delimiter and drops the empty tokens");
    std::string expected_join;
    for (const std::string & token : tokens)
    {
        if (!expected_join.empty())
            expected_join += delimiter;
        expected_join += token;
    }
    const std::string joined = str::join(tokens, std::string_view(&delimiter, 1));
    test::fuzz_check(joined == expected_join, "join puts the delimiter between the tokens");
    test::fuzz_check(str::split(joined, delimiter) == tokens, "split reverses join");
    test::fuzz_check(str::split(text) == reference_split(text, is_space), "split without delimiter cuts on spaces");

    const std::string_view unquoted = str::unquote(text);
    test::fuzz_check(unquoted == reference_unquote(text), "unquote trims and strips one pair of quotes");
    test::fuzz_check(unquoted.empty()
                         || (unquoted.data() >= text.view().data()
                             && unquoted.data() + unquoted.size() <= text.view().data() + text.view().size()),
                     "unquote returns a view of its input");
    test::fuzz_check(str::remove_escape_char(text) == reference_remove_escape(text),
                     "remove_escape_char drops one escape in front of each character");
}
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzedDataProvider provider(data, size);
    switch (provider.ConsumeIntegralInRange<int>(0, 6))
    {
        case 0:
            fuzz_base64(provider);
            break;
        case 1:
            fuzz_generate_random(provider);
            break;
        case 2:
            fuzz_number_text(provider);
            break;
        case 3:
            fuzz_hex(provider);
            break;
        case 4:
            fuzz_glob(provider);
            break;
        case 5:
            fuzz_regex(provider);
            break;
        default:
            fuzz_text(provider);
            break;
    }
    return 0;
}
