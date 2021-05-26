#include <gtest/gtest.h>
#include <iostream>
#include <sihd/core/Logger.hpp>
#include <sihd/core/Buf.hpp>

namespace test
{
    LOGGER;
    using namespace sihd::core;
    class TestBuf:   public ::testing::Test
    {
        protected:
            TestBuf()
            {
                sihd::core::LoggerManager::basic();
            }

            virtual ~TestBuf()
            {
                sihd::core::LoggerManager::clear_loggers();
            }

            virtual void SetUp()
            {
            }

            virtual void TearDown()
            {
            }

            void    test_buffer(Buf & buf)
            {
                size_t i = 0;
                while (i < buf.size())
                {
                    int j = i % 4;
                    EXPECT_EQ(buf[i], j + 1);
                    ++i;
                }
            }
    };

    TEST_F(TestBuf, test_buf_read_write)
    {
        Buf buf;
        uint8_t buf8[4] = {1, 2, 3, 4};
        buf.from(buf8, 4);
        EXPECT_EQ(buf.size(), 4ul);
        EXPECT_EQ(buf[0], 1);
        EXPECT_EQ(buf[1], 2);
        EXPECT_EQ(buf[2], 3);
        EXPECT_EQ(buf[3], 4);
        EXPECT_THROW(buf.at(4), std::out_of_range);

        EXPECT_TRUE(buf.write(1, 42));
        EXPECT_EQ(buf.at(1), 42);
        EXPECT_FALSE(buf.write(4, 42));
    }

    TEST_F(TestBuf, test_buf_resize)
    {
        Buf buf;
        EXPECT_EQ(buf.size(), 0ul);
        EXPECT_EQ(buf.capacity(), 0ul);
        uint8_t buf8[4] = {1, 2, 3, 4};
        buf.from(buf8, 4);
        this->test_buffer(buf);
        EXPECT_EQ(buf.size(), 4ul);
        EXPECT_EQ(buf.capacity(), 4ul);
        buf.resize(8);
        this->test_buffer(buf);
        EXPECT_EQ(buf.size(), 4ul);
        EXPECT_EQ(buf.capacity(), 8ul);
        EXPECT_EQ(buf.at(7), 0);
    }

    TEST_F(TestBuf, test_buf_append)
    {
        Buf buf(20);
        uint8_t buf8[4] = {1, 2, 3, 4};
        LOG(info, "Appending once");
        buf.append(buf8, 4);
        EXPECT_EQ(buf.size(), 4ul);
        EXPECT_EQ(buf.capacity(), 20ul);
        EXPECT_EQ(buf[0], 1);
        EXPECT_EQ(buf[1], 2);
        EXPECT_EQ(buf[2], 3);
        EXPECT_EQ(buf[3], 4);
        LOG(info, "Appending another");
        buf.append(buf8, 4);
        EXPECT_EQ(buf.size(), 8ul);
        EXPECT_EQ(buf.capacity(), 20ul);
        this->test_buffer(buf);
        LOG(info, "Appending time 2");
        buf.append(buf8, 4);
        buf.append(buf8, 4);
        EXPECT_EQ(buf.size(), 16ul);
        EXPECT_EQ(buf.capacity(), 20ul);
        LOG(info, "Appending to limit");
        buf.append(buf8, 4);
        EXPECT_EQ(buf.size(), 20ul);
        EXPECT_EQ(buf.capacity(), 20ul);
        this->test_buffer(buf);
        LOG(info, "Appending after limit");
        buf.append(buf8, 4);
        EXPECT_EQ(buf.size(), 24ul);
        EXPECT_EQ(buf.capacity(), 24ul);
        this->test_buffer(buf);
    }

    TEST_F(TestBuf, test_buf_assign)
    {
        uint8_t *buf8 = new uint8_t[4]{1, 2, 3, 4};
        Buf buf(buf8, 4, 8);
        EXPECT_EQ(buf.size(), 4ul);
        EXPECT_EQ(buf.capacity(), 8ul);
        EXPECT_EQ(buf[0], 1);
        EXPECT_EQ(buf[1], 2);
        EXPECT_EQ(buf[2], 3);
        EXPECT_EQ(buf[3], 4);
        EXPECT_NE(buf.buf(), buf8);
        buf.assign(buf8, 4);
        EXPECT_EQ(buf.buf(), buf8);
        EXPECT_EQ(buf.size(), 4ul);
        EXPECT_EQ(buf.capacity(), 4ul);
        buf.assign(nullptr, 0);
        EXPECT_EQ(buf.buf(), nullptr);
        EXPECT_EQ(buf.size(), 0ul);
        EXPECT_EQ(buf.capacity(), 0ul);
    }

}
