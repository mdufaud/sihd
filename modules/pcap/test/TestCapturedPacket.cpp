#include <cstdint>
#include <stdexcept>
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
    const CapturedPacket decoded(encoded);

    EXPECT_EQ(encoded.size(), CapturedPacket::header_size + bytes.size());
    EXPECT_EQ(decoded.timestamp(), Timestamp(time::sec(3) + 250));
    EXPECT_EQ(decoded.original_length(), 1500u);
    EXPECT_EQ(decoded.datalink(), 127);
    EXPECT_EQ(std::vector<uint8_t>(decoded.data().begin(), decoded.data().end()), bytes);
}

TEST_F(TestCapturedPacket, test_captured_packet_rejects_a_short_buffer)
{
    const std::vector<uint8_t> header_only(CapturedPacket::header_size, 0);
    const std::vector<uint8_t> too_short(CapturedPacket::header_size - 1, 0);
    const std::vector<uint8_t> bytes {0xde, 0xad};
    CapturedPacket empty_packet;
    CapturedPacket kept(Timestamp(time::sec(3)), 1500, 127, bytes);

    auto decoded = empty_packet.decode(header_only);
    auto refused = kept.decode(too_short);

    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(empty_packet.data().size(), 0u);
    ASSERT_FALSE(refused.has_value());
    EXPECT_EQ(refused.error().code, ErrorCode::invalid_argument);
    EXPECT_EQ(refused.error().message, "captured packet shorter than its header");
    EXPECT_EQ(kept.timestamp(), Timestamp(time::sec(3)));
    EXPECT_EQ(kept.original_length(), 1500u);
    EXPECT_EQ(kept.datalink(), 127);
    EXPECT_EQ(kept.data().size(), bytes.size());
    EXPECT_THROW({ CapturedPacket thrown(too_short); }, std::runtime_error);
}

} // namespace test
