#include <cstddef>
#include <cstdint>
#include <fuzz_helper.hpp>
#include <string>
#include <string_view>

#include <sihd/http/HttpResponse.hpp>

#include <fuzzer/FuzzedDataProvider.h>

namespace
{
using namespace sihd::http;

constexpr uint32_t statuses[] = {200, 201, 400, 401, 403, 404, 500, 503};

void fuzz_parse(FuzzedDataProvider & provider)
{
    auto parsed = HttpResponse::from_string(provider.ConsumeRemainingBytesAsString());
    if (!parsed.has_value())
        return;
    const auto reparsed = HttpResponse::from_string(parsed->to_string());
    test::fuzz_check(reparsed.has_value(), "a parsed response serializes to a parsable response");
    test::fuzz_check(reparsed->status() == parsed->status(), "the status survives serialization");
}

void fuzz_round_trip(FuzzedDataProvider & provider)
{
    const uint32_t status = statuses[provider.ConsumeIntegralInRange<size_t>(0, std::size(statuses) - 1)];
    const std::string header_name = "x-" + test::consume_word(provider, 1, 8);
    const std::string header_value = test::consume_word(provider, 1, 16);
    const std::string body = provider.ConsumeRandomLengthString(128);

    HttpResponse original;
    original.set_status(status);
    original.http_header().set_header(header_name, header_value);
    original.set_plain_content(body);

    const auto parsed = HttpResponse::from_string(original.to_string());
    test::fuzz_check(parsed.has_value(), "a built response parses");
    test::fuzz_check(parsed->status() == status, "the status round trips");
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
