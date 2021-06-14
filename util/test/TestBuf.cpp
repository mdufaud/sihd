#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Buf.hpp>

namespace test
{
    LOGGER;
    using namespace sihd::util;
    class TestBuf:   public ::testing::Test
    {
        protected:
            TestBuf()
            {
                sihd::util::LoggerManager::basic();
            }

            virtual ~TestBuf()
            {
                sihd::util::LoggerManager::clear_loggers();
            }

            virtual void SetUp()
            {
            }

            virtual void TearDown()
            {
            }

            void    test_buffer(Buf<uint8_t> & buf)
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
        Buf<uint8_t> buf;
        uint8_t buf8[4] = {1, 2, 3, 4};
        buf.from(buf8, 4);
        EXPECT_EQ(buf.size(), 4ul);
        EXPECT_EQ(buf[0], 1);
        EXPECT_EQ(buf[1], 2);
        EXPECT_EQ(buf[2], 3);
        EXPECT_EQ(buf[3], 4);
        EXPECT_THROW(buf.at(4), std::out_of_range);
        buf[1] = 42;
        EXPECT_EQ(buf.at(1), 42);
    }

    TEST_F(TestBuf, test_buf_resize)
    {
        Buf<uint8_t> buf;
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

    TEST_F(TestBuf, test_buf_push_back)
    {
        Buf<uint8_t> buf(20);
        uint8_t buf8[4] = {1, 2, 3, 4};
        LOG(info, "Appending once");
        buf.push_back(buf8, 4);
        EXPECT_EQ(buf.size(), 4ul);
        EXPECT_EQ(buf.capacity(), 20ul);
        EXPECT_EQ(buf[0], 1);
        EXPECT_EQ(buf[1], 2);
        EXPECT_EQ(buf[2], 3);
        EXPECT_EQ(buf[3], 4);
        LOG(info, "Appending another");
        buf.push_back(buf8, 4);
        EXPECT_EQ(buf.size(), 8ul);
        EXPECT_EQ(buf.capacity(), 20ul);
        this->test_buffer(buf);
        LOG(info, "Appending time 2");
        buf.push_back(buf8, 4);
        buf.push_back(buf8, 4);
        EXPECT_EQ(buf.size(), 16ul);
        EXPECT_EQ(buf.capacity(), 20ul);
        LOG(info, "Appending to limit");
        buf.push_back(buf8, 4);
        EXPECT_EQ(buf.size(), 20ul);
        EXPECT_EQ(buf.capacity(), 20ul);
        this->test_buffer(buf);
        LOG(info, "Appending after limit");
        buf.push_back(buf8, 4);
        EXPECT_EQ(buf.size(), 24ul);
        EXPECT_EQ(buf.capacity(), 24ul);
        this->test_buffer(buf);
    }

    TEST_F(TestBuf, test_buf_assign)
    {
        uint8_t *buf8 = new uint8_t[4]{1, 2, 3, 4};
        Buf<uint8_t> buf(8);
        buf.push_back(buf8, 4);
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
    TEST_F(TestBuf, test_buf_values)
    {
        Buf<float> buf(4);
        buf.push_back(0.1);
        buf.push_back(0.2);
        buf.push_back(0.3);
        buf.push_back(0.4);
        EXPECT_EQ(buf.size(), 4ul);
        EXPECT_EQ(buf.capacity(), 4ul);
        buf.push_back(0.5);
        EXPECT_EQ(buf.size(), 5ul);
        EXPECT_EQ(buf.capacity(), 5ul);
        EXPECT_EQ(buf[4], 0.5);
    }

}
