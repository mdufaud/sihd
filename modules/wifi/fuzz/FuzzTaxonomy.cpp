#include <cstddef>
#include <cstdint>
#include <fuzz_helper.hpp>
#include <string>
#include <vector>

#include <sihd/util/ArrayView.hpp>
#include <sihd/wifi/taxonomy.hpp>

#include <fuzzer/FuzzedDataProvider.h>

using namespace sihd::util;
using namespace sihd::wifi;

namespace
{
constexpr size_t element_header_size = 2;

size_t whole_elements_size(const std::vector<uint8_t> & bytes)
{
    size_t offset = 0;
    while (offset + element_header_size <= bytes.size()
           && offset + element_header_size + bytes[offset + 1] <= bytes.size())
        offset += element_header_size + bytes[offset + 1];
    return offset;
}

std::string signature_of(const std::vector<uint8_t> & elements)
{
    return taxonomy::element_signature(ArrUByteView(elements));
}

void check_half(const std::vector<uint8_t> & elements)
{
    const std::string signature = signature_of(elements);

    const std::vector<uint8_t> copy(elements.begin(), elements.end());
    test::fuzz_check(signature_of(copy) == signature, "a signature is deterministic");

    const size_t whole = whole_elements_size(elements);
    test::fuzz_check(signature.empty() == (whole == 0), "a signature is empty exactly when no element is whole");
    const std::vector<uint8_t> prefix(elements.begin(), elements.begin() + whole);
    test::fuzz_check(signature_of(prefix) == signature, "a damaged element ends the list");

    for (const char c : signature)
        test::fuzz_check(c >= ' ' && c <= '~' && c != '|', "a signature is printable and holds no half separator");
}

void check_line(const std::vector<uint8_t> & probe, const std::vector<uint8_t> & assoc)
{
    const std::string probe_signature = signature_of(probe);
    const std::string assoc_signature = signature_of(assoc);

    const Result<std::string> line = taxonomy::signature(ArrUByteView(probe), ArrUByteView(assoc));
    test::fuzz_check(line.has_value() == (!probe_signature.empty() && !assoc_signature.empty()),
                     "a line needs both halves");
    if (!line.has_value())
    {
        test::fuzz_check(line.error().code == ErrorCode::invalid_argument, "a missing half is invalid_argument");
        return;
    }
    test::fuzz_check(*line == "wifi4|probe:" + probe_signature + "|assoc:" + assoc_signature,
                     "the line is the two halves");
}
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzedDataProvider provider(data, size);
    const size_t probe_size = provider.ConsumeIntegralInRange<size_t>(0, provider.remaining_bytes());
    const std::vector<uint8_t> probe = provider.ConsumeBytes<uint8_t>(probe_size);
    const std::vector<uint8_t> assoc = provider.ConsumeRemainingBytes<uint8_t>();

    check_half(probe);
    check_half(assoc);
    check_line(probe, assoc);
    return 0;
}
