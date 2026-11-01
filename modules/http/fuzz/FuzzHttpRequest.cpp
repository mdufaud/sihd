#include <cstddef>
#include <cstdint>
#include <fuzz_helper.hpp>
#include <string>
#include <string_view>

#include <sihd/http/HttpRequest.hpp>

#include <fuzzer/FuzzedDataProvider.h>

namespace
{
using namespace sihd::http;

constexpr HttpRequest::RequestType request_types[] = {
    HttpRequest::Post,
    HttpRequest::Get,
    HttpRequest::Put,
    HttpRequest::Delete,
    HttpRequest::Options,
    HttpRequest::Patch,
    HttpRequest::Head,
};

void fuzz_parse(FuzzedDataProvider & provider)
{
    auto parsed = HttpRequest::from_string(provider.ConsumeRemainingBytesAsString());
    if (!parsed.has_value())
        return;
    const std::string serialized = parsed->to_string();
    const auto reparsed = HttpRequest::from_string(serialized);
    test::fuzz_check(reparsed.has_value(), "a parsed request serializes to a parsable request");
    test::fuzz_check(reparsed->request_type() == parsed->request_type(), "the request type survives serialization");
    test::fuzz_check(reparsed->url() == parsed->url(), "the url survives serialization");
}

void fuzz_round_trip(FuzzedDataProvider & provider)
{
    const HttpRequest::RequestType
        type = request_types[provider.ConsumeIntegralInRange<size_t>(0, std::size(request_types) - 1)];
    const std::string url = "/" + test::consume_word(provider, 1, 16);
    const std::string header_name = "x-" + test::consume_word(provider, 1, 8);
    const std::string header_value = test::consume_word(provider, 1, 16);
    const std::string body = provider.ConsumeRandomLengthString(128);

    HttpRequest original(url, type);
    original.http_header().set_header(header_name, header_value);
    original.set_content({body.data(), body.size()});

    const auto parsed = HttpRequest::from_string(original.to_string());
    test::fuzz_check(parsed.has_value(), "a built request parses");
    test::fuzz_check(parsed->request_type() == type, "the request type round trips");
    test::fuzz_check(parsed->url() == url, "the url round trips");
    test::fuzz_check(parsed->content().cpp_str() == body, "the body round trips");
    test::fuzz_check(parsed->http_header().find(header_name) == header_value, "a header round trips");
}
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzedDataProvider provider(data, size);
    if (provider.ConsumeBool())
        fuzz_parse(provider);
    else
        fuzz_round_trip(provider);
    return 0;
}
