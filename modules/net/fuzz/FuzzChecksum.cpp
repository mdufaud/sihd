#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fuzz_helper.hpp>
#include <vector>

#include <sihd/net/utils.hpp>

#include <fuzzer/FuzzedDataProvider.h>

using namespace sihd::net;

namespace
{
constexpr uint8_t pad_byte = 0xa5;

uint16_t rfc1071_checksum(const uint8_t *bytes, size_t length)
{
    uint32_t sum = 0;
    for (size_t i = 0; i + 1 < length; i += 2)
    {
        uint16_t word = 0;
        std::memcpy(&word, bytes + i, sizeof(word));
        sum += word;
    }
    if (length % 2 == 1)
    {
        uint16_t word = 0;
        std::memcpy(&word, bytes + length - 1, 1);
        sum += word;
    }
    while ((sum >> 16) != 0)
        sum = (sum & 0xffff) + (sum >> 16);
    return static_cast<uint16_t>(~sum);
}
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzedDataProvider provider(data, size);
    const std::vector<uint8_t> bytes = provider.ConsumeBytes<uint8_t>(4096);
    if (bytes.empty())
        return 0;

    // the checksum reads through a uint16_t pointer: the words storage keeps it aligned
    std::vector<uint16_t> words((bytes.size() + 1) / 2);
    std::memcpy(words.data(), bytes.data(), bytes.size());
    // a non zero pad byte makes a read past an odd buffer change the sum
    if (bytes.size() % 2 == 1)
        reinterpret_cast<uint8_t *>(words.data())[bytes.size()] = pad_byte;
    const uint16_t sum = utils::checksum(words.data(), static_cast<int>(bytes.size()));
    test::fuzz_check(sum == rfc1071_checksum(bytes.data(), bytes.size()), "the checksum is the rfc 1071 checksum");

    // exact size heap copy: ASan sees a read past the last byte
    const std::vector<uint8_t> exact(bytes.begin(), bytes.end());
    uint16_t *exact_words = reinterpret_cast<uint16_t *>(const_cast<uint8_t *>(exact.data()));
    test::fuzz_check(utils::checksum(exact_words, static_cast<int>(exact.size())) == sum,
                     "the checksum does not depend on what follows the buffer");

    const size_t even_size = bytes.size() & ~size_t {1};
    std::vector<uint16_t> verified(words.begin(), words.begin() + static_cast<ptrdiff_t>(even_size / 2));
    verified.push_back(0);
    verified.back() = utils::checksum(verified.data(), static_cast<int>(even_size));
    test::fuzz_check(utils::checksum(verified.data(), static_cast<int>(even_size + sizeof(uint16_t))) == 0,
                     "a buffer followed by its checksum verifies to zero");
    return 0;
}
