#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <sihd/util/Regex.hpp>

#include "fuzz_helper.hpp"
#include <fuzzer/FuzzedDataProvider.h>

using namespace sihd::util;

namespace
{
// pcre2 ends a catastrophic backtracking at its match limit: small inputs keep reaching it cheap
constexpr size_t max_pattern_size = 40;
constexpr size_t max_subject_size = 64;
constexpr std::string_view regex_metacharacters = "\\^$.[]|()?*+{}";
constexpr std::string_view literal_alphabet = "ab.*+?()[]{}|^$\\ x";
constexpr std::string_view replacement_alphabet = "ab.*\\ x";
constexpr std::array<std::string_view, 5> replacements = {"", "<$0>", "$1$2", "${1}x", "$"};

std::string consume_text(FuzzedDataProvider & provider, std::string_view alphabet, size_t min_size, size_t max_size)
{
    std::string text(provider.ConsumeIntegralInRange<size_t>(min_size, max_size), '\0');
    for (char & c : text)
        c = alphabet[provider.ConsumeIntegralInRange<size_t>(0, alphabet.size() - 1)];
    return text;
}

std::string escape_literal(std::string_view literal)
{
    std::string pattern;
    for (const char c : literal)
    {
        if (regex_metacharacters.find(c) != std::string_view::npos)
            pattern += '\\';
        pattern += c;
    }
    return pattern;
}

std::vector<size_t> find_occurrences(std::string_view subject, std::string_view literal)
{
    std::vector<size_t> positions;
    for (size_t pos = subject.find(literal); pos != std::string_view::npos; pos = subject.find(literal, pos))
    {
        positions.push_back(pos);
        pos += literal.size();
    }
    return positions;
}

std::string replace_occurrences(std::string_view subject,
                                std::string_view literal,
                                std::string_view replacement,
                                const std::vector<size_t> & positions)
{
    std::string replaced;
    size_t from = 0;
    for (const size_t pos : positions)
    {
        replaced.append(subject.substr(from, pos - from));
        replaced.append(replacement);
        from = pos + literal.size();
    }
    replaced.append(subject.substr(from));
    return replaced;
}

void check_compile_error(const Error & error, std::string_view pattern)
{
    test::fuzz_check(error.code == ErrorCode::invalid_argument, "a refused pattern is an invalid argument");

    const std::string prefix = "invalid regex '" + std::string(pattern) + "': ";
    const size_t offset_pos = error.message.rfind(" at offset ");
    test::fuzz_check(error.message.starts_with(prefix) && offset_pos != std::string::npos && offset_pos > prefix.size(),
                     "a refused pattern is explained by pcre between the pattern and its offset");
}

void fuzz_raw(FuzzedDataProvider & provider)
{
    const size_t pattern_size = provider.ConsumeIntegralInRange<size_t>(0, max_pattern_size);
    const Regex::Mode mode = provider.ConsumeBool() ? Regex::Mode::full : Regex::Mode::search;
    const size_t replacement_index = provider.ConsumeIntegralInRange<size_t>(0, replacements.size());
    const test::ExactText pattern(provider.ConsumeBytesAsString(pattern_size));
    const test::ExactText subject(provider.ConsumeBytesAsString(max_subject_size));
    const std::string replacement = replacement_index < replacements.size()
                                        ? std::string(replacements[replacement_index])
                                        : std::string(subject.view());

    Regex regex;
    const Result<void> compiled = regex.compile(pattern, mode);
    if (!compiled.has_value())
    {
        check_compile_error(compiled.error(), pattern.view());
        return;
    }

    const Result<bool> matched = regex.matches(subject);
    const Result<std::vector<std::string>> found = regex.find_all(subject);
    if (matched.has_value() && found.has_value())
    {
        test::fuzz_check(*matched == !found->empty(), "matches agrees with find_all");
        const bool resets_match = pattern.view().find("\\K") != std::string_view::npos
                                  || pattern.view().find("(*") != std::string_view::npos;
        if (mode == Regex::Mode::full && !resets_match && !found->empty())
            test::fuzz_check(found->front() == subject.view(), "a full match covers the whole subject");
    }

    const Result<std::string> replaced = regex.replace_all(subject, replacement);
    if (matched.has_value() && !*matched && replaced.has_value())
        test::fuzz_check(*replaced == subject.view(), "without a match the subject is left unchanged");

    const Result<std::string> erased = regex.replace_all(subject, "");
    if (erased.has_value())
        test::fuzz_check(erased->size() <= subject.view().size(), "erasing the matches never grows the subject");
}

void fuzz_literal(FuzzedDataProvider & provider)
{
    const std::string literal = consume_text(provider, literal_alphabet, 1, 8);
    const std::string replacement = consume_text(provider, replacement_alphabet, 0, 8);
    std::string subject_text;
    for (size_t i = provider.ConsumeIntegralInRange<size_t>(0, 12); i > 0; --i)
        subject_text += provider.ConsumeBool() ? literal : consume_text(provider, literal_alphabet, 0, 4);
    const test::ExactText subject(subject_text);
    const std::string pattern = escape_literal(literal);

    Regex search;
    Regex full;
    test::fuzz_check(search.compile(pattern).has_value() && full.compile(pattern, Regex::Mode::full).has_value(),
                     "an escaped literal compiles");

    const std::vector<size_t> positions = find_occurrences(subject, literal);
    const Result<bool> matched = search.matches(subject);
    test::fuzz_check(matched.has_value() && *matched == !positions.empty(), "matches finds a literal like find");

    const Result<std::vector<std::string>> found = search.find_all(subject);
    test::fuzz_check(found.has_value() && *found == std::vector<std::string>(positions.size(), literal),
                     "find_all returns each non overlapping literal");

    const Result<std::string> replaced = search.replace_all(subject, replacement);
    test::fuzz_check(replaced.has_value() && *replaced == replace_occurrences(subject, literal, replacement, positions),
                     "replace_all of a literal equals a find and replace loop");

    const Result<bool> full_matched = full.matches(subject);
    test::fuzz_check(full_matched.has_value() && *full_matched == (subject.view() == literal),
                     "a full match is an equality");
}
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzedDataProvider provider(data, size);
    switch (provider.ConsumeIntegralInRange<int>(0, 1))
    {
        case 0:
            fuzz_raw(provider);
            break;
        default:
            fuzz_literal(provider);
            break;
    }
    return 0;
}
