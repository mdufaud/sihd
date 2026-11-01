#include <cstddef>
#include <cstdint>
#include <string>

#include <sihd/util/TokenPattern.hpp>

#include <fuzzer/FuzzedDataProvider.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzedDataProvider provider(data, size);
    const std::string pattern_text = provider.ConsumeRemainingBytesAsString();

    sihd::util::TokenPattern pattern;
    if (!pattern.compile(pattern_text).has_value())
        return 0;
    // reading every view lets the sanitizer check that the tokens point inside the pattern
    [[maybe_unused]] volatile char sink = 0;
    for (const sihd::util::TokenPattern::Token & token : pattern.tokens())
    {
        for (const char c : token.name)
            sink = c;
        for (const char c : token.text)
            sink = c;
        for (const char c : token.raw_spec)
            sink = c;
    }
    return 0;
}
