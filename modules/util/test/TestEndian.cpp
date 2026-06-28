#include <gtest/gtest.h>
#include <sihd/util/Logger.hpp>
#include <sihd/util/endian.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;
class TestEndian: public ::testing::Test
{
    protected:
        TestEndian() { sihd::util::LoggerManager::stream(); }

        virtual ~TestEndian() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp()
        {
            if (std::endian::native != std::endian::little)
                GTEST_SKIP() << "Those tests are assumed to be in little endianness";
        }

        virtual void TearDown() {}
};

TEST_F(TestEndian, test_endian_switch_buffer)
{
    uint16_t buffer[4] = {0x1234, 0x4242, 0x1337, 0x0420};

    endian::switch_buffer_endianness(buffer, ByteSize::b2, 4);
    EXPECT_EQ(buffer[0], 0x3412);
    EXPECT_EQ(buffer[1], 0x4242);
    EXPECT_EQ(buffer[2], 0x3713);
    EXPECT_EQ(buffer[3], 0x2004);
    endian::switch_buffer_endianness(buffer, ByteSize::b2, 4);
    EXPECT_EQ(buffer[0], 0x1234);
    EXPECT_EQ(buffer[1], 0x4242);
    EXPECT_EQ(buffer[2], 0x1337);
    EXPECT_EQ(buffer[3], 0x0420);
}

TEST_F(TestEndian, test_endian_swap)
{
    int16_t val16 = (int16_t)0x1337;
    EXPECT_EQ((int16_t)0x3713, endian::swap(val16));
    int32_t val32 = (int32_t)0x12345678;
    EXPECT_EQ((int32_t)0x78563412, endian::swap(val32));
    int64_t val64 = (int64_t)0x1122334455667788;
    EXPECT_EQ((int64_t)0x8877665544332211, endian::swap(val64));

    uint16_t uval16 = (uint16_t)0x1337;
    EXPECT_EQ((uint16_t)0x3713, endian::swap(uval16));
    uint32_t uval32 = (uint32_t)0x12345678;
    EXPECT_EQ((uint32_t)0x78563412, endian::swap(uval32));
    uint64_t uval64 = (uint64_t)0x1122334455667788;
    EXPECT_EQ((uint64_t)0x8877665544332211, endian::swap(uval64));

    val32 = 0x12345;
    uint32_t expected_val32 = 0x45230100;
    int32_t *val32_p = &val32;
    float fval = *(reinterpret_cast<float *>(val32_p));
    fval = endian::swap(fval);
    float *fval_swapped_p = &fval;
    EXPECT_EQ(expected_val32, *(reinterpret_cast<uint32_t *>(fval_swapped_p)));

    val64 = 0x12345;
    uint64_t expected_val64 = 0x4523010000000000;
    int64_t *val64_p = &val64;
    double dval = *(reinterpret_cast<double *>(val64_p));
    dval = endian::swap(dval);
    double *dval_swapped_p = &dval;
    EXPECT_EQ(expected_val64, *(reinterpret_cast<uint64_t *>(dval_swapped_p)));
}

TEST_F(TestEndian, test_endian_convert)
{
    const uint16_t u16 = 0x1234;
    const uint32_t u32 = 0x12345678;
    const uint64_t u64 = 0x1234567890abcdefULL;
    uint8_t *ptr;

    uint16_t u16_big = endian::convert<uint16_t, std::endian::big>(u16);
    uint16_t u16_little = endian::convert<uint16_t, std::endian::little>(u16);

    // Asserting the right endianness for the test
    ptr = (uint8_t *)&u16;
    EXPECT_EQ((uint8_t)0x34, ptr[0]);
    EXPECT_EQ((uint8_t)0x12, ptr[1]);

    ptr = (uint8_t *)&u16_big;
    EXPECT_EQ((uint8_t)0x12, ptr[0]);
    EXPECT_EQ((uint8_t)0x34, ptr[1]);

    ptr = (uint8_t *)&u16_little;
    EXPECT_EQ((uint8_t)0x34, ptr[0]);
    EXPECT_EQ((uint8_t)0x12, ptr[1]);

    uint32_t u32_big = endian::convert<uint32_t, std::endian::big>(u32);
    uint32_t u32_little = endian::convert<uint32_t>(u32, std::endian::little);

    ptr = (uint8_t *)&u32_big;
    EXPECT_EQ((uint8_t)0x12, ptr[0]);
    EXPECT_EQ((uint8_t)0x34, ptr[1]);
    EXPECT_EQ((uint8_t)0x56, ptr[2]);
    EXPECT_EQ((uint8_t)0x78, ptr[3]);

    ptr = (uint8_t *)&u32_little;
    EXPECT_EQ((uint8_t)0x12, ptr[3]);
    EXPECT_EQ((uint8_t)0x34, ptr[2]);
    EXPECT_EQ((uint8_t)0x56, ptr[1]);
    EXPECT_EQ((uint8_t)0x78, ptr[0]);

    uint64_t u64_big = endian::convert(u64, std::endian::big);
    uint64_t u64_little = endian::convert<uint64_t, std::endian::little>(u64);

    ptr = (uint8_t *)&u64_big;
    EXPECT_EQ((uint8_t)0x12, ptr[0]);
    EXPECT_EQ((uint8_t)0x34, ptr[1]);
    EXPECT_EQ((uint8_t)0x56, ptr[2]);
    EXPECT_EQ((uint8_t)0x78, ptr[3]);
    EXPECT_EQ((uint8_t)0x90, ptr[4]);
    EXPECT_EQ((uint8_t)0xab, ptr[5]);
    EXPECT_EQ((uint8_t)0xcd, ptr[6]);
    EXPECT_EQ((uint8_t)0xef, ptr[7]);

    ptr = (uint8_t *)&u64_little;
    EXPECT_EQ((uint8_t)0x12, ptr[7]);
    EXPECT_EQ((uint8_t)0x34, ptr[6]);
    EXPECT_EQ((uint8_t)0x56, ptr[5]);
    EXPECT_EQ((uint8_t)0x78, ptr[4]);
    EXPECT_EQ((uint8_t)0x90, ptr[3]);
    EXPECT_EQ((uint8_t)0xab, ptr[2]);
    EXPECT_EQ((uint8_t)0xcd, ptr[1]);
    EXPECT_EQ((uint8_t)0xef, ptr[0]);

    EXPECT_EQ(u16_little, endian::convert_from(u16_little, std::endian::little));
    EXPECT_EQ(u32_little, endian::convert_from(u32_little, std::endian::little));
    EXPECT_EQ(u64_little, endian::convert_from(u64_little, std::endian::little));
    EXPECT_EQ(u16_little, endian::convert_from(u16_big, std::endian::big));
    EXPECT_EQ(u32_little, endian::convert_from(u32_big, std::endian::big));
    EXPECT_EQ(u64_little, endian::convert_from(u64_big, std::endian::big));
}
} // namespace test
