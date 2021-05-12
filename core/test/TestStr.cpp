#include <gtest/gtest.h>
#include <sihd/core/str.hpp>

namespace test
{
    using namespace sihd::core;

    class TestStr:   public ::testing::Test
    {
        protected:
            TestStr()
            {}
            virtual ~TestStr()
            {}
            virtual void SetUp()
            {}
            virtual void TearDown()
            {}
    };

    TEST_F(TestStr, test_split)
    {
        std::vector<std::string> split1 = str::split("hello world", " ");
        EXPECT_EQ(split1.size(), 2u);
        EXPECT_EQ(split1[0], "hello");
        EXPECT_EQ(split1[1], "world");
        std::vector<std::string> split2 = str::split(" hello  world ", " ");
        EXPECT_EQ(split2.size(), 2u);
        EXPECT_EQ(split2[0], "hello");
        EXPECT_EQ(split2[1], "world");
        std::vector<std::string> split3 = str::split("hell", "lo");
        EXPECT_EQ(split3.size(), 1u);
        EXPECT_EQ(split3[0], "hell");
        std::vector<std::string> split4 = str::split("hell", "hell");
        EXPECT_EQ(split4.size(), 0u);
        std::vector<std::string> split5 = str::split("", "");
        EXPECT_EQ(split5.size(), 1u);
        EXPECT_EQ(split5[0], "");
        std::vector<std::string> split6 = str::split("hello", "");
        EXPECT_EQ(split6.size(), 1u);
        EXPECT_EQ(split6[0], "hello");
    }
}
