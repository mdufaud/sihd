#if !defined(__SIHD_WINDOWS__)
# include <fcntl.h>
# include <unistd.h>
#endif

#include <gtest/gtest.h>
#include <sihd/sys/LineReader.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/sys/TmpDir.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/util/build.hpp>
#include <sihd/util/profiling.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;
using namespace sihd::sys;
class TestLineReader: public ::testing::Test
{
    protected:
        TestLineReader()
        {
            sihd::util::LoggerManager::stream();
            ::srand(::time(NULL));
        }

        virtual ~TestLineReader() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}

        TmpDir _tmp_dir;
};

TEST_F(TestLineReader, test_linereader_one_line)
{
    LineReader reader;
    ArrCharView view;

    std::string path = fs::combine(_tmp_dir.path(), "one_line.txt");

    SIHD_LOG(info, "Writing test file: {}", path);
    EXPECT_TRUE(fs::write(path, "hello world"));

    EXPECT_TRUE(reader.open(path));
    SIHD_LOG(info, "Reading");
    EXPECT_TRUE(reader.read_next());
    EXPECT_TRUE(reader.get_read_data(view));
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
    ASSERT_TRUE(view);
    EXPECT_EQ(view, "hello world");
    EXPECT_FALSE(reader.read_next());
    EXPECT_TRUE(reader.close());
}

TEST_F(TestLineReader, test_linereader_two_lines)
{
    LineReader reader;
    ArrCharView view;

    std::string path = fs::combine(_tmp_dir.path(), "two_lines.txt");

    SIHD_LOG(info, "Writing test file: {}", path);
    EXPECT_TRUE(fs::write(path, "hello world\nhow are you"));

    EXPECT_TRUE(reader.open(path));

    SIHD_LOG(info, "First read");
    EXPECT_TRUE(reader.read_next());
    EXPECT_TRUE(reader.get_read_data(view));
    ASSERT_TRUE(view);
    EXPECT_EQ(view, "hello world");

    SIHD_LOG(info, "Second read");
    EXPECT_TRUE(reader.read_next());
    EXPECT_TRUE(reader.get_read_data(view));
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
    ASSERT_TRUE(view);
    EXPECT_EQ(view, "hello world");

    SIHD_LOG(info, "Second read");
    EXPECT_TRUE(reader.read_next());
    EXPECT_TRUE(reader.get_read_data(view));
    ASSERT_TRUE(view);
    EXPECT_EQ(view, "how are you");
    EXPECT_FALSE(reader.read_next());
    EXPECT_FALSE(reader.read_next());
    EXPECT_TRUE(reader.close());
}

TEST_F(TestLineReader, test_linereader_multiple_feeds)
{
    LineReader reader;
    ArrCharView view;

    std::string path = fs::combine(_tmp_dir.path(), "multiple_feeds.txt");

    SIHD_LOG(info, "Writing test file: {}", path);
    EXPECT_TRUE(fs::write(path, "hello world\n!\n\n\n"));

    EXPECT_TRUE(reader.open(path));

    SIHD_LOG(info, "First read");
    EXPECT_TRUE(reader.read_next());
    EXPECT_TRUE(reader.get_read_data(view));
    ASSERT_TRUE(view);
    EXPECT_EQ(view, "hello world");

    SIHD_LOG(info, "Second read");
    EXPECT_TRUE(reader.read_next());
    EXPECT_TRUE(reader.get_read_data(view));
    ASSERT_TRUE(view);
    EXPECT_EQ(view, "!");

    SIHD_LOG(info, "Third read");
    EXPECT_TRUE(reader.read_next());
    EXPECT_TRUE(reader.get_read_data(view));
    ASSERT_TRUE(view);
    EXPECT_EQ(view, "");

    SIHD_LOG(info, "Fourth read");
    EXPECT_TRUE(reader.read_next());
    EXPECT_TRUE(reader.get_read_data(view));
    ASSERT_TRUE(view);
    EXPECT_EQ(view, "");
    EXPECT_FALSE(reader.read_next());
    EXPECT_FALSE(reader.read_next());
    EXPECT_TRUE(reader.close());
}

TEST_F(TestLineReader, test_linereader_none)
{
    LineReader reader;
    std::string path = fs::combine(_tmp_dir.path(), "nothing.txt");

    SIHD_LOG(info, "Writing test file: {}", path);
    EXPECT_TRUE(fs::write(path, ""));

    EXPECT_TRUE(reader.open(path));
    EXPECT_FALSE(reader.read_next());
    EXPECT_TRUE(reader.close());
}

TEST_F(TestLineReader, test_linereader_low_buffer)
{
    LineReader reader;
    ArrCharView view;

    std::string path = fs::combine(_tmp_dir.path(), "buffer_test.txt");

    SIHD_LOG(info, "Writing test file: {}", path);
    EXPECT_TRUE(fs::write(path, "hello world\nhow are you\n?\n"));

    EXPECT_TRUE(reader.set_read_buffsize(1));
    EXPECT_TRUE(reader.open(path));
    EXPECT_TRUE(reader.read_next());
    // test read
    EXPECT_TRUE(reader.get_read_data(view));
    ASSERT_TRUE(view);
    EXPECT_EQ(view, "hello world");
    EXPECT_TRUE(reader.read_next());
    EXPECT_TRUE(reader.read_next());
    EXPECT_FALSE(reader.read_next());
    EXPECT_TRUE(reader.close());
}

TEST_F(TestLineReader, test_linereader_delimiter_in_line)
{
    LineReader reader({.delimiter_in_line = true});
    ArrCharView view;

    std::string path = fs::combine(_tmp_dir.path(), "delim_in_line.txt");
    EXPECT_TRUE(fs::write(path, "hello world\nbye\n"));

    EXPECT_TRUE(reader.open(path));
    EXPECT_TRUE(reader.read_next());
    EXPECT_TRUE(reader.get_read_data(view));
    EXPECT_EQ(view, "hello world\n");
    EXPECT_TRUE(reader.read_next());
    EXPECT_TRUE(reader.get_read_data(view));
    EXPECT_EQ(view, "bye\n");
    EXPECT_FALSE(reader.read_next());
    EXPECT_TRUE(reader.close());
}

TEST_F(TestLineReader, test_linereader_custom_delimiter)
{
    LineReader reader({.delimiter = ';'});
    ArrCharView view;

    std::string path = fs::combine(_tmp_dir.path(), "custom_delim.txt");
    EXPECT_TRUE(fs::write(path, "a;b;c"));

    EXPECT_TRUE(reader.open(path));
    EXPECT_TRUE(reader.read_next());
    EXPECT_TRUE(reader.get_read_data(view));
    EXPECT_EQ(view, "a");
    EXPECT_TRUE(reader.read_next());
    EXPECT_TRUE(reader.get_read_data(view));
    EXPECT_EQ(view, "b");
    EXPECT_TRUE(reader.read_next());
    EXPECT_TRUE(reader.get_read_data(view));
    EXPECT_EQ(view, "c");
    EXPECT_FALSE(reader.read_next());
    EXPECT_TRUE(reader.close());
}

TEST_F(TestLineReader, test_linereader_long_line)
{
    LineReader reader;
    ArrCharView view;

    const std::string long_line(2000, 'a');
    std::string path = fs::combine(_tmp_dir.path(), "long_line.txt");
    EXPECT_TRUE(fs::write(path, long_line));

    EXPECT_TRUE(reader.open(path));
    EXPECT_TRUE(reader.read_next());
    EXPECT_TRUE(reader.get_read_data(view));
    EXPECT_EQ(view, long_line);
    EXPECT_FALSE(reader.read_next());
    EXPECT_TRUE(reader.close());
}

TEST_F(TestLineReader, test_linereader_errors)
{
    LineReader reader;
    ArrCharView view;

    EXPECT_FALSE(reader.get_read_data(view));

    std::string missing = fs::combine(_tmp_dir.path(), "does_not_exist.txt");
    EXPECT_FALSE(reader.open(missing));

    std::string path = fs::combine(_tmp_dir.path(), "clean_eof.txt");
    EXPECT_TRUE(fs::write(path, "one line\n"));
    EXPECT_TRUE(reader.open(path));
    EXPECT_TRUE(reader.read_next());
    EXPECT_FALSE(reader.read_next());
    EXPECT_FALSE(reader.error());
    EXPECT_TRUE(reader.close());
}

#if !defined(__SIHD_WINDOWS__)
TEST_F(TestLineReader, test_linereader_open_fd)
{
    LineReader reader;
    ArrCharView view;

    std::string path = fs::combine(_tmp_dir.path(), "open_fd.txt");
    EXPECT_TRUE(fs::write(path, "first\nsecond\n"));

    int fd = ::open(path.c_str(), O_RDONLY);
    ASSERT_GE(fd, 0);

    EXPECT_TRUE(reader.open_fd(fd));
    EXPECT_TRUE(reader.read_next());
    EXPECT_TRUE(reader.get_read_data(view));
    EXPECT_EQ(view, "first");
    EXPECT_TRUE(reader.read_next());
    EXPECT_TRUE(reader.get_read_data(view));
    EXPECT_EQ(view, "second");
    EXPECT_FALSE(reader.read_next());
    EXPECT_TRUE(reader.close());
}

TEST_F(TestLineReader, test_linereader_fast_read_stdin)
{
    int pipefd[2];
    ASSERT_EQ(::pipe(pipefd), 0);

    const char *msg = "from stdin\n";
    const ssize_t len = static_cast<ssize_t>(::strlen(msg));
    ASSERT_EQ(::write(pipefd[1], msg, len), len);
    ASSERT_EQ(::close(pipefd[1]), 0);

    int saved_stdin = ::dup(STDIN_FILENO);
    ASSERT_GE(saved_stdin, 0);
    ASSERT_GE(::dup2(pipefd[0], STDIN_FILENO), 0);

    std::string line;
    EXPECT_TRUE(LineReader::fast_read_stdin(line));
    EXPECT_EQ(line, "from stdin");

    ASSERT_GE(::dup2(saved_stdin, STDIN_FILENO), 0);
    ::close(saved_stdin);
    ::close(pipefd[0]);
}
#endif

TEST_F(TestLineReader, test_linereader_perf)
{
    constexpr size_t test_multiplier = 1;
    constexpr size_t filesize = (4096 * 70) * test_multiplier;

    std::string random_str = str::generate_random(filesize);

    std::string path_input = fs::combine(_tmp_dir.path(), "perf_filegen.txt");
    std::string path_file = fs::combine(_tmp_dir.path(), "perf_compare_file.txt");
    std::string path_line_reader = fs::combine(_tmp_dir.path(), "perf_compare_line_reader.txt");
    std::string path_line_reader_no_memory
        = fs::combine(_tmp_dir.path(), "perf_compare_line_reader_no_memory.txt");

    SIHD_LOG(info, "Input: {}", path_input);
    SIHD_LOG(info, "Output std::getline: {}", path_file);
    SIHD_LOG(info, "Output LineReader: {}", path_line_reader);
    SIHD_LOG(info, "Output LineReader::fast_read_line: {}", path_line_reader_no_memory);

    File writer(path_input, "wb");
    ASSERT_TRUE(writer.is_open());
    ASSERT_TRUE(writer.write(random_str) == (ssize_t)random_str.size());
    ASSERT_TRUE(writer.close());

    char *line = nullptr;
    size_t size = 0;
    size_t total_file = 0;
    ASSERT_TRUE(writer.open(path_file, "wb"));
    {
        Timeit t("std::getline");
        File file;
        file.open(path_input, "rb");
        while (file.read_line(&line, &size) > 0)
        {
            writer.write(line);
            total_file += strlen(line);
        }
        // important - getdelim allocates line but you have to free it
        free(line);
        ASSERT_TRUE(file.close());
    }
    ASSERT_TRUE(writer.close());

    size_t total_rl = 0;
    ASSERT_TRUE(writer.open(path_line_reader, "wb"));
    ArrCharView view;
    {
        Timeit it("LineReader");
        LineReader reader(path_input,
                          {
                              .read_buffsize = 4096,
                              .delimiter_in_line = true,
                          });
        ASSERT_TRUE(reader.is_open());
        while (reader.read_next())
        {
            reader.get_read_data(view);
            writer.write(view);
            total_rl += view.size();
        }
        ASSERT_TRUE(reader.close());
    }
    ASSERT_TRUE(writer.close());

    size_t total_rl_no_memory = 0;
    ASSERT_TRUE(writer.open(path_line_reader_no_memory, "wb"));
    {
        Timeit it("LineReader::fast_read_line");
        File file(path_input, "rb");
        ASSERT_TRUE(file.is_open());
        std::string line;
        LineReader::LineReaderOptions options = {
            // must read one by one because there is no memory of last read
            .read_buffsize = 1,
            .delimiter_in_line = true,
        };
        while (LineReader::fast_read_line(line, file.file(), options))
        {
            writer.write(line);
            total_rl_no_memory += line.size();
        }
    }
    ASSERT_TRUE(writer.close());

    EXPECT_TRUE(fs::are_equals(path_input, path_file));

    EXPECT_EQ(total_rl, total_file);
    EXPECT_TRUE(fs::are_equals(path_input, path_line_reader));

    EXPECT_EQ(total_rl_no_memory, total_file);
    EXPECT_TRUE(fs::are_equals(path_input, path_line_reader_no_memory));
}

} // namespace test
