#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <sihd/util/Splitter.hpp>

#include "fuzz_helper.hpp"
#include <fuzzer/FuzzedDataProvider.h>

using namespace sihd::util;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzedDataProvider provider(data, size);
    const int delimiter = provider.ConsumeIntegralInRange<int>(1, 127);
    const int escape = provider.ConsumeIntegralInRange<int>(0, 127);
    const std::string sequences = provider.ConsumeRandomLengthString(4);
    const bool use_sequences = provider.ConsumeBool();
    const bool empty_delimitations = provider.ConsumeBool();
    // exact size heap buffer: a read past the end of the view is an out of bounds access
    const std::string remaining = provider.ConsumeRemainingBytesAsString();
    const std::vector<char> buffer(remaining.begin(), remaining.end());
    const std::string_view input(buffer.data(), buffer.size());

    Splitter splitter({.escape_char = escape,
                       .delimiter_char = delimiter,
                       .open_escape_sequences = use_sequences ? std::string_view(sequences) : std::string_view(),
                       .empty_delimitations = empty_delimitations});
    const std::vector<std::string_view> tokens = splitter.split_view(input);
    for (const std::string_view token : tokens)
        test::fuzz_check(
            token.empty()
                || (token.data() >= input.data() && token.data() + token.size() <= input.data() + input.size()),
            "a token is a view of the input");
    if (!use_sequences && escape != delimiter)
    {
        const std::vector<std::string> copies = splitter.split(input);
        test::fuzz_check(copies.size() == tokens.size(), "split and split_view find the same tokens");
        for (size_t i = 0; i < copies.size(); ++i)
            test::fuzz_check(copies[i] == tokens[i], "split and split_view return the same text");
    }
    return 0;
}
