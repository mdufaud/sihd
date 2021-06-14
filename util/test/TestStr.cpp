#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/str.hpp>
#include <sihd/util/Logger.hpp>

namespace test
{
    using namespace sihd::util;

    LOGGER;

    class TestStr:   public ::testing::Test
    {
        protected:
            TestStr()
            {
                sihd::util::LoggerManager::basic();
            }

            virtual ~TestStr()
            {
                sihd::util::LoggerManager::clear_loggers();
            }

            virtual void SetUp()
            {
            }

            virtual void TearDown()
            {
            }
    };

    TEST_F(TestStr, test_str_split)
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

    TEST_F(TestStr, test_str_join)
    {
        std::string res;
        res = str::join({"hello", "world"}, ",");
        EXPECT_EQ(res, "hello,world");
        res = str::join({"hello", "good", "world"}, " . ");
        EXPECT_EQ(res, "hello . good . world");
        res = str::join({"", "", "sup"}, ",");
        EXPECT_EQ(res, ",,sup");
    }

    TEST_F(TestStr, test_str_format)
    {
        std::string res;
        res = str::format("%s -> %d", "hello", 1337);
        EXPECT_EQ(res, "hello -> 1337");
    }

    TEST_F(TestStr, test_str_demangle)
    {
        std::string res = str::demangle(typeid(*this).name());
        LOG(info, "Demangled: " << res);
        EXPECT_EQ(res, "test::TestStr_test_str_demangle_Test");
    }

    TEST_F(TestStr, test_str_trim)
    {
        EXPECT_EQ(str::trim(""), "");
        EXPECT_EQ(str::trim("h"), "h");
        EXPECT_EQ(str::trim("h "), "h");
        EXPECT_EQ(str::trim(" h"), "h");
        EXPECT_EQ(str::trim(" \t hello  world      \t "), "hello  world");
    }

    TEST_F(TestStr, test_str_lowerupper)
    {
        std::string s = "Hello World @123";
        EXPECT_EQ(str::to_lower(s), "hello world @123");
        EXPECT_EQ(str::to_upper(s), "HELLO WORLD @123");
    }

    TEST_F(TestStr, test_str_replace)
    {
        EXPECT_EQ(str::replace("hello wurld", "wurld", "world"), "hello world");
        EXPECT_EQ(str::replace("its nope", "something", "other"), "its nope");
        EXPECT_EQ(str::replace("wibbly wobbly timy wimey stuff", "i", "iii"),
                    "wiiibbly wobbly tiiimy wiiimey stuff");
        EXPECT_EQ(str::replace("heh ih", "h", "hhh"), "hhhehhh ihhh");
        EXPECT_EQ(str::replace("hello", "", ""), "hello");
    }
}
