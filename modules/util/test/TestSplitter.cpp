#include <gtest/gtest.h>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Splitter.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;
class TestSplitter: public ::testing::Test
{
    protected:
        TestSplitter() { sihd::util::LoggerManager::stream(); }

        virtual ~TestSplitter() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestSplitter, test_splitter_from_view)
{
    std::string_view s = "toto;titi;tata";
    // "toto;titi;tata"
    s.remove_suffix(5);
    // "toto;titi"

    Splitter splitter(";");
    std::vector<std::string_view> split = splitter.split_view(s);
    ASSERT_EQ(split.size(), 2u);
    EXPECT_EQ(split[0], "toto");
    EXPECT_EQ(split[1], "titi");
}

TEST_F(TestSplitter, test_splitter_method)
{
    Splitter splitter;

    splitter.set_delimiter_spaces();
    std::vector<std::string> split1 = splitter.split("\thello  \r  world\f \v !\n");
    ASSERT_EQ(split1.size(), 3u);
    EXPECT_EQ(split1[0], "hello");
    EXPECT_EQ(split1[1], "world");
    EXPECT_EQ(split1[2], "!");

    splitter.set_delimiter_method(&isspace);
    std::vector<std::string> split2 = splitter.split("\thello  \r  world\f \v !\n");
    ASSERT_EQ(split2.size(), 3u);
    EXPECT_EQ(split2[0], "hello");
    EXPECT_EQ(split2[1], "world");
    EXPECT_EQ(split2[2], "!");
}

TEST_F(TestSplitter, test_splitter_empty)
{
    Splitter splitter(",");
    std::vector<std::string> split1 = splitter.split("1,2,,,");
    ASSERT_EQ(split1.size(), 2u);
    EXPECT_EQ(split1[0], "1");
    EXPECT_EQ(split1[1], "2");

    splitter.set_empty_delimitations(true);
    std::vector<std::string> split2 = splitter.split("1,2,,,");
    ASSERT_EQ(split2.size(), 5u);
    EXPECT_EQ(split2[0], "1");
    EXPECT_EQ(split2[1], "2");
    EXPECT_EQ(split2[2], "");
    EXPECT_EQ(split2[3], "");
    EXPECT_EQ(split2[4], "");

    std::vector<std::string> split3 = splitter.split("1,2,,4,,,6");
    ASSERT_EQ(split3.size(), 7u);
    EXPECT_EQ(split3[0], "1");
    EXPECT_EQ(split3[1], "2");
    EXPECT_EQ(split3[2], "");
    EXPECT_EQ(split3[3], "4");
    EXPECT_EQ(split3[4], "");
    EXPECT_EQ(split3[5], "");
    EXPECT_EQ(split3[6], "6");
}

TEST_F(TestSplitter, test_splitter_delimiter)
{
    Splitter splitter(" ");

    std::vector<std::string> split1 = splitter.split("hello world");
    ASSERT_EQ(split1.size(), 2u);
    EXPECT_EQ(split1[0], "hello");
    EXPECT_EQ(split1[1], "world");
    std::vector<std::string> split2 = splitter.split(" hello  world ");
    ASSERT_EQ(split2.size(), 2u);
    EXPECT_EQ(split2[0], "hello");
    EXPECT_EQ(split2[1], "world");

    splitter.set_delimiter("lo");
    std::vector<std::string> split3 = splitter.split("hell");
    ASSERT_EQ(split3.size(), 1u);
    EXPECT_EQ(split3[0], "hell");

    splitter.set_delimiter("hell");
    std::vector<std::string> split4 = splitter.split("hell");
    EXPECT_EQ(split4.size(), 0u);

    splitter.set_delimiter("");
    std::vector<std::string> split5 = splitter.split("");
    ASSERT_EQ(split5.size(), 1u);
    EXPECT_EQ(split5[0], "");
    std::vector<std::string> split6 = splitter.split("hello");
    ASSERT_EQ(split6.size(), 1u);
    EXPECT_EQ(split6[0], "hello");

    splitter.set_delimiter("=");
    std::vector<std::string> split7 = splitter.split("key=");
    ASSERT_EQ(split7.size(), 1u);
    EXPECT_EQ(split7[0], "key");
}

TEST_F(TestSplitter, test_splitter_escapes)
{
    Splitter splitter({.delimiter_str = " ", .open_escape_sequences = "(["});

    const char *cmd = "cmd (do 'something')   or don\\'t but[hurry up  mate]";

    SIHD_LOG(info, "Splitting command: '{}'", cmd);
    std::vector<std::string> splits = splitter.split(cmd);
    for (const auto & split : splits)
    {
        SIHD_LOG(debug, "{}", split);
    }
    EXPECT_EQ(splits.size(), 5u);
    if (splits.size() == 5)
    {
        EXPECT_EQ(splits[0], "cmd");
        EXPECT_EQ(splits[1], "(do 'something')");
        EXPECT_EQ(splits[2], "or");
        EXPECT_EQ(splits[3], "don\\'t");
        EXPECT_EQ(splits[4], "but[hurry up  mate]");
    }
    std::cout << std::endl;

    splitter.set_open_escape_sequences("(");
    SIHD_LOG(info, "Same string but with restriction on (");
    std::vector<std::string> splits_with_restriction = splitter.split(cmd);
    for (const auto & split : splits_with_restriction)
    {
        SIHD_LOG(debug, "{}", split);
    }
    EXPECT_EQ(splits_with_restriction.size(), 7u);
    if (splits_with_restriction.size() == 7)
    {
        EXPECT_EQ(splits_with_restriction[0], "cmd");
        EXPECT_EQ(splits_with_restriction[1], "(do 'something')");
        EXPECT_EQ(splits_with_restriction[2], "or");
        EXPECT_EQ(splits_with_restriction[3], "don\\'t");
        EXPECT_EQ(splits_with_restriction[4], "but[hurry");
        EXPECT_EQ(splits_with_restriction[5], "up");
        EXPECT_EQ(splits_with_restriction[6], "mate]");
    }

    std::cout << std::endl;

    splitter.set_escape_sequences_all();
    const char *fullcmd = "'hello world  !'";
    SIHD_LOG(info, "Splitting command: '{}'", fullcmd);
    std::vector<std::string> full_split = splitter.split(fullcmd);
    for (const auto & split : full_split)
    {
        SIHD_LOG(debug, "{}", split);
    }
    EXPECT_EQ(full_split.size(), 1u);
    if (full_split.size() == 1)
    {
        EXPECT_EQ(full_split[0], "'hello world  !'");
    }

    const char *unclosed_seq = "'hello world  !";
    SIHD_LOG(info, "Splitting command: '{}'", unclosed_seq);
    std::vector<std::string> unclosed_split = splitter.split(unclosed_seq);
    for (const auto & split : unclosed_split)
    {
        SIHD_LOG(debug, "{}", split);
    }
    EXPECT_EQ(unclosed_split.size(), 1u);
    if (unclosed_split.size() == 1)
    {
        EXPECT_EQ(unclosed_split[0], "'hello world  !");
    }
}

TEST_F(TestSplitter, test_splitter_char_delimiter)
{
    // Test constructor with char delimiter
    Splitter splitter(':');
    std::vector<std::string> split = splitter.split("one:two:three");
    ASSERT_EQ(split.size(), 3u);
    EXPECT_EQ(split[0], "one");
    EXPECT_EQ(split[1], "two");
    EXPECT_EQ(split[2], "three");

    // Test set_delimiter_char
    splitter.set_delimiter_char('|');
    std::vector<std::string> split2 = splitter.split("a|b|c|d");
    ASSERT_EQ(split2.size(), 4u);
    EXPECT_EQ(split2[0], "a");
    EXPECT_EQ(split2[3], "d");
}

TEST_F(TestSplitter, test_splitter_count_tokens)
{
    Splitter splitter(",");

    // Test count_tokens
    EXPECT_EQ(splitter.count_tokens("a,b,c"), 3);
    EXPECT_EQ(splitter.count_tokens("a"), 1);
    EXPECT_EQ(splitter.count_tokens(""), 0);
    EXPECT_EQ(splitter.count_tokens("a,b,"), 2);

    // With empty delimitations
    splitter.set_empty_delimitations(true);
    EXPECT_EQ(splitter.count_tokens("a,b,c"), 3);
    EXPECT_EQ(splitter.count_tokens("a,,c"), 3);
    EXPECT_EQ(splitter.count_tokens(",,"), 2);
}

TEST_F(TestSplitter, test_splitter_next_token)
{
    Splitter splitter(";");

    std::string_view view = "first;second;third";
    int idx = 0;

    // Get first token
    std::string_view token1 = splitter.next_token(view, &idx);
    EXPECT_EQ(token1, "first");
    EXPECT_EQ(idx, 5); // position after "first;"

    // Get second token
    std::string_view token2 = splitter.next_token(view, &idx);
    EXPECT_EQ(token2, "second");

    // Get third token
    std::string_view token3 = splitter.next_token(view, &idx);
    EXPECT_EQ(token3, "third");
}

TEST_F(TestSplitter, test_splitter_delimiter_at_edges)
{
    Splitter splitter(",");

    // Leading delimiter
    std::vector<std::string> split1 = splitter.split(",hello,world");
    ASSERT_EQ(split1.size(), 2u);
    EXPECT_EQ(split1[0], "hello");
    EXPECT_EQ(split1[1], "world");

    // Trailing delimiter
    std::vector<std::string> split2 = splitter.split("hello,world,");
    ASSERT_EQ(split2.size(), 2u);
    EXPECT_EQ(split2[0], "hello");
    EXPECT_EQ(split2[1], "world");

    // Leading and trailing delimiters
    std::vector<std::string> split3 = splitter.split(",hello,world,");
    ASSERT_EQ(split3.size(), 2u);
    EXPECT_EQ(split3[0], "hello");
    EXPECT_EQ(split3[1], "world");

    // Only delimiters
    std::vector<std::string> split4 = splitter.split(",,,");
    ASSERT_EQ(split4.size(), 0u);
}

TEST_F(TestSplitter, test_splitter_delimiter_at_edges_with_empty_delimitations)
{
    Splitter splitter(",");
    splitter.set_empty_delimitations(true);

    // Leading delimiter - doesn't create empty token at the very start (leading is skipped)
    std::vector<std::string> split1 = splitter.split(",hello,world");
    ASSERT_EQ(split1.size(), 2u);
    EXPECT_EQ(split1[0], "hello");
    EXPECT_EQ(split1[1], "world");

    // Trailing delimiter - creates empty token at the end
    std::vector<std::string> split2 = splitter.split("hello,world,");
    ASSERT_EQ(split2.size(), 3u);
    EXPECT_EQ(split2[0], "hello");
    EXPECT_EQ(split2[1], "world");
    EXPECT_EQ(split2[2], "");

    // Consecutive delimiters in the middle
    std::vector<std::string> split3 = splitter.split("hello,,world");
    ASSERT_EQ(split3.size(), 3u);
    EXPECT_EQ(split3[0], "hello");
    EXPECT_EQ(split3[1], "");
    EXPECT_EQ(split3[2], "world");

    // Only delimiters
    std::vector<std::string> split4 = splitter.split(",,,");
    ASSERT_EQ(split4.size(), 3u);
    EXPECT_EQ(split4[0], "");
    EXPECT_EQ(split4[1], "");
    EXPECT_EQ(split4[2], "");
}

TEST_F(TestSplitter, test_splitter_multi_char_delimiter)
{
    Splitter splitter("::");

    std::vector<std::string> split1 = splitter.split("one::two::three");
    ASSERT_EQ(split1.size(), 3u);
    EXPECT_EQ(split1[0], "one");
    EXPECT_EQ(split1[1], "two");
    EXPECT_EQ(split1[2], "three");

    // Multi-char delimiter with consecutive occurrences
    splitter.set_empty_delimitations(true);
    std::vector<std::string> split2 = splitter.split("a::::b");
    ASSERT_EQ(split2.size(), 3u);
    EXPECT_EQ(split2[0], "a");
    EXPECT_EQ(split2[1], "");
    EXPECT_EQ(split2[2], "b");
}

TEST_F(TestSplitter, test_splitter_options_constructor)
{
    // Test SplitterOptions with delimiter_char
    Splitter splitter1({.delimiter_char = '|'});
    std::vector<std::string> split1 = splitter1.split("x|y|z");
    ASSERT_EQ(split1.size(), 3u);
    EXPECT_EQ(split1[0], "x");
    EXPECT_EQ(split1[1], "y");
    EXPECT_EQ(split1[2], "z");

    // Test SplitterOptions with multiple settings
    Splitter splitter2({.delimiter_str = "-", .empty_delimitations = true});
    std::vector<std::string> split2 = splitter2.split("a--b");
    ASSERT_EQ(split2.size(), 3u);
    EXPECT_EQ(split2[0], "a");
    EXPECT_EQ(split2[1], "");
    EXPECT_EQ(split2[2], "b");
}

TEST_F(TestSplitter, test_splitter_options_invalid_multiple_delimiters)
{
    // Cannot specify both delimiter_char and delimiter_str
    EXPECT_THROW(Splitter splitter({.delimiter_char = ',', .delimiter_str = ";"}), std::logic_error);

    // Cannot specify both delimiter_char and delimiter_method
    EXPECT_THROW(Splitter splitter({.delimiter_char = ',', .delimiter_method = &isspace}), std::logic_error);

    // Cannot specify both delimiter_str and delimiter_method
    EXPECT_THROW(Splitter splitter({.delimiter_str = ";", .delimiter_method = &isspace}), std::logic_error);
}

TEST_F(TestSplitter, test_splitter_empty_string_with_various_delimiters)
{
    Splitter splitter1(",");
    std::vector<std::string> split1 = splitter1.split("");
    ASSERT_EQ(split1.size(), 0u);

    Splitter splitter2(" ");
    std::vector<std::string> split2 = splitter2.split("");
    ASSERT_EQ(split2.size(), 0u);

    Splitter splitter3;
    splitter3.set_delimiter_spaces();
    std::vector<std::string> split3 = splitter3.split("");
    ASSERT_EQ(split3.size(), 0u);
}

TEST_F(TestSplitter, test_splitter_split_view_consistency)
{
    Splitter splitter(":");

    std::string_view view = "a:b:c:d";
    std::vector<std::string> split_str = splitter.split(view);
    std::vector<std::string_view> split_view = splitter.split_view(view);

    // Both methods should return same number of tokens
    ASSERT_EQ(split_str.size(), split_view.size());

    // Content should match
    for (size_t i = 0; i < split_str.size(); ++i)
    {
        EXPECT_EQ(split_str[i], split_view[i]);
    }
}

TEST_F(TestSplitter, test_splitter_escape_char_configuration)
{
    Splitter splitter({.escape_char = '@', .delimiter_str = " "});

    // Test custom escape character - escape sequence doesn't work without open_escape_sequences
    std::vector<std::string> split = splitter.split("hello@ world test");
    EXPECT_EQ(split.size(), 3u);
    if (split.size() == 3)
    {
        EXPECT_EQ(split[0], "hello@");
        EXPECT_EQ(split[1], "world");
        EXPECT_EQ(split[2], "test");
    }

    // Change escape character
    splitter.set_escape_char('\\');
    std::vector<std::string> split2 = splitter.split("hello\\ world test");
    EXPECT_EQ(split2.size(), 3u);
}

TEST_F(TestSplitter, test_splitter_single_token)
{
    Splitter splitter(",");

    std::vector<std::string> split1 = splitter.split("single");
    ASSERT_EQ(split1.size(), 1u);
    EXPECT_EQ(split1[0], "single");

    std::vector<std::string_view> split_view = splitter.split_view("single");
    ASSERT_EQ(split_view.size(), 1u);
    EXPECT_EQ(split_view[0], "single");
}

TEST_F(TestSplitter, test_splitter_no_delimiter_in_string)
{
    Splitter splitter("X");

    std::vector<std::string> split = splitter.split("nodelimiterhere");
    ASSERT_EQ(split.size(), 1u);
    EXPECT_EQ(split[0], "nodelimiterhere");
}

TEST_F(TestSplitter, test_splitter_string_equals_delimiter)
{
    Splitter splitter("test");

    std::vector<std::string> split = splitter.split("test");
    EXPECT_EQ(split.size(), 0u);
}
} // namespace test
