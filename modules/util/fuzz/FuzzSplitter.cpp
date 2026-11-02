#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <sihd/util/Splitter.hpp>

#include "fuzz_helper.hpp"
#include <fuzzer/FuzzedDataProvider.h>

using namespace sihd::util;

namespace
{
constexpr std::string_view delimiter_alphabet = ",;|ab \"'(";
constexpr int no_delimiter_kind = 6;
constexpr int delimiter_kinds = 8;

using DelimiterMatcher = std::function<size_t(std::string_view)>;

DelimiterMatcher text_matcher(std::string delimiter)
{
    return [delimiter = std::move(delimiter)](std::string_view rest) {
        return rest.starts_with(delimiter) ? delimiter.size() : size_t(0);
    };
}

DelimiterMatcher char_set_matcher(std::string chars)
{
    return [chars = std::move(chars)](std::string_view rest) {
        return chars.find(rest.front()) != std::string::npos ? size_t(1) : size_t(0);
    };
}

bool is_space(char c)
{
    return c == ' ' || (c >= '\t' && c <= '\r');
}

DelimiterMatcher space_matcher()
{
    return [](std::string_view rest) {
        return is_space(rest.front()) ? size_t(1) : size_t(0);
    };
}

std::vector<std::string_view>
    reference_tokens(std::string_view input, const DelimiterMatcher & matcher, bool keep_empty)
{
    std::vector<std::string_view> fields;
    size_t begin = 0;
    for (size_t pos = 0; pos < input.size();)
    {
        const size_t length = matcher(input.substr(pos));
        if (length == 0)
        {
            ++pos;
            continue;
        }
        fields.push_back(input.substr(begin, pos - begin));
        pos += length;
        begin = pos;
    }
    fields.push_back(input.substr(begin));

    if (keep_empty)
    {
        // an empty token needs a delimiter on its left: nothing stands before a leading delimiter
        if (fields.front().empty())
            fields.erase(fields.begin());
        return fields;
    }
    std::erase_if(fields, [](std::string_view field) { return field.empty(); });
    return fields;
}

bool is_delimiter_run(std::string_view gap, const DelimiterMatcher & matcher)
{
    while (!gap.empty())
    {
        const size_t length = matcher(gap);
        if (length == 0)
            return false;
        gap.remove_prefix(length);
    }
    return true;
}

// holds with enclosing sequences too: only delimiters may stand outside of the tokens
void check_tokens_cover_input(std::string_view input,
                              const DelimiterMatcher & matcher,
                              const std::vector<std::string_view> & tokens)
{
    const char *cursor = input.data();
    const char *input_end = input.data() + input.size();
    for (const std::string_view token : tokens)
    {
        test::fuzz_check(token.data() >= cursor && token.data() + token.size() <= input_end,
                         "tokens are ordered views of the input");
        test::fuzz_check(is_delimiter_run({cursor, static_cast<size_t>(token.data() - cursor)}, matcher),
                         "only delimiters stand between two tokens");
        cursor = token.data() + token.size();
    }
    test::fuzz_check(is_delimiter_run({cursor, static_cast<size_t>(input_end - cursor)}, matcher),
                     "only delimiters follow the last token");
}

void check_conflicting_options(FuzzedDataProvider & provider, const std::string & delimiter)
{
    const int mask = provider.PickValueInArray({3, 5, 6, 7});
    SplitterOptions options;
    if (mask & 1)
        options.delimiter_char = delimiter.front();
    if (mask & 2)
        options.delimiter_str = delimiter;
    if (mask & 4)
        options.delimiter_method = SplitterOptions::delimiter_spaces();

    bool refused = false;
    try
    {
        const Splitter splitter(options);
    }
    catch (const std::logic_error &)
    {
        refused = true;
    }
    test::fuzz_check(refused, "options with two delimiter types are refused");
}

Splitter make_splitter(int kind,
                       const SplitterOptions & base,
                       const std::string & delimiter,
                       const std::string & chars,
                       DelimiterMatcher & matcher)
{
    SplitterOptions options = base;
    switch (kind)
    {
        case 0:
        {
            Splitter splitter(options);
            if (delimiter.size() == 1)
                splitter.set_delimiter_char(delimiter.front());
            else
                splitter.set_delimiter(delimiter);
            matcher = text_matcher(delimiter);
            return splitter;
        }

        case 1:
            options.delimiter_char = delimiter.front();
            matcher = text_matcher(delimiter.substr(0, 1));
            return Splitter(options);
        case 2:
            options.delimiter_str = delimiter;
            matcher = text_matcher(delimiter);
            return Splitter(options);
        case 3:
        {
            options.delimiter_method = [chars](int c) {
                return chars.find(static_cast<char>(c)) != std::string::npos ? 1 : 0;
            };
            matcher = char_set_matcher(chars);
            return Splitter(options);
        }

        case 4:
            options.delimiter_method = SplitterOptions::delimiter_spaces();
            matcher = space_matcher();
            return Splitter(options);
        case 5:
        {
            Splitter splitter(options);
            splitter.set_delimiter_spaces();
            matcher = space_matcher();
            return splitter;
        }

        default:
            matcher = [](std::string_view) {
                return size_t(0);
            };
            return Splitter(options);
    }
}
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzedDataProvider provider(data, size);
    const std::string delimiter = test::consume_text(provider, delimiter_alphabet, 1, 3);
    const std::string chars = test::consume_text(provider, delimiter_alphabet, 1, 3);
    const int kind = provider.ConsumeIntegralInRange<int>(0, delimiter_kinds - 1);
    const int escape = provider.ConsumeIntegralInRange<int>(0, 127);
    const std::string sequences = provider.ConsumeRandomLengthString(4);
    const int sequences_mode = provider.ConsumeIntegralInRange<int>(0, 2);
    const bool empty_delimitations = provider.ConsumeBool();
    if (kind == delimiter_kinds - 1)
    {
        check_conflicting_options(provider, delimiter);
        return 0;
    }
    // exact size heap buffer: a read past the end of the view is an out of bounds access
    const test::ExactText exact_input(provider.ConsumeRemainingBytesAsString());
    const std::string_view input = exact_input.view();

    DelimiterMatcher matcher;
    Splitter splitter = make_splitter(kind,
                                      {.escape_char = escape, .empty_delimitations = empty_delimitations},
                                      delimiter,
                                      chars,
                                      matcher);
    if (sequences_mode == 1)
        splitter.set_open_escape_sequences(sequences);
    else if (sequences_mode == 2)
        splitter.set_escape_sequences_all();

    const std::vector<std::string_view> tokens = splitter.split_view(input);
    check_tokens_cover_input(input, matcher, tokens);
    const bool whole_input_token = kind == no_delimiter_kind && input.empty();
    if (whole_input_token)
        test::fuzz_check(tokens.size() == 1 && tokens[0].empty(), "without delimiter the input is the only token");
    else if (sequences_mode == 0)
        test::fuzz_check(tokens == reference_tokens(input, matcher, empty_delimitations),
                         "without sequences the tokens are the text between the delimiters");

    const std::vector<std::string> copies = splitter.split(input);
    test::fuzz_check(std::ranges::equal(copies, tokens), "split returns the text of split_view");
    test::fuzz_check(splitter.count_tokens(input) == static_cast<int>(tokens.size()),
                     "count_tokens counts the tokens of split_view");
    int index = 0;
    for (const std::string_view token : tokens)
        test::fuzz_check(splitter.next_token(input, &index) == token, "next_token walks the tokens of split_view");
    test::fuzz_check(splitter.next_token(input, &index).empty(), "next_token ends with the input");
    return 0;
}
