#include <fnmatch.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>
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
constexpr std::array<std::string_view, 16> glob_class_pieces =
    {"a", "b", "x", "*", "?", "[", "]", "!", "-", "\\", "[:alpha:]", "[:digit:]", "[:nope:]", "[a-b]", "[!a]", "[]]"};
constexpr std::string_view glob_class_text_alphabet = "abxA1-]!\\[";
constexpr std::string_view enclosing_alphabet = "ab'\"()[]{}\\ ";

void fuzz_base64(FuzzedDataProvider & provider)
{
    const std::vector<uint8_t> bytes = provider.ConsumeBytes<uint8_t>(provider.ConsumeIntegralInRange<size_t>(0, 256));
    const std::string encoded = str::to_base64(bytes.data(), bytes.size());
    test::fuzz_check(encoded.size() == ((bytes.size() + 2) / 3) * 4, "base64 pads to whole groups");
    const auto decoded = str::from_base64(encoded);
    test::fuzz_check(decoded.has_value(), "base64 output decodes");
    test::fuzz_check(*decoded == bytes, "base64 round trips");

    const test::ExactText raw(provider.ConsumeRemainingBytesAsString());
    (void)str::from_base64(raw);
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

    (void)str::to_unsigned(test::ExactText(provider.ConsumeRandomLengthString(32)), base);
    (void)str::to_signed(test::ExactText(provider.ConsumeRandomLengthString(32)), base);
    (void)str::to_double(test::ExactText(provider.ConsumeRandomLengthString(32)));
    (void)str::to_bool(test::ExactText(provider.ConsumeRandomLengthString(8)));
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

std::string consume_class_pattern(FuzzedDataProvider & provider)
{
    std::string pattern;
    for (size_t i = provider.ConsumeIntegralInRange<size_t>(0, 8); i > 0; --i)
        pattern += glob_class_pieces[provider.ConsumeIntegralInRange<size_t>(0, glob_class_pieces.size() - 1)];
    return pattern;
}

// a random text rarely matches: most of a text taken from its pattern stands a chance
std::string consume_text_like(FuzzedDataProvider & provider, std::string_view pattern)
{
    std::string text;
    for (const char c : pattern)
    {
        const int action = provider.ConsumeIntegralInRange<int>(0, 3);
        if (action == 0)
            text += glob_class_text_alphabet[provider.ConsumeIntegralInRange<size_t>(
                0,
                glob_class_text_alphabet.size() - 1)];
        else if (action != 1)
            text += c;
    }
    return text;
}

// posix leaves these undefined and the libcs differ
bool undefined_for_fnmatch(std::string_view pattern)
{
    const size_t last_kept = pattern.find_last_not_of('\\');
    const size_t escapes = pattern.size() - (last_kept == std::string_view::npos ? 0 : last_kept + 1);
    const bool range_cut_by_the_end = pattern.ends_with('-') && pattern.contains('[');
    return escapes % 2 == 1 || range_cut_by_the_end || pattern.contains("[:nope:]") || pattern.contains("-[");
}

void fuzz_glob_classes(FuzzedDataProvider & provider)
{
    const std::string pattern = consume_class_pattern(provider);
    const std::string text = provider.ConsumeBool() ? consume_text_like(provider, pattern)
                                                    : consume_text(provider, glob_class_text_alphabet, 12);

    if (undefined_for_fnmatch(pattern))
    {
        (void)str::glob_match(text, pattern, provider.ConsumeBool());
        return;
    }
    test::fuzz_check(str::glob_match(text, pattern) == (::fnmatch(pattern.c_str(), text.c_str(), 0) == 0),
                     "glob_match agrees with fnmatch");
    test::fuzz_check(
        str::glob_match(text, pattern, true) == (::fnmatch(pattern.c_str(), text.c_str(), FNM_CASEFOLD) == 0),
        "glob_match with ignore_case agrees with fnmatch");
}

std::string reference_replace(std::string_view text, std::string_view from, std::string_view to)
{
    if (from.empty())
        return std::string(text);
    std::string replaced;
    size_t begin = 0;
    for (size_t hit = text.find(from); hit != std::string_view::npos; hit = text.find(from, begin))
    {
        replaced.append(text.substr(begin, hit - begin));
        replaced.append(to);
        begin = hit + from.size();
    }
    replaced.append(text.substr(begin));
    return replaced;
}

void fuzz_replace(FuzzedDataProvider & provider)
{
    const test::ExactText text(consume_text(provider, "abx ", 32));
    const test::ExactText from(consume_text(provider, "abx", 3));
    const test::ExactText to(consume_text(provider, "abx-", 4));

    const std::string expected = reference_replace(text, from, to);
    test::fuzz_check(str::replace(text, from, to) == expected, "replace rewrites each non overlapping occurrence");
    test::fuzz_check(str::replace(text, {{from.view(), to.view()}}) == expected, "a single pair replaces the same way");
}

void fuzz_wrap(FuzzedDataProvider & provider)
{
    const test::ExactText text(provider.ConsumeRandomLengthString(32));
    const test::ExactText end_with(provider.ConsumeRandomLengthString(4));
    const size_t max_width = provider.ConsumeIntegralInRange<size_t>(0, 40);

    const std::string wrapped = str::wrap(text, max_width, end_with);
    test::fuzz_check(wrapped.size() <= std::max(max_width, text.view().size()), "wrap never adds to a long text");
    if (max_width < end_with.view().size())
        test::fuzz_check(wrapped == end_with.view().substr(0, max_width), "wrap cuts an ending over the width");
    else if (text.view().size() < max_width - end_with.view().size())
        test::fuzz_check(wrapped == text.view(), "wrap keeps a short text");
    else
    {
        const size_t room = max_width - end_with.view().size();
        const size_t kept = room > end_with.view().size() ? room - end_with.view().size() : 0;
        test::fuzz_check(wrapped.size() == kept + end_with.view().size(), "wrap keeps room for the ending twice");
        test::fuzz_check(wrapped.ends_with(end_with.view()), "wrap ends a long text with its ending");
        test::fuzz_check(
            text.view().starts_with(std::string_view(wrapped).substr(0, wrapped.size() - end_with.view().size())),
            "wrap keeps the start of a long text");
    }
}

std::string without_spaces_and_hyphens(std::string_view text)
{
    std::string kept;
    for (const char c : text)
    {
        if (!is_space(c) && c != '-')
            kept += c;
    }
    return kept;
}

// words that fit the width: the layout is greedy, and only a newline of the text breaks a line early
void fuzz_word_wrap_layout(FuzzedDataProvider & provider)
{
    const size_t max_width = provider.ConsumeIntegralInRange<size_t>(1, 16);
    std::string text;
    std::string expected;
    size_t line_width = 0;
    for (size_t words = provider.ConsumeIntegralInRange<size_t>(1, 8); words > 0; --words)
    {
        if (!text.empty())
        {
            const size_t newlines = provider.ConsumeIntegralInRange<size_t>(0, 2);
            text += newlines == 0 ? std::string(1, ' ') : std::string(newlines, '\n');
            expected.append(newlines, '\n');
            line_width = newlines == 0 ? line_width : 0;
        }
        const std::string word = test::consume_text(provider, "ab", 1, max_width);
        text += word;

        if (line_width > 0 && line_width + 1 + word.size() > max_width)
        {
            expected += '\n';
            line_width = 0;
        }
        else if (line_width > 0)
        {
            expected += ' ';
            ++line_width;
        }
        expected += word;
        line_width += word.size();
    }
    test::fuzz_check(str::word_wrap(test::ExactText(text), max_width, false) == expected,
                     "word_wrap breaks a line at a newline or when the next word does not fit");
}

void fuzz_word_wrap(FuzzedDataProvider & provider)
{
    if (provider.ConsumeBool())
    {
        fuzz_word_wrap_layout(provider);
        return;
    }
    const test::ExactText text(consume_text(provider, "ab- \n\t", 64));
    const size_t max_width = provider.ConsumeIntegralInRange<size_t>(0, 16);
    const bool append_hyphen = provider.ConsumeBool();

    const std::string wrapped = str::word_wrap(text, max_width, append_hyphen);
    if (max_width == 0)
    {
        test::fuzz_check(wrapped.empty(), "word_wrap of no width is empty");
        return;
    }
    test::fuzz_check(without_spaces_and_hyphens(wrapped) == without_spaces_and_hyphens(text),
                     "word_wrap keeps the words in order");
    for (const std::string_view line : reference_split(wrapped, [](char c) { return c == '\n'; }))
        test::fuzz_check(line.size() <= max_width, "word_wrap fits each line in the width");
}

void fuzz_to_columns(FuzzedDataProvider & provider)
{
    std::vector<std::string> words(provider.ConsumeIntegralInRange<size_t>(0, 8));
    size_t biggest_word = 0;
    std::string letters;
    for (std::string & word : words)
    {
        word = consume_text(provider, "abx", 6);
        biggest_word = std::max(biggest_word, word.size());
        letters += word;
    }
    const size_t max_width = provider.ConsumeIntegralInRange<size_t>(0, 24);

    const std::vector<std::string> lines = str::to_columns(std::span<const std::string>(words), max_width);
    test::fuzz_check(lines.empty() == (words.empty() || biggest_word > max_width),
                     "to_columns finds a layout when every word fits");

    std::string shown;
    for (const std::string & line : lines)
    {
        test::fuzz_check(line.size() <= max_width, "to_columns fits each line in the width");
        shown += without_spaces_and_hyphens(line);
    }
    if (lines.empty())
        return;
    std::ranges::sort(shown);
    std::ranges::sort(letters);
    test::fuzz_check(shown == letters, "to_columns shows every word");
}

bool is_subsequence(std::string_view part, std::string_view whole)
{
    size_t matched = 0;
    for (const char c : whole)
    {
        if (matched < part.size() && part[matched] == c)
            ++matched;
    }
    return matched == part.size();
}

void fuzz_enclosing(FuzzedDataProvider & provider)
{
    const test::ExactText text(consume_text(provider, enclosing_alphabet, 32));
    const test::ExactText to_find(consume_text(provider, "ab", 3));
    const std::string enclosers = std::string(str::encloses_start()) + str::encloses_stop();
    const bool has_encloser = text.view().find_first_of(enclosers) != std::string_view::npos;

    const std::string removed = str::remove_enclosing(text);
    test::fuzz_check(is_subsequence(removed, text), "remove_enclosing only removes characters");
    test::fuzz_check(has_encloser || removed == text.view(), "remove_enclosing keeps a text without enclosing");

    const int found = str::find_str_not_enclosed(text, to_find);
    if (found >= 0)
        test::fuzz_check(text.view().substr(static_cast<size_t>(found)).starts_with(to_find.view()),
                         "find_str_not_enclosed returns an occurrence");
    if (!has_encloser && !to_find.view().empty())
    {
        const size_t expected = text.view().find(to_find.view());
        test::fuzz_check(found == (expected == std::string_view::npos ? -1 : static_cast<int>(expected)),
                         "without enclosing find_str_not_enclosed is find");
    }
}

constexpr std::string_view plain_alphabet("ab 12\0", 6);

void fuzz_enclose_index(FuzzedDataProvider & provider)
{
    const std::string_view openers = str::encloses_start();
    const std::string_view closers = str::encloses_stop();
    const size_t kind = provider.ConsumeIntegralInRange<size_t>(0, openers.size() - 1);
    const char open = openers[kind];
    const char close = closers[kind];
    const std::string prefix = test::consume_text(provider, plain_alphabet, 0, 4);
    const std::string content = test::consume_text(provider, plain_alphabet, 0, 6);
    const std::string suffix = test::consume_text(provider, plain_alphabet, 0, 4);
    const int start = static_cast<int>(prefix.size());
    const int after_close = start + 1 + static_cast<int>(content.size()) + 1;

    const test::ExactText closed(prefix + open + content + close + suffix);
    test::fuzz_check(str::stopping_enclose_index(closed, start) == after_close,
                     "an enclose ends after its closing char");
    test::fuzz_check(str::stopping_enclose_index(closed, start, std::string(1, open).c_str()) == after_close,
                     "an authorized opening char starts an enclose");
    const char unauthorized = openers[(kind + 1) % openers.size()];
    test::fuzz_check(str::stopping_enclose_index(closed, start, std::string(1, unauthorized).c_str()) == -1,
                     "an unauthorized opening char starts nothing");

    const test::ExactText open_ended(prefix + open + content);
    test::fuzz_check(str::stopping_enclose_index(open_ended, start) == -2, "an enclose never closed is -2");

    const test::ExactText escaped_close(prefix + open + content + '\\' + close + content + close + suffix);
    test::fuzz_check(
        str::stopping_enclose_index(escaped_close, start) == after_close + 2 + static_cast<int>(content.size()),
        "an escaped closing char does not end the enclose");
    const test::ExactText escaped_open(prefix + '\\' + open + content + close);
    test::fuzz_check(str::stopping_enclose_index(escaped_open, start + 1) == -1,
                     "an escaped opening char starts nothing");

    test::fuzz_check(str::stopping_enclose_index(closed, static_cast<int>(closed.view().size())) == -1,
                     "past the end there is no enclose");
    for (size_t i = 0; i < prefix.size(); ++i)
    {
        test::fuzz_check(str::stopping_enclose_index(closed, static_cast<int>(i)) == -1, "a plain char starts nothing");
    }
}

int digit_value(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'z')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'Z')
        return c - 'A' + 10;
    return std::numeric_limits<int>::max();
}

void fuzz_is_number(FuzzedDataProvider & provider)
{
    const uint16_t base = provider.ConsumeIntegralInRange<uint16_t>(2, 36);
    const test::ExactText text(provider.ConsumeBytesAsString(max_text_size));

    const bool expected = std::ranges::all_of(text.view(), [base](char c) {
        return is_space(c) || c == '-' || c == '+' || digit_value(c) < base;
    });
    test::fuzz_check(str::is_number(text, base) == expected, "is_number accepts spaces, signs and digits of the base");
}
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzedDataProvider provider(data, size);
    switch (provider.ConsumeIntegralInRange<int>(0, 14))
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
        case 6:
            fuzz_glob_classes(provider);
            break;
        case 7:
            fuzz_replace(provider);
            break;
        case 8:
            fuzz_wrap(provider);
            break;
        case 9:
            fuzz_word_wrap(provider);
            break;
        case 10:
            fuzz_to_columns(provider);
            break;
        case 11:
            fuzz_enclosing(provider);
            break;
        case 12:
            fuzz_is_number(provider);
            break;
        case 13:
            fuzz_text(provider);
            break;
        default:
            fuzz_enclose_index(provider);
            break;
    }
    return 0;
}
