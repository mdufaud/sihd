#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <format>
#include <fuzz_helper.hpp>
#include <string>
#include <string_view>
#include <vector>

#include <sihd/sys/Uuid.hpp>

#include <fuzzer/FuzzedDataProvider.h>

using namespace sihd::sys;

namespace
{
constexpr size_t uuid_text_size = 36;
constexpr size_t uuid_byte_size = 16;
constexpr size_t version_index = 14;
constexpr size_t variant_index = 19;
constexpr std::string_view nil_text = "00000000-0000-0000-0000-000000000000";
constexpr std::string_view variant_digits = "89ab";

bool is_dash_index(size_t index)
{
    return index == 8 || index == 13 || index == 18 || index == 23;
}

bool is_canonical(const std::string & text)
{
    if (text.size() != uuid_text_size)
        return false;
    for (size_t i = 0; i < text.size(); ++i)
    {
        const bool valid = is_dash_index(i) ? text[i] == '-' : std::isxdigit(static_cast<unsigned char>(text[i])) != 0;
        if (!valid)
            return false;
    }
    return true;
}

std::string lowercase(std::string text)
{
    std::ranges::transform(text, text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

std::string uppercase(std::string text)
{
    std::ranges::transform(text, text.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return text;
}

std::string format_uuid(const std::vector<uint8_t> & bytes)
{
    std::string text;
    for (size_t i = 0; i < bytes.size(); ++i)
    {
        if (i == 4 || i == 6 || i == 8 || i == 10)
            text += '-';
        text += std::format("{:02x}", bytes[i]);
    }
    return text;
}

std::vector<uint8_t> consume_uuid_bytes(FuzzedDataProvider & provider)
{
    std::vector<uint8_t> bytes = provider.ConsumeBytes<uint8_t>(uuid_byte_size);
    bytes.resize(uuid_byte_size);
    return bytes;
}

bool is_all_zero(const std::vector<uint8_t> & bytes)
{
    return std::ranges::all_of(bytes, [](uint8_t byte) { return byte == 0; });
}

void fuzz_parse(FuzzedDataProvider & provider)
{
    const std::string text = provider.ConsumeRemainingBytesAsString();
    const Uuid uuid(text);
    const std::string printed = uuid.str();

    test::fuzz_check(Uuid(printed) == uuid, "a printed uuid parses back to itself");
    test::fuzz_check(Uuid(uuid) == uuid, "a copy equals its source");

    // the parser reads the text as a c string
    if (text.find('\0') != std::string::npos)
        return;
    const bool canonical = is_canonical(text);
    const bool zero = std::ranges::all_of(text, [](char c) { return c == '0' || c == '-'; });
    test::fuzz_check(uuid.is_null() == (!canonical || zero), "only the nil uuid and bad text are null");
    test::fuzz_check(printed == (canonical ? lowercase(text) : std::string(nil_text)),
                     "a uuid prints its text in lowercase, bad text gives the nil uuid");
}

void fuzz_bytes(FuzzedDataProvider & provider)
{
    const std::vector<uint8_t> bytes = consume_uuid_bytes(provider);
    std::vector<uint8_t> other = bytes;
    if (provider.ConsumeBool())
        other[provider.ConsumeIntegralInRange<size_t>(0, uuid_byte_size - 1)] ^= provider.ConsumeIntegral<uint8_t>();

    const std::string text = format_uuid(bytes);
    const Uuid uuid(text);
    test::fuzz_check(uuid.str() == text, "sixteen bytes print as lowercase 8-4-4-4-12 hex");
    test::fuzz_check(uuid.is_null() == is_all_zero(bytes), "only sixteen zero bytes make a null uuid");
    test::fuzz_check(Uuid(uppercase(text)) == uuid, "the case of the hex digits does not matter");
    test::fuzz_check(Uuid(uppercase(text)).str() == text, "an uppercase uuid prints in lowercase");
    test::fuzz_check((Uuid(format_uuid(other)) == uuid) == (other == bytes), "uuids are equal when their bytes are");
}

void fuzz_name_based(FuzzedDataProvider & provider)
{
    const std::array<Uuid, 4> namespaces = {Uuid::DNS(), Uuid::URL(), Uuid::OID(), Uuid::X500()};
    const Uuid & uuid_namespace = namespaces[provider.ConsumeIntegralInRange<size_t>(0, namespaces.size() - 1)];
    const std::string name = provider.ConsumeRemainingBytesAsString();

    const Uuid uuid(uuid_namespace, name);
    const std::string text = uuid.str();
    test::fuzz_check(Uuid(uuid_namespace, name) == uuid, "a name based uuid is deterministic");
    test::fuzz_check(is_canonical(text) && lowercase(text) == text, "a name based uuid prints in canonical form");
    test::fuzz_check(text[version_index] == '5', "a name based uuid is version 5");
    test::fuzz_check(variant_digits.find(text[variant_index]) != std::string_view::npos,
                     "a name based uuid has the rfc 4122 variant");
    test::fuzz_check(Uuid(text) == uuid, "a name based uuid parses back to itself");
}
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzedDataProvider provider(data, size);
    switch (provider.ConsumeIntegralInRange<int>(0, 2))
    {
        case 0:
            fuzz_parse(provider);
            break;
        case 1:
            fuzz_bytes(provider);
            break;
        default:
            fuzz_name_based(provider);
            break;
    }
    return 0;
}
