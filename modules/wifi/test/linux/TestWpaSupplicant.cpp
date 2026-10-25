#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include <fmt/format.h>
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

class TestWpaSupplicant: public ::testing::Test
{
    protected:
        using AddedNetwork = std::map<std::string, std::string>;

        TestWpaSupplicant() { LoggerManager::stream(); }

        virtual ~TestWpaSupplicant() { LoggerManager::clear_loggers(); }

        virtual void SetUp()
        {
            if (!FakeDaemon::available())
                GTEST_SKIP() << "no dbus-daemon";
            ASSERT_TRUE(_daemon.start({wpa_service}));
            serve_wpa_supplicant(_daemon);

            _daemon.set_property(wpa_interface, wpa_interface_interface, "State", text("disconnected"));
            _daemon.set_property(wpa_interface,
                                 wpa_interface_interface,
                                 "Networks",
                                 object_paths({home_network, attic_network}));
            _daemon.set_property(wpa_interface,
                                 wpa_interface_interface,
                                 "BSSs",
                                 object_paths({bss(0), bss(1), bss(2), bss(3), bss(4)}));
            this->add_bss(bss(0), "home", rsn_elements(akm_psk));
            this->add_bss(bss(1), "cafe", {});
            this->add_bss(bss(2), "loft", rsn_elements(akm_psk));
            this->add_bss(bss(3), "corp", rsn_elements(akm_8021x));
            this->add_bss(bss(4), "guest", rsn_elements(akm_owe));
            _daemon.set_property(home_network,
                                 network_interface,
                                 "Properties",
                                 dict({{"ssid", text("\"home\"")}, {"key_mgmt", text("WPA-PSK")}}));
            _daemon.set_property(attic_network,
                                 network_interface,
                                 "Properties",
                                 dict({{"ssid", text("6174746963")}, {"key_mgmt", text("WPA-EAP")}}));

            _daemon.on(wpa_interface_interface, "AddNetwork", [this](const dbus::Message & call) {
                AddedNetwork added;
                auto read = dbus::Iter(call).for_each_entry<std::string>(
                    [&added](const std::string & key, dbus::Iter & value) { return record(added, key, value); });
                EXPECT_TRUE(read.has_value());
                _added.emplace_back(std::move(added));
                _daemon.reply(call, {object_path(added_network(_added.size()))});
            });
            _daemon.on(wpa_interface_interface, "RemoveNetwork", [this](const dbus::Message & call) {
                _removed.push_back(dbus::Iter(call).get<std::string>().value_or(""));
                _daemon.reply(call, {});
            });
        }

        void add_bss(const std::string & path, const std::string & ssid, const std::vector<uint8_t> & elements)
        {
            _daemon.set_property(path, bss_interface, "SSID", bytes(ssid));
            _daemon.set_property(path, bss_interface, "IEs", bytes(elements));
            _daemon.set_property(path, bss_interface, "Privacy", boolean(!elements.empty()));
        }

        static Result<void> record(AddedNetwork & added, const std::string & key, dbus::Iter & value)
        {
            if (value.type() == 'a')
            {
                auto raw = value.get_array<uint8_t>();
                SIHD_UNEXPECTED_RETURN(raw);
                added.insert_or_assign(key, std::string(raw->begin(), raw->end()));
            }
            else if (value.type() == 'u')
                added.insert_or_assign(key, fmt::format("{}", value.get<uint32_t>().value_or(0)));
            else
                added.insert_or_assign(key, value.get<std::string>().value_or(""));
            return {};
        }

        void answer_select_with(const std::vector<std::string> & states)
        {
            _daemon.on(wpa_interface_interface, "SelectNetwork", [this, states](const dbus::Message & call) {
                _selected.push_back(dbus::Iter(call).get<std::string>().value_or(""));
                _daemon.reply(call, {});
                for (const std::string & state : states)
                    _daemon.emit_properties_changed(wpa_interface, wpa_interface_interface, {{"State", text(state)}});
            });
        }

        static std::vector<uint8_t> rsn_elements(uint8_t akm)
        {
            return {0x30, 0x14, 0x01, 0x00, 0x00, 0x0f, 0xac, 0x04, 0x01, 0x00, 0x00,
                    0x0f, 0xac, 0x04, 0x01, 0x00, 0x00, 0x0f, 0xac, akm,  0x00, 0x00};
        }

        static std::string bss(size_t index) { return fmt::format("{}/BSSs/{}", wpa_interface, index); }

        static std::string added_network(size_t count)
        {
            return fmt::format("{}/Networks/{}", wpa_interface, count + 1);
        }

        static constexpr char home_network[] = "/fi/w1/wpa_supplicant1/Interfaces/0/Networks/0";
        static constexpr char attic_network[] = "/fi/w1/wpa_supplicant1/Interfaces/0/Networks/1";
        static constexpr char network_interface[] = "fi.w1.wpa_supplicant1.Network";
        static constexpr char bss_interface[] = "fi.w1.wpa_supplicant1.BSS";
        static constexpr uint8_t akm_8021x = 1;
        static constexpr uint8_t akm_psk = 2;
        static constexpr uint8_t akm_owe = 18;

        FakeDaemon _daemon;
        std::vector<AddedNetwork> _added;
        std::vector<std::string> _removed;
        std::vector<std::string> _selected;
};

TEST_F(TestWpaSupplicant, test_backend_and_state)
{
    _daemon.set_property(wpa_interface, wpa_interface_interface, "State", text("4way_handshake"));
    _daemon.serve();

    auto managing = wifi::backend("wlan-test");
    auto current = wifi::state("wlan-test");

    ASSERT_TRUE(managing.has_value()) << managing.error().message;
    EXPECT_EQ(*managing, wifi::Backend::wpa_supplicant);
    ASSERT_TRUE(current.has_value()) << current.error().message;
    EXPECT_EQ(*current, wifi::State::connecting);
}

TEST_F(TestWpaSupplicant, test_scan_waits_for_scan_done)
{
    int scans = 0;
    _daemon.on(wpa_interface_interface, "Scan", [this, &scans](const dbus::Message & call) {
        _daemon.reply(call, {});
        _daemon.emit(wpa_interface, wpa_interface_interface, "ScanDone", {boolean(scans++ == 0)});
    });
    _daemon.serve();

    auto scanned = wifi::scan("wlan-test", fake_timeout);
    auto failed = wifi::scan("wlan-test", fake_timeout);

    ASSERT_TRUE(scanned.has_value()) << scanned.error().message;
    ASSERT_FALSE(failed.has_value());
    EXPECT_EQ(failed.error().code, ErrorCode::io_error);
    EXPECT_EQ(failed.error().message, "ScanDone reported a failure: wpa_supplicant scan");
}

TEST_F(TestWpaSupplicant, test_connect_known_network)
{
    this->answer_select_with({"associating", "completed"});
    _daemon.serve();

    auto connected = wifi::connect("wlan-test", "home", fake_timeout);

    _daemon.stop();
    ASSERT_TRUE(connected.has_value()) << connected.error().message;
    EXPECT_EQ(_selected, std::vector<std::string> {home_network});
    EXPECT_TRUE(_added.empty());
}

TEST_F(TestWpaSupplicant, test_connect_new_network)
{
    this->answer_select_with({"associating", "4way_handshake", "completed"});
    _daemon.serve();

    auto connected = wifi::connect("wlan-test", "loft", "a loft passphrase", fake_timeout);

    _daemon.stop();
    ASSERT_TRUE(connected.has_value()) << connected.error().message;
    EXPECT_EQ(_added,
              (std::vector<AddedNetwork> {{
                  {"ieee80211w", "1"},
                  {"key_mgmt", "WPA-PSK SAE"},
                  {"psk", "a loft passphrase"},
                  {"ssid", "loft"},
              }}));
    EXPECT_EQ(_selected, std::vector<std::string> {added_network(1)});
    EXPECT_TRUE(_removed.empty());
}

TEST_F(TestWpaSupplicant, test_connect_open_networks)
{
    this->answer_select_with({"associating", "completed"});
    _daemon.serve();

    auto cafe = wifi::connect("wlan-test", "cafe", fake_timeout);
    auto guest = wifi::connect("wlan-test", "guest", fake_timeout);

    _daemon.stop();
    ASSERT_TRUE(cafe.has_value()) << cafe.error().message;
    ASSERT_TRUE(guest.has_value()) << guest.error().message;
    EXPECT_EQ(_added,
              (std::vector<AddedNetwork> {{{"key_mgmt", "NONE"}, {"ssid", "cafe"}},
                                          {{"ieee80211w", "2"}, {"key_mgmt", "OWE"}, {"ssid", "guest"}}}));
    EXPECT_TRUE(_removed.empty());
}

TEST_F(TestWpaSupplicant, test_connect_timeout_removes_the_added_network)
{
    this->answer_select_with({"associating"});
    _daemon.serve();

    auto connected = wifi::connect("wlan-test", "cafe", Duration(time::ms(200)));

    _daemon.stop();
    ASSERT_FALSE(connected.has_value());
    EXPECT_EQ(connected.error().code, ErrorCode::timeout);
    EXPECT_EQ(connected.error().message, "dbus receive timed out: connecting to cafe");
    EXPECT_EQ(_removed, std::vector<std::string> {added_network(1)});
}

TEST_F(TestWpaSupplicant, test_refused_passphrase_removes_the_added_network)
{
    this->answer_select_with({"disconnected", "associating", "4way_handshake", "disconnected"});
    _daemon.serve();

    auto connected = wifi::connect("wlan-test", "loft", "a wrong passphrase", fake_timeout);

    _daemon.stop();
    ASSERT_FALSE(connected.has_value());
    EXPECT_EQ(connected.error().code, ErrorCode::io_error);
    EXPECT_EQ(connected.error().message, "wpa_supplicant disconnected: connecting to loft");
    EXPECT_EQ(_removed, std::vector<std::string> {added_network(1)});
}

TEST_F(TestWpaSupplicant, test_connect_refusals)
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
    EXPECT_EQ(unseen.error().message, "wpa_supplicant did not see elsewhere");

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
    EXPECT_TRUE(_added.empty());
}

TEST_F(TestWpaSupplicant, test_known_networks_and_forget)
{
    _daemon.serve();

    auto known = wifi::known_networks("wlan-test");
    auto forgotten = wifi::forget("wlan-test", "attic");
    auto unknown = wifi::forget("wlan-test", "elsewhere");

    _daemon.stop();
    ASSERT_TRUE(known.has_value()) << known.error().message;
    ASSERT_EQ(known->size(), 2u);
    EXPECT_EQ(known->at(0).ssid, "home");
    EXPECT_EQ(known->at(0).security, wifi::Security::personal);
    EXPECT_EQ(known->at(1).ssid, "attic");
    EXPECT_EQ(known->at(1).security, wifi::Security::enterprise);
    ASSERT_TRUE(forgotten.has_value()) << forgotten.error().message;
    EXPECT_EQ(_removed, std::vector<std::string> {attic_network});
    ASSERT_FALSE(unknown.has_value());
    EXPECT_EQ(unknown.error().code, ErrorCode::not_found);
    EXPECT_EQ(unknown.error().message, "no known network elsewhere");
}

TEST_F(TestWpaSupplicant, test_disconnect_errors)
{
    int disconnects = 0;
    _daemon.on(wpa_interface_interface, "Disconnect", [this, &disconnects](const dbus::Message & call) {
        if (disconnects++ == 0)
            return _daemon.fail(call, "fi.w1.wpa_supplicant1.NotConnected", "This interface is not connected");
        _daemon.fail(call, "org.freedesktop.DBus.Error.AccessDenied", "not allowed");
    });
    _daemon.serve();

    auto not_connected = wifi::disconnect("wlan-test");
    auto denied = wifi::disconnect("wlan-test");

    ASSERT_TRUE(not_connected.has_value()) << not_connected.error().message;
    ASSERT_FALSE(denied.has_value());
    EXPECT_EQ(denied.error().code, ErrorCode::permission_denied);
    EXPECT_EQ(denied.error().message, "org.freedesktop.DBus.Error.AccessDenied: not allowed");
}

} // namespace test
