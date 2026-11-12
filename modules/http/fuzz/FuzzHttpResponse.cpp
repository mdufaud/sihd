#include <array>
#include <cstddef>
#include <cstdint>
#include <fuzz_helper.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <sihd/http/HttpResponse.hpp>
#include <sihd/http/HttpStatus.hpp>

#include <fuzzer/FuzzedDataProvider.h>

namespace
{
using namespace sihd::http;

constexpr uint32_t statuses[] = {200, 201, 400, 401, 403, 404, 500, 503};
constexpr std::array<std::string_view, 3> media_types = {"text/html", "application/x-custom", "image/png"};
constexpr std::array<std::string_view, 2> cookie_prefixes = {"Set-Cookie: ", "set-cookie: "};

void check_same_response(const HttpResponse & got, const HttpResponse & expected, const char *property)
{
    test::fuzz_check(got.status() == expected.status()
                         && got.http_header().headers() == expected.http_header().headers()
                         && got.set_cookie_headers() == expected.set_cookie_headers()
                         && got.content().cpp_str() == expected.content().cpp_str(),
                     property);
}

void fuzz_parse(FuzzedDataProvider & provider)
{
    const test::ExactText raw(provider.ConsumeRemainingBytesAsString());
    HttpResponse parsed;
    if (!parsed.from_string(raw))
        return;
    HttpResponse reparsed;
    test::fuzz_check(reparsed.from_string(parsed.to_string()).has_value(),
                     "a parsed response serializes to a parsable response");
    check_same_response(reparsed, parsed, "status, headers, cookies and body survive serialization");
}

void fuzz_round_trip(FuzzedDataProvider & provider)
{
    const uint32_t status = provider.ConsumeBool()
                                ? provider.ConsumeIntegral<uint32_t>()
                                : statuses[provider.ConsumeIntegralInRange<size_t>(0, std::size(statuses) - 1)];
    const std::string header_name = "x-" + test::consume_word(provider, 1, 8);
    const std::string header_value = test::consume_word(provider, 1, 16);
    const std::string body = provider.ConsumeRandomLengthString(128);
    std::vector<std::string> cookies;

    HttpResponse original;
    original.set_status(status);
    original.http_header().set_header(header_name, header_value);
    original.set_plain_content(body);
    for (size_t i = provider.ConsumeIntegralInRange<size_t>(0, 3); i > 0; --i)
    {
        const std::string name = test::consume_word(provider, 1, 6);
        const std::string value = test::consume_word(provider, 0, 6);
        const std::string options = provider.ConsumeBool() ? "Path=/; " + test::consume_word(provider, 1, 6) : "";
        original.set_cookie(name, value, options);
        cookies.push_back(name + "=" + value + (options.empty() ? "" : "; " + options));
    }

    HttpResponse parsed;
    test::fuzz_check(parsed.from_string(original.to_string()).has_value(), "a built response parses");
    test::fuzz_check(parsed.status() == status, "the status round trips");
    test::fuzz_check(parsed.content().cpp_str() == body, "the body round trips");
    test::fuzz_check(parsed.http_header().find(header_name) == header_value, "a header round trips");
    test::fuzz_check(parsed.set_cookie_headers() == cookies, "the cookies round trip, in order");
    check_same_response(parsed, original, "a built response survives serialization");
}

void fuzz_generated(FuzzedDataProvider & provider)
{
    const uint32_t status = provider.ConsumeIntegral<uint32_t>();
    std::string raw = "HTTP/1." + std::string(1, static_cast<char>('0' + provider.ConsumeIntegralInRange<int>(0, 9)))
                      + test::consume_text(provider, " ", 1, 3) + std::to_string(status);
    if (provider.ConsumeBool())
        raw += " " + test::consume_word(provider, 1, 8);
    raw += "\r\n";

    std::vector<std::pair<std::string, std::string>> headers;
    std::vector<std::string> cookies;
    for (size_t i = provider.ConsumeIntegralInRange<size_t>(0, 5); i > 0; --i)
    {
        if (provider.ConsumeBool())
        {
            std::string cookie = test::consume_word(provider, 1, 6) + "=" + test::consume_word(provider, 1, 6);
            raw += std::string(cookie_prefixes[provider.ConsumeIntegralInRange<size_t>(0, 1)]) + cookie + "\r\n";
            cookies.push_back(std::move(cookie));
        }
        else
        {
            std::string name = "x" + std::to_string(headers.size()) + test::consume_word(provider, 0, 6);
            std::string value = "v" + test::consume_word(provider, 0, 8);
            raw += name + test::consume_text(provider, " \t", 0, 2) + ":" + test::consume_text(provider, " \t", 0, 2)
                   + value + "\r\n";
            headers.emplace_back(std::move(name), std::move(value));
        }
    }
    const std::string body = provider.ConsumeRandomLengthString(48);
    raw += "\r\n" + body;

    const test::ExactText exact(raw);
    HttpResponse parsed;
    test::fuzz_check(parsed.from_string(exact).has_value(),
                     "a status line with or without reason and a well formed block parse");
    test::fuzz_check(parsed.status() == status, "the status is read");
    test::fuzz_check(parsed.set_cookie_headers() == cookies, "every cookie line is kept, whatever its case");
    for (const auto & [name, value] : headers)
        test::fuzz_check(parsed.http_header().find(name) == value, "a header line is read with its value");
    // the response constructor always sets accept-charset
    test::fuzz_check(parsed.http_header().headers().size() == headers.size() + 1,
                     "cookies are not headers and no header is invented");
    test::fuzz_check(parsed.content().cpp_str() == body, "the body is whatever follows the blank line");
}

void fuzz_refused()
{
    const std::string tail = "\r\nA: b\r\n\r\nbody";
    const std::array<std::string, 9> refused = {
        "HTTP/1.1" + tail,
        "HTTP/1.1 abc OK" + tail,
        "HTTP/1.1 -1 X" + tail,
        "HTTP/1.1 4294967296 X" + tail,
        "HTTP/2 200 OK" + tail,
        "HTTPS/1.1 200 OK" + tail,
        "200 OK" + tail,
        "HTTP/1.1 200 OK\r\nA: b",
        std::string(),
    };
    HttpResponse kept;
    kept.set_status(HttpStatus::Accepted);
    for (const std::string & text : refused)
        test::fuzz_check(!kept.from_string(test::ExactText(text)).has_value(),
                         "a malformed status line or a block without blank line is refused");
    test::fuzz_check(kept.status() == HttpStatus::Accepted, "a refused response keeps the parsed one");
}

void check_content_type(FuzzedDataProvider & provider)
{
    const std::string mime(media_types[provider.ConsumeIntegralInRange<size_t>(0, media_types.size() - 1)]);
    const std::string charset = test::consume_word(provider, 1, 6);

    HttpResponse plain;
    plain.set_content_type(mime);
    const auto plain_type = plain.http_header().content_type();
    test::fuzz_check(
        plain_type.has_value() && plain_type->starts_with(mime) && header_param(*plain_type, "charset") == "utf-8",
        "a mime type without charset gets utf-8");

    HttpResponse with_charset;
    const std::string typed = mime + "; charset=" + charset;
    with_charset.set_content_type(typed);
    test::fuzz_check(with_charset.http_header().content_type() == typed,
                     "a mime type already carrying its charset is kept as is");
}

void check_default_type(FuzzedDataProvider & provider)
{
    const bool preset = provider.ConsumeBool();
    const bool as_bytes = provider.ConsumeBool();
    const std::string body = provider.ConsumeRandomLengthString(16);

    HttpResponse response;
    if (preset)
        response.set_content_type("application/x-custom");
    const std::optional<std::string_view> before = response.http_header().content_type();

    const std::vector<int8_t> bytes(body.begin(), body.end());
    if (as_bytes)
        response.set_byte_content({bytes.data(), bytes.size()});
    else
        response.set_plain_content(body);

    const std::optional<std::string_view> type = response.http_header().content_type();
    test::fuzz_check(type.has_value(), "a response with content has a content type");
    if (preset)
        test::fuzz_check(type == before, "setting the content keeps an explicit content type");
    else
        test::fuzz_check(type->starts_with(as_bytes ? "application/octet-stream" : "text/plain")
                             && header_param(*type, "charset") == "utf-8",
                         "setting the content defaults the content type");
    test::fuzz_check(response.content().cpp_str() == body, "the content is kept");
}
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzedDataProvider provider(data, size);
    switch (provider.ConsumeIntegralInRange<int>(0, 5))
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
        case 3:
            fuzz_refused();
            break;
        case 4:
            check_content_type(provider);
            break;
        default:
            check_default_type(provider);
            break;
    }
    return 0;
}
