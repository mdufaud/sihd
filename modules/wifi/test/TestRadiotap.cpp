#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include <sihd/util/Logger.hpp>
#include <sihd/wifi/radiotap.hpp>

namespace test
{
using namespace sihd::util;
using namespace sihd::wifi;

class TestRadiotap: public ::testing::Test
{
    protected:
        TestRadiotap() { LoggerManager::stream(); }

        virtual ~TestRadiotap() { LoggerManager::clear_loggers(); }

        static Result<Radiotap> parse(const std::vector<uint8_t> & bytes)
        {
            return parse_radiotap(ArrUByteView(bytes));
        }

        static ErrorCode failure(const std::vector<uint8_t> & bytes)
        {
            const Result<Radiotap> radiotap = parse(bytes);
            return radiotap.has_value() ? ErrorCode::none : radiotap.error().code;
        }
};

TEST_F(TestRadiotap, test_radiotap_header_without_fields)
{
    const std::vector<uint8_t> bytes = {0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00};
    const Result<Radiotap> radiotap = parse(bytes);

    ASSERT_TRUE(radiotap.has_value());
    EXPECT_EQ(radiotap->header_size, 8u);
    EXPECT_EQ(radiotap->payload.size(), 2u);
    EXPECT_EQ(radiotap->payload[0], 0x80);
    EXPECT_FALSE(radiotap->signal_dbm.has_value());
    EXPECT_FALSE(radiotap->frequency_mhz.has_value());
}

TEST_F(TestRadiotap, test_radiotap_common_fields)
{
    const Result<Radiotap> radiotap = parse(
        {0x00, 0x00, 0x10, 0x00, 0x2e, 0x08, 0x00, 0x00, 0x02, 0x0c, 0x6c, 0x09, 0xa0, 0x00, 0xd0, 0x01, 0x80, 0x00});

    ASSERT_TRUE(radiotap.has_value());
    EXPECT_EQ(radiotap->header_size, 16u);
    EXPECT_TRUE(radiotap->short_preamble);
    EXPECT_FALSE(radiotap->fcs_at_end);
    EXPECT_EQ(radiotap->rate_kbps, 6000u);
    EXPECT_EQ(radiotap->frequency_mhz, 2412u);
    EXPECT_EQ(radiotap->channel_flags, 0x00a0u);
    EXPECT_EQ(radiotap->signal_dbm, -48);
    EXPECT_EQ(radiotap->antenna, 1u);
    EXPECT_EQ(radiotap->payload.size(), 2u);
}

TEST_F(TestRadiotap, test_radiotap_aligns_from_the_header_start)
{
    // flags ends on an odd offset, the channel skips the 0xff padding byte
    const Result<Radiotap> radiotap = parse(
        {0x00, 0x00, 0x0f, 0x00, 0x2a, 0x00, 0x00, 0x00, 0x00, 0xff, 0x6c, 0x09, 0x80, 0x00, 0xc4});

    ASSERT_TRUE(radiotap.has_value());
    EXPECT_EQ(radiotap->frequency_mhz, 2412u);
    EXPECT_EQ(radiotap->channel_flags, 0x0080u);
    EXPECT_EQ(radiotap->signal_dbm, -60);
    EXPECT_EQ(radiotap->payload.size(), 0u);
}

TEST_F(TestRadiotap, test_radiotap_tsft)
{
    const Result<Radiotap> radiotap = parse(
        {0x00, 0x00, 0x11, 0x00, 0x21, 0x00, 0x00, 0x00, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0xd8});

    ASSERT_TRUE(radiotap.has_value());
    EXPECT_EQ(radiotap->tsft_us, 0x0102030405060708u);
    EXPECT_EQ(radiotap->signal_dbm, -40);
}

TEST_F(TestRadiotap, test_radiotap_extended_presence_words)
{
    const Result<Radiotap> radiotap = parse(
        {0x00, 0x00, 0x0d, 0x00, 0x02, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x04});

    ASSERT_TRUE(radiotap.has_value());
    EXPECT_EQ(radiotap->header_size, 13u);
    EXPECT_TRUE(radiotap->wep);
    EXPECT_FALSE(radiotap->short_preamble);
}

TEST_F(TestRadiotap, test_radiotap_signal_per_antenna)
{
    const Result<Radiotap> radiotap = parse({0x00, 0x00, 0x16, 0x00, 0x20, 0x08, 0x00, 0xa0, 0x20, 0x08, 0x00,
                                             0xa0, 0x20, 0x08, 0x00, 0x00, 0xc8, 0x00, 0xc4, 0x01, 0xc2, 0x02});

    ASSERT_TRUE(radiotap.has_value());
    EXPECT_EQ(radiotap->signal_dbm, -56);
    EXPECT_EQ(radiotap->antenna, 0u);
    ASSERT_EQ(radiotap->antenna_signals.size(), 2u);
    EXPECT_EQ(radiotap->antenna_signals[0].antenna, 1u);
    EXPECT_EQ(radiotap->antenna_signals[0].signal_dbm, -60);
    EXPECT_EQ(radiotap->antenna_signals[1].antenna, 2u);
    EXPECT_EQ(radiotap->antenna_signals[1].signal_dbm, -62);
}

TEST_F(TestRadiotap, test_radiotap_skips_vendor_namespace)
{
    const Result<Radiotap> radiotap = parse({0x00, 0x00, 0x1d, 0x00, 0x02, 0x00, 0x00, 0xc0, 0x01, 0x00,
                                             0x00, 0xa0, 0x20, 0x08, 0x00, 0x00, 0x08, 0x00, 0x00, 0x11,
                                             0x22, 0x00, 0x03, 0x00, 0xaa, 0xbb, 0xcc, 0xd8, 0x01});

    ASSERT_TRUE(radiotap.has_value());
    EXPECT_TRUE(radiotap->fragmented);
    EXPECT_FALSE(radiotap->signal_dbm.has_value());
    ASSERT_EQ(radiotap->antenna_signals.size(), 1u);
    EXPECT_EQ(radiotap->antenna_signals[0].antenna, 1u);
    EXPECT_EQ(radiotap->antenna_signals[0].signal_dbm, -40);
}

TEST_F(TestRadiotap, test_radiotap_fcs_and_unknown_field)
{
    // bit 28 (tlv) stops the walk after the flags
    const std::vector<uint8_t> bytes =
        {0x00, 0x00, 0x0c, 0x00, 0x02, 0x00, 0x00, 0x10, 0x50, 0x00, 0x00, 0x00, 0x80, 0x00, 0xde, 0xad, 0xbe, 0xef};
    const Result<Radiotap> radiotap = parse(bytes);

    ASSERT_TRUE(radiotap.has_value());
    EXPECT_TRUE(radiotap->fcs_at_end);
    EXPECT_TRUE(radiotap->bad_fcs);
    EXPECT_EQ(radiotap->payload.size(), 2u);
    EXPECT_EQ(radiotap->payload[0], 0x80);
    EXPECT_FALSE(radiotap->signal_dbm.has_value());
}

TEST_F(TestRadiotap, test_radiotap_mcs)
{
    const Result<Radiotap> forty = parse({0x00, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x08, 0x00, 0x07, 0x05, 0x07});
    const Result<Radiotap> twenty = parse({0x00, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x08, 0x00, 0x07, 0x00, 0x0b});
    const Result<Radiotap> features = parse({0x00, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x08, 0x00, 0x3f, 0x58, 0x0f});
    const Result<Radiotap> unknown_guard = parse({0x00, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x08, 0x00, 0x03, 0x01, 0x07});
    const Result<Radiotap> beyond_table = parse({0x00, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x08, 0x00, 0x07, 0x01, 0x28});
    const Result<Radiotap> format_unknown = parse({0x00, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x08, 0x00, 0x37, 0x58, 0x0f});
    const Result<Radiotap> index_unknown = parse({0x00, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x08, 0x00, 0x01, 0x01, 0x07});

    ASSERT_TRUE(forty.has_value());
    ASSERT_TRUE(twenty.has_value());
    ASSERT_TRUE(features.has_value());
    ASSERT_TRUE(unknown_guard.has_value());
    ASSERT_TRUE(beyond_table.has_value());
    ASSERT_TRUE(format_unknown.has_value());
    ASSERT_TRUE(index_unknown.has_value());

    ASSERT_TRUE(forty->mcs.has_value());
    EXPECT_EQ(forty->mcs->index, 7u);
    EXPECT_EQ(forty->mcs->bandwidth_mhz, 40u);
    EXPECT_TRUE(forty->mcs->short_gi);
    EXPECT_EQ(forty->mcs->rate_kbps, 150000u);

    ASSERT_TRUE(twenty->mcs.has_value());
    EXPECT_EQ(twenty->mcs->index, 11u);
    EXPECT_EQ(twenty->mcs->bandwidth_mhz, 20u);
    EXPECT_FALSE(twenty->mcs->short_gi);
    EXPECT_EQ(twenty->mcs->rate_kbps, 52000u);

    ASSERT_TRUE(features->mcs.has_value());
    EXPECT_TRUE(features->mcs->ldpc);
    EXPECT_TRUE(features->mcs->greenfield);
    EXPECT_EQ(features->mcs->stbc_streams, 2u);
    EXPECT_EQ(features->mcs->rate_kbps, 130000u);

    ASSERT_TRUE(unknown_guard->mcs.has_value());
    EXPECT_EQ(unknown_guard->mcs->bandwidth_mhz, 40u);
    EXPECT_FALSE(unknown_guard->mcs->rate_kbps.has_value());

    ASSERT_TRUE(beyond_table->mcs.has_value());
    EXPECT_EQ(beyond_table->mcs->index, 40u);
    EXPECT_FALSE(beyond_table->mcs->rate_kbps.has_value());

    ASSERT_TRUE(format_unknown->mcs.has_value());
    EXPECT_FALSE(format_unknown->mcs->greenfield);
    EXPECT_FALSE(index_unknown->mcs.has_value());
}

TEST_F(TestRadiotap, test_radiotap_rejects_malformed_headers)
{
    EXPECT_EQ(failure({0x00, 0x00, 0x08}), ErrorCode::invalid_argument);
    EXPECT_EQ(failure({0x01, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00}), ErrorCode::invalid_argument);
    EXPECT_EQ(failure({0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00}), ErrorCode::invalid_argument);
    EXPECT_EQ(failure({0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00}), ErrorCode::invalid_argument);
    EXPECT_EQ(failure({0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x80}), ErrorCode::invalid_argument);
    EXPECT_EQ(failure({0x00, 0x00, 0x0a, 0x00, 0x08, 0x00, 0x00, 0x00, 0x6c, 0x09}), ErrorCode::invalid_argument);
    EXPECT_EQ(failure({0x00, 0x00, 0x09, 0x00, 0x02, 0x00, 0x00, 0x00, 0x10, 0x01, 0x02}), ErrorCode::invalid_argument);
}

} // namespace test
