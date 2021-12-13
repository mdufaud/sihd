#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Files.hpp>
#include <sihd/util/LineReader.hpp>
#include <sihd/util/Profiling.hpp>

namespace test
{
    LOGGER;
    using namespace sihd::util;
    class TestLineReader: public ::testing::Test
    {
        protected:
            TestLineReader()
            {
                sihd::util::LoggerManager::basic();
                sihd::util::Files::make_directories(_base_test_dir);
            }

            virtual ~TestLineReader()
            {
                sihd::util::LoggerManager::clear_loggers();
            }

            virtual void SetUp()
            {
            }

            virtual void TearDown()
            {
            }

            std::string _base_test_dir = sihd::util::Files::combine({getenv("TEST_PATH"), "util", "LineReader"});
    };

    TEST_F(TestLineReader, test_linereader_one_line)
    {
        LineReader reader("line-reader");
        char *line;
        size_t size;

        std::string path = Files::combine(_base_test_dir, "one_line.txt");

        LOG(info, "Writing test file: " << path);
        EXPECT_TRUE(Files::write(path, "hello world"));

        EXPECT_TRUE(reader.open(path));
        LOG(info, "Reading");
        EXPECT_TRUE(reader.read_next());
        EXPECT_TRUE(reader.get_read_data(&line, &size));
        EXPECT_EQ(size, strlen("hello world"));
        EXPECT_NE(line, nullptr);
        if (line != nullptr)
        {
            EXPECT_STREQ(line, "hello world");
        }
        EXPECT_FALSE(reader.read_next());
        EXPECT_TRUE(reader.close());

        // testing with a line feed at the end
        LOG(info, "Writing test file with linefeed at the end: " << path);
        EXPECT_TRUE(Files::write(path, "hello world\n"));

        EXPECT_TRUE(reader.open(path));
        LOG(info, "Reading");
        EXPECT_TRUE(reader.read_next());
        EXPECT_TRUE(reader.get_read_data(&line, &size));
        EXPECT_EQ(size, strlen("hello world"));
        EXPECT_NE(line, nullptr);
        if (line != nullptr)
        {
            EXPECT_STREQ(line, "hello world");
        }
        EXPECT_FALSE(reader.read_next());
        EXPECT_TRUE(reader.close());
    }

    TEST_F(TestLineReader, test_linereader_two_lines)
    {
        LineReader reader("line-reader");
        char *line;
        size_t size;

        std::string path = Files::combine(_base_test_dir, "two_lines.txt");

        LOG(info, "Writing test file: " << path);
        EXPECT_TRUE(Files::write(path, "hello world\nhow are you"));

        EXPECT_TRUE(reader.open(path));

        LOG(info, "First read");
        EXPECT_TRUE(reader.read_next());
        EXPECT_TRUE(reader.get_read_data(&line, &size));
        EXPECT_EQ(size, strlen("hello world"));
        EXPECT_NE(line, nullptr);
        if (line != nullptr)
        {
            EXPECT_STREQ(line, "hello world");
        }

        LOG(info, "Second read");
        EXPECT_TRUE(reader.read_next());
        EXPECT_TRUE(reader.get_read_data(&line, &size));
        EXPECT_EQ(size, strlen("how are you"));
        EXPECT_NE(line, nullptr);
        if (line != nullptr)
        {
            EXPECT_STREQ(line, "how are you");
        }
        EXPECT_FALSE(reader.read_next());
        EXPECT_FALSE(reader.read_next());
        EXPECT_TRUE(reader.close());

        // testing with a line feed at the end
        LOG(info, "Writing test file with linefeed at the end: " << path);
        EXPECT_TRUE(Files::write(path, "hello world\nhow are you\n"));

        EXPECT_TRUE(reader.open(path));

        LOG(info, "First read");
        EXPECT_TRUE(reader.read_next());
        EXPECT_TRUE(reader.get_read_data(&line, &size));
        EXPECT_EQ(size, strlen("hello world"));
        EXPECT_NE(line, nullptr);
        if (line != nullptr)
        {
            EXPECT_STREQ(line, "hello world");
        }

        LOG(info, "Second read");
        EXPECT_TRUE(reader.read_next());
        EXPECT_TRUE(reader.get_read_data(&line, &size));
        EXPECT_EQ(size, strlen("how are you"));
        EXPECT_NE(line, nullptr);
        if (line != nullptr)
        {
            EXPECT_STREQ(line, "how are you");
        }
        EXPECT_FALSE(reader.read_next());
        EXPECT_FALSE(reader.read_next());
        EXPECT_TRUE(reader.close());
    }

    TEST_F(TestLineReader, test_linereader_multiple_feeds)
    {
        LineReader reader("line-reader");
        char *line;
        size_t size;

        std::string path = Files::combine(_base_test_dir, "multiple_feeds.txt");

        LOG(info, "Writing test file: " << path);
        EXPECT_TRUE(Files::write(path, "hello world\n!\n\n\n"));

        EXPECT_TRUE(reader.open(path));

        LOG(info, "First read");
        EXPECT_TRUE(reader.read_next());
        EXPECT_TRUE(reader.get_read_data(&line, &size));
        EXPECT_EQ(size, strlen("hello world"));
        EXPECT_NE(line, nullptr);
        if (line != nullptr)
        {
            EXPECT_STREQ(line, "hello world");
        }

        LOG(info, "Second read");
        EXPECT_TRUE(reader.read_next());
        EXPECT_TRUE(reader.get_read_data(&line, &size));
        EXPECT_EQ(size, strlen("!"));
        EXPECT_NE(line, nullptr);
        if (line != nullptr)
        {
            EXPECT_STREQ(line, "!");
        }

        LOG(info, "Third read");
        EXPECT_TRUE(reader.read_next());
        EXPECT_TRUE(reader.get_read_data(&line, &size));
        EXPECT_EQ(size, 0u);
        EXPECT_NE(line, nullptr);
        if (line != nullptr)
        {
            EXPECT_STREQ(line, "");
        }

        LOG(info, "Fourth read");
        EXPECT_TRUE(reader.read_next());
        EXPECT_TRUE(reader.get_read_data(&line, &size));
        EXPECT_EQ(size, 0u);
        EXPECT_NE(line, nullptr);
        if (line != nullptr)
        {
            EXPECT_STREQ(line, "");
        }
        EXPECT_FALSE(reader.read_next());
        EXPECT_FALSE(reader.read_next());
        EXPECT_TRUE(reader.close());
    }

    TEST_F(TestLineReader, test_linereader_none)
    {
        LineReader reader("line-reader");
        std::string path = Files::combine(_base_test_dir, "nothing.txt");

        LOG(info, "Writing test file: " << path);
        EXPECT_TRUE(Files::write(path, ""));

        EXPECT_TRUE(reader.open(path));
        EXPECT_FALSE(reader.read_next());
        EXPECT_TRUE(reader.close());
    }

    TEST_F(TestLineReader, test_linereader_low_buffer)
    {
        LineReader reader("line-reader");
        char *line;
        size_t size;

        std::string path = Files::combine(_base_test_dir, "buffer_test.txt");

        LOG(info, "Writing test file: " << path);
        EXPECT_TRUE(Files::write(path, "hello world\nhow are you\n?\n"));

        EXPECT_TRUE(reader.set_read_buffsize(1));
        EXPECT_TRUE(reader.open(path));
        EXPECT_TRUE(reader.read_next());
        // test read
        EXPECT_TRUE(reader.get_read_data(&line, &size));
        EXPECT_EQ(size, strlen("hello world"));
        EXPECT_NE(line, nullptr);
        if (line != nullptr)
        {
            EXPECT_STREQ(line, "hello world");
        }
        EXPECT_TRUE(reader.read_next());
        EXPECT_TRUE(reader.read_next());
        EXPECT_FALSE(reader.read_next());
        EXPECT_TRUE(reader.close());
    }



}