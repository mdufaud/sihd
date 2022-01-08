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
                ::srand(::time(NULL));
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

            void gen_random(char *s, size_t size)
            {
                static const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz\n\t ()[]{}'";

                for (size_t i = 0; i < size; ++i)
                    s[i] = charset[rand() % (sizeof(charset) - 1)];
                s[size] = 0;
            }

            std::string _base_test_dir = sihd::util::Files::combine({getenv("TEST_PATH"), "util", "linereader"});
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
        // EXPECT_EQ(size, strlen("hello world"));
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
        // EXPECT_EQ(size, strlen("hello world"));
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
        // EXPECT_EQ(size, strlen("hello world"));
        EXPECT_NE(line, nullptr);
        if (line != nullptr)
        {
            EXPECT_STREQ(line, "hello world");
        }

        LOG(info, "Second read");
        EXPECT_TRUE(reader.read_next());
        EXPECT_TRUE(reader.get_read_data(&line, &size));
        // EXPECT_EQ(size, strlen("how are you"));
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
        // EXPECT_EQ(size, strlen("hello world"));
        EXPECT_NE(line, nullptr);
        if (line != nullptr)
        {
            EXPECT_STREQ(line, "hello world");
        }

        LOG(info, "Second read");
        EXPECT_TRUE(reader.read_next());
        EXPECT_TRUE(reader.get_read_data(&line, &size));
        // EXPECT_EQ(size, strlen("how are you"));
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
        // EXPECT_EQ(size, strlen("hello world"));
        EXPECT_NE(line, nullptr);
        if (line != nullptr)
        {
            EXPECT_STREQ(line, "hello world");
        }

        LOG(info, "Second read");
        EXPECT_TRUE(reader.read_next());
        EXPECT_TRUE(reader.get_read_data(&line, &size));
        // EXPECT_EQ(size, strlen("!"));
        EXPECT_NE(line, nullptr);
        if (line != nullptr)
        {
            EXPECT_STREQ(line, "!");
        }

        LOG(info, "Third read");
        EXPECT_TRUE(reader.read_next());
        EXPECT_TRUE(reader.get_read_data(&line, &size));
        // EXPECT_EQ(size, 0u);
        EXPECT_NE(line, nullptr);
        if (line != nullptr)
        {
            EXPECT_STREQ(line, "");
        }

        LOG(info, "Fourth read");
        EXPECT_TRUE(reader.read_next());
        EXPECT_TRUE(reader.get_read_data(&line, &size));
        // EXPECT_EQ(size, 0u);
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
        // EXPECT_EQ(size, strlen("hello world"));
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

    TEST_F(TestLineReader, test_linereader_perf)
    {
        size_t filesize = 4096 * 25;
        char filecontent[filesize + 1];
        this->gen_random(filecontent, filesize);

        std::string path_input = Files::combine(_base_test_dir, "perf_filegen.txt");
        std::string path_file = Files::combine(_base_test_dir, "perf_compare_file.txt");
        std::string path_line_reader = Files::combine(_base_test_dir, "perf_compare_line_reader.txt");

        LOG(info, "Input: " << path_input);
        LOG(info, "Output file: " << path_file);
        LOG(info, "Output line reader: " << path_line_reader);

        File writer(path_input, "w");
        writer.write(filecontent, filesize);
        writer.close();

        char *line = nullptr;
        size_t size = 0;
        size_t total_file = 0;
        writer.open(path_file, "w");
        {
            Timeit t("file");
            File file;
            file.open(path_input, "r");
            while (file.read_line(&line, &size) > 0)
            {
                writer.write(line);
                total_file += strlen(line);
            }
            // important - getdelim allocates line but you have to free it
            free(line);
            file.close();
        }
        writer.close();
        size_t total_rl = 0;
        writer.open(path_line_reader, "w");
        line = nullptr;
        size = 0;
        {
            Timeit it("line-reader");
            LineReader reader("line-reader");
            // reader.set_read_buffsize(4096 * 4);
            reader.set_delimiter_in_line(true);
            reader.open(path_input);
            while (reader.read_next())
            {
                reader.get_read_data(&line, &size);
                writer.write(line);
                total_rl += strlen(line);
            }
            reader.close();
        }
        writer.close();
        EXPECT_EQ(total_rl, total_file);
        EXPECT_TRUE(Files::are_equals(path_input, path_file));
        EXPECT_TRUE(Files::are_equals(path_input, path_line_reader));
    }

}