#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <sihd/util/mime.hpp>

#include "fuzz_helper.hpp"
#include <fuzzer/FuzzedDataProvider.h>

using namespace sihd::util;

namespace
{
constexpr size_t max_mimes = 4;
constexpr std::array<std::string_view, 12> mime_pieces = {"text/plain",
                                                          "TEXT/Plain",
                                                          "text/html",
                                                          "image/png",
                                                          "*/*",
                                                          "application/json",
                                                          ";charset=utf-8",
                                                          ";q=0.5",
                                                          "+json",
                                                          "/",
                                                          " ",
                                                          ""};

// iequals compares through strncasecmp, which stops at a NUL byte
std::string consume_without_nul(FuzzedDataProvider & provider, size_t max_size)
{
    std::string text = provider.ConsumeRandomLengthString(max_size);
    std::ranges::replace(text, '\0', ' ');
    return text;
}

std::string consume_mime(FuzzedDataProvider & provider)
{
    std::string mime;
    for (size_t i = provider.ConsumeIntegralInRange<size_t>(0, 3); i > 0; --i)
    {
        const size_t index = provider.ConsumeIntegralInRange<size_t>(0, mime_pieces.size());
        mime += index < mime_pieces.size() ? std::string(mime_pieces[index]) : consume_without_nul(provider, 6);
    }
    return mime;
}

std::vector<std::string> consume_mimes(FuzzedDataProvider & provider)
{
    std::vector<std::string> mimes(provider.ConsumeIntegralInRange<size_t>(0, max_mimes));
    for (std::string & mime : mimes)
        mime = consume_mime(provider);
    return mimes;
}

char lower_ascii(char c)
{
    return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
}

bool reference_compatible(std::string_view a, std::string_view b)
{
    if (a.empty() || b.empty())
        return true;
    a = a.substr(0, a.find(';'));
    b = b.substr(0, b.find(';'));
    if (a.size() != b.size())
        return false;
    for (size_t i = 0; i < a.size(); ++i)
    {
        if (lower_ascii(a[i]) != lower_ascii(b[i]))
            return false;
    }
    return true;
}

void check_compatible(std::string_view a, std::string_view b)
{
    const bool result = mime::compatible(a, b);
    test::fuzz_check(result == reference_compatible(a, b), "compatible compares the type and subtype");
    test::fuzz_check(result == mime::compatible(b, a), "compatible is symmetric");
    test::fuzz_check(mime::compatible(a, a), "compatible is reflexive");
    if (!a.empty() && a.find(';') == std::string_view::npos)
        test::fuzz_check(mime::compatible(std::string(a) + ";charset=utf-8", b) == result, "parameters are ignored");
}

void check_match_offered(const std::vector<std::string> & offered, std::string_view wanted)
{
    const std::optional<std::string_view> matched = mime::match_offered(offered, wanted);
    for (const std::string & candidate : offered)
    {
        if (!reference_compatible(candidate, wanted))
            continue;
        test::fuzz_check(
            matched.has_value() && matched->data() == candidate.data() && matched->size() == candidate.size(),
            "match_offered returns the first compatible offer");
        return;
    }
    test::fuzz_check(!matched.has_value(), "match_offered finds nothing without a compatible offer");
}
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzedDataProvider provider(data, size);
    const std::vector<std::string> offered = consume_mimes(provider);
    const std::vector<std::string> wanted = consume_mimes(provider);

    for (const std::string & offer : offered)
    {
        for (const std::string & want : wanted)
            check_compatible(offer, want);
    }
    for (const std::string & want : wanted)
        check_match_offered(offered, want);

    const std::vector<std::string_view> wanted_views(wanted.begin(), wanted.end());
    const std::optional<std::string_view> best = mime::best_match(offered, wanted_views);
    for (const std::string_view want : wanted_views)
    {
        if (!mime::match_offered(offered, want).has_value())
            continue;
        test::fuzz_check(best.has_value() && best->data() == want.data() && best->size() == want.size(),
                         "best_match returns the first wanted mime with an offer");
        return 0;
    }
    test::fuzz_check(!best.has_value(), "best_match finds nothing without an offer");
    return 0;
}
