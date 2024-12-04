#include <cerrno>
#include <climits>

#include <gtest/gtest.h>

#include <sihd/util/Logger.hpp>
#include <sihd/util/Splitter.hpp>
#include <sihd/util/StrConfiguration.hpp>
#include <sihd/util/str.hpp>
#include <sihd/util/time.hpp>

namespace test
{
using namespace sihd::util;

SIHD_LOGGER;

class TestStr: public ::testing::Test
{
    protected:
        TestStr() { sihd::util::LoggerManager::stream(); }

        virtual ~TestStr() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}

        bool test_long(const std::string & str, int base = 0)
        {
            long val;
            bool ret = str::to_long(str, &val, base);
            SIHD_LOG(debug, "Test signed long (errno = {}) -> {}", errno, str);
            if (ret)
            {
                SIHD_LOG(debug, "Value found: {}", val);
                _val = val;
                return true;
            }
            SIHD_LOG(debug, "Failed to find a signed long");
            return ret;
        }

        bool test_ulong(const std::string & str, int base = 0)
        {
            unsigned long val;
            bool ret = str::to_ulong(str, &val, base);
            SIHD_LOG(debug, "Test unsigned long (errno = {}) -> {}", errno, str);
            if (ret)
            {
                SIHD_LOG(debug, "Value found: {}", val);
                _uval = val;
                return true;
            }
            SIHD_LOG(debug, "Failed to find an unsigned long");
            return ret;
        }

        bool test_double(const std::string & str)
        {
            double val;
            bool ret = str::to_double(str, &val);
            SIHD_LOG(debug, "Test double (errno = {}) -> {}", errno, str);
            if (ret)
            {
                SIHD_LOG(debug, "Value found: {}", val);
                _dval = val;
                return true;
            }
            SIHD_LOG(debug, "Failed to find a double");
            return ret;
        }

        long _val;
        unsigned long _uval;
        double _dval;
};

TEST_F(TestStr, test_str_split_pair)
{
    auto pair = str::split_pair_view("TOTO=titi", "=");
    EXPECT_EQ(pair.first, "TOTO");
    EXPECT_EQ(pair.second, "titi");

    auto pair_allocated = str::split_pair("TOTO=titi", "=");
    EXPECT_EQ(pair_allocated.first, "TOTO");
    EXPECT_EQ(pair_allocated.second, "titi");

    pair = str::split_pair_view("TOTO=titi", ";");
    EXPECT_EQ(pair.first, "");
    EXPECT_EQ(pair.second, "");

    pair = str::split_pair_view("TOTO=titi", "");
    EXPECT_EQ(pair.first, "");
    EXPECT_EQ(pair.second, "");

    pair = str::split_pair_view("TOTO=", "=");
    EXPECT_EQ(pair.first, "TOTO");
    EXPECT_EQ(pair.second, "");

    pair = str::split_pair_view("=", "=");
    EXPECT_EQ(pair.first, "");
    EXPECT_EQ(pair.second, "");

    pair = str::split_pair_view("", "=");
    EXPECT_EQ(pair.first, "");
    EXPECT_EQ(pair.second, "");

    pair = str::split_pair_view("TOTO=titi=tata", "=");
    EXPECT_EQ(pair.first, "TOTO");
    EXPECT_EQ(pair.second, "titi=tata");
}

TEST_F(TestStr, test_str_word_wrap)
{
    EXPECT_EQ(str::word_wrap("", 1), "");
    EXPECT_EQ(str::word_wrap("abc", 0), "");
    EXPECT_EQ(str::word_wrap("abc", 3), "abc");
    EXPECT_EQ(str::word_wrap("  \ta  ", 1), "a");
    EXPECT_EQ(str::word_wrap("abc", 3), "abc");
    EXPECT_EQ(str::word_wrap("abc", 2), "a-\nbc");
    EXPECT_EQ(str::word_wrap("ab c", 2), "ab\nc");
    EXPECT_EQ(str::word_wrap("ab c\n", 2), "ab\nc\n");
    EXPECT_EQ(str::word_wrap("a\nbc", 2), "a\nbc");
    EXPECT_EQ(str::word_wrap("a b c", 1), "a\nb\nc");
    EXPECT_EQ(str::word_wrap("hello world", 5), "hello\nworld");
    EXPECT_EQ(str::word_wrap("hello world", 10), "hello\nworld");
    EXPECT_EQ(str::word_wrap("abc + def ghi", 10), "abc + def\nghi");
    EXPECT_EQ(str::word_wrap("iamfartoolong", 5), "iamf-\narto-\nolong");
    EXPECT_EQ(str::word_wrap("helloworld!!", 6), "hello-\nworld-\n!!");
    EXPECT_EQ(str::word_wrap("abc\n\ndef\nghi\n", 4), "abc\n\ndef\nghi\n");

    const char *str = "The quick brown fox jumped over the lazy dog";

    EXPECT_EQ(str::word_wrap(str, 7),
              "The\n"
              "quick\n"
              "brown\n"
              "fox\n"
              "jumped\n"
              "over\n"
              "the\n"
              "lazy\n"
              "dog");

    EXPECT_EQ(str::word_wrap(str, 8),
              "The\n"
              "quick\n"
              "brown\n"
              "fox\n"
              "jumped\n"
              "over the\n"
              "lazy dog");

    EXPECT_EQ(str::word_wrap(str, 9),
              "The quick\n"
              "brown fox\n"
              "jumped\n"
              "over the\n"
              "lazy dog");

    EXPECT_EQ(str::word_wrap(str, 10),
              "The quick\n"
              "brown fox\n"
              "jumped\n"
              "over the\n"
              "lazy dog");

    EXPECT_EQ(str::word_wrap(str, 11),
              "The quick\n"
              "brown fox\n"
              "jumped over\n"
              "the lazy\n"
              "dog");

    EXPECT_EQ(str::word_wrap(str, 12),
              "The quick\n"
              "brown fox\n"
              "jumped over\n"
              "the lazy dog");

    EXPECT_EQ(str::word_wrap(str, 20),
              "The quick brown fox\n"
              "jumped over the lazy\n"
              "dog");
}

TEST_F(TestStr, test_str_base)
{
    EXPECT_EQ(str::to_hex(9), "9");
    EXPECT_EQ(str::to_hex(10), "a");
    EXPECT_EQ(str::to_hex(16), "10");
    EXPECT_EQ(str::to_hex(1337), "539");
    EXPECT_EQ(str::to_hex(283654106644), "420b1a2e14");
}

TEST_F(TestStr, test_str_find_escape)
{
    EXPECT_EQ(str::find_char_not_escaped("hello ?", '?'), 6);
    EXPECT_EQ(str::find_char_not_escaped("hello \\?", '?'), -1);
    EXPECT_EQ(str::find_char_not_escaped("hello \\\\?", '?'), 8);
    EXPECT_EQ(str::find_char_not_escaped("hello \\??", '?'), 8);
    EXPECT_EQ(str::find_char_not_escaped("hello \\? ?", '?'), 9);
    EXPECT_EQ(str::find_char_not_escaped("\\??", '?'), 2);
    EXPECT_EQ(str::find_char_not_escaped("\\", '?'), -1);
    EXPECT_EQ(str::find_char_not_escaped("", ' '), -1);
}

TEST_F(TestStr, test_str_find_enclose)
{
    EXPECT_EQ(str::find_str_not_enclosed("hello world", "world"), 6);
    EXPECT_EQ(str::find_str_not_enclosed("hello world", ""), 0);
    EXPECT_EQ(str::find_str_not_enclosed("hello world", "h"), 0);
    EXPECT_EQ(str::find_str_not_enclosed("hello world", "e"), 1);

    EXPECT_EQ(str::find_str_not_enclosed("hello world", "z"), -1);
    EXPECT_EQ(str::find_str_not_enclosed("hello world", "world2"), -1);

    EXPECT_EQ(str::find_str_not_enclosed("hello 'world'", "world", "'"), -1);
    EXPECT_EQ(str::find_str_not_enclosed("hello [world] world", "world", "["), 14);
    EXPECT_EQ(str::find_str_not_enclosed("hello {world} world", "world", "{"), 14);

    EXPECT_EQ(str::find_str_not_enclosed("hello {world} world", "world2", "{"), -1);

    EXPECT_EQ(str::find_str_not_enclosed("hello world\n", "\n"), 11);
    EXPECT_EQ(str::find_str_not_enclosed("hello [world\n", "\n", "["), -1);
}

TEST_F(TestStr, test_str_remove_escape_char)
{
    std::string escaped = str::remove_escape_char("\\hello\\ world");
    EXPECT_EQ(escaped, "hello world");
    EXPECT_EQ(escaped.size(), strlen("hello world"));

    escaped = str::remove_escape_char("\\\\hello\\\\ world");
    EXPECT_EQ(escaped, "\\hello\\ world");

    escaped = str::remove_escape_char("");
    EXPECT_EQ(escaped, "");
}

TEST_F(TestStr, test_str_remove_escape_sequences)
{
    std::string escaped = str::remove_enclosing("'hello world'");
    EXPECT_EQ(escaped, "hello world");
    EXPECT_EQ(escaped.size(), strlen("hello world"));

    escaped = str::remove_enclosing("'hello '(world)'");
    EXPECT_EQ(escaped, "hello world");
    escaped = str::remove_enclosing("hello ''([world])");
    EXPECT_EQ(escaped, "hello [world]");
    escaped = str::remove_enclosing("\\'hello \\'world");
    EXPECT_EQ(escaped, "\\'hello \\'world");
    escaped = str::remove_enclosing("");
    EXPECT_EQ(escaped, "");
}

TEST_F(TestStr, test_str_bytes)
{
    EXPECT_EQ(str::bytes_str(1), "1B");
    EXPECT_EQ(str::bytes_str(1001), "1001B");
    EXPECT_EQ(str::bytes_str(1024), "1K");
    EXPECT_EQ(str::bytes_str(1025), "1K");
    EXPECT_EQ(str::bytes_str(1024 + 1023), "1.9K");
    ssize_t mbyte = 1024L * 1024L;
    EXPECT_EQ(str::bytes_str(mbyte), "1M");
    EXPECT_EQ(str::bytes_str((mbyte) + (mbyte - 1024)), "1.9M");
    ssize_t gbyte = 1024L * 1024L * 1024L;
    EXPECT_EQ(str::bytes_str(gbyte), "1G");
    EXPECT_EQ(str::bytes_str((gbyte) + (gbyte - mbyte)), "1.9G");
    ssize_t tbyte = 1024L * 1024L * 1024L * 1024L;
    EXPECT_EQ(str::bytes_str(tbyte), "1T");
    EXPECT_EQ(str::bytes_str((tbyte) + (tbyte - gbyte)), "1.9T");
}

TEST_F(TestStr, test_str_time2str)
{
    std::string time_str = str::timeoffset_str(time::micro(123));
    EXPECT_EQ(time_str, "+123us");
    time_str = str::timeoffset_str(time::milli(1));
    EXPECT_EQ(time_str, "+1ms:0us");
    time_str = str::timeoffset_str(time::sec(12) + time::micro(12));
    EXPECT_EQ(time_str, "+12s:0ms:12us");
    time_str = str::timeoffset_str(time::hours(12));
    EXPECT_EQ(time_str, "+12h:0m:0s:0ms:0us");
    time_str = str::timeoffset_str(time::hours(24));
    EXPECT_EQ(time_str, "+1d 0h:0m:0s:0ms:0us");
    time_str = str::timeoffset_str(time::days(24));
    EXPECT_EQ(time_str, "+24d 0h:0m:0s:0ms:0us");
    time_str = str::timeoffset_str(time::days(31));
    EXPECT_EQ(time_str, "+1m:0d 0h:0m:0s:0ms:0us");
    time_str = str::timeoffset_str(time::days(365));
    EXPECT_EQ(time_str, "+1y:0m:0d 0h:0m:0s:0ms:0us");
    time_str = str::timeoffset_str(time::days(365) * 2);
    EXPECT_EQ(time_str, "+2y:0m:0d 0h:0m:0s:0ms:0us");

    time_str = str::timeoffset_str(-time::sec(42));
    EXPECT_EQ(time_str, "-42s:0ms:0us");

    std::string nano_time_str = str::timeoffset_str(123, false, true);
    EXPECT_EQ(nano_time_str, "+0us:123ns");
    nano_time_str = str::timeoffset_str(-123, true, true);
    EXPECT_EQ(nano_time_str, "-0us:123ns (-123)");
}

TEST_F(TestStr, test_str_escapes)
{
    // \[hello  index[1] is '[' and escaped
    EXPECT_TRUE(str::is_escaped_char("\\[hello", 1));
    EXPECT_FALSE(str::is_escaped_char("\\[hello", 0));
    EXPECT_FALSE(str::is_escaped_char("\\[hello", 2));
    // \\[hello  index[1] is '\' and escaped but '[' is not escaped
    EXPECT_TRUE(str::is_escaped_char("\\\\[hello", 1));
    EXPECT_FALSE(str::is_escaped_char("\\\\[hello", 0));
    EXPECT_FALSE(str::is_escaped_char("\\\\[hello", 2));
    EXPECT_FALSE(str::is_escaped_char("\\\\[hello", 3));
}

TEST_F(TestStr, test_str_enclosures)
{
    EXPECT_TRUE(str::is_char_enclose_start('"'));
    EXPECT_TRUE(str::is_char_enclose_start('\''));
    EXPECT_TRUE(str::is_char_enclose_start('{'));
    EXPECT_TRUE(str::is_char_enclose_start('('));
    EXPECT_TRUE(str::is_char_enclose_start('['));
    EXPECT_TRUE(str::is_char_enclose_start('<'));

    EXPECT_TRUE(str::is_char_enclose_stop('"'));
    EXPECT_TRUE(str::is_char_enclose_stop('\''));
    EXPECT_TRUE(str::is_char_enclose_stop('}'));
    EXPECT_TRUE(str::is_char_enclose_stop(')'));
    EXPECT_TRUE(str::is_char_enclose_stop(']'));
    EXPECT_TRUE(str::is_char_enclose_stop('>'));

    EXPECT_EQ(str::stopping_enclose_of('"'), '"');
    EXPECT_EQ(str::stopping_enclose_of('\''), '\'');
    EXPECT_EQ(str::stopping_enclose_of('{'), '}');
    EXPECT_EQ(str::stopping_enclose_of('('), ')');
    EXPECT_EQ(str::stopping_enclose_of('['), ']');
    EXPECT_EQ(str::stopping_enclose_of('<'), '>');

    EXPECT_EQ(str::stopping_enclose_index("'hello'", 0, "'"), 7);
    EXPECT_EQ(str::stopping_enclose_index("'hello'", 0, "'", '\''), 7);
    // not an enclosure
    EXPECT_EQ(str::stopping_enclose_index("", 0, "'", '\''), -1);
    // not an enclosure
    EXPECT_EQ(str::stopping_enclose_index("hello", 0, "'", '\''), -1);
    // never ends
    EXPECT_EQ(str::stopping_enclose_index("'hello\\'", 0, "'"), -2);
    EXPECT_EQ(str::stopping_enclose_index("'hello", 0, "'", '\''), -2);
    // ending is escaped, never ends
    EXPECT_EQ(str::stopping_enclose_index("'hello''", 0, "'", '\''), -2);
    // pointing to an escape
    EXPECT_EQ(str::stopping_enclose_index("'hello''", 6, "'", '\''), -1);
    // pointing to an escaped escape
    EXPECT_EQ(str::stopping_enclose_index("'hello''", 7, "'", '\''), -1);
    // never ends
    EXPECT_EQ(str::stopping_enclose_index("'hello'''", 8, "'", '\''), -2);
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

    EXPECT_FALSE(str::convert_from_string<bool>("", b));
    EXPECT_FALSE(str::convert_from_string("", i16));
    EXPECT_FALSE(str::convert_from_string("", ui32));
    EXPECT_FALSE(str::convert_from_string("", i64));
    EXPECT_FALSE(str::convert_from_string("", f));
    EXPECT_FALSE(str::convert_from_string("", d));

    EXPECT_TRUE(str::convert_from_string<bool>("1", b));
    EXPECT_EQ(b, true);
    EXPECT_TRUE(str::convert_from_string<bool>("0", b));
    EXPECT_EQ(b, false);
    EXPECT_TRUE(str::convert_from_string<bool>("true", b));
    EXPECT_EQ(b, true);
    EXPECT_TRUE(str::convert_from_string<bool>("false", b));
    EXPECT_EQ(b, false);
    EXPECT_FALSE(str::convert_from_string<bool>("nope", b));
    EXPECT_FALSE(str::convert_from_string<bool>("T", b));

    EXPECT_TRUE(str::convert_from_string("c", c));
    EXPECT_EQ(c, 'c');
    EXPECT_TRUE(str::convert_from_string("A", c));
    EXPECT_EQ(c, 'A');
    EXPECT_FALSE(str::convert_from_string("Abc", c));

    EXPECT_TRUE(str::convert_from_string("123", i16));
    EXPECT_EQ(i16, 123);
    EXPECT_TRUE(str::convert_from_string("-321", i16));
    EXPECT_EQ(i16, -321);
    // test overflow
    EXPECT_TRUE(str::convert_from_string("32768", i16));
    EXPECT_EQ(i16, -32768);

    EXPECT_TRUE(str::convert_from_string("1000", ui32));
    EXPECT_EQ(ui32, 1000u);
    EXPECT_TRUE(str::convert_from_string("1000,hello world", ui32));
    EXPECT_EQ(ui32, 1000u);
    // test overflow
    EXPECT_TRUE(str::convert_from_string("-1", ui32));
    EXPECT_EQ(ui32, 4294967295u);

    EXPECT_TRUE(str::convert_from_string("100", i64));
    EXPECT_EQ(i64, 100);
    EXPECT_TRUE(str::convert_from_string("-1", i64));
    EXPECT_EQ(i64, -1);

    EXPECT_TRUE(str::convert_from_string("0.1", f));
    EXPECT_FLOAT_EQ(f, 0.1);
    EXPECT_TRUE(str::convert_from_string("123.456", f));
    EXPECT_FLOAT_EQ(f, 123.456);

    EXPECT_TRUE(str::convert_from_string("0.01", d));
    EXPECT_FLOAT_EQ(d, 0.01);
    EXPECT_TRUE(str::convert_from_string("123.456", d));
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

TEST_F(TestStr, test_strconfiguration)
{
    StrConfiguration conf("key=value;key2=value2");
    EXPECT_TRUE(conf.has("key"));
    EXPECT_TRUE(conf.has("key2"));
    EXPECT_EQ(conf["key"], "value");
    EXPECT_EQ(conf["key2"], "value2");

    auto [key, key2] = conf.find_all("key", "key2");
    EXPECT_TRUE(key.has_value());
    EXPECT_TRUE(key2.has_value());

    conf.parse_configuration("a=1;b=;c;");
    EXPECT_TRUE(conf.has("a"));
    EXPECT_TRUE(conf.has("b"));
    EXPECT_FALSE(conf.has("c"));
    EXPECT_EQ(conf["a"], "1");
    EXPECT_EQ(conf["b"], "");
    EXPECT_EQ(conf["c"], "");
    EXPECT_EQ(conf["d"], "");
    EXPECT_EQ(conf.get("a"), "1");
    EXPECT_EQ(conf.get("b"), "");
    EXPECT_THROW(conf.get("c"), std::out_of_range);
    EXPECT_THROW(conf.get("d"), std::out_of_range);

    conf.parse_configuration("");
    EXPECT_EQ(conf.size(), 0u);
    EXPECT_TRUE(conf.empty());
    conf.parse_configuration("a");
    EXPECT_EQ(conf.size(), 0u);
    EXPECT_TRUE(conf.empty());
}

TEST_F(TestStr, test_str_format)
{
    std::string res = str::format("%s -> %d", "hello", 1337);
    EXPECT_EQ(res, "hello -> 1337");
    std::string res2 = str::format("%s: %d", "hello world", 10);
    EXPECT_EQ(res, "hello -> 1337");
    EXPECT_EQ(res2, "hello world: 10");
}

TEST_F(TestStr, test_str_demangle)
{
    std::string res = str::demangle(typeid(*this).name());
    SIHD_LOG(info, "Demangled: {}", res);
    EXPECT_EQ(res, "test::TestStr_test_str_demangle_Test");
    EXPECT_EQ(res, str::demangle_type_name(*this));
}

TEST_F(TestStr, test_str_trim)
{
    EXPECT_EQ(str::trim(""), "");
    EXPECT_EQ(str::trim("h"), "h");
    EXPECT_EQ(str::trim("h "), "h");
    EXPECT_EQ(str::trim(" h"), "h");
    EXPECT_EQ(str::ltrim("     h"), "h");
    EXPECT_EQ(str::ltrim("     h "), "h ");
    EXPECT_EQ(str::rtrim("h       "), "h");
    EXPECT_EQ(str::rtrim(" h       "), " h");
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
    EXPECT_EQ(str::replace("wibbly wobbly timy wimey stuff", "i", "iii"), "wiiibbly wobbly tiiimy wiiimey stuff");
    EXPECT_EQ(str::replace("heh ih", "h", "hhh"), "hhhehhh ihhh");
    EXPECT_EQ(str::replace("hello", "", ""), "hello");
}

TEST_F(TestStr, test_str_to_columns)
{
    const std::string str = "The quick brown fox jumped over the lazy dog";
    std::vector<std::string> words = str::split(str);

    const auto create_and_print = [&](size_t max_width) {
        std::vector columns = str::to_columns(words, max_width);
        if (!columns.empty())
            fmt::print("{}\n\n", str::join(columns));
        return columns;
    };

    // no room
    EXPECT_EQ(create_and_print(5), std::vector<std::string> {});

    // 1 column
    const std::vector<std::string>
        one_col {"The   ", "quick ", "brown ", "fox   ", "jumped", "over  ", "the   ", "lazy  ", "dog   "};
    EXPECT_EQ(create_and_print(6), one_col);

    // 2 columns
    const std::vector<std::string> two_col {"The    over", "quick  the ", "brown  lazy", "fox    dog ", "jumped"};
    EXPECT_EQ(create_and_print(12), two_col);

    // 3 columns
    const std::vector<std::string> three_col {"The   fox    the ", "quick jumped lazy", "brown over   dog "};
    EXPECT_EQ(create_and_print(17), three_col);

    // 5 columns
    const std::vector<std::string> five_col {"The   brown jumped the  dog", "quick fox   over   lazy"};
    EXPECT_EQ(create_and_print(27), five_col);

    // 6 columns
    const std::vector<std::string> six_col {"The quick brown fox jumped over the lazy dog"};
    EXPECT_EQ(create_and_print(str.size()), six_col);
}

TEST_F(TestStr, test_str_hexdump)
{
    const std::string s = "hello world - how are you";
    SIHD_LOG(debug, "{}", str::addr_str(s.data()));
    EXPECT_EQ(str::addr_str(0x0, 5), "0x00000");
    EXPECT_EQ(str::num_str(312, 10), "312");
    EXPECT_EQ(str::num_str(16, 16), "10");
    EXPECT_EQ(str::num_str(15, 16), "f");
    EXPECT_EQ(str::num_str(255, 16), "ff");
    EXPECT_EQ(str::hexdump(s.data(), s.length(), ','),
              "68,65,6c,6c,6f,20,77,6f,72,6c,64,20,2d,20,68,6f,77,20,61,72,65,20,79,6f,75");
    std::vector<std::string> dump = str::hexdump_fmt(s.data(), s.length(), 8);
    fmt::print("{}\n", str::join(dump));
    ASSERT_EQ(dump.size(), 4u);
    EXPECT_EQ(dump[0], "0x0:  68 65 6c 6c 6f 20 77 6f   hello wo");
    EXPECT_EQ(dump[1], "0x8:  72 6c 64 20 2d 20 68 6f   rld - ho");
    EXPECT_EQ(dump[2], "0x10: 77 20 61 72 65 20 79 6f   w are yo");
    EXPECT_EQ(dump[3], "0x18: 75                        u       ");
}

TEST_F(TestStr, test_str_with)
{
    EXPECT_TRUE(str::starts_with("hello world", "hello"));
    EXPECT_TRUE(str::starts_with("hello world", "h"));
    EXPECT_FALSE(str::starts_with("hello world", "ello"));
    EXPECT_FALSE(str::starts_with("hello world", "world"));

    EXPECT_TRUE(str::ends_with("hello world", "world"));
    EXPECT_TRUE(str::ends_with("hello world", "d"));
    EXPECT_FALSE(str::ends_with("hello world", "worl"));
    EXPECT_FALSE(str::ends_with("hello world", "hello"));

    EXPECT_FALSE(str::starts_with("h", "he"));
    EXPECT_FALSE(str::ends_with("h", "hello"));

    EXPECT_TRUE(str::starts_with("TOTO=", "TOTO", "="));
    EXPECT_TRUE(str::starts_with("TOTO=titi", "TOTO", "="));
    EXPECT_FALSE(str::starts_with("TOTO=", "TOTO", "!"));

    EXPECT_TRUE(str::ends_with("TOTO=titi", "titi", "="));
    EXPECT_TRUE(str::ends_with("=titi", "titi", "="));
    EXPECT_FALSE(str::ends_with("TOTO=titi", "titi", "!"));
}

TEST_F(TestStr, test_str_time_format)
{
    std::string fmt = str::format_time(time::seconds(1) + time::minutes(2) + time::hours(3), "%X");
    EXPECT_EQ(fmt, "03:02:01");
}

} // namespace test
