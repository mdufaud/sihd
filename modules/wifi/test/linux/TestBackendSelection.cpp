#include <string>

#include <gtest/gtest.h>

#include <sihd/util/Logger.hpp>
#include <sihd/wifi/connection.hpp>

#include "fake_daemon.hpp"

namespace test
{
using namespace sihd::util;
namespace wifi = sihd::wifi;

class TestBackendSelection: public ::testing::Test
{
    protected:
        TestBackendSelection() { LoggerManager::stream(); }

        virtual ~TestBackendSelection() { LoggerManager::clear_loggers(); }

        virtual void SetUp()
        {
            if (!FakeDaemon::available())
                GTEST_SKIP() << "no dbus-daemon";
        }

        FakeDaemon _daemon;
};

TEST_F(TestBackendSelection, test_network_manager_comes_first)
{
    ASSERT_TRUE(_daemon.start({nm_service, wpa_service}));
    serve_network_manager(_daemon, true);
    serve_wpa_supplicant(_daemon);
    _daemon.serve();

    auto selected = wifi::backend("wlan-test");

    _daemon.stop();
    ASSERT_TRUE(selected.has_value()) << selected.error().message;
    EXPECT_EQ(*selected, wifi::Backend::network_manager);
    EXPECT_FALSE(_daemon.called(wpa_service, "GetInterface"));
}

TEST_F(TestBackendSelection, test_unmanaged_device_falls_to_wpa_supplicant)
{
    ASSERT_TRUE(_daemon.start({nm_service, wpa_service}));
    serve_network_manager(_daemon, false);
    serve_wpa_supplicant(_daemon);
    _daemon.serve();

    auto selected = wifi::backend("wlan-test");

    ASSERT_TRUE(selected.has_value()) << selected.error().message;
    EXPECT_EQ(*selected, wifi::Backend::wpa_supplicant);
}

TEST_F(TestBackendSelection, test_iwd_before_wpa_supplicant)
{
    ASSERT_TRUE(_daemon.start({iwd_service, wpa_service}));
    serve_iwd_station(_daemon, "connected");
    serve_wpa_supplicant(_daemon);
    _daemon.serve();

    auto selected = wifi::backend("wlan-test");

    _daemon.stop();
    ASSERT_TRUE(selected.has_value()) << selected.error().message;
    EXPECT_EQ(*selected, wifi::Backend::iwd);
    EXPECT_FALSE(_daemon.called(wpa_service, "GetInterface"));
}

TEST_F(TestBackendSelection, test_no_daemon_is_never_called)
{
    ASSERT_TRUE(_daemon.start({}));
    serve_network_manager(_daemon, true);
    serve_iwd_station(_daemon, "connected");
    serve_wpa_supplicant(_daemon);
    _daemon.serve();

    auto selected = wifi::backend("wlan-test");

    _daemon.stop();
    ASSERT_FALSE(selected.has_value());
    EXPECT_EQ(selected.error().code, ErrorCode::not_found);
    EXPECT_EQ(selected.error().message, "no wifi daemon manages wlan-test");
    EXPECT_TRUE(_daemon.calls().empty());
}

} // namespace test
