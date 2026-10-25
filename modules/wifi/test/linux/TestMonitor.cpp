#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <sihd/util/Handler.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/time.hpp>
#include <sihd/wifi/Monitor.hpp>
#include <sihd/wifi/connection.hpp>
#include <sihd/wifi/scan.hpp>

#include "fake_daemon.hpp"

namespace test
{
using namespace sihd::util;
namespace wifi = sihd::wifi;
using Event = wifi::Monitor::Event;

class TestMonitor: public ::testing::Test
{
    protected:
        TestMonitor() { LoggerManager::stream(); }

        virtual ~TestMonitor() { LoggerManager::clear_loggers(); }

        virtual void SetUp()
        {
            if (!FakeDaemon::available())
                GTEST_SKIP() << "no dbus-daemon";
        }

        static std::vector<std::string> described(const std::vector<Event> & events)
        {
            std::vector<std::string> descriptions;
            for (const Event & event : events)
                descriptions.emplace_back(event.type == Event::Type::scan_done ? "scan_done"
                                                                               : wifi::state_str(event.state));
            return descriptions;
        }

        FakeDaemon _daemon;
};

TEST_F(TestMonitor, test_monitor_refuses_an_unmanaged_interface)
{
    ASSERT_TRUE(_daemon.start({}));
    _daemon.serve();
    wifi::Monitor monitor;

    auto opened = monitor.open("wlan-test");
    auto selected = wifi::backend("wlan-test");

    ASSERT_FALSE(opened.has_value());
    EXPECT_EQ(opened.error().code, ErrorCode::not_found);
    EXPECT_EQ(opened.error().message, "no wifi daemon manages wlan-test");
    ASSERT_FALSE(selected.has_value());
    EXPECT_EQ(selected.error().code, ErrorCode::not_found);
    EXPECT_FALSE(monitor.run());
}

TEST_F(TestMonitor, test_network_manager_events)
{
    ASSERT_TRUE(_daemon.start({nm_service}));
    serve_network_manager(_daemon, true);
    _daemon.on(nm_wireless_interface, "RequestScan", [this](const dbus::Message & call) {
        _daemon.reply(call, {});
        _daemon.emit(nm_device, nm_device_interface, "StateChanged", {u32(40), u32(30), u32(0)});
        _daemon.emit(nm_device, nm_device_interface, "StateChanged", {u32(50), u32(40), u32(0)});
        _daemon.emit(nm_device, nm_device_interface, "StateChanged", {u32(100), u32(50), u32(0)});
        _daemon.emit_properties_changed(nm_device, nm_wireless_interface, {{"LastScan", u32(1)}});
    });
    _daemon.serve();
    wifi::Monitor monitor(Duration(time::sec(1)));
    ASSERT_TRUE(monitor.open("wlan-test").has_value());

    int notifications = 0;
    Handler<wifi::Monitor *> counter([&notifications](wifi::Monitor *) { ++notifications; });
    monitor.add_observer(&counter);

    ASSERT_TRUE(wifi::scan("wlan-test", fake_timeout).has_value());
    ASSERT_TRUE(monitor.run());
    auto reopened = monitor.open("wlan-test");

    EXPECT_EQ(described(monitor.events()), (std::vector<std::string> {"connecting", "connected", "scan_done"}));
    EXPECT_EQ(notifications, 1);
    ASSERT_FALSE(reopened.has_value());
    EXPECT_EQ(reopened.error().code, ErrorCode::already_exists);
    EXPECT_EQ(reopened.error().message, "the monitor is already open");
}

TEST_F(TestMonitor, test_iwd_events)
{
    ASSERT_TRUE(_daemon.start({iwd_service}));
    serve_iwd_station(_daemon, "disconnected");
    _daemon.on(iwd_station_interface, "Scan", [this](const dbus::Message & call) {
        _daemon.reply(call, {});
        _daemon.emit_properties_changed(iwd_station, iwd_station_interface, {{"Scanning", boolean(true)}});
        _daemon.emit_properties_changed(iwd_station, iwd_station_interface, {{"State", text("connecting")}});
        _daemon.emit_properties_changed(iwd_station,
                                        iwd_station_interface,
                                        {{"State", text("connected")}, {"Scanning", boolean(false)}});
    });
    _daemon.serve();
    wifi::Monitor monitor(Duration(time::sec(1)));
    ASSERT_TRUE(monitor.open("wlan-test").has_value());

    ASSERT_TRUE(wifi::scan("wlan-test", fake_timeout).has_value());
    ASSERT_TRUE(monitor.run());

    EXPECT_EQ(described(monitor.events()), (std::vector<std::string> {"connecting", "connected", "scan_done"}));
}

TEST_F(TestMonitor, test_wpa_supplicant_events_with_a_blocking_run)
{
    ASSERT_TRUE(_daemon.start({wpa_service}));
    serve_wpa_supplicant(_daemon);
    _daemon.on(wpa_interface_interface, "Scan", [this](const dbus::Message & call) {
        _daemon.reply(call, {});
        _daemon.emit_properties_changed(wpa_interface, wpa_interface_interface, {{"State", text("scanning")}});
        _daemon.emit(wpa_interface, wpa_interface_interface, "ScanDone", {boolean(true)});
    });
    _daemon.serve();
    wifi::Monitor monitor(Duration(-1));
    ASSERT_TRUE(monitor.open("wlan-test").has_value());

    ASSERT_TRUE(wifi::scan("wlan-test", fake_timeout).has_value());
    ASSERT_TRUE(monitor.run());

    EXPECT_EQ(described(monitor.events()), (std::vector<std::string> {"disconnected", "scan_done"}));
}

} // namespace test
