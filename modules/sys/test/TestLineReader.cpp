#if !defined(__SIHD_WINDOWS__)
# include <fcntl.h>
# include <unistd.h>
#endif

#include <gtest/gtest.h>

#include <sihd/sys/LineReader.hpp>
#include <sihd/sys/TmpDir.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/build.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;
using namespace sihd::sys;

namespace
{

bool read_next_ok(LineReader & reader)
{
    auto res = reader.read_next();
    if (!res.has_value())
    {
        ADD_FAILURE() << res.error().message;
        return false;
    }
    return res.value();
}

} // namespace

class TestLineReader: public ::testing::Test
{
    protected:
        TestLineReader() { LoggerManager::stream(); }

        virtual ~TestLineReader() { LoggerManager::clear_loggers(); }

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
    EXPECT_TRUE(fs::write(path, "hello world").has_value());

    EXPECT_TRUE(reader.open(path).has_value());
    SIHD_LOG(info, "Reading");
    EXPECT_TRUE(read_next_ok(reader));
    EXPECT_TRUE(reader.get_read_data(view));
    ASSERT_TRUE(view);
    EXPECT_EQ(view, "hello world");
    EXPECT_FALSE(read_next_ok(reader));
    EXPECT_TRUE(reader.close());

    SIHD_LOG(info, "Writing test file with linefeed at the end: {}", path);
    EXPECT_TRUE(fs::write(path, "hello world\n").has_value());

    EXPECT_TRUE(reader.open(path).has_value());
    SIHD_LOG(info, "Reading");
    EXPECT_TRUE(read_next_ok(reader));
    EXPECT_TRUE(reader.get_read_data(view));
    ASSERT_TRUE(view);
    EXPECT_EQ(view, "hello world");
    EXPECT_FALSE(read_next_ok(reader));
    EXPECT_TRUE(reader.close());
}

TEST_F(TestLineReader, test_linereader_two_lines)
{
    LineReader reader;
    ArrCharView view;

    std::string path = fs::combine(_tmp_dir.path(), "two_lines.txt");

    SIHD_LOG(info, "Writing test file: {}", path);
    EXPECT_TRUE(fs::write(path, "hello world\nhow are you").has_value());

    EXPECT_TRUE(reader.open(path).has_value());

    SIHD_LOG(info, "First read");
    EXPECT_TRUE(read_next_ok(reader));
    EXPECT_TRUE(reader.get_read_data(view));
    ASSERT_TRUE(view);
    EXPECT_EQ(view, "hello world");

    SIHD_LOG(info, "Second read");
    EXPECT_TRUE(read_next_ok(reader));
    EXPECT_TRUE(reader.get_read_data(view));
    ASSERT_TRUE(view);
    EXPECT_EQ(view, "how are you");
    EXPECT_FALSE(read_next_ok(reader));
    EXPECT_FALSE(read_next_ok(reader));
    EXPECT_TRUE(reader.close());

    SIHD_LOG(info, "Writing test file with linefeed at the end: {}", path);
    EXPECT_TRUE(fs::write(path, "hello world\nhow are you\n").has_value());

    EXPECT_TRUE(reader.open(path).has_value());

    SIHD_LOG(info, "First read");
    EXPECT_TRUE(read_next_ok(reader));
    EXPECT_TRUE(reader.get_read_data(view));
    ASSERT_TRUE(view);
    EXPECT_EQ(view, "hello world");

    SIHD_LOG(info, "Second read");
    EXPECT_TRUE(read_next_ok(reader));
    EXPECT_TRUE(reader.get_read_data(view));
    ASSERT_TRUE(view);
    EXPECT_EQ(view, "how are you");
    EXPECT_FALSE(read_next_ok(reader));
    EXPECT_FALSE(read_next_ok(reader));
    EXPECT_TRUE(reader.close());
}

TEST_F(TestLineReader, test_linereader_multiple_feeds)
{
    LineReader reader;
    ArrCharView view;

    std::string path = fs::combine(_tmp_dir.path(), "multiple_feeds.txt");

    SIHD_LOG(info, "Writing test file: {}", path);
    EXPECT_TRUE(fs::write(path, "hello world\n!\n\n\n").has_value());

    EXPECT_TRUE(reader.open(path).has_value());

    SIHD_LOG(info, "First read");
    EXPECT_TRUE(read_next_ok(reader));
    EXPECT_TRUE(reader.get_read_data(view));
    ASSERT_TRUE(view);
    EXPECT_EQ(view, "hello world");

    SIHD_LOG(info, "Second read");
    EXPECT_TRUE(read_next_ok(reader));
    EXPECT_TRUE(reader.get_read_data(view));
    ASSERT_TRUE(view);
    EXPECT_EQ(view, "!");

    SIHD_LOG(info, "Third read");
    EXPECT_TRUE(read_next_ok(reader));
    EXPECT_TRUE(reader.get_read_data(view));
    ASSERT_TRUE(view);
    EXPECT_EQ(view, "");

    SIHD_LOG(info, "Fourth read");
    EXPECT_TRUE(read_next_ok(reader));
    EXPECT_TRUE(reader.get_read_data(view));
    ASSERT_TRUE(view);
    EXPECT_EQ(view, "");
    EXPECT_FALSE(read_next_ok(reader));
    EXPECT_FALSE(read_next_ok(reader));
    EXPECT_TRUE(reader.close());
}

TEST_F(TestLineReader, test_linereader_none)
{
    LineReader reader;
    std::string path = fs::combine(_tmp_dir.path(), "nothing.txt");

    SIHD_LOG(info, "Writing test file: {}", path);
    EXPECT_TRUE(fs::write(path, "").has_value());

    EXPECT_TRUE(reader.open(path).has_value());
    EXPECT_FALSE(read_next_ok(reader));
    EXPECT_TRUE(reader.close());
}

TEST_F(TestLineReader, test_linereader_low_buffer)
{
    LineReader reader;
    ArrCharView view;

    std::string path = fs::combine(_tmp_dir.path(), "buffer_test.txt");

    SIHD_LOG(info, "Writing test file: {}", path);
    EXPECT_TRUE(fs::write(path, "hello world\nhow are you\n?\n").has_value());

    EXPECT_TRUE(reader.set_read_buffsize(1));
    EXPECT_TRUE(reader.open(path).has_value());
    EXPECT_TRUE(read_next_ok(reader));
    EXPECT_TRUE(reader.get_read_data(view));
    ASSERT_TRUE(view);
    EXPECT_EQ(view, "hello world");
    EXPECT_TRUE(read_next_ok(reader));
    EXPECT_TRUE(read_next_ok(reader));
    EXPECT_FALSE(read_next_ok(reader));
    EXPECT_TRUE(reader.close());
}

TEST_F(TestLineReader, test_linereader_delimiter_in_line)
{
    LineReader reader({.delimiter_in_line = true});
    ArrCharView view;

    std::string path = fs::combine(_tmp_dir.path(), "delim_in_line.txt");
    EXPECT_TRUE(fs::write(path, "hello world\nbye\n").has_value());

    EXPECT_TRUE(reader.open(path).has_value());
    EXPECT_TRUE(read_next_ok(reader));
    EXPECT_TRUE(reader.get_read_data(view));
    EXPECT_EQ(view, "hello world\n");
    EXPECT_TRUE(read_next_ok(reader));
    EXPECT_TRUE(reader.get_read_data(view));
    EXPECT_EQ(view, "bye\n");
    EXPECT_FALSE(read_next_ok(reader));
    EXPECT_TRUE(reader.close());
}

TEST_F(TestLineReader, test_linereader_custom_delimiter)
{
    LineReader reader({.delimiter = ';'});
    ArrCharView view;

    std::string path = fs::combine(_tmp_dir.path(), "custom_delim.txt");
    EXPECT_TRUE(fs::write(path, "a;b;c"));

    EXPECT_TRUE(reader.open(path).has_value());
    EXPECT_TRUE(read_next_ok(reader));
    EXPECT_TRUE(reader.get_read_data(view));
    EXPECT_EQ(view, "a");
    EXPECT_TRUE(read_next_ok(reader));
    EXPECT_TRUE(reader.get_read_data(view));
    EXPECT_EQ(view, "b");
    EXPECT_TRUE(read_next_ok(reader));
    EXPECT_TRUE(reader.get_read_data(view));
    EXPECT_EQ(view, "c");
    EXPECT_FALSE(read_next_ok(reader));
    EXPECT_TRUE(reader.close());
}

TEST_F(TestLineReader, test_linereader_long_line)
{
    LineReader reader;
    ArrCharView view;

    const std::string long_line(2000, 'a');
    std::string path = fs::combine(_tmp_dir.path(), "long_line.txt");
    EXPECT_TRUE(fs::write(path, long_line).has_value());

    EXPECT_TRUE(reader.open(path).has_value());
    EXPECT_TRUE(read_next_ok(reader));
    EXPECT_TRUE(reader.get_read_data(view));
    EXPECT_EQ(view, long_line);
    EXPECT_FALSE(read_next_ok(reader));
    EXPECT_TRUE(reader.close());
}

TEST_F(TestLineReader, test_linereader_errors)
{
    LineReader reader;
    ArrCharView view;

    EXPECT_FALSE(reader.get_read_data(view));

    std::string missing = fs::combine(_tmp_dir.path(), "does_not_exist.txt");
    EXPECT_FALSE(reader.open(missing).has_value());

    std::string path = fs::combine(_tmp_dir.path(), "clean_eof.txt");
    EXPECT_TRUE(fs::write(path, "one line\n").has_value());
    EXPECT_TRUE(reader.open(path).has_value());
    EXPECT_TRUE(read_next_ok(reader));
    EXPECT_FALSE(reader.read_next().value());
    EXPECT_TRUE(reader.close());
}

TEST_F(TestLineReader, test_linereader_fast_read_line)
{
    const std::string path = fs::combine(_tmp_dir.path(), "fast.txt");
    ASSERT_TRUE(fs::write_binary(path, "first\nsecond").has_value());

    File file(path, "rb");
    ASSERT_TRUE(file.is_open());
    // no memory between two calls: only a one byte read buffer leaves the next line in the stream
    const LineReader::LineReaderOptions options = {.read_buffsize = 1, .delimiter_in_line = true};

    auto line = LineReader::fast_read_line(file.file(), options);
    ASSERT_TRUE(line.has_value());
    EXPECT_EQ(*line, "first\n");

    line = LineReader::fast_read_line(file.file(), options);
    ASSERT_TRUE(line.has_value());
    EXPECT_EQ(*line, "second");

    line = LineReader::fast_read_line(file.file(), options);
    ASSERT_FALSE(line.has_value());
    EXPECT_EQ(line.error().code, ErrorCode::not_found);
}

TEST_F(TestLineReader, test_linereader_lines_across_read_buffers)
{
    const std::vector<std::string> lines = {"\n",
                                            std::string(63, 'a') + "\n",
                                            std::string(64, 'b') + "\n",
                                            std::string(65, 'c') + "\r\n",
                                            std::string(1000, 'd') + "\n",
                                            "end"};
    std::string content;
    for (const std::string & line : lines)
        content += line;
    const std::string path = fs::combine(_tmp_dir.path(), "across_buffers.txt");
    ASSERT_TRUE(fs::write_binary(path, content).has_value());

    LineReader reader(path, {.read_buffsize = 64, .delimiter_in_line = true});
    ASSERT_TRUE(reader.is_open());
    ArrCharView view;
    for (const std::string & line : lines)
    {
        ASSERT_TRUE(read_next_ok(reader));
        ASSERT_TRUE(reader.get_read_data(view));
        // LineReader opens in text mode: the windows crt reads crlf as lf
        const bool crlf_read_as_lf = build::is_windows && line.ends_with("\r\n");
        EXPECT_EQ(view, crlf_read_as_lf ? line.substr(0, line.size() - 2) + "\n" : line);
    }
    EXPECT_FALSE(read_next_ok(reader));

    File fast_file(path, "rb");
    ASSERT_TRUE(fast_file.is_open());
    for (const std::string & line : lines)
    {
        const auto read = LineReader::fast_read_line(fast_file.file(), {.read_buffsize = 1, .delimiter_in_line = true});
        ASSERT_TRUE(read.has_value());
        EXPECT_EQ(*read, line);
    }

    File file(path, "rb");
    ASSERT_TRUE(file.is_open());
    char *buffer = nullptr;
    size_t size = 0;
    for (const std::string & line : lines)
    {
        const auto read = file.read_line(&buffer, &size);
        ASSERT_TRUE(read.has_value());
        EXPECT_EQ(std::string(buffer, *read), line);
    }
    free(buffer);
}

#if !defined(__SIHD_WINDOWS__)
TEST_F(TestLineReader, test_linereader_open_fd)
{
    LineReader reader;
    ArrCharView view;

    std::string path = fs::combine(_tmp_dir.path(), "open_fd.txt");
    EXPECT_TRUE(fs::write(path, "first\nsecond\n").has_value());

    int fd = ::open(path.c_str(), O_RDONLY);
    ASSERT_GE(fd, 0);

    EXPECT_TRUE(reader.open_fd(fd));
    EXPECT_TRUE(read_next_ok(reader));
    EXPECT_TRUE(reader.get_read_data(view));
    EXPECT_EQ(view, "first");
    EXPECT_TRUE(read_next_ok(reader));
    EXPECT_TRUE(reader.get_read_data(view));
    EXPECT_EQ(view, "second");
    EXPECT_FALSE(read_next_ok(reader));
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

    auto line = LineReader::fast_read_stdin();
    ASSERT_TRUE(line.has_value());
    EXPECT_EQ(line.value(), "from stdin");

    ASSERT_GE(::dup2(saved_stdin, STDIN_FILENO), 0);
    ::close(saved_stdin);
    ::close(pipefd[0]);
}
#endif

} // namespace test
