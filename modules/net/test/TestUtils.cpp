#include <bit>
#include <cstdint>

#include <gtest/gtest.h>

#include <sihd/net/utils.hpp>
#include <sihd/util/Logger.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::net;

class TestUtils: public ::testing::Test
{
    protected:
        TestUtils() { sihd::util::LoggerManager::stream(); }

        virtual ~TestUtils() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestUtils, test_checksum_rfc1071_vector)
{
    uint16_t words[] = {0x0001, 0xf203, 0xf4f5, 0xf6f7};
    EXPECT_EQ(utils::checksum(words, (int)sizeof(words)), 0x220d);
}

TEST_F(TestUtils, test_checksum_all_ones_is_zero)
{
    uint16_t words[] = {0xffff, 0xffff, 0xffff, 0xffff};
    EXPECT_EQ(utils::checksum(words, (int)sizeof(words)), 0);
    uint16_t single = 0xffff;
    EXPECT_EQ(utils::checksum(&single, (int)sizeof(single)), 0);
}

TEST_F(TestUtils, test_checksum_odd_byte)
{
    alignas(uint16_t) unsigned char bytes[] = {0x01, 0x02, 0x03};
    const uint16_t expected = (std::endian::native == std::endian::little) ? 0xfdfb : 0xfbfd;
    EXPECT_EQ(utils::checksum((uint16_t *)bytes, (int)sizeof(bytes)), expected);
}

TEST_F(TestUtils, test_checksum_empty_data)
{
    uint16_t word = 0x1234;
    EXPECT_EQ(utils::checksum(&word, 0), 0xffff);
}

TEST_F(TestUtils, test_checksum_appended_verifies_to_zero)
{
    uint16_t words[] = {0x1234, 0x5678, 0x9abc, 0xdef0};
    const uint16_t sum = utils::checksum(words, (int)sizeof(words));
    uint16_t with_checksum[] = {0x1234, 0x5678, 0x9abc, 0xdef0, sum};
    EXPECT_EQ(utils::checksum(with_checksum, (int)sizeof(with_checksum)), 0);
}

} // namespace test
