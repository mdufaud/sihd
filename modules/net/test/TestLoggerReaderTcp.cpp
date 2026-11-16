#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <sihd/net/LoggerReaderTcp.hpp>
#include <sihd/net/LoggerRecord.hpp>
#include <sihd/net/LoggerWriterTcp.hpp>
#include <sihd/net/Socket.hpp>
#include <sihd/sys/TmpDir.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/endian.hpp>

#include "net_test_helpers.hpp"

namespace test
{
SIHD_LOGGER;
using namespace sihd::net;
using namespace sihd::util;

class TestLoggerReaderTcp: public ::testing::Test
{
    protected:
        TestLoggerReaderTcp() { LoggerManager::stream(); }

        virtual ~TestLoggerReaderTcp() { LoggerManager::clear_loggers(); }

        static std::string frame(std::string_view message)
        {
            const ArrByte record = LoggerRecord(LogInfo(RemoteLogCapture::remote_source, LogLevel::info), message)
                                       .encode();
            std::string bytes(sizeof(uint32_t), '\0');
            endian::write_be(static_cast<uint32_t>(record.size()),
                             reinterpret_cast<uint8_t *>(bytes.data()),
                             sizeof(uint32_t));
            bytes.append(reinterpret_cast<const char *>(record.buf()), record.size());
            return bytes;
        }

        static IpAddr reader_address(const LoggerReaderTcp & reader)
        {
            return IpAddr::localhost(reader.local_port().value_or(0));
        }
};

TEST_F(TestLoggerReaderTcp, test_loggerreadertcp_round_trip_ip)
{
    LoggerReaderScope<LoggerReaderTcp> scope;
    ASSERT_TRUE(scope.reader.set_host("127.0.0.1"));
    ASSERT_TRUE(scope.reader.set_port(0));
    ASSERT_TRUE(scope.start());
    RemoteLogCapture *capture = install_capture();
    auto *writer = install_writer<LoggerWriterTcp>("tcp-writer");
    ASSERT_TRUE(writer->set_host("127.0.0.1"));
    ASSERT_TRUE(writer->set_port(scope.reader.local_port().value_or(0)));
    EXPECT_TRUE(writer->is_connected());

    LogInfo info(RemoteLogCapture::remote_source, LogLevel::warning);
    LoggerManager::log(info, "hello collector");

    const std::vector<LoggedRecord> records = capture->wait_records(1);
    ASSERT_EQ(records.size(), 1u);
    EXPECT_EQ(records[0].level, LogLevel::warning);
    EXPECT_EQ(records[0].source, "test::remote");
    EXPECT_EQ(records[0].thread_name, info.thread_name);
    EXPECT_EQ(records[0].timestamp, info.timestamp());
    EXPECT_EQ(records[0].message, "hello collector");
}

TEST_F(TestLoggerReaderTcp, test_loggerreadertcp_round_trip_unix)
{
    if (sihd::sys::os::is_run_by_wine())
        GTEST_SKIP() << "AF_UNIX stream is not supported by wine";

    sihd::sys::TmpDir tmp_dir;
    const std::string path = sihd::sys::fs::combine({tmp_dir.path(), "collector.sock"});
    LoggerReaderScope<LoggerReaderTcp> scope;
    ASSERT_TRUE(scope.reader.set_unix_path(path));
    ASSERT_TRUE(scope.start());
    RemoteLogCapture *capture = install_capture();
    auto *writer = install_writer<LoggerWriterTcp>("tcp-writer");
    ASSERT_TRUE(writer->set_unix_path(path));

    LoggerManager::log(RemoteLogCapture::remote_source, LogLevel::error, "over unix");

    const std::vector<LoggedRecord> records = capture->wait_records(1);
    ASSERT_EQ(records.size(), 1u);
    EXPECT_EQ(records[0].level, LogLevel::error);
    EXPECT_EQ(records[0].message, "over unix");
}

TEST_F(TestLoggerReaderTcp, test_loggerreadertcp_burst_keeps_every_record)
{
    LoggerReaderScope<LoggerReaderTcp> scope;
    ASSERT_TRUE(scope.reader.set_host("127.0.0.1"));
    ASSERT_TRUE(scope.start());
    RemoteLogCapture *capture = install_capture();
    auto *writer = install_writer<LoggerWriterTcp>("tcp-writer");
    ASSERT_TRUE(writer->set_host("127.0.0.1"));
    ASSERT_TRUE(writer->set_port(scope.reader.local_port().value_or(0)));

    std::vector<std::string> expected;
    for (int i = 0; i < 100; ++i)
    {
        expected.push_back(fmt::format("message {}", i));
        LoggerManager::log(RemoteLogCapture::remote_source, LogLevel::info, expected.back());
    }

    EXPECT_EQ(capture->wait_messages(expected.size()), expected);
}

TEST_F(TestLoggerReaderTcp, test_loggerreadertcp_split_and_coalesced_frames)
{
    LoggerReaderScope<LoggerReaderTcp> scope;
    ASSERT_TRUE(scope.reader.set_host("127.0.0.1"));
    ASSERT_TRUE(scope.start());
    RemoteLogCapture *capture = install_capture();

    Socket client;
    ASSERT_TRUE(client.open(AF_INET, SOCK_STREAM, IPPROTO_TCP).has_value());
    ASSERT_TRUE(client.connect(reader_address(scope.reader)).has_value());
    const std::string second = frame("second");
    const std::string first_send = frame("first") + second.substr(0, 3);
    const std::string second_send = second.substr(3);

    ASSERT_TRUE(client.send_all(first_send).has_value());
    ASSERT_TRUE(client.send_all(second_send).has_value());

    EXPECT_EQ(capture->wait_messages(2), (std::vector<std::string> {"first", "second"}));
}

TEST_F(TestLoggerReaderTcp, test_loggerreadertcp_drops_an_oversized_frame)
{
    LoggerReaderScope<LoggerReaderTcp> scope;
    ASSERT_TRUE(scope.reader.set_host("127.0.0.1"));
    ASSERT_TRUE(scope.start());
    RemoteLogCapture *capture = install_capture();

    Socket oversized;
    ASSERT_TRUE(oversized.open(AF_INET, SOCK_STREAM, IPPROTO_TCP).has_value());
    ASSERT_TRUE(oversized.connect(reader_address(scope.reader)).has_value());
    ASSERT_TRUE(oversized.set_recv_timeout(5000).has_value());
    // record size 0xFFFFFFFF, past max_record_size
    ASSERT_TRUE(oversized.send_all(std::string(4, '\xff')).has_value());
    char byte = 0;
    EXPECT_EQ(oversized.receive(&byte, 1).value_or(1), 0u);

    Socket valid;
    ASSERT_TRUE(valid.open(AF_INET, SOCK_STREAM, IPPROTO_TCP).has_value());
    ASSERT_TRUE(valid.connect(reader_address(scope.reader)).has_value());
    ASSERT_TRUE(valid.send_all(frame("still served")).has_value());

    EXPECT_EQ(capture->wait_messages(1), (std::vector<std::string> {"still served"}));
}

TEST_F(TestLoggerReaderTcp, test_loggerreadertcp_unconfigured_does_not_start)
{
    LoggerReaderTcp unconfigured("unconfigured");

    EXPECT_FALSE(unconfigured.start());
}

TEST_F(TestLoggerReaderTcp, test_loggerreadertcp_factory_name)
{
    expect_created_by_factory("LoggerReaderTcp");
}

TEST_F(TestLoggerReaderTcp, test_loggerreadertcp_conf_keys)
{
    LoggerReaderTcp reader("reader");

    EXPECT_TRUE(reader.set_conf_str("host", "127.0.0.1").has_value());
    EXPECT_TRUE(reader.set_conf("port", 0).has_value());
    EXPECT_FALSE(reader.set_conf_str("unix_path", "collector.sock").has_value());
}

} // namespace test
