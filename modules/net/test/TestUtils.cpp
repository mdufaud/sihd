#include <array>
#include <cstdint>

#include <gtest/gtest.h>

#include <sihd/net/utils.hpp>
#include <sihd/util/Logger.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::net;
using namespace sihd::util;

class TestUtils: public ::testing::Test
{
    protected:
        TestUtils() { LoggerManager::stream(); }

        virtual ~TestUtils() { LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestUtils, test_checksum_rfc1071_vector)
{
    const std::array<uint8_t, 8> bytes = {0x00, 0x01, 0xf2, 0x03, 0xf4, 0xf5, 0xf6, 0xf7};

    EXPECT_EQ(utils::checksum(ArrUByteView(bytes.data(), bytes.size())), 0x220d);
}

TEST_F(TestUtils, test_checksum_all_ones_is_zero)
{
    const std::array<uint8_t, 8> bytes = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

    EXPECT_EQ(utils::checksum(ArrUByteView(bytes.data(), bytes.size())), 0);
    EXPECT_EQ(utils::checksum(ArrUByteView(bytes.data(), 2)), 0);
}

TEST_F(TestUtils, test_checksum_odd_byte_is_padded)
{
    const std::array<uint8_t, 3> bytes = {0x01, 0x02, 0x03};

    EXPECT_EQ(utils::checksum(ArrUByteView(bytes.data(), bytes.size())), 0xfbfd);
}

TEST_F(TestUtils, test_checksum_empty_data)
{
    EXPECT_EQ(utils::checksum(ArrUByteView()), 0xffff);
}

TEST_F(TestUtils, test_checksum_appended_verifies_to_zero)
{
    std::array<uint8_t, 10> bytes = {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0, 0x00, 0x00};

    const uint16_t sum = utils::checksum(ArrUByteView(bytes.data(), 8));
    bytes[8] = static_cast<uint8_t>(sum >> 8);
    bytes[9] = static_cast<uint8_t>(sum);

    EXPECT_EQ(sum, 0x1da6);
    EXPECT_EQ(utils::checksum(ArrUByteView(bytes.data(), bytes.size())), 0);
}

TEST_F(TestUtils, test_checksum_chains_a_pseudo_header)
{
    const std::array<uint8_t, 8> bytes = {0x00, 0x01, 0xf2, 0x03, 0xf4, 0xf5, 0xf6, 0xf7};

    const uint32_t header = utils::checksum_add(ArrUByteView(bytes.data(), 4));

    EXPECT_EQ(header, 0xf204u);
    EXPECT_EQ(utils::checksum(ArrUByteView(bytes.data() + 4, 4), header), 0x220d);
}

} // namespace test
