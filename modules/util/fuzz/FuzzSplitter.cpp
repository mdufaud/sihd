#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <sihd/util/Splitter.hpp>

#include "fuzz_helper.hpp"
#include <fuzzer/FuzzedDataProvider.h>

using namespace sihd::util;

namespace
{
constexpr std::string_view delimiter_alphabet = ",;|ab \"'(";

std::vector<std::string_view> reference_tokens(std::string_view input, std::string_view delimiter, bool keep_empty)
{
    std::vector<std::string_view> fields;
    size_t begin = 0;
    for (size_t hit = input.find(delimiter); hit != std::string_view::npos; hit = input.find(delimiter, begin))
    {
        fields.push_back(input.substr(begin, hit - begin));
        begin = hit + delimiter.size();
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

bool is_delimiter_run(std::string_view gap, std::string_view delimiter)
{
    while (gap.starts_with(delimiter))
        gap.remove_prefix(delimiter.size());
    return gap.empty();
}

// holds with enclosing sequences too: only delimiters may stand outside of the tokens
void check_tokens_cover_input(std::string_view input,
                              std::string_view delimiter,
                              const std::vector<std::string_view> & tokens)
{
    const char *cursor = input.data();
    const char *input_end = input.data() + input.size();
    for (const std::string_view token : tokens)
    {
        test::fuzz_check(token.data() >= cursor && token.data() + token.size() <= input_end,
                         "tokens are ordered views of the input");
        test::fuzz_check(is_delimiter_run({cursor, static_cast<size_t>(token.data() - cursor)}, delimiter),
                         "only delimiters stand between two tokens");
        cursor = token.data() + token.size();
    }
    test::fuzz_check(is_delimiter_run({cursor, static_cast<size_t>(input_end - cursor)}, delimiter),
                     "only delimiters follow the last token");
}
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzedDataProvider provider(data, size);
    const std::string delimiter = test::consume_text(provider, delimiter_alphabet, 1, 3);
    const int escape = provider.ConsumeIntegralInRange<int>(0, 127);
    const std::string sequences = provider.ConsumeRandomLengthString(4);
    const int sequences_mode = provider.ConsumeIntegralInRange<int>(0, 2);
    const bool empty_delimitations = provider.ConsumeBool();
    // exact size heap buffer: a read past the end of the view is an out of bounds access
    const test::ExactText exact_input(provider.ConsumeRemainingBytesAsString());
    const std::string_view input = exact_input.view();

    Splitter splitter({.escape_char = escape, .empty_delimitations = empty_delimitations});
    if (delimiter.size() == 1)
        splitter.set_delimiter_char(delimiter.front());
    else
        splitter.set_delimiter(delimiter);
    if (sequences_mode == 1)
        splitter.set_open_escape_sequences(sequences);
    else if (sequences_mode == 2)
        splitter.set_escape_sequences_all();

    const std::vector<std::string_view> tokens = splitter.split_view(input);
    check_tokens_cover_input(input, delimiter, tokens);
    if (sequences_mode == 0)
        test::fuzz_check(tokens == reference_tokens(input, delimiter, empty_delimitations),
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
