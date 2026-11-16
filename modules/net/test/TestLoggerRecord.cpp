#include <stdexcept>

#include <gtest/gtest.h>

#include <sihd/net/LoggerRecord.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/time.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::net;
using namespace sihd::util;

class TestLoggerRecord: public ::testing::Test
{
    protected:
        TestLoggerRecord() { LoggerManager::stream(); }

        virtual ~TestLoggerRecord() { LoggerManager::clear_loggers(); }

        static LogInfo make_info()
        {
            LogInfo info("sihd::test", LogLevel::warning);
            info.thread_name = "worker";
            info.timespec = {1700000000, 123456789};
            return info;
        }

        static void expect_refused(const ArrByte & bytes, ErrorCode code, std::string_view message)
        {
            const ArrByte kept = LoggerRecord(make_info(), "kept").encode();
            LoggerRecord record(kept);

            const Result<void> refused = record.decode(bytes);

            ASSERT_FALSE(refused.has_value());
            EXPECT_EQ(refused.error().code, code);
            EXPECT_EQ(refused.error().message, message);
            EXPECT_EQ(record.message(), "kept");
            EXPECT_THROW({ LoggerRecord thrown(bytes); }, std::runtime_error);
        }
};

TEST_F(TestLoggerRecord, test_loggerrecord_round_trip)
{
    const LogInfo info = make_info();
    const LoggerRecord record(info, "hello collector");
    const ArrByte bytes = record.encode();

    LoggerRecord decoded;
    ASSERT_TRUE(decoded.decode(bytes).has_value());

    EXPECT_EQ(record.encoded_size(), bytes.size());
    EXPECT_EQ(decoded.level(), LogLevel::warning);
    EXPECT_EQ(decoded.timestamp(), Timestamp(make_info().timespec));
    EXPECT_EQ(decoded.source(), "sihd::test");
    EXPECT_EQ(decoded.thread_name(), "worker");
    EXPECT_EQ(decoded.message(), "hello collector");
}

TEST_F(TestLoggerRecord, test_loggerrecord_log_info)
{
    const ArrByte bytes = LoggerRecord(make_info(), "rebuilt").encode();
    const LoggerRecord decoded(bytes);

    const LogInfo info = decoded.log_info();

    EXPECT_EQ(info.source, "sihd::test");
    EXPECT_EQ(info.level, LogLevel::warning);
    EXPECT_EQ(std::string_view(info.strlevel), "WARNING");
    EXPECT_EQ(info.thread_name, "worker");
    EXPECT_EQ(info.timespec.tv_sec, 1700000000);
    EXPECT_EQ(info.timespec.tv_nsec, 123456789);
}

TEST_F(TestLoggerRecord, test_loggerrecord_byte_layout)
{
    LogInfo info("ab", LogLevel::info);
    info.thread_name = "t";
    info.timespec = time::to_ts(0x0102030405060708);

    const ArrByte bytes = LoggerRecord(info, "hi").encode();

    const ArrByte expected = {
        1,                              // version
        6,                              // level info
        1, 2, 3,   4,   5,   6,   7, 8, // nanoseconds since epoch, big endian
        0, 2, 'a', 'b',                 // source length then source
        0, 1, 't',                      // thread name length then thread name
        0, 0, 0,   2,   'h', 'i',       // message length then message
    };
    EXPECT_EQ(bytes, expected);
}

TEST_F(TestLoggerRecord, test_loggerrecord_empty_message)
{
    const ArrByte bytes = LoggerRecord(LogInfo("", LogLevel::debug), "").encode();

    const LoggerRecord decoded(bytes);

    EXPECT_EQ(decoded.level(), LogLevel::debug);
    EXPECT_EQ(decoded.source(), "");
    EXPECT_EQ(decoded.message(), "");
}

TEST_F(TestLoggerRecord, test_loggerrecord_refuses_malformed_records)
{
    ArrByte bytes = LoggerRecord(make_info(), "hello").encode();

    expect_refused(ArrByte(), ErrorCode::invalid_argument, "empty logger record");

    ArrByte other_version = bytes;
    other_version[0] = 2;
    expect_refused(other_version, ErrorCode::not_supported, "logger record version 2");

    ArrByte truncated = bytes;
    ASSERT_TRUE(truncated.resize(truncated.size() - 1));
    expect_refused(truncated, ErrorCode::invalid_argument, "logger record truncated");

    ArrByte trailing = bytes;
    ASSERT_TRUE(trailing.push_back(0));
    expect_refused(trailing, ErrorCode::invalid_argument, "logger record followed by 1 bytes");

    ArrByte unknown_level = bytes;
    unknown_level[1] = 8;
    expect_refused(unknown_level, ErrorCode::invalid_argument, "logger record level 8");
}

} // namespace test
