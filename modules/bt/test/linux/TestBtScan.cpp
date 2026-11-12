#include <cstdlib>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include <sihd/bt/Scanner.hpp>
#include <sihd/sys/env.hpp>
#include <sihd/util/Handler.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Stopwatch.hpp>
#include <sihd/util/Synchronizer.hpp>
#include <sihd/util/time.hpp>

#include "fake_bluez.hpp"

namespace test
{
using namespace sihd::bt;
using namespace sihd::util;
using namespace sihd::sys;

inline constexpr char other_address[] = "11:22:33:44:55:66";
inline constexpr char other_path[] = "/org/bluez/hci0/dev_11_22_33_44_55_66";

// its first scan is stopped once its loop ended, before start() returns: where a late stop() lands
class LateStopScanner: public Scanner
{
    public:
        LateStopScanner(const std::string & name): Scanner(name) { _stop_reached.init_sync(2); }

        std::jthread stopper;

    protected:
        bool on_start() override
        {
            const bool scanned = Scanner::on_start();
            if (!stopper.joinable())
            {
                stopper = std::jthread([this] { this->stop(); });
                _stop_reached.sync(Duration(time::sec(5)));
            }
            return scanned;
        }

        bool on_stop() override
        {
            const bool stopped = Scanner::on_stop();
            _stop_reached.sync();
            return stopped;
        }

    private:
        Synchronizer _stop_reached;
};

class TestBtScan: public ::testing::Test
{
    protected:
        TestBtScan() { LoggerManager::stream(); }

        virtual ~TestBtScan() { LoggerManager::clear_loggers(); }

        virtual void SetUp()
        {
            if (!FakeDaemon::available())
                GTEST_SKIP() << "no dbus-daemon";
            ASSERT_TRUE(_daemon.start({bluez_service}));
            serve_adapter(_daemon, adapter_path, true);
            serve_device(_daemon);
            answer(_daemon, adapter_interface, {"SetDiscoveryFilter", "StartDiscovery", "StopDiscovery"});
        }

        // read once _daemon stopped
        void record_filter()
        {
            _daemon.on(adapter_interface, "SetDiscoveryFilter", [this](const dbus::Message & call) {
                auto parsed = dbus::Iter(call).for_each_entry<std::string>(
                    [this](const std::string & key, dbus::Iter & value) -> Result<void> {
                        if (key == "Transport")
                            _transport = value.get<std::string>().value_or("");
                        else if (key == "UUIDs")
                            _uuids = value.get_array<std::string>().value_or(std::vector<std::string> {});
                        else if (key == "RSSI")
                            _rssi = value.get<int16_t>().value_or(0);
                        else if (key == "DuplicateData")
                            _duplicates = value.get<bool>().value_or(false);
                        return {};
                    });

                EXPECT_TRUE(parsed.has_value()) << parsed.error().message;
                _daemon.reply(call, {});
            });
        }

        FakeDaemon _daemon;
        std::string _transport;
        std::vector<std::string> _uuids;
        int16_t _rssi = 0;
        std::optional<bool> _duplicates;
};

TEST_F(TestBtScan, test_no_system_bus)
{
    // libdbus reads the bus address once per process: only a fresh process sees the unreachable one
    GTEST_FLAG_SET(death_test_style, "threadsafe");

    EXPECT_EXIT(
        {
            (void)env::set("DBUS_SYSTEM_BUS_ADDRESS", "unix:path=/nonexistent/sihd_bt_test_socket");
            Scanner scanner("scanner");
            const bool unreachable = !bluetooth_enabled().has_value() && !list_adapters().has_value()
                                     && !scanner.scan_for(Duration(time::milli(100))) && scanner.results().empty();
            std::exit(unreachable ? EXIT_SUCCESS : EXIT_FAILURE);
        },
        testing::ExitedWithCode(EXIT_SUCCESS),
        "");
}

TEST_F(TestBtScan, test_scan_reports_seen_devices)
{
    // known from an earlier discovery and silent since: bluez gives it no RSSI
    for (const auto & [name, value] : device_properties(other_address, "silent", 0))
    {
        if (name != "RSSI")
            _daemon.set_property(other_path, device_interface, name, value);
    }
    _daemon.serve();
    Scanner scanner("scanner");

    const bool scanned = scanner.scan_for(Duration(time::milli(50)));
    _daemon.stop();

    EXPECT_TRUE(scanned);
    ASSERT_EQ(scanner.results().size(), 1u);
    const ScanResult & result = scanner.results().at(device_address);
    EXPECT_EQ(result.identifier, "thermometer");
    EXPECT_EQ(result.address, device_address);
    EXPECT_EQ(result.address_type, AddressType::random);
    EXPECT_EQ(result.rssi, -60);
    EXPECT_EQ(result.uuids, std::vector<std::string> {battery_uuid});
    EXPECT_EQ(result.appearance, 0x0300);
    EXPECT_TRUE(_daemon.called(adapter_interface, "StartDiscovery"));
    EXPECT_TRUE(_daemon.called(adapter_interface, "StopDiscovery"));
}

TEST_F(TestBtScan, test_scan_follows_signals)
{
    _daemon.on(adapter_interface, "StartDiscovery", [this](const dbus::Message & call) {
        _daemon.reply(call, {});
        _daemon.emit(
            "/",
            object_manager_interface,
            "InterfacesAdded",
            {object_path(other_path), interfaces(device_interface, device_properties(other_address, "newcomer", -70))});
        _daemon.emit_properties_changed(other_path, device_interface, {{"RSSI", i16(-40)}});
    });

    _daemon.serve();
    Scanner scanner("scanner");
    Synchronizer updated;
    updated.init_sync(2);
    std::vector<int16_t> rssi_seen;
    std::vector<Scanner::Event> events_seen;

    Handler<Scanner *> observer([&updated, &rssi_seen, &events_seen](Scanner *scanner) {
        const std::optional<ScanResult> last = scanner->last_result();
        if (last->address != other_address)
            return;
        rssi_seen.push_back(last->rssi);
        events_seen.push_back(scanner->last_event());
        if (last->rssi == -40)
            updated.sync(Duration(time::sec(5)));
    });
    scanner.add_observer(&observer);

    bool started = false;
    std::jthread scanning([&scanner, &started] { started = scanner.start(); });
    ASSERT_TRUE(scanner.wait_ready(Duration(time::sec(5))));
    const bool synced = updated.sync(Duration(time::sec(5)));
    scanner.stop();
    scanning.join();

    EXPECT_TRUE(synced);
    EXPECT_TRUE(started);
    EXPECT_EQ(rssi_seen, (std::vector<int16_t> {-70, -40}));
    EXPECT_EQ(events_seen, (std::vector<Scanner::Event> {Scanner::Event::found, Scanner::Event::updated}));
    EXPECT_EQ(scanner.results().at(other_address).identifier, "newcomer");
}

TEST_F(TestBtScan, test_lost_device)
{
    this->record_filter();
    _daemon.on(adapter_interface, "StartDiscovery", [this](const dbus::Message & call) {
        _daemon.reply(call, {});
        _daemon.emit(
            "/",
            object_manager_interface,
            "InterfacesAdded",
            {object_path(other_path), interfaces(device_interface, device_properties(other_address, "newcomer", -70))});
    });

    _daemon.serve();
    Scanner scanner("scanner");
    ASSERT_TRUE(scanner.set_lost_timeout(Duration(time::milli(100))));
    Synchronizer lost;
    lost.init_sync(2);
    std::vector<Scanner::Event> events_seen;

    Handler<Scanner *> observer([&lost, &events_seen](Scanner *scanner) {
        if (scanner->last_result()->address != other_address)
            return;
        events_seen.push_back(scanner->last_event());
        if (scanner->last_event() == Scanner::Event::lost)
            lost.sync(Duration(time::sec(5)));
    });
    scanner.add_observer(&observer);

    bool started = false;
    std::jthread scanning([&scanner, &started] { started = scanner.start(); });
    ASSERT_TRUE(scanner.wait_ready(Duration(time::sec(5))));
    const bool synced = lost.sync(Duration(time::sec(5)));
    scanner.stop();
    scanning.join();
    _daemon.stop();

    EXPECT_TRUE(synced);
    EXPECT_TRUE(started);
    EXPECT_EQ(_duplicates, std::optional<bool>(true));
    EXPECT_EQ(events_seen, (std::vector<Scanner::Event> {Scanner::Event::found, Scanner::Event::lost}));
    EXPECT_FALSE(scanner.results().contains(other_address));
}

TEST_F(TestBtScan, test_scan_filter)
{
    this->record_filter();
    _daemon.serve();
    Scanner scanner("scanner");
    ASSERT_TRUE(scanner.set_filter(ScanFilter {.transport = Transport::le, .uuids = {battery_uuid}, .min_rssi = -70}));

    const bool scanned = scanner.scan_for(Duration(time::milli(50)));
    _daemon.stop();

    EXPECT_TRUE(scanned);
    EXPECT_EQ(_transport, "le");
    EXPECT_EQ(_uuids, std::vector<std::string> {battery_uuid});
    EXPECT_EQ(_rssi, -70);
    EXPECT_EQ(_duplicates, std::nullopt);
}

TEST_F(TestBtScan, test_settings_kept_while_scanning)
{
    _daemon.serve();
    Scanner scanner("scanner");

    bool started = false;
    std::jthread scanning([&scanner, &started] { started = scanner.start(); });
    ASSERT_TRUE(scanner.wait_ready(Duration(time::sec(5))));
    const bool filtered = scanner.set_filter(ScanFilter {.transport = Transport::classic, .uuids = {}, .min_rssi = {}});
    const bool timed = scanner.set_lost_timeout(Duration(time::sec(1)));
    scanner.stop();
    scanning.join();

    EXPECT_FALSE(filtered);
    EXPECT_FALSE(timed);
    EXPECT_TRUE(started);
}

TEST_F(TestBtScan, test_negative_lost_timeout)
{
    Scanner scanner("scanner");

    EXPECT_FALSE(scanner.set_lost_timeout(Duration(time::milli(-1))));
    EXPECT_TRUE(scanner.set_lost_timeout(Duration(0)));
}

TEST_F(TestBtScan, test_discovery_refused)
{
    _daemon.on(adapter_interface, "StartDiscovery", [this](const dbus::Message & call) {
        _daemon.fail(call, "org.bluez.Error.NotReady", "Resource Not Ready");
    });
    _daemon.serve();
    Scanner scanner("scanner");

    const bool scanned = scanner.scan_for(Duration(time::milli(50)));
    _daemon.stop();

    EXPECT_FALSE(scanned);
    EXPECT_FALSE(_daemon.called(adapter_interface, "StopDiscovery"));
}

TEST_F(TestBtScan, test_unknown_adapter)
{
    _daemon.serve();
    Scanner scanner("scanner");
    ASSERT_TRUE(scanner.set_adapter(99));

    const bool scanned = scanner.scan_for(Duration(time::milli(50)));
    _daemon.stop();

    EXPECT_FALSE(scanned);
    EXPECT_TRUE(scanner.results().empty());
    EXPECT_FALSE(scanner.last_result().has_value());
    EXPECT_FALSE(_daemon.called(adapter_interface, "StartDiscovery"));
}

TEST_F(TestBtScan, test_scan_twice)
{
    _daemon.serve();
    Scanner scanner("scanner");

    const bool first = scanner.scan_for(Duration(time::milli(50)));
    const bool second = scanner.scan_for(Duration(time::milli(50)));

    EXPECT_TRUE(first);
    EXPECT_TRUE(second);
}

TEST_F(TestBtScan, test_stop_after_the_scan_ended_spares_the_next_scan)
{
    _daemon.serve();
    LateStopScanner scanner("scanner");
    ASSERT_TRUE(scanner.scan_for(Duration(time::milli(50))));
    scanner.stopper.join();

    Stopwatch stopwatch;
    const bool scanned = scanner.scan_for(Duration(time::milli(300)));
    const Duration elapsed = stopwatch.time();

    EXPECT_TRUE(scanned);
    EXPECT_GE(elapsed, Duration(time::milli(300)));
}

} // namespace test
