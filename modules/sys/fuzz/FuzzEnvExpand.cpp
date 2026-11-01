#include <cstddef>
#include <cstdint>
#include <fuzz_helper.hpp>
#include <string>
#include <string_view>
#include <vector>

#include <sihd/sys/env.hpp>

#include <fuzzer/FuzzedDataProvider.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzedDataProvider provider(data, size);
    // exact size heap buffer: a read past the end of the view is an out of bounds access
    const std::vector<char> buffer = provider.ConsumeRemainingBytes<char>();
    const std::string_view text(buffer.data(), buffer.size());

    const std::string expanded = sihd::sys::env::expand(text);

    const size_t dollar = text.find('$');
    test::fuzz_check(expanded.starts_with(text.substr(0, dollar)), "the text before the first dollar is kept");
    if (dollar == std::string_view::npos)
        test::fuzz_check(expanded == text, "a text without dollar is unchanged");
    return 0;
}
