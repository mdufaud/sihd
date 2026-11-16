#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <sihd/net/LoggerReaderUdp.hpp>
#include <sihd/net/LoggerRecord.hpp>
#include <sihd/net/LoggerWriterUdp.hpp>
#include <sihd/net/Socket.hpp>
#include <sihd/sys/TmpDir.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/build.hpp>

#include "net_test_helpers.hpp"

namespace test
{
SIHD_LOGGER;
using namespace sihd::net;
using namespace sihd::util;

class TestLoggerReaderUdp: public ::testing::Test
{
    protected:
        TestLoggerReaderUdp() { LoggerManager::stream(); }

        virtual ~TestLoggerReaderUdp() { LoggerManager::clear_loggers(); }
};

TEST_F(TestLoggerReaderUdp, test_loggerreaderudp_round_trip_ip)
{
    LoggerReaderScope<LoggerReaderUdp> scope;
    ASSERT_TRUE(scope.reader.set_host("127.0.0.1"));
    ASSERT_TRUE(scope.reader.set_port(0));
    ASSERT_TRUE(scope.start());
    RemoteLogCapture *capture = install_capture();
    auto *writer = install_writer<LoggerWriterUdp>("udp-writer");
    ASSERT_TRUE(writer->set_host("127.0.0.1"));
    ASSERT_TRUE(writer->set_port(scope.reader.local_port().value_or(0)));
    EXPECT_TRUE(writer->is_connected());

    LogInfo info(RemoteLogCapture::remote_source, LogLevel::notice);
    LoggerManager::log(info, "one datagram");

    const std::vector<LoggedRecord> records = capture->wait_records(1);
    ASSERT_EQ(records.size(), 1u);
    EXPECT_EQ(records[0].level, LogLevel::notice);
    EXPECT_EQ(records[0].source, "test::remote");
    EXPECT_EQ(records[0].thread_name, info.thread_name);
    EXPECT_EQ(records[0].timestamp, info.timestamp());
    EXPECT_EQ(records[0].message, "one datagram");
}

TEST_F(TestLoggerReaderUdp, test_loggerreaderudp_round_trip_unix)
{
    if constexpr (build::is_windows)
        GTEST_SKIP() << "AF_UNIX datagram is not supported by winsock";

    sihd::sys::TmpDir tmp_dir;
    const std::string path = sihd::sys::fs::combine({tmp_dir.path(), "collector.sock"});
    LoggerReaderScope<LoggerReaderUdp> scope;
    ASSERT_TRUE(scope.reader.set_unix_path(path));
    ASSERT_TRUE(scope.start());
    RemoteLogCapture *capture = install_capture();
    auto *writer = install_writer<LoggerWriterUdp>("udp-writer");
    ASSERT_TRUE(writer->set_unix_path(path));

    LoggerManager::log(RemoteLogCapture::remote_source, LogLevel::debug, "over unix");
    LoggerManager::log(RemoteLogCapture::remote_source, LogLevel::debug, "in order");

    EXPECT_EQ(capture->wait_messages(2), (std::vector<std::string> {"over unix", "in order"}));
}

TEST_F(TestLoggerReaderUdp, test_loggerreaderudp_skips_a_malformed_datagram)
{
    LoggerReaderScope<LoggerReaderUdp> scope;
    ASSERT_TRUE(scope.reader.set_host("127.0.0.1"));
    ASSERT_TRUE(scope.start());
    RemoteLogCapture *capture = install_capture();
    Socket sender;
    ASSERT_TRUE(sender.open(AF_INET, SOCK_DGRAM, IPPROTO_UDP).has_value());
    ASSERT_TRUE(sender.connect(IpAddr::localhost(scope.reader.local_port().value_or(0))).has_value());
    const ArrByte valid = LoggerRecord(LogInfo(RemoteLogCapture::remote_source, LogLevel::info), "after the garbage")
                              .encode();

    ASSERT_TRUE(sender.send_all("garbage").has_value());
    ASSERT_TRUE(sender.send_all(valid).has_value());

    EXPECT_EQ(capture->wait_messages(1), (std::vector<std::string> {"after the garbage"}));
}

TEST_F(TestLoggerReaderUdp, test_loggerreaderudp_factory_name)
{
    expect_created_by_factory("LoggerReaderUdp");
}

TEST_F(TestLoggerReaderUdp, test_loggerreaderudp_unconfigured_does_not_start)
{
    LoggerReaderUdp unconfigured("unconfigured");

    EXPECT_FALSE(unconfigured.start());
}

} // namespace test
