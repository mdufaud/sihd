#include <cstddef>
#include <cstdint>
#include <string>

#include <sihd/http/Multipart.hpp>

#include <fuzzer/FuzzedDataProvider.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzedDataProvider provider(data, size);
    const std::string content_type = provider.ConsumeRandomLengthString(80);
    const std::string body = provider.ConsumeRemainingBytesAsString();

    (void)sihd::http::Multipart::is_content_type(content_type);
    if (const auto boundary = sihd::http::Multipart::boundary(content_type); boundary.has_value())
        (void)sihd::http::Multipart::parse(body, content_type);
    (void)sihd::http::Multipart::parse(body, "multipart/form-data; boundary=b");
    return 0;
}
