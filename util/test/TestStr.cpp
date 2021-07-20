#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Str.hpp>
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
        std::vector<std::string> split1 = Str::split("hello world", " ");
        EXPECT_EQ(split1.size(), 2u);
        EXPECT_EQ(split1[0], "hello");
        EXPECT_EQ(split1[1], "world");
        std::vector<std::string> split2 = Str::split(" hello  world ", " ");
        EXPECT_EQ(split2.size(), 2u);
        EXPECT_EQ(split2[0], "hello");
        EXPECT_EQ(split2[1], "world");
        std::vector<std::string> split3 = Str::split("hell", "lo");
        EXPECT_EQ(split3.size(), 1u);
        EXPECT_EQ(split3[0], "hell");
        std::vector<std::string> split4 = Str::split("hell", "hell");
        EXPECT_EQ(split4.size(), 0u);
        std::vector<std::string> split5 = Str::split("", "");
        EXPECT_EQ(split5.size(), 1u);
        EXPECT_EQ(split5[0], "");
        std::vector<std::string> split6 = Str::split("hello", "");
        EXPECT_EQ(split6.size(), 1u);
        EXPECT_EQ(split6[0], "hello");
    }

    TEST_F(TestStr, test_str_join)
    {
        std::string res;
        res = Str::join({"hello", "world"}, ",");
        EXPECT_EQ(res, "hello,world");
        res = Str::join({"hello", "good", "world"}, " . ");
        EXPECT_EQ(res, "hello . good . world");
        res = Str::join({"", "", "sup"}, ",");
        EXPECT_EQ(res, ",,sup");
    }

    TEST_F(TestStr, test_str_format)
    {
        std::string res;
        res = Str::format("%s -> %d", "hello", 1337);
        EXPECT_EQ(res, "hello -> 1337");
    }

    TEST_F(TestStr, test_str_demangle)
    {
        std::string res = Str::demangle(typeid(*this).name());
        LOG(info, "Demangled: " << res);
        EXPECT_EQ(res, "test::TestStr_test_str_demangle_Test");
    }

    TEST_F(TestStr, test_str_trim)
    {
        EXPECT_EQ(Str::trim(""), "");
        EXPECT_EQ(Str::trim("h"), "h");
        EXPECT_EQ(Str::trim("h "), "h");
        EXPECT_EQ(Str::trim(" h"), "h");
        EXPECT_EQ(Str::trim(" \t hello  world      \t "), "hello  world");
    }

    TEST_F(TestStr, test_str_lowerupper)
    {
        std::string s = "Hello World @123";
        EXPECT_EQ(Str::to_lower(s), "hello world @123");
        EXPECT_EQ(Str::to_upper(s), "HELLO WORLD @123");
    }

    TEST_F(TestStr, test_str_replace)
    {
        EXPECT_EQ(Str::replace("hello wurld", "wurld", "world"), "hello world");
        EXPECT_EQ(Str::replace("its nope", "something", "other"), "its nope");
        EXPECT_EQ(Str::replace("wibbly wobbly timy wimey stuff", "i", "iii"),
                    "wiiibbly wobbly tiiimy wiiimey stuff");
        EXPECT_EQ(Str::replace("heh ih", "h", "hhh"), "hhhehhh ihhh");
        EXPECT_EQ(Str::replace("hello", "", ""), "hello");
    }

    TEST_F(TestStr, test_str_hexdump)
    {
        std::string s = "hello world - how are you";
        TRACE(Str::addr_to_string(s.data()));
        EXPECT_EQ(Str::addr_to_string(0x0, 5), "0x00000");
        EXPECT_EQ(Str::num_to_string(312, 10), "312");
        EXPECT_EQ(Str::num_to_string(16, 16), "10");
        EXPECT_EQ(Str::num_to_string(15, 16), "f");
        EXPECT_EQ(Str::num_to_string(255, 16), "ff");
        EXPECT_EQ(Str::hexdump(s.data(), s.length(), ','), "68,65,6c,6c,6f,20,77,6f,72,6c,64,20,2d,20,68,6f,77,20,61,72,65,20,79,6f,75");
        std::string dump = Str::hexdump_fmt(s.data(), s.length());
        std::cout << dump << std::endl;
        auto splits = Str::split(dump, "\n");
        EXPECT_EQ(splits.size(), 4u);
        EXPECT_EQ(splits[0], "0x0:\t68 65 6c 6c 6f 20 77 6f   hello wo");
        EXPECT_EQ(splits[1], "0x8:\t72 6c 64 20 2d 20 68 6f   rld - ho");
        EXPECT_EQ(splits[2], "0x10:\t77 20 61 72 65 20 79 6f   w are yo");
        EXPECT_EQ(splits[3], "0x18:\t75                        u       ");
    }

    TEST_F(TestStr, test_str_with)
    {
        EXPECT_TRUE(Str::starts_with("hello world", "hello"));
        EXPECT_TRUE(Str::starts_with("hello world", "h"));
        EXPECT_FALSE(Str::starts_with("hello world", "ello"));
        EXPECT_FALSE(Str::starts_with("hello world", "world"));

        EXPECT_TRUE(Str::ends_with("hello world", "world"));
        EXPECT_TRUE(Str::ends_with("hello world", "d"));
        EXPECT_FALSE(Str::ends_with("hello world", "worl"));
        EXPECT_FALSE(Str::ends_with("hello world", "hello"));

        EXPECT_FALSE(Str::starts_with("h", "he"));
        EXPECT_FALSE(Str::ends_with("h", "hello"));
    }
}
