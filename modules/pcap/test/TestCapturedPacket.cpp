#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include <sihd/pcap/CapturedPacket.hpp>
#include <sihd/util/Logger.hpp>

namespace test
{
using namespace sihd::pcap;
using namespace sihd::util;

class TestCapturedPacket: public ::testing::Test
{
    protected:
        TestCapturedPacket() { LoggerManager::stream(); }

        virtual ~TestCapturedPacket() { LoggerManager::clear_loggers(); }
};

TEST_F(TestCapturedPacket, test_captured_packet_round_trip)
{
    const std::vector<uint8_t> bytes {0xde, 0xad, 0xbe, 0xef};
    const CapturedPacket packet(Timestamp(time::sec(3) + 250), 1500, 127, bytes);

    const ArrUByte encoded = packet.encode();
    Result<CapturedPacket> decoded = CapturedPacket::decode(encoded);

    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(encoded.size(), CapturedPacket::header_size + bytes.size());
    EXPECT_EQ(decoded->timestamp(), Timestamp(time::sec(3) + 250));
    EXPECT_EQ(decoded->original_length(), 1500u);
    EXPECT_EQ(decoded->datalink(), 127);
    EXPECT_EQ(std::vector<uint8_t>(decoded->data().begin(), decoded->data().end()), bytes);
}

TEST_F(TestCapturedPacket, test_captured_packet_rejects_a_short_buffer)
{
    const std::vector<uint8_t> header_only(CapturedPacket::header_size, 0);
    const std::vector<uint8_t> too_short(CapturedPacket::header_size - 1, 0);

    Result<CapturedPacket> empty_packet = CapturedPacket::decode(header_only);
    Result<CapturedPacket> cut_header = CapturedPacket::decode(too_short);

    ASSERT_TRUE(empty_packet.has_value());
    EXPECT_EQ(empty_packet->data().size(), 0u);
    ASSERT_FALSE(cut_header.has_value());
    EXPECT_EQ(cut_header.error().code, ErrorCode::invalid_argument);
}

} // namespace test
