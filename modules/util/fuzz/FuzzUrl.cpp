#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>

#include <sihd/util/Url.hpp>

#include "fuzz_helper.hpp"
#include <fuzzer/FuzzedDataProvider.h>

namespace
{
using namespace sihd::util;

constexpr std::string_view letters = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
constexpr std::string_view url_chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
constexpr std::string_view scheme_chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789+-.";
constexpr std::string_view
    invalid_scheme_chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789+-._ ~!";
constexpr std::string_view host_chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-.";
constexpr std::string_view userinfo_chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789:-._~";
constexpr std::string_view path_chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789/-._~";
constexpr std::string_view
    no_authority_path_chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789/-._~:@";

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

std::string percent_encode(std::string_view text)
{
    constexpr std::string_view unreserved = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.~";
    constexpr std::string_view hex = "0123456789ABCDEF";
    std::string encoded;
    for (const unsigned char c : text)
    {
        if (unreserved.find(static_cast<char>(c)) != std::string_view::npos)
            encoded += static_cast<char>(c);
        else
            encoded += {'%', hex[c >> 4], hex[c & 0xF]};
    }
    return encoded;
}

std::string consume_scheme(FuzzedDataProvider & provider)
{
    std::string scheme = test::consume_text(provider, letters, 1, 1);
    scheme += test::consume_text(provider, scheme_chars, 0, 7);
    return scheme;
}

std::string consume_host(FuzzedDataProvider & provider)
{
    switch (provider.ConsumeIntegralInRange<int>(0, 2))
    {
        case 0:
            return test::consume_text(provider, host_chars, 1, 16);
        case 1:
        {
            std::string host = provider.ConsumeBool() ? "::" : "";
            for (size_t i = provider.ConsumeIntegralInRange<size_t>(1, 8); i > 0; --i)
                host += test::consume_text(provider, "0123456789abcdef", 1, 4) + (i > 1 ? ":" : "");
            return host;
        }
        default:
            return "";
    }
}

std::map<std::string, std::string> consume_query(FuzzedDataProvider & provider)
{
    std::map<std::string, std::string> query;
    for (size_t i = provider.ConsumeIntegralInRange<size_t>(0, 4); i > 0; --i)
    {
        std::string key = provider.ConsumeRandomLengthString(8);
        query[key.empty() ? "k" : key] = provider.ConsumeRandomLengthString(8);
    }
    return query;
}

std::string expected_uri(const Url & url)
{
    std::string uri = url.scheme.empty() ? "" : url.scheme + ":";
    if (url.has_authority)
    {
        uri += "//";
        uri += url.userinfo.empty() ? "" : url.userinfo + "@";
        uri += url.host.find(':') == std::string::npos ? url.host : "[" + url.host + "]";
        uri += url.port == 0 ? "" : ":" + std::to_string(url.port);
    }
    uri += url.path;
    char separator = '?';
    for (const auto & [key, value] : url.query_params)
    {
        uri += separator + percent_encode(key) + "=" + percent_encode(value);
        separator = '&';
    }
    uri += url.fragment.empty() ? "" : "#" + url.fragment;
    return uri;
}

void fuzz_structured_round_trip(FuzzedDataProvider & provider)
{
    Url url;
    url.has_authority = provider.ConsumeBool();
    if (url.has_authority)
    {
        url.scheme = provider.ConsumeBool() ? consume_scheme(provider) : "";
        url.userinfo = test::consume_text(provider, userinfo_chars, 0, 8);
        url.host = consume_host(provider);
        url.port = provider.ConsumeBool() ? provider.ConsumeIntegralInRange<int>(1, 65535) : 0;
        url.path = "/" + test::consume_text(provider, path_chars, 0, 16);
    }
    else
    {
        url.scheme = consume_scheme(provider);
        url.path = test::consume_text(provider, letters, 1, 1)
                   + test::consume_text(provider, no_authority_path_chars, 0, 16);
    }
    url.query_params = consume_query(provider);
    url.fragment = test::consume_text(provider, url_chars, 0, 8);

    const std::string uri = expected_uri(url);
    test::fuzz_check(url.encode() == uri, "encode writes the components in uri order");

    const Url reparsed(uri);
    test::fuzz_check(reparsed.scheme == url.scheme, "scheme round trips");
    test::fuzz_check(reparsed.userinfo == url.userinfo, "userinfo round trips");
    test::fuzz_check(reparsed.host == url.host, "host round trips");
    test::fuzz_check(reparsed.port == url.port, "port round trips");
    test::fuzz_check(reparsed.path == url.path, "path round trips");
    test::fuzz_check(reparsed.query_params == url.query_params, "query params round trip");
    test::fuzz_check(reparsed.fragment == url.fragment, "fragment round trips");
    test::fuzz_check(reparsed.has_authority == url.has_authority, "the authority presence round trips");
}

void fuzz_invalid_scheme(FuzzedDataProvider & provider)
{
    const std::string head = test::consume_text(provider, invalid_scheme_chars, 1, 6);
    const bool valid = letters.find(head.front()) != std::string_view::npos
                       && head.find_first_not_of(scheme_chars) == std::string::npos;
    if (valid)
        return;

    const std::string text = head + ":" + test::consume_text(provider, url_chars, 1, 8);
    const Url url(text);
    test::fuzz_check(url.scheme.empty() && !url.has_authority, "a text that is not a scheme is not one");
    test::fuzz_check(url.path == text, "a text before the colon that is not a scheme stays in the path");
}
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzedDataProvider provider(data, size);
    switch (provider.ConsumeIntegralInRange<int>(0, 3))
    {
        case 0:
            fuzz_percent_encoding(provider);
            break;
        case 1:
            fuzz_decode(provider);
            break;
        case 2:
            fuzz_invalid_scheme(provider);
            break;
        default:
            fuzz_structured_round_trip(provider);
            break;
    }
    return 0;
}
