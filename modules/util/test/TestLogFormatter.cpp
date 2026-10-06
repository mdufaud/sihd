#include <cstdio>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <sihd/util/LogFormatter.hpp>
#include <sihd/util/LoggerStream.hpp>
#include <sihd/util/Timestamp.hpp>
#include <sihd/util/str.hpp>
#include <sihd/util/thread.hpp>

namespace test
{
using namespace sihd::util;

class TestLogFormatter: public ::testing::Test
{
    protected:
        TestLogFormatter() = default;

        virtual ~TestLogFormatter() = default;

        virtual void SetUp() override
        {
            _old_thread_name = thread::name();
            (void)thread::set_name("main");
        }

        virtual void TearDown() override { (void)thread::set_name(_old_thread_name); }

        std::string _old_thread_name;
};

TEST_F(TestLogFormatter, test_logformatter_default_matches_loginfo_format)
{
    const std::string source = "test::format";
    const LogInfo info(source, LogLevel::warning);
    const LogFormatter formatter;

    EXPECT_EQ(formatter.format(info, "the message"), info.format("the message"));
    EXPECT_EQ(formatter.pattern(), LogFormatter::default_pattern);

    LogFormatter with_tid;
    ASSERT_TRUE(with_tid.set_pattern("{epoch}\t{tid}\t[{thread}]\t{level:<9} {source}\t{msg}\n").has_value());
    EXPECT_EQ(with_tid.format(info, "the message"), info.format("the message", true));
}

TEST_F(TestLogFormatter, test_logformatter_placeholders)
{
    const std::string source = "test::format";
    const LogInfo info(source, LogLevel::info);
    LogFormatter formatter;

    ASSERT_TRUE(formatter.set_pattern("{source}|{level}|{thread}|{tid}|{epoch}|{msg}").has_value());
    const std::string line = formatter.format(info, "hello");
    const std::vector<std::string> parts = str::split(line, "|");
    ASSERT_EQ(parts.size(), 6u);
    EXPECT_EQ(parts[0], "test::format");
    EXPECT_EQ(parts[1], "INFO");
    EXPECT_EQ(parts[2], "main");
    EXPECT_EQ(parts[3], info.thread_id_str);
    EXPECT_TRUE(parts[4].starts_with(std::to_string(info.timespec.tv_sec) + "."));
    EXPECT_EQ(parts[5], "hello");
}

TEST_F(TestLogFormatter, test_logformatter_time)
{
    const std::string source = "test::format";
    const LogInfo info(source, LogLevel::info);
    LogFormatter formatter;

    ASSERT_TRUE(formatter.set_pattern("{time:%H:%M:%S}|{utc:%F}|{time}").has_value());
    const std::string line = formatter.format(info, "hello");
    const std::vector<std::string> parts = str::split(line, "|");
    ASSERT_EQ(parts.size(), 3u);
    EXPECT_EQ(parts[0], info.timestamp().local_format("%H:%M:%S"));
    EXPECT_EQ(parts[1], info.timestamp().format("%F"));
    EXPECT_EQ(parts[2], info.timestamp().local_format(Timestamp::default_format));
}

TEST_F(TestLogFormatter, test_logformatter_width)
{
    const std::string source = "test::format";
    const LogInfo info(source, LogLevel::info);
    LogFormatter formatter;

    ASSERT_TRUE(formatter.set_pattern("[{level:>9}][{level:<9}] [{level:3}] [{level:<^9}]").has_value());
    const std::string line = formatter.format(info, "hello");
    EXPECT_EQ(line, "[     INFO][INFO     ] [INFO] [<<INFO<<<]");
}

TEST_F(TestLogFormatter, test_logformatter_brace_escape)
{
    const std::string source = "test::format";
    const LogInfo info(source, LogLevel::info);
    LogFormatter formatter;

    ASSERT_TRUE(formatter.set_pattern("100{{ of {msg}}}").has_value());
    EXPECT_EQ(formatter.format(info, "logs"), "100{ of logs}");
}

TEST_F(TestLogFormatter, test_logformatter_refused_patterns)
{
    const std::string source = "test::format";
    const LogInfo info(source, LogLevel::info);
    LogFormatter formatter;

    ASSERT_TRUE(formatter.set_pattern("{msg} ok").has_value());
    const std::string before = formatter.format(info, "hello");

    const std::vector<std::string> bad_patterns = {
        "{unknown}",
        "{time:%H",
        "{-level}",
        "{msg:%Y}",
        "{time:5}",
        "{",
    };
    for (const std::string & pattern : bad_patterns)
    {
        const std::expected<void, Error> res = formatter.set_pattern(pattern);
        ASSERT_FALSE(res.has_value());
        EXPECT_NE(res.error().message.find("at "), std::string::npos) << pattern;
    }
    EXPECT_EQ(formatter.pattern(), "{msg} ok");
    EXPECT_EQ(formatter.format(info, "hello"), before);
}

TEST_F(TestLogFormatter, test_logformatter_logger_stream)
{
    const std::string source = "test::format";
    const LogInfo info(source, LogLevel::info);
    FILE *out = tmpfile();
    ASSERT_NE(out, nullptr);

    LoggerStream stream(out, "hello {msg}\n");
    stream.log(info, "world");

    std::fflush(out);
    std::rewind(out);
    std::string read(64, '\0');
    const size_t size = std::fread(read.data(), sizeof(char), read.size() - 1, out);
    read.resize(size);
    std::fclose(out);
    EXPECT_EQ(read, "hello world\n");
}

} // namespace test
