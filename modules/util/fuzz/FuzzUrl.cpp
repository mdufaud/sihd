#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include <sihd/util/Url.hpp>

#include "fuzz_helper.hpp"
#include <fuzzer/FuzzedDataProvider.h>

namespace
{
using namespace sihd::util;

constexpr std::string_view url_chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

void fuzz_percent_encoding(FuzzedDataProvider & provider)
{
    const test::ExactText text(provider.ConsumeRandomLengthString(128));
    test::fuzz_check(Url::str_decode(Url::str_encode(text)) == text.view(), "percent encoding round trips");

    const test::ExactText encoded(provider.ConsumeRandomLengthString(128));
    (void)Url::str_decode(encoded);
}

void fuzz_decode(FuzzedDataProvider & provider)
{
    const test::ExactText text(provider.ConsumeRandomLengthString(256));
    const Url url = Url::decode(text);
    (void)url.encode();
}

void fuzz_structured_round_trip(FuzzedDataProvider & provider)
{
    Url url;
    url.scheme = "s" + test::consume_text(provider, url_chars, 0, 7);
    url.has_authority = true;
    url.host = test::consume_text(provider, url_chars, 1, 16);
    url.port = provider.ConsumeIntegralInRange<int>(1, 65535);
    url.path = "/" + test::consume_text(provider, url_chars, 0, 16);
    for (size_t i = provider.ConsumeIntegralInRange<size_t>(0, 4); i > 0; --i)
        url.query_params[test::consume_text(provider, url_chars, 1, 8)] = test::consume_text(provider, url_chars, 0, 8);
    url.fragment = test::consume_text(provider, url_chars, 0, 8);

    const Url reparsed(url.encode());
    test::fuzz_check(reparsed.scheme == url.scheme, "scheme round trips");
    test::fuzz_check(reparsed.host == url.host, "host round trips");
    test::fuzz_check(reparsed.port == url.port, "port round trips");
    test::fuzz_check(reparsed.path == url.path, "path round trips");
    test::fuzz_check(reparsed.query_params == url.query_params, "query params round trip");
    test::fuzz_check(reparsed.fragment == url.fragment, "fragment round trips");
}
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzedDataProvider provider(data, size);
    switch (provider.ConsumeIntegralInRange<int>(0, 2))
    {
        case 0:
            fuzz_percent_encoding(provider);
            break;
        case 1:
            fuzz_decode(provider);
            break;
        default:
            fuzz_structured_round_trip(provider);
            break;
    }
    return 0;
}
