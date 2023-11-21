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
        TestSplitter() { sihd::util::LoggerManager::basic(); }

        virtual ~TestSplitter() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestSplitter, test_splitter_method)
{
    Splitter splitter;

    splitter.set_delimiter_spaces();
    std::vector<std::string> split1 = splitter.split("\thello  \r  world\f \v !\n");
    EXPECT_EQ(split1.size(), 3u);
    EXPECT_EQ(split1[0], "hello");
    EXPECT_EQ(split1[1], "world");
    EXPECT_EQ(split1[2], "!");

    splitter.set_delimiter_method(&isspace);
    std::vector<std::string> split2 = splitter.split("\thello  \r  world\f \v !\n");
    EXPECT_EQ(split2.size(), 3u);
    EXPECT_EQ(split2[0], "hello");
    EXPECT_EQ(split2[1], "world");
    EXPECT_EQ(split2[2], "!");
}

TEST_F(TestSplitter, test_splitter_empty)
{
    Splitter splitter(",");
    std::vector<std::string> split1 = splitter.split("1,2,,,");
    EXPECT_EQ(split1.size(), 2u);
    if (split1.size() >= 2)
    {
        EXPECT_EQ(split1[0], "1");
        EXPECT_EQ(split1[1], "2");
    }

    splitter.set_empty_delimitations(true);
    std::vector<std::string> split2 = splitter.split("1,2,,,");
    EXPECT_EQ(split2.size(), 5u);
    if (split2.size() >= 5)
    {
        EXPECT_EQ(split2[0], "1");
        EXPECT_EQ(split2[1], "2");
        EXPECT_EQ(split2[2], "");
        EXPECT_EQ(split2[3], "");
        EXPECT_EQ(split2[4], "");
    }

    std::vector<std::string> split3 = splitter.split("1,2,,4,,,6");
    EXPECT_EQ(split3.size(), 7u);
    if (split3.size() >= 7)
    {
        EXPECT_EQ(split3[0], "1");
        EXPECT_EQ(split3[1], "2");
        EXPECT_EQ(split3[2], "");
        EXPECT_EQ(split3[3], "4");
        EXPECT_EQ(split3[4], "");
        EXPECT_EQ(split3[5], "");
        EXPECT_EQ(split3[6], "6");
    }
}

TEST_F(TestSplitter, test_splitter_delimiter)
{
    Splitter splitter(" ");

    std::vector<std::string> split1 = splitter.split("hello world");
    EXPECT_EQ(split1.size(), 2u);
    EXPECT_EQ(split1[0], "hello");
    EXPECT_EQ(split1[1], "world");
    std::vector<std::string> split2 = splitter.split(" hello  world ");
    EXPECT_EQ(split2.size(), 2u);
    EXPECT_EQ(split2[0], "hello");
    EXPECT_EQ(split2[1], "world");

    splitter.set_delimiter("lo");
    std::vector<std::string> split3 = splitter.split("hell");
    EXPECT_EQ(split3.size(), 1u);
    EXPECT_EQ(split3[0], "hell");

    splitter.set_delimiter("hell");
    std::vector<std::string> split4 = splitter.split("hell");
    EXPECT_EQ(split4.size(), 0u);

    splitter.set_delimiter("");
    std::vector<std::string> split5 = splitter.split("");
    EXPECT_EQ(split5.size(), 1u);
    EXPECT_EQ(split5[0], "");
    std::vector<std::string> split6 = splitter.split("hello");
    EXPECT_EQ(split6.size(), 1u);
    EXPECT_EQ(split6[0], "hello");

    splitter.set_delimiter("=");
    std::vector<std::string> split7 = splitter.split("key=");
    EXPECT_EQ(split7.size(), 1u);
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
        SIHD_LOG(debug, split);
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
        SIHD_LOG(debug, split);
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
        SIHD_LOG(debug, split);
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
        SIHD_LOG(debug, split);
    }
    EXPECT_EQ(unclosed_split.size(), 1u);
    if (unclosed_split.size() == 1)
    {
        EXPECT_EQ(unclosed_split[0], "'hello world  !");
    }
}
} // namespace test
