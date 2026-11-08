#include <array>
#include <stdexcept>
#include <string>
#include <string_view>

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

class TestDeflater: public ::testing::Test
{
    protected:
        TestDeflater() { LoggerManager::stream(); }

        virtual ~TestDeflater() { LoggerManager::clear_loggers(); }

        static constexpr size_t max_output_size = 1 << 20;
};

TEST_F(TestDeflater, test_deflater_stream_chunks_are_decoded_as_they_come)
{
    Deflater deflater;
    Inflater inflater(max_output_size);
    const std::array<std::string, 3> chunks = {"first chunk", "second chunk, a little longer", text_of(5000)};

    for (const std::string & chunk : chunks)
    {
        Result<ArrByte> compressed = deflater.deflate_stream(ArrByteView(chunk));
        ASSERT_TRUE(compressed.has_value()) << compressed.error().message;
        Result<ArrByte> inflated = inflater.inflate_stream(*compressed);
        ASSERT_TRUE(inflated.has_value()) << inflated.error().message;

        EXPECT_EQ(inflated->cpp_str_view(), chunk);
    }
}

TEST_F(TestDeflater, test_deflater_messages_are_decoded_alone)
{
    Deflater deflater;
    const std::array<std::string, 2> messages = {"first message", "second message"};

    for (const std::string & message : messages)
    {
        Result<ArrByte> compressed = deflater.deflate_message(ArrByteView(message));
        ASSERT_TRUE(compressed.has_value()) << compressed.error().message;
        Inflater fresh_inflater(max_output_size);
        Result<ArrByte> inflated = fresh_inflater.inflate_message(*compressed);
        ASSERT_TRUE(inflated.has_value()) << inflated.error().message;

        EXPECT_EQ(inflated->cpp_str_view(), message);
    }
}

TEST_F(TestDeflater, test_deflater_gzip_writes_a_gzip_header)
{
    Deflater deflater(Deflater::Format::gzip);

    Result<ArrByte> compressed = deflater.deflate_message(ArrByteView(std::string_view("gzip payload")));

    ASSERT_TRUE(compressed.has_value()) << compressed.error().message;
    ASSERT_GE(compressed->size(), 2u);
    EXPECT_EQ(static_cast<uint8_t>(compressed->at(0)), 0x1f);
    EXPECT_EQ(static_cast<uint8_t>(compressed->at(1)), 0x8b);
}

TEST_F(TestDeflater, test_deflater_level_trades_size)
{
    const std::string text = text_of(10000);
    Deflater stored(Deflater::Format::zlib, Deflater::min_level);
    Deflater best(Deflater::Format::zlib, Deflater::max_level);

    Result<ArrByte> stored_output = stored.deflate_message(ArrByteView(text));
    Result<ArrByte> best_output = best.deflate_message(ArrByteView(text));

    ASSERT_TRUE(stored_output.has_value()) << stored_output.error().message;
    ASSERT_TRUE(best_output.has_value()) << best_output.error().message;
    EXPECT_GT(stored_output->size(), text.size());
    EXPECT_LT(best_output->size(), text.size() / 10);
}

TEST_F(TestDeflater, test_deflater_rejects_a_level_outside_the_range)
{
    EXPECT_THROW(Deflater(Deflater::Format::zlib, Deflater::min_level - 1), std::invalid_argument);
    EXPECT_THROW(Deflater(Deflater::Format::zlib, Deflater::max_level + 1), std::invalid_argument);
}

TEST_F(TestDeflater, test_deflater_empty_input)
{
    Deflater stream_deflater;
    Deflater message_deflater;
    Inflater stream_inflater(max_output_size);
    Inflater message_inflater(max_output_size);

    Result<ArrByte> stream_output = stream_deflater.deflate_stream(ArrByteView());
    Result<ArrByte> message_output = message_deflater.deflate_message(ArrByteView());

    ASSERT_TRUE(stream_output.has_value()) << stream_output.error().message;
    ASSERT_TRUE(message_output.has_value()) << message_output.error().message;
    Result<ArrByte> stream_inflated = stream_inflater.inflate_stream(*stream_output);
    Result<ArrByte> message_inflated = message_inflater.inflate_message(*message_output);
    ASSERT_TRUE(stream_inflated.has_value()) << stream_inflated.error().message;
    ASSERT_TRUE(message_inflated.has_value()) << message_inflated.error().message;
    EXPECT_EQ(stream_inflated->size(), 0u);
    EXPECT_EQ(message_inflated->size(), 0u);
}

TEST_F(TestDeflater, test_deflater_message_ends_a_running_stream)
{
    Deflater deflater;
    Inflater inflater(max_output_size);

    Result<ArrByte> chunk = deflater.deflate_stream(ArrByteView(std::string_view("streamed then ")));
    Result<ArrByte> last = deflater.deflate_message(ArrByteView(std::string_view("ended")));
    Result<ArrByte> next = deflater.deflate_message(ArrByteView(std::string_view("alone")));

    ASSERT_TRUE(chunk.has_value()) << chunk.error().message;
    ASSERT_TRUE(last.has_value()) << last.error().message;
    ASSERT_TRUE(next.has_value()) << next.error().message;
    Result<ArrByte> chunk_inflated = inflater.inflate_stream(*chunk);
    Result<ArrByte> last_inflated = inflater.inflate_stream(*last);
    Result<ArrByte> next_inflated = inflater.inflate_message(*next);
    ASSERT_TRUE(chunk_inflated.has_value()) << chunk_inflated.error().message;
    ASSERT_TRUE(last_inflated.has_value()) << last_inflated.error().message;
    ASSERT_TRUE(next_inflated.has_value()) << next_inflated.error().message;
    EXPECT_EQ(chunk_inflated->cpp_str_view(), "streamed then ");
    EXPECT_EQ(last_inflated->cpp_str_view(), "ended");
    EXPECT_EQ(next_inflated->cpp_str_view(), "alone");
}

TEST_F(TestDeflater, test_deflater_output_bound_holds_on_incompressible_input)
{
    const std::string noise = noise_of(64 * 1024);
    const ArrByteView input(noise);

    for (Deflater::Format format : {Deflater::Format::zlib, Deflater::Format::gzip})
    {
        for (int level : {Deflater::min_level, Deflater::default_level, Deflater::max_level})
        {
            Deflater deflater(format, level);
            const size_t bound = deflater.output_bound(input.size());

            for (int chunk = 0; chunk < 3; ++chunk)
            {
                Result<ArrByte> stream_output = deflater.deflate_stream(input);
                ASSERT_TRUE(stream_output.has_value()) << stream_output.error().message;
                EXPECT_LE(stream_output->size(), bound) << "level " << level << " chunk " << chunk;
            }
            Result<ArrByte> message_output = deflater.deflate_message(input);
            ASSERT_TRUE(message_output.has_value()) << message_output.error().message;
            EXPECT_LE(message_output->size(), bound) << "level " << level;
        }
    }
}

} // namespace test
