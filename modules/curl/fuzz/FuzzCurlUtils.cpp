#include <array>
#include <cstddef>
#include <cstdint>
#include <fuzz_helper.hpp>
#include <string>

#include <sihd/curl/utils.hpp>

#include <fuzzer/FuzzedDataProvider.h>

using namespace sihd::curl;

namespace
{
constexpr std::array<const char *, 3> bases = {"http://a/b/c/d;p?q",
                                               "https://user:pw@other.example:8443/x/y/?z#f",
                                               "localhost:3001/api/meta"};

std::string consume_word(FuzzedDataProvider & provider, size_t min_length, size_t max_length)
{
    std::string word(provider.ConsumeIntegralInRange<size_t>(min_length, max_length), 'a');
    for (char & c : word)
        c = provider.ConsumeIntegralInRange<char>('a', 'z');
    return word;
}

std::string consume_segment(FuzzedDataProvider & provider)
{
    switch (provider.ConsumeIntegralInRange<int>(0, 2))
    {
        case 0:
            return ".";
        case 1:
            return "..";
        default:
            return consume_word(provider, 0, 8);
    }
}

void fuzz_resolve(FuzzedDataProvider & provider)
{
    const std::string base = provider.ConsumeRandomLengthString(128);
    const std::string reference = provider.ConsumeRemainingBytesAsString();
    (void)resolve_url(base, reference);
}

void fuzz_absolute_reference(FuzzedDataProvider & provider)
{
    const std::string scheme = provider.ConsumeBool() ? "http" : "https";
    const std::string host = consume_word(provider, 1, 12);
    std::string reference = scheme + "://" + host;
    if (provider.ConsumeBool())
        reference += ':' + std::to_string(provider.ConsumeIntegralInRange<int>(1, 65535));
    for (size_t i = provider.ConsumeIntegralInRange<size_t>(0, 4); i > 0; --i)
        reference += '/' + consume_segment(provider);
    if (provider.ConsumeBool())
    {
        const std::string key = consume_word(provider, 1, 8);
        const std::string value = consume_word(provider, 0, 8);
        reference += '?' + key + '=' + value;
    }
    if (provider.ConsumeBool())
        reference += '#' + consume_word(provider, 0, 8);

    const auto expected = resolve_url(bases[0], reference);
    test::fuzz_check(expected.has_value(), "an absolute url resolves");
    test::fuzz_check(expected->starts_with(scheme + "://" + host),
                     "the resolved url keeps the scheme and host of the reference");
    for (const char *base : bases)
    {
        const auto resolved = resolve_url(base, reference);
        test::fuzz_check(resolved.has_value() && *resolved == *expected, "an absolute reference ignores its base");
    }
}
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzedDataProvider provider(data, size);
    if (provider.ConsumeBool())
        fuzz_absolute_reference(provider);
    else
        fuzz_resolve(provider);
    return 0;
}
