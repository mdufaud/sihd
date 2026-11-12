#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <fuzz_helper.hpp>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <sihd/http/HttpRequest.hpp>

#include <fuzzer/FuzzedDataProvider.h>

namespace
{
using namespace sihd::http;
using Params = std::unordered_map<std::string, std::string>;

constexpr HttpRequest::RequestType request_types[] = {
    HttpRequest::Post,
    HttpRequest::Get,
    HttpRequest::Put,
    HttpRequest::Delete,
    HttpRequest::Options,
    HttpRequest::Patch,
    HttpRequest::Head,
};
constexpr std::string_view padding_chars = " \t";
constexpr std::string_view value_chars = "abc012=-_.";

bool same_request(const HttpRequest & got, const HttpRequest & expected)
{
    return got.request_type() == expected.request_type() && got.url() == expected.url()
           && got.query_params() == expected.query_params()
           && got.http_header().headers() == expected.http_header().headers()
           && got.has_content() == expected.has_content() && got.content().cpp_str() == expected.content().cpp_str();
}

std::string flip_case(std::string_view text, bool upper)
{
    std::string result(text);
    for (char & c : result)
        c = static_cast<char>(upper ? std::toupper(static_cast<unsigned char>(c))
                                    : std::tolower(static_cast<unsigned char>(c)));
    return result;
}

void fuzz_parse(FuzzedDataProvider & provider)
{
    const test::ExactText raw(provider.ConsumeRemainingBytesAsString());
    HttpRequest parsed;
    if (!parsed.from_string(raw))
        return;
    const std::string serialized = parsed.to_string();
    HttpRequest reparsed;
    test::fuzz_check(reparsed.from_string(serialized).has_value(), "a parsed request serializes to a parsable request");
    test::fuzz_check(same_request(reparsed, parsed),
                     "type, url, query parameters, headers and body survive serialization");
}

void fuzz_round_trip(FuzzedDataProvider & provider)
{
    const HttpRequest::RequestType
        type = request_types[provider.ConsumeIntegralInRange<size_t>(0, std::size(request_types) - 1)];
    const std::string url = "/" + test::consume_word(provider, 1, 16);
    const std::string header_name = "x-" + test::consume_word(provider, 1, 8);
    const std::string header_value = test::consume_word(provider, 1, 16);
    const std::string body = provider.ConsumeRandomLengthString(128);
    Params query;
    for (size_t i = provider.ConsumeIntegralInRange<size_t>(0, 3); i > 0; --i)
        query["q" + std::to_string(i) + test::consume_word(provider, 0, 4)] = test::consume_text(provider,
                                                                                                 value_chars,
                                                                                                 0,
                                                                                                 6);

    HttpRequest original(url, type);
    original.http_header().set_header(header_name, header_value);
    original.set_content({body.data(), body.size()});
    original.set_query_params(Params(query));

    HttpRequest parsed;
    test::fuzz_check(parsed.from_string(original.to_string()).has_value(), "a built request parses");
    test::fuzz_check(parsed.request_type() == type, "the request type round trips");
    test::fuzz_check(parsed.url() == url, "the url round trips");
    test::fuzz_check(parsed.content().cpp_str() == body, "the body round trips");
    test::fuzz_check(parsed.http_header().find(header_name) == header_value, "a header round trips");
    test::fuzz_check(parsed.query_params() == query, "the query parameters round trip");
    test::fuzz_check(same_request(parsed, original), "a built request survives serialization");
}

void fuzz_generated(FuzzedDataProvider & provider)
{
    const HttpRequest::RequestType
        type = request_types[provider.ConsumeIntegralInRange<size_t>(0, std::size(request_types) - 1)];
    const std::string path = "/" + test::consume_word(provider, 1, 8)
                             + (provider.ConsumeBool() ? "/" + test::consume_word(provider, 1, 6) : "");
    Params query;
    std::string target = path;
    if (provider.ConsumeBool())
    {
        target += "?";
        for (size_t i = provider.ConsumeIntegralInRange<size_t>(0, 4); i > 0; --i)
        {
            const std::string key = provider.ConsumeBool() ? "k" + std::to_string(i % 2) : "";
            const bool with_value = provider.ConsumeBool();
            const std::string value = test::consume_text(provider, value_chars, 0, 6);
            // a parameter without key or without "=" is dropped, a repeated key keeps its last value
            target += key + (with_value ? "=" + value : "") + "&";
            if (!key.empty() && with_value)
                query[key] = value;
        }
    }

    std::string raw = flip_case(HttpRequest::type_str(type), provider.ConsumeBool())
                      + test::consume_text(provider, " ", 1, 3) + target + test::consume_text(provider, " ", 1, 3)
                      + "HTTP/1." + std::string(1, static_cast<char>('0' + provider.ConsumeIntegralInRange<int>(0, 9)))
                      + "\r\n";
    std::vector<std::pair<std::string, std::string>> headers;
    for (size_t i = provider.ConsumeIntegralInRange<size_t>(0, 4); i > 0; --i)
    {
        std::string name = "x" + std::to_string(headers.size()) + test::consume_word(provider, 0, 6);
        std::string value = "v" + test::consume_word(provider, 0, 8);
        raw += name + test::consume_text(provider, padding_chars, 0, 2) + ":"
               + test::consume_text(provider, padding_chars, 0, 2) + value + "\r\n";
        headers.emplace_back(std::move(name), std::move(value));
    }
    const std::string body = provider.ConsumeRandomLengthString(48);
    raw += "\r\n" + body;

    const test::ExactText exact(raw);
    HttpRequest parsed;
    test::fuzz_check(parsed.from_string(exact).has_value(), "a well formed request parses");
    test::fuzz_check(parsed.request_type() == type, "the method is read, whatever its case");
    test::fuzz_check(parsed.url() == path, "the url is the target without its query");
    test::fuzz_check(parsed.query_params() == query, "the query parameters are read, last value wins");
    for (const auto & [name, value] : headers)
        test::fuzz_check(parsed.http_header().find(name) == value, "a header line is read with its value");
    test::fuzz_check(parsed.http_header().headers().size() == headers.size(), "no header is invented");
    test::fuzz_check(parsed.content().cpp_str() == body && parsed.has_content() == !body.empty(),
                     "the body is whatever follows the blank line");

    HttpRequest reparsed;
    test::fuzz_check(reparsed.from_string(parsed.to_string()).has_value() && same_request(reparsed, parsed),
                     "a generated request survives serialization");
}

void fuzz_refused()
{
    const std::string tail = "\r\nA: b\r\n\r\nbody";
    const std::array<std::string, 10> refused = {
        "GET /" + tail,
        "GET / HTTP" + std::string("/2") + tail,
        "GET / FTP/1.1" + tail,
        "FETCH / HTTP/1.1" + tail,
        "/ HTTP/1.1" + tail,
        "GET ?a=1 HTTP/1.1" + tail,
        "GET  HTTP/1.1" + tail,
        "GET / HTTP/1.1\r\nA: b",
        "\r\n\r\nGET / HTTP/1.1",
        std::string(),
    };
    HttpRequest kept("/kept", HttpRequest::Put);
    for (const std::string & text : refused)
        test::fuzz_check(!kept.from_string(test::ExactText(text)).has_value(),
                         "a malformed request line or a block without blank line is refused");
    test::fuzz_check(kept.url() == "/kept" && kept.request_type() == HttpRequest::Put,
                     "a refused request keeps the parsed one");
}
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzedDataProvider provider(data, size);
    switch (provider.ConsumeIntegralInRange<int>(0, 3))
    {
        case 0:
            fuzz_parse(provider);
            break;
        case 1:
            fuzz_round_trip(provider);
            break;
        case 2:
            fuzz_generated(provider);
            break;
        default:
            fuzz_refused();
            break;
    }
    return 0;
}
