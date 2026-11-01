#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <fuzz_helper.hpp>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <sihd/http/navigator/CsrfExtractor.hpp>

#include <fuzzer/FuzzedDataProvider.h>

using namespace sihd::http;

namespace
{
constexpr std::string_view known_fields[] = {
    "_csrf",
    "authenticity_token",
    "__RequestVerificationToken",
    "_token",
    "csrf_token",
    "csrfmiddlewaretoken",
};
constexpr std::string_view junk_chars = "abc \n\"'=>/";

std::string quote(FuzzedDataProvider & provider, const std::string & value)
{
    switch (provider.ConsumeIntegralInRange<int>(0, 2))
    {
        case 0:
            return "\"" + value + "\"";
        case 1:
            return "'" + value + "'";
        default:
            return value;
    }
}

void fuzz_extract_raw(FuzzedDataProvider & provider)
{
    const bool use_hint = provider.ConsumeBool();
    const std::string hint = provider.ConsumeRandomLengthString(16);
    const std::vector<char> html = provider.ConsumeRemainingBytes<char>();

    const CsrfResult result = csrf_extract_from_html(test::view_of(html),
                                                     use_hint ? std::optional<std::string_view>(hint) : std::nullopt);
    if (result.value.empty())
        test::fuzz_check(result.field_name.empty(), "nothing found means no field name");
    else if (use_hint)
        test::fuzz_check(result.field_name == hint, "the hint is the field name of what was found");
    else
        test::fuzz_check(std::ranges::find(known_fields, result.field_name) != std::end(known_fields),
                         "without a hint only the known field names are found");
}

void fuzz_extract_structured(FuzzedDataProvider & provider)
{
    const bool known = provider.ConsumeBool();
    const std::string
        field = known
                    ? std::string(known_fields[provider.ConsumeIntegralInRange<size_t>(0, std::size(known_fields) - 1)])
                    : "f" + test::consume_word(provider, 1, 8);
    const bool use_hint = !known || provider.ConsumeBool();

    std::string html = test::consume_text(provider, junk_chars, 0, 8);
    std::string expected;
    for (size_t i = provider.ConsumeIntegralInRange<size_t>(1, 4); i > 0; --i)
    {
        const bool targeted = provider.ConsumeBool();
        const std::string name = targeted ? field : "d" + test::consume_word(provider, 1, 6);
        const std::string value = provider.ConsumeBool() ? test::consume_word(provider, 1, 8) : std::string();
        if (targeted && expected.empty())
            expected = value;
        html += "<input type=" + quote(provider, "hidden") + " name=" + quote(provider, name)
                + " value=" + quote(provider, value) + ">" + test::consume_text(provider, junk_chars, 0, 8);
    }

    const CsrfResult result = csrf_extract_from_html(html,
                                                     use_hint ? std::optional<std::string_view>(field) : std::nullopt);
    test::fuzz_check(result.value == expected, "the first non empty value of the field is found");
    test::fuzz_check(result.field_name == (expected.empty() ? std::string() : field),
                     "the field name is the one looked for");
}
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzedDataProvider provider(data, size);
    if (provider.ConsumeBool())
        fuzz_extract_structured(provider);
    else
        fuzz_extract_raw(provider);
    return 0;
}
