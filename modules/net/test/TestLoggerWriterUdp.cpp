#include <string>

#include <gtest/gtest.h>

#include <sihd/net/LoggerReaderUdp.hpp>
#include <sihd/net/LoggerWriterUdp.hpp>
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

class TestLoggerWriterUdp: public ::testing::Test
{
    protected:
        TestLoggerWriterUdp() { LoggerManager::stream(); }

        virtual ~TestLoggerWriterUdp() { LoggerManager::clear_loggers(); }
};

TEST_F(TestLoggerWriterUdp, test_loggerwriterudp_factory_name)
{
    expect_created_by_factory("LoggerWriterUdp");
}

TEST_F(TestLoggerWriterUdp, test_loggerwriterudp_reconnects_after_a_peer_restart)
{
    if constexpr (build::is_windows)
        GTEST_SKIP() << "AF_UNIX datagram is not supported by winsock";

    sihd::sys::TmpDir tmp_dir;

    expect_writer_reconnects<LoggerWriterUdp, LoggerReaderUdp>(
        sihd::sys::fs::combine({tmp_dir.path(), "collector.sock"}),
        install_capture());
}

} // namespace test
