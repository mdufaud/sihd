#include <chrono>
#include <string>

#include <gtest/gtest.h>

#include <sihd/net/LoggerReaderTcp.hpp>
#include <sihd/net/LoggerWriterTcp.hpp>
#include <sihd/net/Socket.hpp>
#include <sihd/sys/TmpDir.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/util/Logger.hpp>

#include "net_test_helpers.hpp"

namespace test
{
SIHD_LOGGER;
using namespace sihd::net;
using namespace sihd::util;

class TestLoggerWriterTcp: public ::testing::Test
{
    protected:
        TestLoggerWriterTcp() { LoggerManager::stream(); }

        virtual ~TestLoggerWriterTcp() { LoggerManager::clear_loggers(); }

        const LogInfo info {RemoteLogCapture::remote_source, LogLevel::info};
};

TEST_F(TestLoggerWriterTcp, test_loggerwritertcp_unconfigured_warns_once)
{
    LoggerWriterTcp writer("tcp-writer");

    testing::internal::CaptureStderr();
    writer.log(info, "dropped");
    writer.log(info, "dropped again");
    const std::string warned = testing::internal::GetCapturedStderr();

    EXPECT_EQ(warned, "tcp-writer: no host/port nor unix_path configured, dropping messages\n");
    EXPECT_FALSE(writer.is_connected());
}

TEST_F(TestLoggerWriterTcp, test_loggerwritertcp_factory_name)
{
    expect_created_by_factory("LoggerWriterTcp");
}

TEST_F(TestLoggerWriterTcp, test_loggerwritertcp_refuses_invalid_conf)
{
    LoggerWriterTcp writer("tcp-writer");

    EXPECT_FALSE(writer.set_host("not an ip"));
    EXPECT_FALSE(writer.set_reconnect_interval(-1));
    EXPECT_FALSE(writer.set_connect_timeout(0));
    EXPECT_FALSE(writer.set_send_timeout(0));
    EXPECT_FALSE(writer.set_conf("port", 70000).has_value());
    EXPECT_TRUE(writer.set_conf("connect_timeout", 200).has_value());
    EXPECT_TRUE(writer.set_conf("send_timeout", 200).has_value());
    EXPECT_TRUE(writer.set_conf("reconnect_interval", 200).has_value());
}

TEST_F(TestLoggerWriterTcp, test_loggerwritertcp_unreachable_peer_is_not_fatal)
{
    sihd::sys::TmpDir tmp_dir;
    LoggerWriterTcp writer("tcp-writer");

    ASSERT_TRUE(writer.set_unix_path(sihd::sys::fs::combine({tmp_dir.path(), "nobody.sock"})));
    EXPECT_FALSE(writer.is_connected());

    testing::internal::CaptureStderr();
    writer.log(info, "dropped while the backoff runs");
    EXPECT_EQ(testing::internal::GetCapturedStderr(), "");
    EXPECT_FALSE(writer.is_connected());
}

TEST_F(TestLoggerWriterTcp, test_loggerwritertcp_stalled_collector_bounds_log)
{
    Socket listener;
    ASSERT_TRUE(listener.open(AF_INET, SOCK_STREAM, IPPROTO_TCP).has_value());
    ASSERT_TRUE(listener.bind(IpAddr::localhost(0)).has_value());
    ASSERT_TRUE(listener.listen(1).has_value());
    LoggerWriterTcp writer("tcp-writer");
    ASSERT_TRUE(writer.set_send_timeout(50));
    ASSERT_TRUE(writer.set_reconnect_interval(60000));
    ASSERT_TRUE(writer.set_host("127.0.0.1"));
    ASSERT_TRUE(writer.set_port(listener.local_port().value_or(0)));
    Socket accepted(listener.accept().value_or(-1));
    ASSERT_TRUE(accepted.is_open());

    const std::string message(1024 * 1024, 'x');
    testing::internal::CaptureStderr();
    const auto start = std::chrono::steady_clock::now();
    for (int i = 0; writer.is_connected() && i < 256; ++i)
        writer.log(info, message);
    const auto elapsed = std::chrono::steady_clock::now() - start;
    const std::string warned = testing::internal::GetCapturedStderr();

    EXPECT_FALSE(writer.is_connected());
    EXPECT_LT(elapsed, std::chrono::seconds(5));
    EXPECT_TRUE(warned.starts_with("tcp-writer: cannot reach 127.0.0.1:")) << warned;
}

TEST_F(TestLoggerWriterTcp, test_loggerwritertcp_reconnects_after_a_peer_restart)
{
    if (sihd::sys::os::is_run_by_wine())
        GTEST_SKIP() << "AF_UNIX stream is not supported by wine";

    sihd::sys::TmpDir tmp_dir;

    expect_writer_reconnects<LoggerWriterTcp, LoggerReaderTcp>(
        sihd::sys::fs::combine({tmp_dir.path(), "collector.sock"}),
        install_capture());
}

} // namespace test
