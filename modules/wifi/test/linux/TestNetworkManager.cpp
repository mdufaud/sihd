#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <sihd/util/Logger.hpp>
#include <sihd/util/time.hpp>
#include <sihd/wifi/connection.hpp>
#include <sihd/wifi/scan.hpp>

#include "fake_daemon.hpp"

namespace test
{
using namespace sihd::util;
namespace wifi = sihd::wifi;

class TestNetworkManager: public ::testing::Test
{
    protected:
        TestNetworkManager() { LoggerManager::stream(); }

        virtual ~TestNetworkManager() { LoggerManager::clear_loggers(); }

        virtual void SetUp()
        {
            if (!FakeDaemon::available())
                GTEST_SKIP() << "no dbus-daemon";
            ASSERT_TRUE(_daemon.start({nm_service}));
            serve_network_manager(_daemon, true);

            _daemon.set_property(nm_device, nm_device_interface, "State", u32(30));
            this->add_access_point(home_strong, "home", 70, rsn_psk);
            this->add_access_point(home_weak, "home", 20, rsn_psk);
            this->add_access_point(loft, "loft", 50, rsn_psk);
            this->add_access_point(cafe, "cafe", 40, 0);
            this->add_access_point(corp, "corp", 60, rsn_8021x);

            _daemon.on(nm_wireless_interface, "GetAllAccessPoints", [this](const dbus::Message & call) {
                _daemon.reply(call, {object_paths({home_weak, home_strong, loft, cafe, corp})});
            });
            _daemon.on(settings_interface, "ListConnections", [this](const dbus::Message & call) {
                _daemon.reply(call, {object_paths({home_profile, wired_profile})});
            });
            _daemon.on(profile_interface, "GetSettings", [this](const dbus::Message & call) {
                if (call.path() == wired_profile)
                    return _daemon.reply(call,
                                         {profile_settings({{"connection", {{"type", text("802-3-ethernet")}}}})});
                _daemon.reply(call,
                              {profile_settings({{"connection", {{"type", text("802-11-wireless")}}},
                                                 {"802-11-wireless", {{"ssid", bytes("home")}}},
                                                 {"802-11-wireless-security", {{"key-mgmt", text("wpa-psk")}}}})});
            });
            _daemon.on(profile_interface, "Delete", [this](const dbus::Message & call) {
                _deleted.emplace_back(call.path());
                _daemon.reply(call, {});
            });
        }

        void add_access_point(const std::string & path, const std::string & ssid, uint8_t strength, uint32_t rsn_flags)
        {
            _daemon.set_property(path, access_point_interface, "Ssid", bytes(ssid));
            _daemon.set_property(path, access_point_interface, "Strength", u8(strength));
            _daemon.set_property(path, access_point_interface, "Flags", u32(rsn_flags != 0 ? 1 : 0));
            _daemon.set_property(path, access_point_interface, "WpaFlags", u32(0));
            _daemon.set_property(path, access_point_interface, "RsnFlags", u32(rsn_flags));
        }

        void emit_states(uint32_t final_state, uint32_t reason)
        {
            _daemon.emit(nm_device, nm_device_interface, "StateChanged", {u32(40), u32(30), u32(0)});
            _daemon.emit(nm_device, nm_device_interface, "StateChanged", {u32(final_state), u32(40), u32(reason)});
        }

        // the created profile is named after the ssid it was asked for
        static std::string created_profile(const dbus::Message & call)
        {
            std::string ssid;
            auto groups = dbus::Iter(call).for_each_entry<std::string>(
                [&ssid](const std::string & group, dbus::Iter & values) {
                    return values.for_each_entry<std::string>(
                        [&](const std::string & key, dbus::Iter & value) -> Result<void> {
                            if (group != "802-11-wireless" || key != "ssid")
                                return {};
                            auto raw = value.get_array<uint8_t>();
                            SIHD_UNEXPECTED_RETURN(raw);
                            ssid.assign(raw->begin(), raw->end());
                            return {};
                        });
                });
            EXPECT_TRUE(groups.has_value());
            return "/org/freedesktop/NetworkManager/Settings/" + ssid;
        }

        static constexpr char home_strong[] = "/org/freedesktop/NetworkManager/AccessPoint/1";
        static constexpr char home_weak[] = "/org/freedesktop/NetworkManager/AccessPoint/2";
        static constexpr char loft[] = "/org/freedesktop/NetworkManager/AccessPoint/3";
        static constexpr char cafe[] = "/org/freedesktop/NetworkManager/AccessPoint/4";
        static constexpr char corp[] = "/org/freedesktop/NetworkManager/AccessPoint/5";
        static constexpr char home_profile[] = "/org/freedesktop/NetworkManager/Settings/1";
        static constexpr char wired_profile[] = "/org/freedesktop/NetworkManager/Settings/2";
        static constexpr char active_connection[] = "/org/freedesktop/NetworkManager/ActiveConnection/1";
        static constexpr char access_point_interface[] = "org.freedesktop.NetworkManager.AccessPoint";
        static constexpr char settings_interface[] = "org.freedesktop.NetworkManager.Settings";
        static constexpr char profile_interface[] = "org.freedesktop.NetworkManager.Settings.Connection";
        static constexpr uint32_t rsn_psk = 0x188;
        static constexpr uint32_t rsn_8021x = 0x288;

        FakeDaemon _daemon;
        std::vector<std::string> _deleted;
};

TEST_F(TestNetworkManager, test_backend_detection)
{
    _daemon.serve();

    auto managing = wifi::backend("wlan-test");
    auto unknown = wifi::backend("eth-test");

    ASSERT_TRUE(managing.has_value()) << managing.error().message;
    EXPECT_EQ(*managing, wifi::Backend::network_manager);
    ASSERT_FALSE(unknown.has_value());
    EXPECT_EQ(unknown.error().code, ErrorCode::not_found);
    EXPECT_EQ(unknown.error().message, "no wifi daemon manages eth-test");
}

TEST_F(TestNetworkManager, test_unmanaged_device_is_not_found)
{
    _daemon.set_property(nm_device, nm_device_interface, "Managed", boolean(false));
    _daemon.serve();

    auto unmanaged = wifi::backend("wlan-test");

    ASSERT_FALSE(unmanaged.has_value());
    EXPECT_EQ(unmanaged.error().code, ErrorCode::not_found);
    EXPECT_EQ(unmanaged.error().message, "no wifi daemon manages wlan-test");
}

TEST_F(TestNetworkManager, test_state)
{
    _daemon.set_property(nm_device, nm_device_interface, "State", u32(70));
    _daemon.serve();

    auto current = wifi::state("wlan-test");

    ASSERT_TRUE(current.has_value()) << current.error().message;
    EXPECT_EQ(*current, wifi::State::connecting);
}

TEST_F(TestNetworkManager, test_scan_waits_for_last_scan)
{
    // the real LastScan is an int64, its value is never read
    _daemon.on(nm_wireless_interface, "RequestScan", [this](const dbus::Message & call) {
        _daemon.reply(call, {});
        _daemon.emit_properties_changed(nm_device, nm_wireless_interface, {{"ActiveAccessPoint", object_path("/")}});
        _daemon.emit_properties_changed(nm_device, nm_wireless_interface, {{"LastScan", u32(1234)}});
    });
    _daemon.serve();

    auto scanned = wifi::scan("wlan-test", fake_timeout);

    _daemon.stop();
    ASSERT_TRUE(scanned.has_value()) << scanned.error().message;
    EXPECT_TRUE(_daemon.called(nm_wireless_interface, "RequestScan"));
}

TEST_F(TestNetworkManager, test_connect_known_network)
{
    std::string activated_profile;
    std::string chosen_access_point;
    _daemon.on(nm_service, "ActivateConnection", [&](const dbus::Message & call) {
        dbus::Iter arguments(call);
        activated_profile = arguments.get<std::string>().value_or("");
        arguments.next();
        arguments.next();
        chosen_access_point = arguments.get<std::string>().value_or("");
        _daemon.reply(call, {object_path(active_connection)});
        this->emit_states(100, 0);
    });
    _daemon.serve();

    auto connected = wifi::connect("wlan-test", "home", fake_timeout);

    _daemon.stop();
    ASSERT_TRUE(connected.has_value()) << connected.error().message;
    EXPECT_EQ(activated_profile, home_profile);
    EXPECT_EQ(chosen_access_point, home_strong);
    EXPECT_TRUE(_deleted.empty());
}

TEST_F(TestNetworkManager, test_connect_new_network)
{
    std::string sent_passphrase;
    _daemon.on(nm_service, "AddAndActivateConnection", [&](const dbus::Message & call) {
        auto groups = dbus::Iter(call).for_each_entry<std::string>([&](const std::string & group, dbus::Iter & values) {
            return values.for_each_entry<std::string>([&](const std::string & key, dbus::Iter & value) -> Result<void> {
                if (group == "802-11-wireless-security" && key == "psk")
                    sent_passphrase = value.get<std::string>().value_or("");
                return {};
            });
        });
        EXPECT_TRUE(groups.has_value());
        _daemon.reply(call, {object_path(created_profile(call)), object_path(active_connection)});
        this->emit_states(100, 0);
    });
    _daemon.serve();

    auto connected = wifi::connect("wlan-test", "loft", "a loft passphrase", fake_timeout);

    _daemon.stop();
    ASSERT_TRUE(connected.has_value()) << connected.error().message;
    EXPECT_EQ(sent_passphrase, "a loft passphrase");
    EXPECT_TRUE(_deleted.empty());
}

TEST_F(TestNetworkManager, test_failed_connect_leaves_no_profile)
{
    _daemon.on(nm_service, "AddAndActivateConnection", [this](const dbus::Message & call) {
        const std::string profile = created_profile(call);
        _daemon.reply(call, {object_path(profile), object_path(active_connection)});
        if (profile.ends_with("loft"))
            this->emit_states(120, 7);
    });
    _daemon.serve();

    auto refused = wifi::connect("wlan-test", "loft", "a wrong passphrase", fake_timeout);
    auto timed_out = wifi::connect("wlan-test", "cafe", Duration(time::ms(200)));

    _daemon.stop();
    ASSERT_FALSE(refused.has_value());
    EXPECT_EQ(refused.error().code, ErrorCode::io_error);
    EXPECT_EQ(refused.error().message, "networkmanager failed: reason 7: connecting to loft");
    ASSERT_FALSE(timed_out.has_value());
    EXPECT_EQ(timed_out.error().code, ErrorCode::timeout);
    EXPECT_EQ(timed_out.error().message, "dbus receive timed out: connecting to cafe");
    EXPECT_EQ(_deleted,
              (std::vector<std::string> {"/org/freedesktop/NetworkManager/Settings/loft",
                                         "/org/freedesktop/NetworkManager/Settings/cafe"}));
}

TEST_F(TestNetworkManager, test_connect_refusals)
{
    _daemon.serve();

    auto unseen = wifi::connect("wlan-test", "elsewhere", fake_timeout);
    auto known = wifi::connect("wlan-test", "home", "a new passphrase", fake_timeout);
    auto secured = wifi::connect("wlan-test", "loft", fake_timeout);
    auto enterprise = wifi::connect("wlan-test", "corp", "a corp passphrase", fake_timeout);
    auto open = wifi::connect("wlan-test", "cafe", "a cafe passphrase", fake_timeout);

    _daemon.stop();
    ASSERT_FALSE(unseen.has_value());
    EXPECT_EQ(unseen.error().code, ErrorCode::not_found);
    EXPECT_EQ(unseen.error().message, "networkmanager did not see elsewhere");

    ASSERT_FALSE(known.has_value());
    EXPECT_EQ(known.error().code, ErrorCode::already_exists);
    EXPECT_EQ(known.error().message, "home is already known: forget it first");

    ASSERT_FALSE(secured.has_value());
    EXPECT_EQ(secured.error().code, ErrorCode::invalid_argument);
    EXPECT_EQ(secured.error().message, "loft needs a passphrase");

    ASSERT_FALSE(enterprise.has_value());
    EXPECT_EQ(enterprise.error().code, ErrorCode::not_supported);
    EXPECT_EQ(enterprise.error().message, "corp needs 802.1x, which is not supported");

    ASSERT_FALSE(open.has_value());
    EXPECT_EQ(open.error().code, ErrorCode::invalid_argument);
    EXPECT_EQ(open.error().message, "cafe takes no passphrase");
    EXPECT_FALSE(_daemon.called(nm_service, "AddAndActivateConnection"));
    EXPECT_FALSE(_daemon.called(nm_service, "ActivateConnection"));
}

TEST_F(TestNetworkManager, test_known_networks_and_forget)
{
    _daemon.serve();

    auto known = wifi::known_networks("wlan-test");
    auto forgotten = wifi::forget("wlan-test", "home");
    auto unknown = wifi::forget("wlan-test", "elsewhere");

    _daemon.stop();
    ASSERT_TRUE(known.has_value()) << known.error().message;
    ASSERT_EQ(known->size(), 1u);
    EXPECT_EQ(known->front().ssid, "home");
    EXPECT_EQ(known->front().security, wifi::Security::personal);
    ASSERT_TRUE(forgotten.has_value()) << forgotten.error().message;
    EXPECT_EQ(_deleted, std::vector<std::string> {home_profile});
    ASSERT_FALSE(unknown.has_value());
    EXPECT_EQ(unknown.error().code, ErrorCode::not_found);
    EXPECT_EQ(unknown.error().message, "no known network elsewhere");
}

TEST_F(TestNetworkManager, test_disconnect_errors)
{
    int disconnects = 0;
    _daemon.on(nm_device_interface, "Disconnect", [this, &disconnects](const dbus::Message & call) {
        if (disconnects++ == 0)
            return _daemon.fail(call, "org.freedesktop.NetworkManager.Device.NotActive", "This device is not active");
        _daemon.fail(call, "org.freedesktop.DBus.Error.AccessDenied", "not allowed");
    });
    _daemon.serve();

    auto inactive = wifi::disconnect("wlan-test");
    auto denied = wifi::disconnect("wlan-test");

    ASSERT_TRUE(inactive.has_value()) << inactive.error().message;
    ASSERT_FALSE(denied.has_value());
    EXPECT_EQ(denied.error().code, ErrorCode::permission_denied);
    EXPECT_EQ(denied.error().message, "org.freedesktop.DBus.Error.AccessDenied: not allowed");
}

} // namespace test
