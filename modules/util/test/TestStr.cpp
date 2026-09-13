#include <cerrno>
#include <climits>

#include <gtest/gtest.h>

#include <sihd/util/Array.hpp>
#include <sihd/util/ArrayView.hpp>
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

        bool test_long(const std::string & str, uint16_t base = 10)
        {
            const auto val = str::convert_from_string<long>(str, base);
            if (val)
            {
                SIHD_LOG(debug, "Value found: {} -> {}", str, *val);
                _val = *val;
                return true;
            }
            SIHD_LOG(debug, "Failed to find a signed long -> {}", str);
            return false;
        }

        bool test_ulong(const std::string & str, uint16_t base = 10)
        {
            const auto val = str::convert_from_string<unsigned long>(str, base);
            if (val)
            {
                SIHD_LOG(debug, "Value found: {} -> {}", str, *val);
                _uval = *val;
                return true;
            }
            SIHD_LOG(debug, "Failed to find an unsigned long -> {}", str);
            return false;
        }

        bool test_double(const std::string & str)
        {
            const auto val = str::to_double(str);
            if (val)
            {
                SIHD_LOG(debug, "Value found: {} -> {}", str, *val);
                _dval = *val;
                return true;
            }
            SIHD_LOG(debug, "Failed to find a double -> {}", str);
            return false;
        }

        long _val;
        unsigned long _uval;
        double _dval;
};

#if defined(__SIHD_WINDOWS__)
TEST_F(TestStr, test_str_to_u32str)
{
    std::u32string u32str = str::to_u32str("hello world");
    EXPECT_EQ(u32str, U"hello world");

    u32str = str::to_u32str("こんにちは");
    EXPECT_EQ(u32str, U"こんにちは");

    u32str = str::to_u32str("");
    EXPECT_EQ(u32str, U"");
}

TEST_F(TestStr, test_str_to_wstr)
{
    std::wstring wstr = str::to_wstr("hello world");
    EXPECT_EQ(wstr, L"hello world");

    wstr = str::to_wstr("こんにちは");
    EXPECT_EQ(wstr, L"こんにちは");

    wstr = str::to_wstr("");
    EXPECT_EQ(wstr, L"");
}
#endif

TEST_F(TestStr, test_str_regex_filter)
{
    std::vector<std::string> input = {"apple", "banana", "cherry", "date", "elderberry", "fig", "grape"};
    std::vector<std::string> filtered = str::regex_filter(input, "a.*e");
    std::vector<std::string> expected = {"apple", "date", "grape"};

    EXPECT_EQ(filtered, expected);

    filtered = str::regex_filter(input, "b.*a");
    expected = {"banana"};

    EXPECT_EQ(filtered, expected);

    filtered = str::regex_filter(input, ".*r.*y$");
    expected = {"cherry", "elderberry"};

    EXPECT_EQ(filtered, expected);

    filtered = str::regex_filter(input, "z.*");
    expected = {};

    EXPECT_EQ(filtered, expected);
}

TEST_F(TestStr, test_str_regex_replace)
{
    EXPECT_EQ(str::regex_replace("hello world", "world", "universe"), "hello universe");
    EXPECT_EQ(str::regex_replace("hello world", "o", "0"), "hell0 w0rld");
    EXPECT_EQ(str::regex_replace("hello world", "l+", "L"), "heLo worLd");
    EXPECT_EQ(str::regex_replace("123-456-7890", "\\d{3}", "XXX"), "XXX-XXX-XXX0");
    EXPECT_EQ(str::regex_replace("hello world", "z", "Z"), "hello world"); // No match
    EXPECT_EQ(str::regex_replace("hello world", "(hello) (world)", "$2 $1"), "world hello");
}

TEST_F(TestStr, test_str_regex_match)
{
    EXPECT_TRUE(str::regex_match("hello world", "hello world"));
    EXPECT_FALSE(str::regex_match("hello world", "hello"));
    EXPECT_TRUE(str::regex_match("hello world", ".*world"));
    EXPECT_TRUE(str::regex_match("hello world", ".*world$"));
    EXPECT_TRUE(str::regex_match("hello world", "^hello.*"));
    EXPECT_FALSE(str::regex_match("hello world", "^world"));
    EXPECT_TRUE(str::regex_match("123-456-7890", "\\d{3}-\\d{3}-\\d{4}"));
    EXPECT_FALSE(str::regex_match("123-456-7890", "\\d{4}-\\d{3}-\\d{4}"));
}

TEST_F(TestStr, test_str_regex_search)
{
    std::vector<std::string> results;

    results = str::regex_search("hello world", "hello");
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0], "hello");

    results = str::regex_search("hello world", "world");
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0], "world");

    results = str::regex_search("hello world", "o");
    ASSERT_EQ(results.size(), 2u);
    EXPECT_EQ(results[0], "o");
    EXPECT_EQ(results[1], "o");

    results = str::regex_search("hello world", "l+");
    ASSERT_EQ(results.size(), 2u);
    EXPECT_EQ(results[0], "ll");
    EXPECT_EQ(results[1], "l");

    results = str::regex_search("hello world", "z");
    ASSERT_TRUE(results.empty());

    results = str::regex_search("The quick brown fox jumps over the lazy dog", "\\b\\w{4}\\b");
    ASSERT_EQ(results.size(), 2u);
    EXPECT_EQ(results[0], "over");
    EXPECT_EQ(results[1], "lazy");

    results = str::regex_search("The quick brown fox jumps over the lazy dog", "(\\b\\w{3}\\b)|(\\b\\w{5}\\b)");
    ASSERT_EQ(results.size(), 7u);
    EXPECT_EQ(results[0], "The");
    EXPECT_EQ(results[1], "quick");
    EXPECT_EQ(results[2], "brown");
    EXPECT_EQ(results[3], "fox");
    EXPECT_EQ(results[4], "jumps");
    EXPECT_EQ(results[5], "the");
    EXPECT_EQ(results[6], "dog");
}

TEST_F(TestStr, test_str_glob_match)
{
    EXPECT_TRUE(str::glob_match("hello", "hello"));
    EXPECT_FALSE(str::glob_match("hello", "hell"));
    EXPECT_FALSE(str::glob_match("hello", "helloo"));

    EXPECT_TRUE(str::glob_match("", ""));
    EXPECT_TRUE(str::glob_match("", "*"));
    EXPECT_FALSE(str::glob_match("", "?"));
    EXPECT_FALSE(str::glob_match("a", ""));

    EXPECT_TRUE(str::glob_match("hello world", "*"));
    EXPECT_TRUE(str::glob_match("hello world", "hello*"));
    EXPECT_TRUE(str::glob_match("hello world", "*world"));
    EXPECT_TRUE(str::glob_match("hello world", "hello*world"));
    EXPECT_TRUE(str::glob_match("hello world", "h*d"));
    EXPECT_TRUE(str::glob_match("aaa", "*a"));
    EXPECT_TRUE(str::glob_match("file.log", "*.log"));
    EXPECT_FALSE(str::glob_match("file.log", "*.txt"));

    EXPECT_TRUE(str::glob_match("hello", "hell?"));
    EXPECT_TRUE(str::glob_match("abc", "a?c"));
    EXPECT_TRUE(str::glob_match("a", "?"));
    EXPECT_FALSE(str::glob_match("ab", "?"));
    EXPECT_FALSE(str::glob_match("abc", "a?"));

    EXPECT_TRUE(str::glob_match("b", "[abc]"));
    EXPECT_FALSE(str::glob_match("d", "[abc]"));
    EXPECT_TRUE(str::glob_match("m", "[a-z]"));
    EXPECT_FALSE(str::glob_match("M", "[a-z]"));
    EXPECT_TRUE(str::glob_match("file1.txt", "file[0-9].txt"));
    EXPECT_FALSE(str::glob_match("file12.txt", "file[0-9].txt"));
    EXPECT_TRUE(str::glob_match("a", "[a-c]"));
    EXPECT_TRUE(str::glob_match("c", "[a-c]"));
    EXPECT_FALSE(str::glob_match("d", "[a-c]"));
    EXPECT_TRUE(str::glob_match("d", "[!abc]"));
    EXPECT_FALSE(str::glob_match("a", "[!abc]"));
    EXPECT_FALSE(str::glob_match("m", "[!a-z]"));
    EXPECT_TRUE(str::glob_match("]", "[]]"));
    EXPECT_FALSE(str::glob_match("[", "[]]"));
    EXPECT_TRUE(str::glob_match("-", "[a-]"));
    EXPECT_TRUE(str::glob_match("-", "[-a]"));
    EXPECT_TRUE(str::glob_match("[abc", "[abc"));
    EXPECT_FALSE(str::glob_match("a", "[abc"));

    EXPECT_TRUE(str::glob_match("*", "\\*"));
    EXPECT_FALSE(str::glob_match("a", "\\*"));
    EXPECT_TRUE(str::glob_match("?", "\\?"));
    EXPECT_TRUE(str::glob_match("a?", "a\\?"));
    EXPECT_FALSE(str::glob_match("ab", "a\\?"));
    EXPECT_TRUE(str::glob_match("[abc]", "\\[abc]"));
    EXPECT_TRUE(str::glob_match("a\\", "a\\\\"));
    EXPECT_TRUE(str::glob_match("**", "\\*\\*"));
    EXPECT_TRUE(str::glob_match("]", "[\\]]"));
    EXPECT_TRUE(str::glob_match("a", "[\\a]"));

    EXPECT_TRUE(str::glob_match("aaa", "a**"));
    EXPECT_TRUE(str::glob_match("abcabcab", "*abc*ab"));
    EXPECT_TRUE(str::glob_match("a/b/c", "a*c"));
    EXPECT_TRUE(str::glob_match("[]", "[]"));
    EXPECT_FALSE(str::glob_match("x", "[]"));

    EXPECT_TRUE(str::glob_match("Hello.TXT", "*.txt", true));
    EXPECT_FALSE(str::glob_match("Hello.TXT", "*.txt"));
    EXPECT_TRUE(str::glob_match("WORLD", "world", true));
    EXPECT_TRUE(str::glob_match("HELLO", "h?LLO", true));
    EXPECT_TRUE(str::glob_match("M", "[a-z]", true));
    EXPECT_TRUE(str::glob_match("M", "[A-Z]", true));
    EXPECT_FALSE(str::glob_match("Hello.TXT", "h*.log", true));

    EXPECT_TRUE(str::glob_match("a", "[[:alpha:]]"));
    EXPECT_FALSE(str::glob_match("1", "[[:alpha:]]"));
    EXPECT_TRUE(str::glob_match("7", "[[:digit:]]"));
    EXPECT_FALSE(str::glob_match("a", "[[:digit:]]"));
    EXPECT_TRUE(str::glob_match(" ", "[[:space:]]"));
    EXPECT_TRUE(str::glob_match("f", "[[:lower:]]"));
    EXPECT_FALSE(str::glob_match("F", "[[:lower:]]"));
    EXPECT_TRUE(str::glob_match("F", "[[:upper:]]"));
    EXPECT_TRUE(str::glob_match("_", "[[:punct:]]"));
    EXPECT_TRUE(str::glob_match("ff", "[[:xdigit:]][[:xdigit:]]"));
    EXPECT_FALSE(str::glob_match("gg", "[[:xdigit:]][[:xdigit:]]"));
    EXPECT_TRUE(str::glob_match("a9", "[[:alpha:]][[:digit:]]"));
    EXPECT_TRUE(str::glob_match("a", "[[:alpha:][:digit:]]"));
    EXPECT_TRUE(str::glob_match("3", "[[:alpha:][:digit:]]"));
    EXPECT_FALSE(str::glob_match("x", "[[:foo:]]"));
    EXPECT_TRUE(str::glob_match("[[:alpha", "[[:alpha"));
    EXPECT_FALSE(str::glob_match("a", "[[:alpha"));
    EXPECT_TRUE(str::glob_match("Z9", "[[:alpha:]][[:digit:]]", true));
}

TEST_F(TestStr, test_str_glob_match_combined)
{
    // literal ? [0-9] [!a-z] [[:alpha:]] [[:digit:]] * \* [a-f] \[ \] \\ [[:upper:][:digit:]] \? . [!0-9] [a-z]
    const std::string
        pattern = "log-?[0-9][!a-z][[:alpha:]][[:digit:]]*\\*[a-f]\\[OK\\]\\\\[[:upper:][:digit:]]\\?.[!0-9][a-z]t";
    EXPECT_TRUE(str::glob_match("log-23_x7 build *c[OK]\\Z?.txt", pattern));
    EXPECT_TRUE(str::glob_match("LOG-23_X7 BUILD *C[ok]\\9?.TXT", pattern, true));

    EXPECT_FALSE(str::glob_match("log-23_x7 build *c[ok]\\Z?.txt", pattern));
    EXPECT_FALSE(str::glob_match("log-23_x7 build *c[OK]\\z?.txt", pattern));
    EXPECT_FALSE(str::glob_match("log-23_x7 build *c[OK]Z?.txt", pattern));
    EXPECT_FALSE(str::glob_match("log-23_x7 build c[OK]\\Z?.txt", pattern));
    EXPECT_FALSE(str::glob_match("log-233_x7 build *c[OK]\\Z?.txt", pattern));
}

TEST_F(TestStr, test_str_glob_filter)
{
    std::vector<std::string> input = {"file.txt", "file.cpp", "other.TXT", "notes.md"};
    EXPECT_EQ(str::glob_filter(std::span<const std::string>(input), "*.txt"), (std::vector<std::string> {"file.txt"}));
    EXPECT_EQ(str::glob_filter(std::span<const std::string>(input), "*.txt", true),
              (std::vector<std::string> {"file.txt", "other.TXT"}));

    std::vector<std::string_view> views = {"a.log", "b.LOG", "c.txt"};
    EXPECT_EQ(str::glob_filter(std::span<std::string_view>(views), "*.log", true),
              (std::vector<std::string> {"a.log", "b.LOG"}));

    const char *arr[] = {"x.c", "y.hpp", "z.c"};
    EXPECT_EQ(str::glob_filter(arr, "*.c"), (std::vector<std::string> {"x.c", "z.c"}));
    EXPECT_TRUE(str::glob_filter(std::span<const std::string>(input), "*.nope").empty());
}

TEST_F(TestStr, test_str_search)
{
    const std::vector<std::string> list = {"paydays", "day", "moonday", "sunday", "survey"};
    const auto results = str::search(list, "sunday");
    for (const auto & result : results)
    {
        fmt::print("{} - {}\n", result.distance, result.word);
    }
    ASSERT_EQ(results.size(), 5u);
    EXPECT_EQ(results[0].distance, 0u);
    EXPECT_EQ(results[0].word, "sunday");
    EXPECT_EQ(results[1].word, "survey");
    EXPECT_EQ(results[2].word, "moonday");
    EXPECT_EQ(results[3].word, "day");
    EXPECT_EQ(results[4].word, "paydays");

    std::vector<std::string_view> views = {"paydays", "sunday"};
    const auto view_results = str::search(std::span<std::string_view>(views), "sunday");
    ASSERT_EQ(view_results.size(), 2u);
    EXPECT_EQ(view_results[0].word, "sunday");
    EXPECT_EQ(view_results[1].word, "paydays");

    const char *words[] = {"paydays", "sunday"};
    const auto ptr_results = str::search(words, "sunday");
    ASSERT_EQ(ptr_results.size(), 2u);
    EXPECT_EQ(ptr_results[0].word, "sunday");

    EXPECT_TRUE(str::search(std::span<const std::string> {}, "anything").empty());
}

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

    // a multi-char delimiter stops at the first of its characters but skips its full length (pinned behavior)
    pair = str::split_pair_view("a::b", "::");
    EXPECT_EQ(pair.first, "a");
    EXPECT_EQ(pair.second, "b");
    pair = str::split_pair_view("a:x:b", "::");
    EXPECT_EQ(pair.first, "a");
    EXPECT_EQ(pair.second, ":b");

    // an empty first part gives an empty pair
    auto empty_pair = str::split_pair("=titi", "=");
    EXPECT_EQ(empty_pair.first, "");
    EXPECT_EQ(empty_pair.second, "");
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

    EXPECT_EQ(str::word_wrap("ab", 2), "ab");
    EXPECT_EQ(str::word_wrap("a  b", 2), "a\nb");
    EXPECT_EQ(str::word_wrap("a\tb", 3), "a b");
    // a long word after already placed content
    EXPECT_EQ(str::word_wrap("x iamfartoolong", 5), "x\niamf-\narto-\nolong");

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
    EXPECT_EQ(str::to_dec(1337), "1337");
    EXPECT_EQ(str::to_oct(8), "10");
    EXPECT_EQ(str::num_str(5, 2), "101");
    EXPECT_EQ(str::num_str(255, 36), "73");
    // out of range bases yield an empty string
    EXPECT_EQ(str::num_str(255, 37), "");
    EXPECT_EQ(str::num_to_char(0), '0');
    EXPECT_EQ(str::num_to_char(9), '9');
    EXPECT_EQ(str::num_to_char(10), 'a');
    EXPECT_EQ(str::num_to_char(35), 'z');
    EXPECT_EQ(str::num_to_char(36), '\0');
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

    // the view does not extend to the end of the underlying buffer
    char not_terminated[] = "a]b xyz";
    const std::string_view truncated_view(not_terminated, 4);
    EXPECT_EQ(str::find_str_not_enclosed(truncated_view, "x"), -1);
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

    // an unterminated enclosure only removes its opener (pinned behavior)
    EXPECT_EQ(str::remove_enclosing("'hello"), "hello");
    EXPECT_EQ(str::remove_enclosing("hello 'world"), "hello world");
}

TEST_F(TestStr, test_str_unquote)
{
    EXPECT_EQ(str::unquote("hello"), "hello");
    EXPECT_EQ(str::unquote("\"hello\""), "hello");
    EXPECT_EQ(str::unquote("'hello'"), "hello");
    EXPECT_EQ(str::unquote(" \"hello\" "), "hello");
    EXPECT_EQ(str::unquote("\"it's\""), "it's");
    EXPECT_EQ(str::unquote("\"hello"), "\"hello");
    EXPECT_EQ(str::unquote("'hello\""), "'hello\"");
    EXPECT_EQ(str::unquote(""), "");
    EXPECT_EQ(str::unquote("\""), "\"");
    EXPECT_EQ(str::unquote("'"), "'");
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
    int64_t tbyte = 1024LL * 1024LL * 1024LL * 1024LL;
    EXPECT_EQ(str::bytes_str(tbyte), "1T");
    EXPECT_EQ(str::bytes_str((tbyte) + (tbyte - gbyte)), "1.9T");

    EXPECT_EQ(str::bytes_str(0), "0B");
    // negative sizes have no representation (pinned behavior)
    EXPECT_EQ(str::bytes_str(-1), "");

    // SI units
    EXPECT_EQ(str::bytes_str(999, false), "999B");
    EXPECT_EQ(str::bytes_str(1000, false), "1K");
    EXPECT_EQ(str::bytes_str(1500, false), "1.5K");
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
    EXPECT_FALSE(str::convert_from_string<bool>("").has_value());
    EXPECT_FALSE(str::convert_from_string<int16_t>("").has_value());
    EXPECT_FALSE(str::convert_from_string<uint32_t>("").has_value());
    EXPECT_FALSE(str::convert_from_string<int64_t>("").has_value());
    EXPECT_FALSE(str::convert_from_string<float>("").has_value());
    EXPECT_FALSE(str::convert_from_string<double>("").has_value());

    EXPECT_EQ(str::convert_from_string<bool>("1"), true);
    EXPECT_EQ(str::convert_from_string<bool>("0"), false);
    EXPECT_EQ(str::convert_from_string<bool>("true"), true);
    EXPECT_EQ(str::convert_from_string<bool>("false"), false);
    EXPECT_FALSE(str::convert_from_string<bool>("nope").has_value());
    EXPECT_FALSE(str::convert_from_string<bool>("T").has_value());

    EXPECT_EQ(str::convert_from_string<char>("c"), 'c');
    EXPECT_EQ(str::convert_from_string<char>("A"), 'A');
    EXPECT_FALSE(str::convert_from_string<char>("Abc").has_value());

    EXPECT_EQ(str::convert_from_string<int16_t>("123"), 123);
    EXPECT_EQ(str::convert_from_string<int16_t>("-321"), -321);
    // truncation to a smaller type wraps around
    EXPECT_EQ(str::convert_from_string<int16_t>("32768"), -32768);

    EXPECT_EQ(str::convert_from_string<uint32_t>("1000"), 1000u);
    // trailing data is ignored
    EXPECT_EQ(str::convert_from_string<uint32_t>("1000,hello world"), 1000u);
    // a leading '-' wraps around
    EXPECT_EQ(str::convert_from_string<uint32_t>("-1"), 4294967295u);

    EXPECT_EQ(str::convert_from_string<int64_t>("100"), 100);
    EXPECT_EQ(str::convert_from_string<int64_t>("-1"), -1);

    const auto f01 = str::convert_from_string<float>("0.1");
    ASSERT_TRUE(f01.has_value());
    EXPECT_FLOAT_EQ(*f01, 0.1f);
    const auto f123 = str::convert_from_string<float>("123.456");
    ASSERT_TRUE(f123.has_value());
    EXPECT_FLOAT_EQ(*f123, 123.456f);

    const auto d001 = str::convert_from_string<double>("0.01");
    ASSERT_TRUE(d001.has_value());
    EXPECT_DOUBLE_EQ(*d001, 0.01);
    const auto d123 = str::convert_from_string<double>("123.456");
    ASSERT_TRUE(d123.has_value());
    EXPECT_DOUBLE_EQ(*d123, 123.456);
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

    // a prefix that does not match the base is left untouched, "0x2a" in base 10 stops at 'x'
    EXPECT_TRUE(this->test_long("0x2a", 10));
    EXPECT_EQ(_val, 0);
    // hexadecimal without prefix
    EXPECT_TRUE(this->test_long("2a", 16));
    EXPECT_EQ(_val, 0x2a);
    EXPECT_TRUE(this->test_long("DEADCAFE", 16));
    EXPECT_EQ(_val, (long)0xDEADCAFE);
    // a prefix matching the base is stripped
    EXPECT_TRUE(this->test_long("0x2a", 16));
    EXPECT_EQ(_val, 0x2a);
    EXPECT_TRUE(this->test_long("0XdeadCAFE", 16));
    EXPECT_EQ(_val, (long)0xDEADCAFE);
    EXPECT_TRUE(this->test_long("0b101", 2));
    EXPECT_EQ(_val, 0b101);
    EXPECT_TRUE(this->test_long("0o17", 8));
    EXPECT_EQ(_val, 017);
    EXPECT_TRUE(this->test_long("-0xff", 16));
    EXPECT_EQ(_val, -0xff);
    // base 0 auto-detects from the prefix
    EXPECT_TRUE(this->test_long("0xff", 0));
    EXPECT_EQ(_val, 0xff);
    EXPECT_TRUE(this->test_long("0b101", 0));
    EXPECT_EQ(_val, 0b101);
    EXPECT_TRUE(this->test_long("42", 0));
    EXPECT_EQ(_val, 42);
    EXPECT_TRUE(this->test_ulong("0xff", 16));
    EXPECT_EQ(_uval, 0xffu);

    // unsigned long
    EXPECT_TRUE(this->test_ulong("1234"));
    EXPECT_EQ(_uval, 1234u);
    EXPECT_TRUE(this->test_ulong("-1"));
    EXPECT_EQ(_uval, ULONG_MAX);

    EXPECT_FALSE(this->test_ulong("toto"));
    EXPECT_FALSE(this->test_ulong("toto1234"));
    // overflow
    EXPECT_FALSE(this->test_ulong("15151651651651515113132132132132132"));

    // invalid bases are rejected
    EXPECT_FALSE(this->test_long("101", 1));
    EXPECT_FALSE(this->test_ulong("ff", 37));

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

    // longer than the fixed buffer of the previous implementation
    const std::string big(2000, 'x');
    EXPECT_EQ(str::format("%s", big.c_str()), big);
    EXPECT_EQ(str::format("%d%d", 0, 0), "00");
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
    // occurrences adjacent to a previous match are replaced too
    EXPECT_EQ(str::replace("aa", "a", "b"), "bb");
    EXPECT_EQ(str::replace("abab", "ab", "x"), "xx");
    EXPECT_EQ(str::replace("aaa", "aa", "b"), "ba");
}

TEST_F(TestStr, test_str_to_columns)
{
    const std::string str = "The quick brown fox jumped over the lazy dog";
    std::vector<std::string> words = str::split(str);

    const auto create_and_print = [&](size_t max_width) {
        std::vector<std::string> columns = str::to_columns(words, max_width);
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

    // other overloads
    std::vector<std::string_view> word_views = {"aaa", "b", "cc"};
    EXPECT_EQ(str::to_columns(std::span<std::string_view>(word_views), 7),
              (std::vector<std::string> {"aaa cc", "b  "}));

    const char *word_ptrs[] = {"aaa", "b", "cc"};
    EXPECT_EQ(str::to_columns(word_ptrs, 7), (std::vector<std::string> {"aaa cc", "b  "}));
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

    // zero columns would divide by zero
    EXPECT_TRUE(str::hexdump_fmt(s.data(), s.length(), 0).empty());

    const Array<char> arr = {'h', 'e'};
    EXPECT_EQ(str::hexdump(arr, ','), "68,65");
    const ArrayView<char> view(s);
    EXPECT_EQ(str::hexdump(view, ','), str::hexdump(s.data(), s.length(), ','));
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

    // a suffix/prefix longer than what remains must not read past the string
    EXPECT_FALSE(str::starts_with("ab", "a", "bcd"));
    EXPECT_FALSE(str::ends_with("ab", "b", "cde"));
}

TEST_F(TestStr, test_str_predicates)
{
    EXPECT_TRUE(str::iequals("hello", "HELLO"));
    EXPECT_TRUE(str::iequals("", ""));
    EXPECT_FALSE(str::iequals("hello", "hell"));
    EXPECT_FALSE(str::iequals("hello", "hello "));

    EXPECT_TRUE(str::is_all_spaces(" \t\n "));
    // an empty string is all spaces
    EXPECT_TRUE(str::is_all_spaces(""));
    EXPECT_FALSE(str::is_all_spaces("a "));

    // signs and spaces are tolerated anywhere (pinned behavior)
    EXPECT_TRUE(str::is_number("-123"));
    EXPECT_TRUE(str::is_number(" 12 3 "));
    EXPECT_TRUE(str::is_number("--5"));
    EXPECT_TRUE(str::is_number("1-2"));
    EXPECT_TRUE(str::is_number(""));
    EXPECT_FALSE(str::is_number("a"));
    EXPECT_TRUE(str::is_number("ff", 16));
    EXPECT_FALSE(str::is_number("fg", 16));

    EXPECT_TRUE(str::is_digit('7'));
    EXPECT_TRUE(str::is_digit('7', 8));
    EXPECT_FALSE(str::is_digit('8', 8));
    EXPECT_TRUE(str::is_digit('a', 16));
    EXPECT_TRUE(str::is_digit('F', 16));
    EXPECT_FALSE(str::is_digit('g', 16));
}

TEST_F(TestStr, test_str_time_format)
{
    std::string fmt = str::format_time(time::seconds(1) + time::minutes(2) + time::hours(3), "%X");
    EXPECT_EQ(fmt, "03:02:01");
}

TEST_F(TestStr, test_str_wrap)
{
    EXPECT_EQ(str::wrap("", 5), "");
    EXPECT_EQ(str::wrap("hello", 10), "hello");
    // truncation keeps room for the ellipsis twice (pinned behavior)
    EXPECT_EQ(str::wrap("hello world", 10), "hell...");
    EXPECT_EQ(str::wrap("hello world", 14), "hello wo...");
    EXPECT_EQ(str::wrap("hello world", 16), "hello world");
    // a truncated ellipsis when there is no room for it
    EXPECT_EQ(str::wrap("hello", 2), "..");
}

TEST_F(TestStr, test_str_conversion_bool_char)
{
    bool b = false;
    EXPECT_TRUE(str::to_bool("1", b));
    EXPECT_TRUE(b);
    EXPECT_TRUE(str::to_bool("0", b));
    EXPECT_FALSE(b);
    EXPECT_TRUE(str::to_bool("true", b));
    EXPECT_TRUE(b);
    EXPECT_TRUE(str::to_bool("false", b));
    EXPECT_FALSE(b);
    // case variations are not accepted (pinned behavior)
    EXPECT_FALSE(str::to_bool("True", b));
    EXPECT_FALSE(str::to_bool("FALSE", b));
    EXPECT_FALSE(str::to_bool("", b));
    EXPECT_FALSE(str::to_bool("nope", b));

    char c = 'z';
    EXPECT_TRUE(str::to_char("c", c));
    EXPECT_EQ(c, 'c');
    c = 'z';
    EXPECT_TRUE(str::to_char("'c'", c));
    EXPECT_EQ(c, 'c');
    EXPECT_FALSE(str::to_char("ab", c));
    EXPECT_FALSE(str::to_char("", c));
    // a non printable char leaves the value untouched (pinned behavior)
    c = 'z';
    EXPECT_TRUE(str::to_char("\n", c));
    EXPECT_EQ(c, 'z');
}

TEST_F(TestStr, test_str_join_split)
{
    EXPECT_EQ(str::join({"a", "b", "c"}, "-"), "a-b-c");
    EXPECT_EQ(str::join({"a", "b"}), "a\nb");

    const std::vector<std::string> vec = {"x", "y"};
    EXPECT_EQ(str::join(std::span<const std::string>(vec), "+"), "x+y");

    std::vector<std::string_view> views = {"x", "y"};
    EXPECT_EQ(str::join(std::span<std::string_view>(views), "+"), "x+y");

    const char *arr[] = {"x", "y"};
    EXPECT_EQ(str::join(arr, "+"), "x+y");

    auto splitted = str::split("hello  world");
    ASSERT_EQ(splitted.size(), 2u);
    EXPECT_EQ(splitted[0], "hello");
    EXPECT_EQ(splitted[1], "world");

    splitted = str::split("a;b;c", ';');
    ASSERT_EQ(splitted.size(), 3u);
    EXPECT_EQ(splitted[2], "c");

    splitted = str::split("a::b", "::");
    ASSERT_EQ(splitted.size(), 2u);
    EXPECT_EQ(splitted[1], "b");

    EXPECT_TRUE(str::split("").empty());

    std::string appended;
    str::append_sep(appended, "a");
    EXPECT_EQ(appended, "a");
    str::append_sep(appended, "b");
    EXPECT_EQ(appended, "a,b");
    str::append_sep(appended, "c", "|");
    EXPECT_EQ(appended, "a,b|c");
}

TEST_F(TestStr, test_str_table)
{
    const char *table[] = {"a", "b", nullptr};
    EXPECT_EQ(str::table_len(table), 2u);
    EXPECT_EQ(str::table_span(table).size(), 2u);

    char a[] = "hello";
    char b[] = "world";
    char *mutable_table[] = {a, b, nullptr};
    EXPECT_EQ(str::table_len(mutable_table), 2u);
    EXPECT_EQ(str::table_span(mutable_table).size(), 2u);

    EXPECT_THROW(str::table_len((const char **)nullptr), std::invalid_argument);
}

TEST_F(TestStr, test_str_generate_random)
{
    constexpr std::string_view
        charset = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz\n\t ()[]{}'123456789!@#$%^&*_+";

    const std::string str = str::generate_random(100);
    ASSERT_EQ(str.size(), 100u);
    for (const char c : str)
        EXPECT_NE(charset.find(c), std::string_view::npos) << "unexpected char: " << c;

    EXPECT_TRUE(str::generate_random(0).empty());
    EXPECT_NE(str::generate_random(64), str::generate_random(64));
}

TEST_F(TestStr, test_str_csub)
{
    char *result = str::csub("hello world", {0, 4});
    ASSERT_NE(result, nullptr);
    EXPECT_STREQ(result, "hello");
    delete[] result;

    result = str::csub("hello world", {6, -1});
    ASSERT_NE(result, nullptr);
    EXPECT_STREQ(result, "world");
    delete[] result;

    result = str::csub("hello world", {-5, -1});
    ASSERT_NE(result, nullptr);
    EXPECT_STREQ(result, "world");
    delete[] result;

    result = str::csub("hello world");
    ASSERT_NE(result, nullptr);
    EXPECT_STREQ(result, "hello world");
    delete[] result;

    EXPECT_EQ(str::csub("hello world", {5, 2}), nullptr);
}

TEST_F(TestStr, test_str_csub_negative)
{
    char *result = str::csub("abcdefghij", {-3, -1});
    ASSERT_NE(result, nullptr);
    EXPECT_STREQ(result, "hij");
    delete[] result;

    result = str::csub("abcdefghij", {-30, -2});
    ASSERT_NE(result, nullptr);
    EXPECT_STREQ(result, "abcdefghi");
    delete[] result;
}

TEST_F(TestStr, test_str_csub_empty)
{
    EXPECT_EQ(str::csub("hello", {10, 3}), nullptr);
    EXPECT_EQ(str::csub("hello", {3, 1}), nullptr);
    EXPECT_EQ(str::csub("hello", {-1, -3}), nullptr);
    EXPECT_EQ(str::csub("", {}), nullptr);
}

TEST_F(TestStr, test_str_csub_clamp)
{
    char *result = str::csub("hello", {0, 100});
    ASSERT_NE(result, nullptr);
    EXPECT_STREQ(result, "hello");
    delete[] result;

    result = str::csub("hello", {-100, -1});
    ASSERT_NE(result, nullptr);
    EXPECT_STREQ(result, "hello");
    delete[] result;
}

} // namespace test
