#include <gtest/gtest.h>
#include <sihd/util/Logger.hpp>
#include <sihd/util/fs.hpp>
#include <sihd/util/TmpDir.hpp>
#include <sihd/util/LineReader.hpp>
#include <sihd/util/Profiling.hpp>

namespace test
{
    SIHD_LOGGER;
    using namespace sihd::util;
    class TestLineReader: public ::testing::Test
    {
        protected:
            TestLineReader()
            {
                sihd::util::LoggerManager::basic();
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

            TmpDir _tmp_dir;
    };

    TEST_F(TestLineReader, test_linereader_one_line)
    {
        LineReader reader("line-reader");
        ArrCharView view;

        std::string path = fs::combine(_tmp_dir.path(), "one_line.txt");

        SIHD_LOG(info, "Writing test file: {}", path);
        EXPECT_TRUE(fs::write(path, "hello world"));

        EXPECT_TRUE(reader.open(path));
        SIHD_LOG(info, "Reading");
        EXPECT_TRUE(reader.read_next());
        EXPECT_TRUE(reader.get_read_data(view));
        // EXPECT_EQ(size, strlen("hello world"));
        ASSERT_TRUE(view);
        EXPECT_EQ(view, "hello world");
        EXPECT_FALSE(reader.read_next());
        EXPECT_TRUE(reader.close());

        // testing with a line feed at the end
        SIHD_LOG(info, "Writing test file with linefeed at the end: {}", path);
        EXPECT_TRUE(fs::write(path, "hello world\n"));

        EXPECT_TRUE(reader.open(path));
        SIHD_LOG(info, "Reading");
        EXPECT_TRUE(reader.read_next());
        EXPECT_TRUE(reader.get_read_data(view));
        // EXPECT_EQ(size, strlen("hello world"));
        ASSERT_TRUE(view);
        EXPECT_EQ(view, "hello world");
        EXPECT_FALSE(reader.read_next());
        EXPECT_TRUE(reader.close());
    }

    TEST_F(TestLineReader, test_linereader_two_lines)
    {
        LineReader reader("line-reader");
        ArrCharView view;

        std::string path = fs::combine(_tmp_dir.path(), "two_lines.txt");

        SIHD_LOG(info, "Writing test file: {}", path);
        EXPECT_TRUE(fs::write(path, "hello world\nhow are you"));

        EXPECT_TRUE(reader.open(path));

        SIHD_LOG(info, "First read");
        EXPECT_TRUE(reader.read_next());
        EXPECT_TRUE(reader.get_read_data(view));
        // EXPECT_EQ(size, strlen("hello world"));
        ASSERT_TRUE(view);
        EXPECT_EQ(view, "hello world");

        SIHD_LOG(info, "Second read");
        EXPECT_TRUE(reader.read_next());
        EXPECT_TRUE(reader.get_read_data(view));
        // EXPECT_EQ(size, strlen("how are you"));
        ASSERT_TRUE(view);
        EXPECT_EQ(view, "how are you");
        EXPECT_FALSE(reader.read_next());
        EXPECT_FALSE(reader.read_next());
        EXPECT_TRUE(reader.close());

        // testing with a line feed at the end
        SIHD_LOG(info, "Writing test file with linefeed at the end: {}", path);
        EXPECT_TRUE(fs::write(path, "hello world\nhow are you\n"));

        EXPECT_TRUE(reader.open(path));

        SIHD_LOG(info, "First read");
        EXPECT_TRUE(reader.read_next());
        EXPECT_TRUE(reader.get_read_data(view));
        // EXPECT_EQ(size, strlen("hello world"));
        ASSERT_TRUE(view);
        EXPECT_EQ(view, "hello world");

        SIHD_LOG(info, "Second read");
        EXPECT_TRUE(reader.read_next());
        EXPECT_TRUE(reader.get_read_data(view));
        // EXPECT_EQ(size, strlen("how are you"));
        ASSERT_TRUE(view);
        EXPECT_EQ(view, "how are you");
        EXPECT_FALSE(reader.read_next());
        EXPECT_FALSE(reader.read_next());
        EXPECT_TRUE(reader.close());
    }

    TEST_F(TestLineReader, test_linereader_multiple_feeds)
    {
        LineReader reader("line-reader");
        ArrCharView view;

        std::string path = fs::combine(_tmp_dir.path(), "multiple_feeds.txt");

        SIHD_LOG(info, "Writing test file: {}", path);
        EXPECT_TRUE(fs::write(path, "hello world\n!\n\n\n"));

        EXPECT_TRUE(reader.open(path));

        SIHD_LOG(info, "First read");
        EXPECT_TRUE(reader.read_next());
        EXPECT_TRUE(reader.get_read_data(view));
        // EXPECT_EQ(size, strlen("hello world"));
        ASSERT_TRUE(view);
        EXPECT_EQ(view, "hello world");

        SIHD_LOG(info, "Second read");
        EXPECT_TRUE(reader.read_next());
        EXPECT_TRUE(reader.get_read_data(view));
        // EXPECT_EQ(size, strlen("!"));
        ASSERT_TRUE(view);
        EXPECT_EQ(view, "!");

        SIHD_LOG(info, "Third read");
        EXPECT_TRUE(reader.read_next());
        EXPECT_TRUE(reader.get_read_data(view));
        // EXPECT_EQ(size, 0u);
        ASSERT_TRUE(view);
        EXPECT_EQ(view, "");

        SIHD_LOG(info, "Fourth read");
        EXPECT_TRUE(reader.read_next());
        EXPECT_TRUE(reader.get_read_data(view));
        // EXPECT_EQ(size, 0u);
        ASSERT_TRUE(view);
        EXPECT_EQ(view, "");
        EXPECT_FALSE(reader.read_next());
        EXPECT_FALSE(reader.read_next());
        EXPECT_TRUE(reader.close());
    }

    TEST_F(TestLineReader, test_linereader_none)
    {
        LineReader reader("line-reader");
        std::string path = fs::combine(_tmp_dir.path(), "nothing.txt");

        SIHD_LOG(info, "Writing test file: {}", path);
        EXPECT_TRUE(fs::write(path, ""));

        EXPECT_TRUE(reader.open(path));
        EXPECT_FALSE(reader.read_next());
        EXPECT_TRUE(reader.close());
    }

    TEST_F(TestLineReader, test_linereader_low_buffer)
    {
        LineReader reader("line-reader");
        ArrCharView view;

        std::string path = fs::combine(_tmp_dir.path(), "buffer_test.txt");

        SIHD_LOG(info, "Writing test file: {}", path);
        EXPECT_TRUE(fs::write(path, "hello world\nhow are you\n?\n"));

        EXPECT_TRUE(reader.set_read_buffsize(1));
        EXPECT_TRUE(reader.open(path));
        EXPECT_TRUE(reader.read_next());
        // test read
        EXPECT_TRUE(reader.get_read_data(view));
        // EXPECT_EQ(size, strlen("hello world"));
        ASSERT_TRUE(view);
        EXPECT_EQ(view, "hello world");
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

        std::string path_input = fs::combine(_tmp_dir.path(), "perf_filegen.txt");
        std::string path_file = fs::combine(_tmp_dir.path(), "perf_compare_file.txt");
        std::string path_line_reader = fs::combine(_tmp_dir.path(), "perf_compare_line_reader.txt");

        SIHD_LOG(info, "Input: {}", path_input);
        SIHD_LOG(info, "Output file: {}", path_file);
        SIHD_LOG(info, "Output line reader: {}", path_line_reader);

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
        ArrCharView view;
        {
            Timeit it("line-reader");
            LineReader reader("line-reader");
            // reader.set_read_buffsize(4096 * 4);
            reader.set_delimiter_in_line(true);
            reader.open(path_input);
            while (reader.read_next())
            {
                reader.get_read_data(view);
                writer.write(view);
                total_rl += strlen(view.data());
            }
            reader.close();
        }
        writer.close();
        EXPECT_EQ(total_rl, total_file);
        EXPECT_TRUE(fs::are_equals(path_input, path_file));
        EXPECT_TRUE(fs::are_equals(path_input, path_line_reader));
    }

}
