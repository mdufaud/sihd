#include "test_helper.hpp"

#include <random>

#include <gtest/gtest.h>

#include <sihd/util/Array.hpp>

namespace test
{

using namespace sihd::util;
using namespace sihd::zlib;

std::string text_of(size_t size)
{
    const std::string pattern = "sihd channels carry bytes, zlib squeezes them. ";
    std::string text;
    text.reserve(size + pattern.size());
    while (text.size() < size)
        text += pattern;
    text.resize(size);
    return text;
}

std::string noise_of(size_t size)
{
    std::mt19937 generator(42);
    std::uniform_int_distribution<int> byte(0, 255);
    std::string noise(size, '\0');
    for (char & noise_byte : noise)
        noise_byte = static_cast<char>(byte(generator));
    return noise;
}

std::string compress_message(const std::string & text, Deflater::Format format)
{
    Deflater deflater(format);
    Result<ArrByte> compressed = deflater.deflate_message(ArrByteView(text));
    EXPECT_TRUE(compressed.has_value());
    return compressed.has_value() ? std::string(compressed->cpp_str_view()) : std::string();
}

std::string_view view_of(const sihd::core::Channel *channel)
{
    return std::string_view(reinterpret_cast<const char *>(channel->data()), channel->byte_size());
}

} // namespace test
