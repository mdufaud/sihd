#include <string>

#include <gtest/gtest.h>

#include <sihd/util/Logger.hpp>
#include <sihd/zlib/Deflater.hpp>
#include <sihd/zlib/Inflater.hpp>

#include "test_helper.hpp"

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;
using namespace sihd::zlib;

class TestInflater: public ::testing::Test
{
    protected:
        TestInflater() { LoggerManager::stream(); }

        virtual ~TestInflater() { LoggerManager::clear_loggers(); }

        static constexpr size_t max_output_size = 1 << 20;
};

TEST_F(TestInflater, test_inflater_corrupted_message_then_a_valid_one)
{
    Inflater inflater(max_output_size);
    std::string corrupted = compress_message("payload that will be damaged");
    corrupted[corrupted.size() / 2] ^= 0x55;
    const std::string valid = compress_message("intact payload");

    Result<ArrByte> corrupted_output = inflater.inflate_message(ArrByteView(corrupted));
    Result<ArrByte> valid_output = inflater.inflate_message(ArrByteView(valid));

    ASSERT_FALSE(corrupted_output.has_value());
    EXPECT_EQ(corrupted_output.error().code, ErrorCode::invalid_argument);
    ASSERT_TRUE(valid_output.has_value()) << valid_output.error().message;
    EXPECT_EQ(valid_output->cpp_str_view(), "intact payload");
}

TEST_F(TestInflater, test_inflater_truncated_message)
{
    Inflater inflater(max_output_size);
    const std::string compressed = compress_message("a message cut before its end");
    const std::string truncated = compressed.substr(0, compressed.size() - 3);

    Result<ArrByte> output = inflater.inflate_message(ArrByteView(truncated));

    ASSERT_FALSE(output.has_value());
    EXPECT_EQ(output.error().code, ErrorCode::invalid_argument);
    EXPECT_EQ(output.error().message, "zlib inflate: truncated message");
}

TEST_F(TestInflater, test_inflater_bytes_after_the_message)
{
    Inflater inflater(max_output_size);
    const std::string trailing = compress_message("message") + "tail";

    Result<ArrByte> output = inflater.inflate_message(ArrByteView(trailing));

    ASSERT_FALSE(output.has_value());
    EXPECT_EQ(output.error().code, ErrorCode::invalid_argument);
    EXPECT_EQ(output.error().message, "zlib inflate: bytes after the end of the message");
}

TEST_F(TestInflater, test_inflater_output_limit)
{
    const std::string text(1000, 'x');
    const std::string compressed = compress_message(text);
    Inflater too_small(999);
    Inflater exact(1000);

    Result<ArrByte> refused = too_small.inflate_message(ArrByteView(compressed));
    Result<ArrByte> accepted = exact.inflate_message(ArrByteView(compressed));

    ASSERT_FALSE(refused.has_value());
    EXPECT_EQ(refused.error().code, ErrorCode::overflow);
    EXPECT_EQ(refused.error().message, "zlib inflate: output exceeds 999 bytes");
    ASSERT_TRUE(accepted.has_value()) << accepted.error().message;
    EXPECT_EQ(accepted->cpp_str_view(), text);
}

TEST_F(TestInflater, test_inflater_grows_past_its_first_buffer)
{
    const std::string text = text_of(300 * 1024);
    const std::string noise = noise_of(200 * 1024);
    Inflater message_inflater(max_output_size);
    Inflater stream_inflater(max_output_size);

    Result<ArrByte> text_output = message_inflater.inflate_message(ArrByteView(compress_message(text)));
    Result<ArrByte> noise_output = stream_inflater.inflate_stream(
        ArrByteView(compress_message(noise, Deflater::Format::gzip)));

    ASSERT_TRUE(text_output.has_value()) << text_output.error().message;
    ASSERT_TRUE(noise_output.has_value()) << noise_output.error().message;
    EXPECT_EQ(text_output->cpp_str_view(), text);
    EXPECT_EQ(noise_output->cpp_str_view(), noise);
}

TEST_F(TestInflater, test_inflater_stream_reads_concatenated_streams)
{
    Inflater inflater(max_output_size);
    const std::string concatenated = compress_message("first ") + compress_message("second");

    Result<ArrByte> output = inflater.inflate_stream(ArrByteView(concatenated));

    ASSERT_TRUE(output.has_value()) << output.error().message;
    EXPECT_EQ(output->cpp_str_view(), "first second");
}

TEST_F(TestInflater, test_inflater_stream_reads_a_split_input)
{
    Inflater inflater(max_output_size);
    const std::string compressed = compress_message("split across two writes");
    const size_t half = compressed.size() / 2;

    Result<ArrByte> first_half = inflater.inflate_stream(ArrByteView(compressed.substr(0, half)));
    Result<ArrByte> second_half = inflater.inflate_stream(ArrByteView(compressed.substr(half)));

    ASSERT_TRUE(first_half.has_value()) << first_half.error().message;
    ASSERT_TRUE(second_half.has_value()) << second_half.error().message;
    EXPECT_EQ(std::string(first_half->cpp_str_view()) + std::string(second_half->cpp_str_view()),
              "split across two writes");
}

TEST_F(TestInflater, test_inflater_detects_gzip)
{
    Inflater inflater(max_output_size);
    const std::string compressed = compress_message("gzip payload", Deflater::Format::gzip);

    Result<ArrByte> output = inflater.inflate_message(ArrByteView(compressed));

    ASSERT_TRUE(output.has_value()) << output.error().message;
    EXPECT_EQ(output->cpp_str_view(), "gzip payload");
}

} // namespace test
