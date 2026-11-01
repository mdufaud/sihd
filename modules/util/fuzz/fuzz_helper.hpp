#ifndef __SIHD_UTIL_FUZZ_HELPER_HPP__
#define __SIHD_UTIL_FUZZ_HELPER_HPP__

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>

namespace test
{

class ExactText
{
    public:
        explicit ExactText(std::string_view text): _bytes(text.begin(), text.end()) {}

        std::string_view view() const { return std::string_view(_bytes.data(), _bytes.size()); }

        operator std::string_view() const { return this->view(); }

    private:
        std::vector<char> _bytes;
};

template <typename Provider>
std::vector<uint8_t> consume_exact_bytes(Provider & provider, size_t size)
{
    const std::vector<uint8_t> consumed = provider.template ConsumeBytes<uint8_t>(size);
    std::vector<uint8_t> bytes(size);
    std::copy(consumed.begin(), consumed.end(), bytes.begin());
    return bytes;
}

template <typename Provider>
std::string consume_text(Provider & provider, std::string_view alphabet, size_t min_length, size_t max_length)
{
    std::string text(provider.template ConsumeIntegralInRange<size_t>(min_length, max_length), 'a');
    for (char & c : text)
        c = alphabet[provider.template ConsumeIntegralInRange<size_t>(0, alphabet.size() - 1)];
    return text;
}

template <typename Provider>
std::string consume_word(Provider & provider, size_t min_length, size_t max_length)
{
    return consume_text(provider, "abcdefghijklmnopqrstuvwxyz0123456789", min_length, max_length);
}

inline std::string_view view_of(const std::vector<char> & buffer)
{
    return {buffer.data(), buffer.size()};
}

inline void fuzz_check(bool ok, const char *property)
{
    if (ok)
        return;
    std::fprintf(stderr, "fuzz property violated: %s\n", property);
    std::abort();
}

} // namespace test

#endif
