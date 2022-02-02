#include <gtest/gtest.h>
#include <sihd/util/Str.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Splitter.hpp>
#include <sihd/util/time.hpp>

#include <iostream>
#include <climits>

#include <errno.h>

#include <sihd/util/version.hpp>

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

            bool    test_long(const std::string & str, int base = 0)
            {
                long val;
                bool ret = Str::to_long(str, &val, base);
                LOG(debug, "Test signed long (errno = " << errno << ") -> " << str);
                if (ret)
                {
                    LOG(debug, "Value found: " << val);
                    _val = val;
                    return true;
                }
                LOG(debug, "Failed to find a signed long");
                return ret;
            }

            bool    test_ulong(const std::string & str, int base = 0)
            {
                unsigned long val;
                bool ret = Str::to_ulong(str, &val, base);
                LOG(debug, "Test unsigned long (errno = " << errno << ") -> " << str);
                if (ret)
                {
                    LOG(debug, "Value found: " << val);
                    _uval = val;
                    return true;
                }
                LOG(debug, "Failed to find an unsigned long");
                return ret;
            }

            bool    test_double(const std::string & str)
            {
                double val;
                bool ret = Str::to_double(str, &val);
                LOG(debug, "Test double (errno = " << errno << ") -> " << str);
                if (ret)
                {
                    LOG(debug, "Value found: " << val);
                    _dval = val;
                    return true;
                }
                LOG(debug, "Failed to find a double");
                return ret;
            }

            long _val;
            unsigned long _uval;
            double _dval;
    };

    TEST_F(TestStr, test_str_remove_escape_char)
    {
        std::string escaped = Str::remove_escape_char("\\hello\\ world");
        EXPECT_EQ(escaped, "hello world");
        EXPECT_EQ(escaped.size(), strlen("hello world"));

        escaped = Str::remove_escape_char("\\\\hello\\\\ world");
        EXPECT_EQ(escaped, "\\hello\\ world");

        escaped = Str::remove_escape_char("");
        EXPECT_EQ(escaped, "");
    }

    TEST_F(TestStr, test_str_remove_escape_sequences)
    {
        std::string escaped = Str::remove_escape_sequences("'hello world'");
        EXPECT_EQ(escaped, "hello world");
        EXPECT_EQ(escaped.size(), strlen("hello world"));

        escaped = Str::remove_escape_sequences("'hello '(world)'");
        EXPECT_EQ(escaped, "hello world");
        escaped = Str::remove_escape_sequences("hello ''([world])");
        EXPECT_EQ(escaped, "hello [world]");
        escaped = Str::remove_escape_sequences("\\'hello \\'world");
        EXPECT_EQ(escaped, "\\'hello \\'world");
        escaped = Str::remove_escape_sequences("");
        EXPECT_EQ(escaped, "");
    }

    TEST_F(TestStr, test_str_time2str)
    {
        std::string time_str = Str::gmtime_to_string(time::micro(123));
        EXPECT_EQ(time_str, "+123us");
        time_str = Str::gmtime_to_string(time::milli(1));
        EXPECT_EQ(time_str, "+1ms:0us");
        time_str = Str::gmtime_to_string(time::sec(12) + time::micro(12));
        EXPECT_EQ(time_str, "+12s:0ms:12us");
        time_str = Str::gmtime_to_string(time::hour(12));
        EXPECT_EQ(time_str, "+12h:0m:0s:0ms:0us");
        time_str = Str::gmtime_to_string(time::hour(24));
        EXPECT_EQ(time_str, "+1d::0h:0m:0s:0ms:0us");
        time_str = Str::gmtime_to_string(time::day(24));
        EXPECT_EQ(time_str, "+24d::0h:0m:0s:0ms:0us");
        time_str = Str::gmtime_to_string(time::day(31));
        EXPECT_EQ(time_str, "+1m:0d::0h:0m:0s:0ms:0us");
        time_str = Str::gmtime_to_string(time::day(365));
        EXPECT_EQ(time_str, "+1y:0m:0d::0h:0m:0s:0ms:0us");
        time_str = Str::gmtime_to_string(time::day(365) * 2);
        EXPECT_EQ(time_str, "+2y:0m:0d::0h:0m:0s:0ms:0us");

        time_str = Str::gmtime_to_string(-time::sec(42));
        EXPECT_EQ(time_str, "-42s:0ms:0us");

        std::string nano_time_str = Str::gmtime_to_string(123, false, true);
        EXPECT_EQ(nano_time_str, "+0us:123ns");
        nano_time_str = Str::gmtime_to_string(-123, true, true);
        EXPECT_EQ(nano_time_str, "-0us:123ns (-123)");
    }

    TEST_F(TestStr, test_str_escapes)
    {
        EXPECT_TRUE(Str::is_escape_sequence_open('"'));
        EXPECT_TRUE(Str::is_escape_sequence_open('\''));
        EXPECT_TRUE(Str::is_escape_sequence_open('{'));
        EXPECT_TRUE(Str::is_escape_sequence_open('('));
        EXPECT_TRUE(Str::is_escape_sequence_open('['));
        EXPECT_TRUE(Str::is_escape_sequence_open('<'));

        EXPECT_TRUE(Str::is_escape_sequence_close('"'));
        EXPECT_TRUE(Str::is_escape_sequence_close('\''));
        EXPECT_TRUE(Str::is_escape_sequence_close('}'));
        EXPECT_TRUE(Str::is_escape_sequence_close(')'));
        EXPECT_TRUE(Str::is_escape_sequence_close(']'));
        EXPECT_TRUE(Str::is_escape_sequence_close('>'));

        EXPECT_EQ(Str::closing_escape_of('"'), '"');
        EXPECT_EQ(Str::closing_escape_of('\''), '\'');
        EXPECT_EQ(Str::closing_escape_of('{'), '}');
        EXPECT_EQ(Str::closing_escape_of('('), ')');
        EXPECT_EQ(Str::closing_escape_of('['), ']');
        EXPECT_EQ(Str::closing_escape_of('<'), '>');

        // \[hello  index[1] is '[' and escaped
        EXPECT_TRUE(Str::is_escaped_char("\\[hello", 1));
        EXPECT_FALSE(Str::is_escaped_char("\\[hello", 0));
        EXPECT_FALSE(Str::is_escaped_char("\\[hello", 2));
        // \\[hello  index[1] is '\' and escaped but '[' is not escaped
        EXPECT_TRUE(Str::is_escaped_char("\\\\[hello", 1));
        EXPECT_FALSE(Str::is_escaped_char("\\\\[hello", 0));
        EXPECT_FALSE(Str::is_escaped_char("\\\\[hello", 2));
        EXPECT_FALSE(Str::is_escaped_char("\\\\[hello", 3));
    }

    TEST_F(TestStr, test_str_from_string)
    {
        bool b = false;
        char c;
        int16_t i16;
        uint32_t ui32;
        int64_t i64;
        float f;
        double d;

        EXPECT_FALSE(Str::convert_from_string<bool>("", b));
        EXPECT_FALSE(Str::convert_from_string("", i16));
        EXPECT_FALSE(Str::convert_from_string("", ui32));
        EXPECT_FALSE(Str::convert_from_string("", i64));
        EXPECT_FALSE(Str::convert_from_string("", f));
        EXPECT_FALSE(Str::convert_from_string("", d));

        EXPECT_TRUE(Str::convert_from_string<bool>("1", b));
        EXPECT_EQ(b, true);
        EXPECT_TRUE(Str::convert_from_string<bool>("0", b));
        EXPECT_EQ(b, false);
        EXPECT_TRUE(Str::convert_from_string<bool>("true", b));
        EXPECT_EQ(b, true);
        EXPECT_TRUE(Str::convert_from_string<bool>("false", b));
        EXPECT_EQ(b, false);
        EXPECT_FALSE(Str::convert_from_string<bool>("nope", b));
        EXPECT_FALSE(Str::convert_from_string<bool>("T", b));

        EXPECT_TRUE(Str::convert_from_string("c", c));
        EXPECT_EQ(c, 'c');
        EXPECT_TRUE(Str::convert_from_string("A", c));
        EXPECT_EQ(c, 'A');
        EXPECT_FALSE(Str::convert_from_string("Abc", c));

        EXPECT_TRUE(Str::convert_from_string("123", i16));
        EXPECT_EQ(i16, 123);
        EXPECT_TRUE(Str::convert_from_string("-321", i16));
        EXPECT_EQ(i16, -321);
        // test overflow
        EXPECT_TRUE(Str::convert_from_string("32768", i16));
        EXPECT_EQ(i16, -32768);

        EXPECT_TRUE(Str::convert_from_string("1000", ui32));
        EXPECT_EQ(ui32, 1000u);
        EXPECT_TRUE(Str::convert_from_string("1000,hello world", ui32));
        EXPECT_EQ(ui32, 1000u);
        // test overflow
        EXPECT_TRUE(Str::convert_from_string("-1", ui32));
        EXPECT_EQ(ui32, 4294967295u);

        EXPECT_TRUE(Str::convert_from_string("100", i64));
        EXPECT_EQ(i64, 100);
        EXPECT_TRUE(Str::convert_from_string("-1", i64));
        EXPECT_EQ(i64, -1);

        EXPECT_TRUE(Str::convert_from_string("0.1", f));
        EXPECT_FLOAT_EQ(f, 0.1);
        EXPECT_TRUE(Str::convert_from_string("123.456", f));
        EXPECT_FLOAT_EQ(f, 123.456);

        EXPECT_TRUE(Str::convert_from_string("0.01", d));
        EXPECT_FLOAT_EQ(d, 0.01);
        EXPECT_TRUE(Str::convert_from_string("123.456", d));
        EXPECT_FLOAT_EQ(d, 123.456);

    }

    TEST_F(TestStr, test_str_tonumber)
    {
        // long
        EXPECT_TRUE(this->test_long("1234"));
        EXPECT_EQ(_val, 1234);
        EXPECT_TRUE(this->test_long("-1234"));
        EXPECT_EQ(_val, -1234);
        EXPECT_TRUE(this->test_long("-1234totototootot"));
        EXPECT_EQ(_val, -1234);

        EXPECT_FALSE(this->test_long("toto"));
        EXPECT_FALSE(this->test_long("toto123"));
        EXPECT_FALSE(this->test_long("toto -1234"));
        // overflow
        EXPECT_FALSE(this->test_long("151615165151561132133554654"));
        EXPECT_EQ(errno, ERANGE);

        EXPECT_TRUE(this->test_long("0x2a", 10));
        EXPECT_EQ(_val, 0);
        EXPECT_EQ(errno, 0);
        EXPECT_TRUE(this->test_long("0x2a", 16));
        EXPECT_EQ(_val, 0x2a);
        // auto detect base
        EXPECT_TRUE(this->test_long("0x2a"));
        EXPECT_EQ(_val, 0x2a);
        EXPECT_TRUE(this->test_long("0XDEADCAFE"));
        EXPECT_EQ(_val, (long)0xDEADCAFE);

        // unsigned long
        EXPECT_TRUE(this->test_ulong("1234"));
        EXPECT_EQ(_uval, 1234u);
        EXPECT_TRUE(this->test_ulong("-1"));
        EXPECT_EQ(_uval, ULONG_MAX);

        EXPECT_FALSE(this->test_ulong("toto"));
        EXPECT_FALSE(this->test_ulong("toto1234"));
        // overflow
        EXPECT_EQ(errno, 0);
        EXPECT_FALSE(this->test_ulong("15151651651651515113132132132132132"));
        EXPECT_EQ(errno, ERANGE);

        // double
        EXPECT_TRUE(this->test_double("1234"));
        EXPECT_DOUBLE_EQ(_dval, 1234);
        EXPECT_TRUE(this->test_double("1234.456"));
        EXPECT_DOUBLE_EQ(_dval, 1234.456);
        EXPECT_TRUE(this->test_double("-1234.456"));
        EXPECT_DOUBLE_EQ(_dval, -1234.456);

        EXPECT_FALSE(this->test_double("toto"));
        EXPECT_FALSE(this->test_double("toto-1234.456"));
    }

    TEST_F(TestStr, test_str_conf)
    {
        std::map<std::string, std::string> conf = Str::parse_configuration("key=value;key2=value2");
        EXPECT_TRUE(conf.find("key") != conf.end());
        EXPECT_TRUE(conf.find("key2") != conf.end());
        EXPECT_EQ(conf["key"], "value");
        EXPECT_EQ(conf["key2"], "value2");

        conf = Str::parse_configuration("a=1;b=;c;");
        EXPECT_TRUE(conf.find("a") != conf.end());
        EXPECT_TRUE(conf.find("b") != conf.end());
        EXPECT_TRUE(conf.find("c") == conf.end());
        EXPECT_EQ(conf["a"], "1");
        EXPECT_EQ(conf["b"], "");

        conf = Str::parse_configuration("");
        EXPECT_EQ(conf.size(), 0u);
        conf = Str::parse_configuration("a");
        EXPECT_EQ(conf.size(), 0u);
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
        std::string res = Str::format("%s -> %d", "hello", 1337);
        EXPECT_EQ(res, "hello -> 1337");
        std::string res2 = Str::format("%s: %d", "hello world", 10);
        EXPECT_EQ(res, "hello -> 1337");
        EXPECT_EQ(res2, "hello world: 10");
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
        LOG(debug, Str::addr_to_string(s.data()));
        EXPECT_EQ(Str::addr_to_string(0x0, 5), "0x00000");
        EXPECT_EQ(Str::num_to_string(312, 10), "312");
        EXPECT_EQ(Str::num_to_string(16, 16), "10");
        EXPECT_EQ(Str::num_to_string(15, 16), "f");
        EXPECT_EQ(Str::num_to_string(255, 16), "ff");
        EXPECT_EQ(Str::hexdump(s.data(), s.length(), ','), "68,65,6c,6c,6f,20,77,6f,72,6c,64,20,2d,20,68,6f,77,20,61,72,65,20,79,6f,75");
        std::string dump = Str::hexdump_fmt(s.data(), s.length());
        std::cout << dump << std::endl;
        Splitter splitter("\n");
        auto splits = splitter.split(dump);
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
